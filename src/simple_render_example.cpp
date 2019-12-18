#include <memory>
#include <iostream>
#include <stdexcept>

#include <fmt/format.h>
#include <imgui.h>
#include <imgui_scoped.h>
#include <glm/glm.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <stb_image.h>

#include "window.hpp"
#include "panel_system.hpp"
#include "panel_system_utils.hpp"
#include "utils.hpp"


namespace toy {

using namespace utils;
using glm::ivec2, glm::fvec2, glm::fvec3, glm::fvec4, glm::fmat4;

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
// [x] draw multiple meshes
// [x] draw ui for each model
// [x] support simple mesh base color texture
//   - [x] update shader (vertex attr + uniform)
//   - [x] data structure
//   - [x] example mesh uv
//   - [x] debug
//     - [x] preview image via imgui (data is loaded correctly)
//     - [x] uv coordinate is correct
//     - [x] maybe framebuffer specific thing? (no, default buffer got same result.)
//     - [x] gl version different from imgui_texture_example (no, it's same)
//     - [x] it turns out it's mis understanding of OpenGL texture/sampler state api.
// [@] draw mesh from gltf
//   - cgltf, import vertex array, texture
// [ ] mesh/texture loader ui
// [ ] mesh/texture examples
// [ ] material
//    - no vertex color
//    - property editor
// [ ] organize scene system
//   - [ ] immitate gltf data structure
//   - scene hierarchy
//   - render system (render resource vs render parameter)
//   - [ ] draw world axis and half planes
//   - [ ] load scene from file
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
    fvec3 position;
    fvec4 color = {1, 1, 1, 1};
    fvec2 uv;
  };

  struct Mesh {
    std::vector<VertexData> vertices_;
    std::vector<uint8_t> indices_; // triangles

    using create_func1_t = std::tuple<vector<fvec3>, vector<fvec4>, vector<uint8_t>>();
    static Mesh create(create_func1_t create_func) {
      auto [positions, colors, indices] = create_func();
      std::vector<fvec2> uvs;
      uvs.resize(positions.size());
      return {
        .vertices_ = utils::interleave<VertexData>(positions, colors, uvs),
        .indices_ = indices,
      };
    };

    using create_func2_t = std::tuple<vector<fvec3>, vector<fvec4>, vector<fvec2>, vector<uint8_t>>();
    static Mesh create(create_func2_t create_func) {
      auto [positions, colors, uvs, indices] = create_func();
      return {
        .vertices_ = utils::interleave<VertexData>(positions, colors, uvs),
        .indices_ = indices,
      };
    };
  };

  struct Texture {
    utils::gl::Texture base_;
    std::string name_;
    std::optional<std::string> filename_;
    ivec2 size_;

    Texture() {}
    Texture(const std::string& filename)
      : name_{filename}, filename_{filename} {
      auto data = stbi_load(filename.data(), &size_.x, &size_.y, nullptr, 4);
      TOY_ASSERT_CUSTOM(data, fmt::format("stbi_load failed: {}", filename));
      base_.setData(size_, data);
      stbi_image_free(data);
    }
  };

  struct Material {
    fvec4 base_color_fill_ = {1, 1, 1, 1};
    std::shared_ptr<Texture> base_color_tex_;
    bool use_base_color_tex_ = false;
    bool use_vertex_color_ = false;
  };

  struct Model {
    glm::fmat4 transform_ = glm::fmat4{1};
    Mesh mesh_;
    utils::gl::VertexRenderer renderer_;
    Material material_;
    Model(Mesh&& mesh) : mesh_{std::move(mesh)} {}
  };

  constexpr static inline const char* vertex_shader_source = R"(
#version 330
uniform mat4 view_projection_;
uniform mat4 view_inv_xform_;
uniform mat4 model_xform_;

layout (location = 0) in vec3 vert_position_;
layout (location = 1) in vec4 vert_color_;
layout (location = 2) in vec2 vert_uv_;

out vec4 interp_color_;
out vec2 interp_uv_;

