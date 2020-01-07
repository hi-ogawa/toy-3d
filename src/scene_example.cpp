#include "window.hpp"
#include "panel_system.hpp"
#include "panel_system_utils.hpp"
#include "utils.hpp"
#include "utils_imgui.hpp"
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

  void _draw(const Scene& scene, const Camera& camera) {
    glUseProgram(program_->handle_);

    // global uniform
    program_->setUniform("view_inv_xform_", utils::inverseTR(camera.transform_));
    program_->setUniform("view_projection_", camera.getPerspectiveProjection());
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

  void draw(
      const Scene& scene,
      const Camera& camera,
      const gl::Framebuffer& framebuffer,
      fvec4 clear_color = {0, 0, 0, 0}) {
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer.framebuffer_handle_);

    // rendering configuration
    glEnable(GL_CULL_FACE); // TODO: cull face per material
    glEnable(GL_DEPTH_TEST);

    // clear buffer
    glClearBufferfv(GL_COLOR, 0, (GLfloat*)&clear_color);
    float depth = 1;
    glClearBufferfv(GL_DEPTH, 0, (GLfloat*)&depth);

    // really draw
    glViewport(0, 0, framebuffer.size_.x, framebuffer.size_.y);
    _draw(scene, camera);
  }
};

struct Editor {
  void* ctx_; // (for now, just a loophole for ViewportPanel::UIContext)
};

struct SceneManager {
  Editor editor_; // todo: make it upside down (Editor owns SceneManager)
  unique_ptr<Scene> scene_;
  unique_ptr<SceneRenderer> renderer_;
  vector<unique_ptr<AssetRepository>> asset_repositories_;

  SceneManager() {
    scene_.reset(new Scene);
    renderer_.reset(new SceneRenderer);
  }

  void loadGltf(const char* filename) {
    auto& new_assets = asset_repositories_.emplace_back(
        new AssetRepository{gltf::load(filename)});
    for (auto& node : new_assets->nodes_) {
      scene_->nodes_.push_back(node);
    }
    renderer_->updateRenderResouce(*scene_);
    SceneManager::setupBVH(*scene_);
  }

  // TODO: Not sure where to put this
  static void setupBVH(const Scene& scene) {
    for (auto& node : scene.nodes_) {
      if (node->mesh_ && !node->mesh_->bvh_) {
        Mesh& mesh = *node->mesh_;
        mesh.bvh_.reset(new MeshBVH{mesh});
      }
    }
  }

  struct SceneRayIntersection {
    MeshBVH::RayTestResult result;
    shared_ptr<Node> node;
  };

  SceneRayIntersection rayIntersection(const fvec3& src, const fvec3& dir) const {
    MeshBVH::RayTestResult result = { .hit = false, .t = FLT_MAX };
    shared_ptr<Node> hit_node;

    for (auto& node : scene_->nodes_) {
      if (!node->mesh_) { continue; }

      Mesh& mesh = *node->mesh_;
      auto tmp_result = mesh.bvh_->rayTest(
          glm::inverse(node->transform_) * fvec4{src, 1},
          glm::inverse(fmat3{node->transform_}) * dir);
      if (!tmp_result.hit) { continue; }
      if (!(tmp_result.t < result.t)) { continue; }

      result.hit = true;
      result.t = tmp_result.t;
      result.point = fvec3{node->transform_ * fvec4{tmp_result.point, 1}};
      for (auto i : Range{3}) {
        result.face[i] = fvec3{node->transform_ * fvec4{tmp_result.face[i], 1}};
      }
      hit_node = node;
    }

    return SceneRayIntersection{result, hit_node};
  }
};


//
// UIs
//

// todo: is it possible to support multiple ViewportPanel instantiations? (not really useful though)
struct ViewportPanel : Panel {
  constexpr static const char* type = "Viewport";
  unique_ptr<utils::gl::Framebuffer> framebuffer_;
  SceneManager& mng_;
  ImDrawList* draw_list_;
  Camera camera_;

  struct DrawChannel { enum { // scoped untyped enum
    SCENE_IMAGE = 0,
    GIZMO,
    OVERLAY,
    CHANNELS_COUNT,
  };};

  // todo: migrate to EditorContext
  struct UIContext {
    // refreshed on new frame
    ivec2 mouse_position_imgui;
    ivec2 mouse_position_imgui_delta;
    fvec3 mouse_position_scene;

