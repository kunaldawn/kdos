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

tar xf $PORT_SRC/${name}-vendor-${version}.tar.xz
export CARGO_HOME="$SRC_ROOT/.cargo"
export RUSTFLAGS="-C target-feature=-crt-static"
export CARGO_NET_OFFLINE=true

# maturin builds the ruff binary into the wheel's scripts and installs the
# `ruff` Python package beside it, so both `ruff` and `python3 -m ruff` work;
# python-lsp-ruff reaches it through the module. --frozen makes a lock file
# that disagrees with the bundle fail instead of resolving. jemalloc is the
# copy tikv-jemalloc-sys compiles from its own sources.
export MATURIN_PEP517_ARGS="--frozen"
pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .
test -x "$PKG/usr/bin/ruff"

"$PKG/usr/bin/ruff" generate-shell-completion bash > ruff.bash
"$PKG/usr/bin/ruff" generate-shell-completion zsh > _ruff
"$PKG/usr/bin/ruff" generate-shell-completion fish > ruff.fish
install -Dm644 ruff.bash "$PKG/usr/share/bash-completion/completions/ruff"
install -Dm644 _ruff "$PKG/usr/share/zsh/site-functions/_ruff"
install -Dm644 ruff.fish "$PKG/usr/share/fish/vendor_completions.d/ruff.fish"
