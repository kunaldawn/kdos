# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

export CFLAGS="$CFLAGS -g0"
make LUA_V=$_lv \
     LUA_INC="-I/usr/include/lua$_lv" \
     EXPAT_INC="-I/usr/include" \
     LUA_LDIR=/usr/share/lua/$_lv \
     LUA_CDIR=/usr/lib/lua/$_lv
make DESTDIR=$PKG LUA_V=$_lv \
     LUA_LDIR=/usr/share/lua/$_lv \
     LUA_CDIR=/usr/lib/lua/$_lv install
