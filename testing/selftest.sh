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
INC="-Isrc/libs/libkbase -Isrc/libs/libkcolor -Isrc/libs/libktui -Isrc/libs/libkxdg -Isrc/libs/libkpkg -Isrc/libs/libkbuild -Isrc/devtools/kdos-portup -Isrc/libs/libkproc -Isrc/libs/libksig -Isrc/libs/libkpack"
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

# A PORT IS FOUND BY ITS NAME, NEVER BY A PATH SPELLED WITH ITS SHELF.
# ports/core files every port one shelf down, ports/core/<shelf>/<name>/, and
# a port changes shelf with one `git mv`: a path written out here would then
# name nothing, and every block reading a tarball out of it would skip itself
# as if the port were unfetched. Prints the directory, or nothing when no port
# carries the name; it always succeeds, so `set -e` never ends the suite on a
# lookup.
port_dir() {
    for _pd in ports/core/*/"$1"; do
        [ -f "$_pd/kpkgbuild" ] && { printf '%s\n' "$_pd"; return 0; }
    done
    return 0
}

# WHICH sd-bus THIS HOST HAS, decided ONCE and up here because blocks
# thousands of lines apart all ask. KDOS ships basu; nearly every development
# host has libsystemd, and the API is the same one. A block that reads it unset
# skips itself on every host, and the bus contracts behind it stop being
# checked without a failure.
TRAY_SDBUS=""
pkg-config --exists basu 2>/dev/null && TRAY_SDBUS=basu
[ -z "$TRAY_SDBUS" ] && pkg-config --exists libsystemd 2>/dev/null && \
    TRAY_SDBUS=libsystemd

#
# THE FLAGS FOR A libktui DRIVER, and they are $WARN minus exactly one thing.
# libktui TRUNCATES ON PURPOSE: every label it draws goes into a fixed number
# of CELLS, so a `%s` into a fixed field is the intended behaviour and
# -Wformat-truncation fires on it. Everything else in $WARN stays on, so an
# unused variable or table in a driver still fails this suite.
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

echo "==> selftest"
$CC $STD $WARN $INC -o "$OUT/selftest" src/libs/selftest.c \
    src/libs/libkbase/*.c src/libs/libkcolor/*.c src/libs/libkpkg/*.c \
    src/libs/libkbuild/*.c src/libs/libktui/*.c src/libs/libkproc/*.c \
    src/libs/libkxdg/*.c src/libs/libksig/*.c src/libs/libksig/monocypher/*.c \
    src/libs/libkpack/*.c src/devtools/kdos-portup/extract.c
ASAN_OPTIONS=detect_leaks=1 "$OUT/selftest"

echo
echo "==> every consumer still compiles against the libraries"
# kinstall compiles catalogue.c IN — the Applications page reads the catalogue
# rather than running kdos-appbox, because a live installer cannot assume
# anything is on $PATH in the target it is building. Its backend is
# kinstall-core, which the graphical installer drives too; the core is linked
# on its own, with no libktui, by the kinstall-core block below.
$CC $STD $WARN $INC -Isrc/system/kinstall-core -Isrc/system/kdos-installer \
    -Isrc/system/kdos-appbox \
    -o "$OUT/kinstall" \
    src/system/kdos-installer/*.c src/system/kinstall-core/*.c \
    src/system/kdos-appbox/catalogue.c \
    src/libs/libkbase/*.c \
    src/libs/libktui/*.c src/libs/libkcolor/*.c -lcrypt
echo "  kinstall"
# The plan resolves over the SHIPPED catalogue, and the two renderings agree.
# A disagreement between the text dump and the JSON is what makes a dump
# untrustworthy, and it is the one thing an installer's dumps must not be.
KDOS_CATALOGUE=src/system/kdos-appbox/catalogue "$OUT/kinstall" \
    --dump plan >/dev/null
KDOS_CATALOGUE=src/system/kdos-appbox/catalogue "$OUT/kinstall" \
    --dump plan --json | python3 -c 'import json,sys; d=json.load(sys.stdin);
assert d["apps"], "the plan chose no applications"
assert d["apps_bytes"] > 0, "the chosen set costs nothing"
assert d["apps_route"] in ("none","import","network","pending"), d["apps_route"]'
echo "  kinstall plan"
# An answer file's unknown key is named, and a file `--save` wrote names none:
# a key the parser drops in silence installs its default with no sign.
printf 'packs = x\napps = essential\n' > "$OUT/answers-unknown"
KDOS_CATALOGUE=src/system/kdos-appbox/catalogue "$OUT/kinstall" \
    --config "$OUT/answers-unknown" --dump plan 2>&1 >/dev/null \
    | grep -qx "kinstall: $OUT/answers-unknown: unknown key 'packs' ignored" \
    || { echo "kinstall did not name an unknown answer-file key" >&2; exit 1; }
KDOS_CATALOGUE=src/system/kdos-appbox/catalogue "$OUT/kinstall" \
    --save "$OUT/answers-saved"
if KDOS_CATALOGUE=src/system/kdos-appbox/catalogue "$OUT/kinstall" \
    --config "$OUT/answers-saved" --dump plan 2>&1 >/dev/null | grep -q 'unknown key'; then
    echo "kinstall does not read back a key its own --save writes" >&2
    exit 1
fi
echo "  kinstall answer file"
$CC $STD $WARN $INC -Isrc/system/kdos-appbox -o "$OUT/kdos-appbox" \
    src/system/kdos-appbox/*.c src/libs/libkbase/*.c src/libs/libktui/*.c \
    src/libs/libkcolor/*.c src/libs/libkxdg/*.c
echo "  kdos-appbox"
# The catalogue parser, against a fixture AND against the shipped file. The
# fixture pins the rules — base-first chains, the meta fallback, an expand that
# dedupes; the real catalogue is what a surface will actually read, and a row
# added by hand that the parser rejects is a store that opens empty with no
# error on the screen.
KDOS_CATALOGUE=testing/fixtures/catalogue/catalogue "$OUT/kdos-appbox" \
    catalogue --selftest
KDOS_CATALOGUE=src/system/kdos-appbox/catalogue "$OUT/kdos-appbox" \
    catalogue >/dev/null
KDOS_CATALOGUE=src/system/kdos-appbox/catalogue "$OUT/kdos-appbox" \
    catalogue --groups >/dev/null
# The parser's own test against a file without the fixture's rows reports each
# missing row as a failure and exits 1; a signal here is a lookup that read a
# field of a row that is not there.
rc=0
KDOS_CATALOGUE=src/system/kdos-appbox/catalogue "$OUT/kdos-appbox" \
    catalogue --selftest >/dev/null 2>&1 || rc=$?
if [ "$rc" != 1 ]; then
    echo "catalogue --selftest over the shipped file exits $rc, not 1" >&2
    exit 1
fi
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
    src/system/kdos-appbox/catalogue > "$OUT/catalogue-pinned"
KDOS_CATALOGUE="$OUT/catalogue-pinned" "$OUT/kdos-appbox" \
    install essential --dry-run >/dev/null
echo "  store"
# kdos-theme writes the wallpapers through libpng, so a host without its
# headers cannot build it at all; that is said, not skipped in silence.
THEME_PNG=""
pkg-config --exists libpng 2>/dev/null && THEME_PNG="$(pkg-config --cflags --libs libpng)"
if [ -n "$THEME_PNG" ]; then
    # shellcheck disable=SC2086
    $CC $STD $WARN $INC -Isrc/art/kdos-theme -o "$OUT/kdos-theme" \
        src/art/kdos-theme/*.c src/libs/libkbase/*.c src/libs/libkcolor/*.c \
        $THEME_PNG
    echo "  kdos-theme"
else
    echo "  kdos-theme (skipped — no libpng headers on this host)"
fi

#
# THE IDENTITY KIT HOLDS ITS FLOORS. Every KDE colour file KDOS ships comes
# out of kdos-theme, and `contrast` is what says each clears 4.5:1 for text
# and 3:1 for decorations against both backgrounds of every set. The check is
# proved able to fail before it is trusted: pdark under text — 2.42:1 on
# bone — is put back into a copy's selection and has to be refused, and so
# does a file with a role missing, because a check that can be passed by
# omission is not one. The palette numbers are read out of kcolor.h's own
# table, so the test moves with the palette instead of copying it.
#
if [ -n "$THEME_PNG" ]; then
    TK="$OUT/theme-kit"
    rm -rf "$TK"
    mkdir -p "$TK"
    # <scheme> <field>: 1 primary, 2 dim, 3 secondary, 4 urgent, 5 deep,
    # 6 text, 7 variant, 8 pdark, 9 backdrop.
    kcol_field() {
        grep -A1 "X($1," src/libs/libkcolor/kcolor.h | tail -1 |
            tr -d ' \t\\)' | cut -d, -f"$2"
    }
    hex_rgb() { printf '%d,%d,%d' "0x${1:0:2}" "0x${1:2:2}" "0x${1:4:2}"; }
    ini_get() {  # <file> <group> <key>
        awk -v g="$2" -v k="$3" '
            /^\[/ { cur = substr($0, 2, length($0) - 2); next }
            cur == g && index($0, k "=") == 1 { print substr($0, length(k) + 2) }' "$1"
    }
    # One scheme per accent, and KDOSBox: the neutral title bar kdos-appbox's
    # window rule names for a box with no accent of its own.
    _n=$("$OUT/kdos-theme" accents | wc -l)
    "$OUT/kdos-theme" kde-colors "$TK/colors"
    [ "$(find "$TK/colors" -name 'KDOS*.colors' | wc -l)" = $((_n + 1)) ] || {
        echo "  kde-colors did not write one scheme per accent and KDOSBox ($_n + 1)"; exit 1; }
    _hdr=$(ini_get "$TK/colors/KDOSBox.colors" Colors:Header BackgroundNormal)
    case $_hdr in
    *,*,*) [ "${_hdr%%,*}" = "$(echo "$_hdr" | cut -d, -f2)" ] &&
           [ "${_hdr%%,*}" = "${_hdr##*,}" ] ;;
    *) false ;;
    esac || { echo "  KDOSBox's title bar is not a grey: $_hdr"; exit 1; }
    "$OUT/kdos-theme" contrast -q "$TK/colors"/*.colors || {
        echo "  a generated colour scheme is below a contrast floor"; exit 1; }
    _rows=$("$OUT/kdos-theme" contrast "$TK/colors"/*.colors | grep -c ' ok$')
    [ "$_rows" = $(((_n + 1) * 82)) ] || {
        echo "  contrast measured $_rows rows, not 82 for each of $((_n + 1)) schemes"; exit 1; }
    _sel=$(ini_get "$TK/colors/KDOSBone.colors" Colors:Selection BackgroundNormal)
    [ "$_sel" = "$(hex_rgb "$(kcol_field bone 1)")" ] || {
        echo "  KDOSBone's selection is $_sel, not bone's primary"; exit 1; }
    echo "  the generated schemes clear every floor; the selection is the accent"
    awk -v bg="$(hex_rgb "$(kcol_field bone 8)")" -v fg="$(hex_rgb "$(kcol_field bone 6)")" '
        /^\[/ { sel = ($0 == "[Colors:Selection]") }
        sel && /^BackgroundNormal=/ { print "BackgroundNormal=" bg; next }
        sel && /^ForegroundNormal=/ { print "ForegroundNormal=" fg; next }
        { print }' "$TK/colors/KDOSBone.colors" > "$TK/dark-selection.colors"
    _out=$("$OUT/kdos-theme" contrast "$TK/dark-selection.colors" 2>/dev/null) && {
        echo "  contrast passed pdark under text on bone"; exit 1; }
    echo "$_out" | grep -q 'Colors:Selection *ForegroundNormal .*FAIL$' || {
        echo "  contrast failed the dark selection without naming the selection text"; exit 1; }
    grep -v '^ForegroundInactive=' "$TK/colors/KDOSPaper.colors" > "$TK/missing.colors"
    _out=$("$OUT/kdos-theme" contrast "$TK/missing.colors" 2>/dev/null) && {
        echo "  contrast passed a scheme with a role missing"; exit 1; }
    echo "$_out" | grep -q 'ForegroundInactive .*MISSING$' || {
        echo "  contrast did not report the missing role as missing"; exit 1; }
    "$OUT/kdos-theme" kde-colors "$TK/none" nosuch 2>/dev/null && {
        echo "  kde-colors accepted an accent the palette does not have"; exit 1; }
    echo "  and the check refuses selection text under its floor, and a missing role"
    # The greeter's block is MERGED: what other programs wrote survives, a
    # group it owns is replaced whole, the light accent becomes its dark
    # partner, and a second run changes no byte.
    printf '[General]\nfont=Noto Sans,10\nColorScheme=Old\nColorSchemeHash=abc\n\n[Dolphin]\nViewMode=2\n\n[Colors:Window]\nBackgroundNormal=1,2,3\nStale=1\n' \
        > "$TK/kdeglobals"
    "$OUT/kdos-theme" greeter-globals "$TK/kdeglobals" paper
    cp "$TK/kdeglobals" "$TK/kdeglobals.1"
    "$OUT/kdos-theme" greeter-globals "$TK/kdeglobals" paper
    cmp -s "$TK/kdeglobals" "$TK/kdeglobals.1" || {
        echo "  the greeter merge is not idempotent"; exit 1; }
    grep -qx 'ViewMode=2' "$TK/kdeglobals" && grep -qx 'font=Noto Sans,10' "$TK/kdeglobals" || {
        echo "  the greeter merge dropped a key it does not own"; exit 1; }
    grep -q '^Stale=\|=1,2,3$\|^ColorSchemeHash=' "$TK/kdeglobals" && {
        echo "  the greeter merge kept a key from a group it owns"; exit 1; }
    grep -qx 'ColorScheme=KDOSBone' "$TK/kdeglobals" || {
        echo "  the greeter did not take paper's dark partner"; exit 1; }
    "$OUT/kdos-theme" contrast -q "$TK/kdeglobals" || {
        echo "  the merged greeter kdeglobals is below a floor"; exit 1; }
    "$OUT/kdos-theme" greeter-globals --sddm "$TK/sddm"
    [ "$(find "$TK/sddm" -type f | wc -l)" = $((_n * 3)) ] || {
        echo "  greeter-globals --sddm did not write three files per accent"; exit 1; }
    grep -qx 'LookAndFeelPackage=org.kdos.bone.desktop' "$TK/sddm/paper/kdeglobals" || {
        echo "  the sddm files for paper do not name bone's global theme"; exit 1; }
    case "$(readlink "$TK/sddm/paper/background.png")" in
    /usr/share/wallpapers/KDOS-Bone/contents/images_dark/*.png) ;;
    *) echo "  the greeter background for paper is not bone's dark picture"; exit 1 ;;
    esac
    echo "  the greeter's kdeglobals merges, keeps foreign keys and is byte-stable"
    "$OUT/kdos-theme" konsole "$TK/konsole"
    grep -qx 'Name=KDOS Bone' "$TK/konsole/KDOS-Bone.profile" &&
        grep -qx 'ColorScheme=KDOS-Bone' "$TK/konsole/KDOS-Bone.profile" || {
        echo "  the Konsole profile does not name itself and its scheme"; exit 1; }
    [ "$(grep -lx 'WordModeCoding=false' "$TK/konsole"/*.profile | wc -l)" = "$_n" ] || {
        echo "  a Konsole profile leaves programming ligatures on"; exit 1; }
    [ "$(grep -c '^\[Color[0-7]\(Faint\|Intense\)\?\]$' "$TK/konsole/KDOS-Bone.colorscheme")" = 24 ] || {
        echo "  a Konsole scheme lacks one of its 24 palette entries"; exit 1; }
    "$OUT/kdos-theme" qt5ct-colors "$TK/qt5ct"
    awk -F', ' '/_colors=/ && NF != 21 { bad = 1 } END { exit bad }' "$TK/qt5ct"/*.conf || {
        echo "  a qt5ct palette does not carry 21 colours per group"; exit 1; }
    echo "  Konsole profiles name their schemes, ligatures off; qt5ct palettes carry 21 roles"
    "$OUT/kdos-theme" wallpaper "$TK/wp" paper --src src/art/kdos-wallpapers/mark.png \
        --size 64x40 --size 40x64
    for _f in images/64x40 images_dark/64x40 images/40x64 images_dark/40x64; do
        _want="${_f#*/}"
        _got=$(od -An -tu1 -j16 -N8 "$TK/wp/KDOS-Paper/contents/$_f.png" 2>/dev/null |
               awk '{ printf "%dx%d", $3 * 256 + $4, $7 * 256 + $8 }')
        [ "$_got" = "$_want" ] || {
            echo "  wallpaper $_f is ${_got:-missing}, not $_want"; exit 1; }
    done
    grep -q '"Id": "KDOS-Paper"' "$TK/wp/KDOS-Paper/metadata.json" || {
        echo "  the wallpaper package does not carry its id"; exit 1; }
    cmp -s "$TK/wp/KDOS-Paper/contents/images/64x40.png" \
           "$TK/wp/KDOS-Paper/contents/images_dark/64x40.png" && {
        echo "  the light and dark wallpaper are the same picture"; exit 1; }
    echo "  wallpaper packages carry both grounds at the sizes asked for"
    rm -rf "$TK"
fi
# libkpack (over libksig) is `kdos app update`: reading a PACKAGES index and
# picking the delta is the same parser kdos-packd uses. libkbuild's JSON reader
# alone is `kdos privacy` reading pw-dump. -ldl only for a glibc older than
# 2.34, where dlopen is not in libc; bus.c opens the sd-bus library with it.
$CC $STD $WARN $INC -Isrc/libs/libksig -Isrc/libs/libkpack \
    -Isrc/libs/libkbuild -Isrc/system/kdos-tools -o "$OUT/kdos-tools" \
    src/system/kdos-tools/*.c src/libs/libkbase/*.c src/libs/libkcolor/*.c \
    src/libs/libkpkg/*.c src/libs/libkxdg/*.c src/libs/libkproc/*.c \
    src/libs/libksig/*.c src/libs/libksig/monocypher/*.c src/libs/libkpack/*.c \
    src/libs/libkbuild/kb_json.c -ldl
echo "  kdos-tools"
$CC $STD $WARN $INC -Isrc/libs/libksig -Isrc/system/kdos-kpkg \
    -o "$OUT/kdos-kpkg" \
    src/system/kdos-kpkg/*.c src/libs/libkbase/*.c src/libs/libkpkg/*.c \
    src/libs/libksig/*.c src/libs/libksig/monocypher/*.c
echo "  kdos-kpkg"
# kdos-powerd is a root daemon and links a deliberately short list, so it
# compiles here.
#
# libkcolor IS ON kdos-powerd's LIST and that is the point of the `accent`
# verb's safety argument: the scheme name is matched against the palette's own
# closed table rather than sanitised by a character class copied into the
# daemon. Linking it here keeps this check the same link as the recipe's.
#
# Its bus face, bus.c, needs an sd-bus. Where the host has none the daemon is
# built with -DKP_NO_BUS, which is the shipped daemon on a machine whose system
# bus is down: the socket, the firewall and the accent are all still there, and
# the io.kdos.Power1 block below skips itself.
if [ -n "$TRAY_SDBUS" ]; then
    $CC $STD $WARN $INC -Isrc/libs/libkcolor -o "$OUT/kdos-powerd" \
        src/daemons/kdos-powerd/main.c src/daemons/kdos-powerd/bus.c \
        src/libs/libkbase/*.c src/libs/libkcolor/*.c \
        $(pkg-config --cflags --libs "$TRAY_SDBUS")
    echo "  kdos-powerd"
else
    $CC $STD $WARN $INC -Isrc/libs/libkcolor -DKP_NO_BUS \
        -o "$OUT/kdos-powerd" src/daemons/kdos-powerd/main.c \
        src/libs/libkbase/*.c src/libs/libkcolor/*.c
    echo "  kdos-powerd (no sd-bus on this host: the socket face alone)"
fi
ln -sf kdos-powerd "$OUT/kdos-power"

# kdos-energyd is the other root daemon, and the only one whose ANSWER can be
# checked without root: --fixture replays recorded /proc and powercap trees
# through the same sampler and ledger the daemon runs.
$CC $STD $WARN $INC -Isrc/libs/libkproc -Isrc/daemons/kdos-energyd \
    -o "$OUT/kdos-energyd" \
    src/daemons/kdos-energyd/*.c src/libs/libkbase/*.c src/libs/libkproc/*.c
ln -sf kdos-energyd "$OUT/kdos-energy"
echo "  kdos-energyd"
$CC $STD $WARN $INC -Isrc/devtools/kdosbuild -o "$OUT/kdosbuild" \
    src/devtools/kdosbuild/*.c src/libs/libkbase/*.c src/libs/libkbuild/*.c \
    src/libs/libkpkg/*.c src/libs/libktui/*.c src/libs/libkcolor/*.c
echo "  kdosbuild"
"$OUT/kdosbuild" --selftest
$CC $STD $WARN $INC -Isrc/devtools/kdos-portup -o "$OUT/kdos-portup" \
    src/devtools/kdos-portup/*.c src/libs/libkbase/*.c src/libs/libkpkg/*.c \
    src/libs/libkbuild/*.c
echo "  kdos-portup"
"$OUT/kdos-portup" --selftest --fixture testing/fixtures/portup

# kdos-packd is the sixth root daemon and the only thing in the tree that
# mounts. Its ANSWER is checkable without root: --fixture runs the same scan,
# the same solve and the same graft rules and mounts nothing, which is the seam
# `kdos-oomd` and `kdos-energyd` use too.
$CC $STD $WARN $INC -Isrc/daemons/kdos-packd -o "$OUT/kdos-packd" \
    src/daemons/kdos-packd/*.c src/libs/libkbase/*.c src/libs/libksig/*.c \
    src/libs/libksig/monocypher/*.c src/libs/libkpkg/*.c src/libs/libkpack/*.c
echo "  kdos-packd"
$CC $STD $WARN $INC -o "$OUT/kdos-pack" \
    src/system/kdos-pack/main.c src/libs/libkbase/*.c src/libs/libksig/*.c \
    src/libs/libksig/monocypher/*.c src/libs/libkpkg/*.c src/libs/libkpack/*.c
echo "  kdos-pack"

echo
echo "==> boxes: the profile says what it enforced, and what it could not"
$CC $STD $WARN $INC -Isrc/system/kdos-appbox -o "$OUT/kdos-box" \
    src/system/kdos-appbox/*.c src/libs/libkbase/*.c src/libs/libktui/*.c \
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
grep -q "audio=no gpu=yes cannot take a device away" "$BP" \
    || { echo "  FAIL  a key that cannot be enforced must say so"; cat "$BP"; exit 1; }
echo "  ok    and a key it CANNOT enforce says so rather than reporting success"
# A pack box sees its own runtime directory, so `audio = no` is enforced there
# (no PipeWire socket in it) and printed as such, not as a hope.
grep -q "audio       = no          no audio socket in its runtime directory" "$BP" \
    || { echo "  FAIL  a private runtime directory enforces audio = no"; cat "$BP"; exit 1; }
HOME="$BH" "$OUT/kdos-box" profile frozenbox > "$OUT/box2.txt" 2>&1 || true
grep -q "persistence = frozen      --rootfs …:O" "$OUT/box2.txt" \
    || { echo "  FAIL  a pack box's frozen is a throwaway overlay"; cat "$OUT/box2.txt"; exit 1; }
grep -q "! frozen is not enforced" "$OUT/box2.txt" \
    && { echo "  FAIL  a pack box's frozen IS enforced"; cat "$OUT/box2.txt"; exit 1; }
HOME="$BH" "$OUT/kdos-box" profile ephemeral > "$OUT/box2e.txt" 2>&1 || true
grep -q "persistence = ephemeral   --rootfs …:O --rm" "$OUT/box2e.txt" \
    || { echo "  FAIL  an ephemeral pack box is removed at stop"; cat "$OUT/box2e.txt"; exit 1; }
# A box over somebody else's image is distrobox's and shares the session's
# runtime directory, and its profile says what that costs.
printf 'base=image:docker.io/library/debian:trixie\nwayland=no\npersistence=ephemeral\n' \
    > "$BH/.config/kdos/boxes/foreign.conf"
HOME="$BH" "$OUT/kdos-box" profile foreign > "$OUT/box2f.txt" 2>&1 || true
grep -q "! wayland=no cannot be enforced" "$OUT/box2f.txt" \
    && grep -q "! ephemeral is not enforced" "$OUT/box2f.txt" \
    || { echo "  FAIL  a distrobox box's unenforceable keys say so"; cat "$OUT/box2f.txt"; exit 1; }
rm -f "$BH/.config/kdos/boxes/foreign.conf"
echo "  ok    an app box and a dev box differ in the profile, not in kind"

# `export` names a box's launcher with its launcher id, <box>.<stem>, the app
# id KWin gives the launch's windows, so its pin lights and its rule applies.
HOME="$BH" "$OUT/kdos-box" export arch gimp > "$OUT/box3.txt" 2>&1 || true
test -f "$BH/.local/share/applications/arch.gimp.desktop" \
    || { echo "  FAIL  an exported launcher is <box>.<stem>.desktop"; exit 1; }
test -L "$BH/.local/bin/gimp@arch" \
    || { echo "  FAIL  and its shim is <app>@<box>"; exit 1; }
grep -q 'Exec=kdos-box run -l arch.gimp arch gimp %U' "$BH/.local/share/applications/arch.gimp.desktop" \
    && grep -q '^StartupWMClass=arch.gimp$' "$BH/.local/share/applications/arch.gimp.desktop" \
    && grep -q '^X-KDOS-Box=arch$' "$BH/.local/share/applications/arch.gimp.desktop" \
    || { echo "  FAIL  the Exec line"; cat "$BH/.local/share/applications/arch.gimp.desktop"; exit 1; }
echo "  ok    a box's exported launcher is named with the app id its windows take"
HOME="$BH" "$OUT/kdos-box" unexport arch gimp > /dev/null 2>&1 || true
test ! -e "$BH/.local/share/applications/arch.gimp.desktop" \
    || { echo "  FAIL  unexport removes exactly what export added"; exit 1; }
echo "  ok    and unexport removes exactly what export added"
# The box policy's pure parts: names, launcher ids, the bus proxy's rules, the
# three confinement states and the KWin rules written into a file that is
# also the user's.
TMPDIR="$OUT" HOME="$BH" "$OUT/kdos-box" profile devbox >/dev/null 2>&1
TMPDIR="$OUT" "$OUT/kdos-appbox" boxes --selftest >/dev/null \
    || { TMPDIR="$OUT" "$OUT/kdos-appbox" boxes --selftest; exit 1; }
echo "  ok    boxes --selftest"

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

# THE INSTALLED PACK IS <id>.kpack. An install keeps the version it supersedes
# as <id>-<version>.kpack for rollback, and `-` sorts before `.`, so a store
# holding both lists the kept one unless the scan reads the name. v1 and v2
# are the two versions of v.demo assembled above.
mkdir -p "$OUT/kept"
cp "$OUT/v2.kpack" "$OUT/kept/v.demo.kpack"
cp "$OUT/v1.kpack" "$OUT/kept/v.demo-1.0.kpack"
"$OUT/kdos-packd" --fixture "$OUT/kept" > "$OUT/fxkept.txt"
if grep -qE '^  v\.demo +9\.9 ' "$OUT/fxkept.txt" &&
   [ "$(grep -cE '^  v\.demo +[0-9]' "$OUT/fxkept.txt")" = 1 ]; then
    echo "  ok    the store lists <id>.kpack, never the version kept for rollback"
else
    echo "  FAIL  the kept <id>-<version>.kpack was listed"
    sed 's/^/        /' "$OUT/fxkept.txt"; exit 1
fi

# THE GRAFT RECORD IS THE STORE'S. Two share directories over one store are
# the two slots of an A/B machine over /home/.kdos/packs: a data pack grafted
# in slot A after slot B was filled and armed must be in B's /usr/share once B
# starts, from the one record inside the store, written once per link.
GR="$OUT/graftrec"
rm -rf "$GR"
mkdir -p "$GR/store" "$GR/slot-a" "$GR/slot-b" "$GR/home"
cp "$PKS/data.tiles.kpack" "$GR/store/"
grp() { KDOS_PACK_HOME="$GR/home" KDOS_PACK_SHARE="$GR/$1" "$OUT/kdos-packd" "${@:2}"; }
grp slot-b --fixture-start "$GR/store" >/dev/null || { echo "  FAIL  a start over an empty record"; exit 1; }
grp slot-a --fixture-graft "$GR/store" data.tiles >/dev/null \
    || { echo "  FAIL  the graft in slot A"; exit 1; }
grep -qxF "data.tiles	$GR/slot-a/kdos-tiles" "$GR/store/pack-manifest" && test -L "$GR/slot-a/kdos-tiles" \
    || { echo "  FAIL  the graft is not recorded inside the store"; exit 1; }
test ! -e "$GR/slot-b/kdos-tiles" || { echo "  FAIL  slot B had the graft before it started"; exit 1; }
grp slot-b --fixture-start "$GR/store" | grep -qx 'regrafted 1' && test -L "$GR/slot-b/kdos-tiles" \
    || { echo "  FAIL  slot B did not graft the pack installed after it was armed"; exit 1; }
test "$(readlink "$GR/slot-b/kdos-tiles")" = "$(readlink "$GR/slot-a/kdos-tiles")" \
    || { echo "  FAIL  the two slots' links point at different places"; exit 1; }
grp slot-b --fixture-start "$GR/store" | grep -qx 'regrafted 0' \
    || { echo "  FAIL  a start grafted a link already in place again"; exit 1; }
grp slot-a --fixture-graft "$GR/store" data.tiles >/dev/null
test "$(grep -c "^data.tiles	" "$GR/store/pack-manifest")" = 3 \
    || { echo "  FAIL  a graft made twice is recorded twice:"; cat "$GR/store/pack-manifest"; exit 1; }
! ls "$GR/store" | grep -q '\.tmp' || { echo "  FAIL  a record write left its temporary file"; exit 1; }
echo "  ok    the graft record is the store's: a pack grafted after the arm is in the other slot when it starts"
# The other way round: one share path, as both roots spell /usr/share, that
# is slot A's directory and then slot B's. An ungraft in A removes A's link;
# B's goes when B starts, and a link B's share holds that no graft made stays.
GU="$OUT/graftdrop"
rm -rf "$GU"
mkdir -p "$GU/store" "$GU/slot-a" "$GU/slot-b" "$GU/home"
cp "$PKS/data.tiles.kpack" "$GU/store/"
gup() { ln -sfn "$1" "$GU/share"; KDOS_PACK_HOME="$GU/home" KDOS_PACK_SHARE="$GU/share" "$OUT/kdos-packd" "${@:2}"; }
gup slot-a --fixture-graft "$GU/store" data.tiles >/dev/null
gup slot-b --fixture-start "$GU/store" | grep -qx 'regrafted 1' && test -L "$GU/slot-b/kdos-tiles" \
    || { echo "  FAIL  the shared-path fixture did not graft slot B"; exit 1; }
ln -s /elsewhere "$GU/slot-b/not-ours"
gup slot-a --fixture-ungraft "$GU/store" data.tiles >/dev/null
test ! -L "$GU/slot-a/kdos-tiles" && test -L "$GU/slot-b/kdos-tiles" \
    && grep -qxF "$GU/share/kdos-tiles" "$GU/store/pack-manifest.ungrafted" \
    || { echo "  FAIL  the ungraft in slot A is not recorded for slot B"; exit 1; }
gup slot-b --fixture-start "$GU/store" | grep -qx 'regrafted 0' && test ! -L "$GU/slot-b/kdos-tiles" \
    && test -L "$GU/slot-b/not-ours" \
    || { echo "  FAIL  slot B kept the link an ungraft in slot A dropped"; exit 1; }
gup slot-b --fixture-graft "$GU/store" data.tiles >/dev/null
! grep -qxF "$GU/share/kdos-tiles" "$GU/store/pack-manifest.ungrafted" \
    || { echo "  FAIL  a graft left its path on the ungrafted list"; exit 1; }
gup slot-a --fixture-start "$GU/store" | grep -qx 'regrafted 1' && test -L "$GU/slot-a/kdos-tiles" \
    || { echo "  FAIL  a pack grafted again was not grafted in the other slot"; exit 1; }
echo "  ok    and a pack ungrafted in one slot leaves the other slot's /usr/share when that slot starts"


# kdos-boxsock is what hands a box a socket that is already stamped with a
# security context, so KWin's sandbox filter has something to filter on. It
# needs only wayland-client and one staging protocol, both of which an
# ordinary host has, so this block nearly always runs. The refusals it makes
# before it reaches a compositor are its whole contract with kdos-appbox —
# one `error <reason>` line on stdout and a non-zero exit — and they are
# checked here, because a refusal that printed nothing would leave a launch
# waiting for a socket that is never coming.
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
        src/system/kdos-boxsock/main.c "$BP/security-context-v1-protocol.c" \
        src/libs/libkbase/*.c $(pkg-config --cflags --libs wayland-client)
    echo "  kdos-boxsock"
    BSR="$OUT/bsrun"
    mkdir -p "$BSR/kdos/box"
    out=$(XDG_RUNTIME_DIR="$BSR" "$OUT/kdos-boxsock" app.x app.y 2>/dev/null) && rc=0 || rc=$?
    [ "$rc" = 1 ] && [ "$out" = "error a launcher id is the box, or differs from it by one component" ] \
        || { echo "  FAIL  kdos-boxsock refuses a launcher id outside its box ($rc: $out)"; exit 1; }
    out=$(XDG_RUNTIME_DIR="$BSR" "$OUT/kdos-boxsock" app.x app.x.a.b 2>/dev/null) && rc=0 || rc=$?
    [ "$rc" = 1 ] || { echo "  FAIL  a launcher stem may not carry a dot ($rc: $out)"; exit 1; }
    # A throwaway box, <launcher>.tw<n>, takes its application's launcher id:
    # past the name rule, it stops only at the missing private directory.
    out=$(XDG_RUNTIME_DIR="$BSR" "$OUT/kdos-boxsock" app.x.tw7 app.x 2>/dev/null) && rc=0 || rc=$?
    case "$out" in
        "error the box's private runtime directory is missing"*) ;;
        *) echo "  FAIL  a throwaway box may take its application's launcher id ($out)"; exit 1 ;;
    esac
    out=$(XDG_RUNTIME_DIR="$BSR" "$OUT/kdos-boxsock" app.x 2>/dev/null) && rc=0 || rc=$?
    case "$out" in
        "error the box's private runtime directory is missing"*) ;;
        *) echo "  FAIL  kdos-boxsock binds nothing outside the box's private directory ($out)"; exit 1 ;;
    esac
    mkdir -m 0755 "$BSR/kdos/box/app.x"
    out=$(XDG_RUNTIME_DIR="$BSR" "$OUT/kdos-boxsock" app.x 2>/dev/null) && rc=0 || rc=$?
    case "$out" in
        "error the box's private runtime directory is missing, or is not this user's alone") ;;
        *) echo "  FAIL  a private directory others can read is refused ($out)"; exit 1 ;;
    esac
    chmod 0700 "$BSR/kdos/box/app.x"
    out=$(XDG_RUNTIME_DIR="$BSR" WAYLAND_DISPLAY="$BSR/none" "$OUT/kdos-boxsock" -s tagged app.x app.x.wx 2>/dev/null) && rc=0 || rc=$?
    [ "$out" = "error cannot reach the compositor" ] \
        || { echo "  FAIL  no compositor is a refusal with its reason ($out)"; exit 1; }
    test -f "$BSR/kdos/box/.app.x.wl.wx@none.lock" \
        || { echo "  FAIL  the lock lives beside the private directory, not in it"; exit 1; }
    echo "  ok    kdos-boxsock refuses with one reason line, and its lock is outside the box"
else
    echo "  kdos-boxsock (skipped — no wayland-client or no security-context-v1.xml)"
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

echo
echo "==> kdos verify: checksums, a pack's signature, a package's sidecar"
# Dolphin's Verify action is this verb with --notify. Every answer is
# asserted by its exit status: 0 verified, 1 a check failed, 3 nothing to
# verify against. The pack is the one the pack block signed with
# $OUT/keys/builder; a pack signed by a key outside that ring is a bad
# signature, never an unsigned one. gdbus is a stub that records the
# notification instead of reaching a bus.
_vf="$OUT/verify"
rm -rf "$_vf"
mkdir -p "$_vf/bin" "$_vf/pkgkeys" "$_vf/stranger"
ln -sf kdos-tools "$OUT/kdos"
printf '#!/bin/sh\nprintf "%%s\\n" "$*" >> "$VERIFY_TOAST"\n' > "$_vf/bin/gdbus"
chmod +x "$_vf/bin/gdbus"
_venv="PATH=$_vf/bin:$PATH VERIFY_TOAST=$_vf/toast KDOS_KEYS=$OUT/keys KPKG_KEYRING=$_vf/pkgkeys"
vrc() { _rc=0; env $_venv "$OUT/kdos" verify "$@" > "$_vf/out" 2>&1 || _rc=$?; }
want_rc() { # RC WHAT
    [ "$_rc" = "$1" ] || { echo "  FAIL  $2: exit $_rc, not $1"; sed 's/^/        /' "$_vf/out"; exit 1; }
}
printf 'an image\n' > "$_vf/good.iso"
sha256sum "$_vf/good.iso" | sed "s|$_vf/||" > "$_vf/good.iso.sha256"
vrc "$_vf/good.iso"; want_rc 0 "a file its .sha256 names"
printf 'another\n' > "$_vf/renamed.iso"
sha512sum "$_vf/renamed.iso" | sed "s|$_vf/renamed.iso|download.iso|" > "$_vf/renamed.iso.sha512"
vrc "$_vf/renamed.iso"; want_rc 0 "a per-file .sha512 whose line names the file as downloaded"
printf 'tampered\n' > "$_vf/bad.iso"
printf '%s  bad.iso\n' "$(printf 'original\n' | sha256sum | cut -c1-64)" > "$_vf/SHA256SUMS"
vrc "$_vf/bad.iso"; want_rc 1 "a file SHA256SUMS lists with another hash"
grep -q 'does NOT match SHA256SUMS' "$_vf/out" || { echo "  FAIL  the mismatch is not named"; cat "$_vf/out"; exit 1; }
printf 'nothing\n' > "$_vf/plain.bin"
vrc "$_vf/plain.bin"; want_rc 3 "a file with no sidecar is not verified"
vrc "$_vf/good.iso" "$_vf/plain.bin" "$_vf/bad.iso"; want_rc 1 "the worst file decides"
vrc "$_vf/nonexistent"; want_rc 2 "a file that is not there"
echo "  ok    .sha256, .sha512 and SHA256SUMS: a match, a mismatch, no sidecar"
vrc "$PKS/app.good.kpack"; want_rc 0 "a pack signed by a key in the ring"
grep -q 'signed by key' "$_vf/out" || { echo "  FAIL  the signing key is not named"; cat "$_vf/out"; exit 1; }
cp "$PKS/app.orphan.kpack" "$_vf/forged.kpack"
"$OUT/kdos-pack" keygen "$_vf/stranger/stranger" >/dev/null
"$OUT/kdos-pack" sign "$_vf/forged.kpack" "$_vf/stranger/stranger.key" >/dev/null
vrc "$_vf/forged.kpack"; want_rc 1 "a pack signed by a key outside the ring"
cp "$PKS/app.orphan.kpack" "$_vf/unsigned.kpack"
vrc "$_vf/unsigned.kpack"; want_rc 3 "an unsigned pack is not verified, and not failed"
printf 'a package\n' > "$_vf/tiny-1.0-1.tar.xz"
cp "$_vf/stranger/stranger.pub" "$_vf/pkgkeys/"
"$OUT/kdos-kpkg" kpkg sign "$_vf/tiny-1.0-1.tar.xz" "$_vf/stranger/stranger.key" >/dev/null 2>&1 \
    || { echo "  FAIL  kpkg sign"; exit 1; }
vrc "$_vf/tiny-1.0-1.tar.xz"; want_rc 0 "a package whose .sig a trusted key made"
printf 'x' >> "$_vf/tiny-1.0-1.tar.xz"
vrc "$_vf/tiny-1.0-1.tar.xz"; want_rc 1 "a package changed under its .sig"
printf 'not ours\n' > "$_vf/plain.bin.sig"
vrc "$_vf/plain.bin"; want_rc 3 "a .sig that is not a KDOS signature is not a verdict"
mkdir -p "$_vf/rel"
printf 'a release\n' > "$_vf/rel/kdos.iso"
(cd "$_vf/rel" && sha256sum kdos.iso > SHA256SUMS)
"$OUT/kdos-kpkg" kpkg sign "$_vf/rel/SHA256SUMS" "$_vf/stranger/stranger.key" >/dev/null 2>&1 \
    || { echo "  FAIL  kpkg sign SHA256SUMS"; exit 1; }
vrc "$_vf/rel/kdos.iso"; want_rc 0 "a file a signed SHA256SUMS lists"
grep -q 'SHA256SUMS is signed by key' "$_vf/out" || { echo "  FAIL  the list's signature is not named"; cat "$_vf/out"; exit 1; }
printf '# edited\n' >> "$_vf/rel/SHA256SUMS"
vrc "$_vf/rel/kdos.iso"; want_rc 1 "a SHA256SUMS changed under its .sig"
echo "  ok    a pack's block, a package's .sig and a checksum list's: good, foreign key, changed, unsigned"
: > "$_vf/toast"
printf 'x\n' > "$_vf/-dash.bin"
printf 'x\n' > "$_vf/a<b>.bin"
vrc --notify -- "$_vf/-dash.bin" "$_vf/a<b>.bin"; want_rc 3 "two files nothing vouches for"
_i=0
while ! grep -q 'a&lt;b&gt;.bin' "$_vf/toast" && [ $_i -lt 40 ]; do
    sleep 0.05; _i=$((_i + 1))
done
grep -q -- '0 of 2 files are verified ./-dash.bin: not verified' "$_vf/toast" \
    && grep -q 'a&lt;b&gt;.bin: not verified' "$_vf/toast" \
    || { echo "  FAIL  a dash or markup in a name reached the notification as is"; cat "$_vf/toast"; exit 1; }
: > "$_vf/toast"
vrc --notify "$_vf/bad.iso"; want_rc 1 "--notify keeps the exit status"
# The notification is sent by a detached child; wait for the stub to write.
_i=0
while ! grep -q 'FAILED verification' "$_vf/toast" && [ $_i -lt 40 ]; do
    sleep 0.05; _i=$((_i + 1))
done
grep -q 'bad.iso FAILED verification' "$_vf/toast" \
    || { echo "  FAIL  the failure was not posted"; cat "$_vf/toast"; exit 1; }
echo "  ok    --notify posts the verdict"
rm -rf "$_vf"

echo
echo "==> kdos-portup fixture-backed check (offline, no network)"
# testing/fixtures/portup carries recorded upstream responses AND, under
# ports/, the recipes they were recorded against — kdos-portup reads its ports
# from there under --fixture, so bumping a live recipe cannot change what this
# replays. Seven ports: fuse (git tags under a release-asset tag prefix, whose
# newest candidate was never recorded, so UNKNOWN), zlib (a plain directory
# listing, NEWER), ca-certificates (a directory that is a meta refresh to the
# page that lists, its date pin read through the file name's separators; the
# newest file was never recorded, so NEWER at the one below it), aalib
# (SourceForge unanswered, repology genuinely CURRENT —
# upstream hasn't released since 2001), mesa (a large directory listing),
# imagemagick (repology again, but UNKNOWN: no rendered candidate — including
# its own pinned version — ever proves), and tokei (a `cachename::url`
# source whose tags come from git, NEWER). Replaying them through --fixture
# exercises discovery, pu_extract, the anchors and filters and
# pu_render_candidate end to end with no curl or git involved, and reaches
# all three outcomes (current/newer/unknown). --selftest replays one recorded
# response per adapter on top of this.
#
# --fixture makes kdos-portup skip loading AND saving
# ports/.update-cache.json entirely — fixture 200s are not evidence about the
# real world and must never outlive this process — so there is nothing to
# back up or restore around this run.
CACHE="$PWD/ports/.update-cache.json"
CACHE_BEFORE=$(md5sum "$CACHE" 2>/dev/null || true)
set +e
KDOS_PORTUP_REPO="$PWD" "$OUT/kdos-portup" --check --refresh --json \
    --fixture "$PWD/testing/fixtures/portup" \
    fuse zlib ca-certificates aalib mesa imagemagick tokei \
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
grep -q '"name": "tokei", "version": "14.0.0", "state": "newer"' "$OUT/portup-fixture.json" || {
    echo "  a cachename::url source was not proved"; exit 1; }
echo "  7 ports, all three outcomes reproduced from the recorded corpus"

echo
echo "==> kpkgdepends still agrees with the ports tree"
PORT_REPO="$PWD/ports/core $PWD/src/system $PWD/src/art" KPKG_CONF=/nonexistent \
    PKGDB_DIR=/dev/null "$OUT/kdos-kpkg" kpkgdepends bash >/dev/null
echo "  ok"

echo
echo "==> kpkg refuses a recipe whose depends outgrow the limit"
# A depends line is read whole or refused. `full` names exactly the limit and
# resolves to all of it; `wide` names one more, and the solver and the
# installer both stop on it with the message that names the recipe, before
# anything builds — a closure read short would build `wide` without its last
# dependency while phaseclosure.py, which reads every name, passed it. The
# limit is read out of kpkg.h, so the fixture follows a change to it.
DL="$OUT/deplimit"
rm -rf "$DL"
_dmax=$(sed -n 's/^#define KP_MAX_DEPS[[:blank:]]*\([0-9][0-9]*\).*/\1/p' src/libs/libkpkg/kpkg.h)
[ -n "$_dmax" ] || { echo "  KP_MAX_DEPS is not readable out of kpkg.h"; exit 1; }
_dlist=""
for _i in $(seq 0 "$_dmax"); do
    _dn=$(printf 'd%03d' "$_i")
    mkdir -p "$DL/ports/$_dn"
    printf 'name = %s\nversion = 1\nrelease = 1\n' "$_dn" > "$DL/ports/$_dn/kpkgbuild"
    : > "$DL/ports/$_dn/build.sh"
    _dlist="$_dlist $_dn"
done
mkdir -p "$DL/ports/full" "$DL/ports/wide" "$DL/work" "$DL/pkgs"
printf 'name = full\nversion = 1\nrelease = 1\ndepends =%s\n' \
    "$(printf '%s\n' $_dlist | head -n "$_dmax" | tr '\n' ' ' | sed 's/^/ /; s/ $//')" \
    > "$DL/ports/full/kpkgbuild"
printf 'name = wide\nversion = 1\nrelease = 1\ndepends =%s\n' "$_dlist" > "$DL/ports/wide/kpkgbuild"
: > "$DL/ports/full/build.sh"
: > "$DL/ports/wide/build.sh"
dl_kpkg() {
    env PORT_REPO="$DL/ports" WORK_DIR="$DL/work" PACKAGE_DIR="$DL/pkgs" \
        KPKG_CONF=/nonexistent PKGDB_DIR=/dev/null "$OUT/kdos-kpkg" "$@"
}
_dfull=$(dl_kpkg kpkgdepends full 2>"$DL/full.err") || {
    echo "  a recipe naming exactly $_dmax depends was refused:"
    sed 's/^/    /' "$DL/full.err"; exit 1; }
[ "$(printf '%s\n' $_dfull | wc -l)" = "$((_dmax + 1))" ] || {
    echo "  a recipe at the limit did not resolve to every name it lists"; exit 1; }
_dwant="wide: depends names $((_dmax + 1)) ports; the limit is $_dmax"
if dl_kpkg kpkgdepends wide >"$DL/wide.out" 2>"$DL/wide.err"; then
    echo "  kpkgdepends resolved a recipe naming $((_dmax + 1)) depends"; exit 1
fi
grep -qxF "kpkgdepends: $_dwant" "$DL/wide.err" || {
    echo "  kpkgdepends refused it without saying why:"
    sed 's/^/    /' "$DL/wide.err"; exit 1; }
if dl_kpkg kpkg install --build-only wide >"$DL/inst.out" 2>"$DL/inst.err"; then
    echo "  kpkg install built a recipe naming $((_dmax + 1)) depends"; exit 1
fi
grep -qxF "kpkg: $_dwant" "$DL/inst.err" || {
    echo "  kpkg install refused it without saying why:"
    sed 's/^/    /' "$DL/inst.err"; exit 1; }
[ -z "$(ls "$DL/pkgs")" ] || { echo "  kpkg built a package before refusing"; exit 1; }
echo "  $_dmax depends resolve whole; $((_dmax + 1)) are refused by the solver and the installer"

echo
echo "==> a game's desktop entry is what makes it reachable, and nothing checks one"
#
# A RECIPE THAT BUILDS AND AN ENTRY THAT DOES NOT WORK LOOK THE SAME. Nothing
# in this tree validates a desktop entry's contents: a `Categories=Games;`
# with the plural, a missing `Terminal=true`, or an `Exec=` naming a binary the
# recipe never installs all build, install and then land the program under
# Accessories or nowhere, with no warning at any stage. That is the whole of
# what a game port can get wrong here, so it is what this asserts. An Exec=
# is a bare command, or the full path of a program in /usr/games: that
# directory is on a login's PATH (profile.d/10-wayland.sh) but not on the
# PATH testing/entrycheck.py resolves a bare name against, so a game there is
# named by its path.
#
# The entries are read out of the build.sh heredocs rather than out of a built
# image, so this runs on any host and fails at the recipe rather than after a
# seven-minute image build.
_gamefail=0
for _p in nethack frotz bsd-games moon-buggy; do
    _bs="$(port_dir "$_p")/build.sh"
    [ -f "$_bs" ] || { echo "  $_p: no build.sh"; _gamefail=1; continue; }
    # The recipe parses AND its whole dependency closure resolves, which is
    # the same call the block above makes for bash — a `depends` naming a port
    # this tree does not have is the other way a game recipe fails late.
    PORT_REPO="$PWD/ports/core $PWD/src/system $PWD/src/art" KPKG_CONF=/nonexistent \
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
           if(w[1] ~ /\// && w[1] !~ /^\/usr\/games\/[^\/]+$/)
             printf "  ENTRY %s: Exec names a path outside /usr/games, not a command\n", n
           print "  " n " -> " w[1]
           e=0 }' "$_bs" > "$OUT/game-$_p.entries"
    grep -q "^  ENTRY " "$OUT/game-$_p.entries" && { sed -n 's/^  ENTRY/    /p' \
        "$OUT/game-$_p.entries"; _gamefail=1; }
    sed -n 's/^  \([^ ]*.*\) -> \(.*\)$/    \1 → \2/p' "$OUT/game-$_p.entries"
done
if [ "$_gamefail" = 0 ]; then
    echo "  four recipes parse; every entry is Terminal=true, Game-categorised"
    echo "  and names a bare command or a program in /usr/games"
else
    echo "  A GAME ENTRY WOULD INSTALL AND NOT BE REACHABLE"
    exit 1
fi

# AND THE ONE THING A GAME ENTRY CANNOT CLAIM WITHOUT A TYPE BEHIND IT.
# frotz's entry claims application/x-zmachine, and shared-mime-info — the
# version this image builds — defines no such type, so without the XML the
# recipe installs, the claim resolves to nothing and says so nowhere.
_smi=$(ls "$(port_dir shared-mime-info)"/shared-mime-info-*.tar.xz 2>/dev/null | head -1)
_frotz="$(port_dir frotz)/build.sh"
if grep -q 'MimeType=.*x-zmachine' "$_frotz"; then
    grep -q 'mime/packages/kdos-zmachine.xml' "$_frotz" &&
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
# umask, another time zone, XZ_OPT asking for extreme mode, a stronger check
# and the single-threaded encoder, and XZ_DEFAULTS capping memory below the
# -9 dictionary — because each of those changes the archive unless kpkg pins
# it.
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
( umask 077; kbuild_pkg TZ=Asia/Kolkata XZ_OPT='-e --check=sha256 -T1' \
      XZ_DEFAULTS=--memlimit-compress=300MiB ) \
    || { echo "  the second build failed"; exit 1; }
cmp -s "$RP/one.tar.xz" "$RP/pkgs/tiny-1.0-1.tar.xz" \
    || { echo "  the same recipe produced two different packages"; exit 1; }
# And the normalisation is visible in the archive itself, not just equal by
# luck: uid/gid 0, and the pinned epoch rather than the wall clock.
TZ=UTC tar -tvf "$RP/one.tar.xz" | grep -q "0/0 .*2025-01-01" \
    || { echo "  the archive is not normalised (uid/gid or mtime)"; exit 1; }
echo "  identical under a different umask, TZ, XZ_OPT and XZ_DEFAULTS; uid/gid 0, epoch mtime"
# The kept compressor is stated, not only stable: anyone with xz reproduces the
# package from its tar on two threads where kpkg used all of them, which is also
# the proof that the thread count does not reach the bytes. -T2 is multi-threaded
# mode on every xz release (-T+1 needs 5.4), and the reference xz gets the same
# cleared XZ_OPT and XZ_DEFAULTS as kpkg's, or the caller's shell decides.
if command -v xz >/dev/null 2>&1; then
    xz -dc "$RP/one.tar.xz" \
        | env -u XZ_OPT -u XZ_DEFAULTS xz -9 -T2 --block-size=32MiB -c \
        | cmp -s - "$RP/one.tar.xz" \
        || { echo "  the package is not xz -9 --block-size=32MiB of its tar"; exit 1; }
    echo "  the package is xz -9 --block-size=32MiB of its tar, recompressed on two threads"
fi

echo "==> kpkg refuses a staged tree the lint fails, and rolls one it passes"
# One synthetic port, its build.sh switched by LINT_CASE: each bad case must
# be refused with its own rule's message, and the good case, which holds one
# path from each allowlist, must be rolled. The ELF cases need $CC; the
# Python case needs a python3 on PATH.
LP="$OUT/lint"
rm -rf "$LP"
mkdir -p "$LP/ports/lint" "$LP/work" "$LP/pkgs"
cat > "$LP/ports/lint/kpkgbuild" <<'EOF'
name        = lint
version     = 1.0
release     = 1
description = a synthetic port whose staged tree is whatever the test asks for
homepage    = https://example.invalid/
EOF
cat > "$LP/ports/lint/build.sh" <<'EOF'
mkdir -p "$PKG/usr/share/lint"
printf 'ok\n' > "$PKG/usr/share/lint/ok"
case $LINT_CASE in
unreadable) mkdir -p "$PKG/etc"; printf 'x\n' > "$PKG/etc/lint.conf"; chmod 600 "$PKG/etc/lint.conf" ;;
noexec)     mkdir -p "$PKG/usr/bin"; printf '#!/bin/sh\n' > "$PKG/usr/bin/lint"; chmod 644 "$PKG/usr/bin/lint" ;;
world)      chmod 666 "$PKG/usr/share/lint/ok" ;;
orig)       printf 'x\n' > "$PKG/usr/share/lint/ok.orig" ;;
lib64)      mkdir -p "$PKG/usr/lib64"; printf 'x\n' > "$PKG/usr/lib64/x" ;;
usretc)     mkdir -p "$PKG/usr/etc"; printf 'x\n' > "$PKG/usr/etc/x" ;;
sdunit)     mkdir -p "$PKG/usr/lib/systemd/user"; printf '[Unit]\n' > "$PKG/usr/lib/systemd/user/x.service" ;;
x11)        mkdir -p "$PKG/etc/X11/xorg.conf.d"; printf 'x\n' > "$PKG/etc/X11/xorg.conf.d/x.conf" ;;
pc)         mkdir -p "$PKG/usr/lib/pkgconfig"
            printf 'Name: lint\nDescription: x\nVersion:\nLibs: -llint\n' > "$PKG/usr/lib/pkgconfig/lint.pc" ;;
rpath)      mkdir -p "$PKG/usr/bin"; printf 'int main(void){return 0;}\n' > m.c
            $LINT_CC -o "$PKG/usr/bin/lint" m.c -Wl,-rpath,/var/cache/kpkg/work/lint ;;
execstack)  mkdir -p "$PKG/usr/bin"; printf 'int main(void){return 0;}\n' > m.c
            $LINT_CC -o "$PKG/usr/bin/lint" m.c -Wl,-z,execstack ;;
python)     _sp=$PKG/usr/lib/python3/site-packages
            mkdir -p "$_sp/lint-1.0.dist-info" "$PKG/usr/bin/__pycache__"
            printf 'X = 1\n' > "$_sp/lint.py"
            printf '{}\n' > "$_sp/lint-1.0.dist-info/direct_url.json"
            printf 'x\n' > "$PKG/usr/bin/__pycache__/x.pyc" ;;
good)       mkdir -p "$PKG/etc" "$PKG/var/games" "$PKG/usr/bin" "$PKG/usr/lib/rust/x"
            printf 'x\n' > "$PKG/etc/shadow"; chmod 600 "$PKG/etc/shadow"
            printf '0\n' > "$PKG/var/games/tetris-bsd.scores"
            chmod 666 "$PKG/var/games/tetris-bsd.scores"
            printf 'x\n' > "$PKG/usr/lib/rust/x/Cargo.toml.orig"
            printf '#!/bin/sh\n' > "$PKG/usr/bin/lint"; chmod 755 "$PKG/usr/bin/lint"
            if [ -n "$LINT_CC" ]; then
                printf 'int main(void){return 0;}\n' > m.c
                $LINT_CC -o "$PKG/usr/bin/lint-elf" m.c -Wl,-rpath,'$ORIGIN/../lib'
            fi ;;
esac
EOF
klint() {
    ( cd "$LP/ports/lint" &&
      env PORT_REPO="$LP/ports" WORK_DIR="$LP/work" PACKAGE_DIR="$LP/pkgs" \
          PKGDB_DIR=/dev/null KPKG_CONF=/nonexistent LINT_CASE="$1" \
          LINT_CC="${2:-}" SOURCE_DATE_EPOCH=1735689600 \
          "$OUT/kpkgbuild" >"$LP/out" 2>&1 )
}
_lcc=
printf 'int main(void){return 0;}\n' > "$LP/probe.c"
${CC:-cc} -o "$LP/probe" "$LP/probe.c" 2>/dev/null && _lcc=${CC:-cc}
for _c in "unreadable:not readable by users" "noexec:a program with no x bit" \
          "world:world-writable" "orig:a patch leftover" \
          "lib64:the gcc runtime directory is usr/lib" \
          "usretc:configuration lives in /etc" "pc:an empty Version" \
          "sdunit:KDOS runs no systemd units" \
          "x11:KDOS has no X server configuration" \
          "rpath:points into a build tree" "execstack:an executable stack"; do
    _n=${_c%%:*}
    case $_n in rpath|execstack) [ -n "$_lcc" ] || continue ;; esac
    rm -f "$LP/pkgs/"*
    if klint "$_n" "$_lcc"; then
        echo "  the lint passed the '$_n' case"; cat "$LP/out"; exit 1
    fi
    grep -qF "${_c#*:}" "$LP/out" \
        || { echo "  the '$_n' case failed for another reason:"; cat "$LP/out"; exit 1; }
    [ ! -e "$LP/pkgs/lint-1.0-1.tar.xz" ] \
        || { echo "  the '$_n' case was refused but a package was written"; exit 1; }
done
rm -f "$LP/pkgs/"*
klint good "$_lcc" \
    || { echo "  the lint refused the allowlisted tree:"; cat "$LP/out"; exit 1; }
echo "  refused: modes, layout (a systemd unit, etc/X11), patch leftovers, an empty .pc Version${_lcc:+, a build-tree RPATH, an executable stack}; the allowlists pass"
if command -v python3 >/dev/null 2>&1; then
    rm -f "$LP/pkgs/"*
    klint python || { echo "  the Python case did not build:"; cat "$LP/out"; exit 1; }
    _lst=$(tar -tf "$LP/pkgs/lint-1.0-1.tar.xz")
    printf '%s\n' "$_lst" | grep -q '^\./usr/lib/python3/site-packages/__pycache__/lint\..*\.pyc$' \
        || { echo "  no bytecode was compiled into the package"; exit 1; }
    ! printf '%s\n' "$_lst" | grep -qE 'direct_url\.json|usr/bin/__pycache__' \
        || { echo "  direct_url.json or usr/bin/__pycache__ was packaged"; exit 1; }
    echo "  bytecode compiled into the package; direct_url.json and usr/bin/__pycache__ removed"
fi

echo "==> kpkg copies licences and prunes manuals and translations"
# The tree policy (prune.c) on a synthetic port with a real tarball, so the
# licence copy has a source root to read. Each path below is one rule: what
# must go, what must stay, and the two exempt manuals. A licence symlink out
# of the source is not followed, and a staged symlink at a licence's target
# is not written through. The symbol-table case
# needs $CC, objcopy and readelf, and builds a 36 MB executable beside a 36 MB
# static archive and relocatable object, which must keep theirs. The
# config.guess case needs automake's copy under /usr/share, which a bare host
# may lack.
PP="$OUT/policy"
rm -rf "$PP"
mkdir -p "$PP/ports/pol" "$PP/work" "$PP/pkgs" "$PP/t/pol-1.0/LICENSES" "$PP/t/pol-1.0/aux"
printf 'mit\n' > "$PP/t/pol-1.0/LICENSE"
printf 'gpl\n' > "$PP/t/pol-1.0/COPYING.LIB"
printf 'spdx\n' > "$PP/t/pol-1.0/LICENSES/MIT.txt"
printf 'readme\n' > "$PP/t/pol-1.0/README"
printf 'int x;\n' > "$PP/t/pol-1.0/license.c"
printf 'notice\n' > "$PP/t/pol-1.0/COPYRIGHT"
ln -s /etc/passwd "$PP/t/pol-1.0/NOTICE"
ln -s LICENSE "$PP/t/pol-1.0/LICENCE.alias"
printf "#!/bin/sh\ntimestamp='2001-01-01'\n" > "$PP/t/pol-1.0/aux/config.guess"
chmod 755 "$PP/t/pol-1.0/aux/config.guess"
( cd "$PP/t" && tar -czf "$PP/ports/pol/pol-1.0.tar.gz" pol-1.0 )
_h=$(sha256sum "$PP/ports/pol/pol-1.0.tar.gz" | cut -d' ' -f1)
cat > "$PP/ports/pol/kpkgbuild" <<POLEOF
name        = pol
version     = 1.0
release     = 1
source      = pol-1.0.tar.gz
sha256      = $_h  pol-1.0.tar.gz
description = a synthetic port the tree policy is applied to
homepage    = https://example.invalid/
license     = MIT
POLEOF
cat > "$PP/ports/pol/build.sh" <<'POLEOF'
echo "pol-stamp: $(grep '^timestamp=' aux/config.guess)"
echo "pol-license: [${license-unset}]"
for f in usr/share/locale/de/LC_MESSAGES/pol.mo usr/share/locale/en_GB/LC_MESSAGES/pol.mo \
         usr/lib/locale/fr/LC_MESSAGES/pol.mo usr/share/locale/locale.alias \
         usr/share/pol/translations/pol_de.qm usr/share/pol/translations/pol_en.qm \
         usr/share/pol/translations/pol_rgb2pct.qm usr/share/pol/translations/de.mo \
         usr/share/doc/pol/README usr/share/doc/pol/a.html usr/share/doc/pol/b.PDF \
         usr/share/doc/pol/html/x.png usr/share/doc/mednafen/index.html \
         usr/share/doc/kdos/index.html usr/share/doc/HTML/en/pol/index.docbook \
         usr/share/doc/kfs/index.html usr/share/doc/kfs/kfs.pdf \
         usr/share/doc/HTML/en/kdos/index.html \
         usr/share/doc/HTML/de/pol/index.docbook usr/share/man/man1/pol.1 \
         usr/share/man/de/man1/pol.1 usr/share/texmf-dist/doc/latex/pol/pol.pdf \
         usr/share/licenses/pol/extra.html usr/share/X11/locale/de_DE.UTF-8/Compose \
         usr/share/doc/onlyhtml/index.html \
         usr/share/doc/R/html/index.html usr/share/doc/R/html/R.css usr/share/doc/R/a.pdf \
         usr/share/doc/geany/html/index.html usr/share/doc/wireshark/wireshark.html \
         usr/lib/R/library/pol/po/fr/LC_MESSAGES/R-pol.mo \
         usr/lib/pol/locales/zh_TW.Big5/LC_MESSAGES/pol.mo usr/share/pol/org.kde.pol.mo; do
    mkdir -p "$PKG/${f%/*}"; printf 'x\n' > "$PKG/$f"
done
ln -s "$POL_OUT/escaped" "$PKG/usr/share/licenses/pol/COPYRIGHT"
mkdir -p "$PKG/usr/share/man/cat1"
if [ -n "$POL_CC" ]; then
    mkdir -p "$PKG/usr/bin" "$PKG/usr/lib"
    printf 'int big[9437184] = {1};\nint main(void){return big[0]-1;}\n' > big.c
    $POL_CC -o "$PKG/usr/bin/big" big.c
    $POL_CC -c -o big.o big.c
    ar rcs "$PKG/usr/lib/libbig.a" big.o
    cp big.o "$PKG/usr/lib/big.o"
fi
POLEOF
_pcc=
[ -n "$_lcc" ] && command -v objcopy >/dev/null 2>&1 && command -v readelf >/dev/null 2>&1 \
    && command -v ar >/dev/null 2>&1 && command -v nm >/dev/null 2>&1 && _pcc=$_lcc
( cd "$PP/ports/pol" &&
  env PORT_REPO="$PP/ports" WORK_DIR="$PP/work" PACKAGE_DIR="$PP/pkgs" \
      SOURCE_DIR=/nonexistent PKGDB_DIR=/dev/null KPKG_CONF=/nonexistent \
      SOURCE_DATE_EPOCH=1735689600 POL_CC="$_pcc" POL_OUT="$PP" \
      "$OUT/kpkgbuild" >"$PP/out" 2>&1 ) \
    || { echo "  the policy port did not build:"; cat "$PP/out"; exit 1; }
_lst=$(tar -tf "$PP/pkgs/pol-1.0-1.tar.xz")
for _p in usr/share/licenses/pol/LICENSE usr/share/licenses/pol/COPYING.LIB \
          usr/share/licenses/pol/LICENSES/MIT.txt usr/share/licenses/pol/extra.html \
          usr/share/locale/en_GB/LC_MESSAGES/pol.mo usr/share/locale/locale.alias \
          usr/share/pol/translations/pol_en.qm usr/share/pol/translations/pol_rgb2pct.qm \
          usr/share/doc/pol/README usr/share/doc/mednafen/index.html \
          usr/share/doc/kdos/index.html usr/share/doc/HTML/en/pol/index.docbook \
          usr/share/doc/kfs/index.html usr/share/doc/kfs/kfs.pdf \
          usr/share/doc/HTML/en/kdos/index.html \
          usr/share/man/man1/pol.1 usr/share/X11/locale/de_DE.UTF-8/Compose \
          usr/share/licenses/pol/LICENCE.alias usr/share/pol/org.kde.pol.mo \
          usr/share/doc/R/html/index.html usr/share/doc/R/html/R.css \
          usr/share/doc/geany/html/index.html usr/share/doc/wireshark/wireshark.html; do
    printf '%s\n' "$_lst" | grep -qxF "./$_p" \
        || { echo "  $_p is missing from the package"; exit 1; }
done
for _p in usr/share/licenses/pol/README usr/share/locale/de/ usr/lib/locale/ \
          usr/share/pol/translations/pol_de.qm usr/share/pol/translations/de.mo \
          usr/share/doc/pol/a.html usr/share/doc/pol/b.PDF usr/share/doc/pol/html/ \
          usr/share/doc/HTML/de/ usr/share/man/de/ usr/share/man/cat1/ \
          usr/share/texmf-dist/ usr/share/doc/onlyhtml/ usr/lib/R/ \
          usr/lib/pol/ usr/share/licenses/pol/license.c usr/share/licenses/pol/NOTICE \
          usr/share/doc/R/a.pdf; do
    ! printf '%s\n' "$_lst" | grep -qxF "./$_p" \
        || { echo "  $_p was packaged"; exit 1; }
done
[ ! -e "$PP/escaped" ] \
    || { echo "  a licence was written through a symlink out of \$PKG"; exit 1; }
grep -qxF 'pol-license: [unset]' "$PP/out" \
    || { echo "  the license key reached build.sh as \$license"; exit 1; }
echo "  licences copied; HTML, PDF and non-English catalogs, handbooks and man pages pruned; English, the HTML programs open and both books kept"
if [ -n "$_pcc" ]; then
    rm -rf "$PP/x"; mkdir -p "$PP/x"
    tar -xf "$PP/pkgs/pol-1.0-1.tar.xz" -C "$PP/x" ./usr/bin/big ./usr/lib/libbig.a ./usr/lib/big.o
    ! readelf -SW "$PP/x/usr/bin/big" | grep -qE ' \.symtab | \.strtab ' \
        || { echo "  the 36 MB executable kept its symbol table"; exit 1; }
    readelf -SW "$PP/x/usr/bin/big" | grep -q ' \.note' \
        || { echo "  the 36 MB executable lost its notes"; exit 1; }
    readelf -SW "$PP/x/usr/lib/big.o" | grep -q ' \.symtab ' \
        || { echo "  a relocatable object lost its symbol table"; exit 1; }
    nm "$PP/x/usr/lib/libbig.a" 2>/dev/null | grep -q ' T main$' \
        || { echo "  a static archive lost its symbol table"; exit 1; }
    echo "  .symtab removed from a 36 MB executable; the static archive and the object keep theirs"
fi
_am=$(ls /usr/share/automake-*/config.guess 2>/dev/null | head -1)
if [ -n "$_am" ]; then
    ! grep -qF "pol-stamp: timestamp='2001-01-01'" "$PP/out" \
        || { echo "  the 2001 config.guess reached build.sh"; exit 1; }
    echo "  a stale config.guess is replaced by automake's before build.sh"
fi

echo "==> kpkg install compresses cheaply only what it deletes"
KI="$OUT/kinst"
rm -rf "$KI"; mkdir -p "$KI/work" "$KI/pkgs" "$KI/root"
ln -sf kdos-kpkg "$OUT/kpkg"
for t in kpkgbuild kpkgadd kpkgdepends; do ln -sf kdos-kpkg "$OUT/$t"; done
kinst() {
    env PORT_REPO="$RP/ports" WORK_DIR="$KI/work" PACKAGE_DIR="$KI/pkgs" \
        PKGDB_DIR=/db KPKG_CONF=/nonexistent SOURCE_DATE_EPOCH=1735689600 \
        TZ=UTC "$@" "$OUT/kpkg" install --root "$KI/root" tiny
}
# A logging xz in front of the real one records which preset each build used.
if _xz=$(command -v xz); then
    mkdir -p "$KI/bin"
    printf '#!/bin/sh\necho "$*" >> "%s/xzargs"\nexec %s "$@"\n' "$KI" "$_xz" \
        > "$KI/bin/xz"
    chmod +x "$KI/bin/xz"
fi
kinst PATH="$KI/bin:$PATH" >"$KI/log" 2>&1 \
    || { echo "  kpkg install of the synthetic port failed"; cat "$KI/log"; exit 1; }
if [ -n "$_xz" ]; then
    grep -qx -- '-0 -T0 --block-size=8MiB --no-adjust' "$KI/xzargs" \
        || { echo "  a deleted package was not given the transient preset"; exit 1; }
fi
test "$(cat "$KI/root/usr/share/tiny/b.txt" 2>/dev/null)" = two \
    || { echo "  the transient package did not install its files"; exit 1; }
test -z "$(ls -A "$KI/pkgs")" \
    || { echo "  a package was left behind with no cache kept"; exit 1; }
grep -q '^==> Packaged tiny: [0-9]* MB in [0-9.]* s$' "$KI/log" \
    || { echo "  no packaging time was reported"; exit 1; }
if command -v xz >/dev/null 2>&1; then
    rm -rf "$KI/root"; mkdir -p "$KI/root"
    kinst KPKG_KEEP_CACHE=1 >/dev/null 2>&1 \
        || { echo "  kpkg install with a kept cache failed"; exit 1; }
    xz -lvv "$KI/pkgs/tiny-1.0-1.tar.xz" | grep -q 'dict=64MiB' \
        || { echo "  a kept package is not compressed with the -9 dictionary"; exit 1; }
    cmp -s "$KI/pkgs/tiny-1.0-1.tar.xz" "$RP/one.tar.xz" \
        || { echo "  kpkg install's kept package differs from kpkgbuild's"; exit 1; }
    echo "  installed from a deleted cheap package; a kept one is kpkgbuild's -9 bytes"
else
    echo "  installed from a deleted cheap package (kept check skipped — no xz)"
fi

echo

# ── The shared indexes follow the manifest, both ways ──────────────────────
#
# A MIME XML defines no type and an info page has no menu entry until an index
# built from every package's copy is rebuilt. kpkgadd and kpkgdel rebuild the
# ones whose directory the manifest touched; the stubs below record what they
# were asked to do, so the test needs neither tool.
echo "==> kpkg rebuilds the shared indexes a package feeds"
TR="$OUT/trig"
rm -rf "$TR"; mkdir -p "$TR/ports/feed" "$TR/work" "$TR/pkgs" "$TR/root" "$TR/bin"
cat > "$TR/ports/feed/kpkgbuild" <<'EOF'
name        = feed
version     = 1.0
release     = 1
description = a synthetic port whose files feed two shared indexes
EOF
cat > "$TR/ports/feed/build.sh" <<'EOF'
install -Dm644 /dev/null "$PKG/usr/share/mime/packages/feed.xml"
install -Dm644 /dev/null "$PKG/usr/share/info/feed.info"
printf 'stale\n' > "$PKG/usr/share/info/dir"
install -Dm644 /dev/null "$PKG/etc/udev/hwdb.d/60-feed.hwdb"
install -Dm644 /dev/null "$PKG/usr/share/man/man1/feed.1"
install -Dm644 /dev/null "$PKG/etc/fonts/conf.d/60-feed.conf"
install -Dm644 /dev/null "$PKG/usr/share/texmf-dist/tex/latex/feed/feed.sty"
install -Dm644 /dev/null "$PKG/usr/share/applications/feed.desktop"
install -Dm644 /dev/null "$PKG/usr/share/icons/feedtheme/index.theme"
install -Dm644 /dev/null "$PKG/usr/share/icons/feedtheme/16x16/apps/feed.png"
install -Dm644 /dev/null "$PKG/usr/share/icons/bare/16x16/apps/feed.png"
EOF
mkdir -p "$TR/ports/page"
cat > "$TR/ports/page/kpkgbuild" <<'EOF'
name        = page
version     = 1.0
release     = 1
description = a synthetic port that only adds a manual page
EOF
cat > "$TR/ports/page/build.sh" <<'EOF'
install -Dm644 /dev/null "$PKG/usr/share/man/man1/page.1"
EOF
mkdir -p "$TR/ports/icontool"
cat > "$TR/ports/icontool/kpkgbuild" <<'EOF'
name        = icontool
version     = 1.0
release     = 1
description = a synthetic port that carries the icon cache tool
EOF
cat > "$TR/ports/icontool/build.sh" <<'EOF'
install -Dm755 /dev/null "$PKG/usr/bin/gtk-update-icon-cache"
EOF
mkdir -p "$TR/ports/vlcplug"
cat > "$TR/ports/vlcplug/kpkgbuild" <<'EOF'
name        = vlcplug
version     = 1.0
release     = 1
description = a synthetic port that carries a VLC plugin and the plugin cache tool
EOF
cat > "$TR/ports/vlcplug/build.sh" <<'EOF'
install -Dm755 /dev/null "$PKG/usr/lib/vlc/vlc-cache-gen"
install -Dm755 /dev/null "$PKG/usr/lib/vlc/plugins/misc/libfeed_plugin.so"
EOF
# The MIME and desktop-database stubs leave a generated file behind, as the
# real tools do: that is what keeps each directory alive after the last XML or
# desktop entry is removed. The makewhatis stub writes the database, which is
# what the trigger measures.
for t in update-mime-database install-info udevadm makewhatis fc-cache mktexlsr \
         update-desktop-database gtk-update-icon-cache; do
    printf '#!/bin/sh\necho "%s $*" >> "%s/calls"\n' "$t" "$TR" > "$TR/bin/$t"
    chmod +x "$TR/bin/$t"
done
printf ': > "$1/globs"\n' >> "$TR/bin/update-mime-database"
printf ': > "$2/mimeinfo.cache"\n' >> "$TR/bin/update-desktop-database"
printf ': > "$4/icon-theme.cache"\n' >> "$TR/bin/gtk-update-icon-cache"
printf 'd=$1; [ "$1" = -d ] && d=$2; echo db > "$d/mandoc.db"\n' \
    >> "$TR/bin/makewhatis"
ktrig() {
    env PATH="$TR/bin:$PATH" PORT_REPO="$TR/ports" WORK_DIR="$TR/work" \
        PACKAGE_DIR="$TR/pkgs" PKGDB_DIR=/db KPKG_CONF=/nonexistent \
        SOURCE_DATE_EPOCH=1735689600 TZ=UTC "$@" 2>&1
}
for t in kpkgbuild kpkgadd kpkgdel; do ln -sf kdos-kpkg "$OUT/$t"; done
( cd "$TR/ports/feed" && ktrig "$OUT/kpkgbuild" >/dev/null ) \
    || { echo "  the synthetic port did not build"; exit 1; }
( cd "$TR/ports/page" && ktrig "$OUT/kpkgbuild" >/dev/null ) \
    || { echo "  the synthetic page port did not build"; exit 1; }
( cd "$TR/ports/icontool" && ktrig "$OUT/kpkgbuild" >/dev/null ) \
    || { echo "  the synthetic icon tool port did not build"; exit 1; }
ktrig "$OUT/kpkgadd" --root "$TR/root" "$TR/pkgs/feed-1.0-1.tar.xz" >/dev/null \
    || { echo "  the synthetic package did not install"; exit 1; }
tar -tf "$TR/pkgs/feed-1.0-1.tar.xz" | grep -q 'usr/share/info/dir$' \
    && { echo "  the package carries an info dir file every such package claims"; exit 1; }
grep -qx "update-mime-database $TR/root/usr/share/mime" "$TR/calls" \
    || { echo "  installing a MIME XML did not regenerate the MIME database"; exit 1; }
grep -qx "install-info --info-dir=$TR/root/usr/share/info $TR/root/usr/share/info/feed.info" "$TR/calls" \
    || { echo "  installing an info page did not regenerate the info dir"; exit 1; }
grep -qx "udevadm hwdb --update --root $TR/root" "$TR/calls" \
    || { echo "  installing a hwdb.d file did not recompile the hwdb trie"; exit 1; }
grep -qx "fc-cache -s --sysroot $TR/root" "$TR/calls" \
    || { echo "  installing fontconfig configuration did not rebuild the font cache"; exit 1; }
grep -qx "makewhatis $TR/root/usr/share/man" "$TR/calls" \
    || { echo "  a first manual page did not build the whole manual index"; exit 1; }
grep -qx "mktexlsr --quiet $TR/root/usr/share/texmf-dist" "$TR/calls" \
    || { echo "  installing a TeX file did not rebuild ls-R, or named a tree that does not exist"; exit 1; }
grep -qx "update-desktop-database -q $TR/root/usr/share/applications" "$TR/calls" \
    || { echo "  installing a desktop entry did not rebuild mimeinfo.cache"; exit 1; }
grep -qx "gtk-update-icon-cache -q -t -f $TR/root/usr/share/icons/feedtheme" "$TR/calls" \
    || { echo "  installing into an icon theme did not rebuild its icon-theme.cache"; exit 1; }
grep -q "gtk-update-icon-cache.*/bare" "$TR/calls" \
    && { echo "  an icon directory with no index.theme was given a cache"; exit 1; }
[ "$(stat -c %Y "$TR/root/usr/share/icons/feedtheme/16x16")" -le 1735689600 ] \
    || { echo "  an icon theme's directory mtime was not clamped to SOURCE_DATE_EPOCH"; exit 1; }
: > "$TR/calls"
ktrig "$OUT/kpkgadd" --root "$TR/root" "$TR/pkgs/page-1.0-1.tar.xz" >/dev/null \
    || { echo "  the synthetic page package did not install"; exit 1; }
grep -qx "makewhatis -d $TR/root/usr/share/man man1/page.1" "$TR/calls" \
    || { echo "  an added manual page was not merged into the existing index"; exit 1; }
: > "$TR/calls"
ktrig "$OUT/kpkgadd" --root "$TR/root" "$TR/pkgs/icontool-1.0-1.tar.xz" >/dev/null \
    || { echo "  the synthetic icon tool package did not install"; exit 1; }
grep -qx "gtk-update-icon-cache -q -t -f $TR/root/usr/share/icons/feedtheme" "$TR/calls" \
    || { echo "  installing the icon cache tool did not cache a theme placed before it"; exit 1; }
: > "$TR/calls"
ktrig "$OUT/kpkgdel" --root "$TR/root" feed >/dev/null
grep -qx "update-mime-database $TR/root/usr/share/mime" "$TR/calls" \
    || { echo "  removing a MIME XML left the MIME database naming its types"; exit 1; }
grep -qx "udevadm hwdb --update --root $TR/root" "$TR/calls" \
    || { echo "  removing a hwdb.d file left its entries in the trie"; exit 1; }
grep -qx "makewhatis $TR/root/usr/share/man" "$TR/calls" \
    || { echo "  removing a manual page did not rebuild the whole manual index"; exit 1; }
grep -qx "update-desktop-database -q $TR/root/usr/share/applications" "$TR/calls" \
    || { echo "  removing a desktop entry left mimeinfo.cache naming it"; exit 1; }
[ -e "$TR/root/usr/share/icons/feedtheme" ] \
    && { echo "  removing a theme's last file left its icon-theme.cache and directory"; exit 1; }
mkdir -p "$TR/root2"; : > "$TR/calls"
ktrig KPKG_SKIP_INDEX=man "$OUT/kpkgadd" --root "$TR/root2" "$TR/pkgs/page-1.0-1.tar.xz" \
    > "$TR/skip.log" || { echo "  the page package did not install with the index skipped"; exit 1; }
grep -q 'index skipped: man' "$TR/skip.log" \
    || { echo "  KPKG_SKIP_INDEX=man did not say it skipped the manual index"; exit 1; }
[ -e "$TR/root2/usr/share/man/mandoc.db" ] || grep -q makewhatis "$TR/calls" \
    && { echo "  KPKG_SKIP_INDEX=man still ran makewhatis"; exit 1; }
# vlc-cache-gen runs inside the root, so as a user the trigger can only say it
# needs root; as root it runs the stub, which is not a program.
( cd "$TR/ports/vlcplug" && ktrig "$OUT/kpkgbuild" >/dev/null ) \
    || { echo "  the synthetic VLC plugin port did not build"; exit 1; }
mkdir -p "$TR/root3"
ktrig "$OUT/kpkgadd" --root "$TR/root3" "$TR/pkgs/vlcplug-1.0-1.tar.xz" > "$TR/vlc.log" \
    || { echo "  the synthetic VLC plugin package did not install"; exit 1; }
if [ "$(id -u)" = 0 ]; then
    grep -q 'Updating index: /usr/lib/vlc/vlc-cache-gen' "$TR/vlc.log"
else
    grep -q "vlc-cache-gen under $TR/root3 needs root" "$TR/vlc.log"
fi || { echo "  installing a VLC plugin did not rebuild plugins.dat"; exit 1; }
: > "$TR/root3/usr/lib/vlc/plugins/plugins.dat"
ktrig "$OUT/kpkgdel" --root "$TR/root3" vlcplug >/dev/null
[ -e "$TR/root3/usr/lib/vlc" ] \
    && { echo "  removing vlc-cache-gen left plugins.dat and its directories"; exit 1; }
echo "  install and removal rebuild the MIME database, info dir, hwdb, font cache, manual index, ls-R, desktop database, icon caches and VLC plugin cache"

echo

# ── One file, two spellings, one owner; one fonts.dir per directory ────────
#
# On a merged-/usr root `./bin/x` and `./usr/bin/x` are the same file. An
# upgrade that moves a path from one spelling to the other must not delete the
# file it just placed, and neither an upgrade nor a removal may delete a file
# another package claims. And two font packages sharing `misc/` each carry
# none of the fonts.dir that lists both: kpkgadd writes it from the directory.
echo "==> kpkg ownership follows merged /usr, and fonts.dir follows the directory"
MU="$OUT/mergedusr"
rm -rf "$MU"; mkdir -p "$MU/work" "$MU/pkgs" "$MU/root/usr/bin" "$MU/root/usr/share/fonts" "$MU/bin"
ln -s usr/bin "$MU/root/bin"
mkport() {
    mkdir -p "$MU/ports/$1"
    printf 'name        = %s\nversion     = %s\nrelease     = 1\ndescription = synthetic\n' \
        "$1" "$2" > "$MU/ports/$1/kpkgbuild"
    printf '%s\n' "$3" > "$MU/ports/$1/build.sh"
    ( cd "$MU/ports/$1" && kmu "$OUT/kpkgbuild" >/dev/null ) \
        || { echo "  the synthetic port $1-$2 did not build"; exit 1; }
}
printf '#!/bin/sh\necho "mkfontdir $*" >> "%s/calls"\n: > "$1/fonts.dir"\n' "$MU" > "$MU/bin/mkfontdir"
printf '#!/bin/sh\n' > "$MU/bin/fc-cache"
chmod +x "$MU/bin/mkfontdir" "$MU/bin/fc-cache"
kmu() {
    env PATH="$MU/bin:$PATH" PORT_REPO="$MU/ports" WORK_DIR="$MU/work" \
        PACKAGE_DIR="$MU/pkgs" PKGDB_DIR=/db KPKG_CONF=/nonexistent \
        SOURCE_DATE_EPOCH=1735689600 TZ=UTC "$@" 2>&1
}
mkport mover 1.0 'install -Dm755 /dev/null "$PKG/bin/mover"'
kmu "$OUT/kpkgadd" --root "$MU/root" "$MU/pkgs/mover-1.0-1.tar.xz" >/dev/null
rm -rf "$MU/ports/mover"
mkport mover 2.0 'install -Dm755 /dev/null "$PKG/usr/bin/mover"'
kmu "$OUT/kpkgadd" --root "$MU/root" "$MU/pkgs/mover-2.0-1.tar.xz" >/dev/null
[ -e "$MU/root/usr/bin/mover" ] \
    || { echo "  an upgrade from ./bin/x to ./usr/bin/x deleted the file it placed"; exit 1; }
mkport short 1.0 'install -Dm755 /dev/null "$PKG/usr/bin/shared"'
kmu "$OUT/kpkgadd" --root "$MU/root" "$MU/pkgs/short-1.0-1.tar.xz" >/dev/null
mkport long 1.0 'install -Dm755 /dev/null "$PKG/bin/shared"'
kmu "$OUT/kpkgadd" --root "$MU/root" "$MU/pkgs/long-1.0-1.tar.xz" 2>&1 \
    | grep -q "File conflict" \
    || { echo "  a path claimed under its other spelling was not a conflict"; exit 1; }
printf 'replaces    = other short\n' >> "$MU/ports/long/kpkgbuild"
kmu "$OUT/kpkgadd" --root "$MU/root" "$MU/pkgs/long-1.0-1.tar.xz" | grep -q "Taking 1 path from short" \
    || { echo "  a takeover declared in replaces = was refused or not reported"; exit 1; }
grep -q 'shared' "$MU/root/db/short" \
    && { echo "  a declared takeover left the path in the old owner's manifest"; exit 1; }
printf './usr/bin/shared\n' >> "$MU/root/db/short"
kmu "$OUT/kpkgdel" --root "$MU/root" short >/dev/null
[ -e "$MU/root/usr/bin/shared" ] \
    || { echo "  removing one of two claimants deleted the other's file"; exit 1; }
mkport fonta 1.0 'install -Dm644 /dev/null "$PKG/usr/share/fonts/misc/a.pcf.gz"
: > "$PKG/usr/share/fonts/misc/fonts.dir"'
mkport fontb 1.0 'install -Dm644 /dev/null "$PKG/usr/share/fonts/misc/b.pcf.gz"
: > "$PKG/usr/share/fonts/misc/fonts.dir"'
tar -tf "$MU/pkgs/fonta-1.0-1.tar.xz" | grep -q 'fonts.dir$' \
    && { echo "  a font package carries the fonts.dir its directory shares"; exit 1; }
kmu "$OUT/kpkgadd" --root "$MU/root" "$MU/pkgs/fonta-1.0-1.tar.xz" >/dev/null
kmu "$OUT/kpkgadd" --root "$MU/root" "$MU/pkgs/fontb-1.0-1.tar.xz" 2>&1 \
    | grep -q "File conflict" \
    && { echo "  two font packages sharing misc/ conflict"; exit 1; }
grep -qx "mkfontdir $MU/root/usr/share/fonts/misc" "$MU/calls" \
    || { echo "  installing a bitmap face did not rebuild fonts.dir"; exit 1; }
kmu "$OUT/kpkgdel" --root "$MU/root" fonta fontb >/dev/null
[ -e "$MU/root/usr/share/fonts/misc" ] \
    && { echo "  removing the last face left misc/ and its fonts.dir behind"; exit 1; }
echo "  one owner under either spelling; fonts.dir written from the directory and removed with it"

# One extraction is both the install and the manifest; ownership is read only
# for staged paths already on disk; the writer lock serialises two installers.
echo "==> kpkgadd lists what it extracted, and two installers share one database"
mkport hooked 1.0 'install -Dm644 /dev/null "$PKG/usr/share/hooked/a b.txt"
ln -s "a b.txt" "$PKG/usr/share/hooked/link"
ln "$PKG/usr/share/hooked/a b.txt" "$PKG/usr/share/hooked/hard"'
printf 'echo hooked-ran\n' > "$MU/ports/hooked/postinstall.sh"
( cd "$MU/ports/hooked" && kmu "$OUT/kpkgbuild" >/dev/null ) \
    || { echo "  the synthetic port hooked did not build"; exit 1; }
tar -tf "$MU/pkgs/hooked-1.0-1.tar.xz" | grep -qx './.POSTINSTALL' \
    || { echo "  the fixture carries no hook to leave out"; exit 1; }
kmu "$OUT/kpkgadd" --root "$MU/root" "$MU/pkgs/hooked-1.0-1.tar.xz" | grep -q hooked-ran \
    || { echo "  the hooked package did not install or run its hook"; exit 1; }
tar --quoting-style=literal -tf "$MU/pkgs/hooked-1.0-1.tar.xz" | grep -vx './.POSTINSTALL' > "$MU/want"
tail -n +2 "$MU/root/db/hooked" | cmp -s - "$MU/want" \
    || { echo "  the database entry is not tar -tf minus ./.POSTINSTALL"; exit 1; }
install -Dm644 /dev/null "$MU/root/usr/share/stray/left"
mkport stray 1.0 'install -Dm644 /dev/null "$PKG/usr/share/stray/left"'
kmu "$OUT/kpkgadd" --root "$MU/root" "$MU/pkgs/stray-1.0-1.tar.xz" >/dev/null \
    || { echo "  a file no package claims was refused instead of adopted"; exit 1; }
grep -qx './usr/share/stray/left' "$MU/root/db/stray" \
    || { echo "  the adopted file is not in its new owner's entry"; exit 1; }
mkport gone 1.0 'install -Dm644 /dev/null "$PKG/usr/share/gone/f"'
kmu "$OUT/kpkgadd" --root "$MU/root" "$MU/pkgs/gone-1.0-1.tar.xz" >/dev/null
rm "$MU/root/usr/share/gone/f"
mkport taker 1.0 'install -Dm644 /dev/null "$PKG/usr/share/gone/f"'
kmu "$OUT/kpkgadd" --root "$MU/root" "$MU/pkgs/taker-1.0-1.tar.xz" 2>&1 \
    | grep -q "usr/share/gone/f (owned by gone)" \
    || { echo "  a claimed path deleted from the root was taken without a conflict"; exit 1; }
mkport taker2 1.0 'install -Dm644 /dev/null "$PKG/usr/share/taker2/f"'
kmu "$OUT/kpkgadd" --root "$MU/root" "$MU/pkgs/taker2-1.0-1.tar.xz" >/dev/null
rm -rf "$MU/ports/taker2"
mkport taker2 2.0 'install -Dm644 /dev/null "$PKG/usr/share/taker2/f"
install -Dm644 /dev/null "$PKG/usr/share/gone/f"'
kmu "$OUT/kpkgadd" --root "$MU/root" "$MU/pkgs/taker2-2.0-1.tar.xz" 2>&1 \
    | grep -q "File conflict" \
    || { echo "  an upgrade over another package's path was not a conflict"; exit 1; }
head -1 "$MU/root/db/taker2" | grep -qx '1.0 1' \
    || { echo "  a refused upgrade replaced the installed version"; exit 1; }
mkport lower 1.0 'install -Dm755 /dev/null "$PKG/bin/lower"'
kmu "$OUT/kpkgadd" --root "$MU/root" "$MU/pkgs/lower-1.0-1.tar.xz" >/dev/null
mkport upper 1.0 'install -Dm755 /dev/null "$PKG/usr/bin/lower"'
kmu "$OUT/kpkgadd" --root "$MU/root" "$MU/pkgs/upper-1.0-1.tar.xz" 2>&1 \
    | grep -q "File conflict" \
    || { echo "  a path owned as ./bin/x did not conflict as ./usr/bin/x"; exit 1; }
[ -f "$MU/root/db/.lock" ] \
    || { echo "  kpkgadd left no writer lock in the database"; exit 1; }
kmu "$OUT/kpkg" list --root "$MU/root" 2>/dev/null | grep -q '^\.' \
    && { echo "  kpkg list reports a dot-file of the database as a package"; exit 1; }
for i in 1 2 3 4; do
    mkport "par$i" 1.0 "for f in \$(seq 1 200); do install -Dm644 /dev/null \"\$PKG/usr/share/par$i/\$f\"; done"
done
for i in 1 2 3 4; do
    kmu "$OUT/kpkgadd" --root "$MU/root" "$MU/pkgs/par$i-1.0-1.tar.xz" > "$MU/par$i.log" &
done
wait
for i in 1 2 3 4; do
    grep -q "installed successfully" "$MU/par$i.log" \
        || { echo "  concurrent install par$i did not complete"; cat "$MU/par$i.log"; exit 1; }
    tar --quoting-style=literal -tf "$MU/pkgs/par$i-1.0-1.tar.xz" | cmp -s - <(tail -n +2 "$MU/root/db/par$i") \
        || { echo "  concurrent install par$i wrote a short database entry"; exit 1; }
done
mkdir -p "$MU/root/db/.recipe" "$MU/root/db/.pkgsha"
echo x > "$MU/root/db/.recipe/par1"; echo x > "$MU/root/db/.pkgsha/par1"
kmu "$OUT/kpkgdel" --root "$MU/root" par1 >/dev/null
[ -e "$MU/root/db/.recipe/par1" ] || [ -e "$MU/root/db/.pkgsha/par1" ] \
    && { echo "  kpkgdel left a sidecar of the package it removed"; exit 1; }
echo "  the entry is the extraction's listing; unowned adopted, owned refused across /bin; four installers at once all recorded"

# The record kpkg check compares against, and a name outside ASCII as its own
# bytes in the manifest.
echo "==> kpkg check finds missing, changed and unowned files, and passes edited /etc"
mkport checked 1.0 'install -Dm755 /dev/null "$PKG/usr/bin/checked"
install -Dm644 /dev/null "$PKG/etc/checked.conf"
install -Dm644 /dev/null "$PKG/etc/checked.keep"
mkdir -p "$PKG/usr/share/checked"
printf x > "$PKG/usr/share/checked/$(printf "caf\\303\\251.txt")"'
kmu "$OUT/kpkgadd" --root "$MU/root" "$MU/pkgs/checked-1.0-1.tar.xz" >/dev/null \
    || { echo "  the checked package did not install"; exit 1; }
grep -qx "./usr/share/checked/caf$(printf '\303\251').txt" "$MU/root/db/checked" \
    || { echo "  a non-ASCII name is not its own bytes in the manifest"; exit 1; }
grep -q " 0755 ./usr/bin/checked$" "$MU/root/db/.files/checked" \
    || { echo "  kpkgadd recorded no sha256 and mode for a placed file"; exit 1; }
kmu "$OUT/kpkg" check --root "$MU/root" checked >/dev/null \
    || { echo "  kpkg check failed a package that is as installed"; exit 1; }
mkdir -p "$MU/ports/hookmode"
printf 'chmod 0700 "$PKG_ROOT/usr/bin/hookmode"\n' > "$MU/ports/hookmode/postinstall.sh"
mkport hookmode 1.0 'install -Dm755 /dev/null "$PKG/usr/bin/hookmode"'
kmu "$OUT/kpkgadd" --root "$MU/root" "$MU/pkgs/hookmode-1.0-1.tar.xz" >/dev/null \
    || { echo "  the hookmode package did not install"; exit 1; }
grep -q " 0700 ./usr/bin/hookmode$" "$MU/root/db/.files/hookmode" \
    && kmu "$OUT/kpkg" check --root "$MU/root" hookmode >/dev/null \
    || { echo "  the file record was taken before the install hook set a mode"; exit 1; }
kmu "$OUT/kpkgdel" --root "$MU/root" hookmode >/dev/null
printf y > "$MU/root/usr/share/checked/caf$(printf '\303\251').txt"
chmod 0700 "$MU/root/usr/bin/checked"
rc=0; kmu "$OUT/kpkg" check --root "$MU/root" checked > "$MU/check.log" || rc=$?
[ "$rc" = 1 ] && grep -q "checked: changed ./usr/share/checked/caf" "$MU/check.log" \
    && grep -q "checked: mode ./usr/bin/checked 0700, installed 0755" "$MU/check.log" \
    || { echo "  kpkg check missed a changed file or mode"; cat "$MU/check.log"; exit 1; }
printf x > "$MU/root/usr/share/checked/caf$(printf '\303\251').txt"
chmod 0755 "$MU/root/usr/bin/checked"
printf 'edited=1\n' > "$MU/root/etc/checked.conf"
chmod 0600 "$MU/root/etc/checked.keep"
rc=0; kmu "$OUT/kpkg" check --root "$MU/root" checked > "$MU/check.log" || rc=$?
[ "$rc" = 0 ] && grep -q "checked: config ./etc/checked.conf$" "$MU/check.log" \
    && grep -q "checked: config mode ./etc/checked.keep 0600, installed 0644" "$MU/check.log" \
    && grep -q ", 0 changed, 0 mode, 2 config" "$MU/check.log" \
    || { echo "  kpkg check failed, or did not report, edited configuration under /etc"; cat "$MU/check.log"; exit 1; }
rm "$MU/root/etc/checked.keep"
rc=0; kmu "$OUT/kpkg" check --root "$MU/root" checked > "$MU/check.log" || rc=$?
[ "$rc" = 1 ] && grep -q "checked: missing ./etc/checked.keep" "$MU/check.log" \
    || { echo "  kpkg check passed a file missing under /etc"; cat "$MU/check.log"; exit 1; }
rm "$MU/root/usr/bin/checked"
install -Dm644 /dev/null "$MU/root/usr/share/nobody/f"
kmu "$OUT/kpkg" check --root "$MU/root" --all > "$MU/check.log" || true
grep -q "checked: missing ./usr/bin/checked" "$MU/check.log" \
    && grep -qx "unowned ./usr/share/nobody/f" "$MU/check.log" \
    || { echo "  kpkg check --all missed a missing or an unowned file"; cat "$MU/check.log"; exit 1; }
kmu "$OUT/kpkgdel" --root "$MU/root" checked >/dev/null
[ -e "$MU/root/db/.files/checked" ] \
    && { echo "  kpkgdel left the file record of the package it removed"; exit 1; }
echo "  sha256 and mode recorded per file; missing, changed, mode and unowned reported; edited /etc reported and passed"

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

# THE BOOK'S NOTE IS NOT PART OF THE RECIPE. `kfs.md` beside a recipe is
# docs/kfs's prose about the port and no build reads it, so adding, editing or
# removing it leaves the hash where it was, while build.sh beside it still
# moves it. Below the port directory the name is an ordinary source file.
H4=$(cat "$SIDE")
printf '# demo\n\n## Notes\nA note.\n' > "$SR/ports/demo/kfs.md"
kstrict KPKG_STRICT_RECIPE=1 | grep -q "Nothing to do" \
    || { echo "  adding kfs.md rebuilt the port"; exit 1; }
printf '# demo\n\n## Notes\nAnother note.\n' > "$SR/ports/demo/kfs.md"
kstrict KPKG_STRICT_RECIPE=1 | grep -q "Nothing to do" \
    || { echo "  editing kfs.md rebuilt the port"; exit 1; }
[ "$H4" = "$(cat "$SIDE")" ] \
    || { echo "  the recipe hash moved with kfs.md"; exit 1; }
printf '# with a note beside it\n' >> "$SR/ports/demo/build.sh"
kstrict KPKG_STRICT_RECIPE=1 | grep -q "Building demo" \
    || { echo "  build.sh changed beside kfs.md and the port did not rebuild"
         exit 1; }
H5=$(cat "$SIDE")
[ "$H4" != "$H5" ] || { echo "  the recipe hash did not move with build.sh"; exit 1; }
rm -f "$SR/ports/demo/kfs.md"
kstrict KPKG_STRICT_RECIPE=1 | grep -q "Nothing to do" \
    || { echo "  removing kfs.md rebuilt the port"; exit 1; }
mkdir -p "$SR/ports/demo/src"
printf 'not a note\n' > "$SR/ports/demo/src/kfs.md"
kstrict KPKG_STRICT_RECIPE=1 | grep -q "Building demo" \
    || { echo "  a kfs.md below the port directory was not hashed"; exit 1; }
rm -rf "$SR/ports/demo/src"
kstrict KPKG_STRICT_RECIPE=1 >/dev/null
[ "$H5" = "$(cat "$SIDE")" ] \
    || { echo "  the recipe hash did not return once src/kfs.md went"; exit 1; }
echo "  kfs.md beside a recipe leaves its hash alone; build.sh still moves it"

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

# ── kpkg build: one hash per source, the exact package, the recipe at start ─
#
# verify_declared() is the only place a source's bytes are hashed; extraction
# checks only that each source is declared. kp_build_port() reports the file it
# wrote, so `kpkg install foo` cannot pick up `foo-bar`'s package from a kept
# cache. The recipe hash is taken before the build, so a recipe edited during
# its own build is rebuilt on the next strict run.
echo "==> kpkg build: sources hashed once, the exact package, the recipe at build start"
KB="$OUT/kbuild"
rm -rf "$KB"; mkdir -p "$KB/ports/withsrc" "$KB/work" "$KB/pkgs" "$KB/src" \
    "$KB/tree/withsrc-1.0"
printf 'hello\n' > "$KB/tree/withsrc-1.0/hello.txt"
tar -C "$KB/tree" -czf "$KB/ports/withsrc/withsrc-1.0.tar.gz" withsrc-1.0
KB_GOOD=$(sha256sum "$KB/ports/withsrc/withsrc-1.0.tar.gz" | cut -c1-64)
kb_recipe() {
    { printf 'name        = withsrc\nversion     = 1.0\nrelease     = 1\n'
      printf 'source      = https://example.invalid/withsrc-1.0.tar.gz\n'
      [ -n "$1" ] && printf 'sha256      = %s  withsrc-1.0.tar.gz\n' "$1"
      printf 'description = a synthetic port with one source tarball\n'
    } > "$KB/ports/withsrc/kpkgbuild"
}
cat > "$KB/ports/withsrc/build.sh" <<'EOF'
install -Dm644 hello.txt "$PKG/usr/share/withsrc/hello.txt"
EOF
ln -sf kdos-kpkg "$OUT/kpkgbuild"
kb_build() {
    ( cd "$KB/ports/withsrc" &&
      env PORT_REPO="$KB/ports" WORK_DIR="$KB/work" PACKAGE_DIR="$KB/pkgs" \
          SOURCE_DIR="$KB/src" PKGDB_DIR=/dev/null KPKG_CONF=/nonexistent \
          SOURCE_DATE_EPOCH=1735689600 TZ=UTC "$@" "$OUT/kpkgbuild" 2>&1 )
}
kb_recipe "$KB_GOOD"
kb_build >"$KB/log" || { echo "  a source with the right hash did not build"; cat "$KB/log"; exit 1; }
[ -f "$KB/pkgs/withsrc-1.0-1.tar.xz" ] && [ -z "$(ls "$KB/pkgs" | grep '\.part$')" ] \
    || { echo "  the package is missing, or a .part file was left"; exit 1; }
rm -f "$KB/pkgs/"*
# A wrong hash is refused before the work directory is touched.
kb_recipe 0000000000000000000000000000000000000000000000000000000000000000
mkdir -p "$KB/work/withsrc"; : > "$KB/work/withsrc/keep"
kb_build >"$KB/log" && { echo "  a source with the WRONG hash built"; exit 1; }
grep -q 'sha256 MISMATCH for withsrc-1.0.tar.gz' "$KB/log" \
    || { echo "  a wrong hash was not reported as a MISMATCH"; cat "$KB/log"; exit 1; }
! grep -q 'Extracting' "$KB/log" && [ -e "$KB/work/withsrc/keep" ] \
    || { echo "  a wrong hash was reported only after extraction began"; exit 1; }
# A source the recipe declares no hash for is refused, unless bring-up asks.
kb_recipe ""
kb_build >"$KB/log" && { echo "  an UNDECLARED source was extracted"; exit 1; }
grep -q 'No sha256 for withsrc-1.0.tar.gz' "$KB/log" \
    || { echo "  an undeclared source was refused for the wrong reason"; cat "$KB/log"; exit 1; }
kb_build KDOS_ALLOW_UNVERIFIED=1 >"$KB/log" \
    || { echo "  KDOS_ALLOW_UNVERIFIED=1 no longer admits an undeclared source"; cat "$KB/log"; exit 1; }
grep -q 'UNVERIFIED: no sha256 for withsrc-1.0.tar.gz' "$KB/log" \
    || { echo "  an unverified source was admitted silently"; exit 1; }
rm -f "$KB/pkgs/"*
kb_recipe "$KB_GOOD"
echo "  a wrong hash fails before extraction; an undeclared source is refused unless KDOS_ALLOW_UNVERIFIED=1"

# Two ports, one name the prefix of the other, both packages kept.
for p in foo foo-bar; do
    mkdir -p "$KB/ports/$p"
    printf 'name        = %s\nversion     = 1.0\nrelease     = 1\ndescription = a synthetic port\n' \
        "$p" > "$KB/ports/$p/kpkgbuild"
    printf 'install -Dm644 /dev/null "$PKG/usr/share/%s/who"\n' "$p" > "$KB/ports/$p/build.sh"
done
ln -sf kdos-kpkg "$OUT/kpkg"
for t in kpkgbuild kpkgadd kpkgdepends; do ln -sf kdos-kpkg "$OUT/$t"; done
kb_install() {
    _root=$1 _port=$2; shift 2
    mkdir -p "$_root"
    env PORT_REPO="$KB/ports" WORK_DIR="$KB/work" PACKAGE_DIR="$KB/pkgs" \
        SOURCE_DIR="$KB/src" PKGDB_DIR=/db KPKG_CONF=/nonexistent \
        SOURCE_DATE_EPOCH=1735689600 TZ=UTC "$@" \
        "$OUT/kpkg" install --root "$_root" "$_port" 2>&1
}
kb_install "$KB/r1" foo-bar KPKG_KEEP_CACHE=1 >"$KB/log" \
    || { echo "  foo-bar did not install"; cat "$KB/log"; exit 1; }
kb_install "$KB/r2" foo KPKG_KEEP_CACHE=1 >"$KB/log" \
    || { echo "  foo did not install"; cat "$KB/log"; exit 1; }
[ -e "$KB/r2/usr/share/foo/who" ] && [ ! -e "$KB/r2/usr/share/foo-bar" ] \
    || { echo "  kpkg install foo installed another port's package"; exit 1; }
echo "  kpkg install foo installs foo's package beside a kept foo-bar"

# A recipe edited by its own build: the record is of the recipe the build
# started from, so the edit reads as a change on the next strict run.
mkdir -p "$KB/ports/selfedit"
printf 'name        = selfedit\nversion     = 1.0\nrelease     = 1\ndescription = a synthetic port that edits its own recipe\n' \
    > "$KB/ports/selfedit/kpkgbuild"
cat > "$KB/ports/selfedit/build.sh" <<'EOF'
grep -q '^# edited' "$PORT_SRC/kpkgbuild" || printf '# edited\n' >> "$PORT_SRC/kpkgbuild"
install -Dm644 /dev/null "$PKG/usr/share/selfedit/who"
EOF
kb_install "$KB/r3" selfedit >"$KB/log" \
    || { echo "  selfedit did not install"; cat "$KB/log"; exit 1; }
grep -q '^# edited' "$KB/ports/selfedit/kpkgbuild" \
    || { echo "  the self-editing fixture did not edit its recipe"; exit 1; }
KB_H1=$(cat "$KB/r3/db/.recipe/selfedit")
kb_install "$KB/r3" selfedit KPKG_STRICT_RECIPE=1 | grep -q "Building selfedit" \
    || { echo "  a recipe edited during its own build was recorded as built"; exit 1; }
[ "$KB_H1" != "$(cat "$KB/r3/db/.recipe/selfedit")" ] \
    || { echo "  the rebuild did not record the edited recipe"; exit 1; }
kb_install "$KB/r3" selfedit KPKG_STRICT_RECIPE=1 | grep -q "Nothing to do" \
    || { echo "  the edited recipe did not settle after one rebuild"; exit 1; }
echo "  the recorded hash is of the recipe the build started from"

# kpkg verify --repro reaches its two packages through the same reported path.
env PORT_REPO="$KB/ports" WORK_DIR="$KB/work" PACKAGE_DIR="$KB/pkgs" \
    SOURCE_DIR="$KB/src" PKGDB_DIR=/dev/null KPKG_CONF=/nonexistent \
    SOURCE_DATE_EPOCH=1735689600 TZ=UTC \
    "$OUT/kpkg" verify --repro withsrc >"$KB/log" 2>&1 \
    || { echo "  kpkg verify --repro failed"; cat "$KB/log"; exit 1; }
grep -q 'BYTE-IDENTICAL' "$KB/log" \
    || { echo "  kpkg verify --repro did not report BYTE-IDENTICAL"; exit 1; }
echo "  kpkg verify --repro reports BYTE-IDENTICAL"

# A sourced port's other files are its recipe too: a kernel config or a freeze
# file beside the recipe is read by the build and named by no `sha256 =`, so
# an edit to it must rebuild the port. The tarball its `sha256 =` names is not
# hashed, and a port holding nothing else settles on the first strict run.
kb_install "$KB/r4" withsrc >"$KB/log" \
    || { echo "  withsrc did not install"; cat "$KB/log"; exit 1; }
kb_install "$KB/r4" withsrc KPKG_STRICT_RECIPE=1 | grep -q "Nothing to do" \
    || { echo "  a sourced port with only named files did not settle"; exit 1; }
printf 'CONFIG_DEMO=y\n' > "$KB/ports/withsrc/demo.config"
kb_install "$KB/r4" withsrc KPKG_STRICT_RECIPE=1 | grep -q "Building withsrc" \
    || { echo "  a file beside a sourced recipe was not part of its hash"; exit 1; }
kb_install "$KB/r4" withsrc KPKG_STRICT_RECIPE=1 | grep -q "Nothing to do" \
    || { echo "  the extra file's hash is not stable across runs"; exit 1; }
printf 'CONFIG_DEMO=n\n' > "$KB/ports/withsrc/demo.config"
kb_install "$KB/r4" withsrc KPKG_STRICT_RECIPE=1 | grep -q "Building withsrc" \
    || { echo "  EDITING a file beside a sourced recipe did not rebuild it"; exit 1; }
rm -f "$KB/ports/withsrc/demo.config"
echo "  a file beside a sourced recipe that no sha256 names is part of its hash"
# Except the book's note, which a sourced port skips as a source-less one does.
kb_install "$KB/r4" withsrc KPKG_STRICT_RECIPE=1 >/dev/null
printf '# withsrc\n\n## Notes\nA note.\n' > "$KB/ports/withsrc/kfs.md"
kb_install "$KB/r4" withsrc KPKG_STRICT_RECIPE=1 | grep -q "Nothing to do" \
    || { echo "  kfs.md beside a sourced recipe rebuilt it"; exit 1; }
rm -f "$KB/ports/withsrc/kfs.md"
echo "  kfs.md beside a sourced recipe is not part of its hash"

# ── kpkg install --build-only / --commit ────────────────────────────────────
#
# The split install: --build-only resolves nothing and refuses a dependency
# that is not installed; a second --build-only of an unchanged recipe builds
# nothing, unless the cache is now kept and the record's package is transient;
# --commit installs from the .pending record, records the recipe hash, and
# removes the package unless the cache is kept. A name with no record is an
# error, never a silent success.
echo "==> kpkg install --build-only / --commit"
BC="$OUT/buildcommit"
rm -rf "$BC"; mkdir -p "$BC/ports/a" "$BC/ports/b" "$BC/work" "$BC/pkgs" "$BC/root"
printf 'name        = a\nversion     = 1.0\nrelease     = 1\ndescription = a synthetic dependency\n' \
    > "$BC/ports/a/kpkgbuild"
printf 'name        = b\nversion     = 1.0\nrelease     = 1\ndescription = a synthetic dependent\ndepends     = a\n' \
    > "$BC/ports/b/kpkgbuild"
for p in a b; do
    printf 'install -Dm644 /dev/null "$PKG/usr/share/%s/who"\n' "$p" > "$BC/ports/$p/build.sh"
done
ln -sf kdos-kpkg "$OUT/kpkg"
for t in kpkgbuild kpkgadd kpkgdepends; do ln -sf kdos-kpkg "$OUT/$t"; done
kbc() {
    env PORT_REPO="$BC/ports" WORK_DIR="$BC/work" PACKAGE_DIR="$BC/pkgs" \
        PKGDB_DIR=/db KPKG_CONF=/nonexistent SOURCE_DATE_EPOCH=1735689600 \
        TZ=UTC "$@" 2>&1
}
"$OUT/kpkg" help | grep -q -- '--build-only' \
    || { echo "  kpkg help does not name --build-only"; exit 1; }
kbc "$OUT/kpkg" install --root "$BC/root" --build-only b >"$BC/log" \
    && { echo "  --build-only b built with its dependency missing"; exit 1; }
grep -q 'dependency a ' "$BC/log" \
    || { echo "  --build-only b did not name the missing dependency"; cat "$BC/log"; exit 1; }
[ ! -e "$BC/pkgs/b.pending" ] && [ ! -e "$BC/pkgs/a.pending" ] \
    || { echo "  a refused --build-only left a record"; exit 1; }
kbc "$OUT/kpkg" install --root "$BC/root" --build-only a >"$BC/log" \
    || { echo "  --build-only a failed"; cat "$BC/log"; exit 1; }
grep -q 'Building a' "$BC/log" || { echo "  --build-only a built nothing"; exit 1; }
[ "$(sed -n 1p "$BC/pkgs/a.pending")" = "$BC/pkgs/a-1.0-1.tar.xz" ] \
    && [ -f "$BC/pkgs/a-1.0-1.tar.xz" ] \
    || { echo "  a.pending does not name the package it built"; cat "$BC/pkgs/a.pending"; exit 1; }
BC_H=$(sed -n 2p "$BC/pkgs/a.pending")
[ "${#BC_H}" = 64 ] || { echo "  a.pending carries no recipe hash"; exit 1; }
[ "$(sed -n 3p "$BC/pkgs/a.pending")" = transient ] \
    || { echo "  a.pending does not call its xz -0 package transient"; exit 1; }
[ ! -e "$BC/root/db/a" ] || { echo "  --build-only installed the package"; exit 1; }
kbc "$OUT/kpkg" install --root "$BC/root" --build-only a >"$BC/log" \
    || { echo "  a second --build-only a failed"; cat "$BC/log"; exit 1; }
grep -q 'already built' "$BC/log" && ! grep -q 'Building a' "$BC/log" \
    || { echo "  a second --build-only of an unchanged recipe built again"; cat "$BC/log"; exit 1; }
kbc "$OUT/kpkg" install --root "$BC/root" --commit a >"$BC/log" \
    || { echo "  --commit a failed"; cat "$BC/log"; exit 1; }
grep -qx 'kpkg: committed a' "$BC/log" || { echo "  --commit a did not say so"; exit 1; }
[ -e "$BC/root/usr/share/a/who" ] && [ "$(cat "$BC/root/db/.recipe/a")" = "$BC_H" ] \
    || { echo "  --commit a did not install it, or recorded another hash"; exit 1; }
[ ! -e "$BC/pkgs/a.pending" ] && [ ! -e "$BC/pkgs/a-1.0-1.tar.xz" ] \
    || { echo "  --commit a left its record or its package behind"; exit 1; }
kbc "$OUT/kpkg" install --root "$BC/root" --build-only b >"$BC/log" \
    || { echo "  --build-only b failed with a installed"; cat "$BC/log"; exit 1; }
kbc KPKG_KEEP_CACHE=1 "$OUT/kpkg" install --root "$BC/root" --build-only b >"$BC/log" \
    || { echo "  --build-only b under KPKG_KEEP_CACHE=1 failed"; cat "$BC/log"; exit 1; }
grep -q 'Building b' "$BC/log" && ! grep -q 'already built' "$BC/log" \
    && [ "$(sed -n 3p "$BC/pkgs/b.pending")" = kept ] \
    || { echo "  a transient record was reused for a kept cache"; cat "$BC/log"; exit 1; }
kbc KPKG_KEEP_CACHE=1 "$OUT/kpkg" install --root "$BC/root" --commit b >"$BC/log" \
    || { echo "  --commit b failed"; cat "$BC/log"; exit 1; }
[ -e "$BC/root/usr/share/b/who" ] && [ -f "$BC/pkgs/b-1.0-1.tar.xz" ] \
    && [ ! -e "$BC/pkgs/b.pending" ] \
    || { echo "  --commit under KPKG_KEEP_CACHE=1 did not keep the package"; exit 1; }
kbc "$OUT/kpkg" install --root "$BC/root" --commit b >"$BC/log" \
    && { echo "  --commit of a name with no .pending succeeded"; exit 1; }
grep -q 'no b.pending' "$BC/log" || { echo "  a missing record was not named"; cat "$BC/log"; exit 1; }
echo "  --build-only refuses a missing dependency and resumes; --commit installs from the record"

# ── The package store ───────────────────────────────────────────────────────
#
# A hit installs the stored package without running build.sh and leaves the
# same files and sidecars a build would; a change to a dependency's bytes, to
# the build environment or to the stored file misses; a job count does not; a
# dependency with no .pkgsha, or no $KPKG_STORE_SALT, makes the key unknown and
# nothing is stored; an undeclared link is re-checked on lookup; check mode builds every hit and logs
# a difference without failing.
echo "==> the package store reuses only what a build would have made"
ST="$OUT/pkgstore"
rm -rf "$ST"; mkdir -p "$ST/ports" "$ST/work" "$ST/pkgs" "$ST/root" "$ST/marks"
st_port() {   # name depends build-body
    mkdir -p "$ST/ports/$1"
    { printf 'name        = %s\nversion     = 1.0\nrelease     = 1\n' "$1"
      printf 'description = a synthetic port for the package store\n'
      [ -n "$2" ] && printf 'depends     = %s\n' "$2"
    } > "$ST/ports/$1/kpkgbuild"
    printf 'touch "$ST_MARKS/%s"\n%s\n' "$1" "$3" > "$ST/ports/$1/build.sh"
}
st_port demo-a "" 'install -Dm644 /dev/null "$PKG/usr/share/demo-a/v"; printf one > "$PKG/usr/share/demo-a/v"'
st_port demo-b demo-a 'install -Dm644 /dev/null "$PKG/usr/share/demo-b/v"; printf b > "$PKG/usr/share/demo-b/v"'
ln -sf kdos-kpkg "$OUT/kpkg"
for t in kpkgbuild kpkgadd kpkgdel kpkgdepends; do ln -sf kdos-kpkg "$OUT/$t"; done
kst() {
    env PORT_REPO="$ST/ports" WORK_DIR="$ST/work" PACKAGE_DIR="$ST/pkgs" \
        PKGDB_DIR=/db KPKG_CONF=/nonexistent SOURCE_DATE_EPOCH=1735689600 \
        TZ=UTC ST_MARKS="$ST/marks" KPKG_STORE="$ST/store" DEMO_ROOT="$ST/root" \
        KPKG_STORE_SALT=selftest "$@" 2>&1
}
st_wipe() { rm -rf "$ST/root" "$ST/marks"; mkdir -p "$ST/root" "$ST/marks"; }
kst KPKG_STORE_SALT= "$OUT/kpkg" install --root "$ST/root" demo-a >"$ST/log" \
    || { echo "  a build with no salt failed"; cat "$ST/log"; exit 1; }
[ -z "$(ls "$ST/store"/*/*/META 2>/dev/null)" ] \
    || { echo "  a build with no salt was stored"; exit 1; }
st_wipe; rm -rf "$ST/pkgs" "$ST/work"; mkdir -p "$ST/pkgs" "$ST/work"
kst "$OUT/kpkg" install --root "$ST/root" demo-b >"$ST/log" \
    || { echo "  the first store build failed"; cat "$ST/log"; exit 1; }
[ -e "$ST/marks/demo-a" ] && [ -e "$ST/marks/demo-b" ] \
    || { echo "  an empty store did not build both ports"; exit 1; }
[ "$(cat "$ST/root/db/.pkgsha/demo-a")" != "" ] && [ -s "$ST/root/db/.recipe/demo-b" ] \
    || { echo  "  kpkgadd recorded no .pkgsha"; exit 1; }
[ "$(ls "$ST/store"/*/*/META | wc -l)" = 2 ] \
    || { echo "  the store did not hold both packages"; ls -R "$ST/store"; exit 1; }
st_files() { (cd "$ST/root" && find usr -type f -exec sha256sum {} + | sort); }
BEFORE=$(st_files)
st_wipe
kst "$OUT/kpkg" install --root "$ST/root" demo-b >"$ST/log" \
    || { echo "  the reinstall failed"; cat "$ST/log"; exit 1; }
[ ! -e "$ST/marks/demo-b" ] && grep -q '^==> Reusing demo-b from the store ([0-9a-f]\{12\})$' "$ST/log" \
    || { echo "  a hit ran build.sh, or did not say it reused"; cat "$ST/log"; exit 1; }
[ "$BEFORE" = "$(st_files)" ] && [ -s "$ST/root/db/.recipe/demo-b" ] && [ -s "$ST/root/db/.pkgsha/demo-b" ] \
    || { echo "  a hit installed different files, or no sidecars"; exit 1; }
# A job count is not an input.
st_wipe
kst MAKEFLAGS=-j3 KDOS_JOBS=3 "$OUT/kpkg" install --root "$ST/root" demo-b >"$ST/log"
[ ! -e "$ST/marks/demo-b" ] || { echo "  MAKEFLAGS/KDOS_JOBS missed the store"; exit 1; }
# The flags are.
st_wipe
kst CFLAGS=-O3 "$OUT/kpkg" install --root "$ST/root" demo-b >"$ST/log"
[ -e "$ST/marks/demo-b" ] || { echo "  a changed CFLAGS hit the store"; exit 1; }
# -f always builds.
st_wipe
kst "$OUT/kpkg" install --root "$ST/root" demo-a >/dev/null
kst "$OUT/kpkg" install --root "$ST/root" -f demo-b >"$ST/log"
[ -e "$ST/marks/demo-b" ] || { echo "  -f installed from the store"; exit 1; }
# A dependency with different bytes misses.
st_wipe
kst "$OUT/kpkg" install --root "$ST/root" demo-a >/dev/null
kst "$OUT/kpkgdel" --root "$ST/root" demo-a >/dev/null
st_port demo-a "" 'install -Dm644 /dev/null "$PKG/usr/share/demo-a/v"; printf two > "$PKG/usr/share/demo-a/v"'
rm -f "$ST/marks/"*
kst "$OUT/kpkg" install --root "$ST/root" demo-b >"$ST/log"
[ -e "$ST/marks/demo-a" ] && [ -e "$ST/marks/demo-b" ] \
    || { echo "  demo-b hit the store over a changed demo-a"; cat "$ST/log"; exit 1; }
# A dependency with no .pkgsha: build, do not store.
st_wipe
kst "$OUT/kpkg" install --root "$ST/root" demo-a >/dev/null
rm -f "$ST/root/db/.pkgsha/demo-a"
N=$(ls "$ST/store"/*/*/META | wc -l)
kst CFLAGS=-Os "$OUT/kpkg" install --root "$ST/root" demo-b >"$ST/log"
[ -e "$ST/marks/demo-b" ] && grep -q 'store key unknown' "$ST/log" \
    && [ "$(ls "$ST/store"/*/*/META | wc -l)" = "$N" ] \
    || { echo "  an unknown dependency hash was keyed or stored"; cat "$ST/log"; exit 1; }
# A truncated stored file is not a hit.
st_wipe
kst "$OUT/kpkg" install --root "$ST/root" demo-a >/dev/null
for f in "$ST/store"/*/*/demo-b-*.tar.xz; do : > "$f"; done
kst "$OUT/kpkg" install --root "$ST/root" demo-b >"$ST/log" \
    || { echo "  a truncated entry broke the install"; cat "$ST/log"; exit 1; }
[ -e "$ST/marks/demo-b" ] || { echo "  a truncated stored package was installed"; exit 1; }
echo "  hit without build.sh; deps, flags, -f, unknown and truncated all build"

# The ELF reader.
"$OUT/kpkg" help | grep -q 'store gc' || { echo "  kpkg help does not name store gc"; exit 1; }
if command -v cc >/dev/null 2>&1; then
    mkdir -p "$ST/elf"
    printf 'int demo_lib(void) { return 7; }\n' > "$ST/elf/lib.c"
    st_port demo-lib "" 'mkdir -p "$PKG/usr/lib" "$PKG/usr/include"; cc -shared -fPIC -o "$PKG/usr/lib/libdemolib.so" '"$ST/elf/lib.c"
    printf 'int demo_lib(void);\nint main(void) { return demo_lib(); }\n' > "$ST/elf/app.c"
    st_port demo-app "" 'mkdir -p "$PKG/usr/bin"; cc -o "$PKG/usr/bin/demo-app" '"$ST/elf/app.c"' -L"$DEMO_ROOT/usr/lib" -ldemolib'
    st_wipe
    kst "$OUT/kpkg" install --root "$ST/root" demo-lib >/dev/null
    kst "$OUT/kpkg" install --root "$ST/root" demo-app >"$ST/log" \
        || { echo "  demo-app did not build"; cat "$ST/log"; exit 1; }
    grep -q 'demo-app links demo-lib without declaring it' "$ST/log" \
        || { echo "  the undeclared link was not reported"; cat "$ST/log"; exit 1; }
    grep -q '^X:demo-lib ' "$ST/store"/*/*/META \
        || { echo  "  no X: line for the undeclared link"; exit 1; }
    kst "$OUT/kpkgdel" --root "$ST/root" demo-app >/dev/null
    rm -f "$ST/marks/"*
    kst "$OUT/kpkg" install --root "$ST/root" demo-app >/dev/null
    [ ! -e "$ST/marks/demo-app" ] || { echo "  an unchanged undeclared link missed"; exit 1; }
    kst "$OUT/kpkgdel" --root "$ST/root" demo-app demo-lib >/dev/null
    printf 'int demo_lib(void) { return 8; }\n' > "$ST/elf/lib.c"
    kst "$OUT/kpkg" install --root "$ST/root" -f demo-lib >/dev/null
    kst "$OUT/kpkg" install --root "$ST/root" demo-app >/dev/null
    [ -e "$ST/marks/demo-app" ] || { echo "  demo-app hit over a changed undeclared library"; exit 1; }
    echo "  an undeclared link is recorded and a change to it misses"
fi

# Check mode: a port whose bytes differ every build.
st_port demo-rand "" 'install -Dm644 /dev/null "$PKG/usr/share/demo-rand/v"; echo $RANDOM$RANDOM > "$PKG/usr/share/demo-rand/v"'
st_wipe
kst "$OUT/kpkg" install --root "$ST/root" demo-rand >/dev/null
kst "$OUT/kpkgdel" --root "$ST/root" demo-rand >/dev/null
rm -f "$ST/marks/"*
kst KPKG_STORE_CHECK=1 "$OUT/kpkg" install --root "$ST/root" demo-rand >"$ST/log" \
    || { echo "  check mode failed the install"; cat "$ST/log"; exit 1; }
[ -e "$ST/marks/demo-rand" ] && grep -q '^demo-rand [0-9a-f]\{12\} stored=[0-9a-f]\{64\} built=[0-9a-f]\{64\}$' "$ST/logs/pkgstore-check.log" \
    || { echo "  check mode did not build, or logged no mismatch"; cat "$ST/log"; exit 1; }
grep -q 'usr/share/demo-rand/v' "$ST/logs/pkgstore-check.log" \
    || { echo "  the check log does not name the differing member"; exit 1; }
echo "  check mode builds a hit and logs the difference"

# gc: a tiny cap keeps the newest.
N=$(ls "$ST/store"/*/*/META | wc -l)
OLDEST=$(ls -tr "$ST/store"/*/*/META | head -n 1)
"$OUT/kpkg" store gc "$ST/store" 1 >/dev/null || { echo "  store gc failed"; exit 1; }
[ ! -e "$OLDEST" ] && [ "$(ls "$ST/store"/*/*/META 2>/dev/null | wc -l)" -lt "$N" ] \
    || { echo "  store gc did not evict the oldest entry"; exit 1; }
"$OUT/kpkg" store gc "$ST/store" 1x >/dev/null 2>&1 && { echo "  store gc took a bad size"; exit 1; }
echo "  gc evicts least recently used first"

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
# The release index 010_binhost.sh writes over the directory: every file at
# binhost/<name>, beside the index, signed with the binhost's key.
. ports/releaselib.sh
_bhidx() {  # <dir> <key> <version>
    local f n
    for f in "$1"/*; do
        n=${f##*/}
        case $n in kdos-update.idx*|*.pub) continue ;; esac
        rel_entry "$1" "$n" "binhost/$n"
    done | rel_write "$1/kdos-update.idx" "$3"
    rm -f "$1/kdos-update.idx.sig"
    kpkg_bh kpkg sign "$1/kdos-update.idx" "$2" >/dev/null 2>&1
}
_bhidx "$BH/repo" "$BH/builder.key" 0.2.14
grep -q ' - PACKAGES binhost/PACKAGES$' "$BH/repo/kdos-update.idx" && test -s "$BH/repo/kdos-update.idx.sig" \
    || { echo "  the binhost's release index was not written"; cat "$BH/repo/kdos-update.idx"; exit 1; }
echo "  signed, verified, and every refusal refused"

echo
echo "==> kdos update offers only what the signed binhost covers"
# The update document kdos-sysupdated serves Discover from. `offer` is set
# only when every package that is behind passes kpkg's three tests against a
# cached index a trusted key signed — the same tests `kpkg binhost` repeats at
# install time — so a version Discover shows is one `apply` will take. Each
# way it must not be offered is asserted: a build environment the index was
# not made under, a key nobody trusts, an edited index, no index at all.
UC="$OUT/updcheck"
rm -rf "$UC"
mkdir -p "$UC/db" "$UC/cache" "$UC/keys" "$UC/sys/etc/kdos" "$UC/sys/var/lib/kdos"
ln -sf kdos-tools "$OUT/kdos"
printf '0.9 1\n' > "$UC/db/tiny"
printf 'kdos-tree 1\ncommit 29d742c14cc8aef873af7f1f28e267d6318facc2\ntime 1761600000\nversion 0.2.14\n' \
    > "$RP/TREE"
printf 'Plasma 6.8.2.\n' > "$RP/RELEASE"
printf 'VERSION="0.2.13"\n' > "$UC/sys/etc/kdos-release"
cp "$BH/repo/PACKAGES" "$BH/repo/PACKAGES.sig" "$UC/cache/"
cp "$BH/repo/kdos-update.idx" "$UC/cache/INDEX"
cp "$BH/repo/kdos-update.idx.sig" "$UC/cache/INDEX.sig"
printf 'file://%s\n' "$BH/repo" > "$UC/cache/ORIGIN"
cp "$BH/builder.pub" "$UC/keys/"
upd() {
    env PORT_REPO="$RP/ports" KPKG_CONF=/nonexistent PKGDB_DIR="$UC/db" \
        KPKG_KEYRING="$UC/keys" KDOS_UPDATE_STATE="$UC/state" \
        KDOS_UPDATE_CACHE="$UC/cache" KDOS_UPDATE_SYSROOT="$UC/sys" \
        KDOS_UPDATE_TREE="$UC/nowhere" SOURCE_DATE_EPOCH=1735689600 \
        "$OUT/kdos" update "$@"
}
rc=0; upd check --json > "$UC/doc" 2>"$UC/err" || rc=$?
test "$rc" = 1 || { echo "  check with one package behind exited $rc, not 1"; cat "$UC/err"; exit 1; }
_bhkey=$(awk '{ print $2; exit }' "$BH/repo/PACKAGES.sig")
for want in '"behind":1' '"covered":1,"uncovered":0' '"offer":"0.2.14","lane":"binhost"' \
            '"version":"0.2.14","date":"2025-10-27","installed":"0.2.13"' \
            "\"signed_by\":{\"key\":\"$_bhkey\"" '"notes":"Plasma 6.8.2.\n"' \
            '"packages":[{"name":"tiny","have":"0.9","want":"1.0-1","binary":true}]'; do
    grep -qF -- "$want" "$UC/doc" || { echo "  the update document lacks $want:"; cat "$UC/doc"; exit 1; }
done
python3 -I -c 'import json, sys; json.load(open(sys.argv[1]))' "$UC/doc" \
    || { echo "  the update document is not JSON"; exit 1; }
# The exit status says whether anything is behind, --out or not: 1 here.
rc=0; upd check --out "$UC/sys/var/lib/kdos/update.json" >/dev/null 2>&1 || rc=$?
test "$rc" = 1 || { echo "  check --out with one package behind exited $rc, not 1"; exit 1; }
cmp -s <(sed 's/"verified":[0-9]*/V/; s/"checked":[0-9]*/C/' "$UC/doc") \
       <(sed 's/"verified":[0-9]*/V/; s/"checked":[0-9]*/C/' "$UC/sys/var/lib/kdos/update.json") \
    || { echo "  --out wrote a different document from --json"; exit 1; }
test ! -e "$UC/sys/var/lib/kdos/update.json.new" || { echo "  --out left its temporary file"; exit 1; }
upd check 2>/dev/null | grep -q "binhost: all 1 prebuilt, signed by .*offered as 0.2.14" \
    || { echo "  the human check does not say what is offered"; exit 1; }
# The plan a binhost apply would make.
upd apply --dry-run --in-place > "$UC/plan" 2>&1 \
    || { echo "  a covered dry run failed:"; cat "$UC/plan"; exit 1; }
grep -q "tiny .* 0.9 .* → 1.0-1: tiny-1.0-1.tar.xz" "$UC/plan" \
    || { echo "  the dry run names no package file:"; cat "$UC/plan"; exit 1; }
# `install` is the ports lane's: a port this slot lacks, every package of it
# covered by the signed index, nothing built. Asked of a database without
# tiny, so it is a port to add; asked again with tiny installed, nothing.
mv "$UC/db/tiny" "$UC/tiny.db"
upd install --dry-run tiny > "$UC/inst" 2>&1 \
    || { echo "  a covered install dry run failed:"; cat "$UC/inst"; exit 1; }
grep -qxF 'tiny tiny-1.0-1.tar.xz' "$UC/inst" \
    || { echo "  the install dry run is:"; cat "$UC/inst"; exit 1; }
mv "$UC/tiny.db" "$UC/db/tiny"
upd install --dry-run tiny 2>&1 | grep -qxF 'tiny is already installed' \
    || { echo "  an installed port was planned again"; exit 1; }
rc=0; upd install --dry-run nosuchport >/dev/null 2>&1 || rc=$?
test "$rc" = 1 || { echo "  installing a port the tree lacks exited $rc, not 1"; exit 1; }
rc=0; upd install --dry-run -x >/dev/null 2>&1 || rc=$?
test "$rc" = 2 || { echo "  an option as the port name exited $rc, not 2"; exit 1; }
# `notes --json` is the What's New page's document: the running version, the
# other slot's (none recorded here) and the tree's RELEASE as one string.
KDOS_BOOTSTATE="$UC/nobootstate" upd notes --json > "$UC/notes" \
    || { echo "  notes --json failed"; exit 1; }
grep -qxF '{"version":"0.2.13","previous":"","notes":"Plasma 6.8.2.\n"}' "$UC/notes" \
    || { echo "  the notes document is:"; cat "$UC/notes"; exit 1; }
# Matched under another build's environment: nothing is offered, and apply
# refuses before anything is written, naming the way that works — on the
# progress descriptor too, which is where kdos-sysupdated reads it.
printf 'CC=cc\nCFLAGS="-O3 -march=native"\n' > "$UC/sys/etc/kdos/chroot.env"
upd check --json > "$UC/doc" 2>/dev/null || test $? = 1
grep -qF '"covered":0,"uncovered":1' "$UC/doc" && grep -qF '"offer":null' "$UC/doc" \
    && grep -qF '"binary":false' "$UC/doc" \
    || { echo "  another build's environment still offered the update:"; cat "$UC/doc"; exit 1; }
rc=0; upd apply --dry-run --in-place --progress 3 3>"$UC/prog" >/dev/null 2>&1 || rc=$?
test "$rc" = 3 || { echo "  an uncovered apply exited $rc, not 3"; exit 1; }
grep -qxF 'refused 1 package needs a source build (tiny): `kdos update apply --source`' "$UC/prog" \
    || { echo "  the refusal on the progress descriptor is:"; cat "$UC/prog"; exit 1; }
mv "$UC/db/tiny" "$UC/tiny.db"
rc=0; upd install --dry-run tiny >/dev/null 2>"$UC/err" || rc=$?
test "$rc" = 1 && grep -q '1 package needs a source build (tiny)' "$UC/err" \
    || { echo "  an uncovered install exited $rc:"; cat "$UC/err"; exit 1; }
mv "$UC/tiny.db" "$UC/db/tiny"
rm "$UC/sys/etc/kdos/chroot.env"
# A key nobody trusts, an edited index, no index: never offered.
mv "$UC/keys/builder.pub" "$UC/builder.pub"
upd check --json > "$UC/doc" 2>/dev/null || test $? = 1
grep -qF '"offer":null' "$UC/doc" && grep -qF '"signed_by":null' "$UC/doc" \
    || { echo "  an index no trusted key signed was believed"; exit 1; }
mv "$UC/builder.pub" "$UC/keys/"
# A genuine PACKAGES under a genuine release index that names another one:
# the two were not fetched together, and nothing is offered from them.
rm -rf "$UC/other"; cp -r "$BH/repo" "$UC/other"
printf '\n' >> "$UC/other/PACKAGES"
_bhidx "$UC/other" "$BH/builder.key" 0.2.14
cp "$UC/cache/INDEX" "$UC/index.good"
cp "$UC/other/kdos-update.idx" "$UC/cache/INDEX"
cp "$UC/other/kdos-update.idx.sig" "$UC/cache/INDEX.sig"
upd check --json > "$UC/doc" 2>/dev/null || test $? = 1
grep -qF '"offer":null' "$UC/doc" \
    || { echo "  a binhost index the release index does not name was believed"; exit 1; }
rc=0; upd apply --dry-run --in-place >/dev/null 2>"$UC/err" || rc=$?
test "$rc" = 3 && grep -q 'not the one the signed release index names' "$UC/err" \
    || { echo "  an apply over a mismatched pair exited $rc:"; cat "$UC/err"; exit 1; }
cp "$UC/index.good" "$UC/cache/INDEX"
cp "$BH/repo/kdos-update.idx.sig" "$UC/cache/INDEX.sig"
sed -i 's/^S:[0-9]*$/S:1/' "$UC/cache/PACKAGES"
upd check --json > "$UC/doc" 2>/dev/null || test $? = 1
grep -qF '"offer":null' "$UC/doc" || { echo "  an edited index was believed"; exit 1; }
rm "$UC/cache/PACKAGES"
upd check --json > "$UC/doc" 2>/dev/null || test $? = 1
grep -qF '"offer":null' "$UC/doc" && grep -qF '"covered":0' "$UC/doc" \
    || { echo "  no index offered an update"; exit 1; }
rc=0; upd apply --dry-run --in-place >/dev/null 2>"$UC/err" || rc=$?
test "$rc" = 3 && grep -q -- "--source" "$UC/err" \
    || { echo "  an apply with no index did not point at --source"; exit 1; }
rc=0; upd install --dry-run tiny >/dev/null 2>&1 || rc=$?
test "$rc" = 1 || { echo "  an install with no index exited $rc, not 1"; exit 1; }
rm -f "$RP/RELEASE"
rc=0; upd notes --json >/dev/null 2>&1 || rc=$?
test "$rc" = 1 || { echo "  notes with no RELEASE exited $rc, not 1"; exit 1; }
rm -f "$RP/TREE"
echo "  offered only when a trusted, unedited index covers every package under this build's environment; install and notes answer from the same index and tree"

echo
echo "==> kpkg fetch takes a source from the archive and tells offline from gone"
# A shipped machine has recipes and no sources. The archive here is a
# directory laid out as <tag>/<asset>, which KDOS_SOURCES_BASE reads as a
# file:// URL; upstream is 127.0.0.1:9, where a connection is refused at once,
# so no case touches the network. Exit 3 (offline) must only ever mean that
# nothing answered: a wrong hash from a server that did answer is exit 1.
if command -v curl >/dev/null 2>&1; then
    KF="$OUT/kfetch"
    rm -rf "$KF"
    mkdir -p "$KF/tree/core/misc/fz" "$KF/arch/sources-1" "$KF/src"
    printf 'fz source\n' > "$KF/arch/sources-1/misc--fz-1.0.tar.gz"
    _kfh=$(sha256sum "$KF/arch/sources-1/misc--fz-1.0.tar.gz" | cut -d' ' -f1)
    printf '# kdos-sources-index 2\n%s sources-1 misc--fz-1.0.tar.gz fz/fz-1.0.tar.gz\n' \
        "$_kfh" > "$KF/tree/sources.idx"
    printf 'name = fz\nversion = 1.0\nrelease = 1\nsource = http://127.0.0.1:9/fz-$version.tar.gz\nsha256 = %s  fz-1.0.tar.gz\n' \
        "$_kfh" > "$KF/tree/core/misc/fz/kpkgbuild"
    : > "$KF/tree/core/misc/fz/build.sh"
    kpkg_kf() {
        env PORT_REPO="$KF/tree/core" KPKG_CONF=/nonexistent SOURCE_DIR="$KF/src" \
            "$OUT/kdos-kpkg" kpkg fetch fz >/dev/null 2>&1
    }
    KDOS_SOURCES_BASE="$KF/arch" kpkg_kf \
        || { echo "  a source the archive holds was not fetched"; exit 1; }
    test "$(sha256sum "$KF/src/fz-1.0.tar.gz" | cut -d' ' -f1)" = "$_kfh" \
        || { echo "  the fetched source is not the recipe's bytes"; exit 1; }
    # A vendor bundle is named by no `source =` entry: build.sh opens it as
    # $PORT_SRC/<file>, so it must appear in the port directory.
    printf 'fz vendor\n' > "$KF/arch/sources-1/misc--fz-vendor-1.0.tar.xz"
    _kfv=$(sha256sum "$KF/arch/sources-1/misc--fz-vendor-1.0.tar.xz" | cut -d' ' -f1)
    printf '%s sources-1 misc--fz-vendor-1.0.tar.xz fz/fz-vendor-1.0.tar.xz\n' \
        "$_kfv" >> "$KF/tree/sources.idx"
    printf 'sha256 = %s  fz-vendor-1.0.tar.xz\n' "$_kfv" >> "$KF/tree/core/misc/fz/kpkgbuild"
    KDOS_SOURCES_BASE="$KF/arch" kpkg_kf \
        || { echo "  a vendor bundle the archive holds was not fetched"; exit 1; }
    test "$(sha256sum "$KF/tree/core/misc/fz/fz-vendor-1.0.tar.xz" 2>/dev/null | cut -d' ' -f1)" = "$_kfv" \
        || { echo "  a fetched vendor bundle is not in the port directory"; exit 1; }
    test ! -e "$KF/tree/core/misc/fz/fz-1.0.tar.gz" \
        || { echo "  a source a \`source =\` entry names was linked into the port directory"; exit 1; }
    rm -f "$KF/src/fz-1.0.tar.gz"
    printf 'other bytes\n' > "$KF/arch/sources-1/misc--fz-1.0.tar.gz"
    rc=0; KDOS_SOURCES_BASE="$KF/arch" kpkg_kf || rc=$?
    test "$rc" = 1 || { echo "  a wrong hash from the archive gave exit $rc, not 1"; exit 1; }
    test ! -e "$KF/src/fz-1.0.tar.gz" \
        || { echo "  a source that did not verify was kept"; exit 1; }
    rc=0; KDOS_SOURCES_BASE= kpkg_kf || rc=$?
    test "$rc" = 3 || { echo "  an unreachable upstream gave exit $rc, not 3"; exit 1; }
    echo "  archive first, every copy hashed, offline told apart"
else
    echo "  skipped — no curl"
fi

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
# THE THUMBNAIL CACHE'S NAME, which is the whole interop claim: Dolphin, an
# image viewer and `kdos thumb` all have to compute the SAME path or each
# looks for a thumbnail where the others never wrote one. And a thumbnail is
# answered only while its Thumb::MTime is the file's: one of a file that has
# since changed is a picture of something else.
#
echo "==> a thumbnail is found where every other program writes it, and only while it is current"
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
_tenv="HOME=$_thumbdir XDG_CACHE_HOME=$_thumbdir/.cache"
# shellcheck disable=SC2086
env $_tenv "$OUT/kdos" thumb "$_thumbdir/me.png" >/dev/null 2>&1 \
    && { echo "  a thumbnail nobody wrote was answered"; exit 1; }
# A PNG holding the source's mtime in a tEXt chunk, the way KIO writes one.
mkdir -p "$(dirname "$_tp")"
python3 - "$_tp" "$(stat -c %Y "$_thumbdir/me.png")" <<'PYEOF'
import struct, sys, zlib
def chunk(t, d):
    return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d))
open(sys.argv[1], 'wb').write(b'\x89PNG\r\n\x1a\n'
    + chunk(b'IHDR', struct.pack('>IIBBBBB', 1, 1, 8, 2, 0, 0, 0))
    + chunk(b'tEXt', b'Thumb::MTime\0' + sys.argv[2].encode())
    + chunk(b'IDAT', zlib.compress(b'\0\0\0\0')) + chunk(b'IEND', b''))
PYEOF
# shellcheck disable=SC2086
[ "$(env $_tenv "$OUT/kdos" thumb "$_thumbdir/me.png")" = "$_tp" ] \
    || { echo "  a current thumbnail was not answered"; exit 1; }
touch -d 2001-01-01 "$_thumbdir/me.png"
# shellcheck disable=SC2086
env $_tenv "$OUT/kdos" thumb "$_thumbdir/me.png" >/dev/null 2>&1 \
    && { echo "  a thumbnail of a file since changed was answered"; exit 1; }
rm -rf "$_thumbdir"
echo "  the name is md5(file://<path>), and a stale Thumb::MTime is no thumbnail"

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
# The SHIPPED parser is sourced and run, not copied here: a copy is a copy
# that goes stale, and this file already has that rule about the key card.
# It is the one /etc/init.d/18_timers.sh and the per-user runner both source.
#
echo "==> a timer row splits into an argument vector without globbing"
_tw=$(mktemp -d)
mkdir -p "$_tw/d"
touch "$_tw/aaa" "$_tw/bbb"
cp fs/usr/local/lib/kdos/timers.sh "$_tw/parse.sh"
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
# A CRASH-LOOPING SERVICE IS NOT REPORTED AS RUNNING. The pid file names the
# supervisor, which outlives every exit of its daemon, so `ksvc check` reads
# the supervisor's state file as well: judged by the pid file alone, a daemon
# that aborts on every start is `[ OK ]` for as long as the machine is up. One
# exit with status 0 is not yet a loop — a supervised snooze exits 0 after
# every job. ksvc is built with RUN_DIR inside the suite's directory, so
# nothing touches /run.
#
echo "==> a crash-looping service is [FAIL], a running one [ OK ]"
_kv="$OUT/ksvc-run"
mkdir -p "$_kv/run"
printf 'int ksvc_main(int, char **);\nint main(int c, char **v) { return ksvc_main(c, v); }\n' \
    > "$_kv/main.c"
$CC $STD $WARN $INC -Isrc/system/kdos-tools -DRUN_DIR="\"$_kv/run\"" \
    -o "$_kv/ksvc" "$_kv/main.c" src/system/kdos-tools/ksvc.c \
    src/libs/libkbase/*.c
"$_kv/ksvc" supervise crash sh -c 'exit 3' >/dev/null
"$_kv/ksvc" supervise good sleep 600 >/dev/null
"$_kv/ksvc" supervise clean true >/dev/null
sleep 1
_kfail=""
"$_kv/ksvc" check crash > "$_kv/out" && _kfail="$_kfail crash-exits-0"
grep -q '^\[FAIL\] crash  (exited with code 3' "$_kv/out" || _kfail="$_kfail crash-not-FAIL"
"$_kv/ksvc" check good | grep -q '^\[ OK \] good' || _kfail="$_kfail running-not-OK"
"$_kv/ksvc" check clean | grep -q '^\[ OK \] clean' || _kfail="$_kfail one-exit-0-not-OK"
for _n in crash good clean; do "$_kv/ksvc" stop-supervised "$_n" >/dev/null; done
if [ -e "$_kv/run/kdos-svc.crash.state" ]; then _kfail="$_kfail state-left-by-stop"; fi
if [ -z "$_kfail" ]; then
    echo "  a daemon that exits non-zero is [FAIL]; a running one and one exit 0 are [ OK ]"
else
    echo "  KSVC REPORTS A CRASH LOOP WRONGLY:$_kfail"
    cat "$_kv/out"
    exit 1
fi

#
# ONE SUPERVISOR PER NAME, AND A STOP THAT WAITS FOR THE DAEMON. A second
# `supervise` of a live name would take over the pid file, and the first
# supervisor and its daemon would run on with nothing that can stop them.
# `stop` waits for the process group to empty, up to the name's stop grace:
# a daemon that exits at once is stopped at once, one that needs two seconds
# for its shutdown gets them, and one that ignores SIGTERM is killed when the
# grace runs out.
#
echo "==> a running service is supervised once, and stop waits out its grace"
_kfail=""
# Microseconds from bash itself: busybox date has no %N.
_now() { echo "${EPOCHREALTIME/./}"; }
_ms() { echo $(( ($(_now) - $1) / 1000 )); }
"$_kv/ksvc" supervise twice sleep 600 >/dev/null
_p1=$(cat "$_kv/run/twice.pid")
"$_kv/ksvc" supervise twice sleep 600 > "$_kv/out"
grep -q '^\[SKIP\] twice: already supervised' "$_kv/out" || _kfail="$_kfail no-skip"
[ "$(cat "$_kv/run/twice.pid")" = "$_p1" ] || _kfail="$_kfail pid-file-taken"
sleep 0.3
_t=$(_now)
"$_kv/ksvc" stop-supervised twice >/dev/null
[ "$(_ms "$_t")" -lt 800 ] || _kfail="$_kfail quick-stop-$(_ms "$_t")ms"
kill -0 "$_p1" 2>/dev/null && _kfail="$_kfail first-supervisor-left"
"$_kv/ksvc" supervise twice sleep 600 | grep -q 'Supervisor started' \
    || _kfail="$_kfail no-start-after-stop"
"$_kv/ksvc" stop-supervised twice >/dev/null
"$_kv/ksvc" supervise --stop-grace 10 slow \
    sh -c 'trap "sleep 2; exit 0" TERM; while :; do sleep 0.1; done' >/dev/null
sleep 0.3
_d=$(awk '{print $2}' "$_kv/run/kdos-svc.slow.state")
_t=$(_now)
"$_kv/ksvc" stop-supervised slow > "$_kv/out"
_e=$(_ms "$_t")
{ [ "$_e" -ge 1800 ] && [ "$_e" -lt 5000 ]; } || _kfail="$_kfail slow-stop-${_e}ms"
grep -q 'killing' "$_kv/out" && _kfail="$_kfail slow-killed"
kill -0 "$_d" 2>/dev/null && _kfail="$_kfail slow-daemon-left"
"$_kv/ksvc" supervise --stop-grace 1 deaf \
    sh -c 'trap "" TERM; while :; do sleep 0.1; done' >/dev/null
sleep 0.3
_d=$(awk '{print $2}' "$_kv/run/kdos-svc.deaf.state")
_t=$(_now)
"$_kv/ksvc" stop-supervised deaf > "$_kv/out"
_e=$(_ms "$_t")
{ [ "$_e" -ge 900 ] && [ "$_e" -lt 2500 ]; } || _kfail="$_kfail deaf-stop-${_e}ms"
grep -q 'Still running after 1s, killing' "$_kv/out" || _kfail="$_kfail deaf-not-killed"
sleep 0.2
kill -0 "$_d" 2>/dev/null && _kfail="$_kfail deaf-daemon-left"
if [ -z "$_kfail" ]; then
    echo "  a second supervise is [SKIP]; stop waits for the daemon, up to its grace, then kills"
else
    echo "  KSVC SUPERVISES TWICE OR STOPS WRONGLY:$_kfail"
    cat "$_kv/out"
    exit 1
fi

#
# A SUPERVISOR NEVER LEAVES ITS DAEMON BEHIND UNNOTICED. SIGTERM to the
# supervisor alone is passed on to its group, so the daemon goes with it; a
# SIGTERM `stop` sent the group reaches the daemon once, not twice (some
# daemons take a second as "exit now"). A supervisor that SIGKILL ended
# leaves its daemon running where no live supervisor names it: `check` calls
# that [FAIL], `supervise` refuses to start a second beside it after waiting
# out the grace, and `stop-supervised` ends it.
#
echo "==> a supervisor's daemon is never orphaned silently, and gets one SIGTERM"
_kfail=""
"$_kv/ksvc" supervise direct sleep 600 >/dev/null
sleep 0.3
_s=$(cat "$_kv/run/direct.pid"); _d=$(awk '{print $2}' "$_kv/run/kdos-svc.direct.state")
kill -TERM "$_s"
sleep 0.3
kill -0 "$_d" 2>/dev/null && _kfail="$_kfail direct-term-orphaned"
"$_kv/ksvc" check direct | grep -q '^\[DOWN\] direct' || _kfail="$_kfail direct-not-DOWN"
# The daemon counts its TERMs for a second after the first. Two TERMs that
# arrive together are one pending signal and would count as one, so the
# supervisor is held stopped while `stop` signals the group and continued
# once the daemon has taken that TERM: one it passed on would then be a second.
_trap="n=0; trap 'n=\$((n+1)); echo t >> $_kv/terms' TERM; while [ \$n -lt 1 ]; do sleep 0.05; done; i=0; while [ \$i -lt 20 ]; do sleep 0.05; i=\$((i+1)); done"
"$_kv/ksvc" supervise once bash -c "$_trap" >/dev/null
sleep 0.3
: > "$_kv/terms"
_s=$(cat "$_kv/run/once.pid")
kill -STOP "$_s"
"$_kv/ksvc" stop-supervised once >/dev/null &
_sp=$!
sleep 0.3
kill -CONT "$_s"
wait "$_sp"
[ "$(wc -l < "$_kv/terms")" = 1 ] || _kfail="$_kfail stop-sent-$(wc -l < "$_kv/terms")-TERMs"
"$_kv/ksvc" supervise once bash -c "$_trap" >/dev/null
sleep 0.3
: > "$_kv/terms"
kill -TERM "$(cat "$_kv/run/once.pid")"
sleep 1.4
[ "$(wc -l < "$_kv/terms")" = 1 ] || _kfail="$_kfail kill-passed-$(wc -l < "$_kv/terms")-TERMs"
"$_kv/ksvc" supervise --stop-grace 1 orphan sleep 600 >/dev/null
sleep 0.3
_s=$(cat "$_kv/run/orphan.pid"); _d=$(awk '{print $2}' "$_kv/run/kdos-svc.orphan.state")
kill -KILL "$_s"
sleep 0.2
"$_kv/ksvc" check orphan > "$_kv/out" && _kfail="$_kfail orphan-check-exits-0"
grep -q "^\[FAIL\] orphan  (supervisor gone; daemon pid $_d" "$_kv/out" \
    || _kfail="$_kfail orphan-not-FAIL"
"$_kv/ksvc" supervise orphan sleep 600 > "$_kv/out" && _kfail="$_kfail orphan-second-started"
grep -q '^\[FAIL\] orphan: daemon pid' "$_kv/out" || _kfail="$_kfail orphan-no-refusal"
"$_kv/ksvc" stop-supervised orphan > "$_kv/out"
sleep 0.1
kill -0 "$_d" 2>/dev/null && _kfail="$_kfail orphan-left-by-stop"
"$_kv/ksvc" supervise orphan sleep 600 | grep -q 'Supervisor started' \
    || _kfail="$_kfail no-start-after-orphan-stop"
"$_kv/ksvc" stop-supervised orphan >/dev/null
if [ -z "$_kfail" ]; then
    echo "  a killed supervisor takes its daemon along; an orphan is [FAIL], refused a twin, and stoppable"
else
    echo "  KSVC ORPHANS A DAEMON OR SIGNALS IT TWICE:$_kfail"
    cat "$_kv/out"
    exit 1
fi

#
# --restart-with FOLLOWS ANOTHER SERVICE'S DAEMON. NetworkManager does not
# exit when the system bus goes away, so its supervisor restarts it when the
# bus's daemon changes, once the bus is back, and not as a failure. A crash of
# the dependent alone restarts nothing else.
#
echo "==> --restart-with restarts a daemon with the service it names"
_kfail=""
# The dependent fails its first start, so its state carries exit code 3 and
# one failure into the run the bus restart ends: the restart's wait must still
# read [ OK ], and the count must stay at one.
"$_kv/ksvc" supervise bus sleep 600 >/dev/null
sleep 0.3
"$_kv/ksvc" supervise --restart-with bus dep sh -c \
    "[ -e '$_kv/dep-ran' ] || { : > '$_kv/dep-ran'; exit 3; }; exec sleep 600" >/dev/null
_i=0
while [ "$_i" -lt 120 ]; do
    read -r _ph _d _sn _fl _ls _rest 2>/dev/null < "$_kv/run/kdos-svc.dep.state" || :
    [ "$_ph" = up ] && [ "$_ls" = 3 ] && break
    sleep 0.1; _i=$((_i + 1))
done
[ "$_i" -lt 120 ] || _kfail="$_kfail dep-never-up-after-its-failure"
_b=$(awk '{print $2}' "$_kv/run/kdos-svc.bus.state")
kill "$_b"
sleep 1
kill -0 "$_d" 2>/dev/null && _kfail="$_kfail dep-left-on-old-bus"
"$_kv/ksvc" check dep | grep -q '^\[ OK \] dep' || _kfail="$_kfail restart-wait-not-OK"
_i=0
while [ "$_i" -lt 120 ]; do
    read -r _ph _dp _sn _fl _rest < "$_kv/run/kdos-svc.dep.state"
    [ "$_ph" = up ] && [ "$_dp" != "$_d" ] && break
    sleep 0.1; _i=$((_i + 1))
done
[ "$_i" -lt 120 ] || _kfail="$_kfail dep-not-restarted"
[ "$_fl" = 1 ] || _kfail="$_kfail bounce-counted-as-failure"
_b2=$(awk '{print $2}' "$_kv/run/kdos-svc.bus.state")
[ "$_b2" != "$_b" ] || _kfail="$_kfail bus-not-restarted"
kill -0 "$_b2" 2>/dev/null || _kfail="$_kfail dep-started-before-bus"
"$_kv/ksvc" check dep | grep -q '^\[ OK \] dep' || _kfail="$_kfail dep-not-OK"
grep -q 'bus restarted or stopped: restarting dep' "$_kv/run/kdos-svc.dep.log" \
    || _kfail="$_kfail no-bounce-line"
"$_kv/ksvc" stop-supervised dep >/dev/null
"$_kv/ksvc" stop-supervised bus >/dev/null
if [ -z "$_kfail" ]; then
    echo "  the dependent is restarted after the bus is back, and not counted as a failure"
else
    echo "  KSVC --restart-with IS WRONG:$_kfail"
    cat "$_kv/run/kdos-svc.dep.log"
    exit 1
fi

#
# A `service list` ROW IS ONE LINE. A script's status is free-form: nothing,
# or several lines. Printed as it comes, a silent one joins the next row and a
# long one spills into it.
#
echo "==> service list prints one line per service"
mkdir -p "$_kv/init.d" "$_kv/off"
printf '#!/bin/sh\n' > "$_kv/init.d/02_quiet.sh"
printf '#!/bin/sh\necho "  one: fine"\n' > "$_kv/init.d/10_single.sh"
printf '#!/bin/sh\necho a; echo; echo b; echo c\n' > "$_kv/init.d/20_many.sh"
chmod 755 "$_kv/init.d"/*.sh
$CC $STD $WARN $INC -Isrc/system/kdos-tools -DRUN_DIR="\"$_kv/run\"" \
    -DINIT_DIR="\"$_kv/init.d\"" -DDISABLED_DIR="\"$_kv/off\"" \
    -o "$_kv/ksvc-list" "$_kv/main.c" src/system/kdos-tools/ksvc.c \
    src/libs/libkbase/*.c
"$_kv/ksvc-list" list > "$_kv/out"
_kfail=""
grep -qx '  quiet                \[ENABLED\]    —' "$_kv/out" || _kfail="$_kfail silent-not-dash"
grep -qx '  single               \[ENABLED\]    one: fine' "$_kv/out" || _kfail="$_kfail single"
grep -qx '  many                 \[ENABLED\]    a (+2 more: service status many)' "$_kv/out" \
    || _kfail="$_kfail many"
[ "$(grep -c '^  [a-z]' "$_kv/out")" = 3 ] || _kfail="$_kfail row-count"
if [ -z "$_kfail" ]; then
    echo "  a silent script is —, a multi-line one its first line and a count"
else
    echo "  SERVICE LIST ROWS ARE BROKEN:$_kfail"
    cat "$_kv/out"
    exit 1
fi

#
# A USER'S TABLE IS THE USER'S, AND ROOT NEVER RUNS ONE. KSVC_RUN_DIR (what
# the turnstile backend exports) or `--user` moves every pid, state and log
# file into the user's run directory, which must be the caller's and writable
# by nobody else, because its pid files decide what `stop` signals. The system
# table stays the default and is still readable from there; a supervisor that
# cannot write its pid file where `check` looks is refused instead of started
# unfindable; and the daemon does not inherit KSVC_RUN_DIR, or every `service`
# call the session bus's programs make would land in the user's table. Root
# ignores the variable and refuses `--user`: a root tool must not take the
# path of the pid file it kills from its caller.
#
echo "==> a user's service table lives in the user's run directory, and root runs none"
mkdir -p "$_kv/user.d" "$_kv/xdg" "$_kv/home"
chmod 700 "$_kv/xdg"
printf '#!/bin/sh\nexec ksvc check "%s"\n' u1 > "$_kv/user.d/10_u1.sh"
printf '#!/bin/sh\necho idle\n' > "$_kv/user.d/20_idle.sh"
chmod 755 "$_kv/user.d"/*.sh
$CC $STD $WARN $INC -Isrc/system/kdos-tools -DRUN_DIR="\"$_kv/run\"" \
    -DUSER_INIT_DIR="\"$_kv/user.d\"" \
    -o "$_kv/ksvc-user" "$_kv/main.c" src/system/kdos-tools/ksvc.c \
    src/libs/libkbase/*.c
ln -sf ksvc-user "$_kv/ksvc"
_kfail=""
_uenv="HOME=$_kv/home XDG_CONFIG_HOME=$_kv/home/.config XDG_RUNTIME_DIR=$_kv/xdg"
if [ "$(id -u)" -ne 0 ]; then
    env $_uenv KSVC_RUN_DIR="$_kv/xdg/ksvc" "$_kv/ksvc" supervise u1 sleep 600 >/dev/null \
        || _kfail="$_kfail env-supervise"
    [ -e "$_kv/xdg/ksvc/u1.pid" ] || _kfail="$_kfail no-user-pidfile"
    [ -e "$_kv/run/u1.pid" ] && _kfail="$_kfail pidfile-in-system-table"
    sleep 0.3
    _d=$(awk '{print $2}' "$_kv/xdg/ksvc/kdos-svc.u1.state")
    tr '\0' '\n' < "/proc/$_d/environ" | grep -q '^KSVC_RUN_DIR=' \
        && _kfail="$_kfail daemon-inherits-KSVC_RUN_DIR"
    "$_kv/ksvc" check u1 | grep -q '^\[DOWN\] u1' || _kfail="$_kfail system-table-sees-user"
    env $_uenv "$_kv/ksvc" --user check u1 | grep -q '^\[ OK \] u1' \
        || _kfail="$_kfail --user-not-xdg-ksvc"
    # `service --user list` runs the user's scripts, and their own plain
    # `ksvc check` has to reach the same table.
    env $_uenv PATH="$_kv:$PATH" "$_kv/ksvc" --user list > "$_kv/out"
    grep -q '^  u1 .*\[ENABLED\].*\[ OK \] u1' "$_kv/out" || _kfail="$_kfail list-child-not-user"
    grep -q '^KDOS User Services' "$_kv/out" || _kfail="$_kfail list-title"
    env $_uenv "$_kv/ksvc" --user disable idle >/dev/null
    [ -e "$_kv/home/.config/kdos/user-services.disabled/idle" ] || _kfail="$_kfail disable-path"
    env $_uenv PATH="$_kv:$PATH" "$_kv/ksvc" --user list \
        | grep -q '^  idle .*\[DISABLED\]' || _kfail="$_kfail disabled-not-listed"
    env $_uenv "$_kv/ksvc" --user stop-supervised u1 >/dev/null
    [ -e "$_kv/xdg/ksvc/u1.pid" ] && _kfail="$_kfail user-stop-left-pidfile"
    kill -0 "$_d" 2>/dev/null && _kfail="$_kfail user-daemon-left"
    KSVC_RUN_DIR=relative "$_kv/ksvc" check u1 >/dev/null 2>&1 && _kfail="$_kfail relative-accepted"
    KSVC_RUN_DIR=/ "$_kv/ksvc" check u1 >/dev/null 2>&1 && _kfail="$_kfail foreign-dir-accepted"
    mkdir -p "$_kv/open" && chmod 777 "$_kv/open"
    KSVC_RUN_DIR="$_kv/open" "$_kv/ksvc" check u1 >/dev/null 2>&1 && _kfail="$_kfail shared-dir-accepted"
    chmod 555 "$_kv/run"
    "$_kv/ksvc" supervise ro sleep 1 > /dev/null 2>&1 && _kfail="$_kfail unwritable-table-supervised"
    chmod 755 "$_kv/run"
    [ -e "$_kv/run/ro.pid" ] && _kfail="$_kfail unwritable-left-pidfile"
else
    KSVC_RUN_DIR="$_kv/xdg/ksvc" "$_kv/ksvc" supervise r1 sleep 600 >/dev/null
    [ -e "$_kv/run/r1.pid" ] || _kfail="$_kfail root-took-KSVC_RUN_DIR"
    "$_kv/ksvc" stop-supervised r1 >/dev/null
    env $_uenv "$_kv/ksvc" --user check r1 >/dev/null 2>&1 && _kfail="$_kfail root-ran---user"
fi
if [ -z "$_kfail" ]; then
    echo "  a user's files are in its own run directory, the daemon is clean, and root runs none"
else
    echo "  KSVC'S USER TABLE IS WRONG:$_kfail"
    cat "$_kv/out" 2>/dev/null
    exit 1
fi

#
# THE TURNSTILE BACKEND STARTS A USER'S SERVICES IN ORDER, AND READY. Each
# script is asked `ready` before the next starts, so a service whose
# dependency is not yet answering is never started into the gap; the FIFO,
# which releases the login, is written only after the last; a disabled
# service is skipped; and `stop` stops the table backwards before it ends the
# manager, because turnstile clears the runtime directory the moment the
# manager is gone. A system account — the SDDM greeter — gets its FIFO at once
# and nothing started.
#
echo "==> the turnstile backend starts the user services in order and ready, and stops them backwards"
_bk="$OUT/backend"
mkdir -p "$_bk/svc" "$_bk/home/.config/kdos/user-services.disabled" "$_bk/rt"
chmod 700 "$_bk/rt"
cat > "$_bk/svc/10_one.sh" <<'BKEOF'
#!/bin/bash
case "$1" in
start)  ksvc supervise one sh -c 'sleep 0.4; : > "$XDG_RUNTIME_DIR/one.ready"; exec sleep 600' ;;
stop)   echo "stop one" >> "$XDG_RUNTIME_DIR/order"; ksvc stop-supervised one ;;
status) ksvc check one ;;
ready)  ksvc check one >/dev/null 2>&1 || exit 0
        [ -e "$XDG_RUNTIME_DIR/one.ready" ] || exit 75 ;;
esac
BKEOF
cat > "$_bk/svc/20_two.sh" <<'BKEOF'
#!/bin/bash
case "$1" in
start)  [ -e "$XDG_RUNTIME_DIR/one.ready" ] && echo "two after one" >> "$XDG_RUNTIME_DIR/order"
        ksvc supervise --restart-with one two sleep 600 ;;
stop)   echo "stop two" >> "$XDG_RUNTIME_DIR/order"; ksvc stop-supervised two ;;
status) ksvc check two ;;
ready)  exit 0 ;;
esac
BKEOF
printf '#!/bin/bash\n[ "$1" = start ] && echo off >> "$XDG_RUNTIME_DIR/order"; exit 0\n' \
    > "$_bk/svc/30_off.sh"
chmod 755 "$_bk/svc"/*.sh
: > "$_bk/home/.config/kdos/user-services.disabled/off"
_bfail=""
# The host's /etc/profile is not the image's and may set its own PATH, so the
# backend is pointed at none. A FIFO read is bounded: a backend that died
# before writing it must fail this, not hang it.
_benv="PATH=$_kv:/usr/bin:/bin KDOS_USER_SERVICES=$_bk/svc KDOS_USER_PROFILE=/dev/null HOME=$_bk/home XDG_RUNTIME_DIR=$_bk/rt"
_fifo() { timeout 20 cat "$1" | tr -d '\0'; }
mkfifo "$_bk/ready"
if [ "$(id -u)" -ne 0 ]; then
    env $_benv KDOS_USER_UID_MIN=0 bash fs/usr/lib/turnstile/ksvc run "$_bk/ready" "$_bk/srv" /etc/turnstile/backend &
    _bm=$!
    _rs=$(_fifo "$_bk/ready")
    [ "$_rs" = ksvc ] || _bfail="$_bfail ready-string-$_rs"
    grep -qx 'two after one' "$_bk/rt/order" 2>/dev/null || _bfail="$_bfail not-ordered-by-ready"
    grep -qx off "$_bk/rt/order" 2>/dev/null && _bfail="$_bfail disabled-started"
    env $_benv KSVC_RUN_DIR="$_bk/rt/ksvc" "$_kv/ksvc" check two | grep -q '^\[ OK \] two' \
        || _bfail="$_bfail two-not-up"
    kill -0 "$_bm" 2>/dev/null || _bfail="$_bfail no-manager"
    env -i $_benv bash fs/usr/lib/turnstile/ksvc stop "$_bm"
    wait "$_bm" 2>/dev/null || :
    [ "$(grep '^stop' "$_bk/rt/order" | tr '\n' ' ')" = "stop two stop one " ] \
        || _bfail="$_bfail not-stopped-backwards"
    [ -e "$_bk/rt/ksvc/one.pid" ] && _bfail="$_bfail one-left"
    kill -0 "$_bm" 2>/dev/null && _bfail="$_bfail manager-left"
else
    env $_benv bash fs/usr/lib/turnstile/ksvc run "$_bk/ready" "$_bk/srv" /etc/turnstile/backend &
    _bm=$!
    _rs=$(_fifo "$_bk/ready")
    [ "$_rs" = none ] || _bfail="$_bfail system-account-ready-$_rs"
    [ -e "$_bk/rt/order" ] && _bfail="$_bfail system-account-got-services"
    env -i $_benv bash fs/usr/lib/turnstile/ksvc stop "$_bm"
    wait "$_bm" 2>/dev/null || :
fi
# A system account, whichever user runs this: the floor above every uid.
rm -f "$_bk/rt/order"
mkfifo "$_bk/ready2"
env $_benv KDOS_USER_UID_MIN=65535 bash fs/usr/lib/turnstile/ksvc run "$_bk/ready2" "$_bk/srv" /etc/turnstile/backend &
_bm=$!
[ "$(_fifo "$_bk/ready2")" = none ] || _bfail="$_bfail greeter-not-released"
[ -e "$_bk/rt/order" ] && _bfail="$_bfail greeter-got-services"
kill "$_bm" 2>/dev/null
wait "$_bm" 2>/dev/null || :
if [ -z "$_bfail" ]; then
    echo "  started in order and ready, disabled skipped, stopped backwards; a system account gets nothing"
else
    echo "  THE TURNSTILE BACKEND IS WRONG:$_bfail"
    cat "$_bk/rt/order" "$_bk/rt/ksvc/kdos-user-services.log" 2>/dev/null
    exit 1
fi

#
# A PER-USER TIMER REPEATS, AND STOPPING THE SERVICE LEAVES NOTHING. `snooze`
# waits for its slot, runs the command ONCE and exits, so the runner keeps one
# loop per row that starts it again; a row whose file is removed — a reminder
# that has fired — ends its loop; a row snooze refuses, or whose command is
# not installed, is reported and skipped; SIGHUP reads the tables again,
# SIGUSR1 only the files that have no loops yet; and a SIGTERM ends every loop
# and the snooze or job it is waiting on, which is what the turnstile backend's
# stop of the `timers` service does at the last logout. snooze is a stub that
# fires at once and logs what it was asked to run.
#
echo "==> a per-user timer repeats, a removed row ends, and the runner's TERM leaves no snooze"
_ut="$OUT/usertimers"
mkdir -p "$_ut/bin" "$_ut/sys" "$_ut/home/.config/kdos/timers.d" "$_ut/rt"
cat > "$_ut/bin/snooze" <<'UTEOF'
#!/bin/bash
if [ "$1" = -n ]; then shift; case " $* " in *" 99 "*) exit 1 ;; esac; exit 0; fi
while [ $# -gt 0 ]; do case "$1" in -*) shift 2 ;; *) break ;; esac; done
echo "$*" >> "$UT_LOG"
sleep 0.2
exec "$@"
UTEOF
chmod 755 "$_ut/bin/snooze"
printf 'sysrow -H * -M /10 -- true sys\n' > "$_ut/sys/a.timer"
printf '# a comment\nmine -H * -- true mine\nbad -H 99 -- true bad\ngone -H 1 -- no-such-command-here\n' \
    > "$_ut/home/.config/kdos/timers.d/mine.timer"
_utenv="PATH=$_ut/bin:$PATH HOME=$_ut/home XDG_CONFIG_HOME=$_ut/home/.config XDG_RUNTIME_DIR=$_ut/rt UT_LOG=$_ut/log KDOS_USER_TIMERS_SYSTABLE=$_ut/sys KDOS_TIMERS_LIB=$PWD/fs/usr/local/lib/kdos/timers.sh"
env $_utenv bash fs/usr/local/lib/kdos/user-timers > "$_ut/out" 2>&1 &
_ur=$!
_tfail=""
sleep 3
[ "$(grep -c '^true mine$' "$_ut/log")" -ge 2 ] || _tfail="$_tfail no-repeat"
grep -q '^true sys$' "$_ut/log" || _tfail="$_tfail system-row-not-run"
grep -q "bad: snooze refuses '-H 99'; skipped" "$_ut/out" || _tfail="$_tfail refused-row-silent"
grep -q 'gone needs no-such-command-here' "$_ut/out" || _tfail="$_tfail missing-command-silent"
grep -q 'bad$' "$_ut/log" && _tfail="$_tfail refused-row-ran"
[ "$(cat "$_ut/rt/kdos/user-timers.pid" 2>/dev/null)" = "$_ur" ] || _tfail="$_tfail no-pidfile"
rm "$_ut/home/.config/kdos/timers.d/mine.timer"
sleep 1.6
_n=$(grep -c '^true mine$' "$_ut/log")
sleep 1.4
[ "$(grep -c '^true mine$' "$_ut/log")" = "$_n" ] || _tfail="$_tfail removed-row-still-runs"
printf 'later -H * -- true later\n' > "$_ut/home/.config/kdos/timers.d/later.timer"
kill -HUP "$_ur"
sleep 1
grep -q '^true later$' "$_ut/log" || _tfail="$_tfail hup-not-reread"
# USR1 starts a file with no loops yet and restarts nothing: a job that is
# running when the next file arrives — what `kdos remind` signals — keeps its
# one run and its pid.
printf 'slow -H * -- sleep 41.25\n' > "$_ut/home/.config/kdos/timers.d/slow.timer"
kill -USR1 "$_ur"
sleep 1
_sp=$(pgrep -fx 'sleep 41.25')
[ -n "$_sp" ] || _tfail="$_tfail usr1-not-started"
printf 'next -H * -- true next\n' > "$_ut/home/.config/kdos/timers.d/next.timer"
kill -USR1 "$_ur"
sleep 1
grep -q '^true next$' "$_ut/log" || _tfail="$_tfail usr1-second-not-started"
[ "$(grep -c '^sleep 41.25$' "$_ut/log")" = 1 ] || _tfail="$_tfail usr1-restarted-a-job"
[ -n "$_sp" ] && [ "$(pgrep -fx 'sleep 41.25')" = "$_sp" ] || _tfail="$_tfail usr1-job-pid-changed"
kill -TERM "$_ur"
wait "$_ur" 2>/dev/null
sleep 0.3
pgrep -f "$_ut/bin/snooze" >/dev/null && _tfail="$_tfail snooze-left"
pgrep -fx 'sleep 41.25' >/dev/null && _tfail="$_tfail job-left"
pgrep -f 'lib/kdos/user-timers' >/dev/null && _tfail="$_tfail loop-left"
[ -e "$_ut/rt/kdos/user-timers.pid" ] && _tfail="$_tfail pidfile-left"
if [ -z "$_tfail" ]; then
    echo "  every row repeats, a removed or refused one does not, HUP re-reads, USR1 adds, TERM leaves nothing"
else
    echo "  THE PER-USER TIMER RUNNER IS WRONG:$_tfail"
    cat "$_ut/out"
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
env $_tenv "$OUT/kdos" app tui add "Disk usage" ncdu > "$_tw/add.out" 2>&1
grep -q '^kdos-tui-disk-usage$' "$_tw/add.out" \
    || { echo "  add answered: $(cat "$_tw/add.out")"; exit 1; }
_te="$_tapps/kdos-tui-disk-usage.desktop"
[ -f "$_te" ] || { echo "  no entry was written"; ls -R "$_tw"; exit 1; }
grep -qx 'Terminal=true' "$_te" || { echo "  the entry is not a terminal one"; exit 1; }
grep -qx 'X-KDOS-TUI=true' "$_te" || { echo "  no marker key"; exit 1; }
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
# AN OPTION THE VERB DOES NOT HAVE IS REFUSED, not written into the entry.
env $_tenv "$OUT/kdos" app tui add "Too small" x --size 2x1 >/dev/null 2>&1 \
    && { echo "  an unknown option was accepted"; exit 1; }
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
# WITH THIS USER'S TIMER RUNNER ALIVE, the runner is told with SIGUSR1 and no
# snooze of the command's own is started; SIGHUP would restart every job.
mkdir -p "$_rw/run/kdos"
sh -c 'trap "echo usr1 >> \"$1\"; exit 0" USR1; trap "echo hup >> \"$1\"" HUP
       while :; do sleep 0.1; done' runner "$_rw/runner.got" &
_rpid=$!
echo "$_rpid" > "$_rw/run/kdos/user-timers.pid"
: > "$_rw/armed"
# shellcheck disable=SC2086
env $_renv XDG_RUNTIME_DIR="$_rw/run" "$OUT/kdos" remind in 2h kettle >/dev/null 2>&1
for _i in 1 2 3 4 5 6 7 8 9 10; do [ -s "$_rw/runner.got" ] && break; sleep 0.2; done
kill "$_rpid" 2>/dev/null || true; wait "$_rpid" 2>/dev/null || true
[ "$(cat "$_rw/runner.got" 2>/dev/null)" = usr1 ] \
    || { echo "  the runner was not sent SIGUSR1 alone: $(cat "$_rw/runner.got" 2>/dev/null)"; exit 1; }
[ -s "$_rw/armed" ] \
    && { echo "  a snooze was started beside a live runner"; exit 1; }
rm -f "$_rw/run/kdos/user-timers.pid"
for _f in "$_rt"/remind-*.timer; do grep -q '^# kettle$' "$_f" && rm -f "$_f"; done

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
# A tree without the fs/ overlay builds a root with no configuration.
RBT="$OUT/rb-tree"
mkdir -p "$RBT/script" "$RBT/ports/core" "$RBT/src/devtools/kdosbuild"
: > "$RBT/script/kdosbuild.sh"
rb "$RBT" --dry-run "$RBW" >/dev/null 2>&1 \
    && { echo "  a tree with no fs/ overlay was accepted"; exit 1; }
mkdir -p "$RBT/fs/etc"
rb "$RBT" --dry-run "$RBW" >/dev/null 2>&1 \
    || { echo "  a tree with every part was not recognised"; exit 1; }
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
# like 090_initramfs.sh's output -- an uncompressed cpio carrying both vendors'
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
echo "==> a bad update led by the menu boots three times and rolls itself back"
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
echo "==> each slot boots its own kernel, and the menu follows the state"
# A fixture ESP laid out the way kinstall leaves one — the state file at
# EFI/kdos/bootstate is what tells kdos-bootctl where the ESP is — starting
# from the flat EFI/kdos pair both slots boot before any deploy. The kernels
# are bzImage headers carrying a version string and nothing else.
ES="$OUT/esp-ab"
rm -rf "$ES"
mkdir -p "$ES/esp/EFI/kdos" "$ES/ra/boot" "$ES/rb/boot"
esb() { env KDOS_BOOTSTATE="$ES/esp/EFI/kdos/bootstate" "$OUT/kdos-bootctl" "$@"; }
mkbz() {
    python3 - "$1" "$2" <<'PYEOF'
import sys
b = bytearray(0x400)
b[0x202:0x206] = b'HdrS'
b[0x20E] = 0x100 & 255; b[0x20F] = 0x100 >> 8
v = (sys.argv[2] + ' (kdos@kdos) #1').encode()
b[0x300:0x300 + len(v)] = v
open(sys.argv[1], 'wb').write(b)
PYEOF
}
mkbz "$ES/esp/EFI/kdos/vmlinuz" 7.2.6
echo flat > "$ES/esp/EFI/kdos/initramfs.cpio.gz"
mkbz "$ES/ra/boot/vmlinuz-kdos" 7.2.6
echo image-a > "$ES/ra/boot/initramfs.cpio.gz"
mkbz "$ES/rb/boot/vmlinuz-kdos" 7.2.7
echo image-b > "$ES/rb/boot/initramfs.cpio.gz"
echo kernel-b > "$ES/rb/boot/initramfs-kdos.cpio.gz"
{
    printf 'timeout: 10\ndefault_entry: 1\n\n'
    for e in "KDOS|quiet loglevel=3" "KDOS (verbose)|loglevel=7" \
             "KDOS (single user)|loglevel=7 single"; do
        printf '/%s\n    protocol: linux\n    path: boot():/EFI/kdos/vmlinuz\n' "${e%%|*}"
        printf '    module_path: boot():/EFI/kdos/initramfs.cpio.gz\n'
        printf '    cmdline: bootstate=UUID=ESP-1 root=UUID=AAAA-1111 rw nomodeset %s\n\n' "${e#*|}"
    done
    printf '/Memory Test (memtest86+)\n    protocol: efi\n    path: boot():/EFI/kdos/memtest.efi\n'
} > "$ES/esp/limine.conf"
cp "$ES/esp/limine.conf" "$ES/flat.conf"
esb set-slot a AAAA-1111 >/dev/null
esb set-slot b BBBB-2222 >/dev/null
# A menu with no per-slot directory is left exactly as it is: that is what
# keeps an ESP no deploy has touched booting.
cmp -s "$ES/flat.conf" "$ES/esp/limine.conf" \
    || { echo "  a menu with no per-slot kernel was rewritten"; exit 1; }
esb deploy "$ES/rb" b >/dev/null || { echo "  deploy failed"; exit 1; }
test "$(cat "$ES/esp/EFI/kdos/b/initramfs.cpio.gz")" = kernel-b \
    || { echo "  deploy took the image's initramfs over the kernel's own"; exit 1; }
esb status | grep -q "kernel   b  7.2.7 from EFI/kdos/b" \
    || { echo "  status does not show slot b's own kernel"; exit 1; }
# Slot A still has no directory: its entries stay on the flat pair, and the
# pair stays on the ESP.
grep -A3 '^/KDOS$' "$ES/esp/limine.conf" | grep -q 'path: boot():/EFI/kdos/vmlinuz' \
    || { echo "  slot a lost the flat kernel it boots"; exit 1; }
grep -A5 '^/KDOS (slot b)$' "$ES/esp/limine.conf" | grep -q 'kdos_slot=b .*root=UUID=BBBB-2222 rw nomodeset quiet' \
    || { echo "  slot b's entry is missing or lost the hand-added words"; exit 1; }
grep -q '^/Memory Test' "$ES/esp/limine.conf" \
    || { echo "  an entry that is not ours was dropped"; exit 1; }

# The candidate leads the menu for its attempts; the last attempt hands the
# lead back, so its failure rolls back from the confirmed slot's own kernel.
esb try b >/dev/null || { echo "  try failed"; exit 1; }
first_path() { grep -m1 'path: boot()' "$ES/esp/limine.conf"; }
first_path | grep -q '/EFI/kdos/b/vmlinuz' || { echo "  the candidate does not lead the menu"; exit 1; }
esb select b >/dev/null 2>&1; esb select b >/dev/null 2>&1
first_path | grep -q '/EFI/kdos/b/vmlinuz' || { echo "  the lead moved with attempts left"; exit 1; }
test "$(esb select b 2>/dev/null)" = BBBB-2222 || { echo "  the last attempt did not boot b"; exit 1; }
first_path | grep -q '/EFI/kdos/vmlinuz' || { echo "  the last attempt left the lead on b"; exit 1; }
test "$(esb select a 2>/dev/null)" = AAAA-1111 || { echo "  no rollback onto a's kernel"; exit 1; }
esb status | grep -q "trying   nothing" || { echo "  the rollback left a try flag"; exit 1; }

# The other entry picked by hand boots its own slot, because the kernel is that
# slot's: the confirmed slot abandons a candidate, anything else counts nothing.
esb try b >/dev/null
test "$(esb select a 2>/dev/null)" = AAAA-1111 || { echo "  a hand-picked a did not boot a"; exit 1; }
esb status | grep -q "trying   nothing" || { echo "  picking the confirmed slot kept the candidate"; exit 1; }
test "$(esb select b 2>/dev/null)" = BBBB-2222 || { echo "  a hand-picked b did not boot b"; exit 1; }
esb mark-good >/dev/null
esb status | grep -q "^active   a" || { echo "  a one-off boot was confirmed"; exit 1; }

# Both slots in directories of their own: the flat pair nothing names goes.
esb deploy "$ES/ra" a >/dev/null || { echo "  deploy a failed"; exit 1; }
test ! -e "$ES/esp/EFI/kdos/vmlinuz" || { echo "  the unnamed flat kernel stayed"; exit 1; }
test "$(cat "$ES/esp/EFI/kdos/a/initramfs.cpio.gz")" = image-a \
    || { echo "  deploy did not fall back to the image's initramfs"; exit 1; }
cp "$ES/esp/limine.conf" "$ES/settled.conf"
esb mark-good >/dev/null
cmp -s "$ES/settled.conf" "$ES/esp/limine.conf" || { echo "  an unchanged state rewrote the menu"; exit 1; }
# The splash's motion is kdos.splash= on every /KDOS entry and on nothing
# else, and a menu regenerated from the state keeps it: kdos-splash reads it
# before any root is mounted, so the menu is the only place it can live.
KDOS_LIMINE_CONF="$ES/esp/limine.conf" esb splash phosphor >/dev/null \
    || { echo "  splash phosphor failed"; exit 1; }
test "$(grep -c '^    cmdline: kdos_slot=.* kdos.splash=phosphor$' "$ES/esp/limine.conf")" = \
     "$(grep -c '^    cmdline: kdos_slot=' "$ES/esp/limine.conf")" \
    || { echo "  an entry of ours lacks kdos.splash=phosphor"; cat "$ES/esp/limine.conf"; exit 1; }
grep -A3 '^/Memory Test' "$ES/esp/limine.conf" | grep -q 'kdos.splash' \
    && { echo "  the splash word reached an entry that is not ours"; exit 1; }
esb deploy "$ES/ra" a >/dev/null
test "$(grep -c 'kdos.splash=phosphor' "$ES/esp/limine.conf")" = \
     "$(grep -c '^    cmdline: kdos_slot=' "$ES/esp/limine.conf")" \
    || { echo "  a regenerated menu dropped kdos.splash="; exit 1; }
KDOS_LIMINE_CONF="$ES/esp/limine.conf" esb splash modern >/dev/null
grep -q 'kdos.splash' "$ES/esp/limine.conf" && { echo "  modern left kdos.splash= behind"; exit 1; }
cmp -s "$ES/settled.conf" "$ES/esp/limine.conf" \
    || { echo "  phosphor then modern did not give the menu back byte for byte"; exit 1; }
rc=0; KDOS_LIMINE_CONF="$ES/none.conf" esb splash phosphor >/dev/null 2>&1 || rc=$?
test "$rc" = 1 || { echo "  a missing menu was not refused with 1 (exit $rc)"; exit 1; }
# A slot with no kernel is refused once the ESP has per-slot directories: the
# menu could not lead with it, and select would abandon it on its first boot.
rm -rf "$ES/esp/EFI/kdos/b"
esb try b >/dev/null 2>&1 && { echo "  a slot with no kernel was tried"; exit 1; }
# kinstall writes the first menu and kdos-bootctl every later one. The two are
# the same text, or the first boot's mark-good rewrites a menu nothing changed.
rm -rf "$ES/esp"
mkdir -p "$ES/esp/EFI/kdos/a"
echo k > "$ES/esp/EFI/kdos/a/vmlinuz"
echo i > "$ES/esp/EFI/kdos/a/initramfs.cpio.gz"
esb set-slot a AAAA-1111 LUKS-AAAA >/dev/null
# The menu is rendered from the format strings in kinstall-core's install.c,
# the KI_QUIET words spliced in where the source names the macro, once for a
# machine with one root and once for the A/B layout, whose slot B entry is the
# one kdos-bootctl gives the other slot.
kimenu() {
    python3 - "$ES/esp/limine.conf" "$1" <<'PYEOF' || { echo "  could not render kinstall's limine.conf"; exit 1; }
import re, sys
s = open('src/system/kinstall-core/install.c').read()
quiet = re.search(r'#define KI_QUIET "([^"]*)"', s).group(1)
s = s.replace('" KI_QUIET "', quiet)
def lits(block):
    fmt = ''.join(re.findall(r'"((?:[^"\\]|\\.)*)"', block))
    return fmt.encode().decode('unicode_escape')
m = re.search(r'wr\("/boot/efi/limine.conf",\n(.*?)\n\t   theme, paper', s, re.S)
fmt = lits(m.group(1))
b = re.search(r'snprintf\(b_entry, sizeof\(b_entry\),\n(.*?),\n\t\t\t slot_opt', s, re.S)
bfmt = lits(b.group(1))
st, cr = 'bootstate=UUID=ESP-1 ', 'cryptdevice=UUID=LUKS-AAAA:kdosroot '
bent = bfmt % (st, cr, 'BBBB-2222') if sys.argv[2] == 'ab' else ''
open(sys.argv[1], 'w').write(fmt % ('term_background: 000000\n', '', '',
    st, cr, 'AAAA-1111', st, cr, 'AAAA-1111', st, cr, 'AAAA-1111', bent,
    '\n/Memory Test (memtest86+)\n    protocol: efi\n'))
PYEOF
}
kimenu one
cp "$ES/esp/limine.conf" "$ES/kinstall.conf"
esb mark-good >/dev/null
cmp -s "$ES/kinstall.conf" "$ES/esp/limine.conf" \
    || { echo "  kdos-bootctl regenerates kinstall's menu differently"; diff "$ES/kinstall.conf" "$ES/esp/limine.conf"; exit 1; }
mkdir -p "$ES/esp/EFI/kdos/b"
echo k > "$ES/esp/EFI/kdos/b/vmlinuz"
echo i > "$ES/esp/EFI/kdos/b/initramfs.cpio.gz"
esb set-slot b BBBB-2222 LUKS-AAAA >/dev/null
kimenu ab
cp "$ES/esp/limine.conf" "$ES/kinstall.conf"
esb mark-good >/dev/null
cmp -s "$ES/kinstall.conf" "$ES/esp/limine.conf" \
    || { echo "  kdos-bootctl regenerates kinstall's A/B menu differently"; diff "$ES/kinstall.conf" "$ES/esp/limine.conf"; exit 1; }
rm -rf "$ES/esp/EFI/kdos/b"

# An initramfs is deployed beside a kernel only when it carries that kernel's
# modules, or none at all. The archives are laid out as the image's is: an
# uncompressed microcode cpio first, then gzip, then whatever the linux
# postinstall appended as a second gzip member.
mkird() {
    python3 - "$@" <<'PYEOF'
import gzip, sys
def newc(names):
    out = b''
    for i, n in enumerate(names + ['TRAILER!!!']):
        mode = 0o40755 if n.endswith('/') else 0o100644
        nb = n.rstrip('/').encode() + b'\0'
        f = [i + 1, mode, 0, 0, 1, 0, 0, 0, 0, 0, 0, len(nb), 0]
        h = b'070701' + b''.join(b'%08X' % x for x in f) + nb
        out += h + b'\0' * (-len(h) % 4)
    return out + b'\0' * (-len(out) % 512)
out, parts = sys.argv[1], sys.argv[2:]
data = newc(['kernel', 'kernel/x86/microcode/GenuineIntel.bin'])
for p in parts:
    data += gzip.compress(newc(['init'] + ['lib/modules/%s/kernel/fs/fat/vfat.ko' % v
                                          for v in p.split(',') if v]))
open(out, 'wb').write(data)
PYEOF
}
mkdir -p "$ES/rc/boot"
mkbz "$ES/rc/boot/vmlinuz-kdos" 7.2.8
mkird "$ES/rc/boot/initramfs.cpio.gz" 7.2.6
esb deploy "$ES/rc" a >/dev/null 2>&1 \
    && { echo "  an initramfs with another kernel's modules was deployed"; exit 1; }
test "$(cat "$ES/esp/EFI/kdos/a/vmlinuz")" = k \
    || { echo "  a refused deploy still replaced the kernel"; exit 1; }
mkird "$ES/rc/boot/initramfs-kdos.cpio.gz" 7.2.6 7.2.8
esb deploy "$ES/rc" a >/dev/null \
    || { echo "  an initramfs carrying the kernel's modules was refused"; exit 1; }
mkird "$ES/rc/boot/initramfs-kdos.cpio.gz" ""
esb deploy "$ES/rc" a >/dev/null \
    || { echo "  an initramfs with no module tree was refused"; exit 1; }
# The reader stops at the first matching module path, and whatever follows it
# must not hold deploy up: here a megabyte of archive comes after the match,
# far more than a pipe buffer, as the rest of a real initramfs does.
python3 - "$ES/rc/boot/initramfs-kdos.cpio.gz" <<'PYEOF'
import gzip, sys
def ent(i, n, body=b''):
    nb = n.encode() + b'\0'
    f = [i, 0o100644, 0, 0, 1, 0, len(body), 0, 0, 0, 0, len(nb), 0]
    h = b'070701' + b''.join(b'%08X' % x for x in f) + nb
    h += b'\0' * (-len(h) % 4)
    return h + body + b'\0' * (-len(body) % 4)
a = ent(1, 'lib/modules/7.2.8/kernel/fs/fat/vfat.ko')
a += ent(2, 'blob', bytes(range(256)) * 4096) + ent(3, 'TRAILER!!!')
open(sys.argv[1], 'wb').write(gzip.compress(a))
PYEOF
timeout 20 env KDOS_BOOTSTATE="$ES/esp/EFI/kdos/bootstate" "$OUT/kdos-bootctl" \
    deploy "$ES/rc" a >/dev/null 2>&1 </dev/null
case $? in
0) ;;
124) echo "  deploy hung on an initramfs larger than a pipe buffer"; exit 1 ;;
*) echo "  an initramfs whose module tree comes first was refused"; exit 1 ;;
esac
# With no boot state the kernel goes where the menu boots it from, which is
# slot A's directory on anything kinstall wrote.
rm -f "$ES/esp/EFI/kdos/bootstate" "$ES/esp/EFI/kdos/vmlinuz"
mkird "$ES/rc/boot/initramfs-kdos.cpio.gz" 7.2.8
esb deploy "$ES/rc" >/dev/null || { echo "  deploy with no boot state failed"; exit 1; }
test ! -e "$ES/esp/EFI/kdos/vmlinuz" && cmp -s "$ES/rc/boot/vmlinuz-kdos" "$ES/esp/EFI/kdos/a/vmlinuz" \
    || { echo "  deploy with no boot state missed the directory the menu boots"; exit 1; }
# The linux postinstall takes the module set from the image's archive when the
# root has no /boot/initramfs.modules.
if command -v cpio >/dev/null; then
    ( . /dev/stdin <<EOF
$(sed -n '/^archive_modules() (/,/^)/p' "$(port_dir linux)/postinstall.sh")
EOF
      test "$(archive_modules "$ES/rc/boot/initramfs.cpio.gz")" = vfat ) \
        || { echo "  the postinstall found no module set in the image's archive"; exit 1; }
fi
echo "  per-slot kernels, the menu led by the next slot, the flat pair kept until unnamed"

echo
echo "==> on UEFI a candidate gets one boot, through BootNext"
# The firmware variables are a directory and the disk is an image file: the
# option `try` writes is compared byte for byte against one encoded here from
# the UEFI specification's layout, independently of kdos-bootctl's encoder.
# Deleting BootNext by hand is what the firmware does before it starts the
# loader, so every boot below begins with it.
BN="$OUT/esp-bootnext"
rm -rf "$BN"
mkdir -p "$BN/esp/EFI/kdos/a" "$BN/esp/EFI/kdos/b" "$BN/esp/EFI/BOOT" "$BN/fw/efivars"
for s in a b; do
    echo "kernel-$s" > "$BN/esp/EFI/kdos/$s/vmlinuz"
    echo "initramfs-$s" > "$BN/esp/EFI/kdos/$s/initramfs.cpio.gz"
done
echo limine-x64 > "$BN/esp/EFI/BOOT/BOOTX64.EFI"
echo limine-ia32 > "$BN/esp/EFI/BOOT/BOOTIA32.EFI"
echo 64 > "$BN/fw/fw_platform_size"
{
    printf 'timeout: 10\ndefault_entry: 1\n\n'
    printf '/KDOS\n    protocol: linux\n    path: boot():/EFI/kdos/a/vmlinuz\n'
    printf '    module_path: boot():/EFI/kdos/a/initramfs.cpio.gz\n'
    printf '    cmdline: kdos_slot=a bootstate=UUID=ESP-1 root=UUID=AAAA-1111 rw quiet loglevel=3\n'
} > "$BN/esp/limine.conf"
python3 - "$BN" <<'PYEOF' || { echo "  could not build the fixture disk and variables"; exit 1; }
import struct, sys, uuid
d = sys.argv[1]
G = '8be4df61-93ca-11d2-aa0d-00e098032b8c'
part = uuid.UUID('0fc63daf-8483-4772-8e79-3d69d8477de4').bytes_le
# A 512-byte-sector GPT: protective MBR, header at LBA 1, the array at LBA 2,
# the ESP as its second entry from LBA 4096 to 1052671.
img = bytearray(512 * 34)
img[510:512] = b'\x55\xaa'
img[446 + 4] = 0xEE
hdr = b'EFI PART' + struct.pack('<IIIIQQQQ16sQIII', 0x10000, 92, 0, 0, 1, 0,
                                34, 0, b'\0' * 16, 2, 128, 128, 0)
img[512:512 + len(hdr)] = hdr
ent = uuid.UUID('c12a7328-f81f-11d2-ba4b-00a0c93ec93b').bytes_le + part + \
      struct.pack('<QQQ', 4096, 1052671, 0)
img[1024 + 128:1024 + 128 + len(ent)] = ent
img[1024:1024 + 16] = b'\x11' * 16      # entry 1, some other partition
open(d + '/disk.img', 'wb').write(img)
def option(desc, path):
    node = struct.pack('<BBHIQQ16sBB', 4, 1, 42, 2, 4096, 1048576, part, 2, 2)
    p = (path + '\0').encode('utf-16-le')
    node += struct.pack('<BBH', 4, 4, 4 + len(p)) + p + b'\x7f\xff\x04\x00'
    return struct.pack('<IH', 1, len(node)) + (desc + '\0').encode('utf-16-le') + node
attrs = struct.pack('<I', 7)
open(d + '/want-x64', 'wb').write(attrs + option('KDOS update trial', '\\EFI\\kdos\\trial\\BOOTX64.EFI'))
open(d + '/want-ia32', 'wb').write(attrs + option('KDOS update trial', '\\EFI\\kdos\\trial\\BOOTIA32.EFI'))
open(d + '/want-next', 'wb').write(attrs + struct.pack('<H', 1))
# Boot0000 is the firmware's own and must survive everything.
open(d + '/fw/efivars/Boot0000-' + G, 'wb').write(attrs + option('KDOS', '\\EFI\\BOOT\\BOOTX64.EFI'))
PYEOF
EV="$BN/fw/efivars"
G=8be4df61-93ca-11d2-aa0d-00e098032b8c
cp "$EV/Boot0000-$G" "$BN/firmware-own"
bn() { env KDOS_BOOTSTATE="$BN/esp/EFI/kdos/bootstate" KDOS_EFIVARS="$EV" \
           KDOS_ESP_DISK="$BN/disk.img:2" "$OUT/kdos-bootctl" "$@"; }
bn set-slot a AAAA-1111 >/dev/null
bn set-slot b BBBB-2222 >/dev/null
first_bn() { grep -m1 'path: boot()' "$1"; }

bn try b >/dev/null || { echo "  try failed"; exit 1; }
cmp -s "$EV/Boot0001-$G" "$BN/want-x64" \
    || { echo "  the Boot0001 load option is not the one the spec lays out"; exit 1; }
cmp -s "$EV/BootNext-$G" "$BN/want-next" || { echo "  BootNext does not name Boot0001"; exit 1; }
cmp -s "$EV/Boot0000-$G" "$BN/firmware-own" || { echo "  the firmware's own option was touched"; exit 1; }
cmp -s "$BN/esp/EFI/BOOT/BOOTX64.EFI" "$BN/esp/EFI/kdos/trial/BOOTX64.EFI" \
    || { echo "  the trial directory has no copy of the loader"; exit 1; }
first_bn "$BN/esp/limine.conf" | grep -q '/EFI/kdos/a/' \
    || { echo "  the candidate leads the menu an unattended boot reads"; exit 1; }
grep -q '^default_entry: 4$' "$BN/esp/EFI/kdos/trial/limine.conf" \
    || { echo "  the trial menu does not default to the candidate"; exit 1; }
grep -A5 '^/KDOS (slot b)$' "$BN/esp/EFI/kdos/trial/limine.conf" \
    | grep -q 'kdos_slot=b .*root=UUID=BBBB-2222 .*panic=10' \
    || { echo "  the candidate entry does not reset on a panic"; exit 1; }
bn status | grep -q "trying   b once, through UEFI BootNext" \
    || { echo "  status does not show the BootNext trial"; exit 1; }

# The candidate's kernel dies before its initramfs: the firmware spent
# BootNext, the reset starts EFI/BOOT/ and the confirmed slot, and that slot's
# select rolls the candidate back. rcS's mark-good then removes the option.
rm -f "$EV/BootNext-$G"
test "$(bn select a 2>/dev/null)" = AAAA-1111 || { echo "  the reset did not boot a"; exit 1; }
bn status | grep -q "trying   nothing" || { echo "  the dead candidate is still on trial"; exit 1; }
test ! -e "$BN/esp/EFI/kdos/trial" || { echo "  the trial directory outlived the trial"; exit 1; }
bn mark-good >/dev/null
test ! -e "$EV/Boot0001-$G" || { echo "  mark-good left the trial option behind"; exit 1; }
cmp -s "$EV/Boot0000-$G" "$BN/firmware-own" || { echo "  mark-good touched the firmware's option"; exit 1; }

# One boot and a userland that never confirms: the next boot rolls back, and
# nothing re-arms the candidate.
bn try b >/dev/null
rm -f "$EV/BootNext-$G"
test "$(bn select b 2>/dev/null)" = BBBB-2222 || { echo "  the trial boot did not boot b"; exit 1; }
bn status | grep -q "booted once and not confirmed" || { echo "  the trial boot was not spent"; exit 1; }
test ! -e "$EV/BootNext-$G" || { echo "  the trial re-armed itself"; exit 1; }
test "$(bn select a 2>/dev/null)" = AAAA-1111 || { echo "  the unconfirmed candidate was not rolled back"; exit 1; }

# The good update: its boot reaches mark-good, which makes it the default.
bn try b >/dev/null
rm -f "$EV/BootNext-$G"
test "$(bn select b 2>/dev/null)" = BBBB-2222 || { echo "  the trial boot did not boot b"; exit 1; }
bn mark-good >/dev/null
bn status | grep -q "^active   b" || { echo "  mark-good did not confirm the candidate"; exit 1; }
first_bn "$BN/esp/limine.conf" | grep -q '/EFI/kdos/b/' \
    || { echo "  the confirmed candidate does not lead the menu"; exit 1; }
grep -q 'panic=10' "$BN/esp/limine.conf" && { echo "  panic=10 outlived the trial"; exit 1; }
test ! -e "$EV/Boot0001-$G" || { echo "  the trial option outlived the trial"; exit 1; }

# An init that ignores kdos_slot=, after the candidate's kernel died: its
# kdos-bootctl spent the attempt, dropped the bootnext key and booted the
# candidate's root on the confirmed slot's kernel. mark-good on that kernel
# rolls back rather than confirming a kernel that never ran.
bn try a >/dev/null
rm -f "$EV/BootNext-$G"
grep -v '^bootnext' "$BN/esp/EFI/kdos/bootstate" \
    | awk '/^attempts/ { print "attempts = 0"; next } { print }' > "$BN/old-state"
cp "$BN/old-state" "$BN/esp/EFI/kdos/bootstate"
printf 'kdos_slot=b bootstate=UUID=ESP-1 root=UUID=AAAA-1111 rw\n' > "$BN/cmdline"
KDOS_CMDLINE="$BN/cmdline" bn mark-good >/dev/null 2>&1
bn status | grep -q "^active   b" || { echo "  the old init's boot confirmed a kernel that never ran"; exit 1; }
bn status | grep -q "trying   nothing" || { echo "  the old init's boot left the candidate on trial"; exit 1; }
first_bn "$BN/esp/limine.conf" | grep -q '/EFI/kdos/b/' \
    || { echo "  the dead candidate's kernel leads the menu"; exit 1; }
# The candidate's own kernel still confirms it.
bn try a >/dev/null
rm -f "$EV/BootNext-$G"
bn select a >/dev/null 2>&1
printf 'kdos_slot=a root=UUID=AAAA-1111 rw\n' > "$BN/cmdline"
KDOS_CMDLINE="$BN/cmdline" bn mark-good >/dev/null
bn status | grep -q "^active   a" || { echo "  the candidate's own kernel did not confirm it"; exit 1; }

# A 32-bit firmware is sent to the loader it can execute.
echo 32 > "$BN/fw/fw_platform_size"
bn try b >/dev/null
cmp -s "$EV/Boot0001-$G" "$BN/want-ia32" \
    || { echo "  a 32-bit firmware was sent to BOOTX64.EFI"; exit 1; }
rm -f "$EV/BootNext-$G"
bn select a >/dev/null 2>&1; bn mark-good >/dev/null

# No firmware variables — a BIOS boot — is the menu-led trial, counted.
env KDOS_BOOTSTATE="$BN/esp/EFI/kdos/bootstate" KDOS_EFIVARS="$BN/none" \
    "$OUT/kdos-bootctl" try b >/dev/null 2>&1
test -e "$EV/Boot0001-$G" && { echo "  a BIOS trial wrote a firmware variable"; exit 1; }
first_bn "$BN/esp/limine.conf" | grep -q '/EFI/kdos/b/' \
    || { echo "  a trial with no BootNext is not led by the menu"; exit 1; }
bn status | grep -q "trying   b, 3 attempt(s) left" \
    || { echo "  a trial with no BootNext is not counted"; exit 1; }
echo "  the spec's load option, the confirmed slot's default kept, one boot then rollback, confirmed only on its own kernel, BIOS counted"

echo
echo "==> each slot is named by its version, and a trial armed in this boot is kept"
# What the System Versions page, the logout screen and the notices read: the
# version each slot holds (`deploy` records it on the ESP beside the kernel;
# the running slot is read live), the one-shot entry elogind reports as
# login1's RebootToBootLoaderEntry, `cancel`, and the slot a rollback left
# behind. MARK-GOOD RUNS A MINUTE INTO THE BOOT, once kdos-sysupdated holds
# both bless marks, so an update applied inside that minute has already armed
# its trial: the mark-good of that same boot must leave the trial exactly as
# `try` wrote it.
SV="$OUT/slotver"
rm -rf "$SV"
mkdir -p "$SV/esp/EFI/kdos/a" "$SV/a/etc" "$SV/b/etc" "$SV/b/boot" "$SV/b/ports"
sv() {
    env KDOS_BOOTSTATE="$SV/esp/EFI/kdos/bootstate" KDOS_CMDLINE="$SV/cmdline" \
        KDOS_SYSROOT="$SV/a" KDOS_BOOT_ID="${SVB:-boot-1}" \
        KDOS_REBOOT_ENTRY="$SV/run/systemd/reboot-to-boot-loader-entry" \
        KDOS_BOOT_ENTRIES="$SV/run/kdos/boot-entries.json" \
        KDOS_BOOTSTATE_PUB="$SV/run/kdos/bootstate.json" \
        KDOS_SLOT_PUB="$SV/run/kdos/slot.json" \
        "$OUT/kdos-bootctl" "$@"
}
# A session's read: the state file closed to it, the copies root published.
svuser() {
    env KDOS_BOOTSTATE="$SV/closed/bootstate" \
        KDOS_BOOT_ENTRIES="$SV/run/kdos/boot-entries.json" \
        KDOS_BOOTSTATE_PUB="$SV/run/kdos/bootstate.json" \
        "$OUT/kdos-bootctl" "$@"
}
printf 'kdos_slot=a root=UUID=AAAA-1111 rw\n' > "$SV/cmdline"
printf 'VERSION_ID="0.2"\nBUILD_ID="a81bec1c"\n' > "$SV/a/etc/os-release"
printf 'VERSION="0.2.14"\n' > "$SV/b/etc/kdos-release"
printf 'kdos-tree 1\ncommit 5555666677778888\ntime 1761600000\n' > "$SV/b/ports/TREE"
printf 'not a bzImage\n' > "$SV/b/boot/vmlinuz-kdos"
printf 'not an initramfs\n' > "$SV/b/boot/initramfs.cpio.gz"
sv set-slot a AAAA-1111 >/dev/null
sv set-slot b BBBB-2222 >/dev/null
sv status --json | grep -qF '"a": {"state": "running", "title": "KDOS 0.2+a81bec1c (slot A)", "version": "0.2+a81bec1c"' \
    || { echo "  the running slot is not named from its os-release:"; sv status --json; exit 1; }
grep -qx 'failed   = ' "$SV/esp/EFI/kdos/bootstate" \
    || { echo "  a new boot state records a failed trial"; exit 1; }
printf 'VERSION="0.2.13"\n' > "$SV/a/etc/kdos-release"
sv deploy "$SV/b" b >/dev/null || { echo "  deploy failed"; exit 1; }
for want in 'VERSION="0.2.14"' 'TREE="5555666677778888"' 'TREE_TIME="1761600000"'; do
    grep -qxF "$want" "$SV/esp/EFI/kdos/b/release" \
        || { echo "  deploy did not record $want for slot b"; exit 1; }
done
sv list --json=short > "$SV/list"
printf '%s\n' '[{"type":"kdos-slot","id":"kdos-a","title":"KDOS 0.2.13 (slot A)","showTitle":"KDOS 0.2.13 (slot A)","version":"0.2.13","isDefault":true,"isSelected":true},{"type":"kdos-slot","id":"kdos-b","title":"KDOS 0.2.14 (slot B)","showTitle":"KDOS 0.2.14 (slot B)","version":"0.2.14","isDefault":false,"isSelected":false}]' \
    | cmp -s - "$SV/list" || { echo "  list --json=short printed:"; cat "$SV/list"; exit 1; }
sv list --json=pretty | python3 -I -c 'import json, sys; assert len(json.load(sys.stdin)) == 2' \
    || { echo "  list --json=pretty is not the same two entries"; exit 1; }
test "$(env KDOS_BOOTSTATE="$SV/none" "$OUT/kdos-bootctl" list --json=short)" = "[]" \
    || { echo "  a machine with no boot state listed entries"; exit 1; }
sv status --json | grep -qF '"b": {"state": "inactive", "title": "KDOS 0.2.14 (slot B)", "version": "0.2.14", "build": null, "date": null, "tree": "5555666677778888", "tree_time": 1761600000' \
    || { echo "  status --json does not describe slot b:"; sv status --json; exit 1; }
# The trial: the entry the logout screen names, and this boot's mark-good
# leaving it alone.
sv try b >/dev/null || { echo "  try failed"; exit 1; }
test "$(cat "$SV/run/systemd/reboot-to-boot-loader-entry")" = kdos-b \
    || { echo "  try did not name kdos-b for the logout screen"; exit 1; }
# The greeter, the welcome tour and the tray run as the user, and the ESP is
# root's: a `--json` read that cannot open the boot state prints the copies
# root published, and `try` publishes before it names the entry. The closed
# state is a directory with no search permission, which root ignores, so it
# is proved only where the selftest is not root.
sv list --json=short | cmp -s - "$SV/run/kdos/boot-entries.json" && \
    sv status --json | cmp -s - "$SV/run/kdos/bootstate.json" \
    || { echo "  try did not publish what list and status print"; exit 1; }
# About this System's slot row reads the flat copy: the running slot, its
# version, its state and the other slot.
test "$(cat "$SV/run/kdos/slot.json")" = '{"slot": "a", "version": "0.2.13", "state": "confirmed", "previous_slot": "b", "previous_version": "0.2.14"}' \
    || { echo "  slot.json is:"; cat "$SV/run/kdos/slot.json"; exit 1; }
KDOS_ABOUT_ROOT="$SV" sh src/desktop/kcm-kdos-health/about/about-slot | tail -1 \
    | grep -qxF 'A · 0.2.13 · confirmed (previous: B · 0.2.14)' \
    || { echo "  about-slot does not show the published slot"; exit 1; }
mkdir -p "$SV/closed"
: > "$SV/closed/bootstate"
chmod 000 "$SV/closed"
if [ "$(id -u)" != 0 ]; then
    svuser list --json=short | grep -qF '{"type":"kdos-slot","id":"kdos-b","title":"KDOS 0.2.14 (slot B)","showTitle":"KDOS 0.2.14 (slot B)","version":"0.2.14","isDefault":true' \
        || { echo "  a session's list does not name the trial"; exit 1; }
    svuser status --json | grep -qF '"trying": "b"' \
        || { echo "  a session's status is not the published one"; exit 1; }
    test "$(env KDOS_BOOTSTATE="$SV/none" KDOS_BOOT_ENTRIES="$SV/run/kdos/boot-entries.json" \
            "$OUT/kdos-bootctl" list --json=short)" = "[]" \
        || { echo "  a machine with no boot state printed a published list"; exit 1; }
fi
chmod 755 "$SV/closed"
grep -qx 'armed    = boot-1' "$SV/esp/EFI/kdos/bootstate" \
    || { echo "  try did not record the boot that armed it"; exit 1; }
sv mark-good | grep -q "armed for the next boot" \
    || { echo "  mark-good in the boot that armed the trial did not say so"; exit 1; }
grep -qx 'try      = b' "$SV/esp/EFI/kdos/bootstate" \
    || { echo "  the bless of the boot that armed the trial cancelled it"; exit 1; }
sv status --json | grep -qF '"entry": "kdos-b"' || { echo "  status --json names no entry"; exit 1; }
SVB=boot-2 sv cancel >/dev/null || { echo "  cancel failed"; exit 1; }
grep -qx 'try      = ' "$SV/esp/EFI/kdos/bootstate" || { echo "  cancel left the trial"; exit 1; }
test ! -e "$SV/run/systemd/reboot-to-boot-loader-entry" \
    || { echo "  cancel left the logout screen naming the trial"; exit 1; }
grep -qF '"trying": null' "$SV/run/kdos/bootstate.json" \
    || { echo "  cancel left the published state naming the trial"; exit 1; }
sv try b >/dev/null
printf 'kdos_slot=b root=UUID=BBBB-2222 rw\n' > "$SV/cmdline"
rc=0; SVB=boot-2 sv cancel >/dev/null 2>&1 || rc=$?
test "$rc" = 1 || { echo "  a trial this boot is running was cancelled"; exit 1; }
printf 'kdos_slot=a root=UUID=AAAA-1111 rw\n' > "$SV/cmdline"
# A trial that ends unconfirmed is the rollback notice's slot, until the next
# trial.
for i in 1 2 3 4; do SVB=boot-3 sv select >/dev/null 2>&1; done
sv status --json | grep -qF '"failed": "b"' && \
    sv status --json | grep -qF '"b": {"state": "failed"' \
    || { echo "  the rolled-back slot is not reported failed:"; sv status --json; exit 1; }
sv try b >/dev/null
grep -qx 'failed   = ' "$SV/esp/EFI/kdos/bootstate" \
    || { echo "  a new trial kept the old failure"; exit 1; }
echo "  versions on the ESP, the logout entry, cancel, the rollback slot, the armed boot's own mark-good"

echo
echo "==> kdos update --arm arms only the slot an acquire filled, and says so"
# Install's half of a Discover update, the real command on a fixture slot.
# kdos-sysupdated gives Install its job only once `verified` arrives on the
# progress descriptor, so the arm must write it before it touches the slot,
# and `armed` once the slot is the next boot's candidate; a slot no acquire
# of this tree filled is refused with nothing touched. Root only, as an arm
# is: KDOS_UPDATE_SLOT stands in for which directory the slot is mounted at,
# never for the permission to write into it.
if [ "$(id -u)" = 0 ]; then
    AR="$OUT/arm"
    rm -rf "$AR"
    mkdir -p "$AR/ports/core/misc/one" "$AR/esp/EFI/kdos" "$AR/run" "$AR/bin" \
             "$AR/sys/etc" "$AR/slot/etc" "$AR/slot/boot" "$AR/slot$AR"
    printf 'name = one\nversion = 1.0\nrelease = 1\n' > "$AR/ports/core/misc/one/kpkgbuild"
    : > "$AR/ports/core/misc/one/build.sh"
    printf 'kdos-tree 1\ncommit 29d742c14cc8aef873af7f1f28e267d6318facc2\ntime 1761600000\nversion 0.2.14\n' \
        > "$AR/ports/TREE"
    printf 'VERSION="0.2.13"\n' > "$AR/sys/etc/kdos-release"
    printf 'not a bzImage\n' > "$AR/slot/boot/vmlinuz-kdos"
    printf 'not an initramfs\n' > "$AR/slot/boot/initramfs.cpio.gz"
    printf 'kdos_slot=a root=UUID=AAAA-1111 rw\n' > "$AR/cmdline"
    ln -sf "$OUT/kdos-tools" "$AR/bin/kdos-bootctl"
    arm() {
        env PATH="$AR/bin:$PATH" PORT_REPO="$AR/ports/core" KPKG_CONF=/nonexistent \
            PKGDB_DIR="$AR/db" KDOS_UPDATE_STATE="$AR/state" \
            KDOS_UPDATE_SYSROOT="$AR/sys" KDOS_UPDATE_SLOT="$AR/slot" \
            KDOS_BOOTSTATE="$AR/esp/EFI/kdos/bootstate" KDOS_CMDLINE="$AR/cmdline" \
            KDOS_SYSROOT="$AR/sys" KDOS_BOOT_ID=arm-boot \
            KDOS_REBOOT_ENTRY="$AR/run/reboot-to-boot-loader-entry" "$@"
    }
    arm "$OUT/kdos-bootctl" set-slot a AAAA-1111 >/dev/null
    arm "$OUT/kdos-bootctl" set-slot b BBBB-2222 >/dev/null
    rc=0; arm "$OUT/kdos" update apply --arm --progress 3 3>"$AR/prog" >/dev/null 2>&1 || rc=$?
    test "$rc" = 3 || { echo "  an arm with nothing acquired exited $rc, not 3"; exit 1; }
    grep -qxF 'refused slot b holds no acquired update for 0.2.14: run `kdos update apply --acquire` first' \
        "$AR/prog" || { echo "  the refused arm said:"; cat "$AR/prog"; exit 1; }
    grep -qx 'try      = ' "$AR/esp/EFI/kdos/bootstate" && test ! -e "$AR/slot/etc/kdos-release" \
        || { echo "  a refused arm touched the slot or the boot state"; exit 1; }
    printf 'apply acquired 1761600000 slot b 0.2.14\n' > "$AR/state"
    arm "$OUT/kdos" update apply --arm --progress 3 3>"$AR/prog" >"$AR/out" 2>&1 \
        || { echo "  the arm failed:"; cat "$AR/out"; exit 1; }
    test "$(head -n1 "$AR/prog")" = 'verified - 0.2.14 0 0' && \
        test "$(tail -n1 "$AR/prog")" = 'armed b 0.2.14' \
        || { echo "  the arm's progress lines are:"; cat "$AR/prog"; exit 1; }
    grep -qx 'try      = b' "$AR/esp/EFI/kdos/bootstate" \
        || { echo "  the arm did not make slot b the candidate"; exit 1; }
    test "$(cat "$AR/run/reboot-to-boot-loader-entry")" = kdos-b \
        || { echo "  the arm left the logout screen naming no slot"; exit 1; }
    grep -qxF 'VERSION="0.2.14"' "$AR/slot/etc/kdos-release" && \
        grep -qxF 'VERSION="0.2.14"' "$AR/esp/EFI/kdos/b/release" \
        || { echo "  the armed slot is not recorded as 0.2.14"; exit 1; }
    cmp -s "$AR/ports/TREE" "$AR/slot$AR/ports/TREE" \
        || { echo "  the tree was not copied into the slot"; exit 1; }
    grep -q '^apply ok [0-9]* slot b 0.2.14$' "$AR/state" \
        || { echo "  the arm recorded:"; cat "$AR/state"; exit 1; }
    # Booted into the trial, the slot that is not confirmed is the running
    # root and the confirmed one is the way back: an acquire takes neither.
    printf 'kdos_slot=b root=UUID=BBBB-2222 rw\n' > "$AR/cmdline"
    rc=0; arm "$OUT/kdos" update apply --acquire --progress 3 3>"$AR/prog" >/dev/null 2>&1 || rc=$?
    test "$rc" = 3 && grep -qxF 'refused this boot runs slot b on trial; an update is installed once it is confirmed or rolled back' "$AR/prog" \
        || { echo "  an acquire on a trial boot exited $rc and said:"; cat "$AR/prog"; exit 1; }
    echo "  refused without an acquire; verified, the tree, the version, the kernel, the trial, armed; refused on the trial boot"
else
    echo "  skipped — an arm writes into a slot as root, and this is not root"
fi

echo
echo "==> a slot image is signed, rebuilt from what is here and its delta, and refused when tampered"
# The release's whole-slot image (image.c) over two fixture roots: v2 changes
# one large file in place, adds one and removes one. Its delta from v1 carries
# the change as a zstd patch, so with the full bundle withheld a root written
# from v1 must still become exactly v2 — the delta and the seed alone carry
# it. A tampered delta falls back to the full bundle; a manifest edited after
# signing, or signed by a key nobody trusts, writes nothing; the seed is never
# the target.
IM="$OUT/image"
rm -rf "$IM"
mkdir -p "$IM/keys"
( cd "$IM" && "$OUT/kdos-kpkg" kpkg keygen release >/dev/null 2>&1 &&
  "$OUT/kdos-kpkg" kpkg keygen stranger >/dev/null 2>&1 ) \
    || { echo "  keygen failed"; exit 1; }
cp "$IM/release.pub" "$IM/keys/"
img() { env KPKG_KEYRING="$IM/keys" "$OUT/kdos" image "$@"; }
_imroot() {
    mkdir -p "$1/usr/bin" "$1/usr/lib" "$1/etc" "$1/dev" "$1/boot" "$1/home/kdos" \
             "$1/ports" "$1/var/lib/kdos/packs"
    printf 'kdos-tree 1\ncommit %s\ntime %s\nversion %s\n' "$2" "$3" "$4" > "$1/ports/TREE"
    printf '#!/bin/sh\necho hi\n' > "$1/usr/bin/hello"
    chmod 755 "$1/usr/bin/hello"
    ln -s hello "$1/usr/bin/hi"
    ln -s usr/bin "$1/bin"
    : > "$1/usr/lib/empty"
    printf 'odd name\n' > "$1/usr/lib/a b%c"
    printf 'not a bzImage\n' > "$1/boot/vmlinuz-kdos"
    printf 'not an initramfs\n' > "$1/boot/initramfs.cpio.gz"
    printf 'kdos\n' > "$1/etc/hostname"
    printf 'root:x:0:0::/root:/bin/sh\nsddm:x:60:60::/var/lib/sddm:/bin/false\n' > "$1/etc/passwd"
    printf '# shipped\n' > "$1/etc/fstab"
    printf 'home\n' > "$1/home/kdos/notes"
    printf 'pack\n' > "$1/var/lib/kdos/packs/x.kpack"
    mkdir -p "$1/var/log/cups"
    printf 'a log line\n' > "$1/var/log/cups/error_log"
}
# Lists a root the way the image sees it: type, mode, size, content, link,
# mtime in seconds; the image's own record, the shared home, the pack
# store's contents and the files (not the directories) under var/log are not
# part of it.
_imlist() {
    python3 -I - "$1" <<'PYEOF'
import hashlib, os, stat, sys
root = sys.argv[1]
skip = ("var/lib/kdos/image", "home", "var/lib/kdos/packs")
for d, dirs, files in os.walk(root):
    rel = os.path.relpath(d, root)
    if rel in skip:
        dirs[:] = []
        files = []
    # The walk descends in directory order, which tmpfs gives as creation
    # order; sorted, two trees made in different orders list the same.
    dirs.sort()
    for n in sorted(dirs + files):
        p = os.path.join(d, n)
        r = os.path.relpath(p, root)
        if r == "var/lib/kdos/image":
            continue
        if r.startswith("var/log/") and not stat.S_ISDIR(os.lstat(p).st_mode):
            continue
        st = os.lstat(p)
        extra = ""
        if stat.S_ISREG(st.st_mode):
            extra = hashlib.sha256(open(p, "rb").read()).hexdigest()
        elif stat.S_ISLNK(st.st_mode):
            extra = os.readlink(p)
        print(r, oct(st.st_mode), st.st_size if not stat.S_ISDIR(st.st_mode) else 0,
              extra, int(st.st_mtime))
PYEOF
}
_imroot "$IM/v1" 1111111111111111111111111111111111111111 1761600000 0.3.0
head -c 200000 /dev/urandom > "$IM/v1/usr/lib/libbig.so"
ln "$IM/v1/usr/bin/hello" "$IM/v1/usr/bin/hello-again"
printf 'gone in 0.3.1\n' > "$IM/v1/usr/lib/old"
cp -a "$IM/v1" "$IM/v2"
printf 'kdos-tree 1\ncommit 2222222222222222222222222222222222222222\ntime 1761700000\nversion 0.3.1\n' \
    > "$IM/v2/ports/TREE"
printf 'XXXX' | dd of="$IM/v2/usr/lib/libbig.so" bs=1 seek=5000 conv=notrunc 2>/dev/null
rm "$IM/v2/usr/lib/old"
printf 'new tool\n' > "$IM/v2/usr/bin/new"
img make "$IM/v1" "$IM/r1" --key "$IM/release.key" --level 3 >/dev/null \
    || { echo "  the 0.3.0 image was not made"; exit 1; }
img make "$IM/v2" "$IM/r2" --key "$IM/release.key" --level 3 \
    --prev "$IM/r1" --prev-root "$IM/v1" >/dev/null \
    || { echo "  the 0.3.1 image and its delta were not made"; exit 1; }
grep -q '^delta 0.3.0 kdos-image-0.3.1.from-0.3.0.delta ' "$IM/r2/kdos-image.manifest" \
    || { echo "  the manifest names no delta from 0.3.0"; exit 1; }
grep -qx 'h usr/bin/hello-again usr/bin/hello' "$IM/r1/kdos-image.manifest" \
    || { echo "  a hard link is not recorded as one"; exit 1; }
grep -qF 'usr/lib/a%20b%25c' "$IM/r1/kdos-image.manifest" \
    || { echo "  a path with a space and a percent sign is not escaped"; exit 1; }
! grep -qE ' home/kdos| var/lib/kdos/packs/' "$IM/r1/kdos-image.manifest" \
    || { echo "  the shared home or the pack store is inside the image"; exit 1; }
grep -qE '^d [0-7]+ [0-9]+ [0-9]+ [0-9]+ var/log/cups$' "$IM/r1/kdos-image.manifest" && \
    ! grep -q 'var/log/cups/error_log' "$IM/r1/kdos-image.manifest" \
    || { echo "  a package's directory under var/log is missing, or a log file is inside the image"; exit 1; }
_imfull=$(wc -c < "$IM/r2/kdos-image-0.3.1.full.000")
_imdelta=$(wc -c < "$IM/r2/kdos-image-0.3.1.from-0.3.0.delta")
test "$_imdelta" -lt $((_imfull / 10)) \
    || { echo "  the delta ($_imdelta bytes) is not a patch of the full image ($_imfull)"; exit 1; }
img verify "$IM/r2" >/dev/null || { echo "  a good image did not verify"; exit 1; }
mkdir "$IM/A" "$IM/B"
img unpack "$IM/r1" "$IM/A" >/dev/null || { echo "  the 0.3.0 image did not unpack"; exit 1; }
cmp -s <(_imlist "$IM/v1") <(_imlist "$IM/A") \
    || { echo "  the unpacked 0.3.0 root differs from its source:"; diff <(_imlist "$IM/v1") <(_imlist "$IM/A"); exit 1; }
printf 'stale\n' > "$IM/B/stale"
# The seed is a running machine: its /etc is edited, and the delta still
# carries the release's copy of every file there.
cp -a "$IM/A" "$IM/A2"
printf 'myhost\n' > "$IM/A2/etc/hostname"
mv "$IM/r2/kdos-image-0.3.1.full.000" "$IM/full.held"
img unpack "$IM/r2" "$IM/B" --seed "$IM/A2" > "$IM/out" 2>&1 \
    || { echo "  the delta alone did not make 0.3.1:"; cat "$IM/out"; exit 1; }
grep -q 'the delta from 0.3.0' "$IM/out" && ! grep -q 'from the full image' "$IM/out" \
    || { echo "  the update did not come from the delta:"; cat "$IM/out"; exit 1; }
cmp -s <(_imlist "$IM/v2") <(_imlist "$IM/B") \
    || { echo "  the root made from the delta differs from 0.3.1:"; diff <(_imlist "$IM/v2") <(_imlist "$IM/B"); exit 1; }
test ! -e "$IM/B/stale" && test ! -e "$IM/B/usr/lib/old" \
    || { echo "  an entry the image does not name was left in the root"; exit 1; }
test "$(stat -c %i "$IM/A/usr/bin/hello")" = "$(stat -c %i "$IM/A/usr/bin/hello-again")" \
    || { echo "  the hard link came back as two files"; exit 1; }
grep -qx 'version 0.3.1' "$IM/B/var/lib/kdos/image/MANIFEST" \
    || { echo "  the root does not record the image it holds"; exit 1; }
mkdir "$IM/C"
rc=0; img unpack "$IM/r2" "$IM/C" >/dev/null 2>&1 || rc=$?
test "$rc" = 1 || { echo "  an image with no seed and no full bundle exited $rc, not 1"; exit 1; }
mv "$IM/full.held" "$IM/r2/kdos-image-0.3.1.full.000"
cp "$IM/r2/kdos-image-0.3.1.from-0.3.0.delta" "$IM/delta.good"
printf 'garbage' | dd of="$IM/r2/kdos-image-0.3.1.from-0.3.0.delta" bs=1 seek=60 conv=notrunc 2>/dev/null
mkdir "$IM/D"
img unpack "$IM/r2" "$IM/D" --seed "$IM/A" > "$IM/out" 2>&1 \
    || { echo "  a tampered delta was not replaced by the full image:"; cat "$IM/out"; exit 1; }
grep -q 'from the full image' "$IM/out" && cmp -s <(_imlist "$IM/v2") <(_imlist "$IM/D") \
    || { echo "  after a tampered delta the root is not 0.3.1"; exit 1; }
cp "$IM/delta.good" "$IM/r2/kdos-image-0.3.1.from-0.3.0.delta"
cp "$IM/r2/kdos-image.manifest" "$IM/manifest.good"
sed -i 's/^time .*/time 1761700001/' "$IM/r2/kdos-image.manifest"
mkdir "$IM/E"
printf 'keep\n' > "$IM/E/keep"
rc=0; img unpack "$IM/r2" "$IM/E" >/dev/null 2>&1 || rc=$?
test "$rc" = 3 && test "$(cat "$IM/E/keep")" = keep && test ! -e "$IM/E/usr" \
    || { echo "  a manifest edited after signing was used (exit $rc)"; exit 1; }
rc=0; img verify "$IM/r2" >/dev/null 2>&1 || rc=$?
test "$rc" = 3 || { echo "  verify passed an edited manifest"; exit 1; }
cp "$IM/manifest.good" "$IM/r2/kdos-image.manifest"
img make "$IM/v2" "$IM/rs" --key "$IM/stranger.key" --level 3 >/dev/null
rc=0; img unpack "$IM/rs" "$IM/E" >/dev/null 2>&1 || rc=$?
test "$rc" = 3 && test ! -e "$IM/E/usr" \
    || { echo "  an image signed by an untrusted key was used (exit $rc)"; exit 1; }
rc=0; img unpack "$IM/r2" "$IM/A" --seed "$IM/A" >/dev/null 2>&1 || rc=$?
test "$rc" = 1 && cmp -s <(_imlist "$IM/v1") <(_imlist "$IM/A") \
    || { echo "  an image was written over the root it was filled from"; exit 1; }
echo "  signed; patched by its delta with the full image withheld; tampered delta, edited manifest, stranger's key and seed-as-target refused"

echo
echo "==> the release index says where every update file is, and nothing it does not name is taken"
# A published release as ports/publish --release lays it out: flat assets in
# releases of their own — the binhost sharded at the cap, the slot image in
# image-<version> — and one signed kdos-update.idx naming each by path. Here
# the releases are directories behind a file:// `releases` root, so the
# reader resolves every path the way it resolves GitHub's. Each way an index
# must be refused is asserted: a key nobody trusts, an edit after signing, no
# signature, a file of other bytes, another release's index, a named release
# with no `releases` root.
RI="$OUT/relidx"
rm -rf "$RI"
mkdir -p "$RI/pub" "$RI/rel/image-0.3.1"
for i in $(seq 1 2001); do
    printf '%064d 1 - p%05d.tar.xz binhost/p%05d.tar.xz\n' 0 "$i" "$i"
done > "$RI/bh.entries"
rel_shard binhost-0.3.1 1000 < "$RI/bh.entries" > "$RI/bh.sharded"
test "$(rel_release_counts < "$RI/bh.sharded" | tr '\n' ' ')" = \
     "binhost-0.3.1-01 1000 binhost-0.3.1-02 1000 binhost-0.3.1-03 1 " \
    || { echo "  2,001 files were not split 1,000, 1,000, 1:"; rel_release_counts < "$RI/bh.sharded"; exit 1; }
grep -q ' binhost-0.3.1-01 p01000.tar.xz binhost/p01000.tar.xz$' "$RI/bh.sharded" && \
    grep -q ' binhost-0.3.1-02 p01001.tar.xz binhost/p01001.tar.xz$' "$RI/bh.sharded" \
    || { echo "  the split is not in path order"; exit 1; }
test "$(head -n 7 "$RI/bh.entries" | rel_shard b 3 | rel_release_counts | tr '\n' ' ')" = "b-01 3 b-02 3 b-03 1 " \
    || { echo "  a cap of 3 did not split 7 files 3, 3, 1"; exit 1; }
test "$(rel_asset swcatalog/xml/kdos-binhost.xml)" = swcatalog--xml--kdos-binhost.xml && \
    test "$(rel_gh_name 'libsigc++-3.6.0-1.tar.xz')" = libsigc..-3.6.0-1.tar.xz \
    || { echo "  a path is not made flat, or GitHub's renaming is not predicted"; exit 1; }
rel_entries "$IM/r2/kdos-update.idx" | while read -r sha size rel asset path; do
    cp "$IM/r2/$asset" "$RI/rel/image-0.3.1/$asset"
    printf '%s %s image-0.3.1 %s %s\n' "$sha" "$size" "$asset" "$path"
done > "$RI/im.entries"
_rimake() {  # <version> <key> — the published index over both lanes, signed
    cat "$RI/im.entries" "$RI/bh.sharded" | rel_write "$RI/pub/kdos-update.idx" "$1" "file://$RI/rel"
    rm -f "$RI/pub/kdos-update.idx.sig"
    "$OUT/kdos-kpkg" kpkg sign "$RI/pub/kdos-update.idx" "$2" >/dev/null 2>&1
}
_rimake 0.3.1 "$IM/release.key"
test "$(rel_entries "$RI/pub/kdos-update.idx" | wc -l)" = $((2001 + $(wc -l < "$RI/im.entries"))) \
    || { echo "  the published index does not name every file"; exit 1; }
mkdir "$RI/A"
img unpack "$RI/pub" "$RI/A" > "$RI/out" 2>&1 \
    || { echo "  the image was not found through the published index:"; cat "$RI/out"; exit 1; }
cmp -s <(_imlist "$IM/v2") <(_imlist "$RI/A") \
    || { echo "  the root written through the published index is not 0.3.1"; exit 1; }
_rirefused() {  # <what>
    rm -rf "$RI/B"; mkdir "$RI/B"
    rc=0; img unpack "$RI/pub" "$RI/B" >"$RI/out" 2>&1 || rc=$?
    test "$rc" = 3 && test ! -e "$RI/B/usr" \
        || { echo "  $1 was used (exit $rc):"; cat "$RI/out"; exit 1; }
}
_rimake 0.3.1 "$IM/stranger.key";   _rirefused "an index signed by a key nobody trusts"
_rimake 0.3.1 "$IM/release.key"
sed -i 's|^releases .*|releases file:///nowhere|' "$RI/pub/kdos-update.idx"
_rirefused "an index edited after it was signed"
_rimake 0.3.1 "$IM/release.key"
rm "$RI/pub/kdos-update.idx.sig";   _rirefused "an unsigned index"
_rimake 0.3.2 "$IM/release.key";    _rirefused "the index of another release"
grep -q 'the slot image is 0.3.1 and the release index is 0.3.2' "$RI/out" \
    || { echo "  another release's index was refused for the wrong reason:"; cat "$RI/out"; exit 1; }
_rimake 0.3.1 "$IM/release.key"
sed -i '/^releases /d' "$RI/pub/kdos-update.idx"
rm -f "$RI/pub/kdos-update.idx.sig"
"$OUT/kdos-kpkg" kpkg sign "$RI/pub/kdos-update.idx" "$IM/release.key" >/dev/null 2>&1
_rirefused "an index naming a release with no releases root"
_rimake 0.3.1 "$IM/release.key"
cp "$RI/rel/image-0.3.1/kdos-image.manifest" "$RI/manifest.good"
cp "$IM/manifest.good" "$RI/rel/image-0.3.1/kdos-image.manifest"
sed -i 's/^time .*/time 1761700004/' "$RI/rel/image-0.3.1/kdos-image.manifest"
_rirefused "a manifest other than the one the index names"
grep -q 'does not match the signed release index' "$RI/out" \
    || { echo "  other bytes were refused for the wrong reason:"; cat "$RI/out"; exit 1; }
cp "$RI/manifest.good" "$RI/rel/image-0.3.1/kdos-image.manifest"
echo "  2,001 files split 1,000/1,000/1 in path order; found through the signed index alone; stranger, edit, no signature, other bytes, other release and no root refused"

echo
echo "==> a slot is armed with this machine's own configuration"
# kdt_image_carry, which every arm runs: a file a package installed and
# nobody edited takes the slot's newer copy; an edited or unowned one keeps
# this machine's, the slot's beside it as .kdos-new; accounts merge by name;
# fstab keeps the slot's root line; the slot's release record stays its own;
# machine state under /var follows whole.
CY="$OUT/carry"
rm -rf "$CY"
mkdir -p "$CY/run/etc/ssh" "$CY/run/var/lib/kpkg/db/.files" "$CY/run/var/lib/kdos" \
         "$CY/run/var/lib/bluetooth/aa" "$CY/slot/etc"
printf 'root:x:0:0::/root:/bin/sh\nkd:x:1000:1000::/home/kd:/bin/bash\n' > "$CY/run/etc/passwd"
printf 'root:x:0:0::/root:/bin/sh\nsddm:x:60:60::/var/lib/sddm:/bin/false\n' > "$CY/slot/etc/passwd"
printf 'UUID=AAAA-1111\t/\text4\tdefaults\t0 1\nUUID=HHHH\t/home\text4\tdefaults\t0 2\n' > "$CY/run/etc/fstab"
printf '# shipped\nUUID=BBBB-2222\t/\text4\tdefaults\t0 1\n' > "$CY/slot/etc/fstab"
printf 'default\n' > "$CY/run/etc/untouched.conf"
printf 'new default\n' > "$CY/slot/etc/untouched.conf"
printf 'edited\n' > "$CY/run/etc/edited.conf"
printf 'new default\n' > "$CY/slot/etc/edited.conf"
printf 'myhost\n' > "$CY/run/etc/hostname"
printf 'kdos\n' > "$CY/slot/etc/hostname"
printf 'host key\n' > "$CY/run/etc/ssh/ssh_host_ed25519_key"
chmod 600 "$CY/run/etc/ssh/ssh_host_ed25519_key"
printf 'VERSION="0.3.0"\n' > "$CY/run/etc/kdos-release"
printf 'VERSION="0.3.1"\n' > "$CY/slot/etc/kdos-release"
printf 'zlib\n' > "$CY/run/var/lib/kdos/world"
printf 'paired\n' > "$CY/run/var/lib/bluetooth/aa/info"
printf '%s 0644 ./etc/untouched.conf\n%s 0644 ./etc/edited.conf\n' \
    "$(sha256sum < "$CY/run/etc/untouched.conf" | cut -d' ' -f1)" \
    "$(printf 'as shipped\n' | sha256sum | cut -d' ' -f1)" > "$CY/run/var/lib/kpkg/db/.files/base"
"$OUT/kdos" image carry "$CY/run" "$CY/slot" >/dev/null \
    || { echo "  the carry failed"; exit 1; }
test "$(cat "$CY/slot/etc/untouched.conf")" = "new default" \
    || { echo "  an untouched file did not take the slot's newer copy"; exit 1; }
test "$(cat "$CY/slot/etc/edited.conf")" = edited && \
    test "$(cat "$CY/slot/etc/edited.conf.kdos-new")" = "new default" \
    || { echo "  an edited file was not kept with the new default beside it"; exit 1; }
test "$(cat "$CY/slot/etc/hostname")" = myhost && \
    test "$(stat -c %a "$CY/slot/etc/ssh/ssh_host_ed25519_key")" = 600 \
    || { echo "  this machine's own files were not carried as they are"; exit 1; }
test "$(cut -d: -f1 "$CY/slot/etc/passwd" | tr '\n' ' ')" = "root kd sddm " \
    || { echo "  the accounts did not merge by name:"; cat "$CY/slot/etc/passwd"; exit 1; }
printf 'UUID=BBBB-2222\t/\text4\tdefaults\t0 1\nUUID=HHHH\t/home\text4\tdefaults\t0 2\n' \
    | cmp -s - "$CY/slot/etc/fstab" \
    || { echo "  the slot's fstab is not this machine's with its own root:"; cat "$CY/slot/etc/fstab"; exit 1; }
grep -qx 'VERSION="0.3.1"' "$CY/slot/etc/kdos-release" \
    || { echo "  the slot's release record was overwritten"; exit 1; }
test "$(cat "$CY/slot/var/lib/kdos/world")" = zlib && \
    test "$(cat "$CY/slot/var/lib/bluetooth/aa/info")" = paired \
    || { echo "  machine state under /var was not carried"; exit 1; }
echo "  untouched takes the new default, edited keeps its own, accounts merge, fstab keeps the slot's root, state follows"

echo
echo "==> kdos update offers a newer slot image and writes it into the inactive slot"
# The update document's image lane, against the cached, verified manifest:
# offered only for a release newer than this system, with the size of the
# delta from the image this system was written from; never from a manifest
# edited after it was cached. Which slot receives it is the boot state's:
# the one not active, and none on a trial boot.
UI="$OUT/updimage"
rm -rf "$UI"
mkdir -p "$UI/run" "$UI/cache/image" "$UI/slot/etc" "$UI/esp/EFI/kdos" "$UI/bin" "$UI/keys"
img unpack "$IM/r1" "$UI/run" >/dev/null || { echo "  the running fixture did not unpack"; exit 1; }
printf 'VERSION="0.3.0"\nTREE_TIME="1761600000"\n' > "$UI/run/etc/kdos-release"
printf 'UUID=AAAA-1111\t/\text4\tdefaults\t0 1\n' > "$UI/run/etc/fstab"
printf 'myhost\n' > "$UI/run/etc/hostname"
printf 'UUID=BBBB-2222\t/\text4\tdefaults\t0 1\n' > "$UI/slot/etc/fstab"
cp "$IM/r2/kdos-image.manifest" "$UI/cache/image/MANIFEST"
cp "$IM/r2/kdos-image.manifest.sig" "$UI/cache/image/MANIFEST.sig"
cp "$IM/r2/kdos-update.idx" "$UI/cache/image/INDEX"
cp "$IM/r2/kdos-update.idx.sig" "$UI/cache/image/INDEX.sig"
printf 'file://%s\n' "$IM/r2" > "$UI/cache/image/ORIGIN"
printf 'kdos_slot=a root=UUID=AAAA-1111 rw\n' > "$UI/cmdline"
ln -sf "$OUT/kdos-tools" "$UI/bin/kdos-bootctl"
ln -sf "$OUT/kdos-kpkg" "$UI/bin/kpkg"
cp "$IM/release.pub" "$BH/builder.pub" "$UI/keys/"
uim() {
    env PATH="$UI/bin:$PATH" PORT_REPO="$UI/run/ports/core" KPKG_CONF=/nonexistent \
        PKGDB_DIR="$UI/run/var/lib/kpkg/db" KPKG_KEYRING="$UI/keys" \
        KDOS_UPDATE_STATE="$UI/state" KDOS_UPDATE_CACHE="$UI/cache" \
        KDOS_UPDATE_SYSROOT="$UI/run" KDOS_UPDATE_TREE="$UI/nowhere" \
        KDOS_BOOTSTATE="$UI/esp/EFI/kdos/bootstate" KDOS_CMDLINE="$UI/cmdline" \
        KDOS_SYSROOT="$UI/run" KDOS_BOOT_ID=image-boot KDOS_UPDATE_SLOT="$UI/slot" \
        KDOS_REBOOT_ENTRY="$UI/reboot-entry" "$@"
}
uim "$UI/bin/kdos-bootctl" set-slot a AAAA-1111 >/dev/null
uim "$UI/bin/kdos-bootctl" set-slot b BBBB-2222 >/dev/null
rc=0; uim "$OUT/kdos" update check --json > "$UI/doc" 2>/dev/null || rc=$?
test "$rc" = 1 || { echo "  check with an image on offer exited $rc, not 1"; exit 1; }
_imkey=$(awk '{ print $2; exit }' "$IM/r2/kdos-image.manifest.sig")
for want in '"offer":"0.3.1","lane":"image"' "\"size\":$_imdelta," \
            "\"signed_by\":{\"key\":\"$_imkey\"" "\"image\":\"file://$IM/r2\""; do
    grep -qF -- "$want" "$UI/doc" || { echo "  the update document lacks $want:"; cat "$UI/doc"; exit 1; }
done
uim "$OUT/kdos" update apply --dry-run > "$UI/plan" 2>&1 \
    || { echo "  the image dry run failed:"; cat "$UI/plan"; exit 1; }
grep -q "installing into slot b at $UI/slot" "$UI/plan" && grep -q 'the delta from 0.3.0' "$UI/plan" \
    || { echo "  slot a running did not put the image into slot b by its delta:"; cat "$UI/plan"; exit 1; }
sed -i 's/^active   = a$/active   = b/' "$UI/esp/EFI/kdos/bootstate"
printf 'kdos_slot=b root=UUID=BBBB-2222 rw\n' > "$UI/cmdline"
uim "$OUT/kdos" update apply --dry-run 2>&1 | grep -q "installing into slot a at $UI/slot" \
    || { echo "  slot b running did not choose slot a"; exit 1; }
sed -i 's/^active   = b$/active   = a/; s/^try      = $/try      = b/' "$UI/esp/EFI/kdos/bootstate"
rc=0; uim "$OUT/kdos" update apply --dry-run --progress 3 3>"$UI/prog" >/dev/null 2>&1 || rc=$?
test "$rc" = 3 && grep -q '^refused this boot runs slot b on trial' "$UI/prog" \
    || { echo "  a trial boot was not refused (exit $rc)"; cat "$UI/prog"; exit 1; }
sed -i 's/^try      = b$/try      = /' "$UI/esp/EFI/kdos/bootstate"
printf 'kdos_slot=a root=UUID=AAAA-1111 rw\n' > "$UI/cmdline"
cp "$UI/cache/image/MANIFEST" "$UI/manifest.good"
sed -i 's/^time .*/time 1761700002/' "$UI/cache/image/MANIFEST"
uim "$OUT/kdos" update check --json 2>/dev/null | grep -qF '"lane":null' \
    || { echo "  a cached manifest edited after it was verified was offered"; exit 1; }
rc=0; uim "$OUT/kdos" update apply --dry-run --image >/dev/null 2>&1 || rc=$?
test "$rc" = 3 || { echo "  --image with nothing on offer exited $rc, not 3"; exit 1; }
cp "$UI/manifest.good" "$UI/cache/image/MANIFEST"
# A genuine manifest beside a genuine release index of another release: the
# index an apply would find the bundles by does not name it.
cp "$IM/r1/kdos-update.idx" "$UI/cache/image/INDEX"
cp "$IM/r1/kdos-update.idx.sig" "$UI/cache/image/INDEX.sig"
uim "$OUT/kdos" update check --json 2>/dev/null | grep -qF '"lane":null' \
    || { echo "  a manifest its cached release index does not name was offered"; exit 1; }
cp "$IM/r2/kdos-update.idx" "$UI/cache/image/INDEX"
cp "$IM/r2/kdos-update.idx.sig" "$UI/cache/image/INDEX.sig"
printf 'VERSION="0.3.1"\nTREE_TIME="1761700000"\n' > "$UI/run/etc/kdos-release"
uim "$OUT/kdos" update check --json 2>/dev/null | grep -qF '"lane":null' \
    || { echo "  an image of the release this system runs was offered"; exit 1; }
# With no release time on this system's side the versions decide: an older
# release's manifest is signed too, and offering it is a downgrade.
mv "$UI/run/var/lib/kdos/image/MANIFEST" "$UI/record.held"
printf 'VERSION="0.3.2"\n' > "$UI/run/etc/kdos-release"
uim "$OUT/kdos" update check --json 2>/dev/null | grep -qF '"lane":null' \
    || { echo "  an image older than this system's version was offered with no times to compare"; exit 1; }
printf 'VERSION="0.3.0"\n' > "$UI/run/etc/kdos-release"
uim "$OUT/kdos" update check --json 2>/dev/null | grep -qF '"lane":"image"' \
    || { echo "  a newer image was not offered with no times to compare"; exit 1; }
mv "$UI/record.held" "$UI/run/var/lib/kdos/image/MANIFEST"
printf 'VERSION="0.3.0"\nTREE_TIME="1761600000"\n' > "$UI/run/etc/kdos-release"
if [ "$(id -u)" = 0 ]; then
    # The whole lane as kdos-sysupdated drives it: Acquire writes the slot
    # from the delta and checks it, Install carries this machine's own files
    # in, records the image's version and arms the slot. Then a fetch: the
    # binhost index and the image manifest are each kept only when they
    # verify, and a tampered one leaves the good copy in the cache.
    rc=0; uim "$OUT/kdos" update apply --acquire --progress 3 3>"$UI/prog" >"$UI/out" 2>&1 || rc=$?
    test "$rc" = 0 || { echo "  the image acquire exited $rc:"; cat "$UI/out"; exit 1; }
    grep -q "^verified $_imkey 0.3.1 " "$UI/prog" && grep -q '^item [0-9]* [0-9]* image$' "$UI/prog" && \
        test "$(tail -n1 "$UI/prog")" = 'acquired b 0.3.1' \
        || { echo "  the acquire's progress lines are:"; cat "$UI/prog"; exit 1; }
    cmp -s "$IM/v2/usr/lib/libbig.so" "$UI/slot/usr/lib/libbig.so" && \
        grep -qx 'try      = ' "$UI/esp/EFI/kdos/bootstate" \
        || { echo "  the acquire did not write 0.3.1 into the untried slot"; exit 1; }
    uim "$OUT/kdos" update apply --arm --progress 3 3>"$UI/prog" >"$UI/out" 2>&1 \
        || { echo "  the image arm failed:"; cat "$UI/out"; exit 1; }
    test "$(head -n1 "$UI/prog")" = 'verified - 0.3.1 0 0' && \
        test "$(tail -n1 "$UI/prog")" = 'armed b 0.3.1' \
        || { echo "  the arm's progress lines are:"; cat "$UI/prog"; exit 1; }
    grep -qx 'try      = b' "$UI/esp/EFI/kdos/bootstate" && \
        grep -qxF 'VERSION="0.3.1"' "$UI/slot/etc/kdos-release" && \
        grep -qxF 'INSTALLED_BY="image"' "$UI/slot/etc/kdos-release" \
        || { echo "  the armed slot is not recorded as image 0.3.1"; exit 1; }
    test "$(cat "$UI/slot/etc/hostname")" = myhost && \
        grep -q '^UUID=BBBB-2222	/	' "$UI/slot/etc/fstab" && \
        cmp -s "$IM/v2/ports/TREE" "$UI/slot/ports/TREE" \
        || { echo "  the slot lacks this machine's files, its own root or the image's tree"; exit 1; }
    mkdir -p "$UI/nokeys" "$UI/bh"
    cp "$BH/repo/"* "$UI/bh/"
    uimf() { uim env KDOS_TREE_KEYS="$UI/nokeys" KDOS_UPDATE_BINHOST="$UI/bh" \
                 KDOS_UPDATE_IMAGE="$1" "$OUT/kdos" update fetch >/dev/null 2>&1; }
    # The binhost's release index is for the release the tree in place
    # names; one for another release is refused before PACKAGES is read.
    _bhidx "$UI/bh" "$BH/builder.key" 0.2.99
    uimf "$IM/r2" || true
    grep -q '^binhost unverified .*is for 0.2.99, and this tree is 0.3.0' "$UI/state" && \
        test ! -e "$UI/cache/PACKAGES" \
        || { echo "  another release's binhost index was taken:"; cat "$UI/state"; exit 1; }
    _bhidx "$UI/bh" "$BH/builder.key" 0.3.0
    uimf "$IM/r2" || true
    grep -q '^binhost ok ' "$UI/state" && grep -q '^image ok .*image 0.3.1' "$UI/state" \
        || { echo "  a good index and image were not kept:"; cat "$UI/state"; exit 1; }
    cmp -s "$UI/bh/kdos-update.idx" "$UI/cache/INDEX" && test ! -e "$UI/cache/new" \
        || { echo "  the release index was not kept beside the binhost index"; exit 1; }
    cp "$UI/cache/PACKAGES" "$UI/packages.good"
    sed -i 's/^S:[0-9]*$/S:1/' "$UI/bh/PACKAGES"
    mkdir -p "$UI/r2bad"
    cp "$IM/r2/"* "$UI/r2bad/"
    sed -i 's/^time .*/time 1761700003/' "$UI/r2bad/kdos-image.manifest"
    uimf "$UI/r2bad" || true
    grep -q '^binhost unverified ' "$UI/state" && grep -q '^image unverified ' "$UI/state" \
        || { echo "  a tampered index or image was not refused:"; cat "$UI/state"; exit 1; }
    cmp -s "$UI/packages.good" "$UI/cache/PACKAGES" && \
        cmp -s "$IM/r2/kdos-image.manifest" "$UI/cache/image/MANIFEST" \
        || { echo "  a refused fetch replaced the verified copy"; exit 1; }
    echo "  offered by its delta's size; slot b, slot a, refused on trial; acquired, carried, armed; tampered index and image refused"
else
    echo "  offered by its delta's size; slot b, slot a, refused on trial (acquire, arm and fetch need root)"
fi

echo
echo "==> kdos-sysupdated answers Discover, over fixture slots"
# org.freedesktop.sysupdate1 and io.kdos.Slots1 on a bus of their own, with
# every file under a fixture root (--session --root): Discover's call order
# and the replies it acts on, the API diffed against the XML Discover 6.8.2
# generates its proxies from and against the System Versions page's contract,
# polkit's action ids and its interaction flag, the AppStream file, and the
# two-mark bless including the greeter's. testing/fixtures/sysupdated/drive.sh
# holds the cases; `kdos` there is a stub and `kdos-bootctl` the real one.
if [ -n "$TRAY_SDBUS" ] && command -v dbus-daemon >/dev/null 2>&1 &&
   command -v busctl >/dev/null 2>&1 && command -v dbus-send >/dev/null 2>&1; then
    SUO="$OUT/sysupdated"
    mkdir -p "$SUO"
    $CC $STD $WARN -Isrc/libs/libkbase $(pkg-config --cflags "$TRAY_SDBUS") \
        -o "$SUO/kdos-sysupdated" src/daemons/kdos-sysupdated/*.c \
        src/libs/libkbase/*.c $(pkg-config --libs "$TRAY_SDBUS") \
        || { echo "  FAIL  kdos-sysupdated does not build"; exit 1; }
    for _t in sysupclient polkitstub; do
        $CC $STD $WARN $(pkg-config --cflags "$TRAY_SDBUS") -o "$SUO/$_t" \
            "testing/fixtures/sysupdated/$_t.c" \
            $(pkg-config --libs "$TRAY_SDBUS") \
            || { echo "  FAIL  the $_t fixture does not build"; exit 1; }
    done
    bash testing/fixtures/sysupdated/drive.sh "$SUO/t" "$SUO/kdos-sysupdated" \
        "$SUO/sysupclient" "$SUO/polkitstub" "$OUT/kdos-tools" || exit 1
else
    echo "  kdos-sysupdated (skipped — no sd-bus, dbus-daemon, busctl or dbus-send on this host)"
fi

echo
echo "==> the initramfs unlocks a LUKS root, or says why it cannot"
# The generated init is a heredoc inside an image-phase script, which is exactly
# the kind of code nothing ever tests until it is 3 a.m. and a laptop will not
# boot. It is extracted, syntax-checked, and its unlock function is run against
# stub tools — which is what PASS_TTY and CRYPT_MAPPER_DIR exist for.
IR="$OUT/initramfs"
mkdir -p "$IR/bin" "$IR/mapper"
python3 - "$IR/init" <<'PYEOF' || { echo "  could not extract the generated init"; exit 1; }
import re, sys
s = open('script/phases/70_image/090_initramfs.sh').read()
m = re.search(r"cat > init <<EOF\n(.*?)\nEOF\n", s, re.S)
if not m:
    sys.exit(1)
body = m.group(1).replace('\\$', '$').replace('\\`', '`').replace('\\\\', '\\')
open(sys.argv[1], 'w').write(body)
PYEOF
bash -n "$IR/init" || { echo "  the generated init is not valid bash"; exit 1; }
grep -q "cryptdevice=" "$IR/init" || { echo "  the init does not parse cryptdevice="; exit 1; }
# The entry's slot reaches `select`, or a hand-picked entry boots the other
# slot's root on this slot's kernel.
grep -q 'kdos-bootctl select \${BOOT_SLOT:+"\$BOOT_SLOT"}' "$IR/init" \
    || { echo "  the init does not hand kdos_slot= to select"; exit 1; }
# The passphrase must never reach argv: /proc/<pid>/cmdline is world-readable.
grep -q -- "--key-file=-" "$IR/init" \
    || { echo "  the passphrase is not fed on stdin"; exit 1; }
grep -q "cryptsetup open .*\"\$PASS\"" "$IR/init" \
    && { echo "  the passphrase is passed as an argument"; exit 1; }

# THE STUB ANSWERS `-U <uuid>` AND NOTHING ELSE, which is the whole of what
# unlock_root asks of it. A stub that echoed a device whatever it was handed
# could not tell a blkid that implements the UUID LOOKUP from one that does
# not, and a prober without it drops an installed machine to a shell.
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

# AND THE VOLUME GROUPS, ACTIVATED ONLY WHEN THERE ARE ANY. activate_lvm runs
# on each side of the unlock; a disk boot with no LVM must never start lvm, a
# second call that sees no new physical volume must not activate again, and
# one that sees the volume group an unlock just revealed must. The stubs
# answer `-t TYPE=LVM2_member -o device` and count the activations.
LV="$OUT/lvm"
mkdir -p "$LV/bin"
cat > "$LV/bin/blkid" <<'EOF'
#!/bin/sh
[ "$1 $2 $3 $4" = "-t TYPE=LVM2_member -o device" ] || exit 2
cat "$FAKE_PVS" 2>/dev/null
EOF
cat > "$LV/bin/lvm" <<'EOF'
#!/bin/sh
[ "$1 $2 $3" = "vgchange -aay --sysinit" ] || exit 2
echo x >> "$LVM_CALLS"
EOF
# The targets a thin or cached root needs are loaded before the first
# activation, once: the stub records what it was asked for, never loads it.
cat > "$LV/bin/modprobe" <<'EOF'
#!/bin/sh
echo "$*" >> "$LVM_MODS"
EOF
chmod +x "$LV/bin/"*
lvm_try() {
    ( . /dev/stdin <<EOF
$(sed -n '/^activate_lvm() {/,/^}/p' "$IR/init")
EOF
      sp_total() { :; }; sp_step() { :; }; sp_ok() { :; }; sp_fail() { :; }
      udevadm() { :; }
      export FAKE_PVS="$LV/pvs" LVM_CALLS="$LV/calls" LVM_MODS="$LV/mods"
      PATH="$LV/bin:$PATH" LVM_BIN="$LV/bin/lvm" LVM_PVS=""
      : > "$LV/pvs"
      activate_lvm
      printf '/dev/sda2\n' > "$LV/pvs"
      activate_lvm; activate_lvm
      printf '/dev/sda2\n/dev/mapper/kdosroot\n' > "$LV/pvs"
      activate_lvm ) >/dev/null 2>&1
}
: > "$LV/calls"
: > "$LV/mods"
lvm_try
[ "$(wc -l < "$LV/calls")" -eq 2 ] \
    || { echo "  activate_lvm ran lvm $(wc -l < "$LV/calls") times, not 2 (none, once per new PV set)"; exit 1; }
[ "$(wc -l < "$LV/mods")" -eq 1 ] && grep -q 'dm-thin-pool' "$LV/mods" \
    && grep -q 'dm-cache' "$LV/mods" \
    || { echo "  the thin and cache targets are not loaded once, before the first activation"; exit 1; }
grep -qE '^[[:space:]]+activate_lvm$' "$IR/init" \
    || { echo "  the init no longer activates volume groups after the unlock"; exit 1; }
sed -n '/Wait for device to appear/,/^    done$/p' "$IR/init" | grep -q 'activate_lvm' \
    || { echo "  the root wait no longer activates a late physical volume's group"; exit 1; }
echo "  volume groups: no lvm without a PV, once per newly seen set of PVs, thin and cache targets first"

# AND THE PERSISTENCE STORE IS LOOKED FOR UNTIL USB STORAGE HAS SETTLED.
# find_persist reads a fake USB device tree (USB_DEVICES) and a blkid stub
# that answers `-L KDOS_PERSIST` from its Nth call on, running $PS/on_<n> as
# that call is made so the tree can change mid-wait. A stick whose disk has
# not been scanned is waited for; a boot with nothing pending pays one
# lookup, and one more for each USB disk, whose partitions the first lookup
# may not yet see; a stick that never answers costs the 10-second bound and
# no more.
PS="$OUT/persist"
mkdir -p "$PS/bin"
cat > "$PS/bin/blkid" <<'EOF'
#!/bin/sh
[ "$1 $2" = "-L KDOS_PERSIST" ] || exit 2
n=$(( $(cat "$PS/count") + 1 )); echo "$n" > "$PS/count"
[ -x "$PS/on_$n" ] && "$PS/on_$n"
[ -n "$STORE_AT" ] && [ "$n" -ge "$STORE_AT" ] && { echo /dev/sdb1; exit 0; }
exit 2
EOF
chmod +x "$PS/bin/blkid"
persist_try() {   # <store at call N, or ""> -> prints "<rc> <calls> <PERSIST>"
    echo 0 > "$PS/count"
    ( . /dev/stdin <<EOF
$(sed -n '/^: ".*USB_DEVICES:=/p; /^uptime_cs() {/,/^}/p; /^usb_scan() {/,/^}/p; /^find_persist() {/,/^}/p' "$IR/init")
EOF
      export PS STORE_AT="$1"
      PATH="$PS/bin:$PATH" USB_DEVICES="$PS/sys"
      find_persist >/dev/null 2>&1
      echo "$? $(cat "$PS/count") $PERSIST" )
}
persist_tree() {  # a root hub with two empty ports, plus what the case adds
    rm -rf "$PS/sys" "$PS"/on_*
    mkdir -p "$PS/sys/usb1" "$PS/sys/1-0:1.0/usb1-port1" "$PS/sys/1-0:1.0/usb1-port2"
    echo 09 > "$PS/sys/usb1/bDeviceClass"
    echo "not attached" > "$PS/sys/1-0:1.0/usb1-port1/state"
    echo "not attached" > "$PS/sys/1-0:1.0/usb1-port2/state"
}
persist_stick() { # <device>: configured, its mass-storage interface not yet scanned
    mkdir -p "$PS/sys/$1" "$PS/sys/$1:1.0/host2"
    echo 00 > "$PS/sys/$1/bDeviceClass"
    echo 08 > "$PS/sys/$1:1.0/bInterfaceClass"
}
persist_on() {    # <n> <shell line>: run at the nth lookup
    printf '#!/bin/sh\n%s\n' "$2" > "$PS/on_$1"; chmod +x "$PS/on_$1"
}
_disk='mkdir -p "$PS/sys/1-1:1.0/host2/target2:0:0/2:0:0:0/block/sdb"'

persist_tree
[ "$(persist_try "")" = "1 1 " ] \
    || { echo "  with no USB storage pending, find_persist did not stop after one lookup"; exit 1; }
persist_tree; persist_stick 1-1; persist_on 5 "$_disk"
[ "$(persist_try 6)" = "0 6 /dev/sdb1" ] \
    || { echo "  a store on a stick scanned late was not found ($(persist_try 6))"; exit 1; }
persist_tree; persist_stick 1-1; persist_on 5 "$_disk"
[ "$(persist_try "")" = "1 7 " ] \
    || { echo "  a late stick with no store did not end the wait one lookup after its disk appeared"; exit 1; }
persist_tree; persist_stick 1-1; eval "$_disk"
[ "$(persist_try 2)" = "0 2 /dev/sdb1" ] \
    || { echo "  a store whose partition the first lookup missed was not looked for again"; exit 1; }
persist_tree; persist_stick 1-1; eval "$_disk"
[ "$(persist_try "")" = "1 2 " ] \
    || { echo "  a USB disk already scanned cost more than one extra lookup ($(persist_try ""))"; exit 1; }
persist_tree; echo default > "$PS/sys/1-0:1.0/usb1-port2/state"
persist_on 3 'echo configured > "$PS/sys/1-0:1.0/usb1-port2/state"; mkdir -p "$PS/sys/1-1" "$PS/sys/1-1:1.0/host2"; echo 08 > "$PS/sys/1-1:1.0/bInterfaceClass"'
persist_on 8 "$_disk"
[ "$(persist_try 9)" = "0 9 /dev/sdb1" ] \
    || { echo "  a store on a device still enumerating was not found"; exit 1; }
persist_tree; mkdir -p "$PS/sys/1-2"; echo 09 > "$PS/sys/1-2/bDeviceClass"
_r=$(persist_try ""); _n=${_r#1 }; _n=${_n% }
[ "${_r%% *}" = 1 ] && [ "$_n" -ge 5 ] && [ "$_n" -le 13 ] \
    || { echo "  behind an idle hub, find_persist did not wait its one quiet second ($_r)"; exit 1; }
persist_tree; persist_stick 1-1
_t0=$(date +%s)
_r=$(persist_try "")
_t=$(( $(date +%s) - _t0 ))
[ "${_r%% *}" = 1 ] && [ "$_t" -ge 9 ] && [ "$_t" -le 12 ] \
    || { echo "  a stick that never scans did not cost exactly the 10-second bound ($_r, ${_t}s)"; exit 1; }
grep -qE '^[[:space:]]+find_persist ;;$' "$IR/init" \
    || { echo "  the overlay no longer looks for its store through find_persist"; exit 1; }
echo "  persistence store: one lookup when USB is idle, one more per new disk, waits for a late stick, bounded at 10 s"

# AND THE blkid THE INITRAMFS SHIPS IS UTIL-LINUX'S. Every lookup in the
# generated init is `blkid -U` — the root filesystem, the ESP holding the A/B
# state, and the LUKS container — and a prober without `-U` or `crypto_LUKS`
# drops an installed machine to a shell with "Root device not found". The
# build holds blkid, switch_root and the other util-linux programs to their
# own --version before it copies them.
_ini=script/phases/70_image/090_initramfs.sh
grep -q 'copy_closure bin/blkid /usr/sbin/blkid' "$_ini" \
    || { echo "  the initramfs no longer copies util-linux's blkid"; exit 1; }
grep -q '^util_linux_only /usr/sbin/blkid$' "$_ini" \
    || { echo "  the initramfs blkid is copied without its util-linux check"; exit 1; }
grep -q '\*" from util-linux "\*) return 0' "$_ini" \
    || { echo "  util_linux_only no longer asks --version for util-linux"; exit 1; }
for _u in switch_root mount umount losetup dmesg; do
    sed -n '/^for _p in \/usr\/sbin\/switch_root/,/^done$/p' "$_ini" | grep -q "/$_u " \
        || sed -n '/^for _p in \/usr\/sbin\/switch_root/,/^done$/p' "$_ini" | grep -q "/$_u\$" \
        || { echo "  $_u is no longer held to util-linux in the initramfs"; exit 1; }
done
unset _ini _u
echo "  the initramfs blkid, switch_root, mount, umount, losetup and dmesg are util-linux's"
# AND NO PORT INSTALLS A blkid UNDER /usr/bin. $PATH puts /usr/bin ahead of
# /usr/sbin, so one there would answer every `blkid` the running system and the
# build host run instead of util-linux's. Asked of the built tree's package
# database, the only record of what each port installs; a checkout with no
# build has nothing to ask.
_kdb=build/fs/var/lib/kpkg/db
if [ -d "$_kdb" ]; then
    _bk=$(grep -lxE '\./(usr/)?bin/blkid' "$_kdb"/* 2>/dev/null || true)
    [ -z "$_bk" ] \
        || { echo "  a port installs a blkid under /usr/bin: ${_bk##*/}"; exit 1; }
    echo "  no installed port puts a blkid under /usr/bin"
else
    echo "  no port puts a blkid under /usr/bin (skipped — no built tree)"
fi
unset _kdb _bk

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
echo "==> the boot splash draws two motions, and the command line picks one"
# kdos-splash runs before any root filesystem exists, so its one setting is a
# word on the kernel command line, and nothing but a boot reads that word. The
# preview renders with the frame code the boot runs and no framebuffer: the
# settled frame in each motion, and the first frame of Modern's fade, which a
# handover from Limine needs to be black. The font is a fixture with every
# glyph solid, so no frame depends on the host carrying Terminus, and the rows
# compared are the empty band above the mark, which no glyph reaches.
SP="$OUT/splash"
mkdir -p "$SP"
$CC $STD $WARN -Isrc/libs/libkcolor -o "$SP/kdos-splash" \
    src/art/kdos-splash/kdos-splash.c -lm
{ printf '\066\004\000\020'; head -c 4096 /dev/zero | tr '\000' '\377'; } \
    > "$SP/splash.psf"
spl() { ( cd "$SP" && ./kdos-splash preview 320x200 "$@" ); }
# One row of a frame, after the 15-byte "P6\n320 200\n255\n" header.
spl_row() { tail -c +"$((16 + $2 * 960))" "$SP/$1" | head -c 960; }
spl 2 modern.ppm modern && spl 2 phosphor.ppm phosphor \
    || { echo "  preview failed"; exit 1; }
cmp -s "$SP/modern.ppm" "$SP/phosphor.ppm" \
    && { echo "  the two motions render the same frame"; exit 1; }
cmp -s <(spl_row modern.ppm 0) <(spl_row modern.ppm 1) \
    || { echo "  Modern draws scanlines"; exit 1; }
cmp -s <(spl_row phosphor.ppm 0) <(spl_row phosphor.ppm 1) \
    && { echo "  Phosphor draws no scanlines"; exit 1; }
spl 0 start.ppm modern || { echo "  preview at 0 s failed"; exit 1; }
[ "$(tail -c +16 "$SP/start.ppm" | tr -d '\000' | wc -c)" = 0 ] \
    || { echo "  Modern's power-on does not start from black"; exit 1; }
# The command line: the last kdos.splash= wins, and anything but phosphor,
# an absent word or a command line that cannot be read is Modern.
printf 'quiet kdos.splash=phosphor\n' > "$SP/cl-ph"
printf 'kdos.splash=phosphor quiet kdos.splash=modern\n' > "$SP/cl-mo"
printf 'kdos.splash=PHOSPHOR\n' > "$SP/cl-case"
printf 'root=UUID=1 ro\n' > "$SP/cl-none"
for _c in ph:phosphor mo:modern case:modern none:modern missing:modern; do
    KDOS_SPLASH_CMDLINE="$SP/cl-${_c%%:*}" spl 2 "cl.ppm" \
        || { echo "  preview under cl-${_c%%:*} failed"; exit 1; }
    cmp -s "$SP/cl.ppm" "$SP/${_c#*:}.ppm" \
        || { echo "  cl-${_c%%:*} did not give ${_c#*:}"; exit 1; }
done
spl 2 bad.ppm sepia 2>/dev/null \
    && { echo "  preview took a motion that does not exist"; exit 1; }
echo "  Modern is flat and starts from black, Phosphor has scanlines, the command line chooses"

echo
echo "==> kdosbuild reads the build tree correctly"
# libkbuild is the only view of the build tree, so there is nothing to diff it
# against: what stands in for a differential is its own assertions in
# src/libs/selftest.c plus the end-to-end run below, which drives the same code
# over a real tree.
"$OUT/kdosbuild" --script-dir script --list >/dev/null 2>&1 \
    || { echo "  kdosbuild cannot read script/"; exit 1; }
# --list still answers when a phase is unreadable, and names it on stderr: a
# name there is a phase every real build refuses before its first step.
mkdir -p "$OUT/empty"
"$OUT/kdosbuild" --script-dir script --build-dir "$OUT/empty" --list \
    >/dev/null 2>"$OUT/list-err"
grep -q '^kdosbuild:' "$OUT/list-err" \
    && { echo "  a phase in script/phases cannot run:"; cat "$OUT/list-err"; exit 1; }
echo "  phase discovery and snapshot inventory"

# A phase that cannot say what it installs refuses the whole build with exit 2
# and runs nothing: both packages.txt and packages.d/ (which list is a guess),
# a packages.d/ with no *.txt, a directory with no list and no step (each would
# install nothing and report success), and a script directory with no phase.
# Each is planted beside a step that would leave a mark had it run.
kb_refuses() {  # <tree> <expected stderr text>
    local rc=0
    ( cd "$1" && "$OUT/kdosbuild" --script-dir script --build-dir build --fresh ) \
        > "$1/out.log" 2>&1 || rc=$?
    [ "$rc" = 2 ] || { echo "  $1: exit $rc, not 2"; cat "$1/out.log"; exit 1; }
    grep -qF -- "$2" "$1/out.log" || { echo "  $1: no '$2'"; cat "$1/out.log"; exit 1; }
    [ -e "$1/ran" ] && { echo "  $1: a step ran before the refusal"; exit 1; }
    return 0
}
kb_tree() {  # <tree>: one runnable phase that marks the tree when it runs
    mkdir -p "$1/script/phases/00_ok" "$1/build"
    printf '#!/bin/bash\ntouch "$PWD/ran"\n' > "$1/script/phases/00_ok/00_mark.sh"
    chmod +x "$1/script/phases/00_ok/00_mark.sh"
}
KR="$OUT/kbrefuse"
kb_tree "$KR/both"
mkdir -p "$KR/both/script/phases/10_pk/packages.d"
echo zlib > "$KR/both/script/phases/10_pk/packages.txt"
echo zlib > "$KR/both/script/phases/10_pk/packages.d/base.txt"
kb_refuses "$KR/both" "10_pk has both packages.txt and packages.d/"
kb_tree "$KR/nolist"
mkdir -p "$KR/nolist/script/phases/10_pk/packages.d"
echo zlib > "$KR/nolist/script/phases/10_pk/packages.d/base.list"
kb_refuses "$KR/nolist" "10_pk/packages.d/ holds no *.txt"
kb_tree "$KR/bare"
mkdir -p "$KR/bare/script/phases/10_pk"
echo 'export KDOS_PHASE_TITLE="Nothing"' > "$KR/bare/script/phases/10_pk/phase.env"
kb_refuses "$KR/bare" "10_pk has no packages.txt, no packages.d/ and no *.sh"
mkdir -p "$KR/none/script/phases" "$KR/none/build"
kb_refuses "$KR/none" "no phase under script/phases/"
echo "  an unreadable phase refuses the build before any step runs"

echo
echo "==> kdosbuild runs a build end to end"
# A synthetic two-phase tree, driven headless. This is the only test that
# exercises the ENGINE — forking steps, capturing their output, writing the
# logs, tarring the snapshot, extracting it again — rather than a decision.
mkdir -p "$OUT/empty"
E="$OUT/e2e"
mkdir -p "$E/script/phases/00_alpha" "$E/script/phases/01_beta" "$E/build" "$E/bin"
cat > "$E/script/phases/00_alpha/phase.env" <<'EOF'
export KDOS_PHASE_TITLE="Alpha Phase"
export KDOS_SNAPSHOT_PATHS="fs"
export KDOS_SNAPSHOT_EXCLUDE="fs/tmp/*"
rm -rf /var/cache/kpkg/work
EOF
cat > "$E/script/phases/01_beta/phase.env" <<'EOF'
export KDOS_SNAPSHOT_PATHS="fs ports"
EOF
cat > "$E/script/phases/00_alpha/00_tree.sh" <<'EOF'
#!/bin/bash
# Title: build the rootfs
set -e
mkdir -p "$PWD/build/fs/usr/bin" "$PWD/build/fs/tmp"
echo hello > "$PWD/build/fs/usr/bin/hello"
printf 'ansi \033[31mred\033[0m and\ttab\n'
printf 'no trailing newline'
EOF
cat > "$E/script/phases/01_beta/00_ports.sh" <<'EOF'
#!/bin/bash
mkdir -p "$PWD/build/ports"; echo port > "$PWD/build/ports/one"
echo "replay=$KDOS_REPLAY"
EOF
chmod +x "$E"/script/phases/*/*.sh
KB="$OUT/kdosbuild"

( cd "$E" && "$KB" --script-dir script --build-dir build --fresh --snapshot ) > "$OUT/e2e.log" 2>&1
grep -q "BUILD COMPLETE" "$OUT/e2e.log" || { echo "  build did not complete"; cat "$OUT/e2e.log"; exit 1; }
[ -f "$E/build/snapshots/00_alpha/manifest.json" ] || { echo "  no snapshot written"; exit 1; }
[ -f "$E/build/logs/00_alpha/0000_tree.sh.log" ] || { echo "  no step log written"; exit 1; }
# The log FILE is verbatim, as the step wrote it — escapes and all; only the
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

# A plan writes no snapshot unless --snapshot asks, and must set KDOS_REPLAY
# for a step it named — 17 phase scripts exit 0 on a second pass without it.
beta_id=$(grep -o '"id": "[^"]*"' "$E/build/snapshots/01_beta/manifest.json")
# The first match is the figure: "steps" is written before "machine", which
# names the same key with a machine id.
alpha_t=$(grep -o '"00_alpha/build the rootfs": [0-9.]*' "$E/build/snapshots/timings.json" | head -n 1)
[ -n "$alpha_t" ] && grep -q '"01_beta/ports": [0-9.]*' "$E/build/snapshots/timings.json" \
    && grep -q '"00_alpha": [0-9.]*' "$E/build/snapshots/timings.json" \
    || { echo "  the build recorded no step timings"; cat "$E/build/snapshots/timings.json"; exit 1; }
( cd "$E" && "$KB" --script-dir script --build-dir build \
    --steps beta:00_ports.sh ) > "$OUT/e2e-plan.log" 2>&1
grep -qF "$alpha_t" "$E/build/snapshots/timings.json" \
    || { echo "  a narrower run lost a step it did not run from timings.json"; exit 1; }
[ "$(grep -o '"id": "[^"]*"' "$E/build/snapshots/01_beta/manifest.json")" = "$beta_id" ] \
    || { echo "  a plan wrote a snapshot nobody asked for"; exit 1; }
grep -q "replay=1" "$E"/build/logs/01_beta/*.log \
    || { echo "  KDOS_REPLAY not set for a named step"; exit 1; }
echo "  a plan writes no snapshot unasked, and sets KDOS_REPLAY"

# A failing step stops the build, exits 1, and does NOT snapshot the phase.
F="$OUT/fail"
mkdir -p "$F/script/phases/00_a" "$F/build"
echo 'export KDOS_SNAPSHOT_PATHS="fs"' > "$F/script/phases/00_a/phase.env"
printf '#!/bin/bash\necho starting\nexit 3\n' > "$F/script/phases/00_a/00_boom.sh"
printf '#!/bin/bash\necho unreachable\n' > "$F/script/phases/00_a/01_after.sh"
chmod +x "$F"/script/phases/*/*.sh
if ( cd "$F" && "$KB" --script-dir script --build-dir build --fresh --snapshot ) \
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
grep -q '"phases": \[{"phase": "00_a", "status": "failed", "seconds": [0-9.]*}\]' \
    "$OUT/fail.json" || { echo "  the result does not time its phases"; cat "$OUT/fail.json"; exit 1; }
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

# A snapshot under a phase that declares no paths is a leftover: `latest`
# skips it, restoring it is refused, the layered plan leaves it out, and the
# inventory marks it so `--delete` can find it.
L="$OUT/leftover"
rm -rf "$L"
mkdir -p "$L/script/phases/00_base" "$L/script/phases/01_img" "$L/build"
echo 'export KDOS_SNAPSHOT_PATHS="fs"' > "$L/script/phases/00_base/phase.env"
echo 'export KDOS_SNAPSHOT_PATHS=""' > "$L/script/phases/01_img/phase.env"
printf '#!/bin/bash
mkdir -p "$PWD/build/fs"; echo base > "$PWD/build/fs/who"
' \
    > "$L/script/phases/00_base/00_tree.sh"
printf '#!/bin/bash
echo img > "$PWD/build/fs/who"
' > "$L/script/phases/01_img/00_img.sh"
chmod +x "$L"/script/phases/*/*.sh
( cd "$L" && "$KB" --script-dir script --build-dir build --fresh --snapshot ) > "$OUT/leftover.log" 2>&1
grep -q "BUILD COMPLETE" "$OUT/leftover.log" || { echo "  leftover fixture did not build"; cat "$OUT/leftover.log"; exit 1; }
[ -d "$L/build/snapshots/01_img" ] && { echo "  a phase with no paths was snapshotted"; exit 1; }
cp -a "$L/build/snapshots/00_base" "$L/build/snapshots/01_img"
( cd "$L" && "$KB" --script-dir script --build-dir build --restore latest ) \
    > "$OUT/leftover-restore.log" 2>&1
grep -q "restored 00_base" "$OUT/leftover-restore.log" \
    || { echo "  --restore latest took the leftover"; cat "$OUT/leftover-restore.log"; exit 1; }
[ "$(cat "$L/build/fs/who")" = img ] \
    || { echo "  the phase after the restore did not re-run"; exit 1; }
( cd "$L" && "$KB" --script-dir script --build-dir build --restore 01_img ) \
    > "$OUT/leftover-refuse.log" 2>&1 && { echo "  restored a leftover"; exit 1; }
grep -q "01_img declares no snapshot paths; its snapshot is a leftover" \
    "$OUT/leftover-refuse.log" || { echo "  leftover refusal not explained"; cat "$OUT/leftover-refuse.log"; exit 1; }
( cd "$L" && "$KB" --script-dir script --build-dir build --list ) 2>&1 \
    | grep "01_img" | grep -q "leftover (phase declares no paths)" \
    || { echo "  --list does not mark the leftover"; exit 1; }
( cd "$L" && "$KB" --script-dir script --build-dir build --list --json ) 2>&1 \
    | grep -q '"phase": "01_img".*"leftover": true' \
    || { echo "  --list --json does not mark the leftover"; exit 1; }
echo "  a leftover snapshot is listed, refused and never layered"

# Without --snapshot a complete build writes none.
rm -rf "$L/build/snapshots"
( cd "$L" && "$KB" --script-dir script --build-dir build --fresh ) > "$OUT/nosnap.log" 2>&1
grep -q "BUILD COMPLETE" "$OUT/nosnap.log" || { echo "  the default run did not build"; cat "$OUT/nosnap.log"; exit 1; }
[ -d "$L/build/snapshots/00_base" ] && { echo "  a default run wrote a snapshot"; exit 1; }
echo "  snapshot writing is off unless --snapshot asks"

echo
echo "==> kdosbuild snapshots are layers, and every layer restores exactly"
# Three phases whose steps make the changes a layer has to carry: a file
# rewritten in place at the same size and mtime, one replaced by unlink and
# create, a subtree deleted, a directory renamed, a file turned into a
# directory and a directory into a symlink, a symlink retargeted, hard links
# added and removed, a chmod alone, files changed inside a directory that
# itself did not change, and files under an excluded path. Each
# step saves the tree as it left it, outside fs; every restore is compared
# with that, entry by entry, so a layer that missed a change or a .gone list
# that deleted too much is a diff here.
S="$OUT/layers"
rm -rf "$S"
mkdir -p "$S/build"
for p in 01_a 02_b 03_c; do
    mkdir -p "$S/script/phases/$p"
    printf 'export KDOS_SNAPSHOT_PATHS="fs"\nexport KDOS_SNAPSHOT_EXCLUDE="fs/tmp/*"\n' \
        > "$S/script/phases/$p/phase.env"
    cat > "$S/script/phases/$p/00_edit.sh" <<EOF
#!/bin/bash
set -e
N=$p
EOF
    cat >> "$S/script/phases/$p/00_edit.sh" <<'EOF'
B=$PWD/build F=$PWD/build/fs
[ -e "$B/stop" ] && { echo "stopped before $N"; exit 1; }
if [ -e "$B/m$N" ]; then
    date +%s%N > "$F/retake-$N"; rm -rf "$F/keep/f3"
else case $N in
01_a)
    mkdir -p "$F/keep" "$F/del/sub" "$F/ren/in" "$F/d2s" "$F/tmp/junk"
    for i in 1 2 3 4; do echo "file $i" > "$F/keep/f$i"; done
    echo sub > "$F/del/sub/a"; echo ren > "$F/ren/in/b"; echo q > "$F/d2s/q"
    printf AAAA > "$F/inplace"; touch -d @1700000000 "$F/inplace"
    printf old > "$F/replace"; touch -d @1700000000 "$F/replace"
    echo x > "$F/hl1"; ln "$F/hl1" "$F/hl2"; ln "$F/hl1" "$F/hl3"
    echo t > "$F/f2d"; ln -s keep/f1 "$F/sym"; echo j > "$F/tmp/junk/j"
    touch -d @1600000000 "$F/keep" ;;
02_b)
    printf BBBB | dd of="$F/inplace" conv=notrunc 2>/dev/null
    touch -d @1700000000 "$F/inplace"
    rm "$F/replace"; printf new > "$F/replace"; touch -d @1700000000 "$F/replace"
    rm -rf "$F/del"; mv "$F/ren" "$F/renamed"; rm "$F/hl3"; ln "$F/keep/f2" "$F/f2link"
    rm "$F/f2d"; mkdir "$F/f2d"; echo in > "$F/f2d/in"
    rm -rf "$F/d2s"; ln -s keep "$F/d2s"; ln -sfn keep/f2 "$F/sym"
    chmod 0600 "$F/keep/f4"; echo j2 > "$F/tmp/junk/j2"
    printf CCCC | dd of="$F/keep/f1" conv=notrunc 2>/dev/null ;;
03_c)
    rm -rf "$F/renamed/in"; echo late > "$F/keep/f5"; chmod 0700 "$F/keep" ;;
esac; fi
( cd "$B" && find fs -path fs/tmp -prune -o -exec stat -c '%n %F %a %u:%g %Y %s %h %N' {} + \
    | LC_ALL=C sort; find fs -path fs/tmp -prune -o -type f -exec sha1sum {} + | LC_ALL=C sort ) > "$B/m$N"
EOF
    chmod +x "$S/script/phases/$p/00_edit.sh"
done
KL() { ( cd "$S" && "$KB" --script-dir script --build-dir build --plain --snapshot "$@" ); }
kind_of() {  # <phase dir> -> "full" or "layer:<base id>"
    grep -o '"kind": "[a-z]*"\|"base": "[^"]*"' "$S/build/snapshots/$1/manifest.json" \
        | sed 's/.*": "//; s/"$//' | paste -sd: -
}
id_of() { grep -o '"id": "[^"]*"' "$S/build/snapshots/$1/manifest.json" | sed 's/.*": "//; s/"$//'; }
same_tree() {  # <phase dir>: the restored tree against what its step saved
    ( cd "$S/build" && find fs -path fs/tmp -prune -o -exec stat -c '%n %F %a %u:%g %Y %s %h %N' {} + \
        | LC_ALL=C sort; find fs -path fs/tmp -prune -o -type f -exec sha1sum {} + | LC_ALL=C sort ) \
        > "$S/now"
    cmp -s "$S/now" "$S/build/m$1" \
        || { echo "  restoring $1 did not give back its tree:"; diff "$S/build/m$1" "$S/now" | head; exit 1; }
}
restore_only() {  # <phase dir>, with the phase after it stopped before it runs
    rm -rf "$S/build/fs"; touch "$S/build/stop"
    KL --restore "$1" > "$OUT/layers-restore.log" 2>&1 || true
    rm -f "$S/build/stop"
    grep -q "restored $1" "$OUT/layers-restore.log" \
        || { echo "  restore of $1 failed"; cat "$OUT/layers-restore.log"; exit 1; }
}
KL --fresh > "$OUT/layers.log" 2>&1
grep -q "BUILD COMPLETE" "$OUT/layers.log" || { echo "  layered fixture did not build"; cat "$OUT/layers.log"; exit 1; }
[ "$(kind_of 01_a)" = full ] || { echo "  the first snapshot is not full: $(kind_of 01_a)"; exit 1; }
[ "$(kind_of 02_b)" = "layer:$(id_of 01_a)" ] || { echo "  02_b is not a layer on 01_a: $(kind_of 02_b)"; exit 1; }
[ "$(kind_of 03_c)" = "layer:$(id_of 02_b)" ] || { echo "  03_c is not a layer on 02_b: $(kind_of 03_c)"; exit 1; }
grep -q "snapshot 02_b -> .* (layer on 01_a)" "$OUT/layers.log" \
    || { echo "  the snapshot notice does not name the base"; exit 1; }
for p in 01_a 02_b 03_c 02_b; do restore_only $p; same_tree $p; done
echo "  full, then layers on the snapshot before; each restores its own tree"

# A tree already past 01_a: 01_a is taken full again, and everything the old
# one supported is held until the snapshots replacing them no longer need it.
restore_only 03_c
KL --continue-from 01_a > "$OUT/layers-retake.log" 2>&1 \
    || { echo "  the retake did not build"; cat "$OUT/layers-retake.log"; exit 1; }
[ "$(kind_of 01_a)" = full ] || { echo "  a retake on a later tree is not full"; exit 1; }
[ "$(kind_of 02_b)" = "layer:$(id_of 01_a)" ] || { echo "  02_b does not layer on the new 01_a"; exit 1; }
[ -z "$(ls "$S/build/snapshots/.held" 2>/dev/null)" ] \
    || { echo "  held snapshots outlived their dependants: $(ls "$S/build/snapshots/.held")"; exit 1; }
for p in 01_a 02_b 03_c; do restore_only $p; same_tree $p; done
echo "  a retaken phase is full; the old chain is held until nothing needs it"

KL --delete 03_c > /dev/null
KL --delete 01_a > "$OUT/layers-del.log"
grep -q "deleted 01_a; kept as the base of 02_b" "$OUT/layers-del.log" \
    || { echo "  deleting a base did not hold it"; cat "$OUT/layers-del.log"; exit 1; }
KL --list | grep -q "needed by 02_b" || { echo "  --list does not show the held base"; exit 1; }
restore_only 02_b; same_tree 02_b
KL --delete 02_b > /dev/null
[ -z "$(ls "$S/build/snapshots/.held" 2>/dev/null)" ] \
    || { echo "  the held base outlived its last dependant"; exit 1; }
echo "  a deleted base is held for its dependants and freed after them"

# A schema-3 snapshot is a full one with a legacy id, and the next snapshot
# layers on it once it has been restored.
KL --fresh > /dev/null 2>&1
mf="$S/build/snapshots/01_a/manifest.json"
python3 - "$mf" <<'EOF' 2>/dev/null || sed -i 's/"schema": 4/"schema": 3/; s/"paths"/"entries"/' "$mf"
import json, sys
m = json.load(open(sys.argv[1]))
m["schema"] = 3
m.pop("id")
m["entries"] = [{k: e[k] for k in ("path", "archive", "bytes_raw", "bytes_compressed", "files")}
                for e in m.pop("paths")]
json.dump(m, open(sys.argv[1], "w"))
EOF
restore_only 01_a; same_tree 01_a
KL --continue-from 02_b > /dev/null 2>&1
kind_of 02_b | grep -q "^layer:legacy-01_a-" \
    || { echo "  no layer on the schema-3 snapshot: $(kind_of 02_b)"; exit 1; }
restore_only 03_c; same_tree 03_c
echo "  a schema-3 snapshot restores, and the next snapshot layers on it"

rm -f "$S/build/snapshots/02_b/fs.tar.zst"
KL --list | grep -q "^unusable" || { echo "  a snapshot with a lost base is not flagged"; exit 1; }
KL --restore 03_c > "$OUT/layers-broken.log" 2>&1 && { echo "  restored a snapshot whose base is gone"; exit 1; }
grep -q "the snapshot of 03_c is unusable" "$OUT/layers-broken.log" \
    || { echo "  the broken chain is not explained"; cat "$OUT/layers-broken.log"; exit 1; }
echo "  a snapshot whose base archive is gone is unusable, never half-restored"

echo
echo "==> kdosbuild runs no step for a port installed and current"
# A chroot package phase over a real database: its ports are installed into
# build/fs by the real kpkg, so the recipe-hash sidecars are the ones a build
# writes. The chroot wrapper is a pass-through and kpkgdepends and kpkg are
# stubs, the second logging what it was asked. The order comes from a file so
# each run can change it.
HS="$OUT/hostskip"
rm -rf "$HS"
mkdir -p "$HS/script/phases/10_pk" "$HS/script/phases/20_after" \
    "$HS/script/chroot" "$HS/build/fs" "$HS/bin"
printf '#!/bin/bash\nexec "$@"\n' > "$HS/script/chroot/exec.sh"
printf 'export CHROOT=1\nexport PORT_REPO="/ports/core"\nexport KDOS_SNAPSHOT_PATHS="fs"\n' \
    > "$HS/script/phases/10_pk/phase.env"
printf 'cur\nnew\nfrc\ngone\n' > "$HS/script/phases/10_pk/packages.txt"
printf '#!/bin/bash\ntouch "$PWD/after"\n' > "$HS/script/phases/20_after/00_mark.sh"
printf '#!/bin/bash\ncat "%s/order"\n' "$HS" > "$HS/bin/kpkgdepends"
# A step for `new` runs whatever $HS/during holds first: an edit made while
# the phase is running. It is the one port the stub says it builds.
printf '#!/bin/bash\n[ "${@: -1}" = new ] && [ -f "%s/during" ] && . "%s/during"\n[ "${@: -1}" = new ] && echo "==> Building new..."\necho "$*" >> "%s/kpkg.log"\n' \
    "$HS" "$HS" "$HS" > "$HS/bin/kpkg"
chmod +x "$HS/script/chroot/exec.sh" "$HS/script/phases/20_after/00_mark.sh" \
    "$HS/bin/kpkgdepends" "$HS/bin/kpkg"
for p in cur new frc gone; do
    mkdir -p "$HS/ports/core/demo/$p"
    printf 'name        = %s\nversion     = 1.0\nrelease     = 1\ndescription = a synthetic port\n' \
        "$p" > "$HS/ports/core/demo/$p/kpkgbuild"
    printf 'install -Dm644 /dev/null "$PKG/usr/share/%s/who"\n' "$p" \
        > "$HS/ports/core/demo/$p/build.sh"
done
hs_install() {
    env PORT_REPO="$HS/ports/core" WORK_DIR="$HS/work" PACKAGE_DIR="$HS/pkgs" \
        SOURCE_DIR="$HS/src" PKGDB_DIR=/var/lib/kpkg/db KPKG_CONF=/nonexistent \
        SOURCE_DATE_EPOCH=1735689600 TZ=UTC \
        "$OUT/kpkg" install --root "$HS/build/fs" "$@" >"$HS/install.log" 2>&1 \
        || { echo "  the fixture ports did not install"; cat "$HS/install.log"; exit 1; }
}
hs_run() {  # <order> [kdosbuild args...]: the kpkg calls land in kpkg.log
    echo "$1" > "$HS/order"; shift
    rm -f "$HS/kpkg.log" "$HS/after"; : > "$HS/kpkg.log"
    ( cd "$HS" && PATH="$HS/bin:$PATH" "$KB" --script-dir script \
        --build-dir build --json --snapshot "$@" ) > "$HS/run.json" 2>&1
    grep -q '"event": "result", "status": "complete"' "$HS/run.json" \
        || { echo "  the build did not complete"; cat "$HS/run.json"; exit 1; }
    [ -e "$HS/after" ] || { echo "  the phase after it never ran"; exit 1; }
}
hs_install cur frc gone
[ -s "$HS/build/fs/var/lib/kpkg/db/.recipe/cur" ] \
    || { echo "  no recipe-hash sidecar in the fixture database"; exit 1; }
rm -rf "$HS/ports/core/demo/gone"

# cur is current: never handed to kpkg. frc is current too, but forced. new is
# not installed. gone is installed but has no port the host can find, which
# kp_installed_current alone reads as current: it keeps its step.
hs_run "cur new frc gone" --rebuild frc
grep -qx "install cur" "$HS/kpkg.log" \
    && { echo "  an installed and current port was passed to kpkg"; cat "$HS/kpkg.log"; exit 1; }
for want in "install new" "install -f frc" "install gone"; do
    grep -qx -- "$want" "$HS/kpkg.log" \
        || { echo "  no '$want' step"; cat "$HS/kpkg.log"; exit 1; }
done
grep -q '"installed": 1,' "$HS/run.json" \
    || { echo "  the result does not count the installed port"; cat "$HS/run.json"; exit 1; }
# Only a port whose step built it is timed: `new` said so, `frc` and `gone`
# printed no build line and `cur` never ran.
grep -q '"10_pk/new": [0-9.]*' "$HS/build/snapshots/timings.json" \
    || { echo "  a port that built was not timed"; cat "$HS/build/snapshots/timings.json"; exit 1; }
grep -q '"10_pk/\(cur\|frc\|gone\)"' "$HS/build/snapshots/timings.json" \
    && { echo "  a port that built nothing was timed"; cat "$HS/build/snapshots/timings.json"; exit 1; }

# Every port current: no step runs, and the phase still finishes — its
# snapshot is taken, and counts every port as done.
hs_install new
rm -rf "$HS/build/snapshots"
hs_run "cur new frc"
[ -s "$HS/kpkg.log" ] && { echo "  kpkg ran for a phase that was all current"; cat "$HS/kpkg.log"; exit 1; }
grep -q '"complete": true' "$HS/build/snapshots/10_pk/manifest.json" 2>/dev/null \
    || { echo "  a phase with every port current was not finished"; exit 1; }

# An edited recipe is no longer current, and its step comes back.
printf '# edited\n' >> "$HS/ports/core/demo/cur/build.sh"
hs_run "cur new frc"
[ "$(cat "$HS/kpkg.log")" = "install cur" ] \
    || { echo "  an edited recipe was not passed to kpkg alone"; cat "$HS/kpkg.log"; exit 1; }

# An order that ends on installed ports: a front end announces only a step
# that starts, so neither is reported running.
hs_install -f cur
hs_run "new cur frc" --rebuild new
grep -q '"status": "running", "step": "\(cur\|frc\)"' "$HS/run.json" \
    && { echo "  an installed port was announced as running"; cat "$HS/run.json"; exit 1; }
[ "$(cat "$HS/kpkg.log")" = "install -f new" ] \
    || { echo "  a trailing installed port was passed to kpkg"; cat "$HS/kpkg.log"; exit 1; }

# The host's answer is asked again when a port's turn comes: a recipe edited
# while an earlier step ran brings that port's step back in the same run.
printf 'printf "# edited during\\n" >> "%s/ports/core/demo/cur/build.sh"\n' "$HS" > "$HS/during"
rm -rf "$HS/build/snapshots"
hs_run "new cur frc" --rebuild new
rm -f "$HS/during"
[ "$(tr '\n' ' ' < "$HS/kpkg.log")" = "install -f new install cur " ] \
    || { echo "  a recipe edited mid-phase kept its installed mark"; cat "$HS/kpkg.log"; exit 1; }
echo "  installed and current ports are skipped on the host, re-asked on their turn; forced, edited and unknown ones run"

echo
echo "==> kdosbuild --port-jobs builds a phase by level"
# A host package phase: c depends on a and b, e on c, and d, which depends on
# nothing, is the order run. kpkgdepends is a stub printing the order; kpkg is
# a stub that logs each --build-only's start and end a second apart, each
# --commit, and each plain install, one line per event, in the order they
# happen. What the levels promise is read back off that log.
PJ="$OUT/portjobs"
rm -rf "$PJ"
mkdir -p "$PJ/script/phases/10_pk/packages.d" "$PJ/build" "$PJ/bin"
echo 'export KDOS_PHASE_TITLE="Levels"' > "$PJ/script/phases/10_pk/phase.env"
printf '# pinned\nd\n' > "$PJ/script/phases/10_pk/packages.d/00-order.txt"
printf 'c\ne\n' > "$PJ/script/phases/10_pk/packages.d/demo.txt"
for p in a b c d e; do
    case $p in c) dep="a b" ;; e) dep=c ;; *) dep= ;; esac
    mkdir -p "$PJ/ports/core/demo/$p"
    printf 'name        = %s\nversion     = 1.0\nrelease     = 1\ndescription = a synthetic port\ndepends     = %s\n' \
        "$p" "$dep" > "$PJ/ports/core/demo/$p/kpkgbuild"
done
printf '#!/bin/bash\necho d a b c e\n' > "$PJ/bin/kpkgdepends"
cat > "$PJ/bin/kpkg" <<EOF
#!/bin/bash
L="$PJ/kpkg.log"
[ "\$1" = help ] && { echo "  --build-only       (install) build only"; exit 0; }
shift
case \$1 in
--build-only)
    p=\${@: -1}
    js=none; [ -p "\${KDOS_JOBSERVER:-}" ] && js=fifo
    echo "start \$p jobs=\$KDOS_JOBS js=\$js cpus=\$(nproc)" >> "\$L"
    if [ "\$(cat "$PJ/fail" 2>/dev/null)" = "\$p" ]; then
        echo "fail \$p" >> "\$L"; exit 1
    fi
    sleep 1
    echo "end \$p" >> "\$L" ;;
--commit)
    shift
    echo "commit \$*" >> "\$L"
    for p; do
        [ "\$p" = a ] && [ -e "$PJ/take" ] && echo "==> Taking 1 path from b"
        echo "kpkg: committed \$p"
    done ;;
*)
    echo "install \$*" >> "\$L" ;;
esac
EOF
chmod +x "$PJ/bin/kpkgdepends" "$PJ/bin/kpkg"
pj_run() {  # <expected exit> [kdosbuild args...]
    local want=$1 rc=0; shift
    rm -rf "$PJ/build"; mkdir -p "$PJ/build"; : > "$PJ/kpkg.log"
    ( cd "$PJ" && PATH="$PJ/bin:$PATH" KDOS_JOBS=6 "$KB" --script-dir script \
        --build-dir build --json "$@" ) > "$PJ/run.json" 2>&1 || rc=$?
    [ "$rc" = "$want" ] || { echo "  exit $rc, not $want"; cat "$PJ/run.json" "$PJ/kpkg.log"; exit 1; }
}
pj_line() { grep -n -m1 -- "$1" "$PJ/kpkg.log" | cut -d: -f1; }
pj_steps() {  # the step events, in order, without their timings
    python3 -c '
import json, sys
for l in open(sys.argv[1]):
    e = json.loads(l)
    if e.get("event") == "step":
        print(e["status"], e["phase"], e["step"])' "$1"
}

pj_run 0 --port-jobs 3
for p in a b; do
    [ "$(pj_line "start $p")" -lt "$(pj_line 'end a')" ] && [ "$(pj_line "start $p")" -lt "$(pj_line 'end b')" ] \
        || { echo "  a and b did not build side by side"; cat "$PJ/kpkg.log"; exit 1; }
    [ "$(pj_line 'commit d')" -lt "$(pj_line "start $p")" ] \
        || { echo "  $p started before the order run was committed"; cat "$PJ/kpkg.log"; exit 1; }
done
[ "$(pj_line 'end d')" -lt "$(pj_line 'start a')" ] \
    || { echo "  the order run did not finish first"; cat "$PJ/kpkg.log"; exit 1; }
grep -qx 'commit a b' "$PJ/kpkg.log" \
    || { echo "  a and b were not committed together in serial order"; cat "$PJ/kpkg.log"; exit 1; }
[ "$(pj_line 'commit a b')" -lt "$(pj_line 'start c')" ] \
    && [ "$(pj_line 'commit c')" -lt "$(pj_line 'start e')" ] \
    || { echo "  a port started before its dependencies were committed"; cat "$PJ/kpkg.log"; exit 1; }
grep -q '^install' "$PJ/kpkg.log" && { echo "  a level ran a plain install"; cat "$PJ/kpkg.log"; exit 1; }
# KDOS_JOBS=6 over three slots is 2 each; every port is handed the jobserver
# FIFO, which is gone once the build ends, and none is pinned to fewer CPUs.
grep '^start' "$PJ/kpkg.log" | grep -qv 'jobs=2 ' \
    && { echo "  a port was not given KDOS_JOBS=2"; cat "$PJ/kpkg.log"; exit 1; }
grep '^start' "$PJ/kpkg.log" | grep -qv ' js=fifo ' \
    && { echo "  a port was not given the jobserver"; cat "$PJ/kpkg.log"; exit 1; }
grep '^start' "$PJ/kpkg.log" | grep -qvx ".* cpus=$(nproc)" \
    && { echo "  a port was pinned to fewer CPUs"; cat "$PJ/kpkg.log"; exit 1; }
[ -e "$PJ/build/jobserver" ] && { echo "  the jobserver outlived the build"; exit 1; }
if command -v python3 >/dev/null 2>&1; then
    pj_steps "$PJ/run.json" | sort | uniq -d | grep -q . \
        && { echo "  a step event was reported twice"; cat "$PJ/run.json"; exit 1; }
    [ "$(pj_steps "$PJ/run.json" | grep -c '^running')" = 9 ] \
        && [ "$(pj_steps "$PJ/run.json" | grep -c '^ok')" = 9 ] \
        || { echo "  not every step opened and closed once"; cat "$PJ/run.json"; exit 1; }
fi
echo "  a level builds side by side, commits in serial order, and the next waits for it"

# --port-jobs 1 is the serial runner: the same kpkg calls and the same step
# events as a run without the flag.
pj_run 0
cp "$PJ/kpkg.log" "$PJ/serial.log"
[ "$(tr '\n' ' ' < "$PJ/serial.log")" = "install d install a install b install c install e " ] \
    || { echo "  the serial run changed"; cat "$PJ/serial.log"; exit 1; }
if command -v python3 >/dev/null 2>&1; then
    pj_steps "$PJ/run.json" > "$PJ/serial.steps"
    pj_run 0 --port-jobs 1
    pj_steps "$PJ/run.json" | cmp -s - "$PJ/serial.steps" \
        || { echo "  --port-jobs 1 reported other steps"; cat "$PJ/run.json"; exit 1; }
else
    pj_run 0 --port-jobs 1
fi
cmp -s "$PJ/kpkg.log" "$PJ/serial.log" \
    || { echo "  --port-jobs 1 ran other commands"; cat "$PJ/kpkg.log"; exit 1; }
echo "  --port-jobs 1 runs exactly what the serial runner runs"

# A path taken from a port the serial order installs later is named.
touch "$PJ/take"
pj_run 0 --port-jobs 3
rm -f "$PJ/take"
grep -q 'a took paths from b; the serial order had b last' "$PJ/run.json" \
    || { echo "  an ownership inversion went unreported"; cat "$PJ/run.json"; exit 1; }

# b fails at once: a, already running beside it, finishes; the level is not
# committed; nothing past it starts; the build fails.
echo b > "$PJ/fail"
pj_run 1 --port-jobs 3
rm -f "$PJ/fail"
grep -qx 'end a' "$PJ/kpkg.log" \
    || { echo "  a running sibling was stopped by the failure"; cat "$PJ/kpkg.log"; exit 1; }
grep -q 'commit a\|commit b\|start c' "$PJ/kpkg.log" \
    && { echo "  a failed level was committed or passed"; cat "$PJ/kpkg.log"; exit 1; }
grep -q '"event": "result", "status": "failed".*"failed_step": "b"' "$PJ/run.json" \
    || { echo "  the failure is not b's"; cat "$PJ/run.json"; exit 1; }
"$KB" --port-jobs 9 --list >/dev/null 2>&1 && { echo "  --port-jobs 9 was accepted"; exit 1; }
echo "  a failure drains its level, commits none of it, and stops the build"

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
# reuse does not repartition and makes no second slot, and the accent and the
# desk are written whatever they are: the skip rules, read back off the plan
# rather than off the source. Matched by NAME rather than by step number: the
# number moves whenever a step is added, and a test that pinned it would fail
# for the one reason that is not a regression.
grep -qE "^ +[0-9]+ +Partition +skipped" "$OUT/plan.txt" \
    || { echo "  reuse still plans to partition"; cat "$OUT/plan.txt"; exit 1; }
grep -qE "^ +[0-9]+ +Slot B +skipped" "$OUT/plan.txt" \
    || { echo "  reuse plans a second slot"; cat "$OUT/plan.txt"; exit 1; }
grep -qE "^ +[0-9]+ +Theme +pending" "$OUT/plan.txt" \
    || { echo "  the accent and the desk are not written"; exit 1; }
# No medium, so there is nothing to carry and the step says so rather than
# running and copying nothing.
grep -qE "^ +[0-9]+ +Packs +skipped" "$OUT/plan.txt" \
    || { echo "  the packs step ran with no medium"; exit 1; }
grep -q '"title": "Partition".*"state": "skipped"' "$OUT/plan.json" \
    || { echo "  text and json disagree about Partition"; exit 1; }
# The erase plan's layout, by the names the install gives it: the ESP first,
# the BIOS boot partition Limine's legacy stage goes into second, swap when it
# is a partition, the root last. A disk the probe cannot see has no room for a
# second slot, so the root takes the whole group and the swap stays a
# partition; the A/B layout itself is the kinstall-core block's.
"$KI" --save "$OUT/wipe.conf" >/dev/null
sed -i 's|^disk .*|disk = /dev/kinstall-fixture0|; s/^plan.*/plan = wipe/; s/^swap .*/swap = partition/; s/^lvm .*/lvm = 1/' \
    "$OUT/wipe.conf"
"$KI" --config "$OUT/wipe.conf" --dump plan > "$OUT/wipe.txt" 2>&1
"$KI" --config "$OUT/wipe.conf" --dump plan --json > "$OUT/wipe.json" 2>&1
grep -qx 'esp           /dev/kinstall-fixture0p1 (format)' "$OUT/wipe.txt" \
    && grep -qx 'bios boot     /dev/kinstall-fixture0p2, 1 MiB, made by the layout' "$OUT/wipe.txt" \
    && grep -qx 'root          lvm kdos/root_a on /dev/kinstall-fixture0p4, all of the group (-l 100%FREE); no room for a second slot' "$OUT/wipe.txt" \
    || { echo "  the erase plan's layout is not ESP, BIOS boot, swap, root"; cat "$OUT/wipe.txt"; exit 1; }
python3 -c 'import json,sys; d=json.load(open(sys.argv[1]));
assert d["bios_boot"] == "/dev/kinstall-fixture0p2", d["bios_boot"]
assert d["root"] == "/dev/kinstall-fixture0p4", d["root"]
assert not d["ab"] and not d["lvm"]["ab"] and d["lvm"]["home"] is None, d["lvm"]' "$OUT/wipe.json" \
    || { echo "  text and json disagree about the erase plan's layout"; exit 1; }
# A reused disk the probe cannot see has no BIOS boot partition to find, and
# the plan says what that costs.
grep -qx 'bios boot     none: the disk starts on UEFI only' "$OUT/plan.txt" \
    || { echo "  a reuse plan with no BIOS boot partition claims a BIOS path"; cat "$OUT/plan.txt"; exit 1; }
echo "  the erase plan lays out a BIOS boot partition"
# A dump ends up in bug reports and CI logs, and the answer file carries the
# passwords in the clear because crypt() is about to be called on them. The
# sentinel is what proves neither rendering repeats one.
printf 'password = hunter2-sentinel\nroot_password = hunter2-sentinel\n' \
    >> "$OUT/answers.conf"
# The LUKS passphrase is the third secret cfg carries, so it goes
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

echo
echo "==> kinstall-core: the event stream, the Plasma machine's files, A/B"
# THE BACKEND LINKS WITHOUT THE TERMINAL TOOLKIT. The graphical installer runs
# this same binary headless, and kinstall-core is what it drives: linked here
# with libkbase and libkcolor alone, through a harness that calls the writers
# and the layout decision directly, a libktui call anywhere in the core is an
# undefined symbol.
$CC $STD $WARN -Isrc/libs/libkbase -Isrc/libs/libkcolor \
    -Isrc/system/kinstall-core -Isrc/system/kdos-appbox -o "$OUT/kicore" \
    testing/fixtures/kinstall/harness.c src/system/kinstall-core/*.c \
    src/system/kdos-appbox/catalogue.c src/libs/libkbase/*.c \
    src/libs/libkcolor/*.c -lcrypt \
    || { echo "  kinstall-core does not link without libktui"; exit 1; }
grep -n '#include "ktui.h"' src/system/kinstall-core/*.[ch] \
    && { echo "  kinstall-core includes ktui.h"; exit 1; }
KC="$OUT/kicore"
# THE STREAM: one JSON object per line, the plan first and the exit last. A
# dry run as anybody but root stops in Prepare, which is a stream with a
# failure in it, and answers the rules refuse are refused before the install
# starts, with status 2 and nothing forked. Neither writes anything. The disk
# is named relative to $OUT and the runs start there: kinstall holds a disk
# path of 63 bytes and refuses a longer one, which a deep TMPDIR makes.
truncate -s 1M "$OUT/kidisk"
printf 'disk = kidisk\npassword = p\n' > "$OUT/ev.conf"
(cd "$OUT" && "$KI" --events json --config ev.conf) >/dev/null 2>&1 \
    && { echo "  --events ran without --unattended"; exit 1; }
# The status is taken with `||`: a failed run is an expected outcome here, and
# a bare non-zero command would end the whole selftest under `set -e`.
evrc=0
(cd "$OUT" && KDOS_CATALOGUE="$OUT/cat-none" "$KI" --dry-run --unattended \
    --events json --config - < ev.conf > ev.json 2>/dev/null) || evrc=$?
python3 - "$OUT/ev.json" "$evrc" "$(id -u)" <<'PYEOF' || { echo "  the event stream is not the documented shape"; cat "$OUT/ev.json"; exit 1; }
import json, sys
lines = [json.loads(l) for l in open(sys.argv[1])]
rc, uid = int(sys.argv[2]), sys.argv[3]
assert lines[0]["t"] == "plan" and lines[-1]["t"] == "exit", lines
assert lines[-1]["status"] == rc, (lines[-1], rc)
titles = [s["title"] for s in lines[0]["steps"]]
assert titles[0] == "Prepare" and titles[-1] == "Finish", titles
assert "Slot B" in titles and titles.index("Slot B") < titles.index("Bootloader")
kinds = {l["t"] for l in lines}
assert kinds <= {"plan", "S", "K", "P", "N", "L", "W", "F", "D", "exit"}, kinds
for l in lines:
    if l["t"] == "P":
        assert l["frac"] == -1 or 0 <= l["frac"] <= 1, l
if uid != "0":
    f = [l for l in lines if l["t"] == "F"]
    assert rc == 1 and f and f[0]["step"] == 0 and "root" in f[0]["text"], lines
PYEOF
printf 'disk = kidisk\npassword = p\nautologin = yes\n' > "$OUT/ev2.conf"
ev2rc=0
(cd "$OUT" && "$KI" --dry-run --unattended --events json --config ev2.conf \
    > ev2.json 2>/dev/null) || ev2rc=$?
test "$ev2rc" = 2 || { echo "  refused answers did not exit 2"; exit 1; }
python3 -c 'import json,sys
L = [json.loads(l) for l in open(sys.argv[1])]
assert [l["t"] for l in L] == ["plan", "F", "exit"], L
assert L[1]["step"] == -1 and "encrypted root" in L[1]["text"], L[1]
assert L[2]["status"] == 2, L[2]' "$OUT/ev2.json" \
    || { echo "  autologin on a plain root was not refused before the install"; exit 1; }
echo "  --events json: plan, events, exit; refused answers stop at status 2"

# THE ANSWERS ARRIVE ON STANDARD INPUT, which is how the graphical installer
# hands pkexec's kinstall the passwords without a file holding them.
printf 'username = ann\noem = 0\n' | "$KI" --config - --dump plan 2>&1 \
    | grep -qx 'user          ann (wheel)' \
    || { echo "  --config - did not read standard input"; exit 1; }
# The accents and desks a front end may offer are the ones the backend takes.
"$KI" --dump look --json | python3 -c 'import json,sys
d = json.load(sys.stdin)
names = [a["name"] for a in d["accents"]]
assert len(names) == 8 and "bone" in names, names
assert [a["name"] for a in d["accents"] if a["default"]] == ["bone"], d
assert all(a["lookandfeel"] == "org.kdos.%s.desktop" % a["name"] for a in d["accents"])
assert d["desks"] == ["kdos", "dock", "tiling"], d["desks"]' \
    || { echo "  --dump look does not list the eight accents and three desks"; exit 1; }
"$KC" check "$OUT/ev.conf" | grep -qx ok || { echo "  plain answers were refused"; exit 1; }
printf 'disk = /dev/x\noem = 1\nautologin = yes\nluks = 1\nluks_passphrase = q\n' > "$OUT/kc1.conf"
"$KC" check "$OUT/kc1.conf" | grep -q 'OEM' || { echo "  OEM with autologin was accepted"; exit 1; }
printf 'disk = /dev/x\nusername = sddm\npassword = p\n' > "$OUT/kc2.conf"
"$KC" check "$OUT/kc2.conf" | grep -q 'cannot be a user name' \
    || { echo "  a system account's name was accepted"; exit 1; }
printf 'disk = /dev/x\npassword = p\ndesk = cubes\n' > "$OUT/kc3.conf"
"$KC" check "$OUT/kc3.conf" | grep -q 'not a desk' || { echo "  an unknown desk was accepted"; exit 1; }
# An OEM install makes no account, and still must not unlock root with an
# empty password; the full name is passwd's fifth field; the zone is a path
# and the TZ string is shell.
printf 'disk = /dev/x\noem = 1\nroot_locked = 0\n' > "$OUT/kc4.conf"
"$KC" check "$OUT/kc4.conf" | grep -q 'root password' \
    || { echo "  an OEM install unlocked root with no password"; exit 1; }
printf 'disk = /dev/x\npassword = p\nfullname = Ann:0:0\n' > "$OUT/kc5.conf"
"$KC" check "$OUT/kc5.conf" | grep -q 'full name' \
    || { echo "  a full name with ':' was accepted"; exit 1; }
printf 'disk = /dev/x\npassword = p\ntimezone_label = ../../etc/shadow\n' > "$OUT/kc6.conf"
"$KC" check "$OUT/kc6.conf" | grep -q 'not a time zone' \
    || { echo "  a zone outside zoneinfo was accepted"; exit 1; }
printf "disk = /dev/x\npassword = p\ntimezone = UTC';id\n" > "$OUT/kc7.conf"
"$KC" check "$OUT/kc7.conf" | grep -q 'not a time zone' \
    || { echo "  a TZ string with a quote was accepted"; exit 1; }
printf 'disk = /dev/x\npassword = p\ntimezone_label = America/Argentina/Buenos_Aires\n' > "$OUT/kc8.conf"
"$KC" check "$OUT/kc8.conf" | grep -qx ok || { echo "  a real zone was refused"; exit 1; }
echo "  answers on stdin, the look the backend takes, the rules every front end shares"

# THE A/B LAYOUT. Two slots, the swap and a home of their own fit, or slot A
# takes the whole disk and the swap stays where the person asked for it. The
# slot is the system and half again, never under 8 GiB, a whole GiB; the box
# store lives in the home, so it is not counted into a slot.
"$KC" ab 256 12000000 2000000 4096 | grep -qx 'ab=1 slot=15360 swap=4096 part_swap=0 part_root=3' \
    || { echo "  a 256 GiB disk is not laid out for A/B"; "$KC" ab 256 12000000 2000000 4096; exit 1; }
"$KC" ab 40 12000000 0 4096 | grep -qx 'ab=0 slot=18432 swap=4096 part_swap=3 part_root=4' \
    || { echo "  a disk too small for two slots was laid out for them"; exit 1; }
"$KC" ab 64 1000000 0 0 | grep -qx 'ab=1 slot=8192 swap=0 part_swap=0 part_root=3' \
    || { echo "  a small payload's slot is not the 8 GiB floor"; exit 1; }
"$KC" ab 256 12000000 0 4096 0 | grep -q '^ab=0 ' \
    || { echo "  A/B was laid out without LVM"; exit 1; }
echo "  A/B: two slots, the swap and /home where they fit, one root where not"

# THE WRITERS, into a copy of the target's files. Each edits the image's file
# in place: the key it owns replaced, every other line kept.
KR="$OUT/kiroot"
rm -rf "$KR" && cp -a testing/fixtures/kinstall/root "$KR"
printf 'username = ann\npassword = p\nluks = 1\nluks_passphrase = q\nautologin = yes\ntheme = amber\ndesk = tiling\nxkb_layout = de\nxkb_variant = nodeadkeys\n' \
    > "$OUT/kw1.conf"
"$KC" write "$OUT/kw1.conf" "$KR" > "$OUT/kw1.log" \
    || { echo "  the writers failed"; cat "$OUT/kw1.log"; exit 1; }
test "$(cat "$KR/etc/sddm.conf.d/kde_settings.conf")" = "$(printf '[Autologin]\nRelogin=false\nSession=plasma\nUser=ann')" \
    || { echo "  kde_settings.conf is not this account's autologin"; cat "$KR/etc/sddm.conf.d/kde_settings.conf"; exit 1; }
test "$(cat "$KR/etc/kdos/autologin-xdg/kscreenlockerrc")" = "$(printf '[Daemon]\nLockOnStart=true')" \
    || { echo "  the autologin session does not start locked"; exit 1; }
test -e "$KR/etc/plasma-setup-done" || { echo "  an install with an account left plasma-setup armed"; exit 1; }
grep -qx 'LookAndFeelPackage=org.kdos.amber.desktop' "$KR/etc/xdg/kdeglobals" \
    && test "$(grep -c '^LookAndFeelPackage=' "$KR/etc/xdg/kdeglobals")" = 1 \
    && grep -q 'The installer rewrites it' "$KR/etc/xdg/kdeglobals" \
    || { echo "  kdeglobals lost its explanation or its one global theme"; cat "$KR/etc/xdg/kdeglobals"; exit 1; }
grep -qx 'Layout=tiling' "$KR/etc/xdg/kdosrc" || { echo "  kdosrc does not name the desk"; exit 1; }
grep -qx 'krohnkiteEnabled=true' "$KR/etc/xdg/kwinrc" \
    || { echo "  the tiling desk does not switch Krohnkite on for the first login"; exit 1; }
test "$(cat "$KR/etc/kdos/accent")" = amber || { echo "  /etc/kdos/accent is not the accent"; exit 1; }
test "$(wc -l < "$KR/etc/vtrgb")" = 3 || { echo "  /etc/vtrgb is not three palette rows"; exit 1; }
test "$(cat "$KR/etc/keymap")" = de-latin1 || { echo "  the console keymap is not the one nearest de"; exit 1; }
grep -qx 'XKBLAYOUT=de' "$KR/etc/kdos/xkb.conf" && grep -qx 'XKBVARIANT=nodeadkeys' "$KR/etc/kdos/xkb.conf" \
    && grep -q 'kdos-settingsd serves these four values' "$KR/etc/kdos/xkb.conf" \
    || { echo "  xkb.conf lost its explanation or its layout"; exit 1; }
test "$(cat "$KR/etc/xdg/kxkbrc")" = "$(printf '[Layout]\nLayoutList=de\nVariantList=nodeadkeys\nUse=true')" \
    || { echo "  kxkbrc is not the layout"; exit 1; }
cmp -s "$KR/var/lib/sddm/.config/plasmarc" testing/fixtures/kinstall/root/usr/share/kdos/greeter/amber/plasmarc \
    && cmp -s "$KR/var/lib/sddm/kdos-background.png" testing/fixtures/kinstall/root/usr/share/kdos/greeter/amber/background.png \
    || { echo "  the login screen did not get the accent's set"; exit 1; }
# The lock left out is "no password after the disk passphrase"; autologin off
# leaves no group at all; OEM leaves plasma-setup armed and no autologin.
printf 'autologin_lock = 0\n' >> "$OUT/kw1.conf"
"$KC" write "$OUT/kw1.conf" "$KR" >/dev/null || exit 1
test ! -e "$KR/etc/kdos/autologin-xdg" || { echo "  the lock outlived its choice"; exit 1; }
printf 'username = ann\npassword = p\n' > "$OUT/kw2.conf"
rm -rf "$KR" && cp -a testing/fixtures/kinstall/root "$KR"
"$KC" write "$OUT/kw2.conf" "$KR" >/dev/null || exit 1
grep -q 'Autologin' "$KR/etc/sddm.conf.d/kde_settings.conf" \
    && { echo "  the live medium's autologin reached an installed machine"; exit 1; }
cmp -s "$KR/etc/kdos/xkb.conf" testing/fixtures/kinstall/root/etc/kdos/xkb.conf \
    && test ! -e "$KR/etc/xdg/kxkbrc" && test "$(cat "$KR/etc/keymap")" = us \
    || { echo "  no layout asked, and the keyboard files changed anyway"; exit 1; }
grep -qx 'LookAndFeelPackage=org.kdos.bone.desktop' "$KR/etc/xdg/kdeglobals" \
    && grep -qx 'Layout=kdos' "$KR/etc/xdg/kdosrc" \
    || { echo "  the defaults are not bone and the KDOS desk"; exit 1; }
test ! -e "$KR/etc/xdg/kwinrc" || { echo "  a desk that does not tile wrote /etc/xdg/kwinrc"; exit 1; }
printf 'oem = 1\n' > "$OUT/kw3.conf"
"$KC" write "$OUT/kw3.conf" "$KR" >/dev/null || exit 1
test ! -e "$KR/etc/plasma-setup-done" || { echo "  an OEM install did not arm plasma-setup"; exit 1; }
echo "  login, look and keyboard files: in place, per answer, OEM armed"

# ── the Applications page reads the CATALOGUE ───────────────────────────────
# kinstall links libkbase, libktui and libkcolor and nothing else, which is what
# lets it live in the bootstrap phase — so catalogue.c is compiled IN rather
# than shelled out to, and this asserts the four answers that decide what an
# install carries: `essential` is preselected, an answer file's choice wins, an
# UNKNOWN id falls back rather than failing after the point of no return, and
# the page says which ROUTE the applications will take.
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
# `none` is no group at all — the one way an answer file says it, because an
# empty line reads back as the default.
printf 'apps = none\n' > "$OUT/a3.conf"
kiapp "$OUT/a3.conf" | grep -qE '^apps route +nothing chosen$' \
    || { echo "  apps = none still chose a group"; kiapp "$OUT/a3.conf"; exit 1; }
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
echo "==> polkit answers what the security model says, with the shipped rules"
#
# A .rules file is JavaScript that duktape runs inside polkitd. Nothing
# compiles it, so a syntax error or a wrong action id is a setting that
# quietly does not change, or a password nobody should have been asked for.
#
# polkitd DECIDES WITH THE RULES AND THE DEFAULTS TOGETHER: a rule's answer
# where one returns, the action's allow_active, allow_inactive or allow_any
# otherwise. So the shipped file is evaluated between a stub of polkit's own
# object with the actions' defaults (testing/fixtures/polkit/defaults.js) and
# the table of answers (assert.js) — the file itself, not a copy, which would
# agree with the rules only until somebody edited them. The defaults are held
# against every .policy file under src/ and, where build/fs has been built,
# against the tree's own, so the model cannot drift from what ships.
#
# duk is polkitd's engine. node runs the same table where duk is missing and
# proves the answers, not that duktape's ES5 parser accepts the file.
PKJS=""
command -v duk >/dev/null 2>&1 && PKJS=duk
[ -z "$PKJS" ] && command -v node >/dev/null 2>&1 && PKJS=node
if [ -n "$PKJS" ]; then
    find src -name '*.policy' -exec awk -v table=_shipped \
        -f testing/fixtures/polkit/pol2js.awk {} + > "$OUT/pk-shipped.js"
    : > "$OUT/pk-built.js"
    if [ -d build/fs/usr/share/polkit-1/actions ]; then
        find build/fs/usr/share/polkit-1/actions -name '*.policy' \
            -exec awk -v table=_built \
            -f testing/fixtures/polkit/pol2js.awk {} + > "$OUT/pk-built.js"
    fi
    : > "$OUT/pk-print.js"
    [ "$PKJS" = node ] && \
        echo 'var print = function (s) { console.log(s); };' > "$OUT/pk-print.js"
    cat "$OUT/pk-print.js" testing/fixtures/polkit/stub.js \
        testing/fixtures/polkit/defaults.js \
        "$OUT/pk-shipped.js" "$OUT/pk-built.js" \
        fs/etc/polkit-1/rules.d/50-kdos.rules \
        testing/fixtures/polkit/assert.js > "$OUT/rulescheck.js"
    # The engine's own status as well as the assertions': a rules file that
    # does not parse prints a SyntaxError, which is not a FAIL line.
    _pkrc=0
    "$PKJS" "$OUT/rulescheck.js" > "$OUT/rulescheck.txt" 2>&1 || _pkrc=$?
    cat "$OUT/rulescheck.txt"
    if [ "$_pkrc" != 0 ] || grep -q "FAIL" "$OUT/rulescheck.txt" ||
       ! grep -q "^  ok " "$OUT/rulescheck.txt"; then
        exit 1
    fi
    [ "$PKJS" = node ] && \
        echo "  (evaluated with node: no duk on this host to parse it as polkitd does)"
else
    echo "  the polkit rules are skipped (no duk or node on this host)"
fi

echo
echo "==> kdos-powerd lets wheel change the firewall, the accent and the splash, and asks polkit for anyone else"
# The gate is SO_PEERCRED on the connection, which cannot be tested without two
# uids. `--explain` reads exactly the same two files the gate does and is the
# diagnostic a user gets for a refused toggle, so it is what is asserted here.
"$OUT/kdos-powerd" --explain root | grep -q "permitted every verb" \
    || { echo "  root is not permitted"; exit 1; }
"$OUT/kdos-powerd" --explain no-such-user-here 2>/dev/null | grep -q "refused" \
    || { echo "  an unknown user is not refused"; exit 1; }
"$OUT/kdos-powerd" --explain no-such-user-here >/dev/null 2>&1 \
    && { echo "  an unknown user exited 0"; exit 1; }
# Outside wheel the group rule says no and polkit is named as the other
# answer. `nobody` is in no group anywhere.
"$OUT/kdos-powerd" --explain nobody 2>/dev/null | grep -q "only where polkit grants" \
    || { echo "  a user outside wheel is not told about polkit"; exit 1; }
"$OUT/kdos-powerd" --explain nobody >/dev/null 2>&1 \
    && { echo "  a user outside wheel exited 0"; exit 1; }
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
# ── SUSPEND, POWER-OFF AND REBOOT ARE loginctl's ─────────────────────────
#
# The client execs `loginctl <verb>` as the caller and never reaches the
# daemon, so login1 and polkit see the person who asked. A stand-in loginctl
# first on PATH records the argv it was given.
mkdir -p "$OUT/lbin"
cat > "$OUT/lbin/loginctl" <<LCTL
#!/bin/sh
printf '%s\n' "\$*" > "$OUT/loginctl.args"
LCTL
chmod +x "$OUT/lbin/loginctl"
for _v in suspend poweroff reboot; do
    rm -f "$OUT/loginctl.args"
    KDOS_POWERD_SOCKET="$OUT/nothing.sock" PATH="$OUT/lbin:$PATH" \
        "$OUT/kdos-power" "$_v" \
        || { echo "  kdos-power $_v did not run loginctl"; exit 1; }
    [ "$(cat "$OUT/loginctl.args" 2>/dev/null)" = "$_v" ] \
        || { echo "  kdos-power $_v ran loginctl with the wrong argv"; exit 1; }
done
_rc=0
"$OUT/kdos-power" suspend now 2>/dev/null || _rc=$?
[ "$_rc" -eq 2 ] || { echo "  suspend with an argument is not a usage error"; exit 1; }
# ── THE SOCKET'S TIERS, FROM OUTSIDE WHEEL ───────────────────────────────
#
# A daemon on a test socket, and this host's own user as the client: where
# that user is in neither root nor wheel, ping, the list and the knocks are
# answered and the three changes are refused. Built without the bus, or with
# no polkit to ask, refusal is the only answer the socket may give them.
if [ "$(id -u)" -ne 0 ] && ! id -nG | tr ' ' '\n' | grep -qx wheel; then
    TRW="$OUT/tierwork"
    rm -rf "$TRW"
    mkdir -p "$TRW/etc/nftables.d"
    DBUS_SYSTEM_BUS_ADDRESS="unix:path=$TRW/no-bus" \
    KDOS_POWERD_SOCKET="$TRW/p.sock" KDOS_POWERD_ETC="$TRW/etc" \
        "$OUT/kdos-powerd" 2>"$TRW/err" &
    _tpd=$!
    _i=0
    while [ ! -S "$TRW/p.sock" ] && [ "$_i" -lt 50 ]; do sleep 0.1; _i=$((_i + 1)); done
    tr_fail() { echo "  $1"; kill "$_tpd" 2>/dev/null; wait "$_tpd" 2>/dev/null; exit 1; }
    KDOS_POWERD_SOCKET="$TRW/p.sock" "$OUT/kdos-power" ping \
        || tr_fail "ping was refused outside wheel"
    KDOS_POWERD_SOCKET="$TRW/p.sock" "$OUT/kdos-power" firewall list \
        | grep -q "^ssh	off	" || tr_fail "the list was not answered outside wheel"
    KDOS_POWERD_SOCKET="$TRW/p.sock" "$OUT/kdos-power" firewall ssh on \
        > "$TRW/r" 2>&1 && tr_fail "a firewall toggle was obeyed outside wheel"
    grep -q "not permitted" "$TRW/r" || tr_fail "the toggle's refusal does not say so"
    KDOS_POWERD_SOCKET="$TRW/p.sock" "$OUT/kdos-power" firewall knocks \
        | grep -qx ok || tr_fail "the knocks were not answered outside wheel"
    KDOS_POWERD_SOCKET="$TRW/p.sock" "$OUT/kdos-power" accent amber \
        2>"$TRW/r" && tr_fail "the accent was set outside wheel with no polkit"
    [ -e "$TRW/etc/kdos/accent" ] && tr_fail "a refused accent was written"
    KDOS_POWERD_SOCKET="$TRW/p.sock" "$OUT/kdos-power" splash phosphor \
        2>"$TRW/r" && tr_fail "the splash was set outside wheel with no polkit"
    grep -q "not permitted" "$TRW/r" || tr_fail "the splash's refusal does not say so"
    grep -q "refused uid $(id -u) firewall" "$TRW/err" \
        || tr_fail "the refusal was not logged with the uid and the verb"
    kill "$_tpd" 2>/dev/null
    wait "$_tpd" 2>/dev/null || true
else
    echo "  the outside-wheel tiers are skipped (running as root or in wheel)"
fi
# ── THE FIREWALL VERB, THROUGH THE CLIENT ────────────────────────────────
#
# `kdos-power firewall <name> on|off` is the argv the `kdos` command spawns,
# and `firewall list` is the table it prints, read from stdout. Driven over a
# real test socket, because `--firewall` goes around the client and would
# pass with a client that cannot join two words or read past 64 bytes.
#
# A toggle is root's or wheel's on the socket, so both ends run as root in a
# user namespace; without one, and not root, the block is skipped.
FWW="$OUT/fwwork"
rm -rf "$FWW"
mkdir -p "$FWW/etc/nftables.d"
cat > "$FWW/run.sh" <<'FWEOF'
set -u
B="$1" W="$2"
export KDOS_POWERD_SOCKET="$W/p.sock" KDOS_POWERD_ETC="$W/etc"
"$B/kdos-powerd" 2>/dev/null &
d=$!
trap 'kill $d 2>/dev/null' EXIT
i=0
while [ ! -S "$W/p.sock" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
fail() { echo "  $1"; exit 1; }
"$B/kdos-power" firewall list > "$W/l1" || fail "firewall list failed"
[ "$(tail -n1 "$W/l1")" = ok ] || fail "the list does not end in ok"
[ "$(grep -c "	" "$W/l1")" -ge 21 ] || fail "the list lost rows past the first read"
grep -q "^mosh	off	" "$W/l1" || fail "mosh is not listed off"
# Desktop sharing is krfb's VNC and krdp's RDP, each a row of its own.
grep -qx "vnc	off	this desktop over VNC (krfb)" "$W/l1" || fail "vnc does not name krfb"
grep -qx "rdp	off	this desktop over RDP (krdp)" "$W/l1" || fail "rdp is not listed"
"$B/kdos-power" firewall rdp on > /dev/null || fail "firewall rdp on failed"
grep -q "tcp dport 3389 accept" "$W/etc/nftables.d/50-kdos-services.nft" \
    || fail "rdp on did not open 3389"
"$B/kdos-power" firewall rdp off > /dev/null || fail "firewall rdp off failed"
"$B/kdos-power" firewall mosh on > "$W/t1" || fail "firewall mosh on failed"
grep -q "^ok mosh on" "$W/t1" || fail "the toggle reply is not on stdout"
"$B/kdos-power" firewall list | grep -q "^mosh	on	" || fail "mosh did not read back on"
# `ssh` also enables sshd: on takes the service.disabled marker away, off
# puts it back, and no other name touches it.
mkdir -p "$W/etc/service.disabled"
: > "$W/etc/service.disabled/sshd"
"$B/kdos-power" firewall ssh on > /dev/null || fail "firewall ssh on failed"
[ -e "$W/etc/service.disabled/sshd" ] && fail "ssh on left sshd disabled"
"$B/kdos-power" firewall ssh off > /dev/null || fail "firewall ssh off failed"
[ -e "$W/etc/service.disabled/sshd" ] || fail "ssh off did not disable sshd"
"$B/kdos-power" firewall mosh off > /dev/null || fail "firewall mosh off failed"
[ -e "$W/etc/service.disabled/sshd" ] || fail "another name enabled sshd"
"$B/kdos-power" firewall no-such-name on > "$W/t2" && fail "an unknown name exited 0"
grep -q "^err " "$W/t2" || fail "the refusal is not on stdout"
"$B/kdos-power" firewall mosh 2>/dev/null
[ $? -eq 2 ] || fail "a toggle with no state is not a usage error"
"$B/kdos-power" firewall list extra 2>/dev/null
[ $? -eq 2 ] || fail "list with an argument is not a usage error"
# With no nft to ask, no counter is held: the knocks are an empty table.
[ "$("$B/kdos-power" firewall knocks)" = ok ] || fail "the knocks are not an empty table"
"$B/kdos-power" firewall knocks extra 2>/dev/null
[ $? -eq 2 ] || fail "knocks with an argument is not a usage error"
exit 0
FWEOF
if [ "$(id -u)" -eq 0 ]; then
    sh "$FWW/run.sh" "$OUT" "$FWW" || exit 1
elif unshare -r true 2>/dev/null; then
    unshare -r sh "$FWW/run.sh" "$OUT" "$FWW" || exit 1
else
    echo "  the firewall client check is skipped (no user namespace)"
fi
# ── THE RULES FILE, ITS KNOCK COUNTERS AND A REFUSED LOAD ────────────────
#
# A stand-in nft (KDOS_POWERD_NFT, honoured only under a fixture /etc) records
# what the daemon runs, refuses `--check` on demand and prints a recorded
# `nft list counters`, so the file, the load and the counters are asserted
# without root and without this machine's ruleset.
NKW="$OUT/knockwork"
rm -rf "$NKW"
mkdir -p "$NKW/etc/nftables.d"
cat > "$NKW/nft" <<NFTEOF
#!/bin/sh
printf '%s\n' "\$*" >> "$NKW/nft.args"
case "\$1" in
--check) exit \${NKW_CHECK_RC:-0} ;;
list) cat "$NKW/counters"; exit 0 ;;
esac
exit 0
NFTEOF
chmod +x "$NKW/nft"
cat > "$NKW/counters" <<'CNTEOF'
table inet filter {
	counter kdos_knock_ssh {
		packets 12 bytes 720
	}
	counter kdos_knock_kdeconnect {
		packets 3 bytes 300
	}
	counter kdos_knock_mosh {
		packets 99 bytes 9900
	}
	counter somebody_elses {
		packets 5 bytes 5
	}
}
CNTEOF
nk() { KDOS_POWERD_ETC="$NKW/etc" KDOS_POWERD_NFT="$NKW/nft" "$OUT/kdos-powerd" "$@"; }
nk_fail() { echo "  $1"; [ -n "${2:-}" ] && sed 's/^/      /' "$2"; exit 1; }
NKF="$NKW/etc/nftables.d/50-kdos-services.nft"
nk --firewall mosh on > /dev/null || nk_fail "firewall mosh on failed"
# Checked, then applied, both through the conf that includes the file.
[ "$(cat "$NKW/nft.args")" = "--check -f $NKW/etc/nftables.conf
-f $NKW/etc/nftables.conf" ] || nk_fail "the ruleset was not checked and then applied" "$NKW/nft.args"
# An open name is its rule; a closed one is its match counted, with no
# accept, and a counter of its own; an open one has no counter.
grep -qx "		udp dport 60000-61000 accept" "$NKF" || nk_fail "mosh on is not its rule" "$NKF"
grep -qx '		tcp dport 22 counter name "kdos_knock_ssh"' "$NKF" \
    || nk_fail "a closed name is not counted" "$NKF"
grep -qx '	counter kdos_knock_ssh {' "$NKF" || nk_fail "a closed name has no counter" "$NKF"
grep -q 'kdos_knock_mosh' "$NKF" && nk_fail "an open name has a counter" "$NKF"
grep -q 'counter name.*accept' "$NKF" && nk_fail "a counted line accepts" "$NKF"
[ "$(grep -c 'counter name "kdos_knock_' "$NKF")" = 24 ] \
    || nk_fail "not every other name of the 25 is counted" "$NKF"
# Read back as on and off from the file alone, counters and all.
nk --firewall list | grep -q "^mosh	on	" || nk_fail "mosh did not read back on"
nk --firewall list | grep -q "^ssh	off	" || nk_fail "a counted ssh read back as on"
# The knocks are the closed names the kernel counts: an open name's counter
# and a counter that is not a row's are not reported.
nk --firewall knocks > "$NKW/k"
[ "$(cat "$NKW/k")" = "ssh	12
kdeconnect	3
ok" ] || nk_fail "the knocks are not the closed names' counts" "$NKW/k"
# A ruleset nft refuses puts the previous file back, so the disk says what
# the kernel runs.
cp "$NKF" "$NKW/before"
NKW_CHECK_RC=1 nk --firewall rdp on 2> "$NKW/e" && nk_fail "a refused ruleset exited 0"
grep -q "would not load; nothing changed" "$NKW/e" || nk_fail "the refusal does not say so" "$NKW/e"
cmp -s "$NKW/before" "$NKF" || nk_fail "a refused ruleset was left on disk"
# THE SHIPPED FILE IS THE DAEMON'S OWN OUTPUT with every name off, so the
# counters exist from the first boot and a copy nobody compares cannot go
# stale.
rm -rf "$NKW/fresh"
mkdir -p "$NKW/fresh/nftables.d"
KDOS_POWERD_ETC="$NKW/fresh" "$OUT/kdos-powerd" --firewall ssh off > /dev/null
cmp -s "$NKW/fresh/nftables.d/50-kdos-services.nft" fs/etc/nftables.d/50-kdos-services.nft \
    || { diff -u fs/etc/nftables.d/50-kdos-services.nft \
             "$NKW/fresh/nftables.d/50-kdos-services.nft" | head -20
         nk_fail "fs/etc/nftables.d/50-kdos-services.nft is not what kdos-powerd writes with every name off"; }
echo "  the rules file: rules, counted matches, the knocks, a refused load put back"
# ── THE ACCENT, THE LOGIN SCREEN, THE SPLASH, AND WHAT THEY REFUSE ───────
#
# An accent is one of the palette's eight names and nothing else: there is no
# path in it to aim. Driven through `--accent` rather than over the socket,
# for the same reason `--explain` exists, against a fixture /etc that also
# keeps kdos-bootctl away from this machine's ESP and the login screen away
# from its sddm account.
ACW="$OUT/accentwork"
rm -rf "$ACW"
mkdir -p "$ACW/etc"
ac_fail=0
acwant() {  # <scheme> <expected substring> <what it proves>
    _got=$(KDOS_POWERD_ETC="$ACW/etc" "$OUT/kdos-powerd" --accent "$1" 2>&1 || true)
    case "$_got" in
    *"$2"*) echo "  ok    $3" ;;
    *) echo "  FAIL  $3"; echo "        sent: $1"; echo "        got:  $_got"
       ac_fail=1 ;;
    esac
}
acwant amber "ok amber (boot menu and login screen unchanged)" \
    "a scheme of the palette is taken, and a fixture /etc leaves the boot menu and the greeter"
[ "$(cat "$ACW/etc/kdos/accent" 2>/dev/null)" = amber ] \
    && echo "  ok    and /etc/kdos/accent names it" \
    || { echo "  FAIL  /etc/kdos/accent was not written"; ac_fail=1; }
acwant mauve "not an accent" "a name outside the palette is refused"
acwant ../../etc/shadow "not an accent" "and a path is no name at all"
[ "$(cat "$ACW/etc/kdos/accent" 2>/dev/null)" = amber ] \
    && echo "  ok    and a refusal left the accent as it was" \
    || { echo "  FAIL  a refusal changed /etc/kdos/accent"; ac_fail=1; }
# THE BOOT MENU AND THE LOGIN SCREEN, with stand-ins a fixture /etc may name:
# a kdos-bootctl that records its argv, a set of greeter files per accent and
# a home for the sddm account. paper's set is missing a file, which must
# leave the greeter as amber left it rather than half in paper.
mkdir -p "$ACW/sets/amber" "$ACW/sets/paper" "$ACW/sddm"
printf '[General]\nColorScheme=KDOSAmber\n' > "$ACW/sets/amber/kdeglobals"
printf '[Theme]\nname=kdos\n' > "$ACW/sets/amber/plasmarc"
printf '[Mouse]\ncursorTheme=breeze_cursors\n' > "$ACW/sets/amber/kcminputrc"
printf 'not really a png\000\001' > "$ACW/amber-dark.png"
ln -s "$ACW/amber-dark.png" "$ACW/sets/amber/background.png"
printf '[General]\nColorScheme=KDOSPaper\n' > "$ACW/sets/paper/kdeglobals"
cat > "$ACW/bootctl" <<BCEOF
#!/bin/sh
printf '%s\n' "\$*" >> "$ACW/bootctl.args"
exit \${ACW_BOOTCTL_RC:-0}
BCEOF
chmod +x "$ACW/bootctl"
acfull() {  # <verb> <word> <expected reply> <what it proves>
    _got=$(KDOS_POWERD_ETC="$ACW/etc" KDOS_POWERD_BOOTCTL="$ACW/bootctl" \
           KDOS_POWERD_GREETER="$ACW/sets" KDOS_POWERD_SDDM_HOME="$ACW/sddm" \
           "$OUT/kdos-powerd" "--$1" "$2" 2>&1 || true)
    if [ "$_got" = "$3" ]; then
        echo "  ok    $4"
    else
        echo "  FAIL  $4"; echo "        sent: --$1 $2"
        echo "        want: $3"; echo "        got:  $_got"; ac_fail=1
    fi
}
acfull accent amber "ok amber" "the accent reaches the boot menu and the login screen"
[ "$(tail -n1 "$ACW/bootctl.args" 2>/dev/null)" = "theme amber" ] \
    && echo "  ok    and kdos-bootctl was asked to restamp the theme" \
    || { echo "  FAIL  kdos-bootctl was not run as \`theme amber\`"; ac_fail=1; }
for _f in kdeglobals plasmarc kcminputrc; do
    cmp -s "$ACW/sets/amber/$_f" "$ACW/sddm/.config/$_f" \
        || { echo "  FAIL  the greeter's $_f is not amber's"; ac_fail=1; }
done
cmp -s "$ACW/amber-dark.png" "$ACW/sddm/kdos-background.png" \
    && [ ! -L "$ACW/sddm/kdos-background.png" ] \
    && echo "  ok    and the greeter's files and its picture, copied rather than linked" \
    || { echo "  FAIL  the greeter's picture is not a copy of amber's"; ac_fail=1; }
acfull accent paper "ok paper (login screen unchanged)" \
    "a set missing a file leaves the login screen and says so"
cmp -s "$ACW/sets/amber/kdeglobals" "$ACW/sddm/.config/kdeglobals" \
    && echo "  ok    and the greeter still wears the accent it had" \
    || { echo "  FAIL  a half set reached the greeter"; ac_fail=1; }
ACW_BOOTCTL_RC=1 acfull accent amber "ok amber (boot menu unchanged)" \
    "a boot menu that cannot be written is said, and is no failure"
# The splash's motion: two words, through kdos-bootctl's `splash` verb.
acfull splash phosphor "ok phosphor" "the splash motion reaches the boot menu"
[ "$(tail -n1 "$ACW/bootctl.args" 2>/dev/null)" = "splash phosphor" ] \
    && echo "  ok    and kdos-bootctl was asked for \`splash phosphor\`" \
    || { echo "  FAIL  kdos-bootctl was not run as \`splash phosphor\`"; ac_fail=1; }
ACW_BOOTCTL_RC=1 acfull splash modern "ok modern (boot menu unchanged)" \
    "a boot menu that cannot take the motion is said"
_n=$(wc -l < "$ACW/bootctl.args")
acfull splash crt "err not a splash motion (phosphor or modern)" \
    "a third motion is refused"
acfull splash "phosphor; reboot" "err not a splash motion (phosphor or modern)" \
    "and so is anything that merely starts with one"
[ "$(wc -l < "$ACW/bootctl.args")" = "$_n" ] \
    && echo "  ok    and a refused motion ran nothing" \
    || { echo "  FAIL  a refused motion ran kdos-bootctl"; ac_fail=1; }
[ "$ac_fail" = 0 ] || exit 1

# ── THE BUS FACE, io.kdos.Power1 ─────────────────────────────────────────
#
# What System Settings calls, on a private bus with testing/fixtures/polkit/
# pkstub.c answering for polkitd, so the test asserts what the daemon ASKED
# polkit as well as what it did with the answer. The daemon is the shipped
# binary pointed at a fixture /etc, which leaves the kernel's ruleset, sshd
# and the boot menu alone.
#
# Skipped rather than failed where there is no sd-bus, dbus-daemon or busctl:
# this script runs on a bare host, and basu is a KDOS port.
if [ -n "$TRAY_SDBUS" ] && command -v dbus-daemon >/dev/null 2>&1 &&
   command -v busctl >/dev/null 2>&1; then
    PB="$OUT/powerbus"
    rm -rf "$PB"
    mkdir -p "$PB/etc/nftables.d"
    $CC $STD $WARN -o "$PB/pkstub" testing/fixtures/polkit/pkstub.c \
        $(pkg-config --cflags --libs "$TRAY_SDBUS") \
        || { echo "  FAIL  the polkit stub does not build"; exit 1; }
    # A bus of its own, typed `system`: the daemon opens the system bus, and
    # the host's own polkitd and powerd must never see this one.
    cat > "$PB/bus.conf" <<'PBBUS'
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
PBBUS
    dbus-daemon --config-file="$PB/bus.conf" --print-address=3 --fork \
        --print-pid=4 3>"$PB/addr" 4>"$PB/pid"
    DBUS_SYSTEM_BUS_ADDRESS="$(cat "$PB/addr")"
    export DBUS_SYSTEM_BUS_ADDRESS
    # The firewall needs a password; the accent is refused over the bus and
    # granted to a process asking over the socket, so both of polkit's
    # verdicts reach both faces.
    PKSTUB_ANSWERS="io.kdos.firewall.set=challenge io.kdos.accent.set=no unix-process:io.kdos.accent.set=yes io.kdos.filesharing.start=yes" \
        "$PB/pkstub" "$PB/pk.log" 60 &
    _pk=$!
    # Every signal the daemon sends, where dbus-monitor is there to see it;
    # listening before the daemon starts, so none is sent unseen.
    _mon=""
    if command -v dbus-monitor >/dev/null 2>&1; then
        dbus-monitor --system "type='signal',interface='io.kdos.Power1'" \
            > "$PB/signals" 2>&1 &
        _mon=$!
        sleep 0.3
    fi
    # nft is the stand-in the rules-file check above wrote: it loads anything
    # and prints that check's recorded counters.
    printf '#!/bin/sh\nprintf "%%s\\n" "$*" >> "%s/samba.args"\n' "$PB" > "$PB/samba"
    chmod +x "$PB/samba"
    KDOS_POWERD_SOCKET="$PB/p.sock" KDOS_POWERD_ETC="$PB/etc" \
    KDOS_POWERD_NFT="$NKW/nft" KDOS_POWERD_SAMBA="$PB/samba" \
        "$OUT/kdos-powerd" 2>"$PB/powerd.err" &
    _pd=$!
    pb_stop() {
        kill "$_pd" "$_pk" $_mon 2>/dev/null
        wait "$_pd" "$_pk" $_mon 2>/dev/null || true
        kill "$(cat "$PB/pid")" 2>/dev/null || true
        unset DBUS_SYSTEM_BUS_ADDRESS
    }
    pb_fail() { echo "  FAIL  $1"; [ -n "${2:-}" ] && sed 's/^/          /' "$2"; pb_stop; exit 1; }
    # busctl on the private bus, stdout and stderr together. Its status is
    # dropped: the refusals are the answers under test, and `set -e` would
    # end the suite on the first one.
    pbcall() {
        busctl --system "$@" 2>&1 || true
    }
    _i=0
    until busctl --system status io.kdos.Power1 >/dev/null 2>&1; do
        _i=$((_i + 1))
        [ "$_i" -gt 50 ] && pb_fail "io.kdos.Power1 never appeared on the bus" "$PB/powerd.err"
        sleep 0.1
    done
    _l=$(pbcall call io.kdos.Power1 /io/kdos/Power1 io.kdos.Power1 \
             ListFirewallServices)
    case "$_l" in
        'a(sbs) 25 '*'"rdp" false "this desktop over RDP (krdp)"'*) ;;
        *) echo "$_l" > "$PB/list.txt"
           pb_fail "ListFirewallServices did not return the table with rdp off" "$PB/list.txt" ;;
    esac
    case "$_l" in
        *'"vnc" false "this desktop over VNC (krfb)"'*) ;;
        *) pb_fail "the vnc row does not name krfb" ;;
    esac
    grep -q CHECK "$PB/pk.log" 2>/dev/null \
        && pb_fail "reading the list asked polkit" "$PB/pk.log"
    # A challenge the caller allowed is the agent's prompt answered yes:
    # busctl sets ALLOW_INTERACTIVE_AUTHORIZATION unless told not to.
    _r=$(pbcall call io.kdos.Power1 /io/kdos/Power1 io.kdos.Power1 \
             SetFirewallService sb mosh true)
    [ -z "$_r" ] || { echo "$_r" > "$PB/r.txt"; pb_fail "an authorised toggle did not succeed" "$PB/r.txt"; }
    grep -q "udp dport 60000-61000 accept" "$PB/etc/nftables.d/50-kdos-services.nft" \
        || pb_fail "the authorised toggle did not write the rule"
    grep -qx "CHECK system-bus-name io.kdos.firewall.set flags=1 service=mosh state=on" \
        "$PB/pk.log" || pb_fail "polkit was not asked about the caller's bus name, the action and the change" "$PB/pk.log"
    # The same challenge from a caller that allowed no prompt is the error
    # that tells it to retry with one, and nothing changes.
    _r=$(pbcall --allow-interactive-authorization=no call io.kdos.Power1 \
             /io/kdos/Power1 io.kdos.Power1 SetFirewallService sb ssh true)
    case "$_r" in
        *InteractiveAuthorizationRequired*|*"needs authentication"*) ;;
        *) echo "$_r" > "$PB/r.txt"; pb_fail "a challenge without a prompt was not InteractiveAuthorizationRequired" "$PB/r.txt" ;;
    esac
    grep -q "tcp dport 22 accept" "$PB/etc/nftables.d/50-kdos-services.nft" \
        && pb_fail "a refused toggle changed the rules"
    # A name the table does not carry is refused BEFORE polkit is asked: a
    # password typed for a port nobody can open is a password for nothing.
    _n=$(grep -c CHECK "$PB/pk.log" || true)
    _r=$(pbcall call io.kdos.Power1 /io/kdos/Power1 io.kdos.Power1 \
             SetFirewallService sb telnet true)
    case "$_r" in *NoSuchService*|*"no service called telnet"*) ;;
        *) echo "$_r" > "$PB/r.txt"; pb_fail "an unknown service was not NoSuchService" "$PB/r.txt" ;;
    esac
    [ "$(grep -c CHECK "$PB/pk.log" || true)" = "$_n" ] \
        || pb_fail "polkit was asked about a name the table does not carry"
    # polkit's no is a no, and the accent stays as it was.
    _r=$(pbcall call io.kdos.Power1 /io/kdos/Power1 io.kdos.Power1 \
             SetAccent s amber)
    case "$_r" in *AccessDenied*|*"Access denied"*|*"not authorised"*) ;;
        *) echo "$_r" > "$PB/r.txt"; pb_fail "a refused accent was not AccessDenied" "$PB/r.txt" ;;
    esac
    [ -e "$PB/etc/kdos/accent" ] && pb_fail "a refused accent was written"
    _r=$(pbcall call io.kdos.Power1 /io/kdos/Power1 io.kdos.Power1 \
             SetAccent s mauve)
    case "$_r" in *NoSuchAccent*|*"not an accent"*) ;;
        *) pb_fail "a name outside the palette was not NoSuchAccent" ;;
    esac
    _r=$(pbcall get-property io.kdos.Power1 /io/kdos/Power1 io.kdos.Power1 Accent)
    [ "$_r" = 's "bone"' ] || { echo "$_r" > "$PB/r.txt"; pb_fail "Accent does not read the palette's default" "$PB/r.txt"; }
    _r=$(pbcall get-property io.kdos.Power1 /io/kdos/Power1 io.kdos.Power1 Accents)
    case "$_r" in 'as 8 '*'"phosphor"'*'"paper"'*) ;;
        *) echo "$_r" > "$PB/r.txt"; pb_fail "Accents is not the palette" "$PB/r.txt" ;;
    esac
    # The splash's motion asks the accent's action, with the motion named,
    # and a word that is no motion is refused before polkit is asked.
    _r=$(pbcall call io.kdos.Power1 /io/kdos/Power1 io.kdos.Power1 \
             SetSplash s phosphor)
    case "$_r" in *AccessDenied*|*"Access denied"*|*"not authorised"*) ;;
        *) echo "$_r" > "$PB/r.txt"; pb_fail "a refused splash was not AccessDenied" "$PB/r.txt" ;;
    esac
    grep -qx "CHECK system-bus-name io.kdos.accent.set flags=1 splash=phosphor" "$PB/pk.log" \
        || pb_fail "the splash did not ask the accent's action about the motion" "$PB/pk.log"
    _n=$(grep -c CHECK "$PB/pk.log" || true)
    _r=$(pbcall call io.kdos.Power1 /io/kdos/Power1 io.kdos.Power1 \
             SetSplash s crt)
    case "$_r" in *NoSuchMotion*|*"not a splash motion"*) ;;
        *) echo "$_r" > "$PB/r.txt"; pb_fail "a word that is no motion was not NoSuchMotion" "$PB/r.txt" ;;
    esac
    [ "$(grep -c CHECK "$PB/pk.log" || true)" = "$_n" ] \
        || pb_fail "polkit was asked about a word that is no motion"
    # Starting Samba asks its own action with no details, and polkit's yes
    # runs the service script with `start`.
    _r=$(pbcall call io.kdos.Power1 /io/kdos/Power1 io.kdos.Power1 \
             StartFileSharing)
    [ -z "$_r" ] || { echo "$_r" > "$PB/r.txt"; pb_fail "an authorised StartFileSharing did not succeed" "$PB/r.txt"; }
    grep -qx "CHECK system-bus-name io.kdos.filesharing.start flags=1" "$PB/pk.log" \
        || pb_fail "StartFileSharing did not ask io.kdos.filesharing.start about the caller" "$PB/pk.log"
    [ "$(cat "$PB/samba.args" 2>/dev/null)" = start ] \
        || pb_fail "StartFileSharing did not run the samba script with start"
    # The knocks: the closed names the kernel counts, asking nobody. mosh is
    # open by now, so its counter is not one of them.
    _r=$(pbcall call io.kdos.Power1 /io/kdos/Power1 io.kdos.Power1 \
             ListFirewallKnocks)
    [ "$_r" = 'a(st) 2 "ssh" 12 "kdeconnect" 3' ] \
        || { echo "$_r" > "$PB/r.txt"; pb_fail "ListFirewallKnocks is not the closed names' counts" "$PB/r.txt"; }
    [ "$(grep -c CHECK "$PB/pk.log" || true)" = "$_n" ] \
        || pb_fail "reading the knocks asked polkit"
    # The one change applied so far, the mosh toggle, was announced.
    if [ -n "$_mon" ]; then
        grep -q "member=FirewallServicesChanged" "$PB/signals" \
            || pb_fail "a change sent no FirewallServicesChanged" "$PB/signals"
    fi
    # THE SOCKET ASKS THE SAME POLKIT for a caller outside wheel, about its
    # process and without a prompt. Run only where this user is outside
    # wheel and not root, which is the case under test.
    if [ "$(id -u)" -ne 0 ] && ! id -nG | tr ' ' '\n' | grep -qx wheel; then
        # A challenge is a no on a socket, which has no agent to show it.
        KDOS_POWERD_SOCKET="$PB/p.sock" "$OUT/kdos-power" firewall ssh on \
            > "$PB/r.txt" 2>&1 && pb_fail "a challenge on the socket opened a port"
        grep -q "not permitted" "$PB/r.txt" || pb_fail "the socket's refusal does not say so" "$PB/r.txt"
        grep -qx "CHECK unix-process uid=$(id -u) io.kdos.firewall.set flags=0" "$PB/pk.log" \
            || pb_fail "the socket did not ask polkit about its peer's process" "$PB/pk.log"
        # polkit's yes for the process is the socket's yes.
        KDOS_POWERD_SOCKET="$PB/p.sock" "$OUT/kdos-power" accent amber \
            2>"$PB/r.txt" || pb_fail "polkit granted the accent and the socket refused it" "$PB/r.txt"
        [ "$(cat "$PB/etc/kdos/accent" 2>/dev/null)" = amber ] \
            || pb_fail "the granted accent was not written"
        _r=$(pbcall get-property io.kdos.Power1 /io/kdos/Power1 io.kdos.Power1 Accent)
        [ "$_r" = 's "amber"' ] || pb_fail "Accent does not read back the socket's change"
        # And anybody may read the table and ping.
        KDOS_POWERD_SOCKET="$PB/p.sock" "$OUT/kdos-power" firewall list \
            | grep -q "^mosh	on	" || pb_fail "the list does not read back the bus's change"
        KDOS_POWERD_SOCKET="$PB/p.sock" "$OUT/kdos-power" ping \
            || pb_fail "ping was refused"
    else
        echo "  the socket's polkit half is skipped (running as root or in wheel)"
    fi
    pb_stop
    echo "  io.kdos.Power1: the list, the knocks, polkit asked per change, both refusals, the accent, the splash, samba"
else
    echo "  io.kdos.Power1 is skipped (no sd-bus, dbus-daemon or busctl on this host)"
fi

# ── SAMBA'S START ────────────────────────────────────────────────────────
#
# What StartFileSharing runs: Samba's service script with `start` and nothing
# else, and nothing at all while /etc/service.disabled/samba exists. A
# stand-in script (KDOS_POWERD_SAMBA, honoured only under a fixture /etc)
# records its arguments.
SHW="$OUT/sharework"
rm -rf "$SHW"
mkdir -p "$SHW/etc/service.disabled"
printf '#!/bin/sh\nprintf "%%s\\n" "$*" >> "%s/samba.args"\n' "$SHW" > "$SHW/samba"
chmod +x "$SHW/samba"
sh_share() { KDOS_POWERD_ETC="$SHW/etc" KDOS_POWERD_SAMBA="$SHW/samba" "$OUT/kdos-powerd" --file-sharing; }
sh_share > "$SHW/out" 2>&1 || { cat "$SHW/out"; echo "  FAIL  --file-sharing did not succeed"; exit 1; }
[ "$(cat "$SHW/samba.args")" = start ] \
    || { echo "  FAIL  --file-sharing did not run the samba script with start alone"; exit 1; }
: > "$SHW/etc/service.disabled/samba"
sh_share > "$SHW/out" 2>&1 && { echo "  FAIL  a disabled samba was started"; exit 1; }
grep -q "^err samba is disabled" "$SHW/out" \
    || { cat "$SHW/out"; echo "  FAIL  the disabled refusal does not say so"; exit 1; }
[ "$(wc -l < "$SHW/samba.args")" = 1 ] \
    || { echo "  FAIL  a disabled samba ran its script"; exit 1; }
echo "  ok    samba starts by its script alone, and never while disabled"

echo "  --explain, the tiers, loginctl, the firewall table, the accent, the greeter, the splash and samba"

echo
echo "==> kdos-settingsd keeps hostname1, timedate1 and locale1 in the files KDOS reads"
# TWO HALVES, TESTED APART, because only one of them needs a bus. store.c is
# every file the daemon writes and every rule a value is held to, with no
# sd-bus in it, so unit.c asserts it on any host. run.sh drives every method
# of the three interfaces over a private dbus-daemon, against polkit.c in
# polkit's place, wherever an sd-bus and the dbus tools exist. Both run
# against the temporary /etc, /usr/share and /sys tree.sh builds:
# KDOS_SETTINGSD_ETC, KDOS_SETTINGSD_SHARE and KDOS_SETTINGSD_SYS point the
# daemon there, and the first also keeps it off this machine's kernel
# hostname and clock.
#
# Leak checking is ON for both under a sanitising CC: the daemon is a
# long-lived process that answers the same calls for the life of the machine.
sh testing/fixtures/settingsd/tree.sh "$OUT/settingsd-tree"
$CC $STD $WARN -Isrc/libs/libkbase -o "$OUT/settingsd-unit" \
    testing/fixtures/settingsd/unit.c src/daemons/kdos-settingsd/store.c \
    src/libs/libkbase/*.c
if ! ASAN_OPTIONS=detect_leaks=1 "$OUT/settingsd-unit" "$OUT/settingsd-tree" \
        > "$OUT/settingsd-unit.txt" 2>&1; then
    cat "$OUT/settingsd-unit.txt"
    exit 1
fi
echo "  the files and their rules: $(grep -c '^  ok' "$OUT/settingsd-unit.txt") checks"
if [ -n "$TRAY_SDBUS" ] && command -v dbus-daemon >/dev/null 2>&1 &&
   command -v dbus-send >/dev/null 2>&1 &&
   command -v dbus-monitor >/dev/null 2>&1; then
    $CC $STD $WARN -Isrc/libs/libkbase $(pkg-config --cflags $TRAY_SDBUS) \
        -o "$OUT/kdos-settingsd" src/daemons/kdos-settingsd/*.c \
        src/libs/libkbase/*.c $(pkg-config --libs $TRAY_SDBUS)
    $CC $STD $WARN $(pkg-config --cflags $TRAY_SDBUS) \
        -o "$OUT/settingsd-polkit" testing/fixtures/settingsd/polkit.c \
        $(pkg-config --libs $TRAY_SDBUS)
    sh testing/fixtures/settingsd/tree.sh "$OUT/settingsd-tree"
    if ! ASAN_OPTIONS=detect_leaks=1 bash testing/fixtures/settingsd/run.sh \
            "$OUT" "$OUT/settingsd-tree" > "$OUT/settingsd-bus.txt" 2>&1; then
        cat "$OUT/settingsd-bus.txt"
        exit 1
    fi
    grep '^  skip' "$OUT/settingsd-bus.txt" || true
    echo "  the three interfaces on a bus: $(grep -c '^  ok' "$OUT/settingsd-bus.txt") checks"
else
    echo "  the bus half is skipped (no sd-bus, dbus-daemon, dbus-send or dbus-monitor on this host)"
fi

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
# The two halves of the name, for a reader that groups by box and must not
# parse the display string; an unboxed app's box is null, not "".
grep -q '"app":"firefox-esr","box":"kdos-apps"}' "$E" \
    || { echo "  the app and its box are not fields of their own"; exit 1; }
grep -q '"app":"kwin_wayland","box":null}' "$E" \
    || { echo "  an unboxed app's box is not null"; exit 1; }
# A window the machine slept through is dropped and counted, not charged:
# RAPL runs on from wherever the firmware left it after a resume.
rm -rf "$OUT/energy-slept"
cp -R testing/fixtures/energy "$OUT/energy-slept"
echo 3600 > "$OUT/energy-slept/2/slept"
KDOS_ALIEN_APPS=testing/fixtures/energy/alien-apps \
    "$OUT/kdos-energyd" --fixture "$OUT/energy-slept" --json > "$OUT/energy-slept.json"
grep -q '"samples":2,' "$OUT/energy-slept.json" \
    && grep -q '"suspended_windows":1}' "$OUT/energy-slept.json" \
    || { echo "  a window across a suspend was folded in"
         cat "$OUT/energy-slept.json"; exit 1; }
grep -q '"suspended_windows":0}' "$E" \
    || { echo "  a fixture with no suspend reported one"; exit 1; }
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
echo "==> kdos-ports-helper takes one verb and one port name, and only as root"
# The root side of Discover's Install button for host software, run through
# pkexec. Everything it would do needs root and a binhost, so what is checked
# here is the door: a second verb, a name that could be read as an option or
# a path, and an unprivileged caller are each refused before anything runs.
$CC $STD $WARN $INC -o "$OUT/kdos-ports-helper" \
    src/desktop/kdos-discover/helper/kdos-ports-helper.c src/libs/libkbase/*.c
echo "  kdos-ports-helper"
# Each refusal is told by its message: run as anybody but root, a name the
# check let through would still exit 2, at the privilege test, and pass.
for bad in "" "remove kate" "install" "install kate extra"; do
    # shellcheck disable=SC2086
    "$OUT/kdos-ports-helper" $bad 2>&1 | grep -q '^usage: kdos-ports-helper install' \
        || { echo "  kdos-ports-helper did not refuse '$bad' as usage"; exit 1; }
done
for bad in -rf ../x a/b .hidden 'a b' 'k;rm'; do
    "$OUT/kdos-ports-helper" install "$bad" 2>&1 | grep -q 'is not a port name' \
        || { echo "  kdos-ports-helper took '$bad' for a port name"; exit 1; }
done
if [ "$(id -u)" != 0 ]; then
    "$OUT/kdos-ports-helper" install kate 2>&1 | grep -q 'run through pkexec' \
        || { echo "  kdos-ports-helper ran for an unprivileged caller"; exit 1; }
fi
echo "  usage, unsafe names and an unprivileged caller are refused"

echo
echo "==> kdos-oomd picks the victim the desktop can afford to lose"
# The trigger is memory pressure and the effect is a SIGKILL, so neither can be
# summoned in a test. What can is the SELECTION, and it is the whole of the
# design: --fixture replays a recorded /proc through the same pick_victim() the
# daemon runs and prints the choice without signalling anybody.
#
# testing/fixtures/oomd/preferred is arranged so that the four exclusions are
# each load-bearing — every process the daemon must NOT choose is LARGER than
# the one it must: pipewire-pulse (2.4 G, protected by comm), kwin_wayland
# (2 G, same), a nightly backup at oom_score_adj -1000 (1.6 G), and a kernel
# thread with an empty cmdline (1.2 G). If any one of those checks broke, the
# answer would be that process and not firefox-esr.
$CC $STD $WARN $INC -o "$OUT/kdos-oomd" \
    src/daemons/kdos-oomd/main.c src/libs/libkbase/*.c src/libs/libkproc/*.c
echo "  kdos-oomd"
OM="$OUT/oomd.txt"
"$OUT/kdos-oomd" --fixture testing/fixtures/oomd/preferred > "$OM" \
    || { echo "  the fixture found no candidate at all"; cat "$OM"; exit 1; }
grep -q "would kill firefox-esr (appbox kdos-apps)" "$OM" \
    || { echo "  the boxed app was not chosen or not named with its box"
         cat "$OM"; exit 1; }
for spared in pipewire-pulse kwin_wayland backup kswapd0 init; do
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
# refinement, it is what makes `memory =` mean anything: rootless podman's
# --memory is enforced only inside an elogind session's delegated subtree, and
# a box started anywhere else has the flag accepted and nothing enforced; a
# profile key KDOS cannot enforce is one it does not offer. The fixture's host
# process is LARGER than anything in either box, so only the budget can
# produce the right answer.
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

# …and a stall is not a shortage. One copy streaming through the page cache
# stalls every task that wants to run — itself — with most of memory still
# available, and killing it frees nothing anyone was short of. The same tree
# with a meminfo at 80% available is no kill; at 7.5% it is the usual answer.
cp -R testing/fixtures/oomd/hostwins "$OUT/oomd-mem"
printf 'MemTotal:        4000000 kB\nMemFree:          200000 kB\nMemAvailable:    3200000 kB\n' \
    > "$OUT/oomd-mem/meminfo"
if "$OUT/kdos-oomd" --fixture "$OUT/oomd-mem" > "$OUT/oomd5.txt"; then
    echo "  a stall with 80% of memory available chose a victim"; cat "$OUT/oomd5.txt"; exit 1
fi
grep -q "no kill: 3200000 of 4000000 kB available" "$OUT/oomd5.txt" \
    || { echo "  the refusal does not say how much was available"; cat "$OUT/oomd5.txt"; exit 1; }
printf 'MemTotal:        4000000 kB\nMemFree:          100000 kB\nMemAvailable:     300000 kB\n' \
    > "$OUT/oomd-mem/meminfo"
"$OUT/kdos-oomd" --fixture "$OUT/oomd-mem" > "$OUT/oomd6.txt" \
    && grep -q "would kill kdosbuild (pid 950" "$OUT/oomd6.txt" \
    || { echo "  memory short and stalled, and no victim"; cat "$OUT/oomd6.txt"; exit 1; }
echo "  a stall kills only when MemAvailable is at or under a tenth of MemTotal"

# …and the whole Plasma session is spared. testing/fixtures/oomd/session runs
# every protected name, each larger than the one process it must choose and
# each a different size, so a name that stopped matching is named by the
# answer. The names are comms: a list entry longer than the kernel's 15 bytes
# could never equal one, and is refused here before it can protect nothing.
"$OUT/kdos-oomd" --protected > "$OUT/oomd-names.txt"
awk 'length($0) > 15 { print "  " $0 " is longer than a comm can be"; bad = 1 }
     END { exit bad }' "$OUT/oomd-names.txt" || exit 1
for _n in kwin_wayland kwin_wayland_wr plasmashell ksmserver kded6 sddm \
          elogind ksecretd xdg-desktop-por polkit-kde-auth Xwayland 'pipewire*'; do
    grep -qxF "$_n" "$OUT/oomd-names.txt" \
        || { echo "  $_n is not protected"; exit 1; }
done
# THE LIST IS EXACTLY THE SESSION'S: a protected name that is not one of
# these is a process the killer can never choose, whatever it grows to.
printf '%s\n' kwin_wayland kwin_wayland_wr plasmashell ksmserver kded6 \
    plasma_session kglobalacceld kscreenlocker_g Xwayland xdg-desktop-por \
    polkit-kde-auth ksecretd sddm sddm-helper sddm-greeter-qt elogind \
    turnstiled dbus-daemon ksvc kdos-powerd kdos-energyd kdos-oomd \
    kdos-settingsd accounts-daemon wireplumber 'pipewire*' \
    | LC_ALL=C sort > "$OUT/oomd-want.txt"
LC_ALL=C sort "$OUT/oomd-names.txt" | diff -u "$OUT/oomd-want.txt" - \
    || { echo "  the protected names are not exactly the session's"; exit 1; }
"$OUT/kdos-oomd" --fixture testing/fixtures/oomd/session > "$OUT/oomd7.txt" \
    || { echo "  the session fixture found no candidate"; cat "$OUT/oomd7.txt"; exit 1; }
grep -q "would kill kate (pid 500" "$OUT/oomd7.txt" \
    || { echo "  a session process was chosen over the largest app"
         cat "$OUT/oomd7.txt"; exit 1; }
echo "  the session's $(wc -l < "$OUT/oomd-names.txt") protected names are comms, and every one holds"

# WHO MAY READ IT: root, wheel, and the person at the machine. The third is
# elogind's sd_uid_get_state(), and libelogind is opened by name at run time,
# so testing/fixtures/oomd/sdlogin.c built as libelogind.so.0 first on
# LD_LIBRARY_PATH is the session tracker the shipped binary asks.
mkdir -p "$OUT/fakelogin"
$CC $STD $WARN -shared -fPIC -o "$OUT/fakelogin/libelogind.so.0" \
    testing/fixtures/oomd/sdlogin.c
"$OUT/kdos-oomd" --explain root | grep -q "permitted" \
    || { echo "  root may not read kdos-oomd"; exit 1; }
LD_LIBRARY_PATH="$OUT/fakelogin" KDOS_TEST_UID_STATE="65534=active" \
    "$OUT/kdos-oomd" --explain nobody | grep -q "in an active session — permitted" \
    || { echo "  a user in an active session may not read kdos-oomd"; exit 1; }
LD_LIBRARY_PATH="$OUT/fakelogin" KDOS_TEST_UID_STATE="65534=online" \
    "$OUT/kdos-oomd" --explain nobody >/dev/null \
    && { echo "  a user logged in but not at the machine may read kdos-oomd"; exit 1; }
LD_LIBRARY_PATH="$OUT/fakelogin" "$OUT/kdos-oomd" --explain nobody >/dev/null \
    && { echo "  a user with no login may read kdos-oomd"; exit 1; }
echo "  root, wheel and the active session read it; nobody else does"

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
# The fixtures are hand-built headers: `hybrid.img`
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
echo "==> genlaunchers names a boxed launcher with the app id KWin gives its windows"
# KWin replaces a tagged window's app_id with the launcher id kdos-boxsock tags
# its socket with, and sets desktopFileName from it, so a boxed launcher's FILE
# NAME is that id: <box> for a pack with one launcher, <box>.<stem> for each of
# a pack with several. The build's form reads one extracted pack per
# directory, which is also where its icons are copied out of.
PKX="$OUT/packsx"; FSP="$OUT/fsrootp"
rm -rf "$PKX" "$FSP"
mkdir -p "$PKX/app.one/usr/share/applications" \
         "$PKX/app.one/usr/share/icons/hicolor/48x48/apps" \
         "$PKX/app.one/usr/share/icons/hicolor/scalable/apps" \
         "$PKX/app.multi/usr/share/applications" \
         "$PKX/app.multi/usr/share/pixmaps" "$FSP"
cat > "$PKX/app.one/usr/share/applications/org.example.One.desktop" <<'DESK'
[Desktop Entry]
Type=Application
Name=One
Exec=one %F
Icon=org.example.One
MimeType=text/x-one;
Categories=Office;
Keywords=first;
DESK
: > "$PKX/app.one/usr/share/icons/hicolor/48x48/apps/org.example.One.png"
: > "$PKX/app.one/usr/share/icons/hicolor/scalable/apps/org.example.One.svg"
cat > "$PKX/app.multi/usr/share/applications/maxima.desktop" <<'DESK'
[Desktop Entry]
Type=Application
Name=Maxima
Exec=maxima
Icon=maxima
Terminal=true
Categories=Science;
DESK
cat > "$PKX/app.multi/usr/share/applications/wxmaxima.desktop" <<'DESK'
[Desktop Entry]
Type=Application
Name=wxMaxima
Exec=wxmaxima %F
Icon=wxmaxima
MimeType=text/x-wxm;
Categories=Science;
DESK
: > "$PKX/app.multi/usr/share/pixmaps/wxmaxima.png"
"$OUT/kdos-appbox" genlaunchers --packs-dir "$PKX" "$FSP" 2>/dev/null \
    || { echo "  genlaunchers --packs-dir failed"; exit 1; }
PA="$FSP/etc/skel/.local/share/applications"
PD="$FSP/etc/skel/.local/share"
test -f "$PA/app.one.desktop" \
    || { echo "  FAIL  a pack with one launcher is <box>.desktop"; ls "$PA"; exit 1; }
test -f "$PA/app.multi.maxima.desktop" && test -f "$PA/app.multi.wxmaxima.desktop" \
    || { echo "  FAIL  a pack with several is <box>.<stem>.desktop each"; ls "$PA"; exit 1; }
test ! -e "$PA/org.example.One.desktop" \
    || { echo "  FAIL  upstream's id is not a boxed launcher's name"; exit 1; }
grep -qx 'Exec=kdos-appbox -b app.one -l app.one run one %F' "$PA/app.one.desktop" \
    && grep -qx 'StartupWMClass=app.one' "$PA/app.one.desktop" \
    && grep -qx 'X-KDOS-Box=app.one' "$PA/app.one.desktop" \
    && grep -qx 'X-KDOS-Upstream=org.example.One' "$PA/app.one.desktop" \
    && grep -qx 'Keywords=first;box;' "$PA/app.one.desktop" \
    || { echo "  FAIL  the launcher names its box and its id"; cat "$PA/app.one.desktop"; exit 1; }
grep -qx 'Exec=kdos-appbox -b app.multi -l app.multi.wxmaxima run wxmaxima %F' \
    "$PA/app.multi.wxmaxima.desktop" \
    || { echo "  FAIL  each launcher of a multi-launcher pack passes its own id"; exit 1; }
# The icons come out of the pack under the launcher's name, every size, and a
# pixmap where the pack has no themed one.
grep -qx 'Icon=app.one' "$PA/app.one.desktop" \
    && test -f "$PD/icons/hicolor/48x48/apps/app.one.png" \
    && test -f "$PD/icons/hicolor/scalable/apps/app.one.svg" \
    && test -f "$PD/pixmaps/app.multi.wxmaxima.png" \
    && grep -qx 'Icon=app.multi.wxmaxima' "$PA/app.multi.wxmaxima.desktop" \
    || { echo "  FAIL  the icons are exported under the launcher ids"; find "$PD" -type f; exit 1; }
# An icon the pack does not carry is left as upstream named it.
grep -qx 'Icon=maxima' "$PA/app.multi.maxima.desktop" \
    || { echo "  FAIL  a missing icon keeps its name"; exit 1; }
grep -q '^text/x-one=app.one.desktop;' "$PA/mimeinfo.cache" \
    && grep -q '^text/x-wxm=app.multi.wxmaxima.desktop;' "$PA/mimeinfo.cache" \
    || { echo "  FAIL  the mime cache names the launcher ids"; cat "$PA/mimeinfo.cache"; exit 1; }
grep -q '^one	one %F	app.one	app.one$' "$FSP/usr/share/kdos/alien-apps" \
    && grep -q '^wxmaxima	wxmaxima %F	app.multi	app.multi.wxmaxima$' "$FSP/usr/share/kdos/alien-apps" \
    || { echo "  FAIL  the table's fourth field is the launcher id"; cat "$FSP/usr/share/kdos/alien-apps"; exit 1; }
grep -q '^\[kdos-box-app.one\]' "$FSP/etc/skel/.config/kwinrulesrc" \
    && grep -q '^\[kdos-box-app.multi\]' "$FSP/etc/skel/.config/kwinrulesrc" \
    && grep -q '^decocolor=KDOSBox$' "$FSP/etc/skel/.config/kwinrulesrc" \
    || { echo "  FAIL  every pack box gets a title-bar rule in the skeleton"; cat "$FSP/etc/skel/.config/kwinrulesrc"; exit 1; }
# A second run reconciles: the icons of a pack that is gone go with it, by the
# list the first run left, and nothing else in the icon tree is touched.
: > "$PD/icons/hicolor/48x48/apps/someone-else.png"
rm -rf "$PKX/app.one"
"$OUT/kdos-appbox" genlaunchers --packs-dir "$PKX" "$FSP" 2>/dev/null
test ! -e "$PA/app.one.desktop" && test ! -e "$PD/icons/hicolor/48x48/apps/app.one.png" \
    && test -f "$PD/icons/hicolor/48x48/apps/someone-else.png" \
    || { echo "  FAIL  a gone pack's launcher and icons are swept, and only those"; exit 1; }
echo "  ok    launcher ids, exported icons, the mime cache, the table and the rules agree"

echo
echo "==> a launch hands the box only its own sockets, or does not start"
# THE LAUNCH PATH OVER A FAKE ENGINE: podman, xdg-dbus-proxy and kdos-boxsock
# are stand-ins that record what they were asked, so what is checked is what
# kdos-appbox decided — the state file the Boxes page and the badge read, the
# engine string KWin is handed, and the sockets the application is pointed at.
# The three paths of plan 7.3.3: a confined launch, a box whose container was
# made over the session's runtime directory (tagged), and a tagged socket that
# cannot be had (the launch is refused, never started untagged).
LX="$OUT/launch"
rm -rf "$LX"; mkdir -p "$LX/bin" "$LX/home/.config/kdos/boxes" "$LX/log"
mkdir -m 0700 "$LX/run"
printf 'base=pack:app.one\nwayland=yes\naudio=yes\n' > "$LX/home/.config/kdos/boxes/app.one.conf"
cat > "$LX/bin/podman" <<'FAKE'
#!/bin/bash
echo "$*" >> "$FAKE_LOG/podman.calls"
case "$1 $2" in
    "container exists") exit 0 ;;
    "logs "*) echo container_setup_done; exit 0 ;;
esac
if [ "$1" = inspect ]; then
    case "$*" in
        *Mounts*) printf '/run/user/%s\t%s\n' "$(id -u)" "$FAKE_MOUNT" ;;
        *ConmonPid*) echo "running $FAKE_CONMON" ;;
        *CgroupParent*) echo "" ;;
        *) echo running ;;
    esac
    exit 0
fi
if [ "$1" = exec ]; then
    printf '%s\n' "$@" > "$FAKE_LOG/exec.argv"
    cat "$XDG_RUNTIME_DIR"/kdos/boxes/*.apps > "$FAKE_LOG/ledger" 2>/dev/null
    exit 0
fi
exit 0
FAKE
cat > "$LX/bin/xdg-dbus-proxy" <<'FAKE'
#!/usr/bin/env python3
import os, socket, sys
sync = int(sys.argv[1].split("=")[1]); argfd = int(sys.argv[2].split("=")[1])
data = b""
while True:
    d = os.read(argfd, 65536)
    if not d:
        break
    data += d
words = data.split(b"\0")
open(os.path.join(os.environ["FAKE_LOG"], "proxy.args"), "wb").write(b"\n".join(words))
s = socket.socket(socket.AF_UNIX)
s.bind(words[1].decode())
s.listen(1)
os.write(sync, b"x")
os.read(sync, 1)
FAKE
cat > "$LX/bin/boxsock" <<'FAKE'
#!/bin/sh
echo "$*" >> "$FAKE_LOG/boxsock.calls"
case "$FAKE_BOXSOCK" in
    ok) shift 2; echo "ready $XDG_RUNTIME_DIR/kdos/box/$1/wl@wayland-0" ;;
    error) echo "error the compositor has no security-context-v1" ;;
    hang) sleep 7 ;;
esac
FAKE
chmod +x "$LX/bin/podman" "$LX/bin/xdg-dbus-proxy" "$LX/bin/boxsock"
sleep 60 & FAKE_CONMON=$!
lrun() {  # <boxsock mode> <mount source> — runs one launch of app.one
    env -i PATH="$LX/bin:/usr/bin:/bin" HOME="$LX/home" XDG_RUNTIME_DIR="$LX/run" \
        XDG_DATA_HOME="$LX/home/.local/share" WAYLAND_DISPLAY=wayland-0 \
        DBUS_SESSION_BUS_ADDRESS="unix:path=$LX/nobus" KDOS_PACKD_SOCKET="$LX/nopackd" \
        KDOS_BOXSOCK="$LX/bin/boxsock" FAKE_LOG="$LX/log" FAKE_BOXSOCK="$1" \
        FAKE_MOUNT="$2" FAKE_CONMON="$FAKE_CONMON" LANG=C.UTF-8 \
        "$OUT/kdos-appbox" -b app.one -l app.one run one 2>"$LX/log/stderr"
}
LST="$LX/run/kdos/boxes/app.one.state"
LPV="$LX/run/kdos/box/app.one"
UIDN=$(id -u)
lrun ok "$LPV" || { echo "  FAIL  a confined launch ran"; cat "$LX/log/stderr"; kill $FAKE_CONMON; exit 1; }
grep -qx 'confinement = confined' "$LST" \
    && grep -qx 'engine      = io.kdos.appbox+private' "$LST" \
    && grep -qx 'runtime     = private' "$LST" \
    && grep -qx 'bus         = filtered' "$LST" \
    || { echo "  FAIL  the state says confined"; cat "$LST"; kill $FAKE_CONMON; exit 1; }
grep -qx -- '-s confined app.one app.one' "$LX/log/boxsock.calls" \
    || { echo "  FAIL  KWin is handed the confined engine"; cat "$LX/log/boxsock.calls"; kill $FAKE_CONMON; exit 1; }
grep -qx "WAYLAND_DISPLAY=/run/user/$UIDN/wl@wayland-0" "$LX/log/exec.argv" \
    && grep -qx "DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/$UIDN/dbus-proxy" "$LX/log/exec.argv" \
    && grep -qx "XDG_RUNTIME_DIR=/run/user/$UIDN" "$LX/log/exec.argv" \
    && grep -qx 'XDG_CURRENT_DESKTOP=KDE' "$LX/log/exec.argv" \
    && grep -qx 'DISPLAY=' "$LX/log/exec.argv" \
    || { echo "  FAIL  the application is pointed at the box's own sockets"; cat "$LX/log/exec.argv"; kill $FAKE_CONMON; exit 1; }
test -S "$LPV/dbus-proxy" && test ! -e "$LPV/wayland-0" && test ! -e "$LPV/bus" \
    || { echo "  FAIL  the private directory holds the proxy and no session socket"; ls -la "$LPV"; kill $FAKE_CONMON; exit 1; }
grep -qx -- '--filter' "$LX/log/proxy.args" \
    && grep -qx "unix:path=$LX/run/bus" "$LX/log/proxy.args" \
    || { echo "  FAIL  the keeper's proxy filters the session bus"; cat "$LX/log/proxy.args"; kill $FAKE_CONMON; exit 1; }
grep -q " run app.one	one$" "$LX/log/ledger" \
    || { echo "  FAIL  the launch is in the box's ledger"; kill $FAKE_CONMON; exit 1; }
echo "  ok    confined: its own runtime directory, a filtered bus, the private engine"
: > "$LX/log/boxsock.calls"
lrun ok "$LX/run" || { echo "  FAIL  a tagged launch ran"; kill $FAKE_CONMON; exit 1; }
grep -qx 'confinement = tagged' "$LST" && grep -qx -- '-s tagged app.one app.one' "$LX/log/boxsock.calls" \
    || { echo "  FAIL  a box over the session's runtime directory is tagged"; cat "$LST"; kill $FAKE_CONMON; exit 1; }
echo "  ok    tagged: a box made over the session's runtime directory says so"
rm -f "$LX/log/exec.argv"
if lrun error "$LPV"; then echo "  FAIL  a launch with no tagged socket started"; kill $FAKE_CONMON; exit 1; fi
grep -qx 'confinement = unconfined' "$LST" \
    && grep -q 'reason      = kdos-boxsock: the compositor has no security-context-v1' "$LST" \
    && test ! -e "$LX/log/exec.argv" \
    || { echo "  FAIL  a refused launch records why and runs nothing"; cat "$LST"; kill $FAKE_CONMON; exit 1; }
if lrun hang "$LPV"; then echo "  FAIL  a launch whose socket never came started"; kill $FAKE_CONMON; exit 1; fi
grep -q 'reason      = kdos-boxsock gave no answer within 5 s' "$LST" \
    || { echo "  FAIL  a socket that never comes is a refusal with its reason"; cat "$LST"; kill $FAKE_CONMON; exit 1; }
echo "  ok    unconfined: no tagged socket is no launch, and the state says why"
# The keeper lives exactly as long as the box's conmon: it ends with it, and
# takes the state, the ledger and the proxy's socket along.
kill $FAKE_CONMON; wait $FAKE_CONMON 2>/dev/null || true
for _ in 1 2 3 4 5 6 7 8 9 10; do [ -e "$LST" ] || break; sleep 0.3; done
test ! -e "$LST" && test ! -e "$LPV/dbus-proxy" \
    || { echo "  FAIL  the keeper clears the box's state when the box stops"; exit 1; }
echo "  ok    and the keeper clears it all when the box stops"

echo
echo "==> a throwaway box gets one file, no network and nothing of the host, then goes"
# The same fakes, and a kdos-packd stand-in that composes anything: what is
# checked is the container kdos-appbox asks for (plan 7.3.5) and that every
# trace of it is gone once the application exits.
TW="$OUT/tw"
rm -rf "$TW"; mkdir -p "$TW/bin" "$TW/home/.config/kdos/boxes" "$TW/log" \
    "$TW/home/.local/share/applications" "$TW/docs"
mkdir -m 0700 "$TW/run"
cp "$LX/bin/xdg-dbus-proxy" "$LX/bin/boxsock" "$TW/bin/"
printf 'base=pack:app.viewer\naccent=amber\ngrant=window-list\n' \
    > "$TW/home/.config/kdos/boxes/app.viewer.conf"
cat > "$TW/home/.local/share/applications/app.viewer.desktop" <<'ENTRY'
[Desktop Entry]
Type=Application
Name=Viewer
Exec=kdos-appbox -b app.viewer -l app.viewer run viewer %f
MimeType=application/pdf;
X-KDOS-Box=app.viewer
X-KDOS-Upstream=viewer
X-KDOS-Alien=true
ENTRY
echo secret > "$TW/docs/beside.txt"
echo report > "$TW/docs/report.pdf"
cat > "$TW/bin/podman" <<'FAKE'
#!/bin/bash
echo "$*" >> "$FAKE_LOG/podman.calls"
last=${!#}
case "$1 $2" in
    "container exists") exit 0 ;;
    "logs "*) echo container_setup_done; exit 0 ;;
esac
case "$1" in
    create) printf '%s\n' "$@" > "$FAKE_LOG/create.argv"; exit 0 ;;
    exec) printf '%s\n' "$@" > "$FAKE_LOG/exec.argv"; exit 0 ;;
    inspect)
        case "$*" in
            *Mounts*) printf '/run/user/%s\t%s\n' "$(id -u)" "$XDG_RUNTIME_DIR/kdos/box/$last" ;;
            *ConmonPid*) echo "running $FAKE_CONMON" ;;
            *CgroupParent*) echo "" ;;
            *) echo running ;;
        esac
        exit 0 ;;
esac
exit 0
FAKE
cat > "$TW/bin/packd" <<'FAKE'
#!/usr/bin/env python3
import os, socket, sys
s = socket.socket(socket.AF_UNIX)
s.bind(sys.argv[1]); s.listen(8)
log = open(os.path.join(os.environ["FAKE_LOG"], "packd.calls"), "a")
while True:
    c, _ = s.accept()
    req = c.makefile().readline().strip()
    log.write(req + "\n"); log.flush()
    w = req.split()
    c.sendall(("ok %s/merged/%s\n" % (os.environ["FAKE_LOG"], w[1]) if w and w[0] == "compose" else "ok\n").encode())
    c.close()
FAKE
chmod +x "$TW/bin/podman" "$TW/bin/packd"
FAKE_LOG="$TW/log" "$TW/bin/packd" "$TW/packd.sock" & TWPACKD=$!
sleep 60 & FAKE_CONMON=$!
for _ in 1 2 3 4 5 6 7 8 9 10; do [ -S "$TW/packd.sock" ] && break; sleep 0.2; done
twrun() {  # <file>
    env -i PATH="$TW/bin:/usr/bin:/bin" HOME="$TW/home" XDG_RUNTIME_DIR="$TW/run" \
        XDG_DATA_HOME="$TW/home/.local/share" XDG_CONFIG_HOME="$TW/home/.config" \
        WAYLAND_DISPLAY=wayland-0 KDOS_PACKD_SOCKET="$TW/packd.sock" \
        KDOS_BOXSOCK="$TW/bin/boxsock" FAKE_LOG="$TW/log" FAKE_BOXSOCK=ok \
        FAKE_CONMON="$FAKE_CONMON" LANG=C.UTF-8 ASAN_OPTIONS="$ASAN_OPTIONS" \
        "$OUT/kdos-appbox" throwaway --pack app.viewer "$1" 2>"$TW/log/stderr"
}
twfail() { echo "  FAIL  $1"; cat "$TW/log/stderr"; kill $TWPACKD $FAKE_CONMON; exit 1; }
twrun "$TW/docs/report.pdf" || twfail "a throwaway box opened the file"
CA="$TW/log/create.argv"
TWB=$(grep -A1 -x -- '--name' "$CA" | tail -1)
case "$TWB" in app.viewer.tw[0-9]*) ;; *) twfail "the box is <launcher-id>.tw<n> ($TWB)" ;; esac
grep -qx -- '--rm' "$CA" && grep -qx 'none' "$CA" \
    && grep -qx -- "$TW/log/merged/$TWB:O" "$CA" \
    || twfail "ephemeral (--rm, a throwaway overlay) and --network none"
grep -qx "type=bind,source=$TW/docs/report.pdf,destination=$TW/docs/report.pdf,ro" "$CA" \
    && ! grep -q "$TW/docs:" "$CA" \
    || twfail "the one file, read-only, and not its folder"
grep -qx 'type=tmpfs,destination=/tmp,tmpfs-mode=1777' "$CA" \
    && ! grep -qx '/tmp:/tmp:rslave' "$CA" && ! grep -qx '/dev:/dev:rslave' "$CA" \
    && ! grep -qx -- '--pid' "$CA" && ! grep -qx -- '--ipc' "$CA" \
    || twfail "a /tmp of its own, and no host /dev, pid, ipc or network"
grep -qx "$TW/home/.local/share/kdos/boxes/$TWB/home:$TW/home:rslave" "$CA" \
    && ! grep -qx "$TW/home:$TW/home:rslave" "$CA" \
    || twfail "a private home at the home path, never the user's"
tail -1 "$TW/log/exec.argv" | grep -qx "$TW/docs/report.pdf" \
    && grep -qx "$TWB" "$TW/log/exec.argv" \
    || twfail "the application is handed the file"
grep -qx -- "-s confined $TWB app.viewer" "$TW/log/boxsock.calls" \
    || twfail "its windows take the application's launcher id"
grep -q "^rm -f -t 2 $TWB\$" "$TW/log/podman.calls" \
    && grep -qx "decompose $TWB" "$TW/log/packd.calls" \
    && test ! -e "$TW/home/.config/kdos/boxes/$TWB.conf" \
    && test ! -e "$TW/run/kdos/box/$TWB" \
    && test ! -e "$TW/home/.local/share/kdos/boxes/$TWB" \
    || twfail "the box, its profile, its runtime directory and its layer go with it"
echo "  ok    one file read-only, no network, no host /tmp or /dev, and gone at exit"
# A single-instance application already open in its own box would be handed
# the file over the bus, so the throwaway is refused while it runs.
sed -i 's/^X-KDOS-Upstream=viewer$/X-KDOS-Upstream=org.example.Viewer/' \
    "$TW/home/.local/share/applications/app.viewer.desktop"
mkdir -p "$TW/run/kdos/boxes"
printf '%s %s run app.viewer\tviewer\n' "$FAKE_CONMON" \
    "$(cut -d' ' -f22 < /proc/$FAKE_CONMON/stat)" > "$TW/run/kdos/boxes/app.viewer.apps"
: > "$TW/log/podman.calls"
if twrun "$TW/docs/report.pdf"; then twfail "a throwaway beside a running single-instance copy started"; fi
grep -q '^create' "$TW/log/podman.calls" && twfail "and nothing was created"
grep -q 'close it first' "$TW/log/stderr" || twfail "and it says why"
echo "  ok    and refused while the same application runs in its own box"
kill $TWPACKD $FAKE_CONMON; wait $TWPACKD $FAKE_CONMON 2>/dev/null || true

echo
echo "==> kdos-appbox open resolves a path to the thing that opens it"
# A boxed app's Open hands its path to `kdos-appbox open`, so a missing or
# broken subcommand breaks every one of them. The
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
echo "==> kdos cve compares pins against the vendored security database"
# Six ports and an eleven-row table, which is enough to exercise every rule
# the real 4 099-row one exercises: a pin behind two fixes, a pin that only
# LOOKS behind one because Alpine's `-rN` packaging revision is not upstream's
# version, a port whose Alpine name differs (`secdb =`), a port Alpine has
# never heard of — which must read as UNKNOWN and never as clean — and two on
# a branch: one whose CVEs its own branch's row closed although a newer
# branch's row names them too, which is clean, and one still behind its own
# branch's fix, whose CVE that only the newer branch fixed stays open.
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
if grep -q "branchpkg" "$CV"; then
    echo "  a CVE its own branch closed was reported from a newer branch's row"; cat "$CV"; exit 1
fi
grep -q "branchold .*3.6.5 .*fixed in 4.1.1 .*CVE-2026-7001,CVE-2026-7002" "$CV" \
    || { echo "  a pin behind its own branch's fix was not reported"; cat "$CV"; exit 1; }
grep -q "6 checked, 1 not in the database, 3 behind a recorded fix" "$CV" \
    || { echo "  the summary miscounted"; cat "$CV"; exit 1; }
grep -q "the database is .* days old" "$CV" \
    || { echo "  a stale database was not called stale"; exit 1; }
rc=0
PORT_REPO="$PWD/testing/fixtures/cve/ports" KPKG_CONF=/nonexistent \
    PKGDB_DIR=/dev/null KDOS_SECDB=/nonexistent-secdb "$OUT/kdos" cve \
    >/dev/null 2>&1 || rc=$?
test "$rc" = 2 \
    || { echo "  a missing database did not report itself as unrunnable"; exit 1; }
echo "  fix rows merged, -rN ignored, branches judged per CVE, 'secdb =' honoured, unknown ≠ clean"

# One port per run: a second name would silently replace the first, and the
# answer would be about a port nobody asked for the last.
rc=0
PORT_REPO="$PWD/testing/fixtures/cve/ports" KPKG_CONF=/nonexistent \
    PKGDB_DIR=/dev/null KDOS_SECDB="$PWD/testing/fixtures/cve/secdb.txt" \
    "$OUT/kdos" cve oldpkg newpkg > "$CV" 2>&1 || rc=$?
test "$rc" = 2 && grep -q "usage: kdos cve" "$CV" \
    || { echo "  two port names were not refused with usage and status 2"; cat "$CV"; exit 1; }
rc=0
PORT_REPO="$PWD/testing/fixtures/cve/ports" KPKG_CONF=/nonexistent \
    PKGDB_DIR=/dev/null KDOS_SECDB="$PWD/testing/fixtures/cve/secdb.txt" \
    "$OUT/kdos" cve --jsno > "$CV" 2>&1 || rc=$?
test "$rc" = 2 \
    || { echo "  an unknown option was not refused with status 2"; cat "$CV"; exit 1; }
PORT_REPO="$PWD/testing/fixtures/cve/ports" KPKG_CONF=/nonexistent \
    PKGDB_DIR=/dev/null KDOS_SECDB="$PWD/testing/fixtures/cve/secdb.txt" \
    "$OUT/kdos" cve newpkg > "$CV" 2>&1 \
    || { echo "  one clean port named alone did not exit 0"; cat "$CV"; exit 1; }
grep -q "1 checked" "$CV" \
    || { echo "  a named port was not the only one checked"; cat "$CV"; exit 1; }
echo "  one port name or none; a second name or an unknown option is usage and status 2"

echo
echo "==> kdos hey and kdos appid read KWin's window list, and say when there is none"
# kdos-tools speaks the Wayland wire protocol itself (windows.c), so it is
# driven against testing/fixtures/kdos-command/fakewm.py, a compositor that
# offers only
# org_kde_plasma_window_management: the bind, window_with_uuid, each window's
# state by uuid, and the requests a `run` sends back. appid matches a window to
# an entry by file ID — a NoDisplay entry is a match, because the Task
# Manager resolves a window to it all the same, and Hidden=true is a deleted
# entry that matches nothing.
_ah="$OUT/appid-home"
rm -rf "$_ah"
mkdir -p "$_ah/share/applications" "$_ah/run"
ln -sf kdos-tools "$OUT/kdos"
printf '[Desktop Entry]\nType=Application\nName=Front\nExec=true\nNoDisplay=true\n' \
    > "$_ah/share/applications/kdos-selftest-front.desktop"
printf '[Desktop Entry]\nType=Application\nName=Gone\nExec=true\nHidden=true\n' \
    > "$_ah/share/applications/kdos-selftest-gone.desktop"
printf '{u-1}\t1\t1\tkdos-selftest-front\tfront\tFront window\n{u-2}\t1\t2\tkdos-selftest-gone\tgone\tGone window\n' \
    > "$_ah/windows.tsv"
KDOS_FAKE_LOG="$_ah/log" python3 testing/fixtures/kdos-command/fakewm.py \
    "$_ah/run/wayland-kdos" "$_ah/windows.tsv" "$_ah/ready" &
_wm=$!
for _i in $(seq 50); do [ -e "$_ah/ready" ] && break; sleep 0.1; done
_wenv="XDG_RUNTIME_DIR=$_ah/run WAYLAND_DISPLAY=wayland-kdos XDG_DATA_HOME=$_ah/none XDG_DATA_DIRS=$_ah/share"
# shellcheck disable=SC2086
env $_wenv "$OUT/kdos" hey list > "$_ah/out" 2>&1 \
    || { echo "  kdos hey list failed"; cat "$_ah/out"; kill "$_wm"; exit 1; }
# The column order is what scripts read: id ws state app_id box title.
awk '$4 == "kdos-selftest-front" && $5 == "-" && $3 == "F" { ok = 1 } END { exit !ok }' "$_ah/out" \
    || { echo "  the window list is not in its columns"; cat "$_ah/out"; kill "$_wm"; exit 1; }
# shellcheck disable=SC2086
env $_wenv "$OUT/kdos" hey list --json | python3 -c '
import json, sys
w = json.load(sys.stdin)
assert [x["app_id"] for x in w] == ["kdos-selftest-front", "kdos-selftest-gone"], w
assert w[0]["active"] and w[1]["minimized"] and w[0]["box"] is None, w' \
    || { echo "  kdos hey list --json is not the windows"; kill "$_wm"; exit 1; }
# shellcheck disable=SC2086
if env $_wenv "$OUT/kdos" appid > "$_ah/out" 2>&1; then
    echo "  a Hidden entry was taken as a match"; cat "$_ah/out"; kill "$_wm"; exit 1
fi
grep -q "^kdos-selftest-front	OK	Front window$" "$_ah/out" &&
    grep -q "^kdos-selftest-gone	MISMATCH	Gone window$" "$_ah/out" \
    || { echo "  appid did not report OK and MISMATCH by file ID"; cat "$_ah/out"; kill "$_wm"; exit 1; }
# shellcheck disable=SC2086
env $_wenv "$OUT/kdos" hey run close 2 >/dev/null 2>&1 &&
    env $_wenv "$OUT/kdos" hey run minimize '{u-1}' >/dev/null 2>&1 \
    || { echo "  kdos hey run failed"; kill "$_wm"; exit 1; }
grep -qx 'close {u-2}' "$_ah/log" && grep -qx 'set_state {u-1} 2 2' "$_ah/log" \
    || { echo "  the window requests were not sent"; cat "$_ah/log"; kill "$_wm"; exit 1; }
# A box with no window is not listed; with no session at all, the answer is
# "cannot tell", which kdos-box gc must not read as "no window".
# shellcheck disable=SC2086
[ -z "$(env $_wenv PATH=/nonexistent-kdos-path "$OUT/kdos" hey boxes)" ] \
    || { echo "  a host window was named as a box"; kill "$_wm"; exit 1; }
kill "$_wm"; wait "$_wm" 2>/dev/null || true
_rc=0
# shellcheck disable=SC2086
env $_wenv WAYLAND_DISPLAY=wayland-none "$OUT/kdos" hey boxes >/dev/null 2>&1 || _rc=$?
[ "$_rc" = 1 ] || { echo "  hey boxes with no session exited $_rc, not 1"; exit 1; }
_rc=0
# shellcheck disable=SC2086
env $_wenv WAYLAND_DISPLAY=wayland-none "$OUT/kdos" appid >/dev/null 2>&1 || _rc=$?
[ "$_rc" = 2 ] || { echo "  appid with no session exited $_rc, not 2"; exit 1; }
rm -rf "$_ah"
echo "  the list, its JSON, OK and MISMATCH by file ID, the requests, and no session said as one"

echo "==> the four views draw and step the way their callers rely on"
#
# `ktui_tabs`, `ktui_table`, `ktui_dropdown` and `ktui_textarea` are the widgets
# a TUI would otherwise hand-roll. A DUMP PROVES A CHARACTER AND
# NEVER A COLOUR, so what is asserted here is what a cell grid can carry: the
# abbreviation a narrow strip falls back to, which rows a scrolled table put on
# the screen, that the selection steps OVER a heading rather than landing on
# it, and what a text block holds after a split and a join.
#
$CC $STD $WARN -o "$OUT/viewcheck" testing/fixtures/view/viewcheck.c \
    src/libs/libktui/*.c src/libs/libkcolor/*.c src/libs/libkbase/*.c \
    -Isrc/libs/libkbase -Isrc/libs/libkcolor -Isrc/libs/libktui
"$OUT/viewcheck" || exit 1

echo "==> kdos theme --audit catches what is not the palette's"
# The palette claim, checked the way the audit checks it. Outside a session
# `kdos theme` writes only the home's files, so a full themed $HOME comes from
# one run; the installed half — colour schemes, Konsole schemes and profiles,
# qt5ct palettes — is generated by kdos-theme into a data directory the audit
# searches as $XDG_DATA_DIRS, beside a global theme directory per accent. Then
# one of each KIND of drift — an edited file, a deleted file, a stray file, a
# missing installed file — must each be caught. An audit that cannot fail is
# decoration.
AH="$OUT/audit-home"
rm -rf "$AH"
mkdir -p "$AH/.config" "$AH/bin" "$AH/share"
cp fs/etc/skel/.config/starship.toml "$AH/.config/" 2>/dev/null || true
# THE SHIPPED mc CONFIGURATION IS MERGED INTO, NOT REPLACED. `kdos theme`
# writes one key into this file — mc keeps real user state in it — and a
# generator that rewrote it would silently drop every behaviour the image
# ships, in a file nobody re-reads.
mkdir -p "$AH/.config/mc"
cp fs/etc/skel/.config/mc/ini "$AH/.config/mc/ini"
cp testing/fixtures/kdos-command/kwriteconfig6 "$AH/bin/"
printf '#!/bin/sh\nexit 0\n' > "$AH/bin/kdos-power"
chmod +x "$AH/bin/kdos-power"
_have_kit=0
if [ -x "$OUT/kdos-theme" ]; then
    _have_kit=1
    ln -sf "$OUT/kdos-theme" "$AH/bin/kdos-theme"
    "$OUT/kdos-theme" kde-colors "$AH/share/color-schemes"
    "$OUT/kdos-theme" konsole "$AH/share/konsole"
    "$OUT/kdos-theme" qt5ct-colors "$AH/share/qt5ct/colors"
    for _a in $("$OUT/kdos-theme" accents); do
        mkdir -p "$AH/share/plasma/look-and-feel/org.kdos.$_a.desktop"
    done
fi
audit() {
    env -u XDG_CACHE_HOME -u XDG_DATA_HOME -u KDE_FULL_SESSION \
        HOME="$AH" XDG_CONFIG_HOME="$AH/.config" XDG_CONFIG_DIRS="$AH/xdg" \
        XDG_DATA_DIRS="$AH/share" TMPDIR="$OUT" PATH="$AH/bin:$OUT:$PATH" \
        "$OUT/kdos" theme "$@"
}
audit phosphor >/dev/null 2>&1 || { echo "  the generators did not run"; exit 1; }
if [ "$_have_kit" = 1 ]; then
    audit --audit >/dev/null || { echo "  a freshly generated \$HOME did not audit clean"; audit --audit; exit 1; }
    echo "  a generated \$HOME and a generated kit audit clean"
else
    echo "  (the installed half is skipped — no kdos-theme on this host)"
fi
# Outside a session the next login applies the accent: LookAndFeelPackage is
# written, and the accent file first.
[ "$(cat "$AH/.config/kdos/accent")" = phosphor ] \
    || { echo "  the accent file was not written"; exit 1; }
grep -q '^LookAndFeelPackage=org.kdos.phosphor.desktop$' "$AH/.config/kdeglobals" \
    || { echo "  outside a session the global theme was not named for the next login"; exit 1; }
grep -q '^color_scheme_path=/usr/share/qt5ct/colors/KDOS-Phosphor.conf$' "$AH/.config/qt5ct/qt5ct.conf" \
    || { echo "  qt5ct was not pointed at the accent's palette"; exit 1; }
# GTK's settings belong to kde-gtk-config, which follows the colour scheme;
# a second writer would race it.
for _other in gtk-3.0 gtk-4.0 qt6ct foot; do
    [ -e "$AH/.config/$_other" ] && { echo "  kdos theme writes $_other, which another program owns"; exit 1; }
done
echo "  outside a session: the accent file, the next login's theme, qt5ct, and no toolkit file the session's theme owns"

_mcini="$AH/.config/mc/ini"
[ "$(grep -c '^skin=kdos$' "$_mcini")" = 1 ] || {
    echo "  kdos theme did not leave exactly one skin=kdos in mc/ini"; exit 1; }
for _k in 'use_internal_view=1' 'use_internal_edit=0' 'confirm_exit=0' \
          '\[Layout\]' 'xterm_title=1'; do
    grep -q "^$_k\$" "$_mcini" || {
        echo "  kdos theme dropped $_k from the shipped mc/ini"; exit 1; }
done
echo "  and the shipped mc/ini keeps its keys through a theme run"

audit --preview amber >/dev/null 2>&1 \
    && { echo "  --preview is still accepted; the Global Theme page previews"; exit 1; }

sed -i 's/^/x/' "$AH/.config/btop/themes/kdos.theme"
rm -f "$AH/.config/tmux/themes/kdos.conf"
OUT_TXT=$(audit --audit || true)
echo "$OUT_TXT" | grep -q "btop.*DRIFTED.*differ" || { echo "  an edited file was not caught"; exit 1; }
echo "$OUT_TXT" | grep -q "tmux.*DRIFTED.*missing" || { echo "  a deleted file was not caught"; exit 1; }
audit --audit >/dev/null 2>&1 && { echo "  drift did not fail the exit code"; exit 1; }
audit phosphor >/dev/null 2>&1
if [ "$_have_kit" = 1 ]; then
    audit --audit >/dev/null || { echo "  re-running the accent did not repair it"; exit 1; }
    rm -f "$AH/share/konsole/KDOS-Amber.profile"
    audit --audit amber 2>/dev/null | grep -q "Konsole profile *MISSING" \
        || { echo "  a missing installed profile was not caught"; exit 1; }
    printf '[Colors:Selection]\nBackgroundNormal=0,0,0\n' > "$AH/share/color-schemes/KDOSBroken.colors"
    audit --audit >/dev/null 2>&1 \
        && { echo "  a scheme under its contrast floors audited clean"; exit 1; }
fi
echo "  an edit, a deletion, a missing installed file and a failed floor all caught"
rm -rf "$AH"

echo
echo "==> kdos theme in a session moves only what is KDOS's, and phosphor puts every key back"
# Plasma is faked on a private bus (testing/fixtures/kdos-command/fakesession.py:
# KWin's effect interface and the io.kdos.Crt1 object, a Konsole window,
# plasmashell, the notification server) and KConfig by a kwriteconfig6 that
# writes files back as KConfig does, so a byte comparison means what it says.
# The person's own Konsole default profile, an open tab on their own profile and
# a wallpaper of their own are never moved (P8); Phosphor mode's off writes
# back every key its on changed, raw, and deletes every key on found absent;
# software rendering is refused before anything is written.
if ! command -v dbus-run-session >/dev/null 2>&1 ||
   ! python3 -c 'import dbus, gi' 2>/dev/null ||
   ! { [ -e /usr/lib/libbasu.so.0 ] || ldconfig -p 2>/dev/null | grep -q 'libsystemd.so.0\|libbasu.so.0'; }; then
    echo "  (skipped — no dbus-run-session, python3-dbus or sd-bus library on this host)"
else
    SH="$OUT/session-home"
    rm -rf "$SH"
    mkdir -p "$SH/.config" "$SH/bin" "$SH/share/konsole" "$SH/xdg" "$SH/run"
    chmod 700 "$SH/run"
    cp testing/fixtures/kdos-command/kwriteconfig6 "$SH/bin/"
    for _p in plasma-apply-lookandfeel kdos-power kquitapp6 kstart; do
        printf '#!/bin/sh\necho "%s $*" >> "$KDOS_FAKE_LOG"\n' "$_p" > "$SH/bin/$_p"
        chmod +x "$SH/bin/$_p"
    done
    for _t in Bone Amber Phosphor Paper; do
        printf '[General]\nName=KDOS %s\n\n[Appearance]\nColorScheme=KDOS-%s\nFont=JetBrains Mono,10\n' \
            "$_t" "$_t" > "$SH/share/konsole/KDOS-$_t.profile"
    done
    printf '[Desktop Entry]\nDefaultProfile=KDOS-Bone.profile\n\n[LightDarkTheme]\nDarkThemeProfile=KDOS Bone\nLightThemeProfile=KDOS Paper\nSyncProfileWithSystemTheme=true\n' \
        > "$SH/xdg/konsolerc"
    printf '[General]\nfixed=JetBrains Mono,10\nfont=Noto Sans,10\n\n[KDE]\nAnimationDurationFactor=0.5\n' \
        > "$SH/.config/kdeglobals"
    printf '[Plugins]\nblurEnabled=false\n\n[Xwayland]\nScale=1\n' > "$SH/.config/kwinrc"
    printf '[Desktop Entry]\nDefaultProfile=Mine.profile\n\n[LightDarkTheme]\nDarkThemeProfile=KDOS Bone\n' \
        > "$SH/.config/konsolerc"
    printf '[Greeter][Wallpaper][org.kde.image][General]\nImage=file:///home/me/cat.png\n' \
        > "$SH/.config/kscreenlockerrc"
    cat > "$SH/run.sh" <<'SHEOF'
set -u
cd "$SH"
python3 "$FIX/fakesession.py" & FP=$!
for _i in $(seq 50); do [ -e "$SH/ready" ] && break; sleep 0.1; done
fail() { echo "  $*"; kill "$FP"; exit 1; }
# A Plasma session's switch: the global theme, the wallpaper script, the
# pairing, the Konsole tab — and none of the person's own choices.
kdos theme amber > "$SH/out" 2>&1 || fail "kdos theme amber failed: $(cat "$SH/out")"
[ "$(cat "$SH/.config/kdos/accent")" = amber ] || fail "the accent file is not amber"
grep -qx 'plasma-apply-lookandfeel -a org.kdos.amber.desktop -k' "$KDOS_FAKE_LOG" \
    || fail "the global theme was not applied with -k"
grep -q "^evaluateScript const want='file:///usr/share/wallpapers/KDOS-Amber/'" "$KDOS_FAKE_LOG" \
    || fail "the wallpaper script was not sent"
grep -q 'kscreenlockerrc' "$KDOS_FAKE_LOG" && fail "the person's own lock-screen wallpaper was replaced"
grep -qx 'DefaultProfile=Mine.profile' "$SH/.config/konsolerc" || fail "the person's own Konsole profile was replaced"
grep -qx 'DarkThemeProfile=KDOS Amber' "$SH/.config/konsolerc" || fail "the KDOS dark profile did not follow"
grep -q '^setProfile' "$KDOS_FAKE_LOG" && fail "a tab on the person's own profile was moved"
: > "$KDOS_FAKE_LOG"
cp "$SH/.config/kdeglobals" "$SH/kg.0"; cp "$SH/.config/kwinrc" "$SH/kw.0"; cp "$SH/.config/konsolerc" "$SH/kc.0"
kdos phosphor status --json | python3 -c '
import json, sys
d = json.load(sys.stdin)
assert d["on"] is False and d["supported"] is True and d["reason"] is None, d
assert d["preset"] == "subtle" and d["accent"] == "amber", d' || fail "status --json before on"
kdos phosphor on --terminus > "$SH/out" 2>&1 || fail "phosphor on failed: $(cat "$SH/out")"
tail -1 "$SH/out" | grep -qx 'phosphor mode on (subtle)' || fail "on said: $(cat "$SH/out")"
grep -qx 'kdos_crtEnabled=true' "$SH/.config/kwinrc" && grep -qx 'loginEnabled=false' "$SH/.config/kwinrc" \
    || fail "the Plugins keys were not written"
grep -q '^loadEffect kdos_crt' "$KDOS_FAKE_LOG" && grep -q '^unloadEffect login' "$KDOS_FAKE_LOG" \
    || fail "the effects were not swapped"
grep -qx 'fixed=Terminus (TTF),12' "$SH/.config/kdeglobals" || fail "the Terminus face was not set"
grep -qx 'Font=Terminus (TTF),12' "$SH/.local/share/konsole/KDOS-Terminus.profile" \
    || fail "the Terminus profile was not generated"
grep -qx 'DefaultProfile=Mine.profile' "$SH/.config/konsolerc" || fail "Terminus replaced the person's own profile"
[ "$(kdos theme)" = amber ] || fail "amber was paired; only bone and paper are"
kdos phosphor collapse || fail "collapse did not reach io.kdos.Crt1"
grep -qx 'Crt1.Collapse' "$KDOS_FAKE_LOG" || fail "Collapse was not called"
kdos phosphor off > "$SH/out" 2>&1 || fail "phosphor off failed: $(cat "$SH/out")"
cmp -s "$SH/kg.0" "$SH/.config/kdeglobals" && cmp -s "$SH/kw.0" "$SH/.config/kwinrc" &&
    cmp -s "$SH/kc.0" "$SH/.config/konsolerc" || fail "on then off did not give back kdeglobals, kwinrc and konsolerc byte for byte"
[ -e "$SH/.local/share/konsole/KDOS-Terminus.profile" ] && fail "the Terminus profile outlived off"
kdos phosphor off | grep -qx 'phosphor mode is already off' || fail "a second off was not a no-op"
# B19: bone pairs to phosphor, and off gives bone back.
kdos theme bone > /dev/null 2>&1
kdos phosphor on > /dev/null 2>&1 || fail "phosphor on from bone failed"
[ "$(kdos theme)" = phosphor ] || fail "bone was not paired to phosphor"
kdos phosphor status --json | grep -q '"previous_accent": "bone"' || fail "the previous accent was not kept"
kdos phosphor off > /dev/null 2>&1
[ "$(kdos theme)" = bone ] || fail "off did not give bone back"
# An accent picked while the mode is on is kept by off, and so are its
# Konsole profiles: the keys recorded for bone's pairing are not written
# back under it.
kdos phosphor on > /dev/null 2>&1 || fail "phosphor on from bone failed"
kdos theme ice > /dev/null 2>&1
kdos phosphor off > /dev/null 2>&1
[ "$(kdos theme)" = ice ] || fail "off replaced the accent picked while the mode was on"
grep -qx 'DarkThemeProfile=KDOS Ice' "$SH/.config/konsolerc" \
    || fail "off put the pairing's Konsole profile back under another accent"
kill "$FP"; wait "$FP" 2>/dev/null
# Software rendering: refused with the sentence, exit 3, nothing written.
rm -f "$SH/ready"
FAKE_SUPPORTED=0 python3 "$FIX/fakesession.py" & FP=$!
for _i in $(seq 50); do [ -e "$SH/ready" ] && break; sleep 0.1; done
cp "$SH/.config/kwinrc" "$SH/kw.1"; rm -f "$SH/.config/kdos/phosphor"
_rc=0; kdos phosphor on 2> "$SH/err" || _rc=$?
[ "$_rc" = 3 ] || fail "software rendering exited $_rc, not 3"
[ "$(tail -1 "$SH/err")" = 'phosphor mode needs GPU rendering (this session renders in software)' ] \
    || fail "the refusal said: $(cat "$SH/err")"
[ -e "$SH/.config/kdos/phosphor" ] && fail "a refused on wrote its state file"
cmp -s "$SH/kw.1" "$SH/.config/kwinrc" || fail "a refused on wrote kwinrc"
kill "$FP"; wait "$FP" 2>/dev/null
exit 0
SHEOF
    env -i PATH="$SH/bin:$OUT:/usr/bin:/bin" HOME="$SH" XDG_CONFIG_HOME="$SH/.config" \
        XDG_DATA_HOME="$SH/.local/share" XDG_CACHE_HOME="$SH/.cache" \
        XDG_CONFIG_DIRS="$SH/xdg" XDG_DATA_DIRS="$SH/share" XDG_RUNTIME_DIR="$SH/run" \
        KDE_FULL_SESSION=true KDOS_FAKE_LOG="$SH/log" KDOS_FAKE_READY="$SH/ready" \
        FAKE_PROFILE="My Own" SH="$SH" FIX="$PWD/testing/fixtures/kdos-command" \
        dbus-run-session -- bash "$SH/run.sh" || exit 1
    rm -rf "$SH"
    echo "  the person's own profile, tab and lock-screen wallpaper kept; on and off byte for byte; software refused"
fi

echo
echo "==> the session verbs reach Plasma's own switches, and the machine's through its services"
# The same fake session: auto-tiling and its --only, a desk laid out with its
# undo set taken first and put back by `undo`, a notification and the verbs
# that are gone, KRunner's route, the panel, Do Not Disturb and Night Light;
# and timedate1, hostname1 and locale1, which kdos-settingsd serves on the
# system bus and the fake serves on this one.
if ! command -v dbus-run-session >/dev/null 2>&1 ||
   ! python3 -c 'import dbus, gi' 2>/dev/null ||
   ! { [ -e /usr/lib/libbasu.so.0 ] || ldconfig -p 2>/dev/null | grep -q 'libsystemd.so.0\|libbasu.so.0'; }; then
    echo "  (skipped — no dbus-run-session, python3-dbus or sd-bus library on this host)"
else
    SH="$OUT/verbs-home"
    rm -rf "$SH"
    mkdir -p "$SH/.config" "$SH/bin" "$SH/run"
    chmod 700 "$SH/run"
    cp testing/fixtures/kdos-command/kwriteconfig6 "$SH/bin/"
    for _p in kquitapp6 kstart; do
        printf '#!/bin/sh\necho "%s $*" >> "$KDOS_FAKE_LOG"\n' "$_p" > "$SH/bin/$_p"
        chmod +x "$SH/bin/$_p"
    done
    printf '[Containments][1]\nplugin=org.kde.panel\n' \
        > "$SH/.config/plasma-org.kde.plasma.desktop-appletsrc"
    cat > "$SH/run.sh" <<'SHEOF'
set -u
cd "$SH"
python3 "$FIX/fakesession.py" & FP=$!
for _i in $(seq 50); do [ -e "$SH/ready" ] && break; sleep 0.1; done
fail() { echo "  $*"; kill "$FP"; exit 1; }
[ "$(kdos tiling)" = off ] || fail "tiling is not off by default"
kdos tiling on --only Code || fail "tiling on --only failed"
grep -qx 'krohnkiteEnabled=true' .config/kwinrc && grep -qx 'ignoreVDesktop=Main,Web' .config/kwinrc \
    || fail "--only did not name every other desktop"
grep -qx 'reconfigure' "$KDOS_FAKE_LOG" || fail "KWin was not told to reconfigure"
[ "$(kdos tiling toggle)" = off ] || fail "toggle did not say off"
kdos tiling status --json | grep -qx '{"tiling": false, "loaded": false}' || fail "tiling status --json"
_rc=0; kdos tiling sideways 2>/dev/null || _rc=$?; [ "$_rc" = 2 ] || fail "a bad tiling verb exited $_rc"
[ "$(kdos desk)" = kdos ] || fail "the default desk is not kdos"
_bk=$(kdos desk dock | tail -1)
[ -f "$_bk/plasma-org.kde.plasma.desktop-appletsrc" ] || fail "the panels were not backed up to $_bk"
grep -q "loadTemplate('org.kdos.desk.dock')" "$KDOS_FAKE_LOG" || fail "the dock template was not laid out"
[ "$(kdos desk)" = dock ] || fail "the desk was not recorded"
kdos desk status --json | python3 -c '
import json, sys; d = json.load(sys.stdin)
assert d["desk"] == "dock" and d["undo"] and d["panels_edited"] is False, d' || fail "desk status --json"
[ "$(kdos desk undo)" = restored ] || fail "undo did not restore"
[ "$(kdos desk)" = kdos ] || fail "undo did not give the earlier desk back"
_rc=0; kdos desk undo 2>"$SH/err" || _rc=$?
[ "$_rc" = 1 ] && grep -qx 'nothing to undo' "$SH/err" || fail "a second undo exited $_rc"
kdos notify "build done" "it took long" || fail "notify failed"
grep -qx 'Notify build done | it took long' "$KDOS_FAKE_LOG" || fail "the notification was not sent"
_rc=0; kdos notify --dismiss 2>/dev/null || _rc=$?; [ "$_rc" = 2 ] || fail "a dropped notify verb exited $_rc"
kdos menu summon setup.network && grep -qx 'querySingleRunner kdos route:setup.network' "$KDOS_FAKE_LOG" \
    || fail "the route did not reach KRunner"
kdos panel toggle && grep -q "p.hiding = " "$KDOS_FAKE_LOG" || fail "the panel was not toggled"
kdos toggle dnd on > /dev/null && grep -q '^Until=2100,' .config/plasmanotifyrc || fail "dnd on"
kdos toggle | grep -q '^dnd  *on' || fail "dnd does not read back on"
kdos toggle dnd off > /dev/null && ! grep -q '^Until=' .config/plasmanotifyrc || fail "dnd off"
kdos toggle night-light on > /dev/null && grep -qx 'invokeShortcut Toggle Night Color' "$KDOS_FAKE_LOG" \
    || fail "night light was not toggled through KWin's shortcut"
: > "$KDOS_FAKE_LOG"
kdos toggle night-light on > /dev/null; grep -q invokeShortcut "$KDOS_FAKE_LOG" && fail "an on night light was flipped off"
export DBUS_SYSTEM_BUS_ADDRESS="$DBUS_SESSION_BUS_ADDRESS"
[ "$(kdos timezone)" = UTC ] || fail "timezone did not print the zone"
kdos timezone Europe/Paris && [ "$(kdos timezone)" = Europe/Paris ] || fail "timezone was not set"
grep -qx 'SetTimezone Europe/Paris True' "$KDOS_FAKE_LOG" || fail "SetTimezone was not interactive"
_rc=0; kdos timezone nowhere 2>"$SH/err" || _rc=$?
[ "$_rc" = 1 ] && grep -q 'Invalid or not installed time zone' "$SH/err" || fail "a refused zone exited $_rc"
kdos hostname box1 && [ "$(kdos hostname)" = box1 ] || fail "hostname"
kdos locale LANG=en_GB.UTF-8 LC_TIME=en_DK.UTF-8 && [ "$(kdos locale | tr '\n' ' ')" = 'LANG=en_GB.UTF-8 LC_TIME=en_DK.UTF-8 ' ] \
    || fail "the locale list did not replace the old one whole"
_rc=0; kdos locale nonsense 2>/dev/null || _rc=$?; [ "$_rc" = 2 ] || fail "a bad assignment exited $_rc"
kdos keymap de && grep -qx 'SetVConsoleKeyboard de  True True' "$KDOS_FAKE_LOG" || fail "keymap"
kill "$FP"; wait "$FP" 2>/dev/null
exit 0
SHEOF
    env -i PATH="$SH/bin:$OUT:/usr/bin:/bin" HOME="$SH" XDG_CONFIG_HOME="$SH/.config" \
        XDG_DATA_HOME="$SH/.local/share" XDG_CACHE_HOME="$SH/.cache" \
        XDG_CONFIG_DIRS="$SH/xdg" XDG_RUNTIME_DIR="$SH/run" \
        KDOS_FAKE_LOG="$SH/log" KDOS_FAKE_READY="$SH/ready" \
        SH="$SH" FIX="$PWD/testing/fixtures/kdos-command" \
        dbus-run-session -- bash "$SH/run.sh" || exit 1
    rm -rf "$SH"
    echo "  tiling, desks and undo, notify, KRunner, the panel, the toggles, and the zone, name, locale and keymap"
fi

echo
echo "==> kdos backup run hands restic the configuration as written, and refuses a readable password"
# testing/fixtures/kdos-command/backup.conf names a repository twice (the last
# wins), an include with a space in it, and excludes that are restic patterns
# and must reach restic unescaped. restic is a stub that records its argument
# vector one word a line, and exits with $FAKE_RESTIC_RC.
BH="$OUT/backup-home"
rm -rf "$BH"
mkdir -p "$BH/.config/kdos" "$BH/bin"
cp testing/fixtures/kdos-command/backup.conf "$BH/.config/kdos/backup.conf"
printf 'secret\n' > "$BH/.config/kdos/backup.pass"
chmod 600 "$BH/.config/kdos/backup.pass"
printf '#!/bin/sh\nfor a in "$@"; do printf "%%s\\n" "$a"; done > "$HOME/restic.argv"\nexit ${FAKE_RESTIC_RC:-0}\n' \
    > "$BH/bin/restic"
chmod +x "$BH/bin/restic"
bk() { env HOME="$BH" XDG_CONFIG_HOME="$BH/.config" PATH="$BH/bin:$PATH" "$OUT/kdos" backup "$@"; }
bk run >/dev/null 2>&1 || { echo "  kdos backup run failed"; exit 1; }
printf '%s\n' -r '/srv/backup/my repo' --password-file "$BH/.config/kdos/backup.pass" \
    backup --tag kdos '/home/kdos/My Documents' /home/kdos/src \
    --exclude '*.iso' --exclude '/home/kdos/src/*/target' > "$BH/want"
cmp -s "$BH/want" "$BH/restic.argv" \
    || { echo "  restic was given another argument vector:"; diff "$BH/want" "$BH/restic.argv"; exit 1; }
_rc=0; FAKE_RESTIC_RC=12 bk run >/dev/null 2>&1 || _rc=$?
[ "$_rc" = 12 ] || { echo "  restic's own status 12 came back as $_rc"; exit 1; }
chmod 644 "$BH/.config/kdos/backup.pass"
_rc=0; bk run 2>"$BH/err" >/dev/null || _rc=$?
[ "$_rc" = 1 ] && grep -q 'backup.pass is mode 0644' "$BH/err" \
    || { echo "  a group-readable password file was not refused ($_rc)"; cat "$BH/err"; exit 1; }
chmod 600 "$BH/.config/kdos/backup.pass"
grep -v '^repo' testing/fixtures/kdos-command/backup.conf > "$BH/.config/kdos/backup.conf"
_rc=0; bk run 2>"$BH/err" >/dev/null || _rc=$?
[ "$_rc" = 1 ] && grep -q 'backup.conf names no repo' "$BH/err" \
    || { echo "  a configuration with no repo was not refused ($_rc)"; exit 1; }
_rc=0; bk sideways 2>/dev/null || _rc=$?
[ "$_rc" = 2 ] || { echo "  a bad backup verb exited $_rc"; exit 1; }
rm -rf "$BH"
echo "  the exact vector, restic's own status, and both refusals"

echo
echo "==> kdos-sfx renders the sound theme's eight files"
ln -sf kdos-tools "$OUT/kdos-sfx"
SFX="$OUT/sfx"
rm -rf "$SFX"
KDOS_NO_SFX=1 "$OUT/kdos-sfx" render "$SFX" --format wav \
    || { echo "  render failed"; exit 1; }
for _s in desktop-login desktop-logout message-new-instant dialog-error \
          dialog-warning complete device-added device-removed; do
    [ "$(head -c 4 "$SFX/$_s.wav")" = RIFF ] &&
        [ "$(dd if="$SFX/$_s.wav" bs=1 skip=8 count=4 2>/dev/null)" = WAVE ] \
        || { echo "  $_s.wav is not RIFF/WAVE"; exit 1; }
done
[ "$(ls "$SFX" | wc -l)" = 8 ] || { echo "  render wrote $(ls "$SFX" | wc -l) files, not 8"; exit 1; }
"$OUT/kdos-sfx" render "$SFX.2" --format wav
cmp -s "$SFX/complete.wav" "$SFX.2/complete.wav" || { echo "  two renders differ"; exit 1; }
_rc=0; "$OUT/kdos-sfx" render "$SFX.3" --format mp3 2>/dev/null || _rc=$?
[ "$_rc" = 2 ] || { echo "  an unknown format exited $_rc"; exit 1; }
rm -rf "$SFX" "$SFX.2"
echo "  eight RIFF/WAVE files, byte-stable, with the mute ignored"

echo
echo "==> kdos privacy names who holds the microphone, the camera and the screen"
# testing/fixtures/kdos-command/pw-dump is PipeWire's dump, faked: a muted
# microphone stream named by its client, a camera and a screen cast each read
# by one stream, and an idle stream that is no capture at all.
PH="$OUT/privacy"
rm -rf "$PH"; mkdir -p "$PH/bin"
for _p in pw-cli wpctl; do
    printf '#!/bin/sh\necho "%s $*" >> "$KDOS_FAKE_LOG"\n' "$_p" > "$PH/bin/$_p"
    chmod +x "$PH/bin/$_p"
done
pv() { env KDOS_FAKE_LOG="$PH/log" PATH="$PH/bin:$PWD/testing/fixtures/kdos-command:$PATH" "$OUT/kdos" privacy "$@"; }
pv --json | python3 -c '
import json, sys
d = json.load(sys.stdin)
assert d["version"] == 1, d
assert [(e["id"], e["app"], e["muted"], e["mutable"]) for e in d["mic"]] == [("pw:40", "Firefox", True, True)], d["mic"]
assert [(e["id"], e["app"], e["stoppable"]) for e in d["camera"]] == [("pw:50:52", "OBS Studio", True)], d["camera"]
assert [(e["id"], e["app"]) for e in d["screen"]] == [("pw:60:61", "OBS Studio")], d["screen"]' \
    || { echo "  the three lists are not what PipeWire holds"; exit 1; }
pv unmute pw:40 && pv stop pw:50:52 || { echo "  an action failed"; exit 1; }
grep -qx 'wpctl set-mute 40 0' "$PH/log" && grep -qx 'pw-cli destroy 52' "$PH/log" \
    || { echo "  the actions were not handed to wpctl and pw-cli"; cat "$PH/log"; exit 1; }
_rc=0; pv stop pw:40 2>/dev/null || _rc=$?
[ "$_rc" = 1 ] || { echo "  stopping a microphone stream exited $_rc"; exit 1; }
_rc=0; pv stop -x 2>/dev/null || _rc=$?
[ "$_rc" = 2 ] || { echo "  an id starting with - exited $_rc"; exit 1; }
rm -rf "$PH"
echo "  named by client, muted and stopped through PipeWire's own tools"

echo
echo "==> kdos places adds to KDE's Places file, and never makes one"
PL="$OUT/places"
rm -rf "$PL"; mkdir -p "$PL/data" "$PL/My Docs"
pl() { env XDG_DATA_HOME="$PL/data" "$OUT/kdos" places "$@"; }
pl add "$PL/My Docs" >/dev/null 2>&1 && { echo "  a Places file was created from nothing"; exit 1; }
[ -e "$PL/data/user-places.xbel" ] && { echo "  a refused add left a file"; exit 1; }
printf '<?xml version="1.0" encoding="UTF-8"?>\n<!DOCTYPE xbel>\n<xbel xmlns:bookmark="http://www.freedesktop.org/standards/desktop-bookmarks">\n <bookmark href="file:///home/k">\n  <title>Home</title>\n </bookmark>\n <bookmark href="trash:/">\n  <title>Trash</title>\n  <info><metadata owner="http://www.kde.org"><IsHidden>true</IsHidden></metadata></info>\n </bookmark>\n</xbel>\n' \
    > "$PL/data/user-places.xbel"
pl add "$PL/My Docs" "Docs & more" >/dev/null || { echo "  add failed"; exit 1; }
pl add "$PL/My Docs" | grep -q 'already a place' || { echo "  a second add was not refused"; exit 1; }
python3 -c 'import sys, xml.dom.minidom; xml.dom.minidom.parse(sys.argv[1])' "$PL/data/user-places.xbel" \
    || { echo "  the Places file is no longer XML"; exit 1; }
[ "$(pl | awk '{print $1}' | tr '\n' ' ')" = 'Home Docs ' ] \
    || { echo "  the list is not the visible places: $(pl)"; exit 1; }
rm -rf "$PL"
echo "  one bookmark added, escaped, once; a hidden place not listed"

echo
echo "==> kdos checklist, kdos persist --json and kdos speech sync"
CH="$OUT/checklist"
rm -rf "$CH"; mkdir -p "$CH/.config/kdos" "$CH/bin"
cp testing/fixtures/kdos-command/kwriteconfig6 "$CH/bin/"
ck() { env HOME="$CH" XDG_CONFIG_HOME="$CH/.config" XDG_CACHE_HOME="$CH/.cache" \
           XDG_DATA_HOME="$CH/.local/share" PATH="$CH/bin:$OUT:$PATH" "$OUT/kdos" "$@"; }
ck checklist --json | python3 -c '
import json, sys
d = json.load(sys.stdin)
assert d["version"] == 1 and d["items"], d
assert {i["id"] for i in d["items"]} <= {"backup", "phone", "language", "box", "firmware", "encryption"}, d
assert all(isinstance(i["done"], bool) and i["dismissed"] is False for i in d["items"]), d' \
    || { echo "  checklist --json is not the contract"; exit 1; }
ck checklist dismiss language && ck checklist --json | grep -q '"id":"language","done":[a-z]*,"dismissed":true' \
    || { echo "  a dismissal did not stick"; exit 1; }
ck checklist restore && ! ck checklist --json | grep -q '"dismissed":true' \
    || { echo "  restore left a dismissal"; exit 1; }
_rc=0; ck checklist dismiss nonsense 2>/dev/null || _rc=$?
[ "$_rc" = 2 ] || { echo "  an unknown item exited $_rc"; exit 1; }
# A Kup restic plan ticks the backup item once it names somewhere to write.
printf '#!/bin/sh\nexit 0\n' > "$CH/bin/restic"; chmod +x "$CH/bin/restic"
printf '[Kup settings]\nBackups enabled=true\nNumber of backups=1\n\n[Plan/1]\nBackup type=2\n' \
    > "$CH/.config/kuprc"
ck checklist --json | grep -q '"id":"backup","done":false' \
    || { echo "  a Kup plan with no destination ticked the backup item"; exit 1; }
printf 'External drive UUID=0a1b-2c3d\n' >> "$CH/.config/kuprc"
ck checklist --json | grep -q '"id":"backup","done":true' \
    || { echo "  a Kup restic plan on a drive did not tick the backup item"; exit 1; }
rm -f "$CH/.config/kuprc" "$CH/bin/restic"
ck persist --json | python3 -c '
import json, sys
d = json.load(sys.stdin)
assert isinstance(d["live"], bool) and d["store"] in ("none", "present", "in-use"), d' \
    || { echo "  persist --json is not the contract"; exit 1; }
# Klipper's Read Aloud action exists exactly while a voice does, and the
# person's own action is kept, byte for byte, through an add and a remove.
mkdir -p "$CH/.local/share/piper/voices"
: > "$CH/.local/share/piper/voices/v.onnx"; : > "$CH/.local/share/piper/voices/v.onnx.json"
printf '[Action_0]\nAutomatic=true\nDescription=Mine\nNumber of commands=1\nRegexp=^x\n\n[Action_0/Command_0]\nCommandline=echo %%s\nDescription=Echo\nEnabled=true\nIcon=\nOutput=0\n\n[General]\nNumber of Actions=1\n' \
    > "$CH/.config/klipperrc"
cp "$CH/.config/klipperrc" "$CH/klipperrc.0"
ck speech sync >/dev/null && grep -qx 'Commandline=kdos speech say -- %s' "$CH/.config/klipperrc" &&
    grep -qx 'Number of Actions=2' "$CH/.config/klipperrc" \
    || { echo "  Read Aloud was not added beside the person's action"; exit 1; }
ck speech sync >/dev/null && [ "$(grep -c 'Read Aloud' "$CH/.config/klipperrc")" = 2 ] \
    || { echo "  a second sync added the action twice"; exit 1; }
rm "$CH/.local/share/piper/voices/v.onnx"
ck speech sync >/dev/null && cmp -s "$CH/klipperrc.0" "$CH/.config/klipperrc" \
    || { echo "  removing the action did not give back the person's klipperrc"; exit 1; }
rm -rf "$CH"
echo "  the checklist and its dismissals, the live store, and Read Aloud with a voice only"

echo
echo "==> kdos help --keys groups the shortcuts in force as the key card does"
KH="$OUT/keys-home"
rm -rf "$KH"; mkdir -p "$KH/.config" "$KH/share/kglobalaccel"
printf '[kwin]\nWindow Close=Meta+Q\tAlt+F4,Alt+F4,Close Window\nOverview=none,Meta+W,Toggle Overview\n_k_friendly_name=KWin\n' \
    > "$KH/.config/kglobalshortcutsrc"
cp src/system/kdos-tools/kglobalaccel/org.kdos.tiling.desktop "$KH/share/kglobalaccel/"
env HOME="$KH" XDG_CONFIG_HOME="$KH/.config" XDG_CONFIG_DIRS="$KH/xdg" \
    XDG_DATA_DIRS="$KH/share" \
    KDOS_KEYCARD_GROUPS="$PWD/src/desktop/kwin-effect-kdos-keys/groups.conf" \
    "$OUT/kdos" help --keys > "$KH/out"
grep -q '^windows$' "$KH/out" && grep -q 'Meta+Q / Alt+F4 .*Close Window' "$KH/out" \
    || { echo "  a rebound KWin key is not under windows"; cat "$KH/out"; exit 1; }
grep -q '^kdos$' "$KH/out" && grep -q 'Meta+Shift+T .*Toggle Auto-Tiling' "$KH/out" \
    || { echo "  the tiling key is not under kdos"; cat "$KH/out"; exit 1; }
grep -q 'Toggle Overview' "$KH/out" && { echo "  an action bound to none was listed"; exit 1; }
rm -rf "$KH"
echo "  the person's rebinding, a KDOS service key, and no unbound action"

echo
echo "==> kdos-mpctl mpris answers for mpd on the session bus, and lets go when mpd does"
# testing/fixtures/kdos-command/fakempd.py speaks mpd's protocol, `idle`
# included. The bridge must own org.mpris.MediaPlayer2.mpd while mpd is up,
# report its state and track, carry PlayPause, Volume and Seek to mpd's own
# verbs, announce a change with PropertiesChanged, and release the name the
# moment mpd goes.
if ! command -v dbus-run-session >/dev/null 2>&1 || ! command -v gdbus >/dev/null 2>&1 ||
   ! { [ -e /usr/lib/libbasu.so.0 ] || ldconfig -p 2>/dev/null | grep -q 'libsystemd.so.0\|libbasu.so.0'; }; then
    echo "  (skipped — no dbus-run-session, gdbus or sd-bus library on this host)"
else
    MP="$OUT/mpris"
    rm -rf "$MP"; mkdir -p -m700 "$MP"
    ln -sf kdos-tools "$OUT/kdos-mpctl"
    cat > "$MP/run.sh" <<'MPEOF'
set -u
fail() { echo "  $*"; kill "$M" "$B" 2>/dev/null; exit 1; }
q() { gdbus call --session -d org.mpris.MediaPlayer2.mpd -o /org/mpris/MediaPlayer2 "$@"; }
python3 "$FIX/fakempd.py" "$XDG_RUNTIME_DIR/mpd/socket" "$XDG_RUNTIME_DIR/ready" & M=$!
for _i in $(seq 50); do [ -e "$XDG_RUNTIME_DIR/ready" ] && break; sleep 0.1; done
"$OUT/kdos-mpctl" mpris & B=$!
for _i in $(seq 50); do
    gdbus call --session -d org.freedesktop.DBus -o /org/freedesktop/DBus \
        -m org.freedesktop.DBus.NameHasOwner org.mpris.MediaPlayer2.mpd | grep -q true && break
    sleep 0.1
done
q -m org.freedesktop.DBus.Properties.GetAll org.mpris.MediaPlayer2.Player > "$XDG_RUNTIME_DIR/all" ||
    fail "GetAll failed"
grep -q "'PlaybackStatus': <'Stopped'>" "$XDG_RUNTIME_DIR/all" &&
    grep -q "'xesam:title': <'A Song'>" "$XDG_RUNTIME_DIR/all" &&
    grep -q "'mpris:trackid': <objectpath '/org/mpd/Track/7'>" "$XDG_RUNTIME_DIR/all" ||
    fail "the Player properties are not mpd's: $(cat "$XDG_RUNTIME_DIR/all")"
(timeout 4 gdbus monitor --session -d org.mpris.MediaPlayer2.mpd > "$XDG_RUNTIME_DIR/mon" 2>&1 &)
sleep 0.5
q -m org.mpris.MediaPlayer2.Player.PlayPause >/dev/null || fail "PlayPause failed"
q -m org.freedesktop.DBus.Properties.Set org.mpris.MediaPlayer2.Player Volume '<0.75>' >/dev/null ||
    fail "setting Volume failed"
q -m org.mpris.MediaPlayer2.Player.Seek -- -2000000 >/dev/null || fail "Seek failed"
sleep 1
q -m org.freedesktop.DBus.Properties.Get org.mpris.MediaPlayer2.Player PlaybackStatus |
    grep -q Playing || fail "PlayPause did not start a stopped mpd"
grep -qx 'mpd play' "$KDOS_FAKE_LOG" && grep -qx 'mpd setvol 75' "$KDOS_FAKE_LOG" &&
    grep -qx 'mpd seekcur -2.000' "$KDOS_FAKE_LOG" || fail "the calls were not mpd's verbs: $(cat "$KDOS_FAKE_LOG")"
sleep 3
grep -q PropertiesChanged "$XDG_RUNTIME_DIR/mon" || fail "no PropertiesChanged followed mpd's idle"
kill "$M"; sleep 1
gdbus call --session -d org.freedesktop.DBus -o /org/freedesktop/DBus \
    -m org.freedesktop.DBus.NameHasOwner org.mpris.MediaPlayer2.mpd | grep -q false ||
    fail "the name was kept with mpd gone"
kill "$B"; wait "$B" || fail "the bridge did not end cleanly on SIGTERM"
exit 0
MPEOF
    env -i PATH="/usr/bin:/bin" HOME="$MP" XDG_RUNTIME_DIR="$MP" OUT="$OUT" \
        KDOS_FAKE_LOG="$MP/log" FIX="$PWD/testing/fixtures/kdos-command" \
        dbus-run-session -- bash "$MP/run.sh" || exit 1
    rm -rf "$MP"
    echo "  owned while mpd runs, mpd's state and verbs, PropertiesChanged on idle, released when mpd goes"
fi

echo
echo "==> the KDOS shortcut layer reaches a person once, and never twice"
# kglobalaccel reads ~/.config/kglobalshortcutsrc alone, so the shipped layers
# reach it only through this copy; a copy that ran twice would put back a
# chord the person had reset, and one that rewrote their own value would undo
# a choice. Run under every shell an image could source it with.
SC_SCRIPT=fs/usr/share/kdos/xdg/plasma-workspace/env/kdos-shortcuts.sh
SC_KDOS=fs/usr/share/kdos/xdg
sc_seed() {
    env -i HOME="$SCH" PATH="$PATH" XDG_CONFIG_DIRS="$1" $SCSH \
        -c ". '$PWD/$SC_SCRIPT'"
}
for SCSH in sh dash "busybox sh"; do
    command -v "${SCSH%% *}" >/dev/null 2>&1 || continue
    SCH="$OUT/sc-home"
    SCU="$SCH/.config/kglobalshortcutsrc"
    SCS="$SCH/.local/state/kdos/kglobalshortcutsrc.seeded"
    rm -rf "$SCH" "$OUT/sc-etc"
    mkdir -p "$SCH" "$OUT/sc-etc"
    sc_seed "$OUT/sc-etc:$SC_KDOS"
    grep -qxF "$(printf 'Window Close=Alt+F4\\tMeta+Q,Alt+F4,Close Window')" "$SCU" ||
        { echo "  [$SCSH] a fresh account did not get Meta+Q"; exit 1; }
    [ "$(wc -l < "$SCS")" -eq "$(grep -c '^[^#[].*=' "$SC_KDOS/kglobalshortcutsrc")" ] ||
        { echo "  [$SCSH] the record does not list every shipped entry"; exit 1; }
    cp "$SCU" "$OUT/sc-first"
    sc_seed "$OUT/sc-etc:$SC_KDOS"
    cmp -s "$SCU" "$OUT/sc-first" ||
        { echo "  [$SCSH] a second login rewrote the file"; exit 1; }
    grep -vF 'Meta+Return' "$SCU" > "$SCU.x" && mv "$SCU.x" "$SCU"
    sc_seed "$OUT/sc-etc:$SC_KDOS"
    grep -qF 'Meta+Return' "$SCU" &&
        { echo "  [$SCSH] a shortcut reset to KDE's default came back"; exit 1; }
    rm -rf "$SCH"
    mkdir -p "$SCH/.config"
    printf '[kwin]\nWindow Close=Alt+F4,Alt+F4,Close Window\n\n[plasmashell]\nx=y\n' > "$SCU"
    sc_seed "$OUT/sc-etc:$SC_KDOS"
    grep -qxF 'Window Close=Alt+F4,Alt+F4,Close Window' "$SCU" ||
        { echo "  [$SCSH] a person's own chord was rewritten"; exit 1; }
    [ "$(awk '/^\[/{g=$0} /^Toggle Night Color=/{print g}' "$SCU")" = "[kwin]" ] &&
        [ "$(grep -cxF '[kwin]' "$SCU")" -eq 1 ] ||
        { echo "  [$SCSH] a missing chord was not added inside its group"; exit 1; }
    rm -rf "$SCH"
    mkdir -p "$SCH"
    printf '[services][org.kde.konsole.desktop]\n_launch=Ctrl+Alt+T\n' \
        > "$OUT/sc-etc/kglobalshortcutsrc"
    sc_seed "$OUT/sc-etc:$SC_KDOS"
    grep -qxF '_launch=Ctrl+Alt+T' "$SCU" && grep -qF 'Meta+Space' "$SCU" ||
        { echo "  [$SCSH] /etc/xdg did not win a key over the KDOS layer"; exit 1; }
    [ "$(env -i HOME="$SCH" PATH="$PATH" XDG_CONFIG_DIRS="$SC_KDOS" $SCSH \
        -c ". '$PWD/$SC_SCRIPT'; echo \"\${dst-unset}\"")" = unset ] ||
        { echo "  [$SCSH] the script leaked a variable into the session"; exit 1; }
done
rm -rf "$OUT/sc-home" "$OUT/sc-etc" "$OUT/sc-first"
echo "  seeded once, a reset stays reset, a person's chord and /etc/xdg win"

echo
echo "==> the firewall prompt asks once, about a known program, on one network"
# kdos-fwask's decisions with no bus in them: the knock-counter arithmetic,
# the process scan under a fake /proc, the answers file and the escaping of a
# network's name. Its bus half runs against fakes in the Qt suite
# (testing/plasmatest.sh), because it needs basu and a private bus.
FWA=src/desktop/kcm-kdos-firewall
$CC $WARN $STD $INC -I"$FWA/fwask" -o "$OUT/fwcoretest" \
    "$FWA/tests/fwcoretest.c" "$FWA/fwask/fwcore.c" src/libs/libkbase/*.c
mkdir -p "$OUT/fwcore"
"$OUT/fwcoretest" "$OUT/fwcore" || { echo "  fwcoretest failed"; exit 1; }
echo "  counters, process scan, answers and escaping hold"

echo
echo "==> kdos-runner answers KRunner from KDOS's files, and runs only vectors"
# The palette and the library with no bus (kdos-runner/tests/run.sh): codes,
# verbs and their slots, routes merged by name, chords, KDOS settings pages,
# packs on the boot medium, book headings, manual pages, Zeal docsets where
# this host has SQLite,
# kiwix-serve and Kolibri faked on loopback with a deadline, `ask` against a
# fake llama-server (the model it names, the question as typed, an error, a
# timeout), 50 ms per
# palette match, and routes made of `;`, `$()` and backquotes reaching stub
# programs as single arguments. The bus half (tests/bus.sh) is
# org.kde.krunner1 on a private bus, started through the shipped .service
# file, in KRunner's call order, a Run arriving after Teardown included, and
# an ask's quiet, its reply limit and its Klipper and KRunner calls.
KRU=src/desktop/kdos-runner
KRU_SRC="$KRU/query.c $KRU/sources.c $KRU/match.c $KRU/library.c $KRU/zeal.c $KRU/json.c $KRU/http.c $KRU/run.c $KRU/ask.c"
KRU_SQL=$(pkg-config --cflags --libs sqlite3 2>/dev/null || true)
KRU_BUS=""
[ -n "$TRAY_SDBUS" ] && KRU_BUS=$(pkg-config --cflags --libs "$TRAY_SDBUS")
$CC $STD $WARN -Isrc/libs/libkbase -Isrc/libs/libkxdg -Isrc/libs/libkcolor \
    -I"$KRU" -o "$OUT/kr-matchtest" "$KRU/tests/matchtest.c" $KRU_SRC \
    src/libs/libkbase/*.c src/libs/libkxdg/*.c src/libs/libkcolor/*.c \
    $KRU_SQL $KRU_BUS || { echo "  FAIL  kdos-runner's matcher does not build"; exit 1; }
bash "$KRU/tests/run.sh" "$OUT/kr-run" "$OUT/kr-matchtest" ||
    { echo "  kdos-runner's matcher broke its contract"; exit 1; }
if [ -n "$TRAY_SDBUS" ] && command -v dbus-run-session >/dev/null 2>&1 &&
   command -v dbus-send >/dev/null 2>&1 && command -v dbus-monitor >/dev/null 2>&1; then
    $CC $STD $WARN -Isrc/libs/libkbase -Isrc/libs/libkxdg -Isrc/libs/libkcolor \
        -o "$OUT/kdos-runner" "$KRU"/*.c src/libs/libkbase/*.c \
        src/libs/libkxdg/*.c src/libs/libkcolor/*.c $KRU_SQL $KRU_BUS ||
        { echo "  FAIL  kdos-runner does not build"; exit 1; }
    bash "$KRU/tests/bus.sh" "$OUT/kr-bus" "$OUT/kdos-runner" ||
        { echo "  kdos-runner broke KRunner's D-Bus contract"; exit 1; }
else
    echo "  the bus half is skipped (no sd-bus, dbus-run-session, dbus-send or dbus-monitor on this host)"
fi

echo
echo "==> kdos-notice says each change once, and blesses only a desktop that stays up"
# Facts and decisions with no bus (kdos-notice/tests/run.sh): fixture JSON
# shaped as `kdos restarts`, `kdos cve` and `kdos-bootctl status` print it,
# looked at in sequence so the state file must turn repeats into silence and
# endings into withdrawals. The bus half (tests/bus.sh) runs the daemon
# against a fake notification server, restarts that server and expects the
# waiting notice again under a new id, counts Notify on the wire with
# dbus-monitor, and runs --bless against a fake Plasma shell and a fake
# io.kdos.Slots1.
KNO=src/desktop/kdos-notice
$CC $STD $WARN -Isrc/libs/libkbase -Isrc/libs/libkxdg -I"$KNO" \
    -o "$OUT/kn-decidetest" "$KNO/tests/decidetest.c" "$KNO/json.c" \
    "$KNO/facts.c" "$KNO/decide.c" "$KNO/state.c" src/libs/libkbase/*.c \
    src/libs/libkxdg/*.c || { echo "  FAIL  kdos-notice's decisions do not build"; exit 1; }
bash "$KNO/tests/run.sh" "$OUT/kn-run" "$OUT/kn-decidetest" ||
    { echo "  kdos-notice's decisions broke their contract"; exit 1; }
if [ -n "$TRAY_SDBUS" ] && command -v dbus-run-session >/dev/null 2>&1 &&
   command -v dbus-send >/dev/null 2>&1 && command -v dbus-monitor >/dev/null 2>&1; then
    $CC $STD $WARN -Isrc/libs/libkbase -Isrc/libs/libkxdg \
        $(pkg-config --cflags "$TRAY_SDBUS") -o "$OUT/kdos-notice" "$KNO"/*.c \
        src/libs/libkbase/*.c src/libs/libkxdg/*.c \
        $(pkg-config --libs "$TRAY_SDBUS") || { echo "  FAIL  kdos-notice does not build"; exit 1; }
    $CC $STD $WARN $(pkg-config --cflags "$TRAY_SDBUS") -o "$OUT/kn-fakebus" \
        "$KNO/tests/fakebus.c" $(pkg-config --libs "$TRAY_SDBUS") ||
        { echo "  FAIL  the fakebus fixture does not build"; exit 1; }
    bash "$KNO/tests/bus.sh" "$OUT/kn-bus" "$OUT/kdos-notice" "$OUT/kn-fakebus" ||
        { echo "  kdos-notice broke its contract on the bus"; exit 1; }
else
    echo "  the bus half is skipped (no sd-bus, dbus-run-session, dbus-send or dbus-monitor on this host)"
fi

echo
echo "==> About this System's rows print two lines, at once, whatever the machine"
# kinfocenter runs each about-* script twice on its GUI thread and exits on
# fewer than two lines of output, so every row is run with no environment, in
# the English pass, on a root with no state at all and on hostile state files,
# and the shipped kcm-about-distrorc must name exactly the scripts installed.
bash src/desktop/kcm-kdos-health/tests/aboutrows.sh "$OUT/aboutrows" ||
    { echo "  an about row broke its contract"; exit 1; }

echo
echo "==> the key card places every shortcut KDOS ships, and a planted one fails"
# kdos-keycard-gen --check holds the shipped kglobalshortcutsrc, every
# kglobalaccel desktop entry and every desktop entry with X-KDE-Shortcuts
# under src/ and fs/, and every KWin ShortcutHandler and registerShortcut()
# under src/ against the key card's groups.conf ABOVE its fallbacks: a chord
# KDOS adds that only a fallback places is listed under a group by accident.
# One unplaced action planted in a copy of the shipped file, one in a QML
# file and one in a JavaScript file must each fail it, or a pass proves
# nothing. The files are listed with find, not grep --include: BusyBox grep
# has no --include, and under it the check would hold none of them.
_kc=src/desktop/kwin-effect-kdos-keys
$CC $WARN $STD -o "$OUT/kdos-keycard-gen" "$_kc/keycard-gen.c"
"$OUT/kdos-keycard-gen" --check "$_kc/groups.conf" \
    fs/usr/share/kdos/xdg/kglobalshortcutsrc \
    $( { find src fs -path '*/kglobalaccel/*.desktop'
         find src fs -type f -name '*.desktop' -exec grep -l '^X-KDE-Shortcuts=' {} +; } | sort -u) \
    $(find src -type f -name '*.qml' -exec grep -l ShortcutHandler {} + | sort) \
    $(find src -type f -name '*.js' -exec grep -l 'registerShortcut(' {} + | sort) ||
    { echo "  a shipped shortcut is placed by no rule above the fallbacks"; exit 1; }
mkdir -p "$OUT/keycard"
{ cat fs/usr/share/kdos/xdg/kglobalshortcutsrc
  printf '\n[kwin]\nPlanted Unplaced Action=Meta+Z,none,Planted\n'; } > "$OUT/keycard/planted.rc"
printf 'import org.kde.kwin\nItem {\n    KWin.ShortcutHandler {\n        name: "Planted Handler"\n    }\n}\n' \
    > "$OUT/keycard/planted.qml"
printf '// registerShortcut() in prose is not a call\nregisterShortcut("Planted Script Key", "Planted", "Meta+Z", function () {});\n' \
    > "$OUT/keycard/planted.js"
for _kp in planted.rc planted.qml planted.js; do
    _krc=0
    "$OUT/kdos-keycard-gen" --check "$_kc/groups.conf" "$OUT/keycard/$_kp" \
        2> "$OUT/keycard/err" || _krc=$?
    [ "$_krc" = 1 ] && grep -q 'Planted' "$OUT/keycard/err" ||
        { echo "  $_kp: an unplaced shortcut passed the check (exit $_krc)"
          cat "$OUT/keycard/err"; exit 1; }
done
"$OUT/kdos-keycard-gen" "$_kc/groups.conf" "$OUT/keycard/groups.js"
grep -q '^\.pragma library$' "$OUT/keycard/groups.js" &&
    grep -qF '["kdos", "^kwin$", "^KDOS .*$"],' "$OUT/keycard/groups.js" &&
    grep -qF '["kdos", "^org\\.kdos\\..*\\.desktop$", "^.*$"],' "$OUT/keycard/groups.js" ||
    { echo "  groups.js is not the table as anchored, escaped regular expressions"
      head -12 "$OUT/keycard/groups.js"; exit 1; }
echo "  every shipped chord has a group, and a planted one is refused"

echo
echo "==> the monospace gates fail the faces built to fail them"
# testing/monofont.py holds the KDOS monospace face to what a terminal shows:
# coverage with its fallbacks, one cell measured on 'M', a marked zero, 1, l
# and I apart. Its --selftest builds with fontTools a face that passes and one
# that fails each gate, and checks every verdict, so a gate that cannot fail
# fails here. fontTools is a python3 module the development image carries;
# without it the block says so.
if python3 -c 'import fontTools' 2>/dev/null; then
    _mf=$(TMPDIR="$OUT" python3 testing/monofont.py --selftest 2>&1) ||
        { printf '%s\n' "$_mf" | sed 's/^/  /'; echo "  a monofont gate misjudged its face"; exit 1; }
    printf '%s\n' "$_mf" | sed 's/^/  /'
else
    echo "  monofont gates (skipped — no fontTools on this host)"
fi

echo
echo "all good"
