#pragma once

#include <imgui.h>
#include <glm/glm.hpp>

namespace toy {
namespace misc {

template<typename T>
ImVec2 toImVec2(glm::vec<2, T> v) {
  return ImVec2{static_cast<float>(v[0]), static_cast<float>(v[1])};
};

template<typename T>
glm::vec<2, T> fromImVec2(ImVec2 v) {
  return glm::vec<2, T>{static_cast<T>(v[0]), static_cast<T>(v[1])};
};

} // namespace misc
} // namespace toy
