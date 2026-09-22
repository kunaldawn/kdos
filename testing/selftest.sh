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
INC="-Isrc/libs/libkbase -Isrc/libs/libkwm -Isrc/libs/libkvt -Isrc/libs/libkdisp -Isrc/libs/libkcolor -Isrc/libs/libktui -Isrc/libs/libkxdg -Isrc/libs/libkpkg -Isrc/libs/libkbuild -Isrc/tools/kdos-portup -Isrc/libs/libkproc -Isrc/libs/libksig -Isrc/libs/libkpack"
OUT=$(mktemp -d)

#
# THE SUITE KEEPS ITS OWN STATE DIRECTORY, and every program under test that
# writes one lands here rather than in the home directory of whoever ran it.
#
# A surface that remembers where its window was reads it back the next time one
# opens, so without this a reference frame would depend on what a PREVIOUS run
# of this suite happened to leave behind — a golden that passes on a clean
# machine and drifts on the developer's, which is the worst shape a reference
# frame can have. It also means the suite cannot damage a real desktop's state
# by being run.
#
XDG_STATE_HOME="$OUT/state"
export XDG_STATE_HOME
mkdir -p "$XDG_STATE_HOME"

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
# which is the only way this suite sees an out-of-bounds read or a NULL deref
# in a parser: a plain run takes both for a pass. One thing has to be arranged
# for it, and only one: LEAK CHECKING IS
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
    for f in PNG:libpng JPEG:libjpeg WEBP:libwebp SIXEL:libsixel GIF:libnsgif; do
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
    src/libs/libkdisp/*.c \
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
# kinstall compiles catalogue.c IN — the Applications page reads the catalogue
# rather than running kdos-appbox, because a live installer cannot assume
# anything is on $PATH in the target it is building.
$CC $STD $WARN $INC -Isrc/packages/kdos-installer -Isrc/packages/kdos-appbox \
    -o "$OUT/kinstall" \
    src/packages/kdos-installer/*.c src/packages/kdos-appbox/catalogue.c \
    src/libs/libkbase/*.c \
    src/libs/libktui/*.c src/libs/libkcolor/*.c -lcrypt
echo "  kinstall"
# The plan resolves over the SHIPPED catalogue, and the two renderings agree.
# A disagreement between the text dump and the JSON is what makes a dump
# untrustworthy, and it is the one thing an installer's dumps must not be.
KDOS_CATALOGUE=src/packages/kdos-appbox/catalogue "$OUT/kinstall" \
    --dump plan >/dev/null
KDOS_CATALOGUE=src/packages/kdos-appbox/catalogue "$OUT/kinstall" \
    --dump plan --json | python3 -c 'import json,sys; d=json.load(sys.stdin);
assert d["apps"], "the plan chose no applications"
assert d["apps_bytes"] > 0, "the chosen set costs nothing"
assert d["apps_route"] in ("none","import","network","pending"), d["apps_route"]'
echo "  kinstall plan"
$CC $STD $WARN $INC -Isrc/packages/kdos-appbox -o "$OUT/kdos-appbox" \
    src/packages/kdos-appbox/*.c src/libs/libkbase/*.c src/libs/libktui/*.c \
    src/libs/libkcolor/*.c src/libs/libkxdg/*.c
echo "  kdos-appbox"
# The catalogue parser, against a fixture AND against the shipped file. The
# fixture pins the rules — base-first chains, the meta fallback, an expand that
# dedupes; the real catalogue is what a surface will actually read, and a row
# added by hand that the parser rejects is a store that opens empty with no
# error on the screen.
KDOS_CATALOGUE=testing/fixtures/catalogue/catalogue "$OUT/kdos-appbox" \
    catalogue --selftest
KDOS_CATALOGUE=src/packages/kdos-appbox/catalogue "$OUT/kdos-appbox" \
    catalogue >/dev/null
KDOS_CATALOGUE=src/packages/kdos-appbox/catalogue "$OUT/kdos-appbox" \
    catalogue --groups >/dev/null
echo "  catalogue"
# The Containerfile generator. podman is not here and never will be, so what is
# under test is the one part that is pure — and it is the part that matters: a
# missing snapshot pin is an unreproducible image, a missing
# `dpkg --add-architecture i386` is `wine32 has no installation candidate`
# naming the package rather than the architecture, and an apt RUN emitted
# against a row with no packages is `apt-get: not found` on an alpine rootfs.
KDOS_CATALOGUE=testing/fixtures/catalogue/catalogue "$OUT/kdos-appbox" \
    store --selftest
# And the chain resolves over the SHIPPED catalogue, with the snapshot pinned
# so the generator takes its longest path rather than the `off` short one.
sed 's/^snapshot = auto/snapshot = 20260824T000000Z/' \
    src/packages/kdos-appbox/catalogue > "$OUT/catalogue-pinned"
KDOS_CATALOGUE="$OUT/catalogue-pinned" "$OUT/kdos-appbox" \
    install essential --dry-run >/dev/null
echo "  store"
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
# the tree; both link a deliberately short list, so both compile here.
#
# libkcolor IS ON kdos-powerd's LIST and that is the point of the `accent`
# verb's safety argument: the scheme name is matched against the palette's own
# closed table rather than sanitised by a character class copied into the
# daemon. Linking it here keeps this check the same link as the recipe's.
$CC $STD $WARN $INC -Isrc/libs/libkcolor -o "$OUT/kdos-powerd" \
    src/desktop/kdos-powerd/main.c src/libs/libkbase/*.c src/libs/libkcolor/*.c
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
grep -q "persistence = frozen      recorded in the profile" "$OUT/box2.txt" \
    || { echo "  FAIL  an app box records that it is frozen"; cat "$OUT/box2.txt"; exit 1; }
grep -q "! frozen is not enforced" "$OUT/box2.txt" \
    || { echo "  FAIL  frozen must be printed as unenforced"; cat "$OUT/box2.txt"; exit 1; }
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

#
# THE FOOTER IS INSIDE THE HASH. It is what says where the filesystem, the
# metadata and the icon are, so a hash that stopped at the signature block
# left every one of those offsets rewritable under a signature that still
# verified. `flags` is at byte 12 of the 512-byte footer and passes every
# structural check, so it is the field that proves the HASH is what catches
# this and not footer_consistent.
#
cp "$PKS/app.good.kpack" "$OUT/moved.kpack"
FSZ=$(stat -c%s "$OUT/moved.kpack")
printf '\010' | dd of="$OUT/moved.kpack" bs=1 \
    seek=$((FSZ - 512 + 12)) conv=notrunc status=none
if KDOS_KEYS="$OUT/keys" "$OUT/kdos-pack" verify "$PKS/app.good.kpack" \
       >/dev/null 2>&1; then
    echo "  ok    the signed pack verifies"
else
    echo "  FAIL  the untouched signed pack does not verify"; exit 1
fi
if KDOS_KEYS="$OUT/keys" "$OUT/kdos-pack" verify "$OUT/moved.kpack" \
       >/dev/null 2>&1; then
    echo "  FAIL  a footer field rewritten under the signature still verified"
    exit 1
fi
echo "  ok    and one footer field rewritten under it does not"
rm -f "$OUT/moved.kpack"
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
        # xdg-foreign: kdos-pick tells the compositor whose child a dialog
        # is, which is the only way a portal's file chooser reaches the
        # window that asked for it. libkwl includes this unconditionally too,
        # so it is as mandatory here as the lock role's protocol.
        "$SCANNER" client-header \
            "$(pkg-config --variable=pkgdatadir wayland-protocols)/unstable/xdg-foreign/xdg-foreign-unstable-v2.xml" \
            "$PROTO/xdg-foreign-unstable-v2-client-protocol.h"
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
        "$SCANNER" private-code \
            "$_wp/unstable/xdg-foreign/xdg-foreign-unstable-v2.xml" \
            "$PROTO/xdg-foreign-unstable-v2-protocol.c"
        KCINC="-Isrc/libs/libkbase -Isrc/libs/libktui -Isrc/libs/libkcolor \
-Isrc/libs/libkcell -Isrc/libs/libkwl -Isrc/libs/libkdisp -Isrc/libs/libkwm"
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

        # AND THE SLANT NO BITMAP FACE CARRIES. An italic companion is taken
        # only where its metrics match the upright face's, which on this
        # image's faces they do not — so the slant is sheared out of the
        # upright mask, and the shear's geometry is the half of it that can be
        # measured with no font and no frame.
        $CC $STD $WARN -Isrc/libs/libkbase -Isrc/libs/libktui \
            -Isrc/libs/libkcolor -Isrc/libs/libkcell \
            $(pkg-config --cflags fcft pixman-1) \
            -o "$OUT/obliquecheck" testing/fixtures/oblique/obliquecheck.c \
            src/libs/libkcell/*.c src/libs/libktui/*.c \
            src/libs/libkbase/*.c \
            $(pkg-config --libs fcft pixman-1)
        "$OUT/obliquecheck" >/dev/null
        echo "  obliquecheck (a synthesised italic leans, and leans evenly)"

        # A TRAY ITEM'S OWN PICTURE, decoded from bytes rather than found by
        # name. Every surface turns its icons OFF for a dump — a golden frame
        # is the character grid — so this is the one path in libkicon that no
        # reference frame can reach, and without a fixture it is compiled and
        # never run. It was: the PNG the tray fixture published was malformed
        # and nothing had ever decoded it.
        #
        # STDERR IS DISCARDED because libpng prints its own complaint about
        # the deliberately truncated blob, and a refusal that says so twice
        # reads as a failure.
        #
        # libpng is asked for separately: the block above this one is guarded
        # on fcft and pixman, which libkicon needs too but which say nothing
        # about a decoder.
        if pkg-config --exists libpng 2>/dev/null; then
        $CC $STD $WARN -Isrc/libs/libkbase -Isrc/libs/libktui \
            -Isrc/libs/libkcolor -Isrc/libs/libkcell -Isrc/libs/libkicon \
            -Isrc/libs/libkxdg \
            $(pkg-config --cflags pixman-1 fcft libpng) \
            -o "$OUT/iconpng" testing/fixtures/iconpng/iconpng.c \
            src/libs/libkicon/*.c src/libs/libkcell/*.c src/libs/libktui/*.c \
            src/libs/libkcolor/*.c src/libs/libkbase/*.c src/libs/libkxdg/*.c \
            $(pkg-config --libs pixman-1 fcft libpng)
        "$OUT/iconpng" 2>/dev/null \
            || { echo "  kicon_slot_png FAILED"; exit 1; }
        else
            echo "  kicon_slot_png (skipped — no libpng on this host)"
        fi

        $CC $STD $WARN -c -I"$PROTO" $KCINC \
            $(pkg-config --cflags fcft pixman-1 xkbcommon wayland-client) \
            -o "$OUT/kwl.o" src/libs/libkwl/kwl.c
        $CC $STD $WARN -c -I"$PROTO" $KCINC \
            $(pkg-config --cflags fcft pixman-1 xkbcommon wayland-client) \
            -o "$OUT/kdisp.o" src/libs/libkdisp/kdisp.c
        # kwl_font.c is HERE and not left to the two links below: it is the
        # only file in the archive that reaches fontconfig, so a name it gets
        # wrong is a compile error nothing else in this suite would show.
        for f in src/libs/libkwl/kwl_key.c src/libs/libkwl/kwl_font.c; do
            $CC $STD $WARN -c -I"$PROTO" $KCINC \
                $(pkg-config --cflags fcft fontconfig pixman-1 xkbcommon \
                             wayland-client) \
                -o "$OUT/$(basename "$f" .c).o" "$f"
        done
        echo "  libkwl"

        # kdos-lock's client half draws through exactly the headers just
        # generated, so it costs one more compile and is the only gate it has.
        $CC $STD $WARN -c -I"$PROTO" -Isrc/libs/libkbase -Isrc/libs/libktui \
            -Isrc/libs/libkcolor -Isrc/libs/libkcell -Isrc/libs/libkwl -Isrc/libs/libkdisp -Isrc/libs/libkwm \
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
                # kdos-peek, kdos-pix and the picture unit they share are the
                # files with a decoder and an archive reader behind them.
                # Absent either library they are skipped BY NAME rather than
                # the whole shell compile being gated on libraries the other
                # forty files do not need.
                _pk=""
                case "$f" in
                */peek.c|*/pix.c|*/picture.c)
                    if pkg-config --exists libarchive libpng libjpeg libwebp \
                            2>/dev/null; then
                        _pk="-Isrc/libs/libkimg -DKIMG_HAVE_PNG"
                        _pk="$_pk -DKIMG_HAVE_JPEG -DKIMG_HAVE_WEBP"
                        _pk="$_pk $(pkg-config --cflags libarchive libpng \
                                                libjpeg libwebp)"
                    else
                        echo "  $(basename "$f") (skipped — no libarchive)"
                        continue
                    fi
                    ;;
                esac
                $CC $STD $SHWARN -c -I"$PROTO" -Isrc/desktop/kdos-shell $_pk \
                    -Isrc/libs/libkbase -Isrc/libs/libktui -Isrc/libs/libkcolor \
                    -Isrc/libs/libkcell -Isrc/libs/libkwl -Isrc/libs/libkdisp -Isrc/libs/libkwm -Isrc/libs/libkxdg \
                    -Isrc/libs/libkicon -Isrc/libs/libkchrome -Isrc/libs/libkproc \
                    -Isrc/libs/libkvt \
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
                -Isrc/libs/libkcell -Isrc/libs/libkwl -Isrc/libs/libkdisp -Isrc/libs/libkwm -Isrc/libs/libkxdg \
                -Isrc/libs/libkicon -Isrc/libs/libkchrome -Isrc/libs/libkproc \
                $(ls src/desktop/kdos-res/*.c | grep -v resctl.c) \
                src/libs/libkwl/*.c src/libs/libkdisp/*.c src/libs/libkwm/*.c src/libs/libkcell/*.c src/libs/libktui/*.c \
                src/libs/libkcolor/*.c src/libs/libkbase/*.c \
                src/libs/libkxdg/*.c src/libs/libkicon/*.c \
                src/libs/libkchrome/*.c src/libs/libkproc/*.c \
                "$PROTO"/*-protocol.c \
                $(pkg-config --cflags --libs fcft fontconfig pixman-1 \
                             xkbcommon wayland-client libpng)
            RESBIN="$OUT/kdos-res"
            echo "  kdos-res"

            # kdos-term AS A WINDOW. The stubbed build further down is the
            # one the goldens run, because a `--dump` needs no display at all;
            # this one exists to prove the SAME source still links the real
            # libkwl, which is the half a bare host cannot check.
            $CC $STD $SHWARN -o "$OUT/kdos-term-wl" -I"$PROTO" $KIMG_FLAGS \
                -Isrc/desktop/kdos-term \
                -Isrc/libs/libkbase -Isrc/libs/libktui -Isrc/libs/libkcolor \
                -Isrc/libs/libkcell -Isrc/libs/libkwl -Isrc/libs/libkdisp \
                -Isrc/libs/libkvt -Isrc/libs/libkxdg \
                src/desktop/kdos-term/*.c \
                src/libs/libkwl/*.c src/libs/libkdisp/*.c \
                src/libs/libkcell/*.c src/libs/libktui/*.c src/libs/libkvt/*.c \
                src/libs/libkcolor/*.c src/libs/libkbase/*.c \
                src/libs/libkxdg/*.c $KIMG_SRC \
                "$PROTO"/*-protocol.c \
                $(pkg-config --cflags --libs fcft fontconfig pixman-1 \
                             xkbcommon wayland-client) $KIMG_LIBS
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
for _v in vim htop mc less tmux malformed attrs links prompts; do vt_golden "$_v"; done
if [ "$vt_fail" != 0 ]; then
    echo
    echo "  A recorded stream renders differently than it did. The fixture did"
    echo "  not change — it is bytes — so the state machine did."
    echo "      KDOS_GOLDEN_UPDATE=1 testing/selftest.sh"
    echo "  then read the diff in git before committing it."
    exit 1
fi

#
# THE SHIPPED SHELL SAYS WHERE ITS PROMPT IS, AND WHAT THE LAST COMMAND DID.
#
# The marks are useless without an emitter, and the emitter is one line of
# `/etc/bash.bashrc` that is easy to lose to a prompt rewrite — which is
# exactly the edit that would look harmless. It is checked here rather than
# read, because the two things worth knowing are that PROMPT_COMMAND still
# carries it and that the status it reports is the PREVIOUS command's: `$?` is
# overwritten by anything the function does before it looks.
#
# `-i` because the file returns immediately for a non-interactive shell, which
# is the first thing in it.
#
_pm=$(bash --rcfile fs/etc/bash.bashrc -i -c 'false; __kdos_mark_prompt' \
      </dev/null 2>/dev/null | tr -d '\033')
# AND THAT IT IS INSTALLED, which calling it cannot show: a function defined
# and never put on PROMPT_COMMAND emits exactly the right bytes and never runs.
_pc=$(bash --rcfile fs/etc/bash.bashrc -i -c 'printf %s "$PROMPT_COMMAND"' \
      </dev/null 2>/dev/null)
case "$_pc" in
*__kdos_mark_prompt*) _pc_ok=1 ;;
*) _pc_ok=0 ;;
esac
if [ "$_pm" = ']133;D;1\]133;A\' ] && [ "$_pc_ok" = 1 ]; then
    echo "  the shipped bashrc marks its prompt and reports the exit status"
else
    echo "  THE SHIPPED BASHRC EMITS NO PROMPT MARKS: [$_pm] [$_pc]"
    echo "  fs/etc/bash.bashrc must keep __kdos_mark_prompt on PROMPT_COMMAND"
    exit 1
fi

#
# A FRAME IS BRACKETED WHERE THE TERMINAL SAID IT UNDERSTANDS THE BRACKET.
#
# Synchronized output is asked for with DECRQM and never assumed, and both
# halves of that are failure modes rather than tidiness. Assuming it on would
# put `CSI ?2026h` in front of every frame a terminal that does not know the
# mode receives — harmless there, but the SAME assumption on a terminal that
# does know it and never gets the close leaves a screen frozen. Taking any
# reply for a yes is the other half: `0` is the answer meaning "I do not know
# this mode", and it arrives from exactly the terminals that must not be
# bracketed.
#
# The two queries go out in ONE write and their replies are told apart by
# scanning, which the kitty case below is the proof of: the keyboard push and
# the sync capability both come out of a single read.
#
# stdin is a pipe carrying what the terminal "answered" and stdout is a file,
# so this is the real ktui_term_init/enter_screen path with no tty anywhere.
#
cat > "$OUT/syncdrv.c" <<'SYNCEOF'
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ktui.h"

static int bad;
static char out[65536];

/* One frame of "hi" in `attr`, drawn under `term`, with `reply` as everything
 * the terminal ever says back. Returns what the library wrote to its terminal,
 * NUL terminated. */
static const char *frame(const char *reply, const char *term, int attr,
			 const KtuiCell *put)
{
	int in[2], s0 = dup(0), s1 = dup(1);
	int f = open("syncdrv.out", O_RDWR | O_CREAT | O_TRUNC, 0600);

	if (pipe(in) || f < 0 || s0 < 0 || s1 < 0)
		return "";
	if (write(in[1], reply, strlen(reply)) < 0)
		bad = 1;
	close(in[1]);
	dup2(in[0], 0);
	close(in[0]);
	dup2(f, 1);

	setenv("TERM", term, 1);
	/* Named, not inherited: `COLORTERM` is what decides between 24-bit and
	 * indexed SGR, so a suite that let the surrounding terminal supply it
	 * would assert a different sequence on a developer's machine than in
	 * the build container. */
	setenv("COLORTERM", "truecolor", 1);
	setenv("COLUMNS", "20", 1);
	setenv("LINES", "4", 1);
	ktui_term_init(0);
	ktui_draw_init();
	/* Five frames in one process: the cell buffers survive between them
	 * and a diff against the last one would emit nothing. A program has
	 * this for free on its first frame. */
	ktui_draw_invalidate();
	ktui_draw_clear();
	ktui_draw_text(0, 0, 20, "hi", KT_TEXT, KT_BG, attr);
	/* A whole cell, which is the only way a literal colour reaches a
	 * frame — every other draw call takes slots and clears them. */
	if (put)
		ktui_draw_put(0, 0, put);
	ktui_draw_flush();
	ktui_term_shutdown();

	dup2(s1, 1);
	dup2(s0, 0);
	close(s0);
	close(s1);

	lseek(f, 0, SEEK_SET);
	ssize_t n = read(f, out, sizeof(out) - 1);
	close(f);
	unlink("syncdrv.out");
	out[n > 0 ? n : 0] = 0;
	return out;
}

/* Is `param` one of the parameters of the frame's first SGR? A TOKEN match:
 * ";3" is a substring of ";38;2;" and of ";37", and both are things this
 * emitter writes. */
static int sgr_has(const char *s, const char *param)
{
	const char *p = strstr(s, "\033[1;1H");

	if (!p || !(p = strstr(p, "\033[0")))
		return 0;
	for (p += 2; *p && *p != 'm';) {
		const char *tok = p;

		while (*p && *p != ';' && *p != 'm')
			p++;
		if ((size_t)(p - tok) == strlen(param) &&
		    !strncmp(tok, param, (size_t)(p - tok)))
			return 1;
		if (*p == ';')
			p++;
	}
	return 0;
}

static void want(int cond, const char *what)
{
	if (!cond) {
		printf("    %s\n", what);
		bad = 1;
	}
}

#define RICH "xterm-256color"

int main(void)
{
	/* Answered "reset", which is a terminal that knows the mode. */
	const char *s = frame("\033[?2026;2$y", RICH, KT_A_NONE, NULL);
	const char *h = strstr(s, "\033[?2026h");
	const char *l = h ? strstr(h, "\033[?2026l") : NULL;
	const char *txt = strstr(s, "hi");

	want(ktui_caps & KT_CAP_SYNC, "a terminal that answered 2 is not held");
	want(h && l && txt, "the frame is not bracketed at all");
	want(h && l && txt && txt > h && txt < l,
	     "the frame's cells are outside the bracket");

	/* Answered "not recognised". */
	s = frame("\033[?2026;0$y", RICH, KT_A_NONE, NULL);
	want(!(ktui_caps & KT_CAP_SYNC), "a 0 answer was taken for a yes");
	/* The QUERY is in this output whatever the answer was; what must not
	 * be is either half of the bracket. */
	want(!strstr(s, "\033[?2026h") && !strstr(s, "\033[?2026l"),
	     "a 0 answer still got the bracket");

	/* Said nothing at all, which is most terminals. */
	s = frame("", RICH, KT_A_NONE, NULL);
	want(!(ktui_caps & KT_CAP_SYNC), "silence was taken for a yes");
	want(!strstr(s, "\033[?2026h"), "silence still got the bracket");
	want(strstr(s, "hi") != NULL, "the frame itself went missing");

	/* Both replies, in the order the queries went out, out of one read. */
	s = frame("\033[?0u\033[?2026;2$y", RICH, KT_A_NONE, NULL);
	want(ktui_caps & KT_CAP_SYNC, "the second reply was not read");
	want(strstr(s, "\033[>1u") != NULL, "the first reply was not read");
	want(strstr(s, "\033[<1u") != NULL, "the keyboard push was not popped");

	/* The kitty reply alone, which is what the probe did before it asked
	 * about anything else. */
	s = frame("\033[?0u", RICH, KT_A_NONE, NULL);
	want(!(ktui_caps & KT_CAP_SYNC), "no DECRQM answer was taken for one");
	want(strstr(s, "\033[>1u") != NULL, "the keyboard push was lost");

	/*
	 * THE STYLES, ON THE WIRE AND OFF IT.
	 *
	 * The exact prefix, because the ORDER is part of what a terminal
	 * parses and a reordering here is a change worth being told about.
	 * Bold, underline, italic, strike and overline, then the colour.
	 */
	static const char *pfx = "\033[0;4;1;3;9;53;38;2;";
	int all = KT_A_BOLD | KT_A_UNDERLINE | KT_A_ITALIC | KT_A_STRIKE |
		  KT_A_OVERLINE;

	s = frame("", RICH, all, NULL);
	const char *sgr = strstr(s, "\033[1;1H");

	sgr = sgr ? strstr(sgr, "\033[0") : NULL;
	if (!sgr || strncmp(sgr, pfx, strlen(pfx))) {
		printf("    the styled cell's SGR is not \"%s...\"\n", pfx + 2);
		bad = 1;
	}

	/*
	 * AND NONE OF THEM ON A REAL VT, for bold's reason: there an attribute
	 * bit is a font page or a colour the palette does not own, so an
	 * italic row would come out as line noise or as a shade nothing else
	 * on the screen uses.
	 */
	s = frame("", "linux", all, NULL);
	want(!sgr_has(s, "3"), "italic reached a VT");
	want(!sgr_has(s, "9"), "a strike reached a VT");
	want(!sgr_has(s, "53"), "an overline reached a VT");
	want(!sgr_has(s, "1") && !sgr_has(s, "4"),
	     "bold or underline reached a VT");

	/*
	 * A COLOUR THE CELL NAMED ITSELF, AND THE SHAPE OF ITS UNDERLINE.
	 *
	 * The literal is what goes on the wire, not the slot beside it — a
	 * terminal that reduced a program's own colour back to the palette
	 * would have paid for it and thrown it away. The sub-parameter forms
	 * go only where 24-bit colour does, for the reason the VT case below
	 * makes concrete.
	 */
	KtuiCell lit = { 'q', KT_TEXT, KT_BG,
			 (uint16_t)(KT_A_UNDERLINE | KT_A_FGRGB | KT_A_BGRGB |
				    KT_A_ULCOLOR | KT_UL_SET(KT_UL_CURLY)),
			 0x123456, 0x654321, 0xfedcba };

	s = frame("", RICH, KT_A_NONE, &lit);
	want(strstr(s, "38;2;18;52;86") != NULL,
	     "the cell's own foreground did not reach the wire");
	want(strstr(s, "48;2;101;67;33") != NULL,
	     "the cell's own background did not reach the wire");
	want(strstr(s, ";4:3") != NULL, "the underline's shape was flattened");
	want(strstr(s, ";58:2::254:220:186") != NULL,
	     "the underline's colour was dropped");

	/*
	 * AND NONE OF IT ON A VT, where eight colours are all there are: a
	 * literal has nowhere to go and the slot is the honest one of the two,
	 * and `4:3` read by a terminal that drops the colon is SGR 43 — a
	 * green background where a program asked for a wavy line.
	 */
	s = frame("", "linux", KT_A_NONE, &lit);
	want(!strstr(s, "38;2;"), "a literal reached a VT");
	want(!strstr(s, ":"), "a sub-parameter reached a VT");
	want(sgr_has(s, "37") || sgr_has(s, "30") || sgr_has(s, "31") ||
	     sgr_has(s, "32") || sgr_has(s, "33") || sgr_has(s, "34") ||
	     sgr_has(s, "35") || sgr_has(s, "36"),
	     "the VT was left with no colour at all");
	return bad;
}
SYNCEOF
$CC $STD $SHWARN $INC -o "$OUT/syncdrv" "$OUT/syncdrv.c" \
    src/libs/libkbase/*.c src/libs/libkcolor/*.c src/libs/libktui/*.c
if (cd "$OUT" && ./syncdrv); then
    echo "  a frame is held only where the terminal answered DECRQM"
else
    echo "  A FRAME IS BRACKETED ON A TERMINAL THAT NEVER SAID IT COULD"
    exit 1
fi

#
# THE PICTURE PATH TILES THROUGH libkcell's ONE SCALER, so it needs that
# archive's header — fcft, for the declaration alone; no fcft symbol is called
# from the tiler — and its one file. Without fcft the decoders are left out of
# THIS build rather than half-linked; libkimg's own blocks above still run.
#
# LEAVING THESE UNSET IS A TERMINAL WITH NO PICTURES and a sixel golden that
# silently loses a row, which reads as the terminal drifting rather than as a
# build without a decoder in it.
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

#
# kdos-term AGAINST A STUB libkwl, which is what lets the goldens below run on
# a bare host: a `--dump` needs no display at all, and the state machine, the
# frame and the image path are the whole of what it exercises. The SHIPPED
# binary links the real one — see testing/fixtures/term/kwlstub.c for why the
# stub's probe answers no.
#
$CC $STD $SHWARN $INC -Isrc/libs/libkwl $TERM_KIMG_FLAGS \
    -Isrc/desktop/kdos-term -o "$OUT/kdos-term" \
    src/desktop/kdos-term/*.c testing/fixtures/term/kwlstub.c \
    src/libs/libkbase/*.c src/libs/libkcolor/*.c src/libs/libktui/*.c \
    src/libs/libkdisp/*.c src/libs/libkvt/*.c \
    src/libs/libkxdg/*.c $TERM_KIMG_SRC $TERM_KIMG_LIBS
echo "  kdos-term (against a stub libkwl)"

# Raised by any golden that drifted, anywhere in the suite, and read at the end.
# It is initialised HERE, ahead of the FIRST family of goldens, because a later
# `golden_fail=0` would clear what an earlier one had already recorded — and an
# initialiser that moved below a golden leaves the variable unset, which every
# `!= 0` test below reads as a failure.
golden_fail=0

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
    # The same flag every other golden here honours, and for the same reason:
    # a frame that changed on purpose is regenerated and read in the diff.
    if [ "${KDOS_GOLDEN_UPDATE:-0}" = 1 ]; then
        cp "$OUT/$_name.txt" "testing/goldens/$_name.txt"
        echo "  wrote $_name"
        return 0
    fi
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

#
# THE UNICODE PLACEHOLDERS, which is how a picture reaches a terminal through
# `tmux`. The image is TRANSMITTED and not placed (`a=t`), and then U+10EEEE
# cells say where it goes, carrying its id in their foreground colour. Without
# the substitution those cells are a codepoint no font has and the picture is
# never drawn at all — so what the golden holds is the same shape a placed
# picture leaves: the fallback in the top-left cell and blanks under the rest.
#
case "$KIMG_FLAGS" in
*-DKIMG_HAVE_PNG*)
    term_golden term-kitty-uniph-44x10 44x10 -e /bin/sh -c \
        'printf "\033_Ga=t,f=100,i=42,q=2;%s\033\\\\" \
             "$(base64 -w0 testing/fixtures/img/valid.png)"
         printf "\033[38;2;0;0;42m"
         for r in 1 2; do
             for c in 1 2 3 4; do printf "\364\216\273\256"; done
             printf "\n"
         done
         printf "\033[0mafter\n"'
    ;;
*)
    echo "  term-kitty-uniph-44x10 (skipped — no PNG decoder on this host)"
    ;;
esac

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
echo "==> a game's desktop entry is what makes it reachable, and nothing checks one"
#
# A RECIPE THAT BUILDS AND AN ENTRY THAT DOES NOT WORK LOOK THE SAME. Nothing
# in this tree validates a desktop entry's contents: a `Categories=Games;`
# with the plural, a missing `Terminal=true`, or an `Exec=` naming a binary the
# recipe never installs all build, install and then land the program under
# Accessories or nowhere, with no warning at any stage. That is the whole of
# what a game port can get wrong here, so it is what this asserts.
#
# The entries are read out of the build.sh heredocs rather than out of a built
# image, so this runs on any host and fails at the recipe rather than after a
# seven-minute packaging run.
_gamefail=0
for _p in nethack frotz bsd-games moon-buggy; do
    _bs="ports/core/$_p/build.sh"
    [ -f "$_bs" ] || { echo "  $_p: no build.sh"; _gamefail=1; continue; }
    # The recipe parses AND its whole dependency closure resolves, which is
    # the same call the block above makes for bash — a `depends` naming a port
    # this tree does not have is the other way a game recipe fails late.
    PORT_REPO="$PWD/ports/core $PWD/src/packages" KPKG_CONF=/nonexistent \
        PKGDB_DIR=/dev/null "$OUT/kdos-kpkg" kpkgdepends "$_p" \
        > "$OUT/game-$_p.deps" 2>&1 \
        || { echo "  $_p: the recipe does not parse or its depends do not resolve"
             sed 's/^/    /' "$OUT/game-$_p.deps"; _gamefail=1; continue; }
    bash -n "$_bs" || { echo "  $_p: build.sh does not parse"; _gamefail=1; }
    # Every entry the recipe writes, pulled out of its heredocs.
    _n=$(awk '/^\[Desktop Entry\]$/{e=1} e{print} /^Keywords=/{if(e){print "@@";e=0}}' \
         "$_bs" | grep -c '^\[Desktop Entry\]$')
    [ "${_n:-0}" -ge 1 ] || { echo "  $_p: writes no desktop entry"; _gamefail=1; }
    awk '/^\[Desktop Entry\]$/{e=1;t=0;c=0;x="";n=""} 
         e&&/^Terminal=true$/{t=1}
         e&&/^Categories=/{c=($0 ~ /(^|;)Game(;|$)/ || $0 ~ /Categories=Game;/)}
         e&&/^Exec=/{x=substr($0,6)}
         e&&/^Name=/{n=substr($0,6)}
         e&&/^Keywords=/{
           if(!t) printf "  ENTRY %s: no Terminal=true\n", n
           if(!c) printf "  ENTRY %s: Categories has no Game token\n", n
           if(x=="") printf "  ENTRY %s: no Exec\n", n
           split(x,w," ");
           if(w[1] ~ /\//) printf "  ENTRY %s: Exec names a path, not a command\n", n
           print "  " n " -> " w[1]
           e=0 }' "$_bs" > "$OUT/game-$_p.entries"
    grep -q "^  ENTRY " "$OUT/game-$_p.entries" && { sed -n 's/^  ENTRY/    /p' \
        "$OUT/game-$_p.entries"; _gamefail=1; }
    sed -n 's/^  \([^ ]*.*\) -> \(.*\)$/    \1 → \2/p' "$OUT/game-$_p.entries"
done
if [ "$_gamefail" = 0 ]; then
    echo "  four recipes parse; every entry is Terminal=true, Game-categorised"
    echo "  and names a bare command"
else
    echo "  A GAME ENTRY WOULD INSTALL AND NOT BE REACHABLE"
    exit 1
fi

# AND THE ONE THING A GAME ENTRY CANNOT CLAIM WITHOUT A TYPE BEHIND IT.
# frotz's entry claims application/x-zmachine, and shared-mime-info — the
# version this image builds — defines no such type, so without the XML the
# recipe installs, the claim resolves to nothing and says so nowhere.
_smi=$(ls ports/core/shared-mime-info/shared-mime-info-*.tar.xz 2>/dev/null | head -1)
if grep -q 'MimeType=.*x-zmachine' ports/core/frotz/build.sh; then
    grep -q 'mime/packages/kdos-zmachine.xml' ports/core/frotz/build.sh &&
    grep -q 'update-mime-database' ports/core/frotz/postinstall.sh &&
        echo "  and the z-machine type frotz claims is one frotz installs" ||
        { echo "  frotz claims a MIME type nothing on this image defines"
          exit 1; }
    if [ -n "$_smi" ] && tar -xOf "$_smi" --wildcards '*/freedesktop.org.xml' \
            2>/dev/null | grep -q 'x-zmachine'; then
        echo "  shared-mime-info now defines x-zmachine — drop frotz's copy"
        exit 1
    fi
fi

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

#
# A HOST TERMINAL THAT WENT AWAY, AND THE ONE THING THAT NOTICES.
#
# An `ssh` drop hangs up the pty while the session it was showing carries on.
# Nothing else says so: a write to a hung-up descriptor fails and a read comes
# back empty, and a loop that checked neither paints frames nobody receives
# until the session ends — never reaching the exit that puts the terminal back.
#
# In a process of ITS OWN, and that is not tidiness: the check has to hand
# libktui a broken descriptor as its output, and doing that in the harness
# would take the harness's own stdout with it.
#
echo "==> a view notices when its host terminal hangs up"
cat > "$OUT/hupdrv.c" <<'HUPEOF'
#include <stdio.h>
#include <unistd.h>
#include "ktui.h"

int main(void)
{
	int pipefd[2];

	if (pipe(pipefd) != 0)
		return 2;
	if (dup2(pipefd[1], 1) < 0)
		return 2;

	/* THE READER IS STILL THERE FOR THE SET-UP, which writes a palette and
	 * enters the alternate screen: a terminal that was broken before it was
	 * entered would say nothing about the case this is for, which is a
	 * terminal that goes away while it is being drawn on. */
	ktui_term_init(0);
	if (ktui_term_hungup()) {
		fprintf(stderr, "hung up while the reader was still there\n");
		return 1;
	}

	/* AND NOW IT IS GONE. A write to a pipe with no reader is EPIPE, the
	 * same class of failure a hung-up pty gives — the far end has gone and
	 * no amount of waiting brings it back. SIGPIPE is ignored by
	 * ktui_term_init(), so the write returns rather than killing this. */
	close(pipefd[0]);
	ktui_term_write("hello", 5);
	ktui_term_flush();
	if (!ktui_term_hungup()) {
		fprintf(stderr, "a write to a gone descriptor was not noticed\n");
		return 1;
	}
	/* STICKY. A terminal that has hung up does not come back, and a
	 * consumer may not look until its next turn round the loop. */
	if (!ktui_term_hungup()) {
		fprintf(stderr, "the flag did not stay set\n");
		return 1;
	}
	return 0;
}
HUPEOF
$CC $STD $SHWARN $INC -o "$OUT/hupdrv" "$OUT/hupdrv.c" \
    src/libs/libktui/*.c src/libs/libkbase/*.c src/libs/libkcolor/*.c
if "$OUT/hupdrv" >/dev/null; then
    echo "  a write to a descriptor with nothing behind it is a hangup, and it sticks"
else
    echo "  the hangup was not noticed"
    exit 1
fi

#
# THE TIMER TABLE'S SPLIT, WHICH IS THE ONE THING IN IT THAT CAN GO QUIETLY
# WRONG. A row is split by leaving an expansion unquoted — that is how a line
# becomes an argument vector without a shell — but it also PATHNAME-EXPANDS it,
# and `*` is `snooze`'s own syntax for "every". Unguarded, `-M *` in a
# directory holding two files becomes `-M a b`: a timer that runs at times
# nobody asked for, and nothing says so.
#
# The SHIPPED parser is extracted and run, not copied here: a copy is a copy
# that goes stale, and this file already has that rule about the key card.
#
echo "==> a timer row splits into an argument vector without globbing"
_tw=$(mktemp -d)
mkdir -p "$_tw/d"
touch "$_tw/aaa" "$_tw/bbb"
sed -n '/^timers_each()/,/^}$/p' fs/etc/init.d/18_timers.sh > "$_tw/parse.sh"
cat > "$_tw/d/x.timer" <<'TMREOF'
# a comment, and the blank line under it

daily  -H 3 -M 5 -s 8h  --  true
starry  -H * -M /5  --  true
broken  -H 3  true
TMREOF
( cd "$_tw" && sh -c '. ./parse.sh
show() { printf "%s|%s|" "$1" "$2"; shift 2; printf "%s " "$@"; echo; }
timers_each ./d show' > "$_tw/out" 2>&1 )
grep -q '^daily| -H 3 -M 5 -s 8h|true $' "$_tw/out" \
    || { echo "  a plain row did not split into name, spec and command"
         cat "$_tw/out"; exit 1; }
grep -q '^starry| -H \* -M /5|true $' "$_tw/out" \
    || { echo "  a star spec did not survive the split"; cat "$_tw/out"; exit 1; }
if grep -q 'aaa' "$_tw/out"; then
    echo "  a star spec was expanded against the directory"; exit 1
fi
if grep -q '^broken' "$_tw/out"; then
    echo "  a row with no -- was passed on instead of being skipped"; exit 1
fi
grep -q "no '--' or no command" "$_tw/out" \
    || { echo "  a row with no -- was skipped silently"; exit 1; }
rm -rf "$_tw"
echo "  a star stays a star, and a row with no -- is reported and skipped"

#
# AND A PER-USER TIMER REPEATS, which for a release it did not.
#
# `snooze` waits for its slot, runs the command ONCE and exits. The system
# table repeats because `ksvc supervise` restarts it; a user cannot write a
# pidfile into /run, so the per-user table backgrounded a bare `snooze` and a
# job set for every Monday fired on the Monday somebody happened to log in.
#
# Three things this pins, each of which alone would put it back:
#   - the runner is a LOOP, not a bare background job;
#   - the loop's pid is written where the NEXT login can find it, or a start
#     script that re-execs itself adds a second set on top of the first;
#   - the previous set is stopped CHILD FIRST, because killing the loop alone
#     leaves the `snooze` it is waiting on running and reparented.
#
echo "==> a per-user timer repeats, and the last login's loops are stopped first"
_sc=fs/usr/local/lib/kdos/session-common.sh
_tfail=""
grep -q 'while :; do' "$_sc" || _tfail="$_tfail no-loop"
grep -q 'snooze \$_spec "\$@"' "$_sc" || _tfail="$_tfail no-snooze-in-loop"
grep -q 'echo \$! >> "\$_tpid"' "$_sc" || _tfail="$_tfail no-pidfile"
grep -q 'pkill -P "\$_p"' "$_sc" || _tfail="$_tfail no-child-kill"
# THE ORDER IS THE ASSERTION for the last one: the child is killed before the
# loop, and a file that does them the other way round leaves a snooze behind.
awk '/pkill -P "\$_p"/{p=NR} /^[[:space:]]*kill "\$_p"/{k=NR}
     END{exit !(p && k && p < k)}' "$_sc" \
    || _tfail="$_tfail kill-order"
if [ -z "$_tfail" ]; then
    echo "  the runner loops, records its pid, and stops the child first"
else
    echo "  THE PER-USER TIMER RUNNER IS BACK TO ONE RUN PER LOGIN:$_tfail"
    exit 1
fi

#
# A TERMINAL PROGRAM BECOMES AN APPLICATION, and the marker is what makes `rm`
# safe. A slug is a person's word and the same word can name an entry the image
# shipped, so without `X-KDOS-TUI` this verb would be a way to delete somebody
# else's application — and the file it writes is in the directory that SHADOWS
# /usr/share, so a bare slug could take a shipped entry off the menu instead of
# adding a row beside it.
#
echo "==> kdos app tui writes an entry, lists only its own, and removes only its own"
_tw=$(mktemp -d)
ln -sf kdos-tools "$OUT/kdos"
_tapps="$_tw/.local/share/applications"
# XDG_DATA_HOME AS WELL AS HOME. The writer prefers $XDG_DATA_HOME and a
# developer's session sets it — a test that pinned only HOME would write into
# the real one and pass while doing it.
_tenv="HOME=$_tw XDG_DATA_HOME=$_tw/.local/share"
env $_tenv "$OUT/kdos" app tui add "Disk usage" ncdu --float --size 90x30 \
    > "$_tw/add.out" 2>&1
grep -q '^kdos-tui-disk-usage$' "$_tw/add.out" \
    || { echo "  add answered: $(cat "$_tw/add.out")"; exit 1; }
_te="$_tapps/kdos-tui-disk-usage.desktop"
[ -f "$_te" ] || { echo "  no entry was written"; ls -R "$_tw"; exit 1; }
grep -qx 'Terminal=true' "$_te" || { echo "  the entry is not a terminal one"; exit 1; }
grep -qx 'X-KDOS-TUI=true' "$_te" || { echo "  no marker key"; exit 1; }
grep -qx 'X-KDOS-Float=true' "$_te" || { echo "  --float was not written"; exit 1; }
grep -qx 'X-KDOS-Size=90x30' "$_te" || { echo "  --size was not written"; exit 1; }
# THE EXEC IS QUOTED PER FIELD. A quoted path with a space in it comes back
# quoted, because it goes out through the writer that the reader is the inverse
# of — concatenation would hand the launcher two words.
env $_tenv "$OUT/kdos" app tui add "Odd one" '"/opt/my dir/run" -x' >/dev/null 2>&1
grep -qx 'Exec="/opt/my dir/run" -x' "$_tapps/kdos-tui-odd-one.desktop" \
    || { echo "  a quoted path was not written back quoted:"
         grep '^Exec=' "$_tapps/kdos-tui-odd-one.desktop"; exit 1; }
# AND A PERCENT IS A PERCENT. The command is an argument vector, not an Exec
# line, so `%` is a literal — doubled, because a single one begins a field code
# in the file this is written into.
env $_tenv "$OUT/kdos" app tui add "Percent" 'x 50%' >/dev/null 2>&1
grep -qx 'Exec=x 50%%' "$_tapps/kdos-tui-percent.desktop" \
    || { echo "  a literal percent was not doubled:"
         grep '^Exec=' "$_tapps/kdos-tui-percent.desktop"; exit 1; }
# A REFUSED SHAPE IS REFUSED. --size is cells and a terminal smaller than 4x2
# is not one.
env $_tenv "$OUT/kdos" app tui add "Too small" x --size 2x1 >/dev/null 2>&1 \
    && { echo "  a 2x1 terminal was accepted"; exit 1; }
# ONLY ITS OWN, both ways.
printf '[Desktop Entry]\nType=Application\nName=Foreign\nExec=x\n' \
    > "$_tapps/kdos-tui-foreign.desktop"
env $_tenv "$OUT/kdos" app tui ls 2>/dev/null | grep -q 'Foreign' \
    && { echo "  ls listed an entry it did not write"; exit 1; }
env $_tenv "$OUT/kdos" app tui rm foreign >/dev/null 2>&1 \
    && { echo "  rm deleted an entry it did not write"; exit 1; }
[ -f "$_tapps/kdos-tui-foreign.desktop" ] \
    || { echo "  the foreign entry was removed anyway"; exit 1; }
env $_tenv "$OUT/kdos" app tui rm disk-usage >/dev/null 2>&1 \
    || { echo "  rm refused an entry it did write"; exit 1; }
[ -f "$_te" ] && { echo "  the entry survived its own rm"; exit 1; }
rm -rf "$_tw"
echo "  the marker gates rm, and the Exec is quoted a field at a time"

#
# A REMINDER IS DELIVERED ONCE, and that is the whole of what can go quietly
# wrong here. It is armed twice on purpose — once by `kdos remind` because
# nothing re-reads the per-user table between logins, and again by the next
# login from the file — so "once" is not a property of the schedule. It is a
# property of `fire` removing the file before it returns, and of everything
# else refusing to deliver a reminder that is not there.
#
# Two more things this pins: the text is a COMMENT LINE, because a timer row is
# split into words with no quoting and a sentence cannot survive it; and a
# reminder with nowhere to appear is NOT spent, or a machine that happened to
# have no session at the wrong minute eats it.
#
echo "==> a reminder is written as a timer row and delivered exactly once"
_rw=$(mktemp -d)
mkdir -p "$_rw/bin"
ln -sf kdos-tools "$OUT/kdos"
# `snooze` and `gdbus` are stubbed: what is under test is this program's half.
# The snooze stub records that an arming happened and returns at once — the
# real one would wait for the slot.
printf '#!/bin/sh\nprintf "%%s\\n" "$*" >> "$REMIND_ARMED"\n' \
    > "$_rw/bin/snooze"
printf '#!/bin/sh\nprintf "%%s\\n" "$*" >> "$REMIND_TOAST"\n' \
    > "$_rw/bin/gdbus"
chmod +x "$_rw/bin/snooze" "$_rw/bin/gdbus"
: > "$_rw/armed"
: > "$_rw/toast"
_rt="$_rw/home/.config/kdos/timers.d"
_renv="HOME=$_rw/home XDG_CONFIG_HOME=$_rw/home/.config REMIND_ARMED=$_rw/armed REMIND_TOAST=$_rw/toast PATH=$_rw/bin:$PATH"
# shellcheck disable=SC2086
env $_renv "$OUT/kdos" remind in 90m tea and biscuits > "$_rw/set.out" 2>&1
grep -q 'tea and biscuits at ' "$_rw/set.out" \
    || { echo "  setting a reminder said: $(cat "$_rw/set.out")"; exit 1; }
_rf=$(ls "$_rt" 2>/dev/null | head -1)
[ -n "$_rf" ] || { echo "  no timer row was written"; exit 1; }
case "$_rf" in
*.timer) ;;
*) echo "  the row is $_rf, which neither timer table globs"; exit 1 ;;
esac
# THE TEXT IS THE COMMENT AND THE ROW IS THE JOB.
head -1 "$_rt/$_rf" | grep -q '^# tea and biscuits$' \
    || { echo "  the text is not the first comment line"; cat "$_rt/$_rf"; exit 1; }
grep -q -- '-t .*\.timer' "$_rt/$_rf" \
    || { echo "  no -t timefile, so a missed slot would never fire"; exit 1; }
grep -q -- '-- kdos remind fire ' "$_rt/$_rf" \
    || { echo "  the row does not run the once-only deliverer"; exit 1; }
# AND IT WAS ARMED NOW, not left for the next login.
grep -q 'remind fire' "$_rw/armed" \
    || { echo "  nothing was armed, so it would wait for a re-login"; exit 1; }

_rid=$(printf '%s' "$_rf" | sed 's/^remind-//; s/\.timer$//')
# NOWHERE TO APPEAR IS NOT DELIVERED.
# shellcheck disable=SC2086
( unset DBUS_SESSION_BUS_ADDRESS; env $_renv "$OUT/kdos" remind fire "$_rid" ) >/dev/null 2>&1
[ -e "$_rt/$_rf" ] \
    || { echo "  a reminder with no session to show it in was consumed"; exit 1; }
[ -s "$_rw/toast" ] \
    && { echo "  something was raised with no session"; exit 1; }
# WITH A SESSION: delivered, and the row is gone. The bus address is what says
# there is one — a reminder is a `Notify` and nothing else can carry it.
# shellcheck disable=SC2086
env $_renv DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent-kdos-bus \
    "$OUT/kdos" remind fire "$_rid" >/dev/null 2>&1
grep -q 'tea and biscuits' "$_rw/toast" \
    || { echo "  the reminder was not raised"; cat "$_rw/toast"; exit 1; }
[ -e "$_rt/$_rf" ] \
    && { echo "  the row survived delivery, so it would fire again"; exit 1; }
# AND THE SECOND ARMING FINDS NOTHING.
: > "$_rw/toast"
# shellcheck disable=SC2086
env $_renv DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent-kdos-bus \
    "$OUT/kdos" remind fire "$_rid" >/dev/null 2>&1
[ -s "$_rw/toast" ] \
    && { echo "  a delivered reminder was delivered again"; exit 1; }
# A TIME THAT HAS GONE ROLLS FORWARD, and a shape nobody wrote is refused.
# shellcheck disable=SC2086
env $_renv "$OUT/kdos" remind in 5x nope >/dev/null 2>&1 \
    && { echo "  a unit that does not exist was accepted"; exit 1; }
# shellcheck disable=SC2086
env $_renv "$OUT/kdos" remind at 99:00 nope >/dev/null 2>&1 \
    && { echo "  an hour that does not exist was accepted"; exit 1; }
rm -rf "$_rw"
echo "  the text is the comment, the row carries -t, and it fires once"

#
# THE CODE WORD OUT OF WHAT croc PRINTS, which is the one thing in `kdos share`
# that can go quietly wrong: croc writes the block to STDERR beside its own
# progress, so a parser that took the wrong line would hand somebody a URL to a
# relay this image cannot reach and a QR of it. Both programs are stubbed —
# what is under test is this program's half.
#
echo "==> kdos-share reads the code out of what croc prints"
ln -sf kdos-tools "$OUT/kdos-share"
mkdir -p "$OUT/sharebin"
cat > "$OUT/sharebin/croc" <<'CROCEOF'
#!/bin/sh
# `--ignore-stdin` IS GLOBAL AND COMES FIRST. The real croc reads stdin for a
# piped payload, so `croc send` with it anywhere else prints neither a code nor
# an error and blocks — which is the failure this line refuses on its behalf.
[ "$1" = "--ignore-stdin" ] || { echo "argv: $*" >&2; exit 2; }
[ "$2" = "send" ] || { echo "argv: $*" >&2; exit 2; }
# THE TWO LINES CARRY DIFFERENT WORDS ON PURPOSE. The real croc puts the same
# code in both, which makes them indistinguishable to a test — and the line
# that must be read is the one a person types, because the other names a public
# relay this image's `--local` croc cannot reach.
cat >&2 <<'CROCOUT'
On the other computer, run:
  croc test-code-here

Or open:
  https://getcroc.com/?code=relay-url-not-this
CROCOUT
printf 'Sending 0 files\rSending %s\n' "$3" >&2
CROCEOF
cat > "$OUT/sharebin/qrencode" <<'QRENCEOF'
#!/bin/sh
# The payload arrives on STDIN and never in argv: the code is the transfer's
# whole secret and /proc/<pid>/cmdline is world-readable. Recording it here is
# how the test sees which line was parsed.
cat > "$SHARE_QR_SEEN"
echo '##'
QRENCEOF
chmod +x "$OUT/sharebin/croc" "$OUT/sharebin/qrencode"
echo hi > "$OUT/sharefile.txt"
SHARE_QR_SEEN="$OUT/qr-payload" PATH="$OUT/sharebin:$PATH" \
    "$OUT/kdos-share" --here "$OUT/sharefile.txt" \
    > "$OUT/share.out" 2> "$OUT/share.err" < /dev/null
grep -q "On the other computer" "$OUT/share.err" \
    || { echo "  croc's own block was not relayed"; exit 1; }
grep -q "sharefile.txt" "$OUT/share.err" \
    || { echo "  the file never reached croc's argv"; exit 1; }
[ -s "$OUT/share.out" ] \
    && { echo "  something was written to stdout, which croc leaves empty"; exit 1; }
[ "$(cat "$OUT/qr-payload")" = "test-code-here" ] \
    || { echo "  the QR was drawn for '$(cat "$OUT/qr-payload")'"; exit 1; }
echo "  the code word, not the relay URL, and it reaches qrencode on stdin"

# And the guard on the flag order, which is the failure with no output at all:
# a stub that sees the wrong argv exits 2 and prints no code, so nothing is
# parsed and nothing is drawn.
rm -f "$OUT/qr-payload"
cat > "$OUT/sharebin/croc" <<'CROCBAD'
#!/bin/sh
echo "argv: $*" >&2
CROCBAD
chmod +x "$OUT/sharebin/croc"
SHARE_QR_SEEN="$OUT/qr-payload" PATH="$OUT/sharebin:$PATH" \
    "$OUT/kdos-share" --here "$OUT/sharefile.txt" >/dev/null 2>&1 </dev/null
[ -e "$OUT/qr-payload" ] \
    && { echo "  a QR was drawn for output carrying no code"; exit 1; }
echo "  and output with no code in it draws nothing"

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

# EACH SLOT'S OWN LUKS CONTAINER, which is what joins A/B to encryption. The
# command line names ONE `cryptdevice=`, so a second slot inside a second
# container is reachable only because `crypt` answers per slot — and the case
# that matters is the ROLLBACK, where the filesystem changes under the
# initramfs and the container has to change with it.
rm -f "$AB/bootstate"
bootctl set-slot a AAAA-1111 LUKS-AAAA >/dev/null
bootctl set-slot b BBBB-2222 LUKS-BBBB >/dev/null
test "$(bootctl crypt AAAA-1111)" = "LUKS-AAAA" \
    || { echo "  a slot's container did not come back"; exit 1; }
# Keyed by the FILESYSTEM and not by a slot name: that is what makes it one
# call after `select`, with no second decision that could disagree.
bootctl try b >/dev/null
_sel="$(bootctl select 2>/dev/null)"
test "$_sel" = "BBBB-2222" || { echo "  no candidate"; exit 1; }
test "$(bootctl crypt "$_sel")" = "LUKS-BBBB" \
    || { echo "  the candidate slot got the WRONG container"; exit 1; }
# Exhaust it: the rollback must carry A's container back with A's filesystem,
# because unlocking B's and looking for A's filesystem inside it reads as a
# corrupt disk rather than as a lookup that was never made. THREE attempts is
# the default and one has been spent above, so two more reach the rollback —
# the count is the loop's own a few lines up, not a new one.
bootctl select >/dev/null 2>&1
bootctl select >/dev/null 2>&1
_sel="$(bootctl select 2>/dev/null)"
test "$_sel" = "AAAA-1111" || { echo "  no rollback"; exit 1; }
test "$(bootctl crypt "$_sel")" = "LUKS-AAAA" \
    || { echo "  the rollback kept the OTHER slot's container"; exit 1; }
# A slot described with no container is a slot that is not encrypted, and the
# value is REWRITTEN rather than kept: an updater that puts a plain filesystem
# over an encrypted slot must not leave the initramfs unlocking a container
# that is no longer in the way.
bootctl set-slot a AAAA-1111 >/dev/null
bootctl crypt AAAA-1111 >/dev/null 2>&1 \
    && { echo "  a container survived a slot being rewritten without one"; exit 1; }
echo "  each slot carries its own LUKS container, across a rollback"

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

# THE STUB ANSWERS `-U <uuid>` AND NOTHING ELSE, which is the whole of what
# unlock_root asks of it. A stub that echoed a device whatever it was handed
# could not tell a blkid that implements the UUID LOOKUP from one that does
# not — and toybox's applet does not, which is exactly the defect that hid
# behind the permissive version.
cat > "$IR/bin/blkid" <<'EOF'
#!/bin/sh
[ "$1" = "-U" ] && [ -n "${2:-}" ] || exit 1
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

# AND THE blkid THE INITRAMFS SHIPS IS UTIL-LINUX'S, NOT THE NAME TOYBOX
# CLAIMS. Every lookup in the generated init is `blkid -U` — the root
# filesystem, the ESP holding the A/B state, and the LUKS container — and
# toybox's applet implements no `-U` and cannot see `crypto_LUKS`. With the
# applet, an installed machine drops to a shell with "Root device not found".
# $PATH puts /usr/bin ahead of /usr/sbin, so the name alone decides it.
grep -q 'cp /usr/sbin/blkid bin/blkid' script/06_packaging/01_initramfs.sh \
    || { echo "  the initramfs no longer copies util-linux's blkid"; exit 1; }
grep -q "rm -f bin/blkid" script/06_packaging/01_initramfs.sh \
    || { echo "  the toybox blkid symlink is not removed first — cp writes THROUGH it"; exit 1; }
grep -q "CONFIG_BLKID is not set" ports/core/toybox/build.sh \
    || { echo "  toybox's blkid applet is back, and it shadows util-linux's on PATH"; exit 1; }
echo "  the initramfs blkid is util-linux's, and toybox claims no such name"

# AND THE SLOT'S OWN CONTAINER, WHICH IS ORDERING AND NOT LOGIC. `select` and
# `crypt` both read the state file on the ESP, and the ESP is unmounted a few
# lines later — so a `crypt` call that drifted below the umount reads nothing,
# silently drops the container, and an encrypted second slot is then unlocked
# with the FIRST slot's container. That fails as a corrupt filesystem, which
# is the worst way for a lookup that was never made to present.
#
# Asserted on the GENERATED init, by line number, because the bug is entirely
# a question of which line comes first.
_ln() { grep -n "$1" "$IR/init" | head -1 | cut -d: -f1; }
_sel=$(_ln 'kdos-bootctl select')
_cry=$(_ln 'kdos-bootctl crypt')
_umt=$(_ln 'umount /esp')
if [ -z "$_sel" ] || [ -z "$_cry" ] || [ -z "$_umt" ]; then
    echo "  the init lost its slot block (select=$_sel crypt=$_cry umount=$_umt)"
    exit 1
fi
[ "$_sel" -lt "$_cry" ] \
    || { echo "  the container is read before the slot is chosen"; exit 1; }
[ "$_cry" -lt "$_umt" ] \
    || { echo "  the container is read AFTER the ESP is unmounted"; exit 1; }
# And the answer has to reach the unlock, which is the other half: a lookup
# whose result nothing assigns is a lookup that changes nothing.
grep -q 'CRYPTDEV="UUID=\$SLOT_CRYPT' "$IR/init" \
    || { echo "  the slot's container never reaches CRYPTDEV"; exit 1; }
# The unlock must still come after the whole slot block, or it unlocks
# whatever the command line named and then looks for the other slot inside it.
_unl=$(_ln 'if \[ -n "\$CRYPTDEV" \]')
[ -n "$_unl" ] && [ "$_cry" -lt "$_unl" ] \
    || { echo "  the unlock does not follow the slot selection"; exit 1; }
echo "  a slot's LUKS container is read while the ESP is mounted, before the unlock"

echo
echo "==> the shipped console palette is the default scheme"
# fs/etc/vtrgb IS A GENERATED FILE THAT IS COMMITTED, the way the boot backdrop
# is: kdos-getty loads it onto every VT before the login prompt, and nothing at
# run time derives it. So the tree carries a second copy of KCOL_DEFAULT_ID's
# sixteen colours, and a second copy nothing compares is the copy that goes
# stale — moving the default accent and leaving this behind is a machine whose
# console and desktop are different colours from first boot.
"$OUT/kdos-bootctl" palette > "$OUT/vtrgb.want" \
    || { echo "  kdos-bootctl palette failed"; exit 1; }
if ! diff -u fs/etc/vtrgb "$OUT/vtrgb.want" > "$OUT/vtrgb.diff" 2>&1; then
    echo "  fs/etc/vtrgb is not the default scheme's palette:"
    sed 's/^/    /' "$OUT/vtrgb.diff"
    echo "    regenerate with: kdos-bootctl palette > fs/etc/vtrgb"
    exit 1
fi
# AND THE ACCENT HAS TO REACH IT. Every scheme must give a different table, or
# `kdos theme` writes the same sixteen colours whatever it is handed.
_pal_prev=
for _sc in phosphor amber ice bone norton borland perfect paper; do
    _pal=$("$OUT/kdos-bootctl" palette "$_sc") \
        || { echo "  no palette for $_sc"; exit 1; }
    [ "$_pal" != "$_pal_prev" ] \
        || { echo "  $_sc has the same console palette as the scheme before it"
             exit 1; }
    _pal_prev=$_pal
done
echo "  fs/etc/vtrgb matches kcol_vtrgb(default), and every accent differs"

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

# ── the Applications page reads the CATALOGUE ───────────────────────────────
# kinstall links libkbase, libktui and libkcolor and nothing else, which is what
# lets it live in phase 1 — so catalogue.c is compiled IN rather than shelled
# out to, and this asserts the four answers that decide what an install carries:
# `essential` is preselected, an answer file's choice wins, an UNKNOWN id falls
# back rather than failing after the point of no return, and the page says which
# ROUTE the applications will take.
mkdir -p "$OUT/cat"
cat > "$OUT/cat/catalogue" <<'CAT'
snapshot = 20260824T000000Z
base    base    -      bash curl
runtime rt-gtk  base   libgtk-3-0t64
runtime rt-qt   base   libqt6widgets6
app     app.gimp   rt-gtk  gimp
app     app.krita  rt-qt   krita
app     app.meld   rt-gtk  meld

group essential  What a new machine starts with
group essential  app.gimp
group creative   Images and painting
group creative   app.gimp app.krita
group dev        Programming
group dev        app.meld

meta app.gimp   GIMP|Graphics|96468992|Create images and edit photographs
meta app.krita  Krita|Graphics|188000000|Digital painting
meta app.meld   Meld|Development|24000000|Compare and merge files
CAT
kiapp() { KDOS_CATALOGUE="$OUT/cat/catalogue" "$KI" --config "$1" --dump plan 2>&1; }
kiappj() { KDOS_CATALOGUE="$OUT/cat/catalogue" "$KI" --config "$1" --dump plan --json 2>/dev/null; }
: > "$OUT/none.conf"
kiapp "$OUT/none.conf" | grep -qE '^apps +essential$' \
    || { echo "  essential was not preselected"; exit 1; }
printf 'apps = creative\n' > "$OUT/a1.conf"
kiapp "$OUT/a1.conf" | grep -qE '^apps +creative$' \
    || { echo "  an answer file's choice was ignored"; exit 1; }
printf 'apps = nosuchgroup\n' > "$OUT/a2.conf"
kiapp "$OUT/a2.conf" | grep -qE '^apps +essential$' \
    || { echo "  an unknown id did not fall back to essential"; exit 1; }
# AND IT IS IDEMPOTENT — an answer file that does not name essential must not
# leave essential ticked from the preselect that ran before it.
kiapp "$OUT/a1.conf" | grep -qE '^apps +essential' \
    && { echo "  a group survived an answer file that did not name it"; exit 1; }
# THE ROUTE IS ON THE PAGE. A person must not discover at first boot that
# nothing was installed, so `plan` names which of the four it will take — and
# the two renderings must agree about it, which is what makes a dump worth
# pasting into a bug report.
kiapp "$OUT/none.conf" | grep -qE '^apps route +' \
    || { echo "  the plan does not say how the applications arrive"; exit 1; }
kiappj "$OUT/none.conf" | python3 -c 'import json,sys
d = json.load(sys.stdin)
ids = [g["id"] for g in d["apps"]]
assert ids == ["essential"], ids
assert d["apps_bytes"] == 96468992, d["apps_bytes"]
assert d["apps_route"] in ("none", "import", "network", "pending"), d["apps_route"]' \
    || { echo "  the json plan disagrees with the text one"; exit 1; }
# A GROUP'S ESTIMATE COUNTS EACH MEMBER ONCE AND NO RUNTIME. The shared layers
# under two GTK applications are stored once, so summing per-application
# figures already over-counts; adding rt-gtk would over-count again.
kiappj "$OUT/a1.conf" | python3 -c 'import json,sys
d = json.load(sys.stdin)
assert d["apps_bytes"] == 96468992 + 188000000, d["apps_bytes"]' \
    || { echo "  a group's size estimate is not the sum of its members"; exit 1; }
echo "  the catalogue: essential preselected, unknown falls back, route named"

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
echo "==> the polkit rules grant what the surfaces call and nothing else"
#
# A .rules file is JavaScript that duktape runs inside polkitd. Nothing
# compiles it, so a syntax error or a wrong action id is networking that
# quietly does not work — and this desktop has NO authentication agent by
# decision (docs/kdos/03-architecture/security-model.md), which means a rule
# that fails to grant is a control that can never succeed rather than one that
# asks for a password.
#
# THE SHIPPED FILE IS WHAT IS EVALUATED, concatenated between a stub of
# polkit's own object and the assertions — not a copy, which would agree with
# the rules only until somebody edited them.
if command -v duk >/dev/null 2>&1; then
    cat testing/fixtures/polkit/stub.js \
        fs/etc/polkit-1/rules.d/50-kdos.rules \
        testing/fixtures/polkit/assert.js > "$OUT/rulescheck.js"
    duk "$OUT/rulescheck.js" > "$OUT/rulescheck.txt" 2>&1
    cat "$OUT/rulescheck.txt"
    grep -q "FAIL" "$OUT/rulescheck.txt" && exit 1
else
    echo "  the polkit rules are skipped (no duk on this host)"
fi

echo
echo "==> kdos-netagent answers NetworkManager the way libnm expects"
#
# NetworkManager never prompts, so an agent that gets any part of this contract
# wrong is a passphrase box that never appears or an activation abandoned
# instead of retried — and neither shows in a photograph. What is asserted here
# is the WIRE: the flag that must be set before anybody is asked, the exact
# `a{sa{sv}}` a secret comes back in, and the error names.
#
# THE NAMES HAVE NO `.Error.` IN THEM. libnm builds every D-Bus error name as
# the interface, a dot and the enum nick, so a cancel is
# `...SecretAgent.UserCanceled`. Most agent examples on the web spell it with
# `.Error.`, which NetworkManager reports as an unclassified failure.
#
# nmstub.c is the other end: a bus name, an AgentManager and one GetSecrets
# built from the argument order libnm sends. agentcheck.c links the REAL
# netagent.c against a scripted display, so the keystrokes are the test's and
# everything else is the shipped code.
if [ -n "$TRAY_SDBUS" ] && command -v dbus-daemon >/dev/null 2>&1; then
    NAO="$OUT/netagent"
    GOLDNA="$PWD/testing/goldens"
    mkdir -p "$NAO"
    $CC $STD $WARN -o "$NAO/agentcheck" \
        -Isrc/desktop/kdos-shell -Isrc/libs/libktui -Isrc/libs/libkcolor \
        -Isrc/libs/libkbase -Isrc/libs/libkdisp -Isrc/libs/libkwl \
        -Isrc/libs/libkcell -Isrc/libs/libkchrome -Isrc/libs/libkxdg \
        -Isrc/libs/libkicon -Isrc/libs/libkproc \
        -Isrc/libs/libkwm \
        testing/fixtures/netagent/agentcheck.c \
        src/desktop/kdos-shell/netagent.c \
        src/libs/libktui/*.c src/libs/libkcolor/*.c src/libs/libkbase/*.c \
        $(pkg-config --cflags --libs "$TRAY_SDBUS") \
        || { echo "  FAIL  the agent fixture does not build"; exit 1; }
    $CC $STD $WARN -o "$NAO/nmstub" testing/fixtures/netagent/nmstub.c \
        $(pkg-config --cflags --libs "$TRAY_SDBUS") \
        || { echo "  FAIL  the NetworkManager stub does not build"; exit 1; }
    # A bus of its own. The agent registers on the SYSTEM bus and the host's
    # own NetworkManager must never see this: an agent registered against it
    # would be asked for the passphrases of the machine running the tests.
    cat > "$NAO/bus.conf" <<'NABUS'
<!DOCTYPE busconfig PUBLIC "-//freedesktop//DTD D-BUS Bus Configuration 1.0//EN"
 "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
<busconfig>
  <type>system</type>
  <listen>unix:tmpdir=/tmp</listen>
  <policy context="default">
    <allow send_destination="*" eavesdrop="true"/>
    <allow eavesdrop="true"/>
    <allow own="*"/>
  </policy>
</busconfig>
NABUS
    na_case() {
        _n="$1"; shift
        dbus-daemon --config-file="$NAO/bus.conf" --print-address=3 --fork \
            --print-pid=4 3>"$NAO/addr" 4>"$NAO/pid"
        DBUS_SYSTEM_BUS_ADDRESS="$(cat "$NAO/addr")"
        export DBUS_SYSTEM_BUS_ADDRESS
        "$NAO/nmstub" "$@" > "$NAO/$_n.txt" 2>&1 &
        _sp=$!
        sleep 0.4
        timeout 25 "$NAO/agentcheck" > "$NAO/$_n.agent" 2>&1 &
        _ap=$!
        wait "$_sp" 2>/dev/null || true
        kill "$_ap" 2>/dev/null || true
        wait "$_ap" 2>/dev/null || true
        kill "$(cat "$NAO/pid")" 2>/dev/null || true
        unset DBUS_SYSTEM_BUS_ADDRESS
    }
    na_want() {   # case, expected line
        grep -qxF "$2" "$NAO/$1.txt" && return 0
        echo "  FAIL  $1: expected"
        echo "          $2"
        echo "        got"
        sed 's/^/          /' "$NAO/$1.txt"
        na_fail=1
    }
    na_fail=0

    # Registered at all, and with an identifier NetworkManager accepts:
    # three to 255 characters of alphanumerics, `_`, `-` and `.`.
    na_case a 0x0 802-11-wireless-security psk "Home Wifi"
    na_want a "REGISTER org.kdos.netagent"
    # NetworkManager polls its agents for STORED secrets on paths nobody is
    # watching. Without ALLOW_INTERACTION this answers at once and raises no
    # window: a prompt from one of those is a dialog with no question behind
    # it, and it would block the poll for two minutes.
    na_want a "ERROR org.freedesktop.NetworkManager.SecretAgent.NoSecrets this agent stores nothing"

    # ANSI-C quoting, not a command substitution: `$(printf 'x\n')` strips the
    # trailing newline, and the newline IS the Enter that answers the box.
    KDOS_NETAGENT_KEYS=$'hunter2\n' \
        na_case b 0x1 802-11-wireless-security psk "Home Wifi"
    # ONE setting, the one that was asked about, and the hint's key inside it.
    # A reply that echoed the whole profile would overwrite properties the
    # agent never looked at.
    na_want b "SECRET 802-11-wireless-security.psk=hunter2"

    KDOS_NETAGENT_KEYS=$'\033' \
        na_case c 0x1 802-11-wireless-security psk "Home Wifi"
    na_want c "ERROR org.freedesktop.NetworkManager.SecretAgent.UserCanceled cancelled"

    # A session with no compositor. NoSecrets, not a hang: the activation
    # fails now instead of after NetworkManager's two-minute wait.
    KDOS_NETAGENT_NOWIN=1 na_case d 0x1 802-11-wireless-security psk "Home Wifi"
    na_want d "ERROR org.freedesktop.NetworkManager.SecretAgent.NoSecrets no compositor"

    # CancelGetSecrets while the box is up. The prompt has to come down: a
    # passphrase field left on screen is one collecting an answer nothing is
    # waiting for.
    na_case e 0x1 802-11-wireless-security psk "Home Wifi" 800
    na_want e "ERROR org.freedesktop.NetworkManager.SecretAgent.AgentCanceled NetworkManager withdrew the question"

    # A VPN secret is the exception in SHAPE: the `vpn` setting keeps its
    # secrets in one `a{ss}` under the key `secrets`, and libnm's need_secrets
    # names no key at all for one, so there is no hint to go on.
    KDOS_NETAGENT_KEYS=$'s3cret\n' na_case f 0x5 vpn "" "Work VPN"
    na_want f "SECRET vpn.secrets.password=s3cret"

    # A SETTING THAT NAMED NOTHING IS NOT GUESSED AT. libnm hints every
    # wireless and 802.1X secret and hints nothing for a VPN, so a setting
    # outside those three with no hint is a request this agent cannot answer —
    # and a passphrase written into the wrong property is a join that fails as
    # "wrong password" for as long as the profile exists.
    KDOS_NETAGENT_KEYS=$'x\n' na_case g 0x1 some-other-setting "" "Odd One"
    na_want g "ERROR org.freedesktop.NetworkManager.SecretAgent.NoSecrets nothing usable was named"

    # THE BOX ITSELF. Nothing else looks at the layout, and a label over the
    # border or a button bar off the right edge is invisible both to the
    # compiler and to a test that only reads the bus.
    na_golden() {   # <case> <name>
        _ng="$GOLDNA/$2.txt"
        if [ "${KDOS_GOLDEN_UPDATE:-0}" = 1 ]; then
            cp "$NAO/$1.agent" "$_ng"
            echo "  wrote $2"
        elif [ ! -f "$_ng" ]; then
            echo "  FAIL  $2: no golden committed"
            na_fail=1
        elif diff -u "$_ng" "$NAO/$1.agent" > "$NAO/$1.diff"; then
            echo "  $2"
        else
            echo "  $2 DRIFTED:"
            head -30 "$NAO/$1.diff" | sed 's/^/    /'
            na_fail=1
        fi
    }
    KDOS_NETAGENT_DUMP=1 na_case h 0x1 802-11-wireless-security psk "Home Wifi" 900
    na_golden h netagent-ask-62x8
    # REQUEST_NEW, which is the agent's ORDINARY case rather than an unusual
    # one: net.c writes the psk into the profile, so a first join completes
    # without an agent and it is the supplicant's mismatch retry that asks.
    KDOS_NETAGENT_DUMP=1 na_case i 0x3 802-11-wireless-security psk "Home Wifi" 900
    na_golden i netagent-retry-62x8

    [ "$na_fail" = 0 ] || exit 1
    echo "  the agent registers, gates on the flag, and answers in libnm's own shapes"
else
    echo "  kdos-netagent is skipped (no sd-bus or no dbus-daemon on this host)"
fi

echo
echo "==> the scrollbar you can see is the scrollbar you can grab"
# ASSERTED AGAINST EACH OTHER, not against numbers: the frame is rendered
# offscreen and the thumb is found in the CELLS, so the draw and the hit test
# are compared rather than both compared to somebody's arithmetic. Its own
# binary because selftest.c links no libkchrome.
$CC $STD $WARN $INC -Isrc/libs/libkchrome -Isrc/libs/libkicon \
    -Isrc/libs/libkcell -Isrc/libs/libkwl -o "$OUT/barcheck" \
    testing/barcheck.c src/libs/libkchrome/kch_chrome.c \
    src/libs/libktui/*.c src/libs/libkcolor/*.c src/libs/libkbase/*.c
"$OUT/barcheck"

echo
echo "==> a launcher names a box, and the menu asks whether it is there"
# THE ASYMMETRY IS WHAT THIS PROTECTS. `sh_box_missing` answers 1 only where
# absence is PROVED and every unknown counts as present, because hiding an
# application somebody installed is a worse failure than showing one whose pack
# has gone. That reads like an incomplete check and is not one.
#
# ITS OWN BINARY, for the reason decocheck has one: these two functions live in
# kdos-shell, and linking the shell into selftest.c to reach them would drag a
# Wayland client and a font renderer in behind them. Four stubs — the launch
# path, which none of this calls — are the whole cost of not doing that.
rm -rf "$OUT/boxfix"
mkdir -p "$OUT/boxfix/store" "$OUT/boxfix/home/.config/kdos/boxes"
: > "$OUT/boxfix/store/app.here.kpack"
: > "$OUT/boxfix/home/.config/kdos/boxes/app.built.conf"
$CC $STD $WARN -Isrc/desktop/kdos-shell $INC -Isrc/libs/libkchrome \
    -Isrc/libs/libkicon -Isrc/libs/libkcell -Isrc/libs/libkwl \
    -Isrc/libs/libkimg -o "$OUT/boxcheck" testing/boxcheck.c \
    src/desktop/kdos-shell/apps.c src/libs/libkbase/*.c src/libs/libkxdg/*.c
BOXCHECK_STORE="$OUT/boxfix/store" BOXCHECK_HOME="$OUT/boxfix/home" \
    "$OUT/boxcheck"

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
# ── THE TIMEZONE VERB, AND WHAT IT REFUSES ───────────────────────────────
#
# A zone name is `Area/City`, so a SLASH IS LEGAL — which makes
# `../../etc/shadow` legal-looking, and the character rule is what stops it.
#
# Driven through `--set-timezone` rather than over the socket, for the same
# reason `--explain` exists: the gate is SO_PEERCRED on a connection and cannot
# be exercised without two uids, so the verb's own rules would otherwise be
# asserted by nothing. The flag grants nothing — it is the binary writing to an
# /etc the caller could already write to, and here that /etc is a fixture.
TZW="$OUT/tzwork"
rm -rf "$TZW"
mkdir -p "$TZW/zi/Europe" "$TZW/etc/profile.d"
: > "$TZW/zi/Europe/London"
tzset_() {
    KDOS_POWERD_ZONEDIR="$TZW/zi" KDOS_POWERD_ETC="$TZW/etc" \
        "$OUT/kdos-powerd" --set-timezone "$1" 2>&1 || true
}
tzwant() {  # <zone> <expected substring> <what it proves>
    _got=$(tzset_ "$1")
    case "$_got" in
    *"$2"*) echo "  ok    $3" ;;
    *) echo "  FAIL  $3"; echo "        sent: $1"; echo "        got:  $_got"
       tz_fail=1 ;;
    esac
}
tz_fail=0
tzwant "Europe/London" "ok" "a real zone is taken"
[ -L "$TZW/etc/localtime" ] \
    && echo "  ok    and /etc/localtime is a symlink into the zone tree" \
    || { echo "  FAIL  /etc/localtime was not written"; tz_fail=1; }
grep -q "TZ=':/etc/localtime'" "$TZW/etc/profile.d/20-timezone.sh" \
    && echo "  ok    and TZ names the same file rather than a rules string" \
    || { echo "  FAIL  the profile does not point TZ at /etc/localtime"
         tz_fail=1; }
tzwant "../../etc/shadow" "not a zone name" \
    "a traversal is refused by the character rule, before any stat"
tzwant "Europe/../../../etc/shadow" "not a zone name" \
    "and so is one hidden after a legal-looking area"
tzwant "/etc/shadow" "not a zone name" "an absolute path is not a zone"
tzwant "Mars/Olympus" "no such zone" \
    "a well-formed name that is not a zone is refused by the tree"
# The symlink must still point where the first call put it: a refused verb that
# had already unlinked it would leave the machine on UTC silently.
readlink "$TZW/etc/localtime" | grep -q "Europe/London" \
    && echo "  ok    and a refused verb left the working zone alone" \
    || { echo "  FAIL  a refusal disturbed /etc/localtime"; tz_fail=1; }
[ "$tz_fail" = 0 ] || exit 1

# ── THE AUTOLOGIN VERB ────────────────────────────────────────────────────
#
# OFF IS A COMMENTED KEY AND NOT AN EMPTY ONE. `kdos-login` asks for a password
# when it finds no key, and `autologin =` with nothing after it would be a key
# naming an account called "", which agetty would be handed.
ALW="$OUT/alwork"
rm -rf "$ALW"
mkdir -p "$ALW/kdos"
cp fs/etc/kdos/login.conf "$ALW/kdos/login.conf"
alset() {
    KDOS_POWERD_ETC="$ALW" "$OUT/kdos-powerd" --set-autologin "$1" 2>&1 || true
}
al_fail=0
alwant() {  # <arg> <expected substring> <what it proves>
    _got=$(alset "$1")
    case "$_got" in
    *"$2"*) echo "  ok    $3" ;;
    *) echo "  FAIL  $3"; echo "        got: $_got"; al_fail=1 ;;
    esac
}
alwant "definitely-not-an-account" "no such account" \
    "an account that cannot log in is refused"
# An account `kb_users()` WOULD list, chosen the way it chooses — uid in
# [1000, 65534) with a shell that is not a refusal. This picks the daemon's
# INPUT, it does not re-decide the rule: if the two ever disagreed the daemon
# would refuse and the assertion below would say so. The suite runs as root in
# the build container, and root is not an account this lists, so there is
# frequently no eligible name at all — skipped loudly rather than passed.
_me=$(awk -F: '$3>=1000 && $3<65534 && $7 !~ /nologin|\/false/ {print $1; exit}' \
      /etc/passwd)
if [ -n "$_me" ]; then
    alwant "$_me" "ok" "a real account is taken"
    grep -q "^autologin = $_me" "$ALW/kdos/login.conf" \
        && echo "  ok    and the key names it" \
        || { echo "  FAIL  the key does not name the account"
             grep -E "^#?autologin" "$ALW/kdos/login.conf"; al_fail=1; }
    alwant "off" "ok" "and it can be turned off again"
    grep -q "^#autologin = " "$ALW/kdos/login.conf" \
        && ! grep -q "^autologin = " "$ALW/kdos/login.conf" \
        && echo "  ok    which COMMENTS the key rather than emptying it" \
        || { echo "  FAIL  off left a key kdos-login would still read"
             grep -E "^#?autologin" "$ALW/kdos/login.conf"; al_fail=1; }
else
    echo "  the accept half is skipped (no account this host would offer)"
fi
# EVERY comment line in the shipped file, counted rather than named: a rewrite
# that dropped them would leave a config nobody could read. Turning autologin
# OFF adds one, which is the commented key itself, so the count is compared
# with that one allowed — a rewrite that lost prose still fails.
_alc=$(grep -c '^#' "$ALW/kdos/login.conf")
_alw=$(grep -c '^#' fs/etc/kdos/login.conf)
[ "$_alc" = "$((_alw + 1))" ] || [ "$_alc" = "$_alw" ] \
    && echo "  ok    and every comment in login.conf survived the rewrite" \
    || { echo "  FAIL  the rewrite lost comments ($_alc against $_alw)"
         al_fail=1; }
[ "$al_fail" = 0 ] || exit 1

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
rm -f "$OUT/km.uevent"; mkfifo "$OUT/km.uevent"
mkdir -p "$OUT/media"
KDOS_MOUNTD_SOCKET="$KMSOCK" KDOS_MOUNTD_MOUNTS="$MF/mounts-live" \
KDOS_MOUNTD_CONF="$OUT/mountd.conf" KDOS_MOUNTD_UEVENT="$OUT/km.uevent" \
KDOS_MOUNTD_MEDIA="$OUT/media" \
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

# SMART IS THE DISK'S, NOT THE PARTITION'S. `smartctl` is pointed at the whole
# drive because SMART is a property of the drive: a verb that ran it per
# partition would print the same answer once per row and, worse, would point
# a raw-device tool at an offset nothing owns.
kmwant 'smart 0
' 'ok' "smart answers for a row"
grep -q 'exec /usr/sbin/smartctl -H -i -- .*/sdb$' "$OUT/km.exec" \
    && echo "  ok    and it is aimed at the DISK sdb, not the partition sdb1" \
    || { echo "  FAIL  smart was not aimed at the whole disk"
         grep smartctl "$OUT/km.exec"; mountd_fail=1; }

