#pragma once

#include <string>
#include <map>
#include <variant>

#include <fmt/format.h>
#include <imgui.h>
#include <imgui_scoped.h>

#include "window.hpp"
#include "kdtree.hpp"
#include "utils.hpp"


namespace toy {

using namespace utils;

struct Panel {
  std::string type_;
  std::string id_;
  std::string name_;
  ivec2 offset_;
  ivec2 size_;

  // layout info without menubar and "imgui window" padding
  ivec2 content_offset_;
  ivec2 content_size_;

  // push/pop before/after ImGui::Begin of panel (useful for ImGuiStyleVar_WindowPadding)
  std::vector<std::pair<ImGuiStyleVar, std::variant<float, ImVec2>>> style_vars_;

  virtual ~Panel() {}
  virtual void processUI() {}
  virtual void processMenu() {}

  void _pushStyleVars() {
    for (auto& [var, value] : style_vars_) {
      if (std::holds_alternative<ImVec2>(value)) {
        ImGui::PushStyleVar(var, std::get<ImVec2>(value));
      } else {
        ImGui::PushStyleVar(var, std::get<float>(value));
      }
    }
  }

  void _popStyleVars() {
    for (auto& [var, value] : style_vars_) {
      ImGui::PopStyleVar();
    }
  }
};

struct DefaultPanel : Panel {
  constexpr static const char* type = "Default Panel";
  static Panel* newPanelFunc() { return dynamic_cast<Panel*>(new DefaultPanel); }

  void processUI() override {
    ImGui::Text("-- Example Panel Content --");
  }
};


struct PanelManager {
  using PanelId = std::string;
  using Tree = kdtree::Tree<PanelId>;
  using Leaf = kdtree::Leaf<PanelId>;
  using Branch = kdtree::Branch<PanelId>;
  using Root = kdtree::Root<PanelId>;
  using Command = std::function<void()>;
  Root layout_;
  std::map<PanelId, std::unique_ptr<Panel>> panels_;
  int counter_ = 0;

  using PanelTypeId = std::string;
  using new_panel_func_t = std::function<Panel*()>;
  std::map<std::string, new_panel_func_t> panel_type_map_;

  const Window& window_;
  std::vector<Command> command_queue_;

  ivec2 content_offset_;
  ivec2 content_size_;

  struct ResizeContext {
    // Resize UI rules (cf. _processResize):
    // - false => true : MouseClicked[0] and hiTestSeparator true
    // - true => false : !MouseDown[0] anytime
    // - show resize cursor : resizing or hiTestSeparator true
    bool resizing = false;
    bool hovoring = false;
    Branch* branch;
  };
  ResizeContext resize_context_;

  template<typename T>
  void registerPanelType() {
    panel_type_map_[T::type] = T::newPanelFunc;
  }

  template<typename T>
  void registerPanelType(new_panel_func_t&& new_panel_func) {
    panel_type_map_[T::type] = std::move(new_panel_func);
  }

  PanelManager(Window& window) : window_{window} {
    registerPanelType<DefaultPanel>();
  }

  void _addCommand(Command&& command) {
    command_queue_.emplace_back(std::move(command));
  }

  //
  // KDTree
  //

  Panel* _newPanel(const PanelId& id, const PanelTypeId& panel_type) {
    auto panel = panel_type_map_[panel_type]();
    panel->id_ = id;
    panel->name_ = panel_type;
    panel->type_ = panel_type;
    return panel;
  }

  Leaf* _newLeaf(const PanelTypeId& panel_type = DefaultPanel::type) {
    counter_++;
    PanelId id = fmt::format("{}", counter_);
    auto leaf = new Leaf{id};
    assert(panels_.find(id) == panels_.end());
    panels_[id].reset(_newPanel(id, panel_type));
    return leaf;
  }

  void addPanelToRoot(
      kdtree::SplitType split_type = kdtree::SplitType::HORIZONTAL,
      const PanelTypeId& panel_type = DefaultPanel::type,
      float fraction = 0.5) {
    layout_.insertRoot(_newLeaf(panel_type), split_type, fraction, kdtree::ChildIndex::SECOND);
  }

  void changePanelType(const PanelId& id, PanelTypeId panel_type) {
    panels_.at(id).reset(_newPanel(id, panel_type));
  }

  std::function<bool(Tree&)> _makeLeafFinder(const PanelId& id) {
    return [&](Tree& tree) {
      auto leaf = dynamic_cast<Leaf*>(&tree);
      return leaf && leaf->value_ == id;
    };
  }

  void splitPanelWithNewPanel(const PanelId& id, kdtree::SplitType split_type) {
    auto inserted = layout_.insertNextTo(_makeLeafFinder(id), _newLeaf(), split_type, 0.5, kdtree::ChildIndex::SECOND);
    assert(inserted);
  }

  void removePanel(const PanelId& id) {
    auto removed = layout_.removeIf(_makeLeafFinder(id));
    assert(removed);
    panels_.erase(panels_.find(id));
  }

  //
  // UI
  //

