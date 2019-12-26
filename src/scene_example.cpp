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


struct SceneManager {
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

  MeshBVH::RayTestResult rayTest(const fvec3& src, const fvec3& dir) const {
    MeshBVH::RayTestResult result = { .hit = false, .t = FLT_MAX };
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
    }
    return result;
  }
};


//
// UIs
//

struct ViewportPanel : Panel {
  constexpr static const char* type = "Viewport";
  unique_ptr<utils::gl::Framebuffer> framebuffer_;
  const SceneManager& mng_;
  ImDrawList* draw_list_;
  Camera camera_;

  struct DrawChannel { enum { // scoped untyped enum
    SCENE_IMAGE = 0,
    GIZMO,
    OVERLAY,
    CHANNELS_COUNT,
  };};
  struct UIContext {
    // refreshed on new frame
    ivec2 mouse_position_imgui;
    fvec3 mouse_position_scene;
    fmat3 ndCo_to_imguiCo;           // 2d homog transform (here, ndCo without depth)
    fmat3 imguiCo_to_ndCo;
    fmat4 sceneCo_to_clipCo;
    glm::fmat3x4 ndCo_to_sceneCo;    // injection to SceneCo as CameraCo at z = -1
    glm::fmat3x4 imguiCo_to_sceneCo; // ndCo_to_sceneCo * imguiCo_to_ndCo

    // some shortcut
    fvec3 camera_position;
    fvec3 mouse_direction; // mouse_position_scene - camera_position

    // UI state
    fvec3 pivot = {0, 0, 0};

    // debug
    bool overlay = true;
    bool debug_sceneCo_imguiCo = true;
    bool debug_ray_test = true;
  } ctx_;

  ViewportPanel(const SceneManager& mng) : mng_{mng} {
    framebuffer_.reset(new utils::gl::Framebuffer);
    camera_.transform_[3] = fvec4{0, 0, 4, 1};
  }

  ivec2 convert_sceneCo_to_imguiCo(fvec3 p1) {
    fvec4 p2 = ctx_.sceneCo_to_clipCo * fvec4{p1, 1};  // ClipCo
    fvec2 p3 = fvec2{p2.x, p2.y} / p2.w;               // NDCo (without depth)
    return ivec2{ctx_.ndCo_to_imguiCo * fvec3{p3, 1}};
  }

  fvec3 convert_imguiCo_to_sceneCo(ivec2 q1) {
    return ctx_.imguiCo_to_sceneCo * fvec3{q1, 1};
  }

  void _setupContext() {
    // TODO: move this computation to utils or utils_imgui
    {
      fmat4 projection = camera_.getPerspectiveProjection();
      ctx_.sceneCo_to_clipCo = projection * inverseTR(camera_.transform_);

      float sx = projection[0][0], sy = projection[1][1], n = 1; // TODO: n = camera_.znear_ is more useful?
      glm::fmat3x4 ndCo_to_CameraCo = {
        n/sx,    0,  0, 0,
           0, n/sy,  0, 0,
           0,    0, -n, 1,  // CameraCo at z = -1
      };
      ctx_.ndCo_to_sceneCo = camera_.transform_ * ndCo_to_CameraCo;
    }
    {
      float L = content_offset_.x;
      float T = content_offset_.y;
      float W = content_size_.x;
      float H = content_size_.y;
      // Derived via (T: translation, S: scale)
      // T(L, T) * S(W, H) * S(1/2, 1/2) * T(1/2, 1/2) * S(1, -1)
      ctx_.ndCo_to_imguiCo = {
            W/2,          0,    0,
              0,       -H/2,    0,
        L + W/2,    T + H/2,    1,
      };
      ctx_.imguiCo_to_ndCo = glm::inverse(ctx_.ndCo_to_imguiCo);
    }
    ctx_.imguiCo_to_sceneCo = ctx_.ndCo_to_sceneCo * ctx_.imguiCo_to_ndCo;
    ctx_.mouse_position_imgui = ImGui::GetMousePos().glm();
    ctx_.mouse_position_scene = convert_imguiCo_to_sceneCo(ctx_.mouse_position_imgui);
    ctx_.camera_position = fvec3{camera_.transform_[3]};
    ctx_.mouse_direction = ctx_.mouse_position_scene - ctx_.camera_position;
  }

  void processMenu() override {
    if (auto _ = ImScoped::Menu("Edit")) {
      if (ImGui::MenuItem("Overlay", nullptr, ctx_.overlay)) {
        ctx_.overlay = !ctx_.overlay;
      }
    }
  }

