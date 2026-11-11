#!/bin/bash
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   testing/preflight.sh — everything a full build would catch, minus the build
#
# `make build` takes hours and needs a container. This checks the WIRING in
# about two and a half minutes: that every package named in a phase list
# resolves, that every recipe parses, that the phase scripts are valid shell,
# that no retired path exists and nothing invokes a retired tool, and that the
# shipped rootfs carries no script whose interpreter is gone.
#
# It cannot prove the build works. It can prove the build will not fail for one
# of the dull reasons.
#
# One check is about the change rather than the tree: a .patch or .diff under
# ports/ or src/ that is new or changed — on no remote yet — must carry a
# `Subject:` line in its mail header, which the package page in KDOS from
# Scratch prints as what the patch does. The older patches without one are
# left alone until the patch itself is next added, edited or moved.
# testing/kfscheck.py holds the
# rule and this runs it; KDOS_CHECK_BASE=<ref> measures new and changed
# against <ref> instead of the remotes.

cd "$(dirname "$0")/.."

fail=0
note() { printf '  %-58s %s\n' "$1" "$2"; }
bad()  { fail=$((fail + 1)); printf '  %-58s FAIL\n      %s\n' "$1" "$2"; }

SP=$(mktemp -d)
trap 'rm -rf "$SP"' EXIT

echo "==> building kpkg for the checks"
cc -O2 -std=gnu11 -D_GNU_SOURCE -Wall -Wextra \
   -Isrc/libs/libkbase -Isrc/libs/libkpkg -Isrc/libs/libksig \
   -Isrc/system/kdos-kpkg \
   -o "$SP/kdos-kpkg" src/system/kdos-kpkg/*.c src/libs/libkbase/*.c \
   src/libs/libkpkg/*.c src/libs/libksig/*.c \
   src/libs/libksig/monocypher/*.c || { echo "  cannot build kpkg"; exit 1; }
# Installed as five names and dispatched on its own basename, so the checks
# have to invoke it the same way — `kpkg kpkgdepends ...` correctly reaches
# kpkg's front end and prints usage, which is not what is being tested.
for n in kpkg kpkgadd kpkgbuild kpkgdel kpkgdepends; do
    ln -sf kdos-kpkg "$SP/$n"
done

# Every port repository any phase searches: the desktop phase's PORT_REPO
# (script/phases/50_desktop/phase.env) is the widest, and it names all five.
# This has to match, or preflight reports ports that build fine as missing.
export PORT_REPO="$PWD/ports/core $PWD/src/system $PWD/src/art $PWD/src/desktop $PWD/src/daemons"
export KPKG_CONF=/nonexistent PKGDB_DIR=/dev/null

# EVERY PORT, WALKED ONCE AND FOUND BY NAME. ports/core files each upstream
# port one shelf down, ports/core/<shelf>/<name>/, and ours sit at
# src/<area>/<name>/. A port is named by its directory alone, so every check
# below asks by name: a path spelled with a shelf goes stale the day the port
# changes shelf, and a flat glob over ports/core finds shelves, sees no
# kpkgbuild in any of them and passes having checked nothing.
#
# THE WALK IS ports/srclib.sh's src_port_dirs, the one ports/fetch and
# ports/publish use, so the set checked here is the set they fetch and
# archive. It takes a port directly under a repository as well as one shelf
# down; the layout checks below refuse the first under ports/core, and this
# walk makes sure a loose port is still checked in every other way until then.
#
# PDIR maps a name to its directory and PREPO to the repository root that
# carries it (ports/core for every shelf). A name found twice keeps its first
# directory here and is reported by the layout checks below: kpkg refuses a
# name two shelves of one repository carry, and resolves a name two
# repositories carry to the first, so either has to fail before a build asks
# for it.
# shellcheck source=ports/srclib.sh
. ports/srclib.sh
declare -A PDIR PREPO
PORTS_ALL=()
PDUP=()
for _r in ports/core src/*; do
    [ -d "$_r" ] || continue
    while IFS= read -r _d; do
        _n=${_d##*/}
        if [ -n "${PDIR[$_n]:-}" ]; then
            PDUP+=("port $_n is in two places: ${PDIR[$_n]}/kpkgbuild and $_d/kpkgbuild")
            continue
        fi
        PDIR[$_n]=$_d
        PREPO[$_n]=$_r
        PORTS_ALL+=("$_d")
    done < <(src_port_dirs "$_r")
done
has_port() { [ -n "${PDIR[$1]:-}" ]; }

# A phase names its ports in packages.txt, or in packages.d/*.txt read in
# byte order as one list; never both, which the layout checks enforce. A glob
# sorts by the locale's collation, which is not byte order in every locale, so
# this one runs under C and reads the files in the order the build does.
# Prints the list files of one phase directory.
phase_lists() {
    local LC_ALL=C
    if [ -f "$1/packages.txt" ]; then
        printf '%s\n' "$1/packages.txt"
    elif [ -d "$1/packages.d" ]; then
        for _l in "$1"/packages.d/*.txt; do
            [ -f "$_l" ] && printf '%s\n' "$_l"
        done
    fi
}
PHASE_LISTS=$(for _ph in script/phases/*/; do phase_lists "${_ph%/}"; done)

echo
echo "==> the ports tree is shelved, and every name is one port"
# THE LAYOUT IS WHAT EVERY WALKER ASSUMES, and none of them says so when it is
# wrong. A recipe three levels down is a port kpkg does not resolve and
# `make fetch` never fetches; one at ports/core/<name>/ is one kpkg builds
# and script/lib/port.sh's port_dir never finds; a shelf missing from
# ports/shelves is one nobody placed a port on by its rules; a shelf named
# `libs` turns `<portdir>/../../libs` — the tree a source-less port hashes —
# into a real directory; a `name =` that differs from its directory is a
# package the orphan sweep deletes from the image. Each is silent everywhere
# else.
#
# script/hooks/pre-push holds the same rules for the four it can read out of a
# commit — the depth, the listed shelf, the one name and the shelf's own name —
# and says each in the words used here, so either answer names the same fault.
_lay=0
while IFS= read -r _k; do
    case "$_k" in
        ports/core/*/*/kpkgbuild)
            case "${_k#ports/core/*/*/}" in */*)
                bad "$_k" "not at ports/core/<shelf>/<name>/kpkgbuild"
                _lay=$((_lay + 1)) ;;
            esac ;;
        *)  bad "$_k" "not at ports/core/<shelf>/<name>/kpkgbuild"
            _lay=$((_lay + 1)) ;;
    esac
done < <(find ports/core -name kpkgbuild 2>/dev/null | LC_ALL=C sort)
for _e in ports/core/* ports/core/.[!.]*; do
    [ -e "$_e" ] || continue
    [ -d "$_e" ] && [ ! -L "$_e" ] && continue
    bad "$_e" "ports/core holds shelves only"
    _lay=$((_lay + 1))
done

# The closed shelf list: `<id> <volume> <description>` per line.
declare -A SHELF
if [ -z "$(src_shelves)" ]; then
    bad "ports/shelves" "ports/shelves is missing or lists no shelf"
    _lay=$((_lay + 1))
else
    while read -r _s _vol _rest || [ -n "$_s" ]; do
        case "$_s" in ''|\#*) continue ;; esac
        if [ -n "${SHELF[$_s]:-}" ]; then
            bad "ports/shelves" "lists '$_s' twice"
            _lay=$((_lay + 1))
        fi
        SHELF[$_s]=1
        # A leading `-` is refused too: a shelf is a path every tool is handed
        # as an argument.
        if ! printf '%s' "$_s" | grep -qxE '[a-z0-9][a-z0-9-]*'; then
            bad "ports/shelves" "shelf $_s: an id is lowercase letters, digits and -"
            _lay=$((_lay + 1))
        fi
        # The volume is the source-archive release the shelf's files go to;
        # ports/publish cannot place a file of a shelf without one.
        if ! printf '%s' "$_vol" | grep -qxE '[1-9][0-9]*'; then
            bad "ports/shelves" "shelf $_s has no volume (field 2 is a positive integer; make publish-plan assigns one)"
            _lay=$((_lay + 1))
            case "$_vol" in -) ;; *) _rest="$_vol${_rest:+ $_rest}" ;; esac
        fi
        [ -n "$_rest" ] || { bad "ports/shelves" "shelf $_s has no description"; _lay=$((_lay + 1)); }
        # <portdir>/../../libs is the tree a source-less port hashes.
        case "$_s" in libs|core)
            bad "ports/shelves" "shelf $_s: the name is reserved"
            _lay=$((_lay + 1)) ;;
        esac
        if has_port "$_s"; then
            bad "ports/shelves" "shelf $_s shares its name with the port at ${PDIR[$_s]}/kpkgbuild"
            _lay=$((_lay + 1))
        fi
        if [ ! -d "ports/core/$_s" ]; then
            bad "ports/shelves" "lists '$_s', which is not a directory under ports/core"
            _lay=$((_lay + 1))
        elif ! compgen -G "ports/core/$_s/*/kpkgbuild" >/dev/null; then
            bad "ports/core/$_s" "a listed shelf with no port on it"
            _lay=$((_lay + 1))
        fi
    done < ports/shelves
    for _e in ports/core/*/; do
        _s=${_e%/}; _s=${_s##*/}
        [ -d "$_e" ] || continue
        [ -n "${SHELF[$_s]:-}" ] && continue
        bad "ports/core/$_s" "shelf $_s is not listed in ports/shelves"
        _lay=$((_lay + 1))
    done
fi

# src/ holds exactly its six areas, and a port of ours sits exactly at
# src/<area>/<name>/: `$PORT_SRC/../../libs` is how 17 recipes find the
# libraries, and it resolves at no other depth.
for _e in src/*; do
    case "${_e#src/}" in libs|system|art|desktop|daemons|devtools) continue ;; esac
    bad "$_e" "src/ holds libs, system, art, desktop, daemons and devtools only"
    _lay=$((_lay + 1))
done
for _a in libs system art desktop daemons devtools; do
    [ -d "src/$_a" ] || { bad "src/$_a" "missing"; _lay=$((_lay + 1)); }
done
while IFS= read -r _k; do
    case "${_k#src/*/*/}" in kpkgbuild) continue ;; esac
    bad "$_k" "a recipe not at src/<area>/<name>/kpkgbuild — ../../libs does not resolve from it"
    _lay=$((_lay + 1))
done < <(find src -name kpkgbuild 2>/dev/null | LC_ALL=C sort)

# The orphan sweep deletes every installed package whose port it cannot find,
# and REPOS is where it looks: an area missing there has every one of its
# packages removed from the image.
_orph=script/phases/70_image/040_orphans.sh
_orepos=" $(sed -n 's/^REPOS="\(.*\)"$/\1/p' "$_orph" 2>/dev/null) "
for _a in $(for _k in src/*/*/kpkgbuild; do [ -f "$_k" ] && echo "${_k#src/}"; done |
            cut -d/ -f1 | sort -u); do
    case "$_orepos" in *" /kdos/src/$_a "*) continue ;; esac
    bad "$_orph" "REPOS does not name /kdos/src/$_a — the sweep deletes its packages from the image"
    _lay=$((_lay + 1))
done

# One name, one port, across every shelf and every src area.
for _m in "${PDUP[@]}"; do
    bad "port names" "$_m"
    _lay=$((_lay + 1))
done

# `name =` is the directory's name; nothing else enforces it. And a `group =`
# family moves as one: ports/update bumps every member together, so they share
# a shelf. The KDE release sets are the exception: `group = plasma` and
# `group = gear` join applications filed by what each one does (spectacle on
# graphics, konsole on shells) into the one bump their release needs, and the
# release-set check below holds them to one version instead. One awk over every
# recipe answers both — a process per recipe is half a minute of forks.
declare -A GSHELF
while IFS='	' read -r _d _nm _g; do
    if [ "$_nm" != "${_d##*/}" ]; then
        bad "$_d" "name = '$_nm' is not its directory's name"
        _lay=$((_lay + 1))
    fi
    [ -n "$_g" ] || continue
    case "$_d" in ports/core/*) ;; *) continue ;; esac
    _s=${_d#ports/core/}; _s=${_s%%/*}
    case " ${GSHELF[$_g]:-} " in *" $_s "*) ;; *) GSHELF[$_g]="${GSHELF[$_g]:-} $_s" ;; esac
done < <(for _d in "${PORTS_ALL[@]}"; do printf '%s/kpkgbuild\n' "$_d"; done |
         xargs awk '
             FNR == 1 { if (f != "") print f "\t" n "\t" g; f = FILENAME; n = ""; g = "" }
             /^name[[:blank:]]*=/  && n == "" { n = $0; sub(/^name[[:blank:]]*=[[:blank:]]*/, "", n) }
             /^group[[:blank:]]*=/ && g == "" { g = $0; sub(/^group[[:blank:]]*=[[:blank:]]*/, "", g) }
             END { if (f != "") print f "\t" n "\t" g }' |
         sed 's|/kpkgbuild\t|\t|')

for _g in "${!GSHELF[@]}"; do
    case "$_g" in plasma|gear) continue ;; esac
    set -- ${GSHELF[$_g]}
    [ "$#" -le 1 ] && continue
    bad "group $_g" "its members sit on several shelves:${GSHELF[$_g]}"
    _lay=$((_lay + 1))
done

# A phase names its ports one way. In packages.d/ a file is one shelf's ports,
# named <shelf>.txt, or one src area's, src-<area>.txt, or 00-order.txt: the
# runs a comment pins ahead of every shelf, whatever shelf they sit on. The
# build reads only *.txt there, so any other file is a list nothing installs;
# and a port filed under a shelf it is not on is one the next reader looks for
# in the wrong place.
for _ph in script/phases/*/; do
    _ph=${_ph%/}
    if [ -f "$_ph/packages.txt" ] && [ -e "$_ph/packages.d" ]; then
        bad "$_ph" "has packages.txt and packages.d — a phase uses one or the other"
        _lay=$((_lay + 1))
        continue
    fi
    [ -d "$_ph/packages.d" ] || continue
    if ! compgen -G "$_ph/packages.d/*.txt" >/dev/null; then
        bad "$_ph" "packages.d holds no .txt list"
        _lay=$((_lay + 1))
    fi
    for _l in "$_ph"/packages.d/* "$_ph"/packages.d/.[!.]*; do
        [ -e "$_l" ] || continue
        _b=${_l##*/}
        case "$_b" in
            00-order.txt) continue ;;
            src-*.txt)
                _a=${_b#src-}; _a=${_a%.txt}
                case "$_a" in system|art|desktop|daemons) ;; *)
                    bad "$_l" "src-$_a is not a src area that holds ports"
                    _lay=$((_lay + 1)); continue ;;
                esac ;;
            *.txt)
                _a=${_b%.txt}
                if [ -z "${SHELF[$_a]:-}" ]; then
                    bad "$_l" "$_a is not a shelf ports/shelves lists"
                    _lay=$((_lay + 1)); continue
                fi ;;
            *)  bad "$_l" "not a .txt list — the build never reads it"
                _lay=$((_lay + 1)); continue ;;
        esac
        _off=""
        while read -r _p || [ -n "$_p" ]; do
            case "$_p" in ''|\#*) continue ;; esac
            has_port "$_p" || continue
            case "$_b" in
                src-*) [ "${PREPO[$_p]}" = "src/$_a" ] ;;
                *)     [ "${PDIR[$_p]}" = "ports/core/$_a/$_p" ] ;;
            esac || _off="$_off $_p"
        done < "$_l"
        [ -z "$_off" ] || { bad "$_l" "names ports filed elsewhere:$_off"; _lay=$((_lay + 1)); }
    done
done
[ "$_lay" = 0 ] && note "ports layout" "${#PORTS_ALL[@]} ports on ${#SHELF[@]} shelves and in src/, every name one port"

echo
echo "==> each package phase installs exactly the ports its list names"
# A GRANULAR PHASE MEANS SOMETHING ONLY IF ITS LIST IS ITS CLOSURE. For each
# package phase in order, the dependency closure of its list, minus what every
# earlier phase installed, has to equal the list itself: a port an earlier
# phase starts to need moves there in the list rather than being pulled in
# silently, and a list that reaches forward fails here rather than hours into
# the phase that builds too much. testing/phaseclosure.py computes it and
# names each port that differs with both of its phases.
if [ ! -f testing/phaseclosure.py ]; then
    bad "phase closure" "testing/phaseclosure.py is missing — nothing checks a phase against its list"
elif python3 testing/phaseclosure.py > "$SP/closure" 2>&1; then
    note "phase closure" "every package phase installs exactly its list"
else
    bad "phase closure" "$(grep -c . "$SP/closure") line(s), first: $(head -3 "$SP/closure" | tr '\n' ' ')"
fi

echo
echo "==> every package named in a phase list has a port"
for f in $PHASE_LISTS; do
    missing=""
    # `|| [ -n "$p" ]`: a list whose last line has no newline still names a
    # package the build installs, and a plain `read` would drop it — this gate
    # would then pass a phase whose last entry has no port.
    while read -r p || [ -n "$p" ]; do
        [ -z "$p" ] && continue
        case "$p" in \#*) continue ;; esac
        has_port "$p" || missing="$missing $p"
    done < "$f"
    [ -z "$missing" ] && note "$f" "ok" || bad "$f" "no port for:$missing"
done

echo
echo
echo "==> ports built from one tarball agree on its version"
# Each pair fetches the same upstream archive, and each recipe carries its own
# copy of the version and hash. A bump of one without the other does not fail
# the build. It ships two halves of different releases: perf reports unknown
# record types for every event the running kernel added after its source;
# python3-tkinter builds _tkinter against another release's libpython; clang,
# lld and the other LLVM parts link a different LLVM. The skew is caught here
# instead.
shared=0
for pair in "linux perf" "python3 python3-tkinter" "gettext libintl" \
            "glib glib-introspection" "webkitgtk webkitgtk6" "qca qca-qt5" \
            "qscintilla python3-qscintilla" "qwt qwt-qt5" "mgba libretro-mgba" \
            "llvm clang" "llvm lld" "llvm lldb" "llvm compiler-rt" \
            "llvm libunwind" "llvm openmp" "llvm libclc" \
            "util-linux util-linux-login" "tlp tlp-pd"; do
    set -- $pair
    a=$1; b=$2
    has_port "$a" && has_port "$b" || continue
    av=$(sed -n 's/^version[[:blank:]]*=[[:blank:]]*//p' "${PDIR[$a]}/kpkgbuild" 2>/dev/null | head -1)
    bv=$(sed -n 's/^version[[:blank:]]*=[[:blank:]]*//p' "${PDIR[$b]}/kpkgbuild" 2>/dev/null | head -1)
    [ -z "$av" ] || [ -z "$bv" ] && continue
    ah=$(sed -n 's/^sha256[[:blank:]]*=[[:blank:]]*\([0-9a-f]*\).*/\1/p' "${PDIR[$a]}/kpkgbuild" | head -1)
    bh=$(sed -n 's/^sha256[[:blank:]]*=[[:blank:]]*\([0-9a-f]*\).*/\1/p' "${PDIR[$b]}/kpkgbuild" | head -1)
    if [ "$av" != "$bv" ]; then
        bad "$a/$b" "versions differ: $a is $av, $b is $bv"
    elif [ "$ah" != "$bh" ]; then
        bad "$a/$b" "same version $av but different sha256 — not the same tarball"
    else
        note "$a and $b" "both $av, same tarball"
    fi
    shared=$((shared + 1))
