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

cargo build --release --frozen --offline
install -Dm755 target/release/chess-tui "$PKG/usr/bin/chess-tui"

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