  void UI_Overlay() {
    // NOTE: ImGui::Columns internally use `Channels`,
    //       so instantiating them without a child window would cause conflict with our use.
    if (ctx_.overlay) {
      auto _ = ImScoped::Child(__FILE__, ImVec2{content_size_.x / 2.5f, 0.f}, false, ImGuiWindowFlags_NoMove);
      auto _drawColumns = [](const std::map<const char*, ivec2*>& rows) {
        for (auto [name, data] : rows) {
          ImGui::TextUnformatted(name);
          ImGui::NextColumn();
          ImGui::SetNextItemWidth(-1);
          ImGui::InputInt2(fmt::format("###{}", name).data(), (int*)data, ImGuiInputTextFlags_ReadOnly);
          ImGui::NextColumn();
        }
      };
      if (ImGui::CollapsingHeader("Size/Offset", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Columns(2, __FILE__, true);
        _drawColumns({
            {"offset_",         &offset_        },
            {"size_",           &size_          },
            {"content_offset_", &content_offset_},
            {"content_size_",   &content_size_  },
        });
        ImGui::Columns(1);
      };
      if (ImGui::CollapsingHeader("Mouse", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Columns(2, __FILE__, true);
        _drawColumns({
            {"mouse (imgui)",    &ctx_.mouse_position_imgui },
        });
        ImGui::Columns(1);
        ImGui::InputFloat3("mouse (scene)", (float*)&ctx_.mouse_position_scene, 2, ImGuiInputTextFlags_ReadOnly);
      }
      if (ImGui::CollapsingHeader("Debug", ImGuiTreeNodeFlags_DefaultOpen)) {
        ivec2 v = convert_sceneCo_to_imguiCo(ctx_.mouse_position_scene);
        ImGui::Text("mouse (imgui -> scene -> imgui)");
        ImGui::InputInt2("###sceneCo_to_imguiCo", (int*)&v, ImGuiInputTextFlags_ReadOnly);

        ImGui::Text("Debug sceneCo <-> imguiCo"); ImGui::SameLine();
        ImGui::Checkbox("###scenCo<->imguiCo", &ctx_.debug_sceneCo_imguiCo);
      }
    }
  }

  void UI_Gizmo() {
    if (ImGui::IsMouseDown(0)) {
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
    if (ctx_.debug_ray_test && ImGui::IsMouseDown(0)) {
      auto result = mng_.rayTest(ctx_.camera_position, ctx_.mouse_direction);
      draw_list_->AddCircleFilled(
          ImVec2{convert_sceneCo_to_imguiCo(ctx_.mouse_position_scene)}, 4.0f,
          result.hit ? IM_COL32(255, 255, 0, 255) : IM_COL32(255, 255, 255, 255));

      if (result.hit) {
        draw_list_->PathClear();
        for (auto i : Range{3}) {
          draw_list_->PathLineTo(ImVec2{convert_sceneCo_to_imguiCo(result.face[i])});
        }
        draw_list_->PathFillConvex(ImGui::GetColorU32({1, 0, 1, .8}));
      }
    }
    if (ctx_.debug_sceneCo_imguiCo) {
      // Demo for sceneCo <-> imguiCo convertion
      fvec3 o = {0, 0, 0};
      fvec3 z = glm::normalize(ctx_.camera_position + 2.0f * ctx_.mouse_direction);
      fvec3 x = glm::normalize(glm::cross(fvec3{0, 1, 0}, z));;
      fvec3 y = glm::normalize(glm::cross(z, x));

      auto _o = ImVec2{convert_sceneCo_to_imguiCo(o / 2.0f)};
      auto _x = ImVec2{convert_sceneCo_to_imguiCo(x / 2.0f)};
      auto _y = ImVec2{convert_sceneCo_to_imguiCo(y / 2.0f)};
      auto _z = ImVec2{convert_sceneCo_to_imguiCo(z / 2.0f)};
      ImU32 w = ImGui::GetColorU32({1, 1, 1, 0.6f});
      ImU32 r = ImGui::GetColorU32({1, 0, 0, 0.6f});
      ImU32 g = ImGui::GetColorU32({0, 1, 0, 0.6f});
      ImU32 b = ImGui::GetColorU32({0, 0, 1, 0.6f});

      draw_list_->AddLine(_o, _x, r);
      draw_list_->AddLine(_o, _y, g);
      draw_list_->AddLine(_o, _z, b);
      draw_list_->AddCircleFilled(_o, 4.0f, w);
      draw_list_->AddCircleFilled(_x, 4.0f, r);
      draw_list_->AddCircleFilled(_y, 4.0f, g);
      draw_list_->AddCircleFilled(_z, 4.0f, b);
    }
  }

  void processUI() override {
    // Setup
    camera_.aspect_ratio_ = (float)content_size_[0] / content_size_[1];
    framebuffer_->setSize({content_size_[0], content_size_[1]});
    draw_list_ = ImGui::GetWindowDrawList();
    _setupContext();

    // Draw UI
    draw_list_->ChannelsSplit(DrawChannel::CHANNELS_COUNT);
    draw_list_->ChannelsSetCurrent(DrawChannel::OVERLAY);
    UI_Overlay();
    draw_list_->ChannelsSetCurrent(DrawChannel::GIZMO);
    UI_Gizmo();
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

  void processUI() override {
    if (ImGui::BeginTabBar(__FILE__)){
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
    scene_manager_->loadGltf(GLTF_MODEL_PATH("DamagedHelmet"));
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
