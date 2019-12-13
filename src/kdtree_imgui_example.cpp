#include <memory>

#include <fmt/format.h>
#include <imgui.h>
#include <imgui_internal.h>

#include "window.hpp"
#include "kdtree.hpp"
#include "utils.hpp"

using namespace toy;
using namespace toy::utils;

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

  void splitNewNextToId(const std::string& id, kdtree::SplitType split_type) {
    auto inserted = panels_.insertNextTo(makeFinder(id), newPanel(), split_type, 0.5, kdtree::ChildIndex::SECOND);
    assert(inserted);
  }

  // NOTE:
  // this cannot be called in an immediate-mode manner
  // (e.g. during `processPanel` thus during `forEachLeaf`)
  void removeById(const std::string& id) {
    auto removed = panels_.removeIf(makeFinder(id));
    assert(removed);
  }
};

struct App {
  using Command = std::function<void()>;
  std::vector<Command> commands_;
  PanelManager panel_manager_;
  std::unique_ptr<toy::Window> window_;
  bool done_ = false;
  ivec2 main_offset_;
  ivec2 main_size_;

  App() {
    window_.reset(new toy::Window{"My Window", {800, 600}});
  }

  void processMainMenuBar();
  void processPanels();
  void processPanel(Leaf&);
  void processPanelResize();
  void setMainContentRect() {
    auto g = window_->imgui_context_;
    // cf. ImGui::BeginMainMenuBar
    main_offset_ = {0, g->FontBaseSize + g->Style.FramePadding.y * 2};
    main_size_ = fromImVec2<int>(window_->io_->DisplaySize) - main_offset_;
  }
  void processUI() {
    processPanelResize();
    processMainMenuBar();
    processPanels();
  };
  int exec() {
    while(!done_) {
      window_->newFrame();
      setMainContentRect();
      processUI();
      window_->render();
      for (auto& command : commands_) command();
      commands_ = {};
      done_ = done_ || window_->shouldClose();
    }
    return 0;
  };
};

void App::processMainMenuBar() {
  if (ImGui::BeginMainMenuBar()) {
    if (ImGui::BeginMenu("Menu")) {
      if (panel_manager_.empty()) {
        if (ImGui::MenuItem("Add Panel")) {
          commands_.emplace_back([&]() {
            panel_manager_.addToRoot();
          });
        };
      } else {
        if (ImGui::BeginMenu("Add panel")) {
          if (ImGui::MenuItem("Horizontal")) {
            commands_.emplace_back([&]() {
              panel_manager_.addToRoot(kdtree::SplitType::HORIZONTAL);
            });
          }
          if (ImGui::MenuItem("Vertical")) {
            commands_.emplace_back([&]() {
              panel_manager_.addToRoot(kdtree::SplitType::VERTICAL);
            });
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

void App::processPanel(Leaf& leaf) {
  auto name = leaf.value_.data();
  if (ImGui::BeginMenuBar()) {
    if (ImGui::BeginMenu(name)) {
      if (ImGui::BeginMenu("Split")) {
        if (ImGui::MenuItem("Horizontal")) {
          commands_.emplace_back([&]() {
            panel_manager_.splitNewNextToId(leaf.value_, kdtree::SplitType::HORIZONTAL);
          });
        }
        if (ImGui::MenuItem("Vertical")) {
          commands_.emplace_back([&]() {
            panel_manager_.splitNewNextToId(leaf.value_, kdtree::SplitType::VERTICAL);
          });
        }
        ImGui::EndMenu();
      }
      if (ImGui::MenuItem("Close")) {
        commands_.emplace_back([&]() {
          panel_manager_.removeById(leaf.value_);
        });
      }
      ImGui::EndMenu();
    }
    ImGui::EndMenuBar();
  }
  ImGui::Text("-- Panel Content Here --");
}

void App::processPanelResize() {
  // TODO:
  // - when mouse moves too fast and it goes further than `hit_margin`, resize dragging will stop.
  //   so probably it needs more explicit "focus state" tracking.
  ivec2 input = fromImVec2<int>(window_->io_->MousePos);
  ivec2 hit_margin = {10, 10};
  auto result = panel_manager_.panels_.hitTestSeparator(input - main_offset_, hit_margin, main_size_);
  if (result) {
    Branch* hit_branch;
    float new_fraction;
    std::tie(hit_branch, new_fraction) = result.value();
    switch (hit_branch->split_type_) {
      case kdtree::SplitType::HORIZONTAL : {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        break;
      }
      case kdtree::SplitType::VERTICAL : {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
        break;
      }
    }
    // Resize if mousedown
    if (window_->io_->MouseDown[0]) {
      hit_branch->fraction_ = new_fraction;
    }
  }
}

void App::processPanels() {
  panel_manager_.panels_.forEachLeaf(main_size_, [&](Leaf& leaf, ivec2 offset, ivec2 size) {
    ImVec2 _offset = toImVec2(offset + main_offset_);
    ImVec2 _size = toImVec2(size);
    ImGui::SetNextWindowPos(_offset);
    ImGui::SetNextWindowSize(_size);
    auto flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoResize;
    auto name = leaf.value_.data();
    ImGui::Begin(name, nullptr, flags);
      // "Panel" only deal with content of ImGui window.
      processPanel(leaf);
      ImGui::End();
  });
}

int main(const int argc, const char* argv[]) {
  App app{};
  return app.exec();
}
