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
mkdir -pv $SYSROOT/var/{lib,log,local,run}
mkdir -pv $SYSROOT/var/local/log
chmod 1777 $SYSROOT/tmp

cd $SYSROOT

# merged /usr
ln -svf usr/bin bin
ln -svf usr/sbin sbin
ln -svf usr/lib lib
ln -svf usr/lib64 lib64

cd $WORKSPACE

# copy files from fs
cp -r $WORKSPACE/fs/* $SYSROOT/
