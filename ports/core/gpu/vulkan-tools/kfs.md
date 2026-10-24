## About

Vulkan-Tools is Khronos's pair of Vulkan diagnostics: `vulkaninfo`, which lists every device the
[vulkan-loader](#port-vulkan-loader) finds with its properties, extensions, formats and surfaces,
and `vkcube` (with its C++ twin `vkcubepp`), a spinning textured cube drawn through a real
swapchain. `vulkaninfo --summary` is what says whether a machine has a Vulkan driver at all, and
which, since a Vulkan program falls back to lavapipe on the CPU without saying so. How the tools
are used together with glmark2 and mesa-demos is in [Which tool answers which
question](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/testing.md#which-tool-answers-which-question).

## Flags

- `-DVULKAN_HEADERS_INSTALL_DIR=/usr`, `-DCMAKE_PREFIX_PATH=/usr` — Where the build looks for
  [vulkan-headers](#port-vulkan-headers) and the loader: `CMAKE_PREFIX_PATH` for their CMake
  package files, and `VULKAN_HEADERS_INSTALL_DIR` for the registry, which only the code-generation
  target reads. The build requires headers of at least its own version; this port, the headers and
  the loader carry the same version.
- `-DBUILD_CUBE=ON`, `-DBUILD_VULKANINFO=ON` — The two tools, upstream's defaults.
- `-DBUILD_ICD=OFF` — Builds no mock ICD, the fake Vulkan driver upstream builds by default for its
  own tests. Upstream installs it only when `INSTALL_ICD` is also set, so leaving it out changes no
  installed file and saves its compile.
- `-DBUILD_WSI_WAYLAND_SUPPORT=ON` — Presents to a Wayland window, the platform `vkcube` picks on
  its own here.
- `-DBUILD_WSI_DISPLAY_SUPPORT=ON` — Presents straight to a display through `VK_KHR_display`, from
  a text console while no compositor holds the screen (`vkcube --wsi display`).
- `-DBUILD_WSI_XCB_SUPPORT=OFF`, `-DBUILD_WSI_XLIB_SUPPORT=OFF`, `-DBUILD_WSI_DIRECTFB_SUPPORT=OFF`
  — No X11 or DirectFB presentation. The graphics tools on the host are the Wayland ones; what an
  X11 client sees through Xwayland is tested from inside a box.

## Notes

`build.sh` writes a desktop entry for `vkcube`, so the driver check is one click from the launcher.
`vulkaninfo` has none: it is a terminal program.
