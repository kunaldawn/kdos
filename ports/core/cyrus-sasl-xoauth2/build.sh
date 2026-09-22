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

# THE MECHANISM CYRUS-SASL DOES NOT SHIP. Upstream 2.1.28 carries anonymous,
# cram, digest, gs2, gssapi, login, ntlm, otp, plain, scram, srp and the
# server-side backends — and no XOAUTH2, which is what a provider that has
# withdrawn application passwords now requires. Without this plugin the library
# is installed, mbsync links it, and the account still cannot authenticate.
#
# XOAUTH2 AND NOT OAUTHBEARER. This plugin implements one mechanism; a server
# offering only OAUTHBEARER is still out of reach from mbsync, and aerc — which
# speaks it in its own Go code — is where such an account is read.
#
# THE TOKEN ARRIVES AS THE PASSWORD. The plugin asks SASL for SASL_CB_PASS and
# wraps whatever it gets as `auth=Bearer <token>`, and mbsync fills that
# callback from the account's `PassCmd` — so `PassCmd "pizauth show <account>"`
# is the whole of the configuration and no part of it is here.

# A RELEASE TARBALL WITH NO configure IN IT, so the build system is generated.
# Upstream's autogen.sh, verbatim and for its reasons: `automake --foreign`
# because the tree carries no AUTHORS, ChangeLog or NEWS and the GNU strictness
# automake defaults to refuses it, and `install -d m4` because aclocal is told
# to read a directory that AC_CONFIG_MACRO_DIR names and the tarball omits.
libtoolize
install -d m4
aclocal -I m4
autoheader
automake -c -a --foreign
autoconf

# --with-cyrus-sasl DECIDES WHERE THE PLUGIN IS INSTALLED as well as where the
# header is found: Makefile.am sets `pkglibdir = ${CYRUS_SASL_PREFIX}/lib/sasl2`.
# /usr is its default and is correct here, and it is passed anyway so the two
# halves of that sentence are visible at the call site.
#
# NO STATIC ARCHIVE. A SASL plugin is opened with dlopen() by the library's own
# scan of /usr/lib/sasl2; a `.a` beside it is a file nothing can ever load.
./configure \
	--prefix=/usr \
	--with-cyrus-sasl=/usr \
	--disable-static

make
make DESTDIR=$PKG install

# libtool's archive describes a link nothing performs — the plugin is dlopen'd
# by name. Left behind it is the only file in /usr/lib/sasl2 that is not a
# mechanism, and `libsasl2` logs a failure for every one it cannot load.
rm -f "$PKG/usr/lib/sasl2/libxoauth2.la"
