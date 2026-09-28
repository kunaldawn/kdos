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

# The 0.20 series: its library names end in -0.20, the newest LabPlot's
# finder looks for. The spreadsheet model links ixion, and the tools and
# python module are built. Parquet needs Apache Arrow, which the tree does not
# carry, so that filter is off.
./configure --prefix=/usr --libdir=/usr/lib --disable-static \
	--with-boost=/usr \
	--enable-spreadsheet-model \
	--enable-python \
	--with-tools \
	--with-ods-filter \
	--with-xlsx-filter \
	--with-xls-xml-filter \
	--with-gnumeric-filter \
	--without-parquet-filter \
	--without-benchmark \
	--without-doc-example \
	--disable-debug-utils
make
make DESTDIR=$PKG install
