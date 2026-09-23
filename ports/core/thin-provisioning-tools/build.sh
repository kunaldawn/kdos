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


# THE SHIPPED lvm.conf ALREADY NAMES THESE BY ABSOLUTE PATH —
# `thin_check_executable = "/usr/sbin/thin_check"` — so the install location
# is not a preference. LVM shells out to thin_check before it will activate a
# thin pool and refuses the pool when the call fails.
tar xf $PORT_SRC/${name}-vendor-${version}.tar.xz
export CARGO_HOME="$SRC_ROOT/.cargo"
export RUSTFLAGS="-C target-feature=-crt-static"
export CARGO_NET_OFFLINE=true
cargo build --release --frozen --offline

# ONE BINARY, MANY NAMES, dispatched on argv[0] — upstream's own shape since
# the Rust rewrite. The symlinks are what make `thin_check` a command.
install -Dm755 target/release/pdata_tools "$PKG/usr/sbin/pdata_tools"
for t in thin_check thin_dump thin_restore thin_repair thin_rmap thin_metadata_size \
         thin_trim thin_delta thin_ls thin_metadata_pack thin_metadata_unpack thin_migrate \
         cache_check cache_dump cache_restore cache_repair cache_metadata_size cache_writeback \
         era_check era_dump era_restore era_invalidate; do
	ln -sf pdata_tools "$PKG/usr/sbin/$t"
	make "man8/$t.8"
	install -Dm644 "man8/$t.8" -t "$PKG/usr/share/man/man8"
done
