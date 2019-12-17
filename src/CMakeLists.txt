find_package(PkgConfig)
pkg_check_modules(GLM REQUIRED glm)
pkg_search_module(GTEST REQUIRED gtest)

link_libraries(glfw gl3w imgui fmt stb cgltf)

add_executable(main main.cpp)

add_executable(kdtree_imgui_example kdtree_imgui_example.cpp)
add_executable(panel_system_example panel_system_example.cpp)

add_executable(simple_render_example simple_render_example.cpp)

add_executable(gl_coordinate_system_example gl_coordinate_system_example.cpp)

add_executable(gltf_example gltf_example.cpp)

add_executable(imgui_texture_example imgui_texture_example.cpp)

add_executable(imgui_framebuffer_example imgui_framebuffer_example.cpp)

add_executable(test test.cpp kdtree_test.cpp utils_test.cpp)
target_include_directories(test PRIVATE ${GTEST_INCLUDE_DIRS})
target_link_libraries(test PRIVATE ${GTEST_LIBRARIES} fmt)

add_executable(playground playground.cpp)
