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

cp -a "$SRC_ROOT/f128-$_f128/." f128/
cp -a "$SRC_ROOT/instruction-decoder-$_idecoder/." instruction-decoder/

tar xf $PORT_SRC/${name}-vendor-${version}.tar.xz
export CARGO_HOME="$SRC_ROOT/.cargo"
export RUSTFLAGS="-C target-feature=-crt-static"
export LIBCLANG_PATH=/usr/lib
export CARGO_NET_OFFLINE=true
cargo build --release --frozen --offline -p surfer -p surver
readelf -d target/release/surfer | grep -q 'NEEDED.*\[libzstd\.so\.1\]'
install -Dm755 target/release/surfer "$PKG/usr/bin/surfer"
install -Dm755 target/release/surver "$PKG/usr/bin/surver"

install -Dm644 surfer/assets/com.gitlab.surferproject.surfer.png \
	"$PKG/usr/share/icons/hicolor/256x256/apps/surfer.png"

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
