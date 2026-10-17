# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

make LUA_VERSION=$_lv \
     LUA_INC="-I/usr/include/lua$_lv" \
     CFLAGS="$CFLAGS -fPIC -I/usr/include/lua$_lv" \
     LIB_OPTION="-shared $LDFLAGS"
make DESTDIR=$PKG PREFIX=/usr LUA_VERSION=$_lv install
