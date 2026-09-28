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

# Both window backends stay (the default features): winit picks Wayland first
# and dlopens its libraries, with the Vulkan or GL driver under wgpu, at run
# time. Images a document links by URL are fetched when it is opened, and a
# machine with no network shows the broken-image placeholder for them.
tar xf $PORT_SRC/${name}-vendor-${version}.tar.xz
export CARGO_HOME="$SRC_ROOT/.cargo"
export RUSTFLAGS="-C target-feature=-crt-static"
export LIBCLANG_PATH=/usr/lib
export CARGO_NET_OFFLINE=true
cargo build --release --frozen --offline -p inlyne
install -Dm755 target/release/inlyne "$PKG/usr/bin/inlyne"
install -Dm644 completions/inlyne.bash "$PKG/usr/share/bash-completion/completions/inlyne"
install -Dm644 completions/_inlyne "$PKG/usr/share/zsh/site-functions/_inlyne"
install -Dm644 completions/inlyne.fish "$PKG/usr/share/fish/vendor_completions.d/inlyne.fish"
install -Dm644 inlyne.default.toml "$PKG/usr/share/doc/inlyne/inlyne.default.toml"

# UPSTREAM SHIPS NO ICON, so the entry names the Markdown document icon the
# atlas draws. main.rs sets the window class and app_id to inlyne. MimeType
# is left out: mimeapps.list chooses the Markdown opener.
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
