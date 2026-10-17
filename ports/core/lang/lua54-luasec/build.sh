# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

make linux LUA_VERSION=$_lv \
     INC_PATH="-I/usr/include/lua$_lv" \
     LUAPATH=/usr/share/lua/$_lv \
     LUACPATH=/usr/lib/lua/$_lv
make install DESTDIR=$PKG \
     LUAPATH=/usr/share/lua/$_lv \
     LUACPATH=/usr/lib/lua/$_lv
