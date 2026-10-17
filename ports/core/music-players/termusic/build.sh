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
export LIBCLANG_PATH=/usr/lib

cargo build --release --frozen --offline \
	--features termusic-server/rusty-libopus,termusic-server/rusty-soundtouch,termusic-server/rusty-simd,termusic/cover-viuer-sixel
readelf -d target/release/termusic | grep -q 'NEEDED.*\[libsqlite3\.so\.0\]'
readelf -d target/release/termusic-server | grep -q 'NEEDED.*\[libsqlite3\.so\.0\]'

install -Dm755 target/release/termusic        $PKG/usr/bin/termusic
install -Dm755 target/release/termusic-server $PKG/usr/bin/termusic-server

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/termusic.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Music
GenericName=Music Player
Comment=Play a music library
Exec=termusic
Icon=folder-music
Terminal=true
Categories=AudioVideo;Player;
Keywords=music;audio;player;library;termusic;
EOF
chmod 644 "$PKG/usr/share/applications/termusic.desktop"
