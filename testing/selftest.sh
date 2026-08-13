#!/bin/bash
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   testing/selftest.sh — compile and run the libk* regression net
#
# Host-only and self-contained: no container, no ports tree, no network. It
# builds every library with the HOST compiler and runs src/libs/selftest.c
# against them, then compiles each program that uses them to prove the headers
# still agree.
#
# Run it before trusting a change to anything under src/libs/.

set -e
cd "$(dirname "$0")/.."

CC=${CC:-cc}
WARN="-Wall -Wextra -Werror"
STD="-O2 -std=gnu11 -D_GNU_SOURCE"
INC="-Isrc/libs/libkbase -Isrc/libs/libkcolor -Isrc/libs/libktui -Isrc/libs/libkxdg -Isrc/libs/libkpkg -Isrc/libs/libkbuild -Isrc/tools/kdos-portup"
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT

echo "==> selftest"
$CC $STD $WARN $INC -o "$OUT/selftest" src/libs/selftest.c \
    src/libs/libkbase/*.c src/libs/libkcolor/*.c src/libs/libkpkg/*.c \
    src/libs/libkbuild/*.c src/libs/libktui/*.c \
    src/tools/kdos-portup/extract.c
"$OUT/selftest"

echo
echo "==> every consumer still compiles against the libraries"
$CC $STD $WARN $INC -Isrc/packages/kdos-installer -o "$OUT/kinstall" \
    src/packages/kdos-installer/*.c src/libs/libkbase/*.c \
    src/libs/libktui/*.c src/libs/libkcolor/*.c -lcrypt
echo "  kinstall"
$CC $STD $WARN $INC -Isrc/packages/kdos-appbox -o "$OUT/kdos-appbox" \
    src/packages/kdos-appbox/*.c src/libs/libkbase/*.c src/libs/libktui/*.c \
    src/libs/libkcolor/*.c src/libs/libkxdg/*.c
echo "  kdos-appbox"
$CC $STD $WARN $INC -Isrc/packages/kdos-theme -o "$OUT/kdos-theme" \
    src/packages/kdos-theme/*.c src/libs/libkbase/*.c src/libs/libkcolor/*.c
echo "  kdos-theme"
$CC $STD $WARN $INC -Isrc/packages/kdos-tools -o "$OUT/kdos-tools" \
    src/packages/kdos-tools/*.c src/libs/libkbase/*.c src/libs/libkcolor/*.c \
    src/libs/libkpkg/*.c src/libs/libkxdg/*.c
echo "  kdos-tools"
$CC $STD $WARN $INC -Isrc/libs/libksig -Isrc/packages/kdos-kpkg \
    -o "$OUT/kdos-kpkg" \
    src/packages/kdos-kpkg/*.c src/libs/libkbase/*.c src/libs/libkpkg/*.c \
    src/libs/libksig/*.c src/libs/libksig/monocypher/*.c
echo "  kdos-kpkg"
# kdos-powerd is a root daemon and kdos-checkpass is the one setuid binary in
# the tree; both link libkbase-or-less on purpose, so both compile here.
$CC $STD $WARN $INC -o "$OUT/kdos-powerd" \
    src/desktop/kdos-powerd/main.c src/libs/libkbase/*.c
ln -sf kdos-powerd "$OUT/kdos-power"
echo "  kdos-powerd"

# kdos-energyd is the other root daemon, and the only one whose ANSWER can be
# checked without root: --fixture replays recorded /proc and powercap trees
# through the same sampler and ledger the daemon runs.
$CC $STD $WARN $INC -Isrc/desktop/kdos-energyd -o "$OUT/kdos-energyd" \
    src/desktop/kdos-energyd/*.c src/libs/libkbase/*.c
ln -sf kdos-energyd "$OUT/kdos-energy"
echo "  kdos-energyd"
$CC $STD $WARN -o "$OUT/kdos-checkpass" src/desktop/kdos-lock/checkpass.c -lcrypt
echo "  kdos-checkpass"
$CC $STD $WARN $INC -Isrc/build/kdosbuild -o "$OUT/kdosbuild" \
    src/build/kdosbuild/*.c src/libs/libkbase/*.c src/libs/libkbuild/*.c \
    src/libs/libktui/*.c src/libs/libkcolor/*.c
echo "  kdosbuild"
"$OUT/kdosbuild" --selftest
$CC $STD $WARN $INC -Isrc/tools/kdos-portup -o "$OUT/kdos-portup" \
    src/tools/kdos-portup/*.c src/libs/libkbase/*.c src/libs/libkpkg/*.c \
    src/libs/libkbuild/*.c
echo "  kdos-portup"
"$OUT/kdos-portup" --selftest --fixture testing/fixtures/portup

# libkwl is libktui's Wayland backend and is the ONE library here with real
# external dependencies — which is the whole reason it is a separate archive
# from libktui, whose zero-`-l` property keeps kinstall in phase 1. Skipped
# rather than failed when they are absent: this script's contract is that it
# runs on a bare host with no container and no network.
if pkg-config --exists fcft pixman-1 xkbcommon wayland-client 2>/dev/null &&
   [ -n "$(pkg-config --variable=pkgdatadir wayland-protocols 2>/dev/null)" ]; then
    PROTO="$OUT/proto"
    mkdir -p "$PROTO"
    SCANNER=$(pkg-config --variable=wayland_scanner wayland-scanner)
    XDG=$(pkg-config --variable=pkgdatadir wayland-protocols)/stable/xdg-shell/xdg-shell.xml
    LS=$(ls ports/core/wlroots/wlroots-*.tar.gz 2>/dev/null | head -1)
    if [ -n "$LS" ]; then
        tar xf "$LS" -C "$PROTO" --strip-components=2 \
            "$(tar tf "$LS" | grep 'protocol/wlr-layer-shell-unstable-v1.xml$' | head -1)"
        "$SCANNER" client-header "$PROTO/wlr-layer-shell-unstable-v1.xml" \
            "$PROTO/wlr-layer-shell-unstable-v1-client-protocol.h"
        "$SCANNER" client-header "$XDG" "$PROTO/xdg-shell-client-protocol.h"
        # The lock role's protocol. Missing it fails the compile rather than
        # skipping quietly, which is the point: libkwl is what the lock screen
        # draws through.
        "$SCANNER" client-header \
            "$(pkg-config --variable=pkgdatadir wayland-protocols)/staging/ext-session-lock/ext-session-lock-v1.xml" \
            "$PROTO/ext-session-lock-v1-client-protocol.h"
        $CC $STD $WARN -c -I"$PROTO" -Isrc/libs/libkbase -Isrc/libs/libktui \
            -Isrc/libs/libkcolor -Isrc/libs/libkwl \
            $(pkg-config --cflags fcft pixman-1 xkbcommon wayland-client) \
            -o "$OUT/kwl.o" src/libs/libkwl/kwl.c
        for f in src/libs/libkwl/kwl_font.c src/libs/libkwl/kwl_paint.c \
                 src/libs/libkwl/kwl_key.c; do
            $CC $STD $WARN -c -I"$PROTO" -Isrc/libs/libkbase -Isrc/libs/libktui \
                -Isrc/libs/libkcolor -Isrc/libs/libkwl \
                $(pkg-config --cflags fcft pixman-1 xkbcommon wayland-client) \
                -o "$OUT/$(basename "$f" .c).o" "$f"
        done
        echo "  libkwl"
    fi
else
    echo "  libkwl (skipped — fcft/pixman/xkbcommon/wayland-client not on this host)"
fi

# The compositor's CRT pass. Same shape as the block above and for the same
# reason — wlroots is a port, not something a bare host has — but worth
# compiling wherever it IS available: crt.c is the one file in the tree that
# touches raw GLES2, and its two shader programs are built from a printf format
# whose sampler declaration differs per program. A typo there is a runtime
# failure on a screen nobody here has.
CRT_LS=$(ls ports/core/wlroots/wlroots-*.tar.gz 2>/dev/null | head -1)
if pkg-config --exists wlroots-0.20 glesv2 egl wayland-server pixman-1 2>/dev/null &&
   [ -n "$CRT_LS" ]; then
    # wlr_layer_shell_v1.h includes a wayland-scanner SERVER header that wlroots
    # neither generates nor installs, so every consumer has to run the scanner
    # itself — the port's build.sh does exactly this.
    CP="$OUT/cproto"
    mkdir -p "$CP"
    tar xf "$CRT_LS" -C "$CP" --strip-components=2 \
        "$(tar tf "$CRT_LS" | grep 'protocol/wlr-layer-shell-unstable-v1.xml$' | head -1)"
    $(pkg-config --variable=wayland_scanner wayland-scanner) server-header \
        "$CP/wlr-layer-shell-unstable-v1.xml" \
        "$CP/wlr-layer-shell-unstable-v1-protocol.h"
    $CC $STD $WARN -c -DWLR_USE_UNSTABLE -I"$CP" -Isrc/desktop/kdos-comp \
        -Isrc/libs/libkcolor -Isrc/libs/libkbase \
        $(pkg-config --cflags wlroots-0.20 glesv2 egl wayland-server pixman-1) \
        -o "$OUT/crt.o" src/desktop/kdos-comp/crt.c
    $CC $STD $WARN -c -DWLR_USE_UNSTABLE -I"$CP" -Isrc/desktop/kdos-comp \
        -Isrc/libs/libkcolor -Isrc/libs/libkbase \
        $(pkg-config --cflags wlroots-0.20 glesv2 egl wayland-server pixman-1) \
        -o "$OUT/frames.o" src/desktop/kdos-comp/frames.c
    # capture.c is where grim, wl-clipboard and the portal get their globals.
    # Every one of them is a wlroots type whose create() signature changes
    # between releases, so compiling it here is the cheapest possible check
    # that the pinned wlroots still has the API this file was written against.
    $CC $STD $WARN -c -DWLR_USE_UNSTABLE -I"$CP" -Isrc/desktop/kdos-comp \
        -Isrc/libs/libkcolor -Isrc/libs/libkbase \
        $(pkg-config --cflags wlroots-0.20 glesv2 egl wayland-server pixman-1) \
        -o "$OUT/capture.o" src/desktop/kdos-comp/capture.c
    echo "  kdos-comp crt.c, frames.c and capture.c"
else
    echo "  kdos-comp crt.c/frames.c/capture.c (skipped — wlroots-0.20/glesv2/egl not on this host)"
fi

echo
echo "==> kdos-portup fixture-backed check (offline, no network)"
# testing/fixtures/portup was recorded live against the six ports below, one
# per discovery path: fuse (GitHub forge), zlib (a plain directory listing),
# ca-certificates (directory listing that comes up empty, falling to repology
# and only matching CURRENT's shape through the strip-separators/dot-collapse
# normalisation), aalib (repology, genuinely CURRENT — upstream hasn't
# released since 2001), mesa (a large directory listing), and imagemagick
# (repology again, but UNKNOWN: its recipe's own source URL template is dead,
# so no rendered candidate — including its own pinned version — ever proves).
# Replaying them through --fixture exercises pu_list_upstream, pu_extract,
# the shape filter and pu_render_candidate end to end with no curl involved —
# proved separately with `unshare --net`, not just by inspection here — and
# is what makes this reach all three outcomes (current/newer/unknown) without
# a live network call.
#
# --fixture makes kdos-portup skip loading AND saving
# ports/.update-cache.json entirely — fixture 200s are not evidence about the
# real world and must never outlive this process — so there is nothing to
# back up or restore around this run any more.
CACHE="$PWD/ports/.update-cache.json"
CACHE_BEFORE=$(md5sum "$CACHE" 2>/dev/null || true)
set +e
KDOS_PORTUP_REPO="$PWD" "$OUT/kdos-portup" --check --refresh --json \
    --fixture "$PWD/testing/fixtures/portup" \
    fuse zlib ca-certificates aalib mesa imagemagick \
    > "$OUT/portup-fixture.json" 2> "$OUT/portup-fixture.err"
rc=$?
set -e
[ "$(md5sum "$CACHE" 2>/dev/null || true)" = "$CACHE_BEFORE" ] || {
    echo "  fixture run touched the real update cache"
    exit 1
}
# 0 = every checked port is current, 1 = --check found at least one update —
# both are a completed run; anything else is a crash or a usage error.
[ "$rc" -le 1 ] || { echo "  fixture-backed check exited $rc"; cat "$OUT/portup-fixture.err"; exit 1; }
grep -q '"state": "current"' "$OUT/portup-fixture.json" || { echo "  no current outcome reproduced"; exit 1; }
grep -q '"state": "newer"'   "$OUT/portup-fixture.json" || { echo "  no newer outcome reproduced";   exit 1; }
grep -q '"state": "unknown"' "$OUT/portup-fixture.json" || { echo "  no unknown outcome reproduced"; exit 1; }
echo "  6 ports, all three outcomes reproduced from the recorded corpus"

echo
echo "==> kpkgdepends still agrees with the ports tree"
PORT_REPO="$PWD/ports/core $PWD/src/packages" KPKG_CONF=/nonexistent \
    PKGDB_DIR=/dev/null "$OUT/kdos-kpkg" kpkgdepends bash >/dev/null
echo "  ok"

echo
echo "==> a package built twice is byte-identical"
# The P12 claim, tested against the mechanism rather than against a real port:
# a synthetic source-less recipe whose build.sh installs a few files, built
# twice, must produce the same bytes. Every nondeterminism this guards against
# is in the packaging step and not in the compiler, so a toolchain is not
# needed and the test costs milliseconds.
#
# The second build runs under a HOSTILE environment on purpose — a different
# umask, XZ_OPT asking for threads, and another time zone — because each of
# those silently changed the archive before kpkg pinned them.
RP="$OUT/repro"
rm -rf "$RP"
mkdir -p "$RP/ports/tiny" "$RP/work" "$RP/pkgs"
cat > "$RP/ports/tiny/kpkgbuild" <<'EOF'
name        = tiny
version     = 1.0
release     = 1
description = a synthetic port that exists to be built twice
homepage    = https://example.invalid/
EOF
cat > "$RP/ports/tiny/build.sh" <<'EOF'
install -Dm755 /dev/null "$PKG/usr/bin/tiny"
printf 'hello\n' > "$PKG/usr/bin/tiny"
install -d "$PKG/usr/share/tiny"
printf 'one\n' > "$PKG/usr/share/tiny/a.txt"
printf 'two\n' > "$PKG/usr/share/tiny/b.txt"
ln -sf a.txt "$PKG/usr/share/tiny/link"
touch "$PKG/usr/share/tiny/plain"
EOF
# kpkg dispatches on its own basename, so the builder has to be reached as one.
ln -sf kdos-kpkg "$OUT/kpkgbuild"
kbuild_pkg() {
    ( cd "$RP/ports/tiny" &&
      env PORT_REPO="$RP/ports" WORK_DIR="$RP/work" PACKAGE_DIR="$RP/pkgs" \
          PKGDB_DIR=/dev/null KPKG_CONF=/nonexistent \
          SOURCE_DATE_EPOCH=1735689600 "$@" \
          "$OUT/kpkgbuild" >/dev/null 2>&1 )
}
kbuild_pkg TZ=UTC || { echo "  the synthetic port did not build"; exit 1; }
mv "$RP/pkgs/tiny-1.0-1.tar.xz" "$RP/one.tar.xz"
( umask 077; kbuild_pkg TZ=Asia/Kolkata XZ_OPT=-T0 ) \
    || { echo "  the second build failed"; exit 1; }
cmp -s "$RP/one.tar.xz" "$RP/pkgs/tiny-1.0-1.tar.xz" \
    || { echo "  the same recipe produced two different packages"; exit 1; }
# And the normalisation is visible in the archive itself, not just equal by
# luck: uid/gid 0, and the pinned epoch rather than the wall clock.
TZ=UTC tar -tvf "$RP/one.tar.xz" | grep -q "0/0 .*2025-01-01" \
    || { echo "  the archive is not normalised (uid/gid or mtime)"; exit 1; }
echo "  identical under a different umask, TZ and XZ_OPT; uid/gid 0, epoch mtime"

echo
echo "==> the binhost signs an index and a client checks it"
# The whole P15 chain against the synthetic port from the block above: build a
# package, make a key, index and sign, then ask a client whether it may use it.
# Every refusal is asserted, because a signing feature is only worth having if
# the NO paths work — a verifier that cannot fail verifies nothing.
BH="$OUT/binhost"
rm -rf "$BH"
mkdir -p "$BH/repo" "$BH/keys"
cp "$RP/one.tar.xz" "$BH/repo/tiny-1.0-1.tar.xz"
( cd "$BH" && "$OUT/kdos-kpkg" kpkg keygen builder >/dev/null 2>&1 ) \
    || { echo "  keygen failed"; exit 1; }
test "$(stat -c %a "$BH/builder.key")" = 600 \
    || { echo "  the secret key was not written 0600"; exit 1; }
kpkg_bh() {
    env PORT_REPO="$RP/ports" KPKG_CONF=/nonexistent PKGDB_DIR=/dev/null \
        KPKG_KEYRING="$BH/keys" SOURCE_DATE_EPOCH=1735689600 \
        "$OUT/kdos-kpkg" "$@"
}
kpkg_bh kpkg index "$BH/repo" --sign "$BH/builder.key" >/dev/null 2>&1 \
    || { echo "  indexing failed"; exit 1; }
test -s "$BH/repo/PACKAGES.sig" || { echo "  no index signature"; exit 1; }
test -s "$BH/repo/tiny-1.0-1.tar.xz.sig" || { echo "  no package sidecar"; exit 1; }
grep -q "^E:" "$BH/repo/PACKAGES" || { echo "  no recipe hash in the index"; exit 1; }

# An empty keyring must REFUSE, not fall back to trusting the index.
rc=0; kpkg_bh kpkg binhost "$BH/repo" tiny --dry-run >/dev/null 2>&1 || rc=$?
test "$rc" = 2 || { echo "  an unsigned-for-us index was accepted"; exit 1; }

cp "$BH/builder.pub" "$BH/keys/"
kpkg_bh kpkg binhost "$BH/repo" tiny --dry-run >/dev/null 2>&1 \
    || { echo "  a good index and a matching build were rejected"; exit 1; }
kpkg_bh kpkg verify-index "$BH/repo" >/dev/null 2>&1 \
    || { echo "  verify-index rejected a good signature"; exit 1; }
kpkg_bh kpkg verify-pkg "$BH/repo/tiny-1.0-1.tar.xz" >/dev/null 2>&1 \
    || { echo "  verify-pkg rejected a good sidecar"; exit 1; }

# One byte, three ways: a tampered index, a tampered package, and a signature
# from a key nobody trusts.
cp "$BH/repo/PACKAGES" "$BH/PACKAGES.good"
sed -i 's/^S:[0-9]*$/S:1/' "$BH/repo/PACKAGES"
rc=0; kpkg_bh kpkg binhost "$BH/repo" tiny --dry-run >/dev/null 2>&1 || rc=$?
test "$rc" = 2 || { echo "  an edited index still verified"; exit 1; }
cp "$BH/PACKAGES.good" "$BH/repo/PACKAGES"

printf 'x' >> "$BH/repo/tiny-1.0-1.tar.xz"
rc=0; kpkg_bh kpkg binhost "$BH/repo" tiny --dry-run >/dev/null 2>&1 || rc=$?
test "$rc" = 2 || { echo "  a package whose hash moved was accepted"; exit 1; }
rc=0; kpkg_bh kpkg verify-pkg "$BH/repo/tiny-1.0-1.tar.xz" >/dev/null 2>&1 || rc=$?
test "$rc" = 1 || { echo "  a tampered package passed its sidecar"; exit 1; }
truncate -s -1 "$BH/repo/tiny-1.0-1.tar.xz"

( cd "$BH" && "$OUT/kdos-kpkg" kpkg keygen stranger >/dev/null 2>&1 )
rm -f "$BH/repo/PACKAGES.sig"
kpkg_bh kpkg index "$BH/repo" --sign "$BH/stranger.key" >/dev/null 2>&1
rc=0; kpkg_bh kpkg binhost "$BH/repo" tiny --dry-run >/dev/null 2>&1 || rc=$?
test "$rc" = 2 || { echo "  a signature from an untrusted key was accepted"; exit 1; }
kpkg_bh kpkg index "$BH/repo" --sign "$BH/builder.key" >/dev/null 2>&1

# And the three equality tests: a different build config must send the client
# back to source, with exit 1 (no match) rather than 2 (refused).
rc=0; CFLAGS="-O3 -march=native" kpkg_bh kpkg binhost "$BH/repo" tiny \
    --dry-run >/dev/null 2>&1 || rc=$?
test "$rc" = 1 || { echo "  a different build config was treated as a match"; exit 1; }
echo "  signed, verified, and every refusal refused"

echo
echo "==> a delta rebuilds the next package from the last one"
# P16, against the same synthetic port: build 1.0 and 1.1, take the difference,
# apply it, and require the result to be byte-identical — which is only possible
# because packaging is reproducible (P12). The tampered case matters as much:
# a delta is never trusted, it is applied and the RESULT is checked, so a delta
# that produces the wrong bytes must leave nothing behind.
if command -v zstd >/dev/null 2>&1 && command -v xz >/dev/null 2>&1; then
    sed -i 's/^version     = 1.0$/version     = 1.1/' "$RP/ports/tiny/kpkgbuild"
    printf 'printf "three\\n" > "$PKG/usr/share/tiny/c.txt"\n' >> "$RP/ports/tiny/build.sh"
    kbuild_pkg TZ=UTC || { echo "  the second version did not build"; exit 1; }
    NEW="$RP/pkgs/tiny-1.1-1.tar.xz"
    test -s "$NEW" || { echo "  no second package"; exit 1; }

    kpkg_bh kpkg delta "$RP/one.tar.xz" "$NEW" -o "$RP/tiny.kdelta" >/dev/null 2>&1 \
        || { echo "  making the delta failed"; exit 1; }
    DS=$(stat -c %s "$RP/tiny.kdelta"); NS=$(stat -c %s "$NEW")
    test "$DS" -lt "$NS" \
        || { echo "  the delta ($DS) is not smaller than the package ($NS)"; exit 1; }

    WANT=$(sha256sum "$NEW" | cut -d' ' -f1)
    kpkg_bh kpkg apply-delta "$RP/one.tar.xz" "$RP/tiny.kdelta" \
        -o "$RP/rebuilt.tar.xz" --expect "$WANT" >/dev/null 2>&1 \
        || { echo "  applying the delta failed"; exit 1; }
    cmp -s "$RP/rebuilt.tar.xz" "$NEW" \
        || { echo "  the reconstruction is not byte-identical"; exit 1; }

    # A delta with one byte changed must not produce a package.
    printf 'x' >> "$RP/tiny.kdelta"
    rc=0
    kpkg_bh kpkg apply-delta "$RP/one.tar.xz" "$RP/tiny.kdelta" \
        -o "$RP/bad.tar.xz" --expect "$WANT" >/dev/null 2>&1 || rc=$?
    test "$rc" != 0 || { echo "  a tampered delta was accepted"; exit 1; }
    test ! -e "$RP/bad.tar.xz" \
        || { echo "  a failed reconstruction left a package behind"; exit 1; }
    echo "  delta smaller, reconstruction byte-identical, tampering discarded"
else
    echo "  deltas (skipped — no zstd or no xz on this host)"
fi

echo
echo "==> -march is kept only where the win beat the noise"
# N14's whole claim is the DECISION, and it is testable without building
# anything twice: `kdos march decide <baseline> <optimised> <noise%>` is the
# same function the measurement path calls. The cases below are the four that
# matter, including the one every "optimised distro" gets wrong — a positive
# number smaller than the spread it came out of.
ln -sf kdos-tools "$OUT/kdos"
march() { "$OUT/kdos" march decide "$@"; }
march 10 8 1 | grep -q "kept" \
    || { echo "  a 20% win over 1% noise was not kept"; exit 1; }
march 10 9.8 1 | grep -q "reverted" \
    || { echo "  a 2% win was kept"; exit 1; }
march 10 8 30 | grep -q "reverted" \
    || { echo "  a win inside the noise was kept"; exit 1; }
march 10 12 1 | grep -q "reverted" \
    || { echo "  a REGRESSION was kept"; exit 1; }
# probe must never claim a level this CPU cannot run.
"$OUT/kdos" march probe | grep -q "highest usable" \
    || { echo "  probe said nothing about this CPU"; exit 1; }
if ! grep -q avx512f /proc/cpuinfo 2>/dev/null; then
    "$OUT/kdos" march probe | grep -q "x86-64-v4 *missing" \
        || { echo "  probe claimed v4 on a CPU without avx512"; exit 1; }
fi
echo "  a win over noise is kept; a win under it, and a regression, are not"

echo
echo "==> kdos rebuild refuses what it cannot finish"
# N13's value is in the checks, not the running: a rebuild started in RAM fills
# memory and dies hours in with the machine unusable. Every refusal is tested
# because the successful path is a six-hour build nothing here can run.
ln -sf kdos-tools "$OUT/kdos"
rb() { env KDOS_SOURCES="$1" "$OUT/kdos" rebuild "${@:2}"; }
# From a directory that is not a tree and with nothing pointing at one: the
# search falls back to `.`, which is the repo when this script runs from it.
( cd "$OUT" && env -u KDOS_SOURCES "$OUT/kdos" rebuild --dry-run "$OUT/rb" ) \
    >/dev/null 2>&1 \
    && { echo "  a rebuild with no sources was allowed"; exit 1; }
rb /tmp --dry-run "$OUT/rb" >/dev/null 2>&1 \
    && { echo "  a directory that is not a KDOS tree was accepted"; exit 1; }
rb "$PWD" --dry-run "$OUT/rb" >/dev/null 2>&1 \
    || { echo "  this repo was not recognised as a KDOS tree"; exit 1; }
# /dev/shm is tmpfs on any Linux that has it, which is the case this exists for.
if [ -d /dev/shm ]; then
    rb "$PWD" --dry-run /dev/shm/kdos-rebuild-check >/dev/null 2>&1 \
        && { echo "  a work directory in RAM was accepted"; exit 1; }
    rm -rf /dev/shm/kdos-rebuild-check
fi
rb "$PWD" --dry-run "$OUT/rb" 2>&1 | grep -q "nothing was copied" \
    || { echo "  --dry-run did not say it did nothing"; exit 1; }
echo "  no tree, a wrong tree and a work directory in RAM are all refused"

echo
echo "==> doctor can tell whether the initrd carries this CPU's microcode"
# The early loader does not mount anything: it scans the raw initrd for one
# literal path before decompression. So this builds an initrd shaped exactly
# like 01_initramfs.sh's output -- an uncompressed cpio carrying both vendors'
# blobs, then the gzipped part -- and asserts doctor's answer flips with it.
# Both blobs are present so the assertion does not depend on the host's CPU.
UC="$OUT/ucode"
rm -rf "$UC"; mkdir -p "$UC/src/kernel/x86/microcode"
printf 'not real microcode, but at the right path\n' \
    > "$UC/src/kernel/x86/microcode/GenuineIntel.bin"
cp "$UC/src/kernel/x86/microcode/GenuineIntel.bin" \
   "$UC/src/kernel/x86/microcode/AuthenticAMD.bin"
( cd "$UC/src" && find . | cpio -o -H newc ) > "$UC/ucode.cpio" 2>/dev/null
printf 'the rest of the initramfs\n' | gzip -9 > "$UC/main.gz"
cat "$UC/ucode.cpio" "$UC/main.gz" > "$UC/with.img"
cp "$UC/main.gz" "$UC/without.img"

ln -sf kdos-tools "$OUT/kdos"
KDOS_INITRD="$UC/with.img" "$OUT/kdos" doctor 2>/dev/null \
    | grep -q "microcode in the initrd" \
    || { echo "  microcode in the initrd was not found"; exit 1; }
KDOS_INITRD="$UC/without.img" "$OUT/kdos" doctor 2>/dev/null \
    | grep -q "carries no .* microcode" \
    || { echo "  a missing microcode blob was not reported"; exit 1; }
KDOS_INITRD="$UC/nosuch.img" "$OUT/kdos" doctor 2>/dev/null \
    | grep -q "cannot tell whether microcode is carried" \
    || { echo "  a missing initrd was not reported honestly"; exit 1; }
echo "  found when carried, reported when not, honest when there is no image"

echo
echo "==> a bad update boots three times and rolls itself back"
# The A/B state machine, driven exactly as the machine drives it: `select` is
# what the initramfs runs (decide and spend an attempt), `mark-good` what the
# end of rcS runs. A boot that never reaches mark-good is a boot that failed,
# which is the whole design — so the test simply never calls it.
AB="$OUT/ab"
mkdir -p "$AB"
bootctl() { env KDOS_BOOTSTATE="$AB/bootstate" "$OUT/kdos-bootctl" "$@"; }
ln -sf kdos-tools "$OUT/kdos-bootctl"

bootctl status >/dev/null 2>&1 \
    && { echo "  a machine with no state file claimed to have slots"; exit 1; }
bootctl set-slot a AAAA-1111 >/dev/null || { echo "  set-slot failed"; exit 1; }
bootctl set-slot b BBBB-2222 >/dev/null || { echo "  set-slot failed"; exit 1; }
test "$(bootctl select)" = "AAAA-1111" \
    || { echo "  a confirmed machine did not boot its active slot"; exit 1; }

# The update: try the other slot. Three boots that never confirm, then a
# rollback — and the rollback must be to the slot that was working.
bootctl try b >/dev/null || { echo "  try failed"; exit 1; }
for i in 1 2 3; do
    test "$(bootctl select 2>/dev/null)" = "BBBB-2222" \
        || { echo "  attempt $i did not boot the candidate"; exit 1; }
done
test "$(bootctl select 2>/dev/null)" = "AAAA-1111" \
    || { echo "  a failing slot was not rolled back"; exit 1; }
test "$(bootctl select 2>/dev/null)" = "AAAA-1111" \
    || { echo "  the rollback did not stick"; exit 1; }
bootctl status | grep -q "trying   nothing" \
    || { echo "  the rollback left a try flag behind"; exit 1; }

# The good update: one boot, then rcS confirms it.
bootctl try b >/dev/null
test "$(bootctl select 2>/dev/null)" = "BBBB-2222" || { echo "  no candidate"; exit 1; }
bootctl mark-good >/dev/null || { echo "  mark-good failed"; exit 1; }
test "$(bootctl select)" = "BBBB-2222" \
    || { echo "  a confirmed slot did not become active"; exit 1; }

# A torn state file reads as ABSENT, never as partial: half a file that looked
# complete is how a machine boots a slot that was never installed.
cp "$AB/bootstate" "$AB/good"
printf 'slot_a = AAAA-1111\nactiv' > "$AB/bootstate"
bootctl select >/dev/null 2>&1 \
    && { echo "  a truncated state file was believed"; exit 1; }
cp "$AB/good" "$AB/bootstate"
# And the refusals, which are what keep a state file from naming nowhere.
bootctl try b >/dev/null 2>&1 \
    && { echo "  trying the active slot was allowed"; exit 1; }
bootctl try z >/dev/null 2>&1 && { echo "  a bogus slot was allowed"; exit 1; }
rm -f "$AB/bootstate"
bootctl set-slot a AAAA-1111 >/dev/null
bootctl try b >/dev/null 2>&1 \
    && { echo "  trying a slot with no root was allowed"; exit 1; }
echo "  three attempts then rollback, mark-good confirms, torn state ignored"

echo
echo "==> the initramfs unlocks a LUKS root, or says why it cannot"
# The generated init is a heredoc inside a packaging script, which is exactly
# the kind of code nothing ever tests until it is 3 a.m. and a laptop will not
# boot. It is extracted, syntax-checked, and its unlock function is run against
# stub tools — which is what PASS_TTY and CRYPT_MAPPER_DIR exist for.
IR="$OUT/initramfs"
mkdir -p "$IR/bin" "$IR/mapper"
python3 - "$IR/init" <<'PYEOF' || { echo "  could not extract the generated init"; exit 1; }
import re, sys
s = open('script/06_packaging/01_initramfs.sh').read()
m = re.search(r"cat > init <<EOF\n(.*?)\nEOF\n", s, re.S)
if not m:
    sys.exit(1)
body = m.group(1).replace('\\$', '$').replace('\\`', '`').replace('\\\\', '\\')
open(sys.argv[1], 'w').write(body)
PYEOF
bash -n "$IR/init" || { echo "  the generated init is not valid bash"; exit 1; }
grep -q "cryptdevice=" "$IR/init" || { echo "  the init does not parse cryptdevice="; exit 1; }
# The passphrase must never reach argv: /proc/<pid>/cmdline is world-readable.
grep -q -- "--key-file=-" "$IR/init" \
    || { echo "  the passphrase is not fed on stdin"; exit 1; }
grep -q "cryptsetup open .*\"\$PASS\"" "$IR/init" \
    && { echo "  the passphrase is passed as an argument"; exit 1; }

cat > "$IR/bin/blkid" <<'EOF'
#!/bin/sh
echo "$FAKE_LUKS_DEV"
EOF
cat > "$IR/bin/cryptsetup" <<'EOF'
#!/bin/sh
# Accepts exactly one passphrase, and only on stdin.
# argv is: open --key-file=- <device> <name>
read -r given
[ "$given" = "opensesame" ] || exit 2
touch "$CRYPT_MAPPER_DIR/$4"
EOF
cat > "$IR/bin/modprobe" <<'EOF'
#!/bin/sh
exit 0
EOF
chmod +x "$IR/bin/"*
# The init calls cryptsetup and the splash by absolute path, which is right on a
# real initramfs and is why the stubs are pointed at rather than shadowed.
sed -i "s#/bin/cryptsetup#$IR/bin/cryptsetup#g; s#/bin/kdos-splash#true#g" "$IR/init"
: > "$IR/fake-luks"

luks_try() {
    ( . /dev/stdin <<EOF
$(sed -n '/^unlock_root() {/,/^}/p' "$IR/init")
EOF
      PATH="$IR/bin:$PATH" PASS_TTY="$1" CRYPT_MAPPER_DIR="$IR/mapper" \
          FAKE_LUKS_DEV="$IR/fake-luks" unlock_root "$2" >/dev/null 2>&1 )
}
printf 'opensesame\n' > "$IR/good.tty"
printf 'nope\nnope\nnope\n' > "$IR/bad.tty"
rm -f "$IR/mapper/"*
luks_try "$IR/good.tty" "UUID=1234-abcd:kdosroot" \
    || { echo "  the right passphrase did not unlock"; exit 1; }
test -e "$IR/mapper/kdosroot" \
    || { echo "  no mapper device after a successful unlock"; exit 1; }
rm -f "$IR/mapper/"*
luks_try "$IR/bad.tty" "UUID=1234-abcd:kdosroot" \
    && { echo "  a wrong passphrase unlocked the volume"; exit 1; }
luks_try "$IR/good.tty" "this-is-not-a-spec" \
    && { echo "  a malformed cryptdevice= was accepted"; exit 1; }
echo "  cryptdevice= parsed, passphrase on stdin, three tries then a shell"

echo
echo "==> kdosbuild reads the build tree correctly"
# This used to be a DIFFERENTIAL against script/buildlib: the C and python
# views of the same tree, compared line by line. buildlib is gone, so there is
# nothing left to diff against — the invariant it protected (that the port
# matched the original) has been discharged. What remains is libkbuild's own
# assertions in src/libs/selftest.c and the end-to-end run below, which
# exercises the same code against a real tree.
"$OUT/kdosbuild" --script-dir script --list >/dev/null 2>&1 \
    || { echo "  kdosbuild cannot read script/"; exit 1; }
phases=$("$OUT/kdosbuild" --script-dir script --build-dir "$OUT/empty" --list 2>&1)
echo "  phase discovery and snapshot inventory"

echo
echo "==> kdosbuild runs a build end to end"
# A synthetic two-phase tree, driven headless. This is the only test that
# exercises the ENGINE — forking steps, capturing their output, writing the
# logs, tarring the snapshot, extracting it again — rather than a decision.
mkdir -p "$OUT/empty"
E="$OUT/e2e"
mkdir -p "$E/script/00_alpha" "$E/script/01_beta" "$E/build" "$E/bin"
cat > "$E/script/alpha.env.sh" <<'EOF'
export KDOS_PHASE_TITLE="Alpha Phase"
export KDOS_SNAPSHOT_PATHS="fs"
export KDOS_SNAPSHOT_EXCLUDE="fs/tmp/*"
rm -rf /var/cache/kpkg/work
EOF
cat > "$E/script/beta.env.sh" <<'EOF'
export KDOS_SNAPSHOT_PATHS="fs ports"
EOF
cat > "$E/script/00_alpha/00_tree.sh" <<'EOF'
#!/bin/bash
# Title: build the rootfs
set -e
mkdir -p "$PWD/build/fs/usr/bin" "$PWD/build/fs/tmp"
echo hello > "$PWD/build/fs/usr/bin/hello"
printf 'ansi \033[31mred\033[0m and\ttab\n'
printf 'no trailing newline'
EOF
cat > "$E/script/01_beta/00_ports.sh" <<'EOF'
#!/bin/bash
mkdir -p "$PWD/build/ports"; echo port > "$PWD/build/ports/one"
echo "replay=$KDOS_REPLAY"
EOF
chmod +x "$E"/script/*/*.sh
KB="$OUT/kdosbuild"

