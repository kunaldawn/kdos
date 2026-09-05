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
# seconds: that every package named in a packages.txt resolves, that every
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
   -Isrc/packages/kdos-kpkg \
   -o "$SP/kdos-kpkg" src/packages/kdos-kpkg/*.c src/libs/libkbase/*.c \
   src/libs/libkpkg/*.c src/libs/libksig/*.c \
   src/libs/libksig/monocypher/*.c || { echo "  cannot build kpkg"; exit 1; }
# Installed as five names and dispatched on its own basename, so the checks
# have to invoke it the same way — `kpkg kpkgdepends ...` correctly reaches
# kpkg's front end and prints usage, which is not what is being tested.
for n in kpkg kpkgadd kpkgbuild kpkgdel kpkgdepends; do
    ln -sf kdos-kpkg "$SP/$n"
done

# src/desktop is the third port repo — the compositor and shell live there,
# and script/desktop.env.sh puts it on PORT_REPO for the desktop phase. This
# has to match, or preflight reports ports that build fine as missing.
export PORT_REPO="$PWD/ports/core $PWD/src/packages $PWD/src/desktop"
export KPKG_CONF=/nonexistent PKGDB_DIR=/dev/null

echo
echo "==> every package named in a packages.txt has a port"
for f in script/*/packages.txt; do
    missing=""
    while read -r p; do
        [ -z "$p" ] && continue
        case "$p" in \#*) continue ;; esac
        if [ ! -f "ports/core/$p/kpkgbuild" ] && \
           [ ! -f "src/packages/$p/kpkgbuild" ] && \
           [ ! -f "src/desktop/$p/kpkgbuild" ]; then
            missing="$missing $p"
        fi
    done < "$f"
    [ -z "$missing" ] && note "$f" "ok" || bad "$f" "no port for:$missing"
done

echo
echo "==> every packages.txt resolves to a dependency order"
for f in script/*/packages.txt; do
    pkgs=$(grep -v '^#' "$f" | grep -v '^$' | tr '\n' ' ')
    [ -z "$pkgs" ] && continue
    out=$("$SP/kpkgdepends" $pkgs 2>"$SP/err")
    if [ -s "$SP/err" ]; then
        bad "$f" "kpkgdepends wrote to stderr: $(head -1 "$SP/err")"
    elif [ -z "$out" ]; then
        bad "$f" "kpkgdepends returned nothing"
    elif printf '%s' "$out" | tr ' ' '\n' | grep -qvE '^[A-Za-z0-9][A-Za-z0-9._+-]*$'; then
        bad "$f" "a resolved token is not a package name"
    else
        note "$f" "$(echo "$out" | wc -w) packages"
    fi
done

echo
echo "==> every depends key names a port that exists"
orphans=0
for d in ports/core/* src/packages/* src/desktop/*; do
    [ -f "$d/kpkgbuild" ] || continue
    for dep in $(sed -n 's/^depends[[:blank:]]*=[[:blank:]]*//p' "$d/kpkgbuild"); do
        if [ ! -f "ports/core/$dep/kpkgbuild" ] && \
           [ ! -f "src/packages/$dep/kpkgbuild" ] && \
           [ ! -f "src/desktop/$dep/kpkgbuild" ]; then
            bad "$(basename "$d")" "depends on '$dep', which has no port"
            orphans=$((orphans + 1))
        fi
    done
done
[ "$orphans" = 0 ] && note "all depends resolve" "ok"

