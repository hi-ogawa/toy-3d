#include <memory>
#include <iostream>
#include <stdexcept>

#include <fmt/format.h>
#include <imgui.h>
#include <glm/glm.hpp>
#include <imgui_scoped.h>

#include "window.hpp"
#include "panel_system.hpp"
#include "utils.hpp"


namespace toy {

using namespace utils;
using glm::ivec2, glm::fvec2;

// [x] drawing
//   - draw target (texture, frame buffer)
//   - clear color
// [x] setup shader program
//   - [x] small gl program wrapper
//   - [@] cube vertex array
// [x] OpenGL/gltf cordinate system
//   - right hand frame
//   - "-Z" camera lookat direction
//   - "+X" right
//   - "+Y" up
// [x] debug strategy
//   - try remove projection, transform, etc...
//   - directly specify vertex position
//   - [x] use default framebuffer (so that user-friendly default is setup out-of-box)
//   - [x] my fmat4 inverse might be wrong?
//   - [@] try point rasterization
// [x] mesh
//   - alloc vertex array
//   - transf
//   - draw program
//   - check gl's coordinate system (z depth direction)
// [x] camera
//   - params..
//   - transf
// [x] "transform" property editor
//   - [x] imgui
//   - [-] gizmo
// [@] organize scene system
//   - [@] immitate gltf data structure
//   - scene hierarchy
//   - render system (render resource vs render parameter)
//   - [ ] draw world axis and half planes
// [ ] rendering model
//   - https://github.com/KhronosGroup/glTF/blob/master/specification/2.0/README.md#appendix-b-brdf-implementation
// [ ] uv and texture map
// [ ] in general, just follow gltf's representation
//   - https://github.com/KhronosGroup/glTF/blob/master/specification/2.0/README.md


struct SimpleRenderer {
  struct Rotation {
    glm::fvec3 data_;
    glm::fmat3 ExtrinsicEulerXYZ_to_SO3() {
      auto x = data_[0] * 3.14 / 180., cx = std::cos(x), sx = std::sin(x);
      auto y = data_[1] * 3.14 / 180., cy = std::cos(y), sy = std::sin(y);
      auto z = data_[2] * 3.14 / 180., cz = std::cos(z), sz = std::sin(z);
      auto Rx = glm::fmat3{
          1,   0,   0,
          0,  cx,  sx,
          0, -sx,  cx,
      };
      auto Ry = glm::fmat3{
         cy,   0, -sy,
          0,   1,   0,
         sy,   0,  cy,
      };
      auto Rz = glm::fmat3{
         cz,  sz,   0,
        -sz,  cz,   0,
          0,   0,   1,
      };
      return Rz * Ry * Rx;
    }
  };

  struct Transform {
    glm::fvec3 scale_ = {1, 1, 1};
    glm::fvec3 translation_ = {0, 0, 0};
    Rotation rotation_ = { glm::fvec3{0, 0, 0} };
    glm::fmat4 getMatrix() {
      // TODO
      return {};
    }
  };

  struct Camera {
    // https://github.com/KhronosGroup/glTF/blob/master/specification/2.0/README.md#cameras
    glm::fmat4 transform_ = glm::fmat4{1};
    float yfov_ = std::acos(-1) / 3.; // 60deg
    float aspect_ratio_ = 16. / 9.;
    float znear_ = 0.001;
    float zfar_ = 1000;

    glm::fmat4 getPerspectiveProjection() {
      //
      // Derivation
      // 1. perspective-project xy coordinates of "z < 0" half space onto "z = -1" plane
      // 2. (unique) anti-monotone function P(z) = (A z + B) / z
      //     s.t. P(-n) = -1 and P(-f) = 1
      // 3. scale xy fov to [-1, 1]^2
      //
      auto n = znear_, f = zfar_;
      float A = (f + n) / (f - n);
      float B = 2 * f * n / (f - n);
      float x = 1 / (std::tan(yfov_ / 2) * aspect_ratio_);
      float y = 1 / std::tan(yfov_ / 2);

      //
      // NOTE:
      // It feels "result" and "- result" should be equivalent, but it is not the case.
      // Because OpenGL runs clipping by (-w' <= x', y', z' <= w'), it requires
      // for "z < 0" half space to be mapped to w' = -z > 0.
      //
      auto result = glm::fmat4{
        x, 0,  0,  0,
        0, y,  0,  0,
        0, 0, -A, -1,
        0, 0, -B,  0,
      };
      return result;
    }
  };
  std::unique_ptr<Camera> camera_;

