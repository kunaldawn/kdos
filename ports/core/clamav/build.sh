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

# link-fts is Alpine's: musl has no fts(3), and clamonacc links musl-fts only
# with it.
patch -p1 -i "$PORT_SRC/link-fts.patch"

# The Rust crates are vendored in upstream's tarball under .cargo/vendor, and
# FindRust passes --offline whenever that directory exists; RUSTFLAGS reaches
# cargo through the environment. Bytecode signatures run in the interpreter:
# the LLVM runtime accepts only LLVM 8 to 13. The milter needs sendmail's
# libmilter, which is not a port. Signature databases are read from
# /var/lib/clamav; the package ships none.
export CARGO_HOME="$SRC_ROOT/.cargo"
export RUSTFLAGS="-C target-feature=-crt-static"
export CARGO_NET_OFFLINE=true
cmake -S . -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DCMAKE_SKIP_INSTALL_RPATH=ON \
	-DAPP_CONFIG_DIRECTORY=/etc/clamav \
	-DDATABASE_DIRECTORY=/var/lib/clamav \
	-DBYTECODE_RUNTIME=interpreter \
	-DENABLE_APP=ON \
	-DENABLE_CLAMONACC=ON \
	-DENABLE_MILTER=OFF \
	-DENABLE_SYSTEMD=OFF \
	-DENABLE_UNRAR=ON \
	-DENABLE_EXTERNAL_MSPACK=ON \
	-DENABLE_JSON_SHARED=ON \
	-DHAVE_SYSTEM_LFS_FTS=ON \
	-DENABLE_MAN_PAGES=ON \
	-DENABLE_DOXYGEN=OFF \
	-DENABLE_EXAMPLES=OFF \
	-DENABLE_TESTS=OFF \
	-DENABLE_STATIC_LIB=OFF \
	-DENABLE_SHARED_LIB=ON
cmake --build build
DESTDIR=$PKG cmake --install build

# clamsubmit does nothing but upload a sample to Cisco's servers.
rm -f "$PKG/usr/bin/clamsubmit" "$PKG/usr/share/man/man1/clamsubmit.1"

install -d "$PKG/var/lib/clamav"
