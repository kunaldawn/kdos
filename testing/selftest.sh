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
INC="-Isrc/libs/libkbase -Isrc/libs/libkwm -Isrc/libs/libkvt -Isrc/libs/libkcon -Isrc/libs/libkdisp -Isrc/libs/libkcolor -Isrc/libs/libktui -Isrc/libs/libkxdg -Isrc/libs/libkpkg -Isrc/libs/libkbuild -Isrc/tools/kdos-portup -Isrc/libs/libkproc -Isrc/libs/libksig -Isrc/libs/libkpack"
OUT=$(mktemp -d)

# WHICH sd-bus THIS HOST HAS, decided ONCE and up here because two blocks a
# thousand lines apart both ask. KDOS ships basu; nearly every development host
# has libsystemd, and the API is the same one. Deciding it late meant the
# kdos-shell block read an unset variable and skipped itself on every host,
# which is why the front-end goldens behind it went unlooked-at.
TRAY_SDBUS=""
pkg-config --exists basu 2>/dev/null && TRAY_SDBUS=basu
[ -z "$TRAY_SDBUS" ] && pkg-config --exists libsystemd 2>/dev/null && \
    TRAY_SDBUS=libsystemd

#
# THE FLAGS FOR kdos-shell AND kdos-res, and they are $WARN minus exactly one
# thing. This program TRUNCATES ON PURPOSE: every label it draws goes into a
# fixed number of CELLS, so a `%s` into a fixed field is the intended
# behaviour and -Wformat-truncation fires on a dozen of them. The cases where
# truncating IS a defect are not label fields — a socket path, a device node —
# and those are held by a rule (`SH_SOCK_MAX`, `DV_DEVPATH`) rather than by a
# warning that cannot tell the two apart. The shipped recipes build with
# `-Wall -Wextra` and no `-Werror` at all; everything else in $WARN stays on
# here, which is what caught a dead choice list in settings.c.
#
# Defined once, up here: the kdos-shell block and the dump harness compile the
# same sources a thousand lines apart, and flags that disagree mean one of them
# fails on what the other accepted.
SHWARN="$WARN -Wno-format-truncation"
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

#
# libkimg is OPTIONAL in this build, and guarded the way the kdos-shell block
# is. Its four decoders are ports and none of them is on a bare host, so the
# corpus is checked wherever they exist and the rest of the suite still runs
# where they do not — a library nobody can build is a library nobody runs the
# assertions for, which is worse than a skip that says so.
#
KIMG_SRC=""
KIMG_FLAGS=""
if pkg-config --exists pixman-1 2>/dev/null; then
    KIMG_FLAGS="-DHAVE_KIMG -Isrc/libs/libkimg $(pkg-config --cflags pixman-1)"
    KIMG_SRC="src/libs/libkimg/kimg.c"
    KIMG_LIBS="$(pkg-config --libs pixman-1)"
    for f in PNG:libpng JPEG:libjpeg WEBP:libwebp SIXEL:libsixel; do
        _d=${f%%:*}
        _p=${f#*:}
        pkg-config --exists "$_p" 2>/dev/null || continue
        KIMG_FLAGS="$KIMG_FLAGS -DKIMG_HAVE_$_d $(pkg-config --cflags "$_p")"
        KIMG_LIBS="$KIMG_LIBS $(pkg-config --libs "$_p")"
    done
fi

echo "==> selftest"
$CC $STD $WARN $INC $KIMG_FLAGS -o "$OUT/selftest" src/libs/selftest.c $KIMG_SRC \
    src/libs/libkbase/*.c src/libs/libkcolor/*.c src/libs/libkpkg/*.c \
    src/libs/libkbuild/*.c src/libs/libktui/*.c src/libs/libkproc/*.c \
    src/libs/libkxdg/*.c src/libs/libksig/*.c src/libs/libksig/monocypher/*.c \
    src/libs/libkpack/*.c src/libs/libkwm/*.c src/libs/libkvt/*.c \
    src/libs/libkcon/*.c src/libs/libkdisp/*.c \
    src/tools/kdos-portup/extract.c $KIMG_LIBS
ASAN_OPTIONS=detect_leaks=1 "$OUT/selftest"

#
# THE CORPUS, MUTATED. The block inside selftest.c checks that libkimg gives
# the right answer for files somebody wrote by hand; this checks that it gives
# SOME answer, and reads nothing it was not given, for bytes nobody wrote — every
# fixture truncated at every length and with every byte flipped three ways.
#
# Its own binary because it is worth running under the sanitisers on its own:
# this is the only place in KDOS where untrusted image bytes reach a decoder,
# and a corpus of valid files exercises none of the paths that matter.
#
if [ -n "$KIMG_SRC" ]; then
    echo
    echo "==> libkimg under mutation"
    $CC $STD $WARN $KIMG_FLAGS -o "$OUT/kimgfuzz" \
        testing/fixtures/img/fuzz.c $KIMG_SRC $KIMG_LIBS
    ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
        "$OUT/kimgfuzz" testing/fixtures/img/*.png testing/fixtures/img/*.jpg \
        testing/fixtures/img/*.webp testing/fixtures/img/*.six | sed 's/^/  /'
fi

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
# libkpack (over libksig) is `kdos app update`: reading a PACKAGES index and
# picking the delta is the same parser kdos-packd uses.
$CC $STD $WARN $INC -Isrc/libs/libksig -Isrc/libs/libkpack \
    -Isrc/packages/kdos-tools -o "$OUT/kdos-tools" \
    src/packages/kdos-tools/*.c src/libs/libkbase/*.c src/libs/libkcolor/*.c \
    src/libs/libkpkg/*.c src/libs/libkxdg/*.c src/libs/libkproc/*.c \
    src/libs/libksig/*.c src/libs/libksig/monocypher/*.c src/libs/libkpack/*.c
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
$CC $STD $WARN $INC -Isrc/libs/libkproc -Isrc/desktop/kdos-energyd \
    -o "$OUT/kdos-energyd" \
    src/desktop/kdos-energyd/*.c src/libs/libkbase/*.c src/libs/libkproc/*.c
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

# kdos-packd is the sixth root daemon and the only thing in the tree that
# mounts. Its ANSWER is checkable without root: --fixture runs the same scan,
# the same solve and the same graft rules and mounts nothing, which is the seam
# `kdos stutter`, `kdos-oomd` and `kdos-mountd` all use.
$CC $STD $WARN $INC -Isrc/desktop/kdos-packd -o "$OUT/kdos-packd" \
    src/desktop/kdos-packd/*.c src/libs/libkbase/*.c src/libs/libksig/*.c \
    src/libs/libksig/monocypher/*.c src/libs/libkpkg/*.c src/libs/libkpack/*.c
echo "  kdos-packd"
$CC $STD $WARN $INC -o "$OUT/kdos-pack" \
    src/packages/kdos-pack/main.c src/libs/libkbase/*.c src/libs/libksig/*.c \
    src/libs/libksig/monocypher/*.c src/libs/libkpkg/*.c src/libs/libkpack/*.c
echo "  kdos-pack"

echo
echo "==> boxes: the profile says what it enforced, and what it could not"
$CC $STD $WARN $INC -Isrc/packages/kdos-appbox -o "$OUT/kdos-box" \
    src/packages/kdos-appbox/*.c src/libs/libkbase/*.c src/libs/libktui/*.c \
    src/libs/libkcolor/*.c src/libs/libkxdg/*.c
BH="$OUT/boxhome"
mkdir -p "$BH/.config/kdos/boxes"
cp testing/fixtures/box/*.conf "$BH/.config/kdos/boxes/"
# The binary dispatches on its own BASENAME, which is what makes the same file
# kdos-box here and kdos-appbox on the launch path — the property build.sh's
# symlink relies on, exercised rather than assumed.
BP="$OUT/box.txt"
HOME="$BH" "$OUT/kdos-box" profile devbox > "$BP" 2>&1 || true
grep -q "unknown key 'nonsense'" "$BP" \
    || { echo "  FAIL  an unknown profile key is reported by name"; cat "$BP"; exit 1; }
echo "  ok    an unknown key is reported by name, not ignored"
grep -q "network     = private     --unshare-netns" "$BP" \
    || { echo "  FAIL  the profile prints the flag it enforced with"; cat "$BP"; exit 1; }
echo "  ok    every key names the podman flag behind it"
grep -q "audio=no gpu=yes cannot be enforced separately" "$BP" \
    || { echo "  FAIL  a key that cannot be enforced must say so"; cat "$BP"; exit 1; }
echo "  ok    and a key it CANNOT enforce says so rather than reporting success"
HOME="$BH" "$OUT/kdos-box" profile frozenbox > "$OUT/box2.txt" 2>&1 || true
grep -q "persistence = frozen      (writes discarded)" "$OUT/box2.txt" \
    || { echo "  FAIL  an app box is frozen"; cat "$OUT/box2.txt"; exit 1; }
echo "  ok    an app box and a dev box differ in the profile, not in kind"

# `export` names a secondary box's launcher so it cannot collide with the
# default box's, which keeps upstream's own desktop id.
HOME="$BH" "$OUT/kdos-box" export arch gimp > "$OUT/box3.txt" 2>&1 || true
test -f "$BH/.local/share/applications/gimp.arch.desktop" \
    || { echo "  FAIL  a secondary box's launcher is <id>.<box>.desktop"; exit 1; }
test -L "$BH/.local/bin/gimp@arch" \
    || { echo "  FAIL  and its shim is <app>@<box>"; exit 1; }
grep -q 'Exec=kdos-box run arch gimp %U' "$BH/.local/share/applications/gimp.arch.desktop" \
    || { echo "  FAIL  the Exec line"; cat "$BH/.local/share/applications/gimp.arch.desktop"; exit 1; }
echo "  ok    a second box's launcher and shim cannot collide with the default's"
HOME="$BH" "$OUT/kdos-box" unexport arch gimp > /dev/null 2>&1 || true
test ! -e "$BH/.local/share/applications/gimp.arch.desktop" \
    || { echo "  FAIL  unexport removes exactly what export added"; exit 1; }
echo "  ok    and unexport removes exactly what export added"

echo
echo "==> packs: the store, the solve, the graft and the two signature answers"
# The fixture's packs are ASSEMBLED here rather than committed: a .kpack is a
# binary nobody can read in a diff, and mkfs.erofs is not on a developer's
# host. `--fixture` mounts nothing, so a stub image is as good as a filesystem.
PKS="$OUT/packs"
mkdir -p "$PKS" "$OUT/keys" "$OUT/medium"
printf 'this stands in for an EROFS image; --fixture never mounts it' > "$OUT/stub.img"
for m in testing/fixtures/pack/meta/*.meta; do
    id=$(basename "$m" .meta)
    # app.bad goes on the MEDIUM, because that is the origin whose hash is
    # checked at mount time: a store pack was verified when root wrote it.
    case "$id" in app.bad) d="$OUT/medium" ;; *) d="$PKS" ;; esac
    "$OUT/kdos-pack" assemble "$OUT/stub.img" "$m" "$d/$id.kpack" >/dev/null
done
# A PACK'S IMAGE MUST NOT DEPEND ON ITS VERSION. The EROFS UUID is derived from
# the pack's id and lands in the superblock, so putting the version in it would
# make every rebuild a different image even when no file inside had moved —
# `imagehash` could never answer "unchanged" and a bake would rewrite the whole
# 7.2 GB set for nothing.
printf 'id = v.demo\nkind = app\nversion = 1.0\n' > "$OUT/v1.meta"
printf 'id = v.demo\nkind = app\nversion = 9.9\n' > "$OUT/v2.meta"
"$OUT/kdos-pack" assemble "$OUT/stub.img" "$OUT/v1.meta" "$OUT/v1.kpack" >/dev/null
"$OUT/kdos-pack" assemble "$OUT/stub.img" "$OUT/v2.meta" "$OUT/v2.kpack" >/dev/null
if [ "$("$OUT/kdos-pack" imagehash "$OUT/v1.kpack")" \
   = "$("$OUT/kdos-pack" imagehash "$OUT/v2.kpack")" ]; then
    echo "  ok    two versions of one image hash the same — a rebake can skip it"
else
    echo "  FAIL  the version leaks into the image; imagehash cannot see 'unchanged'"; exit 1
fi
if [ "$(sha256sum < "$OUT/v1.kpack")" = "$(sha256sum < "$OUT/v2.kpack")" ]; then
    echo "  FAIL  the two packs are byte-identical — the version is not recorded at all"; exit 1
fi
echo "  ok    while the packs themselves differ, because the version is in the metadata"

"$OUT/kdos-pack" keygen "$OUT/keys/builder" >/dev/null
"$OUT/kdos-pack" sign "$PKS/app.good.kpack" "$OUT/keys/builder.key" >/dev/null
# One payload byte, flipped after assembly. It must read as a bad HASH: the
# hash is checked before the signature, and a caller told "bad signature" here
# would go looking for a key problem that does not exist.
printf 'X' | dd of="$OUT/medium/app.bad.kpack" bs=1 seek=4 conv=notrunc status=none

fx() { KDOS_KEYS="$OUT/keys" "$OUT/kdos-packd" --fixture "$PKS" "$OUT/medium"; }

fx > "$OUT/fx.txt"
check() {
    if grep -qE "$1" "$OUT/fx.txt"; then
        echo "  ok    $2"
    else
        echo "  FAIL  $2"; sed 's/^/        /' "$OUT/fx.txt"; exit 1
    fi
}
# the solve: base is mounted first, then the runtime, then the app
check 'app\.good .*/base:?.*rt-gtk|app\.good .*rt-gtk.*base' \
      "app.good composes over its runtime and base"
grep -q 'app\.good' "$OUT/fx.txt" || { echo "  FAIL  app.good is listed"; exit 1; }
LOWER=$(grep '^  app\.good  *lowerdir' "$OUT/fx.txt" || grep '^  app\.good' "$OUT/fx.txt")
case "$LOWER" in
    *"mnt/app.good:"*"mnt/rt-gtk:"*"mnt/base"*)
        echo "  ok    the stack reads app, runtime, base — highest priority first" ;;
    *)  echo "  FAIL  lowerdir order"; echo "        $LOWER"; exit 1 ;;
