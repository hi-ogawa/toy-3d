- imgui window as base UI section
- python plugin to add UI
  - imgui python binding
- gltf for basic scene structure
- graph/node system to script object behaviour
- yocto-gl for basic geometry utility

TODO

- define/setup basic app state
  - glfw window system integration
  - imgui integration
    - [@] imgui window layout system
      - no titlebar
      - menu with panel name
      - no move
      - resize like tile by ourselves
      - [@] start from just two panels on the left and right
        - investigate how it's handled by default
          - how to change cursor
          - how to change size, position
            - cf SetNextWindowPos, SetNextWindowSize
    - [?] custom render inside of imgui window

- scene viewport

- scene property editor

- define rendering model

- object/mesh picker
