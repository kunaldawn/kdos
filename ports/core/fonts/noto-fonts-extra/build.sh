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

install -dm755 $PKG/usr/share/fonts/noto
install -dm755 $PKG/usr/share/licenses/$name

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
