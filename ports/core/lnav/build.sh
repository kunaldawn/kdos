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

# THE RUST EXTENSION IS A SECOND CARGO PROJECT INSIDE THE TARBALL, and one of
# its dependencies is a GIT revision rather than a crates.io version, so the
# whole of it has to come out of a vendor bundle.
#
# THE BUNDLE GOES IN src/, NOT BESIDE THE MANIFEST. Cargo finds .cargo/config
# by walking up from the CURRENT DIRECTORY, and lnav's Makefile invokes it from
# src/ with `--manifest-path ./third-party/lnav-rs-ext/Cargo.toml` — so a config
# next to that manifest is never read, and the build fails trying to clone the
# git dependency. Unpacked here, the config's own relative `directory = vendor`
# resolves correctly as well.
#
# The Makefile is what invokes cargo, so the offline rule arrives as
# CARGO_NET_OFFLINE rather than as --offline on a command line we do not own.
tar xf $PORT_SRC/${name}-vendor-${version}.tar.xz -C src
export CARGO_HOME="$SRC_ROOT/.cargo"
export RUSTFLAGS="-C target-feature=-crt-static"
export CARGO_NET_OFFLINE=true

# THE REMOTE TAILER IS COMPILED HERE. lnav embeds two tailers and ships the
# first that a remote host accepts over ssh: the Python one when the host has
# python3, then a native binary. Upstream's native one is a prebuilt
# Cosmopolitan executable; the patch replaces it with tailer.main.c linked
# static against musl, so that fallback reaches x86-64 Linux hosts only.
patch -p1 -i $PORT_SRC/lnav-tailer-static.patch

./autogen.sh

# SQL OVER LOG LINES IS WHY THIS IS HERE AND NOT JUST less. Offline you cannot
# paste a log into a search box, so the machine has to be able to answer a
# question about its own logs — and lnav's sqlite view is the only thing on
# this system that can. libarchive is what lets it read a rotated .gz or .xz
# without unpacking it first; its probe and bzip2's only test and never fail,
# so the depends line is what keeps them in.
#
# --disable-system-paths: otherwise every /usr/local and /opt/local that
# exists on the builder is added to the include and library paths.
# The two cache answers keep re2c and cxxbridge unused whether or not the
# builder has them: the scanners and the Rust bridge are then compiled from
# the sources the tarball ships.
./configure --prefix=/usr --disable-static \
	--with-libarchive \
	--with-cargo \
	--disable-system-paths \
	ac_cv_path_RE2C_CMD= \
	ac_cv_path_CXXBRIDGE=
make
make DESTDIR=$PKG install

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/lnav.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Log Viewer
GenericName=Log Viewer
Comment=Merge, filter and search log files
Exec=lnav %F
Icon=text-x-generic
Terminal=true
Categories=System;Monitor;Utility;
Keywords=log;journal;syslog;filter;lnav;
EOF
chmod 644 "$PKG/usr/share/applications/lnav.desktop"
