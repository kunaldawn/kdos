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
# BLUETOOTH LE IS INSTALLED BECAUSE THE CLI IMPORTS IT UNCONDITIONALLY.
# meshtastic/__main__.py imports BLEInterface at the top, so without bleak the
# `meshtastic` command fails on an import before it reads an argument, serial
# port or not. bleak is held at 3.0.1 because 3.0.2 builds with uv_build, a
# Rust backend with no port here; 3.0.1 builds with poetry-core. bleak's
# Linux backend is dbus-fast, installed as pure python: SKIP_CYTHON leaves out
# its optional Cython accelerator, which dbus-fast 5.0.22 declares for Cython
# below 3.3 and which the Cython port's 3.3 rejects (`'buf' redeclared` in
# marshaller.py). Unset, whether it is attempted depends on Cython being in the
# build root.
mkdir -p vendor
tar -xf $PORT_SRC/$name-vendor-$version.tar.xz --strip-components=1 -C vendor

# BUILD ISOLATION IS OFF, and every backend is an installed port: setuptools,
# setuptools-scm and poetry-core. pypubsub declares `setuptools<77`,
# which no setuptools a port ships satisfies, and an isolated build would try
# to find one in the bundle and fail. With isolation off pip consults no
# declaration. requests, urllib3, idna, charset-normalizer and certifi are ports
# and are not installed from the bundle.
SKIP_CYTHON=1 pip3 install --no-deps --no-index --find-links=vendor --no-build-isolation \
	--root=$PKG --prefix=/usr \
	protobuf pypubsub bleak dbus-fast tabulate .