done
[ "$shared" = 0 ] && note "shared-tarball ports" "none declared"
echo
echo "==> the KDE release set is one version"
# KDE releases Plasma, Frameworks and Gear each as one set built and tested
# together: Plasma's modules require each other at their own version, every
# Gear application is tested against the Gear libraries of its release, and a
# Framework built from another month's release breaks the ABI the rest were
# built against. A family bump is one change that rebuilds the family, and a
# tree holding two releases of one family is refused here, naming each.
#
# A port belongs to a family by its source: download.kde.org's stable/plasma,
# stable/frameworks or stable/release-service, the release directory the
# tarball comes from. `group = plasma` (or `gear`) places a port whose source
# is elsewhere, and the kf6 shelf is the Frameworks whatever their source. A
# Plasma or Gear release is its first three version components and a
# Frameworks release its first two, so a respin (6.8.2.1, 6.31.1) is part of
# the release it fixes; Frameworks of another major version (sonnet-qt5's
# KF5) are a family of their own. Plasma's own side releases — plasma-wayland-protocols,
# kirigami-addons — come from directories of their own and belong to none.
#
# It proves itself on testing/fixtures/releaseset first: the set passes, and
# each copy with one port moved to another release fails.
kde_families() {
    awk '
        function flush(  fam, p, k, rel) {
            if (f == "" || v == "") return
            fam = ""
            if (src ~ /kde\.org\/(stable|unstable)\/plasma\// || g == "plasma") fam = "plasma"
            else if (src ~ /kde\.org\/(stable|unstable)\/frameworks\// || shelf == "kf6") fam = "kf"
            else if (src ~ /kde\.org\/(stable|unstable)\/release-service\// || g == "gear") fam = "gear"
            if (fam == "") return
            k = split(v, p, ".")
            if (fam == "kf") fam = "kf" p[1]
            rel = (fam ~ /^kf/) ? p[1] "." p[2] : p[1] "." p[2] "." p[3]
            print fam "\t" rel "\t" n "\t" v
        }
        FNR == 1 { flush(); f = FILENAME; n = v = g = src = ""; m = split(FILENAME, q, "/"); shelf = q[m - 2] }
        /^name[[:blank:]]*=/    && n == "" { n = $0; sub(/^name[[:blank:]]*=[[:blank:]]*/, "", n) }
        /^version[[:blank:]]*=/ && v == "" { v = $0; sub(/^version[[:blank:]]*=[[:blank:]]*/, "", v) }
        /^group[[:blank:]]*=/   && g == "" { g = $0; sub(/^group[[:blank:]]*=[[:blank:]]*/, "", g) }
        /^source[[:blank:]]*=/             { s = $0; sub(/^source[[:blank:]]*=[[:blank:]]*/, "", s); src = src " " s }
        END { flush() }' "$@"
}
release_sets() {
    kde_families "$@" |
    awk -F'\t' '
        { key = $1 FS $2; c[key]++; fam[$1]; if (c[key] <= 6) who[key] = who[key] " " $3 "-" $4 }
        END {
            for (k in c) {
                split(k, a, FS)
                sets[a[1]]++
                if (c[k] > most[a[1]]) most[a[1]] = c[k]
            }
            for (f in fam) {
                line = ""
                for (k in c) {
                    split(k, a, FS)
                    if (a[1] != f) continue
                    line = line "; " c[k] " at " a[2]
                    if (sets[f] > 1 && c[k] < most[f] || sets[f] > 1 && c[k] <= 6)
                        line = line " (" substr(who[k], 2) (c[k] > 6 ? " and " c[k] - 6 " more" : "") ")"
                }
                print (sets[f] > 1 ? "BAD" : "OK") "\t" f "\t" substr(line, 3)
            }
        }' | LC_ALL=C sort
}
_rsfix=0
release_sets $(find testing/fixtures/releaseset/set -name kpkgbuild | LC_ALL=C sort) > "$SP/rsset"
grep -q '^BAD' "$SP/rsset" && { bad "release set" "the fixture's one-release set fails: $(grep '^BAD' "$SP/rsset" | cut -f2- | tr '\t\n' ': ')"; _rsfix=1; }
[ "$(grep -c '^OK' "$SP/rsset")" = 3 ] || { bad "release set" "the fixture's set is not read as three families"; _rsfix=1; }
for _m in "plasma kde/kwin 6.8.3" "kf6 kf6/kcoreaddons 6.32.0" "gear files/dolphin 26.08.3"; do
    set -- $_m
    rm -rf "$SP/rsbump"
    cp -r testing/fixtures/releaseset/set "$SP/rsbump"
    sed -i "s/^version[[:blank:]]*=.*/version     = $3/" "$SP/rsbump/$2/kpkgbuild"
    release_sets $(find "$SP/rsbump" -name kpkgbuild | LC_ALL=C sort) > "$SP/rsbumped"
    [ "$(grep -c '^BAD' "$SP/rsbumped")" = 1 ] && grep -q "^BAD	$1	" "$SP/rsbumped" ||
        { bad "release set" "moving $2 alone to $3 is not refused as a split $1 release"; _rsfix=1; }
done
release_sets $(for _d in "${PORTS_ALL[@]}"; do case "$_d" in ports/core/*) printf '%s/kpkgbuild\n' "$_d" ;; esac; done) > "$SP/rsreal"
while IFS='	' read -r _k _f _why; do
    case "$_k" in
        BAD) bad "release set" "$_f is more than one release: $_why" ;;
        OK)  [ "$_rsfix" = 0 ] && note "release set" "$_f: $_why" ;;
    esac
done < "$SP/rsreal"
[ -s "$SP/rsreal" ] || note "release set" "no KDE ports"

echo
echo "==> every Gear application is in exactly one kde-gear meta"
# The meta ports are KDE as `kpkg install` gives it to a machine, and the phase
# lists only build it: a Gear application no kde-gear-* meta names is left out
# of `kpkg install kde-gear`, and one two metas name moves with whichever is
# removed first. A library arrives through some application's closure and is
# named by none, so the rule is for the Gear ports no port depends on but the
# metas (kde-all, kde-plasma, kde-gear and kde-gear-*). A port is Gear by the
# family rule above. Each meta also stays under the depends limit below.
#
# It proves itself on testing/fixtures/gearmeta first: the set passes, a copy
# with one game dropped from its meta fails it as named by none, and a copy
# with one game in two metas fails it as named by two.
gear_metas() {
    awk '
        function flush() {
            if (n == "") return
            dep[n] = d
            if (src ~ /kde\.org\/(stable|unstable)\/release-service\// || g == "gear") gear[n] = 1
        }
        FNR == 1 { flush(); n = d = g = src = ""; seen = 0 }
        /^name[[:blank:]]*=/    && n == "" { n = $0; sub(/^name[[:blank:]]*=[[:blank:]]*/, "", n) }
        /^group[[:blank:]]*=/   && g == "" { g = $0; sub(/^group[[:blank:]]*=[[:blank:]]*/, "", g) }
        /^source[[:blank:]]*=/             { s = $0; sub(/^source[[:blank:]]*=[[:blank:]]*/, "", s); src = src " " s }
        /^depends[[:blank:]]*=/ && !seen   { seen = 1; d = $0; sub(/^depends[[:blank:]]*=[[:blank:]]*/, "", d) }
        END {
            flush()
            for (p in dep) {
                k = split(dep[p], w, / +/)
                if (p ~ /^kde-gear-/) {
                    metas++
                    for (i = 1; i <= k; i++) if (w[i] != "") { cnt[w[i]]++; by[w[i]] = by[w[i]] " " p }
                } else if (p !~ /^kde-(all|plasma|gear)$/) {
                    for (i = 1; i <= k; i++) if (w[i] != "") needed[w[i]] = 1
                }
            }
            if (!metas) { print "NONE"; exit }
            for (p in gear) {
                if (p in needed || cnt[p] == 1) continue
                if (!cnt[p]) print "BAD\t" p "\tis a Gear application no kde-gear-* meta names"
                else print "BAD\t" p "\tis named by " cnt[p] " metas:" by[p]
            }
            print "OK\t" metas
        }' "$@" | LC_ALL=C sort
}
_gmfix=0
gear_metas $(find testing/fixtures/gearmeta/set -name kpkgbuild | LC_ALL=C sort) > "$SP/gmset"
if grep -q '^BAD' "$SP/gmset" || ! grep -q '^OK	2$' "$SP/gmset"; then
    bad "gear metas" "the fixture's set is misjudged: $(grep -v '^OK' "$SP/gmset" | cut -f2- | tr '\t\n' ': ')"
    _gmfix=1
fi
for _m in "kapman kde/kde-gear-games bovo" "bovo kde/kde-gear-core dolphin-plugins bovo"; do
    set -- $_m
    _gp=$1 _gr=$2
    shift 2
    rm -rf "$SP/gmbump"
    cp -r testing/fixtures/gearmeta/set "$SP/gmbump"
    sed -i "s/^depends[[:blank:]]*=.*/depends     = $*/" "$SP/gmbump/$_gr/kpkgbuild"
    gear_metas $(find "$SP/gmbump" -name kpkgbuild | LC_ALL=C sort) > "$SP/gmbumped"
    [ "$(grep -c '^BAD' "$SP/gmbumped")" = 1 ] && grep -q "^BAD	$_gp	" "$SP/gmbumped" ||
        { bad "gear metas" "$_gr naming '$*' is not refused for $_gp"; _gmfix=1; }
done
gear_metas $(for _d in "${PORTS_ALL[@]}"; do printf '%s/kpkgbuild\n' "$_d"; done) > "$SP/gmreal"
if grep -q '^NONE' "$SP/gmreal"; then
    [ "$_gmfix" = 0 ] && note "gear metas" "skipped — no kde-gear-* meta ports yet; fixture judged right"
else
    _gm=0
    while IFS='	' read -r _k _p _why; do
        [ "$_k" = BAD ] || continue
        bad "gear metas" "$_p $_why"
        _gm=$((_gm + 1))
    done < "$SP/gmreal"
    [ "$_gm" = 0 ] && [ "$_gmfix" = 0 ] &&
        note "gear metas" "every Gear application is in one of $(awk -F'\t' '$1 == "OK" { print $2 }' "$SP/gmreal") metas"
fi

echo
echo "==> every phase list resolves to a dependency order"
# Each phase resolves with its own phase.env's PORT_REPO (/ports/core, the
# one entry of kpkg.conf's default the chroot has, when it sets none), and kpkgdepends passes an unknown name
# through as if it were a port. A dependency outside the phase's repos
# therefore resolves here and fails the build with no port found. A phase
# with packages.d/ is one list, its files read in sorted order.
for ph in script/phases/*/; do
    ph=${ph%/}
    f=$(phase_lists "$ph")
    [ -n "$f" ] || continue
    pkgs=$(cat $f | grep -v '^#' | grep -v '^$' | tr '\n' ' ')
    [ -z "$pkgs" ] && continue
    f=$(printf '%s\n' "$f" | head -1)
    case "$f" in */packages.d/*) f=$ph/packages.d ;; esac
    repo=$(sed -n 's/^export PORT_REPO="\(.*\)"$/\1/p' "$ph/phase.env" 2>/dev/null)
    repo=${repo:-/ports/core}
    repo=$(printf '%s' "$repo" | sed "s|/kdos/src/|$PWD/src/|g; s|^/ports/core|$PWD/ports/core|")
    out=$(PORT_REPO="$repo" "$SP/kpkgdepends" $pkgs 2>"$SP/err")
    miss=
    for t in $out; do
        h=
        for r in $repo; do
            [ -n "${PREPO[$t]:-}" ] && [ "$r" = "$PWD/${PREPO[$t]}" ] && { h=1; break; }
        done
        [ -z "$h" ] && { miss=$t; break; }
    done
    if [ -s "$SP/err" ]; then
        bad "$f" "kpkgdepends wrote to stderr: $(head -1 "$SP/err")"
    elif [ -z "$out" ]; then
        bad "$f" "kpkgdepends returned nothing"
    elif printf '%s' "$out" | tr ' ' '\n' | grep -qvE '^[A-Za-z0-9][A-Za-z0-9._+-]*$'; then
        bad "$f" "a resolved token is not a package name"
    elif [ -n "$miss" ]; then
        bad "$f" "resolves $miss, which this phase's PORT_REPO does not carry"
    else
        note "$f" "$(echo "$out" | wc -w) packages"
    fi
done

echo
echo "==> every depends key names a port that exists"
orphans=0
for d in "${PORTS_ALL[@]}"; do
    for dep in $(sed -n 's/^depends[[:blank:]]*=[[:blank:]]*//p' "$d/kpkgbuild"); do
        if ! has_port "$dep"; then
            bad "$(basename "$d")" "depends on '$dep', which has no port"
            orphans=$((orphans + 1))
        fi
    done
done
[ "$orphans" = 0 ] && note "all depends resolve" "ok"
echo
echo "==> every recipe's depends fits the limit kpkg reads"
# kpkg refuses a recipe whose `depends =` names more than KP_MAX_DEPS ports
# ("<port>: depends names N ports; the limit is N"), so a build stops at the
# first port that reaches it; this refuses it before a build starts, in the
# same words. Names are counted the way kp_depends counts them: the first
# depends line, split on spaces. The limit is read out of kpkg.h, so the two
# cannot disagree, and a recipe one past it, written here, must fail first.
_dmax=$(sed -n 's/^#define KP_MAX_DEPS[[:blank:]]*\([0-9][0-9]*\).*/\1/p' src/libs/libkpkg/kpkg.h)
_dlawk='
        FNR == 1 { done = 0 }
        !done && /^depends[[:blank:]]*=/ {
            done = 1
            s = $0
            sub(/^depends[[:blank:]]*=[[:blank:]]*/, "", s)
            n = 0
            k = split(s, w, / +/)
            for (i = 1; i <= k; i++) if (w[i] != "") n++
            if (n > max) { d = FILENAME; sub(/\/kpkgbuild$/, "", d); sub(/.*\//, "", d); print d "\t" n }
        }'
deps_over() { awk -v max="$_dmax" "$_dlawk" "$@"; }
if [ -z "$_dmax" ]; then
    bad "depends limit" "KP_MAX_DEPS is not readable out of src/libs/libkpkg/kpkg.h"
else
    mkdir -p "$SP/deplimit/wide"
    { printf 'name = wide\nversion = 1\nrelease = 1\ndepends ='
      seq 0 "$_dmax" | sed 's/^/ d/' | tr -d '\n'; echo; } > "$SP/deplimit/wide/kpkgbuild"
    if [ "$(deps_over "$SP/deplimit/wide/kpkgbuild")" != "wide	$((_dmax + 1))" ]; then
        bad "depends limit" "a recipe naming $((_dmax + 1)) depends is not counted as over the limit"
    else
        _dl=0
        while IFS='	' read -r _p _n; do
            bad "$_p" "depends names $_n ports; the limit is $_dmax"
            _dl=$((_dl + 1))
        done < <(for _d in "${PORTS_ALL[@]}"; do printf '%s/kpkgbuild\n' "$_d"; done |
                 xargs awk -v max="$_dmax" "$_dlawk")
        [ "$_dl" = 0 ] && note "depends limit" "every recipe names at most $_dmax"
    fi
fi

echo
echo "==> every port of OURS is built by something"
# The reverse of the check above, and the one a NEW port needs. `ports/core` is
# upstream and a recipe there may legitimately sit unbuilt; every port under
# src/ is ours, and a port nobody installs is a directory that compiles on a
# developer's machine and is absent from the ISO. kdos-oomd is the live
# example: a daemon, an init script and a recipe, and one missing line in
# script/phases/50_desktop/packages.txt between it and never running.
#
# A port a PHASE SCRIPT builds by name is fine — that is how kinstall is built,
# in the bootstrap phase, long before any phase list is read.
unbuilt=0
for d in src/*/*; do
    [ -f "$d/kpkgbuild" ] || continue
    p=$(basename "$d")
    [ -n "$PHASE_LISTS" ] && grep -qxF "$p" $PHASE_LISTS 2>/dev/null && continue
    grep -rqlF "$p" script/phases/*/*.sh 2>/dev/null && continue
    bad "$p" "in no phase list and named by no phase script — nothing builds it"
    unbuilt=$((unbuilt + 1))
done
[ "$unbuilt" = 0 ] && note "our ports are all built" "ok"

echo
echo "==> every KDOS daemon an init script starts is installed by a port"
# `supervise` is given an absolute path and the script SKIPS quietly when it is
# not there ("[SKIP] foo: /usr/sbin/foo not found"), which is right on a live
# system and useless here: a daemon whose port never installs it is a service
# that silently never runs. Only the kdos-* ones are checked — an upstream
# daemon's path comes out of somebody else's `make install` and no grep over
# this tree can see it.
daemons=0
for f in fs/etc/init.d/*.sh; do
    [ -f "$f" ] || continue
    # Read from a file, not a pipe: `bad` in a pipeline's subshell increments
    # a copy of $fail and the check reports without failing.
    sed -n 's/^[[:space:]]*DAEMON="\([^"]*\)".*/\1/p' "$f" > "$SP/daemons"
    while read -r dpath || [ -n "$dpath" ]; do
        case "$(basename "$dpath")" in kdos-*) ;; *) continue ;; esac
        if ! grep -rqF -- "$dpath\"" ports/core/*/*/build.sh src/*/*/build.sh \
                2>/dev/null; then
            bad "$(basename "$f")" "starts $dpath, which no build.sh installs"
        fi
    done < "$SP/daemons"
    daemons=$((daemons + 1))
done
note "init.d services" "$daemons checked"

echo
echo "==> every init.d script is executable"
# rcS skips a script that is not executable (`[ -x "$_script" ] || continue`)
# and says nothing, and init gets EACCES on one an inittab entry names
# directly (killpower, killprocs). 000_file_system.sh replays the execute bit
# from the work tree, and git's one recorded bit is what a checkout gives the
# work tree, so a file committed 100644 is a service that never runs.
# service_helper is sourced, never executed.
noexec=0
for f in fs/etc/init.d/*; do
    [ -f "$f" ] || continue
    [ "$(basename "$f")" = service_helper ] && continue
    if [ ! -x "$f" ]; then
        bad "$(basename "$f")" "is not executable; rcS and init will not run it"
        noexec=$((noexec + 1))
    fi
done
# The work tree's bit is this clone's; the index's is what the next commit,
# and so every other clone, gets. Outside a checkout there is no index to read.
while read -r mode _ _ path; do
    [ "$(basename "$path")" = service_helper ] && continue
    if [ "$mode" != 100755 ]; then
        bad "$(basename "$path")" "is recorded in git as $mode, not 100755; a clone will not run it"
        noexec=$((noexec + 1))
    fi
done < <(git ls-files -s -- fs/etc/init.d 2>/dev/null)
[ "$noexec" = 0 ] && note "init.d scripts" "all executable"

# THE FIRST SOURCE IS THE RECIPE'S FIRST, NOT THE DIRECTORY'S ALPHABETICAL
# FIRST — kpkg strips a component from that one and no other. libime ships a
# dictionary tarball that sorts before its own, so a glob picks the wrong
# archive and reports a shape the build never sees. Mirrors kp_source_file() in
# build.c, the same way the sha256 check above does.
first_source_file() {
    unset name version source
    eval "$("$SP/kpkg" meta "$1" 2>/dev/null)"
    for s in $source; do
        case "$s" in
            *"::"*) printf '%s\n' "${s%%::*}"; return ;;
            *://*)  base=${s##*/} ;;
            *)      printf '%s\n' "$s"; return ;;
        esac
        case "$base" in
            *.tar.gz|*.tgz)   base="$name-$version.tar.gz" ;;
            *.tar.bz2|*.tbz2) base="$name-$version.tar.bz2" ;;
            *.tar.xz|*.txz)   base="$name-$version.tar.xz" ;;
            *.tar.zst)        base="$name-$version.tar.zst" ;;
            *.zip)            base="$name-$version.zip" ;;
        esac
        printf '%s\n' "$base"; return
    done
}

echo
echo "==> a first source whose members are ./-prefixed is accounted for"
# kpkg extracts the FIRST source into $SRC with --strip-components=1, which
# removes one path component. When a tarball's members are written `./dir/…`
# — GNU tar does this for `tar -c ./dir`, and HDF5's release is built that way
# — the component removed is the DOT, so the tree lands at $SRC/<dir> instead
# of at $SRC. The build then reports whatever it could not find at the top
# level (`does not appear to contain CMakeLists.txt`), which points at the
# wrong thing entirely. One port in 656 is like this; the check exists so the
# second one costs a preflight run rather than a build.
dotp=0
for d in "${PORTS_ALL[@]}"; do
    [ -f "$d/build.sh" ] || continue
    t="$d/$(first_source_file "$d")"
    case "$t" in *.tar.*|*.tgz|*.tbz2|*.txz) ;; *) continue ;; esac
    [ -f "$t" ] || continue
    # A lone `./` entry says nothing about the members: Mozilla's tarballs
    # open with one and write every member after it as `firefox-<v>/…`, which
    # the strip unpacks correctly. The first member other than the dot decides.
    first=$(tar tf "$t" 2>/dev/null | awk '$0 != "./" && $0 != "." { print; exit }')
    case "$first" in
    ./*)
        dotp=$((dotp + 1))
        # ONLY A SINGLE WRAPPER IS THE BUG. `./dir/…` with one directory under
        # the dot loses the dot and lands at $SRC/<dir>, one level too deep.
        # `./a ./b ./c` — a FLAT archive that merely carries the dot — loses
        # the same dot and lands exactly right, so demanding a `cd` there
        # would be demanding a `cd` into nothing. Counting the entries under
        # the dot is what separates them.
        _tops=$(tar tf "$t" 2>/dev/null | sed 's|^\./||' \
                | awk -F/ 'NF && $1 != "" { print $1 }' | sort -u | head -5)
        _ntop=$(printf '%s\n' "$_tops" | grep -c .)
        if [ "$_ntop" -eq 1 ]; then
            # Accounted for means the recipe descends into the directory the
            # strip left behind. Anything else is the silent one-level-down
            # failure.
            grep -qE '^[[:space:]]*cd[[:space:]]+"?[A-Za-z0-9_.-]*\$\{?(name|version)' "$d/build.sh" \
                || bad "$(basename "$d")" "first source is ./-prefixed with one wrapping directory and build.sh never cds into it"
        fi
        ;;
    esac
done
note "./-prefixed sources" "$dotp found, each accounted for"

echo
echo "==> a FLAT first source is unpacked by its own recipe"
# The mirror of the check above, and the worse of the two. kpkg strips one
# component from the first source unconditionally; on an archive with NO
# wrapping directory that removes every TOP-LEVEL FILE outright and promotes
# every subdirectory's contents into its place — yosys lost its Makefile and
# got `docs/Makefile` in the same breath, failing on a target that Makefile
# does not have. tzdata is why the rule is already written down; yosys is why
# it is now checked. A recipe accounts for it by unpacking the tarball itself.
flatp=0
for d in "${PORTS_ALL[@]}"; do
    [ -f "$d/build.sh" ] || continue
    t="$d/$(first_source_file "$d")"
    case "$t" in *.tar.*|*.tgz|*.tbz2|*.txz) ;; *) continue ;; esac
    [ -f "$t" ] || continue
    # The dot of a lone `./` entry is not a top-level entry of its own.
    tops=$(tar tf "$t" 2>/dev/null | head -300 | awk -F/ '(NF>1 || $1!="") && $1!="." {print $1}' | sort -u | wc -l)
    [ "$tops" -le 1 ] && continue
    flatp=$((flatp + 1))
    grep -qE '^[[:space:]]*tar x[a-z]* +"?\$(PORT_SRC|\{PORT_SRC\})' "$d/build.sh" \
        || bad "$(basename "$d")" "first source is FLAT ($tops top-level entries) and build.sh never re-unpacks it"
done
note "flat first sources" "$flatp found, each unpacked by its recipe"

echo
echo "==> every meson -D a recipe passes is an option that port defines"
# MESON FAILS AT SETUP ON AN UNKNOWN OPTION, before a line is compiled — and
# there is no universal spelling, so `-Dtests=disabled` is right for one port
# and fatal for the next. Three found this the slow way (libkiwix, lxi-tools,
# mpd), each an hour-long round trip through a container. The authority is the
# tarball's own meson_options.txt / meson.options; a BUILT-IN option (werror,
# default_library, b_*, and the rest meson defines for every project) is
# always valid and is not in that file, so the known set is listed here.
#
# A port whose tarball is absent is SKIPPED rather than failed. Upstream
# sources are not in git; `make fetch` puts them in the port directories, so
# the case this covers is a clone that has not been fetched yet, where there is
# no meson_options.txt to read and every option would be reported unknown.
meson_checked=0
meson_builtin="auto_features backend b_asneeded b_colorout b_coverage b_lto \
b_lundef b_ndebug b_pch b_pgo b_sanitize b_staticpic b_vscrt buildtype \
debug default_both_libraries default_library errorlogs install_umask \
layout optimization pkg_config_path prefer_static strip unity unity_size \
warning_level werror wrap_mode"
for d in "${PORTS_ALL[@]}"; do
    [ -f "$d/build.sh" ] || continue
    grep -q 'meson setup' "$d/build.sh" || continue
    # EVERY tarball the port ships, not the first: a port with two sources
    # (pipewire carries media-session beside it) would otherwise be checked
    # against the wrong project's options and fail on all of its own.
    set -- "$d"/*.tar.*
    [ -e "$1" ] || continue
    defined=""
    deftypes=""
    for t in "$@"; do
        # BOTH LAYOUTS, because a first source is not always wrapped in a
        # directory. `*/meson_options.txt` alone misses a FLAT tarball's
        # top-level copy — plocate's is one — and the check then reads no
        # options at all and reports every -D the recipe passes as undefined.
        # A check that fails loudest on the ports it understands least is
        # worse than no check.
        flat=$(tar -xOf "$t" --wildcards \
                   '*/meson_options.txt' '*/meson.options' \
                   'meson_options.txt' 'meson.options' \
               2>/dev/null | tr '\n' ' ' || true)
        defined="$defined
$(printf '%s' "$flat" | grep -oE "option[[:space:]]*\([[:space:]]*'[a-zA-Z0-9_-]+" \
          | sed "s/.*'//" || true)"
        deftypes="$deftypes
$(printf '%s' "$flat" \
          | grep -oE "option[[:space:]]*\([[:space:]]*'[a-zA-Z0-9_-]+'[[:space:]]*,[[:space:]]*type[[:space:]]*:[[:space:]]*'[a-z]+'" \
          | sed -E "s/option[[:space:]]*\([[:space:]]*'([a-zA-Z0-9_-]+)'.*'([a-z]+)'\$/\\1\t\\2/" || true)"
    done
    # No options file at all means the port defines none; every -D it is
    # handed then has to be a built-in, which the same comparison covers.
    known=$(printf '%s\n%s\n' "$defined" "$(echo $meson_builtin | tr ' ' '\n')" | sort -u)
    # A subproject-scoped option (-Dsubproj:opt) belongs to a project whose
    # option file is not here, so it is not something this check can answer.
    # COMMENT LINES ARE NOT COMMAND LINES. A recipe routinely NAMES a flag in
    # prose — libraqm's explains why matplotlib passes `-Dsystem-libraqm=true`
    # — and reading that as something this port passes reports a defect in the
    # port that documented the fix. `install -Dm644` is not a meson option
    # either, and `option(` may be followed by a NEWLINE before its name, so
    # the option file is flattened before it is read; `option (` with a
    # space, which Impression writes, is the same call.
    #
    # A COMPILER FLAG IS NOT A MESON OPTION EITHER. `-D` names a preprocessor
    # macro in a CFLAGS assignment and a project option on a meson line, and
    # meson never sees the first: mesa-demos exports `-D_GNU_SOURCE` for a
    # header its sources need, which no meson_options.txt can define. A line
    # that assigns one of the toolchain flag variables is therefore dropped
    # before the -D's are read, or this reports a defect in a recipe that
    # builds. Nor is a -D a recipe searches for: `grep -q -- -DHAVE_AVAHI
    # build/build.ninja` asserts a probe's macro landed, and names no option.
    cmdlines=$(grep -v '^[[:space:]]*#' "$d/build.sh" | grep -v 'install ' \
               | grep -vE '(^|[[:space:]!(])grep[[:space:]]' \
               | grep -vE '^[[:space:]]*(export[[:space:]]+)?(C|CXX|CPP|LD|OBJC|OBJCXX|F|FC)FLAGS\+?=')
    passed=$(printf '%s\n' "$cmdlines" \
             | grep -oE '[-]D[a-zA-Z0-9_-]+[a-zA-Z0-9_:-]*' \
             | sed 's/^-D//' | grep -v ':' | sort -u)
    valued=$(printf '%s\n' "$cmdlines" \
             | grep -oE "[-]D[a-zA-Z0-9_-]+=[^ 	'\"]+" \
             | sed 's/^-D//' | grep -v ':' | sort -u)
    for opt in $passed; do
        printf '%s\n' "$known" | grep -qx "$opt" && continue
        bad "$(basename "$d")" "passes -D$opt, which its meson_options.txt does not define"
    done

    # AND THE VALUE HAS TO SUIT THE TYPE. meson refuses a boolean for a
    # `feature` outright — `Value "false" for option "gd" is not one of the
    # choices` — and that is a configure-time error two hours into a phase, on
    # a line that reads perfectly. Only the two types with a CLOSED, universal
    # value set are checked: a combo's choices are the project's own and a
    # string's are anything. An option whose `type:` does not immediately
    # follow its name records no type and is left alone — unknown is not wrong.
    for pair in $valued; do
        opt=${pair%%=*}; val=${pair#*=}
        case "$val" in *'$'*) continue ;; esac
        ty=$(printf '%s\n' "$deftypes" | grep -m1 "^$opt	" | cut -f2)
        case "$ty" in
            feature)
                case "$val" in enabled|disabled|auto) ;; *)
                    bad "$(basename "$d")" "passes -D$opt=$val, but $opt is a meson 'feature' (enabled/disabled/auto)" ;;
                esac ;;
            boolean)
                case "$val" in true|false) ;; *)
                    bad "$(basename "$d")" "passes -D$opt=$val, but $opt is a meson 'boolean' (true/false)" ;;
                esac ;;
        esac
    done
    meson_checked=$((meson_checked + 1))
done
note "meson options" "$meson_checked meson ports checked against their own option files"

echo
echo "==> every meson port names its buildtype"
# kpkg strips nothing, so the compiler flags decide what a package carries.
# meson's default buildtype is `debug`, which compiles -g -O0 into every
# object; a recipe that names none ships its DWARF and an unoptimised build.
bt_checked=0
for d in "${PORTS_ALL[@]}"; do
    [ -f "$d/build.sh" ] || continue
    grep -v '^[[:space:]]*#' "$d/build.sh" | grep -q 'meson setup' || continue
    grep -v '^[[:space:]]*#' "$d/build.sh" | grep -qE -- '--buildtype[= ]|-Dbuildtype=' \
        || bad "$(basename "$d")" "runs meson setup without --buildtype= or -Dbuildtype="
    bt_checked=$((bt_checked + 1))
done
note "meson buildtype" "$bt_checked meson ports checked"

