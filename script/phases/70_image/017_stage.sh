#!/bin/bash

# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------
#
# Trim the image stage to the base: every installed package outside ports/base
# and its closure is removed from it, and the ISO carries what is left.
#
# THIS RUNS IN THE STAGE, NEVER IN build/fs. script/chroot/exec.sh roots every
# 70_image step from this one on in an overlay over build/fs, this step with
# an empty upper layer, so a removal here writes whiteouts into
# build/image-upper and build/fs keeps every package for the next build and
# the binhost. The step refuses to run unless / is that overlay: the same
# `kpkg remove` over build/fs would delete most of the build.
#
# THE SET IS script/lib/imageset.py's (ports/base, ports/buildonly), read from
# the repository at /kdos. kpkg remove takes the dropped names in one run, so
# no removal is refused for a reverse dependency that is also going, and its
# index triggers rebuild the caches (fonts, icons, MIME, desktop entries,
# ld.so.cache) once for the trimmed tree. Every later step — the cleanup, the
# orphan sweep, the launchers, the indexes and 087_gates.sh's linkcheck and
# entrycheck — reads this tree, so a package the base needs at run time and
# that the closure missed fails the image there.
#
# THE IMAGE CARRIES THE RUN-TIME FACET OF EACH PACKAGE. Once the set is the
# base, `kpkg facets --strip` removes every package's development and
# documentation files from the stage (libkpkg's kp_facet_of: headers, static
# libraries, CMake, pkg-config and aclocal files, GIR and VAPI files, the -l
# links, man sections 0, 2 and 3, share/doc except the handbooks and the two
# books, info, gtk-doc; and what a recipe's facet_dev = adds, gcc's compiler
# among it) and lists each in /var/lib/kpkg/db/.facets/<name>,
# and the stage's /etc/kpkg.conf is rewritten to say FACETS=run, so the
# medium and every machine installed from it leave those files out of each
# later install too. build/fs keeps FACETS=all and every file. 087_gates.sh's
# linkcheck runs after, so a program that links through a removed -l link
# fails the image.
#
# THE BUILD'S ENVIRONMENT IS RECORDED in /etc/kdos/chroot.env before the
# facets go: the compilers, every flag set and KDOS_TARGET as this phase has
# them, KPKG_CC_ID, the compiler's `<CC> <version>`, asked now because the
# strip takes gcc's driver, and what else a recipe's result depends on (the
# Rust -sys switches, CMake's build type, the Cargo and Go settings,
# SOURCE_DATE_EPOCH), with CONFIG_SITE and KDOS_BUILD_PATH naming the
# autoconf site file and the ninja and meson wrappers, copied to
# /usr/share/kdos/build. `kdos update` loads the file before every kpkg it
# runs: kpkg's build-config hash reads KPKG_CC_ID instead of a compiler, so a
# machine installed from the image computes the binhost's B: and takes its
# packages, and a port built there is built as the binhost's was. Without
# the file nothing prebuilt matches; without the rest a Rust port links the
# copy its crate carries and an autoconf port puts its configuration under
# /usr/etc.
#
# Under KDOS_IMAGE=full this step records the environment and does nothing
# else, and the image is build/fs.

set -e
source script/phases/70_image/phase.env

BUILD_SHARE=/usr/share/kdos/build
record_build_env() {
    local k
    install -Dm644 /kdos/script/env/config.site "$BUILD_SHARE/config.site"
    install -Dm755 /kdos/script/bin/ninja "$BUILD_SHARE/bin/ninja"
    install -Dm755 /kdos/script/bin/meson "$BUILD_SHARE/bin/meson"
    {
        for k in CC CXX CFLAGS CXXFLAGS FFLAGS FCFLAGS LDFLAGS KDOS_TARGET \
                 CMAKE_BUILD_TYPE CARGO_PROFILE_RELEASE_DEBUG GOFLAGS \
                 CGO_CFLAGS CGO_CXXFLAGS CGO_LDFLAGS \
                 LIBGIT2_NO_VENDOR RUSTONIG_SYSTEM_LIBONIG \
                 LIBSQLITE3_SYS_USE_PKG_CONFIG ZSTD_SYS_USE_PKG_CONFIG \
                 SOURCE_DATE_EPOCH PIP_ROOT_USER_ACTION; do
            if [ -n "${!k+x}" ]; then printf '%s=%s\n' "$k" "${!k}"; fi
        done
        printf 'CONFIG_SITE=%s\n' "$BUILD_SHARE/config.site"
        printf 'KDOS_BUILD_PATH=%s\n' "$BUILD_SHARE/bin"
        printf 'KPKG_CC_ID=%s %s\n' "${CC:-cc}" "$("${CC:-cc}" -dumpversion)"
    } > /etc/kdos/chroot.env
    chmod 0644 /etc/kdos/chroot.env
    echo "[stage] /etc/kdos/chroot.env: $(grep '^KPKG_CC_ID=' /etc/kdos/chroot.env)"
}

if [ "${KDOS_IMAGE:-base}" = full ]; then
    record_build_env
    echo "[stage] KDOS_IMAGE=full — the image carries every installed package"
    exit 0
fi

rootfs=$(awk '$2 == "/" { t = $3 } END { print t }' /proc/mounts)
if [ "$rootfs" != overlay ]; then
    echo "FATAL: / is $rootfs, not the image stage's overlay; refusing to remove packages from build/fs" >&2
    exit 1
fi

python3 /kdos/script/lib/imageset.py --tree /kdos --check
drop=$(python3 /kdos/script/lib/imageset.py --tree /kdos --root / --drop)
n=$(printf '%s\n' "$drop" | grep -c . || true)
echo "[stage] removing $n package(s) outside the base"
if [ "$n" -gt 0 ]; then
    log=/kdos/build/logs/70_image/stage-remove.log
    if ! printf '%s\n' "$drop" | xargs kpkg remove > "$log" 2>&1; then
        grep -v '^==> \(Removing\|Package .* removed\|Keeping\)' "$log" | tail -40 >&2
        echo "FATAL: kpkg remove failed; its whole output is $log" >&2
        exit 1
    fi
fi
left=$(printf '%s\n' "$drop" | while IFS= read -r p; do
    if [ -n "$p" ] && [ -f "/var/lib/kpkg/db/$p" ]; then printf '%s ' "$p"; fi
done)
if [ -n "$left" ]; then
    echo "FATAL: still installed after the removal: $left" >&2
    exit 1
fi
python3 /kdos/script/lib/imageset.py --tree /kdos --root / --sizes

record_build_env
echo "[stage] removing the development and documentation facets"
strip=$(kpkg facets --strip)
printf '%s\n' "$strip" | sed 's/^==> /[stage] /'
conf=$(awk '!/^FACETS=/' /etc/kpkg.conf)
printf '%s\nFACETS="${KPKG_FACETS:-run}"\n' "$conf" > /etc/kpkg.conf.new
mv /etc/kpkg.conf.new /etc/kpkg.conf
grep -qx 'FACETS="${KPKG_FACETS:-run}"' /etc/kpkg.conf

touch /kdos/build/image-stage.ok
echo "[stage] the image is the base: $(python3 /kdos/script/lib/imageset.py --tree /kdos --root / --keep | wc -l) packages"
