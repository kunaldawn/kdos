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
# seconds: that every package named in a phase list resolves, that every
# recipe parses, that the phase scripts are valid shell, that nothing still
# points at a file the C consolidation removed, and that the shipped rootfs
# carries no script whose interpreter is gone.
#
# It cannot prove the build works. It can prove the build will not fail for one
# of the dull reasons.

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
# a shelf. One awk over every recipe answers both — a process per recipe is
# half a minute of forks.
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
            "llvm libunwind" "llvm openmp" "llvm libclc"; do
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

echo "==> every phase list resolves to a dependency order"
# Each phase resolves with its own phase.env's PORT_REPO (kpkg.conf's
# /ports/core when it sets none), and kpkgdepends passes an unknown name
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

# THE FIRST SOURCE IS THE RECIPE'S FIRST, NOT THE DIRECTORY'S ALPHABETICAL
# FIRST — kpkg strips a component from that one and no other. libime ships a
# dictionary tarball that sorts before its own, so a glob picks the wrong
# archive and reports a shape the build never sees. Mirrors source_file() in
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
    # either, and `option(` may be followed by a NEWLINE before its name, which
    # fcft does, so the option file is flattened before it is read; `option (`
    # with a space, which Impression writes, is the same call.
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

#
# WHAT AN ARCHIVE IS, READ OUT OF THE FILE RATHER THAN ASKED OF `file`.
#
# file(1)'s answer depends on which magic database the machine has, and the
# suite is meant to pass inside kdos-devdeps: the SAME file-5.46 calls the
# 95 MB Noto CJK zip "Zip archive data" on a development host and "data" in
# that container, so the check below reported a good archive as broken in the
# one place it has to be right. A signature is six bytes and no database.
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
        # source_file() in build.c, and this must stay its mirror: an
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
    # the build reads names it. It is counted and reported, never failed:
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
    note "stale fetched archives no recipe names" "$(printf '%s\n' $stale | wc -l) — safe to delete; a fetched copy stays in ports/.srccache"
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
        # `local` is only legal inside a function, and a build.sh IS the
        # function body now — bash accepts it at parse time and dies at RUN
        # time with "can only be used in a function", hours into a build.
        # `bash -n` cannot see it. It is a scar from the recipe conversion:
        # the old format wrapped the build in `build() { ... }`, where local
        # was fine, and the conversion lifted the body out verbatim. No
        # build.sh in the tree defines a function, so any `local` is this bug.
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
         ports/fetch testing/*.sh \
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
# is a file that cannot run. bash and toybox's sh are always there; anything
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
# image time; this says so BEFORE a two-hour build does.
#
# Skipped, not failed, when there is no build tree: preflight's whole point is
# that it needs nothing but the repo.
echo
echo "==> the build tree carries no package whose port is gone"
if [ ! -d build/fs/var/lib/kpkg/db ]; then
    note "orphaned packages" "skipped — no build tree"
else
    orphans=""
    for pkg in $(ls build/fs/var/lib/kpkg/db); do
        has_port "$pkg" || orphans="$orphans $pkg"
    done
    if [ -n "$orphans" ]; then
        bad "orphaned packages" "installed with no recipe:$orphans"
    else
        note "orphaned packages" "none"
    fi
fi

# ── the desktop's own programs link no toolkit and no Xlib ────────────────
#
# Applications may link GTK, Qt, wxWidgets, FLTK, Tk and the X11 client
# libraries; the desktop itself may not. Every ELF that a package built from a
# port under src/ installs is read for its NEEDED entries, and a toolkit or an
# Xlib library among them fails: a compositor, panel or daemon that pulls one
# in makes the session depend on a stack an application is free to leave out.
# libxcb is not on the list. kdos-comp speaks XCB to Xwayland, the one X
# carve-out, and loads no Xlib.
#
# Skipped, not failed, when there is no build tree or no readelf.
echo
echo "==> the desktop's own programs link no GUI toolkit and no Xlib"
if [ ! -d build/fs/var/lib/kpkg/db ]; then
    note "desktop ELF links" "skipped — no build tree"
elif ! command -v readelf >/dev/null 2>&1; then
    note "desktop ELF links" "skipped — no readelf on this host"
else
    _elf_re='^(libgtk-|libgdk-|libadwaita-|libwebkit|libjavascriptcoregtk|libQt[0-9]|libKF[0-9]|libwx_|libfltk|libtk[0-9]|libX[A-Za-z0-9_-]*\.so)'
    _elfn=0 _elfp=0 _elfbad=0
    for _d in src/*/*/; do
        _pkg=$(sed -n 's/^name[[:space:]]*=[[:space:]]*//p' "$_d/kpkgbuild" 2>/dev/null | head -1)
        [ -n "$_pkg" ] && [ -f "build/fs/var/lib/kpkg/db/$_pkg" ] || continue
        _elfp=$((_elfp + 1))
        while IFS= read -r _f; do
            _f=build/fs/${_f#./}
            [ -f "$_f" ] && [ ! -L "$_f" ] || continue
            [ "$(head -c4 "$_f" 2>/dev/null | od -An -c | tr -d ' \n')" = '177ELF' ] || continue
            _elfn=$((_elfn + 1))
            _hit=$(readelf -d "$_f" 2>/dev/null \
                   | sed -n 's/.*(NEEDED).*\[\(.*\)\]/\1/p' | grep -E "$_elf_re" | paste -sd' ')
            [ -z "$_hit" ] && continue
            bad "desktop ELF links" "$_pkg: ${_f#build/fs} links $_hit"
            _elfbad=$((_elfbad + 1))
        done < <(tail -n +2 "build/fs/var/lib/kpkg/db/$_pkg")
    done
    [ "$_elfbad" != 0 ] ||
        note "desktop ELF links" "$_elfn ELF file(s) in $_elfp package(s), none links a toolkit or Xlib"
fi

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
# every account on it — and it makes `kdos-checkpass`, which is setuid root
# precisely so the greeter never opens that file, into decoration.
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
# `kdos-print` runs as the user and administers printers directly, because CUPS
# defines `lpadmin` as exactly that authority and `fs/etc/group` grants it. The
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

# ── the shipped rc.xml must not throw away labwc's default bindings ────────
#
# THE MOST EXPENSIVE ONE-LINE MISTAKE IN THIS TREE.
#
# labwc loads its built-in key and mouse bindings only when the user's config
# defines NONE of that kind (rcxml.c post_processing). A file that binds one
# key therefore silently discards every default — and the defaults are not
# conveniences, they are the desktop: `Client Left Press -> Focus/Raise` is
# what makes CLICKING A WINDOW FOCUS IT (focus_follow_mouse is false), `Title
# Left Drag` is the titlebar, `Close/Iconify/Maximize` are the three buttons
# drawn on every frame, `Border Left Drag` is the edges, and `Root Right Press`
# is the desktop menu.
#
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
fs_conf=src/system/kdos-installer/conf.c
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
# comparing hashes would need a reference build. `limine`, which the rewritten
# installer must mention and the old one cannot, answers it in one grep.
echo
echo "==> the built kinstall is the installer in this tree"
ki_src=src/system/kdos-installer/install.c
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
    # ports/publish and this check the same ones. Absent is fine — nothing is
    # archived yet.
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

# KDOS shipped exactly that file for a release. The symptom on a booted ISO is
# "the mouse does not work" and it is invisible to every other check here: the
# XML is valid, the recipe parses, the build succeeds.
echo
echo "==> the shipped rc.xml keeps labwc's default bindings"
RC=fs/etc/skel/.config/kdos-comp/rc.xml
if [ ! -f "$RC" ]; then
    bad "rc.xml defaults" "$RC is missing"
else
    # AND IT HAS TO BE XML A PARSER WILL TAKE. `--` may not appear inside an
    # XML comment, and this file documents itself in prose that names command
    # arguments: one `--app-id` in a comment makes the whole document
    # ill-formed, and a compositor that cannot parse its configuration loads
    # none of the bindings in it. Nothing else here would notice — every grep
    # below reads the file as text.
    if python3 -c "import sys,xml.dom.minidom;xml.dom.minidom.parse(sys.argv[1])" \
            "$RC" >/dev/null 2>&1; then
        note "rc.xml XML" "well-formed, so labwc reads every binding in it"
    else
        bad "rc.xml XML" "$RC is not well-formed XML — a \`--\` inside a comment is the usual cause"
    fi
    # COMMENTS ARE STRIPPED FIRST, and that is not fussiness: this file's own
    # header explains the trap in prose, so it contains the words <mouse> and
    # <keyboard> and <default /> as TEXT. A grep over the raw file finds them
    # there and passes whatever the config actually says — which is a check
    # that reports on its own documentation.
    awk '
        { line = $0
          while (1) {
              if (inc) { p = index(line, "-->")
                         if (!p) { line = ""; break }
                         line = substr(line, p + 3); inc = 0; continue }
              p = index(line, "<!--")
              if (!p) break
              out = out substr(line, 1, p - 1); line = substr(line, p + 4)
              inc = 1
          }
          out = out line "\n" }
        END { printf "%s", out }
    ' "$RC" > "$SP/rc-nocomment.xml"

    for sect in keyboard mouse; do
        if ! grep -q "<$sect>" "$SP/rc-nocomment.xml"; then
            note "rc.xml <$sect>" "no section — labwc's defaults load"
        elif sed -n "/<$sect>/,/<\/$sect>/p" "$SP/rc-nocomment.xml" \
                | grep -q "<default */>"; then
            note "rc.xml <$sect>" "<default /> present"
        else
            bad "rc.xml <$sect>" \
                "binds something without <default />: every labwc default in that section is discarded"
        fi
    done
fi

# ── every command the desktop's own config names must exist ────────────────
#
# rc.xml and menu.xml are the two files that turn a keystroke or a menu row
# into a program, and NOTHING checks them: the XML is valid whatever the
# command says, the recipe parses, the build succeeds, and the failure is a
# key that does nothing on a booted ISO. This tree has shipped that twice —
# `<promptCommand>` named `labnag`, which is `-Dlabnag=disabled` and therefore
# not on the image at all, and `kdos-desk` called `kdos-appbox open` for a
# release before that subcommand existed.
#
# A command counts as existing when a port of that name is in the tree, when
# some build.sh installs or links it into a bin directory, or when fs/ ships
# it. That is the same question the ISO asks, minus the two hours.
echo
echo "==> every command in the shipped rc.xml, menu.xml and menu.conf exists"
{
for f in fs/etc/skel/.config/kdos-comp/rc.xml \
         fs/etc/skel/.config/kdos-comp/menu.xml; do
    [ -f "$f" ] || continue
    # command="foo -x" and <promptCommand>foo …</promptCommand>; the first
    # word is the program. `foot -e mc` also contributes `mc`, because a
    # terminal wrapper that opens nothing is the same failure one level down.
    { sed -n 's/.*command="\([^"]*\)".*/\1/p' "$f"
      sed -n 's,.*<promptCommand>\([^<]*\)</promptCommand>.*,\1,p' "$f"
    } | while read -r line; do
        set -- $line
        echo "$1"
        [ "$1" = foot ] && [ "$2" = "-e" ] && echo "$3"
    done
done
# AND THE ROUTES. `menu.conf` is `route = argv`, so the first word of the
# value is the program — a route naming a command the image does not carry is
# a name a script can hold and nothing can open, which is the one failure a
# route exists to prevent.
# A key beginning `@` is a SETTING rather than a route: its value is a list
# of menu row labels, not an argument vector, and reading one as a command
# reports the first label as a missing program.
sed -e 's/#.*//' -e '/^[[:space:]]*@/d' -e 's/^[^=]*=//' \
    fs/etc/kdos/menu.conf 2>/dev/null |
    while read -r line; do
        set -- $line
        [ -n "$1" ] && echo "$1"
    done
} | sort -u | while read -r cmd; do
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
       # a desktop that does not work (`-Dlabnag=disabled` matched `labnag`).
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
            grep -q .; then
        continue
    fi
    echo "MISSING $cmd"
done > "$SP/missing-cmds" 2>/dev/null
if [ -s "$SP/missing-cmds" ]; then
    bad "desktop commands" "$(tr '\n' ' ' < "$SP/missing-cmds")"
else
    note "desktop commands" "every one is provided by the tree"
fi

echo
echo "==> kdos-comp's direct-scanout switch has one writer"
#
# The scene's direct_scanout field covers every output, while the phosphor pass
# is decided per output and per frame, so kdos_crt_scanout() writes it before
# every build that reads it: off for the pass's own build, allowed for a plain
# frame. Any other writer holds for the frames after it: an upstream toggle
# (the magnifier has one) lets the pass build from a scanned-out client buffer
# or keeps scanout from the frames it was meant for, and the environment
# variable set in code takes scanout from every frame of the session.
KC=src/desktop/kdos-comp/src
if [ -d "$KC" ]; then
    { grep -rnE 'WLR_PRIVATE\.direct_scanout[[:space:]]*=([^=]|$)' "$KC" |
          grep -v "^$KC/kdos-crt\.c:"
      grep -rnF 'WLR_SCENE_DISABLE_DIRECT_SCANOUT"' "$KC" | grep -F 'setenv'
    } > "$SP/scanout-writers"
    if [ -s "$SP/scanout-writers" ]; then
        bad "direct-scanout writers" \
            "only kdos_crt_scanout() may set it: $(cut -d: -f1,2 "$SP/scanout-writers" | tr '\n' ' ')"
    else
        note "direct-scanout writers" "kdos_crt_scanout() only"
    fi
fi

echo
echo "==> every program fs/etc/inittab names is on the image"
#
# THE WHOLE LOGIN PATH IS IN THIS ONE FILE, and nothing reads it until an ISO
# boots. A typo in a name, or a binary a recipe stopped installing, is a tty
# that respawns into nothing — and the tty it takes first is tty1, which is
# the desktop.
#
# EVERY ABSOLUTE WORD OF THE PROCESS FIELD, not just the first: tty1's entry is
# `kdos-getty tty1 /usr/local/sbin/kdos-login tty1` and kdos-getty execs that
# second path, so a program missing THERE is the exact failure this exists to
# catch. The field is everything after the third colon, which is why the id and
# the runlevels are stripped by position rather than by pattern — `::shutdown:`
# has an empty id and an empty runlevel list and still names a program.
#
# `-L` AS WELL AS `-e`: an installed program may be an absolute symlink into a
# multi-call binary, which dangles in a staged rootfs and which `-e` alone
# reports as missing.
#
# Most of these are on the image and in no `fs/` overlay, so without a build
# tree there is nothing to resolve them against and the check says so rather
# than failing on every line.
_it_n=0
_it_bad=0
for _p in $(sed -e 's/#.*//' -e '/^[[:space:]]*$/d' fs/etc/inittab 2>/dev/null |
            awk -F: 'NF>=4 { sub(/^[^:]*:[^:]*:[^:]*:/, ""); print }' |
            tr ' \t' '\n\n' | grep '^/' | sort -u); do
    _it_n=$((_it_n + 1))
    [ -e "fs$_p" ] || [ -L "fs$_p" ] && continue
    [ -d build/fs/usr/bin ] || continue
    [ -e "build/fs$_p" ] || [ -L "build/fs$_p" ] || {
        bad "inittab" "$_p is named by fs/etc/inittab and is installed by nothing"
        _it_bad=$((_it_bad + 1))
    }
done
if [ "$_it_bad" != 0 ]; then
    :
elif [ "$_it_n" = 0 ]; then
    bad "inittab" "fs/etc/inittab names no program at all"
elif [ -d build/fs/usr/bin ]; then
    note "inittab" "$_it_n programs, every one on the image"
else
    note "inittab" "$_it_n programs; only the ones fs/ ships checked — no build tree"
fi

# ── every flag one kdos-shell tool passes another, the other accepts ──────
#
# kdos-shell is one binary under two dozen names and they spawn each other by
# name with flags on the command line. Nothing checked that the far end knew
# the flag, and an unknown argument in every one of these programs prints a
# usage line to a stderr nobody is reading and exits 2 BEFORE a surface exists.
#
# Three shipped controls were dead that way at once: the panel spawned
# `kdos-cal --at-bottom X Y`, `kdos-menu system --at-bottom X Y` and
# `kdos-menu --windows APP --at-bottom X Y`, and neither kdos-cal nor kdos-menu
# had ever accepted `--at-bottom` — kdos-start and kdos-clip did, which is what
# made it look like a panel fault rather than a missing flag. Clicking the
# clock, the System menu and any grouped task button all did nothing at all,
# silently, and no compile and no golden could see it.
#
# The test is deliberately crude: find the argv literals, take every `--word`
# in them, and require that word to appear as a string in the target's own
# source. A tool that accepts a flag necessarily compares against it.
echo
echo "==> every flag a kdos-shell tool passes another is one it accepts"
python3 - <<'PY' > "$SP/badflags" 2>/dev/null || true
import glob, os, re

SRC = "src/desktop/kdos-shell"
# name -> the file that implements it, from TOOLS[] in main.c
main = open(os.path.join(SRC, "main.c")).read()
tools = dict(re.findall(r'\{\s*"(kdos-[a-z]+)"\s*,\s*([a-z_]+)_main\s*\}', main))
text = {}
for name, fn in tools.items():
    for path in glob.glob(os.path.join(SRC, "*.c")):
        body = open(path).read()
        if re.search(r'\b(int\s+)?%s_main\s*\(' % re.escape(fn), body):
            text.setdefault(name, "")
            text[name] += body
# A front end on sh_run() takes `--font` and `--dump` in shell.c, not in its own
# file, so the runner's source is part of the text its flags are looked for in.
runner = open(os.path.join(SRC, "shell.c")).read()
for name in text:
    if re.search(r'\bsh_run\s*\(', text[name]):
        text[name] += runner

# kdos-res is a separate binary rather than a TOOLS[] name, and the panel and
# the compositor's keybind both spawn it. Its whole source is the text a flag
# has to appear in, for the same reason: an unknown option exits 2 before a
# surface exists, and nothing upstream sees the failure.
RES = "src/desktop/kdos-res"
if os.path.isdir(RES):
    text["kdos-res"] = "".join(open(f).read() for f in glob.glob(os.path.join(RES, "*.c")))

for path in glob.glob(os.path.join(SRC, "*.c")) + glob.glob(os.path.join(RES, "*.c")):
    src = open(path).read()
    # const char *argv[] = { "kdos-foo", "--flag", ... };
    for m in re.finditer(r'argv\[\]\s*=\s*\{(.*?)\}\s*;', src, re.S):
        body = m.group(1)
        words = re.findall(r'"([^"]*)"', body)
        if not words:
            continue
        target = words[0]
        if target not in text:
            continue
        for w in words[1:]:
            if not w.startswith("--"):
                continue
            if ('"%s"' % w) not in text[target]:
                print("%s spawns %s %s — %s never accepts it"
                      % (os.path.basename(path), target, w, target))
PY
if [ -s "$SP/badflags" ]; then
    bad "shell tool flags" "$(head -3 "$SP/badflags" | tr '\n' ';')"
else
    note "shell tool flags" "every spawned flag is accepted"
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
        grep -qE "(^|[/\"'\'' ])$_esc([\"'\'' ]|\$)" "$b" ||
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
echo "==> the application store is wired everywhere it has to be"
# A SURFACE WIRED IN FOUR OF FIVE PLACES IS A CHORD THAT OPENS NOTHING. The
# binary dispatches on its own basename, so a missing symlink, a missing TOOLS
# row, a missing declaration and a missing menu row each fail differently and
# none of them at build time.
for _f in src/desktop/kdos-shell/main.c src/desktop/kdos-shell/build.sh \
          fs/etc/kdos/menu.conf; do
    grep -q 'kdos-store' "$_f" ||
        bad "kdos-store" "not wired in $_f"
done
grep -q 'store_main' src/desktop/kdos-shell/shell.h ||
    bad "kdos-store" "store_main is not declared in shell.h"
# THE CATALOGUE MUST BE INSTALLED, not merely present in the tree. A path
# nothing installs is a store that opens empty with no error on the screen:
# cat_load() cannot tell "no applications" from "no file".
grep -q 'usr/share/kdos/appstore/catalogue' src/system/kdos-appbox/build.sh ||
    bad "catalogue" "build.sh does not install it"
[ -f src/system/kdos-appbox/catalogue ] ||
    bad "catalogue" "src/system/kdos-appbox/catalogue is missing"
note "kdos-store" "the surface is wired in four places and the catalogue ships"

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
echo "==> every consumer of a shared library generates the protocols it includes"
# libkwl is COMPILED INTO each consumer rather than built once, and it includes
# its protocol headers unconditionally. So a protocol added to the library is a
# missing header in every build.sh that did not already generate it — a failure
# that names the library and not the recipe that has to change, and that only
# appears for the consumers a narrowed build happens to reach.
_pg=0
for _p in $(grep -ho '"[a-z0-9-]*-client-protocol\.h"' src/libs/libkwl/*.c 2>/dev/null |
            tr -d '"' | sed 's/-client-protocol\.h$//' | sort -u); do
    for _f in src/*/*/build.sh; do
        [ -f "$_f" ] || continue
        grep -q 'libkwl/\*\.c\|libkwl/kwl\.c' "$_f" 2>/dev/null || continue
        if ! grep -q -- "$_p" "$_f" 2>/dev/null; then
            bad "$(basename "$(dirname "$_f")")" "compiles libkwl but never generates $_p"
            _pg=$((_pg + 1))
        fi
    done
done
note "libkwl protocols" "$((_pg)) consumers are missing a protocol the library includes"

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
for _f in fs/etc/xdg/mimeapps.list fs/etc/xdg/kdos-mimeapps.list \
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
    for _f in fs/etc/xdg/mimeapps.list fs/etc/xdg/kdos-mimeapps.list \
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

# ── every claimed help page exists ──────────────────────────────────────
#
# `KtuiKeys.doc` names a document in /usr/share/kdos/doc and F1 opens it. A
# name with no file there would open an index reading "no such document",
# which teaches that help is broken — worse than a surface that never offered
# it. The rule is enforced here rather than trusted, because the two live in
# different trees and nothing else compares them.
echo
echo "==> help pages: every .doc names a document that ships"
_doc=0
for _d in $(grep -rho 'keys\.doc = "[a-z0-9_-]*"' src/desktop src/daemons src/system src/art 2>/dev/null |
            sed 's/.*"\(.*\)"/\1/' | sort -u); do
    _doc=$((_doc + 1))
    [ -f "fs/usr/share/kdos/doc/$_d.txt" ] ||
        bad "help page $_d" "no fs/usr/share/kdos/doc/$_d.txt"
done
note "help pages" "$_doc claimed, each in fs/usr/share/kdos/doc"

echo
echo "==> every icon name a surface asks for is one the image can resolve"
# A NAME kicon_slot CANNOT RESOLVE RETURNS -1 AND DRAWS A FALLBACK GLYPH, and
# nothing anywhere says so. The surface renders, the flush succeeds, and a row
# that was meant to carry a picture carries a dot. The Start button asked for
# `start-here` that way — the most visible icon on the desktop, silently not an
# icon, because the logo mark was installed into the theme tree and libkicon
# searches the atlas and `icons/hicolor` and not `~/.icons/<theme>`.
#
# Both sources are checked, because both are real lookup paths. Checked against
# what is BUILT rather than a list, for the reason the console-font check is:
# the list is the thing that goes stale.
_atlas=build/fs/usr/share/kdos/icons/atlas.kia
if [ -f "$_atlas" ]; then
    _iconbad=$(python3 - "$_atlas" <<'PYEOF'
import glob, os, re, struct, subprocess, sys

d = open(sys.argv[1], 'rb').read()
if d[:4] != b'KIA1':
    sys.exit(0)
n, = struct.unpack('<I', d[4:8])
names, off = set(), 8
for _ in range(n):
    nlen, size, noff, boff, blen = struct.unpack('<HHIII', d[off:off + 16])
    off += 16
    names.add(d[noff:noff + nlen].decode())

# The other lookup path: hicolor, wherever the build put one.
for p in glob.glob('build/fs/**/icons/hicolor/*/apps/*.png', recursive=True):
    names.add(os.path.basename(p)[:-4])

# Every literal name a surface hands to the icon layer. Three spellings,
# because the name reaches it as an argument, as a row field, or as a table
# column, and a check that knew only one would pass the other two.
pats = (r'kicon_slot(?:_pad)?\("([a-z0-9._-]+)"',
        r'kicon_pixmap\("([a-z0-9._-]+)"',
        r'(?:->|\.)icon = "([a-z0-9._-]+)"')
used = set()
out = subprocess.run(['grep', '-rhoE', '|'.join(pats), 'src/', '--include=*.c'],
                     capture_output=True, text=True).stdout
for line in out.splitlines():
    for pat in pats:
        m = re.search(pat, line)
        if m:
            used.add(m.group(1))
            break

for name in sorted(used - names):
    print(name)
PYEOF
)
    if [ -n "$_iconbad" ]; then
        for _n in $_iconbad; do
            bad "icon $_n" "resolves in neither the atlas nor hicolor"
        done
    else
        note "icon names" "every name resolves in the atlas or in hicolor"
    fi
else
    note "icon names" "skipped — no build tree"
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

echo
echo "==> a desktop toggle has exactly one flag, and libkbase spells the path"
# TWO PLACES ONLY. `kb_toggle_on()` reads and `kb_toggle_set()` writes, and a
# program that builds `kdos/toggles/...` itself is a program looking where
# nothing wrote — the failure is silent in both directions and reads as a
# switch that does nothing.
#
# The daemon keeping a SECOND flag is the same fault a level up: a private
# `dnd` OR'd with the toggle is a state the notification centre's own button
# cannot clear, so Allow Toasts left the toasts silenced and said it had not.
#
# A FORMAT STRING, not the words: every page and header names the directory in
# prose, and only a `%s` beside it is a path being BUILT. libkbase is the two
# places that may, and the library self-test is the third — it spells the
# documented path by hand precisely to prove the library uses it.
_tog=0
for _f in $(grep -rlE 'kdos/toggles.*%s|%s.*kdos/toggles' src/ 2>/dev/null); do
    case "$_f" in
    src/libs/libkbase/*|src/libs/selftest.c) continue ;;
    esac
    bad "$_f" "builds the toggle path itself — use kb_toggle_on/kb_toggle_set"
    _tog=$((_tog + 1))
done
if grep -qE '^static int dnd;' src/desktop/kdos-shell/notifyd.c 2>/dev/null; then
    bad "kdos-notifyd" "keeps a second Do Not Disturb flag beside the toggle"
    _tog=$((_tog + 1))
fi
[ "$_tog" = 0 ] && note "toggles" "one reader, one writer, and no second flag"

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

echo "==> a literal colour is set at the render boundary and nowhere else"
# CHROME IS SLOTS, ALWAYS. A cell carrying a literal stops following
# `kdos theme`, so the bits that say it has one may be SET in exactly four
# places: the render boundary where a terminal's own colour arrives
# (kvt_grid.c), the header that defines them, the accent picker — whose swatches ARE the
# schemes it is offering, so drawing them in slots would show one palette seven
# times — and the translucency pass in ktui_draw.c, which MIXES TWO SLOTS of
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
    src/libs/libkvt/kvt_grid.c) continue ;;
    src/libs/libktui/ktui.h|src/libs/selftest.c) continue ;;
    src/libs/libktui/ktui_draw.c) continue ;;
    src/desktop/kdos-shell/theme.c) continue ;;
    esac
    bad "$_f" "sets a literal colour bit — chrome draws in slots"
    _lit=$((_lit + 1))
done
[ "$_lit" = 0 ] &&
    note "colour" "at the boundary, in the blend, and in the picker"

echo "==> the generated aerc styleset is one aerc will load"
# A KEY IS object[.selected].attribute, and aerc refuses the WHOLE FILE on one
# it cannot parse — so a single wrong key is a mail client that will not start,
# on a machine where nothing else reads this file and nothing else would say so.
_akeys=$(sed -n '/^static void write_aerc/,/^}/p' src/system/kdos-tools/kdos.c 2>/dev/null |
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
echo "==> the control centre's row table is consistent with the files it writes"
# THREE FAULTS THIS TABLE CAN CARRY AND NOTHING ELSE WOULD REPORT.
#
# A DUPLICATE ROW draws twice on its page and edits one value from two places;
# the Panel page carried `task_labels` and `right` twice for a release.
#
# A CHOICE WHOSE DEFAULT IS NOT IN ITS OWN LIST cannot be cycled back to what
# the file says: the cycler searches the list, misses, and starts from the
# first entry — so opening the page and pressing Right once silently changes a
# key nobody touched.
#
# A ST_COMP KEY THAT IS NOT IN comp.conf is a control nothing reads. That file
# is this program's whole contract with the compositor, and a row writing a key
# the compositor has no lookup for is the "change a thing, see nothing" the
# surface exists not to be. It is checked against the SHIPPED copy, which is
# what a fresh account gets and what documents every key.
_srows=$(python3 - <<'PYEOF'
import re

src = open('src/desktop/kdos-shell/settings.c').read()
i = src.index('static struct row rows[] = {')
body = src[i:src.index('\n};', i)]

lists = dict(re.findall(r'static const char \*const (\w+)\[\] = \{([^}]*)\};', src))
conf = open('fs/etc/skel/.config/kdos/comp.conf').read()

# One row is a brace at the start of a line down to the `}` that closes it,
# and its VALUE is the last string literal in it: help, val and orig are all
# concatenated literals, so a fixed field count cannot find them.
starts = [m.start() for m in re.finditer(r'^\t\{ CAT_', body, re.M)]
starts.append(len(body))
seen, bad = set(), []
n = 0
for a, b in zip(starts, starts[1:]):
    row = body[a:b]
    head = re.match(r'\t\{\s*(CAT_\w+),\s*(FT_\w+),\s*(ST_\w+),\s*SC_\w+,'
                    r'\s*(NULL|"[^"]*"),\s*"[^"]*",\s*(NULL|\w+),\s*(-?\d+),',
                    row)
    if not head:
        bad.append('a row does not parse: ' + row.split('\n')[0].strip())
        continue
    n += 1
    cat, ft, st, key, choices, nch = head.groups()
    lits = re.findall(r'"((?:[^"\\]|\\.)*)"', row)
    val = lits[-2] if len(lits) >= 2 else ''
    if key == 'NULL':
        continue
    k = key.strip('"')
    if (cat, st, k) in seen:
        bad.append('%s %s %s is in the table twice' % (cat, st, k))
    seen.add((cat, st, k))
    if ft == 'FT_CHOICE' and choices != 'NULL':
        items = [x.strip().strip('"') for x in lists.get(choices, '').split(',')
                 if x.strip()]
        if items and val not in items:
            bad.append('%s %s default "%s" is not in %s' % (cat, k, val,
                                                            choices))
        if items and int(nch) != len(items):
            bad.append('%s %s says %s choices, %s has %d'
                       % (cat, k, nch, choices, len(items)))
    if st == 'ST_COMP' and not re.search(r'^#?\s*%s\s*=' % re.escape(k), conf,
                                         re.M):
        bad.append('ST_COMP %s is not a key in comp.conf' % k)

for x in bad:
    print('BAD ' + x)
print('N %d' % n)
PYEOF
)
_sbad=$(printf '%s\n' "$_srows" | grep '^BAD ' | sed 's/^BAD //')
if [ -n "$_sbad" ]; then
    while IFS= read -r _l; do bad "settings rows" "$_l"; done <<EOF
$_sbad
EOF
else
    note "settings rows" "$(printf '%s\n' "$_srows" | sed -n 's/^N //p') row(s), \
each one key, one list, one comp.conf line"
fi

echo
echo "==> every desktop entry's icon and command exist on the image"
#
# A ROW WHOSE ICON RESOLVES TO NOTHING QUIETLY LOSES ITS PICTURE while every
# row beside it keeps one, and a row whose Exec names a program this image
# does not carry opens nothing and says nothing about why. Both are invisible
# to every other check here: the entry parses, the recipe installs it, and the
# defect is only in the Start menu.
#
# THE ICON RULE IS THE ATLAS'S, NOT THE FREEDESKTOP SPEC'S. genatlas.py takes
# six contexts at four sizes; `panel/`, `apps/` and 16x16 are deliberately not
# among them, so `file-manager` and `utilities-terminal` are names the spec
# blesses and this image cannot draw. After the atlas, libkicon falls back to
# hicolor's `apps/` PNGs and to `pixmaps/` — SVG there is never read, because
# nothing in the session rasterises one.
if [ ! -d build/fs/usr/share/applications ]; then
    note "desktop entries" "skipped — no build tree"
else
    _ATLAS_SIZES="24 32 48 64"
    _ATLAS_CTX="places devices status mimetypes actions emblems"
    _icon_ok() {
        case "$1" in
            /*) [ -e "build/fs$1" ] || [ -L "build/fs$1" ] && return 0
                return 1 ;;
        esac
        for _s in $_ATLAS_SIZES; do
            for _c in $_ATLAS_CTX; do
                [ -e "build/fs/usr/share/icons/KDOS/${_s}x${_s}/$_c/$1.svg" ] &&
                    return 0
            done
        done
        for _h in build/fs/usr/share/icons/hicolor/*/apps/"$1".png; do
            [ -e "$_h" ] && return 0
        done
        [ -e "build/fs/usr/share/pixmaps/$1.png" ]
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
            bad "desktop entries" "$_n: Icon=$_i is in no atlas context and no hicolor apps/ PNG"
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
if [ "$fail" = 0 ]; then
    echo "preflight clean — the wiring is consistent"
else
    echo "preflight found $fail problem(s)"
fi
exit $((fail > 0))
