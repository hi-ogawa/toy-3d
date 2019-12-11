#include <optional>
#include <tuple>
#include <memory>
#include <functional>

#include <glm/glm.hpp>

namespace toy {
namespace kdtree {

using glm::fvec2, glm::ivec2;

enum struct SplitType {
  HORIZONTAL,
  VERTICAL,
};

enum struct ChildIndex {
  FIRST  = 0,  // FIRST means positionally left for HORIZONTAL and top for VERTICAL
  SECOND = 1,
};

enum struct NodeType {
  LEAF,
  BRANCH
};

template<typename T>
struct Branch;

template<typename T>
struct Leaf;

template<typename T>
struct Tree {
  // TODO: don't need to keep NodeType since dynamic_cast<Leaf*> and dynamic_cast<Branch*> tells it?
  NodeType node_type_;

  Tree(NodeType node_type) : node_type_{node_type} {};
  virtual ~Tree() {};
  virtual std::optional<std::pair<Branch<T>*, float>> hitTestSeparator(fvec2, fvec2) = 0;

  using iter_func_t = const std::function<void(Leaf<T>& /*leaf*/, ivec2 /*offset*/, ivec2 /*size*/)>;
  virtual void forEachLeaf(ivec2 /*offset*/, ivec2 /*size*/, iter_func_t&) = 0;

  // overriden only by `Branch`
  using predicate_t = const std::function<bool(Tree&)>;
  virtual bool insertNextTo(
      predicate_t& /*pred*/, Tree<T>* /*insertee*/,
      SplitType /*split_type*/, float /*fraction*/, ChildIndex /*insertee_index*/) {
    return false;
  };

  virtual bool removeIf(predicate_t& /*pred*/, std::unique_ptr<Tree>& /* holder */) {
    return false;
  };

  // helper
  static void insert(
      std::unique_ptr<Tree>& holder, Tree<T>* insertee,
      SplitType split_type, float fraction, ChildIndex insertee_index) {
    auto index = static_cast<int>(insertee_index);
    auto other_index = (static_cast<int>(insertee_index) + 1) % 2;
    auto old_tree = holder.release();
    auto new_branch = new Branch<T>{split_type, fraction};
    new_branch->children_[index].reset(insertee);
    new_branch->children_[other_index].reset(old_tree);
    holder.reset(new_branch);
  };
};

template<typename T>
struct Leaf : Tree<T> {
  T value_;

  Leaf() : Tree<T>{NodeType::LEAF} {}
  Leaf(const T& value) : Tree<T>{NodeType::LEAF}, value_{value} {}
  Leaf(T&& value) : Tree<T>{NodeType::LEAF}, value_{std::move(value)} {}
  std::optional<std::pair<Branch<T>*, float>> hitTestSeparator(fvec2 input, fvec2 hit_margin) override {
    return {};
  }
  void forEachLeaf(ivec2 offset, ivec2 size, typename Tree<T>::iter_func_t& iter_func) override {
    iter_func(*this, offset, size);
  }
};

template<typename T>
struct Branch : Tree<T> {
  SplitType split_type_;
  float fraction_;
  std::unique_ptr<Tree<T>> children_[2]; // Invariant: children_[i].get() != nullptr

  Branch() : Tree<T>{NodeType::BRANCH} {}

  // children_ should be filled immediately after this construction
  Branch(SplitType split_type, float fraction)
    : Tree<T>{NodeType::BRANCH}, split_type_{split_type}, fraction_{fraction} {}

  // Shortcut construction
  template<typename T0, typename T1>
  Branch(SplitType split_type, float fraction, T0* c0, T1* c1)
    : Tree<T>{NodeType::BRANCH}, split_type_{split_type}, fraction_{fraction} {
    auto tree0 = dynamic_cast<Tree<T>*>(c0);
    auto tree1 = dynamic_cast<Tree<T>*>(c1);
    assert(tree0); assert(tree1); // TODO: static assert this compatibility?
    children_[0].reset(tree0);
    children_[1].reset(tree1);
  }

  // @param    input          \in [0, 1]^2
  // @param    hit_margin     \in [0, 1]^2  (this should be necessary to naturally support non-square)
  // @return   Hit testing result with float showing the franction of hit point within the Branch,
  //           with which you can implement resizing directly on Branch.
  std::optional<std::pair<Branch<T>*, float>> hitTestSeparator(fvec2 input, fvec2 hit_margin) override {
    int coord_index = split_type_ == SplitType::HORIZONTAL ? 0 : 1;
    float input_fraction = input[coord_index];
    float margin = hit_margin[coord_index];

    if (glm::abs(input_fraction - fraction_) < margin) {
      return std::make_pair(this, input_fraction);
    }

    bool choice = input_fraction < fraction_;
    auto& next_tree = children_[choice ? 0 : 1];
    auto next_input = input;
    auto next_hit_margin = hit_margin;
    auto offset = choice ? 0 : fraction_;
    auto rescale = choice ? fraction_ : 1 - fraction_;
    next_input[coord_index] = (input[coord_index] - offset) / rescale;
    next_hit_margin[coord_index] = hit_margin[coord_index] / rescale;
    return next_tree->hitTestSeparator(next_input, next_hit_margin);
  }

