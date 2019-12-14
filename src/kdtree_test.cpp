#include <gtest/gtest.h>
#include <fmt/format.h>
#include "kdtree.hpp"

using namespace toy::kdtree;

TEST(KDTreeTest, Construction) {
  using std::string;
  using Tree = Tree<string>; using Leaf = Leaf<string>; using Branch = Branch<string>; using Root = Root<string>;

  {
    // +-----+--------+
    // |     |        |
    // |     |        |
    // +-----+--------+
    //  0.25    0.75

    Branch root{SplitType::HORIZONTAL, 0.25, new Leaf{"leaf1"}, new Leaf{"leaf2"}};
    EXPECT_EQ(root.node_type_, NodeType::BRANCH);
    EXPECT_EQ(root.children_[0]->node_type_, NodeType::LEAF);
  }

  {
    // +-----+--------+
    // |     |        | 0.4
    // |     |--------|
    // |     |        | 0.6
    // |     |        |
    // +-----+--------+
    //   0.4    0.6

    Branch root{SplitType::HORIZONTAL, 0.4,
        new Leaf{""},
        new Branch{SplitType::VERTICAL, 0.4,
              new Leaf{""},
              new Leaf{""}}};
  }

  {
    Root root;
    root.insertRoot(new Leaf{""}, SplitType::VERTICAL, 0.4, ChildIndex::FIRST);
    root.insertRoot(new Leaf{""}, SplitType::HORIZONTAL, 0.4, ChildIndex::SECOND);
  }
}

TEST(KDTreeTest, hitTestSeparator_Simple) {
  using Tree = Tree<int>; using Leaf = Leaf<int>; using Branch = Branch<int>; using Root = Root<int>;

  // +-----+--------+
  // |     |        |
  // |     |        |
  // +-----+--------+
  //  0.25    0.75

  Branch root{SplitType::HORIZONTAL, 0.25, new Leaf, new Leaf};

  EXPECT_EQ(      root.hitTestSeparator({0.2, 0}, {0.06, 0}).has_value(), true);
  EXPECT_FLOAT_EQ(root.hitTestSeparator({0.2, 0}, {0.06, 0}).value().second, 0.2);
  EXPECT_EQ(      root.hitTestSeparator({0.3, 0}, {0.06, 0}).has_value(), true);
  EXPECT_FLOAT_EQ(root.hitTestSeparator({0.3, 0}, {0.06, 0}).value().second, 0.3);
  EXPECT_EQ(      root.hitTestSeparator({0.2, 0}, {0.04, 0}).has_value(), false);
  EXPECT_EQ(      root.hitTestSeparator({0.3, 0}, {0.04, 0}).has_value(), false);
}

TEST(KDTreeTest, hitTestSeparator_Nested1) {
  using Tree = Tree<int>; using Leaf = Leaf<int>; using Branch = Branch<int>; using Root = Root<int>;

  // +-----+--------+
  // |     |        | 0.4
  // |     |--------|
  // |     |        | 0.6
  // |     |        |
  // +-----+--------+
  //   0.4    0.6

  Branch root{SplitType::HORIZONTAL, 0.4,
      new Leaf,
      new Branch{SplitType::VERTICAL, 0.4,
        new Leaf,
        new Leaf}};

  fvec2 hit_margin{0.1, 0.1};
  fvec2 tests[4] = {
    {0.45, 0.45},
    {0.45, 0.55},
    {0.55, 0.45},
    {0.55, 0.55},
  };
  std::optional<std::pair<Branch*, float>> results[4];
  for (auto i = 0; i < 4; i++) {
    results[i] = root.hitTestSeparator(tests[i], hit_margin);
  }

  EXPECT_EQ(results[0].has_value(), true);
  EXPECT_EQ(results[1].has_value(), true);
  EXPECT_EQ(results[2].has_value(), true);
  EXPECT_EQ(results[3].has_value(), false);

  EXPECT_EQ(results[0].value(), std::make_pair(&root, 0.45f));
  EXPECT_EQ(results[1].value(), std::make_pair(&root, 0.45f));
  EXPECT_EQ(results[2].value(), std::make_pair(dynamic_cast<Branch*>(root.children_[1].get()), 0.45f));
}