# ── THE ONE VERB THAT KEEPS ITS SOCKET ────────────────────────────────────
#
# `subscribe` is the only long-lived connection this daemon has, and the whole
# reason its accept loop became a poll is that a bare blocking accept() would
# have left every OTHER client waiting behind the subscriber forever. So the
# assertion that matters is not that the event arrives — it is that a `list`
# is still answered WHILE a subscriber is attached.
#
# A netlink socket cannot be bound without CAP_NET_ADMIN and cannot be written
# to by a test at all, so --fixture points the daemon at a FIFO carrying the
# same NUL-separated key=value blob the kernel sends.
cat > "$OUT/kmsub.py" <<'KMSUBEOF'
import os, socket, sys, time
sock, fifo = sys.argv[1], sys.argv[2]
s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.connect(sock)
s.sendall(b"subscribe\n")
s.settimeout(5)
if s.recv(64) != b"ok\n":
    print("NOACK"); sys.exit(1)
# A second client, while the first is still attached and idle.
t = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
t.connect(sock)
t.sendall(b"list\n")
t.settimeout(5)
served = b""
try:
    while True:
        d = t.recv(4096)
        if not d:
            break
        served += d
except socket.timeout:
    print("LISTHUNG"); sys.exit(1)
print("SERVED" if b"sdb1" in served else "LISTEMPTY")
# Now the event.
fd = os.open(fifo, os.O_WRONLY | os.O_NONBLOCK)
os.write(fd, b"add@/devices/x\0ACTION=add\0SUBSYSTEM=block\0DEVNAME=sdz1\0")
os.close(fd)
try:
    print("EVENT" if b"changed" in s.recv(64) else "WRONGEVENT")
