# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# THE ONLY THING ON THIS MACHINE THAT CREATES COMMS RATHER THAN READING AN
# ARCHIVE. Everything else offline is a corpus somebody else wrote; a LAN XMPP
# server is two people on a stick talking to each other with no network beyond
# the room. dino-im already ships as a GUI client in the box.
#
# --lua-version 5.4 AND THE THREE PATHS THAT GO WITH IT. ports/core/lua is
# 5.5.0 and Prosody 13 supports 5.2 through 5.4, so this builds against the
# parallel lua54 port: the interpreter is `lua5.4`, its headers are in
# include/lua5.4 and its library is liblua5.4. Naming only the version would
# find the 5.5 headers on the default include path and compile against them.
./configure --prefix=/usr --sysconfdir=/etc/prosody \
	--datadir=/var/lib/prosody \
	--lua-version=$_lv \
	--with-lua-bin=/usr/bin \
	--with-lua-include=/usr/include/lua$_lv \
	--with-lua-lib=/usr/lib \
	--lua-suffix=$_lv \
	--no-example-certs \
	--ostype=linux
make
make DESTDIR=$PKG install
