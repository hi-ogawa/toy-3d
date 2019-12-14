#include <memory>

#include <fmt/format.h>
#include <imgui.h>
#include <glm/glm.hpp>

#include "window.hpp"
#include "panel_system.hpp"
#include "utils.hpp"


namespace toy {

using namespace utils;

struct StyleEditorPanel : Panel {
  void processUI() override {
    ImGui::ShowStyleEditor();
  }
  void processMenu() override {}
};

struct MetricsPanel : Panel {
  void processUI() override {
    ImGui::ShowMetricsWindow(nullptr, /* no_window */ true);
  }
};

struct App {
  std::unique_ptr<toy::Window> window_;
  std::unique_ptr<PanelManager> panel_manager_;
  bool done_ = false;

  App() {
    window_.reset(new Window{"My Window", {800, 600}});
    panel_manager_.reset(new PanelManager{*window_});
    panel_manager_->registerPanelType("Style Editor", [](){ return new StyleEditorPanel; });
    panel_manager_->registerPanelType("Metrics", [](){ return new MetricsPanel; });
    panel_manager_->addPanelToRoot(kdtree::SplitType::HORIZONTAL, DefaultPanel::panel_type_);
    panel_manager_->addPanelToRoot(kdtree::SplitType::VERTICAL, "Style Editor");
    panel_manager_->addPanelToRoot(kdtree::SplitType::HORIZONTAL, "Metrics");
  }

  void processMainMenuBar() {
    if (auto _ = ImScoped::MainMenuBar()) {
      if (auto _ = ImScoped::Menu("Menu")) {
        panel_manager_->processPanelManagerMenuItems();
        if (ImGui::MenuItem("Quit")) {
          done_ = true;
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
      panel_manager_->newFrame();
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
