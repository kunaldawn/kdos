## About

libdrm is the user-space side of the kernel's Direct Rendering Manager: `libdrm` wraps the DRM and
KMS ioctls (devices, connectors, modes, planes, buffers), and one library per GPU family wraps that
driver's own command-submission and memory ioctls. Mesa's drivers, the compositor's DRM backend
through wlroots, VA-API, Xwayland and every program that opens `/dev/dri` sit on it. This build
keeps the driver libraries for PC hardware and none of the ARM SoC ones.

## Flags

- `-D man-pages=enabled` — Builds the `drm(7)`, `drm-kms(7)` and `drm-memory(7)` pages and the
  section 3 pages for the core calls with `rst2man`, from
  [python3-docutils](#port-python3-docutils); `enabled` makes a missing `rst2man` a configure error
  instead of a build with no pages.
- `-D valgrind=disabled` — Builds no Valgrind client annotations into `libdrm`.
  [valgrind](#port-valgrind) is installed in an earlier phase and is not in this port's `depends`,
  so on `auto` the result would follow what happened to be in the root.
- `-D cairo-tests=disabled` — Builds `modetest` without its cairo-drawn test patterns. On `auto`
  the result would follow whether [cairo](#port-cairo), built in the same phase, came first.
- `-D intel=enabled`, `-D amdgpu=enabled`, `-D radeon=enabled`, `-D nouveau=enabled`,
  `-D vmwgfx=enabled` — `libdrm_intel`, `libdrm_amdgpu`, `libdrm_radeon` and `libdrm_nouveau`, the
  driver libraries Mesa's PC drivers link, and the VMware SVGA header `vmwgfx_drm.h`. `enabled`
  turns each from a probe into a requirement: `libdrm_intel` needs
  [libpciaccess](#port-libpciaccess), which is why it is in `depends`. The ARM SoC families (omap,
  exynos, freedreno, tegra, vc4, etnaviv) are left at their defaults, which build none of them on
  x86.
- `-D install-test-programs=true` — Installs the test programs upstream builds, among them
  `modetest`, which lists every connector, encoder, CRTC, plane and mode the kernel exposes and can
  set a mode by hand, `proptest`, `modeprint`, `vbltest` and `drmdevice`. Upstream's default is to
  build them and install none.
