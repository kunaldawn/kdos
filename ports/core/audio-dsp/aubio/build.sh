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

# The waf bundled with 0.4.9 imports the `imp` module, which Python 3.12
# removed. The waf release beside it is the build tool instead; WAFDIR points
# waf-light at its own waflib, or it would import the bundled one from the
# current directory. The patch is the one wscript change waf 2.1 needs: its
# option parser is argparse, which takes a type, not a type name.
patch -p1 -i "$PORT_SRC/waf-2.1.0-compat.patch"
export WAFDIR="$SRC_ROOT/waf-$_waf"
WAF="python3 $WAFDIR/waf-light"

# waf reads the feature switches again at build and install time, and a
# switch given only to configure reverts to its default there; the tests would
# then be built and run through a `python` interpreter this system does not
# name. avcodec is off: the reader is written against an FFmpeg API older than
# the one here, and libsndfile reads the formats the tools take. fftw3f is the
# single-precision FFT matching aubio's float samples. JACK is not on this
# system. The manual pages need txt2man, which is not a port.
OPTS="--disable-jack --disable-avcodec --enable-fftw3f --enable-sndfile
      --enable-samplerate --disable-docs --disable-tests --enable-examples"
$WAF configure --prefix=/usr --libdir=/usr/lib $OPTS
$WAF build $OPTS
$WAF install --destdir="$PKG" $OPTS
