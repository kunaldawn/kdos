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
# The pack store's directories, and optionally KDOS itself as a base pack.
#
# NOTHING IS BAKED. The medium carries a catalogue and no applications: one is
# built by podman on the machine that asks for it, and an exported set is
# imported through kdos-packd. So this step creates the two directories the
# daemon needs and gets out of the way.
#
# THE STAGING DIRECTORY IS 01777 AND THAT IS THE WHOLE OF WHY IT IS HERE. It is
# the one place an unprivileged write may land and the only argument
# kdos-packd's `install` accepts — a filename in it and nothing else, which is
# what keeps a daemon reachable from `wheel` from being `mount /dev/sda2 /etc`.
# The daemon sets the mode at startup, but a first boot that inherited 0755
# would refuse an import until the daemon had run once, which reads as the
# feature not working.

set -e

STORE=/var/lib/kdos/packs

mkdir -p "$STORE/staging" "$STORE/mnt"
chmod 01777 "$STORE/staging"

# ── KDOS itself, as a base pack ────────────────────────────────────────────
#
# `kdos-box create ports base=pack:kdos` gives a running KDOS a clean KDOS to
# build and test ports in, without touching the machine somebody is sitting on
# — which is what "KDOS can build KDOS" means when you only have one machine.
#
# OPT-IN, because it is a second mkfs.erofs over the whole rootfs on every
# build for a base most installs will never compose. `KDOS_PACK_KDOS=1` is the
# same shape as `KDOS_ISO_SOURCES=1`: a developer stick, not the default one.
#
# THE EXCLUDES ARE THE SQUASHFS'S, and they have to stay that way. `kdos` and
# `ports` are bind mounts of the repository — gigabytes, and the output would
# be inside its own source — and the pseudo-filesystems are not a filesystem's
# to carry. A pack that swallowed /kdos would be larger than the ISO.
#
# AND THE PACK STORE IS EXCLUDED FROM THE PACK, which is not tidiness: the
# artefact this step writes lands in `/var/lib/kdos/packs`, so a second run
# would pack the first run's 15 GB pack inside the second one, and a third
# would carry both. `home/kdos/.local/share/containers` goes for a different
# reason: it is podman's own store, which a shipped rootfs must not carry, and
# this pack is a clean tree to build ports in.
#
# THE PSEUDO-FILESYSTEMS GO BY REGEX, NOT BY PATH, BECAUSE THE DIRECTORY HAS
# TO SURVIVE. `--exclude-path=proc` removes the DIRECTORY, and a container
# rootfs with no `/proc`, `/sys`, `/dev`, `/tmp` or `/run` to bind-mount onto
# cannot be started at all: podman answers
#
#   unable to start container "…": open mount point: no such file or directory
#
# for a box that created perfectly. `--exclude-regex='^(proc|…)/'` matches
# everything INSIDE them and not the directories themselves — measured against
# a fixture: the dirs come back as empty directories and their contents are
# gone. `var/cache` and `var/log` ride the same rule because software inside a
# box expects them to exist. What is excluded by PATH is what should genuinely
# not be there: the repository bind mounts, the pack store (the artefact lands
# in it) and podman's own container store.
#
# THEY ARE RELATIVE TO THE SOURCE ROOT AND MUST NOT CARRY A LEADING SLASH.
# `--exclude-path` matches an EXACT LITERAL path, and mkfs.erofs matches it
# against the path it built relative to the tree it is packing — so
# `--exclude-path=/kdos` matches nothing, is not an error, and excludes
# NOTHING. Measured: against a fixture with one 3 MB directory,
# `--exclude-path=/drop` produces an image byte-identical to passing no
# exclusion at all, while `--exclude-path=drop` produces 4 KB. Every flag here
# was accepted for as long as this had never been run over a real rootfs, and
# the first run that was swallowed the repository bind mount and reached
# 107 GB before it was stopped.
# A STALE ONE IS REMOVED WHEN THE FLAG IS OFF, or 02_iso.sh has a 9.1 GB file
# sitting there from an earlier opt-in run and no way to know it was not asked
# for. `build/` survives between builds; the flag does not.
[ "${KDOS_PACK_KDOS:-0}" = "1" ] || rm -rf /kdos/build/kdos-base

if [ "${KDOS_PACK_KDOS:-0}" = "1" ] && command -v mkfs.erofs >/dev/null 2>&1 \
   && command -v kdos-pack >/dev/null 2>&1; then
    echo "[packs] packing this rootfs as base pack 'kdos'..."
    OUT=/kdos/build/kdos-base
    rm -rf "$OUT"
    mkdir -p "$OUT"
    cat > "$OUT/meta" <<META
