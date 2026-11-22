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

for prog in getconf getent iconv; do
	$CC $CFLAGS $LDFLAGS -o $prog $prog.c
	install -Dm755 $prog "$PKG/usr/bin/$prog"
done

install -Dm644 "$SRC/getconf.c" "$PKG/usr/share/licenses/$name/getconf.c"
install -Dm644 "$SRC/getent.c" "$PKG/usr/share/licenses/$name/getent.c"
install -Dm644 "$SRC/iconv.c" "$PKG/usr/share/licenses/$name/iconv.c"