echo
echo "==> every port of OURS is built by something"
# The reverse of the check above, and the one a NEW port needs. `ports/core` is
# upstream and a recipe there may legitimately sit unbuilt; `src/packages` and
# `src/desktop` are ours, and a port nobody installs is a directory that
# compiles on a developer's machine and is absent from the ISO. kdos-oomd is
# the live example: a daemon, an init script and a recipe, and one missing line
# in script/05_desktop/packages.txt between it and never running.
#
# A port a PHASE SCRIPT builds by name is fine — that is how kinstall is built,
# in phase 1, long before any packages.txt exists.
unbuilt=0
for d in src/packages/* src/desktop/*; do
    [ -f "$d/kpkgbuild" ] || continue
    p=$(basename "$d")
    grep -qxF "$p" script/*/packages.txt 2>/dev/null && continue
    grep -rqlF "$p" script/*/*.sh 2>/dev/null && continue
    bad "$p" "in no packages.txt and named by no phase script — nothing builds it"
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
    while read -r dpath; do
        case "$(basename "$dpath")" in kdos-*) ;; *) continue ;; esac
        if ! grep -rqF -- "$dpath\"" ports/core/*/build.sh src/packages/*/build.sh \
                src/desktop/*/build.sh 2>/dev/null; then
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
for d in ports/core/*/ src/packages/*/ src/desktop/*/; do
    [ -f "$d/build.sh" ] || continue
    t="$d$(first_source_file "$d")"
    case "$t" in *.tar.*|*.tgz|*.tbz2|*.txz) ;; *) continue ;; esac
    [ -f "$t" ] || continue
    first=$(tar tf "$t" 2>/dev/null | head -1)
    case "$first" in
    ./*)
        dotp=$((dotp + 1))
        # Accounted for means the recipe descends into the directory the strip
        # left behind. Anything else is the silent one-level-down failure.
        grep -qE '^[[:space:]]*cd[[:space:]]+"?[A-Za-z0-9_.-]*\$\{?(name|version)' "$d/build.sh" \
            || bad "$(basename "$d")" "first source is ./-prefixed and build.sh never cds into the stripped directory"
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
for d in ports/core/*/ src/packages/*/ src/desktop/*/; do
    [ -f "$d/build.sh" ] || continue
    t="$d$(first_source_file "$d")"
    case "$t" in *.tar.*|*.tgz|*.tbz2|*.txz) ;; *) continue ;; esac
    [ -f "$t" ] || continue
    tops=$(tar tf "$t" 2>/dev/null | head -300 | awk -F/ 'NF>1 || $1!="" {print $1}' | sort -u | wc -l)
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
# A port whose tarball is absent (a release asset, not in git) is SKIPPED
# rather than failed — `make bootstrap` is what puts it there.
meson_checked=0
meson_builtin="auto_features backend b_asneeded b_colorout b_coverage b_lto \
b_lundef b_ndebug b_pch b_pgo b_sanitize b_staticpic b_vscrt buildtype \
debug default_both_libraries default_library errorlogs install_umask \
layout optimization pkg_config_path prefer_static strip unity unity_size \
warning_level werror wrap_mode"
for d in ports/core/*/ src/packages/*/ src/desktop/*/; do
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
        flat=$(tar -xOf "$t" --wildcards '*/meson_options.txt' '*/meson.options' \
               2>/dev/null | tr '\n' ' ' || true)
        defined="$defined
$(printf '%s' "$flat" | grep -oE "option\([[:space:]]*'[a-zA-Z0-9_-]+" \
          | sed "s/.*'//" || true)"
        deftypes="$deftypes
