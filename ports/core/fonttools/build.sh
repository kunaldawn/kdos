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
# --no-build-isolation for the same reason: what pip's isolated build
# environment would fetch is setuptools and Cython, both installed ports.
#
# FONTTOOLS_WITH_CYTHON=1 makes the compiled cu2qu, qu2cu, bezierTools,
# momentsPen and iup modules mandatory. Unset, setup.py builds them only when
# Cython happens to be importable and ships the pure-Python fallbacks
# otherwise, so the package would depend on build order. WOFF2 (brotli) and
# the lxml XML backend are imports at run time; they are in `depends` so
# `fonttools ttLib.woff2` and UFO/TTX work on every install.
FONTTOOLS_WITH_CYTHON=1 \
	pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .
