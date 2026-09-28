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

# THE TWO SUBMODULES ARE SOURCES. The tag archive carries f128 and
# instruction-decoder empty, and both are path dependencies of the workspace,
# so cargo cannot resolve it without them. They go where the submodules sit.
cp -a "$SRC_ROOT/f128-$_f128/." f128/
cp -a "$SRC_ROOT/instruction-decoder-$_idecoder/." instruction-decoder/

# THE VENDOR BUNDLE IS MADE BY HAND. ports/fetch vendors from the first
# source alone, where the path dependencies are missing and `cargo vendor`
# fails; the bundle is `cargo vendor` run by the fetch container's cargo over
# this tree with both submodules in place, packed as ports/fetch packs one.
# Its sha256 line is its identity.
tar xf $PORT_SRC/${name}-vendor-${version}.tar.xz
export CARGO_HOME="$SRC_ROOT/.cargo"
export RUSTFLAGS="-C target-feature=-crt-static"
export LIBCLANG_PATH=/usr/lib
export CARGO_NET_OFFLINE=true
# surver is the waveform server a remote machine runs for `surfer <url>`.
# The window is eframe over glow, which dlopens EGL and the Wayland libraries.
cargo build --release --frozen --offline -p surfer -p surver
install -Dm755 target/release/surfer "$PKG/usr/bin/surfer"
install -Dm755 target/release/surver "$PKG/usr/bin/surver"

install -Dm644 surfer/assets/com.gitlab.surferproject.surfer.png \
	"$PKG/usr/share/icons/hicolor/256x256/apps/surfer.png"

# UPSTREAM'S ENTRY IS REPLACED. main.rs sets the app_id to
# org.surfer-project.surfer. The MIME types upstream lists are GTKWave's own
# and exist in no database here.
install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/surfer.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Surfer
GenericName=Waveform Viewer
Comment=View VCD, FST and GHW waveforms from digital simulations
TryExec=surfer
Exec=surfer %f
Icon=surfer
Terminal=false
StartupWMClass=org.surfer-project.surfer
Categories=Development;Electronics;Engineering;
Keywords=waveform;vcd;fst;ghw;simulation;verilog;vhdl;fpga;gtkwave;
DESKTOP
chmod 644 "$PKG/usr/share/applications/surfer.desktop"
