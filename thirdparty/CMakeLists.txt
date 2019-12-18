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
add_subdirectory(glfw EXCLUDE_FROM_ALL)


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
target_link_libraries(gl3w PRIVATE ${OPENGL_LIBRARIES} ${CMAKE_DL_LIBS})

# - imgui
set(IMGUI_SRCS
  ${IMGUI_DIR}/imgui.cpp
  ${IMGUI_DIR}/imgui_demo.cpp
  ${IMGUI_DIR}/imgui_draw.cpp
  ${IMGUI_DIR}/imgui_widgets.cpp
  ${IMGUI_DIR}/examples/imgui_impl_glfw.cpp
  ${IMGUI_DIR}/examples/imgui_impl_opengl3.cpp
)
set(IMGUI_INCLUDE_DIRS
  ${IMGUI_DIR} # imgui.h, imgui_internal.h
  ${IMGUI_DIR}/examples # imgui_impl_glfw.h, imgui_impl_sdl.h,, imgui_impl_opengl3.h
  ${IMGUI_DIR}/misc/cpp # imgui_scoped.h
)
add_library(imgui ${IMGUI_SRCS})
target_compile_definitions(imgui PUBLIC IMGUI_IMPL_OPENGL_LOADER_GL3W)
target_include_directories(imgui PUBLIC ${IMGUI_INCLUDE_DIRS})
target_link_libraries(imgui PUBLIC gl3w glfw)

# - imgui example
add_executable(imgui_example ${IMGUI_DIR}/examples/example_glfw_opengl3/main.cpp)
target_link_libraries(imgui_example PRIVATE imgui)

# - imgui with SDL (only for comparision with glfw)
find_package(PkgConfig)
pkg_check_modules(SDL2 REQUIRED sdl2)
set(IMGUI_SDL_SRCS
  ${IMGUI_DIR}/imgui.cpp
  ${IMGUI_DIR}/imgui_demo.cpp
  ${IMGUI_DIR}/imgui_draw.cpp
  ${IMGUI_DIR}/imgui_widgets.cpp
  ${IMGUI_DIR}/examples/imgui_impl_sdl.cpp
  ${IMGUI_DIR}/examples/imgui_impl_opengl3.cpp
)
add_library(imgui_sdl ${IMGUI_SDL_SRCS})
target_compile_definitions(imgui_sdl PUBLIC IMGUI_IMPL_OPENGL_LOADER_GL3W)
target_include_directories(imgui_sdl PUBLIC ${IMGUI_INCLUDE_DIRS} ${SDL2_INCLUDE_DIRS})
target_link_libraries(imgui_sdl PUBLIC gl3w ${SDL2_LIBRARIES})

add_executable(imgui_sdl_example ${IMGUI_DIR}/examples/example_sdl_opengl3/main.cpp)
target_link_libraries(imgui_sdl_example PRIVATE imgui_sdl)



#
# stb
#

add_library(stb ${CMAKE_CURRENT_SOURCE_DIR}/stb.c)
target_include_directories(stb PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/stb)


#
# cgltf
#

add_library(cgltf ${CMAKE_CURRENT_SOURCE_DIR}/cgltf.c)
target_include_directories(cgltf PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/cgltf)

#
# yocto
#

set(YOCTO_OPENGL OFF CACHE BOOL " " FORCE)
set(YOCTO_TESTING OFF CACHE BOOL " " FORCE)
add_subdirectory(yocto-gl EXCLUDE_FROM_ALL) # => add_library(yocto ...)

# patch the target in order to propagate include directories for targets depending on this
target_include_directories(yocto PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/yocto-gl/yocto)
