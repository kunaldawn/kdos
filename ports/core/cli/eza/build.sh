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

# vendored-libgit2 builds git2's bundled libgit2 whatever pkg-config finds,
# since no libgit2 port exists to pin a system copy to; it compresses through
# the system zlib.
cargo build --release --frozen --offline --features vendored-libgit2
install -Dm755 target/release/eza $PKG/usr/bin/eza
for page in eza.1 eza_colors.5 eza_colors-explanation.5; do
	lowdown -s -Tman -M source=v$version -o target/$page man/$page.md
	install -Dm644 target/$page $PKG/usr/share/man/man${page##*.}/$page
done
install -Dm644 completions/bash/eza     $PKG/usr/share/bash-completion/completions/eza
install -Dm644 completions/fish/eza.fish $PKG/usr/share/fish/vendor_completions.d/eza.fish
install -Dm644 completions/zsh/_eza      $PKG/usr/share/zsh/site-functions/_eza
