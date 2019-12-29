#include "panel_system.hpp"
#include "utils.hpp"
#include "utils_imgui.hpp"

// Demo components from imgui_demo.cpp (`static` is removed to make them external linkage)
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

struct IconViewerPanel : Panel {
  constexpr static const char* type = "Icon Viewer";
  static Panel* newPanelFunc() { return dynamic_cast<Panel*>(new IconViewerPanel); }

  #define _GET_NAME(NAME, CHAR) #NAME,
  #define _GET_CHAR(NAME, CHAR)  CHAR,
  inline static const char* icon_names[] = { MD_FOR_EACH_ICON(_GET_NAME) };
  inline static const char* icon_chars[] = { MD_FOR_EACH_ICON(_GET_CHAR) };
  #undef _GET_NAME
  #undef _GET_CHAR

  ImGuiTextFilter filter_;

  void processUI() override {
    filter_.Draw("Filter");
    for (int i = 0; i < IM_ARRAYSIZE(icon_names); i++)
      if (filter_.PassFilter(icon_names[i]))
        ImGui::BulletText("%s: %s", icon_chars[i], icon_names[i]);
  }
};

struct TestPanel : Panel {
  constexpr static const char* type = "Test";
  static Panel* newPanelFunc() { return dynamic_cast<Panel*>(new TestPanel); }
  void processUI() override {
    auto& ctx = utils::imgui::global_camera_view_experiment_context_;
    ctx.viewport[0] = ImGui::GetWindowContentRegionWidth();
    ctx.viewport[1] = content_size_.y - ImGui::GetFrameHeightWithSpacing() * 5;
    utils::imgui::CameraViewExperiment();
  }
  void processMenu() override {
    if (auto _ = ImScoped::Menu("Edit")) {
      if (ImGui::MenuItem("Reset")) {
        utils::imgui::global_camera_view_experiment_context_ = {};
      }
    }
  }
};

struct xxxTestPanel : Panel {
  constexpr static const char* type = "xxxTest";
  static Panel* newPanelFunc() { return dynamic_cast<Panel*>(new xxxTestPanel); }

  struct DropContext1 {
    bool dropping = false;
    int target = -1;
    int source = 0;
  } ctx1_;

  struct DropContext2 {
    bool dragging = false;
    int target = -1;
    int source = 0;
  } ctx2_;

  void processUI() override {
    //
    // Example 1
    // - Explicitly keep dragging state by yourself
    //
    if (auto _ = ImScoped::TreeNodeEx("Drag&Drop Test 1", ImGuiTreeNodeFlags_DefaultOpen)) {
      if (ImGui::ButtonEx("Start!", ImVec2(100, 50), ImGuiButtonFlags_PressedOnClick)) {
        ctx1_.dropping = true;
        ctx1_.source += 1;
      }

      if (!ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0)) {
        ctx1_.dropping = false;
      }

      if (ctx1_.dropping) {
        ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceExtern);
        ImGui::SetDragDropPayload("CUSTOM1", &ctx1_.source, sizeof(int));
        ImGui::Text("source = %d", ctx1_.source);
        ImGui::EndDragDropSource();
      }

      ImGui::Button(fmt::format("target = {}", ctx1_.target).data(), ImVec2(100, 50));
      if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CUSTOM1")) {
            TOY_ASSERT(payload->DataSize == sizeof(int));
            ctx1_.target = *(const int*)payload->Data;
            ctx1_.dropping = false;
        }
        ImGui::EndDragDropTarget();
      }
    }

    //
    // Example 2
    // - Start "drag" by click and also "drop" by click (i.e. not really "Drag&Drop" anymore)
    //
    if (auto _ = ImScoped::TreeNodeEx("Drag&Drop Test 2", ImGuiTreeNodeFlags_DefaultOpen)) {
      auto __ = ImScoped::ID("2");
      if (ImGui::Button("Start!", ImVec2(100, 50))) {
        ctx2_.dragging = true;
        ctx2_.source += 1;
      }

      if (ImGui::IsKeyPressedMap(ImGuiKey_Escape)) {
        ctx2_.dragging = false;
      }

      if (ctx2_.dragging) {
        ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceExtern);
        ImGui::SetDragDropPayload("CUSTOM2", &ctx2_.source, sizeof(int));
        ImGui::Text("source = %d", ctx2_.source);
        ImGui::Text("<Escape> to cancel");
        ImGui::EndDragDropSource();
      }

      // ButtonEx's `pressed` return value gets some false positive, so use `IsItemClicked`.
      ImGui::ButtonEx(fmt::format("target = {}", ctx2_.target).data(), ImVec2(100, 50));
      auto clicked = ImGui::IsItemClicked();

      if (ImGui::BeginDragDropTarget()) {
        // By default, `AcceptDragDropPayload` only returns value when !mousedown.
        // here `ImGuiDragDropFlags_AcceptBeforeDelivery` forces it to always return payload if any.
        auto const_payload = ImGui::AcceptDragDropPayload("CUSTOM2", ImGuiDragDropFlags_AcceptBeforeDelivery);
        auto payload = const_cast<struct ImGuiPayload*>(const_payload);
        if (payload) {
          // Overwrite `Delivery` in order to keep `DragDropActive` until button is clicked.
          payload->Delivery = false;
          if (clicked) {
            payload->Delivery = true;
            ctx2_.target = *(const int*)payload->Data;
            ctx2_.dragging = false;
          }
        }
        ImGui::EndDragDropTarget();
      }
    }
  }
};

} // namespace toy