esac
check 'app\.orphan .*REFUSED.*rt-qt' \
      "a requirement nothing provides is refused, and the message names it"
check 'app\.good .*signed' "a signed pack verifies against the ring"
check 'app\.orphan .*unsigned' "an unsigned pack is unsigned, not bad"
check 'app\.bad .*bad payload hash' \
      "one flipped byte is a bad hash, never a bad signature"
check 'app\.bad .*REFUSED.*bad payload hash' \
      "and a pack off the medium that fails it is never mounted"
check 'data\.tiles' "the data pack is in the store"
check 'graft    tiles -> /usr/share/kdos-tiles' \
      "a data pack grafts into /usr/share for host consumers"
check 'boxgraft tiles -> ~/.local/share/kdos/packs/tiles' \
      "and into \$HOME for a box, which shares nothing else"
grep -q 'data\.tiles.*REFUSED' "$OUT/fx.txt" && { echo "  FAIL  data pack graft"; exit 1; }
echo "  ok    a data pack is never composed into a box root"


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
        # The window list libkdisp exposes. libkwl includes this header
        # unconditionally — the manager is bound on the first call, not at
        # start-up, but the include is not conditional — so every consumer
        # generates it whether or not it asks for a window list, and so does
        # this harness. `preflight.sh` fails the build when a build.sh forgets;
        # nothing but a container run catches it forgotten HERE.
        tar xf "$LS" -C "$PROTO" --strip-components=2 \
            "$(tar tf "$LS" | grep 'protocol/wlr-foreign-toplevel-management-unstable-v1.xml$' | head -1)"
        "$SCANNER" client-header \
            "$PROTO/wlr-foreign-toplevel-management-unstable-v1.xml" \
            "$PROTO/wlr-foreign-toplevel-management-unstable-v1-client-protocol.h"
        "$SCANNER" client-header "$XDG" "$PROTO/xdg-shell-client-protocol.h"
        # The toplevel's frame. libkwl asks for a SERVER decoration, so this
        # is as mandatory as the lock role's protocol.
        "$SCANNER" client-header \
            "$(pkg-config --variable=pkgdatadir wayland-protocols)/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml" \
            "$PROTO/xdg-decoration-unstable-v1-client-protocol.h"
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
        "$SCANNER" private-code \
            "$PROTO/wlr-foreign-toplevel-management-unstable-v1.xml" \
            "$PROTO/wlr-foreign-toplevel-management-unstable-v1-protocol.c"
        "$SCANNER" private-code "$XDG" "$PROTO/xdg-shell-protocol.c"
        "$SCANNER" private-code \
            "$_wp/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml" \
            "$PROTO/xdg-decoration-unstable-v1-protocol.c"
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
-Isrc/libs/libkcell -Isrc/libs/libkwl -Isrc/libs/libkdisp -Isrc/libs/libkcon -Isrc/libs/libkwm"
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

        #
        # AND A VIEW THAT CAN RASTERISE, where libpng is here too. The view
        # built below with the con family links no fcft and no pixman on
        # purpose — that is what lets it build on a bare host — so `--shot`
        # cannot be exercised there. This second binary is the same source
        # with the rasteriser compiled in, and it is what the shot assertion
        # runs; the same shape the terminal keeps, where one build proves the
        # console half and another proves the Wayland half.
        #
        if pkg-config --exists libpng 2>/dev/null; then
            $CC $STD $SHWARN -DKDOS_VIEW_SHOT $INC \
                -Isrc/desktop/kdos-view -Isrc/libs/libkcell \
                -o "$OUT/kdos-view-shot" \
                src/desktop/kdos-view/*.c src/libs/libkcell/*.c \
                src/libs/libkbase/*.c src/libs/libkcolor/*.c \
                src/libs/libktui/*.c src/libs/libkdisp/*.c \
                src/libs/libkcon/*.c \
                $(pkg-config --cflags --libs fcft pixman-1 libpng)
            VIEWSHOT="$OUT/kdos-view-shot"
            echo "  kdos-view --shot (the same source, with the rasteriser)"
        fi

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
        $CC $STD $WARN -c -I"$PROTO" $KCINC \
            $(pkg-config --cflags fcft pixman-1 xkbcommon wayland-client) \
            -o "$OUT/kdisp.o" src/libs/libkdisp/kdisp.c
        for f in src/libs/libkwl/kwl_key.c; do
            $CC $STD $WARN -c -I"$PROTO" $KCINC \
                $(pkg-config --cflags fcft pixman-1 xkbcommon wayland-client) \
                -o "$OUT/$(basename "$f" .c).o" "$f"
        done
        echo "  libkwl"

        # kdos-lock's client half draws through exactly the headers just
        # generated, so it costs one more compile and is the only gate it has.
        $CC $STD $WARN -c -I"$PROTO" -Isrc/libs/libkbase -Isrc/libs/libktui \
            -Isrc/libs/libkcolor -Isrc/libs/libkcell -Isrc/libs/libkwl -Isrc/libs/libkdisp -Isrc/libs/libkcon -Isrc/libs/libkwm \
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
        # $TRAY_SDBUS, NOT `basu`: sd-bus ships as basu on this distro and as
        # libsystemd nearly everywhere else, and the API is the same one. The
        # hardcoded name meant this whole block — the kdos-shell compile and
        # every front-end golden behind it — was skipped on any ordinary
        # development host, which is why those goldens went unlooked-at.
        if [ -n "$TRAY_SDBUS" ] &&
           pkg-config --exists alsa libpipewire-0.3 2>/dev/null &&
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
                # kdos-peek is the one front end with a decoder and an archive
                # reader behind it. Absent either, it is skipped BY NAME rather
                # than the whole shell compile being gated on libraries the
                # other forty files do not need.
                _pk=""
                case "$f" in
                */peek.c)
                    if pkg-config --exists libarchive libpng libjpeg libwebp \
                            2>/dev/null; then
                        _pk="-Isrc/libs/libkimg -DKIMG_HAVE_PNG"
                        _pk="$_pk -DKIMG_HAVE_JPEG -DKIMG_HAVE_WEBP"
                        _pk="$_pk $(pkg-config --cflags libarchive libpng \
                                                libjpeg libwebp)"
                    else
                        echo "  peek.c (skipped — no libarchive)"
                        continue
                    fi
                    ;;
                esac
                $CC $STD $SHWARN -c -I"$PROTO" -Isrc/desktop/kdos-shell $_pk \
                    -Isrc/libs/libkbase -Isrc/libs/libktui -Isrc/libs/libkcolor \
                    -Isrc/libs/libkcell -Isrc/libs/libkwl -Isrc/libs/libkdisp -Isrc/libs/libkcon -Isrc/libs/libkwm -Isrc/libs/libkxdg \
                    -Isrc/libs/libkicon -Isrc/libs/libkchrome -Isrc/libs/libkproc \
                    $(pkg-config --cflags fcft pixman-1 xkbcommon wayland-client \
                                 "$TRAY_SDBUS" alsa libpipewire-0.3) \
                    -o "$OUT/shell-$(basename "$f" .c).o" "$f"
            done
            echo "  kdos-shell ($(ls src/desktop/kdos-shell/*.c | wc -l) files)"

            # kdos-res: the first xdg-toplevel client in this tree, and the
            # one program here that is a TTY program and a window from the
            # same source. Built whole rather than syntax-checked, because its
            # goldens are rendered by running it.
            $CC $STD $SHWARN -o "$OUT/kdos-res" -I"$PROTO" \
                -DKDOS_RES_VERSION='"'"'"selftest"'"'"' \
                -Isrc/desktop/kdos-res \
                -Isrc/libs/libkbase -Isrc/libs/libktui -Isrc/libs/libkcolor \
                -Isrc/libs/libkcell -Isrc/libs/libkwl -Isrc/libs/libkdisp -Isrc/libs/libkcon -Isrc/libs/libkwm -Isrc/libs/libkxdg \
                -Isrc/libs/libkicon -Isrc/libs/libkchrome -Isrc/libs/libkproc \
                $(ls src/desktop/kdos-res/*.c | grep -v resctl.c) \
                src/libs/libkwl/*.c src/libs/libkdisp/*.c src/libs/libkcon/*.c src/libs/libkwm/*.c src/libs/libkcell/*.c src/libs/libktui/*.c \
                src/libs/libkcolor/*.c src/libs/libkbase/*.c \
                src/libs/libkxdg/*.c src/libs/libkicon/*.c \
                src/libs/libkchrome/*.c src/libs/libkproc/*.c \
                "$PROTO"/*-protocol.c \
                $(pkg-config --cflags --libs fcft pixman-1 xkbcommon \
                             wayland-client libpng)
            RESBIN="$OUT/kdos-res"
            echo "  kdos-res"

            # kdos-term AS A WINDOW. The console-only build below is the one
            # the goldens run, because a `--dump` needs no display at all; this
            # one exists to prove the SAME source still links the Wayland half,
            # which is the half a bare host cannot check.
            $CC $STD $SHWARN -o "$OUT/kdos-term-wl" -I"$PROTO" $KIMG_FLAGS \
                -Isrc/desktop/kdos-term \
                -Isrc/libs/libkbase -Isrc/libs/libktui -Isrc/libs/libkcolor \
                -Isrc/libs/libkcell -Isrc/libs/libkwl -Isrc/libs/libkdisp \
                -Isrc/libs/libkcon -Isrc/libs/libkvt -Isrc/libs/libkxdg \
                src/desktop/kdos-term/*.c \
                src/libs/libkwl/*.c src/libs/libkdisp/*.c src/libs/libkcon/*.c \
                src/libs/libkcell/*.c src/libs/libktui/*.c src/libs/libkvt/*.c \
                src/libs/libkcolor/*.c src/libs/libkbase/*.c \
                src/libs/libkxdg/*.c $KIMG_SRC \
                "$PROTO"/*-protocol.c \
                $(pkg-config --cflags --libs fcft pixman-1 xkbcommon \
                             wayland-client) $KIMG_LIBS
            echo "  kdos-term (as a Wayland window)"

            # The setuid helper, built SEPARATELY and linking libkbase alone:
            # giving a setuid binary the Wayland stack would be handing root a
            # font parser. Compiled here so its three verbs are checked even
            # though nothing in this suite can exercise the bit itself.
            $CC $STD $WARN -o "$OUT/kdos-resctl" -Isrc/libs/libkbase \
                src/desktop/kdos-res/resctl.c src/libs/libkbase/*.c
            echo "  kdos-resctl"
        else
            echo "  kdos-shell (skipped — an sd-bus, alsa, libpipewire-0.3 or ext-workspace-v1.xml missing)"
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
# libxml2, cairo, pango and glib are in the list because labwc.h reaches
# rcxml.h and font.h, which include them. Without their include paths the graft
# files fail to COMPILE, and on a host that has wlroots that is a hard stop
# rather than a skip — the guard has to name every header the compile needs,
# not only the libraries the object would link.
if pkg-config --exists wlroots-0.20 glesv2 egl wayland-server pixman-1 \
        libdrm libpng libxml-2.0 cairo pango glib-2.0 2>/dev/null; then
    KC=src/desktop/kdos-comp
    #
    # wlr_layer_shell_v1.h includes a GENERATED protocol header, which a meson
    # build of wlroots produces and an installed wlroots does not ship. It is
    # generated here from the same XML the client side already uses, because
    # without it this block does not skip: it fails to COMPILE on every host
    # that has wlroots, which is the only host it was written to run on.
    #
    "$SCANNER" server-header "$PROTO/wlr-layer-shell-unstable-v1.xml" \
        "$PROTO/wlr-layer-shell-unstable-v1-protocol.h"
    # labwc.h includes the meson-generated config.h; the graft files read
    # none of its flags, so a stub with the defaults is enough here.
    mkdir -p "$OUT/compconf"
    printf '#pragma once\n#define HAVE_XWAYLAND 0\n#define HAVE_NLS 0\n#define HAVE_RSVG 0\n#define HAVE_LIBSFDO 0\n#define HAVE_LIBINPUT_CONFIG_3FG_DRAG_ENABLED_3FG 0\n#define HAVE_LIBINPUT_CONFIG_DRAG_LOCK_ENABLED_STICKY 0\n#define LABWC_VERSION "selftest"\n' \
        > "$OUT/compconf/config.h"
    for f in "$KC"/src/kdos-*.c; do
        $CC $STD -Wall -Wextra -Wno-unused-parameter -c -DWLR_USE_UNSTABLE \
            -I"$KC/include" -I"$OUT/compconf" -I"$PROTO" \
            -Isrc/libs/libkcolor -Isrc/libs/libkbase \
            $(pkg-config --cflags wlroots-0.20 glesv2 egl wayland-server \
                pixman-1 libdrm libpng libxml-2.0 cairo pango glib-2.0) \
            -o "$OUT/comp-$(basename "$f" .c).o" "$f"
    done
    echo "  kdos-comp grafts ($(ls "$KC"/src/kdos-*.c | wc -l) files)"

    #
    # kdos-cage, the kiosk fork, compiled whole. It is small enough to compile
    # rather than sample, and this is the only automated check it has: wlroots
    # breaks API every release and a fork that is not compiled is a fork that
    # discovers that during a four-hour build.
    #
    mkdir -p "$OUT/cageconf"
    printf '#pragma once\n#define CAGE_HAS_XWAYLAND 1\n#define CAGE_VERSION "selftest"\n#define CAGE_UPSTREAM "selftest"\n' \
        > "$OUT/cageconf/config.h"
    for f in src/desktop/kdos-cage/*.c; do
        $CC $STD -Wall -Wextra -Wno-unused-parameter -c -DWLR_USE_UNSTABLE \
            -I"$OUT/cageconf" -Isrc/desktop/kdos-cage \
            -Isrc/libs/libkcolor -Isrc/libs/libkbase \
            $(pkg-config --cflags wlroots-0.20 wayland-server xkbcommon) \
            -o "$OUT/cage-$(basename "$f" .c).o" "$f"
    done
    echo "  kdos-cage ($(ls src/desktop/kdos-cage/*.c | wc -l) files)"

    #
    # embedcheck is the PARENT half of `kdos-cage --embed`, and it is a second
    # process for the reason decocheck is: the mechanism is a headless wlroots
    # output, a software renderer, a memfd and SCM_RIGHTS, every part of which
    # is a real kernel and library behaviour a mock would only assert about
    # itself. Running it needs a linked kdos-cage and a guest to render;
    # compiling it here is what stops it rotting.
    #
    #   embedcheck --size 640x480 --out f.ppm -- kdos-term -e ...
    #   embedcheck --size 640x480 --key 28 -- kdos-term -e 'read x; ...'
    #
    $CC $STD $WARN -Isrc/desktop/kdos-cage -o "$OUT/embedcheck" \
        testing/fixtures/embed/embedcheck.c
    echo "  embedcheck"
else
    echo "  kdos-comp grafts (skipped — wlroots-0.20, glesv2, egl, libxml2, cairo or pango not on this host)"
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
#
# REAL PROGRAMS, RECORDED ONCE, REPLAYED FOR EVER.
#
# testing/fixtures/vt/*.esc are what vim, htop, mc, less and tmux actually
# wrote to a 80x24 pty, captured once and committed as bytes. They are NOT
# re-derived: a re-recording picks up a different program version, a different
# terminfo and a different hostname, so a fixture that regenerated would be a
# test that changed its own question. The malformed stream beside them is
# written by hand, because no program emits it.
#
# What this proves that the hand-written escapes in the libkvt block cannot:
# real programs use the alternate screen, scroll regions, charset switches,
# mouse modes and SGR in combinations nobody writes on purpose.
#
echo "==> libkvt replays what real programs wrote to a terminal"
$CC $STD $SHWARN $INC -o "$OUT/vtrender" testing/fixtures/vt/vtrender.c \
    src/libs/libkvt/*.c src/libs/libktui/*.c src/libs/libkcolor/*.c \
    src/libs/libkbase/*.c
vt_fail=0
vt_golden() {
    _v_name=$1
    _v_file="testing/goldens/vt-$_v_name.txt"
    "$OUT/vtrender" "testing/fixtures/vt/$_v_name.esc" > "$OUT/vt-$_v_name.txt"
    # A GRID WITH NOTHING ON IT IS A FAILURE, not a golden. A parser that gave
    # up on the first byte renders an empty screen, and so does a recording
    # that ended with the alternate screen being restored — which is why the
    # fixtures stop on a live frame.
    if ! grep -q '[^ ]' "$OUT/vt-$_v_name.txt"; then
        echo "  vt-$_v_name: the stream rendered an empty grid"
        vt_fail=1
        return 0
    fi
    if [ "${KDOS_GOLDEN_UPDATE:-0}" = 1 ]; then
        cp "$OUT/vt-$_v_name.txt" "$_v_file"
        echo "  wrote vt-$_v_name"
        return 0
    fi
    if [ ! -f "$_v_file" ]; then
        echo "  vt-$_v_name: no golden committed"; vt_fail=1; return 0
    fi
    if diff -u "$_v_file" "$OUT/vt-$_v_name.txt" > "$OUT/vt-$_v_name.diff"; then
        echo "  vt-$_v_name"
    else
        echo "  vt-$_v_name DRIFTED:"
        head -20 "$OUT/vt-$_v_name.diff" | sed 's/^/    /'
        vt_fail=1
    fi
}
for _v in vim htop mc less tmux malformed; do vt_golden "$_v"; done
if [ "$vt_fail" != 0 ]; then
    echo
    echo "  A recorded stream renders differently than it did. The fixture did"
    echo "  not change — it is bytes — so the state machine did."
    echo "      KDOS_GOLDEN_UPDATE=1 testing/selftest.sh"
    echo "  then read the diff in git before committing it."
    exit 1
fi

echo "==> kdos-con composites a desktop, and it is the committed one"
#
# The console session links no Wayland, no font renderer and no pixel library,
# so unlike every other desktop program it compiles ANYWHERE — which is why its
# goldens are checked here rather than behind a pkg-config guard.
#
# `--dump` settles every terminal before compositing: a frame taken while a
# program is still writing is a different frame every time it is taken.
#
# kembed.h comes from the kdos-cage tree: the private channel between the
# session and the compositor it forks for an embedded window. It is a header
# with no code, so this pulls in nothing Wayland — which is the property that
# lets the session still compile anywhere.
$CC $STD $SHWARN $INC -Isrc/desktop/kdos-con -Isrc/desktop/kdos-cage \
    -o "$OUT/kdos-con" \
    src/desktop/kdos-con/*.c \
    src/libs/libkbase/*.c src/libs/libkcolor/*.c src/libs/libktui/*.c \
    src/libs/libkdisp/*.c src/libs/libkcon/*.c src/libs/libkvt/*.c \
    src/libs/libkwm/*.c src/libs/libkxdg/*.c
echo "  kdos-con"

# Raised by any golden that drifted, anywhere in the suite, and read at the end.
# It is initialised HERE rather than beside the second family of goldens,
# because the first family runs above that point and a later `golden_fail=0`
# would clear what this one had already recorded.
golden_fail=0

# The same shape as golden_dump() above, and for the same two reasons: one flag
# updates every golden in this suite rather than some of them, and a difference
# RECORDS a failure instead of ending the run — an `exit 1` here took the whole
# suite with it, so a golden that drifted hid every check below it, including
# the one that says every chord is on the key card.
con_golden() {
    _name=$1; shift
    "$OUT/kdos-con" "$@" > "$OUT/$_name.txt"
    if [ "${KDOS_GOLDEN_UPDATE:-0}" = 1 ]; then
        cp "$OUT/$_name.txt" "testing/goldens/$_name.txt"
        echo "  wrote $_name"
        return 0
    fi
    if diff -u "testing/goldens/$_name.txt" "$OUT/$_name.txt" > "$OUT/$_name.diff"; then
        echo "  $_name"
    else
        echo "  $_name DIFFERS from its golden:"
        head -20 "$OUT/$_name.diff" | sed 's/^/    /'
        golden_fail=1
    fi
}
con_golden con-desktop-80x24 --dump 80x24 --term "/bin/echo hello"
con_golden con-two-132x43 --dump 132x43 --term "/bin/echo first" --term "/bin/echo second"

# THE FUNCTION-KEY ROW, which is a con.conf mode rather than a flag — so the
# golden is driven by pointing XDG_CONFIG_HOME at a config that asks for it.
# The row names ten chords and every one must be bound, or the bar teaches a
# key that does nothing; the golden is what notices when a chord is renamed.
mkdir -p "$OUT/fkeys-home/kdos-con"
printf 'taskbar = fkeys\n' > "$OUT/fkeys-home/kdos-con/con.conf"
XDG_CONFIG_HOME="$OUT/fkeys-home" \
    con_golden con-fkeys-80x24 --dump 80x24 --term "/bin/echo hello"

#
# ONE WRITER FOR THE ATTACH PAYLOAD.
#
# `libkcon`'s client sends KCON_OP_ATTACH twice: once on init and once as the
# resize, which IS a second attach. A field added to only one of them makes the
# server read past the end of the message, refuse the attach and drop the
# surface — and the symptom is a toast that vanishes the moment it has
# something to say, which looks like anything but a protocol error. That
# happened when the corner and its margins were added.
#
# The server-side test beside it cannot see this: it builds its own payload.
# What holds the property is that every send goes through one writer.
#
_att=$(grep -c 'KCON_OP_ATTACH' src/libs/libkcon/kcon_client.c)
_put=$(grep -c 'put_attach(&b' src/libs/libkcon/kcon_client.c)
if [ "$_att" = "$_put" ]; then
    echo "  every attach the client sends is written in one place ($_att)"
else
    echo "  THE ATTACH PAYLOAD HAS $_att SENDERS AND $_put WRITERS"
    echo "  a sender that builds its own is a field away from dropping surfaces"
    exit 1
fi

#
# A CHORD A REAL KEYPRESS CAN PRODUCE.
#
# `keys.conf` spells a chord with the plain letter — `Super+Shift+t` — and a
# backend delivers the character the LAYOUT produces, which with Shift held is
# `T`. An exact comparison against the table's `t` matched nothing, so five
# chords fell through to the focused window and typed a capital letter into it:
# quit, restore, the saver, tile and show-desktop. The checks above could not
# see it — the table was spelled right and every action had a card row — so the
# guard has to drive the matcher with the character a keyboard actually sends.
#
# keys.c links only libkbase, which is why this can be a driver rather than a
# whole session.
#
cat > "$OUT/chorddrv.c" <<'CHORDEOF'
#include <stdio.h>
#include "con.h"

int main(void)
{
	struct { int key, mods, want, arg; } c[] = {
		{ 'T', KT_MOD_SUPER | KT_MOD_SHIFT, CON_ACT_TILE, 0 },
		{ 't', KT_MOD_SUPER | KT_MOD_SHIFT, CON_ACT_TILE, 0 },
		{ 'D', KT_MOD_SUPER | KT_MOD_SHIFT, CON_ACT_SHOW_DESKTOP, 0 },
		{ 'Q', KT_MOD_SUPER | KT_MOD_SHIFT, CON_ACT_QUIT, 0 },
		{ 'N', KT_MOD_SUPER | KT_MOD_SHIFT, CON_ACT_RESTORE, 0 },
		{ 'L', KT_MOD_SUPER | KT_MOD_SHIFT, CON_ACT_EXEC, CON_CMD_SAVER },
		{ 'q', KT_MOD_SUPER, CON_ACT_CLOSE, 0 },
		/* Shift is still a MODIFIER: only the character is normalised,
		 * so these two remain different chords. */
		{ 'T', KT_MOD_SUPER, CON_ACT_NONE, 0 },
		{ 'R', KT_MOD_SUPER | KT_MOD_SHIFT, CON_ACT_NONE, 0 },
		/* A window by number, which rides the digit branch. */
		{ '3', KT_MOD_SUPER | KT_MOD_ALT, CON_ACT_WIN_N, 3 },
		{ '3', KT_MOD_SUPER, CON_ACT_WS, 2 },
	};
	int bad = 0;

	for (unsigned i = 0; i < sizeof(c) / sizeof(c[0]); i++) {
		int arg = 0;
		int act = keys_action(c[i].key, c[i].mods, &arg);

		if (act != c[i].want || (c[i].want != CON_ACT_NONE &&
					 arg != c[i].arg)) {
			printf("    '%c' mods %d -> action %d arg %d, "
			       "want action %d arg %d\n", c[i].key, c[i].mods,
			       act, arg, c[i].want, c[i].arg);
			bad = 1;
		}
	}

	/*
	 * EVERY CELL OF THE FUNCTION-KEY ROW NAMES A CHORD THAT EXISTS.
	 *
	 * The row is a pointer target that fires `Super+F<n>`, and a cell
	 * naming an unbound chord is a label a person clicks, learns, and then
	 * presses to no effect — which teaches them the desktop is broken. Ten
	 * cells, ten chords, checked here because the row is drawn from a table
	 * in panel.c and bound from a table in keys.c and nothing else makes
	 * the two agree.
	 */
	for (int n = 1; n <= 10; n++) {
		int arg = 0;
		int act = keys_action(KT_K_F1 + (n - 1), KT_MOD_SUPER, &arg);

		if (act == CON_ACT_NONE) {
			printf("    the function-key row names Super+F%d "
			       "and nothing is bound to it\n", n);
			bad = 1;
		}
	}
	return bad;
}
CHORDEOF
$CC $STD $SHWARN $INC -Isrc/desktop/kdos-con -o "$OUT/chorddrv" \
    "$OUT/chorddrv.c" src/desktop/kdos-con/keys.c src/libs/libkbase/*.c
if HOME=/nonexistent-kdos "$OUT/chorddrv"; then
    echo "  a shifted letter reaches the chord it is bound to"
    echo "  and every cell of the function-key row names a bound chord"
else
    echo "  A CHORD A KEYBOARD SENDS DOES NOT REACH ITS ACTION"
    exit 1
fi

#
# EVERY CHORD THE SHIPPED FILE NAMES IS AN ACTION THE SESSION HAS.
#
# keys.conf's overlay keeps the default for an action no line names, which is
# what lets a person rebind one key without restating the rest — and is also
# what makes a typo silent: `focus-rihgt = ...` rebinds nothing and reports
# nothing, and the chord goes on doing what it did before. `--keys` prints the
# table after the overlay, so the action names on the left are the whole set.
#
"$OUT/kdos-con" --keys | cut -f1 | sort -u > "$OUT/con-actions.txt"
sed -e 's/#.*//' -e 's/[[:space:]]*$//' \
    fs/etc/skel/.config/kdos-con/keys.conf \
    | grep '=' | sed 's/.*=[[:space:]]*//' | sort -u > "$OUT/con-shipped.txt"
