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

patch -p1 -i $PORT_SRC/imagemagick7.patch

# Same 2001 aa-project autoconf 2.13 conftest as aalib: `main(){return(0);}`,
# which GCC 14 promoted from warning to error, so configure decides the
# compiler cannot create executables. See the recurring-fixes note in CLAUDE.md.
export CFLAGS="$CFLAGS -Wno-implicit-function-declaration -Wno-implicit-int \
	-Wno-int-conversion -Wno-incompatible-pointer-types \
	-Wno-return-mismatch -Wno-declaration-missing-parameter-type"

./configure --prefix=/usr \
            --mandir=/usr/share/man
make
make DESTDIR=$PKG install
