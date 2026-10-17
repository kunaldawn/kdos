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
