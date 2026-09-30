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


# Terminus drawn as outlines, compiled here from the same BDF sources the
# terminus-font port builds the console font from. This is upstream's own
# pipeline (mkttf): every pixel size goes into one face as a bitmap strike,
# the largest is traced into curves with potrace, and fontforge writes the
# TrueType file. The family is `Terminus (TTF)` and the files are
# TerminusTTF[-Bold|-Italic|-Bold-Italic]-$version.ttf, the names upstream's
# release carries; $version is mkttf's release number for this Terminus.

# mkitalic slants a BDF, which is where the two italic faces come from. It is
# a build tool only and is not installed.
make -C "$SRC_ROOT/mkbold-mkitalic-$_mkbold" CC="${CC:-cc}" mkitalic
mkttf="$SRC_ROOT/mkttf-$_mkttf"

# The steps of mkttf.sh, run here rather than through it: the script stamps
# the year of the build into every face's copyright notice, and this build
# takes the year from SOURCE_DATE_EPOCH so two builds of one recipe are the
# same file. Everything else it passes to mkttf.py is passed unchanged.
year=$(date -u -d "@${SOURCE_DATE_EPOCH:-0}" +%Y)
bdf="$SRC_ROOT/bdf"
mkdir -p "$bdf"
cp ter-u*[nb].bdf "$bdf/"
for f in "$bdf"/ter-u*n.bdf; do
	base=${f%n.bdf}
	"$SRC_ROOT/mkbold-mkitalic-$_mkbold/mkitalic" < "$f" > "${base}i.bdf"
	"$SRC_ROOT/mkbold-mkitalic-$_mkbold/mkitalic" < "${base}b.bdf" > "${base}BI.bdf"
done

# potrace-wrapper.sh, mkttf.py's default tracer, enlarges each glyph with
# ImageMagick before potrace sees it; fontforge finds it through that path.
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