except socket.timeout:
    print("NOEVENT")
KMSUBEOF
_sub=$(python3 "$OUT/kmsub.py" "$KMSOCK" "$OUT/km.uevent" 2>&1)
case "$_sub" in
*SERVED*) echo "  ok    a list is answered while a subscriber holds its socket" ;;
*) echo "  FAIL  a subscriber blocked every other client"; echo "        $_sub"
   mountd_fail=1 ;;
esac
case "$_sub" in
*EVENT*) echo "  ok    and a block uevent reaches the subscriber as \`changed\`" ;;
*) echo "  FAIL  the uevent did not reach the subscriber"; echo "        $_sub"
   mountd_fail=1 ;;
esac
kmwant 'format 0 ext4 4
sdb1' 'ok sdb1' "and the row's own name confirms it"
kmwant 'format 0 reiserfs 4
sdb1' 'unknown filesystem' "a filesystem outside the allowlist is refused"

# The allowlist. The dispatch this replaced read an index with atoi() and threw
# the rest of the line away, so `mount 0 rm -rf /` was a well-formed mount.
kmwant 'mount 0 rm -rf /
' 'unknown command' "a verb with a token nobody named is not a verb"
# AND A LINE LONGER THAN THE TOKENISER'S ARRAY IS REFUSED, NOT TRUNCATED. The
# array has headroom over the longest verb on purpose: a line one token past it
# reaches the dispatch and is refused there as an unknown command, and a line
# that FILLS it is refused by count. Neither path drops a tail — dropping one
# is how `mount 0 rm -rf /` parsed as a well-formed `mount 0`.
kmwant 'mount 0 a b c d e f g
' 'too many arguments' "a line past the tokeniser is refused rather than cut short"
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
# ── A SHARE ON ANOTHER MACHINE ────────────────────────────────────────────
#
# `mount.cifs` builds its option string by concatenation and escapes nothing
# but the password, so a comma in the server, the share, the username or the
# domain is a NEW MOUNT OPTION handed to the kernel's cifs parser, and a slash
# or a backslash in a server re-aims the mount — the helper's own parse_unc()
# splits on exactly those. Every refusal below is one of those characters.
# ALREADY THERE IS NOT AN ERROR and it is not a second mount: the fixture's
# /proc/mounts already carries //files.example/team, so this is the answer
# `mount` gives for a stick that is already mounted.
kmwant 'cifs files.example team ada - 8
passw0rd' 'ok /media/kdos/files.example-team' \
    "a share that is already mounted answers with where it is"