void main() {
  interp_color_ = vert_color_;
  interp_uv_ = vert_uv_;
  gl_Position = view_projection_ * view_inv_xform_ * model_xform_ * vec4(vert_position_, 1);
}
)";

  constexpr static inline const char* fragment_shader_source = R"(
#version 330
uniform sampler2D base_color_tex_;
uniform bool use_base_color_tex_;
uniform vec4 base_color_fill_;

in vec4 interp_color_;
in vec2 interp_uv_;

layout (location = 0) out vec4 frag_color_;

void main() {
  vec4 base_color =
      interp_color_ *
      ((use_base_color_tex_) ? texture(base_color_tex_, interp_uv_) : base_color_fill_);
  frag_color_ = base_color;
}
)";

  std::unique_ptr<Camera> camera_;
  std::vector<std::unique_ptr<Model>> models_;
  std::unique_ptr<utils::gl::Program> program_;
  std::unique_ptr<utils::gl::Framebuffer> fb_;

  SimpleRenderer() {
    // GL resource
    program_.reset(new utils::gl::Program{vertex_shader_source, fragment_shader_source});
    fb_.reset(new utils::gl::Framebuffer);

    // Scene data
    camera_.reset(new Camera);
    models_.emplace_back(new Model{Mesh::create(utils::createUVCube)});
    models_.emplace_back(new Model{Mesh::create(utils::create4Hedron)});
    models_.emplace_back(new Model{Mesh::create(utils::createUVPlane)});

    std::shared_ptr<Texture> texture{
        new Texture{TOY_PATH("thirdparty/yocto-gl/tests/textures/uvgrid.png")}};

    models_[0]->material_.base_color_tex_ = texture;
    models_[0]->material_.use_base_color_tex_ = true;
    models_[2]->material_.base_color_tex_ = texture;
    models_[2]->material_.use_base_color_tex_ = true;

    // Position them so we can see all
    camera_->transform_[3] = glm::fvec4{-.7, 1.5, 4, 1};
    models_[0]->transform_[3] = glm::fvec4{-2, 0, 0, 1};
    models_[1]->transform_[3] = glm::fvec4{ 0, 2, 0, 1};

    // Setup GL data
    for (auto& model : models_) {
      model->renderer_.setData(model->mesh_.vertices_, model->mesh_.indices_);
      model->renderer_.setFormat(
          glGetAttribLocation(program_->handle_, "vert_position_"),
          3, GL_FLOAT, GL_FALSE, sizeof(VertexData), (GLvoid*)offsetof(VertexData, position));
      model->renderer_.setFormat(
          glGetAttribLocation(program_->handle_, "vert_color_"),
          4, GL_FLOAT, GL_FALSE, sizeof(VertexData), (GLvoid*)offsetof(VertexData, color));
      model->renderer_.setFormat(
          glGetAttribLocation(program_->handle_, "vert_uv_"),
          2, GL_FLOAT, GL_FALSE, sizeof(VertexData), (GLvoid*)offsetof(VertexData, uv));
    }
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
    program_->setUniform("view_inv_xform_", utils::inverse(camera_->transform_));
    program_->setUniform("view_projection_", camera_->getPerspectiveProjection());
    program_->setUniform("base_color_tex_", 0);

    // draw
    for (auto& model : models_) {
      auto& mat = model->material_;
      program_->setUniform("base_color_fill_", mat.base_color_fill_);
      glActiveTexture(GL_TEXTURE0);
      if (mat.base_color_tex_ && mat.use_base_color_tex_) {
        glBindTexture(GL_TEXTURE_2D, mat.base_color_tex_->base_.handle_);
        program_->setUniform("use_base_color_tex_", 1);
      } else {
        glBindTexture(GL_TEXTURE_2D, 0);
        program_->setUniform("use_base_color_tex_", 0);
      }
      program_->setUniform("model_xform_", model->transform_);
      model->renderer_.draw();
    }
  }
};

