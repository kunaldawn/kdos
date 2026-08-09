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

make CFLAGS="$CFLAGS" \
	prefix=/usr \
	gitexecdir=/usr/lib/git-core \
	perllibdir="$(/usr/bin/perl -MConfig -wle 'print $Config{installvendorlib}')" \
	NO_REGEX=NeedsStartEnd \
	NO_TCLTK=Yes \
	DESTDIR=$PKG install
