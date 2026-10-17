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

cargo build --release --frozen --offline
install -Dm755 target/release/$name $PKG/usr/bin/$name

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/ttyper.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Typing Practice
GenericName=Typing Test
Comment=Practise typing speed and accuracy
Exec=ttyper
Icon=system-run
Terminal=true
Categories=Game;Education;
Keywords=typing;speed;wpm;practice;ttyper;
EOF
chmod 644 "$PKG/usr/share/applications/ttyper.desktop"
