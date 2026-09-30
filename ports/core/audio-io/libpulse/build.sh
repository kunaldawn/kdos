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


# The client half of the PulseAudio tree and nothing else: libpulse,
# libpulse-simple, libpulse-mainloop-glib, pactl and pacat (with its paplay,
# parec, parecord and pamon links) and their pages. The server on this image is
# pipewire-pulse, which speaks the same protocol on $XDG_RUNTIME_DIR/pulse/native;
# -Ddaemon=true would install a second server that fights it for the socket.
#
# Every feature is named rather than left to `auto`, so a missing library fails
# the setup instead of producing a narrower libpulse. dbus lets a client
# notice a server that appears on the session bus; glib builds the GLib main-loop
# adapter most GUI consumers link. oss-output stays off: it builds padsp, an
# LD_PRELOAD shim that resolves its passthroughs by large-file alias names musl
# does not export. The daemon-only features are off because the daemon is.
#
# -lintl IS LINKED BY NAME. The tree has no switch to leave translation out, and
# its probe finds musl's own dgettext() and adds no library, while the gettext
# port's libintl.h renames every call to libintl_dgettext, which only the
# libintl port defines: without the flag the first program fails to link. It
# rides LDFLAGS rather than -Dc_link_args, which would replace the build
# environment's LDFLAGS instead of adding to it.
LDFLAGS="$LDFLAGS -lintl" meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib --localstatedir=/var \
	-Dbuildtype=release \
	-Ddaemon=false \
	-Dclient=true \
	-Ddoxygen=false \
	-Dman=true \
	-Dtests=false \
	-Dgcov=false \
	-Dipv6=true \
	-Ddbus=enabled \
	-Dglib=enabled \
	-Dasyncns=disabled \
	-Dgtk=disabled \
	-Dx11=disabled \
	-Dsystemd=disabled \
	-Doss-output=disabled \
	-Dfftw=disabled \
	-Dvalgrind=disabled \
	-Dalsa=disabled \
	-Davahi=disabled \
	-Dbluez5=disabled \
	-Dbluez5-gstreamer=disabled \
	-Dconsolekit=disabled \
	-Delogind=disabled \
	-Dgsettings=disabled \
	-Dgstreamer=disabled \
	-Djack=disabled \
	-Dlirc=disabled \
	-Dopenssl=disabled \
	-Dorc=disabled \
	-Dsamplerate=disabled \
	-Dsoxr=disabled \
	-Dspeex=disabled \
	-Dtcpwrap=disabled \
	-Dudev=disabled \
	-Dwebrtc-aec=disabled \
	-Dbashcompletiondir=/usr/share/bash-completion/completions \
	-Dzshcompletiondir=/usr/share/zsh/site-functions
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
