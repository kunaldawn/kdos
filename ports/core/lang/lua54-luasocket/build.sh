# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# LUAV, LUAPREFIX AND PLAT ON THE make LINE. Upstream defaults to LUAV=5.1 and
# LUAPREFIX_linux=/usr/local, and a `make VAR=` argument beats both the
# environment and the makefile's own `?=`, which is what an export would lose
# to. LUAINC_linux_base is where the headers are LOOKED FOR: the makefile
# appends /lua$(LUAV) itself, so it takes the parent directory.
make PLAT=linux LUAV=$_lv \
     LUAPREFIX_linux=/usr \
     LUAINC_linux_base=/usr/include
make install-unix DESTDIR=$PKG PLAT=linux LUAV=$_lv \
     LUAPREFIX_linux=/usr \
     prefix=/usr
