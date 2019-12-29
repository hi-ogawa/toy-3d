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

// TODO: Rename to ImGui3D
struct DrawList3D {
  ImDrawList* draw_list;
  const fvec3* camera_position;
  const fvec3* mouse_position;      // in sceneCo
  const fvec3* mouse_position_last; // in sceneCo
  const fmat4* sceneCo_to_clipCo;
  const fmat3* ndCo_to_imguiCo;

  ImVec2 clipCo_to_imguiCo(const fvec4& p) {
    fvec2 q = fvec2{p.x, p.y} / p.w;                 // NDCo (without depth)
    return ImVec2{(*ndCo_to_imguiCo) * fvec3{q, 1}}; // imguiCo
  }

  ImVec2 sceneCo_to_imguiCo(const fvec3& p1) {
    fvec4 p2 = (*sceneCo_to_clipCo) * fvec4{p1, 1};  // ClipCo
    return clipCo_to_imguiCo(p2);
  }

  void addLine(
      const std::array<fvec3, 2>& ps,
      const fvec4& color,
      float thickness = 1.0f) {
    auto cp0_cp1 = hit::clip4D_Line_ClipVolume({
        (*sceneCo_to_clipCo) * fvec4{ps[0], 1},
        (*sceneCo_to_clipCo) * fvec4{ps[1], 1}});
    if (!cp0_cp1) { return; }

    std::array<ImVec2, 2> ip0_ip1 = {
        clipCo_to_imguiCo((*cp0_cp1)[0]),
        clipCo_to_imguiCo((*cp0_cp1)[1])};
    draw_list->AddLine(ip0_ip1[0], ip0_ip1[1], ImColor{ImVec4{color}}, thickness);
  }

  // NOTE: not used
  vector<array<fvec4, 2>> _clipPathPoints(const vector<fvec3>& ps, bool closed = false) {
    size_t N = ps.size();
    vector<array<fvec4, 2>> lines;
    for (auto i : Range{closed ? N : N - 1}) {
      auto cp0_cp1 = hit::clip4D_Line_ClipVolume({
          (*sceneCo_to_clipCo) * fvec4{ps[i],           1},
          (*sceneCo_to_clipCo) * fvec4{ps[(i + 1) % N], 1}});
      if (!cp0_cp1) { continue; }
      lines.push_back(*cp0_cp1);
    }
    return lines;
  }

  // NOTE: thought this might be useful to hitTest in ImGui coordinate but not utilized yet...
  // @return possibly disconnected lines due to clipping
  vector<array<ImVec2, 2>> getImguiCo_Path(const vector<fvec3>& ps, bool closed) {
    vector<array<ImVec2, 2>> lines;
    size_t N = ps.size();
    for (auto i : Range{closed ? N : N - 1}) {
      // Clip
      std::optional<array<fvec4, 2>> cp0_cp1 =
          hit::clip4D_Line_ClipVolume({
              (*sceneCo_to_clipCo) * fvec4{ps[i],           1},
              (*sceneCo_to_clipCo) * fvec4{ps[(i + 1) % N], 1}});
      if (!cp0_cp1) { continue; }

      // Project
      lines.push_back({
          clipCo_to_imguiCo((*cp0_cp1)[0]),
          clipCo_to_imguiCo((*cp0_cp1)[1])});
    }
    return lines;
  }

  // @return "clip-project"-ed convex vertex points
  vector<ImVec2> getImguiCo_ConvexFill(const vector<fvec3>& ps) {
    vector<ImVec2> result = {};

    // Clip
    vector<fvec4> qs{ps.size()};
    for (auto i : Range{ps.size()}) {
      qs[i] = (*sceneCo_to_clipCo) * fvec4{ps[i], 1};
    }
    vector<fvec4> cs = hit::clip4D_ConvexPoly_ClipVolume(qs);
    if (cs.size() < 3) { return result; }

    // Project
    result.resize(cs.size());
    for (auto i : Range{cs.size()}) {
      result[i] = clipCo_to_imguiCo(cs[i]);
    }
    return result;
  }

  void addPath(const vector<fvec3>& ps, const fvec4& color, float thickness = 1.0f, bool closed = false) {
    size_t N = ps.size();
    for (auto i : Range{closed ? N : N - 1}) {
      addLine({ps[i], ps[(i + 1) % N]}, color, thickness);
    }
  }