id = kdos
version = ${KDOS_VERSION:-0.2}
kind = base
summary = KDOS itself — a clean tree to build and test ports in
META
    # THE FLAG IS TESTED BEFORE IT IS TRUSTED, because an --exclude-path that
    # matches nothing is not an error and the failure mode is not a warning:
    # it is a pack that swallows the repository bind mount and fills the disk.
    # Two megabytes against a fixture answers in a second whether this
    # mkfs.erofs excludes what it is told to.
    probe=$(mktemp -d)
    mkdir -p "$probe/src/drop" "$probe/src/deep/er/still" "$probe/src/keep"
    head -c 2000000 /dev/urandom > "$probe/src/drop/big"
    head -c 2000000 /dev/urandom > "$probe/src/deep/er/still/big"
    echo k > "$probe/src/keep/a"
    # A NESTED path as well as a top-level one, because half the list is
    # nested — `var/cache`, `var/lib/kdos/packs` — and a top-level-only probe
    # would pass while the excludes that matter did nothing.
    mkdir -p "$probe/src/proc"
    head -c 2000000 /dev/urandom > "$probe/src/proc/big"
    mkfs.erofs -b 4096 --exclude-path=drop --exclude-path=deep/er/still \
        --exclude-regex='^(proc)/' \
        "$probe/out.erofs" "$probe/src" >/dev/null 2>&1 || true
    probe_sz=$(stat -c %s "$probe/out.erofs" 2>/dev/null || echo 0)
    rm -rf "$probe"
    if [ "$probe_sz" = 0 ] || [ "$probe_sz" -gt 500000 ]; then
        echo "[packs] this mkfs.erofs did not honour --exclude-path" >&2
        echo "[packs] (a 2 MB directory told to be excluded came back as" \
             "${probe_sz} bytes); refusing to pack the rootfs" >&2
        exit 1
    fi

    # mkfs.erofs must run as root to preserve ownership and xattrs, which is
    # what this phase already is. The flags are kdos-pack's own reproducible
    # set; `assemble` wraps an image somebody else made, and this is that case
    # because the excludes are ours rather than kdos-pack's.
    # THE OWNERSHIP FLAGS ARE NOT COSMETIC AND ARE THE REASON THIS PACK IS
    # BUILT THE SAME WAY EVERY OTHER ONE IS. A box runs `--userns keep-id`, so
    # the process inside it is uid 1000 and not root; every other pack comes
    # out of `kdos-pack build`, which forces uid/gid 1000 so that user owns the
    # tree. Packed with real ownership instead, this rootfs is root's from `/`
    # down and the container user can create NOTHING in it — podman fails first
    # on `creating /etc/mtab symlink: permission denied` and then, once that
    # exists, on the bind destination for kdos-boxinit, reported as
    #
    #   crun: open `…/usr/libexec/kdos-boxinit`: No such file or directory
    #
    # for a box that composed perfectly. Ownership grants nothing either way:
    # a pack is mounted nosuid.
    #
    # The UUID comes from `kdos-pack uuid`, which is the same derivation
    # `kdos-pack build` applies, because mkfs.erofs's default is RANDOM and an
    # image that differs every run can never answer `kdos-pack imagehash`.
    mkfs.erofs -zzstd -b 4096 \
        -T "${SOURCE_DATE_EPOCH:-1735689600}" --all-time \
        --force-uid=1000 --force-gid=1000 \
        -U "$(kdos-pack uuid kdos)" \
        --exclude-regex='^(proc|sys|dev|tmp|run|mnt|media|var/cache|var/log)/' \
        --exclude-path=kdos --exclude-path=ports --exclude-path=build \
        --exclude-path=var/lib/kdos/packs \
        --exclude-path=home/kdos/.local/share/containers \
        "$OUT/kdos.erofs" / >/dev/null
    # IT LANDS IN build/, NOT IN THE STORE, AND THAT IS THE WHOLE POINT OF
    # WHERE A PACK LIVES. `/var/lib/kdos/packs` is INSIDE the rootfs, so a pack
    # written there is squashed into system.sfs — and this one is a compressed
    # image of that same rootfs, so the medium would carry the tree twice.
    # Measured: system.sfs went to 20 GB and iso_root to 43 GB. Every other
    # pack goes on ISO9660 beside system.sfs for exactly this reason; 02_iso.sh
    # picks this one up from build/ and puts it there with them. kdos-packd
    # scans the medium for *.kpack, so `base=pack:kdos` resolves off the stick
    # with nothing installed.
    kdos-pack assemble "$OUT/kdos.erofs" "$OUT/meta" "$OUT/kdos.kpack"
    rm -f "$OUT/kdos.erofs"
    echo "[packs] kdos base pack: $(du -h "$OUT/kdos.kpack" | cut -f1) (on the medium)"
fi

echo "[packs] store directories ready; the medium carries no applications"
