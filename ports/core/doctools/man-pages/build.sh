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


# The project's own makefile, with GNU make's -R, which it refuses to run
# without. install-man stages the pages alone: install-bin would add the
# project's page-maintenance scripts to /usr/bin.
#
# INSTALL_DATA is cp because the makefile calls it with -T, a GNU install flag
# toybox's install rejects, and toybox is the install on $PATH. cp gives each
# page the build stage's mode under kpkg's 022 umask, which is 0644.
#
# The subsection directories (man2type, man3const, …) are folded into their
# base section's directory, where the pages keep their own suffix
# (FILE.3type). mandoc's `man FILE` looks in man3/ for FILE.3*, and never in
# man3type/, so a page left there answers only an explicit `man 3type FILE`.
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

# A page another port installs belongs to that port, and a second copy here is
# a file conflict that fails the build. rm without -f, so a page upstream drops
# or renames stops the build here rather than leaving a stale exclusion. The
# .so pages that name one of these (fts_open.3, getspent.3, …) stay: mandoc
# resolves a .so against the manual root, so they reach the owner's copy.
M="$PKG$MANDIR"
rm "$M/man7/man.7"                                   # mandoc
rm "$M/man3/fts.3"                                   # musl-fts
rm "$M/man3/getspnam.3" "$M/man5/passwd.5"           # shadow
rm "$M/man5/rpc.5"                                   # libtirpc, beside /etc/rpc
rm "$M/man5/tzfile.5" "$M/man8/tzselect.8" \
   "$M/man8/zdump.8" "$M/man8/zic.8"                 # tzdata, which carries tzcode

# The pages of the project's own maintenance scripts, which install-man does
# not install.
for script in diffman-git grepc grepc_c mansect mansectf pdfman sortman; do
	rm "$M/man1/$script.1"
done

# Section 0's one page is roff source under a .0 suffix, which mandoc's indexer
# takes for a preformatted page: it would index the markup as plain text.
rm -r "$M/man0"
