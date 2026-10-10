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

# Alpine's musl patches: the bundled glog claims <execinfo.h>, which musl does
# not have; one header uses uint64_t without <cstdint>; and readfile.hh
# poisons off_t, which musl's own headers name after it.
patch -p1 -i "$PORT_SRC/0001-musl-fixes.patch"
patch -p1 -i "$PORT_SRC/0002-fix-includes.patch"
patch -p1 -i "$PORT_SRC/0004-dont-poison-off_t.patch"

# The bundled audaspace's FFmpeg writer reads AVCodec's sample_fmts and
# supported_samplerates, which FFmpeg 8 removed; the patch asks
# avcodec_get_supported_config for both, as Blender's own movie writer does.
# A null list means the encoder takes any format or rate.
patch -p1 -i "$PORT_SRC/ffmpeg9-audaspace.patch"

_py=$(python3 -c 'import sys; print("%d.%d" % sys.version_info[:2])')

# WITH_STRICT_BUILD_OPTIONS=ON: an enabled feature whose library is missing
# stops the configure instead of being switched off, so every ON below is a
# feature the package has. The ports that do not exist are named OFF:
# OpenXR (VR), USD, MaterialX and Hydra, OpenImageDenoise, Embree, OSL and
# OpenPGL (Cycles), Draco and meshoptimizer (glTF compression), libspnav
# (3D mice). The Cycles GPU devices need vendor compilers and are off; Cycles
# renders on the CPU, EEVEE and the viewport on OpenGL or Vulkan.
#
# GHOST is built for Wayland and X11 and tries Wayland first. The Wayland
# and audio libraries are linked, not opened at run time, so the ELF names
# what the package needs. glog and gflags are Blender's bundled copies: libmv
# includes glog's headers with none of the definitions glog 0.7's CMake target
# supplies, and the bundled pair matches what Blender tests with.
#
# Blender runs on the system Python and bundles none of its modules: NumPy
# and zstandard are the ports'. zstandard is how Python scripts such as
# blend_render_info read a zstd-compressed .blend file; Blender's own loader
# does not need it.
#
# WITH_INTERNATIONAL=OFF: English only, and no translation catalogues.
# WITH_BUILDINFO=OFF keeps the build date and git hash out of the binary.
# WITH_CPU_CHECK=OFF: the start-up CPU check is a library that only a portable
# install ships, and the strict options stop the configure when it is left on.
# JACK is not on this system; audio goes through PipeWire, PulseAudio and
# OpenAL.
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
	-DWITH_INPUT_NDOF=OFF \
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
	-DWITH_OPENIMAGEDENOISE=OFF \
	-DWITH_DRACO=OFF \
	-DWITH_MESHOPTIMIZER=OFF \
	-DWITH_CYCLES=ON \
	-DWITH_CYCLES_EMBREE=OFF \
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

# The panel reads PNG only: the scalable icon is rasterised, and upstream's
# entry is replaced to carry StartupWMClass=blender (GHOST's Wayland app_id)
# and --offline-mode, which pins Blender's "Allow Online Access" preference
# off so the extensions platform never offers what a machine without a
# network cannot reach.
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