    // sceneCo -> cameraCo -> clipCo -> ndCo -> imguiCo
    fmat4 sceneCo_to_clipCo;
    fmat3 ndCo_to_imguiCo;           // 2d homog transform (here, ndCo without depth)

    // sceneCo <- cameraCo <- clipCo <- ndCo <- imguiCo
    fmat3 imguiCo_to_ndCo;
    glm::fmat3x4 ndCo_to_sceneCo;    // injection to SceneCo as CameraCo at z = -1 (TODO: z = -near is more useful?)
    glm::fmat3x4 imguiCo_to_sceneCo; // ndCo_to_sceneCo * imguiCo_to_ndCo

    // convinience
    imgui::DrawList3D imgui3d;

    // some shortcut
    fvec3 camera_position;
    fvec3 mouse_direction; // mouse_position_scene - camera_position

    // UI state
    fvec3 pivot = {0, 0, 0};
    int grid[3] = {0, 1, 0}; // bools
    int axis[3] = {1, 1, 1}; // bools
    int axis_bound = 10;
    int grid_division = 3;

    imgui::TransformGizmo gizmo;
    shared_ptr<Node> active_node;

    bool overlay = true;
    int debug_ray_test = 0; // bool
  } ctx_;

  ViewportPanel(SceneManager& mng) : mng_{mng} {
    framebuffer_.reset(new utils::gl::Framebuffer);
    camera_.transform_[3] = fvec4{0, 0, 4, 1};
    mng_.editor_.ctx_ = &ctx_;
  }

  void setupContext() {
    ctx_.sceneCo_to_clipCo = camera_.get_sceneCo_to_clipCo();
    ctx_.ndCo_to_sceneCo = camera_.get_ndCo_to_sceneCo();
    ctx_.ndCo_to_imguiCo = get_ndCo_to_windowCo(content_offset_, content_size_);
    ctx_.imguiCo_to_ndCo = glm::inverse(ctx_.ndCo_to_imguiCo);
    ctx_.imguiCo_to_sceneCo = ctx_.ndCo_to_sceneCo * ctx_.imguiCo_to_ndCo;

    ctx_.mouse_position_imgui = ImGui::GetMousePos().glm();
    ctx_.mouse_position_imgui_delta = ImGui::GetIO().MouseDelta.glm();
    ctx_.mouse_position_scene = ctx_.imguiCo_to_sceneCo * fvec3{ctx_.mouse_position_imgui, 1};
    ctx_.camera_position = fvec3{camera_.transform_[3]};
    ctx_.mouse_direction = ctx_.mouse_position_scene - ctx_.camera_position;

    ctx_.imguiCo_to_sceneCo * fvec3{ctx_.mouse_position_imgui, 1};
    ctx_.imgui3d = {
        draw_list_, &ctx_.camera_position, &ctx_.mouse_position_scene,
        &ctx_.sceneCo_to_clipCo, &ctx_.ndCo_to_imguiCo};

    if (ctx_.active_node) {
      ctx_.gizmo.setup(ctx_.imgui3d, ctx_.active_node->transform_);
    }
  }

  void processMenu() override {
    if (auto _ = ImScoped::Menu("Edit")) {
      if (ImGui::MenuItem("Overlay", nullptr, ctx_.overlay)) {
        ctx_.overlay = !ctx_.overlay;
      }
    }
  }

