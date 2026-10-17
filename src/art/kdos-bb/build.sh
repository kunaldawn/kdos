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

cd "$PORT_SRC/src"

SRC="autopilo.c backconv.c bb.c credits.c credits2.c ctrl87.c
     fk1.c fk2.c fk3.c fk4.c formulas.c hh1.c hh2.c hh3.c hh4.c
     image.c julia.c kdostux.c kt1.c kt2.c kt3.c kt4.c main.c messager.c minilzo.c
     ms1.c ms2.c ms3.c ms4.c print.c
     scene1.c scene2.c scene3.c scene4.c scene5.c scene7.c scene8.c scene9.c
     tex.c textform.c timers.c uncompfn.c zeb.c zoom.c"

cc $CFLAGS -O2 -o "$SRC_ROOT/kdos-bb" $SRC \
   -DSOUNDDIR='"/usr/share/kdos-bb"' \
   -Wno-implicit-function-declaration -Wno-implicit-int \
   -Wno-int-conversion -Wno-incompatible-pointer-types \
   -Wno-return-mismatch -Wno-declaration-missing-parameter-type \
   $LDFLAGS -laa -lmikmod -lncurses -lm -lpthread

install -Dm755 "$SRC_ROOT/kdos-bb" "$PKG/usr/bin/kdos-bb"
for m in "$PORT_SRC"/data/*.s3m; do
	install -Dm644 "$m" "$PKG/usr/share/kdos-bb/$(basename "$m")"
done
install -Dm644 "$PORT_SRC/kdos-bb.1" "$PKG/usr/share/man/man1/kdos-bb.1"
install -Dm644 "$PORT_SRC/COPYING" "$PKG/usr/share/licenses/kdos-bb/COPYING"
install -Dm644 "$PORT_SRC/AUTHORS" "$PKG/usr/share/licenses/kdos-bb/AUTHORS"
