# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# THE CLOSURE IS IN THE BUNDLE. `pypackages` names khard's whole runtime set
# and `ports/fetch` crawls each tarball's own build-system.requires from there,
# so one bundle carries what six more recipes would.
#
# vobject IS PINNED BELOW ITS LATEST. 0.9.9's setup.cfg reads its version with
# `attr: vobject.VERSION`, which the package re-exports from a submodule that
# imports dateutil — so setuptools has to IMPORT vobject to learn its version,
# and in the isolated environment a source-only download builds metadata in,
# dateutil is not there. The download fails with `vobject has no attribute
# VERSION`. khard asks for `~= 0.9.7`, and 0.9.8 reads statically.
mkdir -p vendor
tar -xf $PORT_SRC/$name-vendor-$version.tar.xz --strip-components=1 -C vendor

# Backends into the build root, not into $PKG, and in bootstrap order — see
# ports/core/khal/build.sh, which does the same for the same reason.
pyb() { pip3 install --no-index --find-links=vendor --no-build-isolation "$@"; }
pyb flit-core
pyb packaging
pyb setuptools-scm

pip3 install --no-deps --no-index --find-links=vendor --no-build-isolation \
	--root=$PKG --prefix=/usr \
	configobj python-dateutil pytz ruamel.yaml six vobject .

# NO DESKTOP ENTRY. khard is a command that prints and exits — `khard list`,
# `khard show` — with no screen of its own to put a menu row in front of, and
# an entry that opened a terminal to print one page and close it would be a
# row that looks broken. aerc reaches it as a completion source instead.