if comm -13 "$OUT/con-actions.txt" "$OUT/con-shipped.txt" \
        > "$OUT/con-unknown.txt" && [ ! -s "$OUT/con-unknown.txt" ]; then
    echo "  keys.conf names only actions the session has"
else
    echo "  keys.conf names actions kdos-con does not have:"
    sed 's/^/    /' "$OUT/con-unknown.txt"
    exit 1
fi

#
# AND EVERY CHORD IS ON THE CARD.
#
# `kdos-keys` describes and groups what `kdos-con --keys` prints, and an action
# its table has no row for returns -1 and is DROPPED. A new chord then works
# and appears nowhere a person would look for it, which for a keyboard-first
# desktop is the same as not having it. The table is source rather than
# something this host can run — the card links Wayland — so the check is that
# each action name appears in it.
#
# The row SHAPE, not the name anywhere in the file: `net`, `power` and
# `settings` are ordinary words that appear in that source as other strings, so
# a bare name grep passed for eleven chords the card was in fact dropping.
_nocard=""
while read -r _act; do
    grep -q "{ \"$_act\"," src/desktop/kdos-shell/keys.c || _nocard="$_nocard $_act"
done < "$OUT/con-actions.txt"
if [ -z "$_nocard" ]; then
    echo "  every chord kdos-con binds has a row on the key card"
