# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# THE BEST EMBEDDED printf CHANNEL THERE IS. RTT moves bytes through a ring
# buffer in the target's own RAM over the debug probe, so a running MCU prints
# without a UART, without a pin and without changing timing much — and probe-rs
# is the one free implementation that also flashes and debugs over CMSIS-DAP,
# ST-Link and J-Link.
#
# ~4000 CHIP DESCRIPTIONS ARE COMPILED IN, which is what makes this work with
# no network: the target definitions are built into the binary rather than
# fetched. A part outside that set needs a CMSIS-Pack, which cannot be
# downloaded here — that is the honest limit and it is a small one.
tar xf $PORT_SRC/${name}-vendor-${version}.tar.xz
export CARGO_HOME="$SRC_ROOT/.cargo"
export RUSTFLAGS="-C target-feature=-crt-static"
export CARGO_NET_OFFLINE=true

cargo build --release --frozen --offline -p probe-rs-tools
install -Dm755 target/release/probe-rs $PKG/usr/bin/probe-rs
