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

tar xf $PORT_SRC/${name}-vendor-${version}.tar.xz

# THE HIGHLIGHTING SETS ARE bat's. delta embeds them through the vendored bat
# crate (vendor/bat/src/assets.rs, include_bytes!), whose published copy
# carries the serialized sets and none of their sources. The bat port compiles
# them from those sources and installs them under /usr/share/bat/assets, so
# they replace the crate's before cargo runs. The vendored crate must be the
# version the bat port builds: the sets are untagged bincode dumps of the
# syntect types that version links, and a set from another one deserializes
# into garbage or aborts delta at the first highlighted line.
#
# Cargo checks every vendored file against the crate's .cargo-checksum.json,
# so the three entries are rewritten with the new files' hashes; the rest of
# the crate is still verified.
_batver=$(sed -n 's/^version = "\(.*\)"$/\1/p' vendor/bat/Cargo.toml | head -n 1)
_have=$(bat --version)
if [ "$_have" != "bat $_batver" ]; then
	echo "delta vendors bat $_batver; the installed sets are from '$_have'" >&2
	exit 1
fi
_sum=$(< vendor/bat/.cargo-checksum.json)
for _f in syntaxes.bin themes.bin acknowledgements.bin; do
	install -m644 /usr/share/bat/assets/$_f vendor/bat/assets/$_f
	[[ $_sum =~ \"assets/$_f\":\"([0-9a-f]{64})\" ]] || {
		echo "vendor/bat/.cargo-checksum.json lists no assets/$_f" >&2
		exit 1
	}
	_new=$(sha256sum vendor/bat/assets/$_f)
	_sum=${_sum/\"assets\/$_f\":\"${BASH_REMATCH[1]}\"/\"assets\/$_f\":\"${_new%% *}\"}
done
printf '%s' "$_sum" > vendor/bat/.cargo-checksum.json

export RUSTFLAGS="-C target-feature=-crt-static"

# git2/vendored-libgit2: libgit2-sys otherwise links a system libgit2 whenever
# pkg-config finds one in its version range, so the bundled copy is named.
cargo build --release --frozen --offline --features git2/vendored-libgit2
install -Dm755 target/release/delta $PKG/usr/bin/delta
