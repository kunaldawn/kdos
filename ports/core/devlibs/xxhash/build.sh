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

case "$(uname -m)" in
	x86_64|i?86) _dispatch=1 ;;
	*) _dispatch=0 ;;
esac
export CFLAGS="${CFLAGS/-O2/-O3}"
make DISPATCH=$_dispatch LIBXXH_DISPATCH=$_dispatch
make PREFIX=/usr DESTDIR=$PKG DISPATCH=$_dispatch LIBXXH_DISPATCH=$_dispatch install
