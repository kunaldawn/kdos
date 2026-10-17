# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

for pin in "build_revision=$_build_rev" "abseil_revision=$_abseil_rev"; do
	if ! grep -q "'${pin%%=*}': '${pin#*=}'" DEPS; then
		echo "pdfium: DEPS does not name ${pin%%=*} ${pin#*=} — rebuild the vendor bundle" >&2
		exit 1
	fi
done
rm -rf third_party/abseil-cpp
tar -xf "$PORT_SRC/$name-vendor-$version.tar.xz" --strip-components=1

patch -p1 -i "$PORT_SRC/pdfium-shared-library.patch"

printf 'build_with_chromium = false\n' > build/config/gclient_args.gni
install -Dm644 build/linux/unbundle/icu.gn third_party/icu/BUILD.gn

export CC=gcc CXX=g++ AR=ar NM=nm
export CPPFLAGS="$CPPFLAGS -ffp-contract=off"

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
