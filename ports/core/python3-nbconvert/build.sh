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

# nbconvert and the notebook stack under it that JupyterLab and Spyder share:
# nbformat and its fastjsonschema validator, nbclient, and the HTML
# sanitising and Markdown pieces. jsonschema, jsonschema-specifications,
# referencing, Beautiful Soup, Soup Sieve and defusedxml are ports of their
# own: other programs import them from site-packages too, and two packages
# owning one path there is a conflict. The templates' CSS ships in the
# release tarball, so the build hook that would download it finds it
# present. The PDF exporter runs xelatex, from texlive, and the web-PDF
# exporter needs Playwright, which is not a port.
pip3 install --no-deps --no-index --find-links=vendor --no-build-isolation \
	--root=$PKG --prefix=/usr \
	nbclient nbformat fastjsonschema mistune bleach pandocfilters .
test -e "$PKG"/usr/share/jupyter/nbconvert/templates/lab/static/index.css
