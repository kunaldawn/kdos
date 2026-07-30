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
# Bake the kdos-apps distrobox image into the kdos user's rootless podman
# storage, so a live/installed system can create and use the box fully offline.
# The image is built on the host by `make fetch-apps` (this build has no
# network); a missing archive is a warning, not an error — the ISO still
# builds, just without alien apps.
#
# Rootless podman cannot run here: it must create a user namespace, and the
# kernel refuses that for any chrooted process — which every packaging step
# is. So the load runs ROOTFUL, pointed at the kdos user's storage path
# (which is also the path the runtime will see, so the metadata records the
# right locations), and ownership is then remapped to exactly what rootless
# podman would have produced: container uid 0 -> kdos (1000), container
# uid N -> subuid 100000+N-1 (fs/etc/subuid). chown clears setuid/setgid
# bits, so modes are restored afterwards — sudo inside the box depends on it.
#
# Even rootful podman needs one trick: its layer-unpack helper remounts /
# MS_REC|MS_PRIVATE, which EINVALs when / is not a mountpoint — true for
# every chrooted process. The --pivot/--pivot2 stages rebuild the same tree
# as a real mount-ns root (bind, chroot into the bind so the root becomes a
# mountpoint, bind again, pivot_root onto that — pivot_root refuses when the
# current root is not a mountpoint AND when new_root equals the current
# root, hence two stages), all inside a private mount namespace that
# evaporates when the load finishes.
#
# No container is created or started here: /usr/local/bin/kdos-appbox creates
# the box lazily on first launch, from the image loaded below.

set -e

TAR=/ports/appbox/appbox.tar
IMGDIR=/ports/appbox/image
ICONS=/ports/appbox/icons
STORAGE=/home/kdos/.local/share/containers/storage

case "${1:-}" in
--pivot)
    mkdir -p /tmp/.appbox-pivot
    /bin/mount --make-rprivate /tmp 2>/dev/null || true
    /bin/mount --rbind / /tmp/.appbox-pivot
    /bin/mount --make-rprivate /tmp/.appbox-pivot 2>/dev/null || true
    cd /tmp/.appbox-pivot
    exec /usr/sbin/chroot . /bin/bash /kdos/script/06_packaging/01_appbox.sh --pivot2
    ;;
--pivot2)
    /bin/mount --make-rprivate / 2>/dev/null || true
    mkdir -p /tmp/.appbox-pivot2
    /bin/mount --rbind / /tmp/.appbox-pivot2
    cd /tmp/.appbox-pivot2
    /usr/sbin/pivot_root . .
    /bin/umount -l .
    exec /usr/sbin/chroot . /bin/bash /kdos/script/06_packaging/01_appbox.sh --load
    ;;
--load)
    cd /
    export PODMAN_IGNORE_CGROUPSV1_WARNING=1
    mkdir -p "$STORAGE" /tmp/appbox-runroot
    if [ -f "$TAR" ]; then
        podman --root "$STORAGE" --runroot /tmp/appbox-runroot load -i "$TAR"
    else
        # No monolithic tar in the repo (LFS 2G/file limit): stream it back
        # out of the chunked image/ directory instead.
        python3 /ports/appbox/assemble "$IMGDIR" | \
            podman --root "$STORAGE" --runroot /tmp/appbox-runroot load
    fi
    # Flatten to ONE layer. This rootful store is later mounted by ROOTLESS
    # podman, which cannot see the trusted.overlay.* whiteout/opaque
    # metadata the rootful unpack wrote — merged dirs that layers rebuilt
    # (e.g. /etc/alternatives) come up EMPTY in the box, which is how OBS
    # lost libblas and its whole encoder set. Exporting resolves the layers
    # as root (correctly); the re-import has no whiteouts to misread.
    podman --root "$STORAGE" --runroot /tmp/appbox-runroot create --name flatten localhost/kdos-appbox:latest
    podman --root "$STORAGE" --runroot /tmp/appbox-runroot export flatten | \
        podman --root "$STORAGE" --runroot /tmp/appbox-runroot import \
            --change 'CMD ["bash"]' - localhost/kdos-appbox:latest
    podman --root "$STORAGE" --runroot /tmp/appbox-runroot rm flatten
    podman --root "$STORAGE" --runroot /tmp/appbox-runroot image prune -f
    podman --root "$STORAGE" --runroot /tmp/appbox-runroot image exists localhost/kdos-appbox:latest
    exit 0
    ;;