( cd "$E" && "$KB" --script-dir script --build-dir build --fresh ) > "$OUT/e2e.log" 2>&1
grep -q "BUILD COMPLETE" "$OUT/e2e.log" || { echo "  build did not complete"; cat "$OUT/e2e.log"; exit 1; }
[ -f "$E/build/snapshots/00_alpha/manifest.json" ] || { echo "  no snapshot written"; exit 1; }
[ -f "$E/build/logs/00_alpha/0000_tree.sh.log" ] || { echo "  no step log written"; exit 1; }
# The log FILE is verbatim, as build.py wrote it — escapes and all; only the
# in-memory copy the TUI draws is sanitised. What matters here is that a final
# line with no newline on it is not swallowed, which is where a build's real
# error message often is.
grep -q "no trailing newline" "$E/build/logs/00_alpha/0000_tree.sh.log" \
    || { echo "  unterminated last line lost"; exit 1; }
echo "  build, snapshot and logs"

# Restore has to put the tree back and skip the phases it covered.
rm -rf "$E/build/fs" "$E/build/ports"
( cd "$E" && "$KB" --script-dir script --build-dir build --restore 00_alpha ) \
    > "$OUT/e2e-restore.log" 2>&1
[ "$(cat "$E/build/fs/usr/bin/hello" 2>/dev/null)" = "hello" ] \
    || { echo "  restore did not bring the tree back"; cat "$OUT/e2e-restore.log"; exit 1; }
