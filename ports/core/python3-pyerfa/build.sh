# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# THE BINDING, NOT A SECOND COPY OF ERFA. pyerfa vendors liberfa's C when it
# cannot find one; ports/core/erfa is that library, and PYERFA_USE_SYSTEM_LIBERFA
# is what makes this link against it instead — one copy on the machine, and one
# thing to fix when a leap second changes.
export PYERFA_USE_SYSTEM_LIBERFA=1

pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .
