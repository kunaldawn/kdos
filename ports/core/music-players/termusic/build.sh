# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# THE SYMPHONIA BACKEND, WHICH IS WHAT NO BACKEND FEATURE MEANS. termusic can play
# through GStreamer or through libmpv instead, and both are a second media
# stack underneath a terminal music player; symphonia is pure Rust and decodes
# what this machine actually holds. The server crate refuses to compile with no
# backend at all, so the default is not "none" — it is this one.
tar xf $PORT_SRC/${name}-vendor-${version}.tar.xz
export CARGO_HOME="$SRC_ROOT/.cargo"
export RUSTFLAGS="-C target-feature=-crt-static"
export CARGO_NET_OFFLINE=true
export LIBCLANG_PATH=/usr/lib

# None of these is a second backend; each fills a gap in the symphonia build:
#
#   rusty-libopus     symphonia has no Opus decoder, so without it a .opus file
#                     does not play. Off Windows the adapter links the system
#                     libopus (the `opus` port) rather than building its own.
#   rusty-soundtouch  playback speed without a pitch shift; the workspace pins
#                     the crate's bundled SoundTouch, compiled from the vendor
#                     tarball. Its -ffi crate generates its bindings with
#                     bindgen, which dlopens libclang (the `clang` port) at
#                     build time; LIBCLANG_PATH saves it the search.
#   rusty-simd        symphonia's SIMD decode paths.
#   cover-viuer-sixel album art on a sixel terminal; kitty and iTerm are the
#                     tui crate's defaults and sixel is not.
#
# A workspace root takes features as crate/feature.
cargo build --release --frozen --offline \
	--features termusic-server/rusty-libopus,termusic-server/rusty-soundtouch,termusic-server/rusty-simd,termusic/cover-viuer-sixel

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
