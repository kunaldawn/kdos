# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# Alone2 is the one bundle that builds 7zz, the standalone program with every
# format and codec compiled in; the other bundles need a 7z.so loaded beside
# them. The makefile assigns CFLAGS and CXXFLAGS itself, so the tree's flags
# ride in through CFLAGS_BASE2 and CXXFLAGS_BASE2, the two slots it leaves
# unassigned. Without them the reproducibility flags never reach the compiler.
export CFLAGS_BASE2="$CFLAGS"
export CXXFLAGS_BASE2="$CXXFLAGS"
cd CPP/7zip/Bundles/Alone2
make -f ../../cmpl_gcc.mak
install -Dm755 b/g/7zz "$PKG/usr/bin/7zz"
