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

# A CLIENT LIBRARY AND A PLUGIN LOADER, AND NOTHING THAT AUTHENTICATES ANYBODY
# TO THIS MACHINE.
#
# The one consumer is mbsync, which calls sasl_client_* and takes whichever
# mechanism a plugin in /usr/lib/sasl2 provides — so what this port exists for
# is the loader, and the mechanism that matters arrives in
# ports/core/cyrus-sasl-xoauth2. Nothing on this image is a SASL *server*:
# saslauthd, the sasldb password store and the auxprop backends would be a
# second account database beside /etc/passwd, authenticating logins nothing
# here offers.
#
# WHICH IS WHY THE MECHANISM LIST IS SHORT AND EXPLICIT. Every one of these is
# off by a flag rather than by a missing library, so a build host that happens
# to carry Kerberos or OpenLDAP cannot quietly add a plugin to the image:
#
#   gssapi, kerberos4  — krb5 IS a port and is on the image, for the ticket a
#                        file server takes; no mail provider on this lane
#                        offers GSSAPI, and kerberos4 has been dead for
#                        twenty years.
#   ldapdb, sql        — server-side auxprop backends.
#   otp, srp, ntlm     — mechanisms no provider on the mail lane offers.
#   digest, cram       — obsolete challenge-response, refused by every current
#                        IMAP server and kept alive only by the plugins.
#
# PLAIN AND LOGIN STAY, because a password account over TLS is what every
# provider that still allows one accepts, and SCRAM stays because it is the
# only mechanism here that does not send the password at all.
#
# --with-dblib=none, BECAUSE THE ONLY THING A DATABASE HOLDS HERE IS sasldb.
# configure probes for Berkeley DB, GDBM and LMDB by LINKING rather than by a
# flag, so a build host carrying one would put a password store on an image
# that has no use for it.
# TWO THINGS THE 2.1.28 RELEASE NEEDS FROM A CURRENT COMPILER, AND BOTH ARE
# FLAGS RATHER THAN EDITS.
#
#   -std=gnu17  `lib/md5.c` is K&R C throughout — `static void MD5_memset
#               (output, value, len)` with the types on the following lines —
#               and under C23, which GCC 15 defaults to, `()` means "takes no
#               arguments". Every call then fails as `too many arguments`.
#   -include time.h
#               `lib/saslutil.c` calls time() and clock() and includes neither
#               header, which was a warning until it became an error.
#
# The two are appended to whatever the build passes, never assigned over it.
export CFLAGS="$CFLAGS -std=gnu17 -include time.h"

./configure \
	--prefix=/usr \
	--sysconfdir=/etc \
	--with-plugindir=/usr/lib/sasl2 \
	--with-openssl \
	--with-dblib=none \
	--without-pam \
	--without-saslauthd \
	--without-authdaemond \
	--disable-sample \
	--disable-gssapi \
	--disable-krb4 \
	--disable-otp \
	--disable-srp \
	--disable-ntlm \
	--disable-digest \
	--disable-cram \
	--disable-sql \
	--disable-ldapdb \
	--enable-plain \
	--enable-login \
	--enable-scram \
	--enable-shared \
	--disable-static

# THE HEADER RACE IS REAL AND IS NOT OURS. include/Makefile generates
# sasl.h's companions with a helper it also builds, and a parallel make reaches
# a plugin before that helper has run. Built serially, which costs seconds on a
# library this size.
make -j1
make DESTDIR=$PKG install
