#include <glm/glm.hpp>

//
// For glm::vec<2, T> <==> ImVec2
//

//
// #define IM_VEC2_CLASS_EXTRA                                             \
// template<typename T>                                                    \
// ImVec2(const glm::vec<2, T>& v)                                         \
//   : x{static_cast<float>(v.x)},                                         \
//     y{static_cast<float>(v.y)} {}                                       \
// template<typename T>                                                    \
// ImVec2& operator=(const glm::vec<2, T>& v) {                            \
//   x = static_cast<float>(v.x);                                          \
//   y = static_cast<float>(v.y);                                          \
//   return *this;                                                         \
// }                                                                       \
// template<typename T>                                                    \
// inline glm::vec<2, T> glm() {                                           \
//   return glm::vec<2, T>{static_cast<T>(x), static_cast<T>(y)};          \
// };                                                                      \

//
// Only for glm::fvec2/4 <==> ImVec2/4
//

#define IM_VEC2_CLASS_EXTRA                         \
ImVec2(const glm::fvec2& v) : x{v.x}, y{v.y} {}     \
ImVec2& operator=(const glm::fvec2& v) {            \
  x = v.x;                                          \
  y = v.y;                                          \
  return *this;                                     \
}                                                   \
inline glm::fvec2 glm() {                           \
  return {x, y};                                    \
};                                                  \

#define IM_VEC4_CLASS_EXTRA                         \
ImVec4(const glm::fvec4& v)                         \
  : x{v.x}, y{v.y}, z{v.z}, w{v.w} {}               \
ImVec4& operator=(const glm::fvec4& v) {            \
  x = v.x;                                          \
  y = v.y;                                          \
  z = v.z;                                          \
  w = v.w;                                          \
  return *this;                                     \
}                                                   \
inline glm::fvec4 glm() {                           \
  return {x, y, z, w};                              \
};                                                  \
