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
# The pack lane's packaging step, and it is almost nothing — which is the
# point. There is no `podman load`, no flatten, no `remap-uids`, no $STORAGE
# wipe and no libpod database surgery: every one of those five traps is a
# consequence of shipping an IMAGE, and a pack has no equivalent of any of
# them. A pack is a file; installing it is a mount that happens at run time,
# performed by kdos-packd as root.
#
# WHAT GOES INTO THE INSTALLED SYSTEM AND WHAT STAYS ON THE MEDIUM. Only the
# `base` and the runtimes are placed in /var/lib/kdos/packs, because a box
# cannot be composed without them and a machine with no medium in it would
# otherwise have nothing at all. The applications stay on ISO9660 beside
# system.sfs (02_iso.sh puts them there) and are `available` until somebody
# asks for one — which is the whole reason an install stops costing 3.9 GB for
# 105 applications nobody picked.
#
# A missing pack set is a warning, not an error: the monolithic image lane is
# still there and kdos-appbox chooses between them by looking for a `base`.

set -e

# /ports, NOT /kdos/ports. chroot_exec binds $REPO_ROOT onto /kdos with a
# non-recursive `mount --bind`, so the docker mounts UNDER it do not come along
# — /kdos/ports is the empty directory that sat there before docker shadowed it,
# and `ports` is bound separately at /ports for exactly this reason. Reading the
# wrong one made this step find no PACKAGES, exit 2 from awk on a missing file
# with `set -e`, and log nothing at all.
SRC=/ports/appbox/packs
STORE=/var/lib/kdos/packs

mkdir -p "$STORE/staging" "$STORE/mnt"
# The staging directory is the ONE place an unprivileged download may land, and
# it is the only argument kdos-packd's `install` accepts — a filename in here
# and nothing else.
chmod 01777 "$STORE/staging"

# WHICH RUNTIMES GO IN IS DECIDED BY WHAT NEEDS THEM, not by the `rt-` prefix.
# Measured on a full bake the seven runtimes are 1.7 GB, of which rt-wine alone
# is 713 MB — carried onto a machine that may never run a Windows binary. The
# base plus the runtimes the RECOMMENDED applications need is what a live
# session can actually launch; everything else arrives with its application.
#
# `D:` in the index is where the dependency comes from, and the closure is a
# repeat-until-nothing-new rather than a recursion, so a chain of any depth
# resolves and a cycle cannot spin.
want=" base "
if [ -f "$SRC/PACKAGES" ]; then
    want="$want$(awk '
        /^P:/ { id = substr($0, 3) }
        /^D:/ { dep[id] = substr($0, 3) }
        /^R:yes/ { rec[id] = 1 }
        END { for (i in rec) printf "%s ", i }' "$SRC/PACKAGES")"
    # pull in what those need, transitively
    while :; do
        more=$(awk -v want="$want" '
            /^P:/ { id = substr($0, 3) }
            /^D:/ { if (index(want, " " id " ")) print substr($0, 3) }
        ' "$SRC/PACKAGES" | tr ' ' '\n' | sort -u)
        added=0
        for d in $more; do
            case "$want" in *" $d "*) ;; *) want="$want$d "; added=1 ;; esac
        done
        [ "$added" = 0 ] && break
    done
fi

# The APPLICATIONS themselves stay on the medium even when recommended: they
# are already there, and copying one into the squashfs as well would ship it
# twice on one ISO. What the closure is for is the runtimes UNDER them.
kinds=$(awk '/^P:/ { id = substr($0, 3) } /^K:/ { printf "%s=%s ", id, substr($0, 3) }' \
        "$SRC/PACKAGES" 2>/dev/null)

