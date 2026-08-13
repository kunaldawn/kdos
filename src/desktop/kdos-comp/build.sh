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
#
# glesv2 and egl are here for the CRT pass: wlroots has no shader API, so
# crt.c does the final blit itself with a GLES2 program of its own. Only the
# headers and the two libraries — no GL loader, no helper toolkit.
# libpng is for the wallpaper (wallpaper.c): the compositor decodes the image
# and puts it in the scene itself rather than shipping a separate layer-shell
# client for it — libkwl paints cells, so a wallpaper client would be the one
# program in this desktop that is not a character grid.
PKGCFG="wlroots-0.20 wayland-server wayland-client xkbcommon pixman-1 glesv2 egl libpng libdrm fcft"

# libkcolor: the phosphor the shader tints with is the same KCOL_SCHEMES entry
# the boot splash, the TTY palette, the icons and the GTK stylesheet expand.
# libkbase comes with it — kcol_retint_text allocates through kb_calloc. Both
# link nothing but musl, so they cost the compositor no dependency.
LIBS="$PORT_SRC/../../libs"

# wlr_layer_shell_v1.h includes a wayland-scanner SERVER header that wlroots
# neither generates nor installs, so every consumer of layer-shell has to run
# the scanner itself. The XML comes from the wlroots port, which installs it to
# a fixed path for exactly this reason.
SCANNER="$(pkg-config --variable=wayland_scanner wayland-scanner)"
"$SCANNER" server-header \
	/usr/share/wlroots/protocols/wlr-layer-shell-unstable-v1.xml \
	wlr-layer-shell-unstable-v1-protocol.h

# -D_GNU_SOURCE is libkbase's requirement, not ours: kb_landlock.c wants O_PATH.
gcc $CFLAGS -O2 -Wall -Wextra \
	-DWLR_USE_UNSTABLE -D_GNU_SOURCE \
	-I. -I"$LIBS/libkcolor" -I"$LIBS/libkbase" \
	-I"$LIBS/libktui" -I"$LIBS/libkcell" \
	$(pkg-config --cflags $PKGCFG) \
	-o kdos-comp "$PORT_SRC"/*.c "$LIBS"/libkcolor/*.c "$LIBS"/libkbase/*.c \
	"$LIBS"/libktui/*.c "$LIBS"/libkcell/*.c \
	$(pkg-config --libs $PKGCFG) $LDFLAGS

install -Dm755 kdos-comp "$PKG/usr/bin/kdos-comp"