else
    echo "  CHORDS THE KEY CARD WOULD DROP:$_nocard"
    echo "  add a row to con_section() in src/desktop/kdos-shell/keys.c"
    exit 1
fi

#
# THE SAME DESKTOP, THROUGH A VIEW. Two processes and a real socket: the
# session composites and holds no display, kdos-view attaches and holds no
# window state, and what the view prints is what a person would see.
#
# It is a SEPARATE golden from the ones above on purpose. `--dump` renders
# offscreen, and the offscreen backend reports no UTF-8, so the glyph tier
# falls back to ASCII; a live view reports UTF-8 and gets the rich one. Two
# honest pictures of two different backends, and one golden could only ever
# describe one of them.
#
$CC $STD $SHWARN $INC -Isrc/desktop/kdos-view -o "$OUT/kdos-view" \
    src/desktop/kdos-view/*.c \
    src/libs/libkbase/*.c src/libs/libkcolor/*.c src/libs/libktui/*.c \
    src/libs/libkdisp/*.c src/libs/libkcon/*.c
echo "  kdos-view"

#
# kdos-term, CONSOLE ONLY — the same source with the Wayland half left out.
#
# It builds anywhere for the same reason kdos-con does, and that is the point:
# the state machine, the frame, the keys and the image path are what a `--dump`
# exercises, and none of them wants a display. The Wayland build above is the
# one that proves the other half still links.
#
# The picture path tiles through libkcell's one scaler, so it needs that
# archive's header — fcft, for the declaration alone; no fcft symbol is called
# from the tiler — and its one file. Without fcft the decoders are left out of
# THIS build rather than half-linked; libkimg's own blocks above still run.
TERM_KIMG_FLAGS="$KIMG_FLAGS"
TERM_KIMG_SRC="$KIMG_SRC"
TERM_KIMG_LIBS="$KIMG_LIBS"
if [ -n "$KIMG_SRC" ]; then
    if pkg-config --exists fcft 2>/dev/null; then
        TERM_KIMG_FLAGS="$KIMG_FLAGS -Isrc/libs/libkcell $(pkg-config --cflags fcft)"
        TERM_KIMG_SRC="$KIMG_SRC src/libs/libkcell/kcell_tile.c"
    else
        TERM_KIMG_FLAGS=""
        TERM_KIMG_SRC=""
        TERM_KIMG_LIBS=""
        echo "  kdos-term: pictures left out — no fcft for the tiler's header"
    fi
fi
$CC $STD $SHWARN $INC $TERM_KIMG_FLAGS -DKDOS_TERM_CONSOLE_ONLY \
    -Isrc/desktop/kdos-term -o "$OUT/kdos-term" \
    src/desktop/kdos-term/*.c \
    src/libs/libkbase/*.c src/libs/libkcolor/*.c src/libs/libktui/*.c \
    src/libs/libkdisp/*.c src/libs/libkcon/*.c src/libs/libkvt/*.c \
    src/libs/libkxdg/*.c $TERM_KIMG_SRC $TERM_KIMG_LIBS
echo "  kdos-term (console only)"

#
# AND RUN, because a `--dump` is the whole terminal short of a display: the
# state machine, the frame, the child on its pty and — where the decoders
# exist — a picture.
#
# THE PICTURE IS PART OF THE ASSERTION, in the only way a text golden can hold
# one. `--dump` has no pixels, so a sprite renders as its fallback in the
# picture's top-left cell and as blanks under the rest — which is exactly what
# a tty and a view with no pixel library show. What the golden holds is the
# SHAPE: how many rows the picture took and where the cursor was left.
#
term_golden() {
    _name=$1; _size=$2; shift 2
    XDG_CONFIG_HOME=/nonexistent-kdos-config \
    XDG_CACHE_HOME=/nonexistent-kdos-cache \
        "$OUT/kdos-term" --dump "$_size" "$@" > "$OUT/$_name.txt" 2>/dev/null
    if diff -u "testing/goldens/$_name.txt" "$OUT/$_name.txt" \
            > "$OUT/$_name.diff"; then
        echo "  $_name"
    else
        echo "  $_name DIFFERS from its golden:"
        head -20 "$OUT/$_name.diff" | sed 's/^/    /'
        exit 1
    fi
}
term_golden term-hello-44x8 44x8 -e /bin/echo hello
# Colour, cursor addressing and an attribute, which is the whole of what a
# curses program does to a screen — driven by printf so the golden needs no
# program installed.
term_golden term-ansi-44x10 44x10 -e /bin/sh -c \
    'printf "\033[31mred\033[0m \033[1mbold\033[0m\n\033[3;10Hmoved\n"'
# THE SIXEL DECODER, not "some decoder": pixman alone compiles libkimg with no
# sixel in it, and this golden holds a sixel picture. Guarding on the wrong one
# runs a picture test against a build that cannot decode the picture, and the
# diff blames the terminal.
case "$KIMG_FLAGS" in
*-DKIMG_HAVE_SIXEL*)
    term_golden term-sixel-44x10 44x10 -e /bin/sh -c \
        'printf "\033P"; cat testing/fixtures/img/valid.six; printf "\033\\\\after\n"'
    ;;
*)
    echo "  term-sixel-44x10 (skipped — no sixel decoder on this host)"
    ;;
esac

# A short path: sun_path is 108 bytes and $OUT can be longer than that.
# Six X's, not four: busybox mktemp takes only the six-character template, and
# a shorter one is "Invalid argument" rather than a shorter directory.
VSOCK=$(mktemp -d /tmp/kdos-st.XXXXXX)
# KDOS_CON_DUMP freezes the clock. A golden with a real time in it passes the
# minute it is taken and fails every minute after.
KDOS_CON_DUMP=1 "$OUT/kdos-con" --serve --socket "$VSOCK/s" \
    --term "/bin/echo hello" &
VPID=$!
for _ in $(seq 1 100); do [ -S "$VSOCK/s" ] && break; sleep 0.05; done
"$OUT/kdos-view" --dump 80x24 --socket "$VSOCK/s" > "$OUT/con-view-80x24.txt" 2>/dev/null
#
# AND THE SAME FRAME AS A PICTURE. `--shot` settles exactly as `--dump` does
# and takes the same one; what it adds is the rasteriser, so this asserts the
# artefact rather than the pixels: a real PNG signature, and an IHDR whose
# geometry is the grid times the cell — a shot that came out one cell wide
# would still be a valid PNG.
#
# NOT A GOLDEN. The rasterising depends on the font this host happens to have,
# and a byte comparison would be asserting fontconfig.
[ -n "${VIEWSHOT:-}" ] &&
    "$VIEWSHOT" --shot "$OUT/con-view.png" --socket "$VSOCK/s" 2>/dev/null || true
# `wait` reports the status of a process we KILLED, which is 143 — and under
# `set -e` that ends the suite with no message at all.
kill $VPID 2>/dev/null || true
wait $VPID 2>/dev/null || true
rm -rf "$VSOCK"
if [ "${KDOS_GOLDEN_UPDATE:-0}" = 1 ]; then
    cp "$OUT/con-view-80x24.txt" testing/goldens/con-view-80x24.txt
    echo "  wrote con-view-80x24"
elif diff -u testing/goldens/con-view-80x24.txt "$OUT/con-view-80x24.txt" \
        > "$OUT/con-view.diff"; then
    echo "  con-view-80x24 (a session and a view, two processes)"
else
    echo "  con-view-80x24 DIFFERS from its golden:"
    head -20 "$OUT/con-view.diff" | sed 's/^/    /'
    echo "      KDOS_GOLDEN_UPDATE=1 testing/selftest.sh"
    exit 1
fi

if [ -s "$OUT/con-view.png" ]; then
    # The eight-byte signature, then IHDR's width and height as big-endian
    # 32-bit words at offsets 16 and 20.
    # Byte at a time and assembled here: busybox od has no --endian, and a
    # host one reading a big-endian word natively would answer differently on
    # each. Four hex bytes is the same arithmetic everywhere.
    _be32() {
        set -- $(od -An -tx1 -j"$2" -N4 "$1" | tr -d '\n')
        printf '%d' "0x$1$2$3$4"
    }
    _pngmagic=$(dd if="$OUT/con-view.png" bs=1 skip=1 count=3 2>/dev/null)
    _pngw=$(_be32 "$OUT/con-view.png" 16)
    _pngh=$(_be32 "$OUT/con-view.png" 20)
    [ "$_pngmagic" = "PNG" ] || {
        echo "  kdos-view --shot did not write a PNG"; exit 1; }
    # 80 columns and 24 rows, times a cell that is at least 4x8 on any font
    # this could have loaded.
    [ "${_pngw:-0}" -ge 320 ] && [ "${_pngh:-0}" -ge 192 ] || {
        echo "  kdos-view --shot wrote ${_pngw}x${_pngh}, which is not the grid"
        exit 1; }
    echo "  con-view --shot: a ${_pngw}x${_pngh} picture of the same frame"
elif [ -n "${VIEWSHOT:-}" ]; then
    echo "  kdos-view --shot wrote nothing"
    exit 1
else
    echo "  con-view --shot (skipped — no libpng or fcft on this host)"
fi

#
# A VIEW THAT IMPOSES NO SIZE, which is what a screenshot and a screencast both
# are: taking a picture of the desktop must not resize the desktop. It attaches
# asking for nothing, is told the grid, and gets a frame — and for a long time
# it got none at all, because a size of zero was refused at the attach and the
# view was never counted as attached.
#
VSOCK=$(mktemp -d /tmp/kdos-cv.XXXXXX)
KDOS_CON_DUMP=1 "$OUT/kdos-con" --serve --socket "$VSOCK/s" \
    --term "/bin/echo hello" &
VPID=$!
for _ in $(seq 1 100); do [ -S "$VSOCK/s" ] && break; sleep 0.05; done
"$OUT/kdos-view" --dump --socket "$VSOCK/s" > "$OUT/con-view-auto.txt" 2>/dev/null
kill $VPID 2>/dev/null || true
wait $VPID 2>/dev/null || true
rm -rf "$VSOCK"
if diff -u testing/goldens/con-view-80x24.txt "$OUT/con-view-auto.txt" \
        > "$OUT/con-view-auto.diff"; then
    echo "  con-view, no size imposed (the session's own grid)"
else
    echo "  a view that imposed no size got a DIFFERENT frame:"
    head -20 "$OUT/con-view-auto.diff" | sed 's/^/    /'
    exit 1
fi

echo
echo "==> the console desktop opens no network socket, anywhere"
#
# A REMOTE DESKTOP HERE IS A FORWARDED UNIX SOCKET AND NOTHING ELSE. `kdos con
# forward` carries the view socket over ssh, so the remote case inherits ssh's
# authentication and needs none of its own — and that argument only holds while
# there is no other way in. A TCP listener appearing in any of these sources
# would silently turn "off by default" into "off unless somebody connects".
#
_net=$(grep -rn 'AF_INET\|SOCK_DGRAM\|getaddrinfo\|htons' \
    src/libs/libkcon src/desktop/kdos-con src/desktop/kdos-view 2>/dev/null || true)
if [ -z "$_net" ]; then
    echo "  no AF_INET, no getaddrinfo — a unix socket is the only door"
else
    echo "  A NETWORK SOCKET APPEARED in the console desktop:"
    echo "$_net" | sed 's/^/    /'
    exit 1
fi

#
# AND NEITHER PUBLISHED PROTOCOL CARRIES A FILE DESCRIPTOR.
#
# `kdos con forward` sends the view socket over ssh, and a descriptor passed on
# it would arrive as a number meaning something on the other machine — so the
# wire is cells, keys and strings, and nothing that is only valid in one
# process. The ONE socketpair that does pass descriptors is private and local:
# kdos-cage hands the session a compositor's, over a pair neither protocol
# reaches, so the two ends of that pair are the only SCM_RIGHTS allowed.
#
_fds=$(grep -rn 'SCM_RIGHTS' src/libs/libkcon src/desktop/kdos-con \
    src/desktop/kdos-view 2>/dev/null \
    | grep -v 'kdos-con/embed.c' || true)
if [ -z "$_fds" ]; then
    echo "  no SCM_RIGHTS outside the private embed pair"
else
    echo "  A DESCRIPTOR IS BEING PASSED on a forwardable socket:"
    echo "$_fds" | sed 's/^/    /'
    exit 1
fi

echo
echo "==> libkkms takes a screen, where there is one to take"
#
# The only thing on the console path that needs a GPU device, and the reason it
# is a separate archive: kdos-con links none of it. There is no display in a
# build container, so what is proved here is that it COMPILES and LINKS against
# the real drm, input, seat and xkb — not that a mode gets set.
#
if pkg-config --exists libdrm libinput libseat xkbcommon libudev fcft pixman-1 \
        2>/dev/null; then
    KKMS_PC="libdrm libinput libseat xkbcommon libudev fcft pixman-1"
    $CC $STD $WARN -c -Isrc/libs/libkbase -Isrc/libs/libkcolor \
        -Isrc/libs/libktui -Isrc/libs/libkcell -Isrc/libs/libkkms \
        $(pkg-config --cflags $KKMS_PC) \
        -o "$OUT/kkms.o" src/libs/libkkms/kkms.c
    $CC $STD $WARN -c -Isrc/libs/libkbase -Isrc/libs/libkcolor \
        -Isrc/libs/libktui -Isrc/libs/libkcell -Isrc/libs/libkkms \
        $(pkg-config --cflags $KKMS_PC) \
        -o "$OUT/kkms_input.o" src/libs/libkkms/kkms_input.c
    echo "  libkkms"

    # And the view that uses it, linked for real.
    #
    # WITH THE CAST MODE where PipeWire is here: a recording rasterises through
    # the same cell painter and writes into a stream instead of onto a screen,
    # so it is the same binary and the same code path short of the last copy.
    CAST_FLAGS=""
    CAST_PC=""
    if pkg-config --exists libpipewire-0.3 2>/dev/null; then
        CAST_FLAGS="-DKDOS_VIEW_CAST"
        CAST_PC="libpipewire-0.3"
    fi
    $CC $STD $WARN -DKDOS_VIEW_KMS $CAST_FLAGS -Isrc/desktop/kdos-view \
        -Isrc/libs/libkbase -Isrc/libs/libkcolor -Isrc/libs/libktui \
        -Isrc/libs/libkdisp -Isrc/libs/libkcon -Isrc/libs/libkcell \
        -Isrc/libs/libkkms $(pkg-config --cflags $KKMS_PC $CAST_PC) \
        -o "$OUT/kdos-view-kms" src/desktop/kdos-view/*.c \
        src/libs/libkbase/*.c src/libs/libkcolor/*.c src/libs/libktui/*.c \
        src/libs/libkdisp/*.c src/libs/libkcon/*.c src/libs/libkcell/*.c \
        src/libs/libkkms/*.c $(pkg-config --libs $KKMS_PC $CAST_PC)
    if [ -n "$CAST_FLAGS" ]; then
        echo "  kdos-view --kms --cast"
        #
        # castcheck is the CONSUMER half of --cast, and a second process for the
        # reason embedcheck is one: a PipeWire node, a format negotiation and a
        # shared buffer are real daemon behaviours. Running it needs a running
        # PipeWire; compiling it is what stops it rotting.
        #
        $CC $STD $WARN $(pkg-config --cflags libpipewire-0.3) \
            -o "$OUT/castcheck" testing/fixtures/cast/castcheck.c \
            $(pkg-config --libs libpipewire-0.3)
        echo "  castcheck"
    else
        echo "  kdos-view --kms (no PipeWire: cast mode not compiled)"
    fi
else
    echo "  libkkms (skipped — no drm, input, seat or xkb on this host)"
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

# A SOURCE-LESS PORT'S OWN FILES ARE ITS RECIPE. `demo` names no `source =`,
# which is what every port under src/ does: it builds out of $PORT_SRC, so
# nothing names those files and no `sha256 =` covers them. With only the four
# recipe files hashed, editing a .c changed nothing the build could see — the
# port read as installed and current and the tree kept the binary it had. That
# is not a build error, it is a shipped program behaving like an older one, and
# it is how an ISO came to carry a kdos-packd that looked for its keyring in
# the wrong directory.
#
# It runs HERE, while the sidecar is still valid: the corrupt-sidecar case
# below leaves the port reading as UNKNOWN, which correctly skips for ever.
H3=$(cat "$SIDE")
printf 'int demo_probe(void) { return 1; }\n' > "$SR/ports/demo/probe.c"
kstrict KPKG_STRICT_RECIPE=1 | grep -q "Building demo" \
    || { echo "  a source-less port did not rebuild after its own .c changed"
         exit 1; }
[ "$H3" != "$(cat "$SIDE")" ] \
    || { echo "  the sidecar did not move when a source file changed"; exit 1; }
# And it settles: a hash that kept moving would rebuild for ever.
kstrict KPKG_STRICT_RECIPE=1 | grep -q "Nothing to do" \
    || { echo "  the source-aware hash is not stable across runs"; exit 1; }
rm -f "$SR/ports/demo/probe.c"
kstrict KPKG_STRICT_RECIPE=1 | grep -q "Building demo" \
    || { echo "  REMOVING a source file did not rebuild it either"; exit 1; }
kstrict KPKG_STRICT_RECIPE=1 >/dev/null
echo "  a source-less port's own sources are part of its recipe"

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
#
# THE THUMBNAIL CACHE'S NAME, which is the whole interop claim: a file
# manager, an image viewer and this desktop all have to compute the SAME path
# or each writes a thumbnail the others cannot find. `--path` needs no image
# library, so it is checkable on any host — the decoding half is not.
#
echo "==> a thumbnail lands where every other program looks for it"
ln -sf kdos-tools "$OUT/kdos"
_thumbdir=$(mktemp -d)
: > "$_thumbdir/me.png"
_tp=$(HOME="$_thumbdir" XDG_CACHE_HOME="$_thumbdir/.cache" \
      "$OUT/kdos" thumb --path "$_thumbdir/me.png")
case "$_tp" in
*/thumbnails/normal/*.png) ;;
*) echo "  kdos thumb --path answered $_tp"; exit 1 ;;
esac
# The leaf is the md5 of the escaped URI: thirty-two lowercase hex digits and
# nothing else. A name of any other shape is one no other program computes.
_leaf=$(basename "$_tp" .png)
printf '%s' "$_leaf" | grep -Eq '^[0-9a-f]{32}$' \
    || { echo "  the cache name is not an md5: $_leaf"; exit 1; }
# And it is the md5 of THAT uri, not of the path — checked against the value
# `md5sum` gives for the same string, which is what interop means here.
_want=$(printf 'file://%s' "$_thumbdir/me.png" | md5sum | cut -d' ' -f1)
[ "$_leaf" = "$_want" ] \
    || { echo "  cache name $_leaf is not md5(file://$_thumbdir/me.png)"; exit 1; }
rm -rf "$_thumbdir"
echo "  the name is md5(file://<path>), which is what every reader computes"

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
# `cpio` is not on every host — Debian's slim images have none — and this is
# the only block that needs it. A missing tool is a SKIP WITH A NAME, the rule
# every other conditional block here keeps; without the guard the subshell
# exits 127 and takes the rest of the suite with it.
if ! command -v cpio >/dev/null 2>&1; then
    echo "  microcode (skipped — no cpio on this host)"
else
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
fi

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
# Matched by NAME rather than by step number: the number moves whenever a step
# is added, and a test that pinned it would fail for the one reason that is not
# a regression.
grep -qE "^ +[0-9]+ +Partition +skipped" "$OUT/plan.txt" \
    || { echo "  reuse still plans to partition"; cat "$OUT/plan.txt"; exit 1; }
grep -qE "^ +[0-9]+ +Theme +pending" "$OUT/plan.txt" \
    || { echo "  a non-default accent is not regenerated"; exit 1; }
# No medium, so there is nothing to carry and the step says so rather than
# running and copying nothing.
grep -qE "^ +[0-9]+ +Packs +skipped" "$OUT/plan.txt" \
    || { echo "  the packs step ran with no medium"; exit 1; }
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
echo "  no secret reaches a dump or the answer file it writes"

# ── the packs page reads the FLAT index ─────────────────────────────────────
# kinstall links libkbase, libktui and libkcolor and nothing else, which is what
# lets it live in phase 1 — so it reads `PACKAGES` itself and `R:`/`T:` are in
# that file for this reader. The three answers that matter: the recommended set
# is preselected, an answer file's choice wins, and an UNKNOWN id falls back to
# the recommended set rather than failing after the point of no return.
mkdir -p "$OUT/medium"
cat > "$OUT/medium/PACKAGES" <<'PKGS'
P:app.gimp
V:3.0.4-1
A:x86_64
K:app
S:96468992
C:1111111111111111111111111111111111111111111111111111111111111111
F:app.gimp.kpack
D:rt-gtk
R:yes
T:Create images and edit photographs

P:app.krita
V:5.2.6-1
A:x86_64
K:app
S:188743680
C:2222222222222222222222222222222222222222222222222222222222222222
F:app.krita.kpack
D:rt-kde
T:Digital painting

P:rt-gtk
V:1.0-1
A:x86_64
K:runtime
S:24000000
C:5555555555555555555555555555555555555555555555555555555555555555
F:rt-gtk.kpack
D:base

P:rt-qt
V:1.0-1
A:x86_64
K:runtime
S:83000000
C:6666666666666666666666666666666666666666666666666666666666666666
F:rt-qt.kpack
D:base

P:rt-kde
V:1.0-1
A:x86_64
K:runtime
S:277000000
C:7777777777777777777777777777777777777777777777777777777777777777
F:rt-kde.kpack
D:rt-qt

P:rt-wine
V:1.0-1
A:x86_64
K:runtime
S:713000000
C:8888888888888888888888888888888888888888888888888888888888888888
F:rt-wine.kpack
D:base

P:app.krita
V:5.2.5-1
A:x86_64
K:app
S:188000000
C:4444444444444444444444444444444444444444444444444444444444444444
F:app.krita-5.2.5.kpack
O:app.krita-5.2.4.kpack

P:base
V:1.0-1
A:x86_64
K:base
S:54525952
C:3333333333333333333333333333333333333333333333333333333333333333
F:base.kpack
T:Debian trixie, the whole filesystem
PKGS
kipack() { KDOS_PACK_MEDIUM="$OUT/medium" "$KI" --config "$1" --dump plan 2>&1; }
: > "$OUT/none.conf"
kipack "$OUT/none.conf" | grep -qE '^packs +app\.gimp$' \
    || { echo "  the recommended set was not preselected"; exit 1; }
# The base is carried whatever anybody ticks, and a DELTA stanza is not a pack:
# offering one would offer something the installer cannot apply.
KDOS_PACK_MEDIUM="$OUT/medium" "$KI" --config "$OUT/none.conf" --dump plan --json \
    2>&1 > "$OUT/packs.json"
grep -q '"id": "base"' "$OUT/packs.json" \
    || { echo "  the base pack is not carried"; exit 1; }
grep -q 'app.krita-5.2.5' "$OUT/packs.json" \
    && { echo "  a delta was offered as a pack"; exit 1; }
printf 'packs = app.krita\n' > "$OUT/p1.conf"
kipack "$OUT/p1.conf" | grep -qE '^packs +app\.krita$' \
    || { echo "  an answer file's pack choice was ignored"; exit 1; }
printf 'packs = app.nosuch\n' > "$OUT/p2.conf"
kipack "$OUT/p2.conf" | grep -qE '^packs +app\.gimp$' \
    || { echo "  an unknown pack id did not fall back to the recommended set"; exit 1; }
kipack "$OUT/none.conf" | grep -qE "^ +[0-9]+ +Packs +pending" \
    || { echo "  the packs step is skipped with a medium in the machine"; exit 1; }
# A RUNTIME IS CARRIED BECAUSE SOMETHING NEEDS IT. `D:` is in the index for
# this reader, which links no solver: the recommended app.gimp pulls rt-gtk and
# base, and rt-wine — 713 MB on a real bake — is left on the medium. Carrying
# every runtime because it exists was 1.7 GB where 313 MB does.
# `cfg.packs` is the ANSWER FILE's key and names applications only — a runtime
# is derived, never written there. What is CARRIED is the json dump's array.
kijson() { KDOS_PACK_MEDIUM="$OUT/medium" "$KI" --config "$1" --dump plan --json 2>/dev/null; }
kijson "$OUT/none.conf" | grep -q '"id": "rt-gtk"' \
    || { echo "  a needed runtime was not pulled in by its app"; exit 1; }
kijson "$OUT/none.conf" | grep -q '"id": "rt-wine"' \
    && { echo "  an unneeded runtime was carried anyway"; exit 1; }
# app.krita needs rt-kde, which needs rt-qt, which needs base: the closure is
# transitive or a two-deep chain installs something that cannot start.
kijson "$OUT/p1.conf" | grep -q '"id": "rt-qt"' \
    || { echo "  the requires closure is not transitive"; exit 1; }
# and it is IDEMPOTENT — an answer file that does not name gimp must not leave
# gimp's runtime ticked from the preselect that ran before it.
kijson "$OUT/p1.conf" | grep -q '"id": "rt-gtk"' \
    && { echo "  a runtime survived the app that needed it being unticked"; exit 1; }
echo "  a runtime is carried because something needs it, transitively"
echo "  the medium's packs: recommended preselected, unknown falls back"

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
#
# Only as an unprivileged user: run as root the daemon is SUPPOSED to bind
# /run/kdos-powerd.sock and stay in the foreground, so asserting it here would
# not fail, it would hang for ever — which is what it did in a root container.
if [ "$(id -u)" -ne 0 ]; then
    "$OUT/kdos-powerd" >/dev/null 2>&1 \
        && { echo "  served /run as non-root"; exit 1; }
else
    echo "  the non-root refusal is skipped (running as root)"
fi
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
#
# AND IT IS SKIPPED WHERE THE PREMISE IS FALSE, which is a host whose RAPL
# counter this user CAN read — some machines grant it to the console user, and
# there the daemon is right to start and the refusal cannot be provoked. Asking
# for it anyway made a correct daemon fail the suite: it got past the counter
# and stopped at `bind /run/kdos-energyd.sock: Permission denied`, which names
# neither powercap nor anything the assertion was about.
en_readable=no
for d in /sys/class/powercap/*/energy_uj; do
    [ -r "$d" ] && head -c1 "$d" >/dev/null 2>&1 && en_readable=yes
