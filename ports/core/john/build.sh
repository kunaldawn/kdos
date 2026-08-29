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
./configure --disable-native-tests --without-openmpi

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
for f in john *.pl *.py *.rb *.lua; do
	[ -e "$f" ] || continue
	install -Dm755 "$f" $PKG/usr/share/john/"$f"
done
cp -a *.conf *.chr *.lst rules dynamic*.conf $PKG/usr/share/john/ 2>/dev/null || true
ln -s ../share/john/john $PKG/usr/bin/john

# The *2john converters are the half people actually reach for — they read a
# container and print the hash john takes. Each is a symlink so `zip2john` is a
# command rather than a path somebody has to remember.
for f in *2john*; do
	[ -f "$f" ] || continue
	ln -s ../share/john/"$f" $PKG/usr/bin/"$f"
done
