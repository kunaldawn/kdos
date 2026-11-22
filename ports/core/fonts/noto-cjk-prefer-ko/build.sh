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

install -dm755 $PKG/etc/fonts/conf.d
cat > $PKG/etc/fonts/conf.d/64-noto-cjk-prefer-ko.conf <<'XML'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE fontconfig SYSTEM "urn:fontconfig:fonts.dtd">
<fontconfig>
  <alias>
    <family>sans-serif</family>
    <prefer><family>Noto Sans CJK KR</family></prefer>
  </alias>
  <alias>
    <family>serif</family>
    <prefer><family>Noto Sans CJK KR</family></prefer>
  </alias>
  <alias>
    <family>monospace</family>
    <prefer><family>Noto Sans Mono CJK KR</family></prefer>
  </alias>
</fontconfig>
XML
