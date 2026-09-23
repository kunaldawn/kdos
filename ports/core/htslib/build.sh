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

# THE THREE VERSIONS MOVE TOGETHER. htslib, samtools and bcftools are released
# from one project on one version number and each checks the library it was
# built against; mixing them produces a runtime complaint about the htslib
# version rather than a link error, so all three are pinned to 1.24 here and
# bumping one alone is the way this breaks.
#
# libcurl is what lets these read a remote CRAM reference, which sounds like a
# network feature and is not only that: a CRAM file stores reads as differences
# against a reference it names by checksum, so without the fetch path a CRAM
# with no local reference is unreadable rather than slow.
#
# libdeflate replaces zlib for BGZF deflate and CRC, which makes BAM
# compression several times faster in samtools and bcftools as well.
./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--enable-libcurl \
	--enable-s3 \
	--enable-gcs \
	--enable-ref-cache \
	--enable-bz2 \
	--enable-lzma \
	--with-libdeflate \
	--disable-plugins \
	--disable-static
make
make DESTDIR=$PKG install