done
if [ "$en_readable" = yes ]; then
    echo "  the refusal is skipped — this host's RAPL counter is readable, so"
    echo "    the daemon is correct to start and the path cannot be exercised"
else
    "$OUT/kdos-energyd" 2>&1 | grep -q "powercap" \
        || { echo "  an unreadable counter did not stop the daemon"; exit 1; }
fi
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

# …and a box that broke its OWN declared budget goes first. This is not a
# refinement, it is what makes `memory =` mean anything: rootless podman with no
# systemd frequently has no cgroup delegation, so --memory is accepted and
# enforces nothing, and a profile key KDOS cannot enforce is one it does not
# offer. The fixture's host process is LARGER than anything in either box, so
# only the budget can produce the right answer.
KDOS_BOX_PROFILES=testing/fixtures/oomd/overbudget/profiles \
    "$OUT/kdos-oomd" --fixture testing/fixtures/oomd/overbudget > "$OUT/oomd3.txt" \
    || { echo "  the over-budget fixture found no candidate"; exit 1; }
grep -q "would kill rustc (appbox arch)" "$OUT/oomd3.txt" \
    || { echo "  the box over its declared budget was not preferred"
         cat "$OUT/oomd3.txt"; exit 1; }
"$OUT/kdos-oomd" --fixture testing/fixtures/oomd/overbudget > "$OUT/oomd4.txt"
grep -q "would kill kdosbuild" "$OUT/oomd4.txt" \
    || { echo "  without the profiles the answer should be the larger host process"
         cat "$OUT/oomd4.txt"; exit 1; }
