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

# A git snapshot carries no configure; autogen.sh is a wrapper around this.
autoreconf -f -i -s

# HAVE_LSEEK64 IS FORCED, AND ONLY MUSL NEEDS IT FORCED. On musl `lseek64` is a
# MACRO expanding to `lseek` rather than a symbol, so configure's link probe
# does not find it and the source falls back to defining its own
# `static inline off64_t lseek64(...)` — which the macro rewrites into a second
# declaration of `lseek` with a different argument type, and the compile stops
# on a conflict in code that is correct everywhere else. off64_t is a typedef
# musl already provides under the same feature macro, so the guarded block has
# nothing left to supply.
export CPPFLAGS="${CPPFLAGS:-} -DHAVE_LSEEK64=1"

# include/f2fs_fs.h guards `typedef u8 bool` with `#ifndef bool`. In C23 `bool`
# is a keyword rather than a macro, so the guard passes and the typedef stops
# the compile; the code is C17.
export CFLAGS="${CFLAGS:-} -std=gnu17"

# --without-selinux because there is no libselinux on the host. The other three
# are libraries this tree already ships, and a missing one here is answered by
# silently dropping the feature rather than by failing: without blkid,
# mkfs.f2fs cannot see that it is about to overwrite a filesystem, and without
# lz4/lzo2 the compressed-file features are absent from a binary whose recipe
# claims them.
#
# libuuid has no switch, only a probe that drops it quietly, and without it
# uuid_generate() compiles to nothing and every mkfs.f2fs writes the same
# all-zero filesystem UUID. Presetting the probe's cache answer makes it
# required: a missing libuuid fails the link instead.
./configure \
	ac_cv_lib_uuid_uuid_clear=yes \
	--prefix=/usr \
	--libdir=/usr/lib \
	--sbindir=/usr/sbin \
	--disable-static \
	--with-blkid \
	--with-lz4 \
	--with-lzo2 \
	--without-selinux
make
make DESTDIR=$PKG install
