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

tar -xf "$PORT_SRC/$name-$version.tar.zst" --strip-components=1

# The pinyinhelper module `file(DOWNLOAD)`s two data sets at BUILD time — the
# pinyin-to-character table and the stroke table — which under `--network none`
# is a failed hostname lookup and a dead build. Same shape as libime's three
# data tarballs and the same fix: the port carries them, and Fcitx5Download's
# cmake skips the fetch when the file is already where it would have put it.
# Copied rather than extracted because the tarball is what the hash check
# downstream of the fetch looks at.
cp "$PORT_SRC/$_pytable" "$PORT_SRC/$_pystroke" modules/pinyinhelper/

# ENABLE_GUI is the configuration tool and it is Qt — Qt Widgets, and with
# ENABLE_BROWSER also QtWebEngine. Both are off because there is no Qt on this
# host; fcitx5's own config file is text and `fcitx5-configtool` was never going
# to run here anyway.
#
# ENABLE_CLOUDPINYIN is off for a different reason and it is not about size:
# cloud pinyin sends what you are typing to a remote service to be completed.
# That is a keystroke leaving the machine, on a distro that builds offline and
# ships an appbox so nothing has to phone home.
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D ENABLE_GUI=Off \
	-D ENABLE_BROWSER=Off \
	-D ENABLE_CLOUDPINYIN=Off \
	-D ENABLE_OPENCC=On \
	-D ENABLE_TEST=Off \
	-D ENABLE_DATA=On \
	-D ENABLE_TOOLS=On \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build
