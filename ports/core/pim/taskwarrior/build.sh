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

export LDFLAGS="$LDFLAGS -Wl,--strip-debug"

tar xf $PORT_SRC/${name}-vendor-${version}.tar.xz
export CARGO_HOME="$SRC_ROOT/.cargo"
export RUSTFLAGS="-C target-feature=-crt-static"
export CARGO_NET_OFFLINE=true

export LIBSQLITE3_SYS_USE_PKG_CONFIG=1

cp -r vendor/cxxbridge-cmd "$SRC_ROOT/cxxbridge-cmd"
cp Cargo.lock "$SRC_ROOT/cxxbridge-cmd/Cargo.lock"
cargo install --path "$SRC_ROOT/cxxbridge-cmd" --offline \
	--root "$SRC_ROOT/cxxbridge" \
	--config "source.crates-io.replace-with='vendored-sources'" \
	--config "source.vendored-sources.directory='$SRC/vendor'"

mkdir -p build && cd build
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DENABLE_WASM=OFF \
	-DSYSTEM_CORROSION=ON \
	-DCMAKE_CXX_STANDARD_LIBRARIES=-lsqlite3 \
	-DINSTALLED_CXXBRIDGE="$SRC_ROOT/cxxbridge/bin/cxxbridge"
ninja
DESTDIR=$PKG ninja install
cd ..

install -Dm644 scripts/bash/task.sh "$PKG/usr/share/bash-completion/completions/task"
