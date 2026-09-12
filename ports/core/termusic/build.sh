# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# THE SYMPHONIA BACKEND, WHICH IS WHAT NO FEATURE FLAG MEANS. termusic can play
# through GStreamer or through libmpv instead, and both are a second media
# stack underneath a terminal music player; symphonia is pure Rust and decodes
# what this machine actually holds. The server crate refuses to compile with no
# backend at all, so the default is not "none" — it is this one.
tar xf $PORT_SRC/${name}-vendor-${version}.tar.xz
export CARGO_HOME="$SRC_ROOT/.cargo"
export RUSTFLAGS="-C target-feature=-crt-static"
export CARGO_NET_OFFLINE=true

cargo build --release --frozen --offline

# Two binaries and they are not interchangeable: the daemon owns the audio
# device and the TUI talks to it, so shipping only the front end gives a player
# that starts and cannot play.
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
