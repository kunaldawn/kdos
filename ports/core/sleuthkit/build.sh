#!/bin/bash


autoreconf -f -i

# IT READS DAMAGED FILESYSTEMS mount(8) REFUSES, which is the property that
# earns it a place beside testdisk rather than overlapping it: testdisk repairs
# a partition table so the kernel will mount the thing, and this one never asks
# the kernel at all — `fls` and `icat` walk the metadata directly out of an
# image, so a filesystem too damaged to mount still gives up its files.
#
# --disable-java because Autopsy is a Java application and is not shipped; the
# bindings would build a jar nothing on this machine can run.
#
# --without-libewf and --without-afflib: both are forensic CONTAINER formats
# (E01 and AFF), and neither library is a port. The consequence is stated
# rather than hidden — a raw image, a split raw image and a live device all
# work, and an E01 acquired by somebody else's tool does not.
./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--disable-static \
	--disable-java \
	--without-afflib \
	--without-libewf \
	--with-sqlite
make
make DESTDIR=$PKG install
