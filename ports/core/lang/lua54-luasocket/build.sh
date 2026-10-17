# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

make PLAT=linux LUAV=$_lv \
     LUAPREFIX_linux=/usr \
     LUAINC_linux_base=/usr/include \
     CFLAGS_linux="$CFLAGS -I/usr/include/lua$_lv -DLUASOCKET_NODEBUG" \
     MYLDFLAGS="$LDFLAGS"
make install-unix DESTDIR=$PKG PLAT=linux LUAV=$_lv \
     LUAPREFIX_linux=/usr \
     prefix=/usr
