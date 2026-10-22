## About

The Vulkan loader is `libvulkan.so.1`, the library every Vulkan program links. It reads the driver
(ICD) and layer manifests under `/etc/vulkan` and `/usr/share/vulkan`, loads the drivers they name
(Mesa's for the GPU, and lavapipe on the CPU) and dispatches each call to the right one. It is
built here with every window-system surface a driver can offer, so a Vulkan program can present to
a Wayland window or, under Xwayland, to an X11 one.

## Flags

- `-DCMAKE_INSTALL_SYSCONFDIR=/etc` — The configuration directory compiled into the loader as
  `SYSCONFDIR`. It searches `/etc/vulkan/icd.d`, `explicit_layer.d` and `implicit_layer.d` for
  manifests, after the XDG configuration directories and before the data directories.
- `-DCMAKE_INSTALL_DATADIR=/usr/share` — Read by the loader's build only to find the Vulkan
  registry for code generation, which is off; it changes nothing that is installed. The data
  directories searched at run time, `/usr/local/share` and `/usr/share`, are compiled in from
  upstream's `FALLBACK_DATA_DIRS`, and `/usr/share/vulkan/icd.d` is where [mesa](#port-mesa)
  installs its ICD manifests.
- `-DVULKAN_HEADERS_INSTALL_DIR=/usr`, `-DCMAKE_PREFIX_PATH=/usr` — Where the build looks for
  [vulkan-headers](#port-vulkan-headers): `CMAKE_PREFIX_PATH` for its `VulkanHeadersConfig.cmake`,
  and `VULKAN_HEADERS_INSTALL_DIR` for its registry, which only the code-generation target reads.
  The loader asks for headers of at least its own version and warns when they are newer, and the
  two ports carry the same version.
- `-DBUILD_WSI_WAYLAND_SUPPORT=ON` — The `VK_KHR_wayland_surface` entry points, for Wayland
  windows.
- `-DBUILD_WSI_XCB_SUPPORT=ON`, `-DBUILD_WSI_XLIB_SUPPORT=ON`, `-DBUILD_WSI_XLIB_XRANDR_SUPPORT=ON`
  — The XCB and Xlib surface extensions, and the Xlib display extension built on XRandR. Every
  platform an ICD advertises is compiled into the loader; without these the loader drops the X11
  surface extensions, and a Vulkan program under Xwayland cannot create a surface. They are
  upstream's defaults, named here.
