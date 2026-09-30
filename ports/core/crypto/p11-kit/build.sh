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

# trust_paths names the Mozilla bundle ca-certificates lays down, then the
# local trust source. A trust path that is a plain file — not a directory — is
# loaded whole and every certificate in it becomes an anchor. A directory is
# not an anchor itself: only the certificates in its anchors/ subdirectory are,
# so a local root goes in /etc/ca-certificates/trust-source/anchors/ and is
# read live, with nothing to regenerate. gnutls is built with
# --with-default-trust-store-pkcs11, so this string is the only source of
# anchors it has: point it at a path the image does not ship and every gnutls
# consumer verifies against an empty store and rejects every peer.
meson setup build \
	--prefix=/usr --libdir=lib --sysconfdir=/etc \
	--buildtype=release \
	-D trust_module=enabled \
	-D trust_paths=/usr/share/ca-certificates/mozilla.pem:/etc/ca-certificates/trust-source \
	-D module_path=/usr/lib/pkcs11 \
	-D libffi=enabled \
	-D systemd=disabled \
	-D bash_completion=enabled \
	-D zsh_completion=enabled \
	-D nls=false \
	-D man=true
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

# `trust extract-compat` is what a tool runs after writing a root into the
# trust source — `caddy trust` does. Upstream installs a placeholder that
# fails; here it regenerates the OpenSSL bundle, the one consumer that does not
# read the trust source live.
cat > "$PKG/usr/libexec/p11-kit/trust-extract-compat" <<'SH'
#!/bin/sh
if [ $# -ne 0 ]; then
	echo "usage: trust extract-compat" >&2
	exit 2
fi
exec /usr/bin/update-ca-certificates
SH
chmod 755 "$PKG/usr/libexec/p11-kit/trust-extract-compat"
