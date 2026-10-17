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

_b=3rd_party/bergamot-translator
_m=$_b/3rd_party/marian-dev
cp -a "$SRC_ROOT/bergamot-translator-$_bergamot/." "$_b/"
cp -a "$SRC_ROOT/marian-dev-$_marian/." "$_m/"
cp -a "$SRC_ROOT/ssplit-cpp-$_ssplit/." "$_b/3rd_party/ssplit-cpp/"
cp -a "$SRC_ROOT/intgemm-$_intgemm/." "$_m/src/3rd_party/intgemm/"
cp -a "$SRC_ROOT/sentencepiece-$_spm/." "$_m/src/3rd_party/sentencepiece/"

patch -p1 -i "$PORT_SRC/marian-musl-no-execinfo.patch"
patch -p1 -i "$PORT_SRC/marian-musl-strerror-r.patch"
patch -p1 -i "$PORT_SRC/marian-cstdint.patch"
patch -p1 -i "$PORT_SRC/no-werror.patch"
patch -p1 -i "$PORT_SRC/marian-honour-flags.patch"
patch -p1 -i "$PORT_SRC/marian-no-git.patch"

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
