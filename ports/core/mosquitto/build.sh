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

mkdir -p build && cd build
# WITH_DOCS, not DOCUMENTATION: cmake reports an unknown -D as a WARNING and
# carries on, so a misspelt option is a setting that silently does nothing.
# WITH_TESTS wants gtest, which is not a port. cJSON is found by pkg-config
# with no option of its own.
#
# Three features each need a library declared in depends. The http_api
# listener (libmicrohttpd) and the mosquitto_ctrl shell are found by a silent
# find_library/find_path and dropped without a word when it misses; the shell
# takes libedit's editline/readline.h ahead of readline whenever both are
# installed. The persist-sqlite plugin (sqlite) is a REQUIRED find_package, so
# a missing sqlite stops configure.
# HTTP_API_DIR is emptied because its cache default is the literal string
# "Default http_api directory": an http_api listener with no http_dir of its
# own would realpath() that, fail, and refuse to start.
# Websockets use the builtin implementation, which needs no libwebsockets and
# listens only where a listener is configured with `protocol websockets`.
cmake .. -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
	-DCMAKE_INSTALL_SYSCONFDIR=/etc \
	-DWITH_WEBSOCKETS=ON -DWITH_WEBSOCKETS_BUILTIN=ON \
	-DWITH_DOCS=ON -DWITH_TESTS=OFF \
	-DWITH_TLS=ON \
	-DWITH_HTTP_API=ON -DHTTP_API_DIR= \
	-DWITH_PLUGIN_PERSIST_SQLITE=ON \
	-DWITH_CTRL_SHELL=ON \
	-DWITH_PLUGIN_EXAMPLES=OFF \
	-DWITH_SYSTEMD=OFF -DWITH_SRV=OFF -DWITH_DLT=OFF
make
make DESTDIR=$PKG install
