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

# matplotlib 3.11 REQUIRES this and fetches it otherwise. Its subprojects/
# carries wraps for libraqm, harfbuzz and sheenbidi, so a build with no
# `-Dsystem-libraqm=true` downloads all three — and harfbuzz and fribidi are
# already ports, so the vendored set would be a second copy of each.
#
# fribidi rather than SheenBidi: it is the port this tree has, and libraqm
# treats the two as interchangeable bidi backends.
meson setup build --prefix=/usr --libdir=lib --buildtype=release \
	-Ddocs=false -Dtests=false -Dsheenbidi=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
