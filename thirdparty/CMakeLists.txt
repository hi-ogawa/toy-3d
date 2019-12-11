#
# fmtlib/fmt: add_library(fmt ..)
#

add_subdirectory(fmt)


#
# GLFW: add_library(glfw ..)
#

set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL " " FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL " " FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL " " FORCE)
set(GLFW_INSTALL OFF CACHE BOOL " " FORCE)
add_subdirectory(glfw)


#
# ImGui, GL3W
#

set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/imgui)
set(OpenGL_GL_PREFERENCE GLVND)
find_package(OpenGL REQUIRED)

# - gl3w
set(GL3W_SRCS ${IMGUI_DIR}/examples/libs/gl3w/GL/gl3w.c)
set(GL3W_INCLUDE_DIRS ${IMGUI_DIR}/examples/libs/gl3w)
add_library(gl3w ${GL3W_SRCS})
target_include_directories(gl3w PUBLIC ${GL3W_INCLUDE_DIRS})
target_link_libraries(gl3w PRIVATE ${OPENGL_LIBRARIES})

# - imgui
set(IMGUI_SRCS
  ${IMGUI_DIR}/imgui.cpp
  ${IMGUI_DIR}/imgui_demo.cpp
  ${IMGUI_DIR}/imgui_draw.cpp
  ${IMGUI_DIR}/imgui_widgets.cpp
  ${IMGUI_DIR}/examples/imgui_impl_glfw.cpp
  ${IMGUI_DIR}/examples/imgui_impl_opengl3.cpp
  ${IMGUI_DIR}/
)
set(IMGUI_INCLUDE_DIRS
  ${IMGUI_DIR} # imgui.h, imgui_internal.h
  ${IMGUI_DIR}/examples # imgui_impl_glfw.h, imgui_impl_opengl3.h
)
set(IMGUI_EXAMPLE_SRCS ${IMGUI_DIR}/examples/example_glfw_opengl3/main.cpp)
add_library(imgui ${IMGUI_SRCS})
target_compile_definitions(imgui PRIVATE IMGUI_IMPL_OPENGL_LOADER_GL3W)
target_include_directories(imgui PUBLIC ${IMGUI_INCLUDE_DIRS})
target_link_libraries(imgui PRIVATE gl3w)

# - imgui example
add_executable(imgui_example ${IMGUI_EXAMPLE_SRCS} ${IMGUI_SRCS})
target_compile_definitions(imgui_example PRIVATE IMGUI_IMPL_OPENGL_LOADER_GL3W)
target_include_directories(imgui_example PRIVATE ${IMGUI_INCLUDE_DIRS})
target_link_libraries(imgui_example PRIVATE imgui gl3w glfw)


#
# yocto-gl
#
