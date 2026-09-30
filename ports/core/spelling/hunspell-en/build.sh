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

cd dictionaries/en

# One directory per engine, where hunspell, hyphen and mythes consumers look:
# LibreOffice's system dictionaries, enchant's hunspell provider and Firefox
# all read /usr/share/hunspell, /usr/share/hyphen and /usr/share/mythes.
install -dm755 $PKG/usr/share/hunspell
for d in en_US en_GB en_AU en_CA en_ZA; do
	install -m644 $d.aff $d.dic $PKG/usr/share/hunspell/
done

install -Dm644 -t $PKG/usr/share/hyphen hyph_en_US.dic hyph_en_GB.dic

# The release carries the thesaurus without its index, and MyThes opens
# nothing without one.
th_gen_idx.pl -o th_en_US_v2.idx < th_en_US_v2.dat
install -Dm644 -t $PKG/usr/share/mythes th_en_US_v2.dat th_en_US_v2.idx
ln -s th_en_US_v2.dat $PKG/usr/share/mythes/th_en_GB_v2.dat
ln -s th_en_US_v2.idx $PKG/usr/share/mythes/th_en_GB_v2.idx

install -Dm644 -t $PKG/usr/share/licenses/$name \
	license.txt WordNet_license.txt README_en_US.txt README_en_GB.txt \
	README_en_AU.txt README_en_CA.txt README_en_ZA.txt \
	README_hyph_en_US.txt README_hyph_en_GB.txt README_en_GB_thes.txt
