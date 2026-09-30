# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# Every provider is named, so a missing library stops the configure instead
# of quietly dropping HTTPS or SSH transport. The HTTP parser is the bundled
# llhttp: neither http-parser nor llhttp is a port. The git2 command-line
# tool is a test driver, not a git replacement, and is not built.
cmake -S . -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON \
	-DBUILD_TESTS=OFF \
	-DBUILD_CLI=OFF \
	-DBUILD_EXAMPLES=OFF \
	-DUSE_HTTPS=OpenSSL \
	-DUSE_SHA1=CollisionDetection \
	-DUSE_SHA256=HTTPS \
	-DUSE_SSH=libssh2 \
	-DUSE_HTTP_PARSER=builtin \
	-DREGEX_BACKEND=pcre2 \
	-DUSE_BUNDLED_ZLIB=OFF \
	-DUSE_GSSAPI=OFF \
	-DUSE_NTLMCLIENT=ON \
	-DUSE_THREADS=ON \
	-DUSE_NSEC=ON \
	-DENABLE_REPRODUCIBLE_BUILDS=ON
cmake --build build
DESTDIR=$PKG cmake --install build
