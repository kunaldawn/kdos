# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# The snapshot is Alpine's, because gitiles serves no byte-stable archive. It
# carries out/last_commit_position.h already generated; gen.py would otherwise
# build that header from `git describe`, and there is no repository here.
tar -xf "$PORT_SRC/$name-$version.tar.zst" --strip-components=1

# gen.py picks clang++ on Linux unless CXX says otherwise, and adds -Werror
# unless told not to — a set tuned to upstream's own clang, not to this gcc.
export CC=gcc CXX=g++ AR=ar
python3 build/gen.py --no-last-commit-position --no-static-libstdc++ --allow-warnings
ninja -C out gn
install -Dm755 out/gn "$PKG/usr/bin/gn"
