## About

Vulkan-Headers is Khronos's official set of Vulkan API headers: `vulkan/vulkan.h` and the rest for
C, the Vulkan-Hpp headers for C++, the `vk_video` codec headers, and the machine-readable API
registry (`vk.xml`, `video.xml` and the Python generator scripts that read them), installed under
`/usr/share/vulkan/registry`. Nothing in it is compiled: it is an interface package with a CMake
config file, `VulkanHeadersConfig.cmake`, and many ports build against it, among them
[vulkan-loader](#port-vulkan-loader), [wlroots](#port-wlroots), [gtk4](#port-gtk4),
[ffmpeg](#port-ffmpeg), [mpv](#port-mpv) and [blender](#port-blender).

## Flags

- `-DVULKAN_HEADERS_ENABLE_TESTS=OFF` — Build none of the project's own tests, which compile small C
  and C++ programs against the headers. The option defaults to on when Vulkan-Headers is the top-level CMake
  project, as it is here.
- `-DVULKAN_HEADERS_ENABLE_MODULE=OFF` — Build and install no Vulkan-Hpp C++ named modules
  (`vulkan.cppm`, `vulkan_video.cppm`), the `Vulkan::HppModule` object target a consumer imports
  instead of including the C++ headers. Upstream turns the option on only when the compiler can
  import the standard library as a module. Off, the headers install the same, and the `.cppm`
  files are copied into `/usr/include/vulkan` as plain files.