  void addConvexFill(const vector<fvec3>& ps, const fvec4& color) {
    vector<fvec4> qs{ps.size()};
    for (auto i : Range{ps.size()}) {
      qs[i] = (*sceneCo_to_clipCo) * fvec4{ps[i], 1};
    }
    vector<fvec4> cs = hit::clip4D_ConvexPoly_ClipVolume(qs);
    if (cs.size() < 3) { return; }

    // Fix orientation for PathFillConvex's AA
    bool flip; {
      ImVec2 v1 = clipCo_to_imguiCo(cs[1] - cs[0]);
      ImVec2 v2 = clipCo_to_imguiCo(cs[2] - cs[0]);
      flip = v1.x * v2. y - v1.y * v2.x < 0;
    }

    draw_list->PathClear();
    if (flip) {
      for (auto& c : Reverse{cs})
        draw_list->PathLineTo(clipCo_to_imguiCo(c));
    } else {
      for (auto& c : cs)
        draw_list->PathLineTo(clipCo_to_imguiCo(c));
    }
    draw_list->PathFillConvex(ImColor{ImVec4{color}});
  }

  vector<fvec3> _makeCirclePoints(
      const fvec3& center, float radius, const fvec3& axis, int num_segments) {
    float pi = glm::pi<float>();
    fmat4 xform = lookatTransform({0, 0, 0}, axis, getNonParallel(axis));
    fvec3 u = fvec3{xform[0]};
    fvec3 v = fvec3{xform[1]};

    vector<fvec3> ps{(size_t)num_segments};
    for (auto i : Range{num_segments}) {
      using std::cos, std::sin;
      float t = 2 * pi * i / num_segments;
      auto& c = center;
      auto& r = radius;
      ps[i] = c + r * cos(t) * u + r * sin(t) * v;
    }
    return ps;
  }

  vector<fvec3> _makeArcPoints(
      const fvec3& center, float radius, const fvec3& v1, const fvec3& v2,
      int arc_begin, int arc_end, int num_segments) {
    float pi = glm::pi<float>();

    vector<fvec3> ps; ps.reserve(arc_end - arc_begin);
    for (auto i : Range{arc_begin, arc_end}) {
      using std::cos, std::sin;
      float t = 2 * pi * i / num_segments;
      auto& c = center;
      auto& r = radius;
      ps.push_back(c + r * cos(t) * v1 + r * sin(t) * v2);
    }
    return ps;
  }

  vector<fvec3> _makeArcPoints_v2(
      const fvec3& center, float radius, const fvec3& v1, const fvec3& v2,
      float arc_begin, float arc_end, int num_segments) {
    float pi = glm::pi<float>();

    vector<fvec3> ps; ps.resize(num_segments + 1);
    for (auto i : Range{num_segments + 1}) {
      using std::cos, std::sin;
      float t = arc_begin + (arc_end - arc_begin) * i / num_segments;
      auto& c = center;
      auto& r = radius;
      ps[i] = c + r * cos(t) * v1 + r * sin(t) * v2;
    }
    return ps;
  }

  // TODO: migrate _makeArcPoints_v2
  void addArc(
      const fvec3& center, float radius, const fvec3& v1, const fvec3& v2, const fvec4& color,
      int arc_begin, int arc_end, int num_segments, float thickness = 1.0f) {
    vector<fvec3> ps = _makeArcPoints(center, radius, v1, v2, arc_begin, arc_end, num_segments);
    vector<array<ImVec2, 2>> qs = getImguiCo_Path(ps, /*closed*/ false);
    for (auto& [p1, p2] : qs) {
      draw_list->AddLine(p1, p2, ImColor{ImVec4{color}}, thickness);
    }
  }

  void addArcFill(
      const fvec3& center, float radius, const fvec3& v1, const fvec3& v2, const fvec4& color,
      float arc_begin, float arc_end, int num_segments) {
    vector<fvec3> ps = _makeArcPoints_v2(center, radius, v1, v2, arc_begin, arc_end, num_segments);
    ps.push_back(center);
    addConvexFill(ps, color);
  }

  void addCircle(
      const fvec3& center, float radius, const fvec3& axis,
      const fvec4& color, float thickness = 1.0f, int num_segments = 48) {
    addPath(_makeCirclePoints(center, radius, axis, num_segments), color, thickness, /*closed*/ true);
  }

  void addCircleFill(
      const fvec3& center, float radius, const fvec3& axis,
      const fvec4& color, int num_segments = 48) {
    addConvexFill(_makeCirclePoints(center, radius, axis, num_segments), color);
  }

  void addSphere(const fvec3& center, float radius, const fvec4& color, int num_segments = 48) {
    auto [
      cone_base_center, // fvec3
      cone_base_radius  // float
    ] = utils::getTangentCone(*camera_position, center, radius);
    fvec3 camera_to_center = center - (*camera_position);
    addCircleFill(cone_base_center, cone_base_radius, camera_to_center, color, num_segments);
  }

