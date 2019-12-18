#include "panel_system.hpp"

// Demo components from imgui_demo.cpp (`static` is removed for make them external linkage)
void ShowDemoWindowWidgets();
void ShowDemoWindowLayout();
void ShowDemoWindowPopups();
void ShowDemoWindowColumns();
void ShowDemoWindowMisc();

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

struct DemoPanel : Panel {
  constexpr static const char* type = "Demo";
  static Panel* newPanelFunc() { return dynamic_cast<Panel*>(new DemoPanel); }

  void processUI() override {
    ShowDemoWindowWidgets();
    ShowDemoWindowLayout();
    ShowDemoWindowPopups();
    ShowDemoWindowColumns();
    ShowDemoWindowMisc();
  }
};

} // namespace toy
