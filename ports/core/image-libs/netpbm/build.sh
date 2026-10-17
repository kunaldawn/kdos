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

cp config.mk.in config.mk

conf=(
	DEFAULT_TARGET=nonmerge
	NETPBMLIBTYPE=unixshared
	NETPBMLIBSUFFIX=so
	STATICLIB_TOO=Y
	CC=gcc
	LDRELOC="ld --reloc"
	LINKER_CAN_DO_EXPLICIT_LIBRARY=Y
	LINKERISCOMPILER=Y
	CFLAGS_SHLIB=-fPIC
	STRIPFLAG=
	WANT_SSE=Y
	TIFFLIB=-ltiff
	TIFFHDR_DIR=
	JPEGLIB=-ljpeg
	JPEGHDR_DIR=
	PNGLIB=-lpng
	PNGHDR_DIR=
	ZLIB=-lz
	ZHDR_DIR=
	JASPERLIB=-ljasper
	JASPERHDR_DIR=
	X11LIB=NONE
	LINUXSVGALIB=NONE
	RGB_DB_PATH=/usr/share/netpbm/rgb.txt
)

make "${conf[@]}"
make "${conf[@]}" package pkgdir="$SRC_ROOT/netpbm-pkg"

P=$SRC_ROOT/netpbm-pkg
install -d "$PKG/usr/bin" "$PKG/usr/lib/pkgconfig" "$PKG/usr/include" "$PKG/usr/share/netpbm"
cp -a "$P"/bin/. "$PKG/usr/bin/"
cp -a "$P"/lib/. "$PKG/usr/lib/"
cp -a "$P"/sharedlink/. "$PKG/usr/lib/"
cp -a "$P"/staticlink/. "$PKG/usr/lib/"
cp -a "$P"/include/. "$PKG/usr/include/"
cp -a "$P"/misc/. "$PKG/usr/share/netpbm/"
cat > "$PKG/usr/lib/pkgconfig/netpbm.pc" <<PC
Name: Netpbm
Description: Graphics utilities
Version: $version
Libs: -L/usr/lib -lnetpbm
Cflags: -I/usr/include
PC

mkdir man && cd man
python3 ../buildtools/makeman -d ../userguide $(cd ../userguide && ls *.html | grep -v -x -e directory.html -e error.html -e hpcdtoppm.html -e liberror.html -e libnetpbm_dir.html -e ppmsvgalib.html -e vidtoppm.html)
mv index.1 netpbm.1
for s in 1 3 5; do
	install -d "$PKG/usr/share/man/man$s"
	install -m644 *.$s "$PKG/usr/share/man/man$s/"
done
