# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# THE XML PARSER PROSODY READS EVERY STANZA WITH. XMPP is XML on the wire, so
# without this the server starts and cannot understand a single message.
#
# LUA_INC AND EXPAT_INC CARRY THEIR OWN -I. They are pasted straight into the
# compiler line, so a bare path there is an argument the compiler reads as a
# source file.
make LUA_V=$_lv \
     LUA_INC="-I/usr/include/lua$_lv" \
     EXPAT_INC="-I/usr/include" \
     LUA_LDIR=/usr/share/lua/$_lv \
     LUA_CDIR=/usr/lib/lua/$_lv
make DESTDIR=$PKG LUA_V=$_lv \
     LUA_LDIR=/usr/share/lua/$_lv \
     LUA_CDIR=/usr/lib/lua/$_lv install
