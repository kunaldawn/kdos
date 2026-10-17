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
cargo build --release --frozen --offline

install -Dm755 target/release/pizauth $PKG/usr/bin/pizauth
install -Dm644 pizauth.1 -t $PKG/usr/share/man/man1
install -Dm644 pizauth.conf.5 -t $PKG/usr/share/man/man5
install -Dm644 share/bash/completion.bash $PKG/usr/share/bash-completion/completions/pizauth
install -Dm644 share/fish/pizauth.fish $PKG/usr/share/fish/vendor_completions.d/pizauth.fish
install -Dm644 share/zsh/_pizauth $PKG/usr/share/zsh/site-functions/_pizauth
