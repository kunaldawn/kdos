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
#
# THE HIGHLIGHTING SETS ARE bat's. src/code/highlighting.rs embeds
# bat/syntaxes.bin and bat/themes.bin with include_bytes!, and main.rs prints
# bat/acknowledgements.txt for --acknowledgements; upstream copies all three
# out of a bat checkout, and the tarball carries none of their sources. The bat
# port compiles the sets from those sources and installs them under
# /usr/share/bat/assets, so they replace upstream's copies before cargo runs.
# presenterm reads them as bincode dumps of its own syntect's types and of
# bat's lazy theme-set layout, which carry no version tag: a bat that links
# another syntect major gives sets that abort presenterm at the first code
# block, and a bump of either port checks the two Cargo.lock files agree.
tar xf $PORT_SRC/${name}-vendor-${version}.tar.xz

install -m644 /usr/share/bat/assets/syntaxes.bin /usr/share/bat/assets/themes.bin \
	/usr/share/bat/assets/acknowledgements.txt bat/

export RUSTFLAGS="-C target-feature=-crt-static"
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
X-KDOS-Term=kdos-term
EOF
chmod 644 "$PKG/usr/share/applications/presenterm.desktop"
