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
    [ -f "$SRC/$f" ] && cp -a "$SRC/$f" "$STORE/$f"
done

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
# THE EXCLUDES ARE THE SQUASHFS'S, and they have to stay that way. `/kdos` and
# `/ports` are bind mounts of the repository — gigabytes, and the output would
# be inside its own source — and the pseudo-filesystems are not a filesystem's
# to carry. A pack that swallowed /kdos would be larger than the ISO.
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
    # mkfs.erofs must run as root to preserve ownership and xattrs, which is
    # what this phase already is. The flags are kdos-pack's own reproducible
    # set; `assemble` wraps an image somebody else made, and this is that case
    # because the excludes are ours rather than kdos-pack's.
    mkfs.erofs -zzstd -b 4096 \
        -T "${SOURCE_DATE_EPOCH:-1735689600}" --all-time \
        --exclude-path=/kdos --exclude-path=/ports \
        --exclude-path=/proc --exclude-path=/sys --exclude-path=/dev \
        --exclude-path=/tmp --exclude-path=/run --exclude-path=/mnt \
        --exclude-path=/media --exclude-path=/var/cache --exclude-path=/var/log \
        --exclude-path=/build \
        "$OUT/kdos.erofs" / >/dev/null
    kdos-pack assemble "$OUT/kdos.erofs" "$OUT/meta" "$STORE/kdos.kpack"
    echo "[packs] kdos base pack: $(du -h "$STORE/kdos.kpack" | cut -f1)"
fi

echo "[packs] $n pack(s) installed (base + what the recommended set needs);"
echo "[packs] everything else is on the medium"
