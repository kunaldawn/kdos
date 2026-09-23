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

install -Dm644 cert.pem $PKG/etc/ssl/cert.pem

install -d $PKG/etc/ssl/certs
ln -s /etc/ssl/cert.pem $PKG/etc/ssl/certs/ca-certificates.crt
ln -s /etc/ssl/cert.pem $PKG/etc/ssl/ca-bundle.crt
