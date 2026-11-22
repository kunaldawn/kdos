# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

export CFLAGS_BASE2="$CFLAGS -Wno-error=array-bounds"
export CXXFLAGS_BASE2="$CXXFLAGS -Wno-error=array-bounds"
export LDFLAGS_STATIC_3="$LDFLAGS"
cd CPP/7zip/Bundles/Alone2
make -f ../../cmpl_gcc.mak
install -Dm755 b/g/7zz "$PKG/usr/bin/7zz"

ln -s 7zz "$PKG/usr/bin/7z"

install -Dm644 "$SRC/DOC/License.txt" "$PKG/usr/share/licenses/$name/License.txt"
install -Dm644 "$SRC/DOC/copying.txt" "$PKG/usr/share/licenses/$name/copying.txt"
install -Dm644 "$SRC/DOC/unRarLicense.txt" "$PKG/usr/share/licenses/$name/unRarLicense.txt"
