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

cargo build --release --frozen --offline
readelf -d target/release/delta | grep -q 'NEEDED.*\[libgit2\.so\.1\.9\]'
readelf -d target/release/delta | grep -q 'NEEDED.*\[libonig\.so\.5\]'
install -Dm755 target/release/delta $PKG/usr/bin/delta
