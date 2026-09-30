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

# The bundle is converted here from certdata.txt at the NSS release tag in
# `version`, by the converter that produces curl's cacert.pem, taken from the
# curl release in `_curl` and run with its defaults: the CAs NSS trusts to
# issue server certificates. -n converts the local certdata.txt instead of
# downloading one; -d is the URL the bundle's header names as its source.
#
# The converter drops every certificate already expired when it runs, so the
# bundle is a function of the tag and of the build date: a rebuild after a
# root's expiry ships one certificate fewer. Its "as of" header line is the
# modification time of certdata.txt, which is when the build copied it in, and
# is dropped so the file does not change with every build.
mv certdata-$version.txt certdata.txt
perl mk-ca-bundle-$_curl.pl -n -f -q \
	-d https://raw.githubusercontent.com/nss-dev/nss/$_tag/lib/ckfw/builtins/certdata.txt \
	cert.pem.raw
grep -v '^## Certificate data from Mozilla ' cert.pem.raw > cert.pem
grep -q 'BEGIN CERTIFICATE' cert.pem

# The Mozilla bundle is the package's; /etc/ssl/cert.pem, the file every
# OpenSSL consumer reads, is not. update-ca-certificates writes it from the
# bundle plus every local root in /etc/ca-certificates/trust-source/anchors/,
# and postinstall.sh runs it, so an upgrade of this package keeps the local
# roots rather than replacing the file they were added to. GnuTLS reads the
# bundle and the anchors directory through p11-kit directly and needs no
# regeneration. The anchors directory is the first one `caddy trust` finds,
# and after writing a root there it runs `trust extract-compat`, which p11-kit
# hands to this tool.
install -Dm644 cert.pem $PKG/usr/share/ca-certificates/mozilla.pem
install -d $PKG/etc/ca-certificates/trust-source/anchors

install -d $PKG/etc/ssl/certs
ln -s /etc/ssl/cert.pem $PKG/etc/ssl/certs/ca-certificates.crt
ln -s /etc/ssl/cert.pem $PKG/etc/ssl/ca-bundle.crt

# Only the certificate blocks of an anchor are copied, so a PEM that also holds
# a private key does not publish it in a world-readable bundle. A DER file has
# no such block and is skipped with a warning: GnuTLS would trust it through
# p11-kit and OpenSSL would not, and the warning is the only place that shows.
# The new bundle is written beside the old and renamed over it, so a reader
# never sees a half-written file.
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