grep -q 'mount.cifs' "$OUT/km.exec" \
    && { echo "  FAIL  a mounted share was mounted again"; mountd_fail=1; } \
    || echo "  ok    and no helper was run for it"

kmwant 'cifs files.example archive ada - 8
passw0rd' 'ok ' "a share names a server, a share, a user and a domain"
grep -q 'exec /sbin/mount.cifs //files.example/archive ' "$OUT/km.exec" \
    && echo "  ok    and the helper is given the UNC it asked for" \
    || { echo "  FAIL  mount.cifs was not aimed at //files.example/archive"
         grep mount.cifs "$OUT/km.exec"; mountd_fail=1; }
# THE PASSWORD TRAVELS ON A DESCRIPTOR. An option string is argv, an
# environment VALUE is /proc/<pid>/environ and a file is a file somebody has to
# delete; the descriptor is the only route that is none of those. The fixture
# prints the environment with the argv for exactly this assertion — an
# argv-only dump cannot tell a PASSWD_FD run from a `pass=` one.
grep -q '^env PASSWD_FD=0$' "$OUT/km.exec" \
    && echo "  ok    the password is handed over on stdin, through PASSWD_FD" \
    || { echo "  FAIL  mount.cifs was not told to read a descriptor"
         mountd_fail=1; }
