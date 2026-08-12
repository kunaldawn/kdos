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

# source= is empty: this is ours, like kdos-splash and the tools. build()
# compiles straight out of $PORT_SRC.
#
# wlroots' pkg-config name carries the major.minor, and that is deliberate on
# their part — the API breaks every release, so a build that silently picked
# up a different one would be worse than a build that fails to find it.
# wayland-client is on this list even though kdos-comp never acts as a
# client: libwlroots itself pulls in wl_proxy_* for its nested backends, and
# without it the link fails on wl_proxy_get_queue. Measured, not guessed.
PKGCFG="wlroots-0.20 wayland-server wayland-client xkbcommon pixman-1"

# wlr_layer_shell_v1.h includes a wayland-scanner SERVER header that wlroots
# neither generates nor installs, so every consumer of layer-shell has to run
# the scanner itself. The XML comes from the wlroots port, which installs it to
# a fixed path for exactly this reason.
SCANNER="$(pkg-config --variable=wayland_scanner wayland-scanner)"
"$SCANNER" server-header \
	/usr/share/wlroots/protocols/wlr-layer-shell-unstable-v1.xml \
	wlr-layer-shell-unstable-v1-protocol.h

gcc $CFLAGS -O2 -Wall -Wextra \
	-DWLR_USE_UNSTABLE \
	-I. \
	$(pkg-config --cflags $PKGCFG) \
	-o kdos-comp "$PORT_SRC"/*.c \
	$(pkg-config --libs $PKGCFG) $LDFLAGS

install -Dm755 kdos-comp "$PKG/usr/bin/kdos-comp"
