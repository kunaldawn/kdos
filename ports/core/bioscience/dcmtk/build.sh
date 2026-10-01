# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# The CMake package names only the libraries. Exported executables make
# find_package(DCMTK) fail in a consumer as soon as one tool is missing.
patch -p1 -i "$PORT_SRC/dont-export-executables.patch"

patch -p1 -i "$PORT_SRC/openssl4.patch"

# DCMTK_ENABLE_LFS=lfs: musl's off_t is 64-bit already, and the lfs64 route
# calls fopen64() and friends, which musl declares only as macros under
# _LARGEFILE64_SOURCE. Character sets are converted by the bundled oficonv,
# which knows every DICOM character set; musl's iconv does not. tcp-wrappers
# and libsndfile are not ports, and neither is wanted. The prebuilt manual
# pages are installed; doxygen is not run.
cmake -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DCMAKE_INSTALL_SYSCONFDIR=/etc \
	-DBUILD_SHARED_LIBS=ON \
	-DBUILD_TESTING=OFF \
	-DDCMTK_ENABLE_LFS=lfs \
	-DDCMTK_ENABLE_STL=ON \
	-DDCMTK_ENABLE_PRIVATE_TAGS=ON \
	-DDCMTK_ENABLE_MANPAGES=ON \
	-DDCMTK_ENABLE_CHARSET_CONVERSION=oficonv \
	-DDCMTK_WITH_OPENSSL=ON \
	-DDCMTK_WITH_OPENJPEG=ON \
	-DDCMTK_WITH_PNG=ON \
	-DDCMTK_WITH_TIFF=ON \
	-DDCMTK_WITH_XML=ON \
	-DDCMTK_WITH_ZLIB=ON \
	-DDCMTK_WITH_THREADS=ON \
	-DDCMTK_WITH_ICONV=OFF \
	-DDCMTK_WITH_SNDFILE=OFF \
	-DDCMTK_WITH_WRAP=OFF \
	-DDCMTK_WITH_DOXYGEN=OFF
cmake --build build
DESTDIR=$PKG cmake --install build

# The release notes and the test dumps are not documentation a user reads.
rm -rf "$PKG/usr/share/doc"
rm -f "$PKG/usr/share/dcmtk-$version/SC.dump" "$PKG/usr/share/dcmtk-$version/VLP.dump"