  void addSphereBorder(
      const fvec3& center, float radius, const fvec4& color,
      float thickness = 1.0f, int num_segments = 48) {
    auto [
      cone_base_center, // fvec3
      cone_base_radius  // float
    ] = utils::getTangentCone(*camera_position, center, radius);
    fvec3 camera_to_center = center - (*camera_position);
    addCircle(cone_base_center, cone_base_radius, camera_to_center, color, thickness, num_segments);
  }
};

// TODO:
// - [x] fixed step change
// - [x] draw diff angle
// - [x] cancel by escape
// - [ ] scene axial rotation (currently local frame mode)
struct GizmoRotation {
  DrawList3D* imgui3d;
  fmat4* xform_;

  // state
  uint8_t axis_; // x: 0, y: 1, z: 2
  bool active_  = false;
  bool hovered_ = false;
  array<fvec3, 3> plane_hits_;         // setup on each frame `handleEvent`
  array<fvec3, 3> plane_hits_initial_; // setup on activate
  fmat4 xform_initial_;                // setup on activate

  // parameters, constants
  bool local; // todo (currently local frame mode)
  float step = glm::pi<float>() * 1 / 180;
  float radius = 1.0;
  float arc_radius = 0.95;
  int N = 48;
  int arc_begin = - N / 4;
  int arc_end   = + N / 4 + 1;


  void handleEvent() {
    auto [xform_s, xform_r, xform_t] = decomposeTransform_v2(*xform_);

    // Update plane_hits_ (hit test on sphere sections (i.e. 3 disks))
    hovered_ = false;
    {
      // project mouse to plane + disk hit test
      bool disk_hits[3] = {false, false, false};
      float plane_depths[3];
      const fvec3& p = *imgui3d->mouse_position;
      const fvec3& q = *imgui3d->camera_position;
      fvec3 v = p - q;
      for (auto i : Range{3}) {
        std::optional<float> t = hit::Line_Plane(q, v, xform_t, xform_r[i]);
        if (t) {
          plane_depths[i] = *t;
          plane_hits_[i] = q + (*t) * v;
          disk_hits[i] = glm::length(plane_hits_[i] - xform_t) < radius;
        }
      }
      // find disk at the front
      float min_depth = FLT_MAX;
      for (auto i : Range{3}) {
        if (!disk_hits[i] || plane_depths[i] < 0) { continue; }

        if (plane_depths[i] <= min_depth) {
          min_depth = plane_depths[i];
          hovered_ = true;

          // Update `axis_` only when not `active_`
          if (!active_) {
            axis_ = i;
          }
        }
      }
    }

    // "immidiate update" during active
    if (active_) {
      auto [_, xform_r_init, __] = decomposeTransform_v2(xform_initial_);
      fvec3 Z = xform_r[axis_];
      fvec3 v_init = plane_hits_initial_[axis_] - xform_t;
      fvec3 v      = plane_hits_[axis_]         - xform_t;
      fvec3 X = glm::normalize(v_init);
      fvec3 Y = glm::normalize(glm::cross(Z, X));
      float diff = std::atan2(dot(Y, v), dot(X, v));

      // apply "fixed step" mode
      diff = diff - std::fmod(diff, step);

      fvec3 angles = {0, 0, 0}; angles[axis_] = diff;
      fmat3 new_xform_r = xform_r_init * ExtrinsicEulerXYZ_to_SO3(angles);
      *xform_ = composeTransform_v2(xform_s, new_xform_r, xform_t);
    }

    // deactivate and reset to initial when escape is pressed
    if (active_ && ImGui::IsKeyPressedMap(ImGuiKey_Escape)) {
      *xform_ = xform_initial_;
      active_ = false;
    }

    // activate on click
    if (ImGui::GetIO().MouseClicked[0]) {
      if (!active_ && hovered_) {
        active_ = true;
        xform_initial_ = *xform_;
        plane_hits_initial_ = plane_hits_;
      }
    }

    // deactivate on mouse up
    if (active_ && !ImGui::GetIO().MouseDown[0]) {
      active_ = false;
    }
  }

