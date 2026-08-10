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

# source="" — kpkg skips the extract loop and this builds out of $PORT_SRC,
# the same shape as kdos-splash. The autotools layer upstream shipped is gone;
# src/aconfig.h states what configure used to probe for.
#
# -I$PORT_SRC/../kdos-splash: penguin.h is INCLUDED from the splash package,
# not copied here. One mascot array, three consumers — the splash, the login
# banner's generator and this — so they cannot drift apart.
gcc $CFLAGS -O2 -Wno-implicit-function-declaration \
	-I"$PORT_SRC/src" -I"$PORT_SRC/../kdos-splash" \
	-DHAVE_LIBMIKMOD \
	-o kk "$PORT_SRC"/src/*.c \
	-laa -lmikmod -lm -lncurses $LDFLAGS

install -Dm755 kk "$PKG/usr/bin/kk"

# SOUNDDIR in src/aconfig.h. Upstream made the modules conditional on libmikmod
# having been found at configure time, which is exactly how a demo ships mute.
for m in "$PORT_SRC"/music/*.xm; do
	install -Dm644 "$m" "$PKG/usr/share/kk/$(basename "$m")"
done
install -Dm644 "$PORT_SRC/music/MUSIC.credits" "$PKG/usr/share/kk/MUSIC.credits"
install -Dm644 "$PORT_SRC/LICENSE.notice" "$PKG/usr/share/licenses/kk/LICENSE.notice"
install -Dm644 "$PORT_SRC/COPYING" "$PKG/usr/share/licenses/kk/COPYING"
