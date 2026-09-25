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

if [ -f "$MARK/toybox" ] && [ "${KDOS_REPLAY:-0}" != "1" ]; then
    exit 0
fi

echo ">>> Building toybox..."

# Extract toybox and dependencies from ports
TOYBOX_SRC=$(extract_port_source toybox)

cd "$TOYBOX_SRC"

make defconfig -j1
sed -i 's/# CONFIG_EXPR is not set/CONFIG_EXPR=y/' .config
sed -i 's/# CONFIG_GETTY is not set/CONFIG_GETTY=y/' .config
sed -i 's/# CONFIG_INIT is not set/CONFIG_INIT=y/' .config
sed -i 's/# CONFIG_TR is not set/CONFIG_TR=y/' .config
sed -i 's/# CONFIG_AWK is not set/CONFIG_AWK=y/' .config
sed -i 's/CONFIG_TAR=y/# CONFIG_TAR is not set/' .config
# `file` is the `file` port's, here as in the phase-4 recipe: lesspipe and the
# desktop's type guessing both ask `file -L -s -b --mime`, which the applet has
# no database to answer, and two answers to "what is this file" is one too many.
sed -i 's/CONFIG_FILE=y/# CONFIG_FILE is not set/' .config
# `blkid` is util-linux's, here as in the phase-4 recipe, and util-linux puts
# it in /usr/sbin only — so a /usr/bin/blkid planted here is a name no later
# package ever overwrites. Phase 1 installs outside the package database, so
# the orphan sweep cannot see it either, and $PATH puts /usr/bin ahead of
# /usr/sbin: the symlink would shadow the real tool for every caller while
# pointing at a toybox that has the applet compiled out.
sed -i 's/CONFIG_BLKID=y/# CONFIG_BLKID is not set/' .config
# The same holds for every applet the phase-4 recipe switches off whose real
# tool lives in another directory — blkdiscard, rtcwake, nologin, lspci and
# iotop are /usr/sbin programs, and a /usr/bin symlink left from here outlives
# the applet. gunzip and zcat are gzip's, installed just before this: the
# applet would replace them with a symlink the recipe's toybox cannot answer.
# netcat and ulimit are names no port installs at any path — the netcat port
# ships only `nc`, util-linux only `prlimit`, and `ulimit` is bash's builtin —
# so a link planted here would outlive the applet; their `nc` and `prlimit`
# aliases go with them, and phase 1 needs neither. The recipe's other names
# land on the same paths as their real tools, which replace these symlinks
# when they are installed.
for _applet in BLKDISCARD RTCWAKE NOLOGIN LSPCI IOTOP GUNZIP ZCAT NETCAT ULIMIT; do
	sed -i "s/^CONFIG_${_applet}=y\$/# CONFIG_${_applet} is not set/" .config
done
CC=$KDOS_TARGET-gcc make PREFIX=$SYSROOT install -j1

rm -rf "$TOYBOX_SRC"
touch "$MARK/toybox"

