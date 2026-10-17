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

export VERGEN_GIT_SHA="$version"
export VERGEN_IDEMPOTENT=1

export YAZI_GEN_COMPLETIONS=1
export RUSTFLAGS="-C target-feature=-crt-static"
cargo build --release --frozen --offline
readelf -d target/release/yazi | grep -q 'NEEDED.*\[libonig\.so\.5\]'
install -Dm755 target/release/yazi $PKG/usr/bin/yazi
install -Dm755 target/release/ya   $PKG/usr/bin/ya

for b in yazi-boot/completions/yazi yazi-cli/completions/ya; do
	n=${b##*/}
	install -Dm644 "$b.bash" "$PKG/usr/share/bash-completion/completions/$n"
	install -Dm644 "${b%/*}/_$n" "$PKG/usr/share/zsh/site-functions/_$n"
	install -Dm644 "$b.fish" "$PKG/usr/share/fish/vendor_completions.d/$n.fish"
done

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/yazi.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Files (yazi)
GenericName=File Manager
Comment=Browse files with yazi
Exec=yazi %f
Icon=folder
Terminal=true
X-KDOS-Term=kdos-term
Categories=System;FileTools;FileManager;
Keywords=file;manager;browser;yazi;preview;
EOF
chmod 644 "$PKG/usr/share/applications/yazi.desktop"
