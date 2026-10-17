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

./configure --prefix=/usr --libdir=/usr/lib --disable-static \
	--with-boost=/usr \
	boost_cv_rpath_link_ldflag=-L \
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
