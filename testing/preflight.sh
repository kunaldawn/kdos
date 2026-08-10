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
   -Isrc/libs/libkbase -Isrc/libs/libkpkg -Isrc/packages/kdos-kpkg \
   -o "$SP/kdos-kpkg" src/packages/kdos-kpkg/*.c src/libs/libkbase/*.c \
   src/libs/libkpkg/*.c || { echo "  cannot build kpkg"; exit 1; }
# Installed as five names and dispatched on its own basename, so the checks
# have to invoke it the same way — `kpkg kpkgdepends ...` correctly reaches
# kpkg's front end and prints usage, which is not what is being tested.
for n in kpkg kpkgadd kpkgbuild kpkgdel kpkgdepends; do
    ln -sf kdos-kpkg "$SP/$n"
done

export PORT_REPO="$PWD/ports/core $PWD/src/packages"
export KPKG_CONF=/nonexistent PKGDB_DIR=/dev/null

echo
echo "==> every package named in a packages.txt has a port"
for f in script/*/packages.txt; do
    missing=""
    while read -r p; do
        [ -z "$p" ] && continue
        case "$p" in \#*) continue ;; esac
        if [ ! -f "ports/core/$p/kpkgbuild" ] && \
           [ ! -f "src/packages/$p/kpkgbuild" ]; then
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
for d in ports/core/* src/packages/*; do
    [ -f "$d/kpkgbuild" ] || continue
    for dep in $(sed -n 's/^depends[[:blank:]]*=[[:blank:]]*//p' "$d/kpkgbuild"); do
        if [ ! -f "ports/core/$dep/kpkgbuild" ] && \
           [ ! -f "src/packages/$dep/kpkgbuild" ]; then
            bad "$(basename "$d")" "depends on '$dep', which has no port"
            orphans=$((orphans + 1))
        fi
    done
done
[ "$orphans" = 0 ] && note "all depends resolve" "ok"

echo
echo "==> every port has a build.sh, and it parses"
# The build is a shell script in its own file, so it can actually be checked:
# `bash -n` on 396 recipes is a real syntax gate, and it was impossible while
# the build lived inside the recipe.
missing=0
scripts=0
for d in ports/core/* src/packages/*; do
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
for d in ports/core/* src/packages/*; do
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
for d in ports/core/* src/packages/*; do
    [ -f "$d/kpkgbuild" ] || continue
    for k in name version release; do
        grep -qE "^$k[[:blank:]]*=" "$d/kpkgbuild" || \
            bad "$(basename "$d")" "no '$k'"
    done
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
for f in $(grep -rl '^#!' fs/ 2>/dev/null); do
    interp=$(head -1 "$f" | sed 's|^#!||; s| .*||')
    case "$interp" in
        /bin/bash|/bin/sh) ;;
        *) bad "$f" "unexpected interpreter $interp" ;;
    esac
done
note "rootfs interpreters" "bash and sh only"

echo
if [ "$fail" = 0 ]; then
    echo "preflight clean — the wiring is consistent"
else
    echo "preflight found $fail problem(s)"
fi
exit $((fail > 0))