  void UI_Overlay() {
    if (ctx_.overlay) {
      auto _ = ImScoped::Child(__FILE__, ImVec2{content_size_.x / 2.5f, 0.f}, false, ImGuiWindowFlags_NoMove);

      if (ImGui::CollapsingHeader("UI")) {
        if (auto _ = ImScoped::TreeNodeEx("Gizmo")) {
          utils::imgui::RadioButtons(
              &ctx_.gizmo.mode,
              { { "Translation", utils::imgui::TransformGizmo::Mode::kTranslation },
                { "Rotation",    utils::imgui::TransformGizmo::Mode::kRotation    },
                { "Scale",       utils::imgui::TransformGizmo::Mode::kScale       }, });
        }

        if (auto _ = ImScoped::TreeNodeEx("Axis/Grid")) {
          ImGui::InputInt("bound", &ctx_.axis_bound);
          ImGui::InputInt("division", &ctx_.grid_division);
          ImGui::SliderInt3("axis (x, y, z)", ctx_.axis, 0, 1, "");
          ImGui::SliderInt3("grid (yz, zx, xy)", ctx_.grid, 0, 1, "");
          ImGui::NextColumn();
        }
      }

      if (ImGui::CollapsingHeader("Debug")) {
        // ray-face intersection
        ImGui::SliderInt("ray-face intersect", &ctx_.debug_ray_test, 0, 1, "");

        if (auto _ = ImScoped::TreeNodeEx("Size/Offset")) {
          // offset
          ImGui::InputInt2("offset_", (int*)&offset_, ImGuiInputTextFlags_ReadOnly);

          // size
          ImGui::InputInt2("size_", (int*)&size_, ImGuiInputTextFlags_ReadOnly);

          // content offset
          ImGui::InputInt2("content_offset_", (int*)&content_offset_, ImGuiInputTextFlags_ReadOnly);

          // content size
          ImGui::InputInt2("content_size_", (int*)&content_size_, ImGuiInputTextFlags_ReadOnly);
        }

        if (auto _ = ImScoped::TreeNodeEx("Mouse")) {
          // mouse (imgui)
          ImGui::InputInt2("mouse (imgui)", (int*)&ctx_.mouse_position_imgui, ImGuiInputTextFlags_ReadOnly);

          // mouse (imgui*)
          ImVec2 _v = ctx_.imgui3d.sceneCo_to_imguiCo(ctx_.mouse_position_scene);
          ImGui::InputFloat2("mouse (imgui*)", (float*)&_v, 0, ImGuiInputTextFlags_ReadOnly);
          ImGui::SameLine();
          utils::imgui::HelpInfo("coord transf. via imgui -> scene -> imgui");

          // mouse (scene)
          ImGui::InputFloat3("mouse (scene)", (float*)&ctx_.mouse_position_scene, 2, ImGuiInputTextFlags_ReadOnly);
        }
      }
    }
  }

  void UI_3D() {
    ctx_.imgui3d.addAxes(ctx_.axis_bound, ctx_.axis);
    ctx_.imgui3d.addGridPlanes(ctx_.axis_bound, ctx_.grid_division, ctx_.grid);

    // (temporary) viewport camera interaction demo
    if (ImGui::IsMouseDown(1)) {
      fvec2 delta = ImGui::GetIO().MouseDelta.glm() / fvec2{framebuffer_->size_};
      if (ImGui::GetIO().KeyCtrl) {
        pivotControl(camera_.transform_, ctx_.pivot, delta * fvec2{2 * 3.14, 3.14}, PivotControlType::ROTATION);
      }
      if (ImGui::GetIO().KeyAlt) {
        pivotControl(camera_.transform_, ctx_.pivot, delta * 4.f, PivotControlType::ZOOM);
      }
      if (ImGui::GetIO().KeyShift) {
        pivotControl(camera_.transform_, ctx_.pivot, delta * 4.f, PivotControlType::MOVE);
      }
    }

    //
    // [ editor state rule (todo: get comprehensive including all UI event on this viewport) ]
    //
    // no active_node => active_node
    // - on viewport click hits node's mesh
    //
    // active_node => no active_node
    // - on viewport click misses node's mesh and misses current gizmo
    //
    // active_node => new active_node
    // - on viewport click hits new node's mesh and misses current gizmo
    //

    if (ctx_.active_node) {
      // todo: "scale by diagonal" is not working?
      ctx_.gizmo.use();
    }

    // todo: detect only clicked on viewport
    if (ImGui::IsMouseClicked(0)) {
      if (!ctx_.gizmo.hovered()) {
        auto intersection = mng_.rayIntersection(ctx_.camera_position, ctx_.mouse_direction);
        if (intersection.result.hit) {
          ctx_.active_node = intersection.node;
        } else {
          ctx_.active_node = nullptr;
        }
      }
    }

    // (temporary) ray intersection triangle demo
    if (ctx_.debug_ray_test) {
      auto intersection = mng_.rayIntersection(ctx_.camera_position, ctx_.mouse_direction);
      if (intersection.result.hit) {
        array<fvec3, 3>& face = intersection.result.face;
        ctx_.imgui3d.addConvexFill({face[0], face[1], face[2]}, {1, 0, 1, .5});
      }
    }
  }

