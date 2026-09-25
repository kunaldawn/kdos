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
export CARGO_NET_OFFLINE=true

# pcre2-sys compiles its own bundled PCRE2 on any musl target unless
# PCRE2_SYS_STATIC=0 says otherwise, and the system library links only into a
# binary that is not crt-static — so both are set, and pcre2 on the depends
# line is the library `rg -P` actually runs.
export PCRE2_SYS_STATIC=0
export RUSTFLAGS="-C target-feature=-crt-static"

cargo build --release --frozen --offline --features 'pcre2'
install -Dm755 target/release/rg $PKG/usr/bin/rg

# Since ripgrep 14, the source tree no longer ships pre-generated
# completions or man page; the freshly-built rg emits them via
# --generate. See FAQ.md ("manpage" / "complete" sections).
target/release/rg --generate man          > rg.1
target/release/rg --generate complete-bash > rg.bash
target/release/rg --generate complete-zsh  > _rg
target/release/rg --generate complete-fish > rg.fish
install -Dm644 rg.1     $PKG/usr/share/man/man1/rg.1
install -Dm644 rg.bash  $PKG/usr/share/bash-completion/completions/rg
install -Dm644 _rg      $PKG/usr/share/zsh/site-functions/_rg
install -Dm644 rg.fish  $PKG/usr/share/fish/vendor_completions.d/rg.fish
