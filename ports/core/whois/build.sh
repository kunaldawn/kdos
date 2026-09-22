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

# THE FEATURE PROBES ARE A LIST OF PREPROCESSOR MACROS IN config.h, not a
# configure run, and every one of them keys off __GLIBC__ or a BSD. musl is
# none of them, so the whole list evaluates false and the build silently
# produces a client with no long options, no getaddrinfo and no regex; the
# capabilities musl does have are asserted here instead. CPPFLAGS is the only
# place they can go: the Makefile appends its own -D list to it, and a DEFS= on
# the command line would beat that append and discard with it the -DHAVE_LIBIDN2
# that the pkg-config probe adds.
export CFLAGS="$CFLAGS -O2"
export CPPFLAGS="$CPPFLAGS -DHAVE_GETADDRINFO -DHAVE_GETOPT_LONG"
export CPPFLAGS="$CPPFLAGS -DHAVE_REGEXEC"

# HAVE_ICONV IS A MAKE VARIABLE, NOT A DEFINE: it also adds simple_recode.o to
# the link, so spelling it into CPPFLAGS would compile the calls and leave the
# implementation out. musl carries iconv in libc, so nothing extra is linked.
#
# CONFIG_FILE names /etc/whois.conf. The package installs whois.conf(5) either
# way, so leaving it undefined ships a manual page for a file the binary never
# reads.
MAKEVARS=(CONFIG_FILE=/etc/whois.conf HAVE_ICONV=1)

# `mkpasswd` IS TOYBOX'S ON THIS IMAGE, so this package builds and installs
# only the client. /usr/bin/mkpasswd is a file the toybox package owns, and the
# orchestrator installs every package with KPKG_OVERWRITE=1, so a second
# claimant is absorbed rather than refused: nothing fails, the later package
# takes the path, and which binary the image ends up with follows the install
# order. -DHAVE_SHA_CRYPT is left out for the same reason: mkpasswd.c is the
# only source that reads it.
#
# THE `all` TARGET BUILDS THE MESSAGE CATALOGUES, and those need xgettext and
# msgfmt for translations no binary here can use: ENABLE_NLS is glibc-only, so
# the catalogues would be installed and never opened. Name the program instead.
make "${MAKEVARS[@]}" whois

# BASHCOMPDIR IS PROBED WITH pkg-config AND bash-completion SHIPS NO .pc FILE
# HERE, so the probe falls through to /etc/bash_completion.d — a directory the
# shell reads only through a compatibility path. Name the real one.
#
# `install-bashcomp` writes both completions from one target and there is no
# narrower one, so the mkpasswd file is removed after it runs: it describes
# options the toybox applet does not take.
make DESTDIR=$PKG prefix=/usr "${MAKEVARS[@]}" \
     BASHCOMPDIR=/usr/share/bash-completion/completions \
     install-whois install-bashcomp
rm -f "$PKG/usr/share/bash-completion/completions/mkpasswd"

# NO DESKTOP ENTRY. `whois` is a query and takes the thing being asked about as
# its argument; launched with none it prints a usage line and exits.
