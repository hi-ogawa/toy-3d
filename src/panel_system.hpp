#pragma once

#include <string>
#include <map>

#include <fmt/format.h>
#include <imgui.h>
#include <imgui_scoped.h>

#include "window.hpp"
#include "kdtree.hpp"
#include "utils.hpp"


namespace toy {

using namespace utils;

struct Panel {
  std::string id_;
  std::string name_;

  virtual ~Panel() {}

  // TODO: consider interface (what should be the argument)
  virtual void processUI() {}
  virtual void processMenu() {}
};

struct DefaultPanel : Panel {
  static inline std::string panel_type_ = "Default Panel";

  void processUI() override {
    ImGui::Text("-- Default Panel Content --");
  }
};


struct PanelManager {
  using PanelId = std::string;
  using Tree = kdtree::Tree<PanelId>;
  using Leaf = kdtree::Leaf<PanelId>;
  using Branch = kdtree::Leaf<PanelId>;
  using Root = kdtree::Root<PanelId>;
  using Command = std::function<void()>;
  Root layout_;
  std::map<PanelId, std::unique_ptr<Panel>> panels_;
  int counter_ = 0;

  using PanelTypeId = std::string;
  using panel_new_func_t = std::function<Panel*()>;
  std::map<std::string, panel_new_func_t> panel_types_;

  const Window& window_;
  std::vector<Command> command_queue_;

  ivec2 content_offset_;
  ivec2 content_size_;

  PanelManager(Window& window) : window_{window} {
    registerPanelType(
        DefaultPanel::panel_type_,
        []() { return dynamic_cast<Panel*>(new DefaultPanel); });
  }

  void _addCommand(Command&& command) {
    command_queue_.emplace_back(std::move(command));
  }

  void registerPanelType(const PanelTypeId& panel_type, const panel_new_func_t& new_func) {
    panel_types_[panel_type] = new_func;
  }

  //
  // KDTree
  //

  Panel* _newPanel(const PanelId& id, const PanelTypeId& panel_type) {
    auto panel = panel_types_[panel_type]();
    panel->id_ = id;
    panel->name_ = panel_type;
    return panel;
  }

  Leaf* _newLeaf(const PanelTypeId& panel_type = DefaultPanel::panel_type_) {
    counter_++;
    PanelId id = fmt::format("{}", counter_);
    auto leaf = new Leaf{id};
    assert(panels_.find(id) == panels_.end());
    panels_[id].reset(_newPanel(id, panel_type));
    return leaf;
  }

  void addPanelToRoot(kdtree::SplitType split_type = kdtree::SplitType::HORIZONTAL) {
    layout_.insertRoot(_newLeaf(), split_type, 0.5, kdtree::ChildIndex::SECOND);
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
      auto menu_label = fmt::format("{} ({})", panel.name_, panel.id_);
      if (auto _ = ImScoped::Menu(menu_label.data())) {
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
          for (auto& type : panel_types_) {
            auto& name = type.first;
            if (ImGui::MenuItem(name.data())) {
              _addCommand([&]() { changePanelType(panel.id_, name); });
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
    ivec2 hit_margin = {10, 10};
    auto result = layout_.hitTestSeparator(input - content_offset_, hit_margin, content_size_);
    if (result) {
      auto [hit_branch, new_fraction] = *result;
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
      if (window_.io_->MouseDown[0]) {
        hit_branch->fraction_ = new_fraction;
      }
    }
  }

  void _processPanels() {
    layout_.forEachLeaf(content_size_, [&](Leaf& leaf, ivec2 offset, ivec2 size) {
      ImVec2 _offset = toImVec2(offset + content_offset_);
      ImVec2 _size = toImVec2(size);
      ImGui::SetNextWindowPos(_offset);
      ImGui::SetNextWindowSize(_size);
      auto flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
      auto panelId = leaf.value_;
      auto& panel = panels_.at(panelId);
      if (auto _ = ImScoped::Window(panelId.data(), nullptr, flags)) {
        processPanelMenu(*panel);
        panel->processUI();
      }
    });
  }

  void newFrame() {
    auto g = window_.imgui_context_;
    content_offset_ = {0, g->FontBaseSize + g->Style.FramePadding.y * 2};
    content_size_ = fromImVec2<int>(window_.io_->DisplaySize) - content_offset_;
  }

  void processUI() {
    _processResize();
    _processPanels();
  }

  void processPostUI() {
    for (auto& command : command_queue_) command();
    command_queue_ = {};
  }
};

} // namespace toy