$(printf '%s' "$flat" \
          | grep -oE "option\([[:space:]]*'[a-zA-Z0-9_-]+'[[:space:]]*,[[:space:]]*type[[:space:]]*:[[:space:]]*'[a-z]+'" \
          | sed -E "s/option\([[:space:]]*'([a-zA-Z0-9_-]+)'.*'([a-z]+)'\$/\\1\t\\2/" || true)"
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
    # fcft does, so the option file is flattened before it is read.
    cmdlines=$(grep -v '^[[:space:]]*#' "$d/build.sh" | grep -v 'install ')
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
echo "==> every source a port ships is named by a sha256 in its recipe"
# kpkg refuses to extract a source it has no hash for, so a gap here is a
# port that cannot build. The enumeration is the RECIPE's own source list
# read through the same parser the build uses, NOT a glob of archive
# extensions: that glob knew about six suffixes, so ca-certificates' .pem
# and iana-etc's four plain files were invisible here and failed instead
# two hours into phase 3. The hashes were bootstrapped from the git-LFS
# pointers, where the oid IS the file's sha256.
unhashed=0
for d in ports/core/* src/packages/* src/desktop/*; do
    [ -f "$d/kpkgbuild" ] || continue
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
        [ -f "$d/$base" ] || continue
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
                if ! file -b "$d/$base" | grep -qiE 'compress|archive|Zip'; then
                    bad "$p" "ships $base, which is $(file -b "$d/$base" | cut -c1-40), not an archive"
                    unhashed=$((unhashed + 1))
                fi
                ;;
            esac
        fi
    done

    # AN ARCHIVE THE RECIPE CANNOT NAME IS ONE THE BUILD WILL NOT FIND. The
    # loop above skips a resolved name that is not on disk, which is right —
    # tarballs are fetched rather than committed. The consequence is that a
    # file sitting there under some OTHER name is invisible to every check
    # here and fails at `Source not found`, minutes into a phase. That is what
    # a hand-placed download looks like: kpkg renames a FIRST source to
    # <name>-<version>.<ext> and a `.tgz` saved under the URL's own suffix
    # matches nothing.
    for f in "$d"/*.tar.* "$d"/*.tgz "$d"/*.tbz2 "$d"/*.txz "$d"/*.zip; do
        [ -f "$f" ] || continue
        fb=${f##*/}
        case " $resolved " in *" $fb "*) continue ;; esac
        [ "$fb" = "$name-vendor-$version.tar.xz" ] && continue
        bad "$p" "ships $fb, which no 'source =' line resolves to"
        unhashed=$((unhashed + 1))
    done

    # A VENDOR BUNDLE IS A SOURCE THIS PORT BUILDS FROM AND IS NOT IN `source`.
    # `ports/fetch` generates it from `vendoring =`, build.sh untars it out of
    # $PORT_SRC, and the loop above enumerates the recipe's own source list —
    # so a bundle with no sha256 line beside it is invisible here and verified
    # by nothing, anywhere.
    if [ -n "${vendoring:-}" ]; then
        vf="$name-vendor-$version.tar.xz"
        if [ ! -f "$d/$vf" ]; then
            bad "$p" "declares vendoring=$vendoring and ships no $vf"
            unhashed=$((unhashed + 1))
        elif [ ! -s "$d/$vf" ]; then
            bad "$p" "ships $vf as an empty file"
            unhashed=$((unhashed + 1))
        elif ! grep -q "^sha256[[:blank:]]*=.*[[:blank:]]$vf\$" "$d/kpkgbuild"; then
            bad "$p" "ships $vf with no sha256 line"
            unhashed=$((unhashed + 1))
        fi
    fi
done
[ "$unhashed" = 0 ] && note "every source is hashed and non-empty" "ok"

# The escape hatch must be unused in a committed tree.
if grep -rq "KDOS_ALLOW_UNVERIFIED" ports/core/*/kpkgbuild src/packages/*/kpkgbuild src/desktop/*/kpkgbuild 2>/dev/null; then
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
for r in src/packages/kdos-tools/reasons/*.txt; do
    [ -f "$r" ] || continue
    _rn=$(basename "$r" .txt)
    grep -q "^title:" "$r" || { bad "$_rn" "has no title:"; rot=$((rot + 1)); }
    grep -q "^path:\|^port:" "$r" || { bad "$_rn" "claims nothing"; rot=$((rot + 1)); }

    sed -n 's/^port:[[:blank:]]*//p' "$r" | while read -r _p; do
        [ -n "$_p" ] || continue
        if [ ! -f "ports/core/$_p/kpkgbuild" ] && [ ! -f "src/packages/$_p/kpkgbuild" ] && \
           [ ! -f "src/desktop/$_p/kpkgbuild" ]; then
            bad "$_rn" "names port '$_p', which no longer exists"
        fi
    done

    # A path is real if fs/ provides it, or if the reason also names a port
    # (which is what installs it — preflight cannot see an installed tree).
    _hasport=$(grep -c "^port:" "$r")
    sed -n 's/^path:[[:blank:]]*//p' "$r" | while read -r _q; do
        [ -n "$_q" ] || continue
        if [ ! -e "fs$_q" ] && [ "$_hasport" = 0 ]; then
            bad "$_rn" "names path '$_q', which fs/ does not provide and no port claims"
        fi
    done

    sed -n 's/^see:[[:blank:]]*//p' "$r" | while read -r _s; do
        [ -n "$_s" ] || continue
        [ -f "src/packages/kdos-tools/reasons/$_s.txt" ] || bad "$_rn" "sees '$_s', which is not a reason"
    done
done
[ "$rot" = 0 ] && note "reasons resolve" "$(ls src/packages/kdos-tools/reasons/*.txt 2>/dev/null | wc -l) recorded"