  struct Material {
    // TODO:
    // https://github.com/KhronosGroup/glTF/blob/master/specification/2.0/README.md#metallic-roughness-material
  };
  std::unique_ptr<Material> material_;

  struct VertexData {
    glm::fvec3 position;
    glm::fvec4 color;
  };

  struct Mesh {
    std::vector<VertexData> vertices_;
    std::vector<uint8_t> indices_; // triangles

    std::vector<glm::u8vec3> indices;

    static Mesh createCube() {
      Mesh result;
      result.vertices_ = {
        { { 0, 0, 0 }, { 0, 0, 0, 1 } },
        { { 1, 0, 0 }, { 1, 0, 0, 1 } },
        { { 1, 1, 0 }, { 1, 1, 0, 1 } },
        { { 0, 1, 0 }, { 0, 1, 0, 1 } },
        { { 0, 0, 1 }, { 0, 0, 1, 1 } },
        { { 1, 0, 1 }, { 1, 0, 1, 1 } },
        { { 1, 1, 1 }, { 1, 1, 1, 1 } },
        { { 0, 1, 1 }, { 0, 1, 1, 1 } },
      };

      // CCW face in right-hand system
      // NOTE:
      // OpenGL's "window" space (where glCullFace applies) is left-hand system,
      // but, "getPerspectiveProjection" flips z within NDC, so this is okay.
      #define QUAD_TO_TRIS(A, B, C, D) A, B, D, C, D, B,
      result.indices_ = {
        QUAD_TO_TRIS(0, 3, 2, 1) // bottom
        QUAD_TO_TRIS(4, 5, 6, 7) // top
        QUAD_TO_TRIS(0, 1, 5, 4) // side
        QUAD_TO_TRIS(1, 2, 6, 5) // side
        QUAD_TO_TRIS(2, 3, 7, 6) // side
        QUAD_TO_TRIS(3, 0, 4, 7) // side
      };
      #undef QUAD_TO_TRIS
      return result;
    }

    static Mesh create4hedron() {
      Mesh result;
      result.vertices_ = {
        { { 0, 0, 0 }, { 0, 0, 0, 1 } },
        { { 1, 1, 0 }, { 1, 0, 0, 1 } },
        { { 0, 1, 1 }, { 0, 1, 0, 1 } },
        { { 1, 0, 1 }, { 0, 0, 1, 1 } },
      };
      result.indices_ = {
        0, 2, 1,
        0, 3, 2,
        0, 1, 3,
        1, 2, 3,
      };
      return result;
    }

    static Mesh createPlane() {
      Mesh result;
      result.vertices_ = {
        { { 0, 0, 0 }, { 1, 1, 1, 1 } },
        { { 1, 0, 0 }, { 1, 0, 0, 1 } },
        { { 1, 1, 0 }, { 0, 1, 0, 1 } },
        { { 0, 1, 0 }, { 0, 0, 1, 1 } },
      };
      #define QUAD_TO_TRIS(A, B, C, D) A, B, D, C, D, B,
      result.indices_ = {
        QUAD_TO_TRIS(0, 1, 2, 3)
        QUAD_TO_TRIS(4, 5, 6, 7)
      };
      #undef QUAD_TO_TRIS
      return result;
    }
  };

  struct MeshRenderer {
    TOY_CLASS_DELETE_COPY(MeshRenderer)
    GLuint array_buffer_, element_array_buffer_, vertex_array_;
    const GLsizei num_elements_;

    MeshRenderer(const Mesh& mesh, const utils::gl::Program& program)
      : num_elements_{(GLsizei)mesh.indices_.size()} {
      glGenBuffers(1, &array_buffer_);
      glGenBuffers(1, &element_array_buffer_);
      glGenVertexArrays(1, &vertex_array_);

      // Setup data
      glBindBuffer(GL_ARRAY_BUFFER, array_buffer_);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, element_array_buffer_);
      #define SIZEOF_VECTOR(VECTOR) VECTOR.size() * (sizeof VECTOR[0])
      glBufferData(GL_ARRAY_BUFFER, SIZEOF_VECTOR(mesh.vertices_), mesh.vertices_.data(), GL_STREAM_DRAW);
      glBufferData(GL_ELEMENT_ARRAY_BUFFER, SIZEOF_VECTOR(mesh.indices_), mesh.indices_.data(), GL_STREAM_DRAW);
      #undef SIZEOF_VECTOR

