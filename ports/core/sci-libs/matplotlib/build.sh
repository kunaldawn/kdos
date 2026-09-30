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

# AGG IS THE DEFAULT BACKEND, set at build time by rcParams-backend: plt.show()
# draws nothing and a plot is saved as a PNG, which libsixel puts in a terminal.
# An application that embeds matplotlib selects its own backend with
# matplotlib.use(), which this default does not stop.

# NO VENDOR BUNDLE, for numpy's reason: mesonpy, pybind11 and setuptools_scm
# are ports and --no-build-isolation reaches them.
# setuptools_scm derives the version from a VCS and there is no .git in a
# tarball; the sdist carries PKG-INFO, which it reads instead, but only when it
# is not told to look for a repository.
export SETUPTOOLS_SCM_PRETEND_VERSION=$version

# system-freetype=true, OR THE BUILD DOWNLOADS FREETYPE. matplotlib's default
# is to fetch and compile its own copy from savannah — which under
# `--network none` is `could not get …; is the internet available?` after every
# other dependency has already resolved. freetype2 is a port and is the same
# rasteriser the rest of this desktop draws with, so a second private copy
# would also mean two answers to how a glyph is hinted.
#
# system-libraqm=true for the same reason one level up: 3.11 requires libraqm
# for text shaping and its subprojects/ carries wraps for libraqm, harfbuzz AND
# sheenbidi, so the default fetches all three — while harfbuzz and fribidi are
# already ports here.
#
# system-qhull=true, and the sdist does NOT carry a bundled copy — it
# downloads `v8.0.2` from github by URL, which is a third fetch in the same
# build after freetype and the libraqm/harfbuzz wraps. Each one only appears
# once the previous is answered.
#
# rcParams-backend=Agg is written into the installed matplotlibrc, so a
# machine with no matplotlibrc of its own starts on Agg rather than probing
# for a GUI toolkit.
pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr . \
	--config-settings=setup-args=-Dsystem-freetype=true \
	--config-settings=setup-args=-Dsystem-libraqm=true \
	--config-settings=setup-args=-Dsystem-qhull=true \
	--config-settings=setup-args=-DrcParams-backend=Agg
