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

autoreconf -fi
./configure --prefix=/usr --libdir=/usr/lib --disable-static \
	--enable-flac \
	--disable-examples \
	--disable-werror \
	--disable-coverage \
	--disable-valgrind
make
make DESTDIR=$PKG install
