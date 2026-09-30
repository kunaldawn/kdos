# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# Wine looks for an unpacked runtime at /usr/share/wine/mono/wine-mono-<v>
# before it offers to download the MSI, and it accepts only the version its
# own mscoree names (WINE_MONO_VERSION), so this moves with the wine port.
#
# The payload is Windows code that upstream builds with a MinGW toolchain and
# a .NET SDK, neither of which this tree carries; it is shipped as released.
install -d "$PKG/usr/share/wine/mono/wine-mono-$version"
cp -a . "$PKG/usr/share/wine/mono/wine-mono-$version/"
