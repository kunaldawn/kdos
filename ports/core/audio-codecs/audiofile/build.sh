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

# Debian's series, in its order: a GCC 6+ build fix and the bounds and
# overflow checks for the IMA, MS ADPCM, NeXT and WAVE decoders. The library
# parses whatever file an editor is handed, so none of them is optional. The
# series is applied by name because a glob would sort 0013 before 01.
for p in \
	01_gcc6 \
	03_CVE-2015-7747 \
	04_clamp-index-values-to-fix-index-overflow-in-IMA.cpp \
	05_Always-check-the-number-of-coefficients \
	06_Check-for-multiplication-overflow-in-MSADPCM-decodeSam \
	07_Check-for-multiplication-overflow-in-sfconvert \
	08_Fix-signature-of-multiplyCheckOverflow.-It-returns-a-b \
	09_Actually-fail-when-error-occurs-in-parseFormat \
	10_Check-for-division-by-zero-in-BlockCodec-runPull \
	11_CVE-2018-13440 \
	12_CVE-2018-17095 \
	0013-Fix-CVE-2022-24599 \
	0014-Partial-fix-of-CVE-2019-13147 \
	0015-Partial-fix-of-CVE-2019-13147; do
	patch -p1 -i "$PORT_SRC/$p.patch"
done

# The series edits test/Makefile.am, which makes the release's generated
# Makefile.in stale; regenerating up front keeps make from calling the 1.13
# automake the tarball was made with. The manual pages ship prebuilt in the
# tarball and are installed as they are.
autoreconf -fi
./configure --prefix=/usr --libdir=/usr/lib --disable-static \
	--enable-flac \
	--disable-examples \
	--disable-werror \
	--disable-coverage \
	--disable-valgrind
make
make DESTDIR=$PKG install
