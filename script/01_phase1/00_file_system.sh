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

cp -r $WORKSPACE/fs/* $SYSROOT/

for account in $ACCOUNT_FILES; do
    extra="$EXTRA_DIR/$account"
    [ -s "$extra" ] || continue
    cat "$extra" >> "$SYSROOT/etc/$account"
    echo "kept $(wc -l < "$extra") runtime-added entry(ies) in /etc/$account:" \
         "$(cut -d: -f1 "$extra" | tr '\n' ' ')"
done

rm -rf "$EXTRA_DIR"
