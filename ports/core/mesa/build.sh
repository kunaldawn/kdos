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
# builds. libudev has no switch and is linked whenever found, so eudev is in
# depends.
# nouveau in vulkan-drivers is NVK, and it is what the six vendored crates
# above are for. gallium-rusticl installs /etc/OpenCL/vendors/rusticl.icd,
# which only ocl-icd's loader reads. libunwind is off because the libunwind
# port is LLVM's and ships no libunwind.pc for this probe to find.
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
	-D glx=disabled \
	-D gallium-drivers=zink,crocus,iris,nouveau,r300,r600,radeonsi,svga,llvmpipe,softpipe,virgl,i915 \
	-D gallium-rusticl=true \
	-D platforms=wayland \
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
