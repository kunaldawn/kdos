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

tar xf "$PORT_SRC/$name-$version.tar.xz"

install -dm755 "$PKG/usr/share/fonts/nerd-fonts"
install -m644 SymbolsNerdFont-Regular.ttf SymbolsNerdFontMono-Regular.ttf \
	"$PKG/usr/share/fonts/nerd-fonts/"

install -Dm644 LICENSE "$PKG/usr/share/licenses/$name/LICENSE"

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