  void processUI() override {
    // Setup
    camera_.aspect_ratio_ = (float)content_size_[0] / content_size_[1];
    framebuffer_->setSize({content_size_[0], content_size_[1]});
    draw_list_ = ImGui::GetWindowDrawList();
    setupContext();

    // Draw UI
    draw_list_->ChannelsSplit(DrawChannel::CHANNELS_COUNT);

    draw_list_->ChannelsSetCurrent(DrawChannel::OVERLAY);
    UI_Overlay();

    draw_list_->ChannelsSetCurrent(DrawChannel::GIZMO);
    UI_3D();
  }

  // NOTE: this will be called after PanelManager handled insert/split/close etc...
  //       so, ImGui won't use texture if panel is closed within current event loop,
  //       which would cause "message = GL_INVALID_OPERATION in glBindTexture(non-gen name)".
  void processPostUI() override {
    mng_.renderer_->draw(*mng_.scene_, camera_, *framebuffer_);

    // Show texture as ImGui quad
    draw_list_->ChannelsSetCurrent(DrawChannel::SCENE_IMAGE);
    ImGui::GetWindowDrawList()->AddImage(
      reinterpret_cast<ImTextureID>(framebuffer_->texture_handle_),
      ImVec2{content_offset_}, ImVec2{content_offset_ + content_size_},
        /* uv0 */ {0, 1}, /* uv1 */ {1, 0});

    draw_list_->ChannelsMerge();
  }
};

// todo: rename to PropertyPanel
struct AssetsPanel : Panel {
  constexpr static const char* type = "Assets";
  SceneManager& mng_;
  string filename_;

  AssetsPanel(SceneManager& mng) : mng_{mng}, filename_{""} {
    filename_.reserve(128);
  }

  void UI_GltfImporter() {
    auto dd_active = ImGui::GetCurrentContext()->DragDropActive;
    if (!dd_active) {
      ImGui::InputTextWithHint("", "Type .gltf file or drag&drop here", filename_.data(), filename_.capacity() + 1);
    } else {
      auto _ = ImScoped::StyleColor(ImGuiCol_Button, ImGui::GetColorU32(ImGuiCol_ButtonHovered));
      ImGui::Button("DROP A FILE HERE", {ImGui::CalcItemWidth(), 0});
      auto clicked = ImGui::IsItemClicked();
      if (auto _ = ImScoped::DragDropTarget()) {
        auto const_payload = ImGui::AcceptDragDropPayload("CUSTOM_FILE", ImGuiDragDropFlags_AcceptBeforeDelivery);
        auto payload = const_cast<struct ImGuiPayload*>(const_payload);
        auto drag_drop_files = reinterpret_cast<vector<string>**>(payload->Data);
        if (payload) {
          if (clicked) {
            filename_ = (**drag_drop_files)[0];
            (**drag_drop_files).clear();
            payload->Delivery = true;
          } else {
            payload->Delivery = false;
          }
        }
      }
    }
    ImGui::SameLine();
    auto _ = ImScoped::StyleColor(ImGuiCol_Text, ImGui::GetColorU32(dd_active ? ImGuiCol_TextDisabled : ImGuiCol_Text));
    if (ImGui::ButtonEx("LOAD", {0, 0}, dd_active ? ImGuiButtonFlags_Disabled : 0)) {
      try {
        // TODO: we can defer actual loading somewhere else
        mng_.loadGltf(filename_.data());
        filename_ = "";
      } catch (std::runtime_error e) {
        fmt::print("=== exception ===\n{}\n", e.what());
      }
    }
  }

