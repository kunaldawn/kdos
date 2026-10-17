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

export RUSTFLAGS="-C target-feature=-crt-static"
export LIBCLANG_PATH=/usr/lib
export OUCH_ARTIFACTS_FOLDER="$PWD/artifacts"

cargo build --release --frozen --offline
readelf -d target/release/ouch | grep -q 'NEEDED.*\[libzstd\.so\.1\]'

install -Dm755 target/release/ouch $PKG/usr/bin/ouch
install -Dm644 artifacts/*.1 -t $PKG/usr/share/man/man1
install -Dm644 artifacts/ouch.bash "$PKG/usr/share/bash-completion/completions/ouch"
install -Dm644 artifacts/_ouch "$PKG/usr/share/zsh/site-functions/_ouch"
install -Dm644 artifacts/ouch.fish "$PKG/usr/share/fish/vendor_completions.d/ouch.fish"
