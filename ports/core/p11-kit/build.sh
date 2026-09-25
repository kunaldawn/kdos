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

export XML_CATALOG_FILES=/etc/xml/catalog

# trust_paths names the CA bundle ca-certificates lays down. A trust path
# that is a plain file — not a directory — is loaded whole and every
# certificate in it becomes an anchor, so there is no anchors/ directory to
# populate and nothing to regenerate after an update. gnutls is built with
# --with-default-trust-store-pkcs11, so this string is the only source of
# anchors it has: point it at a path the image does not ship and every
# gnutls consumer verifies against an empty store and rejects every peer.
meson setup build \
	--prefix=/usr --libdir=lib --sysconfdir=/etc \
	--buildtype=release \
	-D trust_module=enabled \
	-D trust_paths=/etc/ssl/cert.pem \
	-D module_path=/usr/lib/pkcs11 \
	-D libffi=enabled \
	-D systemd=disabled \
	-D bash_completion=enabled \
	-D zsh_completion=enabled \
	-D nls=false \
	-D man=true
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
