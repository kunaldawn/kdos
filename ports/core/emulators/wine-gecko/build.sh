# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

install -d "$PKG/usr/share/wine/gecko/wine-gecko-$version-x86"
cp -a . "$PKG/usr/share/wine/gecko/wine-gecko-$version-x86/"
cp -a "$SRC_ROOT/wine-gecko-$version-x86_64" "$PKG/usr/share/wine/gecko/"
install -d "$PKG/usr/share/licenses/$name"
unzip -p omni.ja chrome/toolkit/content/global/license.html > "$PKG/usr/share/licenses/$name/license.html" || [ $? -eq 2 ]
grep -q 'Mozilla Public License' "$PKG/usr/share/licenses/$name/license.html"
