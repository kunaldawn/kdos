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

# distlib's Windows script launchers are prebuilt PE binaries; distlib reads
# them only when os.name is 'nt', so nothing on this system can load them.
find "$PKG/usr/lib" -path '*/pip/_vendor/distlib/*.exe' -delete

# THE WHEEL IS WHAT `python3 -m venv` INSTALLS PIP FROM. python3 is built with
# --with-wheel-pkg-dir=/usr/share/python-wheels and no bundled wheel, and
# ensurepip installs only from a pip-*.whl there: without this file venv exits
# 1 and leaves an environment with no pip and no activate scripts. Built from
# the same source and the same vendored flit_core as the install above.
install -d "$PKG/usr/share/python-wheels"
PYTHONPATH=src python3 -m pip wheel . --no-deps --no-index --find-links=vendor \
	-w "$PKG/usr/share/python-wheels"
# pip writes the wheel through a private temporary file, 0600, and venv runs
# as the user who asked for it.
chmod 644 "$PKG"/usr/share/python-wheels/pip-*.whl

# Completions come from pip itself, one bash file per launcher. The script pip
# prints names the command it was run as, and bash-completion loads a file by
# the command being completed, so `pip3` needs its own `pip3` file naming pip3.
_site=$(echo "$PKG"/usr/lib/python3*/site-packages)
install -d "$PKG/usr/share/bash-completion/completions"
for _pip in "$PKG"/usr/bin/pip*; do
	PYTHONPATH="$_site" "$_pip" completion --bash \
		> "$PKG/usr/share/bash-completion/completions/${_pip##*/}"
done
