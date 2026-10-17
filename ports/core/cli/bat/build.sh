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
export BAT_ASSETS_GEN_DIR="$SRC/gen"

tar xf $PORT_SRC/${name}-assets-${version}.tar.xz
for p in assets/patches/*.patch; do
	patch -d assets -p0 < "$p"
done

export RUSTFLAGS="-C target-feature=-crt-static"

cargo build --release --frozen --offline
target/release/bat cache --build --blank --acknowledgements \
	--source=assets --target=assets
cargo build --release --frozen --offline
readelf -d target/release/bat | grep -q 'NEEDED.*\[libgit2\.so\.1\.9\]'
readelf -d target/release/bat | grep -q 'NEEDED.*\[libonig\.so\.5\]'

install -Dm755 target/release/bat $PKG/usr/bin/bat
install -Dm644 gen/assets/manual/bat.1 -t "$PKG/usr/share/man/man1"
install -Dm644 gen/assets/completions/bat.bash "$PKG/usr/share/bash-completion/completions/bat"
install -Dm644 gen/assets/completions/bat.zsh  "$PKG/usr/share/zsh/site-functions/_bat"
install -Dm644 gen/assets/completions/bat.fish "$PKG/usr/share/fish/vendor_completions.d/bat.fish"

install -d "$PKG/usr/libexec/bat"
cat > "$PKG/usr/libexec/bat/man-pager" <<'KDOS_SH'
#!/bin/sh
sed -e 's/\x1b\[[0-9;]*m//g' -e 's/.\x08//g' "$@" | bat -l man -p
KDOS_SH
chmod 755 "$PKG/usr/libexec/bat/man-pager"

install -Dm644 assets/syntaxes.bin assets/themes.bin assets/acknowledgements.bin \
	-t "$PKG/usr/share/bat/assets"
{
	cat LICENSE-MIT
	target/release/bat --no-config --acknowledgements
} > "$PKG/usr/share/bat/assets/acknowledgements.txt"
chmod 644 "$PKG/usr/share/bat/assets/acknowledgements.txt"
