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

# THE RELEASE TARBALL IS FLAT — the faces, the licence and upstream's own
# fontconfig file sit at its top level with no directory above them — and kpkg
# extracts the first source with `--strip-components=1`. Every member loses its
# only path component, tar drops it and exits 0, and $SRC is left EMPTY. So the
# archive is unpacked here, out of the port directory, the way yosys unpacks
# its own.
tar xf "$PORT_SRC/$name-$version.tar.xz"

# BOTH FACES, because they are different shapes of the same glyphs. `Symbols
# Nerd Font Mono` is spacing=100 and each icon fits one advance, which is what
# a cell grid needs; `Symbols Nerd Font` is the double-width drawing, correct
# where the text is laid out proportionally and wrong in a terminal, where it
# would run into the next cell.
install -dm755 "$PKG/usr/share/fonts/nerd-fonts"
install -m644 SymbolsNerdFont-Regular.ttf SymbolsNerdFontMono-Regular.ttf \
	"$PKG/usr/share/fonts/nerd-fonts/"

install -Dm644 LICENSE "$PKG/usr/share/licenses/$name/LICENSE"

# UPSTREAM'S OWN `10-nerd-font-symbols.conf` IS NOT INSTALLED, and this file
# replaces it. That one `<prefer>`s the PROPORTIONAL face for `monospace` at
# priority 10, which lands it in the fallback list ahead of the real monospace
# family's own bold and oblique faces: a cell grid then reaches the
# double-width drawing first and an icon runs into the next cell. Its other 397
# aliases name patched families this image does not ship.
#
# `<accept>` is the other end of that list — the family is inserted AFTER the
# generic, behind every real text face — so it answers only a codepoint those
# faces do not carry, which is the Private Use Area this font exists for.
# Numbered above 56-noto-preferred.conf and the 57-dejavu-*.conf set: those add
# the preferred families to the pattern, and an accept can only append behind
# what is already there.
#
# AND THE CELL FAMILIES ARE NAMED ONE BY ONE, because a pattern asking for a
# concrete family carries no generic for an alias on `monospace` to match.
# `Terminus:pixelsize=32` is what the shell asks for, and without its own rule
# the first symbols face fontconfig sorts to is the proportional one — the
# wrong width in a grid. `Terminus` is terminus-font's bitmap build and
# `Terminus (TTF)` is terminus-ttf's; `Noto Sans Mono` and `DejaVu Sans Mono`
# already carry the generic through their own ports' rules.
install -dm755 "$PKG/etc/fonts/conf.d"
cat > "$PKG/etc/fonts/conf.d/66-nerd-font-symbols.conf" <<'XML'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE fontconfig SYSTEM "urn:fontconfig:fonts.dtd">
<fontconfig>
  <alias>
    <family>monospace</family>
    <accept><family>Symbols Nerd Font Mono</family></accept>
  </alias>
  <alias>
    <family>Terminus</family>
    <accept><family>Symbols Nerd Font Mono</family></accept>
  </alias>
  <alias>
    <family>Terminus (TTF)</family>
    <accept><family>Symbols Nerd Font Mono</family></accept>
  </alias>
  <alias>
    <family>sans-serif</family>
    <accept><family>Symbols Nerd Font</family></accept>
  </alias>
  <alias>
    <family>serif</family>
    <accept><family>Symbols Nerd Font</family></accept>
  </alias>
</fontconfig>
XML