echo
echo "==> every go build and go install strips its binary"
# The Go linker writes DWARF and a symbol table unless -ldflags carries -s and
# -w, and kpkg strips nothing, so the binary ships at about twice its size.
# A command is read whole, its backslash continuations joined, because the
# flags routinely sit on a later line than `go build`. GOFLAGS is not read: a
# recipe's own -ldflags replaces the one GOFLAGS names.
go_checked=0
for d in "${PORTS_ALL[@]}"; do
    [ -f "$d/build.sh" ] || continue
    grep -qE '(^|[[:space:];&|(])go[[:space:]]+(build|install)' "$d/build.sh" || continue
    while IFS= read -r line; do
        go_checked=$((go_checked + 1))
        fl=$(printf '%s\n' "$line" \
             | grep -oE -- "-ldflags[= ](\"[^\"]*\"|'[^']*'|[^[:space:]]+)" | head -1 \
             | sed -E "s/^-ldflags[= ]//; s/^[\"']//; s/[\"']\$//")
        printf ' %s ' "$fl" | grep -qE '[[:space:]]-s[[:space:]]' \
            && printf ' %s ' "$fl" | grep -qE '[[:space:]]-w[[:space:]]' && continue
        bad "$(basename "$d")" "go build/install without -ldflags \"-s -w\": ${line:0:100}"
    done < <(awk '
        { if (acc != "") { line = acc " " $0 } else { line = $0 } }
        /\\$/ { sub(/\\$/, "", line); acc = line; next }
        { acc = "" }
        line ~ /^[[:space:]]*#/ { next }
        line ~ /(^|[[:space:];&|(])go[[:space:]]+(build|install)([[:space:]]|$)/ { print line }
    ' "$d/build.sh")
done
note "go strip flags" "$go_checked go build/install commands checked"

echo
echo "==> every pip install into \$PKG ignores what the build root has installed"
# pip uninstalls from the build root any distribution it finds there before
# writing the new copy into --root, so a rebuilt port deletes its own files,
# and any file another package also claims, from the live tree. A command is
# read whole, as the go check reads it.
pip_checked=0
for d in "${PORTS_ALL[@]}"; do
    [ -f "$d/build.sh" ] || continue
    grep -qE '(pip3?|-m pip)[[:space:]]+install' "$d/build.sh" || continue
    while IFS= read -r line; do
        pip_checked=$((pip_checked + 1))
        case "$line" in *--ignore-installed*) continue ;; esac
        bad "$(basename "$d")" "pip install --root without --ignore-installed: ${line:0:100}"
    done < <(awk '
        { if (acc != "") { line = acc " " $0 } else { line = $0 } }
        /\\$/ { sub(/\\$/, "", line); acc = line; next }
        { acc = "" }
        line ~ /^[[:space:]]*#/ { next }
        line ~ /(pip3?|-m pip)[[:space:]]+install/ && line ~ /--root/ { print line }
    ' "$d/build.sh")
done
note "pip ignore-installed" "$pip_checked pip installs into a package root checked"

#
# WHAT AN ARCHIVE IS, READ OUT OF THE FILE RATHER THAN ASKED OF `file`.
#
# file(1)'s answer depends on which magic database the machine has, and the
# suite is meant to pass inside kdos-devdeps: the SAME file-5.46 calls the
# 95 MB Noto CJK zip "Zip archive data" on a development host and "data" in
# that container, so a check asking `file` would report a good archive as
# broken in the one place it has to be right. A signature is six bytes and no
# database.
#
# Every suffix the caller matches is covered. A plain uncompressed tar carries
# no leading signature at all — `ustar` sits at offset 257 of the first header
# block — which is why that arm is a seek rather than a prefix.
archive_magic() {
    case "$(od -An -N6 -tx1 "$1" 2>/dev/null | tr -d ' \n')" in
    1f8b*)                          return 0 ;;   # gzip, and .tgz
    425a68*)                        return 0 ;;   # bzip2
    fd377a585a00*)                  return 0 ;;   # xz
    28b52ffd*)                      return 0 ;;   # zstd
    4c5a4950*)                      return 0 ;;   # lzip
    504b0304*|504b0506*|504b0708*)  return 0 ;;   # zip: normal, empty, spanned
    esac
    [ "$(dd if="$1" bs=1 skip=257 count=5 2>/dev/null)" = "ustar" ]
}

echo
echo "==> every source a port declares is hashed, and on disk once fetched"
# kpkg refuses to extract a source it has no hash for, so a gap here is a
# port that cannot build. The enumeration is the RECIPE's own source list
# read through the same parser the build uses, NOT a glob of archive
# extensions: a glob knows only the suffixes it lists, and a plain-file
# source (ca-certificates', iana-etc's) is invisible to it and fails instead
# hours into the foundation phase.
#
# A HASHED SOURCE THAT IS NOT ON DISK IS UNFETCHED, NOT BROKEN. Upstream
# sources are not in git: `make fetch` resolves each `sha256 =` entry from the
# local cache, the kunaldawn/kdos archive or upstream, so a fresh clone
# has none of them and must still pass here. They are counted and reported
# with the command that supplies them; whether each one is in the archive is
# what `ports/publish --check` and the pre-push hook answer.
unhashed=0
unfetched=0
stale=""
for d in "${PORTS_ALL[@]}"; do
    p=$(basename "$d")
    unset name version source vendoring
    eval "$("$SP/kpkg" meta "$d" 2>/dev/null)"
    idx=0
    resolved=""
    for s in $source; do
        # kp_source_file() in build.c, and this must stay its mirror: an
        # explicit `<name>::<url>` is taken LITERALLY and is not renamed, a
        # non-URL is its own name, a URL is its basename, and only the FIRST
        # source is renamed to <name>-<version> when it carries an archive
        # suffix. The `::` case has to be tested before the URL one — the
        # right-hand side contains `://`, so a plain URL match claims it and
        # then derives a filename the recipe deliberately overrode.
        explicit=0
        case "$s" in
            *"::"*) base=${s%%::*}; explicit=1 ;;
            *://*)  base=${s##*/} ;;
            *)      base=$s ;;
        esac
        if [ "$idx" = 0 ] && [ "$explicit" = 0 ]; then
            case "$base" in
                *.tar.gz|*.tgz)   base="$name-$version.tar.gz" ;;
                *.tar.bz2|*.tbz2) base="$name-$version.tar.bz2" ;;
                *.tar.xz|*.txz)   base="$name-$version.tar.xz" ;;
                *.tar.zst)        base="$name-$version.tar.zst" ;;
                *.zip)            base="$name-$version.zip" ;;
            esac
        fi
        idx=$((idx + 1))
        resolved="$resolved $base"
        # A DECLARED SOURCE WITH NO HASH AND NO FILE IS A BUILD THAT DIES AT
        # THE UNPACK. `make build` runs with no network, and `make fetch`
        # takes nothing from the archive for a name the recipe does not hash,
        # so a name nothing provides is reported here rather than at whatever
        # hour of the build its phase reaches that package. Every port is
        # judged, not only the ones a phase list names: a port wired into no
        # phase yet is exactly the one whose source was never hashed.
        if [ ! -f "$d/$base" ]; then
            if grep -q "^sha256[[:blank:]]*=.*[[:blank:]]$base\$" "$d/kpkgbuild"; then
                unfetched=$((unfetched + 1))
            else
                bad "$p" "declares $base, which is neither in the port directory nor hashed"
                unhashed=$((unhashed + 1))
            fi
            continue
        fi
        # AN EMPTY ARCHIVE IS NOT AN ARCHIVE, and a hash does not catch it:
        # sha256 of nothing is a stable digest, so a recipe written while the
        # download was empty verifies clean everywhere and fails only when tar
        # is handed the file, hours into a build.
        if [ ! -s "$d/$base" ]; then
            bad "$p" "ships $base as an empty file"
            unhashed=$((unhashed + 1))
        elif ! grep -q "^sha256[[:blank:]]*=.*[[:blank:]]$base\$" "$d/kpkgbuild"; then
            bad "$p" "ships $base with no sha256 line"
            unhashed=$((unhashed + 1))
        # AND A HASH DOES NOT PROVE IT IS AN ARCHIVE. A mirror that answers a
        # download with a 502 page writes an HTML file under the tarball's
        # name; hashing THAT and recording the result gives a recipe that
        # verifies perfectly and dies at `tar: Error is not recoverable`
        # minutes into a phase — with a checksum line that looks deliberate.
        # Real, on lzip, whose recorded sha256 was the hash of a Savannah
        # error page.
        else
            # ONLY A NAME THAT CLAIMS TO BE AN ARCHIVE IS JUDGED AS ONE. A
            # `source =` list may legitimately carry a bare .c or a .patch —
            # netcat's does — and those are not archives and must not be
            # reported as broken ones.
            case "$base" in
            *.tar.*|*.tgz|*.tbz2|*.txz|*.zip)
                if ! archive_magic "$d/$base"; then
                    bad "$p" "ships $base, whose first bytes are none of gzip, bzip2, xz, zstd, lzip, zip or tar"
                    unhashed=$((unhashed + 1))
                fi
                ;;
            esac
        fi
    done

    # AN ARCHIVE THE RECIPE CANNOT NAME IS ONE THE BUILD WILL NOT FIND. The
    # loop above judges the names the recipe resolves to; a file sitting in
    # the port directory under some OTHER name is claimed by none of them and
    # fails at `Source not found`, minutes into a phase. That is what a
    # hand-placed download looks like: kpkg renames a FIRST source to
    # <name>-<version>.<ext> and a `.tgz` saved under the URL's own suffix
    # matches nothing. An archive with its own sha256 line is declared rather
    # than unclaimed: build.sh unpacks it out of $PORT_SRC the way it unpacks a
    # vendor bundle (bat's bat-assets bundle), and kpkg verifies every
    # sha256 entry, not only the ones a source names.
    #
    # AN UNCLAIMED ARCHIVE GIT IGNORES IS A STALE FETCH, NOT A DEFECT. Fetched
    # archives are untracked, so a checkout or pull that moves a port to
    # another version leaves the old one in the port directory, and nothing
    # the build reads names it. `make fetch` removes each one that is a hard
    # link into ports/.srccache; what is left here is a copy or a file with no
    # other link, the only one on this machine. It is counted and reported, never failed:
    # failing it would make every branch switch fail preflight. Only a
    # TRACKED unclaimed archive, or one outside a checkout where git cannot
    # say, is the hand-placed download this check exists for.
    for f in "$d"/*.tar.* "$d"/*.tgz "$d"/*.tbz2 "$d"/*.txz "$d"/*.zip; do
        [ -f "$f" ] || continue
        fb=${f##*/}
        case " $resolved " in *" $fb "*) continue ;; esac
        [ "$fb" = "$name-vendor-$version.tar.xz" ] && continue
        grep -q "^sha256[[:blank:]]*=.*[[:blank:]]$fb\$" "$d/kpkgbuild" && continue
        if ! git ls-files --error-unmatch -- "$f" >/dev/null 2>&1 \
           && git check-ignore -q -- "$f" 2>/dev/null; then
            stale="$stale $p/$fb"
            continue
        fi
        bad "$p" "ships $fb, which no 'source =' line resolves to"
        unhashed=$((unhashed + 1))
    done

    # A VENDOR BUNDLE IS A SOURCE THIS PORT BUILDS FROM AND IS NOT IN `source`.
    # `ports/fetch` generates it from `vendoring =`, build.sh untars it out of
    # $PORT_SRC, and the loop above enumerates the recipe's own source list —
    # so a bundle with no sha256 line beside it is invisible here and verified
    # by nothing, anywhere.
    # An absent bundle WITH its sha256 line is unfetched, like any other
    # hashed source; without one, nothing can fetch or verify it.
    if [ -n "${vendoring:-}" ]; then
        vf="$name-vendor-$version.tar.xz"
        if [ ! -f "$d/$vf" ]; then
            if grep -q "^sha256[[:blank:]]*=.*[[:blank:]]$vf\$" "$d/kpkgbuild"; then
                unfetched=$((unfetched + 1))
            else
                bad "$p" "declares vendoring=$vendoring and neither ships nor hashes $vf"
                unhashed=$((unhashed + 1))
            fi
        elif [ ! -s "$d/$vf" ]; then
            bad "$p" "ships $vf as an empty file"
            unhashed=$((unhashed + 1))
        elif ! grep -q "^sha256[[:blank:]]*=.*[[:blank:]]$vf\$" "$d/kpkgbuild"; then
            bad "$p" "ships $vf with no sha256 line"
            unhashed=$((unhashed + 1))
        fi
    fi
done
[ "$unhashed" = 0 ] && note "every source is hashed; every one on disk is non-empty" "ok"
[ "$unfetched" = 0 ] || note "sources not fetched yet" "$unfetched — run make fetch before make build"
if [ -n "$stale" ]; then
    note "stale fetched archives no recipe names" "$(printf '%s\n' $stale | wc -l) — make fetch removes each one linked into ports/.srccache; any other is the only copy here"
    for s in $stale; do printf '      %s\n' "$s"; done
fi

# The escape hatch must be unused in a committed tree.
if grep -rq "KDOS_ALLOW_UNVERIFIED" ports/core/*/*/kpkgbuild src/*/*/kpkgbuild 2>/dev/null; then
    bad "recipes" "a recipe references KDOS_ALLOW_UNVERIFIED"
else
    note "no recipe needs the unverified escape hatch" "ok"
fi

echo
echo "==> every reason names things that still exist"
# Without this the reasons corpus diverges from the tree within a month and
# then actively lies, which is worse than not existing at all. Same gate as
# an unresolvable `# depends`.
rot=0
# Basenames of every file in the tree, which is what a `cite:` resolves
# against. The list comes off the disk rather than out of `git ls-files`: a
# run outside a checkout — a container that owns the tree differently, an
# export — gets no file list from git and would then reject every cite. build/
# is generated and .git/ is not the tree, so neither may answer for a cite.
find . -path ./build -prune -o -path ./.git -prune -o -type f -print 2>/dev/null |
    sed 's,.*/,,' | sort -u > "$SP/treenames"
for r in src/system/kdos-tools/reasons/*.txt; do
    [ -f "$r" ] || continue
    _rn=$(basename "$r" .txt)
    grep -q "^title:" "$r" || { bad "$_rn" "has no title:"; rot=$((rot + 1)); }
    grep -q "^path:\|^port:" "$r" || { bad "$_rn" "claims nothing"; rot=$((rot + 1)); }

    # Every one of these loops reads from a file, never from a pipe: `bad` in
    # a pipeline's subshell increments a copy of $fail, so the check would
    # print FAIL and preflight would still exit 0.
    sed -n 's/^port:[[:blank:]]*//p' "$r" > "$SP/rports"
    while read -r _p || [ -n "$_p" ]; do
        [ -n "$_p" ] || continue
        if ! has_port "$_p"; then
            bad "$_rn" "names port '$_p', which no longer exists"
            rot=$((rot + 1))
        fi
    done < "$SP/rports"

    # A path is real if fs/ provides it, or if the reason also names a port
    # (which is what installs it — preflight cannot see an installed tree).
    _hasport=$(grep -c "^port:" "$r")
    sed -n 's/^path:[[:blank:]]*//p' "$r" > "$SP/rpaths"
    while read -r _q || [ -n "$_q" ]; do
        [ -n "$_q" ] || continue
        if [ ! -e "fs$_q" ] && [ "$_hasport" = 0 ]; then
            bad "$_rn" "names path '$_q', which fs/ does not provide and no port claims"
            rot=$((rot + 1))
        fi
    done < "$SP/rpaths"

    sed -n 's/^see:[[:blank:]]*//p' "$r" > "$SP/rsees"
    while read -r _s || [ -n "$_s" ]; do
        [ -n "$_s" ] || continue
        if [ ! -f "src/system/kdos-tools/reasons/$_s.txt" ]; then
            bad "$_rn" "sees '$_s', which is not a reason"
            rot=$((rot + 1))
        fi
    done < "$SP/rsees"

    # A cite is prose, and only the words in it that carry an extension are
    # file names — those must still name a tracked file, by basename, wherever
    # in the tree it lives. A cite is the one key a reader retypes into a
    # search, so a cite naming a step or a source that is gone sends them
    # looking for nothing.
    sed -n 's/^cite:[[:blank:]]*//p' "$r" | tr ' \t,' '\n\n\n' > "$SP/cites"
    while read -r _c || [ -n "$_c" ]; do
        case "$_c" in *.*) ;; *) continue ;; esac
        if ! grep -qxF "$_c" "$SP/treenames"; then
            bad "$_rn" "cites '$_c', which is not a file in the tree"
            rot=$((rot + 1))
        fi
    done < "$SP/cites"
done
[ "$rot" = 0 ] && note "reasons resolve" "$(ls src/system/kdos-tools/reasons/*.txt 2>/dev/null | wc -l) recorded"

echo
echo "==> every port has a build.sh, and it parses"
# The build is a shell script in its own file, so it can actually be checked:
# `bash -n` on every build.sh is a real syntax gate.
missing=0
scripts=0
for d in "${PORTS_ALL[@]}"; do
    p=$(basename "$d")
    if [ ! -f "$d/build.sh" ]; then
        bad "$p" "no build.sh beside kpkgbuild"
        missing=$((missing + 1))
        continue
    fi
    for f in "$d/build.sh" "$d/postinstall.sh"; do
        [ -f "$f" ] || continue
        scripts=$((scripts + 1))
        bash -n "$f" 2>"$SP/err" || bad "$p" "$(basename "$f"): $(head -1 "$SP/err")"
        # `local` is only legal inside a function, and a build.sh runs as a
        # plain script — bash accepts it at parse time and dies at RUN time
        # with "can only be used in a function", hours into a build. `bash -n`
        # cannot see it. No build.sh in the tree defines a function, so any
        # `local` in one is this failure.
        if grep -qn '^[[:space:]]*local[[:space:]]' "$f"; then
            bad "$p" "$(basename "$f"): 'local' outside a function (line $(grep -n '^[[:space:]]*local[[:space:]]' "$f" | head -1 | cut -d: -f1))"
            missing=$((missing + 1))
        fi
    done
done
[ "$missing" = 0 ] && note "build scripts" "$scripts parse, none missing"

echo
echo "==> every recipe parses as metadata"
# The same reader the build uses. A recipe that does not parse has no name,
# version or release, and nothing downstream would find out until it ran.
for d in "${PORTS_ALL[@]}"; do
    p=$(basename "$d")
    out=$("$SP/kpkg" meta "$d" 2>"$SP/err")
    if [ -s "$SP/err" ] || [ -z "$out" ]; then
        bad "$p" "kpkgbuild does not parse: $(head -1 "$SP/err")"
    fi
done
note "recipe metadata" "all recipes parse"

echo
echo "==> every recipe declares a name, version and release"
for d in "${PORTS_ALL[@]}"; do
    for k in name version release; do
        grep -qE "^$k[[:blank:]]*=" "$d/kpkgbuild" || \
            bad "$(basename "$d")" "no '$k'"
    done
    # A PACKAGE FILE IS <name>-<version>-<release>.tar.xz AND IS TAKEN APART
    # FROM THE RIGHT, so a hyphen anywhere in the version makes the split
    # ambiguous: the tail of the version is read as the version and its head
    # joins the name. Nothing fails — the package installs under a database
    # entry named for something that is not a port, which the orphan sweep
    # then removes. Upstreams that version by date-time are where this comes
    # up; a dot separates just as well.
    unset name version
    eval "$("$SP/kpkg" meta "$d" 2>/dev/null)"
    case "$version" in
        *-*) bad "$(basename "$d")" "version '$version' contains a hyphen" ;;
    esac
done
note "recipe fields" "checked $(find ports/core src -name kpkgbuild 2>/dev/null | wc -l) ports"

echo
echo "==> shell that ships or builds is syntactically valid"
_sh=0
for f in script/*.sh script/*/*.sh script/phases/*/*.sh \
         script/phases/*/phase.env script/env/*.env fs/etc/init.d/* \
         ports/fetch testing/*.sh testing/flows/*.sh \
         ports/core/*/*/build.sh src/*/*/build.sh \
         ports/core/*/*/postinstall.sh src/*/*/postinstall.sh \
         fs/etc/profile fs/etc/profile.d/* fs/usr/local/bin/* \
         fs/usr/local/lib/kdos/* fs/etc/skel/.config/notmuch/default/hooks/*; do
    # A SYMLINK IS NOT A SCRIPT: `bash -n` on one reads whatever it points
    # at, which need not be in this tree. Regular files whose first line names a shell, and nothing
    # else — which is also what keeps a config file out of the loop.
    [ -f "$f" ] || continue
    [ -L "$f" ] && continue
    head -1 "$f" | grep -qE '^#!.*(^|/)(sh|bash|dash)( |$)' ||
        case "$f" in
            # A profile fragment is SOURCED and carries no shebang; so does
            # /etc/profile itself. Everything else in the list without one is
            # not a script.
            script/*|testing/*|ports/*|src/*|\
            fs/etc/profile|fs/etc/profile.d/*) ;;
            *) continue ;;
        esac
    _sh=$((_sh + 1))
    bash -n "$f" 2>"$SP/err" || bad "$f" "$(head -1 "$SP/err")"
done
note "shell syntax" "$_sh file(s) parse"

echo
echo "==> a script shipped inside a recipe parses, and names only programs the image has"
# A HEREDOC IS A SCRIPT NOTHING ELSE CAN SEE. `bash -n` on a build.sh reads the
# heredoc as one word, so a script written inside one is unchecked — and a
# recipe body is where such a script has to live, because the recipe hash
# covers kpkgbuild, build.sh, postinstall.sh and *.patch only: a file beside
# them ships stale after every later edit, with no error anywhere. The
# delimiter KDOS_SH marks a heredoc whose body is a /bin/sh script, and both
# checks below run on it.
#
# THE PROGRAMS ARE CHECKED AGAINST THE BUILD TREE, not against a phase list,
# because a port's name and its binaries' names are different things —
# `mutool` comes from `mupdf` — and a name table mapping one to the other
# would be a second place to keep right. Skipped when there is no build tree.
#
# ONLY WHERE A COMMAND IS THE FIRST WORD OF A LINE, after `if `, or after
# `set -- `. A name inside a command substitution or a `trap` string is not
# seen, so this is a check on the renderers a script dispatches to and not a
# proof that every program it could ever run exists.
#
# `bash -n`, NOT `sh -n`: /bin/sh on the image is bash, and the host's is
# whatever the developer's distribution ships — a `sh -n` verdict would then
# change with the machine preflight runs on, which is the opposite of what
# this is for.
_hd=0
_hdbad=0
for f in ports/core/*/*/build.sh src/*/*/build.sh; do
    [ -f "$f" ] || continue
    grep -q "<<'KDOS_SH'" "$f" || continue
    _p=$(basename "$(dirname "$f")")
    # ONE FILE PER HEREDOC. Two bodies concatenated parse as one script, so two
    # halves that are each invalid can be valid joined — an unclosed `case` in
    # the first closed by an `esac` in the second.
    rm -f "$SP"/heredoc.*.sh
    awk -v out="$SP/heredoc" '
        /<<.KDOS_SH./ { k = 1; n++; next }
        k && /^KDOS_SH$/ { k = 0; next }
        k { print > (out "." n ".sh") }
    ' "$f"
    for _b in "$SP"/heredoc.*.sh; do
        [ -f "$_b" ] || continue
        _hd=$((_hd + 1))
        bash -n "$_b" 2>"$SP/err" || {
            bad "$_p" "KDOS_SH heredoc: $(head -1 "$SP/err")"
            _hdbad=$((_hdbad + 1))
        }
        [ -d build/fs/usr/bin ] || continue
        # A function is not a program: the heredoc's own, and service_helper's
        # when an init script sources it.
        _fn=" $(sed -n 's/^[[:space:]]*\([a-z_][a-z0-9_]*\)[[:space:]]*()[[:space:]]*{.*/\1/p' \
                 "$_b" fs/etc/init.d/service_helper | tr '\n' ' ') "
        for _c in $(sed 's/^[[:space:]]*//; s/#.*//' "$_b" |
                    sed -n 's/^if \([a-z][a-z0-9_.-]*\) .*/\1/p
                            s/^set -- \([a-z][a-z0-9_.-]*\) .*/\1/p
                            s/^\([a-z][a-z0-9_.-]*\)[[:space:]].*/\1/p' |
                    sort -u); do
            case "$_c" in
                set|if|then|elif|else|fi|case|esac|until|while|for|do|done|\
                trap|exit|return|break|continue|command|export|local|read|\
                eval|exec|cd|shift|unset|wait|getopts|source) continue ;;
            esac
            case "$_fn" in *" $_c "*) continue ;; esac
            [ -e "build/fs/usr/bin/$_c" ] || [ -e "build/fs/bin/$_c" ] ||
            [ -e "build/fs/usr/sbin/$_c" ] || [ -e "build/fs/sbin/$_c" ] || {
                bad "$_p" "KDOS_SH heredoc runs '$_c', which is on no image"
                _hdbad=$((_hdbad + 1))
            }
        done
    done
done
if [ "$_hdbad" != 0 ]; then
    :
elif [ "$_hd" = 0 ]; then
    note "recipe heredocs" "none"
elif [ -d build/fs/usr/bin ]; then
    note "recipe heredocs" "$_hd parse, every program on the image"
else
    note "recipe heredocs" "$_hd parse; programs unchecked — no build tree"
fi

echo
echo "==> none of the retired paths exists, and nothing invokes a retired tool"
for gone in fs/usr/local/bin/kdos fs/usr/local/bin/kdos-banner \
            fs/usr/local/bin/kdos-shot fs/usr/local/bin/kdos-fetch-app \
            fs/usr/local/bin/kdos-fetch-static fs/usr/sbin/service \
            fs/usr/local/sbin/kdos-getty src/kpkg/kpkg \
            ports/appbox ports/sources ports/sources.manifest \
            fs/etc/kdos/pack-sources; do
    [ -e "$gone" ] && bad "$gone" "must not exist"
done
# Only things that would INVOKE a retired tool count; a C file may name one in
# a comment without running it. The script name is the token straight after
# `python3`: a `depends` line that names python3 and then wavpack or libmspack
# invokes nothing.
# THE ARCHIVES ARE EXCLUDED BY NAME, NOT BY A grep FLAG. `ports` holds the
# fetched tarballs beside the recipes and the baked packs beside their build
# scripts — about 39 GB of them — and grep reads a compressed file whole
# before it can decide the file is binary. The suite's own container has
# BusyBox grep, which has no --include, no --exclude and no -I, so the list is
# built with find instead: a recipe or a script is what can INVOKE a retired
# tool, and a tarball never can.
hits=$(find script ports fs Makefile -type f \
        ! -name '*.kpack' ! -name '*.tar.*' ! -name '*.tgz' ! -name '*.tbz2' \
        ! -name '*.txz' ! -name '*.zip' ! -name '*.lz' 2>/dev/null |
       xargs grep -l 'python3 [^ ]*genlaunchers\|python3 [^ ]*pack \|python3 [^ ]*assemble\|python3 [^ ]*gengtk\|python3 [^ ]*genicons\|python3 [^ ]*gencursors' \
        2>/dev/null || true)
[ -z "$hits" ] && note "no stale invocations" "ok" || bad "stale invocations" "$hits"

echo
echo "==> the rootfs carries no script whose interpreter is gone"
# The question is whether the interpreter EXISTS on the target, not whether it
# is a shell: a shipped file whose `#!` names something the tree does not build
# is a file that cannot run. bash, and sh as its link, are always there; anything
# else has to be a binary some port installs, and is listed here with the port
# that provides it so the list cannot drift into an unchecked allowlist.
#
#   /usr/sbin/nft   nftables   — fs/etc/nftables.conf carries nft's own `-f`
#                                shebang; 25_nftables.sh runs it explicitly, so
#                                the shebang documents the format rather than
#                                being the execution path.
for f in $(grep -rl '^#!' fs/ 2>/dev/null); do
    interp=$(head -1 "$f" | sed 's|^#!||; s| .*||')
    case "$interp" in
        /bin/bash|/bin/sh) ;;
        /usr/sbin/nft)
            has_port nftables \
                || bad "$f" "interpreter $interp has no port" ;;
        *) bad "$f" "unexpected interpreter $interp" ;;
    esac
