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
readelf -d target/release/iamb | grep -q 'NEEDED.*\[libsqlite3\.so\.0\]'

install -Dm755 target/release/iamb $PKG/usr/bin/iamb
install -Dm644 docs/iamb.1 -t "$PKG/usr/share/man/man1"
install -Dm644 docs/iamb.5 -t "$PKG/usr/share/man/man5"

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/iamb.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Matrix
GenericName=Chat
Comment=Matrix chat with vim's keys
Exec=iamb
Icon=mail-message
Terminal=true
Categories=Network;InstantMessaging;
Keywords=chat;matrix;im;message;iamb;
EOF
chmod 644 "$PKG/usr/share/applications/iamb.desktop"