[ -e "$E/build/.restore-in-progress" ] && { echo "  restore marker not cleared"; exit 1; }
grep -q "restored 00_alpha" "$OUT/e2e-restore.log" || { echo "  restore not announced"; exit 1; }
echo "  restore and resume"

# A plan that narrows must suppress snapshots and must set KDOS_REPLAY for a
# step it named — 17 phase scripts exit 0 on a second pass without it.
( cd "$E" && "$KB" --script-dir script --build-dir build \
    --steps beta:00_ports.sh ) > "$OUT/e2e-plan.log" 2>&1
grep -q "snapshots disabled for this partial run" "$OUT/e2e-plan.log" \
    || { echo "  a narrowing plan did not suppress snapshots"; exit 1; }
grep -q "replay=1" "$E"/build/logs/01_beta/*.log \
    || { echo "  KDOS_REPLAY not set for a named step"; exit 1; }
echo "  build plan narrowing and KDOS_REPLAY"

# A failing step stops the build, exits 1, and does NOT snapshot the phase.
F="$OUT/fail"
mkdir -p "$F/script/00_a" "$F/build"
echo 'export KDOS_SNAPSHOT_PATHS="fs"' > "$F/script/a.env.sh"
printf '#!/bin/bash\necho starting\nexit 3\n' > "$F/script/00_a/00_boom.sh"
printf '#!/bin/bash\necho unreachable\n' > "$F/script/00_a/01_after.sh"
chmod +x "$F"/script/*/*.sh
if ( cd "$F" && "$KB" --script-dir script --build-dir build --fresh ) \
        > "$OUT/fail.log" 2>&1; then
    echo "  a failing step did not fail the build"; exit 1
