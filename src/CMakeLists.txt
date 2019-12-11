find_package(PkgConfig)
pkg_check_modules(GLM REQUIRED glm)
pkg_search_module(GTEST REQUIRED gtest)

include_directories(${GLM_INCLUDE_DIRS})
link_libraries(glfw gl3w imgui fmt)

add_executable(main main.cpp)

add_executable(kdtree_imgui_example kdtree_imgui_example.cpp)

add_executable(test test.cpp kdtree_test.cpp)
target_include_directories(test PRIVATE ${GTEST_INCLUDE_DIRS})
target_link_libraries(test PRIVATE ${GTEST_LIBRARIES} fmt)