  void forEachLeaf(ivec2 offset, ivec2 size, typename Tree<T>::iter_func_t& iter_func) override {
    int index = split_type_ == SplitType::HORIZONTAL ? 0 : 1;
    ivec2 first_size  = size;
    ivec2 second_size = size;
    ivec2 first_offset = offset;
    ivec2 second_offset = offset;
    first_size[index] = size[index] * fraction_;
    second_size[index] = size[index] - first_size[index];
    second_offset[index] = offset[index] + first_size[index];
    children_[0]->forEachLeaf(first_offset, first_size, iter_func);
    children_[1]->forEachLeaf(second_offset, second_size, iter_func);
  }

  bool insertNextTo(
      typename Tree<T>::predicate_t& pred, Tree<T>* insertee,
      SplitType split_type, float fraction, ChildIndex insertee_index) override {
    if (pred(*children_[0].get())) {
      Tree<T>::insert(children_[0], insertee, split_type, fraction, insertee_index);
      return true;
    }
    if (pred(*children_[1].get())) {
      Tree<T>::insert(children_[1], insertee, split_type, fraction, insertee_index);
      return true;
    }
    return (
        children_[0]->insertNextTo(pred, insertee, split_type, fraction, insertee_index) ||
        children_[1]->insertNextTo(pred, insertee, split_type, fraction, insertee_index));
  }

  // helper
  static void remove(std::unique_ptr<Tree<T>>& branch_holder, ChildIndex deletee_index) {
    assert(branch_holder->node_type_ == NodeType::BRANCH);
    auto index = static_cast<int>(deletee_index);
    auto other_index = (static_cast<int>(deletee_index) + 1) % 2;
    auto old_branch = dynamic_cast<Branch<T>*>(branch_holder.release());
    old_branch->children_[index].reset(); // aka delete
    branch_holder.reset(old_branch->children_[other_index].release()); // move non-deleted child up the ladder
  }

  bool removeIf(typename Tree<T>::predicate_t& pred, std::unique_ptr<Tree<T>>& holder) override {
    if (pred(*children_[0].get())) {
      Branch<T>::remove(holder, ChildIndex::FIRST);
      return true;
    }
    if (pred(*children_[1].get())) {
      Branch<T>::remove(holder, ChildIndex::SECOND);
      return true;
    }
    return (
      children_[0]->removeIf(pred, children_[0]) ||
      children_[1]->removeIf(pred, children_[1]));
  }
};

// NOTE: "empty" state has to be handled specifically
//       (i.e. not reducable to usual Leaf/Branch case.)
template<typename T>
struct Root {
  std::unique_ptr<Tree<T>> root_;

  std::optional<std::pair<Branch<T>*, float>> hitTestSeparator(ivec2 input, ivec2 hit_margin, ivec2 size) {
    if (!root_) return {};

    fvec2 n_input = static_cast<fvec2>(input) / static_cast<fvec2>(size);
    fvec2 n_hit_margin = static_cast<fvec2>(hit_margin) / static_cast<fvec2>(size);
    return root_->hitTestSeparator(n_input, n_hit_margin);
  }

  std::optional<std::pair<Branch<T>*, float>> applyResize(ivec2 input, ivec2 hit_margin, ivec2 size) {
    auto result = hitTestSeparator(input, hit_margin, size);
    if (!result) return {};

    Branch<T>* hit_branch;
    float new_fraction;
    std::tie(hit_branch, new_fraction) = result.value();
    hit_branch->fraction_ = new_fraction;
    return result;
  }

  bool removeIf(typename Tree<T>::predicate_t& pred) {
    if (!root_) return false;
    if (pred(*root_.get())) {
      root_.reset();
      return true;
    }
    return root_->removeIf(pred, root_);
  }

  void insertRoot(Tree<T>* insertee, SplitType split_type, float fraction, ChildIndex insertee_index) {
    if (!root_) {
      // in this case, other arguments (split_type, etc ..) are irrelavant.
      root_.reset(insertee);
      return;
    }
    Tree<T>::insert(root_, insertee, split_type, fraction, insertee_index);
  }

  // Insert `insertee` next to the first tree with pred = true
  // TODO: this has to assert returned bool, otherwise `insertee` is leaked.
  bool insertNextTo(
      typename Tree<T>::predicate_t& pred, Tree<T>* insertee,
      SplitType split_type, float fraction, ChildIndex insertee_index) {
    if (!root_) return false;
    if (pred(*root_.get())) {
      insertRoot(insertee, split_type, fraction, insertee_index);
      return true;
    }
    return root_->insertNextTo(pred, insertee, split_type, fraction, insertee_index);
  }

  // NOTE: during this loop, you cannot call `removeIf`.
  void forEachLeaf(ivec2 size, typename Tree<T>::iter_func_t& iter_func) {
    if (!root_) return;
    root_->forEachLeaf({0, 0}, size, iter_func);
  }

};

} // namespace kdtree
} // namespace toy