echo
echo "==> every port has a build.sh, and it parses"
# The build is a shell script in its own file, so it can actually be checked:
# `bash -n` on 396 recipes is a real syntax gate, and it was impossible while
# the build lived inside the recipe.
missing=0
scripts=0
for d in ports/core/* src/packages/* src/desktop/*; do
    [ -f "$d/kpkgbuild" ] || continue
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
for d in ports/core/* src/packages/* src/desktop/*; do
    [ -f "$d/kpkgbuild" ] || continue
    p=$(basename "$d")
    out=$("$SP/kpkg" meta "$d" 2>"$SP/err")
    if [ -s "$SP/err" ] || [ -z "$out" ]; then
        bad "$p" "kpkgbuild does not parse: $(head -1 "$SP/err")"
    fi
done
note "recipe metadata" "all recipes parse"

echo
echo "==> every recipe declares a name, version and release"
for d in ports/core/* src/packages/* src/desktop/*; do
    [ -f "$d/kpkgbuild" ] || continue
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
note "recipe fields" "checked $(ls -d ports/core/*/ src/packages/*/ 2>/dev/null | wc -l) ports"

echo
echo "==> shell that ships or builds is syntactically valid"
for f in script/*.sh script/*/*.sh fs/etc/init.d/* fs/usr/share/kdos/init \
         ports/appbox/fetch ports/fetch testing/*.sh \
         ports/core/*/build.sh src/packages/*/build.sh \
         ports/core/*/postinstall.sh src/packages/*/postinstall.sh; do
    [ -f "$f" ] || continue
    case "$f" in *packages.txt) continue ;; esac
    bash -n "$f" 2>"$SP/err" || bad "$f" "$(head -1 "$SP/err")"
done
note "shell syntax" "ok"

echo
echo "==> nothing still points at a file the rewrite removed"
for gone in fs/usr/local/bin/kdos fs/usr/local/bin/kdos-banner \
            fs/usr/local/bin/kdos-shot fs/usr/local/bin/kdos-fetch-app \
            fs/usr/local/bin/kdos-fetch-static fs/usr/sbin/service \
            fs/usr/local/sbin/kdos-getty src/kpkg/kpkg \
            ports/appbox/pack ports/appbox/assemble \
            ports/appbox/genlaunchers.py; do
    [ -e "$gone" ] && bad "$gone" "should have been removed"
done
# Only things that would INVOKE the removed tools count. A C file naming one in
# a comment is documenting what it replaced, which is the point.
hits=$(grep -rln 'python3 .*genlaunchers\|python3 .*pack \|python3 .*assemble\|python3 .*gengtk\|python3 .*genicons\|python3 .*gencursors' \
        script ports fs Makefile 2>/dev/null || true)
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
            [ -d ports/core/nftables ] \
                || bad "$f" "interpreter $interp has no port" ;;
        *) bad "$f" "unexpected interpreter $interp" ;;
    esac
done
note "rootfs interpreters" "every #! is provided by the tree"

# ── the build tree still carries packages whose port is gone ───────────────
#
# `fs/` is manifest-guarded, packages were not, and the build tree is
# incremental: a port deleted from `ports/` left its package installed forever.
# Measured on v0.2 — the ISO shipped all sixteen `cosmic-*` packages,
# `pop-launcher`, `kdos-theme-helper` and `xdg-desktop-portal-cosmic`, 529 MB of
# a desktop removed a milestone earlier. `06_packaging/00_orphans.sh` sweeps
# them at package time; this says so BEFORE a two-hour build does.
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
        if [ ! -f "ports/core/$pkg/kpkgbuild" ] && \
           [ ! -f "src/packages/$pkg/kpkgbuild" ] && \
           [ ! -f "src/desktop/$pkg/kpkgbuild" ]; then
            orphans="$orphans $pkg"
        fi
    done
    if [ -n "$orphans" ]; then
        bad "orphaned packages" "installed with no recipe:$orphans"
    else
        note "orphaned packages" "none"
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
for f in ports/core/*/kpkgbuild ports/core/*/build.sh ports/core/*/postinstall.sh \
         src/packages/*/kpkgbuild src/packages/*/build.sh src/packages/*/postinstall.sh \
         src/desktop/*/kpkgbuild src/desktop/*/build.sh src/desktop/*/postinstall.sh; do
    [ -f "$f" ] || continue
    grep -q "KD's Homebrew Linux Distro" "$f" || {
        bad "${f#ports/core/}" "has no KDOS banner header"
        noban=$((noban + 1))
    }
