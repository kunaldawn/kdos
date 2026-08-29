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

# SYSTEM_CORROSION=ON, or src/taskchampion-cpp reaches for corrosion with
# FetchContent_Declare(GIT_REPOSITORY …) — a git clone in a build that has no
# network. The switch is upstream's own and turns the fetch into find_package.
mkdir -p build && cd build
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DENABLE_WASM=OFF \
	-DSYSTEM_CORROSION=ON
ninja
DESTDIR=$PKG ninja install