n=0
for p in "$SRC"/*.kpack; do
    [ -e "$p" ] || continue
    id=$(basename "$p" .kpack)
    case " $kinds " in
        *" $id=app "*|*" $id=data "*) continue ;;
    esac
    case "$want" in
        *" $id "*)
            cp -a "$p" "$STORE/$id.kpack"
            # ROOT, AND THAT IS THE WHOLE BASIS FOR NOT RE-HASHING IT.
            # kdos-packd trusts a pack in the store because only root can
            # write there — `cp -a` preserves the SOURCE's owner, and the
            # source is a bind mount of the repository, whose files belong to
            # whoever cloned it. Left alone, the store ships owned by uid 1000,
            # which on the target is the desktop user: they could replace a
            # pack and the daemon would mount it unverified.
            chown 0:0 "$STORE/$id.kpack"
            chmod 0644 "$STORE/$id.kpack"
            n=$((n + 1))
            ;;
        *)
            # Everything else stays on the medium until somebody asks for it.
            ;;
    esac
done

if [ "$n" = 0 ]; then
    echo "[packs] none baked — 'make fetch-packs' builds them (needs a network)."
    echo "[packs] this ISO uses the monolithic appbox image."
fi

# The index travels with the packs it covers: one signature over it covers
# every pack's hash transitively, which is what makes a pack on the medium
# verifiable without a sidecar of its own.
for f in PACKAGES PACKAGES.sig; do
    [ -f "$SRC/$f" ] || continue
    cp -a "$SRC/$f" "$STORE/$f"
    chown 0:0 "$STORE/$f"
    chmod 0644 "$STORE/$f"
done

# ── Launchers for the recommended set ──────────────────────────────────────
#
# THE ISO HAS TO SHIP A START MENU WITH APPLICATIONS IN IT, and nothing had
# written one since the monolith went: `01_appbox.sh` used to run genlaunchers
# against the image, the pack lane's `genlaunchers --packs` needs kdos-packd
# and a mount, and the build is a chroot in an unprivileged container that has
# neither. What shipped was the stale two-field table from the image lane, so
# every launcher on the live ISO dispatched to a box called `kdos-apps` that
# no longer exists — measured: `could not compose box 'kdos-apps' from packs`
# on the first click of Firefox.
#
# The build reads INSIDE each recommended pack without mounting it:
# `kdos-pack image` writes the EROFS bytes, `fsck.erofs --extract` (our
# erofs-utils, built with zstd — the distro's images are -zzstd and a
# fsck.erofs without it says "Failed to extract filesystem") pulls out
# /usr/share/applications, and `genlaunchers --packs-dir` reads one directory
# per pack, the directory NAME being the pack id the table's third field
# carries. The result is byte-for-byte what a runtime regeneration would write
# for the same packs, so a launcher clicked on the live ISO composes the same
# box the installed system would.
#
# Only the RECOMMENDED set: those are the applications the medium promises at
# first sight; everything else is a `kdos app install` away and gets its
# launcher from the user tree at that moment.
if [ -f "$SRC/PACKAGES" ] && command -v fsck.erofs >/dev/null 2>&1 \
   && command -v kdos-pack >/dev/null 2>&1; then
    LDIR=/kdos/build/launchers
    rm -rf "$LDIR"; mkdir -p "$LDIR"
    nrec=0
    for id in $(awk '/^P:/ { id = substr($0, 3) } /^R:yes/ { print id }' "$SRC/PACKAGES"); do
        [ -f "$SRC/$id.kpack" ] || continue
        # an app pack only; a runtime's applications directory is whatever its
        # libraries dropped there and is not this desktop's business
        case " $kinds " in *" $id=app "*) ;; *) continue ;; esac
        img=$(mktemp /tmp/kpack-XXXXXX)   # toybox mktemp takes no suffix after the Xs
        if kdos-pack image "$SRC/$id.kpack" "$img" 2>/dev/null \
           && mkdir -p "$LDIR/$id/usr/share/applications" \
           && fsck.erofs --extract="$LDIR/$id/usr/share/applications" \
                         --path=usr/share/applications "$img" >/dev/null 2>&1; then
            nrec=$((nrec + 1))
        else
            echo "[packs] $id: could not read its desktop entries — no launcher" >&2
            rm -rf "$LDIR/$id"
        fi
        rm -f "$img"
    done
    if [ "$nrec" -gt 0 ]; then
        kdos-appbox genlaunchers --packs-dir "$LDIR" / 2>&1 | sed 's/^/[packs] /'
        # THE LIVE USER'S HOME WAS MATERIALISED FROM SKEL BEFORE THIS STEP RAN
        # (00_user.sh sorts first), so the launchers written into skel just
        # now are on the ISO for every FUTURE home and absent from the one
        # the live session logs into — the shim works from a prompt and the
        # Start menu shows nothing. Measured. The same copy 00_user.sh does,
        # for the one directory this step is responsible for.
        for h in /home/*; do
            [ -d "$h/.local/share" ] || continue
            mkdir -p "$h/.local/share/applications"
            cp -a /etc/skel/.local/share/applications/. "$h/.local/share/applications/"
            chown -R "$(stat -c %u:%g "$h")" "$h/.local/share/applications"
        done
    else
        echo "[packs] no recommended pack readable — the ISO ships no launchers" >&2
    fi
    rm -rf "$LDIR"
fi

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
# would carry both. The monolithic image under
# `home/kdos/.local/share/containers` goes for a different reason — it is
# 11 GB of the OTHER lane, and this pack is a clean tree to build ports in.
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
# in it) and the monolithic image.
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

echo "[packs] $n pack(s) installed (base + what the recommended set needs);"
echo "[packs] everything else is on the medium"