      // Configure vertex format
      glBindVertexArray(vertex_array_);
      glBindBuffer(GL_ARRAY_BUFFER, array_buffer_);
      auto position_index_ = glGetAttribLocation(program.handle_, "vert_position_");
      auto color_index_ = glGetAttribLocation(program.handle_, "vert_color_");
      glEnableVertexAttribArray(position_index_);
      glEnableVertexAttribArray(color_index_);
      glVertexAttribPointer(position_index_, 3, GL_FLOAT, GL_FALSE, sizeof(VertexData), (GLvoid*)offsetof(VertexData, position));
      glVertexAttribPointer(color_index_,    4, GL_FLOAT, GL_FALSE, sizeof(VertexData), (GLvoid*)offsetof(VertexData, color));
    }

    ~MeshRenderer() {
      glDeleteBuffers(1, &array_buffer_);
      glDeleteBuffers(1, &element_array_buffer_);
      glDeleteVertexArrays(1, &vertex_array_);
    }

    void draw() {
      glBindVertexArray(vertex_array_);
      glBindBuffer(GL_ARRAY_BUFFER, array_buffer_);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, element_array_buffer_);
      glDrawElements(GL_TRIANGLES, num_elements_, GL_UNSIGNED_BYTE, 0);
    }
  };
  std::unique_ptr<MeshRenderer> mesh_renderer_;

  struct Model {
    glm::fmat4 transform_ = glm::fmat4{1};
    Mesh mesh_;
    Model(Mesh&& mesh) : mesh_{std::move(mesh)} {}
  };
  std::unique_ptr<Model> model_;

  struct Framebuffer {
    TOY_CLASS_DELETE_COPY(Framebuffer)
    GLuint framebuffer_handle_, texture_handle_, depth_texture_handle_;
    ivec2 size_ = {0, 0};

    Framebuffer() {
      glGenFramebuffers(1, &framebuffer_handle_);
      glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer_handle_);

      // color attachment
      glGenTextures(1, &texture_handle_);
      glBindTexture(GL_TEXTURE_2D, texture_handle_);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size_.x, size_.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
      glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_handle_, 0);
      glDrawBuffer(GL_COLOR_ATTACHMENT0);

      // depth attachment
      glGenTextures(1, &depth_texture_handle_);
      glBindTexture(GL_TEXTURE_2D, depth_texture_handle_);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, size_.x, size_.y, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
      glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_texture_handle_, 0);
    }
    ~Framebuffer() {
      glDeleteTextures(1, &depth_texture_handle_);
      glDeleteTextures(1, &texture_handle_);
      glDeleteFramebuffers(1, &framebuffer_handle_);
    }

    void setSize(const ivec2& size) {
      size_ = size;
      glBindTexture(GL_TEXTURE_2D, texture_handle_);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size_.x, size_.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
      glBindTexture(GL_TEXTURE_2D, depth_texture_handle_);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, size_.x, size_.y, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    }
  };
  std::unique_ptr<Framebuffer> fb_;

  std::unique_ptr<utils::gl::Program> program_;

  constexpr static inline const char* vertex_shader_source = R"(
#version 410
uniform mat4 view_projection_;
uniform mat4 view_inv_xform_;
uniform mat4 model_xform_;
layout (location = 0) in vec3 vert_position_;
layout (location = 1) in vec4 vert_color_;
out vec4 interp_color_;
void main() {
  interp_color_ = vert_color_;
  gl_Position = view_projection_ * view_inv_xform_ * model_xform_ * vec4(vert_position_, 1);
}
)";

  constexpr static inline const char* fragment_shader_source = R"(
