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

export SETUPTOOLS_SCM_PRETEND_VERSION=$version

pip3 install --ignore-installed --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr . \
	--config-settings=setup-args=-Dsystem-freetype=true \
	--config-settings=setup-args=-Dsystem-libraqm=true \
	--config-settings=setup-args=-Dsystem-qhull=true \
	--config-settings=setup-args=-DrcParams-backend=Agg
find "$PKG"/usr/lib/python3*/site-packages/matplotlib "$PKG"/usr/lib/python3*/site-packages/mpl_toolkits -type d -name tests -prune -exec rm -rf {} +
