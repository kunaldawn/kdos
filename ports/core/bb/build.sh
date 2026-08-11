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

patch -p1 -i $PORT_SRC/zbuff-int-not-long.patch
patch -p1 -i $PORT_SRC/messager-overlapping-copy.patch

# Same 2001-vintage autoconf as aalib: the K&R conftest main(){return(0);}
# that GCC 14 promoted from warning to error, which surfaces as the
# thoroughly misleading "C compiler cannot create executables".
export CFLAGS="$CFLAGS -Wno-implicit-function-declaration -Wno-implicit-int \
	-Wno-int-conversion -Wno-incompatible-pointer-types \
	-Wno-return-mismatch -Wno-declaration-missing-parameter-type"

./configure --prefix=/usr \
            --mandir=/usr/share/man
make
make DESTDIR=$PKG install