done
note "rootfs interpreters" "every #! is provided by the tree"

# ── the build tree carries packages whose port is gone ─────────────────────
#
# The build tree is incremental and nothing but the orphan sweep removes a
# package: a port deleted from `ports/` leaves its package installed, and the
# ISO ships it — a whole desktop's worth of packages, hundreds of megabytes,
# when a desktop's ports go at once. `70_image/040_orphans.sh` sweeps them at
# image time, with every package whose port ports/unbuilt lists; this says so
# BEFORE a two-hour build does.
#
# Skipped, not failed, when there is no build tree: preflight's whole point is
# that it needs nothing but the repo.
echo
echo "==> the build tree carries no package whose port is gone"
if [ ! -d build/fs/var/lib/kpkg/db ]; then
    note "orphaned packages" "skipped — no build tree"
else
    orphans=""
    _unb=$(grep -v '^#' ports/unbuilt 2>/dev/null)
    for pkg in $(ls build/fs/var/lib/kpkg/db); do
        has_port "$pkg" || orphans="$orphans $pkg"
        printf '%s\n' $_unb | grep -qxF "$pkg" && orphans="$orphans $pkg(unbuilt)"
    done
    if [ -n "$orphans" ]; then
        bad "orphaned packages" "installed with no recipe, or listed in ports/unbuilt:$orphans"
    else
        note "orphaned packages" "none"
    fi
fi

# ── KDOS code links only the toolkit its area allows ──────────────────────
#
# Applications may link any toolkit and the X11 client libraries. KDOS's own
# code may not, by area: src/desktop holds the desktop's Qt code (KCMs,
# plasmoids, KWin plugins, the installer's front end) and may link Qt 6 and
# KDE Frameworks 6 and the Plasma libraries built on them, and nothing else
# from the toolkit list — no GTK, no Qt 5, no Xlib. src/system, src/daemons,
# src/art and src/devtools link no toolkit at all, and neither does a
# set-id file, anything under sbin or a program rcS starts, whatever its
# area. Root, early-boot and library code that pulls in a GUI stack makes the
# login depend on that stack, and a daemon's crash on a toolkit bug takes the
# session's services with it.
#
# Every ELF a package built from a port under src/ installs is read for its
# NEEDED entries, and so is each library those name, one level down: a daemon
# linking a Plasma library links Qt through it. libxcb is not on the list, and
# Xlib is refused only when named directly: Xwayland speaks XCB, and the KF6
# libraries the desktop links reach Xlib themselves.
#
# The guard reads ELF headers itself, so it needs python3 and no readelf. It
# proves itself first on testing/fixtures/toolkit/cases, built here with cc,
# and then reads build/fs; without a build tree only the proof runs.
echo
echo "==> KDOS code links only the toolkit its area allows"
cat > "$SP/toolkitguard.py" <<'PYEOF'
import os, re, struct, sys

fsroot, srcroot, initdir, inittab = sys.argv[1:5]
ALWAYS = re.compile(r"^(libgtk-|libgdk-|libadwaita-|libwebkit|libjavascriptcoregtk|"
                    r"libQt(?:[0-57-9]|6\d)|libKF(?:[0-57-9]|6\d)|libwx_|libfltk|libtk[0-9]|"
                    r"libX[A-Za-z0-9_-]*\.so)")
QTKF6 = re.compile(r"^(libQt6|libKF6)")
TOOLKIT = re.compile(r"^(libgtk-|libgdk-|libadwaita-|libwebkit|libjavascriptcoregtk|libQt[0-9]|"
                     r"libKF[0-9]|libwx_|libfltk|libtk[0-9])")
DESKTOP_DEEP = re.compile(r"^(libgtk-|libgdk-|libadwaita-|libwebkit|libjavascriptcoregtk|"
                          r"libQt(?:[0-57-9]|6\d)|libKF(?:[0-57-9]|6\d)|libwx_|libfltk|libtk[0-9])")


def needed(path):
    """DT_NEEDED of an ELF file, or None when it is not one."""
    try:
        with open(path, "rb") as f:
            if f.read(4) != b"\x7fELF":
                return None
            f.seek(0)
            data = f.read()
    except OSError:
        return None
    if len(data) < 52:
        return None
    is64, end = data[4] == 2, "<" if data[5] == 1 else ">"
    if is64:
        phoff, shoff = struct.unpack_from(end + "QQ", data, 32)
        phentsize, phnum, shentsize, shnum = struct.unpack_from(end + "HHHH", data, 54)
    else:
        phoff, shoff = struct.unpack_from(end + "II", data, 28)
        phentsize, phnum, shentsize, shnum = struct.unpack_from(end + "HHHH", data, 42)
    dyn = strtab = None
    for i in range(shnum):
        o = shoff + i * shentsize
        if is64:
            _n, typ, _fl, _addr, off, size, link = struct.unpack_from(end + "IIQQQQI", data, o)
        else:
            _n, typ, _fl, _addr, off, size, link = struct.unpack_from(end + "IIIIIII", data, o)
        if typ == 6:
            dyn = (off, size)
            lo = shoff + link * shentsize
            strtab = struct.unpack_from(end + ("Q" if is64 else "I"), data, lo + (24 if is64 else 16))[0]
            break
    entsz = 16 if is64 else 8
    if dyn is None:
        loads, strvaddr = [], None
        for i in range(phnum):
            o = phoff + i * phentsize
            if is64:
                typ, _fl, off, vaddr, _pa, filesz = struct.unpack_from(end + "IIQQQQ", data, o)
            else:
                typ, off, vaddr, _pa, filesz = struct.unpack_from(end + "IIIII", data, o)
            if typ == 1:
                loads.append((vaddr, off, filesz))
            elif typ == 2:
                dyn = (off, filesz)
        if dyn is None:
            return []
        for j in range(dyn[1] // entsz):
            tag, val = struct.unpack_from(end + ("qQ" if is64 else "iI"), data, dyn[0] + j * entsz)
            if tag == 5:
                strvaddr = val
        for vaddr, off, filesz in loads:
            if strvaddr is not None and vaddr <= strvaddr < vaddr + filesz:
                strtab = off + (strvaddr - vaddr)
        if strtab is None:
            return []
    out = []
    for j in range(dyn[1] // entsz):
        tag, val = struct.unpack_from(end + ("qQ" if is64 else "iI"), data, dyn[0] + j * entsz)
        if tag == 0:
            break
        if tag == 1:
            s = strtab + val
            out.append(data[s:data.index(b"\0", s)].decode("ascii", "replace"))
    return out


def inside(rel):
    """A path in the tree, its symlinks followed inside the tree: an absolute
    link names the image's root, never this host's."""
    p, hops = rel.lstrip("/"), 0
    while hops < 40:
        full = os.path.join(fsroot, p)
        if not os.path.islink(full):
            return full
        t = os.readlink(full)
        p = os.path.normpath(t.lstrip("/") if t.startswith("/") else os.path.join(os.path.dirname(p), t))
        hops += 1
    return None


deep = {}


def links_of(soname):
    if soname not in deep:
        hit = None
        for d in ("usr/lib", "lib"):
            full = inside(os.path.join(d, soname))
            if full and os.path.isfile(full):
                try:
                    hit = needed(full) or []
                except (struct.error, ValueError, IndexError):
                    hit = []
                break
        deep[soname] = hit or []
    return deep[soname]


started = set()
for d in (initdir,):
    if os.path.isdir(d):
        for n in sorted(os.listdir(d)):
            try:
                with open(os.path.join(d, n), encoding="utf-8", errors="replace") as f:
                    started.update(re.findall(r"(/(?:usr/)?(?:s?bin|lib|libexec)/[A-Za-z0-9._+/-]+)", f.read()))
            except OSError:
                pass
if os.path.isfile(inittab):
    with open(inittab, encoding="utf-8", errors="replace") as f:
        started.update(re.findall(r"(/(?:usr/)?(?:s?bin|lib|libexec)/[A-Za-z0-9._+/-]+)", f.read()))

nelf = npkg = 0
dbdir = os.path.join(fsroot, "var/lib/kpkg/db")
for area in sorted(os.listdir(os.path.join(srcroot, "src"))):
    adir = os.path.join(srcroot, "src", area)
    if not os.path.isdir(adir):
        continue
    for port in sorted(os.listdir(adir)):
        kp = os.path.join(adir, port, "kpkgbuild")
        if not os.path.isfile(kp):
            continue
        pkg = None
        with open(kp, encoding="utf-8", errors="replace") as f:
            for line in f:
                m = re.match(r"name\s*=\s*(\S+)", line)
                if m:
                    pkg = m.group(1)
                    break
        man = os.path.join(dbdir, pkg or "")
        if not pkg or not os.path.isfile(man):
            continue
        npkg += 1
        with open(man, encoding="utf-8", errors="replace") as f:
            paths = [l.strip() for i, l in enumerate(f) if i and l.strip()]
        for rel in paths:
            rel = rel[2:] if rel.startswith("./") else rel.lstrip("/")
            full = os.path.join(fsroot, rel)
            if rel.endswith("/") or os.path.islink(full) or not os.path.isfile(full):
                continue
            # A file that opens like an ELF and whose tables run off its end
            # is named, not skipped: its links are unknown, and one unread
            # file must not stop the reading of the rest.
            try:
                libs = needed(full)
            except (struct.error, ValueError, IndexError):
                print("BAD\t%s\t/%s\tis an ELF whose dynamic section cannot be read" % (pkg, rel))
                continue
            if libs is None:
                continue
            nelf += 1
            mode = os.stat(full).st_mode
            why = []
            if mode & 0o6000:
                why.append("set-id")
            if rel.startswith(("usr/sbin/", "sbin/")):
                why.append("under sbin")
            if "/" + rel in started:
                why.append("started by rcS")
            strict = area != "desktop" or bool(why)
            scope = "src/" + area + (", " + " and ".join(why) if why else "")
            for so in libs:
                if ALWAYS.match(so):
                    print("BAD\t%s\t/%s\tlinks %s, which no KDOS code may" % (pkg, rel, so))
                elif strict and QTKF6.match(so):
                    print("BAD\t%s\t/%s\tlinks %s; only src/desktop code that is neither set-id, "
                          "in sbin nor started by rcS may (%s)" % (pkg, rel, so, scope))
                else:
                    pat = TOOLKIT if strict else DESKTOP_DEEP
                    hit = [x for x in links_of(so) if pat.match(x)]
                    if hit:
                        print("BAD\t%s\t/%s\tlinks %s, which links %s (%s)" % (pkg, rel, so, hit[0], scope))
print("OK\t%d\t%d" % (nelf, npkg))
PYEOF

# The proof: every row of the fixture, built and judged.
_tk=$SP/toolkit
rm -rf "$_tk"
mkdir -p "$_tk/stub" "$_tk/fs/var/lib/kpkg/db" "$_tk/init.d" "$_tk/src"
_tkerr=""
_tk_so() {      # <output> <soname or -> <needed sonames...>
    local _o=$1 _so=$2 _l _libs=()
    shift 2
    for _l in "$@"; do
        if [ -f "$_tk/fs/usr/lib/$_l" ]; then _libs+=("$_tk/fs/usr/lib/$_l")
        else
            [ -f "$_tk/stub/$_l" ] || cc -shared -fPIC -o "$_tk/stub/$_l" -Wl,-soname,"$_l" \
                -x c /dev/null 2>/dev/null || return 1
            _libs+=("$_tk/stub/$_l")
        fi
    done
    mkdir -p "$(dirname "$_o")"
    if [ "$_so" = - ]; then
        cc -shared -fPIC -o "$_o" -x c /dev/null -x none -Wl,--no-as-needed "${_libs[@]}" 2>/dev/null
    else
        cc -shared -fPIC -o "$_o" -Wl,-soname,"$_so" -x c /dev/null -x none \
            -Wl,--no-as-needed "${_libs[@]}" 2>/dev/null
    fi
}
declare -A _tkwant
while read -r _a _p _x _m _path _sos; do
    case "$_a" in ''|\#*) continue ;; esac
    if [ "$_a" = lib ]; then
        _tk_so "$_tk/fs/$_path" "$(basename "$_path")" $_sos || _tkerr="cannot build $_path"
        continue
    fi
    _tk_so "$_tk/fs/$_path" - $_sos || { _tkerr="cannot build $_path"; continue; }
    chmod "${_m%%,*}" "$_tk/fs/$_path"
    case "$_m" in *,rcs) printf 'DAEMON="/%s"\n' "$_path" >> "$_tk/init.d/50_fixture.sh" ;; esac
    mkdir -p "$_tk/src/src/$_a/$_p"
    printf 'name = %s\nversion = 1\nrelease = 1\n' "$_p" > "$_tk/src/src/$_a/$_p/kpkgbuild"
    printf '1 1\n./%s\n' "$_path" > "$_tk/fs/var/lib/kpkg/db/$_p"
    _tkwant[$_p]=$_x
done < testing/fixtures/toolkit/cases
if [ -n "$_tkerr" ]; then
    bad "toolkit guard" "the fixture does not build: $_tkerr"
elif ! python3 "$SP/toolkitguard.py" "$_tk/fs" "$_tk/src" "$_tk/init.d" /nonexistent > "$SP/tkfix" 2>&1; then
    bad "toolkit guard" "the guard crashed on its fixture: $(tail -1 "$SP/tkfix")"
else
    _tkbad=0
    for _p in "${!_tkwant[@]}"; do
        if grep -q "^BAD	$_p	" "$SP/tkfix"; then _got=refused; else _got=allowed; fi
        [ "$_got" = "${_tkwant[$_p]}" ] && continue
        bad "toolkit guard" "fixture $_p should be ${_tkwant[$_p]}, the guard says $_got"
        _tkbad=$((_tkbad + 1))
    done
    [ "$_tkbad" = 0 ] &&
        note "toolkit guard" "refuses and allows every one of its ${#_tkwant[@]} fixture cases"
fi
if [ ! -d build/fs/var/lib/kpkg/db ]; then
    note "KDOS ELF links" "skipped — no build tree"
elif ! python3 "$SP/toolkitguard.py" build/fs . fs/etc/init.d fs/etc/inittab > "$SP/tkreal" 2>&1; then
    bad "KDOS ELF links" "the guard crashed: $(tail -1 "$SP/tkreal")"
else
    _tkn=0
    while IFS='	' read -r _k _pkg _f _why; do
        [ "$_k" = BAD ] || continue
        bad "KDOS ELF links" "$_pkg: $_f $_why"
        _tkn=$((_tkn + 1))
    done < "$SP/tkreal"
    [ "$_tkn" = 0 ] && note "KDOS ELF links" "$(awk -F'\t' '$1 == "OK" { print $2 " ELF file(s) in " $3 " package(s), each within its area" }' "$SP/tkreal")"
fi

# ── KDOS desktop code takes colour from a role, never a literal ───────────
#
# The KDOS accents reach Plasma only as the .colors files generated from
# libkcolor's palette, and every Qt surface reads them back through a role:
# Kirigami.Theme in QML, KColorScheme in C++. A literal in src/desktop is a
# colour that no accent, no light/dark switch and no Phosphor mode can reach,
# and on one of the eight schemes it is unreadable. Each *.qml, *.js, *.cpp
# and *.h under src/desktop, and each *.qml and *.js under src/art (the
# add-on plasmoids and scripts; its C is the palette's own writer), is read
# with its comments removed for a quoted #rgb/#rrggbb/#aarrggbb, a
# Qt.rgba/hsla/hsva or QColor built from numbers, a string or a
# Qt::GlobalColor other than transparent, and a named colour assigned to a
# colour property. A test (under tests/ or autotests/) is not read: its
# colours are the known values it checks a role mapping against, and nothing
# draws them.
#
# _colour_allow names each literal that is no scheme's to give, by its file
# and the exact text the check reports, with the reason beside it. A whole
# file is never let through, so a second literal in the same file fails. A
# colour the palette owns outside the schemes (the boot splash's screen-off
# black, its mascot) is not one: genlook.py writes it into the package from
# libkcolor and kdos-splash, and the QML reads it from there.
#
# The check proves itself first on testing/fixtures/colourlit, where every
# line ending in `// literal` must be reported and no other line may be; the
# line ending in `// allowed` is let through by an entry made for it alone.
echo
echo "==> KDOS desktop code takes colour from a role, never a literal"
_colour_allow=()
cat > "$SP/colourlit.py" <<'PYEOF'
import os, re, sys

qml_only = sys.argv[1] == "--qml-only"
args = sys.argv[2:] if qml_only else sys.argv[1:]
root, allow = args[0], set(args[1:])
HEX = re.compile(r"""["']#(?:[0-9a-fA-F]{3}|[0-9a-fA-F]{4}|[0-9a-fA-F]{6}|[0-9a-fA-F]{8})["']""")
QML_FN = re.compile(r"\bQt\.(?:rgba|hsla|hsva)\s*\(\s*[-+.\d][^()]*\)?")
NAMED = re.compile(r"""\b\w*[cC]olor\s*:\s*["'](?!transparent["'])[A-Za-z]+["']""")
CPP_CTOR = re.compile(r"""\bQColor\s*[({]\s*(?:[-+.\d]|0x|QStringLiteral\s*\(\s*"|QLatin1String\s*\(\s*"|u?"|"""
                      r"""Qt::(?!transparent\b)[A-Za-z]+)""")
CPP_FROM = re.compile(r"""\bQColor::from(?:Rgb|RgbF|Rgba64|Hsv|HsvF|Hsl|HslF|Cmyk|CmykF|String)\s*\(\s*"""
                      r"""(?:[-+.\d]|0x|u?"|QStringLiteral|QLatin1String)""")
CPP_GLOBAL = re.compile(r"\bQt::(?:white|black|red|darkRed|green|darkGreen|blue|darkBlue|cyan|darkCyan|"
                        r"magenta|darkMagenta|yellow|darkYellow|gray|darkGray|lightGray|color0|color1)\b")
RULES = {".qml": (HEX, QML_FN, NAMED), ".js": (HEX, QML_FN), ".mjs": (HEX, QML_FN),
         ".cpp": (HEX, CPP_CTOR, CPP_FROM, CPP_GLOBAL), ".cc": (HEX, CPP_CTOR, CPP_FROM, CPP_GLOBAL),
         ".h": (HEX, CPP_CTOR, CPP_FROM, CPP_GLOBAL), ".hpp": (HEX, CPP_CTOR, CPP_FROM, CPP_GLOBAL)}


def code(text):
    """The text with every comment blanked and every string kept, the line
    breaks where they were."""
    out, i, n, quote = [], 0, len(text), None
    while i < n:
        c = text[i]
        if quote is None:
            if text.startswith("//", i):
                j = text.find("\n", i)
                j = n if j < 0 else j
                out.append(" " * (j - i))
                i = j
                continue
            if text.startswith("/*", i):
                j = text.find("*/", i + 2)
                j = n if j < 0 else j + 2
                out.append(re.sub(r"[^\n]", " ", text[i:j]))
                i = j
                continue
            if c in "\"'`":
                quote = c
            out.append(c)
            i += 1
            continue
        if c == "\\" and i + 1 < n:
            out.append(text[i:i + 2])
            i += 2
            continue
        if c == quote or (c == "\n" and quote != "`"):
            quote = None
        out.append(c)
        i += 1
    return "".join(out)


for dirpath, dirs, files in os.walk(root):
    dirs[:] = sorted(d for d in dirs if d not in ("tests", "autotests"))
    for name in sorted(files):
        ext = os.path.splitext(name)[1]
        rules = RULES.get(ext) if not qml_only or ext in (".qml", ".js", ".mjs") else None
        if not rules:
            continue
        path = os.path.join(dirpath, name)
        with open(path, encoding="utf-8", errors="replace") as f:
            lines = code(f.read()).split("\n")
        for n, line in enumerate(lines, 1):
            hit = next((m.group(0).strip() for rule in rules for m in rule.finditer(line)
                        if "%s\t%s" % (path, m.group(0).strip()) not in allow), None)
            if hit:
                print("%s:%d:%s" % (path, n, hit))
PYEOF
_clfix=testing/fixtures/colourlit
if ! python3 "$SP/colourlit.py" "$_clfix" "$_clfix/Fixture.qml"$'\t''Qt.rgba(0, 0, 0, 1)' > "$SP/clfix" 2>&1; then
    bad "colour literals" "the check crashed on its fixture: $(tail -1 "$SP/clfix")"
else
    grep -n '// literal$' "$_clfix"/* | cut -d: -f1,2 | sort > "$SP/clwant"
    cut -d: -f1,2 "$SP/clfix" | sort > "$SP/clgot"
    if ! grep -q '// allowed$' "$_clfix/Fixture.qml"; then
        bad "colour literals" "the fixture has no allowed literal to prove the allowlist on"
    elif cmp -s "$SP/clwant" "$SP/clgot"; then
        note "colour literals" "the fixture's $(wc -l < "$SP/clwant") literals reported, nothing else"
    else
        bad "colour literals" "the fixture is misjudged — missed: $(comm -23 "$SP/clwant" "$SP/clgot" | tr '\n' ' ')reported: $(comm -13 "$SP/clwant" "$SP/clgot" | tr '\n' ' ')"
    fi
fi
_clok=1
: > "$SP/clreal"
for _r in src/desktop "--qml-only src/art"; do
    # shellcheck disable=SC2086
    [ -d "${_r#--qml-only }" ] || continue
    python3 "$SP/colourlit.py" $_r "${_colour_allow[@]}" >> "$SP/clreal" 2> "$SP/clerr" ||
        { bad "colour literals" "the check could not read ${_r#--qml-only }: $(tail -1 "$SP/clerr")"; _clok=0; }
done
_cln=0
while IFS= read -r _l; do
    bad "colour literals" "${_l%%:*}:$(printf '%s' "$_l" | cut -d: -f2) sets ${_l#*:*:} — read a Kirigami.Theme or KColorScheme role"
    _cln=$((_cln + 1))
done < "$SP/clreal"
[ "$_clok" = 1 ] && [ "$_cln" = 0 ] &&
    note "colour literals" "none in src/desktop or src/art's QML, ${#_colour_allow[@]} let through by name"

# ── the Plasma session as KDOS ships it ───────────────────────────────────
#
# One helper reads every file format these checks hold to: unit paths, KPackage
# metadata, window rules, the greeter's configuration and KConfig keys. Each
# check that parses something proves itself on its fixture under
# testing/fixtures before it reads the tree.
cat > "$SP/plasmacheck.py" <<'PYEOF'
import json, os, re, sys


def kconfig(path):
    """{group: {key: value}} of a KConfig/INI file; "[a][b]" is "a/b"."""
    groups, cur = {}, None
    with open(path, encoding="utf-8", errors="replace") as f:
        for raw in f:
            s = raw.strip()
            if not s or s.startswith("#"):
                continue
            if re.fullmatch(r"(\[[^\]]*\])+", s):
                cur = "/".join(p for p in re.findall(r"\[([^\]]*)\]", s) if not p.startswith("$"))
                groups.setdefault(cur, {})
            elif "=" in s and cur is not None:
                k, v = s.split("=", 1)
                groups[cur][re.sub(r"\[[^\]]*\]$", "", k.strip())] = v.strip()
            else:
                raise ValueError("%s: not a group or a key: %r" % (path, s))
    return groups


def units():
    """Paths on stdin; a systemd unit or path is reported with its rule."""
    for line in sys.stdin:
        p = line.strip().lstrip("./")
        if not p:
            continue
        doc = p.startswith("usr/share/doc/") or "/examples/" in "/" + p
        if re.match(r"(usr/)?lib/systemd/|etc/systemd/", p):
            print("%s\tis a systemd path; KDOS runs no systemd units" % p)
        elif p.endswith((".service", ".socket")) and not doc and not p.startswith(
                ("usr/share/dbus-1/", "usr/share/accounts/services/")):
            print("%s\tis a systemd unit; only a D-Bus service file under usr/share/dbus-1 "
                  "or a libaccounts service under usr/share/accounts/services is a .service" % p)
        elif p.endswith(".timer") and not doc and not p.startswith(
                ("etc/kdos/timers.d/", "etc/kdos/user-services/timers.d/",
                 "etc/skel/.config/kdos/timers.d/")):
            print("%s\tis a timer snooze never reads; KDOS timers live in etc/kdos/timers.d" % p)


PLUGIN_JSON = re.compile(r'K_PLUGIN_(?:CLASS|FACTORY)_WITH_JSON\s*\([^,]*,\s*"([^"]+)"')
KDOS_IDS = ("Plasma/Applet", "Plasma/LookAndFeel", "Plasma/LayoutTemplate", "Plasma/Wallpaper")


def check_json(path):
    try:
        with open(path, encoding="utf-8") as f:
            return json.load(f), None
    except (OSError, ValueError) as e:
        return None, "does not parse as JSON: %s" % e


def metadata(root):
    """Every KPackage and plugin JSON under root, held to what Plasma and
    KCMUtils refuse to load without."""
    for dirpath, dirs, files in os.walk(root):
        dirs[:] = sorted(d for d in dirs if d not in ("__pycache__", ".git"))
        here = set(files)
        if "metadata.json" in here:
            path = os.path.join(dirpath, "metadata.json")
            data, err = check_json(path)
            if err:
                print("%s\t%s" % (path, err))
                continue
            kp = data.get("KPlugin") if isinstance(data, dict) else None
            if not isinstance(kp, dict) or not kp.get("Name"):
                print("%s\thas no KPlugin.Name; Plasma lists it as nothing" % path)
                continue
            st = data.get("KPackageStructure", "")
            if st and not kp.get("Id") and st != "Plasma/Theme":
                print("%s\thas no KPlugin.Id; kpackagetool6 installs a package by its Id" % path)
            if st in KDOS_IDS and kp.get("Id") and not kp["Id"].startswith("org.kdos."):
                print("%s\tis KDOS's %s with Id %s; KDOS plugin ids are org.kdos.*" % (path, st, kp["Id"]))
            if st == "Plasma/Applet":
                api = str(data.get("X-Plasma-API-Minimum-Version", ""))
                if not api.startswith("6"):
                    print("%s\thas no X-Plasma-API-Minimum-Version 6.x; plasmashell 6 will not load it" % path)
                if not os.path.isfile(os.path.join(dirpath, "contents/ui/main.qml")):
                    print("%s\tis a plasmoid with no contents/ui/main.qml" % path)
        elif "contents" in dirs and os.path.isfile(os.path.join(dirpath, "contents/ui/main.qml")) \
                and "metadata.json.in" not in here and "metadata.json.cmake" not in here:
            print("%s\tholds contents/ui/main.qml and no metadata.json" % dirpath)
        for name in sorted(files):
            if not name.endswith((".cpp", ".cc", ".h")):
                continue
            with open(os.path.join(dirpath, name), encoding="utf-8", errors="replace") as f:
                text = f.read()
            for ref in PLUGIN_JSON.findall(text):
                cands = [os.path.join(dirpath, ref + s) for s in ("", ".in", ".cmake")]
                hit = next((c for c in cands if os.path.isfile(c)), None)
                if not hit:
                    print("%s\tnames %s, which is not beside it" % (os.path.join(dirpath, name), ref))
                elif hit.endswith(".json"):
                    data, err = check_json(hit)
                    if err:
                        print("%s\t%s" % (hit, err))
                    elif not isinstance(data.get("KPlugin"), dict) or not data["KPlugin"].get("Name"):
                        print("%s\thas no KPlugin.Name; System Settings lists it as nothing" % hit)


def kwinrules(paths):
    for path in paths:
        try:
            g = kconfig(path)
        except ValueError as e:
            print("%s\t%s" % (path, e))
            continue
        gen = g.get("General", {})
        rules = [r for r in gen.get("rules", "").split(",") if r]
        try:
            count = int(gen.get("count", "0"))
        except ValueError:
            print("%s\t[General] count is not a number" % path)
            continue
        if count != len(rules):
            print("%s\t[General] count=%d but rules names %d" % (path, count, len(rules)))
        for r in rules:
            if r not in g:
                print("%s\t[General] rules names %s, which has no group" % (path, r))
            elif not g[r].get("Description"):
                print("%s\t[%s] has no Description; the Window Rules page lists it blank" % (path, r))


def sddm(paths):
    for path in paths:
        try:
            g = kconfig(path)
        except ValueError as e:
            print("BAD\t%s\t%s" % (path, e))
            continue
        ds = g.get("General", {}).get("DisplayServer")
        if ds is not None and ds != "wayland":
            print("BAD\t%s\tDisplayServer=%s; the greeter runs on kwin_wayland and there is no X server" % (path, ds))
        elif ds == "wayland":
            print("WAYLAND\t%s" % path)
        for k, v in g.get("X11", {}).items():
            if k == "SessionDir" and v == "":
                continue
            print("BAD\t%s\t[X11] %s configures an X server KDOS does not have" % (path, k))


def get(path, group, key):
    try:
        print(kconfig(path).get(group, {}).get(key, ""))
    except ValueError:
        print("")


def sddm_services(paths):
    for path in paths:
        try:
            for g in kconfig(path):
                if g.startswith("services/"):
                    print(g[len("services/"):])
        except ValueError:
            pass


cmd = sys.argv[1]
if cmd == "units":
    units()
elif cmd == "metadata":
    metadata(sys.argv[2])
elif cmd == "kwinrules":
    kwinrules(sys.argv[2:])
elif cmd == "sddm":
    sddm(sys.argv[2:])
elif cmd == "get":
    get(*sys.argv[2:5])
elif cmd == "services":
    sddm_services(sys.argv[2:])
PYEOF

# Every file named <name> that ships into an XDG config dir: fs/etc/xdg, the
# distribution layer fs/usr/share/kdos/xdg, and any xdg/ directory a port of
# ours installs from.
shipped_cfg() {
    local _c
    for _c in "fs/etc/xdg/$1" "fs/usr/share/kdos/xdg/$1"; do
        [ -f "$_c" ] && printf '%s\n' "$_c"
    done
    find src -path "*/xdg/$1" -type f 2>/dev/null | LC_ALL=C sort
}

