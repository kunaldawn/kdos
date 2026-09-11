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

# source= is empty: this is ours. It compiles out of $PORT_SRC together with
# libkwl and libktui, which are static archives in the tree rather than
# installed libraries.
LIBS="$PORT_SRC/../../libs"

SCANNER="$(pkg-config --variable=wayland_scanner wayland-scanner)"
PROTO="$(pkg-config --variable=pkgdatadir wayland-protocols)"

# Four protocol definitions from three places, which is the usual state of
# Wayland: xdg-shell is stable and in wayland-protocols, ext-workspace and
# ext-session-lock are staging and also there, and layer-shell and
# foreign-toplevel are wlroots' own — installed by the wlroots port precisely
# because upstream does not.
# cursor-shape: libkwl sets the pointer shape on enter, or the cursor
# vanishes over every chrome surface. The tablet protocol comes along because
# cursor-shape-v1's generated code references zwp_tablet_tool_v2.
"$SCANNER" client-header "$PROTO/staging/cursor-shape/cursor-shape-v1.xml" \
	cursor-shape-v1-client-protocol.h
"$SCANNER" private-code  "$PROTO/staging/cursor-shape/cursor-shape-v1.xml" \
	cursor-shape-v1-protocol.c
"$SCANNER" private-code  "$PROTO/unstable/tablet/tablet-unstable-v2.xml" \
	tablet-unstable-v2-protocol.c
# primary-selection: middle-click paste is a SECOND selection with its own
# device manager, and libkwl includes the header unconditionally. The
# private-code is what carries the zwp_primary_selection_* interface symbols
# the link needs; generating only the header builds and then fails at link.
"$SCANNER" client-header \
	"$PROTO/unstable/primary-selection/primary-selection-unstable-v1.xml" \
	primary-selection-unstable-v1-client-protocol.h
"$SCANNER" private-code \
	"$PROTO/unstable/primary-selection/primary-selection-unstable-v1.xml" \
	primary-selection-unstable-v1-protocol.c
"$SCANNER" client-header "$PROTO/stable/xdg-shell/xdg-shell.xml" \
	xdg-shell-client-protocol.h
"$SCANNER" private-code  "$PROTO/stable/xdg-shell/xdg-shell.xml" \
	xdg-shell-protocol.c
"$SCANNER" client-header \
	"$PROTO/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml" \
	xdg-decoration-unstable-v1-client-protocol.h
"$SCANNER" private-code \
	"$PROTO/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml" \
	xdg-decoration-unstable-v1-protocol.c
"$SCANNER" client-header "$PROTO/staging/ext-workspace/ext-workspace-v1.xml" \
	ext-workspace-v1-client-protocol.h
"$SCANNER" private-code  "$PROTO/staging/ext-workspace/ext-workspace-v1.xml" \
	ext-workspace-v1-protocol.c
# The panel is not a lock screen and binds no lock role — but it compiles the
# WHOLE of libkwl, and libkwl carries KDISP_ROLE_LOCK for kdos-lock, so kwl.c
# includes this header unconditionally. Generating it only in kdos-lock's
# recipe is what made kdos-shell the first package of this phase to fail.
"$SCANNER" client-header \
	"$PROTO/staging/ext-session-lock/ext-session-lock-v1.xml" \
	ext-session-lock-v1-client-protocol.h
"$SCANNER" private-code \
	"$PROTO/staging/ext-session-lock/ext-session-lock-v1.xml" \
	ext-session-lock-v1-protocol.c
# wlr-output-management is kdos-display's: labwc takes its screen configuration
# over it like every wlroots compositor, and nothing in this tree spoke it, so
# there was no way at all to set a mode, a scale or a second monitor's place.
# wlr-data-control is kdos-clip's, and it is the only protocol that can carry a
# clipboard HISTORY: wl_data_device delivers a selection event solely to the
# client with keyboard focus, so a manager built on it records nothing.
for p in wlr-layer-shell-unstable-v1 wlr-foreign-toplevel-management-unstable-v1 \
	 wlr-output-management-unstable-v1 wlr-data-control-unstable-v1; do
	"$SCANNER" client-header "/usr/share/wlroots/protocols/$p.xml" \
		"$p-client-protocol.h"
	"$SCANNER" private-code  "/usr/share/wlroots/protocols/$p.xml" \
		"$p-protocol.c"
