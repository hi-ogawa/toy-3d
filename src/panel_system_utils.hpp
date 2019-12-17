#include "panel_system.hpp"

namespace toy {

struct StyleEditorPanel : Panel {
  constexpr static const char* type = "Style Editor";
  static Panel* newPanelFunc() { return dynamic_cast<Panel*>(new StyleEditorPanel); }

  void processUI() override {
    ImGui::ShowStyleEditor();
  }
};

struct MetricsPanel : Panel {
  constexpr static const char* type = "Metrics";
  static Panel* newPanelFunc() { return dynamic_cast<Panel*>(new MetricsPanel); }

  void processUI() override {
    ImGui::ShowMetricsWindow(nullptr, /* no_window */ true);
  }
};

} // namespace toy