echo
echo "==> no systemd unit ships, and a timer lives only where snooze reads it"
# kpkg refuses usr/lib/systemd and etc/X11 in a package; this holds fs/ and
# every installed path to the rest of the rule. A .service is a D-Bus service
# file under usr/share/dbus-1 and nothing else, a .socket is never needed,
# and a .timer is a row in one of the tables KDOS hands to snooze — the
# system's (etc/kdos/timers.d, 18_timers.sh), every user's
# (etc/kdos/user-services/timers.d, the timers user service) and a person's
# own (seeded from etc/skel/.config/kdos/timers.d). Anywhere else it is a unit
# some recipe forgot to drop, which nothing on KDOS runs and the next reader
# takes for a schedule. A unit kept as documentation (usr/share/doc, an
# examples/ directory) configures nothing and passes.
python3 "$SP/plasmacheck.py" units < <(grep -v '^#' testing/fixtures/units/paths | awk 'NF { print $1 }') > "$SP/unitfix"
_ufbad=0
while read -r _p _x; do
    case "$_p" in ''|\#*) continue ;; esac
    if grep -q "^$_p	" "$SP/unitfix"; then _g=refused; else _g=allowed; fi
    [ "$_g" = "$_x" ] || { bad "unit paths" "fixture $_p should be $_x, the check says $_g"; _ufbad=1; }
done < testing/fixtures/units/paths
{ git ls-files fs 2>/dev/null || find fs -type f; } | sed 's|^fs/||' > "$SP/unitpaths"
_usrc="fs/"
if [ -d build/fs/var/lib/kpkg/db ]; then
    cat build/fs/var/lib/kpkg/db/* 2>/dev/null | grep '^\./' >> "$SP/unitpaths"
    _usrc="fs/ and every installed package"
fi
python3 "$SP/plasmacheck.py" units < "$SP/unitpaths" | sort -u > "$SP/unitbad"
while IFS='	' read -r _p _why; do
    bad "unit paths" "$_p $_why"
done < "$SP/unitbad"
[ "$_ufbad" = 0 ] && [ ! -s "$SP/unitbad" ] &&
    note "unit paths" "no unit in $_usrc; fixture judged right"

echo
echo "==> startplasma is told there is no systemd"
# startplasma-wayland starts the session through systemd --user unless
# startkderc says otherwise; on KDOS that path finds no user manager and the
# login returns to the greeter. The key ships in the distribution layer.
if has_port plasma-workspace; then
    _sk=$(shipped_cfg startkderc)
    _skbad=""
    for _f in $_sk; do
        [ "$(python3 "$SP/plasmacheck.py" get "$_f" General systemdBoot)" = false ] || _skbad="$_skbad $_f"
    done
    if [ -z "$_sk" ]; then
        bad "startkderc" "plasma-workspace is a port and no startkderc ships [General] systemdBoot=false"
    elif [ -n "$_skbad" ]; then
        bad "startkderc" "[General] systemdBoot is not false in:$_skbad"
    else
        note "startkderc" "systemdBoot=false in $(printf '%s ' $_sk)"
    fi
else
    note "startkderc" "skipped — plasma-workspace is not a port"
fi

echo
echo "==> every KDOS plasmoid, KCM and look-and-feel carries valid metadata"
# plasmashell, kpackagetool6 and System Settings drop a package whose
# metadata.json does not parse, or lacks what they key on, without a message
# anyone sees: a plasmoid with no X-Plasma-API-Minimum-Version 6 is "not
# compatible", one with no Id cannot be installed, and a KCM whose plugin
# JSON is missing does not build. KDOS's own package ids are org.kdos.*.
python3 "$SP/plasmacheck.py" metadata testing/fixtures/kpackage > "$SP/metafix" 2>&1
_mfbad=0
for _d in testing/fixtures/kpackage/*/; do
    _d=${_d%/}
    if grep -q "^$_d/" "$SP/metafix" || grep -q "^$_d	" "$SP/metafix"; then _g=bad; else _g=good; fi
    case "${_d##*/}" in "$_g"-*) ;; *) bad "metadata" "fixture ${_d##*/} misjudged: the check calls it $_g"; _mfbad=1 ;; esac
done
python3 "$SP/plasmacheck.py" metadata src > "$SP/metareal" 2>&1
while IFS='	' read -r _p _why; do
    bad "metadata" "$_p $_why"
done < "$SP/metareal"
[ "$_mfbad" = 0 ] && [ ! -s "$SP/metareal" ] &&
    note "metadata" "every package and plugin JSON under src/ loads; fixture judged right"

echo
echo "==> the shipped shortcuts and window rules parse, and every chord has one owner"
# A kglobalshortcutsrc entry kglobalacceld cannot split into current,default,
# friendly is ignored without a word, and a chord two owners want goes to the
# first; testing/chordcheck.py fails both, after proving on its fixtures that
# it fails Meta+Space bound to KRunner while fcitx5 keeps its default group
# switch (from files, and from a recorded kglobalaccel session), Meta+Q
# rebound while plasmashell keeps it, fcitx5's forward switch moved onto
# its own backward one, Meta+Q taken by a KDOS KWin effect's QML
# ShortcutHandler, and a disabled upstream script whose switch a KDOS kwinrc
# names taking a chord an entry holds through an absolute link — and passes
# that script once a shipped entry unbinds the chord, and the set KDOS ships. A [services][<id>]
# group names a desktop entry, which some port must install. kwinrulesrc's
# count and rules list must agree with its groups, or the Window Rules page
# shows rules that apply to nothing.
_ccf=testing/fixtures/chordcheck
while read -r _want _args; do
    # shellcheck disable=SC2086
    python3 testing/chordcheck.py --quiet $_args >/dev/null 2>&1
    case "$?:$_want" in
        0:pass|1:fail) ;;
        *) bad "chords" "chordcheck $_args should $_want" ;;
    esac
done <<EOF
fail --xdg $_ccf/meta-space
fail --xdg $_ccf/rebind
fail --xdg $_ccf/forward-only
fail --root $_ccf/qml-root
fail --root $_ccf/switch-root
pass --root $_ccf/switch-unbound-root
pass --xdg $_ccf/clean
fail --live-dump $_ccf/live.gdbus --xdg $_ccf/meta-space
pass --live-dump $_ccf/live.gdbus --xdg $_ccf/live-clean
EOF
_ccroot=""
[ -f build/fs/var/lib/kpkg/db/kglobalacceld ] && _ccroot="--root build/fs"
if python3 testing/chordcheck.py --quiet $_ccroot > "$SP/chords" 2>&1; then
    note "chords" "$(python3 testing/chordcheck.py $_ccroot 2>/dev/null | tail -1 | sed 's/^chordcheck: //')"
else
    bad "chords" "$(head -3 "$SP/chords" | sed 's/^ *FAIL *//; s/^ *//' | tr '\n' ' ')"
fi
_svc=$(python3 "$SP/plasmacheck.py" services $(shipped_cfg kglobalshortcutsrc) | sort -u)
# kglobalaccel also reads a service's entry from usr/share/kglobalaccel, which
# is where KRunner installs org.kde.krunner.desktop; such an id is resolved.
if [ -n "$_ccroot" ] && [ -n "$_svc" ]; then
    _svc=$(for _i in $_svc; do [ -e "build/fs/usr/share/kglobalaccel/$_i" ] || echo "$_i"; done)
fi
if [ -n "$_svc" ]; then
    if [ -n "$_ccroot" ]; then _mc="--root build/fs"; else _mc=""; fi
    python3 testing/mimecheck.py --quiet $_mc --ids $_svc > "$SP/svcids" 2>&1 ||
        bad "shortcut entries" "$(grep FAIL "$SP/svcids" | head -3 | sed 's/^ *FAIL *//' | tr '\n' ' ')"
fi
python3 "$SP/plasmacheck.py" kwinrules testing/fixtures/kwinrules/*.kwinrulesrc > "$SP/kwrfix"
_kwbad=0
for _f in testing/fixtures/kwinrules/*.kwinrulesrc; do
    if grep -q "^$_f	" "$SP/kwrfix"; then _g=bad; else _g=good; fi
    case "${_f##*/}" in "$_g"*) ;; *) bad "window rules" "fixture ${_f##*/} misjudged"; _kwbad=1 ;; esac
done
_kwr=$(shipped_cfg kwinrulesrc)
if [ -n "$_kwr" ]; then
    python3 "$SP/plasmacheck.py" kwinrules $_kwr > "$SP/kwrreal"
    while IFS='	' read -r _p _why; do bad "window rules" "$_p: $_why"; done < "$SP/kwrreal"
    [ ! -s "$SP/kwrreal" ] && [ "$_kwbad" = 0 ] && note "window rules" "$(printf '%s ' $_kwr)parse"
else
    [ "$_kwbad" = 0 ] && note "window rules" "none ship; fixture judged right"
fi

echo
echo "==> every desk layout script parses"
# A desk is a layout.js plasmashell evaluates when the desk is applied; one
# that does not parse leaves the person who picked it with no panel at all.
# node checks the syntax, and the check is skipped where it is absent.
if command -v node >/dev/null 2>&1; then
    if ! node --check testing/fixtures/layoutjs/good.js >/dev/null 2>&1 ||
            node --check testing/fixtures/layoutjs/bad.js >/dev/null 2>&1; then
        bad "layout scripts" "node --check misjudges testing/fixtures/layoutjs"
    fi
    _ljn=0 _ljbad=0
    while IFS= read -r _f; do
        _ljn=$((_ljn + 1))
        node --check "$_f" > "$SP/lj" 2>&1 ||
            { bad "layout scripts" "$_f: $(grep -m1 -i 'error' "$SP/lj")"; _ljbad=1; }
    done < <(find src fs -path '*/contents/layouts/*.js' -o -name layout.js -path '*/contents/*' \
             -o -path '*/plasmoidsetupscripts/*.js' 2>/dev/null | LC_ALL=C sort)
    [ "$_ljbad" = 0 ] && note "layout scripts" "$_ljn parse"
else
    note "layout scripts" "skipped — no node on this host"
fi

echo
echo "==> Get New… is off for every application"
# D10: nothing downloads at run time. KNewStuff honours the kiosk key
# [KDE Action Restrictions] ghns=false in a kdeglobals along the XDG config
# dirs, and greys out every "Get New…" button with it.
if has_port knewstuff && has_port plasma-workspace; then
    _gh=""
    for _f in $(shipped_cfg kdeglobals); do
        [ "$(python3 "$SP/plasmacheck.py" get "$_f" "KDE Action Restrictions" ghns)" = false ] && _gh="$_gh $_f"
    done
    if [ -n "$_gh" ]; then note "ghns" "false in$_gh"
    else bad "ghns" "no shipped kdeglobals sets [KDE Action Restrictions] ghns=false"; fi
else
    note "ghns" "skipped — no KNewStuff or no Plasma port"
fi

echo
echo "==> the greeter runs on Wayland, and nothing configures an X server"
# SDDM's own default display server is X11, and KDOS has none: unless a
# drop-in sets DisplayServer=wayland, the greeter never appears. An [X11]
# key is a configuration for a server that does not exist; the one let
# through is an empty SessionDir, which lists no X session. fs/etc/X11 must
# not exist (kpkg refuses it in a package for the same reason).
python3 "$SP/plasmacheck.py" sddm testing/fixtures/sddm/*.conf > "$SP/sddmfix"
_sdfix=0
for _f in testing/fixtures/sddm/*.conf; do
    if grep -q "^BAD	$_f	" "$SP/sddmfix"; then _g=bad; else _g=good; fi
    case "${_f##*/}" in "$_g"*) ;; *) bad "greeter" "fixture ${_f##*/} misjudged"; _sdfix=1 ;; esac
done
[ -e fs/etc/X11 ] && bad "greeter" "fs/etc/X11 exists"
_sdf=$( { [ -f fs/etc/sddm.conf ] && echo fs/etc/sddm.conf; ls fs/etc/sddm.conf.d/*.conf 2>/dev/null
          find src -path '*/sddm.conf.d/*.conf' -type f 2>/dev/null; } | LC_ALL=C sort -u)
if [ -n "$_sdf" ]; then
    python3 "$SP/plasmacheck.py" sddm $_sdf > "$SP/sddmreal"
    while IFS='	' read -r _k _p _why; do
        [ "$_k" = BAD ] && bad "greeter" "$_p: $_why"
    done < "$SP/sddmreal"
fi
if has_port sddm && ! grep -q '^WAYLAND' "$SP/sddmreal" 2>/dev/null; then
    bad "greeter" "sddm is a port and no shipped drop-in sets [General] DisplayServer=wayland"
elif [ "$_sdfix" = 0 ] && ! grep -q '^BAD' "$SP/sddmreal" 2>/dev/null && [ ! -e fs/etc/X11 ]; then
    note "greeter" "$(grep '^WAYLAND' "$SP/sddmreal" 2>/dev/null | cut -f2 | tr '\n' ' ')${_sdf:+sets Wayland; }no X configuration"
fi

echo
echo "==> every About-page script kinfocenter runs prints a label and a value"
# kinfocenter runs each ExtraSoftwareData program from kcm-about-distrorc and
# stops the whole page with qFatal when one prints fewer than two lines — on a
# machine with one root, no update and no box, too. Each script is found
# where fs/ or a port of ours ships it and run here with an empty environment
# and no KDOS state, which is that machine. A compiled program cannot run
# here, and is named instead.
about_run() {   # <fsroot> <installed path> -> prints why, or nothing
    local _src="" _bn
    _bn=$(basename "$2")
    if [ -f "$1$2" ]; then _src=$1$2
    else _src=$(find src -type f \( -name "$_bn" -o -name "$_bn.sh" \) 2>/dev/null | LC_ALL=C sort | head -1)
    fi
    if [ -z "$_src" ]; then
        grep -rqlF -- "$2" src/*/*/build.sh 2>/dev/null &&
            { echo "COMPILED"; return; }
        echo "names $2, which nothing in the tree ships"; return
    fi
    local _sh
    set -- "$1" "$2" $(head -1 "$_src" | sed -n 's/^#![[:space:]]*//p')
    [ -n "${3:-}" ] || { echo "$_src has no #! line, and kinfocenter execs it directly"; return; }
    _sh=$(basename "$3")
    [ "$_sh" = env ] && _sh=${4:-}
    [ "${_src#fs/}" = "$_src" ] || [ -x "$_src" ] || { echo "$_src is not executable"; return; }
    mkdir -p "$SP/aboutrun"
    local _out
    _out=$(cd "$SP/aboutrun" && env -i PATH=/usr/bin:/bin HOME="$SP/aboutrun" TMPDIR="$SP/aboutrun" \
           timeout 10 "$_sh" "$OLDPWD/$_src" 2>/dev/null; printf x)
    _out=${_out%x}
    case "$_out" in
        *$'\n'*) [ -n "${_out%%$'\n'*}" ] || echo "$_src prints an empty label" ;;
        *) echo "$_src prints $( [ -n "$_out" ] && echo one line || echo nothing ); kinfocenter needs two" ;;
    esac
}
_abfix=0
for _p in $(python3 "$SP/plasmacheck.py" get testing/fixtures/aboutdistro/kcm-about-distrorc General ExtraSoftwareData | tr ';' ' '); do
    _r=$(about_run testing/fixtures/aboutdistro/fs "$_p")
    case "$_p" in
        */about-two) [ -z "$_r" ] || { bad "About page" "fixture about-two misjudged: $_r"; _abfix=1; } ;;
        *) [ -n "$_r" ] && [ "$_r" != COMPILED ] || { bad "About page" "fixture ${_p##*/} passed"; _abfix=1; } ;;
    esac
done
_abn=0 _abc="" _abbad=0
for _f in $(shipped_cfg kcm-about-distrorc); do
    for _p in $(python3 "$SP/plasmacheck.py" get "$_f" General ExtraSoftwareData | tr ';' ' '); do
        _abn=$((_abn + 1))
        _r=$(about_run fs "$_p")
        case "$_r" in
            '') ;;
            COMPILED) _abc="$_abc ${_p##*/}" ;;
            *) bad "About page" "$_f: $_r"; _abbad=1 ;;
        esac
    done
done
[ "$_abfix" = 0 ] && [ "$_abbad" = 0 ] &&
    note "About page" "$_abn script(s) print two lines${_abc:+; built, so not run here:$_abc}"

echo
echo "==> no profile.d script exports the cursor theme KWin would prefer"
# KWin takes $XCURSOR_THEME and $XCURSOR_SIZE over kcminputrc whenever both
# are set, so an export in a login script pins every session to it and the
# global theme's cursor (breeze_cursors) and the Cursors page change nothing.
if has_port kwin; then
    _xc=$(grep -nE '^[[:space:]]*(export[[:space:]]+)?XCURSOR_(THEME|SIZE)=|^[[:space:]]*export[[:space:]].*\bXCURSOR_(THEME|SIZE)\b' \
          fs/etc/profile.d/*.sh fs/etc/profile 2>/dev/null)
    if [ -n "$_xc" ]; then bad "cursor exports" "$(printf '%s' "$_xc" | tr '\n' ' ')"
    else note "cursor exports" "none"; fi
else
    note "cursor exports" "skipped — kwin is not a port"
fi

echo
echo "==> every desktop id the default-applications list names is installed"
# A desktop id in kde-mimeapps.list that the image does not install is
# skipped by the lookup without a word, and the next association opens the
# file. testing/mimecheck.py traces each id to a port, or, once a build tree
# carries plasma-desktop, to the image itself. It proves both readings on
# testing/fixtures/mimecheck first: an id no port installs fails, a list with
# a duplicate key or a misspelt group does not parse, and a fixture tree has
# the ids it installs (one in the specification's subdirectory form) and
# lacks the rest.
_mcf=testing/fixtures/mimecheck
while read -r _want _args; do
    # shellcheck disable=SC2086
    python3 testing/mimecheck.py --quiet $_args >/dev/null 2>&1
    [ "$?" = "$_want" ] || bad "default applications" "mimecheck $_args should exit $_want"
done <<EOF
0 --list $_mcf/good.list
1 --list $_mcf/dangling.list
2 --list $_mcf/malformed.list
0 --root $_mcf/root --list $_mcf/root.list
1 --root $_mcf/root --list $_mcf/good.list
EOF
_mroot=""
[ -f build/fs/var/lib/kpkg/db/plasma-desktop ] && _mroot="--root build/fs"
python3 testing/mimecheck.py --quiet $_mroot > "$SP/mime" 2>&1
case $? in
    0) note "default applications" "$(python3 testing/mimecheck.py $_mroot 2>/dev/null | tail -1 | sed 's/^mimecheck: //')" ;;
    3) if has_port plasma-desktop; then bad "default applications" "no kde-mimeapps.list ships"
       else note "default applications" "skipped — no list and no Plasma"; fi ;;
    *) while IFS= read -r _l; do
           case "$_l" in *FAIL*|mimecheck:*) bad "default applications" "$(printf '%s' "$_l" | sed 's/^ *FAIL *//')" ;; esac
       done < "$SP/mime" ;;
esac

echo
echo "==> the build tree's root carries nothing but a root filesystem"
# A CHROOT INTO build/fs LEAVES ITS MOUNTPOINTS BEHIND, and the ISO is built
# from build/fs, so they ship. `docker run -v inputs:/rootfs/in` creates
# `build/fs/in`; a probe that writes to `/spool` or `$HOME` inside the chroot
# leaves that too. None of it is owned by a package or by fs/, so the orphan
# sweep and the fs-manifest guard both step over it and the only symptom is a
# shipped image with somebody's scratch directory at `/`.
#
# `kdos` and `ports` ARE expected: the build's own chroot binds the repo and
# the ports tree at those paths.
#
# ── the modes git cannot record ───────────────────────────────────────────
#
# git stores one permission bit, so nothing under fs/ can carry a mode narrower
# than 644 and the file-system step hands every non-executable file exactly
# that. `/etc/shadow` at 644 is every password hash on the machine readable by
# every account on it — and it makes unix_chkpwd, the setuid helper through
# which pam_unix checks a password for SDDM, the lock screen and every other
# stack that runs unprivileged, so that none of them opens that file, into
# decoration.
#
# Checked on the BUILT tree, because the source tree cannot express the answer:
# this asserts what will ship, not what was intended.
if [ -f build/fs/etc/shadow ]; then
    _sm=$(stat -c %a build/fs/etc/shadow)
    case "$_sm" in
        600|640) note "sensitive modes" "/etc/shadow is $_sm on the image" ;;
        *) bad "sensitive modes" "/etc/shadow is $_sm on the image — every hash is readable" ;;
    esac
    grep -q "^etc/shadow " script/phases/10_bootstrap/000_file_system.sh \
        || bad "sensitive modes" "nothing in 000_file_system.sh narrows etc/shadow"
else
    note "sensitive modes" "skipped — no build tree"
fi

# polkitd reads every rule it finds with no ownership check, so a rules
# directory the desktop user can write is that user granting themselves
# whatever they like — and the grant is the whole of this machine's network
# authorisation, with no agent to fall back on.
if [ -f build/fs/etc/polkit-1/rules.d/50-kdos.rules ]; then
    _ro=$(stat -c %u build/fs/etc/polkit-1/rules.d)
    _fo=$(stat -c %u build/fs/etc/polkit-1/rules.d/50-kdos.rules)
    if [ "$_ro" = 0 ] && [ "$_fo" = 0 ]; then
        note "polkit rules" "the rules and their directory are root's"
    else
        bad "polkit rules" "rules.d is uid $_ro and the file uid $_fo — the granted user can rewrite the grant"
    fi
else
    note "polkit rules" "skipped — no build tree"
fi

