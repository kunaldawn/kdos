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

# Fix xargs -e issue for Toybox xargs
sed -i 's/xargs -e/xargs/g' pam_cap/Makefile

# use our buildflags
sed -i "s/CFLAGS :=/CFLAGS += \$(CPPFLAGS) /" Make.Rules
sed -i "s/LDFLAGS :=/LDFLAGS +=/" Make.Rules

# Disable tests, as they cause cross-build failure
sed -e '/test:/,/.sh/d' -e '/tests/d' -i Makefile

make GOLANG=no RAISE_SETFCAP=no lib=lib prefix=/usr DESTDIR=$PKG install
