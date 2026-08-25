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
    unset name version source
    eval "$("$SP/kpkg" meta "$d" 2>/dev/null)"
    idx=0
    for s in $source; do
        # source_file() in build.c: a non-URL is its own name, a URL is its
        # basename, and only the FIRST source is renamed to <name>-<version>
        # when it carries an archive suffix.
        case "$s" in
            *://*) base=${s##*/} ;;
            *)     base=$s ;;
        esac
        if [ "$idx" = 0 ]; then
            case "$base" in
                *.tar.gz|*.tgz)   base="$name-$version.tar.gz" ;;
                *.tar.bz2|*.tbz2) base="$name-$version.tar.bz2" ;;
                *.tar.xz|*.txz)   base="$name-$version.tar.xz" ;;
                *.tar.zst)        base="$name-$version.tar.zst" ;;
                *.zip)            base="$name-$version.zip" ;;
            esac
        fi
        idx=$((idx + 1))
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
        fi
    done
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

echo
if [ "$fail" = 0 ]; then
    echo "preflight clean — the wiring is consistent"
else
    echo "preflight found $fail problem(s)"
fi
exit $((fail > 0))
