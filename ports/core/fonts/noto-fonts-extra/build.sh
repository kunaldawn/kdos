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

# THE RECIPE'S `version` IS A META-VERSION and each face is pinned in a helper
# of its own, because no one of the nineteen repositories can stand for the rest.
# `ports/update` probes the FIRST source's repository and compares its newest
# release to `version`, and notofonts/arabic releases two styles from one
# repository — NotoNaskhArabic outruns NotoSansArabic — so a `version` that
# tracked it would be rewritten to a tag whose NotoSansArabic asset does not
# exist, and the next fetch would 404. The cost of the meta-version is that
# ports/update offers this port an upgrade it must not take; bump the helper
# by hand instead, and the recipe hash forces the rebuild.

# kpkg copies a .zip into $SRC rather than unpacking it, so the recipe does.
# Each archive into a directory of its own: every one of them carries a
# top-level OFL.txt and they would otherwise overwrite each other. Five of them
# spell those top-level files `../OFL.txt`; unzip drops the parent component
# and lands them in the target directory with the rest, so the path below is
# the same for all nineteen.
install -dm755 $PKG/usr/share/fonts/noto
install -dm755 $PKG/usr/share/licenses/$name

# The five upright weights, hinted, the same set noto-fonts installs: the
# desktop asks for Light through Bold, and /etc/fonts/conf.d sets slight
# hinting, which an unhinted face cannot honour. Oriya is published in Regular
# and Bold only, and its entry names those two; every other entry takes all
# five. None of these scripts has an italic at any weight — fontconfig's
# 90-synthetic.conf shears the upright one, which is what an italic run gets.
# The condensed widths, the UI variants and the variable builds are the same
# glyphs again; nothing here names them.
#
# A list and not a `while read`, because a heredoc fed to a loop is the loop
# body's stdin as well and any tool in it that reads a byte eats the next
# archive's line.
all="Regular Light Medium SemiBold Bold"
for entry in \
	"$name-arabic-$_arab.zip:NotoSansArabic:$all" \
	"$name-armenian-$_armn.zip:NotoSansArmenian:$all" \
	"$name-bengali-$_beng.zip:NotoSansBengali:$all" \
	"$name-devanagari-$_deva.zip:NotoSansDevanagari:$all" \
	"$name-ethiopic-$_ethi.zip:NotoSansEthiopic:$all" \
	"$name-georgian-$_geor.zip:NotoSansGeorgian:$all" \
	"$name-gujarati-$_gujr.zip:NotoSansGujarati:$all" \
	"$name-gurmukhi-$_guru.zip:NotoSansGurmukhi:$all" \
	"$name-hebrew-$_hebr.zip:NotoSansHebrew:$all" \
	"$name-kannada-$_knda.zip:NotoSansKannada:$all" \
	"$name-khmer-$_khmr.zip:NotoSansKhmer:$all" \
	"$name-lao-$_laoo.zip:NotoSansLao:$all" \
	"$name-malayalam-$_mlym.zip:NotoSansMalayalam:$all" \
	"$name-myanmar-$_mymr.zip:NotoSansMyanmar:$all" \
	"$name-oriya-$_orya.zip:NotoSansOriya:Regular Bold" \
	"$name-sinhala-$_sinh.zip:NotoSansSinhala:$all" \
	"$name-tamil-$_taml.zip:NotoSansTamil:$all" \
	"$name-telugu-$_telu.zip:NotoSansTelugu:$all" \
	"$name-thai-$_thai.zip:NotoSansThai:$all"
