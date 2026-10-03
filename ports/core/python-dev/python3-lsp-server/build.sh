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

mkdir -p vendor
tar -xf $PORT_SRC/$name-vendor-$version.tar.xz --strip-components=1 -C vendor

# Two build backends in the bundle are not ports. They are installed into a
# directory of their own on PYTHONPATH for this build only, so nothing outside
# the package is written.
_back="$SRC_ROOT/backends"
pip3 install --no-deps --no-index --find-links=vendor --no-build-isolation \
	--target "$_back" hatch-fancy-pypi-readme pdm-backend
export PYTHONPATH="$_back${PYTHONPATH:+:$PYTHONPATH}"

# docstring-to-markdown imports the importlib_metadata backport, which is not a
# port; the patch points it at the standard library's importlib.metadata, which
# has the same entry_points(group=) call. Unpatched, pylsp fails on its first
# import.
_d2m="$SRC_ROOT/docstring_to_markdown-0.17"
tar -xzf vendor/docstring_to_markdown-0.17.tar.gz -C "$SRC_ROOT"
(cd "$_d2m" && patch -p1 -i "$PORT_SRC/stdlib-importlib-metadata.patch")

# pylsp pins autopep8 below 2.1, and 2.0.4 predates Python 3.14: its setup.py
# reads the version through ast's .s, which 3.14 removed, so metadata
# generation fails; and detect_encoding() imports lib2to3, which 3.13 removed,
# so the autopep8 command fails on every file it is given. The patch reads
# .value, takes detect_encoding from the standard library's tokenize, and has
# the lib2to3 fixers that --aggressive's W690 runs leave the source unchanged.
_ap8="$SRC_ROOT/autopep8-2.0.4"
tar -xzf vendor/autopep8-2.0.4.tar.gz -C "$SRC_ROOT"
(cd "$_ap8" && patch -p1 -i "$PORT_SRC/autopep8-py314.patch")

# pylsp with every plugin it ships (Jedi from the ipython port; Rope, Pylint,
# pyflakes, pycodestyle, pydocstyle, mccabe, flake8, autopep8, YAPF) and
# python-lsp-black; the linters and formatters install their own commands.
# pytokens, black and isort are built as plain Python: their mypyc-compiled
# variants need mypy, which is not a port.
export PYTOKENS_USE_MYPYC=0
pip3 install --no-deps --no-index --find-links=vendor --no-build-isolation \
	--root=$PKG --prefix=/usr \
	python-lsp-jsonrpc ujson "$_d2m" pylint astroid dill isort \
	mccabe pycodestyle pyflakes flake8 "$_ap8" yapf rope pytoolconfig \
	whatthepatch pydocstyle snowballstemmer python-lsp-black black pytokens \
	mypy-extensions .
test -x "$PKG/usr/bin/pylsp"
