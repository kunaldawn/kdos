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

# RTREE AND GEOPOLY ARE NOT OPTIONAL FEATURES HERE, they are what makes a
# GeoPackage work. GPKG stores its spatial index as an R*Tree virtual table, so
# a sqlite without it reads the file and cannot answer a bounding-box query —
# gdal refuses to build the driver rather than ship that, which is how this was
# found. Both are compile-time and cost nothing when unused.
export CPPFLAGS="$CPPFLAGS \
	-DSQLITE_ENABLE_COLUMN_METADATA=1 \
	-DSQLITE_ENABLE_UNLOCK_NOTIFY \
	-DSQLITE_ENABLE_DBSTAT_VTAB=1 \
	-DSQLITE_ENABLE_FTS3_TOKENIZER=1 \
	-DSQLITE_ENABLE_FTS3_PARENTHESIS \
	-DSQLITE_SECURE_DELETE \
	-DSQLITE_ENABLE_STMTVTAB \
	-DSQLITE_MAX_VARIABLE_NUMBER=250000 \
	-DSQLITE_MAX_EXPR_DEPTH=10000 \
	-DSQLITE_ENABLE_MATH_FUNCTIONS \
	-DSQLITE_ENABLE_RTREE=1 \
	-DSQLITE_ENABLE_GEOPOLY=1"
# --session is the changeset API (it brings the pre-update hook with it) and
# --dbpage the raw-page table that the shell's .recover reads; both are
# compile-time only. --enable-readline makes a missing readline stop the build
# instead of leaving a shell with no line editing.
./configure --prefix=/usr \
	--enable-fts3 \
	--enable-fts4 \
	--enable-fts5 \
	--session \
	--dbpage \
	--enable-readline
make
make DESTDIR=$PKG install
