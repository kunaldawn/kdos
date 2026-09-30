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

# THE RELEASE ARCHIVES CARRY THEIR SUBMODULES EMPTY. The five this build
# compiles are later sources at the commits translateLocally pins, copied
# into the empty directories: bergamot-translator, its marian-dev and
# ssplit-cpp, and marian's intgemm and sentencepiece. The submodules left
# empty are for GPUs (nccl), ARM (ruy, simd_utils), WebAssembly (onnxjs),
# FBGEMM, the server, Python and tests, none of which is built on x86-64.
_b=3rd_party/bergamot-translator
_m=$_b/3rd_party/marian-dev
cp -a "$SRC_ROOT/bergamot-translator-$_bergamot/." "$_b/"
cp -a "$SRC_ROOT/marian-dev-$_marian/." "$_m/"
cp -a "$SRC_ROOT/ssplit-cpp-$_ssplit/." "$_b/3rd_party/ssplit-cpp/"
cp -a "$SRC_ROOT/intgemm-$_intgemm/." "$_m/src/3rd_party/intgemm/"
cp -a "$SRC_ROOT/sentencepiece-$_spm/." "$_m/src/3rd_party/sentencepiece/"

# musl has no execinfo.h: marian's exception call stack is compiled only
# against glibc and prints a one-line note elsewhere. musl's strerror_r is
# the XSI one whatever _GNU_SOURCE says, and zstr takes the GNU branch
# unless the patch sends every C library but glibc to the XSI one. faiss's
# misc.h and marian's quicksand.h use the fixed-width integer types without
# <cstdint>, which GCC 15's headers no longer bring in. marian and intgemm
# build with -Werror, and a warning a newer compiler adds would stop the
# build; the warnings themselves stay on.
patch -p1 -i "$PORT_SRC/marian-musl-no-execinfo.patch"
patch -p1 -i "$PORT_SRC/marian-musl-strerror-r.patch"
patch -p1 -i "$PORT_SRC/marian-cstdint.patch"
patch -p1 -i "$PORT_SRC/no-werror.patch"
# marian sets CMAKE_CXX_FLAGS and CMAKE_C_FLAGS outright, and bergamot hands
# them up to everything it builds: without the patch the exported flags reach
# no object. The patch puts them first; marian's own flags and its Release -O3
# -funroll-loops follow them.
patch -p1 -i "$PORT_SRC/marian-honour-flags.patch"
# marian reads its revision from .git with git log; the archives carry no .git
# and the chroot has no git. The patch writes git_revision.h from
# MARIAN_GIT_REVISION, which names the pinned commit.
patch -p1 -i "$PORT_SRC/marian-no-git.patch"

# BUILD_ARCH defaults to native, which tunes to the build machine and would
# SIGILL elsewhere. marian passes it to -march, and any other value adds
# -msse4.1, marian's floor; x86-64 names no level above that floor. intgemm
# compiles its AVX2 and AVX-512 kernels regardless and picks one at run time.
# The float products go to OpenBLAS (MKL off) through marian's FindCBLAS,
# whose link test is the cblas_openblas_WORKS entry: without it marian
# compiles no BLAS path at all, and nothing else says so. USE_STATIC_LIBS
# would prefer .a files, which the tree does not ship. Models are not
# bundled: the model list is fetched only when the person asks, and a
# downloaded .tar.gz model is imported from the disk through the settings.
cmake -S . -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_ARCH=x86-64 \
	-DMARIAN_GIT_REVISION="${_marian:0:7}" \
	-DCOMPILE_CUDA=OFF \
	-DUSE_MKL=OFF \
	-DUSE_STATIC_LIBS=OFF \
	-DUSE_DOXYGEN=OFF \
	-DUSE_CCACHE=OFF \
	-DCOMPILE_SERVER=OFF \
	-DCOMPILE_PYTHON=OFF \
	-DCOMPILE_TESTS=OFF \
	-DBUILD_EXTERNAL_LIBARCHIVE=OFF \
	-DSSPLIT_USE_INTERNAL_PCRE2=OFF \
	-DSSPLIT_PREFER_STATIC_COMPILE=OFF
grep -q '^cblas_openblas_WORKS:INTERNAL=1$' build/CMakeCache.txt ||
	{ echo "translatelocally: OpenBLAS not found at configure" >&2; exit 1; }
cmake --build build
DESTDIR=$PKG cmake --install build
rm -f "$PKG"/usr/share/icons/translateLocally_logo.png \
	"$PKG"/usr/share/icons/translateLocally_logo.svg \
	"$PKG"/usr/share/applications/translateLocally.desktop

for s in 48 128 256; do
	install -d "$PKG/usr/share/icons/hicolor/${s}x${s}/apps"
	rsvg-convert -w $s -h $s logo/translateLocally_logo.svg \
		-o "$PKG/usr/share/icons/hicolor/${s}x${s}/apps/translateLocally.png"
done

# No desktop file name is set, so the Wayland app_id is the executable's
# name, translateLocally.
cat > "$PKG/usr/share/applications/translateLocally.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=translateLocally
GenericName=Translator
Comment=Translate text between languages on this machine, with no network
Exec=translateLocally
Icon=translateLocally
Terminal=false
StartupWMClass=translateLocally
Categories=Qt;Office;Education;Languages;
Keywords=translate;translation;language;bergamot;marian;offline;
DESKTOP
chmod 644 "$PKG/usr/share/applications/translateLocally.desktop"