esac

source script/packaging.env.sh

if [ ! -f "$TAR" ] && [ ! -f "$IMGDIR/INDEX.json" ]; then
    echo "Warning: neither $TAR nor $IMGDIR found — run 'make fetch-apps' first."
    echo "         Building ISO without the offline alien-app image."
    exit 0
fi

echo "Loading appbox image into kdos's podman storage (rootful, pivoted)..."
# The bake is authoritative: start from an empty store every time. Loading
# on top of a previous bake would double-remap the ownership below (uids
# already pushed into the subuid range get clamped to 165535), leaving a
# store the kdos user cannot write into — every distrobox start then fails.
rm -rf "$STORAGE"
unshare -m --propagation unchanged /bin/bash "$0" --pivot
rm -rf /tmp/appbox-runroot
grep -q kdos-appbox "$STORAGE"/*-images/images.json

echo "Remapping ownership to the rootless layout..."
python3 - "$STORAGE" <<'EOF'
import os, stat, sys

BASE_UID = 1000        # kdos
SUB_BASE = 100000      # fs/etc/subuid: kdos:100000:65536
SUB_COUNT = 65536

def remap(n):
    # Idempotent: ids already in the rootless layout stay put, so an
    # accidental second pass cannot corrupt the store.
    if n == BASE_UID or SUB_BASE <= n < SUB_BASE + SUB_COUNT:
        return n
    if n == 0:
        return BASE_UID
    return SUB_BASE + min(n, SUB_COUNT) - 1

count = 0
for root, dirs, files in os.walk(sys.argv[1]):
    for name in dirs + files:
        p = os.path.join(root, name)
        st = os.lstat(p)
        os.lchown(p, remap(st.st_uid), remap(st.st_gid))
        # chown strips setuid/setgid from regular files; put the mode back
        if not stat.S_ISLNK(st.st_mode) and st.st_mode & 0o6000:
            os.chmod(p, stat.S_IMODE(st.st_mode))
        count += 1
top = sys.argv[1]
st = os.lstat(top)
os.lchown(top, remap(st.st_uid), remap(st.st_gid))
print("remapped %d entries" % count)
EOF
chown kdos:kdos /home/kdos/.local /home/kdos/.local/share \
                /home/kdos/.local/share/containers
# The rootful load/prune leaves an empty volumes/ skeleton whose remapped
# owner the kdos user cannot write into — runtime podman then fails to
# create volume _data dirs and every `distrobox enter` dies with
# "crun: readlink ''". Nothing bakes volumes; drop it, rootless podman
# recreates it correctly on first use.
rm -rf "$STORAGE/volumes"

# The launchers reference the apps' own icons. They go into the SYSTEM
# hicolor tree (merged under /usr/share/icons/hicolor's index.theme) — a
# user-dir icon tree without its own index.theme is not searched by the
# shell's icon lookup, which is how every icon came up as a pink fallback.
# Context dirs are flattened to apps/: several packages ship icons in
# nonstandard spots (16x16/audacity.png, stock/media/...) that no index.theme
# lists.
if [ -d "$ICONS/usr/share/icons" ]; then
    echo "Installing appbox icons..."
    find "$ICONS/usr/share/icons" -type f | while read -r f; do
        rel="${f#"$ICONS/usr/share/icons/"}"   # <theme>/<size>/<context>/<file>
        rel="${rel#*/}"
        size="${rel%%/*}"
        case "$size" in
            *x*|scalable) ;;
            *) continue ;;
        esac
        install -Dm644 "$f" "/usr/share/icons/hicolor/$size/apps/$(basename "$f")"
    done
fi
if [ -d "$ICONS/usr/share/pixmaps" ]; then
    mkdir -p /usr/share/pixmaps
    cp -r "$ICONS"/usr/share/pixmaps/. /usr/share/pixmaps/
fi
# Stale copies from earlier bakes of the user-dir approach
rm -rf /home/kdos/.local/share/icons/hicolor /etc/skel/.local/share/icons

echo "Appbox baked: $(du -sh /home/kdos/.local/share/containers 2>/dev/null | cut -f1)"