echo "  a box over its own declared budget is preferred, and the check is load-bearing"

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
# Five: the stick, the encrypted stick, the live medium's two partitions and
# the data disc. The count is asserted so a rule that started offering the
# INTERNAL disk shows up as an extra row rather than as nothing.
grep -q "^5 eligible" "$MO" \
    || { echo "  the eligible set is not the five the fixture carries"
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

# ── THE PRIVILEGED VERBS, AND THE REFUSALS THAT MATTER MORE ───────────────
#
# This daemon spawns children now — eject, cryptsetup, mkfs — which it never
# did before, and one of its verbs writes a filesystem. Every assertion below
# is a REFUSAL except three, because the failure that costs a person their data
# is a verb that runs when it should not have.
#
# THE LIVE-MEDIUM CASE IS THE ONE TO KEEP. `sdd` is a boot medium: an iso9660
# partition AND a vfat ESP beside it. Every per-partition rule offers the ESP —
# it is removable, it probes as vfat, it is unmounted and no fstab claims it —
# so a format there destroys the running session, and the typed-name
# confirmation does not help because the person genuinely typed the name of the
# row they meant. The refusal has to be by the DISK.
#
# `--fixture-serve` runs the real dispatch over a real socket with the
# fixture's roots, and km_exec PRINTS the argv instead of running it. Nothing
# it decides to do is done.
KMSOCK="$OUT/km.sock"
cat > "$OUT/kmask.py" <<'KMASKEOF'
import socket, sys
s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.connect(sys.argv[1])
s.sendall(sys.argv[2].encode())
s.shutdown(socket.SHUT_WR)
b = b''
while True:
    d = s.recv(4096)
    if not d:
        break
    b += d
sys.stdout.write(b.decode(errors='replace'))
KMASKEOF
printf 'format = yes\n' > "$OUT/mountd.conf"
rm -f "$KMSOCK"
KDOS_MOUNTD_SOCKET="$KMSOCK" KDOS_MOUNTD_MOUNTS="$MF/mounts-live" \
KDOS_MOUNTD_CONF="$OUT/mountd.conf" \
    "$OUT/kdos-mountd" --fixture-serve "$MF/sys" "$MF/dev" > "$OUT/km.exec" 2>&1 &
KMPID=$!
for _i in $(seq 1 50); do [ -S "$KMSOCK" ] && break; sleep 0.1; done
kmask() { python3 "$OUT/kmask.py" "$KMSOCK" "$1"; }
kmwant() {  # <request> <expected substring> <what it proves>
    _got=$(kmask "$1")
    case "$_got" in
    *"$2"*) echo "  ok    $3" ;;
    *) echo "  FAIL  $3"; echo "        sent: $(printf '%s' "$1" | tr '\n' '|')"
       echo "        got:  $_got"; echo "        want: $2"; mountd_fail=1 ;;
    esac
}
mountd_fail=0

kmwant 'list
' 'crypto_LUKS' "a LUKS container is probed as one, not as what it replaced"
kmwant 'list
' 'SAFEBOX' "and its LUKS2 label is read"

