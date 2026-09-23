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

# 3.x IS PART RUST AND THAT IS WHY THIS HAS A VENDOR BUNDLE. Taskwarrior moved
# its storage layer to `taskchampion`, a Rust crate, in 3.0 — so a C++ program
# now needs cargo at build time and an offline crate set. The 2.x line had
# neither and is not what upstream maintains.
#
# TASKCHAMPION_SYNC IS OFF BY CONSTRUCTION: sync needs a server, and the same
# argument that turned off fcitx5's cloud pinyin and croc's public relay
# applies — the tasks are a local sqlite under `~/.local/share/task` and stay
# there. `task export` is the way out.
tar xf $PORT_SRC/${name}-vendor-${version}.tar.xz
export CARGO_HOME="$SRC_ROOT/.cargo"
export RUSTFLAGS="-C target-feature=-crt-static"
export CARGO_NET_OFFLINE=true

# CXXBRIDGE COMES FROM THE VENDOR BUNDLE. corrosion_add_cxxbridge() accepts a
# cxxbridge only at exactly the cxx version the lockfile names, and otherwise
# runs `cargo install cxxbridge-cmd`, which is a download. The bundle carries
# vendor/cxxbridge-cmd at that version, so it is built here and handed over
# as INSTALLED_CXXBRIDGE, the cache variable corrosion's find_program fills.
#
# The crate is copied out of the tree because inside it cargo takes it for an
# undeclared member of taskwarrior's workspace and refuses. Its own Cargo.lock
# pins versions the bundle does not carry, so --locked cannot be used; the
# workspace's lockfile goes in its place, and resolution offline against the
# vendor directory can only pick what the bundle holds.
cp -r vendor/cxxbridge-cmd "$SRC_ROOT/cxxbridge-cmd"
cp Cargo.lock "$SRC_ROOT/cxxbridge-cmd/Cargo.lock"
cargo install --path "$SRC_ROOT/cxxbridge-cmd" --offline \
	--root "$SRC_ROOT/cxxbridge" \
	--config "source.crates-io.replace-with='vendored-sources'" \
	--config "source.vendored-sources.directory='$SRC/vendor'"

# SYSTEM_CORROSION=ON, or src/taskchampion-cpp reaches for corrosion with
# FetchContent_Declare(GIT_REPOSITORY …) — a git clone in a build that has no
# network. The switch is upstream's own and turns the fetch into find_package.
mkdir -p build && cd build
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DENABLE_WASM=OFF \
	-DSYSTEM_CORROSION=ON \
	-DINSTALLED_CXXBRIDGE="$SRC_ROOT/cxxbridge/bin/cxxbridge"
ninja
DESTDIR=$PKG ninja install