grep -q 'pass=' "$OUT/km.exec" \
    && { echo "  FAIL  a password option reached the option string"
         mountd_fail=1; } \
    || echo "  ok    and no pass= option was assembled"
grep -q 'passw0rd' "$OUT/km.exec" \
    && { echo "  FAIL  the password appeared in an argument vector"
         mountd_fail=1; } \
    || echo "  ok    and the password itself appears nowhere in the request"
grep -q 'domain=' "$OUT/km.exec" \
    && { echo "  FAIL  a dash domain became a domain= option"; mountd_fail=1; } \
    || echo "  ok    a \`-\` domain is no domain rather than a domain named -"

kmwant 'cifs files.example,uid=0 team ada - 8
passw0rd' 'cannot' "a comma in the server is refused, not quoted"
kmwant 'cifs files.example team,uid=0 ada - 8
passw0rd' 'cannot' "a comma in the share is refused"
kmwant 'cifs files.example team ada,uid=0 - 8
passw0rd' 'cannot' "a comma in the username is refused"
kmwant 'cifs files.example/other team ada - 8
passw0rd' 'cannot' "a slash in the server cannot re-aim the mount"
kmwant 'cifs ../../etc team ada - 8
passw0rd' 'cannot' "and neither can a traversal"
# THE REQUEST LINE HAS TO HOLD A LEGAL CORPORATE SHARE. A DNS name may be 253
# bytes and a username 104; at the old 128-byte ceiling this exact request was
# refused as malformed, which is the one shape the verb exists to serve.
_long="fileserver-04.corp.subsidiary.example.co.uk"
kmwant "cifs $_long department-share-archive administrator.services CORPORATE 8
passw0rd" 'ok ' "a full corporate name, share, user and domain all fit"

kmwant 'shares
' '//files.example/team' "a connected share is listed from /proc/mounts"
# `browse` IS A DIFFERENT LIST FROM `shares` AND NEITHER IS AN INDEX. What is
# asserted here is the protocol and never the neighbours: under the fixture
# nothing is broadcast, so the answer is an empty list and an `ok` — which is
# also the answer a real network with nothing on it gives, and the reason a
# surface must not read "no servers" as "the verb failed".
kmwant 'browse
' 'ok' "a browse on a network with nothing on it answers ok and no rows"
grep -q 'nmblookup\|avahi-browse' "$OUT/km.exec" \
    && { echo "  FAIL  the fixture broadcast on somebody's network"
         mountd_fail=1; } \
    || echo "  ok    and the fixture broadcast nothing"
# A NAME THE RESOLVER CAN ALREADY ANSWER IS NEVER BROADCAST FOR. Every request
# above names a dotted server, which is the shape km_resolve returns on
# immediately — so no helper ran for one, and the option string carries no
# `ip=`. A workgroup name is the shape that does, and it cannot be asserted
# here: under the fixture nothing resolves, and on a real network the answer is
# the network's rather than this suite's.
grep -q 'ip=' "$OUT/km.exec" \
    && { echo "  FAIL  a dotted server was resolved by hand"; mountd_fail=1; } \
    || echo "  ok    a name musl can resolve reaches the helper unresolved"
kmwant 'disconnect 9
' 'no such share' "a share index past the list is refused"
kmwant 'cifs a b c d e
' 'bad request' "a byte count that is not a number is not a request"

# ── A TICKET INSTEAD OF A PASSWORD ────────────────────────────────────────
#
# `krb5` IS A VERB OF ITS OWN AND NOT `cifs` WITH AN EMPTY COUNT, and the
# reason is on the wire: a ticket is in the caller's credential cache and
# nothing about it crosses this socket, so there is no second frame to read
# and nothing to wipe afterwards. km_count() refuses a zero for that reason.
#
# `cruid=` IS THE ONE OPTION THIS VERB EXISTS FOR. The daemon is root and the
# mount is the caller's, so without it `cifs.upcall` looks in ROOT'S cache —
# empty on a machine where nobody has any reason to kinit as root — and the
# mount fails with `Required key not available` naming no user.
#
# THE LOG IS NOT TRUNCATED BETWEEN REQUESTS. It is the daemon's own stdout and
# the daemon is still running, so a truncation from outside leaves it writing
# past a hole; every string grepped for below appears in no other request.
kmwant 'krb5 files.example archive ada -
' 'ok ' "a ticket mount names a server, a share, a user and a domain"
if grep -q 'sec=krb5,cruid=' "$OUT/km.exec"; then
    echo "  ok    and the helper is told to read the CALLER's ticket cache"
else
    echo "  FAIL  a ticket mount did not carry sec=krb5,cruid="
    grep mount.cifs "$OUT/km.exec"; mountd_fail=1
fi
grep -q '^stdin 0 bytes$' "$OUT/km.exec" \
    && echo "  ok    and no secret was fed to it" \
    || { echo "  FAIL  a ticket mount fed something to mount.cifs"
         mountd_fail=1; }
kmwant 'krb5 files.example archive - -
' 'ok ' "a ticket may name no user at all — the principal says who you are"
grep -q 'user=-' "$OUT/km.exec" \
    && { echo "  FAIL  a dash username became a user= option"; mountd_fail=1; } \
    || echo "  ok    and a \`-\` user is no user rather than a user named -"
kmwant 'krb5 files.example,uid=0 archive ada -
' 'cannot' "and every field is checked exactly as the password verb's is"
kmwant 'krb5 files.example archive ada
' 'unknown command' "a ticket request one token short is not a request"
# EVERY FIELD AT ITS CEILING AT ONCE, which is the shape that overflows an
# option string. A username may be 104 bytes and an NT domain 255, and with
# `sec=krb5`, a `cruid=` and the ownership tail that is over four hundred and
# fifty — a buffer that merely LOOKED big enough truncated `dir_mode=0700` to
# `dir_mode=07`, which is a mount with permissions nobody asked for.
_ku=$(printf '%0104d' 0 | tr '0' 'a')
_kd=$(printf '%0255d' 0 | tr '0' 'd')
kmwant "krb5 fileserver-04.corp.subsidiary.example.co.uk department-share $_ku $_kd
" 'ok ' "a ticket mount with every field at its ceiling still fits"
grep -q 'dir_mode=0700,nosuid,nodev' "$OUT/km.exec" \
    && echo "  ok    and the option string is not truncated at the tail" \
    || { echo "  FAIL  the longest option string lost its tail"
         grep mount.cifs "$OUT/km.exec" | tail -1; mountd_fail=1; }

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
    # to xdg-open — BY ABSOLUTE PATH, because /usr/local/bin/xdg-open is this
    # same binary under another name and /usr/local/bin comes first on $PATH.
    # Naming it without a path here would re-enter cmd_open until the process
    # died.
    echo "$out" | grep -q "^exec	/usr/bin/xdg-open$" \
        || { echo "  no fallback for an unclaimed type: $out"; exit 1; }

    # A URL IS NOT A FILE NAME. Before kxdg_mime_for_arg the basename decided,
    # so `mailto:a@b.c` matched the `*.c` glob and a mail address resolved to
    # C++ source — offered to a text editor. Nothing in this scratch tree
    # claims the scheme, so the answer is the fallback; the point is the TYPE.
    out=$(open_print "mailto:a@b.c")
    echo "$out" | grep -q "^mime	x-scheme-handler/mailto$" \
        || { echo "  a mail address was not read as a scheme: $out"; exit 1; }
    out=$(open_print "https://example.com/page.html")
    echo "$out" | grep -q "^mime	x-scheme-handler/https$" \
        || { echo "  a URL was typed by its suffix: $out"; exit 1; }
    # file: names a path and the handler must be handed the PATH.
    out=$(open_print "file://$OPENH/files/shot.png")
    echo "$out" | grep -q "^mime	image/png$" \
        || { echo "  file:// was not unwrapped: $out"; exit 1; }
    echo "$out" | grep -q "^exec	kdos-appbox	run	gimp	$OPENH/files/shot.png$" \
        || { echo "  file:// reached the handler unwrapped: $out"; exit 1; }
    echo "  mime by longest glob, mimeapps and the cache, field codes,"
    echo "  a URL by its scheme, file:// unwrapped, xdg-open last"
fi

echo
echo "==> a desktop entry can ask for the terminal it needs, and only for one"
#
# X-KDOS-Term names the emulator an entry needs rather than the one the session
# runs, and the three cases below are the whole of it:
#
#   THE KEY IS HONOURED WHATEVER THE SESSION RUNS. `yazi`'s previews are drawn
#   by the terminal, not by yazi, so an entry asking for `kdos-term` must get
#   it even where `foot` is what the session would otherwise pick.
#
#   THE OPENER WRAPS AND NAMES NOTHING ELSE. `kdos-appbox open` puts the
#   emulator and `-e` in front of the Exec and stops there; the identity flag
#   — `--title` for kdos-term, `--app-id` for foot — is the launcher's, because
#   only a launcher knows which entry a window should answer to.
#
#   A NAME AND NEVER A PROGRAM. An entry is a file anything can write, so a key
#   naming something that is not an emulator this image ships falls back to the
#   session's own rather than being executed.
#
XTH="$OUT/xtermhome"
rm -rf "$XTH"
mkdir -p "$XTH/.config" "$XTH/.local/share/applications" "$XTH/files"
: > "$XTH/files/a.png"
for _e in pics:kdos-term plain: liar:/bin/sh; do
    _n=${_e%%:*}; _w=${_e#*:}
    {
        printf '[Desktop Entry]\nType=Application\nName=%s\n' "$_n"
        printf 'Exec=yazi %%f\nTerminal=true\n'
        [ -n "$_w" ] && printf 'X-KDOS-Term=%s\n' "$_w"
    } > "$XTH/.local/share/applications/$_n.desktop"
done
xterm_print() {			# <entry stem>
    printf '[Default Applications]\nimage/png=%s.desktop\n' "$1" \
        > "$XTH/.config/mimeapps.list"
    env HOME="$XTH" XDG_CONFIG_HOME="$XTH/.config" \
        XDG_DATA_HOME="$XTH/.local/share" \
        XDG_DATA_DIRS=/nonexistent-kdos-datadirs \
        "$OUT/kdos-appbox" open --print "$XTH/files/a.png"
}
if [ ! -f /usr/share/mime/globs ]; then
    echo "  X-KDOS-Term (skipped — no shared-mime-info on this host)"
else
    out=$(xterm_print pics)
    echo "$out" | grep -q "^exec	kdos-term	-e	yazi	" \
        || { echo "  the entry's terminal was not used: $out"; exit 1; }
    out=$(xterm_print plain)
    echo "$out" | grep -q "^exec	foot	-e	yazi	" \
        || { echo "  an entry with no key did not get the session's: $out"
             exit 1; }
    out=$(xterm_print liar)
    echo "$out" | grep -q "^exec	foot	-e	yazi	" \
        || { echo "  a key naming a program was honoured: $out"; exit 1; }
    echo "  the named emulator wins over the session's own,"
    echo "  and a key naming anything else falls back"
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
        -Isrc/desktop/kdos-shell -Isrc/libs/libkdisp -Isrc/libs/libkwl \
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
    # Every wlr protocol a front end INCLUDES, not the one the window list
    # needs: kdos-display includes output-management, and a header this
    # directory is missing takes that surface out of the harness with a
    # "goldens are skipped" line rather than a failure.
    for _wp in wlr-foreign-toplevel-management-unstable-v1 \
               wlr-output-management-unstable-v1; do
        tar xf "$DWLR" -C "$DPROTO" --strip-components=2 \
            "$(tar tf "$DWLR" | grep "protocol/$_wp.xml\$" | head -1)"
    done
    for x in "$DPROTO/wlr-foreign-toplevel-management-unstable-v1.xml" \
             "$DPROTO/wlr-output-management-unstable-v1.xml" \
             "$DWP/staging/ext-workspace/ext-workspace-v1.xml"; do
        b=$(basename "$x" .xml)
        "$DSCAN" client-header "$x" "$DPROTO/$b-client-protocol.h"
        "$DSCAN" private-code  "$x" "$DPROTO/$b-protocol.c"
    done

    # The four original surfaces, plus any Phase B front end that has landed.
    # dumpmain.c declares every entry point WEAK, so a file that is not on the
    # tree yet is a name it declines rather than a link error.
    # libkchrome is the header band, group headings and button bar the device
    # surfaces share; kch_px.c is NOT here and must not be: dumpmain.c stubs
    # the whole plate API, so compiling the real one in is a multiple
    # definition. A plate symbol a surface calls belongs in that stub set, and
    # one missing from it is a LINK failure that skips every golden below.
    # fav.c is the favourites store several of them write;
    # mountd.c is the one kdos-mountd client kdos-devices and kdos-disks both
    # call. None is a front end, so all belong in the base set rather than in
    # the candidate loop — a surface that uses one would otherwise fail to
    # LINK, which the harness reports as "the new front ends do not link" and
    # which reads as a defect in those files.
    # privacy.c is NOT here and must not be: dumpmain.c stubs the whole
    # sh_priv_* API, so compiling the real one in is a multiple definition.
    # A privacy symbol panel.c calls belongs in that stub set.
    # osd.c IS here and is a front end as well, which is cal.c's and shell.c's
    # shape: it DEFINES sh_volume_* and sh_mic_*, which panel.c calls, so the
    # harness needs it whether or not its own golden is wanted — and a file
    # that is sometimes linked and sometimes stubbed is a multiple definition
    # on exactly the hosts where it compiles.
    # routes.c, chords.c and filesearch.c are READERS, not front ends: start.c,
    # keys.c and find.c each lost one to a shared file so the palette could use
    # the same one, and a reader missing from this list is a LINK failure that
    # the harness reports as "the new front ends do not link" — which silently
    # skips every golden below it rather than failing.
    DFRONTS="src/desktop/kdos-shell/routes.c src/desktop/kdos-shell/chords.c
             src/desktop/kdos-shell/filesearch.c
             src/desktop/kdos-shell/progkeys.c
             src/desktop/kdos-shell/cal.c src/desktop/kdos-shell/menu.c
             src/desktop/kdos-shell/pick.c
             src/desktop/kdos-shell/shell.c src/desktop/kdos-shell/apps.c
             src/desktop/kdos-shell/fav.c src/desktop/kdos-shell/cells.c
             src/desktop/kdos-shell/logo.c
             src/desktop/kdos-shell/mountd.c
             src/desktop/kdos-shell/osd.c
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
        DPEEK_SRC="$DPEEK_SRC src/desktop/kdos-shell/picture.c"
        DPEEK_CF="-Isrc/libs/libkimg -DKIMG_HAVE_PNG -DKIMG_HAVE_JPEG -DKIMG_HAVE_WEBP"
        DEXTRA_PC="$DEXTRA_PC $DPEEK_PC"
    fi

    # Each candidate is admitted on its OWN compile, not the batch's: one file
    # that does not build must cost its own golden and nobody else's.
    #
    # A CANDIDATE IS LINKED IN WHETHER OR NOT IT HAS A GOLDEN, which is what
    # makes this list the only syntax check several of these files get. Three
    # of the shell's front ends cannot be on it:
    #
    #   clip     wants wlr-data-control-unstable-v1 and display wants
    #   display  wlr-output-management-unstable-v1; the proto step above
    #            extracts two protocols and neither is one of them
    #   asciicmd calls kcell_font_load/kcell_w/kcell_h, which live in
    #            libkcell/kcell_font.c — an fcft font loader, and a harness
    #            that links one stops running on a host without fcft, which
    #            is the whole point of this build
    DNEW=""
    DBAD=""
    for s in keys teams saver slit doc settings openwith audio \
             start net bt devices notify status tip panel trash peek \
             find pix rec chars disks print timezone users update firewall \
             netagent backup theme palette contacts store \
             run prompt notifyd desk connect traymenu \
             about calc note ime mediad display; do
        [ -f "src/desktop/kdos-shell/$s.c" ] || continue
        case "$s" in
        peek|pix)
            [ -z "$DPEEK_PC" ] && {
                DBAD="$DBAD $s(no libarchive)"
                continue
            }
            ;;
        esac
        if $CC $STD $SHWARN -fsyntax-only -I"$DPROTO" $DPEEK_CF \
                -Isrc/desktop/kdos-shell -Isrc/libs/libkwl -Isrc/libs/libkdisp -Isrc/libs/libkwm -Isrc/libs/libktui \
                -Isrc/libs/libkcolor -Isrc/libs/libkxdg -Isrc/libs/libkbase \
                -Isrc/libs/libkicon -Isrc/libs/libkchrome -Isrc/libs/libkproc \
                -Isrc/libs/libkcell -Isrc/libs/libkvt \
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
            -DKDOS_WHISPER_DIR="\"/nonexistent-kdos-whisper\"" \
            -Isrc/desktop/kdos-shell -Isrc/libs/libkwl -Isrc/libs/libkdisp -Isrc/libs/libkwm -Isrc/libs/libktui \
            -Isrc/libs/libkcolor -Isrc/libs/libkxdg -Isrc/libs/libkbase \
            -Isrc/libs/libkicon -Isrc/libs/libkchrome -Isrc/libs/libkproc \
            -Isrc/libs/libkcell -Isrc/libs/libkvt \
            $(pkg-config --cflags pixman-1 fcft 2>/dev/null) \
            -Wl,--wrap=ktui_offscreen_init \
            testing/fixtures/shell/dumpmain.c $DFRONTS "$@" \
            "$DPROTO"/*-protocol.c \
            src/libs/libktui/*.c src/libs/libkcolor/*.c src/libs/libkxdg/*.c \
            src/libs/libkbase/*.c src/libs/libkproc/*.c \
            src/libs/libkvt/*.c \
            $(pkg-config --cflags --libs wayland-client pixman-1 $DEXTRA_PC)
    }
    # The libraries kdos-peek pulls in are added only when it was admitted:
    # a harness that linked four decoders for a surface it does not carry
    # would fail on a host that has them and nothing that needs them.
    case " $DNEW " in
    *peek.c*|*pix.c*) DNEW="$DNEW $DPEEK_SRC" ;;
    esac
    if [ -n "$DNEW" ] && dumpbuild $DNEW 2>"$OUT/dumpnew.err"; then
        DUMPCK="$OUT/dumpcheck"
        echo "  harness: cal menu pick $(echo $DNEW | \
            sed 's,src/libs/[^ ]*,,g; s,[^ ]*/picture\.c,,g; \
                 s,src/desktop/kdos-shell/,,g; s,\.c,,g')"
    elif dumpbuild; then
        # Every candidate compiled on its own, so a failure here is a LINK
        # failure — a surface wanting a library this harness does not offer.
        DUMPCK="$OUT/dumpcheck"
        [ -n "$DNEW" ] && {
            echo "  NOTE: the new front ends do not LINK into the dump harness,"
            echo "        so their goldens are skipped:$(echo $DNEW | \
                sed 's,src/desktop/kdos-shell/,,g; s,\.c,,g')"
            # `multiple definition` carries neither of the other two words,
            # and it is HALF of what breaks this link: a stub in dumpmain.c
            # for a symbol one of the files above now defines. A filter that
            # missed it reported only the undefined half and sent the next
            # reader looking for a missing library.
            grep -m5 "undefined\|multiple definition\|error" \
                "$OUT/dumpnew.err" | sed 's/^/        /'
        }
    else
        echo "  the dump harness does not build"; exit 1
    fi

    #
    # HOW A SEARCH IS ORDERED, WHICH NO GOLDEN COVERS. The dump fixture points
    # XDG_DATA_DIRS at /nonexistent, so the launcher's committed golden reads
    # "no match" and every application golden is an empty list — the ranking is
    # invisible to all of them. That is why replacing three private matchers
    # with `kb_fuzzy()` moved no golden at all, and why it needs a check of its
    # own.
    #
    # A FIXTURE BUILT HERE AND NOT UNDER testing/fixtures/. A fixture file
    # added there to enrich one surface's frame moves another surface's
    # committed goldens — that has happened once already, with a `places` file.
    #
    # The launch path is stubbed: this asks how a list is ORDERED and starts
    # nothing, so the four symbols that would run a program are exactly the
    # four that must not be linked in.
    #
    # THE FOCUSED PROGRAM'S OWN KEYS, one reader per program. Each is silent
    # when its program or its source is absent, and silence is the whole
    # failure mode — the card shows the page only when a reader answers with
    # rows — so what this asserts is that the ones that CAN answer do, and
    # that the ones that cannot stay quiet rather than inventing a page.
    #
    # tmux answers live and mc answers from a file, which is why both are
    # here: they are the two shapes a reader can have. micro is the third
    # shape — its defaults are compiled in and no flag prints them, so its
    # reader shows the overrides file and NOTHING on a machine where nobody
    # has rebound anything, which is exactly what a fresh container is.
    cat > "$OUT/pkdrv.c" <<'PKEOF'
#include <stdio.h>
#include <string.h>
#include "progkeys.h"