done
[ "$noban" = 0 ] && note "banner header" "present in every recipe and build script"

# A ROOT FILESYSTEM THE INITRAMFS CANNOT MOUNT INSTALLS PERFECTLY AND NEVER
# BOOTS AGAIN, and nothing else here would see it: `ki_filesystems[]` is what
# the installer OFFERS and `01_initramfs.sh`'s MODULES line is what makes the
# offer bootable. The two are edited in different languages in different
# directories, so a row added to one and not the other compiles, passes every
# other gate, and bricks exactly the machine that picked it.
#
# ext4 and btrfs are built into this kernel, so they are not expected in
# MODULES; anything else in the table must be there by name.
echo
echo "==> every filesystem the installer offers, the initramfs can mount"
fs_conf=src/packages/kdos-installer/conf.c
fs_ini=script/06_packaging/01_initramfs.sh
if [ -f "$fs_conf" ] && [ -f "$fs_ini" ]; then
    fs_mods=$(grep -E '^MODULES=' "$fs_ini")
    fs_missing=""
    for fs in $(sed -n '/^const Filesystem ki_filesystems\[\]/,/^};/p' "$fs_conf" \
                | grep -oE '^\s*\{ "[a-z0-9]+"' | grep -oE '"[a-z0-9]+"' | tr -d '"'); do
        case "$fs" in ext4|btrfs) continue ;; esac
        case "$fs_mods" in *" $fs "*|*" $fs\""*) ;; *) fs_missing="$fs_missing $fs" ;; esac
    done
    if [ -n "$fs_missing" ]; then
        bad "01_initramfs.sh" "ki_filesystems[] offers$fs_missing, which the initramfs does not carry"
    else
        note "installer filesystems" "every offered root fs is in the initramfs"
    fi
else
    note "installer filesystems" "skipped — conf.c or 01_initramfs.sh not found"
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
echo "==> every command in the shipped rc.xml and menu.xml exists"
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
done | sort -u | while read -r cmd; do
    [ -n "$cmd" ] || continue
    if [ -d "ports/core/$cmd" ] || [ -d "src/packages/$cmd" ] ||
       [ -d "src/desktop/$cmd" ] ||
       [ -e "fs/usr/local/bin/$cmd" ] || [ -e "fs/usr/bin/$cmd" ] ||
       grep -rqF -- "/bin/$cmd\"" ports/core/*/build.sh src/packages/*/build.sh \
            src/desktop/*/build.sh 2>/dev/null ||
       # ...or in a `for t in …` list, which is what a name installed by a
       # loop looks like: kdos-tools links five of its names that way and the
       # path form never appears in the file at all. Restricted to those lines
       # ON PURPOSE — a bare word search over the whole recipe passes on a
       # COMMENT, which is exactly how a check like this ends up green against
       # a desktop that does not work (`-Dlabnag=disabled` matched `labnag`).
       grep -rhE '^[[:space:]]*for [A-Za-z_]+ in ' src/packages/*/build.sh \
            src/desktop/*/build.sh 2>/dev/null |
            grep -qE "(^|[[:space:]])$cmd([[:space:]]|;|\$)"; then
        continue
    fi
    echo "MISSING $cmd"
done > "$SP/missing-cmds" 2>/dev/null
if [ -s "$SP/missing-cmds" ]; then
    bad "desktop commands" "$(tr '\n' ' ' < "$SP/missing-cmds")"
else
    note "desktop commands" "every one is provided by the tree"
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
for d in src/desktop/*/ src/packages/*/; do
    b="$d/build.sh"
    [ -f "$b" ] || continue
    # A recipe that globs *.c compiles whatever is there; nothing to check.
    grep -q '\*\.c' "$b" && continue
    for f in "$d"*.c; do
        [ -e "$f" ] || continue
        base=$(basename "$f")
        grep -q "$base" "$b" || echo "$d$base" >> "$SP/uncompiled"
    done
done
if [ -s "$SP/uncompiled" ]; then
    bad "port sources" "$(head -3 "$SP/uncompiled" | tr '\n' ' ')not compiled by its build.sh"
else
    note "port sources" "every .c is named or globbed by its recipe"
fi

echo
echo "==> every helper the Makefile runs is on disk, and none shadows its own output"
# `make fetch-packs` ran `bash ports/appbox/packs`, and `ports/appbox/packs` is
# ALSO the directory `01_packs.sh` and `02_iso.sh` read .kpack files out of. The
# script's own `mkdir -p "$OUT"` therefore failed on its first line of work and
# `set -e` ended the bake before a single row was built — invisible to every
# other gate, because nothing here runs a bake.
for _h in $(sed -n 's/^\t.*\bbash \([a-z][a-zA-Z0-9._/-]*\).*/\1/p' Makefile | sort -u); do
    if [ ! -f "$_h" ]; then
        bad "makefile helpers" "$_h is invoked by the Makefile and is not a file"
    fi