fi
grep -q "BUILD FAILED" "$OUT/fail.log" || { echo "  failure not reported"; exit 1; }
[ -d "$F/build/snapshots/00_a" ] && { echo "  snapshotted a failed phase"; exit 1; }
[ -f "$F/build/logs/00_a/0001_after.sh.log" ] && { echo "  ran a step after the failure"; exit 1; }
echo "  failure stops the build and is not snapshotted"

# --json is the SAME traversal, so what it says has to match what the text run
# said. The failing tree is the one worth asserting on: a reporter that skips
# the step it failed at, or attributes it to the wrong phase, is exactly the
# defect a grep for "BUILD FAILED" cannot see.
if ( cd "$F" && rm -rf build && "$KB" --script-dir script --build-dir build \
        --fresh --json ) > "$OUT/fail.json" 2>&1; then
    echo "  --json did not fail the build"; exit 1
fi
grep -q '{"event": "build", .*"phases": 1' "$OUT/fail.json" \
    || { echo "  no build event"; cat "$OUT/fail.json"; exit 1; }
grep -q '"event": "step", "status": "failed", "step": "boom", "phase": "00_a".*"rc": 3' \
    "$OUT/fail.json" || { echo "  the failing step is not in the stream"; exit 1; }
grep -q '"event": "result", "status": "failed".*"failed": 1.*"failed_step": "boom"' \
    "$OUT/fail.json" || { echo "  no result event"; exit 1; }