TEST(KDTreeTest, hitTestSeparator_Nested2) {
  using Tree = Tree<int>; using Leaf = Leaf<int>; using Branch = Branch<int>; using Root = Root<int>;
  using glm::fvec2;

  // +-----+--------   +
  // |     |       |   |
  // |     |       |   |
  // +-----+-----------+
  //        0.75  0.25
  //       (0.6) (0.2)
  //  0.2     0.8

  Branch root{SplitType::HORIZONTAL, 0.2,
      new Leaf,
      new Branch{SplitType::HORIZONTAL, 0.75,
        new Leaf,
        new Leaf}};

  fvec2 hit_margin{0.1, 0};
  fvec2 tests[6] = {
    {0.25, 0},
    {0.35, 0},
    {0.65, 0},
    {0.75, 0},
    {0.85, 0},
    {0.95, 0},
  };

  std::optional<std::pair<Branch*, float>> results[6];
  for (auto i = 0; i < 6; i++) {
    results[i] = root.hitTestSeparator(tests[i], hit_margin);
  }

  EXPECT_EQ(results[0].has_value(), true);
  EXPECT_EQ(results[1].has_value(), false);
  EXPECT_EQ(results[2].has_value(), false);
  EXPECT_EQ(results[3].has_value(), true);
  EXPECT_EQ(results[4].has_value(), true);
  EXPECT_EQ(results[5].has_value(), false);

  auto child = dynamic_cast<Branch*>(root.children_[1].get());
  EXPECT_EQ(results[0].value(), std::make_pair(&root, 0.25f));
  EXPECT_EQ(results[3].value(), std::make_pair(child, (0.75f - 0.2f) / 0.8f));
  EXPECT_EQ(results[4].value(), std::make_pair(child, (0.85f - 0.2f) / 0.8f));
}

TEST(KDTreeTest, insertNextTo) {
  using Tree = Tree<int>; using Leaf = Leaf<int>; using Branch = Branch<int>; using Root = Root<int>;
  auto arg1 = SplitType::VERTICAL; float arg2 = 0.5; auto arg3 = ChildIndex::FIRST;

  Root root;
  auto make_finder = [](int id) {
    return [id](Tree& tree) {
      auto leaf = dynamic_cast<Leaf*>(&tree);
      return leaf && leaf->value_ == id;
    };
  };
  root.insertRoot(new Leaf{1}, arg1, arg2, arg3);
  EXPECT_EQ(root.insertNextTo(make_finder( 1), new Leaf{2}, arg1, arg2, arg3), true);
  EXPECT_EQ(root.insertNextTo(make_finder(-1),     nullptr, arg1, arg2, arg3), false);
  EXPECT_EQ(root.insertNextTo(make_finder( 2), new Leaf{3}, arg1, arg2, arg3), true);
}

TEST(KDTreeTest, removeIf) {
  using Tree = Tree<int>; using Leaf = Leaf<int>; using Branch = Branch<int>; using Root = Root<int>;
  auto arg1 = SplitType::VERTICAL; float arg2 = 0.5; auto arg3 = ChildIndex::FIRST;

  Root root;
  auto make_finder = [](int id) {
    return [id](Tree& tree) {
      auto leaf = dynamic_cast<Leaf*>(&tree);
      return leaf && leaf->value_ == id;
    };
  };
  // []
  EXPECT_EQ(root.removeIf(make_finder(1)), false);

  // [] => [1]
  root.insertRoot(new Leaf{1}, arg1, arg2, arg3);
  EXPECT_EQ(root.removeIf(make_finder(1)), true);

  // [] => [1, 2]
  root.insertRoot(new Leaf{1}, arg1, arg2, arg3);
  root.insertNextTo(make_finder(1), new Leaf{2}, arg1, arg2, arg3);
  // [1, 2] => [1]
  EXPECT_EQ(root.removeIf(make_finder(2)), true);

  // [1] => [1, 2]
  root.insertNextTo(make_finder(1), new Leaf{2}, arg1, arg2, arg3);
  // [1, 2] => [2]
  EXPECT_EQ(root.removeIf(make_finder(1)), true);

  auto leaf = dynamic_cast<Leaf*>(root.root_.get());
  EXPECT_EQ(leaf->value_, 2);
}

