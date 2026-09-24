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

# use-jemalloc makes jemalloc fd's global allocator; the feature is off by
# default. fd's parallel walker allocates from every thread at once, which
# musl's malloc serialises behind one lock. The crate and the jemalloc sources
# it compiles are in the vendor bundle.
cargo build --release --frozen --offline --features use-jemalloc
install -Dm755 target/release/fd $PKG/usr/bin/fd

# The tarball ships only the zsh completion (_fd) and the man page; the bash
# and fish completions come from the built fd via --gen-completions.
target/release/fd --gen-completions bash > fd.bash
target/release/fd --gen-completions fish > fd.fish
install -Dm644 fd.bash              $PKG/usr/share/bash-completion/completions/fd
install -Dm644 fd.fish              $PKG/usr/share/fish/vendor_completions.d/fd.fish
install -Dm644 contrib/completion/_fd $PKG/usr/share/zsh/site-functions/_fd
install -Dm644 doc/fd.1             $PKG/usr/share/man/man1/fd.1