grep -q '"status": "running", "step": "after"' "$OUT/fail.json" \
    && { echo "  reported a step that never ran"; exit 1; }
( cd "$E" && "$KB" --script-dir script --build-dir build --list --json ) \
    > "$OUT/list.json" 2>&1
grep -q '"phase": "00_alpha"' "$OUT/list.json" \
    || { echo "  --list --json lost a snapshot"; cat "$OUT/list.json"; exit 1; }
( cd "$E" && "$KB" --script-dir script --build-dir "$OUT/empty" --list --json ) \
    | grep -q '"count": 0' \
    || { echo "  an empty inventory is not an empty array"; exit 1; }
# It has to PARSE, not merely look like JSON. Shell cannot tell the difference,
# so this half is skipped rather than faked when python3 is absent.
if command -v python3 >/dev/null 2>&1; then
    python3 - "$OUT/fail.json" "$OUT/list.json" <<'EOF' || exit 1
import json, sys
for line in open(sys.argv[1]):
    if line.strip():
        json.loads(line)
json.load(open(sys.argv[2]))
EOF
    echo "  --json: NDJSON events, the inventory, and both parse"
else
    echo "  --json: NDJSON events and the inventory (no python3: not parsed)"
fi

echo
echo "==> kinstall says what it would do, without doing it"
# The installer cannot be tested by running it, so `--dump plan` is the test:
# an answer file in, the step list out, with the skips those answers imply.
# `--dump probe` is NOT run here — it walks the whole rootfs to measure the
# payload, which is seconds on a live ISO and much longer on a dev machine.
KI="$OUT/kinstall"
"$KI" --save "$OUT/answers.conf" >/dev/null \
    || { echo "  --save failed"; exit 1; }
sed -i 's/^plan.*/plan = reuse/; s/^theme.*/theme = amber/' "$OUT/answers.conf"
"$KI" --config "$OUT/answers.conf" --dump plan > "$OUT/plan.txt" 2>&1
"$KI" --config "$OUT/answers.conf" --dump plan --json > "$OUT/plan.json" 2>&1
# reuse does not repartition, and a non-default accent has to be regenerated:
# the two skip rules, read back off the plan rather than off the source.
grep -qE "^  2 +Partition +skipped" "$OUT/plan.txt" \
    || { echo "  reuse still plans to partition"; cat "$OUT/plan.txt"; exit 1; }
grep -qE "^  8 +Theme +pending" "$OUT/plan.txt" \
    || { echo "  a non-default accent is not regenerated"; exit 1; }
grep -q '"title": "Partition".*"state": "skipped"' "$OUT/plan.json" \
    || { echo "  text and json disagree about Partition"; exit 1; }
# A dump ends up in bug reports and CI logs, and the answer file carries the
# passwords in the clear because crypt() is about to be called on them. The
# sentinel is what proves neither rendering repeats one.
printf 'password = hunter2-sentinel\nroot_password = hunter2-sentinel\n' \
    >> "$OUT/answers.conf"
# The LUKS passphrase is the third secret cfg carries and the newest, so it goes
# through the same sentinel: an answer file may hold it, a dump may not.
printf 'luks = 1\nluks_passphrase = hunter2-sentinel\n' >> "$OUT/answers.conf"
"$KI" --config "$OUT/answers.conf" --dump plan --json > "$OUT/plan.json" 2>&1
"$KI" --config "$OUT/answers.conf" --dump plan > "$OUT/plan.txt" 2>&1
grep -q 'hunter2-sentinel' "$OUT/plan.json" "$OUT/plan.txt" \
    && { echo "  a password or passphrase reached the dump"; exit 1; }
# And the answer file kinstall WRITES never carries the passphrase either.
"$KI" --config "$OUT/answers.conf" --save "$OUT/saved.conf" >/dev/null 2>&1
grep -q 'hunter2-sentinel' "$OUT/saved.conf" \
    && { echo "  --save wrote a secret into the answer file"; exit 1; }
grep -q '^luks  *= *1' "$OUT/saved.conf" \
    || { echo "  --save lost the luks flag"; exit 1; }
"$KI" --dump bogus >/dev/null 2>&1 && { echo "  --dump took a bad subject"; exit 1; }

# The root filesystem choice, read back as what it actually becomes. The three
# things that must move together are the mkfs, its overwrite flag and the fstab
# line — and fs_passno must be 0 for anything but ext4, because there is no
# fsck.btrfs worth running and no fsck.xfs on this image at all.
fsdump() {
    printf 'fstype = %s\n' "$1" > "$OUT/fs.conf"
    "$KI" --config "$OUT/fs.conf" --dump plan 2>&1
}
fsdump ext4  | grep -q "^mkfs  *mkfs.ext4 -F" \
    || { echo "  ext4 does not use mkfs.ext4 -F"; exit 1; }
fsdump ext4  | grep -q "^fstab root  *ext4 defaults,noatime 0 1" \
    || { echo "  ext4 lost its fsck pass"; exit 1; }
fsdump btrfs | grep -q "^mkfs  *mkfs.btrfs -f" \
    || { echo "  btrfs does not use mkfs.btrfs -f"; exit 1; }
fsdump btrfs | grep -q "^fstab root  *btrfs .* 0 0" \
    || { echo "  btrfs was given a non-zero fsck pass"; exit 1; }
fsdump xfs   | grep -q "^fstab root  *xfs .* 0 0" \
    || { echo "  xfs was given a non-zero fsck pass"; exit 1; }
# An answer file naming a filesystem this build cannot create must install a
# working machine, not fail at the mkfs.
fsdump zfs   | grep -q "fs ext4" \
    || { echo "  an unknown fstype was not refused back to ext4"; exit 1; }
echo "  the filesystem choice reaches mkfs and fstab, and only ext4 is fsck'd"
if command -v python3 >/dev/null 2>&1; then
    python3 -c 'import json,sys; json.load(open(sys.argv[1]))' "$OUT/plan.json" \
        || exit 1
    echo "  --dump plan: both renderings agree, no secrets, json parses"
else
    echo "  --dump plan: both renderings agree, no secrets (no python3)"
fi

echo
echo "==> kdos-powerd only lets root and wheel near the power"
# The gate is SO_PEERCRED on the connection, which cannot be tested without two
# uids. `--explain` reads exactly the same two files the gate does and is the
# diagnostic a user gets for a dead power key, so it is what is asserted here.
"$OUT/kdos-powerd" --explain root | grep -q "permitted" \
    || { echo "  root is not permitted"; exit 1; }
"$OUT/kdos-powerd" --explain no-such-user-here 2>/dev/null | grep -q "refused" \
    || { echo "  an unknown user is not refused"; exit 1; }
"$OUT/kdos-powerd" --explain no-such-user-here >/dev/null 2>&1 \
    && { echo "  an unknown user exited 0"; exit 1; }
# A group whose name merely STARTS with wheel must not count, and the daemon
# must refuse to serve the real socket without privilege.
"$OUT/kdos-powerd" >/dev/null 2>&1 && { echo "  served /run as non-root"; exit 1; }
# The client says what to do when nothing is listening, rather than failing mute.
KDOS_POWERD_SOCKET="$OUT/nothing.sock" "$OUT/kdos-power" ping 2>&1 \
    | grep -q "no kdos-powerd" || { echo "  no message for a dead daemon"; exit 1; }
echo "  --explain, the non-root refusal, and the client's message"

echo
echo "==> kdos-energyd attributes energy to apps, not to pids"
# The two real inputs are a root-only counter and a machine that happens to be
# busy in a particular way, so the fixture IS the test: four recorded snapshots
# with a nested RAPL tree, a counter wrap, a boxed process tree and a /proc/stat
# busy figure deliberately larger than the surviving pids account for.
E="$OUT/energy.json"
KDOS_ALIEN_APPS=testing/fixtures/energy/alien-apps \
    "$OUT/kdos-energyd" --fixture testing/fixtures/energy --json > "$E" \
    || { echo "  the fixture did not replay"; exit 1; }
# The idle floor is 15 W only if the subdomains were NOT summed with their
# parent. Counting intel-rapl:0:0 and :0:1 as well gives 26.25 W.
grep -q '"idle_floor_w":15.000' "$E" \
    || { echo "  nested RAPL domains were double-counted"; exit 1; }
# 600 J attributable of 1050 J total, which is only true if the wrapped counter
# in the last window was corrected; without it the fraction comes out 1.0000.
grep -q '"attributable_fraction":0.5714' "$E" \
    || { echo "  the wrapped energy counter was mishandled"; exit 1; }
# Identity: a content process is rolled up onto the app that is its ancestor,
# and the app is named with the box conmon says it is in.
grep -q '"name":"firefox-esr (appbox kdos-apps)","impact":0.7550' "$E" \
    || { echo "  the boxed app was not named or not rolled up"; exit 1; }
grep -q "Web Content" "$E" \
    && { echo "  a helper process was reported as an app of its own"; exit 1; }
# The ticks /proc/stat counted that no surviving pid claims are their own line,
# never spread over the survivors.
grep -q '"short_lived":0.0867' "$E" \
    || { echo "  exited processes were not accounted separately"; exit 1; }
# Both honesty flags, which are what stop the number being read as watt-hours.
grep -q '"gpu_in_domain":true' "$E" \
    || { echo "  the uncore domain was not detected"; exit 1; }
KDOS_ALIEN_APPS=testing/fixtures/energy/alien-apps \
    "$OUT/kdos-energyd" --fixture testing/fixtures/energy \
    | grep -q "no watt-hours here" \
    || { echo "  the report does not say what it refuses to claim"; exit 1; }
if command -v python3 >/dev/null 2>&1; then
    python3 -c 'import json,sys; json.load(open(sys.argv[1]))' "$E" || exit 1
fi
# A daemon that cannot read the counter must refuse to start rather than report
# a machine using no energy at all.
"$OUT/kdos-energyd" 2>&1 | grep -q "PLATYPUS" \
    || { echo "  an unreadable counter did not stop the daemon"; exit 1; }
KDOS_ENERGYD_SOCKET="$OUT/nothing.sock" "$OUT/kdos-energy" 2>&1 \
    | grep -q "no kdos-energyd" || { echo "  no message for a dead daemon"; exit 1; }
echo "  the fixture replays: nesting, the wrap, the roll-up and the residue"

echo
echo "==> genlaunchers turns an image's desktop entries into host commands"
# Four outputs, and dropping any one of them breaks something visible: the
# launcher, the mime cache beside it, the name -> in-box command table, and the
# /usr/local/bin shim that makes every alien app an ordinary command. The fake
# image also carries wine's shape — a NoDisplay entry plus a binary — because
# that is the case the COMMANDS table exists for.
IMG="$OUT/img"; FSR="$OUT/fsroot"
rm -rf "$IMG" "$FSR"
mkdir -p "$IMG/usr/share/applications" "$IMG/usr/bin" "$FSR"
cat > "$IMG/usr/share/applications/gimp.desktop" <<'DESK'
[Desktop Entry]
Type=Application
Name=GIMP
Exec=gimp %U
MimeType=image/png;
Categories=Graphics;
DESK
cat > "$IMG/usr/share/applications/wine.desktop" <<'DESK'
[Desktop Entry]
Type=Application
Name=Wine
Exec=wine %f
NoDisplay=true
DESK
touch "$IMG/usr/bin/wine" "$IMG/usr/bin/winecfg"
"$OUT/kdos-appbox" genlaunchers "$IMG/usr/share/applications" "$FSR" 2>/dev/null \
    || { echo "  genlaunchers failed"; exit 1; }
APPS="$FSR/etc/skel/.local/share/applications"
# The launcher FILENAME must be upstream's own desktop id — a dock matches a
# running window to an entry by that id and nothing else.
test -f "$APPS/gimp.desktop" || { echo "  no launcher for a normal app"; exit 1; }
grep -q "^image/png=gimp.desktop;" "$APPS/mimeinfo.cache" \
    || { echo "  MimeType did not reach the cache"; exit 1; }
# NoDisplay is the box's own business and must never become a launcher.
test -f "$APPS/wine.desktop" && { echo "  a NoDisplay entry became a launcher"; exit 1; }
# …but the COMMAND must still be reachable, or the box carries wine and the host
# cannot run it.
grep -q "^wine	wine$" "$FSR/usr/share/kdos/alien-apps" \
    || { echo "  a command-only app got no alien-apps row"; exit 1; }
test -L "$FSR/usr/local/bin/wine" || { echo "  no shim for wine"; exit 1; }
test -L "$FSR/usr/local/bin/gimp" || { echo "  no shim for gimp"; exit 1; }
# winetricks is in the COMMANDS table and NOT in this image: an older bake must
# not gain a shim that dies on "not found".
test -e "$FSR/usr/local/bin/winetricks" \
    && { echo "  a shim was made for a binary the image lacks"; exit 1; }
echo "  launcher, mime cache, command table and shims — and no shim without a binary"

echo
echo "==> kdos stutter names the process, not just the pressure"
# A real stutter cannot be summoned on demand, so the fixture IS the test:
# testing/fixtures/stutter is two /proc snapshots 500 ms apart and the two miss
# events kdos-comp would have sent between them. The sampler reads it with the
# same code that reads /proc, so what is exercised is the join — the naming, the
# container lookup, the blocked-before-busy ordering, and the one causal branch.
S="$OUT/stutter.txt"
# The binary dispatches on its own basename, so it has to be reached as `kdos`.
ln -sf kdos-tools "$OUT/kdos"
TZ=UTC "$OUT/kdos" stutter --fixture testing/fixtures/stutter > "$S" \
    || { echo "  the fixture did not replay"; exit 1; }
# The whole point of the milestone, in one line: the app's name AND its box.
grep -q "calibre (appbox kdos-apps)" "$S" \
    || { echo "  the busy process was not named with its box"; exit 1; }
# A process asleep in D state shows almost no CPU, so sorting on CPU alone would
# hide exactly the case the io half exists for. It must come first.
grep -q "busiest just then: calibre-idx (appbox kdos-apps) (waiting on the disk)" "$S" \
    || { echo "  the disk-blocked process was not put first"; exit 1; }
# The one causal claim the tool is entitled to make, and only for the event whose
# render used most of the budget.
grep -q "the desktop itself was late: the compositor took 14.9 ms" "$S" \
    || { echo "  a compositor-side stall was not attributed to the compositor"; exit 1; }
test "$(grep -c 'the desktop itself was late' "$S")" = 1 \
    || { echo "  the compositor was blamed for a miss that was not its"; exit 1; }
grep -q "cpu pressure 71%, io 45%" "$S" \
    || { echo "  PSI was not reported alongside"; exit 1; }
TZ=UTC "$OUT/kdos" stutter --json --fixture testing/fixtures/stutter \
    | python3 -c 'import sys,json;[json.loads(l) for l in sys.stdin]' 2>/dev/null \
    || echo "  (--json not parsed — no python3 on this host)"
echo "  the fixture replays: the app, its box, the disk waiter first, and PSI"

echo
echo "==> the tray host talks to a real StatusNotifierItem"
# SNI is a conversation between two processes on a session bus, so the fixture
# is a second process rather than a mock: testing/fixtures/tray/traycheck.c
# forks a Qt-shaped tray app and drives tray.c against it. Both bugs this caught
# were silent — properties read from inside a bus callback never get their reply,
# and a click sent to the interface spelling the app does not implement goes
# nowhere without an error, because a click is fire-and-forget.
#
# Skipped rather than failed where there is no sd-bus or no dbus-daemon: this
# script runs on a bare host, and basu is a KDOS port.
TRAY_SDBUS=""
pkg-config --exists basu 2>/dev/null && TRAY_SDBUS=basu
[ -z "$TRAY_SDBUS" ] && pkg-config --exists libsystemd 2>/dev/null && \
    TRAY_SDBUS=libsystemd
if [ -n "$TRAY_SDBUS" ] && command -v dbus-daemon >/dev/null 2>&1; then
    $CC $STD $WARN -o "$OUT/traycheck" \
        -Isrc/desktop/kdos-shell -Isrc/libs/libktui -Isrc/libs/libkcolor \
        -Isrc/libs/libkxdg -Isrc/libs/libkbase \
        $(pkg-config --cflags $TRAY_SDBUS) \
        testing/fixtures/tray/traycheck.c src/desktop/kdos-shell/tray.c \
        $(pkg-config --libs $TRAY_SDBUS)
    # A private bus, so the test never touches the developer's own session and
    # never inherits a tray that is already there.
    TRAY_ADDR=$(dbus-daemon --session --print-address --fork \
        --print-pid=3 3>"$OUT/tray-bus.pid")
    TRAY_BUS_PID=$(cat "$OUT/tray-bus.pid")
    DBUS_SESSION_BUS_ADDRESS="$TRAY_ADDR" "$OUT/traycheck" "$OUT/tray.log" \
        || { kill "$TRAY_BUS_PID" 2>/dev/null; exit 1; }
    kill "$TRAY_BUS_PID" 2>/dev/null