#version 410
in vec4 interp_color_;
layout (location = 0) out vec4 out_color_;
void main() {
  out_color_ = interp_color_;
}
)";

  SimpleRenderer() {
    fb_.reset(new Framebuffer);
    program_.reset(new utils::gl::Program{vertex_shader_source, fragment_shader_source});
    model_.reset(new Model{Mesh::createCube()});
    mesh_renderer_.reset(new MeshRenderer{model_->mesh_, *program_});
    camera_.reset(new Camera);

    // Default camera position
    camera_->transform_[3] = glm::fvec4{-.7, 1.5, 4, 1};
  }

  void draw() {
    // Setup
    camera_->aspect_ratio_ = (float)fb_->size_[0] / fb_->size_[1];

    //
    // gl calls
    //
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fb_->framebuffer_handle_);
    glViewport(0, 0, fb_->size_[0], fb_->size_[1]);

    // rendering configuration
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);

    // clear buffer
    glm::fvec4 color = {0.2, 0.2, 0.2, 1.0};
    glClearBufferfv(GL_COLOR, 0, (GLfloat*)&color);
    float depth = 1;
    glClearBufferfv(GL_DEPTH, 0, (GLfloat*)&depth);

    // setup uniforms
    glUseProgram(program_->handle_);
    program_->setUniform("model_xform_", model_->transform_);
    program_->setUniform("view_inv_xform_", utils::inverse(camera_->transform_));
    program_->setUniform("view_projection_", camera_->getPerspectiveProjection());

    // draw
    mesh_renderer_->draw();
  }
};


struct RenderPanel : Panel {
  constexpr static const char* type = "Render Panel";
  const SimpleRenderer& renderer_;

  RenderPanel(SimpleRenderer& renderer) : renderer_{renderer} {}

  void processUI() override {
    renderer_.fb_->setSize({size_[0], size_[0] * 3. / 4.});
    ImGui::Text("Size = (%d, %d)", renderer_.fb_->size_[0], renderer_.fb_->size_[1]);
    ImGui::Image(
        reinterpret_cast<ImTextureID>(renderer_.fb_->texture_handle_),
        toImVec2(renderer_.fb_->size_),
        /* uv0 */ {0, 1}, /* uv1 */ {1, 0}); // framebuffer's pixel is ordered from left-bottom.
  }
};

struct PropertyPanel : Panel {
  constexpr static const char* type = "Property Panel";
  const SimpleRenderer& renderer_;

  PropertyPanel(SimpleRenderer& renderer) : renderer_{renderer} {}

  void processUI() override {
    if (auto _ = ImScoped::TreeNodeEx("Model transform", ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::Text("Location");
      ImGui::SetNextItemWidth(-1);
      ImGui::DragFloat3("##model-loc", (float*)&renderer_.model_->transform_[3], .1);

      ImGui::Text("Rotation");
      ImGui::SetNextItemWidth(-1);

      static SimpleRenderer::Rotation rotation_;
      auto updated = ImGui::DragFloat3("##model-rot", (float*)&rotation_.data_, .5);
      if (updated) {
        auto so3 = rotation_.ExtrinsicEulerXYZ_to_SO3();
        renderer_.model_->transform_[0] = {so3[0], 0};
        renderer_.model_->transform_[1] = {so3[1], 0};
        renderer_.model_->transform_[2] = {so3[2], 0};
      }

      ImGui::Text("SO(3)");
      ImGui::SetNextItemWidth(-1);
      ImGui::DragFloat3("##model-xform0", (float*)&renderer_.model_->transform_[0], .1);
      ImGui::SetNextItemWidth(-1);
      ImGui::DragFloat3("##model-xform1", (float*)&renderer_.model_->transform_[1], .1);
      ImGui::SetNextItemWidth(-1);
      ImGui::DragFloat3("##model-xform2", (float*)&renderer_.model_->transform_[2], .1);
    }

    if (auto _ = ImScoped::TreeNodeEx("Camera transform", ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::Text("Location");
      ImGui::SetNextItemWidth(-1);
      ImGui::DragFloat3("##camera-loc", (float*)&renderer_.camera_->transform_[3], .1);
    }
  }
};



struct App {
  std::unique_ptr<toy::Window> window_;
  std::unique_ptr<PanelManager> panel_manager_;
  std::unique_ptr<SimpleRenderer> renderer_;
  bool done_ = false;

  App() {
    window_.reset(new Window{"My Window", {800, 600}, { .gl_debug = true, .hint_maximized = true }});
    renderer_.reset(new SimpleRenderer{});

    panel_manager_.reset(new PanelManager{*window_});
    panel_manager_->registerPanelType(RenderPanel::type, [&]() { return new RenderPanel{*renderer_}; });
    panel_manager_->registerPanelType(PropertyPanel::type, [&]() { return new PropertyPanel{*renderer_}; });
    panel_manager_->addPanelToRoot(kdtree::SplitType::HORIZONTAL, RenderPanel::type);
    panel_manager_->addPanelToRoot(kdtree::SplitType::HORIZONTAL, PropertyPanel::type);
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
