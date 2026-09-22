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
# of its own, because no one of the ten repositories can stand for the rest.
# `ports/update` probes the FIRST source's repository and compares its newest
# release to `version`, and notofonts/arabic releases two styles from one
# repository — NotoNaskhArabic outruns NotoSansArabic — so a `version` that
# tracked it would be rewritten to a tag whose NotoSansArabic asset does not
# exist, and the next fetch would 404. The cost of the meta-version is that
# ports/update offers this port an upgrade it must not take; bump the helper
# by hand instead, and the recipe hash forces the rebuild.

# kpkg copies a .zip into $SRC rather than unpacking it, so the recipe does.
# Each archive into a directory of its own: every one of them carries a
# top-level OFL.txt and they would otherwise overwrite each other. Two of them
# spell those top-level files `../OFL.txt`; unzip drops the parent component
# and lands them in the target directory with the rest, so the path below is
# the same for all ten.
install -dm755 $PKG/usr/share/fonts/noto
install -dm755 $PKG/usr/share/licenses/$name

# The five upright weights, hinted, the same set noto-fonts installs: the
# desktop asks for Light through Bold, and /etc/fonts/conf.d sets slight
# hinting, which an unhinted face cannot honour. None of these scripts has an
# italic at any weight — fontconfig's 90-synthetic.conf shears the upright one,
# which is what an italic run gets. The condensed widths, the UI variants and
# the variable builds are the same glyphs again; nothing here names them.
#
# A list and not a `while read`, because a heredoc fed to a loop is the loop
# body's stdin as well and any tool in it that reads a byte eats the next
# archive's line.
for pair in \
	"$name-arabic-$_arab.zip:NotoSansArabic" \
	"$name-bengali-$_beng.zip:NotoSansBengali" \
	"$name-devanagari-$_deva.zip:NotoSansDevanagari" \
	"$name-ethiopic-$_ethi.zip:NotoSansEthiopic" \
	"$name-gujarati-$_gujr.zip:NotoSansGujarati" \
	"$name-hebrew-$_hebr.zip:NotoSansHebrew" \
	"$name-khmer-$_khmr.zip:NotoSansKhmer" \
	"$name-tamil-$_taml.zip:NotoSansTamil" \
	"$name-telugu-$_telu.zip:NotoSansTelugu" \
	"$name-thai-$_thai.zip:NotoSansThai"
do
	zip=${pair%%:*}
	fam=${pair#*:}
	unzip -q -o "$zip" -d "$fam"
	for w in Regular Light Medium SemiBold Bold; do
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
# behind an earlier file's, and the resulting order is Noto Sans, these ten,
# DejaVu. The last step is the one that matters: DejaVu Sans declares lang=ar
# and lang=he, so ahead of these faces it answers a whole Arabic or Hebrew run
# out of a face that carries the codepoints and not the typography.
#
# SAFE ONLY BECAUSE NONE OF THEM IS EVER REACHED FOR A SYMBOL. Ten more
# families ahead of DejaVu would otherwise break the rule
# 56-noto-preferred.conf states: DejaVu is the face that answers a symbol Noto
# Sans Mono lacks. Over all ten cmaps: no arrow, no dingbat, no box-drawing
# character, no Latin letter, no Greek, no Cyrillic. What they carry outside
# their own script is the space, the ASCII digits and a little shared
# punctuation, every one of which Noto Sans and Noto Sans Mono answer at the
# head of the chain, so the walk never descends this far for them.
# A face added to this list is safe only on that measurement — read its cmap,
# not its name; one with symbol coverage belongs after 57 instead.
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
      <family>Noto Sans Devanagari</family>
      <family>Noto Sans Bengali</family>
      <family>Noto Sans Tamil</family>
      <family>Noto Sans Telugu</family>
      <family>Noto Sans Gujarati</family>
      <family>Noto Sans Thai</family>
      <family>Noto Sans Khmer</family>
      <family>Noto Sans Ethiopic</family>
    </prefer>
  </alias>
  <alias>
    <family>serif</family>
    <prefer>
      <family>Noto Sans Arabic</family>
      <family>Noto Sans Hebrew</family>
      <family>Noto Sans Devanagari</family>
      <family>Noto Sans Bengali</family>
      <family>Noto Sans Tamil</family>
      <family>Noto Sans Telugu</family>
      <family>Noto Sans Gujarati</family>
      <family>Noto Sans Thai</family>
      <family>Noto Sans Khmer</family>
      <family>Noto Sans Ethiopic</family>
    </prefer>
  </alias>
  <alias>
    <family>monospace</family>
    <prefer>
      <family>Noto Sans Arabic</family>
      <family>Noto Sans Hebrew</family>
      <family>Noto Sans Devanagari</family>
      <family>Noto Sans Bengali</family>
      <family>Noto Sans Tamil</family>
      <family>Noto Sans Telugu</family>
      <family>Noto Sans Gujarati</family>
      <family>Noto Sans Thai</family>
      <family>Noto Sans Khmer</family>
      <family>Noto Sans Ethiopic</family>
    </prefer>
  </alias>
</fontconfig>
XML