# udevd runs every RUN+= as root and reads every file it finds in rules.d with
# no ownership check, so a rules directory the desktop user can write is that
# user running arbitrary code as root on the next uevent — strictly worse than
# the polkit hole above, because it needs no service to be up.
if [ -d build/fs/etc/udev/rules.d ]; then
    _uo=$(stat -c %u build/fs/etc/udev/rules.d)
    _ubad=""
    for _f in build/fs/etc/udev/rules.d/*.rules; do
        [ -f "$_f" ] || continue
        [ "$(stat -c %u "$_f")" = 0 ] || _ubad="$_ubad $(basename "$_f")"
    done
    if [ "$_uo" = 0 ] && [ -z "$_ubad" ]; then
        note "udev rules" "the rules and their directory are root's"
    else
        bad "udev rules" "rules.d is uid $_uo and these are not root's:$_ubad — RUN+= is root code"
    fi
else
    note "udev rules" "skipped — no build tree"
fi

# ── what a udev rule can and cannot grant ─────────────────────────────────
#
# TWO TRAPS, BOTH SILENT, BOTH ALREADY PAID FOR ONCE.
#
# GROUP=/MODE= APPLY TO A DEVICE NODE. A class device with no node in /dev —
# backlight, leds, thermal, power_supply, hwmon — gets neither, and eudev goes
# further: GROUP= sets the rule's `can_set_name`, and a rule with that set is
# SKIPPED ENTIRELY for a nodeless device. So a GROUP= on such a line silently
# kills every other key on it, RUN+= included.
#
# AND A GROUP THE RULE NAMES HAS TO EXIST AND HAVE kdos IN IT. eudev logs
# "specified group '<x>' unknown" and carries on with gid 0, so the rule loads,
# matches, applies a mode, and grants nothing.
if [ -d fs/etc/udev/rules.d ]; then
    _rbad=""
    _gbad=""
    for _f in fs/etc/udev/rules.d/*.rules; do
        [ -f "$_f" ] || continue
        while IFS= read -r _line; do
            case "$_line" in \#*|"") continue ;; esac
            case "$_line" in
            *SUBSYSTEM==\"backlight\"*|*SUBSYSTEM==\"leds\"*|\
            *SUBSYSTEM==\"thermal\"*|*SUBSYSTEM==\"power_supply\"*|\
            *SUBSYSTEM==\"hwmon\"*)
                case "$_line" in
                *GROUP=*|*MODE=*|*OWNER=*)
                    _rbad="$_rbad $(basename "$_f")" ;;
                esac ;;
            esac
            _g=$(printf '%s' "$_line" | sed -n 's/.*GROUP="\([^"]*\)".*/\1/p')
            [ -n "$_g" ] || continue
            grep -qE "^$_g:[^:]*:[^:]*:.*\bkdos\b" fs/etc/group ||
                _gbad="$_gbad $(basename "$_f"):$_g"
        done < "$_f"
    done
    [ -n "$_rbad" ] && bad "udev rules" \
        "GROUP=/MODE=/OWNER= on a nodeless class device, which skips the whole line:$_rbad"
    [ -n "$_gbad" ] && bad "udev rules" \
        "grants to a group kdos is not in:$_gbad"
    [ -z "$_rbad$_gbad" ] &&
        note "udev grants" "every GROUP= names a group kdos is in, and none is on a nodeless class"
fi

# ── an account a shipped daemon drops to has to exist ─────────────────────
#
# A daemon that setuids to an account the image does not carry does not warn
# and does not degrade: it exits at once, and a supervisor respawns it for
# ever. dnsmasq is the measured case — its compiled-in default is `nobody`
# (CHUSER in src/config.h), NetworkManager passes no --user when it starts one
# for a shared connection, and dnsmasq dies "unknown user or group: nobody".
# The `nobody` GROUP is in fs/etc/group, which is what made the absence of the
# USER look fine.
if [ -f fs/etc/passwd ]; then
    _amiss=""
    for _a in nobody; do
        grep -q "^$_a:" fs/etc/passwd || _amiss="$_amiss $_a"
    done
    if [ -n "$_amiss" ]; then
        bad "daemon accounts" "no account for:$_amiss — the daemon that drops to it exits at once"
    else
        note "daemon accounts" "every account a shipped daemon drops to is in fs/etc/passwd"
    fi
fi

# ── the groups a surface's authority comes from ───────────────────────────
#
# The Printers page runs as the user and administers printers directly, because
# CUPS defines `lpadmin` as exactly that authority and `fs/etc/group` grants it. The
# installer carries it to the created account by RENAMING `kdos` in every
# membership list — so the membership in skel is what the whole arrangement
# rests on, and dropping it would leave printing silently unadministrable for
# everyone but root, with no error anywhere to say why.
if [ -f fs/etc/group ]; then
    _gmiss=""
    for _g in lpadmin video audio input wheel; do
        grep -qE "^$_g:[^:]*:[^:]*:.*\bkdos\b" fs/etc/group || _gmiss="$_gmiss $_g"
    done
    if [ -n "$_gmiss" ]; then
        bad "group membership" "kdos is not in:$_gmiss"
    else
        note "group membership" "kdos is in the five groups its surfaces need"
    fi
fi

# Skipped, not failed, when there is no build tree.
if [ ! -d build/fs ]; then
    note "root filesystem" "skipped — no build tree"
else
    _stray=""
    for _e in build/fs/* build/fs/.[!.]*; do
        [ -e "$_e" ] || continue
        case "$(basename "$_e")" in
            bin|boot|dev|etc|home|kdos|lib|lib64|ports|proc|root|run|sbin|\
            srv|sys|tmp|usr|var|opt|mnt|media) continue ;;
        esac
        _stray="$_stray $(basename "$_e")"
    done
    if [ -n "$_stray" ]; then
        bad "root filesystem" "build/fs carries:$_stray"
    else
        note "root filesystem" "no stray entries at /"
    fi
fi

# THE BANNER IS "KEEP VERBATIM" IN CLAUDE.md AND NOTHING ENFORCED IT. It is
# the one marker that says a file is ours rather than upstream's, and it is
# lost the way every boilerplate is lost: a generator whose header variable
# went out of scope writes 120 recipes without it, every one of which parses,
# builds and installs correctly. Nothing else here would ever notice.
echo
echo "==> every recipe and build script carries the KDOS banner"
noban=0
for f in ports/core/*/*/kpkgbuild ports/core/*/*/build.sh ports/core/*/*/postinstall.sh \
         src/*/*/kpkgbuild src/*/*/build.sh src/*/*/postinstall.sh; do
    [ -f "$f" ] || continue
    grep -q "KD's Homebrew Linux Distro" "$f" || {
        bad "${f#ports/core/*/}" "has no KDOS banner header"
        noban=$((noban + 1))
    }
done
[ "$noban" = 0 ] && note "banner header" "present in every recipe and build script"

# A ROOT FILESYSTEM THE INITRAMFS CANNOT MOUNT INSTALLS PERFECTLY AND NEVER
# BOOTS AGAIN, and nothing else here would see it: `ki_filesystems[]` is what
# the installer OFFERS and `090_initramfs.sh`'s MODULES line is what makes the
# offer bootable. The two are edited in different languages in different
# directories, so a row added to one and not the other compiles, passes every
# other gate, and bricks exactly the machine that picked it.
#
# ext4 and btrfs are built into this kernel, so they are not expected in
# MODULES; anything else in the table must be there by name.
echo
echo "==> every filesystem the installer offers, the initramfs can mount"
fs_conf=src/system/kinstall-core/conf.c
fs_ini=script/phases/70_image/090_initramfs.sh
if [ -f "$fs_conf" ] && [ -f "$fs_ini" ]; then
    fs_mods=$(grep -E '^MODULES=' "$fs_ini")
    fs_missing=""
    for fs in $(sed -n '/^const Filesystem ki_filesystems\[\]/,/^};/p' "$fs_conf" \
                | grep -oE '^\s*\{ "[a-z0-9]+"' | grep -oE '"[a-z0-9]+"' | tr -d '"'); do
        case "$fs" in ext4|btrfs) continue ;; esac
        case "$fs_mods" in *" $fs "*|*" $fs\""*) ;; *) fs_missing="$fs_missing $fs" ;; esac
    done
    if [ -n "$fs_missing" ]; then
        bad "090_initramfs.sh" "ki_filesystems[] offers$fs_missing, which the initramfs does not carry"
    else
        note "installer filesystems" "every offered root fs is in the initramfs"
    fi
else
    note "installer filesystems" "skipped — conf.c or 090_initramfs.sh not found"
fi

# A WRITTEN STICK BOOTS BY ITS PARTITION TABLE AND BY NOTHING ELSE. `dd` copies
# bytes, so an El Torito record — which lives in the ISO9660 boot catalogue and
# is read only by an optical drive — carries nothing to a USB stick. UEFI wants
# a partition of type EFI System; BIOS wants boot code in the first sector. The
# image has to answer all four combinations of firmware and medium, and each
# one is a separate flag that xorriso accepts in silence when it does nothing.
# 110_iso.sh verifies the finished image too, but that costs a full image
# phase to learn; this costs a second.
echo
echo "==> the ISO boots BIOS and UEFI, from a disc and from a written stick"
iso_sh=script/phases/70_image/110_iso.sh
if [ -f "$iso_sh" ]; then
    iso_missing=""
    for f in "limine-bios-cd.bin" "--efi-boot" "-efi-boot-part" \
             "--efi-boot-image" "--protective-msdos-label" \
             "limine bios-install"; do
        grep -qF -- "$f" "$iso_sh" || iso_missing="$iso_missing '$f'"
    done
    if [ -n "$iso_missing" ]; then
        bad "110_iso.sh" "a boot path is missing from the image —$iso_missing"
    else
        note "iso boot structure" "BIOS + UEFI, El Torito + partition table"
    fi
else
    note "iso boot structure" "skipped — 110_iso.sh not found"
fi

# A BINARY BUILT BY A PHASE STEP IS NOT REBUILT BY `--rebuild <port>`, because
# it is not a port. kinstall comes out of 10_bootstrap/130_kinstall.sh, which
# ALSO exits early when its marker exists — so editing the installer's sources,
# rebuilding, and running 70_image produces an ISO carrying the binary from whenever
# that marker was first written. Nothing fails: the build is green and the image
# boots, and only the installed system is wrong.
#
# The check is against a STRING THE SOURCE OWNS. Comparing timestamps cannot
# work — build/fs is stamped to a fixed epoch for reproducibility — and
# comparing hashes would need a reference build. The limine share path that
# install.c names must appear in the built binary's strings: one grep proves
# the binary was built from an install.c that names that path, and nothing
# about any other edit to the installer's sources.
echo
echo "==> the built kinstall is the installer in this tree"
ki_src=src/system/kinstall-core/install.c
ki_bin=build/fs/usr/bin/kinstall
if [ -f "$ki_src" ] && [ -f "$ki_bin" ]; then
    ki_want=$(grep -oE '"[a-z/]*share/limine"' "$ki_src" | head -1 | tr -d '"')
    if [ -z "$ki_want" ]; then
        note "kinstall freshness" "skipped — install.c names no limine path"
    elif strings "$ki_bin" 2>/dev/null | grep -qF "$ki_want"; then
        note "kinstall freshness" "built binary carries $ki_want"
    else
        bad "130_kinstall.sh" "build/fs/usr/bin/kinstall predates install.c — rm build/mark/bootstrap/kinstall and rebuild the bootstrap phase"
    fi
else
    note "kinstall freshness" "skipped — no build tree"
fi

# UPSTREAM SOURCES ARE NOT IN GIT. A port's recipe names each source by its
# sha256, `make fetch` resolves that hash from ports/.srccache, the
# kunaldawn/kdos archive or upstream, and `ports/publish` puts a new one
# in the archive. A recipe-hashed archive that git tracks as well is either the
# bytes themselves in every clone's history forever, or a pointer file that
# `make fetch` sees as a present-but-wrong source. An archive NO recipe hashes
# (a test input a build.sh reads) is allowed and listed, since the archive has
# no name for it.
#
# THE IGNORE RULES ARE WHAT KEEP A FETCHED TREE CLEAN. Every fetched source is
# a file in its port directory, two levels below ports/core; without a pattern
# for its suffix at that depth, `git add -A` commits it. The patterns are
# anchored to ports/core, so the archive fixtures under testing/fixtures must
# stay visible to git or every test replaying them is silently absent on a
# clone. `git check-ignore` on a synthetic path answers what git WILL do with a
# file nobody has written yet.
#
# BOTH SIDES OF THE COMPARISON ARE <port>/<file>. The hashed set takes the
# port's directory name off the recipe path and the tracked set strips
# ports/core/<shelf>/ off the index path; a shelf left on one side and not the
# other makes the intersection empty and passes every tracked archive.
#
# THE THREE SCRIPTS AND THE INDEX ARE THE WHOLE MECHANISM. ports/srclib.sh
# (sourced) is the archive's addressing and ports/sources.idx says which
# release and asset hold each file; ports/fetch and ports/publish run it as programs, and
# script/hooks/pre-push is what git runs once `git config core.hooksPath
# script/hooks` is set. A syntax error in any of them surfaces only on the
# command that needed it.
echo
echo "==> port sources: untracked, ignored, and the archive scripts sound"
if [ -d ports/core ] && git rev-parse --git-dir >/dev/null 2>&1; then
    pa_hashed=$(awk '/^sha256[[:blank:]]*=/ {
                    n = split(FILENAME, a, "/"); print a[n-1] "/" $NF }' \
                ports/core/*/*/kpkgbuild | LC_ALL=C sort -u)
    pa_trk=$(git ls-files --cached ports/core \
             | grep -E '\.(tar|tgz|tbz2|txz|zip|7z)$|\.tar\.' \
             | sed 's|^ports/core/[^/]*/||' | LC_ALL=C sort)
    pa_bad=$(comm -12 <(printf '%s\n' "$pa_hashed") <(printf '%s\n' "$pa_trk") | grep .)
    pa_free=$(comm -13 <(printf '%s\n' "$pa_hashed") <(printf '%s\n' "$pa_trk") | grep .)
    if [ -n "$pa_bad" ]; then
        bad "ports/core" "$(printf '%s\n' "$pa_bad" | grep -c .) recipe-hashed archives are tracked by git — git rm --cached them: $(printf '%s\n' "$pa_bad" | head -5 | tr '\n' ' ')…"
    else
        note "no recipe-hashed archive is tracked" "ok"
    fi
    [ -z "$pa_free" ] || note "tracked archives no recipe hashes" "$(printf '%s' "$pa_free" | tr '\n' ' ')"

    pa_open=""
    for e in tar tar.gz tar.xz tar.bz2 tar.zst tgz tbz2 txz zip 7z tar.gz.part; do
        git check-ignore -q --no-index "ports/core/.preflight-shelf/.preflight-probe/probe.$e" \
            || pa_open="$pa_open .$e"
    done
    git check-ignore -q --no-index ports/.srccache/sha256-00/probe \
        || pa_open="$pa_open ports/.srccache/"
    if [ -n "$pa_open" ]; then
        bad ".gitignore" "does not ignore under ports/core:$pa_open"
    else
        note ".gitignore covers every source suffix and the cache" "ok"
    fi
    # A pattern naming one port spells its shelf, and one `git mv` to another
    # shelf leaves it matching nothing: that port's odd-suffix sources are then
    # a `git add -A` away from being committed.
    pa_one=""
    for _gp in $(sed -n 's|^/ports/core/\([^*/]*/[^*/]*\)/.*|\1|p' .gitignore | sort -u); do
        [ -f "ports/core/$_gp/kpkgbuild" ] || pa_one="$pa_one $_gp"
    done
    [ -z "$pa_one" ] || bad ".gitignore" "names a port directory that holds no port:$pa_one"
    pa_fix=$(git ls-files testing/fixtures \
             | grep -E '\.(tar|tgz|tbz2|txz|zip|7z)$|\.tar\.' \
             | git check-ignore --no-index --stdin 2>/dev/null)
    [ -z "$pa_fix" ] || bad ".gitignore" "ignores tracked fixtures: $(printf '%s' "$pa_fix" | tr '\n' ' ')"

    pa_scripts=ok
    for s in ports/srclib.sh ports/fetch ports/publish script/hooks/pre-push; do
        if [ ! -f "$s" ]; then
            bad "$s" "missing"; pa_scripts=; continue
        fi
        case "$s" in
            *.sh) ;;
            *) [ -x "$s" ] || { bad "$s" "is not executable"; pa_scripts=; } ;;
        esac
        bash -n "$s" 2>/dev/null || { bad "$s" "bash -n: $(bash -n "$s" 2>&1 | head -1)"; pa_scripts=; }
    done
    [ -n "$pa_scripts" ] && note "srclib.sh, fetch, publish, pre-push parse" "ok"

    # ports/sources.idx is how fetch finds an archived file, and fetch reads
    # it only in format 2: a malformed line is a file nothing can locate, a
    # hash on two lines names two places for one file, two assets one name
    # apart in a tag are one download URL, and a format-1 index is one fetch
    # skips whole. srclib.sh's src_index_problems holds every rule, so
    # ports/publish and this check the same ones. An absent index is skipped,
    # not refused: fetch then resolves every source from the cache or
    # upstream.
    if [ -f ports/sources.idx ]; then
        pa_idx_bad=$(src_index_problems ports/sources.idx)
        pa_idx_n=$(grep -cE '^[0-9a-f]{64} ' ports/sources.idx)
        pa_idx_p=$(grep -cE '^[0-9a-f]{64} .* parts=' ports/sources.idx)
        if [ -n "$pa_idx_bad" ]; then
            bad "ports/sources.idx" "$(printf '%s\n' "$pa_idx_bad" | grep -c .) problem(s): $(printf '%s\n' "$pa_idx_bad" | head -3 | tr '\n' ';')…"
        else
            note "ports/sources.idx" "format 2, $pa_idx_n archived files ($pa_idx_p in parts), well-formed"
            # A legacy src-<name> line still fetches; it is a move not yet
            # made, not a fault.
            pa_idx_old=$(src_index_legacy ports/sources.idx)
            [ "$pa_idx_old" = 0 ] ||
                note "ports/sources.idx" "WARNING: $pa_idx_old lines in legacy src-* releases; make publish-rehome moves them"
        fi
    else
        note "ports/sources.idx" "absent — nothing archived yet"
    fi

    [ "$(git config core.hooksPath 2>/dev/null)" = script/hooks ] \
        || note "pre-push hook" "not enabled — git config core.hooksPath script/hooks"
else
    note "port sources" "skipped — not a git checkout"
fi

# A PATCH SAYS WHAT IT CHANGES, in the Subject: line of its mail header: the
# package page prints that line, and a patch without one is "undescribed" to
# every reader of the book. Only a patch on no remote yet is held to it, so the
# hundreds that predate the rule fail nothing until the patch itself is next
# added, edited or moved. testing/kfscheck.py's `patches` check is the one statement of the
# rule (which lines count as the header, what "new or changed" means), and
# script/hooks/pre-push applies the same rule to the commits it pushes.
echo
echo "==> every new or changed patch carries a Subject: line"
if [ ! -f testing/kfscheck.py ]; then
    bad "testing/kfscheck.py" "missing"
