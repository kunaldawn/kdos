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

make -C "$SRC_ROOT/mkbold-mkitalic-$_mkbold" CC="${CC:-cc}" mkitalic
mkttf="$SRC_ROOT/mkttf-$_mkttf"

year=$(date -u -d "@${SOURCE_DATE_EPOCH:-0}" +%Y)
bdf="$SRC_ROOT/bdf"
mkdir -p "$bdf"
cp ter-u*[nb].bdf "$bdf/"
for f in "$bdf"/ter-u*n.bdf; do
	base=${f%n.bdf}
	"$SRC_ROOT/mkbold-mkitalic-$_mkbold/mkitalic" < "$f" > "${base}i.bdf"
	"$SRC_ROOT/mkbold-mkitalic-$_mkbold/mkitalic" < "${base}b.bdf" > "${base}BI.bdf"
done

for weight in Normal Bold Italic Bold-Italic; do
	case $weight in
		Normal)      suffix=n;  fname=TerminusTTF ;          nice='Terminus (TTF)' ;;
		Bold)        suffix=b;  fname=TerminusTTF-Bold ;     nice='Terminus (TTF) Bold' ;;
		Italic)      suffix=i;  fname=TerminusTTF-Italic ;   nice='Terminus (TTF) Italic' ;;
		Bold-Italic) suffix=BI; fname=TerminusTTF-Bold-Italic; nice='Terminus (TTF) Bold-Italic' ;;
	esac
	mkdir -p "$SRC_ROOT/out/$weight"
	(
		cd "$SRC_ROOT/out/$weight"
		python3 "$mkttf/mkttf.py" \
			-f 'Terminus (TTF)' -n "$fname" -N "$nice" \
			-C "; Copyright (C) $year Tilman Blumenbach; Licensed under the SIL Open Font License, Version 1.1" \
			-e AX86 -A ' -a 0' -V "$version" -O \
			"$bdf"/ter-u*"$suffix".bdf
	)
done

install -dm755 "$PKG/usr/share/fonts/TTF"
install -m644 "$SRC_ROOT"/out/*/*.ttf "$PKG/usr/share/fonts/TTF/"

install -Dm644 "$mkttf/terminus_ttf_distribution_license.txt" \
	"$PKG/usr/share/licenses/$name/COPYING"
