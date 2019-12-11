// c
#include <cassert>

// c++
#include <string>
#include <vector>
#include <memory>

// other lib
#include <GL/gl3w.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <glm/glm.hpp>
#include <fmt/format.h>

#include "window.hpp"

namespace toy {

using glm::ivec2;
using std::string, std::vector, std::unique_ptr;

// Correspond to ImGuiWindow
struct Panel {
  string name_;
  ivec2 position_;
  ivec2 size_;
  // type (builtin type or custom type)

  Panel(const string& name)
    : name_{name}, position_{0, 0}, size_{0, 0} {
  }

  // TODO: should accept whole application state
  void process(float values[4]) {
    auto flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_MenuBar;
    // | ImGuiWindowFlags_NoResize
    ImGui::Begin(name_.data(), nullptr, flags);

    if (ImGui::BeginMenuBar()) {
      if (ImGui::BeginMenu(name_.data())) {
        if (ImGui::BeginMenu("Split panel")) {
          // TODO
          ImGui::MenuItem("Horizontal");
          ImGui::MenuItem("Vertical");
          ImGui::EndMenu();
        }
        ImGui::EndMenu();
      }
      ImGui::EndMenuBar();
    }

    processContent(values);
    ImGui::End();
  }

  void processContent(float values[4]) {
    static int counter = 0;
    ImGui::ColorEdit4("clear color", values); // Edit 3 floats representing a color
    if (ImGui::Button("Button")) {
      counter++;
    }
    ImGui::SameLine();
    ImGui::Text("counter = %d", counter);
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
  }
};

// struct ViewportPanel : Panel {
//   // open scene etc...
//   void process() {
//   }
// };

struct App {
  unique_ptr<Window> window_;
  vector<unique_ptr<Panel>> panels_;

  // state
  bool done_ = false;
  bool show_imgui_demo_ = true;
  float clear_color_[4] = {0, 0, 0, 1};
  // operations
  // QUIT, LOAD_SCENE, SPLIT_PANEL

  App() {
    window_.reset(new Window{"My Window", {800, 600}});
    addPanel();
    addPanel();
  }
  void addPanel() {
    auto name = fmt::format("Panel {}", panels_.size() + 1);
    panels_.push_back(std::make_unique<Panel>(name));
  }
  void layoutPanels() {
    // ImGuiMouseCursor_ResizeNS, EW
    // ImGui::SetMouseCursor(i)

    // cf. ImGui::UpdateManualResize
  }

  int exec() {
    while(!done_) {
      window_->newFrame();
      processUI();
      draw();
      window_->render();
      done_ = done_ || window_->shouldClose();
    }
    return 0;
  }

  void draw() {
    glClearColor(clear_color_[0], clear_color_[1], clear_color_[2], clear_color_[3]);
  }

  void processUI() {
    processMainMenu();

    for (auto& panel : panels_) {
      panel->process(clear_color_);
    }

    if (show_imgui_demo_) {
      ImGui::ShowDemoWindow();
    }
  }

  void processMainMenu() {
    if (ImGui::BeginMainMenuBar()) {
      if (ImGui::BeginMenu("Menu")) {
        if (ImGui::MenuItem("Add Panel")) {
          addPanel();
        };
        if (ImGui::MenuItem("Quit")) {
          done_ = true;
        };
        ImGui::EndMenu();
      }
      if (ImGui::BeginMenu("Help")) {
        if (ImGui::MenuItem("Shwo ImGui Demo", nullptr, show_imgui_demo_)) {
          show_imgui_demo_ = !show_imgui_demo_;
        }
        ImGui::EndMenu();
      }
      ImGui::EndMainMenuBar();
    }
  }
};

} // namespace toy


int main(const int argc, const char* argv[]) {
  toy::App app{};
  return app.exec();
}
