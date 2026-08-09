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
pkg-config --exists vulkan-icd-loader && OPT_MESA_GALLIUM='zink,'
pkg-config --exists libva && OPT_MESA_VAAPI='-D gallium-va=enabled' || OPT_MESA_VAAPI='-D gallium-va=disabled'
export RUSTFLAGS="-C prefer-dynamic"

meson setup build \
	--prefix=/usr --libdir=lib --sysconfdir=/etc \
	--buildtype=release \
	-D b_ndebug=true \
	-D opengl=true \
	-D gallium-extra-hud=true \
	-D xmlconfig=disabled \
	-D egl=enabled \
	-D llvm=enabled \
	-D shared-llvm=enabled \
	-D gbm=enabled \
	-D gles1=disabled \
	-D gles2=enabled \
	-D glx=disabled \
	-D gallium-drivers=${OPT_MESA_GALLIUM}crocus,iris,nouveau,r300,r600,radeonsi,svga,llvmpipe,softpipe,virgl,i915 \
	-D platforms=wayland \
	-D shared-glapi=enabled \
	-D vulkan-drivers=amd,intel,intel_hasvk,swrast,virtio \
	-D vulkan-layers=device-select,intel-nullhw,overlay \
	-D video-codecs=vc1dec,h264dec,h264enc,h265dec,h265enc \
	$OPT_MESA_VAAPI -D glvnd=true

meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
