# Graphics drivers

This page covers the graphics stack of a KDOS machine: which kernel driver drives which [GPU]{.idx}, the
[Mesa]{.idx} drivers above it for OpenGL, [Vulkan]{.idx}, video decoding and OpenCL, the [firmware]{.idx} some
GPUs need before they start, and how to tell what a machine is actually using. It also covers
choosing the second GPU of a laptop that has two. Everything here is the same on the live medium
and on an installed system. The compositor's own use of the GPU, and why it switches its shader off
on software rendering, is in [The phosphor pass](../../kdos/04-programs/kdos-comp.md#the-phosphor-pass);
the video-decoding drivers per GPU are tabled in
[Media, colour and time](../../kdos/02-user-guide/administration.md#media-colour-and-time).

## Installation

| Port | Provides |
|:--|:--|
| [linux](#port-linux) | The kernel's DRM drivers, most of them as modules loaded when udev finds the device |
| [linux-firmware](#port-linux-firmware) | The firmware the AMD, Intel and NVIDIA kernel drivers load at start; see [Microcode and firmware](microcode-and-firmware.md) |
| [libdrm](#port-libdrm) | The user-space side of the kernel's DRM interface |
| [mesa](#port-mesa) | The OpenGL, OpenGL ES, EGL, GBM and Vulkan drivers, the software renderers, a [VA-API]{.idx} driver and OpenCL (rusticl) |
| [libglvnd](#port-libglvnd) | `libGL`, `libEGL` and `libGLX`, which dispatch to Mesa |
| [vulkan-loader](#port-vulkan-loader) | `libvulkan.so.1`, which finds Mesa's Vulkan drivers |
| [libva](#port-libva), [intel-media-driver](#port-intel-media-driver), [intel-gmmlib](#port-intel-gmmlib), [libva-intel-driver](#port-libva-intel-driver) | VA-API, and the two Intel video drivers (`iHD` and `i965`) |
| [ocl-icd](#port-ocl-icd) | The OpenCL loader that finds rusticl |
| [switcheroo-control](#port-switcheroo-control) | Which GPU is discrete, on a machine with two |
| [mesa-demos](#port-mesa-demos), [vulkan-tools](#port-vulkan-tools), [libva-utils](#port-libva-utils), [glmark2](#port-glmark2), [nvtop](#port-nvtop) | `eglinfo`, `vulkaninfo`, `vkcube`, `vainfo`, a GL benchmark (`glmark2-wayland`, `glmark2-es2-wayland` and their `-drm` and `-gbm` builds) and a GPU monitor |

All of these are on the image. No port provides NVIDIA's proprietary driver: an NVIDIA GPU is
driven by `nouveau`, with Mesa's `nouveau` OpenGL driver and NVK for Vulkan.

The only service is `44_switcheroo-control`, which the [`switcheroo-control`]{.idx} recipe writes and
which starts its daemon only when there are two GPUs:

<!-- kfs:include ports/core/hardware/switcheroo-control/build.sh lines=26-55 pin=3a21371ea8f0 -->

## Configuration

### The kernel drivers

The kernel configuration, `ports/core/boot/linux/kdos.config`, builds the drivers for the GPUs of
AMD, Intel and NVIDIA as modules, which udev loads when it finds the device:

<!-- kfs:include ports/core/boot/linux/kdos.config lines=3444-3446 pin=36aa6a93a26c -->

<!-- kfs:include ports/core/boot/linux/kdos.config lines=3458-3464 pin=695dbc122b85 -->

<!-- kfs:include ports/core/boot/linux/kdos.config lines=3470-3472 pin=cdfdf4ee3873 -->

<!-- kfs:include ports/core/boot/linux/kdos.config lines=3496-3500 pin=108c9a793332 -->

| GPU | Kernel driver | Mesa OpenGL | Mesa Vulkan |
|:--|:--|:--|:--|
| Intel GMA 900 to 3150 | `i915` | `i915` | none |
| Intel GMA X3000 to Haswell | `i915` | `crocus` | `intel_hasvk`, from Ivy Bridge |
| Intel Broadwell | `i915` | `iris` | `intel_hasvk` |
| Intel Skylake and newer | `i915` or `xe` | `iris` | `intel` (ANV) |
| AMD, before GCN | `radeon` | `r300`, `r600` | none |
| AMD, GCN and newer | `amdgpu` | `radeonsi` | `amd` (RADV) |
| NVIDIA | `nouveau` | `nouveau` | `nouveau` (NVK) |
| QEMU virtio, VMware | `virtio_gpu`, `vmwgfx` | `virgl`, `svga` | `virtio` (Venus) |
| None of the above | `simpledrm` on the firmware's framebuffer | `llvmpipe`, `softpipe` | `swrast` (lavapipe) |

The first two GCN generations, Southern Islands and Sea Islands, can be driven by either AMD
driver. `CONFIG_DRM_AMDGPU_SI` and `CONFIG_DRM_AMDGPU_CIK` build their support into `amdgpu`, and
the kernel then gives them to `amdgpu` by default; `radeon.si_support=1 amdgpu.si_support=0` (and
the same pair with `cik_support`) on the kernel command line hands them back to `radeon`, which
has no Vulkan driver.

`virtio_gpu`, `simpledrm` and `bochs` are built into the kernel. `02_modules` also loads the
virtual machines' display drivers by name at boot, rather than leaving them to udev's modalias
match, from `fs/etc/modules-load.d/kdos-drm.conf`:

<!-- kfs:include fs/etc/modules-load.d/kdos-drm.conf -->

A built-in driver's line loads nothing, although `02_modules` still logs it as `loaded:` in
`/run/kdos-init.modules.log`, because `modprobe` succeeds; a module on hardware without its device
loads, shows in `lsmod`, and binds nothing.

### Mesa

The build names every Gallium and Vulkan driver it compiles; each flag is explained on its
[package page](#port-mesa):

<!-- kfs:include ports/core/gpu/mesa/build.sh lines=35-65 pin=f0a32cb6518d -->

Per-application driver options (the `xmlconfig` build option) come from Mesa's own files under
`/usr/share/drirc.d/`, then from [`/etc/drirc`]{.idx} and `~/.drirc` when they exist. KDOS ships neither
of the last two.

### Video decoding

libva is built for Wayland and DRM, with no X11:

<!-- kfs:include ports/core/gpu/libva/build.sh lines=12-20 pin=1ef4c6c5ee2f -->

Which driver it loads depends on the kernel driver: for `i915` it tries `iHD` and then `i965`, for
`xe` only `iHD`, and for the Gallium GPUs it loads Mesa's.

### OpenCL

Mesa's rusticl is the one OpenCL driver, and it offers a GPU only when told which drivers to
expose. Every login shell gets this from `fs/etc/profile.d/50-opencl.sh`:

<!-- kfs:include fs/etc/profile.d/50-opencl.sh lines=16-16 pin=6fbcb30bc062 -->

A value you set yourself, empty included, is kept.

## Usage

### Finding which driver a GPU uses

1. List the GPUs and their kernel drivers:

   ```bash
   lspci -k | grep -A3 -E 'VGA|3D|Display'
   ```

   The `Kernel driver in use:` line names it. No such line means no driver took the device. A
   QEMU virtio GPU shows `virtio-pci` there: that is the PCI transport, and the DRM driver,
   `virtio_gpu`, sits on the virtio bus beneath it.
2. List the DRM devices the kernel made: `ls /sys/class/drm`. Each `cardN` is a GPU, and each
   `cardN-<connector>` (`card0-Virtual-1`, `card0-eDP-1`) one of its outputs; `renderD*` nodes are
   their render-only twins. In `/dev/dri`, `cardN` is mode 0660 for the `video` group and
   `renderD*` is open to everyone.
3. Read what the driver said as it started: `dmesg | grep -i -E 'drm|i915|xe|amdgpu|nouveau'`.

### Checking that the desktop is accelerated

1. Read the compositor's log for the phosphor pass:

   ```bash
   grep 'crt:' "$XDG_RUNTIME_DIR/kdos-comp.log"
   ```

   `crt: on` means the compositor runs on the GLES2 renderer. `crt: off — this is not the GLES2
   renderer` means it fell back to software rendering. `crt: off (crt = 0)` says only that the
   pass is turned off in `comp.conf`.
2. Ask EGL which driver answers: `eglinfo -B`.
3. Ask Vulkan: `vulkaninfo --summary`. A list holding only `llvmpipe` means no hardware Vulkan
   driver loaded. `vkcube` draws a spinning cube in a window.
4. Ask VA-API: `vainfo`. It names the driver it loaded and the profiles it decodes and encodes.

### Running a program on the other GPU

On a laptop with an integrated and a discrete GPU, programs run on the one that drives the
screen. To start one on the other:

1. `DRI_PRIME=1 <program>` selects the first other GPU, for OpenGL and Vulkan alike.
   `DRI_PRIME=1 vkcube` shows which one answers.
2. Where `switcheroo-control` is running (`service status switcheroo-control`), `switcherooctl
   list` names each GPU, and `switcherooctl launch <program>` starts a program on the discrete one
   with the environment that GPU needs.
3. For Vulkan alone, `MESA_VK_DEVICE_SELECT=list vulkaninfo` prints the devices and their
   `vendor:device` ids, and `MESA_VK_DEVICE_SELECT=<vendor>:<device> <program>` puts one first.

### Choosing a video driver by hand

`LIBVA_DRIVER_NAME=i965 vainfo` loads the older Intel driver instead of `iHD`, and
`LIBVA_DRIVER_NAME=iHD` the newer one. The variable works for any VA-API program, [`mpv`]{.idx} and
[`ffmpeg`]{.idx} among them.

### Rendering in software

`LIBGL_ALWAYS_SOFTWARE=true <program>` runs an OpenGL program on `llvmpipe`, which tells a driver
fault from a fault in the program.

### Watching the GPU

- `kdos-res` has a GPU page: utilisation where the driver publishes it (`amdgpu`), engine time
  otherwise, with memory, clock, temperature and power where the driver gives them; see
  [GPU](../../kdos/04-programs/kdos-res.md#gpu).
- `nvtop` shows the same per process, in a terminal.
- `GALLIUM_HUD=fps <program>` draws Mesa's own overlay over a GL client on a Gallium driver;
  `GALLIUM_HUD=help` lists what it can show. See
  [The overlay, which is already on every machine](../../kdos/05-developer/testing.md#the-overlay-which-is-already-on-every-machine).

## Tips

- In a virtual machine with plain graphics the compositor runs on software rendering and the
  phosphor pass is off. That is expected; the [rig](../../kdos/05-developer/testing.md) photographs
  are taken that way. The checks above behave differently there: `eglinfo -B` and `vulkaninfo`
  crash with a segmentation fault in the desktop session, `vainfo` loads
  `virtio_gpu_drv_video.so` and stops at `vaInitialize failed with error code 2`, and `nvtop` says
  `No GPU to monitor.`. `LIBGL_ALWAYS_SOFTWARE=true eglinfo -B` and
  `VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json vulkaninfo --summary` answer for
  the software renderers, `llvmpipe` and lavapipe.
- With no VA-API driver for a GPU, `mpv` and `ffmpeg` decode in software without saying so.
  `vainfo` is the check.
- An empty `RUSTICL_ENABLE` hides every OpenCL device; `RUSTICL_ENABLE=llvmpipe` offers the CPU.
- `kdos why /usr/lib/<file>` names the package that owns a driver file; see
  [kdos why](../../kdos/04-programs/kdos-command.md#kdos-why-and-kdos-explain).

## Troubleshooting

**The desktop is slow and `kdos-comp.log` says the pass is off because this is not GLES2.** The
compositor found no hardware renderer.

1. Run the steps in [Finding which driver a GPU uses](#finding-which-driver-a-gpu-uses). A GPU
   with no `Kernel driver in use:` line was not claimed.
2. Look for a firmware request in `dmesg | grep -i firmware`. A driver that cannot load firmware
   it requires does not start, and the screen stays on the firmware's framebuffer.
3. Run `kdos doctor`. With `nouveau` bound and no [`/lib/firmware/nvidia`]{.idx}, it warns that the GPU
   cannot initialise.

**`kdos doctor` warns that nouveau is bound and `/lib/firmware/nvidia` is absent.** `nouveau`
needs NVIDIA's GSP firmware to initialise a Turing or later GPU, and that firmware comes in
`linux-firmware`. Check that the package is installed: `ls /var/lib/kpkg/db/linux-firmware`.

**`vainfo` fails or a video plays on the CPU.**

1. Run `vainfo` and read which driver it tried to open.
2. On Intel, try the other driver: `LIBVA_DRIVER_NAME=i965 vainfo`. Sandy Bridge, Ivy Bridge and
   Haswell are `i965`'s; Broadwell and newer are `iHD`'s.

**`vulkaninfo` lists only `llvmpipe`.** No hardware Vulkan driver loaded for this GPU. Check the
table under [The kernel drivers](#the-kernel-drivers): Intel GPUs before Ivy Bridge, and AMD GPUs
before GCN or handed to `radeon`, have no Mesa Vulkan driver.

**OpenCL finds no device.** Run `echo "$RUSTICL_ENABLE"`. A shell that did not source
`/etc/profile` has it unset, and rusticl then offers nothing.

**`switcherooctl` finds no daemon.** `44_switcheroo-control` skips itself with fewer than two GPUs;
`service status switcheroo-control` says whether it runs (`[DOWN] switcheroo-control` when it does
not). Read its boot output in `/run/kdos-init.switcheroo-control.log`, which on a machine with one
GPU is `[SKIP] switcheroo-control: fewer than two GPUs`.

## Known gaps

- [Broad hardware support is not a goal](../../kdos/06-reference/known-gaps.md#broad-hardware-support-is-not-a-goal)
- [`btop`'s GPU panel is empty on Intel graphics unless btop runs as root](../../kdos/06-reference/known-gaps.md#btops-gpu-panel-is-empty-on-intel-graphics-unless-btop-runs-as-root)
- [Intel Quick Sync through oneVPL has no runtime](../../kdos/06-reference/known-gaps.md#intel-quick-sync-through-onevpl-has-no-runtime)
- [The phosphor shader is not in any rig photograph](../../kdos/06-reference/known-gaps.md#the-phosphor-shader-is-not-in-any-rig-photograph)
- [`kdos doctor` cannot check hardware a machine does not have](../../kdos/06-reference/known-gaps.md#kdos-doctor-cannot-check-hardware-a-machine-does-not-have)
- [Features whose library has no port](../../kdos/06-reference/known-gaps.md#features-whose-library-has-no-port):
  VDPAU, CUDA and NVENC among them

## See also

- [The phosphor pass](../../kdos/04-programs/kdos-comp.md#the-phosphor-pass) and
  [Debugging](../../kdos/04-programs/kdos-comp.md#debugging): the compositor's renderer and its log
- [Media, colour and time](../../kdos/02-user-guide/administration.md#media-colour-and-time):
  hardware video decoding and OpenCL
- [Firmware](../../kdos/02-user-guide/administration.md#firmware)
- [GPU](../../kdos/04-programs/kdos-res.md#gpu): what the resource monitor reads
- [kdos doctor](../../kdos/04-programs/kdos-command.md#kdos-doctor)
- `microcode-and-firmware.md`: [Microcode and firmware](microcode-and-firmware.md)
- `displays-and-scaling.md`: [Displays and scaling](displays-and-scaling.md)
- `modules-and-sysctl.md`: [Modules and sysctl](modules-and-sysctl.md)

<!-- book-nav -->
---

*Part XVI. System administration, chapter 400.* Previous: [399. Displays and scaling](displays-and-scaling.md) · [Contents](../README.md) · Next: [401. Power management](power-management.md)
