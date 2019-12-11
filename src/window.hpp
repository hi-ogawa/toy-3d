// c++
#include <string>

// others
#include <GL/gl3w.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <glm/glm.hpp>


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
  ivec2 size_;
  ivec2 fb_size_;

  Window(const char* name, ivec2 size, bool hit_maximized = true) : name_{name}, size_{size} {
    // Create GLFW window
    assert(glfwInit());
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, gl_version_major);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, gl_version_minor);
    glfwWindowHint(GLFW_MAXIMIZED, hit_maximized ? GLFW_TRUE : GLFW_FALSE);

    glfw_window_ = glfwCreateWindow(size_[0], size_[1], name_.data(), NULL, NULL);
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

    glfwGetWindowSize(glfw_window_, &size_[0], &size_[1]);
    glfwGetFramebufferSize(glfw_window_, &fb_size_[0], &fb_size_[1]);

    glViewport(0, 0, fb_size_[0], fb_size_[1]);
    glClear(GL_COLOR_BUFFER_BIT);
  }

  void render() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(glfw_window_);
  }

  // Should borrow ImGui's convinient data directly
  ImGuiIO& getIO() {
    // ImVec2      MousePos;      // Mouse position, in pixels. Set to ImVec2(-FLT_MAX,-FLT_MAX) if mouse is unavailable (on another screen, etc.)
    // bool        MouseDown[5];  // Mouse buttons: 0=left, 1=right, 2=middle + extras. ImGui itself mostly only uses left button (BeginPopupContext** are using right button). Others buttons allows us to track if the mouse is being used by your application + available to user as a convenience via IsMouse** API.
    // float       MouseWheel;    // Mouse wheel Vertical: 1 unit scrolls about 5 lines text.
    // float       MouseWheelH;   // Mouse wheel Horizontal. Most users don't have a mouse with an horizontal wheel, may not be filled by all back-ends.
    // bool        KeyCtrl;       // Keyboard modifier pressed: Control
    // bool        KeyShift;      // Keyboard modifier pressed: Shift
    // bool        KeyAlt;        // Keyboard modifier pressed: Alt
    // bool        KeySuper;      // Keyboard modifier pressed: Cmd/Super/Windows
    // bool        KeysDown[512]; // Keyboard keys that are pressed (ideally left in the "native" order your engine has access to keyboard keys, so you can use your own defines/enums for keys).
    return ImGui::GetIO();
  }
};


} // namespace toy
