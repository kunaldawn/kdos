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

# THE PATCH IS THE ONE CASE THE HARD RULE ALLOWS, because there is no flag.
# blake2.h writes `JTR_ALIGN(64) typedef struct __blake2b_state {...}
# blake2b_state;` — the attribute is BEFORE the typedef, so it attaches to the
# declaration rather than to the type, the struct's SIZE is never padded to its
# alignment, and `blake2b_state S[4][1]` inside blake2bp_state is then an array
# whose element size is not a multiple of its element alignment. GCC made that
# a hard error, with no -Wno- to answer it. Moving the attribute after the
# closing brace applies it to the type, which is what upstream meant.
#
# Only the two `_state` structs are touched. The `_sp_state` pair has the same
# misplacement and is NOT in an array, so it does not error — and its
# attribute sits inside an `#if defined(JOHN_NO_SIMD)` where "fixing" it would
# change the alignment of the non-SIMD build.
patch -p1 -i "$PORT_SRC/blake2-align.patch"

cd src
./configure --prefix=/usr --disable-native-tests --without-openmpi

# --disable-native-tests IS WHAT MAKES THIS REPRODUCIBLE. john's configure
# probes THIS CPU's instruction set and bakes the best it finds into the
# binary, so a package built on a machine with AVX-512 crashes on one without
# it — and the failure is SIGILL at run time, not a link error. That is exactly
# the blind optimisation `kdos march` exists to replace with a measurement.
make -j1

# WHAT IT IS FOR: reading a hash out of a LUKS header, a KeePass database, an
# encrypted PDF or ZIP, or /etc/shadow, and answering "how long would this
# take" — about your own material. NO WORDLIST IS SHIPPED. The bundled
# password.lst is upstream's small default and stays; breach corpora of unclear
# provenance are not something this distro redistributes, and SecLists is MIT
# and one clone away for anyone who wants it.
install -dm755 $PKG/usr/share/john $PKG/usr/bin
cd ../run

# --prefix=/usr builds john system-wide: it reads john.conf, the rules and the
# .chr files from /usr/share/john and keeps its pot, log and session files in
# ~/.john. Without it john looks for its configuration beside argv[0], which a
# command found on $PATH does not have. Compiled programs and the links to
# john go to /usr/bin; everything else in run/ — scripts, the python modules
# they import, data — goes to /usr/share/john.
for f in *; do
	if [ -L "$f" ]; then
		ln -s "$(readlink "$f")" $PKG/usr/bin/"$f"
	elif [ -f "$f" ] && [ "$(head -c4 "$f")" = $'\x7fELF' ]; then
		install -m755 "$f" $PKG/usr/bin/"$f"
	else
		cp -a "$f" $PKG/usr/share/john/
	fi
done

# The *2john converters are the half people actually reach for — they read a
# container and print the hash john takes. The scripted ones get a symlink so
# `ssh2john.py` is a command rather than a path somebody has to remember.
for f in *2john*; do
	[ -e $PKG/usr/bin/"$f" ] && continue
	[ "$(head -c2 "$f")" = '#!' ] || continue
	chmod 755 $PKG/usr/share/john/"$f"
	ln -s ../share/john/"$f" $PKG/usr/bin/"$f"
done
