# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# Wine looks for unpacked engines at
# /usr/share/wine/gecko/wine-gecko-<v>-<arch> before it offers to download
# the MSIs, and it accepts only the version its own appwiz.cpl names
# (GECKO_VERSION), so this moves with the wine port. A WoW64 prefix needs
# both: x86 for 32-bit programs and x86_64 for 64-bit ones.
#
# The payload is Windows code that upstream builds with a MinGW toolchain from
# a Firefox-sized tree; it is shipped as released.
install -d "$PKG/usr/share/wine/gecko/wine-gecko-$version-x86"
cp -a . "$PKG/usr/share/wine/gecko/wine-gecko-$version-x86/"
cp -a "$SRC_ROOT/wine-gecko-$version-x86_64" "$PKG/usr/share/wine/gecko/"
