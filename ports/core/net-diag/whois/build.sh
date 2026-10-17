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

export CFLAGS="$CFLAGS -O2"
export CPPFLAGS="$CPPFLAGS -DHAVE_GETADDRINFO -DHAVE_GETOPT_LONG"
export CPPFLAGS="$CPPFLAGS -DHAVE_REGEXEC -DHAVE_SHA_CRYPT"

MAKEVARS=(CONFIG_FILE=/etc/whois.conf HAVE_ICONV=1)

make "${MAKEVARS[@]}" whois mkpasswd

make DESTDIR=$PKG prefix=/usr "${MAKEVARS[@]}" \
     BASHCOMPDIR=/usr/share/bash-completion/completions \
     install-whois install-mkpasswd install-bashcomp