TEST(KDTreeTest, forEachLeaf) {
  using Tree = Tree<int>; using Leaf = Leaf<int>; using Branch = Branch<int>; using Root = Root<int>;

  {
    // +-----+--------+
    // |     |        | 0.4
    // |     |--------|
    // |     |        | 0.6
    // |     |        |
    // +-----+--------+
    //   0.4    0.6

    Branch root{SplitType::HORIZONTAL, 0.4,
        new Leaf{1},
        new Branch{SplitType::VERTICAL, 0.4,
              new Leaf{2},
              new Leaf{3}}};

    std::string result1 = "\n";
    std::string expected1 = R"(
leaf_id = 1, offset = (0, 0), size = (40, 200)
leaf_id = 2, offset = (40, 0), size = (60, 80)
leaf_id = 3, offset = (40, 80), size = (60, 120)
)";

    std::string result2 = "\n";
    std::string expected2 = R"(
leaf_id = 1, offset = (15, 5), size = (40, 200)
leaf_id = 2, offset = (55, 5), size = (60, 80)
leaf_id = 3, offset = (55, 85), size = (60, 120)
)";

    root.forEachLeaf({0, 0}, {100, 200}, [&](Leaf& leaf, ivec2 offset, ivec2 size) {
      result1 += fmt::format("leaf_id = {}, offset = ({}, {}), size = ({}, {})\n",
          leaf.value_, offset[0], offset[1], size[0], size[1]);
    });
    EXPECT_EQ(result1, expected1);

    root.forEachLeaf({15, 5}, {100, 200}, [&](Leaf& leaf, ivec2 offset, ivec2 size) {
      result2 += fmt::format("leaf_id = {}, offset = ({}, {}), size = ({}, {})\n",
          leaf.value_, offset[0], offset[1], size[0], size[1]);
    });
    EXPECT_EQ(result2, expected2);
  }
}

TEST(KDTreeTest, forEachTree) {
  using Tree = Tree<int>; using Leaf = Leaf<int>; using Branch = Branch<int>; using Root = Root<int>;

  {
    // +-----+--------+
    // |     |        | 0.4
    // |     |--------|
    // |     |        | 0.6
    // |     |        |
    // +-----+--------+
    //   0.4    0.6

    Branch root{SplitType::HORIZONTAL, 0.4,
        new Leaf{1},
        new Branch{SplitType::VERTICAL, 0.4,
              new Leaf{2},
              new Leaf{3}}};

    std::string result1 = "\n";
    std::string expected1 = R"(
offset = (0, 0), size = (100, 200), split_type = 0
offset = (0, 0), size = (40, 200), leaf_id = 1
offset = (40, 0), size = (60, 200), split_type = 1
offset = (40, 0), size = (60, 80), leaf_id = 2
offset = (40, 80), size = (60, 120), leaf_id = 3
)";

    std::string result2 = "\n";
    std::string expected2 = R"(
offset = (15, 5), size = (100, 200), split_type = 0
offset = (15, 5), size = (40, 200), leaf_id = 1
offset = (55, 5), size = (60, 200), split_type = 1
offset = (55, 5), size = (60, 80), leaf_id = 2
offset = (55, 85), size = (60, 120), leaf_id = 3
)";

    root.forEachTree({0, 0}, {100, 200}, [&](Tree* tree, ivec2 offset, ivec2 size) {
      auto branch = dynamic_cast<Branch*>(tree);
      auto leaf = dynamic_cast<Leaf*>(tree);
      if (branch) {
        result1 += fmt::format("offset = ({}, {}), size = ({}, {}), split_type = {}\n",
            offset[0], offset[1], size[0], size[1], branch->split_type_);
      }
      if (leaf) {
        result1 += fmt::format("offset = ({}, {}), size = ({}, {}), leaf_id = {}\n",
            offset[0], offset[1], size[0], size[1], leaf->value_);
      }
    });
    EXPECT_EQ(result1, expected1);

    root.forEachTree({15, 5}, {100, 200}, [&](Tree* tree, ivec2 offset, ivec2 size) {
      auto branch = dynamic_cast<Branch*>(tree);
      auto leaf = dynamic_cast<Leaf*>(tree);
      if (branch) {
        result2 += fmt::format("offset = ({}, {}), size = ({}, {}), split_type = {}\n",
            offset[0], offset[1], size[0], size[1], branch->split_type_);
      }
      if (leaf) {
        result2 += fmt::format("offset = ({}, {}), size = ({}, {}), leaf_id = {}\n",
            offset[0], offset[1], size[0], size[1], leaf->value_);
      }
    });
    EXPECT_EQ(result2, expected2);
  }
}
