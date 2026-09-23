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


# GnuTLS AND NOT OpenSSL. openconnect's DTLS support against Cisco's
# non-standard handshake is the better-tested path on GnuTLS, and gnutls is
# already the image's second TLS stack rather than a new one.
#
# THE SCRIPT IS NAMED BY ABSOLUTE PATH. openconnect execs whatever
# --with-vpnc-script says on every connect; leaving it to the default points
# it at /etc/vpnc/vpnc-script, which nothing installs.
#
# --without-openssl-version-check: the check rejects any OpenSSL it was not
# taught about, and this build does not use OpenSSL at all.
#
# THE SSO BROWSER IS NAMED BY ABSOLUTE PATH TOO. configure otherwise searches
# PATH for xdg-open at build time and compiles in whatever it found, so SAML
# login on AnyConnect, GlobalProtect and Pulse would depend on whether
# xdg-utils happened to be installed first.
#
# --with-gssapi and --with-gnutls-tss2=tss2-esys fail configure when krb5 or
# tpm2-tss is missing, which is the point: Kerberos proxy authentication and
# TPM-held keys are otherwise dropped without a word. p11-kit (PKCS#11
# tokens), pcsc-lite (YubiKey OATH) and nettle (the HPKE exchange of Cisco's
# external-browser login) have no switch that fails, so the depends line is
# what keeps them. libproxy, stoken and libpskc are not built here and are
# pinned off, and json-parser is not a port, so the bundled copy is named.
./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
	--disable-static \
	--with-gnutls \
	--without-openssl \
	--with-gssapi \
	--with-gnutls-tss2=tss2-esys \
	--with-libpcsclite \
	--without-stoken \
	--without-libproxy \
	--without-libpskc \
	--with-builtin-json \
	--with-external-browser=/usr/bin/xdg-open \
	--with-vpnc-script=/usr/share/vpnc-scripts/vpnc-script \
	--disable-nls
make
make DESTDIR=$PKG install
