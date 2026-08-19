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
INC="-Isrc/libs/libkbase -Isrc/libs/libkcolor -Isrc/libs/libktui -Isrc/libs/libkxdg -Isrc/libs/libkpkg -Isrc/libs/libkbuild -Isrc/tools/kdos-portup -Isrc/libs/libkproc"
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT

# Worth running this whole script as
#
#     CC="cc -fsanitize=address,undefined -g" testing/selftest.sh
#
# which is how the kb_tar size-field overflow and the kxdg NULL memcpy were
# found. One thing has to be arranged for it, and only one: LEAK CHECKING IS
# OFF BY DEFAULT HERE. Every program below is a one-shot that owns its parsed
# state until it exits — kpkgbuild holds the recipe, kdosbuild holds the plan —
# and LeakSanitizer reports that as a leak and makes the exit code non-zero, so
# a run that SUCCEEDED gets reported as "the synthetic port did not build".
# It stays ON for the library suite, two lines down, because that is the one
# binary here whose subject is code called over and over inside a long-lived
# process, and where a leak is therefore a real defect rather than an exit
# strategy. Both settings are inert without a sanitized CC.
export ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}"

echo "==> selftest"
$CC $STD $WARN $INC -o "$OUT/selftest" src/libs/selftest.c \
    src/libs/libkbase/*.c src/libs/libkcolor/*.c src/libs/libkpkg/*.c \
    src/libs/libkbuild/*.c src/libs/libktui/*.c src/libs/libkproc/*.c \
    src/tools/kdos-portup/extract.c
ASAN_OPTIONS=detect_leaks=1 "$OUT/selftest"

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
    src/libs/libkpkg/*.c src/libs/libkxdg/*.c src/libs/libkproc/*.c
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

# libkcell and libkwl are the TWO libraries here with real external
# dependencies — libkcell is the glyph cache and the cell painter, libkwl is
# the Wayland half built on it. They are separate archives from libktui, whose
# zero-`-l` property keeps kinstall in phase 1, and separate from EACH OTHER so
# that a consumer wanting the cell painter is not made to link a Wayland CLIENT
# library to get it. (kdos-comp does not link either: since the labwc fork its
# window frames are labwc's own SSD, drawn with pango and coloured from the
# generated `themerc-override`.) Skipped rather than failed when the deps are
# absent:
# this script's contract is that it runs on a bare host with no container and
# no network.
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
        tar xf "$LS" -C "$PROTO" --strip-components=2 \
            "$(tar tf "$LS" | grep 'protocol/wlr-screencopy-unstable-v1.xml$' | head -1)" \
            2>/dev/null || true
        "$SCANNER" client-header "$PROTO/wlr-layer-shell-unstable-v1.xml" \
            "$PROTO/wlr-layer-shell-unstable-v1-client-protocol.h"
        "$SCANNER" client-header "$XDG" "$PROTO/xdg-shell-client-protocol.h"
        # The lock role's protocol. Missing it fails the compile rather than
        # skipping quietly, which is the point: libkwl is what the lock screen
        # draws through.
        "$SCANNER" client-header \
            "$(pkg-config --variable=pkgdatadir wayland-protocols)/staging/ext-session-lock/ext-session-lock-v1.xml" \
            "$PROTO/ext-session-lock-v1-client-protocol.h"
        "$SCANNER" client-header \
            "$(pkg-config --variable=pkgdatadir wayland-protocols)/staging/cursor-shape/cursor-shape-v1.xml" \
            "$PROTO/cursor-shape-v1-client-protocol.h"
        # Primary selection: the middle-click paste half of the clipboard.
        # libkwl binds it beside wl_data_device, so it is as mandatory here as
        # the lock role's protocol — a missing header is a compile failure, not
        # a quiet skip.
        "$SCANNER" client-header \
            "$(pkg-config --variable=pkgdatadir wayland-protocols)/unstable/primary-selection/primary-selection-unstable-v1.xml" \
            "$PROTO/primary-selection-unstable-v1-client-protocol.h"
        # The private-code halves. The blocks above only COMPILE, so headers
        # were enough for them; kdos-res LINKS, and an interface referenced
        # with no generated code is an undefined symbol at link rather than a
        # missing header at compile. tablet comes along because
        # cursor-shape-v1's generated code references zwp_tablet_tool_v2.
        _wp="$(pkg-config --variable=pkgdatadir wayland-protocols)"
        "$SCANNER" private-code "$PROTO/wlr-layer-shell-unstable-v1.xml" \
            "$PROTO/wlr-layer-shell-unstable-v1-protocol.c"
        "$SCANNER" private-code "$XDG" "$PROTO/xdg-shell-protocol.c"
        "$SCANNER" private-code \
            "$_wp/staging/ext-session-lock/ext-session-lock-v1.xml" \
            "$PROTO/ext-session-lock-v1-protocol.c"
        "$SCANNER" private-code \
            "$_wp/staging/cursor-shape/cursor-shape-v1.xml" \
            "$PROTO/cursor-shape-v1-protocol.c"
        "$SCANNER" private-code \
            "$_wp/unstable/tablet/tablet-unstable-v2.xml" \
            "$PROTO/tablet-unstable-v2-protocol.c"
        "$SCANNER" private-code \
            "$_wp/unstable/primary-selection/primary-selection-unstable-v1.xml" \
            "$PROTO/primary-selection-unstable-v1-protocol.c"
        KCINC="-Isrc/libs/libkbase -Isrc/libs/libktui -Isrc/libs/libkcolor \
-Isrc/libs/libkcell -Isrc/libs/libkwl"
        # libkcell first and on its OWN: it must compile with no Wayland
        # header anywhere on the command line, because that is the property
        # that lets kdos-comp link it. Handing it $PROTO would let a stray
        # include pass here and fail in the compositor's build.
        for f in src/libs/libkcell/*.c; do
            $CC $STD $WARN -Isrc/libs/libkbase -Isrc/libs/libktui \
                -Isrc/libs/libkcolor -Isrc/libs/libkcell \
                $(pkg-config --cflags fcft pixman-1) \
                -c -o "$OUT/$(basename "$f" .c).o" "$f"
        done
        echo "  libkcell"

        # The whole of libktui is on both link lines below, not a chosen file
        # or two: kcell_paint.c resolves a sprite cell through
        # ktui_sprite_get(), which reaches ktui_draw_cell(), and the cell
        # painter and the cell buffer are one path, and libktui's widgets
        # reach libkbase for the clock. Both libraries link nothing but musl,
        # so taking all of each costs the check nothing.
        #
        # The ASCII engine's two claims, run rather than compiled: a
        # black-to-white ramp must be monotonic in ink, and a vertical bar and
        # a horizontal one must pick DIFFERENT glyphs. The second is the whole
        # difference from aalib, which picks by luminance alone and cannot tell
        # them apart. Its own binary because selftest.c links no fcft — adding
        # one would put a real `-l` on the suite that proves libktui has none.
        $CC $STD $WARN -Isrc/libs/libkbase -Isrc/libs/libktui \
            -Isrc/libs/libkcolor -Isrc/libs/libkcell \
            $(pkg-config --cflags fcft pixman-1) \
            -o "$OUT/asciicheck" testing/fixtures/ascii/asciicheck.c \
            src/libs/libkcell/*.c src/libs/libktui/*.c \
            src/libs/libkbase/*.c \
            $(pkg-config --libs fcft pixman-1)
        "$OUT/asciicheck" >/dev/null
        echo "  asciicheck (ramp monotonic, orientation distinguished)"

        # kcell_paint must not write outside the caller's buffer. A guard region
        # rather than ASan, because the offending store happens inside
        # libpixman's uninstrumented fill loop — see the file's header.
        $CC $STD $WARN -Isrc/libs/libkbase -Isrc/libs/libktui \
            -Isrc/libs/libkcolor -Isrc/libs/libkcell \
            $(pkg-config --cflags fcft pixman-1) \
            -o "$OUT/clipcheck" testing/fixtures/cellclip/clipcheck.c \
            src/libs/libkcell/*.c src/libs/libktui/*.c \
            src/libs/libkbase/*.c \
            $(pkg-config --libs fcft pixman-1)
        "$OUT/clipcheck" >/dev/null
        echo "  clipcheck (no writes past a ragged cell grid)"

        $CC $STD $WARN -c -I"$PROTO" $KCINC \
            $(pkg-config --cflags fcft pixman-1 xkbcommon wayland-client) \
            -o "$OUT/kwl.o" src/libs/libkwl/kwl.c
        for f in src/libs/libkwl/kwl_key.c; do
            $CC $STD $WARN -c -I"$PROTO" $KCINC \
                $(pkg-config --cflags fcft pixman-1 xkbcommon wayland-client) \
                -o "$OUT/$(basename "$f" .c).o" "$f"
        done
        echo "  libkwl"

        # kdos-lock's client half draws through exactly the headers just
        # generated, so it costs one more compile and is the only gate it has.
        $CC $STD $WARN -c -I"$PROTO" -Isrc/libs/libkbase -Isrc/libs/libktui \
            -Isrc/libs/libkcolor -Isrc/libs/libkcell -Isrc/libs/libkwl \
            $(pkg-config --cflags fcft pixman-1 xkbcommon wayland-client) \
            -o "$OUT/kdos-lock.o" src/desktop/kdos-lock/main.c
        echo "  kdos-lock"

        # decocheck is the window frames' test and it is a second PROCESS — a
        # frame is a conversation between a client and a compositor, and a mock
        # of either side would pass while the real pair failed. Running it needs
        # a compositor; compiling it here is what stops it rotting.
        if [ -f "$PROTO/wlr-screencopy-unstable-v1.xml" ]; then
            "$SCANNER" client-header \
                "$PROTO/wlr-screencopy-unstable-v1.xml" \
                "$PROTO/wlr-screencopy-unstable-v1-client-protocol.h"
            # xdg-decoration: decocheck asks for server-side mode, which is the
            # branch that reached an ISO untested because the client did not.
            "$SCANNER" client-header \
                "$(pkg-config --variable=pkgdatadir wayland-protocols)/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml" \
                "$PROTO/xdg-decoration-unstable-v1-client-protocol.h"
            # NOT masked behind the xml check: a decocheck that stops compiling
            # is a real failure, and reporting it as "skipped" is how a test
            # quietly stops being one.
            $CC $STD $WARN -Wno-missing-field-initializers -c -I"$PROTO" \
                -o "$OUT/decocheck.o" testing/fixtures/deco/decocheck.c \
                $(pkg-config --cflags wayland-client)
            echo "  decocheck"
        else
            echo "  decocheck (skipped — no screencopy xml in the tarball)"
        fi

        # kdos-shell wants three more libraries and two more protocols than
        # libkwl does — basu for the tray's bus, alsa for the volume OSD and
        # libpipewire for the recording indicator. Gated separately so a host
        # that has fcft but not those still gets the libkwl and kdos-lock
        # checks above rather than an error.
        if pkg-config --exists basu alsa libpipewire-0.3 2>/dev/null &&
           [ -f "$(pkg-config --variable=pkgdatadir wayland-protocols)/staging/ext-workspace/ext-workspace-v1.xml" ]; then
            "$SCANNER" client-header \
                "$(pkg-config --variable=pkgdatadir wayland-protocols)/staging/ext-workspace/ext-workspace-v1.xml" \
                "$PROTO/ext-workspace-v1-client-protocol.h"
            # Every wlr protocol a kdos-shell source includes, not a chosen
            # subset: the window list needs foreign-toplevel, the clipboard
            # history needs data-control and kdos-display needs
            # output-management, and a missing header stops the compile at
            # whichever file happens to be first.
            for wp in wlr-foreign-toplevel-management-unstable-v1 \
                      wlr-data-control-unstable-v1 \
                      wlr-output-management-unstable-v1; do
                tar xf "$LS" -C "$PROTO" --strip-components=2 \
                    "$(tar tf "$LS" | grep "protocol/$wp.xml\$" | head -1)"
                "$SCANNER" client-header "$PROTO/$wp.xml" \
                    "$PROTO/$wp-client-protocol.h"
            done
            for f in src/desktop/kdos-shell/*.c; do
                $CC $STD $WARN -c -I"$PROTO" -Isrc/desktop/kdos-shell \
                    -Isrc/libs/libkbase -Isrc/libs/libktui -Isrc/libs/libkcolor \
                    -Isrc/libs/libkcell -Isrc/libs/libkwl -Isrc/libs/libkxdg \
                    -Isrc/libs/libkicon -Isrc/libs/libkchrome -Isrc/libs/libkproc \
                    $(pkg-config --cflags fcft pixman-1 xkbcommon wayland-client \
                                 basu alsa libpipewire-0.3) \
                    -o "$OUT/shell-$(basename "$f" .c).o" "$f"
            done
            echo "  kdos-shell ($(ls src/desktop/kdos-shell/*.c | wc -l) files)"

            # kdos-res: the first xdg-toplevel client in this tree, and the
            # one program here that is a TTY program and a window from the
            # same source. Built whole rather than syntax-checked, because its
            # goldens are rendered by running it.
            $CC $STD $WARN -o "$OUT/kdos-res" -I"$PROTO" \
                -DKDOS_RES_VERSION='"'"'"selftest"'"'"' \
                -Isrc/desktop/kdos-res \
                -Isrc/libs/libkbase -Isrc/libs/libktui -Isrc/libs/libkcolor \
                -Isrc/libs/libkcell -Isrc/libs/libkwl -Isrc/libs/libkxdg \
                -Isrc/libs/libkicon -Isrc/libs/libkchrome -Isrc/libs/libkproc \
                $(ls src/desktop/kdos-res/*.c | grep -v resctl.c) \
                src/libs/libkwl/*.c src/libs/libkcell/*.c src/libs/libktui/*.c \
                src/libs/libkcolor/*.c src/libs/libkbase/*.c \
                src/libs/libkxdg/*.c src/libs/libkicon/*.c \
                src/libs/libkchrome/*.c src/libs/libkproc/*.c \
                "$PROTO"/*-protocol.c \
                $(pkg-config --cflags --libs fcft pixman-1 xkbcommon \
                             wayland-client libpng)
            RESBIN="$OUT/kdos-res"
            echo "  kdos-res"

            # The setuid helper, built SEPARATELY and linking libkbase alone:
            # giving a setuid binary the Wayland stack would be handing root a
            # font parser. Compiled here so its three verbs are checked even
            # though nothing in this suite can exercise the bit itself.
            $CC $STD $WARN -o "$OUT/kdos-resctl" -Isrc/libs/libkbase \
                src/desktop/kdos-res/resctl.c src/libs/libkbase/*.c
            echo "  kdos-resctl"
        else
            echo "  kdos-shell (skipped — basu/alsa/libpipewire-0.3 or ext-workspace-v1.xml missing)"
        fi
    fi
else
    echo "  libkwl (skipped — fcft/pixman/xkbcommon/wayland-client not on this host)"
    echo "  kdos-lock, kdos-shell (skipped with it)"
fi

# The compositor's CRT pass. Same shape as the block above and for the same
# reason — wlroots is a port, not something a bare host has — but worth
# compiling wherever it IS available: crt.c is the one file in the tree that
# touches raw GLES2, and its two shader programs are built from a printf format
# whose sampler declaration differs per program. A typo there is a runtime
# failure on a screen nobody here has.
# kdos-comp is a hard fork of labwc and builds with meson out of its own
# tree — generated protocol headers, libxml2, glib, cairo, pango and a
# libinput floor of 1.26 — which is more toolchain than this half-minute
# suite may assume. The graft files (src/kdos-*.c) still get a compile
# gate here, because they are the KDOS-owned code and each includes
# labwc.h + wlroots headers, which is where version drift would bite.
if pkg-config --exists wlroots-0.20 glesv2 egl wayland-server pixman-1 \
        libdrm libpng 2>/dev/null; then
    KC=src/desktop/kdos-comp
    # labwc.h includes the meson-generated config.h; the graft files read
    # none of its flags, so a stub with the defaults is enough here.
    mkdir -p "$OUT/compconf"
    printf '#pragma once\n#define HAVE_XWAYLAND 0\n#define HAVE_NLS 0\n#define HAVE_RSVG 0\n#define HAVE_LIBSFDO 0\n#define HAVE_LIBINPUT_CONFIG_3FG_DRAG_ENABLED_3FG 0\n#define HAVE_LIBINPUT_CONFIG_DRAG_LOCK_ENABLED_STICKY 0\n' \
        > "$OUT/compconf/config.h"
    for f in "$KC"/src/kdos-*.c; do
        $CC $STD -Wall -Wextra -Wno-unused-parameter -c -DWLR_USE_UNSTABLE \
            -I"$KC/include" -I"$OUT/compconf" \
            -Isrc/libs/libkcolor -Isrc/libs/libkbase \
            $(pkg-config --cflags wlroots-0.20 glesv2 egl wayland-server \
                pixman-1 libdrm libpng) \
            -o "$OUT/comp-$(basename "$f" .c).o" "$f"
    done
    echo "  kdos-comp grafts ($(ls "$KC"/src/kdos-*.c | wc -l) files)"
else
    echo "  kdos-comp grafts (skipped — wlroots-0.20/glesv2/egl not on this host)"
fi

# kdos-boxsock is the enforcement half of N1: it is what hands a box a socket
# that is already stamped with a security context, so the compositor's filter
# has something to filter on. It needs only wayland-client and one staging
# protocol, both of which an ordinary host has — no wlroots, no fcft — so
# unlike the two blocks above this one nearly always runs.
if pkg-config --exists wayland-client 2>/dev/null &&
   [ -n "$(pkg-config --variable=pkgdatadir wayland-protocols 2>/dev/null)" ] &&
   [ -f "$(pkg-config --variable=pkgdatadir wayland-protocols)/staging/security-context/security-context-v1.xml" ]; then
    BP="$OUT/bproto"
    mkdir -p "$BP"
    BSCAN=$(pkg-config --variable=wayland_scanner wayland-scanner)
    BXML=$(pkg-config --variable=pkgdatadir wayland-protocols)/staging/security-context/security-context-v1.xml
    "$BSCAN" client-header "$BXML" "$BP/security-context-v1-client-protocol.h"
    "$BSCAN" private-code  "$BXML" "$BP/security-context-v1-protocol.c"
    $CC $STD $WARN -Isrc/libs/libkbase -I"$BP" -o "$OUT/kdos-boxsock" \
        src/desktop/kdos-boxsock/main.c "$BP/security-context-v1-protocol.c" \
        src/libs/libkbase/*.c $(pkg-config --cflags --libs wayland-client)
    echo "  kdos-boxsock"
else
    echo "  kdos-boxsock (skipped — no wayland-client or no security-context-v1.xml)"
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

# ── Skip-if-installed compares the RECIPE, not just the entry ──────────────
#
# A skip that asks only whether a database entry exists ships the previously
# built binary from a build that reports success. The comparison is the recipe
# hash, and it has THREE states — conflating any two of them is a defect:
#   match     skip
#   differ    rebuild
#   unknown   SKIP — a tree with no recorded hashes must not rebuild wholesale
#
# The unknown cases carry the most weight here: they are what makes this safe
# to turn on, and getting them wrong costs a mass rebuild rather than a wrong
# answer, so nothing would report it until it hurt.
echo "==> kpkg skip-if-installed compares the recipe hash"
SR="$OUT/strict"
rm -rf "$SR"; mkdir -p "$SR/ports/demo" "$SR/work" "$SR/pkgs" "$SR/root"
cat > "$SR/ports/demo/kpkgbuild" <<'EOF'
name        = demo
version     = 1.0
release     = 1
description = a synthetic port that exists to have its recipe changed
EOF
cat > "$SR/ports/demo/build.sh" <<'EOF'
install -Dm755 /dev/null "$PKG/usr/bin/demo"
printf 'v1
' > "$PKG/usr/bin/demo"
EOF
ln -sf kdos-kpkg "$OUT/kpkg"
kstrict() {
    env PORT_REPO="$SR/ports" WORK_DIR="$SR/work" PACKAGE_DIR="$SR/pkgs" \
        PKGDB_DIR=/db KPKG_CONF=/nonexistent SOURCE_DATE_EPOCH=1735689600 \
        TZ=UTC "$@" "$OUT/kpkg" install --root "$SR/root" demo 2>&1
}
kstrict >/dev/null || { echo "  the synthetic port did not install"; exit 1; }
SIDE="$SR/root/db/.recipe/demo"
[ -s "$SIDE" ] || { echo "  no recipe-hash sidecar was recorded"; exit 1; }
H1=$(cat "$SIDE")
[ "${#H1}" = 64 ] || { echo "  the sidecar is not a 64-char hash"; exit 1; }

kstrict KPKG_STRICT_RECIPE=1 | grep -q "Nothing to do" \
    || { echo "  an UNCHANGED recipe was rebuilt under strict mode"; exit 1; }

# The recipe changes. Without strict mode nothing happens: the flag is the only
# thing that makes the comparison apply.
printf '# a change\n' >> "$SR/ports/demo/build.sh"
kstrict | grep -q "Nothing to do" \
    || { echo "  strict mode leaked into the default behaviour"; exit 1; }

kstrict KPKG_STRICT_RECIPE=1 | grep -q "Building demo" \
    || { echo "  a CHANGED recipe was not rebuilt under strict mode"; exit 1; }
H2=$(cat "$SIDE")
[ "$H1" != "$H2" ] || { echo "  the sidecar was not updated after the rebuild"; exit 1; }
kstrict KPKG_STRICT_RECIPE=1 | grep -q "Nothing to do" \
    || { echo "  it rebuilt again after the hash was brought up to date"; exit 1; }

# UNKNOWN, both ways. An absent sidecar is every package on a tree that
# predates the mechanism; a corrupt one is a truncated write. Neither may read
# as "changed".
rm -f "$SIDE"
kstrict KPKG_STRICT_RECIPE=1 | grep -q "Nothing to do" \
    || { echo "  a MISSING sidecar was treated as a changed recipe"; exit 1; }
printf 'garbage\n' > "$SIDE"
kstrict KPKG_STRICT_RECIPE=1 | grep -q "Nothing to do" \
    || { echo "  a CORRUPT sidecar was treated as a changed recipe"; exit 1; }
echo "  match skips, drift rebuilds, unknown and corrupt are left alone"

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
# Undo the appended byte. Sized absolutely rather than with `-s -1`: the
# relative form is GNU coreutils', and busybox's truncate rejects it.
_bhsz=$(wc -c < "$BH/repo/tiny-1.0-1.tar.xz")
truncate -s "$((_bhsz - 1))" "$BH/repo/tiny-1.0-1.tar.xz"

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
# THE WORK DIRECTORY FOR THE ACCEPTING CASES MUST NOT BE IN RAM. $OUT is
# `mktemp -d`, which is /tmp, which is tmpfs on essentially every modern Linux
# — exactly what `kdos rebuild` exists to refuse. The repo is on a real
# filesystem by construction, since the sources being checked are in it.
RBW="$PWD/.selftest-rebuild-work"
rm -rf "$RBW"; mkdir -p "$RBW"
rb() { env KDOS_SOURCES="$1" "$OUT/kdos" rebuild "${@:2}"; }
# From a directory that is not a tree and with nothing pointing at one: the
# search falls back to `.`, which is the repo when this script runs from it.
( cd "$OUT" && env -u KDOS_SOURCES "$OUT/kdos" rebuild --dry-run "$OUT/rb" ) \
    >/dev/null 2>&1 \
    && { echo "  a rebuild with no sources was allowed"; exit 1; }
rb /tmp --dry-run "$OUT/rb" >/dev/null 2>&1 \
    && { echo "  a directory that is not a KDOS tree was accepted"; exit 1; }
rb "$PWD" --dry-run "$RBW" >/dev/null 2>&1 \
    || { echo "  this repo was not recognised as a KDOS tree"; exit 1; }
# /dev/shm is tmpfs on any Linux that has it, which is the case this exists for.
if [ -d /dev/shm ]; then
    rb "$PWD" --dry-run /dev/shm/kdos-rebuild-check >/dev/null 2>&1 \
        && { echo "  a work directory in RAM was accepted"; exit 1; }
    rm -rf /dev/shm/kdos-rebuild-check
fi
rb "$PWD" --dry-run "$RBW" 2>&1 | grep -q "nothing was copied" \
    || { echo "  --dry-run did not say it did nothing"; exit 1; }
rm -rf "$RBW"
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
#
# Matched on "powercap" rather than on either refusal's wording: there are two
# legitimate ways to fail here — no energy domain at all, and domains that are
# root-only since PLATYPUS — and which one a machine gives depends on the
# machine. Both name the interface, and the claim under test is that it
# refuses and says why, not which of the two it hit.
"$OUT/kdos-energyd" 2>&1 | grep -q "powercap" \
    || { echo "  an unreadable counter did not stop the daemon"; exit 1; }
KDOS_ENERGYD_SOCKET="$OUT/nothing.sock" "$OUT/kdos-energy" 2>&1 \
    | grep -q "no kdos-energyd" || { echo "  no message for a dead daemon"; exit 1; }
echo "  the fixture replays: nesting, the wrap, the roll-up and the residue"

echo
echo "==> kdos-oomd picks the victim the desktop can afford to lose"
# The trigger is memory pressure and the effect is a SIGKILL, so neither can be
# summoned in a test. What can is the SELECTION, and it is the whole of the
# design: --fixture replays a recorded /proc through the same pick_victim() the
# daemon runs and prints the choice without signalling anybody.
#
# testing/fixtures/oomd/preferred is arranged so that the four exclusions are
# each load-bearing — every process the daemon must NOT choose is LARGER than
# the one it must: pipewire-pulse (2.4 G, protected by comm), kdos-comp (2 G,
# same), a nightly backup at oom_score_adj -1000 (1.6 G), and a kernel thread
# with an empty cmdline (1.2 G). If any one of those checks broke, the answer
# would be that process and not firefox-esr.
$CC $STD $WARN $INC -o "$OUT/kdos-oomd" \
    src/desktop/kdos-oomd/main.c src/libs/libkbase/*.c src/libs/libkproc/*.c
echo "  kdos-oomd"
OM="$OUT/oomd.txt"
"$OUT/kdos-oomd" --fixture testing/fixtures/oomd/preferred > "$OM" \
    || { echo "  the fixture found no candidate at all"; cat "$OM"; exit 1; }
grep -q "would kill firefox-esr (appbox kdos-apps)" "$OM" \
    || { echo "  the boxed app was not chosen or not named with its box"
         cat "$OM"; exit 1; }
for spared in pipewire-pulse kdos-comp backup kswapd0 init; do
    grep -q "would kill $spared" "$OM" \
        && { echo "  $spared was chosen — an exclusion is not holding"; exit 1; }
done
grep -q "avg10 full=41.02 some=61.23" "$OM" \
    || { echo "  the pressure that triggered it was not reported"; exit 1; }
# …and the preference is not ABSOLUTE. Same tree with the host process three
# times the boxed one's size: an alien app relaunches in seconds, but shooting
# a 800 MB browser while a 3.6 G host leak keeps the machine wedged is not a
# trade, it is a second failure.
"$OUT/kdos-oomd" --fixture testing/fixtures/oomd/hostwins > "$OUT/oomd2.txt" \
    || { echo "  the second fixture found no candidate"; exit 1; }
grep -q "would kill kdosbuild (pid 950" "$OUT/oomd2.txt" \
    || { echo "  a boxed victim was preferred over a host process twice its size"
         cat "$OUT/oomd2.txt"; exit 1; }
grep -q "appbox" "$OUT/oomd2.txt" \
    && { echo "  an unboxed victim was reported as boxed"; exit 1; }
echo "  boxed preferred, adj-shielded and protected comms spared, kthreads skipped"

echo
echo "==> kdos-mountd offers the stick and refuses everything else"
# Mounting needs root and a real device, so the ACTION cannot be summoned in a
# test. The SELECTION can, and it is the whole of the design: --fixture replays
# a recorded /sys/block plus two hand-built superblocks through the same scan()
# the daemon serves from, and mounts nothing.
#
# The two REFUSALS matter more than the acceptance. An internal disk must never
# be offered whatever it is formatted with — testing/fixtures/mountd/dev/sda1
# carries a real ext4 superblock precisely so that a broken removable check
# would show up as an extra row rather than as nothing. And a device claimed by
# /etc/fstab is somebody's existing decision, which this daemon does not get to
# second-guess.
$CC $STD $WARN $INC -o "$OUT/kdos-mountd" \
    src/desktop/kdos-mountd/main.c src/libs/libkbase/*.c
MF=testing/fixtures/mountd
MO="$OUT/mountd.txt"
# The mounts file is the fixture's too, for every invocation below: the
# live-session rule reads it to decide whether to refuse optical media, so
# taking it from the host makes the answer depend on whether the host's own
# root happens to be an overlay — which it is inside a container.
export KDOS_MOUNTD_MOUNTS="$MF/mounts"
"$OUT/kdos-mountd" --fixture "$MF/sys" "$MF/dev" > "$MO" 2>&1
grep -q "sdb1	KDOSSTICK	vfat" "$MO" \
    || { echo "  the removable stick was not offered"; cat "$MO"; exit 1; }
grep -q "^2 eligible" "$MO" \
    || { echo "  the stick and the disc were not the whole answer"
         cat "$MO"; exit 1; }
grep -q "sda1" "$MO" \
    && { echo "  the INTERNAL disk was offered"; cat "$MO"; exit 1; }
# The optical disc IS offered on a machine with a real root — this run has one.
# On the LIVE ISO it must not be, and that half cannot be asserted here: the
# rule keys off `/` being an overlay, which is the running system's own
# /proc/mounts and not something a fixture can supply. Recorded rather than
# faked; it was found by kdos-mountd offering the disc it had booted from.
grep -q "sr0	-	iso9660" "$MO" \
    || { echo "  the data disc was not offered"; cat "$MO"; exit 1; }
# The fstab claims the STICK by label and says nothing about the disc, so the
# refusal has to be visible as the stick leaving and the disc staying — "0
# eligible" would also pass if the whole scan had broken.
KDOS_MOUNTD_FSTAB="$MF/fstab" "$OUT/kdos-mountd" --fixture "$MF/sys" "$MF/dev" \
    > "$MO" 2>&1
grep -q "KDOSSTICK" "$MO" \
    && { echo "  a device claimed by fstab was still offered"; cat "$MO"; exit 1; }
grep -q "sr0" "$MO" \
    || { echo "  the fstab entry took the disc with it"; cat "$MO"; exit 1; }
echo "  the stick and the disc are offered; the internal disk and an fstab"
echo "  entry are not"

unset KDOS_MOUNTD_MOUNTS
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

# ── an Exec line is not a whitespace-separated list ──────────────────────
#
# Two shapes out of the SHIPPED appbox that a `strtok(" ")` gets wrong, and
# both looked from the desktop exactly like an application that does not start:
# debian's gsmartcontrol is `Exec="/usr/bin/gsmartcontrol-root"`, whose quotes
# ended up part of the path, and its wesnoth is
# `Exec=sh -c "wesnoth-1.18 >/dev/null 2>&1"`, whose single shell argument was
# handed to sh in three pieces. The rewrite has to preserve the quoting (these
# go into a table that is read back) and the reader has to undo it.
cat > "$IMG/usr/share/applications/gsmart.desktop" <<'DESK'
[Desktop Entry]
Type=Application
Name=GSmartControl
Exec="/usr/bin/gsmart root"
DESK
cat > "$IMG/usr/share/applications/wesnoth.desktop" <<'DESK'
[Desktop Entry]
Type=Application
Name=Wesnoth
Exec=sh -c "wesnoth-1.18 >/dev/null 2>&1"
DESK
rm -rf "$FSR"; mkdir -p "$FSR"
"$OUT/kdos-appbox" genlaunchers "$IMG/usr/share/applications" "$FSR" 2>/dev/null
grep -q '^gsmart	"/usr/bin/gsmart root"$' "$FSR/usr/share/kdos/alien-apps" \
    || { echo "  a quoted Exec did not survive the rewrite as one argument"
         grep '^gsmart' "$FSR/usr/share/kdos/alien-apps"; exit 1; }
grep -q '^wesnoth	sh -c "wesnoth-1.18 >/dev/null 2>&1"$' \
    "$FSR/usr/share/kdos/alien-apps" \
    || { echo "  a quoted shell argument was split by the rewrite"
         grep '^wesnoth' "$FSR/usr/share/kdos/alien-apps"; exit 1; }
# And the READING side: field codes vanish with nothing picked, so
# `mpv -- %U` does not go looking for a file called %U.
$CC $STD $WARN -o "$OUT/execsplit" -Isrc/libs/libkxdg -Isrc/libs/libkbase \
    -x c - src/libs/libkxdg/kxdg_exec.c <<'EOF'
#include <stdio.h>
#include <string.h>
#include "kxdg.h"
static int fail;
static void want(const char *exec, const char *const *f, int nf,
                 const char *expect)
{
        char store[1024], out[1024] = "";
        const char *a[32];
        int n = kxdg_exec_split(exec, f, nf, store, sizeof store, a, 32);
        for (int i = 0; i < n; i++) {
                if (i) strcat(out, "|");
                strcat(out, a[i]);
        }
        if (strcmp(out, expect)) {
                printf("  [%s] -> <%s>, wanted <%s>\n", exec, out, expect);
                fail = 1;
        }
}
int main(void)
{
        const char *one[] = { "/tmp/a b.png" };
        want("\"/usr/bin/gsmart root\"", NULL, 0, "/usr/bin/gsmart root");
        want("sh -c \"a >b 2>&1\"", NULL, 0, "sh|-c|a >b 2>&1");
        want("mpv --pseudo-gui -- %U", NULL, 0, "mpv|--pseudo-gui|--");
        want("gimp-3.0 %U", one, 1, "gimp-3.0|/tmp/a b.png");
        want("foo %%bar %i %c baz", NULL, 0, "foo|%bar|baz");
        want("keep %U codes", NULL, -1, "keep|%U|codes");
        return fail;
}
EOF
"$OUT/execsplit" || { echo "  kxdg_exec_split does not read back what it writes"
                      exit 1; }
echo "  Exec quoting round-trips, and a field code with no file vanishes"

echo
echo "==> kdos-appbox open resolves a path to the thing that opens it"
# `kdos-desk` called `kdos-appbox open` for a release before the subcommand
# existed, so every double-click on the desktop died on "unknown". The
# resolution is the freedesktop one — globs for the MIME type, then
# mimeapps.list, then the mimeinfo.cache genlaunchers writes above — and
# `--print` is what makes it checkable without launching LibreOffice.
#
# XDG_DATA_HOME is pointed at the tree genlaunchers just produced, so this also
# proves the two halves agree: the cache one wrote is the cache the other reads.
OPENH="$OUT/openhome"
rm -rf "$OPENH"
mkdir -p "$OPENH/.config" "$OPENH/files"
: > "$OPENH/files/shot.png"
# The longest suffix has to win, or every .tar.gz opens in a decompressor.
: > "$OPENH/files/roll.tar.gz"
cat > "$OPENH/.config/mimeapps.list" <<'MIME'
[Default Applications]
application/x-compressed-tar=gimp.desktop
MIME
open_print() {
    HOME="$OPENH" XDG_CONFIG_HOME="$OPENH/.config" \
    XDG_DATA_HOME="$FSR/etc/skel/.local/share" XDG_DATA_DIRS="$FSR/usr/share" \
        "$OUT/kdos-appbox" open --print "$1"
}
if [ ! -f /usr/share/mime/globs ]; then
    echo "  kdos-appbox open (skipped — no shared-mime-info on this host)"
else
    out=$(open_print "$OPENH/files/shot.png")
    echo "$out" | grep -q "^mime	image/png$" \
        || { echo "  a .png was not recognised: $out"; exit 1; }
    # image/png reaches gimp only through the mimeinfo.cache above — there is
    # no [Default Applications] line for it.
    echo "$out" | grep -q "gimp.desktop" \
        || { echo "  the mime cache was not consulted: $out"; exit 1; }
    # And the whole chain in one line: genlaunchers rewrote the entry's Exec to
    # go through `kdos-appbox run`, so opening a file with a BOXED app lands on
    # the launch path with all its fixes rather than exec'ing a binary the host
    # does not have.
    echo "$out" | grep -q "^exec	kdos-appbox	run	gimp	$OPENH/files/shot.png$" \
        || { echo "  the field code was not substituted: $out"; exit 1; }

    out=$(open_print "$OPENH/files/roll.tar.gz")
    echo "$out" | grep -q "^mime	application/x-compressed-tar$" \
        || { echo "  the longest suffix did not win: $out"; exit 1; }

    out=$(open_print "$OPENH/files")
    echo "$out" | grep -q "^mime	inode/directory$" \
        || { echo "  a directory was not recognised: $out"; exit 1; }
    # Nothing in this scratch tree claims a directory, so it must fall through
    # to xdg-open rather than inventing a handler.
    echo "$out" | grep -q "^exec	xdg-open$" \
        || { echo "  no fallback for an unclaimed type: $out"; exit 1; }
    echo "  mime by longest glob, mimeapps and the cache, field codes, xdg-open last"
fi

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
echo "==> the FileChooser portal keeps serving while a dialog is open"
# THE REGRESSION THIS EXISTS FOR. The first version forked kdos-pick and sat in
# waitpid() inside the method handler, so the backend answered nothing at all
# for as long as anybody had a file dialog open — a second application's Open
# queued behind the first, and a boxed app asking Settings for the colour
# scheme (which happens on every launch) hung until the dialog was dismissed.
#
# So the test is not "does OpenFile work": it is "does ANOTHER call get an
# answer while OpenFile is still outstanding". A stub chooser that sleeps and
# then prints a URI stands in for a person reading a directory listing.
if [ -n "$TRAY_SDBUS" ] && command -v dbus-daemon >/dev/null 2>&1 &&
   command -v busctl >/dev/null 2>&1; then
    $CC $STD $WARN -o "$OUT/xdp-kdos" \
        $(pkg-config --cflags $TRAY_SDBUS) \
        src/desktop/xdg-desktop-portal-kdos/main.c \
        $(pkg-config --libs $TRAY_SDBUS)
    mkdir -p "$OUT/pbin"
    cat > "$OUT/pbin/kdos-pick" <<'PICK'
#!/bin/sh
sleep 3
echo "file:///tmp/chosen.txt"
PICK
    chmod +x "$OUT/pbin/kdos-pick"
    # OpenURI forwards to `kdos-appbox open`, so the stand-in records what it
    # was handed. The percent-encoding matters: the URI is a URI and the
    # program underneath takes a PATH, and every space in every Downloads
    # folder goes through this decode.
    cat > "$OUT/pbin/kdos-appbox" <<APPBOX
#!/bin/sh
printf '%s\n' "\$*" >> "$OUT/opened.log"
APPBOX
    chmod +x "$OUT/pbin/kdos-appbox"
    : > "$OUT/opened.log"

    PORTAL_ADDR=$(dbus-daemon --session --print-address --fork \
        --print-pid=3 3>"$OUT/portal-bus.pid")
    PORTAL_BUS_PID=$(cat "$OUT/portal-bus.pid")
    PATH="$OUT/pbin:$PATH" DBUS_SESSION_BUS_ADDRESS="$PORTAL_ADDR" \
        "$OUT/xdp-kdos" & PORTAL_PID=$!
    # Let it take its bus name before anything calls it.
    for _ in 1 2 3 4 5 6 7 8 9 10; do
        DBUS_SESSION_BUS_ADDRESS="$PORTAL_ADDR" busctl --user \
            --address="$PORTAL_ADDR" status \
            org.freedesktop.impl.portal.desktop.kdos >/dev/null 2>&1 && break
        sleep 0.2
    done

    # OpenFile in the background; it cannot return for 3 seconds.
    ( busctl --address="$PORTAL_ADDR" call \
        org.freedesktop.impl.portal.desktop.kdos \
        /org/freedesktop/portal/desktop \
        org.freedesktop.impl.portal.FileChooser OpenFile \
        "osssa{sv}" /org/f/p/r1 app.Test "" "Open" 0 \
        > "$OUT/portal-open.out" 2>&1 ) & OPEN_PID=$!
    sleep 1   # the chooser is now up and the call is outstanding

    # …and Settings must answer NOW, not in two seconds' time.
    t0=$(date +%s%N)
    DBUS_SESSION_BUS_ADDRESS="$PORTAL_ADDR" timeout 2 busctl \
        --address="$PORTAL_ADDR" call \
        org.freedesktop.impl.portal.desktop.kdos \
        /org/freedesktop/portal/desktop \
        org.freedesktop.impl.portal.Settings Read ss \
        org.freedesktop.appearance color-scheme > "$OUT/portal-set.out" 2>&1
    set_rc=$?
    t1=$(date +%s%N)
    took=$(( (t1 - t0) / 1000000 ))

    # OpenURI — "open this on the host for me", which is what a containerised
    # application asks when a link or a downloaded file is clicked. There was no
    # backend for it at all, so the click did nothing, silently. It must answer
    # AT ONCE (there is no dialog) and hand the decoded path to the program that
    # knows what opens it.
    DBUS_SESSION_BUS_ADDRESS="$PORTAL_ADDR" timeout 2 busctl \
        --address="$PORTAL_ADDR" call \
        org.freedesktop.impl.portal.desktop.kdos \
        /org/freedesktop/portal/desktop \
        org.freedesktop.impl.portal.OpenURI OpenURI \
        "osssa{sv}" /org/f/p/r2 app.Test "" \
        "file:///tmp/a%20b.txt" 0 > "$OUT/portal-uri.out" 2>&1
    uri_rc=$?

    wait $OPEN_PID 2>/dev/null
    kill $PORTAL_PID 2>/dev/null
    kill "$PORTAL_BUS_PID" 2>/dev/null

    [ "$uri_rc" = 0 ] || {
        echo "  OpenURI did not answer: $(cat "$OUT/portal-uri.out")"; exit 1; }
    grep -q "^ua{sv} 0 " "$OUT/portal-uri.out" || {
        echo "  OpenURI did not report success: $(cat "$OUT/portal-uri.out")"
        exit 1; }
    # The child is double-forked, so give it a moment to land.
    for _ in 1 2 3 4 5 6 7 8 9 10; do
        [ -s "$OUT/opened.log" ] && break
        sleep 0.2
    done
    grep -qx "open /tmp/a b.txt" "$OUT/opened.log" || {
        echo "  OpenURI did not hand the decoded path to kdos-appbox: $(cat "$OUT/opened.log")"
        exit 1; }

    [ "$set_rc" = 0 ] || {
        echo "  Settings blocked behind the open dialog (rc=$set_rc)"
        cat "$OUT/portal-set.out"; exit 1; }
    [ "$took" -lt 1000 ] || {
        echo "  Settings answered, but only after ${took}ms — it waited for the dialog"
        exit 1; }
    grep -q "^v u 1$" "$OUT/portal-set.out" || {
        echo "  color-scheme is not 'prefer dark': $(cat "$OUT/portal-set.out")"
        exit 1; }
    # And the deferred reply really is the chooser's answer.
    grep -q "file:///tmp/chosen.txt" "$OUT/portal-open.out" || {
        echo "  the deferred OpenFile reply lost the URI"
        cat "$OUT/portal-open.out"; exit 1; }
    grep -q "^ua{sv} 0 " "$OUT/portal-open.out" || {
        echo "  OpenFile did not report success: $(cat "$OUT/portal-open.out")"
        exit 1; }
    echo "  Settings answered in ${took}ms with a dialog open; the deferred reply carried the URI"
    echo "  OpenURI answered at once and handed the decoded path on"
else
    echo "  portal (skipped — no sd-bus, dbus-daemon or busctl on this host)"
fi

echo
echo "==> the shell's front ends draw offscreen, and the boxes line up"
# WHAT THIS CATCHES. Six geometry defects have shipped in this toolkit and not
# one was visible to a compiler: text drawn over a box border, a button on top
# of the hint row, a column that drifted out from under its own header. They are
# only visible when somebody LOOKS at the grid — so the grid is printed, without
# a compositor, and the shape of it is asserted.
#
# libkwl is stubbed (testing/fixtures/shell/dumpmain.c), which is what makes it
# runnable on a host with no fcft and no wlroots — the dump path touches
# neither.
#
# It DOES need libwayland-client now, and that is not a loosening: menu.c and
# shell.c reach for wlr-foreign-toplevel and ext-workspace on the interactive
# path (the window list a task chip opens, the workspace names the strip
# draws), so the generated glue is compiled in whether the dump calls it or
# not. wayland-client is on nearly every host; fcft and wlroots are not, which
# is the distinction this harness was built around and still keeps.
DUMPCK=""
DPROTO="$OUT/dproto"
DWLR=$(ls ports/core/wlroots/wlroots-*.tar.gz 2>/dev/null | head -1)
DSCAN=$(pkg-config --variable=wayland_scanner wayland-scanner 2>/dev/null || true)
DWP=$(pkg-config --variable=pkgdatadir wayland-protocols 2>/dev/null || true)
if pkg-config --exists wayland-client 2>/dev/null && [ -n "$DSCAN" ] &&
   [ -n "$DWLR" ] && [ -n "$DWP" ] &&
   [ -f "$DWP/staging/ext-workspace/ext-workspace-v1.xml" ]; then
    mkdir -p "$DPROTO"
    tar xf "$DWLR" -C "$DPROTO" --strip-components=2 \
        "$(tar tf "$DWLR" | grep 'protocol/wlr-foreign-toplevel-management-unstable-v1.xml$' | head -1)"
    for x in "$DPROTO/wlr-foreign-toplevel-management-unstable-v1.xml" \
             "$DWP/staging/ext-workspace/ext-workspace-v1.xml"; do
        b=$(basename "$x" .xml)
        "$DSCAN" client-header "$x" "$DPROTO/$b-client-protocol.h"
        "$DSCAN" private-code  "$x" "$DPROTO/$b-protocol.c"
    done

    # The four original surfaces, plus any Phase B front end that has landed.
    # dumpmain.c declares every entry point WEAK, so a file that is not on the
    # tree yet is a name it declines rather than a link error.
    # libkchrome is the header band, group headings and button bar the device
    # surfaces share; fav.c is the favourites store several of them write.
    # Neither is a front end, so both belong in the base set rather than in the
    # candidate loop — a surface that uses them would otherwise fail to LINK,
    # which the harness reports as "the new front ends do not link" and which
    # reads as a defect in those files.
    DFRONTS="src/desktop/kdos-shell/cal.c src/desktop/kdos-shell/menu.c
             src/desktop/kdos-shell/launcher.c src/desktop/kdos-shell/pick.c
             src/desktop/kdos-shell/shell.c src/desktop/kdos-shell/apps.c
             src/desktop/kdos-shell/fav.c src/libs/libkchrome/kch_chrome.c"
    # A new surface may want alsa or an sd-bus; offer them when the host has
    # them rather than making the whole harness conditional on either.
    DEXTRA_PC=""
    pkg-config --exists alsa 2>/dev/null && DEXTRA_PC="alsa"
    [ -n "$TRAY_SDBUS" ] && DEXTRA_PC="$DEXTRA_PC $TRAY_SDBUS"

    # Each candidate is admitted on its OWN compile, not the batch's: one file
    # that does not build must cost its own golden and nobody else's.
    DNEW=""
    DBAD=""
    for s in keys teams saver slit doc settings openwith audio \
             start net bt devices notify status tip; do
        [ -f "src/desktop/kdos-shell/$s.c" ] || continue
        if $CC $STD $WARN -fsyntax-only -I"$DPROTO" \
                -Isrc/desktop/kdos-shell -Isrc/libs/libkwl -Isrc/libs/libktui \
                -Isrc/libs/libkcolor -Isrc/libs/libkxdg -Isrc/libs/libkbase \
                -Isrc/libs/libkicon -Isrc/libs/libkchrome -Isrc/libs/libkproc \
                $(pkg-config --cflags wayland-client $DEXTRA_PC) \
                "src/desktop/kdos-shell/$s.c" 2>"$OUT/dump-$s.err"; then
            DNEW="$DNEW src/desktop/kdos-shell/$s.c"
        else
            DBAD="$DBAD $s"
        fi
    done
    [ -n "$DBAD" ] && {
        echo "  NOTE: these front ends do not compile, so their dumps and"
        echo "        goldens are skipped:$DBAD"
        for s in $DBAD; do
            grep -m1 "error:" "$OUT/dump-$s.err" | sed 's/^/        /'
        done
    }
    dumpbuild() {
        # The mime glob table is a COMPILED file that only exists on a booted
        # target (update-mime-database writes it in a postinstall), so the
        # harness is pointed at the fixture's copy. Without it kdos-openwith
        # resolves every file to application/octet-stream and the two traps
        # the fixture carries — longest suffix wins, and the default beats the
        # cache's first entry — are never exercised.
        $CC $STD $WARN -o "$OUT/dumpcheck" -I"$DPROTO" \
            -DKXDG_MIME_GLOBS="\"$PWD/testing/fixtures/openwith/data/mime/globs\"" \
            -Isrc/desktop/kdos-shell -Isrc/libs/libkwl -Isrc/libs/libktui \
            -Isrc/libs/libkcolor -Isrc/libs/libkxdg -Isrc/libs/libkbase \
            -Isrc/libs/libkicon -Isrc/libs/libkchrome -Isrc/libs/libkproc \
            -Wl,--wrap=ktui_offscreen_init \
            testing/fixtures/shell/dumpmain.c $DFRONTS "$@" \
            "$DPROTO"/*-protocol.c \
            src/libs/libktui/*.c src/libs/libkcolor/*.c src/libs/libkxdg/*.c \
            src/libs/libkbase/*.c src/libs/libkproc/*.c \
            $(pkg-config --cflags --libs wayland-client $DEXTRA_PC)
    }
    if [ -n "$DNEW" ] && dumpbuild $DNEW 2>"$OUT/dumpnew.err"; then
        DUMPCK="$OUT/dumpcheck"
        echo "  harness: cal menu launcher pick $(echo $DNEW | \
            sed 's,src/desktop/kdos-shell/,,g; s,\.c,,g')"
    elif dumpbuild; then
        # Every candidate compiled on its own, so a failure here is a LINK
        # failure — a surface wanting a library this harness does not offer.
        DUMPCK="$OUT/dumpcheck"
        [ -n "$DNEW" ] && {
            echo "  NOTE: the new front ends do not LINK into the dump harness,"
            echo "        so their goldens are skipped:$(echo $DNEW | \
                sed 's,src/desktop/kdos-shell/,,g; s,\.c,,g')"
            grep -m3 "undefined\|error" "$OUT/dumpnew.err" | sed 's/^/        /'
        }
    else
        echo "  the dump harness does not build"; exit 1
    fi
else
    echo "  front-end dumps (skipped — no wayland-client, wayland-scanner,"
    echo "    ext-workspace-v1.xml or wlroots tarball on this host)"
fi

# Every row the same width, which is the whole of "the box lines up": a frame
# whose bottom border is shorter than its top is a rect that was drawn past the
# surface, and that is exactly how the notification daemon once painted an empty
# box in the corner.
check_box() {
    awk -v what="$1" '
        { n = length($0)
          if (w == 0) w = n
          else if (n != w) { printf "  %s: row %d is %d wide, the box is %d\n",
                             what, NR, n, w; bad = 1 } }
        END { if (NR < 4) { printf "  %s: drew %d rows\n", what, NR; bad = 1 }
              exit bad }
    ' || exit 1
}

# Body deliberately unindented: it is a long stretch of assertions that used to
# be top-level, and reindenting all of it would bury the one thing that changed.
if [ -n "$DUMPCK" ]; then
"$DUMPCK" cal --dump > "$OUT/dump-cal.txt"
check_box cal < "$OUT/dump-cal.txt"
grep -q "Mo Tu We Th Fr Sa Su" "$OUT/dump-cal.txt" || {
    echo "  the calendar lost its weekday header"; exit 1; }

"$DUMPCK" menu system --dump > "$OUT/dump-menu.txt"
check_box menu < "$OUT/dump-menu.txt"
grep -q "Shut Down" "$OUT/dump-menu.txt" || {
    echo "  the System menu lost its last row — the box is shorter than the list"
    exit 1; }

"$DUMPCK" launcher --dump > "$OUT/dump-launcher.txt"
check_box launcher < "$OUT/dump-launcher.txt"

"$DUMPCK" pick --dump > "$OUT/dump-pick.txt"
check_box pick < "$OUT/dump-pick.txt"
# The chooser's two buttons and the hint text share one row, and the row is the
# narrowest thing in this dialog. Both present means neither pushed the other
# off the end — the check that would have caught a hint wide enough to overwrite
# the Open button.
grep -q "\[ Open \]" "$OUT/dump-pick.txt" || {
    echo "  kdos-pick has no Open button"; exit 1; }
grep -q "\[ Cancel \].*\[ Open \]" "$OUT/dump-pick.txt" || {
    echo "  kdos-pick's buttons are not both on their row"; exit 1; }
grep -q "Esc cancel.*\[ Cancel \]" "$OUT/dump-pick.txt" || {
    echo "  kdos-pick's hint row and its buttons collide"; exit 1; }
echo "  cal, menu, launcher and pick draw square boxes with their controls in them"

echo
echo "==> golden frames — the committed cell grid, diffed"
# G14. check_box above proves the frame is SQUARE; a golden proves it is the
# SAME. That is the net S1 (stale rows in the other swap buffer) went through
# untouched: every row was the right width and every row was wrong.
#
# A golden is only committed for a dump that is deterministic on ANY host, and
# that rules a good deal of this desktop out. Left out, and why:
#
#   cal        draws the CURRENT month and honours no date override
#   launcher   scans /usr/share/applications, which is the host's
#   menu apps  likewise
#   menu places reads /proc/mounts and the $HOME xdg dirs
#   panel      needs a live compositor to dump at all — it reads real protocol
#              state on purpose (see panel.c)
#   saver      seeds from time() ^ getpid(); phosphor rain is never twice the
#              same picture, which is the point of it
#   slit       renders the OUTPUT of forked gadget commands, arriving
#              asynchronously — a dump catches whatever had answered by then
#   openwith   its header carries the file's absolute path. Its resolution is
#              checked below instead, which is the part that can be wrong
#   net bt     both need a system bus, and what is ON it — an access point
#              list, a paired headset — is the machine's, not a fixture's
#   devices    /dev/video* and /proc/asound are the host's
#
# What is goldened reads its inputs from testing/fixtures/shell: `tree/` for
# pick and `config/` for the surfaces that parse one (a frozen rc.xml for the
# keybind card, a comp.conf for settings), so a golden cannot move because
# somebody edited skel.
#
# Each is rendered at two sizes: KDOS_DUMP_SIZE overrides the geometry a
# surface asked for (dumpmain.c wraps ktui_offscreen_init), which is also the
# only check there is that a draw pass does not assume the buffer is exactly
# the size it hoped for.
#
# The dumps are ASCII: ktui_caps is 0 with no terminal, so every surface draws
# in the ascii tier here. The rich and vt tiers are covered by the ramp
# assertions in src/libs/selftest.c instead.
GOLD="$PWD/testing/goldens"
golden_fail=0
golden() {			# <name> <WxH> <argv…>
    _g_name=$1; _g_size=$2; shift 2
    _g_file="$GOLD/$_g_name-$_g_size.txt"
    _g_got="$OUT/golden-$_g_name-$_g_size.txt"
    ( cd testing/fixtures/shell &&
      env LC_ALL=C TZ=UTC HOME="$PWD" \
          XDG_CACHE_HOME=/nonexistent-kdos-cache \
          XDG_CONFIG_HOME="$PWD/config" \
          XDG_DATA_HOME=/nonexistent-kdos-data \
          XDG_DATA_DIRS=/nonexistent-kdos-datadirs \
          XDG_RUNTIME_DIR=/nonexistent-kdos-run \
          KDOS_DUMP_SIZE="$_g_size" "$DUMPCK" "$@" ) > "$_g_got"
    if [ "${KDOS_GOLDEN_UPDATE:-0}" = 1 ]; then
        mkdir -p "$GOLD"
        cp "$_g_got" "$_g_file"
        echo "  wrote $_g_name-$_g_size"
        return 0
    fi
    if [ ! -f "$_g_file" ]; then
        echo "  $_g_name-$_g_size: no golden committed"
        golden_fail=1
        return 0
    fi
    if diff -u "$_g_file" "$_g_got" > "$OUT/golden-$_g_name-$_g_size.diff"; then
        echo "  $_g_name-$_g_size"
    else
        echo "  $_g_name-$_g_size DRIFTED:"
        head -30 "$OUT/golden-$_g_name-$_g_size.diff" | sed 's/^/    /'
        golden_fail=1
    fi
}
# kdos-res is its own binary, not a kdos-shell front end, so it renders its
# own goldens against testing/fixtures/res — a recorded machine, which is what
# makes a monitor's output deterministic at all. It is built above only where
# the Wayland stack exists; where it is not, its goldens are skipped with a
# name rather than silently dropped.
if [ -n "${RESBIN:-}" ] && [ -x "$RESBIN" ]; then
    res_golden() {          # <page> <WxH>
        _r_file="$GOLD/res-$1-$2.txt"
        _r_got="$OUT/golden-res-$1-$2.txt"
        env LC_ALL=C TZ=UTC XDG_CACHE_HOME=/nonexistent-kdos-cache \
            XDG_CONFIG_HOME=/nonexistent-kdos-config \
            "$RESBIN" --fixture testing/fixtures/res --dump \
            --page "$1" --dump-size "$2" > "$_r_got"
        if [ "${KDOS_GOLDEN_UPDATE:-0}" = 1 ]; then
            cp "$_r_got" "$_r_file"; echo "  wrote res-$1-$2"; return 0
        fi
        if [ ! -f "$_r_file" ]; then
            echo "  res-$1-$2: no golden committed"; golden_fail=1; return 0
        fi
        if diff -u "$_r_file" "$_r_got" > "$OUT/golden-res-$1-$2.diff"; then
            echo "  res-$1-$2"
        else
            echo "  res-$1-$2 DRIFTED:"
            head -30 "$OUT/golden-res-$1-$2.diff" | sed 's/^/    /'
            golden_fail=1
        fi
    }
    for _p in applications processes cpu memory gpu drives network \
              batteries energy; do
        [ -f "$GOLD/res-$_p-80x24.txt" ] || continue
        res_golden "$_p" 80x24
        res_golden "$_p" 132x43
    done
else
    echo "  kdos-res goldens (skipped — the binary was not built on this host)"
fi

golden menu-system 80x24  menu system --dump
golden menu-system 132x43 menu system --dump
golden pick        80x24  pick --dir tree --dump
golden pick        132x43 pick --dir tree --dump
# The Phase B surfaces, each only if it linked in. `--have` is dumpmain.c
# answering for its own weak symbols, so a surface that has not landed is a
# skip with a name on it rather than a silent gap.
for _s in keys teams doc settings start notify; do
    if "$DUMPCK" --have "$_s"; then
        golden "$_s" 80x24  "$_s" --dump
        golden "$_s" 132x43 "$_s" --dump
    elif [ -f "$GOLD/$_s-80x24.txt" ]; then
        # A COMMITTED golden that stops being asserted is a test weakening
        # itself in response to a regression: the surface used to link into
        # this harness and no longer does, which is the change to look at.
        echo "  $_s: a golden is committed but the surface no longer links"
        golden_fail=1
    else
        echo "  $_s (skipped — not linked into the harness)"
    fi
done
# The overflow popup reads its list from a FILE the panel writes, so the golden
# gets a fixture rather than whatever this machine's own panel published a
# moment ago — three rows, one of them wanting attention and one of them the
# tray item whose menu this desktop cannot draw.
if "$DUMPCK" --have status; then
    golden status 80x24  status --from status.tbl --dump
    golden status 132x43 status --from status.tbl --dump
elif [ -f "$GOLD/status-80x24.txt" ]; then
    echo "  status: a golden is committed but the surface no longer links"
    golden_fail=1
else
    echo "  status (skipped — not linked into the harness)"
fi

# The keybind card's one claim beyond its shape: rc.xml documents itself with
# commented-out bindings, and every one of them would be advertised as live if
# strip_comments ever stopped running first. The fixture carries exactly one.
if "$DUMPCK" --have keys; then
    grep -q "never-bound" "$OUT/golden-keys-80x24.txt" \
        && { echo "  the keybind card advertised a commented-out binding"
             exit 1; }
fi
if [ "$golden_fail" != 0 ]; then
    echo
    echo "  A golden frame changed. If the change is intended:"
    echo "      KDOS_GOLDEN_UPDATE=1 testing/selftest.sh"
    echo "  then read the diff in git before committing it."
    exit 1
fi

# --dump-cells is the other half of the GOLDEN FRAMES contract: one line per
# non-blank cell, `row col U+XXXX fg bg attr`, which is what makes a COLOUR
# regression visible as well as a geometry one. No surface implements it yet;
# said out loud rather than quietly not run, because a check that silently
# covers nothing is worse than one that is missing.
if "$DUMPCK" menu system --dump-cells >"$OUT/cells.txt" 2>/dev/null &&
   grep -qE '^[0-9]+ [0-9]+ U\+[0-9A-Fa-f]+ ' "$OUT/cells.txt"; then
    echo "  --dump-cells answers; cell goldens are not committed yet"
else
    echo "  --dump-cells: not implemented by any surface — skipped"
fi

# G7's chooser resolves a file the same way `kdos-appbox open` does, so it
# inherits the same two traps: the LONGEST matching suffix wins (or every
# .tar.gz opens in a decompressor), and mimeapps.list's [Default Applications]
# beats whatever mimeinfo.cache happens to list first. testing/fixtures/openwith
# is a complete XDG data/config pair carrying exactly that pair of cases —
# *.gz against *.tar.gz, and a cache whose FIRST handler is not the default.
if "$DUMPCK" --have openwith; then
    OW="$PWD/testing/fixtures/openwith"
    ow_rc=0
    env HOME="$OW" XDG_DATA_HOME="$OW/data" XDG_DATA_DIRS="$OW/data" \
        XDG_CONFIG_HOME="$OW/config" \
        "$DUMPCK" openwith --print "$OW/files/roll.tar.gz" \
        > "$OUT/openwith.txt" 2>&1 || ow_rc=$?
    if [ "$ow_rc" != 0 ]; then
        echo "  kdos-openwith --print exited $ow_rc"
        cat "$OUT/openwith.txt"; exit 1
    fi
    grep -q "application/x-compressed-tar" "$OUT/openwith.txt" \
        || { echo "  the longest suffix did not win: $(cat "$OUT/openwith.txt")"
             exit 1; }
    grep -q "filezip" "$OUT/openwith.txt" \
        || { echo "  [Default Applications] did not beat the cache order"
             cat "$OUT/openwith.txt"; exit 1; }
    echo "  kdos-openwith: longest suffix wins and the default handler overrides"
else
    echo "  kdos-openwith (skipped — the surface has not landed)"
fi
else
    echo "  the front-end dumps and their goldens are skipped with the harness"
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
        -Isrc/libs/libkxdg -Isrc/libs/libkbase -Isrc/libs/libkicon \
        -Isrc/libs/libkchrome \
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
