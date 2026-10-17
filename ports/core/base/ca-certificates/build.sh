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

mv certdata-$version.txt certdata.txt
perl mk-ca-bundle-$_curl.pl -n -f -q \
	-d https://raw.githubusercontent.com/nss-dev/nss/$_tag/lib/ckfw/builtins/certdata.txt \
	cert.pem.raw
grep -v '^## Certificate data from Mozilla ' cert.pem.raw > cert.pem
grep -q 'BEGIN CERTIFICATE' cert.pem

install -Dm644 cert.pem $PKG/usr/share/ca-certificates/mozilla.pem
install -d $PKG/etc/ca-certificates/trust-source/anchors

install -d $PKG/etc/ssl/certs
ln -s /etc/ssl/cert.pem $PKG/etc/ssl/certs/ca-certificates.crt
ln -s /etc/ssl/cert.pem $PKG/etc/ssl/ca-bundle.crt

install -d $PKG/usr/bin
cat > $PKG/usr/bin/update-ca-certificates <<'SH'
#!/bin/sh
# update-ca-certificates [--root DIR]
#
# Writes /etc/ssl/cert.pem: the Mozilla bundle, then every PEM certificate in
# /etc/ca-certificates/trust-source/anchors/. Run it after adding or removing
# a local root there.
set -e
usage() { echo "usage: update-ca-certificates [--root DIR]" >&2; exit 2; }
root=
case "$#:$1" in
0:) ;;
2:--root) root=${2%/} ;;
*) usage ;;
esac

bundle=$root/usr/share/ca-certificates/mozilla.pem
anchors=$root/etc/ca-certificates/trust-source/anchors
out=$root/etc/ssl/cert.pem
tmp=$out.new.$$

[ -s "$bundle" ] || { echo "update-ca-certificates: $bundle is missing" >&2; exit 1; }
mkdir -p "${out%/*}"
trap 'rm -f "$tmp"' EXIT
umask 022
n=0
{
	cat "$bundle"
	for f in "$anchors"/*; do
		[ -f "$f" ] || continue
		if grep -q -e '-----BEGIN CERTIFICATE-----' "$f"; then
			printf '\n# Local anchor: %s\n' "${f##*/}"
			sed -n '/-----BEGIN CERTIFICATE-----/,/-----END CERTIFICATE-----/p' "$f"
			n=$((n + 1))
		else
			echo "update-ca-certificates: $f holds no PEM certificate, skipped" >&2
		fi
	done
} > "$tmp"
mv -f "$tmp" "$out"
trap - EXIT
echo "update-ca-certificates: $out: the Mozilla bundle and $n local anchor file(s)"
SH
chmod 755 $PKG/usr/bin/update-ca-certificates
