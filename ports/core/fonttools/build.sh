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

# NO VENDOR BUNDLE, because there is nothing to vendor: fontTools is
# zero-dependency by design and everything in its requirements.txt is an
# OPTIONAL extra — that file reaches scipy, thirty megabytes and a Fortran
# compiler, for a font inspector.
#
# --no-build-isolation: pip's isolated build environment would have to fetch
# setuptools, and it would not see the installed Cython, so setup.py would ask
# for Cython through setup_requires and fail with no network.
#
# FONTTOOLS_WITH_CYTHON=1 makes the compiled cu2qu, qu2cu, bezierTools,
# momentsPen, iup and feaLib lexer modules mandatory. Unset, setup.py builds
# them only when Cython happens to be importable and ships the pure-Python
# fallbacks otherwise, so the package would depend on build order. brotli and
# lxml are imports at run time: without brotli `fonttools ttLib.woff2` cannot
# read or write WOFF2, and without lxml the XML readers fall back to the
# standard library's ElementTree. Both are in `depends` so every install has
# them.
FONTTOOLS_WITH_CYTHON=1 \
	pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .
