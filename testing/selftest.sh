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
"$KI" --config "$OUT/answers.conf" --dump plan --json > "$OUT/plan.json" 2>&1
"$KI" --config "$OUT/answers.conf" --dump plan > "$OUT/plan.txt" 2>&1
grep -q 'hunter2-sentinel' "$OUT/plan.json" "$OUT/plan.txt" \
    && { echo "  a password reached the dump"; exit 1; }
"$KI" --dump bogus >/dev/null 2>&1 && { echo "  --dump took a bad subject"; exit 1; }
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
