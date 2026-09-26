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

make defconfig -j1
sed -i 's/# CONFIG_EXPR is not set/CONFIG_EXPR=y/' .config
sed -i 's/# CONFIG_GETTY is not set/CONFIG_GETTY=y/' .config
sed -i 's/# CONFIG_INIT is not set/CONFIG_INIT=y/' .config
sed -i 's/# CONFIG_TR is not set/CONFIG_TR=y/' .config
sed -i 's/# CONFIG_AWK is not set/CONFIG_AWK=y/' .config
sed -i 's/# CONFIG_STTY is not set/CONFIG_STTY=y/' .config
sed -i 's/CONFIG_TAR=y/# CONFIG_TAR is not set/' .config
sed -i 's/CONFIG_GETOPT=y/# CONFIG_GETOPT is not set/' .config
sed -i 's/CONFIG_PATCH=y/# CONFIG_PATCH is not set/' .config
sed -i 's/CONFIG_FILE=y/# CONFIG_FILE is not set/' .config
sed -i 's/CONFIG_LOGIN=y/# CONFIG_LOGIN is not set/' .config
sed -i 's/CONFIG_SU=y/# CONFIG_SU is not set/' .config
# BLKID IS UTIL-LINUX'S, AND TWO OF THEM IS TWO ANSWERS TO "WHAT IS THIS
# DEVICE". The applet's prober knows ext, vfat, ntfs, btrfs, f2fs, squashfs
# and swap — not `crypto_LUKS` — and it implements no `-U` or `-L` LOOKUP at
# all, only reporting on devices it is handed. $PATH puts /usr/bin ahead of
# /usr/sbin, so leaving this on shadows the real one for every caller: the
# initramfs resolving a root, an ESP or a LUKS container by UUID, `kdos
# persist` finding its store by label, and anybody typing `blkid`.
sed -i 's/CONFIG_BLKID=y/# CONFIG_BLKID is not set/' .config
# EVERY OTHER NAME A PORT ON THE IMAGE INSTALLS IS THAT PORT'S, for the same
# two reasons. Where toybox's directory differs from the real tool's —
# blkdiscard, rtcwake, nologin, lspci, iotop — the applet in /usr/bin shadows
# the one in /usr/sbin for every caller. Where it is the same file, whichever
# package is installed LAST owns it, so the binary behind a name would follow
# the install order and change hands on every toybox upgrade. The applets are not drop-ins: `swapon -a`
# and `swapoff -a` are refused, so the swap kinstall writes into fstab is never
# turned on; `umount -R` does not exist; `mount` never runs a mount.<type>
# helper and does not know `nofail`; `lspci -d` and `lsusb -d` do not exist,
# which airmon-ng's driver detection rests on; `readelf` and `strings` are not
# binutils'. `prlimit` is compiled into the `ulimit` applet and goes with it;
# bash's builtin is the `ulimit` anybody reaches.
#
# sed, find, xargs, awk, expr and ln stay: every configure script between this
# port and GNU sed, findutils and coreutils runs them, and phase 1 has no other
# copy. The GNU ports come later in dependency order and take the names over.
for _applet in \
	BLKDISCARD BLOCKDEV CAL CHRT DMESG EJECT FALLOCATE FLOCK FSFREEZE HWCLOCK \
	IONICE KILL LINUX32 LOGGER LOSETUP MCOOKIE MKSWAP MOUNT MOUNTPOINT \
	NSENTER PIVOT_ROOT RENICE REV RFKILL RTCWAKE SETSID SWAPOFF SWAPON \
	SWITCH_ROOT TASKSET UCLAMPSET ULIMIT UMOUNT UNSHARE UUIDGEN \
	NOLOGIN PARTPROBE GUNZIP ZCAT LSATTR CHATTR \
	INSMOD LSMOD RMMOD MODINFO \
	FREE PGREP PIDOF PKILL PMAP PS PWDX SYSCTL TOP UPTIME VMSTAT W WATCH \
	CHVT DEALLOCVT OPENVT \
	LSPCI LSUSB KILLALL IOTOP \
	I2CDETECT I2CDUMP I2CGET I2CSET I2CTRANSFER \
	GPIODETECT GPIOGET GPIOINFO GPIOSET \
	NETCAT \
	READELF STRINGS CMP CLEAR RESET SETFATTR BUNZIP2 BZCAT; do
	sed -i "s/^CONFIG_${_applet}=y\$/# CONFIG_${_applet} is not set/" .config
done
make PREFIX=$PKG install -j1
