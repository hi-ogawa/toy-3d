#include <memory>
#include <iostream>
#include <stdexcept>

#include <fmt/format.h>
#include <imgui.h>
#include <imgui_scoped.h>
#include <glm/glm.hpp>
#include <glm/ext/matrix_clip_space.hpp>

#include "window.hpp"
#include "panel_system.hpp"
#include "panel_system_utils.hpp"
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

  struct Camera {
    glm::fmat4 transform_ = glm::fmat4{1};
    float yfov_ = std::acos(-1) / 3.; // 60deg
    float aspect_ratio_ = 16. / 9.;
    float znear_ = 0.001;
    float zfar_ = 1000;

    glm::fmat4 getPerspectiveProjection() {
      // Derivation
      // 1. perspective-project xy coordinates of "z < 0" half space onto "z = -1" plane
      // 2. (unique) anti-monotone function "P(z) = (A z + B) / z" s.t. P(-n) = -1 and P(-f) = 1
      // 3. scale xy fov to [-1, 1]^2
      // 4. "z < 0" has to be mapped to "w' = -z > 0"
      return glm::perspectiveRH_NO(yfov_, aspect_ratio_, znear_, zfar_);
    }
  };

  struct VertexData {
    glm::fvec3 position;
    glm::fvec4 color;
  };

  struct Mesh {
    std::vector<VertexData> vertices_;
    std::vector<uint8_t> indices_; // triangles

    static Mesh createCube() {
      auto [positions, colors, indices] = utils::createCube();
      return {
        .vertices_ = utils::interleave<VertexData>(positions, colors),
        .indices_ = indices
      };
    }
  };

  struct Node {
    glm::fmat4 transform_ = glm::fmat4{1};
    Mesh mesh_;
    Node(Mesh&& mesh) : mesh_{std::move(mesh)} {}
  };

  std::unique_ptr<Camera> camera_;
  std::unique_ptr<Node> model_;

  std::unique_ptr<utils::gl::Program> program_;
  std::unique_ptr<utils::gl::Framebuffer> fb_;
  std::unique_ptr<utils::gl::VertexRenderer> vertex_renderer_;

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
    // GL resource
    program_.reset(new utils::gl::Program{vertex_shader_source, fragment_shader_source});
    fb_.reset(new utils::gl::Framebuffer);
    vertex_renderer_.reset(new utils::gl::VertexRenderer);

    // Scene data
    model_.reset(new Node{Mesh::createCube()});
    camera_.reset(new Camera);
    camera_->transform_[3] = glm::fvec4{-.7, 1.5, 4, 1}; // Default camera position

    // Setup GL data
    vertex_renderer_->setData(model_->mesh_.vertices_, model_->mesh_.indices_);
    vertex_renderer_->setFormat(
        glGetAttribLocation(program_->handle_, "vert_position_"),
        3, GL_FLOAT, GL_FALSE, sizeof(VertexData), (GLvoid*)offsetof(VertexData, position));
    vertex_renderer_->setFormat(
        glGetAttribLocation(program_->handle_, "vert_color_"),
        4, GL_FLOAT, GL_FALSE, sizeof(VertexData), (GLvoid*)offsetof(VertexData, color));
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
    vertex_renderer_->draw();
  }
};


struct RenderPanel : Panel {
  constexpr static const char* type = "Render Panel";
  const SimpleRenderer& renderer_;

  RenderPanel(SimpleRenderer& renderer) : renderer_{renderer} {}

  void processUI() override {
    renderer_.fb_->setSize({content_size_[0], content_size_[1]});
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

      static fvec3 angles_;
      auto updated = ImGui::DragFloat3("##model-rot", (float*)&angles_, .5);
      if (updated) {
        auto so3 = utils::ExtrinsicEulerXYZ_to_SO3(angles_);
        renderer_.model_->transform_[0] = {so3[0], 0};
        renderer_.model_->transform_[1] = {so3[1], 0};
        renderer_.model_->transform_[2] = {so3[2], 0};
      }

      ImGui::Text("SO(3) (for debug)");
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

    if (auto _ = ImScoped::TreeNodeEx("Framebuffer", ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::Text("Size = (%d, %d)", renderer_.fb_->size_[0], renderer_.fb_->size_[1]);
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
    panel_manager_->registerPanelType<StyleEditorPanel>();
    panel_manager_->registerPanelType<RenderPanel>([&]() { return new RenderPanel{*renderer_}; });
    panel_manager_->registerPanelType<PropertyPanel>([&]() { return new PropertyPanel{*renderer_}; });

    panel_manager_->addPanelToRoot(kdtree::SplitType::HORIZONTAL, RenderPanel::type);
    panel_manager_->addPanelToRoot(kdtree::SplitType::HORIZONTAL, PropertyPanel::type, 0.6);
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
