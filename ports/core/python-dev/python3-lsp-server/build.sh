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

_back="$SRC_ROOT/backends"
pip3 install --no-deps --no-index --find-links=vendor --no-build-isolation \
	--target "$_back" hatch-fancy-pypi-readme pdm-backend
export PYTHONPATH="$_back${PYTHONPATH:+:$PYTHONPATH}"

_d2m="$SRC_ROOT/docstring_to_markdown-0.17"
tar -xzf vendor/docstring_to_markdown-0.17.tar.gz -C "$SRC_ROOT"
(cd "$_d2m" && patch -p1 -i "$PORT_SRC/stdlib-importlib-metadata.patch")

_ap8="$SRC_ROOT/autopep8-2.0.4"
tar -xzf vendor/autopep8-2.0.4.tar.gz -C "$SRC_ROOT"
(cd "$_ap8" && patch -p1 -i "$PORT_SRC/autopep8-py314.patch")

export PYTOKENS_USE_MYPYC=0
pip3 install --ignore-installed --no-deps --no-index --find-links=vendor --no-build-isolation \
	--root=$PKG --prefix=/usr \
	python-lsp-jsonrpc ujson "$_d2m" pylint astroid dill isort \
	mccabe pycodestyle pyflakes flake8 "$_ap8" yapf rope pytoolconfig \
	whatthepatch pydocstyle snowballstemmer python-lsp-black black pytokens \
	mypy-extensions .
test -x "$PKG/usr/bin/pylsp"