int main(int argc, char **argv)
{
	struct sh_progkey k[200];
	int n = sh_progkeys(argv[1], k, 200);

	(void)argc;
	printf("%s known=%d rows=%d\n", argv[1], sh_progkeys_known(argv[1]), n);
	for (int i = 0; i < n && i < 3; i++)
		printf("  %s\t%s\n", k[i].key, k[i].desc);
	return 0;
}
PKEOF
    if $CC $STD $SHWARN -Isrc/desktop/kdos-shell -Isrc/libs/libkbase \
        -o "$OUT/pkdrv" "$OUT/pkdrv.c" src/desktop/kdos-shell/progkeys.c \
        src/libs/libkbase/*.c 2>"$OUT/pkdrv.err"; then
        _pkfail=0
        # A program with no reader answers nothing and says it knows nothing:
        # the card asks `known` before it offers the page, so a Tab is never
        # offered into an empty screen.
        "$OUT/pkdrv" nosuchprogram | grep -qx "nosuchprogram known=0 rows=0" ||
            { echo "  a program with no reader did not answer nothing"
              _pkfail=1; }
        # helix is NAMED by the plan and is in no packages.txt at all, so it
        # has never been built or shipped: a reader for it could not run and
        # could not be checked, and it must not pretend otherwise.
        "$OUT/pkdrv" helix | grep -qx "helix known=0 rows=0" ||
            { echo "  helix answers as though a reader existed"; _pkfail=1; }
        if command -v tmux >/dev/null 2>&1; then
            _pkt=$("$OUT/pkdrv" tmux)
            case "$_pkt" in
            *"known=1 rows=0"*)
                echo "  tmux is installed and its reader found nothing"
                _pkfail=1 ;;
            *) # The prefix has to be ON the row: `c` is not what anybody
               # presses, and the whole reason the prefix is asked for is that
               # a row without it names a key that does nothing.
               echo "$_pkt" | grep -q "	" &&
               echo "$_pkt" | sed -n '2p' | grep -qE "^  (C-|M-|[^ ]+ )" ||
                   { echo "  tmux rows do not carry the prefix:"
                     echo "$_pkt" | sed -n '1,3p' | sed 's/^/    /'
                     _pkfail=1; } ;;
            esac
        else
            echo "  tmux keys (skipped — tmux not on this host)"
        fi
        if [ -r /etc/mc/mc.keymap ] || [ -r fs/etc/skel/.config/mc/ini ]; then
            # The keymap the IMAGE ships, read from the build tree rather than
            # from this host's /etc: the plan says /usr/share/mc/mc.keymap and
            # the port configures --sysconfdir=/etc, so the file is at
            # /etc/mc/ and a reader looking in the plan's place finds nothing.
            _mck=$(ls build/fs/etc/mc/mc.default.keymap 2>/dev/null | head -1)
            if [ -n "$_mck" ]; then
                mkdir -p "$OUT/mcroot/mc"
                cp "$_mck" "$OUT/mcroot/mc/mc.keymap"
                _pkm=$(XDG_CONFIG_HOME="$OUT/mcroot" "$OUT/pkdrv" mc)
                case "$_pkm" in
                *"rows=0"*)
                    echo "  mc's shipped keymap parsed to nothing"
                    echo "$_pkm" | sed 's/^/    /'; _pkfail=1 ;;
                *) : ;;
                esac
            fi
        fi
        [ "$_pkfail" = 0 ] &&
            echo "  the program pages read what their programs publish," &&
            echo "  and a program with no reader offers no page" ||
            exit 1
    else
        echo "  program keys (skipped — the driver does not build)"
        head -3 "$OUT/pkdrv.err" | sed 's/^/    /'
    fi

    _rankfix="$OUT/rank-apps"
    rm -rf "$_rankfix"
    mkdir -p "$_rankfix/applications"
    _mkapp() {  # <file> <Name> <Exec> <Keywords>
        printf '[Desktop Entry]\nType=Application\nName=%s\nExec=%s\nKeywords=%s\n' \
            "$2" "$3" "$4" > "$_rankfix/applications/$1.desktop"
    }
    _mkapp sysmon "System Monitor" kdos-res   "cpu;memory;"
    _mkapp shares "Network Shares" kdos-mount "wobble;share;"
    _mkapp asm    "Assembler"      as         "compile;"
    _mkapp term   "Terminal"       kdos-term  "shell;"
    _mkapp kterm  "KDOS Terminal"  kdos-term2 "shell;"

    cat > "$OUT/rankdrv.c" <<'RANKEOF'
#include <stdio.h>
#include <string.h>
#include "shell.h"

static int bad;

static void first_is(const char *q, const char *want)
{
	const struct sh_app *out[8];
	int n = sh_apps_match(q, out, 8);

	if (!n) {
		printf("    '%s' matched nothing\n", q);
		bad = 1;
		return;
	}
	if (strcmp(out[0]->name, want)) {
		printf("    '%s' ranked '%s' first, want '%s' (%d hits)\n",
		       q, out[0]->name, want, n);
		bad = 1;
	}
}

int main(void)
{
	if (sh_apps_load() <= 0) {
		printf("    the fixture index is empty\n");
		return 1;
	}
	/* AN ACRONYM OVER A MID-WORD SUBSEQUENCE, which is the whole reason
	 * this is a subsequence matcher: the substring matcher it replaced
	 * found nothing at all for `sm`. Two PERFECT acronyms would tie and
	 * the name would break it, so the rival here is `Assembler` — a
	 * preference between two equally good answers is a ranking nobody
	 * could justify from the outside. */
	first_is("sm", "System Monitor");
	/* A prefix beats the same word further in. */
	first_is("term", "Terminal");
	/* A whole name still wins outright. */
	first_is("assembler", "Assembler");
	/* A weaker field is a reason and a weaker one: nothing is NAMED
	 * `wobble` and one entry carries it as a keyword. */
	first_is("wobble", "Network Shares");
	return bad;
}

/* Stubs: see above. The launch path, which this driver never reaches —
 * `sh_desktop_entry` is `sh_launch_id`'s resolver and lives in shell.c, which
 * is a Wayland client. */
void sh_strip_field_codes(char *s) { (void)s; }
void sh_spawn(const char *const argv[]) { (void)argv; }
int sh_term_argv_in(const char *w, int flt, const char *size,
		    const char *argv[], int n, int max,
		    const char *cmd, char *id, size_t idsz)
{ (void)w; (void)flt; (void)size; (void)argv; (void)max; (void)cmd; (void)id;
  (void)idsz; return n; }
