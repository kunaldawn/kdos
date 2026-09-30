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

# The default features, which are the sound: moves play through rodio and
# cpal to ALSA, which PipeWire answers. HTTPS for the Lichess mode goes through
# rustls on aws-lc, compiled from the vendored crate; that mode needs a
# network, and every other mode works without one.
cargo build --release --frozen --offline
install -Dm755 target/release/chess-tui "$PKG/usr/bin/chess-tui"

# chess-tui has no default engine and asks for one until it is given a path;
# the entry names the stockfish port's. The path given is also written to the
# player's configuration.
install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/chess-tui.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Chess
GenericName=Chess Game
Comment=Play chess against Stockfish or a second player
Exec=chess-tui -e /usr/bin/stockfish
Icon=input-gaming
Terminal=true
Categories=Game;BoardGame;
Keywords=chess;stockfish;board;game;uci;
DESKTOP
chmod 644 "$PKG/usr/share/applications/chess-tui.desktop"
