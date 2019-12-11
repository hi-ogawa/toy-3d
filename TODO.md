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
- [@] custom render inside of imgui window
  - Demo > Widgets > Images
  - ImTextureID
  - render some texture onto imgui window
    - load static asset into texture
    - draw into texture
- [ ] try https://github.com/ocornut/imgui/pull/2197
- [ ] scene viewport

- scene property editor

- define rendering model

- object/mesh picker
