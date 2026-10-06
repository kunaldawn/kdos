# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# gitiles serves no byte-stable archive, so the source is a snapshot made from
# the commit Chromium's DEPS pins as gn_version: `git archive --prefix=gn/` of
# that commit, plus out/last_commit_position.h holding the position `git
# describe HEAD --abbrev=12 --match initial-commit` reports, which is the
# version's minor number. gen.py would otherwise build that header from git,
# and there is no repository here. A gn older than the pin rejects variables
# Chromium's BUILD files use.
tar -xf "$PORT_SRC/$name-$version.tar.zst" --strip-components=1

# gen.py picks clang++ on Linux unless CXX says otherwise, and adds -Werror
# unless told not to — a set tuned to upstream's own clang, not to this gcc.
export CC=gcc CXX=g++ AR=ar
python3 build/gen.py --no-last-commit-position --no-static-libstdc++ --allow-warnings
ninja -C out gn
install -Dm755 out/gn "$PKG/usr/bin/gn"
