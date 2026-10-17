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

cat > "$PKG/usr/libexec/p11-kit/trust-extract-compat" <<'SH'
#!/bin/sh
if [ $# -ne 0 ]; then
	echo "usage: trust extract-compat" >&2
	exit 2
fi
exec /usr/bin/update-ca-certificates
SH
chmod 755 "$PKG/usr/libexec/p11-kit/trust-extract-compat"
