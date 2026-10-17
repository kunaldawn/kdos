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

patch -p1 -i "$PORT_SRC/waf-2.1.0-compat.patch"
export WAFDIR="$SRC_ROOT/waf-$_waf"
WAF="python3 $WAFDIR/waf-light"

export CFLAGS="$CFLAGS -g0"
OPTS="--disable-jack --disable-avcodec --enable-fftw3f --enable-sndfile
      --enable-samplerate --disable-docs --disable-tests --enable-examples"
$WAF configure --prefix=/usr --libdir=/usr/lib $OPTS
$WAF build $OPTS
$WAF install --destdir="$PKG" $OPTS