done
[ -e ports/appbox/packs ] && [ ! -d ports/appbox/packs ] &&
    bad "pack output" "ports/appbox/packs must be the .kpack directory, not a file"
note "makefile helpers" "every 'bash <path>' resolves, and the pack output path is free"

echo
echo "==> no build script NAMES a command inside double quotes and RUNS it"
# `echo "Packs: none baked — `make fetch-packs` builds them"` is not a message,
# it is a command substitution: 02_iso.sh executed `make fetch-packs` in the
# middle of packaging, inside a chroot with no Makefile, and printed `No rule to
# make target` from a step that was otherwise fine. A diagnostic that names a
# command the reader should run must quote it so the shell does not.
#
# Only ECHO lines are checked, and only in the build tree: a backtick elsewhere
# is ordinary (00_toolchain/01_gcc.sh uses one to place limits.h) and rewriting
# those buys nothing. `shellcheck` would flag SC2006 on every one of them.
_bt=0
for _f in script/*.sh script/*/*.sh; do
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
echo "==> no chroot step reads the ports tree through /kdos/ports"
# chroot_exec binds $REPO_ROOT onto /kdos with a NON-RECURSIVE `mount --bind`,
# so the container's own mounts underneath it do not come along: /kdos/ports is
# the empty directory that sat there before docker shadowed it, and the ports
# tree is bound separately at /ports. Every env file and 01_appbox.sh already
# say /ports; 01_packs.sh and 02_iso.sh said /kdos/ports and therefore found no
# packs on a machine with 135 of them — exit 2 out of awk under `set -e`, with
# an empty step log and nothing naming the path.
_kp=0
for _f in script/*/*.sh; do
    [ -f "$_f" ] || continue
    if grep -qE '(^|[^#])/kdos/ports/' "$_f" 2>/dev/null; then
        bad "$(basename "$_f")" "reads /kdos/ports — that is an empty shadow in the chroot; use /ports"
        _kp=$((_kp + 1))
    fi
done
note "chroot ports path" "$((_kp)) steps read the ports tree through the shadowed path"

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
    for _f in src/desktop/*/build.sh src/packages/*/build.sh; do
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

names = set()
for d in ("ports/core", "src/packages", "src/desktop"):
    if os.path.isdir(d):
        names |= {n for n in os.listdir(d)
                  if os.path.isfile(os.path.join(d, n, "kpkgbuild"))}
boxed = set()
if os.path.isfile("ports/appbox/packs.conf"):
    for line in open("ports/appbox/packs.conf"):
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        boxed |= set(line.split()[3:])

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
        src/desktop/*/build.sh src/packages/*/build.sh 2>/dev/null
    sed -n 's/^for t in \(.*\); do/\1/p; s/^for _t in \(.*\); do/\1/p' \
        src/desktop/*/build.sh src/packages/*/build.sh 2>/dev/null | tr ' ' '\n'
    cat script/04_phase4/packages.txt 2>/dev/null
    ls ports/core 2>/dev/null
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
          fs/etc/xdg/kdos-console-mimeapps.list \
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
for _d in $(grep -rho 'keys\.doc = "[a-z0-9_-]*"' src/desktop src/packages 2>/dev/null |
            sed 's/.*"\(.*\)"/\1/' | sort -u); do
    _doc=$((_doc + 1))
    [ -f "fs/usr/share/kdos/doc/$_d.txt" ] ||
        bad "help page $_d" "no fs/usr/share/kdos/doc/$_d.txt"
done
note "help pages" "$_doc claimed, each in fs/usr/share/kdos/doc"

echo
if [ "$fail" = 0 ]; then
    echo "preflight clean — the wiring is consistent"
else
    echo "preflight found $fail problem(s)"
fi
exit $((fail > 0))
