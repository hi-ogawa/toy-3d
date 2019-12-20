#include "window.hpp"
#include "panel_system.hpp"
#include "panel_system_utils.hpp"
#include "utils.hpp"
#include "scene.hpp"

namespace toy {

using namespace scene;
using glm::ivec2, glm::fvec2, glm::fvec3, glm::fvec4, glm::fmat4;
using std::map, std::vector, std::string, std::unique_ptr, std::shared_ptr, std::weak_ptr;

// TODO:
// - is it possible to do similar thing without embedding unique_ptr<XxxRR> within Mesh, Texture, etc... ??
//   and possibly move those OpenGL resource to be owned by this "SceneRenderer" ??
struct SceneRenderer {
  unique_ptr<utils::gl::Program> program_;

  SceneRenderer() {
    #include "scene_example_shaders.hpp"
    program_.reset(new utils::gl::Program{vertex_shader_source, fragment_shader_source});
  }

  void updateRenderResouce(const Scene& scene) {
    for (auto& node : scene.nodes_) {
      if (node->mesh_ && !node->mesh_->rr_) {
        auto mesh_rr = new MeshRR(*node->mesh_);
        node->mesh_->rr_.reset(mesh_rr);
        mesh_rr->base_.setFormat(program_->handle_, {
            { "vert_position_", {3, GL_FLOAT, GL_FALSE, sizeof(VertexAttrs), (GLvoid*)offsetof(VertexAttrs, position)} },
            { "vert_color_",    {4, GL_FLOAT, GL_FALSE, sizeof(VertexAttrs), (GLvoid*)offsetof(VertexAttrs, color)   } },
            { "vert_texcoord_", {2, GL_FLOAT, GL_FALSE, sizeof(VertexAttrs), (GLvoid*)offsetof(VertexAttrs, texcoord)} },
        });
      }

      if (node->material_ && node->material_->base_color_texture_) {
        auto texture = node->material_->base_color_texture_;
        if (!texture->rr_) {
          texture->rr_.reset(new TextureRR(*texture));
        }
      }
    }
  }

  void draw(const Scene& scene) {
    glUseProgram(program_->handle_);

    // global uniform
    program_->setUniform("view_inv_xform_", utils::inverse(scene.camera_.transform_));
    program_->setUniform("view_projection_", scene.camera_.getPerspectiveProjection());
    program_->setUniform("base_color_texture_", 0);

    for (auto& node : scene.nodes_) {
      if (!node->mesh_) { continue; }

      // per-node uniform
      program_->setUniform("model_xform_", node->transform_);

      if (auto mat = node->material_) {
        program_->setUniform("base_color_factor_", mat->base_color_factor_);
        bool use_texture = mat->base_color_texture_ && mat->use_base_color_texture_;

        auto& texture_rr = mat->base_color_texture_->rr_;
        TOY_ASSERT(texture_rr);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, use_texture ? texture_rr->base_.handle_ : 0);
        program_->setUniform("use_base_color_texture_", (GLint)use_texture);
      }

      // draw
      TOY_ASSERT(node->mesh_->rr_);
      node->mesh_->rr_->base_.draw();
    }
  }
};


struct SceneManager {
  unique_ptr<utils::gl::Framebuffer> framebuffer_; // TODO: should `RenderPanel` own framebuffer?
  unique_ptr<Scene> scene_;
  unique_ptr<SceneRenderer> renderer_;
  vector<unique_ptr<AssetRepository>> asset_repositories_;

  SceneManager() {
    framebuffer_.reset(new utils::gl::Framebuffer);
    scene_.reset(new Scene);
    renderer_.reset(new SceneRenderer);

    // default camera position
    scene_->camera_.transform_[3] = fvec4{0, 0, 4, 1};
  }

  void loadGltf(const char* filename) {
    auto& new_assets = asset_repositories_.emplace_back(
          new AssetRepository{gltf::load(filename)});
    for (auto& node : new_assets->nodes_) {
      scene_->nodes_.push_back(node);
    }
  }

  void draw() {
    // Setup
    renderer_->updateRenderResouce(*scene_);

    // bind non-default framebuffer
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer_->framebuffer_handle_);

    // rendering configuration
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);

    // clear buffer
    glm::fvec4 color = {0.2, 0.2, 0.2, 1.0};
    glClearBufferfv(GL_COLOR, 0, (GLfloat*)&color);
    float depth = 1;
    glClearBufferfv(GL_DEPTH, 0, (GLfloat*)&depth);

    // draw
    glViewport(0, 0, framebuffer_->size_.x, framebuffer_->size_.y);
    scene_->camera_.aspect_ratio_ = (float)framebuffer_->size_.x / framebuffer_->size_.y;
    renderer_->draw(*scene_);
  }
};

struct RenderPanel : Panel {
  constexpr static const char* type = "Render";
  gl::Framebuffer& fb_;

  RenderPanel(gl::Framebuffer& fb) : fb_{fb} {
    style_vars_ = { {ImGuiStyleVar_WindowPadding, ImVec2{0, 0} }};
  }

  void processUI() override {
    fb_.setSize({content_size_[0], content_size_[1]});
    ImGui::Image(
        reinterpret_cast<ImTextureID>(fb_.texture_handle_),
        toImVec2(fb_.size_), /* uv0 */ {0, 1}, /* uv1 */ {1, 0});
  }
};

struct App {
  unique_ptr<toy::Window> window_;
  unique_ptr<PanelManager> panel_manager_;
  unique_ptr<SceneManager> scene_manager_;
  bool done_ = false;

  App() {
    window_.reset(new Window{"My Window", {800, 600}, { .gl_debug = true, .hint_maximized = true }});
    scene_manager_.reset(new SceneManager);

    // load asssets
    scene_manager_->loadGltf(GLTF_MODEL_PATH("DamagedHelmet"));
    scene_manager_->loadGltf(GLTF_MODEL_PATH("Suzanne"));

    // panel system setup
    panel_manager_.reset(new PanelManager{*window_});
    panel_manager_->registerPanelType<StyleEditorPanel>();
    panel_manager_->registerPanelType<MetricsPanel>();
    panel_manager_->registerPanelType<DemoPanel>();

    panel_manager_->registerPanelType<RenderPanel>([&]() {
        return new RenderPanel{*scene_manager_->framebuffer_}; });

    panel_manager_->addPanelToRoot(kdtree::SplitType::HORIZONTAL, MetricsPanel::type);
    panel_manager_->addPanelToRoot(kdtree::SplitType::VERTICAL, DemoPanel::type, 0.6);
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
      panel_manager_->processPostUI();
      scene_manager_->draw();
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