# The live medium. sdd2 is the ESP on the disk this session booted from.
kmwant 'eject 2
' 'booted from' "eject refuses a partition of the boot DISK, not just the iso"
kmwant 'format 2 vfat 4
sdd2' 'booted from' "and so does format, even with the right name typed"

# The typed confirmation is the row's own kernel name.
kmwant 'format 0 vfat 4
sdc1' 'type sdb1 to confirm' "a format confirmed with another row's name is refused"
kmwant 'format 0 ext4 4
sdb1' 'ok sdb1' "and the row's own name confirms it"
kmwant 'format 0 reiserfs 4
sdb1' 'unknown filesystem' "a filesystem outside the allowlist is refused"

# The allowlist. The dispatch this replaced read an index with atoi() and threw
# the rest of the line away, so `mount 0 rm -rf /` was a well-formed mount.
kmwant 'mount 0 rm -rf /
' 'unknown command' "a verb with a token nobody named is not a verb"
kmwant 'mount 0zzz
' 'no such device' "an index that is not a number is not index zero"
kmwant 'mount 99
' 'no such device' "and an index past the list is refused"

# The passphrase is a FRAME, not a token: a tokeniser splits on spaces and a
# passphrase may contain them.
kmwant 'unlock 1 21
correct horse battery' 'ok kdos-sdc1' "a passphrase with spaces in it survives the wire"
kmwant 'unlock 0 4
abcd' 'not an encrypted volume' "unlock refuses a plain filesystem"
kmwant 'unlock 1 99
tooshort' 'short frame' "a frame shorter than it declared is refused"

# THE PASSPHRASE NEVER REACHES ARGV. /proc/<pid>/cmdline is world-readable for
# the life of the process, so a secret passed as an argument is one every user
# on the machine can read. Read while the daemon is still up — km_exec flushes
# each line as it prints it, so there is nothing to wait for.
if grep -q -e '--key-file=-' "$OUT/km.exec" 2>/dev/null; then
    echo "  ok    cryptsetup is fed the passphrase on stdin"
else
    echo "  FAIL  cryptsetup was not given --key-file=-"; mountd_fail=1
fi
if grep -q 'correct horse' "$OUT/km.exec" 2>/dev/null; then
    echo "  FAIL  the passphrase appeared in an argument vector"; mountd_fail=1
else
    echo "  ok    and it appears in no argument vector"
fi
kill $KMPID 2>/dev/null || true
wait $KMPID 2>/dev/null || true
[ "$mountd_fail" = 0 ] || exit 1

unset KDOS_MOUNTD_MOUNTS

echo
echo "==> kdos clone takes an image's length from the image, not from the device"
# The copy itself needs root and a real block device, so the WRITE cannot be
# summoned here. The decision in front of it can, and it is the whole design: a
# 3 GB image written to a 64 GB stick leaves the device reporting 64 GB, and
# copying that copies 61 GB of whatever was there before.
#
# TWO RECORDS DESCRIBE THE IMAGE AND THE OBVIOUS ONE IS SHORT. ISO9660's
# Primary Volume Descriptor is exact on an optical-only image — measured
# against the shipped ISO, 4970509 blocks x 2048 is its byte-for-byte file
# size. On a hybrid image the EFI System Partition is APPENDED after the
# ISO9660 volume and the PVD does not count it, so the PVD alone truncates away
# the partition that makes the copy boot. The GPT's backup header is the record
# that spans it. Both are read and the larger wins.
#
# The fixtures are hand-built headers, the kdos-mountd shape: `hybrid.img`
# carries a PVD claiming 40960 bytes and a GPT claiming 51200, so a reader that
# preferred either one alone gives a different answer and the test says which.
ln -sf kdos-tools "$OUT/kdos"
CF=testing/fixtures/clone
[ "$("$OUT/kdos" clone --source "$CF/iso-only.img" --extent)" = "40960" ] \
    || { echo "  an optical-only image's PVD is not being read"; exit 1; }
[ "$("$OUT/kdos" clone --source "$CF/hybrid.img" --extent)" = "51200" ] \
    || { echo "  the appended partition was truncated away — the PVD won"; exit 1; }
# Neither record is not "copy it anyway": a device is about to be destroyed.
"$OUT/kdos" clone --source "$CF/not-a-medium.img" --extent >/dev/null 2>&1 \
    && { echo "  a file that is not a medium was accepted"; exit 1; }
