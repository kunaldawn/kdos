## About
Qt Shader Tools provides `qsb`, the Qt Shader Baker, and the QtShaderTools library behind it. It
compiles a Vulkan-style GLSL shader to SPIR-V and translates it to the GLSL, HLSL and Metal
variants Qt's rendering hardware interface may need, packing them into one `.qsb` file. Qt Quick,
Qt Multimedia and Qt 5 Compat compile their shaders with it at build time, so it is a build
dependency of those modules.

## Notes
Like every Qt 6 module but `qt6-qtbase`, it passes no install paths of its own: it reads them, and
its CMake setup, from [qt6-qtbase](#port-qt6-qtbase) (see [How Qt 6 is built](../../../../docs/kdos/05-developer/writing-ports.md#how-qt-6-is-built)).
