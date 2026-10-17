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

MANDIR=/usr/share/man
make -R \
	prefix=/usr \
	DESTDIR="$PKG" \
	"INSTALL_DATA=cp -T" \
	man2constdir=$MANDIR/man2 \
	man2typedir=$MANDIR/man2 \
	man3attrdir=$MANDIR/man3 \
	man3constdir=$MANDIR/man3 \
	man3headdir=$MANDIR/man3 \
	man3typedir=$MANDIR/man3 \
	install-man

M="$PKG$MANDIR"
rm "$M/man7/man.7"
rm "$M/man3/fts.3"
rm "$M/man3/getspnam.3" "$M/man5/passwd.5"
rm "$M/man5/rpc.5"
rm "$M/man5/tzfile.5" "$M/man8/tzselect.8" \
   "$M/man8/zdump.8" "$M/man8/zic.8"

for script in diffman-git grepc grepc_c mansect mansectf pdfman sortman; do
	rm "$M/man1/$script.1"
done

rm -r "$M/man0"
