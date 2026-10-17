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

patch -p1 -i "$PORT_SRC/0001-musl-fixes.patch"
patch -p1 -i "$PORT_SRC/0002-fix-includes.patch"
patch -p1 -i "$PORT_SRC/0004-dont-poison-off_t.patch"

patch -p1 -i "$PORT_SRC/ffmpeg9-audaspace.patch"

_py=$(python3 -c 'import sys; print("%d.%d" % sys.version_info[:2])')

mkdir build && cd build
cmake .. -G Ninja -Wno-dev \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DWITH_STRICT_BUILD_OPTIONS=ON \
	-DWITH_LIBS_PRECOMPILED=OFF \
	-DWITH_STATIC_LIBS=OFF \
	-DWITH_INSTALL_PORTABLE=OFF \
	-DWITH_CPU_CHECK=OFF \
	-DWITH_PYTHON_INSTALL=OFF \
	-DWITH_PYTHON_INSTALL_NUMPY=OFF \
	-DWITH_PYTHON_INSTALL_REQUESTS=OFF \
	-DWITH_PYTHON_INSTALL_ZSTANDARD=OFF \
	-DWITH_PYTHON_MODULE=OFF \
	-DWITH_PYTHON_NUMPY=ON \
	-DWITH_PYTHON_SECURITY=ON \
	-DPYTHON_VERSION="$_py" \
	-DWITH_BUILDINFO=OFF \
	-DWITH_INTERNATIONAL=OFF \
	-DWITH_DOC_MANPAGE=ON \
	-DWITH_BLENDER_THUMBNAILER=ON \
	-DWITH_GTESTS=OFF \
	-DWITH_LINKER_LLD=OFF \
	-DWITH_LINKER_MOLD=OFF \
	-DWITH_COMPILER_CCACHE=OFF \
	-DWITH_HEADLESS=OFF \
	-DWITH_GHOST_WAYLAND=ON \
	-DWITH_GHOST_WAYLAND_DYNLOAD=OFF \
	-DWITH_GHOST_CSD=ON \
	-DWITH_GHOST_X11=ON \
	-DWITH_GHOST_XDND=ON \
	-DWITH_X11_XINPUT=ON \
	-DWITH_X11_XFIXES=ON \
	-DWITH_GHOST_SDL=OFF \
	-DWITH_INPUT_IME=ON \
	-DWITH_INPUT_NDOF=ON \
	-DWITH_OPENGL_BACKEND=ON \
	-DWITH_VULKAN_BACKEND=ON \
	-DWITH_SYSTEM_FREETYPE=ON \
	-DWITH_HARFBUZZ=OFF \
	-DWITH_FRIBIDI=OFF \
	-DWITH_SYSTEM_GLOG=OFF \
	-DWITH_SYSTEM_GFLAGS=OFF \
	-DWITH_SYSTEM_BULLET=OFF \
	-DWITH_SYSTEM_AUDASPACE=OFF \
	-DWITH_AUDASPACE=ON \
	-DWITH_RUBBERBAND=ON \
	-DWITH_OPENAL=ON \
	-DWITH_PULSEAUDIO=ON \
	-DWITH_PULSEAUDIO_DYNLOAD=OFF \
	-DWITH_PIPEWIRE=ON \
	-DWITH_PIPEWIRE_DYNLOAD=OFF \
	-DWITH_JACK=OFF \
	-DWITH_SDL_AUDIO=OFF \
	-DWITH_CODEC_FFMPEG=ON \
	-DWITH_CODEC_SNDFILE=ON \
	-DWITH_FFTW3=ON \
	-DWITH_IMAGE_OPENJPEG=ON \
	-DWITH_IMAGE_WEBP=ON \
	-DWITH_IMAGE_CINEON=ON \
	-DWITH_OPENSUBDIV=ON \
	-DWITH_OPENVDB=ON \
	-DWITH_OPENVDB_BLOSC=ON \
	-DWITH_NANOVDB=ON \
	-DWITH_ALEMBIC=ON \
	-DWITH_HARU=ON \
	-DWITH_POTRACE=ON \
	-DWITH_PUGIXML=ON \
	-DWITH_GMP=ON \
	-DWITH_MANIFOLD=ON \
	-DWITH_TBB=ON \
	-DWITH_TBB_MALLOC_PROXY=OFF \
	-DWITH_LIBMV=ON \
	-DWITH_USD=OFF \
	-DWITH_MATERIALX=OFF \
	-DWITH_HYDRA=OFF \
	-DWITH_XR_OPENXR=OFF \
	-DWITH_OPENIMAGEDENOISE=ON \
	-DWITH_DRACO=ON \
	-DWITH_MESHOPTIMIZER=ON \
	-DWITH_CYCLES=ON \
	-DWITH_CYCLES_EMBREE=ON \
	-DWITH_CYCLES_OSL=OFF \
	-DWITH_CYCLES_PATH_GUIDING=OFF \
	-DWITH_CYCLES_DEVICE_CUDA=OFF \
	-DWITH_CYCLES_DEVICE_OPTIX=OFF \
	-DWITH_CYCLES_DEVICE_HIP=OFF \
	-DWITH_CYCLES_DEVICE_HIPRT=OFF \
	-DWITH_CYCLES_DEVICE_ONEAPI=OFF \
	-DWITH_CYCLES_CUDA_BINARIES=OFF \
	-DWITH_CYCLES_HIP_BINARIES=OFF \
	-DWITH_LLVM=OFF \
	-DWITH_CLANG=OFF \
	-DWITH_TRACY=OFF \
	-DWITH_RENDERDOC=OFF
ninja
DESTDIR=$PKG ninja install

rm -rf "$PKG/usr/share/icons/hicolor/scalable"
install -d "$PKG/usr/share/icons/hicolor/256x256/apps"
rsvg-convert -w 256 -h 256 ../release/freedesktop/icons/scalable/apps/blender.svg \
	-o "$PKG/usr/share/icons/hicolor/256x256/apps/blender.png"
rm -f "$PKG/usr/share/applications/blender.desktop"
cat > "$PKG/usr/share/applications/blender.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Blender
GenericName=3D Modeller
Comment=3D modelling, animation, rendering, compositing and video editing
Exec=blender --offline-mode %f
Icon=blender
Terminal=false
StartupWMClass=blender
PrefersNonDefaultGPU=true
Categories=Graphics;3DGraphics;
MimeType=application/x-blender;
Keywords=3d;cg;modeling;modelling;animation;sculpting;texturing;rendering;cycles;eevee;video editing;motion tracking;
DESKTOP
chmod 644 "$PKG/usr/share/applications/blender.desktop"
