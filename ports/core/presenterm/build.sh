# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# DEFAULT FEATURES, WHICH ARE NONE. The only optional one is `json-schema`,
# which exists to emit a schema for the config file and pulls schemars in for
# it; a presenter does not need to describe itself to an editor.
#
# THE PICTURES ARE THE TERMINAL'S. presenterm draws images through the sixel
# and kitty protocols, which kdos-term speaks — so a slide with a diagram in it
# works on the console, and falls back to blocks where it does not.
tar xf $PORT_SRC/${name}-vendor-${version}.tar.xz

cargo build --release --frozen --offline

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
EOF
chmod 644 "$PKG/usr/share/applications/presenterm.desktop"