else
    echo "  tray (skipped — no sd-bus or no dbus-daemon on this host)"
fi

echo
echo "==> the recording indicator names the app holding the camera"
# The camera half is a /proc walk, so the fixture is a /proc: three processes,
# one of which holds an ALSA capture device and must be ignored — on a PipeWire
# system that process IS PipeWire, and naming it is the non-answer every other
# desktop gives. The microphone half needs a live graph and is proved by hand;
# here PIPEWIRE_RUNTIME_DIR points nowhere, so its absence is what gets checked.
if pkg-config --exists libpipewire-0.3 2>/dev/null; then
    $CC $STD $WARN -o "$OUT/privacycheck" \
        -Isrc/desktop/kdos-shell -Isrc/libs/libktui -Isrc/libs/libkcolor \
        -Isrc/libs/libkxdg -Isrc/libs/libkbase \
        $(pkg-config --cflags libpipewire-0.3) \
        testing/fixtures/privacy/privacycheck.c src/desktop/kdos-shell/privacy.c \
        $(pkg-config --libs libpipewire-0.3)
    KDOS_PRIVACY_PROC=testing/fixtures/privacy/proc \
        PIPEWIRE_RUNTIME_DIR=/nonexistent-pipewire "$OUT/privacycheck" || exit 1
else
    echo "  privacy (skipped — libpipewire-0.3 not on this host)"
fi

echo
echo "==> kdos cve compares pins against the vendored security database"
# Four ports and a five-row table, which is enough to exercise every rule the
# real 4 099-row one exercises: a pin behind two fixes, a pin that only LOOKS
# behind one because Alpine's `-rN` packaging revision is not upstream's
# version, a port whose Alpine name differs (`secdb =`), and a port Alpine has
# never heard of — which must read as UNKNOWN and never as clean.
CV="$OUT/cve.txt"
ln -sf kdos-tools "$OUT/kdos"
if PORT_REPO="$PWD/testing/fixtures/cve/ports" KPKG_CONF=/nonexistent \
    PKGDB_DIR=/dev/null KDOS_SECDB="$PWD/testing/fixtures/cve/secdb.txt" \
    "$OUT/kdos" cve > "$CV" 2>&1; then
    echo "  a vulnerable pin did not fail the exit code"; exit 1
fi
grep -q "oldpkg .*1.2.2 .*fixed in 1.4.0" "$CV" \
    || { echo "  the newest fix a pin is behind was not reported"; cat "$CV"; exit 1; }
grep -q "CVE-2024-1111,CVE-2024-2222,CVE-2025-3333" "$CV" \
    || { echo "  CVEs from several fix rows were not merged"; exit 1; }
if grep -q "CVE-2020-0001" "$CV"; then
    echo "  Alpine's 'fixed in 0' row was treated as a finding"; exit 1
fi
if grep -q "newpkg" "$CV"; then
    echo "  an -rN packaging revision was read as an upstream version"; exit 1
fi
grep -q "mappedpkg .*fixed in 3.1" "$CV" \
    || { echo "  the 'secdb =' name mapping was not honoured"; exit 1; }
grep -q "4 checked, 1 not in the database, 2 behind a recorded fix" "$CV" \
    || { echo "  the summary miscounted"; cat "$CV"; exit 1; }
grep -q "the database is .* days old" "$CV" \
    || { echo "  a stale database was not called stale"; exit 1; }
rc=0
PORT_REPO="$PWD/testing/fixtures/cve/ports" KPKG_CONF=/nonexistent \
    PKGDB_DIR=/dev/null KDOS_SECDB=/nonexistent-secdb "$OUT/kdos" cve \
    >/dev/null 2>&1 || rc=$?
test "$rc" = 2 \
    || { echo "  a missing database did not report itself as unrunnable"; exit 1; }
echo "  fix rows merged, -rN ignored, 'secdb =' honoured, unknown ≠ clean"

echo
echo "==> kdos theme --audit catches artwork that is not the palette's"
# The palette claim, checked the way the audit checks it: generate a full themed
# $HOME from the repo's own art, audit it (must be clean), then break one of each
# KIND of drift — an edited file, a deleted file, a stray file, a symlink pointed
# somewhere else — and require all four to be caught. This is the test that keeps
# `--audit` honest; an audit that cannot fail is decoration.
AH="$OUT/audit-home"
rm -rf "$AH"
mkdir -p "$AH/.config"
cp fs/etc/skel/.config/starship.toml "$AH/.config/" 2>/dev/null || true
export KDOS_GTK_SRC="$PWD/src/packages/kdos-gtk-theme/theme"
export KDOS_ICON_ART="$PWD/src/packages/kdos-icons/art"
export KDOS_ICON_MARKS="$PWD/src/packages/kdos-icons/marks"
export KDOS_CURSOR_ART="$PWD/src/packages/kdos-cursors/art"
audit() {
    env -u XDG_CONFIG_HOME -u XDG_CACHE_HOME -u XDG_DATA_HOME \
        HOME="$AH" TMPDIR="$OUT" PATH="$OUT:$PATH" "$OUT/kdos" theme "$@"
}
audit phosphor >/dev/null 2>&1 || { echo "  the generators did not run"; exit 1; }
audit --audit >/dev/null || { echo "  a freshly generated \$HOME did not audit clean"; exit 1; }
echo "  a generated \$HOME audits clean"

sed -i 's/#39ff14/#ff00ff/' "$AH/.config/gtk-3.0/gtk.css"
rm -f "$AH/.icons/KDOS/16x16/places/folder.svg"
echo stray > "$AH/.icons/KDOS/16x16/places/not-ours.svg"
rm -f "$AH/.icons/KDOS-cursors/cursors/left_ptr"
ln -s wait "$AH/.icons/KDOS-cursors/cursors/left_ptr"
OUT_TXT=$(audit --audit || true)
echo "$OUT_TXT" | grep -q "GTK3 palette.*DRIFTED" || { echo "  an edited stylesheet was not caught"; exit 1; }
echo "$OUT_TXT" | grep -q "icon theme.*1 missing 1 not ours" || { echo "  a deleted and a stray icon were not caught"; exit 1; }
echo "$OUT_TXT" | grep -q "cursor theme.*DRIFTED" || { echo "  a re-pointed cursor alias was not caught"; exit 1; }
audit --audit >/dev/null 2>&1 && { echo "  drift did not fail the exit code"; exit 1; }
echo "  an edit, a deletion, a stray file and a re-pointed alias all caught"

audit phosphor >/dev/null 2>&1
audit --audit >/dev/null || { echo "  re-running the accent did not repair it"; exit 1; }
echo "  and \`kdos theme <accent>\` puts it back"

# kdeglobals is the one generated file that is MERGED: KDE apps write their own
# settings into it, so a user's section has to survive an accent change, the
# result has to be byte-stable across runs (an unstable merge makes --audit
# complain forever), and a section we own has to be replaced WHOLE rather than
# appended to — that last one grew the file by a stale colour on every run.
printf '[Dolphin]\nViewMode=2\n\n[Colors:Window]\nBackgroundNormal=35,38,41\n' \
    >> "$AH/.config/kdeglobals"
audit amber >/dev/null 2>&1
grep -q "ViewMode=2" "$AH/.config/kdeglobals" \
    || { echo "  the merge dropped a KDE app's own settings"; exit 1; }
grep -q "BackgroundNormal=35,38,41" "$AH/.config/kdeglobals" \
    && { echo "  a section we own kept a foreign key"; exit 1; }
grep -q "ColorScheme=KDOS" "$AH/.config/kdeglobals" \
    || { echo "  the merge did not apply the KDOS scheme"; exit 1; }
cp "$AH/.config/kdeglobals" "$OUT/kdeglobals.1"
audit amber >/dev/null 2>&1
cmp -s "$OUT/kdeglobals.1" "$AH/.config/kdeglobals" \
    || { echo "  the kdeglobals merge is not idempotent"; exit 1; }
audit --audit amber >/dev/null \
    || { echo "  a merged kdeglobals does not audit clean"; exit 1; }
audit phosphor >/dev/null 2>&1
echo "  kdeglobals merges, keeps foreign keys, and is byte-stable"
rm -rf "$AH"

echo
echo "all good"