int sh_desktop_entry(const char *id, struct sh_entry *out)
{ (void)id; (void)out; return -1; }
RANKEOF
    if $CC $STD $SHWARN -I"$PROTO" -Isrc/desktop/kdos-shell \
        -Isrc/libs/libkbase -Isrc/libs/libktui -Isrc/libs/libkcolor \
        -Isrc/libs/libkcell -Isrc/libs/libkwl -Isrc/libs/libkdisp \
        -Isrc/libs/libkwm -Isrc/libs/libkxdg \
        -Isrc/libs/libkicon -Isrc/libs/libkchrome -Isrc/libs/libkproc \
        $(pkg-config --cflags fcft pixman-1 xkbcommon wayland-client) \
        -o "$OUT/rankdrv" "$OUT/rankdrv.c" src/desktop/kdos-shell/apps.c \
        src/libs/libkbase/*.c src/libs/libkxdg/*.c src/libs/libkproc/*.c \
        2>"$OUT/rankdrv.err"; then
        if XDG_DATA_HOME="$_rankfix" XDG_DATA_DIRS=/nonexistent-kdos-datadirs \
           HOME="$OUT" "$OUT/rankdrv"; then
            echo "  one matcher: an acronym, a prefix, a whole name and a keyword"
            echo "  each rank where a person means them"
        else
            echo "  THE SHARED MATCHER RANKS A SEARCH THE WRONG WAY ROUND"
            exit 1
        fi
    else
        echo "  ranking (skipped — the driver does not build)"
        head -3 "$OUT/rankdrv.err" | sed 's/^/    /'
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
#
# EVERY SURFACE ANSWERS THE CONTRACT, AND SAYS SO ON ITS BOTTOM ROW.
#
# Wave K's contract is four keys and a line that names them: `F1` where there
# is a page, `F10` and `Shift+F10` where there is a menu, `Esc` always — and a
# surface with none of the first three neither advertises nor swallows them,
# which is the half that makes the row honest rather than decorative.
#
# WHAT IS OBSERVABLE IN A FRAME IS THE ROW. The keys themselves are proved
# against the widget in src/libs/selftest.c, where a KtuiKeys can be driven
# without a surface; what a golden can say is that the surface DREW the row,
# and a surface that stopped drawing it is a surface whose keys nobody can
# find.
#
# FURNITURE IS NOT A SURFACE. The taskbar, the tooltip, the savers and the
# menus are drawn ON the desktop rather than in a window: a saver closes on any
# key and a tooltip answers none, so a row naming Esc on either would be a row
# teaching a key that does nothing. They are named here with that reason rather
# than skipped by a pattern that would also hide a real surface.
#
# A TRAY MENU IS THE SAME FURNITURE AS THE OTHER MENUS, and it is named here
# although its own goldens are narrower than this loop reads: an exemption that
# exists only because of a frame's width is one that surprises whoever takes
# the first wide frame of it.
#
# AND SO ARE THE DESKTOP, THE TOAST STACK AND THE BEZEL, each for the reason
# above and each with nowhere to put a row. `desk` has no frame at all — it IS
# the background, and its keys belong to the icons on it; `notifyd` is the
# toast stack, which answers no key and goes on a timer; the three `osd` frames
# are the volume, brightness and microphone bezels, which appear on a change
# and go the same way. A row naming Esc on any of them would name a key that
# does nothing.
#
echo "==> every surface draws the row that names its keys"
_furniture=" start start-route start-system menu-system traymenu traymenu-folders tip desk notifyd osd-volume osd-brightness osd-mic saver saver-clock saver-fire saver-matrix saver-pipes saver-starfield "
_norow=""
for _g in testing/goldens/*-80x24.txt; do
    _n=$(basename "$_g" -80x24.txt)
    case "$_n" in cells-*|res-*|term-*|vt-*|shell*) continue ;; esac
    case "$_furniture" in *" $_n "*) continue ;; esac
    # THE ROW ABOVE THE BOTTOM BORDER. A hint is `Key verb`, so the row must
    # hold at least two words between the frame's own columns — a blank row
    # there is a surface whose keys nobody can find.
    _row=$(awk 'NR>1{print p} {p=$0}' "$_g" | tail -1 | sed 's/^.//; s/.$//')
    case "$_row" in
    *[A-Za-z]*[[:space:]]*[A-Za-z]*) ;;
    *) _norow="$_norow $_n" ;;
    esac
done
if [ -z "$_norow" ]; then
    echo "  every surface's frame carries a row naming its keys"
else
    echo "  SURFACES WHOSE BOTTOM ROW NAMES NO KEY:$_norow"
    golden_fail=1
fi

#
# AND THE ROW IS PUSHED THROUGH THE CONTRACT, not written by hand. A surface
# that drew its own bottom line would be a surface whose Esc verb stopped
# following the layer it is on — `ktui_esc_verb` says Back where a rung is
# open and Close where none is, and a hand-written row says one of them for
# ever.
#
# menu.c IS THE ONE FILE THAT HOLDS A KtuiKeys AND DRAWS NO ROW, and its own
# header says why: a menu answers the keys its shape implies — arrows, Enter,
# Esc — and "a row is worth a row where a surface answers keys its shape does
# not imply, which is every other surface here and not this one". Named with
# that reason rather than skipped by a pattern that would also hide a real one.
_nokeys=""
for _f in src/desktop/kdos-shell/*.c; do
    case "$(basename "$_f")" in menu.c) continue ;; esac
    grep -q 'KtuiKeys' "$_f" || continue
    grep -q 'ktui_hint_row(' "$_f" || _nokeys="$_nokeys $(basename "$_f")"
done
if [ -z "$_nokeys" ]; then
    echo "  and every surface holding a KtuiKeys draws it through ktui_hint_row"
else
    echo "  SURFACES WITH A KtuiKeys AND NO HINT ROW:$_nokeys"
    golden_fail=1
fi

if [ -n "$DUMPCK" ]; then
"$DUMPCK" cal --dump > "$OUT/dump-cal.txt"
check_box cal < "$OUT/dump-cal.txt"
grep -q "Mo Tu We Th Fr Sa Su" "$OUT/dump-cal.txt" || {
    echo "  the calendar lost its weekday header"; exit 1; }
# WITH NOTHING ON, THE POPUP IS THE ONE IT ALWAYS WAS. The strip costs rows
# only when there is something to put in them, so a machine with no calendar
# draws exactly what it drew before khal existed.
_calrows=$(wc -l < "$OUT/dump-cal.txt")
[ "$_calrows" = 12 ] || {
    echo "  an empty calendar is $_calrows rows, not 12"; exit 1; }

# AND WITH SOMETHING ON IT. khal is not forked here — $PATH is not fixed for a
# dump and neither is a calendar store — so KDOS_CAL_LIST stands in with
# exactly the lines khal is asked to print. The dates are TODAY's because cal
# reads the clock and honours no override: the strip is today's by definition.
_today=$(date +%Y-%m-%d)
printf '%s\t09:30\tStandup\n%s\t\tRelease day\n' "$_today" "$_today" \
    > "$OUT/cal-list.txt"
KDOS_CAL_LIST="$OUT/cal-list.txt" "$DUMPCK" cal --dump > "$OUT/dump-cal2.txt"
check_box cal < "$OUT/dump-cal2.txt"
grep -q "09:30 Standup" "$OUT/dump-cal2.txt" || {
    echo "  the agenda strip did not draw a timed event"; exit 1; }
grep -q "Release day" "$OUT/dump-cal2.txt" || {
    echo "  the agenda strip did not draw an all-day event"; exit 1; }
# The day that has something carries a mark, in the column the grid leaves
# spare — a character, because a dump proves a character and never a colour.
grep -qE '\*' "$OUT/dump-cal2.txt" || {
    echo "  the day with events was not marked"; exit 1; }
_calrows2=$(wc -l < "$OUT/dump-cal2.txt")
[ "$_calrows2" = 14 ] || {
    echo "  two events should add two rows, got $_calrows2"; exit 1; }
echo "  the calendar: empty is 12 rows, two events add two and a day mark"

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
#   menu apps  scans /usr/share/applications, which is the host's
#   menu places reads /proc/mounts and the $HOME xdg dirs
#   saver rain seeds from time() ^ getpid(); phosphor rain is never twice the
#              same picture, which is the point of it. The ART mode IS
#              goldened, below: its dump takes a fixed seed and its picture is
#              a file, so the fixture supplies one and the frame is the same
#              everywhere
#   slit       renders the OUTPUT of forked gadget commands, arriving
#              asynchronously — a dump catches whatever had answered by then
#   bt         needs a system bus, and what is ON it — a paired headset — is
#              the machine's, not a fixture's
#   devices    /dev/video* and /proc/asound are the host's
#   time       draws a running CLOCK, which is a different frame every second
#   audio      enumerates the machine's ALSA cards and connects to a live
#              PipeWire registry; there is no fixture seam, so a dump is
#              whatever sound hardware answered
#   users      kb_users() walks getpwent() and the group list comes from
#              getgrent(); KDOS_ETC redirects only login.conf, so the account
#              list is the developer's own
#   about      reads /etc/os-release, uname(), /proc/cpuinfo, /proc/meminfo and
#              /proc/uptime and counts /var/lib/kpkg/db — a machine report, and
#              a frame of one is true on the host that wrote it and nowhere
#              else
#
# Two are goldened but need their own environment rather than the loop's, and
# both are set up below: `disks` is pointed at a mountd socket that is not
# there, and `print` at recorded `lpstat`/`lpinfo` answers.
#
# Two more need an ARGUMENT rather than an environment. `launcher` is goldened
# with `--apps`, and the loop's XDG_DATA_DIRS points at nothing, so its frame is
# the empty state instead of the host's menu. `openwith` is goldened against a
# file inside the openwith fixture: its header row is right-truncated to the
# field, so what the frame carries is the tail of that path and not the
# checkout's.
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
    _g_rc=0
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
          ${KDOS_GOLDEN_MODEL:+KDOS_WHISPER_MODEL=$KDOS_GOLDEN_MODEL} \
          ${KDOS_GOLDEN_CHARIDX:+KDOS_CHARIDX=$KDOS_GOLDEN_CHARIDX} \
          KDOS_DUMP_SIZE="$_g_size" "$DUMPCK" "$@" ) > "$_g_got" || _g_rc=$?
    #
    # A SURFACE THAT EXITS NON-ZERO COSTS ITS OWN GOLDEN AND NOT THE RUN.
    # `set -e` is on, so without catching this a single surface refusing a
    # flag ends the whole suite where it stands — every check below it,
    # including the ones that say whether the goldens are committed at all,
    # simply never runs and the failure reads as "the suite stopped".
    # It is the rule the candidate compile loop already keeps: each is
    # admitted on its own.
    #
    if [ "${_g_rc:-0}" != 0 ]; then
        echo "  $_g_name-$_g_size: the surface exited ${_g_rc}"
        sed 's/^/      /' "$_g_got" | head -3
        _g_rc=0
        golden_fail=1
        return 0
    fi
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
# kdos-disks AND kdos-print EACH NEED THEIR OWN ENVIRONMENT, which is why
# neither is in the loop above.
#
# The disks window draws what kdos-mountd published, and on a host that happens
# to be running one it would draw that host's sticks. Pointed at a socket that
# is not there it draws the refusal, which is a real state and the one every
# machine without the daemon shows.
#
# The printers window runs `lpstat` and `lpinfo`, so a machine with CUPS set up
# and one without draw different frames and neither is wrong. `--fixture` reads
# recorded answers instead, the same seam kdos-mountd and kdos-energyd use.
if "$DUMPCK" --have disks; then
    KDOS_MOUNTD_SOCKET=/nonexistent-kdos-mountd \
        golden disks 80x24  disks --dump
    KDOS_MOUNTD_SOCKET=/nonexistent-kdos-mountd \
        golden disks 56x24  disks --dump
    KDOS_MOUNTD_SOCKET=/nonexistent-kdos-mountd \
        golden disks 132x43 disks --dump
fi
# THE CONNECT WINDOW, POINTED AT THE SAME ABSENT SOCKET and for the same
# reason: on a host running a real kdos-mountd it would draw that host's
# shares. `--dump --browse` is the browse list, whose row count is the
# network's and therefore never the same twice — against a socket that is not
# there it is the refusal, which is the one frame every machine draws alike.
if "$DUMPCK" --have connect; then
    KDOS_MOUNTD_SOCKET=/nonexistent-kdos-mountd \
        golden connect 80x24  connect --dump
    KDOS_MOUNTD_SOCKET=/nonexistent-kdos-mountd \
        golden connect 56x24  connect --dump
    KDOS_MOUNTD_SOCKET=/nonexistent-kdos-mountd \
        golden connect-browse 80x24 connect --dump --browse
fi

#
# A TRAY ITEM'S OWN MENU, AGAINST A REAL dbusmenu SERVER ON A BUS OF ITS OWN.
#
# THE STUB IS NOT OPTIONAL AND A MOCK WOULD PROVE NOTHING. What can go wrong
# in that surface is the READER: `(ia{sv}av)` is a recursive signature with a
# variant per child, and a reader that miscounts a container leaves sd-bus's
# cursor somewhere it cannot name — every row after the mistake is nonsense and
# the frame still draws. So the tree is built by a real sd-bus and read by the
# real reader, and the golden is what the two agree on.
#
# EVERYTHING THE PARSER CAN GET WRONG IS IN ONE TREE: a mnemonic underscore
# that must be stripped, a separator, a disabled row, a submenu with three
# children, two toggle states, and a row marked `visible: false` that must not
# appear at all.
#
# A BUS OF ITS OWN, for the netagent block's reason: a stub on the host's
# session bus is a name on the machine running the tests.
#
if [ -n "$TRAY_SDBUS" ] && command -v dbus-daemon >/dev/null 2>&1 &&
   "$DUMPCK" --have traymenu; then
    TMO="$OUT/traymenu"
    mkdir -p "$TMO"
    if $CC $STD $WARN -o "$TMO/menustub" \
            testing/fixtures/traymenu/menustub.c \
            $(pkg-config --cflags --libs "$TRAY_SDBUS") \
            2>"$TMO/stub.err"; then
        cat > "$TMO/bus.conf" <<'TMBUS'
<!DOCTYPE busconfig PUBLIC "-//freedesktop//DTD D-BUS Bus Configuration 1.0//EN"
 "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
<busconfig>
  <type>session</type>
  <listen>unix:tmpdir=/tmp</listen>
  <policy context="default">
    <allow send_destination="*" eavesdrop="true"/>
    <allow eavesdrop="true"/>
    <allow own="*"/>
  </policy>
</busconfig>
TMBUS
        dbus-daemon --config-file="$TMO/bus.conf" --print-address=3 --fork \
            --print-pid=4 3>"$TMO/addr" 4>"$TMO/pid"
        DBUS_SESSION_BUS_ADDRESS="$(cat "$TMO/addr")"
        export DBUS_SESSION_BUS_ADDRESS
        "$TMO/menustub" 20 > "$TMO/stub.txt" 2>&1 &
        _tmp=$!
        # The name has to be ON the bus before the surface asks for it; a
        # dump that raced it would draw "the menu did not answer" and the
        # golden would record a timing accident.
        _tw=0
        while ! grep -q '^READY ' "$TMO/stub.txt" 2>/dev/null &&
              [ "$_tw" -lt 50 ]; do
            sleep 0.1
            _tw=$((_tw + 1))
        done
        golden traymenu 42x12 traymenu org.kdos.test.TrayMenu /MenuBar \
            --name Syncthing --dump
        # AND THE SUBMENU, which is the half the recursion is for: three rows
        # that only exist one level down, two of them carrying a toggle.
        golden traymenu-folders 42x12 traymenu org.kdos.test.TrayMenu \
            /MenuBar --name Syncthing --open 2 --dump
        # AND THE CALL THE SURFACE MAKES BEFORE IT READS. An application fills
        # a submenu in when it is told the menu is about to be shown, so a host
        # that skipped it would draw the tree as it stood before anybody looked.
        grep -q '^ABOUTTOSHOW$' "$TMO/stub.txt" \
            && echo "  the menu is told it is about to be shown" \
            || { echo "  THE MENU WAS READ WITHOUT AboutToShow"; golden_fail=1; }
        # AND A PICK REACHES THE APPLICATION as the id the tree named, over
        # `Event`. It is the surface's only outward effect and the one thing a
        # frame cannot show.
        "$DUMPCK" traymenu org.kdos.test.TrayMenu /MenuBar --dump --pick 6 \
            >/dev/null 2>&1
        grep -q '^EVENT 6 clicked$' "$TMO/stub.txt" \
            && echo "  and a pick reaches it as Event(id, clicked)" \
            || { echo "  A PICK DID NOT REACH THE APPLICATION:"
                 sed 's/^/    /' "$TMO/stub.txt"; golden_fail=1; }
        kill "$_tmp" 2>/dev/null || true
        wait "$_tmp" 2>/dev/null || true
        kill "$(cat "$TMO/pid")" 2>/dev/null || true
        unset DBUS_SESSION_BUS_ADDRESS
    else
        echo "  kdos-traymenu goldens (skipped — the stub does not build)"
        sed 's/^/    /' "$TMO/stub.err" | head -5
    fi
else
    echo "  kdos-traymenu goldens (skipped — no sd-bus or no dbus-daemon)"
fi
# kdos-update computes none of its three answers — `kdos update check --json`,
# `kdos cve --json` and `kdos-bootctl status` do — so its picture depends on
# the host's ports tree, package database and boot state. All three are pointed
# at recordings.
if "$DUMPCK" --have update; then
    _uf="$PWD/testing/fixtures/update"
    KDOS_UPDATE_JSON="$_uf/update.json" KDOS_CVE_JSON="$_uf/cve.json" \
    KDOS_SLOT_TEXT="$_uf/slot.txt" golden update 80x24  update --dump
    KDOS_UPDATE_JSON="$_uf/update.json" KDOS_CVE_JSON="$_uf/cve.json" \
    KDOS_SLOT_TEXT="$_uf/slot.txt" golden update 56x24  update --dump
    KDOS_UPDATE_JSON="$_uf/update.json" KDOS_CVE_JSON="$_uf/cve.json" \
    KDOS_SLOT_TEXT="$_uf/slot.txt" golden update 132x43 update --dump
    KDOS_UPDATE_JSON="$_uf/update.json" KDOS_CVE_JSON="$_uf/cve.json" \
    KDOS_SLOT_TEXT="$_uf/slot.txt" \
        golden update-security 80x24 update --security --dump
fi
# kdos-firewall asks kdos-powerd for the service table, so its picture depends
# on a running daemon. Recorded instead.
# kdos-display's rows are the compositor's, so a dump has none and the only
# frame it could draw unaided is the empty one. The fixture is what makes the
# row drawing — the scaled screen, the preferred-mode star, the transform and
# the off row — something a golden can hold.
if "$DUMPCK" --have display; then
    _dl="$PWD/testing/fixtures/display/list.txt"
    KDOS_DISPLAY_LIST="$_dl" golden display 80x24  display --dump
    KDOS_DISPLAY_LIST="$_dl" golden display 132x43 display --dump
    golden display-empty 80x24 display --dump
fi
if "$DUMPCK" --have firewall; then
    _ff="$PWD/testing/fixtures/firewall/list.txt"
    KDOS_FIREWALL_LIST="$_ff" golden firewall 80x24  firewall --dump
    KDOS_FIREWALL_LIST="$_ff" golden firewall 56x24  firewall --dump
    KDOS_FIREWALL_LIST="$_ff" golden firewall 132x43 firewall --dump
fi
if "$DUMPCK" --have print; then
    _pf="$PWD/testing/fixtures/print"
    golden print       80x24  print --fixture "$_pf" --dump
    golden print       56x24  print --fixture "$_pf" --dump
    golden print       132x43 print --fixture "$_pf" --dump
    golden print-found 80x24  print --fixture "$_pf" --found --dump
fi

# kdos-backup IS GOLDENED AGAINST A RECORDED RESTIC REPOSITORY. The window is
# a list of what `restic snapshots --json` returned, so pointing it at a
# recording is the whole of what it needs — and the recording came from a real
# repository this tree's own restic created, not from a hand-written document
# that would agree with the parser by luck.
#
# THE PASSWORD PATH IS NOT EXERCISED HERE and that is deliberate: with
# --fixture the surface never reads the password file at all, so the golden
# cannot accidentally depend on one existing. The mode refusal is asserted
# separately below, where it can be given a file with the wrong mode.
if [ -n "${DUMPCK:-}" ] && "$DUMPCK" --have backup; then
    # golden() runs from testing/fixtures/shell, so the recording is named from
    # the repository root before that cd rather than relative to it.
    _bkf="$PWD/testing/fixtures/backup"
    # The config comes from testing/fixtures/shell/config, which golden() already
    # points XDG_CONFIG_HOME at — the same place the keybind card's frozen
    # rc.xml and the settings window's comp.conf live.
    for _bs in 80x24 56x24 132x43; do
        golden backup "$_bs" backup --fixture "$_bkf" --dump
    done

    # THE MODE REFUSAL, WHICH IS THE SECURITY LINE OF THIS SURFACE. A password
    # file the group or the world can read hands the key to every backup on the
    # machine to every account on it, and 0644 is what an editor leaves. The
    # refusal has to come BEFORE the password is used, so it is asserted on the
    # `--once` path, which is the one a timer runs unattended.
    _bkc="$OUT/backup-conf"
    mkdir -p "$_bkc/kdos"
    cp testing/fixtures/shell/config/kdos/backup.conf "$_bkc/kdos/"
    printf 'hunter2\n' > "$_bkc/kdos/backup.pass"
    chmod 644 "$_bkc/kdos/backup.pass"
    _bko="$OUT/backup-mode.txt"
    # The command is the CONDITION, because it is meant to fail and `set -e`
    # would take the script down before the status could be looked at.
    if XDG_CONFIG_HOME="$_bkc" "$DUMPCK" backup --once > "$_bko" 2>&1; then
        echo "  FAIL  a 0644 password file was accepted"
        sed 's/^/    /' "$_bko"
        exit 1
    fi
    grep -q "0644" "$_bko" || {
        echo "  FAIL  the refusal does not name the mode:"
        sed 's/^/    /' "$_bko"
        exit 1
    }
    echo "  a group-readable password file is refused before it is used"
else
    echo "  the kdos-backup goldens are skipped (it did not link)"
fi

# kdos-devices' SCANNER SECTION, WITHOUT A GOLDEN FOR THE SURFACE. The rest of
# that window is /dev/video*, /proc/asound and the host's input devices, so the
# frame cannot be committed — but the scanner list comes from one command, and
# that one is recordable. What is asserted is the PARSE, which is the part that
# can be wrong: `scanimage` names a device with colons inside it and a model
# with spaces in it, so splitting on either loses one of the two shapes.
if [ -n "${DUMPCK:-}" ] && "$DUMPCK" --have devices; then
    _dvo="$OUT/devices-scan.txt"
    ( cd testing/fixtures/shell && env LC_ALL=C TZ=UTC HOME="$PWD" \
        XDG_CACHE_HOME=/nonexistent-kdos-cache \
        XDG_CONFIG_HOME="$PWD/config" \
        XDG_DATA_HOME=/nonexistent-kdos-data \
        XDG_DATA_DIRS=/nonexistent-kdos-datadirs \
        XDG_RUNTIME_DIR=/nonexistent-kdos-run \
        KDOS_MOUNTD_SOCKET=/nonexistent-kdos-mountd \
        KDOS_DUMP_SIZE=132x43 "$DUMPCK" devices \
        --fixture "$PWD/../devices" --dump ) > "$_dvo" 2>&1 || true
    _dvfail=0
    for _w in "SCANNERS" "CANON Canon TR8500 series" \
              "airscan:e0:Canon TR8500 series" "Plustek OpticBook 3800" \
              "genesys:libusb:001:004"; do
        grep -qF "$_w" "$_dvo" || {
            echo "  FAIL  the scanner section has no '$_w'"
            _dvfail=1
        }
    done
    [ "$_dvfail" = 0 ] || { sed 's/^/    /' "$_dvo" | head -20; exit 1; }
    echo "  the scanner section lists both device shapes with their models"
else
    echo "  the scanner section is skipped (kdos-devices did not link)"
fi

# kdos-net GETS A GOLDEN AT LAST, because the reason it had none stopped being
# true: it needs a system bus, and a test can start one. testing/fixtures/net/
# serves the single GetManagedObjects the surface makes, with the object paths
# and property types read off the interface XML this image ships.
#
# WHAT THE RECORDING IS FOR. An AccessPoint is exported under
# /org/freedesktop/NetworkManager/AccessPoint/<n> and a device under
# .../Devices/<n>; they share a prefix and nothing else, so a surface that
# associated the two by path drew its radios over an EMPTY list and no test
# could see it. The fixture gives the second radio a network the first cannot
# see, so a guess at the association is a wrong frame rather than a lucky one.
if [ -n "${DUMPCK:-}" ] && [ -n "$TRAY_SDBUS" ] &&
   command -v dbus-daemon >/dev/null 2>&1 && "$DUMPCK" --have net; then
    NETO="$OUT/net"
    mkdir -p "$NETO"
    if $CC $STD $WARN -o "$NETO/nmobjstub" testing/fixtures/net/nmobjstub.c \
            $(pkg-config --cflags --libs "$TRAY_SDBUS"); then
        cat > "$NETO/bus.conf" <<'NETBUS'
<!DOCTYPE busconfig PUBLIC "-//freedesktop//DTD D-BUS Bus Configuration 1.0//EN"
 "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
<busconfig>
  <type>system</type>
  <listen>unix:tmpdir=/tmp</listen>
  <policy context="default">
    <allow send_destination="*" eavesdrop="true"/>
    <allow eavesdrop="true"/>
    <allow own="*"/>
  </policy>
</busconfig>
NETBUS
        # A bus of its own. The surface opens the SYSTEM bus, and the host's
        # own NetworkManager must never be what answers: the frame would then
        # be this machine's networks and would drift on every run.
        dbus-daemon --config-file="$NETO/bus.conf" --print-address=3 --fork \
            --print-pid=4 3>"$NETO/addr" 4>"$NETO/pid"
        DBUS_SYSTEM_BUS_ADDRESS="$(cat "$NETO/addr")"
        export DBUS_SYSTEM_BUS_ADDRESS
        "$NETO/nmobjstub" > "$NETO/stub.log" 2>&1 &
        _nsp=$!
        sleep 0.4
        for _ns in 80x24 56x24 132x43; do
            golden net "$_ns" net --dump
        done
        kill "$_nsp" 2>/dev/null || true
        wait "$_nsp" 2>/dev/null || true
        kill "$(cat "$NETO/pid")" 2>/dev/null || true
        unset DBUS_SYSTEM_BUS_ADDRESS
    else
        echo "  the kdos-net goldens are skipped (the fixture does not build)"
    fi
else
    echo "  the kdos-net goldens are skipped (no sd-bus, no dbus-daemon or net did not link)"
fi

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
              batteries energy sensors boxes; do
        # NO "SKIP IF THERE IS NO GOLDEN" HERE. That test made a page ADDED
        # to this list unreachable: it has no golden yet, so it is skipped, so
        # it never gets one, and the suite reports a clean run over a page
        # nothing has ever looked at. `res_golden` already says "no golden
        # committed" and fails, which is the right answer for a page named
        # here — being in this list IS the claim that it should have one.
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
# THE FIVE SURFACES THAT HAD NO REFERENCE FRAME, and they are the five somebody
# sees most: the run box, the yes/no dialog, the volume bezel, the toast stack
# and the desktop itself. Each draws from something outside itself — a typed
# line, a caller's message, a mixer, a bus, a home directory — which is exactly
# why none of them had a dump and exactly why each needs one: a frame that is a
# function of the machine it ran on is not a frame anybody can compare.
#
# So each dump supplies its own content. The bezel shows a fixed 60%, the toast
# stack two notifications nobody sent, and the desktop five entries no home
# directory decides; the run box and the dialog already draw from their
# arguments. `--dump-size` on the desktop is not a convenience: that surface is
# anchored to all four edges and has no size of its own at all.
#
if "$DUMPCK" --have run; then
    golden run 80x24  run --dump
    golden run 56x24  run --dump
fi
if "$DUMPCK" --have prompt; then
    golden prompt 80x24 prompt --message "End this session? Every program it started stops with it." --yes "Log out" --dump
    golden prompt 56x24 prompt --message "End this session? Every program it started stops with it." --yes "Log out" --dump
fi
if "$DUMPCK" --have osd; then
    golden osd-volume     80x24 osd --dump volume
    golden osd-brightness 80x24 osd --dump brightness
    golden osd-mic        80x24 osd --dump mic
fi
if "$DUMPCK" --have notifyd; then
    golden notifyd 80x24  notifyd --dump
    golden notifyd 56x24  notifyd --dump
fi
if "$DUMPCK" --have desk; then
    golden desk 80x24  desk --dump
    golden desk 132x43 desk --dump
fi

golden menu-system 80x24  menu system --dump
golden menu-system 56x24  menu system --dump
golden menu-system 132x43 menu system --dump
golden pick        80x24  pick --dir tree --dump
golden pick       56x24  pick --dir tree --dump
golden pick        132x43 pick --dir tree --dump
# THE PLACES RUNG, which is the half of this dialog the frame underneath cannot
# show. It is a LIST rather than a fourth column because the dialog is
# sixty-four columns and already spends its right-hand one on a preview: a
# third column leaves a file's name about thirty cells, and a chooser that
# cannot show a name is not a chooser. What the golden is FOR is that the names
# fit. The frecency half — `zoxide query -l`, whose answer is the host's own
# shell history — is skipped in a dump, so the frame is the fixture HOME's own
# directories and nothing else.
#
# AND THERE IS NO FIXTURE `places` FILE, deliberately: kdos-start reads the same
# list, so one added here to make this frame richer moved six of that surface's
# committed goldens. A fixture for one surface that changes another's frames is
# a fixture that will be blamed for the wrong thing.
golden pick-places 80x24  pick --dir tree --places --dump
golden pick-places 56x24  pick --dir tree --places --dump
# THE CONTROL CENTRE'S FRONT DOOR. Two settings goldens were committed and
# driven by nothing, so a category added to the grid left them describing a
# surface that no longer existed. A golden nothing runs is a file that agrees
# with the tree only by accident.
if "$DUMPCK" --have settings; then
    golden settings 80x24  settings --dump
    golden settings   56x24  settings --dump
    golden settings 132x43 settings --dump
    # AND TWO PAGES, because the grid is the one frame the front door draws and
    # every control this program has is behind it. These pin what the FRONT
    # DOOR cannot: the section rules that cut a page into the files it writes,
    # a slider where a number is, a dropdown where a choice is, and the row
    # order — a row inserted in the wrong block lands under the wrong heading,
    # which is a lie the compiler cannot see.
    #
    # Input is every pointing-device key and every window gesture; Desktop is
    # four stores on one page, which is the case the rules exist for.
    golden settings-input 80x24  settings --page input --dump
    golden settings-input 132x43 settings --page input --dump
    golden settings-desktop 132x43 settings --page desktop --dump
fi
# THE ACCENT PICKER. A dump carries characters and no colour, so what this
# asserts is the LAYOUT — a row per scheme, the two name columns lined up, and
# a swatch that is eight block glyphs rather than eight blanks. The colours are
# the one thing here a golden cannot hold, which is exactly why the swatch is
# drawn as blocks: a swatch of coloured spaces would be an empty rectangle in
# this file and in every terminal that cannot do colour.
#
# It also pins the width to the longest scheme name, so an accent added to
# `kcolor.h` moves this golden — which is the reminder that the window sizes
# itself from the table rather than from a constant.
#
# THE ADDRESS BOOK, AGAINST A LIST BUILT HERE. A dump cannot fork `khard`:
# neither $PATH nor an address book is fixed for one, and a frame of whoever
# ran the suite's own contacts is a frame that differs on every machine.
# `$KDOS_CONTACT_LIST` is the seam every other host-dependent surface uses, and
# the file is built in $OUT rather than under testing/fixtures/shell — a
# fixture added there to enrich one surface moves another surface's committed
# goldens.
#
# The rows carry what khard's `--parsable` carries: value, name, type, tab
# separated. One is longer than its column so the frame proves the name is CUT
# and the value is not.
#
if "$DUMPCK" --have contacts; then
    printf '%s\t%s\t%s\n' \
        'ada@example.org'     'Ada Lovelace'                 'home' \
        '+44 7700 900123'     'Ada Lovelace'                 'cell' \
        'g.hopper@example.mil' 'Grace Brewster Murray Hopper' 'work' \
        > "$OUT/contacts.txt"
    KDOS_CONTACT_LIST="$OUT/contacts.txt" \
        golden contacts 80x24 contacts --dump
    # AND THE EMPTY BOOK, which is what a machine that has never run `khard
    # new` shows. A window that said only "0" would read as a window that
    # failed to load rather than as an address book nobody has filled.
    : > "$OUT/contacts-empty.txt"
    KDOS_CONTACT_LIST="$OUT/contacts-empty.txt" \
        golden contacts-empty 80x24 contacts --dump
    if [ "${KDOS_GOLDEN_UPDATE:-0}" = 1 ] ||
       grep -q 'khard new' "$GOLD/contacts-empty-80x24.txt" 2>/dev/null; then
        echo "  an empty book says which program adds a contact"
    else
        echo "  THE EMPTY ADDRESS BOOK NAMES NO WAY TO FILL IT"
        golden_fail=1
    fi
fi

if "$DUMPCK" --have theme; then
    golden theme 80x24  theme --dump
    golden theme 132x43 theme --dump
    #
    # THE FONT PAGE, AGAINST A LIST BUILT HERE.
    #
    # A REAL ENUMERATION IS THE HOST'S FONTS and would differ on every machine
    # — the image carries one family and a developer's box carries hundreds —
    # which is the worst shape a reference frame can have. `$KDOS_FONT_LIST` is
    # the seam every other host-dependent surface already uses, and the file is
    # built in $OUT rather than under testing/fixtures/shell: a fixture added
    # there to enrich one surface moves another surface's committed goldens.
    #
    # The names carry what a real fontconfig name carries — a family with a
    # space, one with the escaped punctuation that would otherwise open a size,
    # and the size key — so the frame proves the column is CUT and the sample
    # is not.
    #
    printf '%s\n' \
        'Terminus (TTF):size=11' \
        'DejaVu Sans Mono:size=11' \
        'Noto Sans Mono CJK JP:size=11' \
        'Liberation Mono:size=11' \
        'A Family Whose Name Is Far Too Long To Fit:size=11' \
        > "$OUT/fontlist.txt"
    KDOS_FONT_LIST="$OUT/fontlist.txt" \
        golden theme-font 80x24 theme --page font --dump
    #
    # AND THE ANSWER WHERE THERE IS NOTHING TO OFFER, which is every display
    # that enumerates no face: the list is empty and the page names the file
    # and the keys that do set the font instead of drawing an empty box. An
    # empty list with no sentence reads as a list still loading. The grep
    # anchors on the two key names, not the whole sentence: the em-dash in it
    # dumps as `?` in the ascii tier.
    #
    golden theme-font-none 80x24 theme --page font --dump
    if grep -q "chrome_font and panel_font" "$GOLD/theme-font-none-80x24.txt" 2>/dev/null ||
       [ "${KDOS_GOLDEN_UPDATE:-0}" = 1 ]; then
        echo "  the font page names comp.conf when it cannot offer a font"
    else
        echo "  THE EMPTY FONT PAGE DOES NOT NAME WHERE THE FONT COMES FROM"
        golden_fail=1
    fi
fi

# THE PALETTE, WITH A FIXED QUERY. An empty one is a list of nothing in this
# fixture — XDG_DATA_DIRS points at /nonexistent, so there are no applications
# — so what the frame proves is the SHAPE: the input row, the headings in their
# fixed order, and a heading drawn only where it has rows under it.
#
# `prin` is the plan's own example and reaches two sources at once: the
# printers settings page and, on a machine with applications, kdos-print.
#
# TWO SOURCES ARE INVISIBLE HERE AND THAT IS STATED RATHER THAN LEFT TO BE
# NOTICED. dumpmain.c stubs the whole libkdisp window list to zero, so the
# WINDOWS heading can never appear in a golden; and the file source forks `fd`,
# which the dump path stops before drawing, because a golden of somebody's home
# directory is a golden of whoever ran the suite.
if "$DUMPCK" --have palette; then
    golden palette 80x24  palette --dump-query prin --dump
    # THE CHORDS SOURCE, against a query the FIXTURE actually binds: its
    # rc.xml binds W-d to kdos-launcher, so `launch` reaches a chord row
    # through its detail. A query nothing in the fixture binds would golden an
    # empty list and prove the source only by not crashing.
    golden palette-chord 80x24 palette --dump-query launch --dump
fi
# kdos-chars reads a MAPPED index, and the shipped one is six megabytes built
# from ICU — not something a golden may depend on being present, and not
# something whose frame anybody could read a diff of. This writes eight entries
# through the same header the surface reads, so the file format has one
# definition and a change to it fails here rather than goldening a surface
# reading its own garbage.
if $CC $STD $WARN -Isrc/desktop/kdos-shell -o "$OUT/mkfixidx" \
        testing/fixtures/shell/mkfixidx.c 2>/dev/null &&
   "$OUT/mkfixidx" "$OUT/charnames.idx"; then
    KDOS_GOLDEN_CHARIDX="$OUT/charnames.idx"
fi

# The Phase B surfaces, each only if it linked in. `--have` is dumpmain.c
# answering for its own weak symbols, so a surface that has not landed is a
# skip with a name on it rather than a silent gap.
# The store, over a RECORDED catalogue: what it draws is whatever the shipped
# catalogue and podman say, and both are the host's — so a golden taken without
# the fixture would hold the machine that ran it. Two rows in the recording are
# marked installed, so the `[·]` mark and the footer's count are in the picture
# as well as the ordinary case.
if "$DUMPCK" --have store; then
    _stf="$PWD/testing/fixtures/shell/store"
    golden store 80x24  store --fixture "$_stf" --dump
    golden store 56x24  store --fixture "$_stf" --dump
    golden store 132x43 store --fixture "$_stf" --dump
    # THE APPLICATION ROWS, which the groups page never shows. `Graphics` is
    # the most-populated category so it is the first category tab, and the
    # recording marks app.gimp installed — so this frame carries the `[.]` mark
    # and an untickable row, which is the whole of what the list has to get
    # right and none of which the default page reaches.
    golden store-apps 80x24  store --fixture "$_stf" --page Graphics --dump
    golden store-apps 132x43 store --fixture "$_stf" --page Graphics --dump
elif [ -f "$GOLD/store-80x24.txt" ]; then
    # A COMMITTED golden that stops being asserted is a test weakening itself
    # in response to a regression.
    echo "  store: a golden is committed but the surface no longer links"
    golden_fail=1
else
    echo "  store (skipped — not linked into the harness)"
fi

for _s in keys teams doc settings start notify trash chars; do
    if "$DUMPCK" --have "$_s"; then
        golden "$_s" 80x24  "$_s" --dump
        golden "$_s" 56x24  "$_s" --dump
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
# THE THREE-COLUMN THRESHOLD, PINNED FROM BOTH SIDES.
#
# `ST_WIDE_AT` is a hundred columns. One golden cannot tell a shape that
# changed at the right width from a shape that was always there, so the two
# below sit either side of it and differ by one column and one whole layout.
#
# AND THE FOLDED SYSTEM GROUP, OPENED — the narrow menu's second page, which
# no other frame reaches. `@toplevel` in the fixture's `menu.conf` decides
# what stays outside the fold, and this is the only frame that shows what went
# inside it.
if "$DUMPCK" --have start; then
    golden start 100x24 start --dump
    golden start 99x24  start --dump
    golden start-system 80x24 start --dump-view system --dump
fi

# EVERY LABEL IN THE SHIPPED `@toplevel` NAMES A ROW THE MENU PUSHES.
#
# A label that names no row promotes nothing and says nothing — the menu is
# deliberately forgiving there, because a preference file is not a wiring
# diagram and a typo in one must not be an error a menu reports. That is
# exactly why the typo has to be caught HERE: nothing else would ever mention
# it, and the row would simply stay behind the fold forever.
_tlbad=""
for _l in $(sed -n 's/^@toplevel[[:space:]]*=[[:space:]]*//p' \
        fs/etc/kdos/menu.conf); do
    grep -qF "push(right, &nright, \"$_l\")" \
        src/desktop/kdos-shell/start.c || _tlbad="$_tlbad $_l"