echo "  the PVD alone, the GPT over a short PVD, and neither are three answers"
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
# The shim is named after the PROGRAM the entry runs — `wesnoth-1.18`, read
# out of the `sh -c` string — not after the entry's file id.
grep -q '^wesnoth-1.18	sh -c "wesnoth-1.18 >/dev/null 2>&1"$' \
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
if [ -n "$TRAY_SDBUS" ] && command -v dbus-daemon >/dev/null 2>&1; then
    $CC $STD $WARN -o "$OUT/traycheck" \
        -Isrc/desktop/kdos-shell -Isrc/libs/libkdisp -Isrc/libs/libkcon -Isrc/libs/libkwl \
        -Isrc/libs/libktui -Isrc/libs/libkcolor \
        -Isrc/libs/libkxdg -Isrc/libs/libkbase -Isrc/libs/libkchrome \
        -Isrc/libs/libkproc -Isrc/libs/libkicon -Isrc/libs/libkcell \
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
# libkcon is linked REAL rather than stubbed. It brings no dependency — it links
# libktui and nothing else — and the shell calls into it on the launch path,
# where the console desktop's answer differs from the graphical one. A stub
# there would be a second implementation of that decision.
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
    # privacy.c is NOT here and must not be: dumpmain.c stubs the whole
    # sh_priv_* API, so compiling the real one in is a multiple definition.
    # A privacy symbol panel.c calls belongs in that stub set.
    DFRONTS="src/desktop/kdos-shell/cal.c src/desktop/kdos-shell/menu.c
             src/desktop/kdos-shell/launcher.c src/desktop/kdos-shell/pick.c
             src/desktop/kdos-shell/shell.c src/desktop/kdos-shell/apps.c
             src/desktop/kdos-shell/fav.c src/desktop/kdos-shell/cells.c
             src/desktop/kdos-shell/logo.c
             src/libs/libkchrome/kch_chrome.c
             src/libs/libkchrome/kch_tone.c"
    # A new surface may want alsa or an sd-bus; offer them when the host has
    # them rather than making the whole harness conditional on either.
    DEXTRA_PC=""
    pkg-config --exists alsa 2>/dev/null && DEXTRA_PC="alsa"
    [ -n "$TRAY_SDBUS" ] && DEXTRA_PC="$DEXTRA_PC $TRAY_SDBUS"
    # libpipewire is audio.c's, which the candidate loop was reporting as
    # "does not compile" when what it lacked was a header nobody had offered
    # it. privacy.c itself is stubbed by dumpmain.c and is not compiled here.
    pkg-config --exists libpipewire-0.3 2>/dev/null && \
        DEXTRA_PC="$DEXTRA_PC libpipewire-0.3"

    # kdos-peek decodes with libkimg and asks libarchive whether a file is an
    # archive, and it tiles the result through libkcell's one scaler. All four
    # decoders or none: kimg.c is compiled with the KIMG_HAVE_* its build
    # declares, and a half-configured decoder set is a link error rather than a
    # smaller feature. Absent any of them the surface is not offered here and
    # says so, like every other candidate.
    DPEEK_PC=""
    DPEEK_SRC=""
    DPEEK_CF=""
    if pkg-config --exists libarchive libpng libjpeg libwebp 2>/dev/null; then
        DPEEK_PC="libarchive libpng libjpeg libwebp"
        DPEEK_SRC="src/libs/libkimg/kimg.c src/libs/libkcell/kcell_tile.c"
        DPEEK_CF="-Isrc/libs/libkimg -DKIMG_HAVE_PNG -DKIMG_HAVE_JPEG -DKIMG_HAVE_WEBP"
        DEXTRA_PC="$DEXTRA_PC $DPEEK_PC"
    fi

    # Each candidate is admitted on its OWN compile, not the batch's: one file
    # that does not build must cost its own golden and nobody else's.
    DNEW=""
    DBAD=""
    for s in keys teams saver slit doc settings openwith audio \
             start net bt devices notify status tip panel trash peek; do
        [ -f "src/desktop/kdos-shell/$s.c" ] || continue
        [ "$s" = peek ] && [ -z "$DPEEK_PC" ] && {
            DBAD="$DBAD peek(no libarchive)"
            continue
        }
        if $CC $STD $SHWARN -fsyntax-only -I"$DPROTO" $DPEEK_CF \
                -Isrc/desktop/kdos-shell -Isrc/libs/libkwl -Isrc/libs/libkdisp -Isrc/libs/libkcon -Isrc/libs/libkwm -Isrc/libs/libktui \
                -Isrc/libs/libkcolor -Isrc/libs/libkxdg -Isrc/libs/libkbase \
                -Isrc/libs/libkicon -Isrc/libs/libkchrome -Isrc/libs/libkproc \
                -Isrc/libs/libkcell \
                $(pkg-config --cflags wayland-client pixman-1 fcft \
                             $DEXTRA_PC) \
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
        $CC $STD $SHWARN -o "$OUT/dumpcheck" -I"$DPROTO" $DPEEK_CF \
            -DKXDG_MIME_GLOBS="\"$PWD/testing/fixtures/openwith/data/mime/globs\"" \
            -Isrc/desktop/kdos-shell -Isrc/libs/libkwl -Isrc/libs/libkdisp -Isrc/libs/libkcon -Isrc/libs/libkwm -Isrc/libs/libktui \
            -Isrc/libs/libkcolor -Isrc/libs/libkxdg -Isrc/libs/libkbase \
            -Isrc/libs/libkicon -Isrc/libs/libkchrome -Isrc/libs/libkproc \
            -Isrc/libs/libkcell \
            $(pkg-config --cflags pixman-1 fcft 2>/dev/null) \
            -Wl,--wrap=ktui_offscreen_init \
            testing/fixtures/shell/dumpmain.c $DFRONTS "$@" \
            "$DPROTO"/*-protocol.c \
            src/libs/libktui/*.c src/libs/libkcolor/*.c src/libs/libkxdg/*.c \
            src/libs/libkbase/*.c src/libs/libkproc/*.c \
            src/libs/libkcon/*.c \
            $(pkg-config --cflags --libs wayland-client pixman-1 $DEXTRA_PC)
    }
    # The libraries kdos-peek pulls in are added only when it was admitted:
    # a harness that linked four decoders for a surface it does not carry
    # would fail on a host that has them and nothing that needs them.
    case " $DNEW " in
    *peek.c*) DNEW="$DNEW $DPEEK_SRC" ;;
    esac
    if [ -n "$DNEW" ] && dumpbuild $DNEW 2>"$OUT/dumpnew.err"; then
        DUMPCK="$OUT/dumpcheck"
        echo "  harness: cal menu launcher pick $(echo $DNEW | \
            sed 's,src/libs/[^ ]*,,g; s,src/desktop/kdos-shell/,,g; s,\.c,,g')"
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
grep -q "Esc Close.*\[ Cancel \]" "$OUT/dump-pick.txt" || {
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
# pick, `config/` for the surfaces that parse one (a frozen rc.xml for the
# keybind card, a comp.conf for settings), and `panelroot/` for the taskbar,
# whose whole right wing is the host's own load, battery and clock. So a golden
# cannot move because somebody edited skel, ran a build, or looked at it after
# midnight.
#
# THE TASKBAR IS GOLDENED WITH NO COMPOSITOR AND NO WINDOWS. `sh_connect`
# failing is an empty window list rather than a refusal — which is also the
# honest picture of a fresh login — so what this asserts is the chrome: the
# Start button, the separators, the right wing's walk and the meters strip's
# degradation. The plates, the hover fills and the icons are PIXELS and are
# absent here by design; a layout that only lines up once they arrive is a
# layout that is broken.
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
          KDOS_PANEL_ROOT="$PWD/panelroot" KDOS_PANEL_NOW=1735689600 ${KDOS_PANEL_DEBUG:+KDOS_PANEL_DEBUG=$KDOS_PANEL_DEBUG} \
          ${KDOS_GOLDEN_CON:+KDOS_CON=$KDOS_GOLDEN_CON} \
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
    res_golden() {          # <name> <WxH> <argv…>
        _r_file="$GOLD/res-$1-$2.txt"
        _r_got="$OUT/golden-res-$1-$2.txt"
        _r_n=$1; _r_s=$2; shift 2
        env LC_ALL=C TZ=UTC XDG_CACHE_HOME=/nonexistent-kdos-cache \
            XDG_CONFIG_HOME=/nonexistent-kdos-config \
            "$RESBIN" --fixture testing/fixtures/res --dump \
            --dump-size "$_r_s" "$@" > "$_r_got"
        set -- "$_r_n" "$_r_s"
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
    #
    # THREE WIDTHS, AND 56 IS THE ONE THAT MATTERS. 80 and 132 are the two
    # terminals anybody actually has; 56 is the narrow band where the sidebar
    # collapses and where every layout defect this toolkit has shipped first
    # showed itself. The height is held at 24 across 56 and 80 so that a diff
    # between them is a response to WIDTH and nothing else.
    for _p in applications processes cpu memory gpu drives network \
              batteries energy boxes; do
        [ -f "$GOLD/res-$_p-80x24.txt" ] || continue
        res_golden "$_p" 56x24  --page "$_p"
        res_golden "$_p" 80x24  --page "$_p"
        res_golden "$_p" 132x43 --page "$_p"
    done
    #
    # The detail page is the one surface --page cannot reach: it is opened
    # with Enter on a row, so a golden of it needs a way in. pid 950 is the
    # fixture's boxed firefox-esr, which is the interesting subject — a
    # process with a box, a cmdline and real io counters.
    res_golden detail 56x24  --page processes --detail 950
    res_golden detail 80x24  --page processes --detail 950
    res_golden detail 132x43 --page processes --detail 950
else
    echo "  kdos-res goldens (skipped — the binary was not built on this host)"
fi


#
# THE START MENU AS THE CONSOLE DESKTOP SEES IT. $KDOS_CON is what a program
# started inside a console session inherits, and two rows differ because of it:
# Terminal becomes kdos-term, which is a cell surface and can be a window here,
# and a Desktop row appears — the graphical session, on a terminal of its own.
#
# The socket path is a name and nothing connects to it: a dump draws a menu, it
# does not launch out of one.
#
KDOS_GOLDEN_CON=/nonexistent-kdos-con \
    golden start-console 80x24 start --dump

golden menu-system 80x24  menu system --dump
golden menu-system 132x43 menu system --dump
golden pick        80x24  pick --dir tree --dump
golden pick        132x43 pick --dir tree --dump
# THE CONTROL CENTRE'S FRONT DOOR. Two settings goldens were committed and
# driven by nothing, so a category added to the grid left them describing a
# surface that no longer existed. A golden nothing runs is a file that agrees
# with the tree only by accident.
if "$DUMPCK" --have settings; then
    golden settings 80x24  settings --dump
    golden settings 132x43 settings --dump
fi
# The Phase B surfaces, each only if it linked in. `--have` is dumpmain.c
# answering for its own weak symbols, so a surface that has not landed is a
# skip with a name on it rather than a silent gap.
for _s in keys teams doc settings start notify trash; do
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
# kdos-peek takes a FILE, so it cannot ride the loop above. The fixture is a
# committed tar built with a fixed mtime and uid: the listing draws names and
# sizes only, so the frame is the same on every machine.
if "$DUMPCK" --have peek; then
    golden peek-archive 80x24  peek peek.tar --dump
    golden peek-archive 132x43 peek peek.tar --dump
    # Text is the OTHER half of the contract: a dump must not fork the pager,
    # so it draws what it would have done instead.
    golden peek-text 80x24 peek panelroot/0/proc/meminfo --dump
elif [ -f "$GOLD/peek-archive-80x24.txt" ]; then
    echo "  peek: a golden is committed but the surface no longer links"
    golden_fail=1
else
    echo "  peek (skipped — not linked into the harness)"
fi

# THE THREE SURFACES THAT WERE DUMPABLE AND UNGOLDENED. Each has had `--dump`
# since it landed and nothing has ever looked at one: the launcher is the
# full-screen search `W-d` opens, the chooser is what every "Open with" goes
# through, and the tooltip is the only thing on this desktop that explains an
# icon with no label. Their inputs are fixed here for the same reason the rest
# are — the launcher's app index comes from the XDG variables above, which
# point at nothing, so it renders its empty state; the chooser is given the
# openwith fixture's own tar.gz, which is the longest-suffix case it already
# asserts on; and the tip is given the two strings a panel passes it.
for _s in launcher tip; do
    if "$DUMPCK" --have "$_s"; then
        :
    elif [ -f "$GOLD/$_s-80x24.txt" ]; then
        echo "  $_s: a golden is committed but the surface no longer links"
        golden_fail=1
    else
        echo "  $_s (skipped — not linked into the harness)"
    fi
done
if "$DUMPCK" --have launcher; then
    golden launcher 80x24  launcher --dump
    golden launcher 132x43 launcher --dump
fi
if "$DUMPCK" --have tip; then
    golden tip 80x24  tip --dump "Firefox" "left-click opens   middle-click a new window"
    golden tip 132x43 tip --dump "Firefox" "left-click opens   middle-click a new window"
fi
# The chooser is rendered with the SHELL fixture's XDG variables, which point
# at nothing — so what is goldened is its EMPTY state, the branch that says
# nothing on this machine claims this type. That is the reading worth holding:
# the resolution itself is asserted a few lines down against the openwith
# fixture, and a layout is only ever wrong at the size where the content runs
# out.
if "$DUMPCK" --have openwith; then
    golden openwith 80x24  openwith --dump "$PWD/testing/fixtures/openwith/files/roll.tar.gz"
    golden openwith 132x43 openwith --dump "$PWD/testing/fixtures/openwith/files/roll.tar.gz"
fi

# THE TASKBAR AT ITS OWN HEIGHT, not the card sizes above. Every other surface
# here is a window and 24 or 43 rows is a plausible one; the bar is two rows by
# definition and forcing it to 24 would golden a layout that cannot occur. The
# two WIDTHS are the point: 80 is the shipped 1280x800 bar at the 20-pixel cell
# and is where the degradation ladder bites, 132 is the width at which
# everything fits at once.
if "$DUMPCK" --have shell; then
    golden shell 80x2  shell --dump
    golden shell 132x2 shell --dump
elif [ -f "$GOLD/shell-80x2.txt" ]; then
    echo "  shell: a golden is committed but the surface no longer links"
    golden_fail=1
else
    echo "  shell (skipped — not linked into the harness)"
fi

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

# --dump-cells is the OTHER HALF of the golden-frame contract: one line per
# non-blank cell, `row col U+XXXX fg bg attr`, which is what makes a COLOUR
# regression visible as well as a geometry one. A text dump is byte-identical
# across a selection that stopped being an accent fill, a label that dropped
# from KT_MID to the unreadable KT_DIM, and a glyph drawn in the background's
# own slot — all three have shipped.
#
# The surfaces that carry the flag are the ones whose colour is load-bearing:
# the two menus this desktop is aimed with, the keybind card, the chooser and
# the help. `cells.c` is the one backend behind all of them, so a golden
# written for one is a golden in the format every other one prints.
cells_golden() {		# <name> <argv…>
    _c_name=$1; shift
    _c_file="$GOLD/cells-$_c_name.txt"
    _c_got="$OUT/cells-$_c_name.txt"
    ( cd testing/fixtures/shell &&
      env LC_ALL=C TZ=UTC HOME="$PWD" \
          XDG_CACHE_HOME=/nonexistent-kdos-cache \
          XDG_CONFIG_HOME="$PWD/config" \
          XDG_DATA_HOME=/nonexistent-kdos-data \
          XDG_DATA_DIRS=/nonexistent-kdos-datadirs \
          XDG_RUNTIME_DIR=/nonexistent-kdos-run \
          KDOS_PANEL_ROOT="$PWD/panelroot" KDOS_PANEL_NOW=1735689600 \
          "$DUMPCK" "$@" ) > "$_c_got" 2>/dev/null
    if ! grep -qE '^[0-9]+ [0-9]+ U\+[0-9A-Fa-f]+ ' "$_c_got"; then
        echo "  cells-$_c_name: --dump-cells printed no cells"
        golden_fail=1
        return 0
    fi
    if [ "${KDOS_GOLDEN_UPDATE:-0}" = 1 ]; then
        cp "$_c_got" "$_c_file"; echo "  wrote cells-$_c_name"; return 0
    fi
    if [ ! -f "$_c_file" ]; then
        echo "  cells-$_c_name: no golden committed"; golden_fail=1; return 0
    fi
    if diff -u "$_c_file" "$_c_got" > "$OUT/cells-$_c_name.diff"; then
        echo "  cells-$_c_name"
    else
        echo "  cells-$_c_name DRIFTED:"
        head -20 "$OUT/cells-$_c_name.diff" | sed 's/^/    /'
        golden_fail=1
    fi
}
cells_golden start       start --dump-cells
cells_golden menu-system menu system --dump-cells
cells_golden keys        keys --dump-cells
cells_golden doc         doc --dump-cells

# THE CELL VERDICT IS ITS OWN CHECK, because the frame check above has already
# run: a `golden_fail` raised by a cells_golden after it would be recorded and
# never read, and a comparison whose answer nothing acts on is a comparison
# that cannot fail. The message names the CELL half specifically — a colour
# drift and a geometry drift are fixed by looking at different things.
if [ "$golden_fail" != 0 ]; then
    echo
    echo "  A cell golden changed — a glyph, a colour slot or an attribute."
    echo "  If the change is intended:"
    echo "      KDOS_GOLDEN_UPDATE=1 testing/selftest.sh"
    echo "  then read the diff in git before committing it."
    exit 1
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
        -Isrc/desktop/kdos-shell -Isrc/libs/libkdisp -Isrc/libs/libkcon -Isrc/libs/libkwl \
        -Isrc/libs/libktui -Isrc/libs/libkcolor \
        -Isrc/libs/libkxdg -Isrc/libs/libkbase -Isrc/libs/libkicon \
        -Isrc/libs/libkchrome -Isrc/libs/libkproc \
        $(pkg-config --cflags libpipewire-0.3) \
        testing/fixtures/privacy/privacycheck.c src/desktop/kdos-shell/privacy.c \
        src/libs/libkproc/*.c src/libs/libkbase/*.c \
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
echo "==> the key card can be printed without a display server"
# `--print` exists so the card reaches a printer, a wall and an ssh session. It
# is only useful if it returns BEFORE a display server is opened — a print mode
# that needed a session would fail in exactly the cases it was added for. The
# structural check is that the branch comes before the KDispConfig.
_pl=$(grep -n 'print_rows()' src/desktop/kdos-shell/keys.c | tail -1 | cut -d: -f1)
_kl=$(grep -n 'KDispConfig cfg = {' src/desktop/kdos-shell/keys.c | head -1 | cut -d: -f1)
if [ -n "$_pl" ] && [ -n "$_kl" ] && [ "$_pl" -lt "$_kl" ]; then
    echo "  --print returns before any display server is opened"
else
    echo "  FAIL  --print does not return before kdisp_init"
    exit 1
fi

echo "==> the tone ladder gives the bar a legible middle in every accent"
#
# The eight VT slots cannot say what a raised button is: `variant` against
# `backdrop` is 1.00:1, so a panel painted in its own background colour is the
# same colour as the desktop. libkchrome derives the missing middle, and this
# is the claim that it works — in all four accents, not just the one anybody
# looks at.
#
TONE_BIN="$OUT/tonecheck"
if $CC $STD -o "$TONE_BIN" testing/fixtures/tone/tonecheck.c \
        src/libs/libkchrome/kch_tone.c src/libs/libkcolor/kcolor.c \
        src/libs/libktui/ktui_theme.c src/libs/libkbase/*.c \
        -Isrc/libs/libkbase -Isrc/libs/libkcolor -Isrc/libs/libktui \
        -Isrc/libs/libkcell -Isrc/libs/libkicon -Isrc/libs/libkchrome \
        $(pkg-config --cflags pixman-1 2>/dev/null) >/dev/null 2>&1; then
    "$TONE_BIN" || exit 1
else
    echo "  tone ladder (skipped — no pixman headers on this host)"
fi

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
