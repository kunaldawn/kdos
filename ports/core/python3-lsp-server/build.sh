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

# pylsp with every plugin it ships (Jedi from the ipython port; Rope, Pylint,
# pyflakes, pycodestyle, pydocstyle, mccabe, flake8, autopep8, YAPF) and
# python-lsp-black; the linters and formatters install their own commands.
# pytokens, black and isort are built as plain Python: their mypyc-compiled
# variants need mypy, which is not a port.
export PYTOKENS_USE_MYPYC=0
pip3 install --no-deps --no-index --find-links=vendor --no-build-isolation \
	--root=$PKG --prefix=/usr \
	python-lsp-jsonrpc ujson docstring-to-markdown pylint astroid dill isort \
	mccabe pycodestyle pyflakes flake8 autopep8 yapf rope pytoolconfig \
	whatthepatch pydocstyle snowballstemmer python-lsp-black black pytokens \
	mypy-extensions .
test -x "$PKG/usr/bin/pylsp"
