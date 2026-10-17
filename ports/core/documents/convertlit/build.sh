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


unzip -q -o convertlit-$version.zip
patch -p1 -i "$PORT_SRC/bounds.patch"

cd lib
${CC:-cc} $CFLAGS -funsigned-char -Ilzx -I. -c litsections.c
${CC:-cc} $CFLAGS -funsigned-char -include string.h -Inewlzx -I. -c newlzx/lzxglue.c
${CC:-cc} $CFLAGS -funsigned-char -Ides -Isha -Inewlzx -I. -c \
	litatom.c litdrm.c litlib.c litembiggen.c littags.c litmetatags.c \
	litmanifest.c litdirectory.c litheaders.c litutil.c \
	sha/mssha1.c des/des.c newlzx/lzxd.c
cd ../clit$_ver
${CC:-cc} $CFLAGS $LDFLAGS -funsigned-char -include string.h -include ctype.h -include sys/stat.h -I../lib -I../lib/des -I. -o clit \
	clit.c hexdump.c drm5.c explode.c transmute.c display.c utils.c manifest.c \
	../lib/*.o -ltommath

install -Dm755 clit $PKG/usr/bin/clit
