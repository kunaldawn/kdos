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
export RUSTC="rustc -C target-feature=-crt-static"

meson setup build \
	--prefix=/usr --libdir=lib --sysconfdir=/etc \
	--buildtype=release \
	-D b_ndebug=true \
	-D opengl=false \
	-D egl=disabled \
	-D gbm=disabled \
	-D gles1=disabled \
	-D gles2=disabled \
	-D glx=disabled \
	-D glvnd=disabled \
	-D platforms= \
	-D vulkan-drivers= \
	-D vulkan-layers= \
	-D gallium-va=disabled \
	-D gallium-drivers=iris,radeonsi,nouveau,llvmpipe,zink \
	-D gallium-rusticl=true \
	-D gallium-rusticl-enable-drivers=radeonsi \
	-D llvm=enabled \
	-D shared-llvm=enabled \
	-D xmlconfig=enabled \
	-D expat=enabled \
	-D zstd=enabled \
	-D spirv-tools=enabled \
	-D lmsensors=disabled \
	-D libunwind=disabled \
	-D valgrind=disabled

meson compile -C build
DESTDIR="$PWD/stage" meson install --no-rebuild -C build

so=$(readlink "stage/usr/lib/libRusticlOpenCL.so.1")
if readelf -Ws "stage/usr/lib/$so" | awk '$5 == "LOCAL" && ($8 == "__libc" || $8 == "__libc_malloc_impl" || $8 == "__stdio_write") { f = 1 } END { exit !f }'; then
	echo "mesa-opencl: /usr/lib/$so carries a private copy of the C library" >&2
	exit 1
fi
install -Dm755 "stage/usr/lib/$so" "$PKG/usr/lib/$so"
ln -s "$so" "$PKG/usr/lib/libRusticlOpenCL.so.1"
install -Dm644 -t "$PKG/etc/OpenCL/vendors" "stage/etc/OpenCL/vendors/rusticl.icd"
