# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# WITHOUT THIS THE SERVER TALKS IN THE CLEAR. luasocket carries the transport
# and nothing else; luasec is the TLS layer over it, and Prosody refuses to
# offer STARTTLS without it — which on a LAN XMPP server means every message
# and every password crosses the wire readable.
#
# LUAPATH AND LUACPATH DEFAULT TO 5.1 and are passed on the make line for the
# reason every module here is: a `make VAR=` argument beats the makefile's own
# `?=`, and an exported variable does not.
make linux LUA_VERSION=$_lv \
     INC_PATH="-I/usr/include/lua$_lv" \
     LUAPATH=/usr/share/lua/$_lv \
     LUACPATH=/usr/lib/lua/$_lv
make install DESTDIR=$PKG \
     LUAPATH=/usr/share/lua/$_lv \
     LUACPATH=/usr/lib/lua/$_lv