  void processPanelManagerMenuItems() {
    if (!layout_.root_) {
      if (ImGui::MenuItem("Add Panel")) {
        _addCommand([&]() { addPanelToRoot(); });
      };
    } else {
      if (auto _ = ImScoped::Menu("Add panel")) {
        if (ImGui::MenuItem("Horizontal")) {
          _addCommand([&]() { addPanelToRoot(kdtree::SplitType::HORIZONTAL); });
        }
        if (ImGui::MenuItem("Vertical")) {
          _addCommand([&]() { addPanelToRoot(kdtree::SplitType::VERTICAL); });
        }
      }
    }
  }

  void processPanelMenu(Panel& panel) {
    if (auto _ = ImScoped::MenuBar()) {
      if (auto _ = ImScoped::Menu(panel.name_.data())) {
        if (auto _ = ImScoped::Menu("Split")) {
          if (ImGui::MenuItem("Horizontal")) {
            _addCommand([&]() {
                splitPanelWithNewPanel(panel.id_, kdtree::SplitType::HORIZONTAL); });
          }
          if (ImGui::MenuItem("Vertical")) {
            _addCommand([&]() {
                splitPanelWithNewPanel(panel.id_, kdtree::SplitType::VERTICAL); });
          }
        }
        if (auto _ = ImScoped::Menu("Change to")) {
          for (auto& pair : panel_type_map_) {
            auto& type_name = pair.first;
            if (ImGui::MenuItem(type_name.data(), nullptr, /*selected*/ panel.type_ == type_name)) {
              if (panel.type_ != type_name) {
                _addCommand([&]() { changePanelType(panel.id_, type_name); });
              }
            }
          }
        }
        if (ImGui::MenuItem("Close")) {
          _addCommand([&]() { removePanel(panel.id_); });
        }
      }
      panel.processMenu();
    }
  }

  void _processResize() {
    ivec2 input = fromImVec2<int>(window_.io_->MousePos);
    ivec2 hit_margin = {5, 5};
    auto result = layout_.hitTestSeparator(input - content_offset_, hit_margin, content_size_);

    if (!window_.io_->MouseDown[0]) {
      resize_context_.resizing = false;
    }

    if (resize_context_.resizing || result) {
      Branch* branch;
      if (resize_context_.resizing) branch = resize_context_.branch;
      if (result)                   branch = result->first;
      switch (branch->split_type_) {
        case kdtree::SplitType::HORIZONTAL : {
          ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
          break;
        }
        case kdtree::SplitType::VERTICAL : {
          ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
          break;
        }
      }
      resize_context_.hovoring = true;
    } else {
      resize_context_.hovoring = false;
    }

    if (resize_context_.resizing) {
      auto branchRect = layout_.getTreeRect(content_size_, [&](Tree& tree) {
        auto branch = dynamic_cast<Branch*>(&tree);
        return branch && branch == resize_context_.branch;
      });
      assert(branchRect);
      auto [offset, size] = *branchRect;
      auto i = (int)resize_context_.branch->split_type_;
      auto new_fraction = (input - content_offset_ - offset)[i] / (float)size[i];
      if (0.05 < new_fraction && new_fraction < 0.95) {
        resize_context_.branch->fraction_ = new_fraction;
      }

    } else if (window_.io_->MouseClicked[0] && result) {
      resize_context_.resizing = true;
      resize_context_.branch = result->first;
    }
  }

  void _processPanels() {
    layout_.forEachLeaf(content_size_, [&](Leaf& leaf, ivec2 offset, ivec2 size) {
      ImVec2 _offset = toImVec2(offset + content_offset_);
      ImVec2 _size = toImVec2(size);
      ImGui::SetNextWindowPos(_offset);
      ImGui::SetNextWindowSize(_size);
      auto flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_MenuBar |
                   ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_HorizontalScrollbar;
      if (resize_context_.hovoring) {
        flags |= ImGuiWindowFlags_NoMouseInputs;
      }
      auto& panel = panels_.at(leaf.value_);
      panel->offset_ = offset;
      panel->size_ = size;
      panel->_pushStyleVars();
      if (auto _ = ImScoped::Window(panel->id_.data(), nullptr, flags)) {
        panel->_popStyleVars();
        // Directly probe ImGuiWindow about layout info
        auto this_imgui_window = ImGui::GetCurrentWindowRead();
        auto window_padding = fromImVec2<int>(this_imgui_window->WindowPadding);
        panel->content_offset_ = ivec2{0, this_imgui_window->MenuBarHeight()} + window_padding;
        panel->content_size_ = size - panel->content_offset_ - window_padding;
        processPanelMenu(*panel);
        panel->processUI();
      }
    });
  }

  void _setDefaultContentRect() {
    auto main_menu_bar = ImGui::FindWindowByName("##MainMenuBar");
    content_offset_ = {0, main_menu_bar ? main_menu_bar->Size.y : 0};
    content_size_ = fromImVec2<int>(window_.io_->DisplaySize) - content_offset_;
  }

  void processUI() {
    _setDefaultContentRect();
    _processResize();
    _processPanels();
  }

  void processUI(const ivec2& content_offset, const ivec2& content_size) {
    content_offset_ = content_offset;
    content_size_ = content_size;
    _processResize();
    _processPanels();
  }

  void processPostUI() {
    for (auto& command : command_queue_) command();
    command_queue_ = {};
  }
};

} // namespace toy
