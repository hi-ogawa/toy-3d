#pragma once

#include <imgui.h>
#include <imgui_scoped.h>
#include <glm/glm.hpp>
#include <glm/geometric.hpp>
#include <glm/ext/scalar_constants.hpp>
#include <glm/ext/matrix_clip_space.hpp>

#include "utils.hpp"

namespace toy { namespace utils {
namespace imgui {

//
// Transform component editor
//

enum class InputTransformFlag : uint8_t {
  Rotation_ExtrinsicXYZ   = 1 << 0,
  Rotation_UnitQuaternion = 1 << 1, // TODO
};

struct InputTransformContext {
  // For now, it seems easier to manage id by ourself for tracking activity,
  // than using ImGui's built-in facility (Group, GetActiveId, ItemHoverable, etc...)
  // since group will work for `IsItemActive` but not for `GetActiveID`
  void* active_id;
  fvec3 rdeg;
};

inline static InputTransformContext global_input_transform_context_;

inline bool InputTransform(
    fmat4& xform,
    InputTransformFlag flags = InputTransformFlag::Rotation_ExtrinsicXYZ,
    InputTransformContext& context = global_input_transform_context_) {
  auto id = (void*)&xform;
  auto _ = ImScoped::ID(id);
  auto [s, r, t] = decomposeTransform(xform);
  fvec3 rdeg = context.active_id == id ? context.rdeg : glm::degrees(r);
  bool changed = false;
  {
    auto _g = ImScoped::Group();
    changed |= ImGui::DragFloat3("Location",       (float*)&t,    .05);
    changed |= ImGui::DragFloat3("Rotation (deg)", (float*)&rdeg, .50);
    changed |= ImGui::DragFloat3("Scale",          (float*)&s,    .05);
  }
  if (ImGui::IsItemActivated()) {
    context.rdeg = rdeg;
    context.active_id = id;
  }
  if (ImGui::IsItemDeactivated()) {
    context.active_id = nullptr;
  }
  if (changed) {
    context.rdeg = rdeg;
    xform = composeTransform(s, glm::radians(rdeg), t);
  }
  return changed;
};


//
// Camera view interaction (experiment)
//

// TODO
// - [x] draw frame for viewport
// - [x] setup imgui helpers (math functions, constructor etc...)
// - [x] camera lookat xform and view projection
// - [x] draw axis
// - [x] draw grid
//   - [x] grid division
// - [x] camera interaction
//   - [x] rotation around vertical axis
//   - [x] zoom
//   - [x] move
// - [x] draw plane with imgui PathFillConvex
// - [x] plane hit testing
// - [x] 3d primitive clipping
//   - [x] theory (projective space, convexity preservation, etc...)
//   - [x] line
//   - [-] triangle
// - [x] explore imgui primitive anti-aliasing method
// - [x] draw sphere with great circles of axis section
// - [x] sphere surface hit testing with drawing normal and tangent surface
// - [@] fix camera pivot rotation interaction

struct CameraViewExperimentContext {
  fvec2 viewport = {400, 400};
  fvec3 position = {2.5, 2.5, 2.5}; // support spherical coord mode
  fvec3 lookat = {0, 0, 0};
  float yfov = 3.14 * 2 / 3;
  float znear = 0.01;
  float zfar = 100;
  int axis_bound = 5;
  int grid_division = 3;
  int show_axis[3] = {1, 1, 1};
  int show_grid[3] = {0, 1, 0};
  bool clip_line = true;
};

inline static CameraViewExperimentContext global_camera_view_experiment_context_;

inline void CameraViewExperiment(CameraViewExperimentContext& ctx = global_camera_view_experiment_context_) {
  namespace ig = ImGui;

  //
  // Property input
  //
  ig::DragFloat("yfov", &ctx.yfov, 0.01f);
  ig::InputInt("axis_bound", &ctx.axis_bound);
  ig::InputInt("grid_division", &ctx.grid_division);
  {
    auto _1 = ImScoped::StyleVar(ImGuiStyleVar_FrameRounding, 8);
    auto _2 = ImScoped::StyleVar(ImGuiStyleVar_GrabRounding,  8);
    ig::SliderInt3("show_axis", ctx.show_axis, 0, 1, "");
    ig::SliderInt3("show_grid", ctx.show_grid, 0, 1, "");
  }

  //
  // Transformation setup
  //
  fmat4 projection = glm::perspectiveRH_NO(ctx.yfov, ctx.viewport[0] / ctx.viewport[1], ctx.znear, ctx.zfar);
  fmat4 view_xform;
  {
    // look at origin from "position" with new frame x' y' z' such that:
    // z' = normalize(position)
    // x' = normalize(y [cross] z')    (TODO: take care z' ~ y)
    // y' = z' [cross] x'
    auto z = glm::normalize(ctx.position - ctx.lookat);
    auto x = glm::normalize(glm::cross({0.f, 1.f, 0.f}, z));
    auto y = glm::cross(z, x);
    view_xform = {
      fvec4{x, 0},
      fvec4{y, 0},
      fvec4{z, 0},
      fvec4{ctx.position, 1},
    };
  }
  fmat4 inv_view_xform = inverseTR(view_xform);

  //
  // Viewport setup
  //
  auto window = ig::GetCurrentWindow();
  auto draw = window->DrawList;
  auto io = ig::GetIO();
  ImRect bb(window->DC.CursorPos, window->DC.CursorPos + ctx.viewport);
  ig::ItemSize(bb);
  if (!ig::ItemAdd(bb, 0)) { return; };

  auto id = window->GetID(__FILE__);
  auto active = ig::ItemHoverable(ImRect(bb.Min, bb.Max), id);

  // frame rect
  ig::RenderFrame(bb.Min, bb.Max, ig::GetColorU32(ImGuiCol_FrameBg));

  // camera transform ui
  if (active && ig::IsMouseDragging(0)) {
    auto v = io.MouseDelta / ctx.viewport;

    // zoom in/out "lookat"
    if (ig::GetIO().KeyAlt) {
      constexpr float m = 1.5, M = 10;
      auto dl = -v.x * (M - m);
      auto dp = ctx.position - ctx.lookat;
      auto l = glm::length(dp);
      ctx.position = ctx.lookat + dp * glm::clamp(l + dl, m, M) / l;
    }
    // rotation around "lookat" (TODO: take care when position.z < 0)
    if (ig::GetIO().KeyCtrl) {
      fvec2 u = { v.x * 2.f * 3.14, v.y * 3.14 };
      if (u.x != 0 || u.y != 0) {
        // rotate camera wrt. instantaneous "lookat" frame
        // TODO:
        // It cannot go over vertical axis (this is due to view_xform's construction).
        // To support this, it view_xform should be directly tracked in context.
        float l = glm::length(ctx.position - ctx.lookat);
        fmat4 lookat_xform = view_xform * fmat4{{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, -l, 1}};
        fmat3 A = ExtrinsicEulerXYZ_to_SO3({-u.y, -u.x, 0});
        fvec4 new_camera_wrt_lookat = fvec4{A * fvec3{0, 0, l}, 1};
        fvec4 new_camera = lookat_xform * new_camera_wrt_lookat;
        ctx.position = fvec3{new_camera};
      }
    }
    // move "lookat"
    if (ig::GetIO().KeyShift) {
      auto x = fmat3{view_xform} * fvec3{1, 0, 0};
      auto y = fmat3{view_xform} * fvec3{0, 1, 0};
      ctx.lookat += (x * (-v.x) + y * (v.y)) * 4.f;
    }
  }

  auto clip_coord_to_window_coord = [&bb](const fvec4& v) -> ImVec2 {
    auto u = fvec2{v.x, v.y} / v.w;                                               //     ND coord
    auto w = ((fvec2{u.x, -u.y} + fvec2{1, 1}) / 2) * (bb.Max - bb.Min) + bb.Min; //     window coord
    return w;
  };

  auto project_3d = [&projection, &inv_view_xform, &clip_coord_to_window_coord](const fvec3& p) -> ImVec2 {
    auto v = projection * inv_view_xform * fvec4{p, 1};                           // aka clip coord
    return clip_coord_to_window_coord(v);
  };

  auto rev_project_3d = [&](const ImVec2& w) -> fvec3 {
    auto s = fvec2{projection[0][0], projection[1][1]};
    auto _u = ((w - bb.Min) / (bb.Max - bb.Min)) * 2 - ImVec2{1, 1};
    auto u = fvec2{_u.x, -_u.y};
    auto p = fvec3{view_xform * fvec4{u / s, -1, 1}};
    return p;
  };

  auto project_clip_line = [&](const fvec3& p, const fvec3& q) -> std::optional<std::pair<ImVec2, ImVec2>> {
    auto _p = projection * inv_view_xform * fvec4{p, 1};
    auto _q = projection * inv_view_xform * fvec4{q, 1};
    auto clip_p_q = hit::clipLineSegment(_p, _q);
    if (!clip_p_q) { return {}; }
    auto& [clip_p, clip_q] = *clip_p_q;
    return std::make_pair(clip_coord_to_window_coord(clip_p), clip_coord_to_window_coord(clip_q));
  };

  bool _debug_lookat = true;
  if (_debug_lookat) {
    draw->AddCircleFilled(project_3d(ctx.lookat), 3.f, ig::GetColorU32({1, 1, 1, 1}));
  }

  // Axis
  for (auto i : range(3)) {
    if (!ctx.show_axis[i]) { continue; }

    auto p1 = glm::fmat3{1}[i] * (float)ctx.axis_bound;
    auto p2 = - p1;

    auto w1 = project_3d(p1);
    auto w2 = project_3d(p2);

    auto color = ig::GetColorU32({p1.x, p1.y, p1.z, .8});
    if (ctx.clip_line) {
      auto clip_p1_p2 = project_clip_line(p1, p2);
      if (clip_p1_p2) {
        auto& [_p1, _p2] = *clip_p1_p2;
        draw->AddLine(_p1, _p2, color);
        draw->AddCircle(_p2, 4.f, color);       // negative end
        draw->AddCircleFilled(_p1, 4.f, color); // positive end
      }
    } else {
      draw->AddLine(w1, w2, color, 1.f);
      draw->AddCircle(w2, 4.f, color);       // negative end
      draw->AddCircleFilled(w1, 4.f, color); // positive end
    }
  }

  // Grid plane
  for (auto i : range(3)) {
    if (!ctx.show_grid[i]) { continue; }
    auto j = (i + 1) % 3;                        // e.g. i = 0, j = 1, k = 2
    auto k = (i + 2) % 3;
    auto B = ctx.axis_bound;
    fvec3 v{0}; v[k] = B;                        // e.g. {0, 0, B}
    for (auto s : range(-B, B + 1)) {
      fvec3 u{0}; u[j] = s;                      // e.g. {0, s, 0}
      fvec3 p1 = u + v;                          // e.g. {0, s, B}
      fvec3 p2 = u - v;                          // e.g. {0, s,-B}
      fvec3 q1{0}; q1[j] = p1[k]; q1[k] = p1[j]; // e.g. {0, B, s}
      fvec3 q2{0}; q2[j] = p2[k]; q2[k] = p2[j]; // e.g. {0,-B, s}
      //
      // e.g.
      //               p1 (s, B)
      //         |----------|----|
      //    q2   |          |    |   q1
      // (-B, s) |----------|----| (B, s)
      //         |          |    |          z
      //         |          |    |          |
      //         |----------|--- |          +-- y
      //               p2 (s,-B)
      //

      // integer grid
      auto color = ig::GetColorU32({1, 1, 1, .5});

      if (ctx.clip_line) {
        auto clip_p1_p2 = project_clip_line(p1, p2);
        auto clip_q1_q2 = project_clip_line(q1, q2);
        if (clip_p1_p2) { draw->AddLine(clip_p1_p2->first, clip_p1_p2->second, color); }
        if (clip_q1_q2) { draw->AddLine(clip_q1_q2->first, clip_q1_q2->second, color); }
      } else {
        draw->AddLine(project_3d(p1), project_3d(p2), color);
        draw->AddLine(project_3d(q1), project_3d(q2), color);
      }

      if (s == B) { continue; }

      // fractional grid
      auto D = ctx.grid_division;
      for (auto l : range(1, D)) {
        auto f = (float)l / D;
        fvec3 g{0}; g[j] = f;
        fvec3 h{0}; h[k] = f;
        auto color = ig::GetColorU32({1, 1, 1, .2});
        if (ctx.clip_line) {
          auto clip_p1_p2 = project_clip_line(p1 + g, p2 + g);
          auto clip_q1_q2 = project_clip_line(q1 + h, q2 + h);
          if (clip_p1_p2) { draw->AddLine(clip_p1_p2->first, clip_p1_p2->second, color); }
          if (clip_q1_q2) { draw->AddLine(clip_q1_q2->first, clip_q1_q2->second, color); }
        } else {
          draw->AddLine(project_3d(p1 + g), project_3d(p2 + g), color);
          draw->AddLine(project_3d(q1 + h), project_3d(q2 + h), color);
        }
      }
    }
  }

  // Plane at z = 1
  bool _show_plane = true;
  if (_show_plane) {
    fvec3 p[4] = { {1, 1, 1}, {-1, 1, 1}, {-1, -1, 1}, {1, -1, 1} };
    // ImGui applies anti-aliasing for CW face so we order points based on
    // whether plane faces to camera (i.e. position wrt plane's frame).
    fmat4 model_xform = { {1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 1, 0} };
    bool ccw = (inverseTR(model_xform) * fvec4(ctx.position, 1)).z > 0;
    draw->PathClear();
    for (auto i = (ccw ? 3 : 0); i != (ccw ? -1 : 4); i += (ccw ? -1 : 1)) {
      draw->PathLineTo(project_3d(p[i]));
    }
    draw->PathFillConvex(ig::GetColorU32({0, 0, 1, .8}));

    bool _debug_ccw = true;
    if (_debug_ccw) {
      draw->AddCircleFilled(
          project_3d({0, 0, 1}), 4.f,
          ig::GetColorU32(ccw ? ImVec4{1, 1, 0, 1} : ImVec4{1, 0, 1, 1}));
    }
  }

  // Mouse position in world frame
  bool _debug_rev_project = true;
  if (active && _debug_rev_project) {
    auto p = rev_project_3d(ig::GetMousePos());
    draw->AddCircleFilled(project_3d(p), 3.f, ig::GetColorU32({0, 1, 1, 1}));
  }

  // Hit testing plane
  if (active) {
    auto& camera = ctx.position;
    auto mouse_ray = rev_project_3d(ig::GetMousePos()) - camera;
    auto t = hit::Line_Plane(camera, mouse_ray, {0, 0, 1}, {0, 0, 1});
    if (t && *t > 0) {
      auto intersection =  camera + (*t) * mouse_ray;
      bool rect_hit; {
        auto v = intersection - fvec3{0, 0, 1};
        rect_hit = glm::max(glm::abs(v.x), glm::abs(v.y)) < 1;
      }
      draw->AddLine(
          project_3d(ctx.lookat), project_3d(intersection),
          rect_hit ? ig::GetColorU32({0, 1, 1, .8}) : ig::GetColorU32({0, 1, 1, .3}),
          rect_hit ? 3.f : 1.f);
    }
  }

  // Draw sphere (outline)
  {
    fvec3 c_model = {1, 0, 0}; // center
    float r = 1;             // radius
    fvec3 c = c_model - ctx.position; // location w.r.t camera point

    // Strategy 1.
    // - find tangental cone's base circle
    // - project circle's points and draw them
    {
      using std::cos, std::sin, std::asin;
      float pi = glm::pi<float>();
      float cone_half_angle = asin(r / glm::length(c));
      float theta = (pi / 2.f) + cone_half_angle;
      int ps_size = 48;
      fvec3 ps[ps_size];

      // cone base circle as "tilted" spherical coord
      fmat3 frame; {
        auto v1 = glm::normalize(c);
        auto v2 = glm::normalize(glm::cross(v1, {1, 0, 0}));
        auto v3 = glm::cross(v1, v2);
        frame = {v2, v3, v1};
      }
      for (auto i : range(ps_size)) {
        float phi = 2.f * pi * i / ps_size;
        fvec3 v = { sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta) };
        ps[i] = c_model + frame * r * v;
      }

      // project and draw outline (by construction, it's already CW)
      draw->PathClear();
      for (auto i : range(ps_size)) { draw->PathLineTo(project_3d(ps[i])); }
      draw->PathFillConvex(ig::GetColorU32({1, 1, 1, .8}));
    }

    // Strategy 2. (TODO)
    // - analytically find projected ellipse parameter (center, major/minor axis, etc...)
    //   - some Dandelin sphere trick?
    // - draw ellipse
  }