done

# libpng is libkicon's: the icon layer decodes the alien apps' own PNGs out of
# /usr/share/icons/hicolor and the theme's atlas, and there is no SVG parser
# anywhere in this tree.
#
# libjpeg, libwebp and libarchive are kdos-peek's, through libkimg and its own
# listing: the viewer decodes a picture or a rendered page with the one decoder
# this tree has, and asks libarchive whether a file is an archive by opening it.
# No libsixel — a file on disk is not an escape sequence, and kdos-term is where
# sixel arrives.
#
# libkvt is linked for ONE reason and brings no dependency: it links musl and
# nothing else. kdos-desk's console background is a text file with SGR colour in
# it, which is exactly what a program writes to a terminal, so the parser that
# reads it is the parser that reads a terminal — a private SGR reader in this
# binary would be a second answer to what an escape means.
PKGCFG="fcft pixman-1 xkbcommon wayland-client basu alsa libpipewire-0.3 libpng libjpeg libwebp libnsgif libarchive"

gcc $CFLAGS -O2 -std=gnu11 -D_GNU_SOURCE -Wall -Wextra \
	-DKIMG_HAVE_PNG -DKIMG_HAVE_JPEG -DKIMG_HAVE_WEBP -DKIMG_HAVE_GIF \
	-I. -I"$PORT_SRC" \
	-I"$LIBS/libkbase" -I"$LIBS/libktui" -I"$LIBS/libkcolor" -I"$LIBS/libkcell" -I"$LIBS/libkwl" -I"$LIBS/libkdisp" -I"$LIBS/libkcon" -I"$LIBS/libkwm" \
	-I"$LIBS/libkxdg" -I"$LIBS/libkicon" -I"$LIBS/libkchrome" \
	-I"$LIBS/libkimg" \
	-I"$LIBS/libkvt" \
	-I"$LIBS/libkproc" \
	$(pkg-config --cflags $PKGCFG) \
	-o kdos-shell \
	"$PORT_SRC"/*.c \
	"$LIBS"/libkwl/*.c "$LIBS"/libkdisp/*.c "$LIBS"/libkcon/*.c "$LIBS"/libkwm/*.c "$LIBS"/libkcell/*.c "$LIBS"/libktui/*.c "$LIBS"/libkcolor/*.c \
	"$LIBS"/libkbase/*.c "$LIBS"/libkxdg/*.c "$LIBS"/libkicon/*.c \
	"$LIBS"/libkchrome/*.c "$LIBS"/libkproc/*.c "$LIBS"/libkimg/*.c \
	"$LIBS"/libkvt/*.c \
	./*-protocol.c \
	$(pkg-config --libs $PKGCFG) $LDFLAGS

gcc $CFLAGS -O2 -std=gnu11 -D_GNU_SOURCE -Wall -Wextra \
	-I"$PORT_SRC" -o mkcharidx "$PORT_SRC/tools/mkcharidx.c" \
	$(pkg-config --cflags --libs icu-uc) $LDFLAGS
./mkcharidx charnames.idx
install -Dm644 charnames.idx "$PKG/usr/share/kdos/charnames.idx"

install -Dm755 kdos-shell "$PKG/usr/bin/kdos-shell"
# Dispatched on its own basename, so the launcher is a link rather than a
# second binary. The skel rc.xml's W-d keybind Executes it by that name.
ln -s kdos-shell "$PKG/usr/bin/kdos-mediad"
ln -s kdos-shell "$PKG/usr/bin/kdos-launcher"
ln -s kdos-shell "$PKG/usr/bin/kdos-menu"
ln -s kdos-shell "$PKG/usr/bin/kdos-desk"
ln -s kdos-shell "$PKG/usr/bin/kdos-pick"
ln -s kdos-shell "$PKG/usr/bin/kdos-peek"
ln -s kdos-shell "$PKG/usr/bin/kdos-find"
ln -s kdos-shell "$PKG/usr/bin/kdos-pix"
# The recorder forks `sox` and `whisper-cli` by name, which is why neither is
# a pkg-config entry above — the same shape kdos-calc uses for qalc.
ln -s kdos-shell "$PKG/usr/bin/kdos-rec"
ln -s kdos-shell "$PKG/usr/bin/kdos-ascii"
# What this machine is, read out of /proc and /etc rather than fetched by a
# second program: an About window that shelled out to a screenfetch would
# draw somebody else's colours on a surface that paints in slots.
# One search over everything the desktop can reach: windows, applications,
# routes, settings pages, files and chords, ranked by the one matcher in
# libkbase. `--apps` is the launcher, which is why that is a flag and not a
# second program with a second idea of ranking.
ln -s kdos-shell "$PKG/usr/bin/kdos-palette"
ln -s kdos-shell "$PKG/usr/bin/kdos-about"
# HOW THE SCREEN LOOKS: the accent on one page and the screen's font on the
# other. Each accent row is drawn in the scheme it names, which is the one place
# on this desktop where a cell may carry a literal colour — a swatch taken from
# the palette in force would show seven identical rows.
#
# `kdos-style` AND NOT `kdos-theme`. `kdos-theme` is the artwork GENERATOR, a
# port of its own that `kdos theme` runs as `kdos-theme gtk|icons|cursors`, and
# two packages installing one path means one of the two programs is whichever
# was written last — which for a whole release was this one, so the chord, the
# route and the settings row all opened a command-line generator and printed a
# usage line. One name, one program.
ln -s kdos-shell "$PKG/usr/bin/kdos-style"
# The desk accessory Sidekick had. It forks `qalc` rather than linking
# libqalculate, which is C++ and would put libstdc++ on the panel package.
ln -s kdos-shell "$PKG/usr/bin/kdos-calc"
# The character map. The index it searches is built HERE, once, by a program
# that links ICU — kdos-shell does not and must not: ICU is thirty megabytes
# of library and data, and every one of the thirty surfaces this binary is
# would carry it. The names do not change between builds of an image.
ln -s kdos-shell "$PKG/usr/bin/kdos-chars"
# Sidekick's fifth accessory dialed a modem; its descendant is a lookup. The
# store is `khard` and this holds none — a second vCard parser would be a
# second answer to what a contact is, and the one that is not the store's is
# the one that goes stale.
ln -s kdos-shell "$PKG/usr/bin/kdos-contacts"
# The disks window. Every privileged operation on it is a kdos-mountd verb and
# this binary opens no block device: what it does is draw a list the daemon
# published and send back a row number. Partitioning is `cfdisk` in a terminal
# and is not reimplemented.
ln -s kdos-shell "$PKG/usr/bin/kdos-disks"
# Printers, over `lpstat`, `lpinfo` and `lpadmin` rather than libcups: those
# three are what the CUPS documentation tells a person to type and are the
# interface upstream keeps stable. The `lpadmin` GROUP is the authority CUPS
# itself defines, so this needs no daemon of ours in front of it.
ln -s kdos-shell "$PKG/usr/bin/kdos-print"
# The zone and the clock. The list is `zone1970.tab`, which tzdata ships;
# setting it is a kdos-powerd verb, because /etc/localtime is root's.
ln -s kdos-shell "$PKG/usr/bin/kdos-time"
# The accounts. Reading /etc/passwd is anybody's and creating an account is
# root's, so this reads and does not write — except the autologin, which is a
# KDOS file and goes through the same wheel-gated daemon the power verbs do.
ln -s kdos-shell "$PKG/usr/bin/kdos-users"
# What is behind and what is vulnerable. It computes neither: `kdos update
# check --json` and `kdos cve --json` already answer, and a surface that
# re-derived a version comparison would be a second answer that drifts.
ln -s kdos-shell "$PKG/usr/bin/kdos-update"
# Which services answer the network. It carries no table of ports: a client
# that could name a port could open any port, so kdos-powerd owns the names
# and this asks for them.
ln -s kdos-shell "$PKG/usr/bin/kdos-firewall"
# What is in the restic repository, and one key to add to it. It restores
# nothing: `restic restore` is the operation you do once under pressure and it
# wants the full command rather than a button whose defaults you cannot see.
ln -s kdos-shell "$PKG/usr/bin/kdos-backup"
ln -s kdos-shell "$PKG/usr/bin/kdos-note"
ln -s kdos-shell "$PKG/usr/bin/kdos-run"
# kdos-comp's <core><promptCommand> — the yes/no dialog labwc's If/prompt
# action needs. Upstream's labnag is not built (-Dlabnag=disabled).
ln -s kdos-shell "$PKG/usr/bin/kdos-prompt"
ln -s kdos-shell "$PKG/usr/bin/kdos-notifyd"
# The NetworkManager secret agent. NetworkManager never prompts: without an
# agent registered on the SYSTEM bus the only passphrase this desktop can use
# is one written into the profile when it was created, and a changed key,
# 802.1X and a VPN one-time code are all a silent activation failure.
ln -s kdos-shell "$PKG/usr/bin/kdos-netagent"
# The notification CENTRE: the daemon keeps a history and this draws it. A
# notification that expired is not a notification that was read, and on a
# desktop whose fat applications all live in containers a boxed app's toast is
# often the only thing that says its work has finished.
ln -s kdos-shell "$PKG/usr/bin/kdos-notify"
ln -s kdos-shell "$PKG/usr/bin/kdos-osd"
# The clock's other half, and the screens. The panel spawns kdos-cal by name.
ln -s kdos-shell "$PKG/usr/bin/kdos-cal"
ln -s kdos-shell "$PKG/usr/bin/kdos-display"
# The keybind card is generated from the same rc.xml the compositor reads, so
# help cannot drift from what the keys actually do; W-F1 opens it.
ln -s kdos-shell "$PKG/usr/bin/kdos-keys"
ln -s kdos-shell "$PKG/usr/bin/kdos-teams"
ln -s kdos-shell "$PKG/usr/bin/kdos-saver"
ln -s kdos-shell "$PKG/usr/bin/kdos-slit"
ln -s kdos-shell "$PKG/usr/bin/kdos-doc"
ln -s kdos-shell "$PKG/usr/bin/kdos-settings"
ln -s kdos-shell "$PKG/usr/bin/kdos-openwith"
ln -s kdos-shell "$PKG/usr/bin/kdos-audio"
# The Start menu — the panel's own button, and the one front door this desktop
# did not have. kdos-menu and kdos-launcher keep the jobs they are good at.
ln -s kdos-shell "$PKG/usr/bin/kdos-start"
# The device managers. Everything they talk to (NetworkManager, bluez, V4L2)
# has been running here since before there was a desktop; what was missing was
# a surface, and `foot -e nmtui` was it.
ln -s kdos-shell "$PKG/usr/bin/kdos-net"
ln -s kdos-shell "$PKG/usr/bin/kdos-bt"
ln -s kdos-shell "$PKG/usr/bin/kdos-devices"
# The clipboard history: the daemon the compositor supervises, and the picker
# `W-v` opens. One binary, one name, two roles.
ln -s kdos-shell "$PKG/usr/bin/kdos-clip"
# The way BACK out of the trash. `kdos trash` puts a file in and lists what is
# there; this is the surface that takes one out again.
ln -s kdos-shell "$PKG/usr/bin/kdos-trash"
# The notification area's overflow: the occasional widgets live behind one
# chevron of fixed width so the right wing stops changing size, and this is
# what the chevron opens. It also runs `kdos stutter` and `kdos-energy` inside
# a pane, which is what those two had instead of a surface.
ln -s kdos-shell "$PKG/usr/bin/kdos-status"
# The tooltip. Half the bar is 32-pixel pictures with no words beside them, and
# a control nobody can name is a control nobody clicks.
ln -s kdos-shell "$PKG/usr/bin/kdos-tip"
# The input-method candidate window, drawn as cells rather than by the engine's
# own toolkit. It owns org.kde.impanel and speaks kimpanel.
ln -s kdos-shell "$PKG/usr/bin/kdos-ime"
