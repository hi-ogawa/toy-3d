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
using std::vector, std::string;
}

struct RangeHelper {
  int start_, end_;

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

  Iterator begin() { return Iterator{start_}; };
  Iterator end() { return Iterator{end_}; };
};

RangeHelper inline range(int start, int stop) {
  return RangeHelper{start, std::max(start, stop)};
}

RangeHelper inline range(int stop) {
  return range(0, stop);
}


template<typename T>
struct EnumerateHelper {
  size_t start_, end_;
  T* data;

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

template<typename T>
inline EnumerateHelper<T> enumerate(vector<T>& container) {
  return EnumerateHelper<T>{0, container.size(), container.data()};
}

template<typename T>
inline EnumerateHelper<T> enumerate(T container[], size_t size) {
  return EnumerateHelper<T>{0, size, container};
}

// Primitive closest point (cf. closed convex projection theorem)

namespace hit {

inline auto _isSmall(float f) { return glm::abs(f) < glm::epsilon<float>(); }

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
  if (_isSmall(a)) { return {}; }
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
  TOY_ASSERT(!_isSmall(a));
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

inline vector<fvec4> clip4D_ConvexPoly_HalfSpace(
    const vector<fvec4>& vs, // dim(span{vi - v0 | i}) = 2 (thus vs.size() >= 3)
    const fvec4& q, // half space as { u | dot(u - q, v) >= 0 }
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
    for (auto i : range(1, N)) {
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
  result.reserve((end - start) % N + 2);

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

inline auto Line_Plane_4D(
    const fvec4& p, // line point
    const fvec4& v, // ray vector
    const fvec4& q, // plane point
    const fvec4& n  // plane normal
)-> std::optional<float> // t where p + t v is intersection unless <v, n> = 0
{
  // <(p + t v) - q, n> = 0  <=>  t <v, n> = <q - p, n>
  auto a = glm::dot(v, n);
  auto b = glm::dot(q - p, n);
  if (glm::abs(a) < glm::epsilon<float>()) { return {}; }
  return b / a;
}

// ClipVolume = { (x, w) \in R^3 x R | w >=0 and \forall i. |x_i| <= w }
inline auto Line_ClipVolume(
    const fvec4& p, // line point
    const fvec4& v  // ray vector with v.w >= 0
)-> std::optional<std::pair<float, float>>
    // t_in <= t_out where p + t v \in Boundary(ClipVolume)
    // t_out = FLT_MAX means ray doesn't get out (i.e. intersect at infinity).
{
  // fmt::print("p = ({}, {}, {}, {}), v = ({}, {}, {}, {})\n",
  //     p.x, p.y, p.z, p.w, v.x, v.y, v.z, v.w);

  float t_in_ = -FLT_MAX, t_out_ = FLT_MAX;

  auto _subRoutine = [&p, &v, &t_in_, &t_out_](
    const std::optional<float>& t1,
    const std::optional<float>& t2
  ) {
    vector<float> ts; // float ts[2];
    if (t1) {
      auto q = p + (*t1) * v;
      if (q.w >= 0) {
        ts.push_back(*t1);
      }
    }
    if (t2) {
      auto q = p + (*t2) * v;
      if (q.w >= 0) {
        ts.push_back(*t2);
      }
    }
    if (ts.size() == 2) {
      // TODO:
      // for now, ad-hocly handle t1 ~= t2 (which happens for exact y-axis (p.x = v.x = 0))
      // probably, it needs to check if e.g. v.w > v.x (otherwise it doesn't go inside ClipVolume)
      if (_isSmall(ts[0] - ts[1])) {
        t_in_ = std::max(t_in_,  ts[0]);
        return;
      }

      auto m = ts[0] < ts[1] ? ts[0] : ts[1];
      auto M = ts[0] < ts[1] ? ts[1] : ts[0];
      t_in_  = std::max(t_in_,  m);
      t_out_ = std::min(t_out_, M);
    } else if (ts.size() == 1) {
      t_in_ = std::max(t_in_, ts[0]);
    }
  };

  _subRoutine(
      Line_Plane_4D(p, v, {0, 0, 0, 0}, {1, 0, 0,-1}),  // x = +w plane
      Line_Plane_4D(p, v, {0, 0, 0, 0}, {1, 0, 0, 1})); // x = -w plane
  _subRoutine(
      Line_Plane_4D(p, v, {0, 0, 0, 0}, {0, 1, 0,-1}),  // y = +w plane
      Line_Plane_4D(p, v, {0, 0, 0, 0}, {0, 1, 0, 1})); // y = -w plane
  _subRoutine(
      Line_Plane_4D(p, v, {0, 0, 0, 0}, {0, 0, 1,-1}),  // z = +w plane
      Line_Plane_4D(p, v, {0, 0, 0, 0}, {0, 0, 1, 1})); // z = -w plane

  if (t_in_ == FLT_MIN) { return {}; }
  return std::make_pair(t_in_, t_out_);
};

// this takes only p.w <= q.w
inline std::optional<std::pair<fvec4, fvec4>> _clipLineSegment(
    const fvec4& p, const fvec4& q) {
  auto v = q - p;
  auto t_in_out = Line_ClipVolume(p, v);
  if (!t_in_out) { return {}; }

  auto& [t_in, t_out] = *t_in_out;
  if (1 < t_in) { return {}; }
  if (t_out < 0) {
    // fmt::print("p = ({}, {}, {}, {}), v = ({}, {}, {}, {})\n",
    //     p.x, p.y, p.z, p.w, v.x, v.y, v.z, v.w);
    // fmt::print("(t_in, t_out) = ({}, {})\n", t_in, t_out);
    return {};
  }

  fvec4 p_in = p + std::max(0.f, t_in) * v;
  fvec4 q_out = p + std::min(t_out, 1.f) * v;
  return std::make_pair(p_in, q_out);
}

inline std::optional<std::pair<fvec4, fvec4>> clipLineSegment(
    const fvec4& p, const fvec4& q) {
  bool flip = p.w > q.w;
  auto tmp = _clipLineSegment(flip ? q : p, flip ? p : q);
  if (!tmp) { return {}; }
  auto& [r, s] = *tmp;
  return std::make_pair(flip ? s : r, flip ? r : s);
}

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

inline fmat4 composeTransform(const fvec3 s, const fvec3 r, const fvec3 t) {
  fmat3 R = ExtrinsicEulerXYZ_to_SO3(r);
  return { {R[0] * s.x, 0},
           {R[1] * s.y, 0},
           {R[2] * s.z, 0},
           {         t, 1}, };
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

// TODO: superceded by scene::gltf in scene.hpp
// cf. https://github.com/KhronosGroup/glTF/blob/master/specification/2.0/README.md
struct GltfData {

  struct Texture {
    string name;
    string filename;
  };

  struct Material {
    string name;
    fvec4 base_color_factor = {1, 1, 1, 1};
    std::shared_ptr<Texture> base_color_tex;
  };

  struct Mesh {
    struct VertexAttrs {
      fvec3 position;
      fvec3 normal;
      fvec4 tangent;
      fvec2 texcoord;
      fvec4 color = {1, 1, 1, 1};
    };

    string name;
    vector<uint16_t> indices;
    vector<VertexAttrs> vertices;
    std::shared_ptr<Material> material;
  };

  string filename;

  vector<Texture> textures;
  vector<Material> materials;
  vector<Mesh> meshes;

  // Cf. cgltf_accessor_read_index, cgltf_calc_size
  static void* readAccessor(const cgltf_accessor* accessor, size_t index) {
    cgltf_size offset = accessor->offset + accessor->buffer_view->offset;
    uint8_t* element = (uint8_t*)accessor->buffer_view->buffer->data;
    return (void*)(element + offset + accessor->stride * index);
  }

  static GltfData load(const char* filename) {
    GltfData result;
    result.filename = filename;
    std::string dirname = {filename , 0, std::string{filename}.rfind('/')};
    std::map<cgltf_texture*, std::shared_ptr<Texture>> tmp_map1;
    std::map<cgltf_material*, std::shared_ptr<Material>> tmp_map2;

    // Load gltf file
    cgltf_options params = {};
    cgltf_data* data;
    if (cgltf_parse_file(&params, filename, &data) != cgltf_result_success) {
      throw std::runtime_error{fmt::format("cgltf_parse_file failed: {}", filename)};
    }
    std::unique_ptr<cgltf_data, decltype(&cgltf_free)> final_action{data, &cgltf_free};

    if (cgltf_load_buffers(&params, data, filename) != cgltf_result_success) {
      throw std::runtime_error{fmt::format("cgltf_load_buffers failed: {}", filename)};
    }

    //
    // Convert data
    //
    // - Strategy
    //   - read Texture -> Material -> Mesh in this order
    //   - each cgltf_primitive becomse Mesh
    //   - everything triangle
    // - Assertions
    //   - indices type is uint16_t (this is the case all but SciFiHelmet (https://github.com/KhronosGroup/glTF-Sample-Models/blob/master/2.0/SciFiHelmet))
    //   - vertex attribute is already float
    //

    // Load textures
    for (auto [i, gtex] : enumerate(data->textures, data->textures_count)) {
      TOY_ASSERT(gtex->image->uri);
      result.textures.push_back({
          .name = gtex->image->uri,
          .filename = dirname + "/" + gtex->image->uri });
    }

    // Load materials
    for (auto [i, gmat] : enumerate(data->materials, data->materials_count)) {
      auto& mat = result.materials.emplace_back();
      mat.name = gmat->name;
      if (gmat->has_pbr_metallic_roughness) {
        auto& pbr = gmat->pbr_metallic_roughness;
        mat.base_color_factor = *(fvec4*)(pbr.base_color_factor);
        mat.base_color_tex = tmp_map1[pbr.base_color_texture.texture];
      }
    }

    // Load meshes
    for (auto [i, gmesh] : enumerate(data->meshes, data->meshes_count)) {
      for (auto [j, gprim] : enumerate(gmesh->primitives, gmesh->primitives_count)) {
        auto& mesh = result.meshes.emplace_back();
        mesh.name = fmt::format("{} ({})", gmesh->name, j + 1);
        mesh.material = tmp_map2[gprim->material];

        // Read indices
        {
          auto accessor = gprim->indices;
          TOY_ASSERT(accessor->component_type == cgltf_component_type_r_16u);
          mesh.indices.resize(accessor->count);
          for (auto k : range(accessor->count)) {
            mesh.indices[k] = *(uint16_t*)readAccessor(accessor, k);
          }
        }

        // Read vertex attributes
        // TODO: read in non-interleaved mode and check which attributes are found
        int vertex_count = -1;
        for (auto [k, gattr] : enumerate(gprim->attributes, gprim->attributes_count)) {
          TOY_ASSERT(gattr->index == 0);

          auto accessor = gattr->data;
          if (vertex_count == -1) {
            vertex_count = accessor->count;
            mesh.vertices.resize(vertex_count);
          }
          TOY_ASSERT(vertex_count == accessor->count);
          TOY_ASSERT(accessor->component_type == cgltf_component_type_r_32f);

          switch (gattr->type) {
            case cgltf_attribute_type_position: {
              for (auto k : range(accessor->count)) {
                mesh.vertices[k].position = *(fvec3*)readAccessor(accessor, k);
              }
              break;
            }
            case cgltf_attribute_type_normal: {
              for (auto k : range(accessor->count)) {
                mesh.vertices[k].normal = *(fvec3*)readAccessor(accessor, k);
              }
              break;
            }
            case cgltf_attribute_type_tangent: {
              for (auto k : range(accessor->count)) {
                mesh.vertices[k].tangent = *(fvec4*)readAccessor(accessor, k);
              }
              break;
            }
            case cgltf_attribute_type_texcoord: {
              for (auto k : range(accessor->count)) {
                mesh.vertices[k].texcoord = *(fvec2*)readAccessor(accessor, k);
              }
              break;
            }
            case cgltf_attribute_type_color: {
              for (auto k : range(accessor->count)) {
                mesh.vertices[k].color = *(fvec4*)readAccessor(accessor, k);
              }
              break;
            }
            default:;
          }
        }
      }
    }


    return result;
  }
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
      for (auto i : range(v.size())) {
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
