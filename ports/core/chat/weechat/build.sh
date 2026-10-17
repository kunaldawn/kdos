#!/bin/bash
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

mkdir -p build
cd build
cmake .. \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DLIBDIR=/usr/lib \
	-DENABLE_MAN=ON \
	-DENABLE_DOC=OFF \
	-DENABLE_TESTS=OFF \
	-DENABLE_NLS=OFF \
	-DENABLE_NCURSES=ON \
	-DENABLE_HEADLESS=ON \
	-DENABLE_ZSTD=ON \
	-DENABLE_CJSON=ON \
	-DENABLE_SCRIPT=ON \
	-DENABLE_SCRIPTS=ON \
	-DENABLE_PYTHON=ON \
	-DENABLE_PERL=ON \
	-DENABLE_LUA=ON \
	-DENABLE_TCL=ON \
	-DTCL_LIBRARY=/usr/lib/libtcl9.0.so \
	-DTCL_INCLUDE_PATH=/usr/include \
	-DENABLE_RUBY=ON \
	-DENABLE_GUILE=ON \
	-DENABLE_PHP=OFF \
	-DENABLE_JAVASCRIPT=OFF \
	-DENABLE_SPELL=ON \
	-DENABLE_ENCHANT=ON
make
make DESTDIR=$PKG install
cd ..

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/weechat.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=IRC
GenericName=Chat
Comment=IRC client with scripting and a relay
Exec=weechat
Icon=weechat
Terminal=true
Categories=Network;InstantMessaging;
Keywords=irc;chat;im;message;weechat;
EOF
chmod 644 "$PKG/usr/share/applications/weechat.desktop"
