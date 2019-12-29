Cf.

- https://github.com/google/material-design-icons
- https://github.com/juliettef/IconFontCppHeaders
  - https://github.com/juliettef/IconFontCppHeaders/blob/master/IconsMaterialDesign.h


Download files

```
DIR=https://github.com/google/material-design-icons/blob/3.0.1/iconfont/
curl -L "${DIR}/MaterialIcons-Regular.ttf?raw=true" > MaterialIcons-Regular.ttf
curl -L "${DIR}/codepoints?raw=true"                > codepoints
```


Generate `material_icons.h`

```
python generate.py > md_icon.h
```
