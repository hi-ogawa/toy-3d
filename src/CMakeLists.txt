find_package(PkgConfig)
pkg_check_modules(GLM REQUIRED glm)
pkg_search_module(GTEST REQUIRED gtest)

set(GLTF_MODEL_DIR "${CMAKE_SOURCE_DIR}/../../others/glTF-Sample-Viewer/assets/models"
    CACHE PATH "local directly of gltf model repository")

# dependencies
link_libraries(glfw gl3w imgui fmt stb cgltf material_icons)

# include dirs (only for <imgui_user_config.h>)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# defines
add_compile_definitions(
  TOY_DIR="${CMAKE_SOURCE_DIR}"
  GLTF_MODEL_DIR="${GLTF_MODEL_DIR}"
  IMGUI_USER_CONFIG=<imgui_user_config.h> # cf ingui.h
)

# executables
add_executable(main main.cpp)
add_executable(kdtree_imgui_example kdtree_imgui_example.cpp)
add_executable(panel_system_example panel_system_example.cpp)
add_executable(simple_render_example simple_render_example.cpp)
add_executable(gl_coordinate_system_example gl_coordinate_system_example.cpp)
add_executable(gltf_example gltf_example.cpp)
add_executable(imgui_texture_example imgui_texture_example.cpp)
add_executable(imgui_framebuffer_example imgui_framebuffer_example.cpp)
add_executable(playground playground.cpp)
add_executable(scene_example scene_example.cpp)

# testing
add_executable(test test.cpp kdtree_test.cpp utils_test.cpp scene_test.cpp)
target_include_directories(test PRIVATE ${GTEST_INCLUDE_DIRS})
target_link_libraries(test PRIVATE ${GTEST_LIBRARIES} fmt)
