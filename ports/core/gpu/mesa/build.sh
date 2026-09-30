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

# Vendor rust crates
ln -sf "$SRC_ROOT/syn-2.0.87" subprojects/syn-2.0.87
cp -r subprojects/packagefiles/syn-2-rs/* subprojects/syn-2.0.87/

ln -sf "$SRC_ROOT/unicode-ident-1.0.12" subprojects/unicode-ident-1.0.12
cp -r subprojects/packagefiles/unicode-ident-1-rs/* subprojects/unicode-ident-1.0.12/

ln -sf "$SRC_ROOT/quote-1.0.35" subprojects/quote-1.0.35
cp -r subprojects/packagefiles/quote-1-rs/* subprojects/quote-1.0.35/

ln -sf "$SRC_ROOT/proc-macro2-1.0.86" subprojects/proc-macro2-1.0.86
cp -r subprojects/packagefiles/proc-macro2-1-rs/* subprojects/proc-macro2-1.0.86/

ln -sf "$SRC_ROOT/paste-1.0.14" subprojects/paste-1.0.14
cp -r subprojects/packagefiles/paste-1-rs/* subprojects/paste-1.0.14/

ln -sf "$SRC_ROOT/rustc-hash-2.1.1" subprojects/rustc-hash-2.1.1
cp -r subprojects/packagefiles/rustc-hash-2-rs/* subprojects/rustc-hash-2.1.1/

export LIBCLANG_PATH=/usr/lib/
export LIBCLANG_STATIC_PATH=/usr/lib/
export RUST_BACKTRACE=1
export RUSTFLAGS="-C prefer-dynamic"

# Every probed dependency that has a switch is pinned: an option left on auto
# turns a feature on or off by whatever happens to be installed when mesa
# builds. libudev and xcb-keysyms have no switch and are linked whenever found,
# so eudev and xcb-util-keysyms are in depends.
# The x11 platform and glx=dri are what an X11 client under Xwayland draws
# with: libGLX_mesa.so behind libglvnd's libGL and libGLX, and the Vulkan X11
# WSI. glx=dri needs libXxf86vm for direct rendering, and xlib-lease (Vulkan's
# VK_EXT_acquire_xlib_display) needs libXrandr; it is pinned because on auto it
# turns on with the x11 platform. A Wayland client never loads libGLX_mesa,
# but libEGL_mesa and the Vulkan drivers link libxcb and libX11 for their X11
# platform, so the X client libraries are on the host either way.
# nouveau in vulkan-drivers is NVK, and it is what the six vendored crates
# above are for. gallium-rusticl installs /etc/OpenCL/vendors/rusticl.icd,
# which only ocl-icd's loader reads. libunwind is off: it serves only mesa's
# debug stack dumps, and would be linked into every process that loads a
# driver.
meson setup build \
	--prefix=/usr --libdir=lib --sysconfdir=/etc \
	--buildtype=release \
	-D b_ndebug=true \
	-D opengl=true \
	-D gallium-extra-hud=true \
	-D xmlconfig=enabled \
	-D expat=enabled \
	-D egl=enabled \
	-D llvm=enabled \
	-D shared-llvm=enabled \
	-D gbm=enabled \
	-D gles1=disabled \
	-D gles2=enabled \
	-D glx=dri \
	-D gallium-drivers=zink,crocus,iris,nouveau,r300,r600,radeonsi,svga,llvmpipe,softpipe,virgl,i915 \
	-D gallium-rusticl=true \
	-D platforms=wayland,x11 \
	-D xlib-lease=enabled \
	-D vulkan-drivers=amd,intel,intel_hasvk,nouveau,swrast,virtio \
	-D vulkan-layers=device-select,intel-nullhw,overlay,anti-lag \
	-D video-codecs=all \
	-D gallium-va=enabled \
	-D display-info=enabled \
	-D lmsensors=enabled \
	-D zstd=enabled \
	-D spirv-tools=enabled \
	-D intel-rt=enabled \
	-D libunwind=disabled \
	-D valgrind=disabled \
	-D glvnd=true

meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
