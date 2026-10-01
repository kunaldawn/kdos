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
#
# CFLAGS_linux is upstream's compile line for the platform and carries -ggdb3
# after anything MYCFLAGS adds, so it is given whole: the exported flags, the
# headers' directory and the NODEBUG define upstream's own line sets. The
# exported LDFLAGS go in through MYLDFLAGS, which the link line puts first.
make PLAT=linux LUAV=$_lv \
     LUAPREFIX_linux=/usr \
     LUAINC_linux_base=/usr/include \
     CFLAGS_linux="$CFLAGS -I/usr/include/lua$_lv -DLUASOCKET_NODEBUG" \
     MYLDFLAGS="$LDFLAGS"
make install-unix DESTDIR=$PKG PLAT=linux LUAV=$_lv \
     LUAPREFIX_linux=/usr \
     prefix=/usr
