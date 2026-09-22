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

# Every feature is stated, never left on `auto`: each of the three is a
# dependency this recipe declares, and an auto probe that misses one produces
# a wayvnc that builds green and is silently narrower than the recipe says —
# no GPU capture path, no system-password login, no man pages.
meson setup build \
	--prefix=/usr --libdir=lib --sysconfdir=/etc \
	--buildtype=release \
	-Dscreencopy-dmabuf=enabled \
	-Dpam=enabled \
	-Dman-pages=enabled \
	-Dtests=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

# PAM authenticates against the service name `wayvnc`, and a missing service
# file is not a fallback: pam_start() fails and every login is refused. meson
# installs nothing for it, so the package owns the file.
#
# pam_unix's option table holds `nodelay` and not `deny=` or `unlock_time=`,
# which belong to pam_faillock: pam_unix logs "unrecognized option" for each
# and authenticates anyway. A policy carrying them reads as a three-strike
# lockout on a service that hands out the desktop over TCP and enforces none,
# so the stack names only what the module implements.
#
# `nodelay` is required rather than a relaxation: pam_unix's fail delay sleeps
# inside wayvnc's single aml event loop, so one wrong password stalls every
# other client for two seconds.
#
# pam_unix directly, never `include system-auth` — that stack carries `nullok`
# and would admit a passwordless account from the network. The two phases are
# the two wayvnc calls, pam_authenticate and pam_acct_mgmt; it opens no session.
install -d -m 755 "$PKG/etc/pam.d"
cat > "$PKG/etc/pam.d/wayvnc" << "EOF"
auth    required pam_unix.so nodelay
account required pam_unix.so
EOF
chmod 644 "$PKG/etc/pam.d/wayvnc"