do
	zip=${entry%%:*}
	rest=${entry#*:}
	fam=${rest%%:*}
	weights=${rest#*:}
	unzip -q -o "$zip" -d "$fam"
	for w in $weights; do
		install -m644 "$fam/$fam/hinted/ttf/$fam-$w.ttf" \
			$PKG/usr/share/fonts/noto/
	done
	install -m644 "$fam/OFL.txt" \
		$PKG/usr/share/licenses/$name/$fam-OFL.txt
done

# `<prefer>` and not `<accept>`, and numbered BETWEEN 56-noto-preferred.conf
# and the 57-dejavu-*.conf set — conf.d is read in filename order, so
# `56-noto-scripts` follows `56-noto-preferred` and precedes `57-dejavu-sans`.
# A prefer inserts immediately before the generic, so a later file's entries sit
# behind an earlier file's, and the resulting order is Noto Sans, these
# seventeen, DejaVu. The last step is the one that matters: DejaVu Sans declares
# lang=ar and lang=he and carries Armenian, Georgian and Lao, so ahead of these
# faces it answers a whole run in any of them out of a face that carries the
# codepoints and not the typography.
#
# SAFE ONLY BECAUSE NONE OF THEM IS EVER REACHED FOR A SYMBOL. Seventeen more
# families ahead of DejaVu would otherwise break the rule
# 56-noto-preferred.conf states: DejaVu is the face that answers a symbol Noto
# Sans Mono lacks. Over the seventeen cmaps: no arrow, no dingbat, no
# box-drawing character, no Latin letter, no Greek, no Cyrillic. What they
# carry outside their own script is the space, the ASCII digits, a little
# shared punctuation, × ÷ and the minus sign, the dotted circle combining marks
# are drawn on, and their own currency sign, every one of which Noto Sans and
# Noto Sans Mono answer at the head of the chain, so the walk never descends
# this far for them.
#
# TWO FACES ARE INSTALLED AND LEFT OFF THE LIST on that same measurement.
# Noto Sans Gurmukhi carries U+262C from Miscellaneous Symbols, and Noto Sans
# Sinhala carries the whole ASCII alphabet — ahead of DejaVu Serif it would
# draw every Latin letter of a serif request. DejaVu has neither script, so
# both are still found by coverage for their own runs, behind every listed face.
# A face added to this list is safe only on that measurement — read its cmap,
# not its name; one with symbol or Latin coverage stays off it.
#
# THE SERIF LIST IS THE SANS FACES, because this port ships no serif for these
# scripts. A serif request for Thai then draws Noto Sans Thai instead of falling
# past every face on the image; the alternative is not a serif Thai, it is
# DejaVu Serif, which has no Thai at all.
#
# CJK IS NOT NAMED HERE and must not be. `noto-cjk` ships one collection and no
# fontconfig rule, which is what lets fontconfig score its five regional faces
# by the pattern's language; putting those families on the pattern would make
# family outrank language and draw every Han codepoint in one region's forms.
install -dm755 $PKG/etc/fonts/conf.d
cat > $PKG/etc/fonts/conf.d/56-noto-scripts.conf <<'XML'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE fontconfig SYSTEM "../fonts.dtd">
<fontconfig>
  <alias>
    <family>sans-serif</family>
    <prefer>
      <family>Noto Sans Arabic</family>
      <family>Noto Sans Hebrew</family>
      <family>Noto Sans Armenian</family>
      <family>Noto Sans Georgian</family>
      <family>Noto Sans Devanagari</family>
      <family>Noto Sans Bengali</family>
      <family>Noto Sans Tamil</family>
      <family>Noto Sans Telugu</family>
      <family>Noto Sans Gujarati</family>
      <family>Noto Sans Kannada</family>
      <family>Noto Sans Malayalam</family>
      <family>Noto Sans Oriya</family>
      <family>Noto Sans Thai</family>
      <family>Noto Sans Lao</family>
      <family>Noto Sans Khmer</family>
      <family>Noto Sans Myanmar</family>
      <family>Noto Sans Ethiopic</family>
    </prefer>
  </alias>
  <alias>
    <family>serif</family>
    <prefer>
      <family>Noto Sans Arabic</family>
      <family>Noto Sans Hebrew</family>
      <family>Noto Sans Armenian</family>
      <family>Noto Sans Georgian</family>
      <family>Noto Sans Devanagari</family>
      <family>Noto Sans Bengali</family>
      <family>Noto Sans Tamil</family>
      <family>Noto Sans Telugu</family>
      <family>Noto Sans Gujarati</family>
      <family>Noto Sans Kannada</family>
      <family>Noto Sans Malayalam</family>
      <family>Noto Sans Oriya</family>
      <family>Noto Sans Thai</family>
      <family>Noto Sans Lao</family>
      <family>Noto Sans Khmer</family>
      <family>Noto Sans Myanmar</family>
      <family>Noto Sans Ethiopic</family>
    </prefer>
  </alias>
  <alias>
    <family>monospace</family>
    <prefer>
      <family>Noto Sans Arabic</family>
      <family>Noto Sans Hebrew</family>
      <family>Noto Sans Armenian</family>
      <family>Noto Sans Georgian</family>
      <family>Noto Sans Devanagari</family>
      <family>Noto Sans Bengali</family>
      <family>Noto Sans Tamil</family>
      <family>Noto Sans Telugu</family>
      <family>Noto Sans Gujarati</family>
      <family>Noto Sans Kannada</family>
      <family>Noto Sans Malayalam</family>
      <family>Noto Sans Oriya</family>
      <family>Noto Sans Thai</family>
      <family>Noto Sans Lao</family>
      <family>Noto Sans Khmer</family>
      <family>Noto Sans Myanmar</family>
      <family>Noto Sans Ethiopic</family>
    </prefer>
  </alias>
</fontconfig>
XML
