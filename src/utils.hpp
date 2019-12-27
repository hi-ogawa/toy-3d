#pragma once

#include <stdexcept>
#include <string>
#include <vector>
#include <optional>
#include <map>
#include <sstream>
#include <numeric> // iota

#include <fmt/format.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_scoped.h>
#include <glm/glm.hpp>
#include <glm/ext/scalar_constants.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <GL/gl3w.h>
#include <cgltf.h>


//
// Delete move/copy constructure/assignment
//
#define TOY_CLASS_DELETE_MOVE(CLASS)  \
  CLASS(CLASS&&) = delete;            \
  CLASS &operator=(CLASS&&) = delete;

#define TOY_CLASS_DELETE_COPY(CLASS)      \
  CLASS(const CLASS&) = delete;           \
  CLASS& operator=(const CLASS&) = delete;

#define TOY_CLASS_DELETE_MOVE_COPY(CLASS) \
  TOY_CLASS_DELETE_MOVE(CLASS)            \
  TOY_CLASS_DELETE_COPY(CLASS)

// Cf. https://code.woboq.org/userspace/glibc/assert/assert.h.html#88
#define TOY_ASSERT(EXPR) \
  if(!static_cast<bool>(EXPR)) {   \
    throw std::runtime_error{fmt::format("[{}:{}] {}", __FILE__, __LINE__, #EXPR)}; \
  }

#define TOY_ASSERT_CUSTOM(EXPR, MESSAGE) \
  if(!static_cast<bool>(EXPR)) {   \
    throw std::runtime_error{fmt::format("[{}:{}] {}", __FILE__, __LINE__, MESSAGE)}; \
  }

#define TOY_PATH(PATH) TOY_DIR "/" PATH

#define GLTF_MODEL_PATH(NAME) GLTF_MODEL_DIR "/2.0/" NAME "/glTF/" NAME ".gltf"

inline std::string getGltfModelPath(const char* name) {
  return std::string{GLTF_MODEL_DIR} + "/2.0/" + name + "/glTF/" + name + ".gltf";
}

//
// fmt::format glm vector/matrix
//

namespace fmt {

template<glm::length_t L, typename T>
struct formatter<glm::vec<L, T>> {
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  template <typename FormatContext>
  auto format(const glm::vec<L, T>& v, FormatContext& ctx) {
    std::string s;
    for (auto i = 0; i < L; i++) {
      if (i > 0) { s += ", "; }
      s += fmt::to_string(v[i]);
    }
    return format_to(ctx.out(), "{}", s);
  }
};

template<glm::length_t C, glm::length_t R, typename T>
struct formatter<glm::mat<C, R, T>> {
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

  template <typename FormatContext>
  auto format(const glm::mat<C, R, T>& A, FormatContext& ctx) {
    std::string s;
    for (auto i = 0; i < C; i++) {
      s += fmt::to_string(A[i]);
      s += "\n";
    }
    return format_to(ctx.out(), "{}", s);
  }
};

} // fmt


namespace toy { namespace utils {

namespace {
using glm::ivec2, glm::fvec2, glm::fvec3, glm::fvec4, glm::fmat3, glm::fmat4;
using std::vector, std::string, std::array;
}

template <typename T>
struct Reverse {
  T& iterable_;
  Reverse(T&  iterable) : iterable_{iterable} {}
  Reverse(T&& iterable) : iterable_{iterable} {}
  auto begin() { return std::rbegin(iterable_); };
  auto end()   { return std::rend(iterable_);   };
};

struct Range {
  int start_, end_;
  Range(int start, int end) : start_{start}, end_{std::max(start, end)} {}
  Range(int end) : Range(0, end) {}
  Range(size_t start, size_t end) : start_{static_cast<int>(start)}, end_{static_cast<int>(std::max(start, end))} {}
  Range(size_t end) : start_{0}, end_{static_cast<int>(end)} {}

  struct Iterator {
    int i_;

    int operator*() { return i_; }
    Iterator& operator++() {
      i_++;
      return *this;
    }
    bool operator!=(const Iterator& other) {
      return i_ != other.i_;
    }
  };
  struct ReverseIterator {
    int i_;

    int operator*() { return i_; }
    ReverseIterator& operator++() {
      i_--;
      return *this;
    }
    bool operator!=(const ReverseIterator& other) {
      return i_ != other.i_;
    }
  };

  Iterator begin() { return Iterator{start_}; };
  Iterator end() { return Iterator{end_}; };
  ReverseIterator rbegin() { return ReverseIterator{end_ - 1}; };
  ReverseIterator rend() { return ReverseIterator{start_ - 1}; };
};

template<typename T>
struct Enumerate {
  size_t start_, end_;
  T* data;

  Enumerate(vector<T>& container)
    : start_{0}, end_{container.size()}, data{container.data()} {}
  Enumerate(T* data, size_t size)
    : start_{0}, end_{size}, data{data} {}

  struct Iterator {
    size_t i_;
    T* data;

    std::pair<size_t, T*> operator*() {
      return std::make_pair(i_, &data[i_]);
    }
    Iterator& operator++() {
      i_++;
      return *this;
    }
    bool operator!=(const Iterator& other) {
      return i_ != other.i_;
    }
  };

  Iterator begin() { return Iterator{start_, data}; };
  Iterator end() { return Iterator{end_, data}; };
};

template<glm::length_t N>
inline bool isSmall(glm::vec<N, float> v) {
  return glm::length(v) < glm::epsilon<float>();
}

inline bool isSmall(float a) {
  return std::abs(a) < glm::epsilon<float>();
}

// Primitive closest point (cf. closed convex projection theorem)

namespace hit {

inline auto Line_Plane(
    const fvec3& p, // line point
    const fvec3& v, // ray vector
    const fvec3& q, // plane point
    const fvec3& n  // plane normal
)-> std::optional<float> // t where p + t v is intersection unless <v, n> = 0
{
  // <(p + t v) - q, n> = 0  <=>  t <v, n> = <q - p, n>
  auto a = glm::dot(v, n);
  auto b = glm::dot(q - p, n);
  if (isSmall(a)) { return {}; }
  return b / a;
}

inline auto Line_Point(
    const fvec3& p, // line point
    const fvec3& v, // ray vector
    const fvec3& q  // point
)-> float // t where p + t v is closest point
{
  // <(p + t v) - q, v> = 0  <=>  t <v, v> = <q - p, v>
  auto a = glm::dot(v, v);
  TOY_ASSERT(!isSmall(a));
  auto b = glm::dot(q - p, v);
  return b / a;
}

inline auto Line_Sphere(
    const fvec3& p, // line point
    const fvec3& v, // ray vector
    const fvec3& c, // center
    const float  r  // radius
)-> std::optional<std::pair<float, float>> // t1 <= t2 where p + t v is on sphere (ball's surface)
{
  float t = hit::Line_Point(p, v, c);
  fvec3 closest_point = p + t * v;
  fvec3 off_center = closest_point - c;
  float l = glm::length(off_center);
  if (l >= r) { return {}; }

  using std::asin, std::cos;
  float in_angle = asin(l / r);
  float dt = r * cos(in_angle) / glm::length(v);
  return std::make_pair(t - dt, t + dt);
}

inline float Line_Line(           // @return t s.t. p + t u is 1st line's closest point to 2nd line
  const fvec3& p, const fvec3& u, // 1st line
  const fvec3& q, const fvec3& v  // 2nd line
) {
  using glm::cross;
  fvec3 s = cross(u, v);
  if (isSmall(s)) {
    return Line_Point(p, u, q);
  }
  std::optional<float> t = *Line_Plane(p, u, q, cross(s, v));
  TOY_ASSERT(t);
  return *t;
}

struct RayTriangleResult {
  bool valid = false; // "hit" is determined by uv \in 2-standard-simplex (i.e. u + v <= 1 and u, v >= 0)
  float t;            // always t >= 0
  fvec2 uv;
  fvec3 p;            // return this since it's used during the test anyway.
};

inline RayTriangleResult Ray_Triangle(
    const fvec3& src, const fvec3& dir,
    const fvec3& p0, const fvec3& p1, const fvec3& p2) {
  using glm::cross, glm::dot, glm::fmat2;
  RayTriangleResult result;

  fvec3 v1 = p1 - p0;
  fvec3 v2 = p2 - p0;
  fvec3 n = cross(v1, v2);

  // degenerate triangle
  if (isSmall(glm::length(n))) { return result; }

  std::optional<float> t = Line_Plane(src, dir, p0, n);

  // parallel or opposite direction to the plane spanned by v1, v2
  if (!t || (t && *t < 0)) { return result; }

  result.valid = true;
  result.t = t.value();
  result.p = src + t.value() * dir;
  fvec3 q = result.p - p0;
  //
  // Here q \in span{v1, v2}, i.e.
  //
  // q = / v1, v2 \  / s \  for some s, t.
  //     \        /  \ t /
  // thus,
  // / <v1, q> \  =  / <v1, v1>, <v1, v2> \  / s \
  // \ <v2, q> /     \ <v1, v2>, <v2, v2> /  \ t /
  //
  float x = dot(v1,  q), y = dot(v2, q);
  float a = dot(v1, v1), b = dot(v1, v2), d = dot(v2, v2);
  float det = a * d - b * b;
  result.uv = {(d * x - b * y) / det, (-b * x + a * y) / det};
  return result;
}

inline vector<fvec4> clip4D_ConvexPoly_HalfSpace(
    const vector<fvec4>& vs, // dim(span{vi - v0 | i}) = 2 (thus vs.size() >= 3)
    const fvec4& q,          // half space as { u | dot(u - q, v) >= 0 }
    const fvec4& n
) {
  using glm::dot;
  auto mod = [](int i, int m) { return (i + m) % m; };

  int N = vs.size();
  float dots[vs.size()]; // dots[i] > 0  <=>  vs[i] \in (interior of) HalfSpace
  int start = -1, end = -1;
  //
  // Compute start >= 0, end >=0 s.t.
  //
  //       outside <= | => inside
  //                  |
  //  ...>---v(i)-----|---v(i+1)-->...
  //                  |
  //  ...<---v(j+1)---|---v(j)----<...
  //                  |
  //   `start` = i+1
  //   `end`   = j+1  (as modular arith. of `N`)
  //
  // (Then, later, find intersection u1, u2 and returns new polygon vs[i+1], ... vs[j], u1, u2)
  //
  {
    dots[0] = dot(vs[0] - q, n);
    bool in_first = dots[0] > 0;
    bool in_tmp   = in_first;
    int in_out_idx = -1; // by convexty, e.g. "in -> out -> in -> out" is impossible
    int out_in_idx = -1;
    for (auto i : Range{1, N}) {
      dots[i] = dot(vs[i] - q, n);
      if ( in_tmp && !(dots[i] > 0)) { in_out_idx = i; in_tmp = false; }
      if (!in_tmp &&   dots[i] > 0 ) { out_in_idx = i; in_tmp = true;  }
    }
    if (in_first) {
      if (in_out_idx == -1) { return vs; } // all inside
      start = out_in_idx == -1 ? 0 : out_in_idx;
      end = in_out_idx;
    } else {
      if (out_in_idx == -1) { return {}; } // all outside
      start = out_in_idx;
      end = in_out_idx == -1 ? 0 : in_out_idx;
    }
  }
  //
  // Find intersection u1, u2 as in
  //   outside <= | => inside
  //       p1-----u1--->(w1)
  //              |
  //      (w2)<---u2-----p2
  //
  fvec4 p1 = vs[mod(start - 1, N)];   // outside
  float d1 = dots[mod(start - 1, N)]; // <p1 - q, n> <= 0
  fvec4 w1 = vs[start] - p1;

  fvec4 p2 = vs[mod(end - 1, N)];     // inside
  float d2 = dots[mod(end - 1, N)];   // <p2 - q, n> >  0
  fvec4 w2 = vs[end] - p2;

  // <(p + t w) - q, n> = 0  <=> t <w, n> = - <p - q, n>
  float a1 = glm::dot(w1, n); // by construction, a1 > 0
  float a2 = glm::dot(w2, n); //                  a2 < 0
  float t1 = - d1 / a1;
  float t2 = - d2 / a2;
  fvec4 u1 = p1 + t1 * w1; // NOTE: at the same time, you could interpolate vertex attribute if any.
  fvec4 u2 = p2 + t2 * w2; //

  vector<fvec4> result = { u2, u1 };
  result.reserve(mod(end - start, N) + 2);

  if (start < end) {
    result.insert(result.end(), vs.begin() + start, vs.begin() + end);
  } else {
    result.insert(result.end(), vs.begin() + start, vs.end());
    result.insert(result.end(), vs.begin(), vs.begin() + end);
  }
  return result;
};

inline vector<fvec4> clip4D_ConvexPoly_ClipVolume(const vector<fvec4>& poly) {
  vector<fvec4> result = poly;
  TOY_ASSERT(result.size() >= 3); // and also assert dim(poly) = 2

  // "ClipVolume" as intersection of 7 half spaces
  fvec4 q  = {0, 0, 0, 0};
  result = clip4D_ConvexPoly_HalfSpace(result, q, { 0, 0, 0, 1}); if (result.size() < 3) return {};
  result = clip4D_ConvexPoly_HalfSpace(result, q, { 1, 0, 0, 1}); if (result.size() < 3) return {};
  result = clip4D_ConvexPoly_HalfSpace(result, q, {-1, 0, 0, 1}); if (result.size() < 3) return {};
  result = clip4D_ConvexPoly_HalfSpace(result, q, { 0, 1, 0, 1}); if (result.size() < 3) return {};
  result = clip4D_ConvexPoly_HalfSpace(result, q, { 0,-1, 0, 1}); if (result.size() < 3) return {};
  result = clip4D_ConvexPoly_HalfSpace(result, q, { 0, 0, 1, 1}); if (result.size() < 3) return {};
  result = clip4D_ConvexPoly_HalfSpace(result, q, { 0, 0,-1, 1});
  return result;
};

inline std::optional<std::array<fvec4, 2>> clip4D_Line_HalfSpace(
    const std::array<fvec4, 2>& ps, // line segment
    const fvec4& q,                 // half space as { u | dot(u - q, v) >= 0 }
    const fvec4& n
) {
  // <(p + t v) - q, n> = 0  <=> t <v, n> = - <p - q, n>
  fvec4 v = ps[1] - ps[0];
  float a = glm::dot(v, n);
  float b = glm::dot(ps[0] - q, n);
  bool p0_in = b > 0;
  float t = - b / a;
  if (p0_in) {
    // [case 1] p0_in && dot(v, n) >= 0
    if (a >= 0) { return ps; }
    // [case 2] p0_in && dot(v, n) < 0  (thus t > 0)
    fvec4 new_p1 = ps[0] + std::min(t, 1.f) * v;
    return std::array<fvec4, 2>{ ps[0], new_p1 };
  }
  // [case 3] !p0_in && dot(v, n) > 0 && t < 1 (thus t > 0)
  if (a > 0 && t < 1) {
    fvec4 new_p0 = ps[0] + t * v;
    return std::array<fvec4, 2>{ new_p0, ps[1] };
  }
  return {};
};

inline std::optional<std::array<fvec4, 2>> clip4D_Line_ClipVolume(const std::array<fvec4, 2>& ps) {
  std::optional<std::array<fvec4, 2>> result = ps;

  // "ClipVolume" as intersection of 7 half spaces
  fvec4 q  = {0, 0, 0, 0};
  result = clip4D_Line_HalfSpace(*result, q, { 0, 0, 0, 1}); if (!result) return {};
  result = clip4D_Line_HalfSpace(*result, q, { 1, 0, 0, 1}); if (!result) return {};
  result = clip4D_Line_HalfSpace(*result, q, {-1, 0, 0, 1}); if (!result) return {};
  result = clip4D_Line_HalfSpace(*result, q, { 0, 1, 0, 1}); if (!result) return {};
  result = clip4D_Line_HalfSpace(*result, q, { 0,-1, 0, 1}); if (!result) return {};
  result = clip4D_Line_HalfSpace(*result, q, { 0, 0, 1, 1}); if (!result) return {};
  result = clip4D_Line_HalfSpace(*result, q, { 0, 0,-1, 1});
  return result;
};

} // hit


//
// inverse in euclidian group SO(3) x R^3 (aka translation and rotation)
//

inline fmat4 inverseTR(const fmat4& F) {
  fmat3 A{F};
  fvec3 b{F[3]};
  fmat3 AT = transpose(A); // i.e. inverse
  fvec3 c = - AT * b;
  fmat4 G{AT};
  G[3] = fvec4{c, 1};
  return G;
}

inline fmat3 ExtrinsicEulerXYZ_to_SO3(fvec3 radians) {
  auto x = radians.x, cx = std::cos(x), sx = std::sin(x);
  auto y = radians.y, cy = std::cos(y), sy = std::sin(y);
  auto z = radians.z, cz = std::cos(z), sz = std::sin(z);
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

// Range
// - x \in [  -pi,   pi]
// - y \in [-pi/2, pi/2]
// - z \in [  -pi,   pi]
inline fvec3 SO3_to_ExtrinsicXYZ(fmat3 A) {
  constexpr auto pi = glm::pi<float>();
  auto clamp = [](float f) { return glm::clamp(f, -1.f, 1.f); }; // assure z \in [-1, 1] for acos(z)
  fvec3 r;

  //
  // Extrinsic rotation applied to e1 and e3 (x and z)
  //    rx      ry       rz
  // x  ==  x' ---> x'' ---> x''' (= v)
  // z ---> z' ---> z'' ---> z'''
  //       (= u)
  //

  // 1. Derive angles ry and rz from spherical coordinate of v
  fvec3 v = A[0];
  r.y = glm::acos(clamp(v.z)) - pi / 2.f;
  r.z = atan2(v.y, v.x);

  // 2. Invert A by ry and rz, then find rx from sperical coordinate of u
  auto getR = ExtrinsicEulerXYZ_to_SO3;
  fvec3 u = getR({0, -r.y, 0}) * getR({0, 0, -r.z}) * A[2];
  u = glm::normalize(u); // assure u.z \in [-1, 1]
  r.x = atan2(-u.y, u.z); // \in [-pi, pi]

  return r;
}

inline fmat3 UnitQuaternion_to_SO3(fvec4 q) {
  TOY_ASSERT_CUSTOM(false, "Not implemented");
  // TODO
  // 1. apply q to e1, e2, e3
  return {};
}
inline fvec4 SO3_to_UnitQuaternion(fmat3 so3) {
  TOY_ASSERT_CUSTOM(false, "Not implemented");
  // TODO
  // 1. compute eigenvector u (up to scalar unless so3 = I)
  // 2. compute rotation in orthogonal space of span{u}
  return {};
}

inline std::tuple<fvec3, fvec3, fvec3> decomposeTransform(const fmat4& xform) {
  fmat3 A = xform;
  fmat3 R;
  fvec3 s;
  float h;
  //
  // Decomposition of rotation and scale.
  // - Find A = R * s * h where
  //   - R: SO(3)
  //   - s: diagonal (scale without sign)
  //   - h: sign (handedness)
  //
  // - 1. h = sign(det(A))
  // - 2. s_i = |A e_i|
  // - 3. R = A * inv(s * h)
  //
  h = glm::sign(glm::determinant(A));
  if (h == 0) { h = 1; }

  s = { (glm::dot(A[0], A[0]) > 0) ? glm::length(A[0]) : glm::epsilon<float>(),
        (glm::dot(A[1], A[1]) > 0) ? glm::length(A[1]) : glm::epsilon<float>(),
        (glm::dot(A[2], A[2]) > 0) ? glm::length(A[2]) : glm::epsilon<float>(), };

  R = { A[0] * h / s.x,   A[1] * h / s.y,   A[2] * h / s.z };

  return {
      h * s,                  // (signed) scale
      SO3_to_ExtrinsicXYZ(R), // rotation
      xform[3],               // translation
  };
}

inline fmat4 composeTransform(const fvec3& s, const fvec3& r, const fvec3& t) {
  fmat3 R = ExtrinsicEulerXYZ_to_SO3(r);
  return { {R[0] * s.x, 0},
           {R[1] * s.y, 0},
           {R[2] * s.z, 0},
           {         t, 1}, };
}

inline fmat4 translateTransform(const fvec3& t) {
  return fmat4{{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {t, 1}};
}

inline fmat4 lookatTransform(fvec3 src, fvec3 dest, fvec3 up) {
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

inline fvec3 getNonParallel(fvec3 v1) {
  auto is_small = [](const fvec3& u) {
    return glm::abs(glm::length(u)) < glm::epsilon<float>(); };
  fvec3 v2 = {1, 0, 0};
  fvec3 v3 = {0, 1, 0};
  return is_small(glm::cross(v1, v2)) ? v3 : v2;
}

// Extracted from imgui::CameraViewExperiment
enum struct PivotControlType { ROTATION, ZOOM, MOVE };
inline void pivotControl(
    fmat4& xform,       // inout
    fvec3& pivot,       // in (out when MOVE)
    const fvec2& delta, // radians (ROTATION), length (ZOOM, MOVE)
    PivotControlType type
) {
  fvec3 position = fvec3{xform[3]};
  float L = glm::length(position - pivot);
  fmat4 pivot_xform = xform * translateTransform({0.f, 0.f, -L});

  switch (type) {
    case PivotControlType::ROTATION: {
      // NOTE:
      // - "delta.x" is for rotation of "usual" axial part of spherical coord.
      // - "delta.y" cannot be handled this way since it's domain is "theta \in [0, phi]".
      //   but, by utilizing our `pivot_xform`, it can traverse great circle which passes north/south pole.
      // - In tern, "delta.x" is not handled like "delta.y" since that would make camera pass through equator.
      // - Note that another complication of "u.x" where camera's up vector can be flipped.

      // up/down (u.y)
      fmat3 A = ExtrinsicEulerXYZ_to_SO3({-delta.y, 0, 0});
      xform = pivot_xform * fmat4{A} * translateTransform({0, 0, L});

      // left/right (u.x)
      fmat4 pivot_xform_v2 = lookatTransform(pivot, {position.x, pivot.y, position.z}, {0, 1, 0});
      fmat4 view_xform_wrt_pivot_v2 = inverseTR(pivot_xform_v2) * xform;
      bool camera_flipped = view_xform_wrt_pivot_v2[1].y < 0;
      fmat3 B = ExtrinsicEulerXYZ_to_SO3({0, camera_flipped ? delta.x : -delta.x, 0});
      xform = pivot_xform_v2 * fmat4{B} * view_xform_wrt_pivot_v2;
      break;
    }
    case PivotControlType::ZOOM: {
      xform = pivot_xform * translateTransform({0, 0, L - delta.x});
      break;
    }
    case PivotControlType::MOVE: {
      xform = pivot_xform * translateTransform({-delta.x, delta.y, L});
      pivot = fvec3{pivot_xform * fvec4{-delta.x, delta.y, 0, 1}};
      break;
    }
  }
}

inline float gizmoControl_Rotation(
    const fvec3& mouse, const fvec3& mouse_last, const fvec3& camera,
    const fvec3& origin, const fvec3 axis) {
  auto& p      = mouse;
  auto& p_last = mouse_last;
  auto& q      = camera;
  std::optional<float> t      = hit::Line_Plane(q, p      - q, origin, axis);
  std::optional<float> t_last = hit::Line_Plane(q, p_last - q, origin, axis);
  if (!(t && t_last)) { return 0; }

  fvec3 v      = q + (*t)      * (p      - q) - origin;
  fvec3 v_last = q + (*t_last) * (p_last - q) - origin;
  if (isSmall(v) || isSmall(v_last)) { return 0; }

  using glm::normalize, glm::cross, glm::dot;
  fvec3 x = normalize(v_last);
  fvec3 y = normalize(cross(axis, v_last));
  float angle_delta = atan2(dot(y, v), dot(x, v));
  return angle_delta;
}

inline float gizmoControl_Translation1D(
    const fvec3& mouse, const fvec3& mouse_last, const fvec3& camera,
    const fvec3& origin, const fvec3 axis) {
  auto& p      = mouse;
  auto& p_last = mouse_last;
  auto& q      = camera;

  // NOTE: this projection doesn't seems useful but this is only canonical choice...
  float t      = hit::Line_Line(q, p      - q, origin, axis);
  float t_last = hit::Line_Line(q, p_last - q, origin, axis);

  fvec3 v = t * (p - q) - t_last * (p_last - q);
  return glm::dot(v, axis);
}

// Probably, this is less convinient than below definition.
// (float, float) delta is necessary, for example, for "stepped" constraint.
inline fvec3 gizmoControl_Translation2D(
    const fvec3& mouse, const fvec3& mouse_last, const fvec3& camera,
    const fvec3& origin, const fvec3& normal) {
  auto& p      = mouse;
  auto& p_last = mouse_last;
  auto& q      = camera;
  std::optional<float> t      = hit::Line_Plane(q, p      - q, origin, normal);
  std::optional<float> t_last = hit::Line_Plane(q, p_last - q, origin, normal);
  if (!(t && t_last)) { return fvec3{0}; }

  fvec3 v = (*t) * (p - q) - (*t_last) * (p_last - q);
  return v - glm::dot(normal, v) * normal;
}

inline array<float, 2> gizmoControl_Translation2D(
    const fvec3& mouse, const fvec3& mouse_last, const fvec3& camera,
    const fvec3& origin, const fvec3& u1, const fvec3& u2) {
  auto& p      = mouse;
  auto& p_last = mouse_last;
  auto& q      = camera;
  fvec3 normal = glm::cross(u1, u2);
  TOY_ASSERT(!isSmall(normal));
  std::optional<float> t      = hit::Line_Plane(q, p      - q, origin, normal);
  std::optional<float> t_last = hit::Line_Plane(q, p_last - q, origin, normal);
  if (!(t && t_last)) { return {0, 0}; }

  using glm::fmat2x3, glm::inverse, glm::transpose;
  fmat2x3 F = {u1, u2};
  fvec3 v = (*t) * (p - q) - (*t_last) * (p_last - q);
  fvec2 st = inverse(transpose(F) * F) * transpose(F) * v;
  return {st[0], st[1]};
}

inline float gizmoControl_Scale3D(
    const fvec3& mouse, const fvec3& mouse_last, const fvec3& camera,
    const fvec3& origin) {
  auto& p      = mouse;
  auto& p_last = mouse_last;
  auto& q      = camera;
  fvec3 normal = origin - camera;
  std::optional<float> t      = hit::Line_Plane(q, p      - q, origin, normal);
  std::optional<float> t_last = hit::Line_Plane(q, p_last - q, origin, normal);
  if (!(t && t_last)) { return 0; }

  fvec3 v      = q + (*t)      * (p      - q) - origin;
  fvec3 v_last = q + (*t_last) * (p_last - q) - origin;
  return glm::length(v) / glm::length(v_last);
}

inline fmat3 get_ndCo_to_windowCo(const fvec2& offset, const fvec2& size) {
  // Derived via (T: translation, S: scale)
  // T(L, T) * S(W, H) * S(1/2, 1/2) * T(1/2, 1/2) * S(1, -1)
  float L = offset.x;
  float T = offset.y;
  float W = size.x;
  float H = size.y;
  return {
        W/2,          0,    0,
          0,       -H/2,    0,
    L + W/2,    T + H/2,    1,
  };
}

//
// Mesh examples
//

// NOTE: This can be also used to cast single component e.g. vector<uint8_t> => vector<uint16_t>
template<typename TOut, typename T, typename... Ts>
inline vector<TOut> interleave(const vector<T>& v, const vector<Ts>&... vs) {
  vector<TOut> result;
  result.resize(v.size());
  for (auto i = 0; i < v.size(); i++) {
    result[i] = {v[i], vs[i]...};
  }
  return result;
}

template<typename T>
inline vector<T> Quads_to_Triangles(const vector<T>& quad_indices) {
  auto n = quad_indices.size();
  if (n % 4 != 0) {
    throw std::runtime_error{"Invalid argument: quad_indices.size() % 4 != 0"};
  }
  vector<T> result;
  result.resize(n / 4 * 6);
  for (auto i = 0; i < n / 4; i++) {
    auto A = quad_indices[4 * i + 0];
    auto B = quad_indices[4 * i + 1];
    auto C = quad_indices[4 * i + 2];
    auto D = quad_indices[4 * i + 3];
    result[6 * i + 0] = A;
    result[6 * i + 1] = B;
    result[6 * i + 2] = D;
    result[6 * i + 3] = C;
    result[6 * i + 4] = D;
    result[6 * i + 5] = B;
  }
  return result;
};

inline auto createCube() {
  std::tuple<vector<fvec3>, vector<fvec4>, vector<uint8_t>> result;
  auto& [positions, colors, indices] = result;
  positions = {
    { 0, 0, 0 },
    { 1, 0, 0 },
    { 1, 1, 0 },
    { 0, 1, 0 },
    { 0, 0, 1 },
    { 1, 0, 1 },
    { 1, 1, 1 },
    { 0, 1, 1 },
  };
  colors = {
    { 0, 0, 0, 1 },
    { 1, 0, 0, 1 },
    { 1, 1, 0, 1 },
    { 0, 1, 0, 1 },
    { 0, 0, 1, 1 },
    { 1, 0, 1, 1 },
    { 1, 1, 1, 1 },
    { 0, 1, 1, 1 },
  };
  indices = Quads_to_Triangles(vector<uint8_t>{
    0, 3, 2, 1, // z = 0 plane
    4, 5, 6, 7, // z = 1
    0, 1, 5, 4, // y = 0
    1, 2, 6, 5, // x = 1
    2, 3, 7, 6, // y = 1
    3, 0, 4, 7, // x = 0
  });
  return result;
}

inline std::tuple<vector<fvec3>, vector<fvec4>, vector<fvec2>, vector<uint8_t>> createUVCube() {
  // Duplicate vertex by "x 3" so that
  // the single original vertex can have different uv coord for each surrounding face.
  // V: 8  --(x 3)--> V': 24
  std::tuple<vector<fvec3>, vector<fvec4>, vector<fvec2>, vector<uint8_t>> result;
  auto& [positions, colors, uvs, indices] = result;
  positions = {
    // z = 0
    { 0, 0, 0 },
    { 0, 1, 0 },
    { 1, 1, 0 },
    { 1, 0, 0 },
    // z = 1
    { 0, 0, 1 },
    { 1, 0, 1 },
    { 1, 1, 1 },
    { 0, 1, 1 },
    // x = 0
    { 0, 0, 0 },
    { 0, 0, 1 },
    { 0, 1, 1 },
    { 0, 1, 0 },
    // x = 1
    { 1, 0, 0 },
    { 1, 1, 0 },
    { 1, 1, 1 },
    { 1, 0, 1 },
    // y = 0
    { 0, 0, 0 },
    { 1, 0, 0 },
    { 1, 0, 1 },
    { 0, 0, 1 },
    // y = 1
    { 0, 1, 0 },
    { 0, 1, 1 },
    { 1, 1, 1 },
    { 1, 1, 0 },
  };
  uvs = {
    { 0, 1 }, { 1, 1 }, { 1, 0 }, { 0, 0 },
    { 0, 1 }, { 1, 1 }, { 1, 0 }, { 0, 0 },
    { 0, 1 }, { 1, 1 }, { 1, 0 }, { 0, 0 },
    { 0, 1 }, { 1, 1 }, { 1, 0 }, { 0, 0 },
    { 0, 1 }, { 1, 1 }, { 1, 0 }, { 0, 0 },
    { 0, 1 }, { 1, 1 }, { 1, 0 }, { 0, 0 },
  };
  colors = {positions.size(), {1, 1, 1, 1}};
  {
    vector<uint8_t> quads;
    quads.resize(positions.size());
    std::iota(quads.begin(), quads.end(), 0);
    indices = Quads_to_Triangles(quads);
  }
  return result;
}

inline auto create4Hedron() {
  std::tuple<vector<fvec3>, vector<fvec4>, vector<uint8_t>> result;
  auto& [positions, colors, indices] = result;
  positions = {
    { 0, 0, 0 },
    { 1, 1, 0 },
    { 0, 1, 1 },
    { 1, 0, 1 },
  };
  colors = {
    { 0, 0, 0, 1 },
    { 1, 0, 0, 1 },
    { 0, 1, 0, 1 },
    { 0, 0, 1, 1 },
  };
  indices = {
    0, 2, 1,
    0, 3, 2,
    0, 1, 3,
    1, 2, 3,
  };
  return result;
}

inline auto createPlane() {
  std::tuple<vector<fvec3>, vector<fvec4>, vector<uint8_t>> result;
  auto& [positions, colors, indices] = result;
  positions = {
    { 0, 0, 0 },
    { 1, 0, 0 },
    { 1, 1, 0 },
    { 0, 1, 0 },
  };
  colors = {
    { 1, 1, 1, 1 },
    { 1, 0, 0, 1 },
    { 0, 1, 0, 1 },
    { 0, 0, 1, 1 },
  };
  indices = Quads_to_Triangles(vector<uint8_t>{
    0, 1, 2, 3,
    4, 5, 6, 7,
  });
  return result;
}

inline std::tuple<vector<fvec3>, vector<fvec4>, vector<fvec2>, vector<uint8_t>> createUVPlane() {
  auto [positions, colors, indices] = createPlane();
  vector<fvec2> uvs = {
    { 0, 0 },
    { 1, 0 },
    { 1, 1 },
    { 0, 1 },
  };
  return std::make_tuple(positions, colors, uvs, indices);
}

//
// hsl <--> rgb \in [0, 1]^3
//

inline glm::fvec4 RGBtoHSL(glm::fvec4 c) {
  int sort[3];
  if (c.x >= c.y) {
    if (c.y >= c.z) {
      sort[0] = 0; sort[1] = 1; sort[2] = 2;
    } else {
      sort[2] = 1;
      if (c.x >= c.z) { sort[0] = 0; sort[1] = 2; }
      else            { sort[0] = 2; sort[1] = 0; }
    }
  } else {
    if (c.y >= c.z) {
      sort[0] = 1;
      if (c.x >= c.z) { sort[1] = 0; sort[2] = 2; }
      else            { sort[1] = 2; sort[2] = 0; }
    } else {
      sort[0] = 2; sort[1] = 1; sort[2] = 0;
    }
  }
  float h_base = 2 * sort[0];
  float h_dir = (sort[1] - sort[0]) % 3 == 1 ? 1 : -1;
  float dh = c[sort[1]] - c[sort[2]];
  auto h = (h_base + h_dir * dh) / 6;
  auto s = c[sort[0]] - c[sort[2]];
  auto l = (c.x + c.y + c.z) / 3;
  return glm::fvec4{ h, s, l, c.w };
};

//
// NOTE:
// Linear transf. (invertible)
// - c[sort[0]], c[sort[1]], c[sort[2]] |-> s, dh, l
// [   1    0   -1 ]
// [   0    1   -1 ]
// [ 1/3  1/3  1/3 ]
// and its inverse
// [ 2/3 -1/2  1 ]
// [-1/3  2/3  1 ]
// [ 1/3  1/3  1 ]
//
inline glm::fvec4 HSLtoRGB(glm::fvec4 d) {
  float h6 = d.x * 6, s = d.y, l = d.z;
  float dh;
  float csort[3];
  int inv[3];
  if (h6 <= 1) {
    inv[0] = 0; inv[1] = 1; inv[2] = 2; dh = h6;
  } else if (h6 <= 2) {
    inv[0] = 1; inv[1] = 0; inv[2] = 2; dh = 2 - h6;
  } else if (h6 <= 3) {
    inv[0] = 2; inv[1] = 0; inv[2] = 1; dh = h6 - 2;
  } else if (h6 <= 4) {
    inv[0] = 2; inv[1] = 1; inv[2] = 0; dh = 4 - h6;
  } else if (h6 <= 5) {
    inv[0] = 1; inv[1] = 2; inv[2] = 0; dh = h6 - 4;
  } else {
    inv[0] = 0; inv[1] = 2; inv[2] = 1; dh = 6 - h6;
  }
  csort[0] = + 2./3. * s - 1./3. * dh + l;
  csort[1] = - 1./3. * s + 2./3. * dh + l;
  csort[2] = - 1./3. * s - 1./3. * dh + l;
  return glm::fvec4{ csort[inv[0]], csort[inv[1]], csort[inv[2]], d.w };
};


//
// glm::ivec2, fvec2  <-->  ImVec2
// TODO: use IM_VEC2_CLASS_EXTRA
//

template<typename T>
inline ImVec2 toImVec2(glm::vec<2, T> v) {
  return ImVec2{static_cast<float>(v[0]), static_cast<float>(v[1])};
};

template<typename T>
inline glm::vec<2, T> fromImVec2(ImVec2 v) {
  return glm::vec<2, T>{static_cast<T>(v[0]), static_cast<T>(v[1])};
};

//
// checkGLShader, checkGLProgram
//

namespace gl {
  inline void enableDebugMessage() {
    static auto callback = [](
        GLenum source, GLenum type, GLuint id,
        GLenum severity, GLsizei length,
        const GLchar* message, void* userParam) {
      fprintf(
          stderr, "OpenGL Debug Message: %s type = 0x%x, severity = 0x%x, message = %s\n",
          type == GL_DEBUG_TYPE_ERROR ? "[ERROR]" : "",
          type, severity, message);
    };
    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(callback, 0);
  }

  inline std::pair<bool, std::string> checkShader(GLuint handle) {
    std::string log;
    GLint status = 0, log_length = 0;
    glGetShaderiv(handle, GL_COMPILE_STATUS, &status);
    glGetShaderiv(handle, GL_INFO_LOG_LENGTH, &log_length);
    if (log_length > 0) {
      log.resize(log_length);
      glGetShaderInfoLog(handle, log_length, NULL, (GLchar*)log.data());
    }
    return make_pair(status == GL_TRUE, log);
  }

  inline std::pair<bool, std::string> checkProgram(GLuint handle) {
    std::string log;
    GLint status = 0, log_length = 0;
    glGetProgramiv(handle, GL_LINK_STATUS, &status);
    glGetProgramiv(handle, GL_INFO_LOG_LENGTH, &log_length);
    if (log_length > 0) {
      log.resize(log_length);
      glGetProgramInfoLog(handle, log_length, NULL, (GLchar*)log.data());
    }
    return make_pair(status == GL_TRUE, log);
  }

  struct Program {
    GLuint handle_, vertex_shader_, fragment_shader_;

    Program(const char* vs_src, const char* fs_src) {
      vertex_shader_ = glCreateShader(GL_VERTEX_SHADER);
      fragment_shader_ = glCreateShader(GL_FRAGMENT_SHADER);
      handle_ = glCreateProgram();

      glShaderSource(vertex_shader_, 1, &vs_src, nullptr);
      glCompileShader(vertex_shader_);
      if (auto result = checkShader(vertex_shader_); !result.first) {
        throw std::runtime_error{"glCompileShader(vertex_shader_) faild\n" + result.second};
      }

      glShaderSource(fragment_shader_, 1, &fs_src, nullptr);
      glCompileShader(fragment_shader_);
      if (auto result = checkShader(fragment_shader_); !result.first) {
        throw std::runtime_error{"glCompileShader(fragment_shader_) faild\n" + result.second};
      }

      glAttachShader(handle_, vertex_shader_);
      glAttachShader(handle_, fragment_shader_);
      glLinkProgram(handle_);
      if (auto result = checkProgram(handle_); !result.first) {
        throw std::runtime_error{"glLinkProgram(handle_) faild\n" + result.second};
      }
    }
    ~Program() {
      glDetachShader(handle_, vertex_shader_);
      glDetachShader(handle_, fragment_shader_);
      glDeleteShader(vertex_shader_);
      glDeleteShader(fragment_shader_);
      glDeleteProgram(handle_);
    }

    void setUniform(const char* name, const glm::fvec4& value) {
      auto location = glGetUniformLocation(handle_, name);
      TOY_ASSERT_CUSTOM(location != -1, fmt::format("Uniform ({}) not found", name));
      glUniform4fv(location, 1, (GLfloat*)&value);
    }

    void setUniform(const char* name, const glm::fmat4& value) {
      auto location = glGetUniformLocation(handle_, name);
      TOY_ASSERT_CUSTOM(location != -1, fmt::format("Uniform ({}) not found", name));
      glUniformMatrix4fv(location, 1, GL_FALSE, (GLfloat*)&value);
    }

    void setUniform(const char* name, GLint value) {
      auto location = glGetUniformLocation(handle_, name);
      TOY_ASSERT_CUSTOM(location != -1, fmt::format("Uniform ({}) not found", name));
      glUniform1i(location, value);
    }
  };

  struct Framebuffer {
    TOY_CLASS_DELETE_COPY(Framebuffer)
    GLuint framebuffer_handle_, texture_handle_, depth_texture_handle_;
    glm::ivec2 size_ = {1, 1};

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

    void setSize(const glm::ivec2& size) {
      if (size[0] == 0 || size[1] == 0) {
        throw std::runtime_error{"Invalid argument: size[0] == 0 || size[1] == 0"};
      }
      size_ = size;
      glBindTexture(GL_TEXTURE_2D, texture_handle_);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size_.x, size_.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
      glBindTexture(GL_TEXTURE_2D, depth_texture_handle_);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, size_.x, size_.y, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    }
  };

  struct Texture {
    TOY_CLASS_DELETE_COPY(Texture)
    GLuint handle_;
    GLenum target_ = GL_TEXTURE_2D;
    std::map<GLenum, GLenum> params_ = {{GL_TEXTURE_MIN_FILTER, GL_NEAREST}};
    std::tuple<GLint, GLenum, GLenum> format_triple_ = {GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE};
    ivec2 size_;

    Texture() {
      glGenTextures(1, &handle_);
    }
    ~Texture() {
      glDeleteTextures(1, &handle_);
    }
    void applyParams() {
      glBindTexture(target_, handle_);
      for (auto& [name, value] : params_) {
        glTexParameteri(target_, name, value);
      }
    }
    // TODO: 1D, 3D, and other variants
    void setData(const ivec2& size, const GLvoid* data = nullptr) {
      size_ = size;
      applyParams();
      glTexImage2D(
          target_, 0, std::get<0>(format_triple_),
          size.x, size.y, 0, std::get<1>(format_triple_),
          std::get<2>(format_triple_), data);
    }
  };


  // Usable only for interleaved vertex buffer
  struct VertexRenderer {
    TOY_CLASS_DELETE_COPY(VertexRenderer)
    GLuint vertex_array_, array_buffer_, element_array_buffer_;
    GLenum primitive_mode_ = GL_TRIANGLES;
    GLenum index_type_;
    GLsizei num_indices_;

    VertexRenderer() {
      glGenBuffers(1, &array_buffer_);
      glGenBuffers(1, &element_array_buffer_);
      glGenVertexArrays(1, &vertex_array_);
    }

    ~VertexRenderer() {
      glDeleteBuffers(1, &array_buffer_);
      glDeleteBuffers(1, &element_array_buffer_);
      glDeleteVertexArrays(1, &vertex_array_);
    }

    template<typename T> void setIndexType();
    template<> void setIndexType< uint8_t>() { index_type_ = GL_UNSIGNED_BYTE;  };
    template<> void setIndexType<uint16_t>() { index_type_ = GL_UNSIGNED_SHORT; };
    template<> void setIndexType<uint32_t>() { index_type_ = GL_UNSIGNED_INT;   };

    template<typename T1, typename T2>
    void setData(const std::vector<T1>& vertices, const std::vector<T2>& indices) {
      glBindBuffer(GL_ARRAY_BUFFER, array_buffer_);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, element_array_buffer_);
      glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(T1), vertices.data(), GL_STREAM_DRAW);
      glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(T2), indices.data(), GL_STREAM_DRAW);
      num_indices_ = indices.size();
      setIndexType<T2>();
    }

    void setFormat(
        GLuint program, const char* name,
        GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer) {
      auto location = glGetAttribLocation(program, name);
      TOY_ASSERT_CUSTOM(location != -1, fmt::format("Vertex attribute ({}) not found", name));
      glBindVertexArray(vertex_array_);
      glBindBuffer(GL_ARRAY_BUFFER, array_buffer_);
      glEnableVertexAttribArray(location);
      glVertexAttribPointer(location, size, type, normalized, stride, pointer);
    }

    struct FormatParam {
      GLint size; GLenum type; GLboolean normalized; GLsizei stride; const void* pointer;
    };
    void setFormat(GLuint program, std::map<const char*, FormatParam> format_args) {
      for (auto& [name, f] : format_args) {
        setFormat(program, name, f.size, f.type, f.normalized, f.stride, f.pointer);
      }
    }

    void draw() {
      glBindVertexArray(vertex_array_);
      glBindBuffer(GL_ARRAY_BUFFER, array_buffer_);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, element_array_buffer_);
      glDrawElements(primitive_mode_, num_indices_, index_type_, 0);
    }
  };
}


//
// Example:
//
// Cli cli{argc, argv};
// auto names = cli.getArgs<string>();   // vector<string>
// auto n     = cli.getArg<int>("-n");   // optional<int>
// auto force = cli.checkArg("--force"); // bool
// if (names.empty())
//   cout << cli.help();
//

struct Cli {
  const int argc_; const char** argv_;
  bool has_positional_arg_ = false;
  std::vector<std::string> value_flags_;
  std::vector<std::string> boolean_flags_;

  std::string help() {
    auto join = [](const std::vector<std::string>& v) {
      std::string result;
      for (auto i : Range{v.size()}) {
        result += " " + v[i];
      }
      return result;
    };
    return fmt::format(
        "Usage: <program>{}{}{}\n",
        join(boolean_flags_), join(value_flags_),
        has_positional_arg_ ? " <arg-0> <arg-1> ..." : "");
  }

  template<typename T>
  T _convert(const char* s) {
    std::istringstream stream{s};
    T result;
    stream >> result;
    return result;
  }

  template<>
  std::string _convert(const char* s) {
    return std::string{s};
  }

  template<typename T>
  std::optional<T> getArg(const char* flag) {
    value_flags_.push_back(std::string(flag) + " <arg>");
    for (auto i = 1; i < argc_; i++) {
      if (strcmp(argv_[i], flag) == 0 && i + 1 < argc_) {
        return _convert<T>(argv_[i + 1]);
      }
    }
    return {};
  }

  template<typename T>
  std::vector<T> getArgs() {
    has_positional_arg_ = true;
    std::vector<T> results;
    for (auto i = 1; i < argc_; i++) {
      if (argv_[i][0] == '-') {
        i += 2;
        continue;
      }
      results.push_back(_convert<T>(argv_[i]));
    }
    return results;
  }

  bool checkArg(const char* flag) {
    boolean_flags_.push_back(flag);
    for (auto i = 1; i < argc_; i++) {
      if (strcmp(argv_[i], flag) == 0) {
        return true;
      }
    }
    return false;
  }
};


} } // namespace utils // namespace toy
