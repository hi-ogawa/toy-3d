#pragma once

// c++
#include <string>

// others
#include <GL/gl3w.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <glm/glm.hpp>
#include <material_icons.h> // MD_FOR_EACH_ICON, MD_ICON_MIN, MD_ICON_MAX
#include "utils.hpp"


namespace toy {

using glm::ivec2;

struct WindowInitParams {
  // gl
  bool gl_debug = false;
  const char* glsl_version = "#version 330";
  int gl_version_major = 3;
  int gl_version_minor = 3;

  // glfw
  bool hint_maximized = false;

  // imgui
  const char* imgui_font = TOY_PATH("thirdparty/imgui/misc/fonts/Roboto-Medium.ttf");
  const char* imgui_icon_font = TOY_PATH("thirdparty/material-design-icons/MaterialIcons-Regular.ttf");
};

struct Window {
  GLFWwindow* glfw_window_;
  std::string name_;
  ImGuiContext* imgui_context_;
  ImGuiIO* io_; // it tracks MousePos, DisplaySize, etc...
  std::optional<std::function<void(const std::vector<std::string>&)>> drop_callback_;
  bool wait_event_ = false;

  static void dropCallback(GLFWwindow* glfw_window, int path_count, const char* paths[]) {
    std::vector<std::string> _paths;
    for (auto i = 0; i < path_count; i++) {
      _paths.emplace_back(paths[i]);
    }
    auto _this = reinterpret_cast<Window*>(glfwGetWindowUserPointer(glfw_window));
    if (_this->drop_callback_) {
      (*_this->drop_callback_)(_paths);
      glfwFocusWindow(glfw_window);
    }
  }

  Window(const char* name, ivec2 size, WindowInitParams params = {}) : name_{name} {
    // Create GLFW window
    assert(glfwInit());
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, params.gl_version_major);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, params.gl_version_minor);
    glfwWindowHint(GLFW_MAXIMIZED, params.hint_maximized);

    glfw_window_ = glfwCreateWindow(size[0], size[1], name_.data(), NULL, NULL);
    assert(glfw_window_);
    glfwMakeContextCurrent(glfw_window_);
    glfwSwapInterval(1);

    // Handle drag&drop (ImGui_ImplGlfw_InitForOpenGL doesn't setup this)
    // NOTE: gnome/mutter has some issue cf. https://gitlab.gnome.org/GNOME/mutter/issues/845
    // NOTE: this only supports "local file path" data
    glfwSetWindowUserPointer(glfw_window_, reinterpret_cast<void*>(this));
    glfwSetDropCallback(glfw_window_, &Window::dropCallback);

    // Load GL
    assert(!gl3wInit());

    if (params.gl_debug) {
      utils::gl::enableDebugMessage();
    }

    // Setup ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;

    // My preference
    ImGui::GetIO().Fonts->AddFontFromFileTTF(params.imgui_font, 15.0f);
    ImGui::StyleColorsDark();
    ImGui::GetStyle().WindowRounding = 0;
    ImGui::GetStyle().TabRounding = 1;
    ImGui::GetStyle().ScrollbarRounding = 2;
    ImGui::GetStyle().ScrollbarSize = 10;

    // Setup icon font (cf. IconViewerPanel in panel_system_utils.hpp)
    ImFontConfig config;
    config.MergeMode = true;
    config.GlyphMinAdvanceX = 20.0f;
    static const ImWchar icon_ranges[] = { MD_ICON_MIN, MD_ICON_MAX, 0 };
    ImGui::GetIO().Fonts->AddFontFromFileTTF(params.imgui_icon_font, 15.0f, &config, icon_ranges);

    // Initialize ImGui backend
    ImGui_ImplGlfw_InitForOpenGL(glfw_window_, true);
    ImGui_ImplOpenGL3_Init(params.glsl_version);
    imgui_context_ = ImGui::GetCurrentContext();
    io_ = &imgui_context_->IO;
  }

  ~Window() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(glfw_window_);
    glfwTerminate();
  }

  bool shouldClose() {
    return glfwWindowShouldClose(glfw_window_);
  }

  void newFrame() {
    wait_event_ ? glfwWaitEvents() : glfwPollEvents();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    auto convert = utils::fromImVec2<int>;
    ivec2 size = convert(io_->DisplaySize) * convert(io_->DisplayFramebufferScale);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, size[0], size[1]);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
  }

  void render() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(glfw_window_);
  }
};


} // namespace toy
