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

# TLS is glib-networking's, loaded as a GIO module at run time, so it is a
# runtime dependency the build cannot see: tls_check would compile a probe
# against whatever GIO modules the chroot happens to hold. The depends line
# carries it instead, and without it every https request fails at the
# handshake with "TLS/SSL support not available".
#
# NTLM is off because it is a helper, `ntlm_auth` from Samba, and that is not a
# port. GSSAPI is on: krb5 is a port and Negotiate is what a Kerberos realm's
# web services answer with. Every other option is named so the result does not
# depend on what the chroot happened to hold.
meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release \
	-Dgssapi=enabled \
	-Dntlm=disabled \
	-Dbrotli=enabled \
	-Dtls_check=false \
	-Dintrospection=disabled \
	-Dvapi=disabled \
	-Ddocs=disabled \
	-Dtests=false \
	-Dautobahn=disabled \
	-Dinstalled_tests=false \
	-Dsysprof=disabled \
	-Dfuzzing=disabled \
	-Dpkcs11_tests=disabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