done
if [ -n "$_tlbad" ]; then
    echo "  FAIL  menu.conf @toplevel names no such menu row:$_tlbad"
    golden_fail=1
else
    echo "  ok    every @toplevel label names a right-column row"
fi

# THE ROUTES, which have no column of their own: their whole existence is a
# name to search for, so the only frame that can show one is a search. The
# fixture's `menu.conf` is a USER copy — the system file is `/etc/kdos` and no
# test host has one — so this proves the half of the merge that adds.
if "$DUMPCK" --have start; then
    golden start-route 80x24 start --dump-view search:setup --dump
fi

# THE SAVER'S ART MODE, at the one size its dump computes a position for: the
# bounce is placed against 80x24 whatever the buffer turns out to be, so a
# second size would golden a picture placed for a screen it is not on.
#
# The art is the FIXTURE'S — `config/kdos/screensaver.txt`, which is the file a
# person overrides — so this frame proves the override as well as the effect. A
# machine with no art at all falls back to the rain, which is the one thing
# here that cannot be goldened.
if "$DUMPCK" --have saver; then
    golden saver 80x24 saver --mode art --dump
    #
    # ONE GOLDEN PER EFFECT, all at 80x24 for the reason above, and all
    # settled: the dump steps a fixed count from a fixed seed before it draws,
    # because half of these have nothing on the screen at frame zero — a pipe
    # has drawn no cell yet and a fire is one hot row — and a golden of an
    # empty rectangle passes whatever the effect goes on to do.
    #
    # `bounce` is not in the list because it is not a second effect: it is the
    # name `art` goes by, one row of the table pointing at the same three
    # functions, and a golden of it would be a byte-identical copy of the one
    # above.
    #
    # `clock` IS in the list, and only because the wall clock has one reader:
    # golden() exports KDOS_PANEL_NOW and sh_wall() is what both the panel's
    # bar and the saver's face read. Without that the frame would be a
    # different picture every minute — a golden that fails an hour after it is
    # written, which is how a surface that tells the time ends up ungoldened.
    for _m in matrix pipes starfield fire clock; do
        golden "saver-$_m" 80x24 saver --mode "$_m" --dump
    done

    #
    # AND THE DEFAULT, which no golden of a named mode can reach: a run with no
    # `--mode` at all must draw the same frame `--mode art` does, or the saver
    # a machine actually starts is not the one the goldens cover.
    _svflag=$( cd testing/fixtures/shell &&
        env LC_ALL=C TZ=UTC HOME="$PWD" \
            KDOS_DUMP_SIZE=80x24 "$DUMPCK" saver --mode art --dump )
    _svdef=$( cd testing/fixtures/shell &&
        env LC_ALL=C TZ=UTC HOME="$PWD" \
            KDOS_DUMP_SIZE=80x24 "$DUMPCK" saver --dump )
    [ "$_svflag" = "$_svdef" ] \
        && echo "  saver: no --mode draws the art, which is what the goldens cover" \
        || { echo "  saver: the default is not the art the goldens cover"
             golden_fail=1; }
    # An unknown name on the COMMAND LINE is an error, because somebody typed
    # it and is watching.
    if ( cd testing/fixtures/shell &&
         env LC_ALL=C TZ=UTC HOME="$PWD" KDOS_DUMP_SIZE=80x24 \
             "$DUMPCK" saver --mode nosucheffect --dump ) >/dev/null 2>&1; then
        echo "  kdos-saver accepted a --mode it does not have"
        golden_fail=1
    else
        echo "  and an unknown --mode is refused"
    fi
fi

# THE FIRST-RUN TOUR is a flag rather than a size, so it cannot ride the loop
# above. Its four rows are generated from the same parse the list below them
# is: a chord rebound in rc.xml moves the tour in the same edit, and a step
# nothing binds is absent from the tour rather than wrong in it.
if "$DUMPCK" --have keys; then
    golden keys-first-run 80x24 keys --first-run --dump
    golden keys-first-run 56x24 keys --first-run --dump

    # THE CARD IS SEARCHABLE, and a golden of the whole card cannot show it:
    # the frame above is every row, which is what the card looks like before
    # anybody types. `work` is the plan's own example — the workspace chords
    # and nothing else — so this proves both the filter and the rule that a
    # section with no hit draws no heading.
    golden keys-search 80x24 keys --dump-query work --dump

    # AND THE SHIPPED rc.xml BINDS ALL FOUR. The fixture the goldens above
    # draw is deliberately small — it exists to catch the comment trap — so
    # its tour is two rows and proves only the drop. The file the image ships
    # is the one a first login actually reads, and the console half of this
    # is asserted against the shipped rc.xml further up.
    XDG_CONFIG_HOME=fs/etc/skel/.config KDOS_DUMP_SIZE=80x24 \
        "$DUMPCK" keys --first-run --dump > "$OUT/tour-rc.txt"
    _notour=""
    # The words are the tour's own, so they move when it does — which is how
    # repointing W-space from labwc's root menu to the palette was caught.
    for _w in "open a terminal" "search for anything" "switch workspaces" \
              "reach another terminal"; do
        grep -q "$_w" "$OUT/tour-rc.txt" || _notour="$_notour [$_w]"
    done
    if [ -z "$_notour" ]; then
        echo "  the shipped rc.xml binds all four of the tour's steps"
    else
        echo "  THE TOUR ON THE SHIPPED rc.xml WOULD DROP:$_notour"
        golden_fail=1
    fi
fi

# A DIRECTORY IS REFUSED BY THE VIEWER, and this is a refusal rather than a
# gap: a file manager inside a viewer that a file manager opened is a
# circularity, and `mc` already shows directories. The refusal has to come
# BEFORE any display is opened, or a viewer asked for a directory over ssh
# would fail on the display instead of on the argument. It names the way out,
# because a program that exits silently reads as a broken one.
if "$DUMPCK" --have peek; then
    _pkerr=$("$DUMPCK" peek testing/fixtures/shell 2>&1 >/dev/null) && _pkrc=0 ||
        _pkrc=$?
    if [ "${_pkrc:-0}" != 0 ] &&
       printf '%s' "$_pkerr" | grep -q 'is a directory'; then
        echo "  kdos-peek refuses a directory, and says where to open one"
    else
        echo "  kdos-peek did not refuse a directory: rc=${_pkrc:-0} $_pkerr"
        golden_fail=1
    fi
else
    echo "  peek (skipped — not linked into the harness)"
fi

# kdos-peek takes a FILE, so it cannot ride the loop above. The fixture is a
# committed tar built with a fixed mtime and uid: the listing draws names and
# sizes only, so the frame is the same on every machine.
# kdos-pix on a fixture folder of two: the title carries the position in the
# folder and the zoom, and a dump has no pixels — so what the frame asserts is
# the chrome and the fallback the picture leaves in its top-left cell.
if "$DUMPCK" --have pix; then
    golden pix 80x24  pix pix/one.png --dump
    golden pix        56x24  pix pix/one.png --dump
    golden pix 132x43 pix pix/one.png --dump
elif [ -f "$GOLD/pix-80x24.txt" ]; then
    echo "  pix: a golden is committed but the surface no longer links"
    golden_fail=1
else
    echo "  pix (skipped — not linked into the harness)"
fi

# kdos-find with no question drawn: the empty state is the frame a person sees
# first, and it is the one that says the field takes typing.
if "$DUMPCK" --have find; then
    golden find 80x24  find --dump /tmp
    golden find       56x24  find --dump /tmp
    golden find 132x43 find --dump /tmp
elif [ -f "$GOLD/find-80x24.txt" ]; then
    echo "  find: a golden is committed but the surface no longer links"
    golden_fail=1
else
    echo "  find (skipped — not linked into the harness)"
fi

# kdos-rec takes flags, so it cannot ride the loop above either.
#
# The first three frames differ in ONE ROW — Transcribe dim against live, and
# the subtitle naming the model against the directory that was searched — which
# is the whole proof of the greyed half. The fourth is the picture of a MOVING
# meter with a real peak label, which is the one thing the rig cannot
# photograph: --meter drains the committed tone into the ring before the single
# draw, so the chart is the arithmetic and not a screenshot of silence.
#
# --fixture points /proc at the recorded pair of PCMs, one playback-only and
# one with a capture stream, so the frame asserts the FILTER rather than the
# list. Without it the input rows would be this host's sound card.
if "$DUMPCK" --have rec; then
    golden rec        80x24  rec --fixture rec --dump
    golden rec        56x24  rec --fixture rec --dump
    golden rec        132x43 rec --fixture rec --dump
    # AN ENVIRONMENT PREFIX BINDS TO ONE COMMAND AND THE INDENTATION SAYS
    # OTHERWISE. Written as a prefix on the first line and a continuation on
    # the second, the 56x24 frame ran with NO model at all — so the golden
    # named `rec-model` was a golden of a machine that has none, which is the
    # frame `rec` already commits. Both lines carry it.
    KDOS_GOLDEN_MODEL=rec/whisper/ggml-tiny.bin \
        golden rec-model 80x24 rec --fixture rec --dump
    KDOS_GOLDEN_MODEL=rec/whisper/ggml-tiny.bin \
        golden rec-model 56x24 rec --fixture rec --dump
    golden rec-meter  80x24  rec --fixture rec --meter rec/tone.raw --dump
    golden rec-meter  56x24  rec --fixture rec --meter rec/tone.raw --dump

    # The arithmetic, against committed bytes and through the same code the
    # live meter runs. tone.raw is 8 ticks: four at half full scale, four of
    # exact zeros, so the boundary falls between ticks and no reading straddles
    # it.
    echo "==> kdos-rec: the level, from a recorded tone"
    ( cd testing/fixtures/shell &&
      "$DUMPCK" rec --meter rec/tone.raw ) > "$OUT/rec-meter.txt"
    _want="0 16384 -6 dBFS
1 16384 -6 dBFS
2 16384 -6 dBFS
3 16384 -6 dBFS
4 0 -inf dBFS
5 0 -inf dBFS
6 0 -inf dBFS
7 0 -inf dBFS"
    if [ "$(cat "$OUT/rec-meter.txt")" = "$_want" ]; then
        echo "  peak and dBFS over 8 ticks"
    else
        echo "  rec --meter DRIFTED:"
        diff -u <(printf '%s\n' "$_want") "$OUT/rec-meter.txt" | sed 's/^/    /'
        golden_fail=1
    fi

    # The WAV writer, byte-exact against a committed file: the twelve header
    # fields and both rewritten lengths are the one part of this surface a
    # reference frame cannot see.
    ( cd testing/fixtures/shell &&
      "$DUMPCK" rec --meter rec/tone.raw --write "$OUT/rec-write.wav" ) \
        > /dev/null
    if cmp -s "$OUT/rec-write.wav" \
              testing/fixtures/shell/Recordings/2026-01-01-000000.wav; then
        echo "  the 44-byte header and both length fields"
    else
        echo "  rec --write does not match the committed WAV"
        golden_fail=1
    fi
elif [ -f "$GOLD/rec-80x24.txt" ]; then
    echo "  rec: a golden is committed but the surface no longer links"
    golden_fail=1
else
    echo "  rec (skipped — not linked into the harness)"
fi

if "$DUMPCK" --have peek; then
    golden peek-archive 80x24  peek peek.tar --dump
    golden peek-archive 56x24  peek peek.tar --dump
    golden peek-archive 132x43 peek peek.tar --dump
    # Text is the OTHER half of the contract: a dump must not fork the pager,
    # so it draws what it would have done instead.
    golden peek-text 80x24 peek panelroot/0/proc/meminfo --dump
    golden peek-text  56x24 peek panelroot/0/proc/meminfo --dump
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
    # THE LAUNCHER IS THE PALETTE WITH ONE SOURCE, so it is driven with the
    # flag rather than by its name: the dump harness dispatches on the short
    # front-end name, and the basename check that turns `kdos-launcher` into
    # apps-only never sees it. Without the flag this frame would golden the
    # whole palette under the launcher's name.
    golden launcher 80x24  launcher --apps --dump
    golden launcher   56x24  launcher --apps --dump
    golden launcher 132x43 launcher --apps --dump

    # THE FILE SECTION IS OFF UNLESS ASKED FOR, and that is the assertion —
    # not that it works, which needs an index, but that a launcher with no
    # launcher.conf never reaches for one. A search a person did not ask for
    # puts their filenames on screen in front of whoever is behind them, so
    # "off by default" is the security property and it is worth a check that
    # fails if the default ever flips.
    # A `plocate` on PATH that announces itself, so "was it run" is answerable
    # without an index and without the real binary.
    mkdir -p "$OUT/nolocate"
    printf '#!/bin/sh\necho PLOCATE-WAS-RUN\n' > "$OUT/nolocate/plocate"
    chmod +x "$OUT/nolocate/plocate"
    _lqo="$OUT/launcher-files.txt"
    ( cd testing/fixtures/shell && env LC_ALL=C TZ=UTC HOME="$PWD" \
        XDG_CACHE_HOME=/nonexistent-kdos-cache \
        XDG_CONFIG_HOME="$PWD/config" \
        XDG_DATA_HOME=/nonexistent-kdos-data \
        XDG_DATA_DIRS=/nonexistent-kdos-datadirs \
        XDG_RUNTIME_DIR=/nonexistent-kdos-run \
        PATH="$OUT/nolocate:$PATH" \
        KDOS_DUMP_SIZE=80x24 "$DUMPCK" launcher --query zzq --dump ) \
        > "$_lqo" 2>&1 || true
    if grep -q "PLOCATE-WAS-RUN" "$_lqo"; then
        echo "  FAIL  the launcher searched the file index with no config asking it to"
        exit 1
    fi
    echo "  the launcher does not reach for the file index unless asked"
fi
if "$DUMPCK" --have tip; then
    golden tip 80x24  tip --dump "Firefox" "left-click opens   middle-click a new window"
    golden tip        56x24  tip --dump "Firefox" "left-click opens   middle-click a new window"
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
    golden openwith   56x24  openwith --dump "$PWD/testing/fixtures/openwith/files/roll.tar.gz"
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
    golden shell      56x2  shell --dump
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
    golden status     56x24  status --from status.tbl --dump
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
    #
    # A CONTAINER THAT QUERIES A MARKER IS NOT A RUN-OR-RAISE ROW.
    #
    # A <query identifier> is gated on `kb_have_prog` only when the container
    # begins with `Focus`, which is the run-or-raise shape. The scratchpad's
    # begins with ToggleOmnipresent and looks for an app id NO program is
    # called, so a gate that asked the same question of both would drop a key
    # that works. The fixture carries one of each and the card must show the
    # marker row and drop the program row — either half failing alone is a
    # rule that has stopped discriminating.
    #
    grep -q "the drop-down terminal, over every window" "$OUT/golden-keys-80x24.txt" \
        || { echo "  the key card dropped the scratchpad's marker row"
             exit 1; }
    grep -q "kdos-no-such-program" "$OUT/golden-keys-80x24.txt" \
        && { echo "  the key card kept a row for a program no host carries"
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
    _c_rc=0
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
          "$DUMPCK" "$@" ) > "$_c_got" 2>/dev/null || _c_rc=$?
    # Its own golden and not the run — see golden() above.
    if [ "${_c_rc:-0}" != 0 ]; then
        echo "  cells-$_c_name: the surface exited ${_c_rc}"
        _c_rc=0
        golden_fail=1
        return 0
    fi
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
#
# THE SURFACES THAT CARRY A SELECTION ARE THE ONES THAT NEED THIS MOST, and
# they were the ones without it. A selected row is the only chrome on this
# desktop whose whole job is a COLOUR — the glyphs either side of it are
# identical — so a text dump of a list is byte-identical whether the caret is
# drawn as an accent plate, as a quiet fill, or not at all. Four surfaces had
# cells and twenty-six did not.
#
# `settings` is here twice on purpose: the home grid and a PAGE are different
# selection shapes — a tile and a two-pane row — and the page is the one with a
# cold pane in it, which is the state that reads as "no caret" when it breaks.
cells_golden start          start --dump-cells
cells_golden menu-system    menu system --dump-cells
cells_golden keys           keys --dump-cells
cells_golden doc            doc --dump-cells
cells_golden settings       settings --dump-cells
cells_golden settings-input settings --page input --dump-cells
cells_golden pick           pick --dir tree --dump-cells
cells_golden find           find --dump-cells /tmp
cells_golden openwith       openwith --dump-cells "$PWD/testing/fixtures/openwith/files/roll.tar.gz"

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

#
# AND THE VERDICT AGAIN, OUTSIDE THE HARNESS'S OWN BLOCK.
#
# The two gates above are inside `if [ -n "$DUMPCK" ]`, because they read the
# frames that harness produced. The bottom-row and hint-row checks come BEFORE
# that block and read the committed frames straight off the tree, so they raise
# the same `golden_fail` on a host where the harness cannot be built — and
# without this gate that answer would be recorded and never read, and the run
# would say `all good`. A comparison whose answer nothing acts on is a
# comparison that cannot fail.
#
# kdos-term's frames and libkvt's need no gate here: term_golden exits on drift
# where it stands and vt_golden carries its own flag.
#
if [ "$golden_fail" != 0 ]; then
    echo
    echo "  A golden frame changed. If the change is intended:"
    echo "      KDOS_GOLDEN_UPDATE=1 testing/selftest.sh"
    echo "  then read the diff in git before committing it."
    exit 1
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
        -Isrc/desktop/kdos-shell -Isrc/libs/libkdisp -Isrc/libs/libkwl \
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

echo "==> the four views draw and step the way their callers rely on"
#
# `ktui_tabs`, `ktui_table`, `ktui_dropdown` and `ktui_textarea` are the widgets
# the panel surfaces were each hand-rolling. A DUMP PROVES A CHARACTER AND
# NEVER A COLOUR, so what is asserted here is what a cell grid can carry: the
# abbreviation a narrow strip falls back to, which rows a scrolled table put on
# the screen, that the selection steps OVER a heading rather than landing on
# it, and what a text block holds after a split and a join.
#
$CC $STD $WARN -o "$OUT/viewcheck" testing/fixtures/view/viewcheck.c \
    src/libs/libktui/*.c src/libs/libkcolor/*.c src/libs/libkbase/*.c \
    -Isrc/libs/libkbase -Isrc/libs/libkcolor -Isrc/libs/libktui
"$OUT/viewcheck" || exit 1

echo "==> the tone ladder gives the bar a legible middle in every accent"
#
# The eight VT slots cannot say what a raised button is: `variant` against
# `backdrop` is 1.00:1, so a panel painted in its own background colour is the
# same colour as the desktop. libkchrome derives the missing middle, and this
# is the claim that it works — in every accent, not just the one anybody
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
# THE SHIPPED mc CONFIGURATION IS MERGED INTO, NOT REPLACED. `kdos theme`
# writes one key into this file — mc keeps real user state in it — and a
# generator that rewrote it would silently drop every behaviour the image
# ships, in a file nobody re-reads.
mkdir -p "$AH/.config/mc"
cp fs/etc/skel/.config/mc/ini "$AH/.config/mc/ini"
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

_mcini="$AH/.config/mc/ini"
[ "$(grep -c '^skin=kdos$' "$_mcini")" = 1 ] || {
    echo "  kdos theme did not leave exactly one skin=kdos in mc/ini"; exit 1; }
for _k in 'use_internal_view=1' 'use_internal_edit=0' 'confirm_exit=0' \
          '\[Layout\]' 'xterm_title=1'; do
    grep -q "^$_k\$" "$_mcini" || {
        echo "  kdos theme dropped $_k from the shipped mc/ini"; exit 1; }
done
echo "  and the shipped mc/ini keeps its keys through a theme run"

# A PREVIEW IS THE STATE FILE AND THE SIGNAL AND NOTHING ELSE. It is what the
# accent picker's arrow keys run, once per keystroke, so a generator reached
# from here would put ten thousand icons and every cursor behind a cursor key.
# The check is what is NOT written: a preview into a fresh $HOME leaves the one
# state file and no configuration directory at all.
PVH="$OUT/preview-home"
rm -rf "$PVH"
mkdir -p "$PVH"
env -u XDG_CONFIG_HOME -u XDG_CACHE_HOME -u XDG_DATA_HOME HOME="$PVH" \
    "$OUT/kdos" theme --preview amber >/dev/null 2>&1 || {
    echo "  kdos theme --preview refused a real accent"; exit 1; }
[ "$(cat "$PVH/.cache/kdos/theme" 2>/dev/null)" = "amber" ] || {
    echo "  a preview did not write the accent state file"; exit 1; }
[ ! -d "$PVH/.config" ] && [ ! -d "$PVH/.local" ] && [ ! -d "$PVH/.icons" ] || {
    echo "  a preview generated artefacts; it must write the state file only"
    exit 1; }
env -u XDG_CONFIG_HOME -u XDG_CACHE_HOME -u XDG_DATA_HOME HOME="$PVH" \
    "$OUT/kdos" theme --preview nosuch >/dev/null 2>&1 && {
    echo "  a preview accepted an accent that does not exist"; exit 1; }
echo "  a preview writes the accent state file and generates nothing"

sed -i 's/#39ff14/#ff00ff/' "$AH/.config/gtk-4.0/gtk.css"
rm -f "$AH/.icons/KDOS/16x16/places/folder.svg"
echo stray > "$AH/.icons/KDOS/16x16/places/not-ours.svg"
rm -f "$AH/.icons/KDOS-cursors/cursors/left_ptr"
ln -s wait "$AH/.icons/KDOS-cursors/cursors/left_ptr"
OUT_TXT=$(audit --audit || true)
echo "$OUT_TXT" | grep -q "GTK4 palette.*DRIFTED" || { echo "  an edited stylesheet was not caught"; exit 1; }
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
