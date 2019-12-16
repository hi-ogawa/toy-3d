#include <memory>

#include <fmt/format.h>
#include <imgui.h>
#include <glm/glm.hpp>
#include <stdexcept>

#include "window.hpp"
#include "panel_system.hpp"
#include "utils.hpp"

// References
// - https://www.khronos.org/opengl/wiki/Vertex_Post-Processing
// - https://www.khronos.org/opengl/wiki/GLAPI/glDepthRange
// - https://www.khronos.org/opengl/wiki/GLAPI/glCullFace

namespace toy {

struct Renderer {
  struct Framebuffer {
    TOY_CLASS_DELETE_COPY(Framebuffer)
    GLuint framebuffer_handle_, texture_handle_, depth_texture_handle_;
    ivec2 size_;

    Framebuffer(const ivec2& size) : size_{size} {
      glGenTextures(1, &texture_handle_);
      glBindTexture(GL_TEXTURE_2D, texture_handle_);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size_.x, size_.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

      glGenFramebuffers(1, &framebuffer_handle_);
      glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer_handle_);
      glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_handle_, 0);
      glDrawBuffer(GL_COLOR_ATTACHMENT0);

      glGenTextures(1, &depth_texture_handle_);
      glBindTexture(GL_TEXTURE_2D, depth_texture_handle_);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, size_.x, size_.y, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

      glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer_handle_);
      glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_texture_handle_, 0);
    }
    ~Framebuffer() {
      glDeleteTextures(1, &texture_handle_);
      glDeleteFramebuffers(1, &framebuffer_handle_);
    }
  };
  std::unique_ptr<Framebuffer> fb_;

  // Mesh data
  static inline glm::fvec3 vertices_[] = {
    // Unit cube
    // { 0, 0, 0 },
    // { 1, 0, 0 },
    // { 1, 1, 0 },
    // { 0, 1, 0 },
    // { 0, 0, 1 },
    // { 1, 0, 1 },
    // { 1, 1, 1 },
    // { 0, 1, 1 },

    // Arange vertices so that it shows 3 faces
    { 0, 0, .1 },
    {.5, 0, .1 },
    {.5,.5, .1 },
    { 0,.5, .1 },
    { 0, 0, .9 },
    {.9, 0, .9 },
    {.9,.9, .9 },
    { 0,.9, .9 },
  };

  static inline uint8_t indices_[] = {
    // CCW face in right-hand frame
    // #define QUAD_TO_TRIS(A, B, C, D) A, B, D, C, D, B,

    // CCW face in left-hand frame (e.g. OpenGL's "window" space where glCullFace applies)
    #define QUAD_TO_TRIS(A, B, C, D) D, B, A, B, D, C,
    QUAD_TO_TRIS(0, 3, 2, 1) // z = 0 plane
    QUAD_TO_TRIS(4, 5, 6, 7) // z = 1
    QUAD_TO_TRIS(0, 1, 5, 4) // y = 0
    QUAD_TO_TRIS(1, 2, 6, 5) // x = 1
    QUAD_TO_TRIS(2, 3, 7, 6) // y = 1
    QUAD_TO_TRIS(3, 0, 4, 7) // x = 0
    #undef QUAD_TO_TRIS
  };

  static inline const char* vertex_shader_source = R"(
#version 410
layout (location = 0) in vec3 vertex_;
out vec4 interp_color_;
void main() {
  interp_color_ = vec4(vertex_, 1);
  gl_Position = vec4(vertex_, 1);
}
)";

  static inline const char* fragment_shader_source = R"(
#version 410
in vec4 interp_color_;
layout (location = 0) out vec4 out_color_;
void main() {
  out_color_ = interp_color_;
}
)";

  std::unique_ptr<utils::gl::Program> program_;
  GLuint array_buffer_, element_array_buffer_, vertex_array_;

  Renderer(const ivec2& size) {
    fb_.reset(new Framebuffer{size});
    program_.reset(new utils::gl::Program{vertex_shader_source, fragment_shader_source});

    // Setup vertex data
    glGenBuffers(1, &array_buffer_);
    glGenBuffers(1, &element_array_buffer_);
    glBindBuffer(GL_ARRAY_BUFFER,         array_buffer_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, element_array_buffer_);
    glBufferData(GL_ARRAY_BUFFER,         (sizeof vertices_), vertices_, GL_STREAM_DRAW);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (sizeof indices_),  indices_,  GL_STREAM_DRAW);

    // Configure vertex format
    glGenVertexArrays(1, &vertex_array_);
    glBindVertexArray(vertex_array_);
    glBindBuffer(GL_ARRAY_BUFFER, array_buffer_);
    auto location = glGetAttribLocation(program_->handle_, "vertex_");
    glEnableVertexAttribArray(location);
    glVertexAttribPointer(location, 3, GL_FLOAT, GL_FALSE, sizeof(glm::fvec3), (GLvoid*)0);
  }

  void draw() {
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fb_->framebuffer_handle_);
    glViewport(0, 0, fb_->size_[0], fb_->size_[1]);

    // rendering configuration
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);

    // clear buffer
    glm::fvec4 color = {.4, .4, .4, 1};
    float depth = 1;
    glClearBufferfv(GL_COLOR, 0, (GLfloat*)&color);
    glClearBufferfv(GL_DEPTH, 0, (GLfloat*)&depth);

    // draw
    glUseProgram(program_->handle_);
    glBindVertexArray(vertex_array_);
    glBindBuffer(GL_ARRAY_BUFFER, array_buffer_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, element_array_buffer_);
    glDrawElements(GL_TRIANGLES, (sizeof indices_), GL_UNSIGNED_BYTE, 0);
  }
};

struct RenderPanel : Panel {
  constexpr static const char* type = "Render Panel";
  const Renderer& renderer_;

  RenderPanel(Renderer& renderer) : renderer_{renderer} {}

  void processUI() override {
    ImGui::Text("Render Result (size = (%d, %d))", renderer_.fb_->size_[0], renderer_.fb_->size_[1]);
    ImGui::Image(
        reinterpret_cast<ImTextureID>(renderer_.fb_->texture_handle_),
        toImVec2(renderer_.fb_->size_),
        /* uv0 */ {0, 1}, /* uv1 */ {1, 0}); // framebuffer's pixel is ordered from left-bottom.
  }
};

struct App {
  std::unique_ptr<toy::Window> window_;
  std::unique_ptr<PanelManager> panel_manager_;
  std::unique_ptr<Renderer> renderer_;
  bool done_ = false;

  App() {
    window_.reset(new Window{"My Window", {800, 600}, { .gl_debug = true }});
    renderer_.reset(new Renderer{ivec2{512, 512}});

    panel_manager_.reset(new PanelManager{*window_});
    panel_manager_->registerPanelType(RenderPanel::type, [&]() { return new RenderPanel{*renderer_}; });
    panel_manager_->addPanelToRoot(kdtree::SplitType::HORIZONTAL, RenderPanel::type);
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
      renderer_->draw();
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
