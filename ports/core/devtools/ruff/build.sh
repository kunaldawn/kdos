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

export MATURIN_PEP517_ARGS="--frozen"
pip3 install --ignore-installed --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .
test -x "$PKG/usr/bin/ruff"

"$PKG/usr/bin/ruff" generate-shell-completion bash > ruff.bash
"$PKG/usr/bin/ruff" generate-shell-completion zsh > _ruff
"$PKG/usr/bin/ruff" generate-shell-completion fish > ruff.fish
install -Dm644 ruff.bash "$PKG/usr/share/bash-completion/completions/ruff"
install -Dm644 _ruff "$PKG/usr/share/zsh/site-functions/_ruff"
install -Dm644 ruff.fish "$PKG/usr/share/fish/vendor_completions.d/ruff.fish"
