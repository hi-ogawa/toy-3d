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
    ivec2 mouse_position_imgui_delta;
    fvec3 mouse_position_scene;
    fvec3 mouse_position_scene_last;

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
    bool grid[3] = {0, 1, 0};
    bool axis[3] = {1, 1, 1};
    int axis_bound = 10;
    int grid_division = 3;

    fmat4 gizmo_xform = fmat4{1};
    utils::imgui::GizmoRotation gizmo_rotation;

    // debug
    bool overlay = true;
    bool debug_sceneCo_imguiCo = false;
    bool debug_ray_test = true;
    bool debug_gizmo = true;
  } ctx_;

  ViewportPanel(const SceneManager& mng) : mng_{mng} {
    framebuffer_.reset(new utils::gl::Framebuffer);
    camera_.transform_[3] = fvec4{0, 0, 4, 1};
  }

  // TODO: move these to ImGui3D
  ImVec2 convert_clipCo_to_imguiCo(const fvec4& p) {
    fvec2 q = fvec2{p.x, p.y} / p.w;                   // NDCo (without depth)
    return ImVec2{ctx_.ndCo_to_imguiCo * fvec3{q, 1}}; // imguiCo
  }

  ImVec2 convert_sceneCo_to_imguiCo(const fvec3& p1) {
    fvec4 p2 = ctx_.sceneCo_to_clipCo * fvec4{p1, 1};  // ClipCo
    return convert_clipCo_to_imguiCo(p2);
  }

  fvec3 convert_imguiCo_to_sceneCo(const ivec2& q1) {
    return ctx_.imguiCo_to_sceneCo * fvec3{q1, 1};
  }

  void setupContext() {
    ctx_.sceneCo_to_clipCo = camera_.get_sceneCo_to_clipCo();
    ctx_.ndCo_to_sceneCo = camera_.get_ndCo_to_sceneCo();
    ctx_.ndCo_to_imguiCo = get_ndCo_to_windowCo(content_offset_, content_size_);
    ctx_.imguiCo_to_ndCo = glm::inverse(ctx_.ndCo_to_imguiCo);
    ctx_.imguiCo_to_sceneCo = ctx_.ndCo_to_sceneCo * ctx_.imguiCo_to_ndCo;

    ctx_.mouse_position_imgui = ImGui::GetMousePos().glm();
    ctx_.mouse_position_imgui_delta = ImGui::GetIO().MouseDelta.glm();
    ctx_.mouse_position_scene = convert_imguiCo_to_sceneCo(ctx_.mouse_position_imgui);
    ctx_.mouse_position_scene_last = convert_imguiCo_to_sceneCo(ctx_.mouse_position_imgui - ctx_.mouse_position_imgui_delta);
    ctx_.camera_position = fvec3{camera_.transform_[3]};
    ctx_.mouse_direction = ctx_.mouse_position_scene - ctx_.camera_position;

    ctx_.imgui3d = {
        draw_list_, &ctx_.camera_position, &ctx_.mouse_position_scene, &ctx_.mouse_position_scene_last,
        &ctx_.sceneCo_to_clipCo, &ctx_.ndCo_to_imguiCo};

    ctx_.gizmo_rotation.imgui3d = &ctx_.imgui3d;
    ctx_.gizmo_rotation.xform_ = &ctx_.gizmo_xform;
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
      if (ImGui::CollapsingHeader("Size/Offset")) {
        ImGui::Columns(2, __FILE__, true);
        _drawColumns({
            {"offset_",         &offset_        },
            {"size_",           &size_          },
            {"content_offset_", &content_offset_},
            {"content_size_",   &content_size_  },
        });
        ImGui::Columns(1);
      };
      if (ImGui::CollapsingHeader("Mouse")) {
        ImGui::Columns(2, __FILE__, true);
        _drawColumns({
            {"mouse (imgui)",    &ctx_.mouse_position_imgui },
        });
        ImGui::Columns(1);
        ImGui::InputFloat3("mouse (scene)", (float*)&ctx_.mouse_position_scene, 2, ImGuiInputTextFlags_ReadOnly);
      }
      if (ImGui::CollapsingHeader("Debug")) {
        ivec2 v = convert_sceneCo_to_imguiCo(ctx_.mouse_position_scene).glm();
        ImGui::Text("mouse (imgui -> scene -> imgui)");
        ImGui::InputInt2("###sceneCo_to_imguiCo", (int*)&v, ImGuiInputTextFlags_ReadOnly);

        ImGui::Text("Debug sceneCo <-> imguiCo"); ImGui::SameLine();
        ImGui::Checkbox("###scenCo<->imguiCo", &ctx_.debug_sceneCo_imguiCo);
      }
    }
  }

  void UI_Axes() {
    int B = ctx_.axis_bound;
    for (auto i : Range{3}) {
      if (!ctx_.axis[i]) { continue; }

      fvec3 p{0}; p[i] = 1;
      fvec3 p1 = p * (float)B;
      fvec3 p2 = - p1;
      ctx_.imgui3d.addLine({p1, p2}, fvec4{p, .4f});
    }
  }

  void UI_GridPlanes() {
    int B = ctx_.axis_bound;
    int D = ctx_.grid_division;

    for (auto i : Range{3}) {
      if (!ctx_.grid[i]) { continue; }
      auto j = (i + 1) % 3;
      auto k = (i + 2) % 3;

      for (auto s : Range{-B, B + 1}) {
        // integral coords
        fvec3 p1_a; p1_a[i] = 0, p1_a[j] = s, p1_a[k] =  B;
        fvec3 p2_a; p2_a[i] = 0, p2_a[j] = s, p2_a[k] = -B;
        fvec3 p1_b; p1_b[i] = 0, p1_b[k] = s, p1_b[j] =  B;
        fvec3 p2_b; p2_b[i] = 0, p2_b[k] = s, p2_b[j] = -B;
        ctx_.imgui3d.addLine({p1_a, p2_a}, {1, 1, 1, .3});
        ctx_.imgui3d.addLine({p1_b, p2_b}, {1, 1, 1, .3});

        if (s == B) { break; } // skip last fractional grids
        for (auto l : Range{1, D}) {
          // fractional coords
          float f = (float)l / D;
          fvec3 q1_a; q1_a[i] = 0, q1_a[j] = s + f, q1_a[k] =  B;
          fvec3 q2_a; q2_a[i] = 0, q2_a[j] = s + f, q2_a[k] = -B;
          fvec3 q1_b; q1_b[i] = 0, q1_b[k] = s + f, q1_b[j] =  B;
          fvec3 q2_b; q2_b[i] = 0, q2_b[k] = s + f, q2_b[j] = -B;
          ctx_.imgui3d.addLine({q1_a, q2_a}, {1, 1, 1, .15});
          ctx_.imgui3d.addLine({q1_b, q2_b}, {1, 1, 1, .15});
        }
      }
    }
  }

  void UI_Gizmo() {
    UI_GridPlanes();
    UI_Axes();

    if (ctx_.debug_gizmo) {
      ctx_.gizmo_rotation.use();

      auto [xform_s, xform_r, xform_t] = decomposeTransform(ctx_.gizmo_xform);
      if (ImGui::IsMouseDown(0)) {
        if (ImGui::GetIO().KeyShift) {
          {
            fvec3 axis = {1, 0, 0};
            float delta = gizmoControl_Translation1D(
                ctx_.mouse_position_scene, ctx_.mouse_position_scene_last,
                ctx_.camera_position, xform_t, axis);
            // xform_t += delta * axis;
          }
          {
            fvec3 u1 = {1, 0, 0};
            fvec3 u2 = {0, 1, 0};
            array<float, 2> delta = gizmoControl_Translation2D(
                ctx_.mouse_position_scene, ctx_.mouse_position_scene_last,
                ctx_.camera_position, xform_t, u1, u2);
            xform_t += delta[0] * u1 + delta[1] * u2;
          }
        }
        if (ImGui::GetIO().KeyAlt) {
          float delta_ratio = gizmoControl_Scale3D(
              ctx_.mouse_position_scene, ctx_.mouse_position_scene_last,
              ctx_.camera_position, xform_t);
          xform_s *= delta_ratio;
        }
      }
      ctx_.gizmo_xform = composeTransform(xform_s, xform_r, xform_t);
    }

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

    if (ctx_.debug_ray_test && ImGui::IsMouseDown(0)) {
      auto result = mng_.rayTest(ctx_.camera_position, ctx_.mouse_direction);
      if (result.hit) {
        vector<fvec3> ps = {result.face[0], result.face[1], result.face[2]};
        ctx_.imgui3d.addConvexFill(ps, {1, 0, 1, 1});
      }
    }

    if (ctx_.debug_sceneCo_imguiCo) {
      // Demo for sceneCo <-> imguiCo convertion
      fvec3 o = {0, 0, 0};
      fvec3 z = glm::normalize(ctx_.camera_position + 2.0f * ctx_.mouse_direction);
      fvec3 x = glm::normalize(glm::cross(fvec3{0, 1, 0}, z));;
      fvec3 y = glm::normalize(glm::cross(z, x));

      ImVec2 _o = convert_sceneCo_to_imguiCo(o / 2.0f);
      ImVec2 _x = convert_sceneCo_to_imguiCo(x / 2.0f);
      ImVec2 _y = convert_sceneCo_to_imguiCo(y / 2.0f);
      ImVec2 _z = convert_sceneCo_to_imguiCo(z / 2.0f);
      ImColor w = {1.f, 1.f, 1.f, 0.6f};
      ImColor r = {1.f, 0.f, 0.f, 0.6f};
      ImColor g = {0.f, 1.f, 0.f, 0.6f};
      ImColor b = {0.f, 0.f, 1.f, 0.6f};

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
    setupContext();

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
