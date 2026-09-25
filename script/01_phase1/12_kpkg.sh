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

# kpkg has to exist before anything can be built with kpkg, so it is compiled
# straight into the sysroot here rather than being a port of its own — the same
# bootstrap the five shell scripts this replaces needed.
#
# It links libkbase, libkpkg and libksig and nothing else, so like kinstall it
# cross-compiles in phase 1 against musl and the kernel headers alone. bash is
# still exec'd to run a recipe's build.sh, but that is a runtime dependency of a
# BUILD, not a link-time one.
#
# libksig is Ed25519 for the binary repository, and it brings Monocypher with it
# — the one vendored third-party source in the tree, chosen because it is the
# only implementation that also links nothing but the C library (see
# src/libs/libksig/ksig.c).

set -e
source script/phase1.env.sh
source script/util/port.sh

SRC=$WORKSPACE/src/packages/kdos-kpkg
LIBS=$WORKSPACE/src/libs
OUT=$BUILD_DIR/tmp/kdos-kpkg

# The mark holds a hash of every source the binary is compiled from, not just
# its existence: kpkg is not a port, so nothing else rebuilds it, and a tree
# whose kpkg predates a change to it keeps installing and upgrading with the
# old logic for every later phase.
SUM=$(find "$SRC" "$LIBS"/libkbase "$LIBS"/libkpkg "$LIBS"/libksig \
        -type f \( -name '*.[ch]' -o -name kpkg.conf \) -print0 |
    LC_ALL=C sort -z | xargs -0 sha256sum | sha256sum | cut -d' ' -f1)

if [ "$(cat "$MARK/kpkg" 2>/dev/null)" = "$SUM" ] &&
   [ "${KDOS_REPLAY:-0}" != "1" ]; then
    exit 0
fi

$KDOS_TARGET-gcc \
    -O2 -pipe -std=gnu11 -D_GNU_SOURCE -Wall -Wextra \
    -I"$LIBS"/libkbase -I"$LIBS"/libkpkg -I"$LIBS"/libksig -I"$SRC" \
    -o "$OUT" \
    "$SRC"/*.c "$LIBS"/libkbase/*.c "$LIBS"/libkpkg/*.c \
    "$LIBS"/libksig/*.c "$LIBS"/libksig/monocypher/*.c

install -Dm755 "$OUT" $SYSROOT/usr/bin/kpkg

# One binary, five names — dispatched on its own basename. The names are what
# the recipes, the build driver, testing/ and muscle memory all call.
for t in kpkgadd kpkgbuild kpkgdel kpkgdepends; do
    ln -sf kpkg $SYSROOT/usr/bin/$t
done

cp "$SRC"/kpkg.conf $SYSROOT/etc/kpkg.conf

echo "$SUM" > "$MARK/kpkg"
