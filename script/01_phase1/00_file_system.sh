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

set -e
source script/phase1.env.sh
source script/util/port.sh

mkdir -pv $SYSROOT/{etc,var,tmp,root,home,run,dev,proc,sys}
mkdir -pv $SYSROOT/usr/{bin,lib,sbin,include,share,local}
mkdir -pv $SYSROOT/var/{lib,log,local,run,tmp}
mkdir -pv $SYSROOT/var/local/log
# 1777 on both: rootless podman/skopeo stage pulled image layers in /var/tmp.
chmod 1777 $SYSROOT/tmp $SYSROOT/var/tmp

cd $SYSROOT

# merged /usr
ln -svf usr/bin bin
ln -svf usr/sbin sbin
ln -svf usr/lib lib
ln -svf usr/lib64 lib64

cd $WORKSPACE

# Copy the fs/ overlay into the rootfs.
#
# /etc/{passwd,group,shadow} are merged rather than overwritten. Package
# postinstalls add service users (polkitd, messagebus, sshd, ...) long after
# this step runs, so re-running the overlay sync on an existing tree — which is
# exactly what the build plan's step picker is for — would otherwise delete
# them and quietly break polkit, dbus and sshd. Repo entries win; entries that
# exist only in the tree are appended back.
ACCOUNT_FILES="passwd group shadow"
EXTRA_DIR=$(mktemp -d)

for account in $ACCOUNT_FILES; do
    src="$WORKSPACE/fs/etc/$account"
    dst="$SYSROOT/etc/$account"
    if [ -f "$src" ] && [ -f "$dst" ]; then
        awk -F: 'NR==FNR { seen[$1]=1; next } !($1 in seen)' "$src" "$dst" \
            > "$EXTRA_DIR/$account"
    fi
done

# A file DELETED from fs/ has to disappear from the tree too. cp -r overwrites
# but never removes, so a path the repo has dropped lingers forever: the shell
# kdos-appbox survived being replaced by the C port and kpkg then refused the
# install with a file conflict, and before that a stale icon rode three ISO
# rebuilds. So: remember what fs/ provided last time, and on the next sync
# delete the paths it no longer provides.
#
# Only ever files, and only ever files this manifest itself put there — a
# package that later installs to the same path takes ownership and is not the
# manifest's to remove, which is why the manifest is rewritten AFTER the copy.
MANIFEST="$SYSROOT/var/lib/kdos/fs-manifest"
if [ -f "$MANIFEST" ]; then
    while IFS= read -r rel; do
        [ -n "$rel" ] || continue
        [ -e "$WORKSPACE/fs/$rel" ] && continue
        if [ -f "$SYSROOT/$rel" ] || [ -L "$SYSROOT/$rel" ]; then
            echo "removing $rel (no longer in fs/)"
            rm -f "$SYSROOT/$rel"
        fi
    done < "$MANIFEST"
fi

cp -r $WORKSPACE/fs/* $SYSROOT/

# cp overwrites CONTENT and keeps the DESTINATION's mode, so a permission
# change in fs/ never reached a tree that had already been synced once. That
# is not cosmetic: rcS runs `[ -x "$_script" ] || continue`, so 57_oomd.sh
# arriving 644 meant kdos-oomd never started and nothing said so. Replay the
# execute bit — 755/644 is exactly what cp itself produces for a NEW file
# under the umask this build runs with, so a first sync and a re-sync agree.
# Regular files only: chmod follows a symlink, and every alien-app shim in
# usr/local/bin points at kdos-appbox.
( cd "$WORKSPACE/fs" && find . -type f ) | while IFS= read -r rel; do
    rel="${rel#./}"
    [ -f "$SYSROOT/$rel" ] || continue
    if [ -x "$WORKSPACE/fs/$rel" ]; then
        chmod 755 "$SYSROOT/$rel"
    else
        chmod 644 "$SYSROOT/$rel"
    fi
done

mkdir -p "$(dirname "$MANIFEST")"
# -printf is a GNU extension and the build image's find is busybox's, which
# silently wrote an EMPTY manifest — and an empty manifest protects nothing.
( cd "$WORKSPACE/fs" && find . \( -type f -o -type l \) ) \
    | sed 's|^\./||' | sort > "$MANIFEST"
echo "fs manifest: $(wc -l < "$MANIFEST") paths"

for account in $ACCOUNT_FILES; do
    extra="$EXTRA_DIR/$account"
    [ -s "$extra" ] || continue
    cat "$extra" >> "$SYSROOT/etc/$account"
    echo "kept $(wc -l < "$extra") runtime-added entry(ies) in /etc/$account:" \
         "$(cut -d: -f1 "$extra" | tr '\n' ' ')"
done

rm -rf "$EXTRA_DIR"