  void draw() {
    auto [_, xform_r, xform_t] = decomposeTransform_v2(*xform_);

    // small dots on center
    imgui3d->draw_list->AddCircleFilled(imgui3d->sceneCo_to_imguiCo(xform_t), 3, ImColor{fvec4{1, 1, 0, 1}});

    // sphere with border at the position
    imgui3d->addSphere(xform_t, radius, {1, 1, 1, .2});
    imgui3d->addSphereBorder(xform_t, radius, {1, 1, 1, .6}, 2);

    // axix-orthogonal section of sphere
    {
      fvec3 v = (*imgui3d->camera_position) - xform_t;
      for (auto i : Range{3}) {
        fmat3 lookat = fmat3{lookatTransform({0, 0, 0}, xform_r[i], v)};
        if (axis_ == i && active_) {
          imgui3d->addCircleFill(xform_t, arc_radius, xform_r[i], {1, 1, 0, .2});
          imgui3d->addCircle(    xform_t, arc_radius, xform_r[i], {1, 1, 0, .5}, 2);

          // radial line passing initial/current mouse
          fvec3 v_init = glm::normalize(plane_hits_initial_[i] - xform_t) * arc_radius;
          fvec3 v      = glm::normalize(plane_hits_[i]         - xform_t) * arc_radius;
          imgui3d->addLine({xform_t, xform_t + v_init}, {1, 1, 0, .8});
          imgui3d->addLine({xform_t, xform_t + v     }, {1, 1, 0, .8});

          // Fill current diff angle
          fvec3 Z = xform_r[axis_];
          fvec3 X = glm::normalize(v_init);
          fvec3 Y = glm::normalize(glm::cross(Z, X));
          float diff = std::atan2(dot(Y, v), dot(X, v));
          imgui3d->addArcFill(xform_t, arc_radius, X, Y, {1, 1, 0, .5}, 0, diff, 24);

        } else if (axis_ == i && hovered_) {
          fvec3 color = {0, 0, 0}; color[i] = 1;
          imgui3d->addCircleFill(xform_t, arc_radius, xform_r[i], fvec4{color, 0.2});
          imgui3d->addArc(xform_t, arc_radius, lookat[1], -lookat[0], fvec4{color, 0.5}, arc_begin, arc_end, N, 2);

          // radial line passing current mouse
          fvec3 v = glm::normalize(plane_hits_[i] - xform_t) * arc_radius;
          imgui3d->addLine({xform_t, xform_t + v}, fvec4{color, 0.5}, 2);

        } else {
          // draw only half arc towards camera
          fvec4 color = {0, 0, 0, 0.5}; color[i] = 1;
          imgui3d->addArc(xform_t, arc_radius, lookat[1], -lookat[0], color, arc_begin, arc_end, N, 2);
        }
      }
    }
  }

  void use() {
    handleEvent();
    draw();
  }
};

// TODO:
// - [ ] non-local frame mode
struct GizmoTranslation {
  DrawList3D* imgui3d;
  fmat4* xform_;

  // state
  uint8_t axis_; // x: 0, y: 1, z: 2
  bool plane_mode_ = false;            // mode for 2D translation orthgonal to "axis_"
  bool active_  = false;               // "active_ && !hovered_" is a legitimate state
  bool hovered_ = false;
  array<fvec3, 3> axis_hits_;          // setup on each frame during `handleEvent` (it's not "hit", but "closest point")
  array<fvec3, 3> plane_hits_;         // setup on each frame during `handleEvent`
  array<fvec3, 3> axis_hits_initial_;  // setup on activate (quite redundant but code looks cleaner)
  array<fvec3, 3> plane_hits_initial_; // setup on activate
  fmat4 xform_initial_;                // setup on activate

  // parameters, constants
  bool local; // todo (currently local frame mode only)
  float step = 0.01;
  float len1 = 1;               // arrow length
  float len2 = 0.1, len3 = 0.4; // rect side start/end