else
    _ph=$(python3 testing/kfscheck.py --only patches 2>&1)
    _phrc=$?
    _phn=0
    while IFS= read -r _l; do
        case "$_l" in
            PATCHHDR*)
                _l=${_l#PATCHHDR}; _l=${_l#"${_l%%[! ]*}"}
                bad "${_l%%: *}" "${_l#*: }"
                _phn=$((_phn + 1)) ;;
        esac
    done <<< "$_ph"
    if [ "$_phrc" = 0 ]; then
        note "patch headers" "$(sed -n 's/^  patches: //p' <<< "$_ph")"
    elif [ "$_phn" = 0 ]; then
        bad "testing/kfscheck.py --only patches" "$(tail -1 <<< "$_ph")"
    fi
fi

# ── every command a KRunner route names must exist ─────────────────────
#
# menu.conf turns a route into a program, and NOTHING else checks it: the file
# parses whatever the command says, the recipe parses, the build succeeds, and
# the failure is a route that opens nothing on a booted ISO.
#
# A command counts as existing when a port of that name is in the tree, when
# some build.sh installs or links it into a bin directory, or when fs/ ships
# it. That is the same question the ISO asks, minus the two hours.
echo
echo "==> every command a menu.conf route names exists"
# `menu.conf` is `route = argv`, so the first word of the value is the program
# — a route naming a command the image does not carry is a name a script can
# hold and nothing can open, which is the one failure a route exists to
# prevent. A key beginning `@` is a setting rather than a route, and is not
# read as a command.
sed -e 's/#.*//' -e '/^[[:space:]]*@/d' -e 's/^[^=]*=//' \
    fs/etc/kdos/menu.conf 2>/dev/null |
    while read -r line; do
        set -- $line
        [ -n "$1" ] && echo "$1"
    done | sort -u | while read -r cmd; do
    [ -n "$cmd" ] || continue
    if has_port "$cmd" ||
       [ -e "fs/usr/local/bin/$cmd" ] || [ -e "fs/usr/bin/$cmd" ] ||
       grep -rqF -- "/bin/$cmd\"" ports/core/*/*/build.sh src/*/*/build.sh \
            2>/dev/null ||
       # ...or in a `for t in …` list, which is what a name installed by a
       # loop looks like: kdos-tools links five of its names that way and the
       # path form never appears in the file at all. Restricted to those lines
       # ON PURPOSE — a bare word search over the whole recipe passes on a
       # COMMENT, which is exactly how a check like this ends up green against
       # a route that opens nothing.
       # CONTINUATIONS ARE JOINED FIRST, the way the mc guard below does it:
       # a backslash-wrapped list puts most of its names on a later line that
       # does not begin with `for`, so every one of them drops out of the
       # match and the guard reports a program that is installed as missing.
       sed -e ':a' -e '/\\$/{N;s/\\\n/ /;ba' -e '}' \
            src/*/*/build.sh 2>/dev/null |
            grep -E '^[[:space:]]*for [A-Za-z_]+ in ' |
            grep -qE "(^|[[:space:]])$cmd([[:space:]]|;|\$)" ||
       # ...or as the `Exec=` of a desktop entry a recipe WRITES. A Python
       # console script is installed by pip from an entry point and appears in
       # no path this can grep: `khal` ships `ikhal` that way. The recipe
       # writing an entry for it is the assertion that it exists, and it is a
       # file in this tree rather than a guess about one.
       grep -rhE "^Exec=$cmd([[:space:]]|\$)" ports/core/*/*/build.sh \
            src/*/*/build.sh 2>/dev/null |
            grep -q . ||
       # ...or as the `Exec=` of a desktop entry one of OUR ports ships. A
       # Plasma program is installed by cmake from a tarball this tree does
       # not unpack, so no recipe line names it; a KDOS entry that launches it
       # is the tree's own assertion that it exists.
       grep -rhsE --include='*.desktop' "^Exec=$cmd([[:space:]]|\$)" src |
            grep -q .; then
        continue
    fi
    echo "MISSING $cmd"
done > "$SP/missing-cmds" 2>/dev/null
if [ -s "$SP/missing-cmds" ]; then
    bad "route commands" "$(tr '\n' ' ' < "$SP/missing-cmds")"
else
    note "route commands" "every one is provided by the tree"
fi

echo
echo "==> every program fs/etc/inittab names is on the image"
#
# THE WHOLE LOGIN PATH IS IN THIS ONE FILE, and nothing reads it until an ISO
# boots. A typo in a name, or a binary a recipe stopped installing, is a tty
# that respawns into nothing — and the tty it takes first is tty1, which is
# the desktop.
#
# EVERY ABSOLUTE WORD OF THE PROCESS FIELD, not just the first: tty2's entry is
# `kdos-getty tty2 /sbin/agetty …` and kdos-getty execs that second path, so a
# program missing THERE is the exact failure this exists to catch. The field
# is everything after the third colon, which is why the id and the runlevels
# are stripped by position rather than by pattern — `ca::ctrlaltdel:` has an
# empty runlevel list and still names a program.
#
# AND WHAT A SCRIPT THE ENTRY NAMES EXECS. tty1's entry is kdos-dm, a script
# fs/ ships, and the login is the program it execs: sddm, or the console login
# it falls back to (kdos-getty and agetty). So every script an entry names is
# read for its `exec` lines, a `$VAR` resolved from that script's own
# `VAR=/path`, one level down.
#
# `-L` AS WELL AS `-e`: an installed program may be an absolute symlink into a
# multi-call binary, which dangles in a staged rootfs and which `-e` alone
# reports as missing.
#
# QUOTES ARE STRIPPED AND RUNTIME TREES SKIPPED. A `sh -c '…'` entry carries
# its programs inside quotes, which would otherwise hide them, and the
# mount points and devices it acts on (/mnt, /run, /tmp, /dev, /proc, /sys)
# are arguments no package installs.
#
# Most of these are on the image and in no `fs/` overlay, so without a build
# tree there is nothing to resolve them against and the check says so rather
# than failing on every line.
_it_n=0
_it_bad=0
_it_named=$(sed -e 's/#.*//' -e '/^[[:space:]]*$/d' fs/etc/inittab 2>/dev/null |
            awk -F: 'NF>=4 { sub(/^[^:]*:[^:]*:[^:]*:/, ""); print }' |
            tr -d "'\"" | tr ' \t' '\n\n' | grep '^/' |
            grep -Ev '^/(mnt|run|tmp|dev|proc|sys)(/|$)' | sort -u)
_it_execd=$(for _p in $_it_named; do
    [ -f "fs$_p" ] && [ "$(head -c2 "fs$_p")" = '#!' ] || continue
    awk '
        match($0, /^[[:space:]]*[A-Za-z_][A-Za-z0-9_]*=\/[^[:space:]";:]+/) {
            kv = substr($0, RSTART, RLENGTH); sub(/^[[:space:]]+/, "", kv)
            n = index(kv, "="); var[substr(kv, 1, n - 1)] = substr(kv, n + 1)
        }
        match($0, /^[[:space:]]*exec[[:space:]]+[^[:space:]]+/) {
            w = substr($0, RSTART, RLENGTH); sub(/^[[:space:]]*exec[[:space:]]+/, "", w); gsub(/"/, "", w)
            if (w ~ /^\$/) { v = w; gsub(/[${}]/, "", v); w = var[v] }
            if (w ~ /^\//) print w
        }' "fs$_p" | sed "s|\$|	$_p|"
done | sort -u)
while IFS='	' read -r _p _by; do
    [ -n "$_p" ] || continue
    _it_n=$((_it_n + 1))
    [ -e "fs$_p" ] || [ -L "fs$_p" ] && continue
    [ -d build/fs/usr/bin ] || continue
    [ -e "build/fs$_p" ] || [ -L "build/fs$_p" ] || {
        if [ -n "$_by" ]; then
            bad "inittab" "$_p is run by $_by, which fs/etc/inittab names, and is installed by nothing"
        else
            bad "inittab" "$_p is named by fs/etc/inittab and is installed by nothing"
        fi
        _it_bad=$((_it_bad + 1))
    }
done < <({ printf '%s\n' $_it_named; printf '%s\n' "$_it_execd"; } | awk -F'\t' 'NF && !seen[$1]++')
if [ "$_it_bad" != 0 ]; then
    :
elif [ "$_it_n" = 0 ]; then
    bad "inittab" "fs/etc/inittab names no program at all"
elif [ -d build/fs/usr/bin ]; then
    note "inittab" "$_it_n programs, every one on the image"
else
    note "inittab" "$_it_n programs; only the ones fs/ ships checked — no build tree"
fi

echo
echo "==> every source file in one of OUR ports is compiled by its recipe"
# A .c that no build.sh names and no glob picks up is a file that passes every
# gate on this machine and fails to LINK in the build — testing/selftest.sh
# globs these directories, so a whole page can be exercised by the harness and
# be absent from the shipped binary. Only our own trees: an upstream tarball
# is entitled to carry sources its own build system chooses between.
: > "$SP/uncompiled"
for d in src/*/*/; do
    b="$d/build.sh"
    [ -f "$b" ] || continue
    # A recipe that globs its OWN source directory compiles whatever is
    # there; nothing to check. The glob has to be anchored to $PORT_SRC: a
    # bare `*.c` test also matches the `"$LIBS"/libk*/*.c` link line that
    # nearly every recipe carries, which exempts precisely the recipes that
    # hand-list their sources — the only ones where a file can be left out.
    grep -qE '("?\$\{?PORT_SRC\}?"?|\.)/\*\.c' "$b" && continue
    # A recipe that runs CMake on its own directory compiles what that
    # directory's CMakeLists.txt names, so a file named there is compiled.
    _lists=$b
    [ -f "${d}CMakeLists.txt" ] && grep -qE 'cmake[[:blank:]]+-S[[:blank:]]+"?\$\{?PORT_SRC\}?"?' "$b" &&
        _lists="$b ${d}CMakeLists.txt"
    for f in "$d"*.c; do
        [ -e "$f" ] || continue
        base=$(basename "$f")
        # A LITERAL FILENAME AT A BOUNDARY, not a bare grep. `grep "store.c"`
        # is a REGEX whose `.` matches any character, so an unrelated
        # `appstore/catalogue` in the same file answered yes for a store.c
        # nothing compiled — and the link failure was the first thing that
        # noticed. The dot is escaped and the name must sit at a path or quote
        # boundary, so `re.c` no longer matches `core.c` either.
        _esc=$(printf '%s' "$base" | sed 's/\./\\./g')
        grep -qE "(^|[/\"'\''( ])$_esc([\"'\'' )]|\$)" $_lists ||
            echo "$d$base" >> "$SP/uncompiled"
    done
done
if [ -s "$SP/uncompiled" ]; then
    bad "port sources" "$(head -3 "$SP/uncompiled" | tr '\n' ' ')not compiled by its build.sh"
else
    note "port sources" "every .c is named or globbed by its recipe"
fi

echo
echo "==> every helper the Makefile runs is on disk"
for _h in $(sed -n 's/^\t.*\bbash \([a-z][a-zA-Z0-9._/-]*\).*/\1/p' Makefile | sort -u); do
    if [ ! -f "$_h" ]; then
        bad "makefile helpers" "$_h is invoked by the Makefile and is not a file"
    fi
done
note "makefile helpers" "every 'bash <path>' resolves"

echo
echo "==> the application catalogue ships"
# THE CATALOGUE MUST BE INSTALLED, not merely present in the tree. A path
# nothing installs is a store that opens empty with no error on the screen:
# cat_load() cannot tell "no applications" from "no file".
grep -q 'usr/share/kdos/appstore/catalogue' src/system/kdos-appbox/build.sh ||
    bad "catalogue" "build.sh does not install it"
[ -f src/system/kdos-appbox/catalogue ] ||
    bad "catalogue" "src/system/kdos-appbox/catalogue is missing"
note "catalogue" "installed by kdos-appbox"

echo
echo "==> no build script NAMES a command inside double quotes and RUNS it"
# A backtick inside a double-quoted echo is not a message, it is a command
# SUBSTITUTION: a packaging step that wrote one ran the named command inside a
# chroot with no Makefile and printed `No rule to make target` from a step that
# was otherwise fine. A diagnostic that names a command the reader should run
# must quote it so the shell does not.
#
# Only ECHO lines are checked, and only in the build tree: a backtick elsewhere
# is ordinary (00_cross/01_gcc.sh uses one to place limits.h) and rewriting
# those buys nothing. `shellcheck` would flag SC2006 on every one of them.
_bt=0
for _f in script/*.sh script/*/*.sh script/phases/*/*.sh; do
    [ -f "$_f" ] || continue
    while IFS= read -r _line; do
        case "$_line" in
            *'`'*) bad "$(basename "$_f")" "an echo names a command in backticks inside double quotes — the shell RUNS it: ${_line#"${_line%%[![:space:]]*}"}"
                   _bt=$((_bt + 1)) ;;
        esac
    done <<EOF
$(grep -nE '^[[:space:]]*echo[[:space:]]+"[^"]*`' "$_f" 2>/dev/null || true)
EOF
done
note "echo backticks" "$((_bt)) build scripts run a command they meant to name"

echo
echo "==> every recipe that runs cargo builds against a shared C library"
# The musl target links statically unless told otherwise. A crate that links a
# system library then takes its .a and none of the libraries that one needs,
# and the link fails at the end of a long compile (cargo-c on libcurl.a), or
# succeeds with a private copy of the library that no update of its port
# reaches.
_cs=0
for _f in ports/core/*/*/build.sh; do
    grep -qE '(^|[[:space:]])cargo[[:space:]]+(build|install|cbuild|cinstall)' "$_f" || continue
    grep -q -- '-crt-static' "$_f" && continue
    bad "$(basename "$(dirname "$_f")")" "runs cargo without RUSTFLAGS=\"-C target-feature=-crt-static\""
    _cs=$((_cs + 1))
done
note "crt-static" "$_cs cargo recipes link statically"

echo
echo "==> no chroot step reads the ports tree through /kdos/ports"
# chroot/exec.sh binds $REPO_ROOT onto /kdos with a NON-RECURSIVE `mount --bind`,
# so the container's own mounts underneath it do not come along: /kdos/ports is
# the empty directory that sat there before docker shadowed it, and the ports
# tree is bound separately at /ports. A step that spells it /kdos/ports finds
# an empty directory on a machine holding every pack, and reports it as awk's
# exit 2 under `set -e`: an empty step log and nothing naming the path.
_kp=0
for _f in script/*/*.sh script/phases/*/*.sh; do
    [ -f "$_f" ] || continue
    if grep -qE '(^|[^#])/kdos/ports/' "$_f" 2>/dev/null; then
        bad "$(basename "$_f")" "reads /kdos/ports — that is an empty shadow in the chroot; use /ports"
        _kp=$((_kp + 1))
    fi
done
note "chroot ports path" "$((_kp)) steps read the ports tree through the shadowed path"

echo
echo "==> every file a build script sources exists, and a step reads its own phase.env"
# A `source` of a missing file is the first line of a step failing, which for
# the image phase is hours into a build; and a step copied from another phase
# that still sources that phase's phase.env runs with the wrong title, repos
# and flags and fails nowhere. A path spelled from the repository root, or from
# the sourcing file's own directory as ${BASH_SOURCE[0]%/*}, is checked; one
# built from any other variable is not, since only the build knows its value.
_src=0 _srcn=0
for _f in script/*.sh script/*/*.sh script/phases/*/*.sh script/phases/*/phase.env \
          script/env/*.env; do
    [ -f "$_f" ] || continue
    _own=${_f#script/phases/}; _own=${_own%%/*}
    while IFS= read -r _s; do
        _s=${_s//\"/}
        _s=${_s//\$\{BASH_SOURCE\[0\]%\/\*\}/${_f%/*}}
        case "$_s" in *'$'*) continue ;; esac
        _srcn=$((_srcn + 1))
        if [ ! -f "$_s" ]; then
            bad "$_f" "sources $_s, which does not exist"
            _src=$((_src + 1))
            continue
        fi
        case "$_f:$_s" in script/phases/*:script/phases/*/phase.env)
            [ "$_s" = "script/phases/$_own/phase.env" ] || {
                bad "$_f" "sources $_s — a step reads its own phase's phase.env"
                _src=$((_src + 1)); } ;;
        esac
    done < <(sed -n 's/^[[:space:]]*\(source\|\.\)[[:space:]]\+\([^[:space:];&|]*\).*/\2/p' "$_f")
done
[ "$_src" = 0 ] && note "sourced files" "$_srcn source line(s) resolve"

echo
echo "==> the catalogue's rows against the tree"
# W8-0 and W9-6. Two lints over apps.plan.md's Part II tables, and both exist
# because the same rows were written twice: nine of that document's "ground
# zero" prerequisites had already LANDED when the section was re-read, and
# twelve of W8's modern-CLI rows were already ports. The check is one listing
# and it is the difference between a wave and a re-litigation.
#
# NEITHER LINT FAILS THE BUILD. A catalogue is a specification, and an
# outstanding row is not a defect — it is work. What it must not do is go
# quiet, so the counts are printed either way and a row that ALREADY EXISTS is
# named, because that is the one that must be struck rather than proposed.
if [ -f apps.plan.md ]; then
    python3 - <<'PYCAT' || true
import os, re

# A port is its directory's name: ports/core/<shelf>/<name>/ and
# src/<area>/<name>/ are both one level of grouping above it.
names = set()
for top in ("ports/core", "src"):
    if not os.path.isdir(top):
        continue
    for g in os.listdir(top):
        d = os.path.join(top, g)
        if os.path.isdir(d):
            names |= {n for n in os.listdir(d)
                      if os.path.isfile(os.path.join(d, n, "kpkgbuild"))}
boxed = set()
if os.path.isfile("src/system/kdos-appbox/catalogue"):
    # ONLY A PACK ROW CARRIES PACKAGES IN COLUMN 3. `group` and `meta` rows put
    # prose there, and slicing them in fills this set with English.
    for line in open("src/system/kdos-appbox/catalogue"):
        f = line.strip().split()
        if not f or f[0].startswith("#"):
            continue
        if f[0] not in ("base", "runtime", "app", "data"):
            continue
        boxed |= {x for x in f[3:] if x != "-"}

rows, have = set(), set()
in_table = False
for line in open("apps.plan.md"):
    if line.startswith("| Name |"):
        in_table = True
        continue
    if in_table and not line.startswith("|"):
        in_table = False
        continue
    if not in_table or line.startswith("|---"):
        continue
    cells = [c.strip() for c in line.strip().strip("|").split("|")]
    if len(cells) < 2:
        continue
    first = re.split(r"[(]", cells[0])[0]
    for tok in re.split(r"\s*\+\s*|\s*/\s*", first):
        tok = tok.strip().strip("`*_").lower()
        if not tok or " " in tok or len(tok) < 2:
            continue
        rows.add(tok)
        if tok in names or tok in boxed:
            have.add(tok)

print("  %-58s %d landed, %d outstanding"
      % ("catalogue rows", len(have), len(rows) - len(have)))
if have:
    shown = sorted(have)
    print("  %-58s %s" % ("already in the tree",
                          ", ".join(shown[:8]) + (" …" if len(shown) > 8 else "")))
PYCAT
else
    note "catalogue" "apps.plan.md is not here — nothing to lint"
fi

# AN `app` ROW FOR A PROGRAM A HOST PORT INSTALLS FAILS. The boxed copy and
# the native one carry the same Name from the same upstream entry, so Kickoff
# and Discover show two identical rows and nothing tells them apart. A package
# on an app row that is a port's directory name is that program; the allowed
# names are libraries and firmware a row pulls into its box, never a program a
# person launches. The lint runs first on two fixtures, a clean row that must
# pass and a kcalc row that must fail: an empty port-name walk would otherwise
# pass every catalogue. KDOS_PREFLIGHT_CATALOGUE points the real run at
# another file.
catalogue_dups() {
    KDOS_DUPCAT_FILE="$1" python3 - <<'PYDUP'
import os

allowed = {"ca-certificates", "curl", "sigrok-firmware-fx2lafw"}
names = set()
for top in ("ports/core", "src"):
    if not os.path.isdir(top):
        continue
    for g in os.listdir(top):
        d = os.path.join(top, g)
        if os.path.isdir(d):
            names |= {n for n in os.listdir(d)
                      if os.path.isfile(os.path.join(d, n, "kpkgbuild"))}
path = os.environ["KDOS_DUPCAT_FILE"]
if os.path.isfile(path):
    for line in open(path):
        f = line.split()
        if len(f) < 4 or f[0] != "app":
            continue
        for pkg in f[3:]:
            if pkg in names and pkg not in allowed:
                print("%s %s" % (f[1], pkg))
PYDUP
}
printf 'meta base - 1\napp app.x rt-gtk nosuchport\n' > "$SP/dupcat-clean"
printf 'meta base - 1\napp app.x rt-kde kcalc\n' > "$SP/dupcat-bad"
if [ -n "$(catalogue_dups "$SP/dupcat-clean")" ] ||
   [ "$(catalogue_dups "$SP/dupcat-bad")" != "app.x kcalc" ]; then
    bad "catalogue" "the host-port lint does not pass its clean fixture and flag its kcalc one"
fi
_dupcat=$(catalogue_dups "${KDOS_PREFLIGHT_CATALOGUE:-src/system/kdos-appbox/catalogue}")
if [ -n "$_dupcat" ]; then
    while read -r _row _pkg; do
        bad "catalogue: $_row" "carries $_pkg, which a host port installs — drop the row"
    done <<<"$_dupcat"
else
    note "catalogue" "no app row repeats a host port"
fi

# ── every program the shipped mc rows name is on the image ──────────────
#
# `mc.ext.ini` and `menu` are TEXT FILES: a row in one cannot hide itself when
# the program it names is missing, the way a surface's own table can. A verb
# still being built therefore belongs in the surfaces and not in these files,
# and this is what refuses one that slipped in.
#
# THE NAME LIST IS BUILT FIRST, and it has to cover the loop form: kdos-tools
# links six names out of one `for t in ...`, so a check that only looked for
# `bin/<name>"` would miss every one of them — and a check that matched the
# loop line itself would match for EVERY name and never fail at all.
echo
echo "==> mc's shipped rows name programs that exist"
_names="$SP/imagenames"
{
    sed -n 's|.*bin/\([a-z][a-z0-9-]*\)".*|\1|p' \
        src/*/*/build.sh 2>/dev/null
    # CONTINUATIONS ARE JOINED FIRST. The loop is matched by its `; do`, which
    # a backslash-wrapped list puts on a later line — and the extraction then
    # silently yields nothing rather than failing, so every name the loop links
    # drops out of the list this guard compares against.
    sed -e ':a' -e '/\\$/{N;s/\\\n/ /;ba' -e '}' \
        src/*/*/build.sh 2>/dev/null |
        sed -n 's/^for t in \(.*\); do/\1/p; s/^for _t in \(.*\); do/\1/p' |
        tr ' ' '\n'
    for _d in "${PORTS_ALL[@]}"; do
        case "$_d" in ports/core/*) printf '%s\n' "${_d##*/}" ;; esac
    done
} | sed 's/[^a-z0-9-]//g' | grep . | sort -u > "$_names"

_mcp=0
for _f in fs/etc/skel/.config/mc/mc.ext.ini fs/etc/skel/.config/mc/menu; do
    [ -f "$_f" ] || continue
    # The first word of a command line, minus a leading `(`; mc's own macros
    # and the shell builtins a row may use are not programs.
    for _p in $(sed -n 's/^Open=(*\([a-z][a-z0-9-]*\).*/\1/p; s/^        (*\([a-z][a-z0-9-]*\) .*/\1/p' \
                "$_f" | sort -u); do
        case "$_p" in cd|for|do|done|test|exit) continue ;; esac
        _mcp=$((_mcp + 1))
        grep -qx "$_p" "$_names" ||
            bad "mc row $_p" "$_f names $_p, which is on no image"
    done
done
note "mc rows" "$_mcp program(s) named, each on the image"

# ── every KDOS handler a mimeapps table names is shipped ────────────────
#
# A row whose desktop id nothing provides falls through to the next candidate
# in SILENCE, so a type the image claims to handle simply opens something else.
# Only the `kdos-*` ids are checked: those are ours to ship, and a row naming a
# port's own entry is the port's to provide.
echo
echo "==> mimeapps rows: every kdos-* handler is shipped"
_mh=0
for _f in fs/etc/xdg/mimeapps.list fs/usr/share/kdos/xdg/kde-mimeapps.list \
          fs/etc/skel/.config/mimeapps.list; do
    [ -f "$_f" ] || continue
    for _id in $(sed -n 's/^[^#=][^=]*=//p' "$_f" | tr ';' '\n' |
                 grep '^kdos-.*\.desktop$' | sort -u); do
        _mh=$((_mh + 1))
        [ -f "fs/usr/share/applications/$_id" ] ||
            find src -name "$_id" 2>/dev/null | grep -q . ||
            bad "mimeapps $_id" "$_f names $_id, which nothing installs"
    done
done
note "mimeapps handlers" "$_mh kdos-* row(s), each with an entry"

# ── AND EVERY OTHER ID, against the build tree ──────────────────────────
#
# The rows above are ours to ship; a row naming a PORT's entry is the port's,
# and no name table in this repo lists the entries a port installs. The build
# tree has them, so that is what is compared against — and the symptom being
# guarded is the same one either way: a row whose id nothing provides falls
# through to the next candidate in silence, so a type the image claims to
# handle simply opens something else.
#
# Skipped, not failed, when there is no build tree.
if [ ! -d build/fs/usr/share/applications ]; then
    note "mimeapps entries" "skipped — no build tree"
else
    _me=0
    _mebad=0
    for _f in fs/etc/xdg/mimeapps.list fs/usr/share/kdos/xdg/kde-mimeapps.list \
              fs/etc/skel/.config/mimeapps.list; do
        [ -f "$_f" ] || continue
        for _id in $(sed -n 's/^[^#=][^=]*=//p' "$_f" | tr ';' '\n' |
                     grep '\.desktop$' | sort -u); do
            _me=$((_me + 1))
            # EVERY DIRECTORY THE OPENER SEARCHES, not just the system one:
            # a box's own entry is generated into $XDG_DATA_HOME and exists
            # nowhere else, so checking /usr/share alone would fail a row the
            # opener resolves perfectly well.
            [ -e "build/fs/usr/share/applications/$_id" ] ||
            [ -e "build/fs/usr/local/share/applications/$_id" ] ||
            [ -e "build/fs/etc/skel/.local/share/applications/$_id" ] || {
                bad "mimeapps $_id" "$_f names $_id, which is on no image"
                _mebad=$((_mebad + 1))
            }
        done
    done
    [ "$_mebad" = 0 ] &&
        note "mimeapps entries" "$_me row(s), each naming an installed entry"
fi

echo
echo "==> every chrome glyph is one the console font can actually draw"
# A GLYPH THE CONSOLE FONT DOES NOT CARRY RENDERS AS A BLANK ON tty1, and
# nothing anywhere says so: the cell is written, the flush succeeds, and the
# desktop is missing a piece of its own chrome on tty1.
#
# `ter-kdos32n` is 512 glyphs. The toolkit picks `glyph_utf8` whenever the
# backend reports UTF-8 — which the Linux console does — so EVERY entry in that
# table has to be in the font, not merely in Unicode. `▓`, the half blocks and
# the double tees all look reasonable in an editor and are all absent from the
# font; a slider built on `▓` draws its filled run as nothing at all.
#
# Checked against the SHIPPED font rather than a list, because the list is the
# thing that goes stale.
_font=build/fs/usr/share/consolefonts/ter-kdos32n.psf.gz
if [ -f "$_font" ]; then
    _glyphbad=$(python3 - "$_font" <<'PYEOF'
import gzip, re, struct, sys

d = gzip.open(sys.argv[1], 'rb').read()
if d[:4] != b'\x72\xb5\x4a\x86':
    sys.exit(0)                     # not PSF2: nothing to check against
ver, hdr, flags, length, charsize, h, w = struct.unpack('<IIIIIII', d[4:32])
if not flags & 1:
    sys.exit(0)                     # no unicode table: the question is unanswerable
cps, cur = set(), b''
for b in d[hdr + length * charsize:]:
    if b in (0xFF, 0xFE):
        cur = b''
    else:
        cur += bytes([b])
        try:
            cps.add(ord(cur.decode('utf8')[0]))
            cur = b''
        except UnicodeDecodeError:
            pass

src = open('src/libs/libktui/ktui_draw.c').read()
tbl = src.split('static const char *glyph_utf8')[1].split('};')[0]
for name, glyph in re.findall(r'\[(KT_G_\w+)\]\s*=\s*"([^"]+)"', tbl):
    for ch in glyph:
        if ord(ch) > 0x7f and ord(ch) not in cps:
            print(f"{name} {ch} U+{ord(ch):04X}")
PYEOF
)
    if [ -n "$_glyphbad" ]; then
        while read -r _n _g _u; do
            bad "glyph $_n" "$_g $_u is not in ter-kdos32n — blank on tty1"
        done <<< "$_glyphbad"
    else
        _ng=$(grep -c '\[KT_G_' src/libs/libktui/ktui_draw.c)
        note "chrome glyphs" "every glyph_utf8 entry is in the console font"
    fi
else
    note "chrome glyphs" "skipped — no build tree"
fi

echo "==> a frame that opens the synchronized bracket closes it on every path"
# A terminal left inside `CSI ?2026h` DRAWS NOTHING FURTHER. That is the whole
# risk of the mode: an unclosed block is not a cosmetic tear, it is a screen
# frozen on the last frame with the program still running behind it. So every
# path out of a bracketed flush writes the close — the frame's own end, the
# dropped-write recovery, and the shutdown that hands the terminal back.
#
# The self-test drives the first; a dropped write needs a terminal that has
# stopped reading, which no test process can hold open, so the other two are
# checked HERE, where the shape of the code is the evidence.
_sync=0
if ! awk '/if \(ktui_term_flush_dropped\(\)\) \{/,/^\t\}$/' \
        src/libs/libktui/ktui_draw.c | grep -q '2026l'; then
    bad "libktui" "a dropped frame leaves the synchronized bracket open"
    _sync=$((_sync + 1))
fi
if ! awk '/^static void leave_screen/,/^\}$/' src/libs/libktui/ktui_term.c |
        grep -q '2026l'; then
    bad "libktui" "the terminal is handed back inside a synchronized bracket"
    _sync=$((_sync + 1))
fi
[ "$_sync" = 0 ] && note "libktui" "the bracket closes on the drop and on the way out"

echo "==> no text is drawn in KT_DIM"
# KT_DIM IS A FILL. It measures about 1.6:1 against the page and IS the
# selection fill, so a word drawn in it is a word nobody can read — and on a
# selected row it is not there at all. Labels take KT_MID; a secondary column
# on a selectable row takes ktui_sel_dim(). The check reads the foreground
# argument of every text draw (ktui_draw_text, _textf, _text_right, _cell,
# ktui_kv, ktui_para, ktui_menu_label, ktui_table_text, ktui_textarea_draw,
# and the slot-taking text wrappers in kdosbuild) across line breaks, so
# `on ? KT_SURFACE : KT_DIM` is caught as surely as a bare KT_DIM. A call
# whose string is a rule, shade or block glyph (ktui_glyph[KT_G_HL], "─") or
# a blank cell is a fill and passes; a "-" does not, because that is what an
# empty cell says, and neither does a button's "[", which is KT_MID.
# Anything else in KT_DIM that is not read — a grid of dots, a rule of
# hyphens, a scrollbar's spent cap, the unpinned half of a pinned/unpinned
# pair of marks — carries `/* not text */` inside the call, which is the whole
# allow list.
_dimtext=$(python3 - src <<'PYEOF'
import glob, re, sys

# function -> (index of the foreground argument, index of the string or None)
TEXT = {
    'ktui_draw_text': (4, 3), 'ktui_draw_text_right': (4, 3),
    'ktui_draw_textf': (3, None), 'ktui_draw_cell': (3, 2),
    'ktui_kv': (5, 4), 'ktui_para': (4, 3), 'ktui_menu_label': (4, 3),
    'ktui_table_text': (5, 4), 'ktui_textarea_draw': (5, 2),
    # Text wrappers that take a slot (kdosbuild/tui.c).
    'centre': (2, 1), 'wrap2': (5, 4), 'hud_cap': (4, 3),
    'hud_glyph': (3, 2),
}
FILL_GLYPH = re.compile(r'^ktui_glyph\[KT_G_(?:HL|VL|TL|TR|BL|BR|CROSS|TEE_[TBLR]'
                        r'|D(?:HL|VL|TL|TR|BL|BR)|FULL|SHADE|SHADE_MED)\]$')
FILL_LIT = re.compile(r'^"[─│▒░█ ]*"$|^\' \'$')
CALL = re.compile(r'\b(' + '|'.join(TEXT) + r')\s*\(')

def strip(s):
    # Comments to blanks and string contents kept, offsets preserved.
    return re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\'',
                  lambda m: m.group(0) if m.group(0)[0] in '"\''
                  else re.sub(r'[^\n]', ' ', m.group(0)), s, flags=re.S)

def args(s, i):
    out, depth, st, k = [], 0, i + 1, i
    while k < len(s):
        c = s[k]
        if c in '"\'':
            # Past the literal, or a "[" in it would count as a bracket.
            k += 1
            while k < len(s) and s[k] != c:
                k += 2 if s[k] == '\\' else 1
        elif c in '([{':
            depth += 1
        elif c in ')]}':
            depth -= 1
            if depth == 0:
                out.append(s[st:k])
                return out, k
        elif c == ',' and depth == 1:
            out.append(s[st:k])
            st = k + 1
        k += 1
    return out, len(s)

for p in sorted(glob.glob(sys.argv[1] + '/**/*.[ch]', recursive=True)):
    raw = open(p, errors='replace').read()
    s = strip(raw)
    for m in CALL.finditer(s):
        a, end = args(s, m.end() - 1)
        fi, si = TEXT[m.group(1)]
        if fi >= len(a) or not re.search(r'\bKT_DIM\b', a[fi]):
            continue
        if si is not None and si < len(a):
            arg = ' '.join(a[si].split())
            # A glyph picked by a ternary is a fill when every glyph it
            # can pick is one.
            names = re.findall(r'\bKT_G_\w+', arg)
            if FILL_LIT.match(arg):
                continue
            if 'ktui_glyph[' in arg and '"' not in arg and names and all(
                    FILL_GLYPH.match('ktui_glyph[%s]' % n) for n in names):
                continue
        if '/* not text */' in raw[m.start():end]:
            continue
        print('%s:%d' % (p, s.count('\n', 0, m.start()) + 1))
PYEOF
) || _dimtext="testing/preflight.sh:the-KT_DIM-scanner-did-not-run"
if [ -n "$_dimtext" ]; then
    for _f in $_dimtext; do
        bad "KT_DIM text: $_f" "a text foreground in KT_DIM — KT_MID, or ktui_sel_dim() on a selectable row"
    done
