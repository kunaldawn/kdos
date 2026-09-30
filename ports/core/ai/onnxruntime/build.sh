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

# EVERY THIRD-PARTY LIBRARY IS THE ONE THIS RELEASE PINS, built into
# libonnxruntime. cmake/deps.txt names each by URL and SHA1, and FetchContent
# reads onnxruntime_CMAKE_DEPS_MIRROR_DIR first: a file at <mirror>/<URL
# without https://> is used in place of the download, still checked against
# the SHA1 and still given the project's own patches from cmake/patches. The
# later sources are those files, linked into that layout here.
# FETCHCONTENT_TRY_FIND_PACKAGE_MODE=NEVER keeps the tree's abseil and
# protobuf out: abseil's CMake package accepts only its exact version, so the
# pinned one is built regardless, and a system protobuf linked against another
# abseil would put two abseils in one process.
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

# The CPU execution provider only: every accelerator backend needs a vendor
# runtime this tree does not carry. MLAS picks its AVX2 and AVX-512 kernels
# at run time, so one package serves every x86-64. TELEMETRY is off (it is
# already off on Linux; the flag makes that the recipe's decision). The Python
# module is what piper imports; it is built against numpy from its port.
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

# setup.py runs from the build directory, where the module and the provider
# bridge were built. The wheel carries copies of the two shared libraries
# already installed under /usr/lib; they become links, so one file is loaded
# whichever way a program reaches it.
cd build
python3 ../setup.py bdist_wheel --dist-dir="$SRC_ROOT/ort-dist"
pip3 install --no-deps --no-index --no-build-isolation \
	--root="$PKG" --prefix=/usr "$SRC_ROOT"/ort-dist/onnxruntime-*.whl
_capi=$(echo "$PKG"/usr/lib/python3*/site-packages/onnxruntime/capi)
[ -f "$_capi/onnxruntime_pybind11_state.so" ] ||
	{ echo "onnxruntime: the Python module was not installed" >&2; exit 1; }
for lib in "$_capi"/libonnxruntime*.so*; do
	[ -f "$lib" ] || continue
	b=${lib##*/}
	[ -e "$PKG/usr/lib/$b" ] && ln -sf "../../../../$b" "$lib"
done
