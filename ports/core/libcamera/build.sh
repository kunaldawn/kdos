#!/bin/bash
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# --wrap-mode=nodownload IS THE ONLY THING STOPPING THIS BUILD FROM REACHING THE
# NETWORK. subprojects/ carries four wraps — libyaml, gtest, libyuv and libpisp —
# and meson silently clones or downloads any of them the moment the matching
# system dependency is missing. `yaml-0.1` is a hard dependency with a wrap
# behind it, so a build root without the `yaml` port would fetch libyaml from
# pyyaml.org instead of failing. With nodownload a missing dependency is an
# error, which is the answer a recipe wants.

# -Dpipelines IS NAMED RATHER THAN LEFT AT `auto`, AND `virtual` IS THE REASON.
# `auto` enables every handler whose architecture list contains 'any', which
# includes `virtual` — and virtual needs libyuv, which no distribution ships and
# which meson resolves through a wrap-git clone of chromium.googlesource.com.
# The three named here are what this image can actually drive: `ipu3` for Intel
# ISP hardware, `uvcvideo` for every USB webcam, and `simple` for the
# MIPI/IPU6 sensors the kernel config enables — IPU6 has no handler of its own
# and is driven as a simple pipeline plus the software ISP.
#
# -Dipas MUST CONTAIN A PIPELINE'S NAME OR THAT PIPELINE GETS NO TUNING. The IPA
# module name matches the pipeline name; a pipeline whose IPA is absent runs
# with no auto-exposure and no auto-white-balance. uvcvideo has no IPA because
# the camera does that work itself.

# THE SOFTWARE ISP IS BUILT (it comes with `simple`) BUT ITS GPU PATH IS NOT.
# -Dsoftisp-gpu=enabled would link EGL and GLES into libcamera itself, which
# puts mesa between the camera stack and every consumer of it; the CPU debayer
# has no such edge. The cost is measured in frames per second on a raw Bayer
# sensor, and nothing else on this image reads one yet.

# -Dv4l2=disabled: the V4L2 adaptation layer is an LD_PRELOAD shim that resolves
# its passthrough with dlsym(RTLD_NEXT, "open64") and dlsym(RTLD_NEXT, "mmap64")
# and defines glibc's fortify entry point __open64_2. musl has no such symbols,
# so the lookups return NULL and are called anyway — every process started under
# `libcamerify` would fault on its first open().

# -Dgstreamer=enabled builds `libcamerasrc`, which is the only way a pipeline
# reads a libcamera camera. It costs nothing on this image: glib, gstreamer and
# gst-plugins-base are already installed, because pipewire pulls all three.

# THIS PACKAGE IS NOT BYTE-REPRODUCIBLE, AND NO FLAG MAKES IT ONE. src/meson.build
# generates a fresh ipa-priv-key.pem per build and bakes the matching public key
# into libcamera.so, so the library and the two .sign files differ between two
# builds of the same recipe — `kpkg verify --repro libcamera` reports a payload
# difference and that result is expected, not a defect to chase. The only way to
# remove the key is to build with no `openssl` program in the root, and then
# every IPA module runs isolated in its own process instead of in-library.

# -Dwerror=false: the project defaults to werror=true with warning_level=2, so a
# compiler newer than the one upstream tested turns any new diagnostic into a
# failed build.
meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib --libexecdir=/usr/lib \
	--wrap-mode=nodownload \
	-Dbuildtype=release \
	-Dwerror=false \
	-Dpipelines=ipu3,simple,uvcvideo \
	-Dipas=ipu3,simple \
	-Dudev=enabled \
	-Dgstreamer=enabled \
	-Dcam=enabled \
	-Dcam-output-kms=disabled \
	-Dcam-output-sdl2=disabled \
	-Dcam-jpeg=disabled \
	-Dapps-output-dng=disabled \
	-Dlc-compliance=disabled \
	-Dqcam=disabled \
	-Dpycamera=disabled \
	-Dv4l2=disabled \
	-Dandroid=disabled \
	-Ddocumentation=disabled \
	-Dtracing=disabled \
	-Dsoftisp-gpu=disabled \
	-Drpi-awb-nn=disabled \
	-Dlibdw=disabled \
	-Dlibunwind=disabled \
	-Dtest=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
