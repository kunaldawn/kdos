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
export LIBCLANG_PATH=/usr/lib
export CARGO_NET_OFFLINE=true
cargo build --release --frozen --offline -p inlyne
readelf -d target/release/inlyne | grep -q 'NEEDED.*\[libonig\.so\.5\]'
install -Dm755 target/release/inlyne "$PKG/usr/bin/inlyne"
install -Dm644 completions/inlyne.bash "$PKG/usr/share/bash-completion/completions/inlyne"
install -Dm644 completions/_inlyne "$PKG/usr/share/zsh/site-functions/_inlyne"
install -Dm644 completions/inlyne.fish "$PKG/usr/share/fish/vendor_completions.d/inlyne.fish"
install -Dm644 inlyne.default.toml "$PKG/usr/share/doc/inlyne/inlyne.default.toml"

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/inlyne.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Inlyne
GenericName=Markdown Viewer
Comment=View Markdown and HTML documents, reloaded as they change
TryExec=inlyne
Exec=inlyne view %f
Icon=text-markdown
Terminal=false
StartupWMClass=inlyne
Categories=Office;Viewer;
Keywords=markdown;md;readme;viewer;preview;
DESKTOP
chmod 644 "$PKG/usr/share/applications/inlyne.desktop"
