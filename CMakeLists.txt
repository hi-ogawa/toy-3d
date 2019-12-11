cmake_minimum_required(VERSION 3.0.0)
project(toy-3d LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS
  -fcolor-diagnostics
)

add_subdirectory(thirdparty)
add_subdirectory(src)
