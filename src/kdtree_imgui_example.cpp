#include <memory>

#include <fmt/format.h>
#include <imgui.h>
#include <imgui_internal.h>

#include "window.hpp"
#include "kdtree.hpp"

// TODO:
// - [x] add panels, draw panels
// - [x] subtract main menu bar size
// - [x] panel split
// - [@] show resize cursor on separator
// - [ ] handle resize on click
// - [ ] panel remove (handle in a "non-immediate" mode)

using namespace toy;

using Tree   = kdtree::Tree<std::string>;
using Leaf   = kdtree::Leaf<std::string>;
using Branch = kdtree::Branch<std::string>;
using Root   = kdtree::Root<std::string>;

using glm::ivec2;

struct PanelManager {
  Root panels_;
  int counter_ = 1;

  bool empty() {
    return panels_.root_.get() == nullptr;
  }

  Leaf* newPanel() {
    std::string id = fmt::format("Panel {}", counter_);
    auto leaf = new Leaf{id};
    counter_++;
    return leaf;
  }

  void addToRoot(kdtree::SplitType split_type = kdtree::SplitType::HORIZONTAL) {
    panels_.insertRoot(newPanel(), split_type, 0.5, kdtree::ChildIndex::SECOND);
  }

  std::function<bool(Tree&)> makeFinder(const std::string& id) {
    return [&](Tree& tree) {
      auto leaf = dynamic_cast<Leaf*>(&tree);
      return leaf && leaf->value_ == id;
    };
  }

  void splitNewNextToId(const std::string& id, kdtree::SplitType split_type = kdtree::SplitType::HORIZONTAL) {
    auto inserted = panels_.insertNextTo(makeFinder(id), newPanel(), split_type, 0.5, kdtree::ChildIndex::SECOND);
    assert(inserted); // Not found Leaf next to which new leaf is supposed to be inserted
  }

  // TODO:
  // this cannot be called in an immediate-mode manner
  // (e.g. during `processPanel` thus during `forEachLeaf`)
  void removeById(const std::string& id) {
    auto removed = panels_.removeIf(makeFinder(id));
    assert(removed);
  }
};

static PanelManager panel_manager_;
static std::unique_ptr<toy::Window> window_;
static bool done_ = false;

void processMainMenuBar() {
  if (ImGui::BeginMainMenuBar()) {
    if (ImGui::BeginMenu("Menu")) {
      if (panel_manager_.empty()) {
        if (ImGui::MenuItem("Add Panel")) {
          panel_manager_.addToRoot();
        };
      } else {
        if (ImGui::BeginMenu("Add panel")) {
          if (ImGui::MenuItem("Horizontal")) {
            panel_manager_.addToRoot(kdtree::SplitType::HORIZONTAL);
          }
          if (ImGui::MenuItem("Vertical")) {
            panel_manager_.addToRoot(kdtree::SplitType::VERTICAL);
          }
          ImGui::EndMenu();
        }
      }
      if (ImGui::MenuItem("Quit")) {
        done_ = true;
      };
      ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
  }
}

void processPanel(Leaf& leaf) {
  auto flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoResize;
  auto name = leaf.value_.data();
  ImGui::Begin(name, nullptr, flags);

  if (ImGui::BeginMenuBar()) {
    if (ImGui::BeginMenu(name)) {
      if (ImGui::BeginMenu("Split")) {
        if (ImGui::MenuItem("Horizontal")) {
          panel_manager_.splitNewNextToId(leaf.value_, kdtree::SplitType::HORIZONTAL);
        }
        if (ImGui::MenuItem("Vertical")) {
          panel_manager_.splitNewNextToId(leaf.value_, kdtree::SplitType::VERTICAL);
        }
        ImGui::EndMenu();
      }
      if (ImGui::MenuItem("Close")) {
        panel_manager_.removeById(leaf.value_);
      }
      ImGui::EndMenu();
    }
    ImGui::EndMenuBar();
  }

  ImGui::Text("This is Panel");
  ImGui::End();
}

void processPanels() {
  // Take care offset due to MainMenuBar
  ImGuiContext& g = *ImGui::GetCurrentContext();
  ivec2 total_offset = {0, g.FontBaseSize + g.Style.FramePadding.y * 2};

  panel_manager_.panels_.forEachLeaf(total_offset, window_->size_, [&](Leaf& leaf, ivec2 offset, ivec2 size) {
    ImVec2 _offset{(float)offset[0], (float)offset[1]};
    ImVec2 _size{(float)size[0], (float)size[1]};
    ImGui::SetNextWindowPos(_offset);
    ImGui::SetNextWindowSize(_size);
    processPanel(leaf);
  });
}

void processUI() {
  processMainMenuBar();
  processPanels();
}

int main(const int argc, const char* argv[]) {
  window_.reset(new toy::Window{"My Window", {800, 600}});
  while(!done_) {
    window_->newFrame();
    processUI();
    window_->render();
    done_ = done_ || window_->shouldClose();
  }
  return 0;
}
