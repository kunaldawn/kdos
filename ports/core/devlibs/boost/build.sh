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

patch -p1 -i "$PORT_SRC/musl-filesystem-locale.patch"

./bootstrap.sh --prefix=/usr --libdir=/usr/lib \
	--with-icu=/usr \
	--with-python=/usr/bin/python3 \
	--with-libraries=iostreams,system,filesystem,regex,date_time,test,program_options,thread,atomic,charconv,chrono,container,locale,random,coroutine,context,graph,process,log,nowide,serialization,stacktrace,python

cat > user-config.jam <<JAM
using gcc : : $CXX : <cflags>"$CFLAGS" <cxxflags>"$CXXFLAGS" <linkflags>"$LDFLAGS" ;
JAM
./b2 \
	--user-config=user-config.jam \
	--prefix=/usr \
	--libdir=/usr/lib \
	staging-prefix=$PKG/usr \
	--with-iostreams \
	--with-system \
	--with-filesystem \
	--with-regex \
	--with-date_time \
	--with-test \
	--with-program_options \
	--with-thread \
	--with-atomic \
	--with-charconv \
	--with-chrono \
	--with-container \
	--with-locale \
	--with-random \
	--with-coroutine \
	--with-context \
	--with-graph \
	--with-process \
	--with-log \
	--with-nowide \
	--with-serialization \
	--with-stacktrace \
	--with-python \
	boost.stacktrace.backtrace=off \
	variant=release \
	link=shared \
	threading=multi \
	install
test "$(grep -rl /var/cache/kpkg "$PKG/usr/lib/cmake" | wc -l)" = 0
test -e "$PKG/usr/lib/libboost_regex.so"
