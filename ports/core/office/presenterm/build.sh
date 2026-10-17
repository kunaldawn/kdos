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

install -m644 /usr/share/bat/assets/syntaxes.bin /usr/share/bat/assets/themes.bin \
	/usr/share/bat/assets/acknowledgements.txt bat/

export RUSTFLAGS="-C target-feature=-crt-static"
cargo build --release --frozen --offline
readelf -d target/release/presenterm | grep -q 'NEEDED.*\[libonig\.so\.5\]'

install -Dm755 target/release/presenterm $PKG/usr/bin/presenterm

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/presenterm.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Slides
GenericName=Presentation
Comment=Present a markdown file as slides
Exec=presenterm %f
Icon=x-office-presentation
Terminal=true
Categories=Office;Presentation;
Keywords=slides;presentation;markdown;talk;presenterm;
X-KDOS-Term=kdos-term
EOF
chmod 644 "$PKG/usr/share/applications/presenterm.desktop"
