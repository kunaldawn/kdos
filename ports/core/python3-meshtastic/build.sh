# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# THE RUNG BETWEEN "THE LAN WORKS" AND "SOMEBODY HAS AN AMATEUR LICENCE". A
# LoRa mesh needs no licence, no infrastructure and no subscription, and it
# carries text and position over kilometres — which on a stick meant for a
# place with no network is the only comms link that is both legal and
# unattended. The radio is a serial device, so the dialout group and the
# CP210x/CH341 udev rules this tree ships are what reach it.
#
# BLUETOOTH LE IS NOT INSTALLED, AND THAT IS STATED RATHER THAN DISCOVERED.
# meshtastic's `bleak` dependency reaches dbus-fast and a build backend that is
# itself written in Rust, which inside pip's isolated environment means
# compiling maturin from source with no network. The SERIAL path — a radio on
# USB, which is how a board is flashed and configured anyway — needs none of
# it. `meshtastic --port /dev/ttyUSB0` works; `--ble` will fail on an import.
#
mkdir -p vendor
tar -xf $PORT_SRC/$name-vendor-$version.tar.xz --strip-components=1 -C vendor

# BUILD ISOLATION IS OFF, AND THE BACKENDS ARE INSTALLED INTO THE BUILD ROOT
# rather than into $PKG. The reason isolation cannot be used is a version
# conflict with no resolution: protobuf declares `setuptools<77` and hatchling
# declares `setuptools>=77`, so one pip invocation cannot satisfy both and
# reports it as a version it cannot find. With isolation off pip consults
# neither declaration and uses the setuptools that is INSTALLED, which is the
# port.
#
# The first pip call is therefore a BUILD DEPENDENCY step: these land in the
# chroot's own site-packages, which is where a build dependency belongs, and
# --root is deliberately absent so none of them reaches the package. A
# --target directory was tried first and does not work: pip's metadata
# subprocess does not pick PYTHONPATH up, and the build fails on
# `No module named 'pathspec'` with pathspec sitting in that directory.
#
# AND THE ORDER IS A BOOTSTRAP, not tidiness. hatchling's own build backend IS
# hatchling, which imports pathspec — so installing it in the same breath as
# its dependencies fails on `No module named 'pathspec'` while pathspec is
# sitting in the same bundle. flit-core is self-hosting and goes first, then
# the plain-setuptools leaves, then the backends built on them.
# Each rung uses only backends the rungs above it have already installed:
# flit-core and poetry-core are self-hosting; packaging, pathspec and calver
# build on those and on the setuptools port; tomlkit needs poetry-core,
# trove-classifiers needs calver, setuptools-scm needs packaging; pluggy needs
# setuptools-scm; hatchling needs packaging, pathspec, pluggy and
# trove-classifiers; hatch-vcs needs hatchling and setuptools-scm. Putting two
# rungs in one pip call fails with the LOWER one reported as a version that
# cannot be found, which reads like a missing file and is an ordering problem.
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
pyb hatch-vcs

pip3 install --no-deps --no-index --find-links=vendor --no-build-isolation \
	--root=$PKG --prefix=/usr \
	protobuf pypubsub PyYAML requests tabulate \
	certifi charset-normalizer idna urllib3 .
