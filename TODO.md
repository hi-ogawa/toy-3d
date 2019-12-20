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

- toy-3d
  - [x] panel system
    - [x] show main menu via PanelManager
    - [x] add/split/remove panel
    - [x] resize panels
    - [x] show base panel menu
    - [x] consider panel interface
    - [x] better resize interaction
  - [@] simple 3d viewport
    - brainstorming
      - yocto/gl
        - scene data structure
        - gltf loader
      - load yocto's test scene
        - integrate yocto library (scene struct, loader etc...)
      - draw scene with simpler shaders
        - component based "mesh", "material" system?
    - todo
      - [@] borrow yocto/gl's utility first
        - [@] load tests/shapes1.yaml via `load_scene`
        - [ ] render scene
          - defualt camera
          - cubeshape -> vertex array
          -
          -
          - framebuffer
    - later
      - [ ] object (no hierarchy)
      - [ ] mesh component (triangles)
      - [ ] transform component
      - [ ] camera component
      - object hierarchy
      - material component

- git
  - undo merge and rebase (example: https://github.com/ocornut/imgui/pull/2197)
  - `git log --graph --oneline master HEAD`
  - `git rebase feature-before-merge`
  - `git rebase master` (or --interactive variant)

```
* master
*
| * feature-done (HEAD)
| * feature-merged
| |\
| | * feature-before-merge
| | * feature-started-2
| |/
|/|
* |
* |
| * master-old-more
| * feature-started-1
|/
* branch-point1
*

==>

* feature-done
* feature-more
* feature-started-2
* feature-started-1
| * master
| *
| *
| *
|/
* branch-point1
*
```


# 2019/12/15

- [x] simple_render_example
  - frame buffer setup
    - depth buffer setup
  - perspective projection


# 2019/12/16

- [x] OpenGL coordinate frame
  - surface orientation
  - depth

- [@] Transform property editor

- unity3d and c# scripting
  - mesh generation

- normal vector in pixel shader
  - tangent space
  - https://archive.blender.org/wiki/index.php/Dev:Shading/Tangent_Space_Normal_Maps/


# 2019/12/17

- scene system
  - node hierarchy
    - for starter, all flat
  - component system


- rendering model
  - [ ] yocto/gl
  - [ ] gltf viewer
  - diffuse
  - image based lighting


- viewport ui
  - [ ] gizmo state, rendering
  - [ ] ray cast

- imgui style in panel system
  - [x] change size of main menu
    - need to tell PanelManager real main menu's size
  - [x] panel content size

- render offscreen for testing?
  - only depends on glfw context creation?


# 2019/12/18

- explore more imgui
  - https://github.com/CedricGuillemet/ImGuizmo
  - https://github.com/ocornut/imgui/issues/786
  - https://github.com/ocornut/imgui/issues/2117
    - gltf multi window on linux

- explore imgui drag&drop
  - gnome/wayland issue
    - due to recent mutter bug, drag&drop from nautilus doesn't work.
      use `GDK_BACKEND=x11 nautilus`.
  - maybe we better off using sdl
    - https://wiki.libsdl.org/SDL_DropEvent
    - it has `SDL_DROPBEGIN`, but it's only the marker for multifile drop.
  - [@] since we cannot be notified when start dragging from external source,
    let's employ "two-stage" drag&drop ui.

# 2019/12/19

- [x] gltf mesh/material import

- Custom drawing in ImGui
  - curve, grid, plot, color

- ImGui
  - utilize imconfig.h
    - IM_VEC2_CLASS_EXTRA, IMGUI_DEFINE_MATH_OPERATORS


- Organize scene system
  - model list
  - mesh list
  - material list
  - mesh, material, model
  - mesh import/change

```
scene
- meshes (shared ptr)
- textures (shared ptr)
  - gl resource
- instances (shared ptr)
  - mesh (weak ptr)
  - material (weak ptr)
- materials (shared ptr)
  - textures (weak ptr)


from editor,
- add/remove mesh, texture
```

- clangd extension syntax color fix

- panel layout system e.g.

```
{
  "split": 0.6,
  "type": "Horizontal",
  "children": [
    { "panel": "Render" },
    {
      "split": 0.6,
      "type": "Vertical",
      "children": [
        { "panel": "Property" },
        { "panel": "Image" }
      ]
    }
  ]
}
```

- c++ smart pointer implementation
  - shared ptr, weak ptr

- lighting technique
  - https://github.com/KhronosGroup/glTF/blob/master/extensions/2.0/Khronos/KHR_lights_punctual/README.md
  - https://github.com/KhronosGroup/glTF/blob/master/extensions/2.0/Vendor/EXT_lights_image_based/README.md


# 2019/12/20

- explore 3d gizmo
  - low-level imgui geometry draw/interaction api

- [x] scene system with example
  - [x] gltf import to our scene system
  - [ ] example app
    - [x] renderer
    - [@] scene property editor
    - [@] gltf file loader ui

- [-] gltf example model statistics
  - uint16_t vertex index type?
    - exception: SciFiHelmet (70074 vertices > 2**16)
  - single mesh
    - exception: AlphaBlendModeTest
  - single primitive per mesh
    - exception: BrainStem
  - single scene
    - exception: ??
  - integer-encoded float
    - CesiumMan?


```
TODO simpler_render_example

[x] drawing
  - draw target (texture, frame buffer)
  - clear color
[x] setup shader program
  - [x] small gl program wrapper
  - [x] cube vertex array
[x] OpenGL/gltf cordinate system
  - right hand frame
  - "-Z" camera lookat direction
  - "+X" right
  - "+Y" up
[x] debug strategy
  - try remove projection, transform, etc...
  - directly specify vertex position
  - [x] use default framebuffer (so that user-friendly default is setup out-of-box)
  - [x] my fmat4 inverse might be wrong?
  - [x] try point rasterization
[x] mesh
  - alloc vertex array
  - transf
  - draw program
  - check gl's coordinate system (z depth direction)
[x] camera
  - params..
  - transf
[x] "transform" property editor
  - [x] imgui
  - [-] gizmo
[x] draw multiple meshes
[x] draw ui for each model
[x] support simple mesh base color texture
  - [x] update shader (vertex attr + uniform)
  - [x] data structure
  - [x] example mesh uv
  - [x] debug
    - [x] preview image via imgui (data is loaded correctly)
    - [x] uv coordinate is correct
    - [x] maybe framebuffer specific thing? (no, default buffer got same result.)
    - [x] gl version different from imgui_texture_example (no, it's same)
    - [x] it turns out it's mis understanding of OpenGL texture/sampler state api.
[x] draw mesh from gltf
  - [x] only import vertex positions, uv, indices, and base color texture
[ ] mesh/texture loader ui
[ ] mesh/texture examples
[ ] material
   - no vertex color
   - property editor
[ ] organize scene system
  - [ ] immitate gltf data structure
  - scene hierarchy
  - render system (render resource vs render parameter)
  - [ ] draw world axis and half planes
  - [ ] load scene from file
[ ] rendering model
  - https://github.com/KhronosGroup/glTF/blob/master/specification/2.0/README.md#appendix-b-brdf-implementation
```
