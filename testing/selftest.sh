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
    src/tools/kdos-portup/vercmp.c src/tools/kdos-portup/extract.c
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
$CC $STD $WARN $INC -Isrc/packages/kdos-kpkg -o "$OUT/kdos-kpkg" \
    src/packages/kdos-kpkg/*.c src/libs/libkbase/*.c src/libs/libkpkg/*.c
echo "  kdos-kpkg"
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

echo
echo "all good"
