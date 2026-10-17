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
PYTHONPATH=src python3 -m pip install . --ignore-installed --no-deps --find-links=vendor --no-index --root="$PKG" --prefix=/usr

find "$PKG/usr/lib" -path '*/pip/_vendor/distlib/*.exe' -delete

install -d "$PKG/usr/share/python-wheels"
PYTHONPATH=src python3 -m pip wheel . --no-deps --no-index --find-links=vendor \
	-w "$PKG/usr/share/python-wheels"
chmod 644 "$PKG"/usr/share/python-wheels/pip-*.whl

_site=$(echo "$PKG"/usr/lib/python3*/site-packages)
install -d "$PKG/usr/share/bash-completion/completions"
for _pip in "$PKG"/usr/bin/pip*; do
	PYTHONPATH="$_site" "$_pip" completion --bash \
		> "$PKG/usr/share/bash-completion/completions/${_pip##*/}"
done
