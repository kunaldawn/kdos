# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# THE SOURCE IS LIBREOFFICE'S TARBALL OF THE chromium/$version BRANCH, because
# gitiles serves no byte-stable archive and the GitHub mirror stopped at 7533.
# It carries pdfium, fast_float and a partial abseil; the rest of what gn
# needs is the vendor bundle: Chromium's //build at DEPS' build_revision,
# third_party/abseil-cpp (with its BUILD.gn files) at abseil_revision, and
# tools/generate_shim_headers/generate_shim_headers.py from the $_chromium
# tag. The bundle and the branch must name the same revisions — a //build from
# another branch fails in gn with errors that name neither — so DEPS is
# checked against the pins before anything is built.
for pin in "build_revision=$_build_rev" "abseil_revision=$_abseil_rev"; do
	if ! grep -q "'${pin%%=*}': '${pin#*=}'" DEPS; then
		echo "pdfium: DEPS does not name ${pin%%=*} ${pin#*=} — rebuild the vendor bundle" >&2
		exit 1
	fi
done
rm -rf third_party/abseil-cpp
tar -xf "$PORT_SRC/$name-vendor-$version.tar.xz" --strip-components=1

# Upstream builds pdfium as a `component`, which is a static library outside a
# component build, and exports FPDF_* only under COMPONENT_BUILD. The patch
# makes it one shared object with abseil linked in and the FPDF_* API
# visible, and makes public/cpp include its siblings relatively so the
# installed headers resolve under /usr/include/pdfium.
patch -p1 -i "$PORT_SRC/pdfium-shared-library.patch"

# gclient writes this file on a checkout; //build imports it.
printf 'build_with_chromium = false\n' > build/config/gclient_args.gni
# ICU comes from the system through Chromium's own unbundling shim.
install -Dm644 build/linux/unbundle/icu.gn third_party/icu/BUILD.gn

# The unbundle toolchain takes CC, CXX, AR, NM and the *FLAGS from the
# environment. -ffp-contract=off: gcc fuses multiply-adds that upstream's clang
# build does not, which moves rendered pixels (crbug.com/402282789).
export CC=gcc CXX=g++ AR=ar NM=nm
export CPPFLAGS="$CPPFLAGS -ffp-contract=off"

# JavaScript (V8) and XFA forms stay off: V8 is a second browser engine to
# build, and XFA needs it. Every codec is the system's; libtiff is reached only
# through XFA. --root-pattern keeps gn to the library's own graph, so the
# test targets, whose fonts are not in the tarball, are never loaded.
gn gen out/Release --root-pattern=//:pdfium --args='
  is_debug = false
  is_clang = false
  is_component_build = false
  custom_toolchain = "//build/toolchain/linux/unbundle:default"
  host_toolchain = "//build/toolchain/linux/unbundle:default"
  treat_warnings_as_errors = false
  clang_use_chrome_plugins = false
  use_custom_libcxx = false
  use_sysroot = false
  use_glib = false
  use_siso = false
  pdf_is_standalone = true
  pdf_enable_v8 = false
  pdf_enable_xfa = false
  pdf_use_skia = false
  pdf_use_partition_alloc = false
  pdf_bundle_freetype = false
  use_system_freetype = true
  use_system_harfbuzz = true
  use_system_lcms2 = true
  use_system_libjpeg = true
  use_system_libopenjpeg2 = true
  use_system_libpng = true
  use_system_libtiff = true
  use_system_zlib = true
'
ninja -C out/Release pdfium

install -Dm755 out/Release/libpdfium.so "$PKG/usr/lib/libpdfium.so"
install -d "$PKG/usr/include/pdfium/cpp"
install -m644 public/*.h "$PKG/usr/include/pdfium/"
install -m644 public/cpp/*.h "$PKG/usr/include/pdfium/cpp/"

# python3-pypdfium2's build takes the Chromium version from here and records
# it in its bindings. Without a version it records NaN, every `build >= N`
# gate in its helpers is false, and the constants and error paths those gates
# guard silently go missing.
install -Dm644 /dev/stdin "$PKG/usr/lib/pkgconfig/libpdfium.pc" <<PC
prefix=/usr
libdir=\${prefix}/lib
includedir=\${prefix}/include/pdfium

Name: libpdfium
Description: Chromium's PDF renderer
Version: $_chromium
Libs: -L\${libdir} -lpdfium
Cflags: -I\${includedir}
PC

# libvips looks the library up as `pdfium` and compares its version against a
# pdfium BUILD number (>= 4200), so it gets a file of its own carrying
# $version (the branch number) rather than the Chromium version above, which
# would compare as 153 and fail. A symlink would hand both consumers the same
# Version line, and one of them the wrong one.
install -Dm644 /dev/stdin "$PKG/usr/lib/pkgconfig/pdfium.pc" <<PC
prefix=/usr
libdir=\${prefix}/lib
includedir=\${prefix}/include/pdfium

Name: pdfium
Description: Chromium's PDF renderer
Version: $version
Libs: -L\${libdir} -lpdfium
Cflags: -I\${includedir}
PC
install -Dm644 LICENSE "$PKG/usr/share/licenses/$name/LICENSE"