else
    note "KT_DIM text" "every text draw takes a readable slot"
fi

echo "==> no dark ink on the selection fill"
# THE SELECTION FILL IS KT_DIM, AND DARK INK ON IT IS NOT THERE. A row whose
# background came from ktui_sel_slots() or ktui_sel_row() is lit KT_DIM, so a
# cell on it drawn `on ? KT_SURFACE : ...` — the ink for a KT_ACCENT fill —
# measures about 1.6:1 and the selected row loses the very word it selected.
# The ink on that fill is the fg ktui_sel_slots() returned (KT_TEXT where none
# is in scope), and a secondary column is ktui_sel_dim(). The check takes every
# text, menu-label, sprite and gauge draw inside a function that got its
# background from one of those calls, or whose background is a parameter a
# caller in the same file fills from one, and fails one whose background is
# that fill and whose foreground can be KT_SURFACE or KT_BG, written in the
# call or in a local the function sets. A row lit KT_ACCENT keeps its dark
# ink, because its background never came from the selection rule.
_selink=$(python3 - src <<'PYEOF'
import glob, re, sys

# function -> (index of the foreground argument, index of the background)
DRAW = {
    'ktui_draw_text': (4, 5), 'ktui_draw_text_right': (4, 5),
    'ktui_draw_textf': (3, 4), 'ktui_draw_cell': (3, 4),
    'ktui_draw_sprite': (2, 3), 'ktui_gauge': (4, 5),
    'ktui_menu_label': (4, 5),
}
CALL = re.compile(r'\b(' + '|'.join(DRAW) + r')\s*\(')
SEL = re.compile(r'\bktui_sel_(?:slots|row)\s*\([^;]*&\s*(\w+)\s*\)\s*;')
DARK = re.compile(r'\bKT_(?:SURFACE|BG)\b')

def strip(s):
    return re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\'',
                  lambda m: m.group(0) if m.group(0)[0] in '"\''
                  else re.sub(r'[^\n]', ' ', m.group(0)), s, flags=re.S)

def args(s, i):
    out, depth, st, k = [], 0, i + 1, i
    while k < len(s):
        c = s[k]
        if c in '"\'':
            k += 1
            while k < len(s) and s[k] != c:
                k += 2 if s[k] == '\\' else 1
        elif c in '([{':
            depth += 1
        elif c in ')]}':
            depth -= 1
            if depth == 0:
                out.append(s[st:k])
                return out, k
        elif c == ',' and depth == 1:
            out.append(s[st:k])
            st = k + 1
        k += 1
    return out, len(s)

# The name and parameters right before a "{" in column 0, so a return type
# on the line above the name is found as well as one beside it.
SIG = re.compile(r'\b(\w+)\s*\(((?:[^()]|\([^()]*\))*)\)\s*\n\{')

def dark(b, fg):
    # The ink itself, or a local the function sets to it before the draw.
    if DARK.search(fg):
        return True
    v = fg.strip()
    return bool(re.fullmatch(r'\w+', v)) and any(
        DARK.search(x) for x in
        re.findall(r'\b' + v + r'\s*=(?!=)([^;]*);', b))

def scan(b, fills, p, base, s):
    for m in CALL.finditer(b):
        a, _ = args(b, m.end() - 1)
        fi, bi = DRAW[m.group(1)]
        if bi < len(a) and a[bi].strip() in fills and dark(b, a[fi]):
            print('%s:%d' % (p, s.count('\n', 0, base + m.start()) + 1))

for p in sorted(glob.glob(sys.argv[1] + '/**/*.c', recursive=True)):
    s = strip(open(p, errors='replace').read())
    # A function body runs from a "{" in column 0 to the next "}" there.
    funcs = []
    for sig in SIG.finditer(s):
        end = re.compile(r'^\}', re.M).search(s, sig.end())
        if not end:
            continue
        params = [re.findall(r'\w+', q)[-1] if re.findall(r'\w+', q) else ''
                  for q in sig.group(2).split(',')]
        funcs.append((sig.group(1), params, sig.end() - 1, s[sig.end() - 1:end.end()]))
    # A parameter is a fill when a caller in this file hands it one.
    names = {f[0]: f[1] for f in funcs}
    passed = {}
    for name, params, base, b in funcs:
        fills = set(SEL.findall(b))
        scan(b, fills, p, base, s)
        if not fills:
            continue
        for m in re.finditer(r'\b(' + '|'.join(map(re.escape, names)) + r')\s*\(', b):
            a, _ = args(b, m.end() - 1)
            for i, x in enumerate(a):
                if x.strip() in fills and i < len(names[m.group(1)]):
                    passed.setdefault(m.group(1), set()).add(names[m.group(1)][i])
    for name, params, base, b in funcs:
        if name in passed:
            scan(b, passed[name], p, base, s)
PYEOF
) || _selink="testing/preflight.sh:the-selection-ink-scanner-did-not-run"
if [ -n "$_selink" ]; then
    for _f in $_selink; do
        bad "dark ink on KT_DIM: $_f" "KT_SURFACE/KT_BG text on a ktui_sel_slots() fill — use the fg it returns, or ktui_sel_dim()"
    done
else
    note "selection ink" "every cell on the selection fill takes a readable slot"
fi

echo "==> a literal colour is set at the render boundary and nowhere else"
# CHROME IS SLOTS, ALWAYS. A cell carrying a literal stops following
# `kdos theme`, so the bits that say it has one may be SET in exactly two
# places: the header that defines them, and the translucency pass in
# ktui_draw.c, which MIXES TWO SLOTS of
# the palette in force and does it again on every frame, so a retint moves it
# like everything else (it sets the FOREGROUND bit too, on a reversed cell,
# because that is the half of such a cell the painter shows as a background).
# A surface that set one anywhere else would be a piece
# of chrome wearing a colour a retint cannot move — and it would look right on
# the machine it was written on.
_lit=0
for _f in $(grep -rlE '\|= *\(?(KT_A_FGRGB|KT_A_BGRGB|KT_A_ULCOLOR)|KT_UL_SET\(|attr *= *KT_A_(FGRGB|BGRGB|ULCOLOR)' \
        src/ 2>/dev/null); do
    case "$_f" in
    src/libs/libktui/ktui.h|src/libs/selftest.c) continue ;;
    src/libs/libktui/ktui_draw.c) continue ;;
    esac
    bad "$_f" "sets a literal colour bit — chrome draws in slots"
    _lit=$((_lit + 1))
done
[ "$_lit" = 0 ] &&
    note "colour" "in the header and in the blend"

echo "==> the generated aerc styleset is one aerc will load"
# A KEY IS object[.selected].attribute, and aerc refuses the WHOLE FILE on one
# it cannot parse — so a single wrong key is a mail client that will not start,
# on a machine where nothing else reads this file and nothing else would say so.
_akeys=$(sed -n '/^static void write_aerc/,/^}/p' src/system/kdos-tools/theme.c 2>/dev/null |
         grep -oE '"[A-Za-z_*][A-Za-z0-9_*.]*=' | tr -d '"=')
_aerc=0
for _k in $_akeys; do
    printf '%s\n' "$_k" | grep -qE \
        '^[A-Za-z_*][A-Za-z0-9_*]*(\.selected)?\.(fg|bg|bold|italic|underline|reverse|blink|dim)$' &&
        continue
    bad "aerc styleset" "$_k is not object[.selected].attribute"
    _aerc=$((_aerc + 1))
done
[ "$_aerc" = 0 ] &&
    note "aerc styleset" "$(printf '%s\n' $_akeys | grep -c .) key(s), each one aerc's grammar"

echo
echo "==> every desktop entry's icon and command exist on the image"
#
# A ROW WHOSE ICON RESOLVES TO NOTHING QUIETLY LOSES ITS PICTURE while every
# row beside it keeps one, and a row whose Exec names a program this image
# does not carry opens nothing and says nothing about why. Both are invisible
# to every other check here: the entry parses, the recipe installs it, and the
# defect is only in the Start menu.
#
# THE ICON RULE IS THE FREEDESKTOP SPEC'S, as Plasma's icon loader applies it:
# a name resolves when some installed theme — KDOS, the Breeze it inherits,
# hicolor — carries it as SVG or PNG at any size and in any context, or when
# `pixmaps/` has it.
if [ ! -d build/fs/usr/share/applications ]; then
    note "desktop entries" "skipped — no build tree"
else
    # Every icon name the image carries, read once: a find per entry would
    # walk tens of thousands of files several hundred times. A symlink counts
    # as a file: Breeze carries thousands of its names (speedometer, plasma,
    # text-markdown) as links to another icon, and the loader follows them.
    find build/fs/usr/share/icons \( -type f -o -type l \) \( -name '*.svg' \
        -o -name '*.svgz' -o -name '*.png' \) 2>/dev/null |
        sed 's|.*/||; s/\.svgz\?$//; s/\.png$//' | sort -u > "$SP/icon-names"
    _icon_ok() {
        case "$1" in
            /*) [ -e "build/fs$1" ] || [ -L "build/fs$1" ] && return 0
                return 1 ;;
        esac
        grep -qxF -- "$1" "$SP/icon-names" && return 0
        [ -e "build/fs/usr/share/pixmaps/$1.png" ] ||
            [ -e "build/fs/usr/share/pixmaps/$1.svg" ]
    }
    # -e OR -L. Half the commands here are symlinks into /usr/sbin written
    # with an ABSOLUTE target, which resolves inside the image and dangles on
    # the host running this — so -e alone reports every multicall front end as
    # missing.
    _exec_ok() {
        case "$1" in
            /*) [ -e "build/fs$1" ] || [ -L "build/fs$1" ] && return 0
                return 1 ;;
        esac
        for _d in usr/bin bin usr/sbin sbin usr/games usr/local/bin \
                  usr/local/sbin; do
            [ -e "build/fs/$_d/$1" ] || [ -L "build/fs/$_d/$1" ] && return 0
        done
        return 1
    }
    _de=0; _debad=0
    for _f in build/fs/usr/share/applications/*.desktop; do
        [ -e "$_f" ] || continue
        grep -qi '^NoDisplay=true' "$_f" && continue
        _de=$((_de + 1))
        _n=$(basename "$_f")
        _i=$(sed -n 's/^Icon=//p' "$_f" | head -1)
        _x=$(sed -n 's/^Exec=//p' "$_f" | head -1 | awk '{print $1}')
        if [ -n "$_i" ] && ! _icon_ok "$_i"; then
            bad "desktop entries" "$_n: Icon=$_i is in no installed icon theme and not in pixmaps/"
            _debad=$((_debad + 1))
        fi
        if [ -n "$_x" ] && ! _exec_ok "$_x"; then
            bad "desktop entries" "$_n: Exec=$_x is on no PATH directory of this image"
            _debad=$((_debad + 1))
        fi
    done
    # AND NO TWO VISIBLE ENTRIES MAY SHARE A Name=. The Start menu, the
    # launcher and the search all list entries by their name, so two rows
    # reading `Calendar` are two rows a person cannot choose between. The
    # convention is that the program filling a role keeps the plain name and
    # every alternative is qualified — `Files` and `Files (lf)`.
    _dupe=$(for _f in build/fs/usr/share/applications/*.desktop; do
                [ -e "$_f" ] || continue
                grep -qi '^NoDisplay=true' "$_f" && continue
                sed -n 's/^Name=//p' "$_f" | head -1
            done | sort | uniq -d)
    if [ -n "$_dupe" ]; then
        while IFS= read -r _l; do
            [ -n "$_l" ] || continue
            bad "desktop entries" "two visible entries are both called '$_l'"
            _debad=$((_debad + 1))
        done <<EOF
$_dupe
EOF
    fi
    [ "$_debad" != 0 ] ||
        note "desktop entries" \
             "$_de visible, every icon drawable, every command present, every name distinct"
fi

echo
echo "==> the compiler cache cannot change a byte"
# script/env/chroot.env puts ccache in front of every CMake compile. A hit must
# be the object a compile would write, and four settings decide that:
# CCACHE_BASEDIR rewrites paths so -ffile-prefix-map stops matching them; an
# mtime compiler check cannot tell a rebuilt gcc from the old one, because kpkg
# pins every mtime; a knob the Makefile passes but exec.sh's `env -i` does not
# name never reaches the chroot; and a masquerade directory on PATH makes CMake
# record /usr/lib/ccache/gcc as the compiler in shipped files.
_cr=.
_cc=0
if grep -rqE '^[^#]*CCACHE_BASEDIR' "$_cr"/script/env/ 2>/dev/null; then
    bad "ccache base_dir" "script/env sets CCACHE_BASEDIR — paths leave -ffile-prefix-map and bytes change"
    _cc=$((_cc + 1))
fi
_ccon=$(sed -n '/^if \[ "\${KDOS_CCACHE:-1}" = 1 \]/,/^else/p' "$_cr"/script/env/chroot.env 2>/dev/null)
if [ -z "$_ccon" ] || ! grep -qE '^[^#]*CCACHE_COMPILERCHECK="string:' <<<"$_ccon" \
   || grep -qE '^[^#]*CCACHE_COMPILERCHECK=[^ ]*mtime' <<<"$_ccon"; then
    bad "ccache compiler check" "chroot.env's enabled branch must set CCACHE_COMPILERCHECK to a string, never mtime"
    _cc=$((_cc + 1))
fi
_ccm=0 _cce=0
grep -qE '^[[:space:]]*-e KDOS_CCACHE=' "$_cr"/Makefile && _ccm=1
grep -qE '^[[:space:]]*KDOS_CCACHE=' "$_cr"/script/chroot/exec.sh && _cce=1
if [ "$_ccm" != "$_cce" ]; then
    bad "ccache knob" "KDOS_CCACHE is passed by the Makefile ($_ccm) and named by exec.sh's env -i ($_cce) — both or neither"
    _cc=$((_cc + 1))
fi
if grep -rqE '^[^#]*PATH=[^[:space:]]*/usr/lib/ccache' "$_cr"/script/ 2>/dev/null; then
    bad "ccache masquerade" "a PATH under script/ holds /usr/lib/ccache — CMake records it as the compiler"
    _cc=$((_cc + 1))
fi
[ "$_cc" = 0 ] && note "compiler cache" "no base_dir, string compiler check, knob forwarded, no masquerade PATH"

echo
echo "==> the package store's knobs reach the chroot"
# KDOS_PKG_STORE and KDOS_PKG_STORE_MAX come from the Makefile; exec.sh's
# `env -i` must name them and the three kpkg reads (KPKG_STORE,
# KPKG_STORE_CHECK, KPKG_STORE_SALT), or `make build KDOS_PKG_STORE=1` builds
# every port with the store silently off.
_ps=0
for _v in KDOS_PKG_STORE KDOS_PKG_STORE_MAX; do
    if ! grep -qE "^[[:space:]]*-e $_v=" "$_cr"/Makefile; then
        bad "package store knob" "the Makefile does not pass $_v into the container"
        _ps=$((_ps + 1))
    fi
done
for _v in KDOS_PKG_STORE KDOS_PKG_STORE_MAX KPKG_STORE KPKG_STORE_CHECK KPKG_STORE_SALT; do
    if ! grep -qE "^[[:space:]]*$_v=" "$_cr"/script/chroot/exec.sh; then
        bad "package store knob" "exec.sh's env -i does not name $_v"
        _ps=$((_ps + 1))
    fi
done
if ! grep -qE '! -name pkgstore' "$_cr"/Makefile; then
    bad "package store cleanbuild" "make cleanbuild removes build/pkgstore"
    _ps=$((_ps + 1))
fi
[ "$_ps" = 0 ] && note "package store" "knobs passed by the Makefile and named by exec.sh; cleanbuild keeps it"

echo
echo "==> ninja takes KDOS_JOBS, and only when no job count is given"
# script/bin/ninja is on the chroot's PATH ahead of /usr/bin/ninja. It must add
# -jN to a call with no job count and hand every other call on exactly: a -j
# added twice is harmless, but one added to `-t` or `--version` changes what
# meson and CMake read back, and a lost argument breaks every build. Run
# against a stand-in ninja that prints its arguments one per line, so an
# argument split or joined shows.
_nj=0
if [ -x script/bin/ninja ] && grep -q 'env/../bin\|%/\*}/../bin' script/env/chroot.env; then
    printf '#!/bin/bash\nprintf "%%s\\n" "$@"\n' > "$SP/ninja-real"
    chmod +x "$SP/ninja-real"
    _njcase() {  # expected-output, then the arguments
        local want=$1; shift
        local got
        got=$(KDOS_JOBS=7 KDOS_NINJA="$SP/ninja-real" script/bin/ninja "$@" | paste -sd'|')
        if [ "$got" != "$want" ]; then
            bad "ninja wrapper" "ninja $* gave '$got', want '$want'"
            _nj=$((_nj + 1))
        fi
    }
    _njcase "-j7"
    _njcase "-j7|-C|build|install" -C build install
    _njcase "-j7|-Cbuild|-v" -Cbuild -v
    _njcase "-j7|-C|-j3" -C -j3
    _njcase "-j3|-C|build" -j3 -C build
    _njcase "-C|build|-j|3" -C build -j 3
    _njcase "-vj2" -vj2
    _njcase "-t|compdb|-x" -t compdb -x
    _njcase "--version" --version
    _njcase "-j7|a b|--|-j2" "a b" -- -j2
    _got=$(KDOS_JOBS= KDOS_NINJA="$SP/ninja-real" script/bin/ninja all | paste -sd'|')
    [ "$_got" = all ] || { bad "ninja wrapper" "empty KDOS_JOBS still added a count: '$_got'"; _nj=$((_nj + 1)); }
    grep -qE '^real=\$\{KDOS_NINJA:-/usr/bin/ninja\}' script/bin/ninja ||
        { bad "ninja wrapper" "script/bin/ninja must run /usr/bin/ninja by absolute path"; _nj=$((_nj + 1)); }
else
    bad "ninja wrapper" "script/bin/ninja is missing, or chroot.env does not put script/bin on PATH"
    _nj=1
fi
[ "$_nj" = 0 ] && note "ninja jobs" "-jN only when absent; -t, --version and a given -j pass through"

echo
echo "==> meson setup never downloads, and only setup is changed"
# script/bin/meson sits beside the ninja wrapper on the chroot's PATH. It must
# add --wrap-mode=nodownload to an explicit or implicit setup that chooses no
# wrap mode, and hand every other call on exactly: an option added to
# `compile` or `install` is refused by meson and fails every meson port.
_ms=0
if [ -x script/bin/meson ]; then
    printf '#!/bin/bash\nprintf "%%s\\n" "$@"\n' > "$SP/meson-real"
    chmod +x "$SP/meson-real"
    _mscase() {  # expected-output, then the arguments
        local want=$1; shift
        local got
        got=$(KDOS_MESON="$SP/meson-real" script/bin/meson "$@" | paste -sd'|')
        if [ "$got" != "$want" ]; then
            bad "meson wrapper" "meson $* gave '$got', want '$want'"
            _ms=$((_ms + 1))
        fi
    }
    _mscase "setup|--wrap-mode=nodownload|build|--prefix=/usr" setup build --prefix=/usr
    _mscase "--wrap-mode=nodownload|build" build
    _mscase "--wrap-mode=nodownload|--prefix=/usr|build" --prefix=/usr build
    _mscase "setup|build|--wrap-mode=nofallback" setup build --wrap-mode=nofallback
    _mscase "setup|-D|wrap_mode=default|build" setup -D wrap_mode=default build
    _mscase "setup|-Dwrap_mode=forcefallback|b" setup -Dwrap_mode=forcefallback b
    _mscase "compile|-C|build" compile -C build
    _mscase "install|-C|build|--destdir|a b" install -C build --destdir "a b"
    _mscase "configure|build|-Dx=y" configure build -Dx=y
    _mscase "--version" --version
    _mscase "subprojects|download" subprojects download
    grep -qE '^real=\$\{KDOS_MESON:-/usr/bin/meson\}' script/bin/meson ||
        { bad "meson wrapper" "script/bin/meson must run /usr/bin/meson by absolute path"; _ms=$((_ms + 1)); }
else
    bad "meson wrapper" "script/bin/meson is missing"
    _ms=1
fi
[ "$_ms" = 0 ] && note "meson setup" "--wrap-mode=nodownload on every setup that names no wrap mode; other calls pass through"

echo
echo "==> every recipe carries its metadata, and the catalogue repeats its description"
# testing/recipemeta.py reads every kpkgbuild the way kpkg info, the version
# checker and kdos cve read it: the first line of a key, taken literally. A
# missing description, homepage or license is a recipe those tools print
# nothing for; a license that is not an SPDX expression is one no tool can
# read; a first source that spells the version literally fetches the old
# release after a bump; a description that differs from its row in
# ports-catalogue.md is a book that says one thing while kpkg info says
# another. One line per rule, with a count and the first names; run the script
# alone for every finding.
if python3 testing/recipemeta.py --brief > "$SP/rmeta" 2>&1 || [ -s "$SP/rmeta" ]; then
    while IFS=$'\t' read -r _st _rule _msg; do
        case "$_st" in
            FAIL) bad "recipe $_rule" "$_msg" ;;
            ok)   note "recipe $_rule" "clean" ;;
            *)    bad "recipemeta" "$_st $_rule $_msg" ;;
        esac
    done < "$SP/rmeta"
else
    bad "recipemeta" "testing/recipemeta.py printed nothing"
fi

echo
echo "==> a feature switched off although its library is a port"
# A REPORT, NOT A GATE. testing/optdeps.py maps every --without-X, --disable-X,
# -D…=OFF and -D…=disabled switch to a port through its alias table, and a
# switch whose X is a port is either a choice or a feature lost to the belief
# that X is not a port. The switch cannot say which, so the count is printed
# and the script lists them; a gate would fail on every deliberate one.
_od=$(python3 testing/optdeps.py --count 2>/dev/null)
case "$_od" in
    ''|*[!0-9]*) bad "optdeps" "testing/optdeps.py did not run" ;;
    *) note "optional features off" "$_od switches name a port: python3 testing/optdeps.py" ;;
esac

echo
echo "==> Start lists only what a person starts"
# A visible entry for a server, a session picker, a launch pad, a tray helper
# or a diagnostic is a Start row that opens nothing a person wanted; the rule
# is writing-ports.md's "Desktop entries belong to the port". The words those
# entries carry are matched in Name= and Exec=, and a match is refused unless
# testing/visible-entries.allow names the file: the reviewed entries that read
# that way and are still what a person opens. An allowlist line whose entry is
# no longer visible, or no longer matches, is refused too, so the list cannot
# keep a name nothing needs.
if [ ! -d build/fs/usr/share/applications ]; then
    note "start entries" "skipped — no build tree"
else
    _ve=0
    grep -v '^[[:blank:]]*\(#\|$\)' testing/visible-entries.allow 2>/dev/null |
        sort -u > "$SP/veallow"
    : > "$SP/vematch"
    for _f in build/fs/usr/share/applications/*.desktop; do
        [ -e "$_f" ] || continue
        grep -qi '^NoDisplay=true' "$_f" && continue
        _n=$(sed -n 's/^Name=//p' "$_f" | head -1)
        _x=$(sed -n 's/^Exec=//p' "$_f" | head -1)
        printf '%s %s\n' "$_n" "$_x" |
            grep -qiE '(^|[^[:alpha:]])(server|daemon|session|launch ?pad|tray|latency|diagnos)' ||
            continue
        _b=${_f##*/}
        echo "$_b" >> "$SP/vematch"
        if ! grep -qxF "$_b" "$SP/veallow"; then
            bad "start entries" "$_b ($_n) reads as a server, session, launch pad, tray helper or diagnostic: NoDisplay=true, or name it in testing/visible-entries.allow"
            _ve=$((_ve + 1))
        fi
    done
    while IFS= read -r _b; do
        grep -qxF "$_b" "$SP/vematch" && continue
        bad "start entries" "testing/visible-entries.allow names $_b, which is not a visible matching entry"
        _ve=$((_ve + 1))
    done < "$SP/veallow"
    [ "$_ve" = 0 ] && note "start entries" "$(wc -l < "$SP/vematch") match, every one allowed"
fi

echo
echo "==> every kdos-* command has a manual page"
# `man kdos-<command>` is how a command on this image explains itself, and
# kdos-doc generates one page per command from the book. Read off the built
# tree: the commands are what the ports installed, under every directory on
# the session's PATH.
if [ ! -d build/fs/usr/bin ]; then
    note "kdos manual pages" "skipped — no build tree"
else
    _mp=0; _mpn=""
    _mc=0
    for _c in build/fs/usr/bin/kdos-* build/fs/usr/sbin/kdos-* \
              build/fs/usr/local/bin/kdos-*; do
        [ -e "$_c" ] || [ -L "$_c" ] || continue
        _mc=$((_mc + 1))
        _b=${_c##*/}
        _hit=
        for _m in build/fs/usr/share/man/man[1-8]/"$_b".[1-8]*; do
            [ -e "$_m" ] && { _hit=1; break; }
        done
        [ -n "$_hit" ] && continue
        _mp=$((_mp + 1))
        [ "$_mp" -le 8 ] && _mpn="$_mpn $_b"
    done
    if [ "$_mp" = 0 ]; then
        note "kdos manual pages" "$_mc commands, every one with a page"
    else
        bad "kdos manual pages" "$_mp of $_mc kdos-* commands have no manual page:$_mpn$([ "$_mp" -gt 8 ] && echo ' …')"
    fi
fi

echo
if [ "$fail" = 0 ]; then
    echo "preflight clean — the wiring is consistent"
else
    echo "preflight found $fail problem(s)"
fi
exit $((fail > 0))
