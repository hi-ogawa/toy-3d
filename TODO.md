- imgui window as base UI section
- python plugin to add UI
  - imgui python binding
- gltf for basic scene structure
- graph/node system to script object behaviour
- yocto-gl for basic geometry utility

TODO

- [x] glfw window system integration
- [x] imgui integration
- [x] introduce "command" for better handling of global state mutation
  - just `vector<std::function<void()>>`, for starter.
- [-] kdtree-based imgui window layout
  - [x] add panels, draw panels
  - [x] subtract main menu bar size
  - [x] panel split
  - [x] show resize cursor on separator
  - [x] panel remove (handle in a "non-immediate" mode)
  - [-] handle resize on mousedown (not that good yet. see)
- [x] imgui texture
- [x] render texture quad
  - [x] OpenGL texture spec
    - https://www.khronos.org/opengl/wiki/Image_Format
    - https://www.khronos.org/opengl/wiki/GLAPI/glTexImage2D
  - [x] PNG image spec
    - https://tools.ietf.org/html/rfc2083
- [x] render onto non default framebuffer
  - https://www.khronos.org/opengl/wiki/Framebuffer_Object
  - render texture onto imgui window

- [@] simple scene viewport
  - yocto/gl
    - scene data structure
    - gltf loader
  - load yocto's test scene
    - integrate yocto library (scene struct, loader etc...)
  - draw scene with simpler shaders
    - component based "mesh", "material" system?

- scene property editor

- define rendering model

- object/mesh picker

- theory
  - multisampling (rasterization)
  - texture sampler


# 2019/12/13

- c++
  - inline
  - struct parameter

- unity3d and c# scripting
  - mesh generation

# 2019/12/14

- toy-yaml