  void handleEvent() {
    auto [xform_s, xform_r, xform_t] = decomposeTransform_v2(*xform_);

    // Hit testing (update hovered_, axis_hits_, plane_hits_)
    hovered_ = false;
    {
      // project mouse to axes and planes
      bool arrow_hits[3]    = {false, false, false};
      bool rect_hits[3]     = {false, false, false};
      float axis_depths[3]  = {FLT_MAX, FLT_MAX, FLT_MAX};
      float plane_depths[3] = {FLT_MAX, FLT_MAX, FLT_MAX};
      {
        const fvec3& p = *imgui3d->mouse_position;
        const fvec3& q = *imgui3d->camera_position;
        fvec3 v = p - q;
        for (auto i : Range{3}) {
          int j = (i + 1) % 3;
          int k = (i + 2) % 3;
          {
            // project to plane
            std::optional<float> t = hit::Line_Plane(q, v, xform_t, xform_r[i]);
            if (t) {
              plane_depths[i] = *t;
              plane_hits_[i] = q + (*t) * v;

              // hit test rectangle
              fvec3 r = glm::inverse(xform_r) * (plane_hits_[i] - xform_t);
              if (len2 <= r[j] && r[j] <= len3 && len2 <= r[k] && r[k] <= len3) {
                rect_hits[i] = true;
              }
            }
          }
          {
            // project to axis (i.e. closest point)
            float t = hit::Line_Line(q, v, xform_t, xform_r[i]);
            axis_depths[i] = t;
            axis_hits_[i] = q + t * v;

            // hit test arrow
            float s = hit::Line_Point(xform_t, xform_r[i], axis_hits_[i]);
            fvec3 r = xform_t + s * xform_r[i];
            if (0 <= s && s <= len1 && glm::length(axis_hits_[i] - r) < len2) {
              arrow_hits[i] = true;
            }
          }
        }
      }

      // Update "hovered_" based on current "arrow_hits" and "rect_hits".
      // Also we change/setup "axis_" and "plane_mode_" only when not "active_".
      float min_depth = FLT_MAX;
      for (auto i : Range{3}) {
        if (rect_hits[i] && 0 < plane_depths[i] && plane_depths[i] <= min_depth) {
          min_depth = plane_depths[i];
          hovered_ = true;
          if (!active_) {
            axis_ = i;
            plane_mode_ = true;
          }
        }
        if (arrow_hits[i] && 0 < axis_depths[i] && axis_depths[i] <= min_depth) {
          min_depth = axis_depths[i];
          hovered_ = true;
          if (!active_) {
            axis_ = i;
            plane_mode_ = false;
          }
        }
      }
    }

    // "immidiate update" during active
    if (active_) {
      using glm::dot;
      auto [_, __, xform_t_init] = decomposeTransform_v2(xform_initial_);
      int i = axis_;
      int j = (i + 1) % 3;
      int k = (i + 2) % 3;

      if (plane_mode_) {
        // 2D constrained translation
        fvec3 v = plane_hits_[i] - plane_hits_initial_[i];
        fvec2 diff = {dot(v, xform_r[j]), dot(v, xform_r[k])};

        // apply "fixed step" mode
        diff[0] = diff[0] - std::fmod(diff[0], step);
        diff[1] = diff[1] - std::fmod(diff[1], step);

        fvec3 new_xform_t = xform_t_init + diff[0] * xform_r[j] +  diff[1] * xform_r[k];
        *xform_ = composeTransform_v2(xform_s, xform_r, new_xform_t);

      } else {
        // 1D constrained translation
        fvec3 v = axis_hits_[i] - axis_hits_initial_[i];
        float diff = dot(v, xform_r[i]);

        // apply "fixed step" mode
        diff = diff - std::fmod(diff, step);

        fvec3 new_xform_t = xform_t_init + diff * xform_r[i];
        *xform_ = composeTransform_v2(xform_s, xform_r, new_xform_t);
      }
    }

    // deactivate and reset to initial when escape is pressed
    if (active_ && ImGui::IsKeyPressedMap(ImGuiKey_Escape)) {
      *xform_ = xform_initial_;
      active_ = false;
    }

    // activate on click
    if (ImGui::GetIO().MouseClicked[0]) {
      if (!active_ && hovered_) {
        active_ = true;
        xform_initial_ = *xform_;
        axis_hits_initial_ = axis_hits_;
        plane_hits_initial_ = plane_hits_;
      }
    }

    // deactivate on mouse up
    if (active_ && !ImGui::GetIO().MouseDown[0]) {
      active_ = false;
    }
  }

  void draw() {
    auto [_, xform_r, xform_t] = decomposeTransform_v2(*xform_);

    // small dots on center
    imgui3d->draw_list->AddCircleFilled(imgui3d->sceneCo_to_imguiCo(xform_t), 3, ImColor{fvec4{1, 1, 0, 1}});

    for (auto i : Range{3}) {
      int j = (i + 1) % 3;
      int k = (i + 2) % 3;
      fvec3 color = {0, 0, 0}; color[i] = 1;
      fvec3& o = xform_t;
      fvec3& u1 = xform_r[i];
      fvec3& u2 = xform_r[j];
      fvec3& u3 = xform_r[k];

      // arrow
      {
        fvec3 c = color;
        float alpha = 0.5;
        float circle_size = 4;
        if (axis_ == i && !plane_mode_) {
          if (active_) {
            c = {1, 1, 0};
          }
          if (active_ || hovered_) {
            alpha = 0.8;
            circle_size = 5;
          }
        }
        imgui3d->addLine({o, o + u1 * len1}, fvec4{c, alpha}, 5);
        imgui3d->draw_list->AddCircleFilled(imgui3d->sceneCo_to_imguiCo(o + u1 * len1), circle_size, ImColor{fvec4{c, 1}});
      }

      // rectangle
      {
        fvec3 c = color;
        float alpha = 0.3;
        if (axis_ == i && plane_mode_) {
          if (active_) {
            c = {1, 1, 0};
          }
          if (active_ || hovered_) {
            alpha = 0.5;
          }
        }
        glm::fmat2x3 F = {u2, u3};
        fvec2 vs[4] = {{len2, len2}, {len3, len2}, {len3, len3}, {len2, len3}};
        imgui3d->addConvexFill({o + F * vs[0], o + F * vs[1], o + F * vs[2], o + F * vs[3]}, fvec4{c, alpha});
      }
    }
  }

