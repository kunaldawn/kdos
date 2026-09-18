# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# THE CLOSURE IS IN THE BUNDLE, and this one is not pure python. Six of
# vdirsyncer's dependencies carry C — aiohttp (which vendors llhttp),
# charset-normalizer, frozenlist, multidict, propcache and yarl — so this port
# needs a compiler on the target, which every phase-4 port has.
#
# THREE OF THEM BUILD THROUGH AN IN-TREE PEP 517 BACKEND. frozenlist, propcache
# and yarl declare `build-backend = pep517_backend.hooks`, which lives inside
# their own sdists and needs `expandvars`; aiohttp additionally wants
# `pkgconfig`. Both are in the bundle because ports/fetch crawls
# build-system.requires out of every tarball it has already fetched.
mkdir -p vendor
tar -xf $PORT_SRC/$name-vendor-$version.tar.xz --strip-components=1 -C vendor

# Backends into the build root, not into $PKG, in bootstrap order — see
# ports/core/khal/build.sh, which does the same for the same reason.
pyb() { pip3 install --no-index --find-links=vendor --no-build-isolation "$@"; }
pyb flit-core
pyb poetry-core
pyb packaging pathspec calver
pyb tomlkit
pyb trove-classifiers
pyb vcs-versioning
pyb setuptools-scm
pyb pluggy
pyb hatchling
pyb hatch-vcs hatch-fancy-pypi-readme
pyb expandvars pkgconfig

pip3 install --no-deps --no-index --find-links=vendor --no-build-isolation \
	--root=$PKG --prefix=/usr \
	aiohappyeyeballs aiohttp aiosignal aiostream attrs certifi \
	charset-normalizer click click-log frozenlist idna multidict \
	propcache requests tenacity typing-extensions urllib3 yarl .

# NO DESKTOP ENTRY. vdirsyncer is a command a person runs or a timer does —
# `vdirsyncer sync` — with no screen to put a menu row in front of.
