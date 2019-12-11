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
#include "misc.hpp"


namespace toy {

using glm::ivec2;

// constants
const char* glsl_version = "#version 130";
const int gl_version_major = 3;
const int gl_version_minor = 0;

// TODO: better resource management
const char* font_filename = "./thirdparty/imgui/misc/fonts/Roboto-Medium.ttf";

struct Window {
  GLFWwindow* glfw_window_;
  std::string name_;
  ImGuiContext* imgui_context_;
  ImGuiIO* io_; // it tracks MousePos, DisplaySize, etc...

  Window(const char* name, ivec2 size, bool hit_maximized = false) : name_{name} {
    // Create GLFW window
    assert(glfwInit());
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, gl_version_major);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, gl_version_minor);
    glfwWindowHint(GLFW_MAXIMIZED, hit_maximized ? GLFW_TRUE : GLFW_FALSE);

    glfw_window_ = glfwCreateWindow(size[0], size[1], name_.data(), NULL, NULL);
    assert(glfw_window_);
    glfwMakeContextCurrent(glfw_window_);
    glfwSwapInterval(1);

    // Load GL
    assert(!gl3wInit());

    // Setup ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    ImGui::GetIO().Fonts->AddFontFromFileTTF(font_filename, 15.0f);
    ImGui::GetStyle().WindowRounding = 0;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(glfw_window_, true);
    ImGui_ImplOpenGL3_Init(glsl_version);
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
    glfwPollEvents();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    auto convert = misc::fromImVec2<int>;
    ivec2 size = convert(io_->DisplaySize) * convert(io_->DisplayFramebufferScale);
    glViewport(0, 0, size[0], size[1]);
    glClear(GL_COLOR_BUFFER_BIT);
  }

  void render() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(glfw_window_);
  }
};


} // namespace toy