  void use() {
    handleEvent();
    draw();
  }
};

struct GizmoScale {
  void handleEvent() {
  }
  void draw() {
  }
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
// - [x] camera pivot based interaction implementation

fmat4 _translation_xform(fvec3 v) {
  return fmat4{{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {v, 1}};
}

fmat4 _lookat(fvec3 src, fvec3 dest, fvec3 up) {
  // look at "dest" from "src" with new frame x' y' z' such that dot(up, y') > 0 and dot(up, x') = 0:
  // z' = normalize(src - dest)
  // x' = normalize(y [cross] z')    (TODO: take care z' ~ y)
  // y' = z' [cross] x'
  auto z = glm::normalize(src - dest);
  auto x = glm::normalize(glm::cross(up, z));
  auto y = glm::cross(z, x);
  return {
    fvec4{x, 0},
    fvec4{y, 0},
    fvec4{z, 0},
    fvec4{src, 1},
  };
}

struct CameraViewExperimentContext {
  fvec2 viewport = {400, 400};
  fvec3 pivot = {0, 0, 0};
  fmat4 view_xform = _lookat({2.0, 2.0, 2.5}, pivot, {0, 1, 0});
  float yfov = 3.14 * 2 / 3;
  float znear = 0.01;
  float zfar = 100;
  int axis_bound = 5;
  int grid_division = 3;
  int show_axis[3] = {1, 1, 1};
  int show_grid[3] = {0, 1, 0};
  bool clip_line = true;
  bool show_pivot = true;
  bool show_test_plane = true;
  bool show_test_sphere = true;
  bool show_clip_poly = true;
};

inline static CameraViewExperimentContext global_camera_view_experiment_context_;

inline void CameraViewExperiment(CameraViewExperimentContext& ctx = global_camera_view_experiment_context_) {
  namespace ig = ImGui;

  //
  // Property input
  //
  ig::Columns(2, __FILE__); {
    {
      ig::DragFloat("yfov", &ctx.yfov, 0.01f);
      ig::InputInt("axis_bound", &ctx.axis_bound);
      ig::InputInt("grid_division", &ctx.grid_division);
      {
        auto _1 = ImScoped::StyleVar(ImGuiStyleVar_FrameRounding, 8);
        auto _2 = ImScoped::StyleVar(ImGuiStyleVar_GrabRounding,  8);
        ig::SliderInt3("show_axis (x, y, z)", ctx.show_axis, 0, 1, "");
        ig::SliderInt3("show_grid (yz, zx, xy)", ctx.show_grid, 0, 1, "");
      }
      ig::NextColumn();
    }
    {
      ig::Checkbox("clip_line", &ctx.clip_line); ig::SameLine();
      ig::Checkbox("show_pivot", &ctx.show_pivot);
      ig::Checkbox("show_test_plane", &ctx.show_test_plane); ig::SameLine();
      ig::Checkbox("show_test_sphere", &ctx.show_test_sphere);
      ig::Checkbox("show_clip_poly", &ctx.show_clip_poly);
      if (ig::Button("Reset")) { ctx = { .viewport = ctx.viewport }; };
      ig::Text("viewport size = { %d, %d }", (int)ctx.viewport.x, (int)ctx.viewport.y);
      ImGui::NextColumn();
    }
    ImGui::Columns(1);
  }


  //
  // Transformation setup
  //
  fmat4 projection = glm::perspectiveRH_NO(ctx.yfov, ctx.viewport[0] / ctx.viewport[1], ctx.znear, ctx.zfar);
  fvec3 camera_position = fvec3{ctx.view_xform[3]};
  fmat4 inv_view_xform = inverseTR(ctx.view_xform);

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
  if (active && ig::IsMouseDragging(0) && (io.MouseDelta.x != 0 || io.MouseDelta.y != 0) ) {
    ImVec2 v = io.MouseDelta / ctx.viewport;

    // "transform" camera wrt instantaneous pivot frame which we define to as below:
    float L = glm::length(camera_position - ctx.pivot);
    fmat4 pivot_xform = ctx.view_xform * _translation_xform({0, 0, -L});

    // zoom in/out to/from pivot
    if (ig::GetIO().KeyAlt) {
      constexpr float m = 1.5, M = 10;
      float dl = -v.x * (M - m);
      float new_L = glm::clamp(L + dl, m, M);
      ctx.view_xform = pivot_xform * _translation_xform({0, 0, new_L});
    }
    // rotation around pivot
    if (ig::GetIO().KeyCtrl) {
      ImVec2 u = v * ImVec2{2.f * 3.14f, 3.14f};

      // NOTE:
      // - "u.x" is for rotation of "usual" axial part of spherical coord.
      // - "u.y" cannot be handled this way since it's domain is "theta \in [0, phi]".
      //   but, by utilizing our `pivot_xform`, it can traverse great circle which passes north/south pole.
      // - in tern, "u.x" is not handled like "u.y" since that would make camera pass through equator.
      // - another complication of "u.x" is when camera's up vector can be flipped.

      // up/down (u.y)
      fmat3 A = ExtrinsicEulerXYZ_to_SO3({-u.y, 0, 0});
      ctx.view_xform = pivot_xform * fmat4{A} * _translation_xform({0, 0, L});

      // left/right (u.x)
      fmat4 pivot_v3_xform = _lookat(ctx.pivot, {camera_position.x, ctx.pivot.y, camera_position.z}, {0, 1, 0});
      fmat4 view_xform_wrt_pivot_v3 = inverseTR(pivot_v3_xform) * ctx.view_xform;
      bool camera_flipped = view_xform_wrt_pivot_v3[1].y < 0;
      fmat3 B = ExtrinsicEulerXYZ_to_SO3({0, camera_flipped ? u.x : -u.x, 0});
      ctx.view_xform = pivot_v3_xform * fmat4{B} * view_xform_wrt_pivot_v3;
    }
    // move with pivot
    if (ig::GetIO().KeyShift) {
      ImVec2 u = v * 3.f; // quite arbitrary scaling
      ctx.view_xform = pivot_xform * _translation_xform({-u.x, u.y, L});
      ctx.pivot      = fvec3{pivot_xform * fvec4{-u.x, u.y, 0, 1}};
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
    auto p = fvec3{ctx.view_xform * fvec4{u / s, -1, 1}};
    return p;
  };

  auto project_clip_line = [&](const fvec3& p, const fvec3& q) -> std::optional<std::pair<ImVec2, ImVec2>> {
    auto clip_pq = hit::clip4D_Line_ClipVolume({
        projection * inv_view_xform * fvec4{p, 1},
        projection * inv_view_xform * fvec4{q, 1}});
    if (!clip_pq) { return {}; }
    return std::make_pair(
          clip_coord_to_window_coord((*clip_pq)[0]),
          clip_coord_to_window_coord((*clip_pq)[1]));
  };

  auto project_clip_polygon = [&](const vector<fvec3>& ps) -> vector<ImVec2> {
    vector<fvec4> qs; qs.resize(ps.size());
    for (auto i : Range{ps.size()}) {
      qs[i] = projection * inv_view_xform * fvec4{ps[i], 1};
    }
    vector<fvec4> rs = hit::clip4D_ConvexPoly_ClipVolume(qs);
    vector<ImVec2> result; result.resize(rs.size());
    for (auto i : Range{rs.size()}) {
      result[i] = clip_coord_to_window_coord(rs[i]);
    }
    return result;
  };

  if (ctx.show_pivot) {
    draw->AddCircleFilled(project_3d(ctx.pivot), 3.f, ig::GetColorU32({1, 1, 1, 1}));
  }

  // Axis
  for (auto i : Range{3}) {
    if (!ctx.show_axis[i]) { continue; }

    auto p1 = glm::fmat3{1}[i] * (float)ctx.axis_bound;
    auto p2 = - p1;

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
      auto w1 = project_3d(p1);
      auto w2 = project_3d(p2);
      draw->AddLine(w1, w2, color, 1.f);
      draw->AddCircle(w2, 4.f, color);       // negative end
      draw->AddCircleFilled(w1, 4.f, color); // positive end
    }
  }

  // Grid plane
  for (auto i : Range{3}) {
    if (!ctx.show_grid[i]) { continue; }
    auto j = (i + 1) % 3;                        // e.g. i = 0, j = 1, k = 2
    auto k = (i + 2) % 3;
    auto B = ctx.axis_bound;
    fvec3 v{0}; v[k] = B;                        // e.g. {0, 0, B}
    for (auto s : Range{-B, B + 1}) {
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
      for (auto l : Range{1, D}) {
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
  if (ctx.show_test_plane) {
    fvec3 p[4] = { {1, 1, 1}, {-1, 1, 1}, {-1, -1, 1}, {1, -1, 1} };
    // ImGui applies anti-aliasing for CW face so we order points based on
    // whether plane faces to camera (i.e. position wrt plane's frame).
    fmat4 model_xform = { {1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 1, 1} };
    bool ccw = (inverseTR(model_xform) * fvec4(camera_position, 1)).z > 0;
    draw->PathClear();
    if (ccw) {
      for (auto i : Reverse{Range{4}})
        draw->PathLineTo(project_3d(p[i]));
    } else {
      for (auto i : Range{4})
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

  // Plane at z = 1
  if (ctx.show_clip_poly) {
    vector<fvec3> ps = { {2, 0, 2}, {2, 0, -2}, {-2, 0, -2}, {-2, 0, 2} };
    for (auto& p : ps) { p += fvec3{0, 1, 0}; }

    fmat4 model_xform = { {1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 1, 0, 1} };
    fvec3 model_normal = {0, 1, 0};
    bool ccw = glm::dot(model_normal, fvec3{inverseTR(model_xform) * fvec4(camera_position, 1)}) > 0;

    vector<ImVec2> qs = project_clip_polygon(ps);
    draw->PathClear();
    if (ccw) {
      for (auto& q : Reverse{qs}) draw->PathLineTo(q);
    } else {
      for (auto& q : qs) draw->PathLineTo(q);
    }
    draw->PathFillConvex(ig::GetColorU32({1, 1, 0, .8}));

    draw->AddCircleFilled(
        project_3d({0, 1, 0}), 5.f,
        ig::GetColorU32(ccw ? ImVec4{1, 1, 0, 1} : ImVec4{1, 0, 1, 1}));
  }

  // Mouse position in world frame
  bool _debug_rev_project = true;
  if (active && _debug_rev_project) {
    auto p = rev_project_3d(ig::GetMousePos());
    draw->AddCircleFilled(project_3d(p), 3.f, ig::GetColorU32({0, 1, 1, 1}));
  }

  // Hit testing plane
  if (active && ctx.show_test_plane) {
    auto mouse_ray = rev_project_3d(ig::GetMousePos()) - camera_position;
    auto t = hit::Line_Plane(camera_position, mouse_ray, {0, 0, 1}, {0, 0, 1});
    if (t && *t > 0) {
      auto intersection =  camera_position + (*t) * mouse_ray;
      bool rect_hit; {
        auto v = intersection - fvec3{0, 0, 1};
        rect_hit = glm::max(glm::abs(v.x), glm::abs(v.y)) < 1;
      }
      draw->AddLine(
          project_3d(ctx.pivot), project_3d(intersection),
          rect_hit ? ig::GetColorU32({0, 1, 1, .8}) : ig::GetColorU32({0, 1, 1, .3}),
          rect_hit ? 3.f : 1.f);
    }
  }

  // Draw sphere (outline)
  if (ctx.show_test_sphere) {
    fvec3 c_model = {1, 0, 0}; // center
    float r = 1;             // radius
    fvec3 c = c_model - camera_position; // location w.r.t camera point

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
      for (auto i : Range{ps_size}) {
        float phi = 2.f * pi * i / ps_size;
        fvec3 v = { sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta) };
        ps[i] = c_model + frame * r * v;
      }

      // project and draw outline (by construction, it's already CW)
      draw->PathClear();
      for (auto i : Range{ps_size}) { draw->PathLineTo(project_3d(ps[i])); }
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

    for (auto i : Range{3}) {
      auto j = (i + 1) % 3;
      auto k = (i + 2) % 3;
      fvec3 u{0}; u[j] = 1;
      fvec3 v{0}; v[k] = 1;

      draw->PathClear();
      for (auto i : Range{num_segments}) {
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
    fvec3 mouse_ray = rev_project_3d(ig::GetMousePos()) - camera_position;
    auto t1_t2 = hit::Line_Sphere(camera_position, mouse_ray, center, radius);
    if (t1_t2) {
      auto [t, _] = *t1_t2;
      fvec3 intersection = camera_position + t * mouse_ray;
      fvec3 normal = glm::normalize(intersection - center);

      // tangent space basis
      fvec3 u = glm::normalize(glm::cross(normal, {0, -1, 0}));
      fvec3 v = glm::normalize(glm::cross(normal, u));
      glm::fmat2x3 F = {u, v};

      float scale = 0.5;
      fvec2 plane[4] = { {1, 1}, {1, -1}, {-1, -1}, {-1, 1} };

      draw->PathClear();
      for (auto i : Range{4}) {
        fvec3 p = intersection + F * scale * plane[i];
        draw->PathLineTo(project_3d(p));
      }
      draw->PathFillConvex(ig::GetColorU32({0, 1, 1, .8}));
    }
  }
}

} // imgui
} } // toy::utils