struct RenderPanel : Panel {
  constexpr static const char* type = "Render Panel";

  const SimpleRenderer& renderer_;

  RenderPanel(SimpleRenderer& renderer) : renderer_{renderer} {
    style_vars_ = { {ImGuiStyleVar_WindowPadding, ImVec2{0, 0} }};
  }

  void processUI() override {
    renderer_.fb_->setSize({content_size_[0], content_size_[1]});
    ImGui::Image(
        reinterpret_cast<ImTextureID>(renderer_.fb_->texture_handle_),
        toImVec2(renderer_.fb_->size_),
        /* uv0 */ {0, 1}, /* uv1 */ {1, 0}); // framebuffer's pixel start from bottom-left.
  }
};

struct PropertyPanel : Panel {
  constexpr static const char* type = "Property Panel";
  const SimpleRenderer& renderer_;

  PropertyPanel(SimpleRenderer& renderer) : renderer_{renderer} {}

  void processUI() override {
    for (auto& model : renderer_.models_) {
      auto _ = ImScoped::ID(model.get());
      if (auto _ = ImScoped::TreeNodeEx("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Location");
        ImGui::SetNextItemWidth(-1);
        ImGui::DragFloat3("##model-loc", (float*)&model->transform_[3], .02);

        ImGui::Text("Rotation");
        ImGui::SetNextItemWidth(-1);

        static fvec3 angles_;
        auto updated = ImGui::DragFloat3("##model-rot", (float*)&angles_, .5);
        if (updated) {
          auto so3 = utils::ExtrinsicEulerXYZ_to_SO3(angles_);
          model->transform_[0] = {so3[0], 0};
          model->transform_[1] = {so3[1], 0};
          model->transform_[2] = {so3[2], 0};
        }

        if (auto _ = ImScoped::TreeNode("Matrix")) {
          ImGui::SetNextItemWidth(-1);
          ImGui::DragFloat4("##model-xform0", (float*)&model->transform_[0], .1);
          ImGui::SetNextItemWidth(-1);
          ImGui::DragFloat4("##model-xform1", (float*)&model->transform_[1], .1);
          ImGui::SetNextItemWidth(-1);
          ImGui::DragFloat4("##model-xform2", (float*)&model->transform_[2], .1);
          ImGui::SetNextItemWidth(-1);
          ImGui::DragFloat4("##model-xform3", (float*)&model->transform_[3], .1);
        }
      }
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

struct ImagePanel : Panel {
  constexpr static const char* type = "Image";
  std::shared_ptr<SimpleRenderer::Texture> texture_;

  ImagePanel(const std::shared_ptr<SimpleRenderer::Texture>& ptr)
    : texture_{ptr} {}

  void processUI() override {
    if (texture_) {
      auto handle = reinterpret_cast<ImTextureID>(texture_->base_.handle_);
      ImGui::Image(handle, toImVec2(texture_->size_ / 2));
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
    panel_manager_->registerPanelType<MetricsPanel>();
    panel_manager_->registerPanelType<RenderPanel>([&]() { return new RenderPanel{*renderer_}; });
    panel_manager_->registerPanelType<PropertyPanel>([&]() { return new PropertyPanel{*renderer_}; });
    panel_manager_->registerPanelType<DemoPanel>();

    // TODO: better way to pass texture to ImagePanel
    panel_manager_->registerPanelType<ImagePanel>([&]() {
        return new ImagePanel{renderer_->models_[0]->material_.base_color_tex_}; });

    panel_manager_->addPanelToRoot(kdtree::SplitType::HORIZONTAL, PropertyPanel::type);
    panel_manager_->addPanelToRoot(kdtree::SplitType::VERTICAL, ImagePanel::type, 0.4);
    panel_manager_->addPanelToRoot(kdtree::SplitType::HORIZONTAL, RenderPanel::type, 0.4);
  }

  void processMainMenuBar() {
    auto _ = ImScoped::StyleVar(ImGuiStyleVar_FramePadding, {4, 6});
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