  // Draw sphere's great circles
  {
    float pi = glm::pi<float>();
    fvec3 c_model = {1, 0, 0}; // center
    float r = 1;               // radius

    int num_segments = 48;

    for (auto i : range(3)) {
      auto j = (i + 1) % 3;
      auto k = (i + 2) % 3;
      fvec3 u{0}; u[j] = 1;
      fvec3 v{0}; v[k] = 1;

      draw->PathClear();
      for (auto i : range(num_segments)) {
        float t = 2 * pi * i / num_segments;
        auto p = c_model + std::cos(t) * u + std::sin(t) * v;
        draw->PathLineTo(project_3d(p));
      }
      fvec3 col{0}; col[i] = 1;
      draw->PathStroke(ig::GetColorU32({col[0], col[1], col[2], .5}), true, 2.f);
    }
  }

  // Hit test sphere surface and draw tangent plane quad
  if (active) {
    fvec3 center = {1, 0, 0};
    float radius = 1.f;

    fvec3& camera = ctx.position;
    fvec3 mouse_ray = rev_project_3d(ig::GetMousePos()) - camera;

    auto t1_t2 = hit::Line_Sphere(camera, mouse_ray, center, radius);
    if (t1_t2) {
      auto [t, _] = *t1_t2;
      fvec3 intersection = camera + t * mouse_ray;
      fvec3 normal = glm::normalize(intersection - center);

      // tangent space basis
      fvec3 u = glm::normalize(glm::cross(normal, {0, -1, 0}));
      fvec3 v = glm::normalize(glm::cross(normal, u));
      glm::fmat2x3 F = {u, v};

      float scale = 0.5;
      fvec2 plane[4] = { {1, 1}, {1, -1}, {-1, -1}, {-1, 1} };

      draw->PathClear();
      for (auto i : range(4)) {
        fvec3 p = intersection + F * scale * plane[i];
        draw->PathLineTo(project_3d(p));
      }
      draw->PathFillConvex(ig::GetColorU32({0, 1, 1, .8}));
    }
  }
}

} // imgui
} } // toy::utils
