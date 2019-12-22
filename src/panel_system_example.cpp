#include <memory>

#include <fmt/format.h>
#include <imgui.h>
#include <glm/glm.hpp>

#include "window.hpp"
#include "panel_system.hpp"
#include "panel_system_utils.hpp"
#include "utils.hpp"


namespace toy {

using namespace utils;

struct App {
  std::unique_ptr<toy::Window> window_;
  std::unique_ptr<PanelManager> panel_manager_;
  bool done_ = false;

  App() {
    window_.reset(new Window{"My Window", {800, 600}, { .hint_maximized = true }});
    panel_manager_.reset(new PanelManager{*window_});
    panel_manager_->registerPanelType<StyleEditorPanel>();
    panel_manager_->registerPanelType<MetricsPanel>();
    panel_manager_->registerPanelType<DemoPanel>();
    panel_manager_->registerPanelType<TestPanel>();
    panel_manager_->addPanelToRoot(kdtree::SplitType::HORIZONTAL, DemoPanel::type);
    panel_manager_->addPanelToRoot(kdtree::SplitType::VERTICAL, StyleEditorPanel::type);
    panel_manager_->addPanelToRoot(kdtree::SplitType::HORIZONTAL, TestPanel::type);
  }

  void processMainMenuBar() {
    auto _ = ImScoped::StyleVar(ImGuiStyleVar_FramePadding, {4, 6});
    if (auto _ = ImScoped::MainMenuBar()) {
      if (auto _ = ImScoped::Menu("Menu")) {
        panel_manager_->processPanelManagerMenuItems();
        if (ImGui::MenuItem("Quit")) {
          done_ = true;
        }
      }
      if (auto _ = ImScoped::Menu("Window")) {
        if (ImGui::MenuItem("WaitEvents", nullptr, /*selected*/ window_->wait_event_)) {
          window_->wait_event_ = !window_->wait_event_;
        }
      }
    }
  }

  void processUI() {
    processMainMenuBar();
    panel_manager_->processUI();
  }

  int exec() {
    while(!done_) {
      window_->newFrame();
      processUI();
      panel_manager_->processPostUI();
      window_->render();
      done_ = done_ || window_->shouldClose();
    }
    return 0;
  }
};

} // namespace toy


int main(const int argc, const char* argv[]) {
  toy::App app{};
  return app.exec();
}
