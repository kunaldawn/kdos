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

# EVERY SCRIPTING BACKEND IS A HARD CONFIGURE DEPENDENCY, not a feature that
# degrades. `find_package(... REQUIRED)` sits in each plugin's CMakeLists, so a
# backend switched on without its interpreter stops cmake before a line is
# compiled. Python, Perl and Lua are ports and are on; Guile, PHP and
# JavaScript (v8) are not ports.
#
# RUBY IS OFF ALTHOUGH THE PORT EXISTS. The plugin has not been built against
# Ruby 4.0, and turning it on puts a libruby link under the plugin module.
#
# TCL IS OFF ALTHOUGH THE PORT EXISTS, and the reason is the probe rather than
# the language. The tcl plugin asks cmake's own FindTCL, whose newest library
# name is `tcl8.7`; this image ships `libtcl9.0.so` with no unversioned link,
# so the probe finds the header and `tclsh` but no library, and weechat turns
# that into `Tcl not found` — a failed configure, not a missing plugin. The
# plugin's own sources compile clean against Tcl 9 headers, so an unversioned
# `libtcl.so` in the tcl port is all that stands between here and switching
# this to ON.
#
# LUA IS THE 5.5 LIBRARY. `pkg_search_module(LUA lua lua5.4 …)` takes `lua.pc`
# first, which is 5.5 here; the plugin uses no interface 5.5 dropped, and the
# -Werror-implicit-function-declaration this build sets is what would catch it
# if that changed.
#
# SPELL IS OFF because it is Aspell or Enchant and neither is a port —
# `ENABLE_SPELL=ON` reaches `find_package(Aspell REQUIRED)` and fails the same
# way a missing interpreter does.
#
# MAN IS ON AND DOC IS OFF. Both are asciidoctor, and a missing asciidoctor is
# SEND_ERROR, so the depends line carries it. MAN builds weechat.1 and
# weechat-headless.1 in English and each language upstream translates them
# into, installed under /usr/share/man/<lang>/man1; DOC is the HTML guides,
# and its autogen step loads every plugin, including the ones switched off
# here.
#
# NLS IS OFF: the translations would put a libintl link under the binary and
# under every plugin module for message catalogues nothing on this image
# selects, and `find_package(Intl REQUIRED)` makes that dependency mandatory
# rather than optional.
#
# HEADLESS IS ON AND IT IS THE BOUNCER. `weechat-headless` is the same client
# with no interface, which with the relay plugin is what keeps a connection up
# across logins; without it staying connected means a curses process parked in
# a multiplexer.
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
	-DENABLE_TCL=OFF \
	-DENABLE_RUBY=OFF \
	-DENABLE_GUILE=OFF \
	-DENABLE_PHP=OFF \
	-DENABLE_JAVASCRIPT=OFF \
	-DENABLE_SPELL=OFF \
	-DENABLE_ENCHANT=OFF
make
make DESTDIR=$PKG install
cd ..

# UPSTREAM'S ENTRY IS REPLACED, NOT KEPT. It carries `Exec=weechat %u` with
# `MimeType=x-scheme-handler/irc`, and a scheme handler is not a type in the
# shared MIME database — the opener chain reads `/usr/share/mime/globs`, where
# it never appears, so the key claims a handler nothing can route. Its
# `Categories=Network;Chat;IRCClient;` is upstream's own vocabulary rather than
# the registered one, and `Name=WeeChat` names the program where this menu
# names the role.
#
# Icon=weechat resolves through hicolor: the atlas leaves `apps/` out, and
# libkicon falls back to `icons/hicolor/*/apps/*.png` — which is exactly what
# this port installs.
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
