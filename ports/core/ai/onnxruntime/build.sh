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

patch -p1 -i "$PORT_SRC/gcc16-cstdint.patch"

export ASMFLAGS="-Wa,--noexecstack" LDFLAGS="$LDFLAGS -Wl,-z,noexecstack"

_mirror="$SRC/mirror"
for s in $source; do
	case $s in ort-*::https://*) ;; *) continue ;; esac
	f=${s%%::*}
	u=${s#*::https://}
	for d in "$SRC" "$PORT_SRC" "$SOURCE_DIR"; do
		[ -f "$d/$f" ] && break
	done
	[ -f "$d/$f" ] || { echo "onnxruntime: $f is missing" >&2; exit 1; }
	mkdir -p "$_mirror/${u%/*}"
	ln -sf "$d/$f" "$_mirror/$u"
done

cmake -S cmake -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DPython_EXECUTABLE=/usr/bin/python3 \
	-DFETCHCONTENT_TRY_FIND_PACKAGE_MODE=NEVER \
	-Donnxruntime_CMAKE_DEPS_MIRROR_DIR="$_mirror" \
	-Donnxruntime_BUILD_SHARED_LIB=ON \
	-Donnxruntime_BUILD_UNIT_TESTS=OFF \
	-Donnxruntime_BUILD_BENCHMARKS=OFF \
	-Donnxruntime_ENABLE_PYTHON=ON \
	-Donnxruntime_USE_TELEMETRY=OFF \
	-Donnxruntime_ENABLE_LTO=OFF \
	-Donnxruntime_BUILD_FOR_NATIVE_MACHINE=OFF \
	-Donnxruntime_USE_XNNPACK=OFF \
	-Donnxruntime_USE_MIMALLOC=OFF \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

cd build
python3 ../setup.py bdist_wheel --dist-dir="$SRC_ROOT/ort-dist"
pip3 install --ignore-installed --no-deps --no-index --no-build-isolation \
	--root="$PKG" --prefix=/usr "$SRC_ROOT"/ort-dist/onnxruntime-*.whl
_capi=$(echo "$PKG"/usr/lib/python3*/site-packages/onnxruntime/capi)
[ -f "$_capi/onnxruntime_pybind11_state.so" ] ||
	{ echo "onnxruntime: the Python module was not installed" >&2; exit 1; }
if readelf -lW "$_capi/onnxruntime_pybind11_state.so" | grep GNU_STACK | grep -q RWE; then
	echo "onnxruntime: the Python module asks for an executable stack" >&2
	exit 1
fi
for lib in "$_capi"/libonnxruntime*.so*; do
	[ -f "$lib" ] || continue
	b=${lib##*/}
	[ -e "$PKG/usr/lib/$b" ] && ln -sf "../../../../$b" "$lib"
done
