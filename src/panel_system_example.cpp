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
  ivec2 main_menu_padding_ = {4, 6};
  bool done_ = false;

  App() {
    window_.reset(new Window{"My Window", {800, 600}});
    panel_manager_.reset(new PanelManager{*window_});
    panel_manager_->registerPanelType<StyleEditorPanel>();
    panel_manager_->registerPanelType<MetricsPanel>();
    panel_manager_->addPanelToRoot(kdtree::SplitType::HORIZONTAL, DefaultPanel::type);
    panel_manager_->addPanelToRoot(kdtree::SplitType::VERTICAL, StyleEditorPanel::type);
    panel_manager_->addPanelToRoot(kdtree::SplitType::HORIZONTAL, MetricsPanel::type);
  }

  void processMainMenuBar() {
    auto _ = ImScoped::StyleVar(ImGuiStyleVar_FramePadding, toImVec2(main_menu_padding_));
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

    // NOTE:
    // you can give the exact content offset/size to `PanelManager`,
    // which is necessary, for example, when you change main menu bar's style.
    auto g = window_->imgui_context_;
    ivec2 content_offset = {0, g->FontBaseSize + main_menu_padding_.y * 2};
    ivec2 content_size = fromImVec2<int>(window_->io_->DisplaySize) - content_offset;
    panel_manager_->processUI(content_offset, content_size);
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