  void UI_Scene() {
    for (auto& node : mng_.scene_->nodes_) {
      auto _ = ImScoped::ID(node.get());
      if (auto _ = ImScoped::TreeNodeEx(node->name_.data(), ImGuiTreeNodeFlags_DefaultOpen)) {

        if (auto _ = ImScoped::TreeNodeEx("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
          ImGui::SameLine();
          if (ImGui::SmallButton("Reset")) { node->transform_ = fmat4{1}; };
          imgui::InputTransform(node->transform_);
        }

        if (auto _ = ImScoped::TreeNodeEx("(Transform Matrix)")) {
          for (auto i : utils::Range{4}) {
            auto _ = ImScoped::ID(i);
            ImGui::DragFloat4(fmt::format("transform[{}]", i).data(), (float*)&node->transform_[i], .05);
          }
        }
      }
    }

    if (auto _ = ImScoped::TreeNodeEx("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
      auto& camera = mng_.scene_->camera_;
      if (auto _ = ImScoped::TreeNodeEx("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SameLine();
        if (ImGui::SmallButton("Reset")) { camera.transform_ = fmat4{1}; };
        imgui::InputTransform(camera.transform_);
      }
    }
  }

  void UI_Active() {
    auto& node = ((ViewportPanel::UIContext*)mng_.editor_.ctx_)->active_node;
    if (!node) { return; }

    if (auto _ = ImScoped::TreeNodeEx(node->name_.data(), ImGuiTreeNodeFlags_DefaultOpen)) {

      if (auto _ = ImScoped::TreeNodeEx("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SameLine();
        if (ImGui::SmallButton("Reset")) { node->transform_ = fmat4{1}; };
        imgui::InputTransform(node->transform_);
      }

      if (auto _ = ImScoped::TreeNodeEx("(Transform Matrix)")) {
        for (auto i : utils::Range{4}) {
          auto _ = ImScoped::ID(i);
          ImGui::DragFloat4(fmt::format("transform[{}]", i).data(), (float*)&node->transform_[i], .05);
        }
      }
    }
  }

  void processUI() override {
    if (ImGui::BeginTabBar(__FILE__)){
      if (ImGui::BeginTabItem("Active")) {
        auto __ = ImScoped::ID(__LINE__);
        UI_Active();
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem("Scene")) {
        auto __ = ImScoped::ID(__LINE__);
        UI_Scene();
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem("Importer")) {
        auto __ = ImScoped::ID(__LINE__);
        UI_GltfImporter();
        ImGui::EndTabItem();
      }
      ImGui::EndTabBar();
    }
  }
};

struct App {
  unique_ptr<toy::Window> window_;
  unique_ptr<PanelManager> panel_manager_;
  unique_ptr<SceneManager> scene_manager_;
  vector<string> drag_drop_files_;
  bool done_ = false;

  App() {
    window_.reset(new Window{"My Window", {800, 600}, { .gl_debug = true, .hint_maximized = true }});
    window_->drop_callback_ = [&](const vector<string>& paths) {
      drag_drop_files_ = paths;
    };

    scene_manager_.reset(new SceneManager);

    // load asssets
    scene_manager_->loadGltf(GLTF_MODEL_PATH("BoxTextured"));
    // scene_manager_->loadGltf(GLTF_MODEL_PATH("DamagedHelmet"));
    scene_manager_->loadGltf(GLTF_MODEL_PATH("Suzanne"));

    // panel system setup
    panel_manager_.reset(new PanelManager{*window_});
    panel_manager_->registerPanelType<StyleEditorPanel>();
    panel_manager_->registerPanelType<MetricsPanel>();
    panel_manager_->registerPanelType<DemoPanel>();

    panel_manager_->registerPanelType<ViewportPanel>([&]() {
        return new ViewportPanel{*scene_manager_}; });
    panel_manager_->registerPanelType<AssetsPanel>([&]() {
        return new AssetsPanel{*scene_manager_}; });

    panel_manager_->addPanelToRoot(kdtree::SplitType::HORIZONTAL, AssetsPanel::type);
    panel_manager_->addPanelToRoot(kdtree::SplitType::VERTICAL, DemoPanel::type, 0.6);
    panel_manager_->addPanelToRoot(kdtree::SplitType::HORIZONTAL, ViewportPanel::type, 0.3);
  }

  void UI_MainMenuBar() {
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

  void UI_DropSource() {
    if (!drag_drop_files_.empty()) {
      ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
      auto _ = ImScoped::DragDropSource(ImGuiDragDropFlags_SourceExtern);
      auto ptr = &drag_drop_files_;
      ImGui::SetDragDropPayload("CUSTOM_FILE", &ptr, (sizeof ptr));
      ImGui::Text("Click to drop files (ESC to cancel)");
      for (auto& file : drag_drop_files_) {
        auto _ = ImScoped::ID(&file);
        ImGui::BulletText("%s", file.data());
      }
      if (ImGui::IsKeyPressedMap(ImGuiKey_Escape)) {
        drag_drop_files_ = {};
      }
    }
  }

  void processUI() {
    UI_MainMenuBar();
    UI_DropSource();
    panel_manager_->processUI();
  }

  int exec() {
    while(!done_) {
      window_->newFrame();
      processUI();
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
