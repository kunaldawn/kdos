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

TL_ROOTS="collection-basic collection-latex collection-latexrecommended
	collection-latexextra collection-fontsrecommended collection-fontutils
	collection-luatex collection-xetex collection-metapost
	collection-plaingeneric collection-mathscience collection-pictures
	collection-pstricks collection-bibtexextra collection-binextra
	collection-langenglish latexmk"

TEXMF_SRC="$SRC_ROOT/$name-$version-texmf"
ITL_SRC="$SRC_ROOT/install-tl-$version"

patch -p1 -i "$PORT_SRC/texmfmp-fix-format-specifier.patch"

patch -p1 -i "$PORT_SRC/dvisvgm-cxx20.patch"

mkdir build
cd build
../configure \
	--prefix=/usr \
	--sysconfdir=/etc \
	--libdir=/usr/lib \
	--datarootdir=/usr/share \
	--mandir=/usr/share/man \
	--infodir=/usr/share/info \
	--disable-native-texlive-build \
	--disable-multiplatform \
	--disable-missing \
	--enable-shared \
	--disable-static \
	--with-banner-add=/KDOS \
	--without-x \
	--disable-xdvik \
	--disable-xindy \
	--disable-xpdfopen \
	--with-system-zlib \
	--with-system-libpng \
	--with-system-freetype2 \
	--with-system-gmp \
	--with-system-mpfr \
	--with-system-cairo \
	--with-system-pixman \
	--with-system-gd \
	--with-system-potrace \
	--with-system-libpaper \
	--with-system-icu \
	--with-system-harfbuzz \
	--with-system-graphite2 \
	--with-system-zziplib
make
make DESTDIR="$PKG" install
cd ..

rm -f "$PKG/usr/bin/tlmgr" "$PKG/usr/share/man/man1/tlmgr.1"
rm -rf "$PKG/usr/share/texmf-dist/doc"

mkdir -p "$PKG/usr/share/tlpkg"
cp -r "$ITL_SRC/tlpkg/TeXLive" "$PKG/usr/share/tlpkg/"

perl -I"$ITL_SRC/tlpkg" - "$SRC_ROOT" "$TEXMF_SRC" "$PKG/usr/share" $TL_ROOTS <<'PERL'
use strict; use warnings;
use File::Basename qw(dirname);
use File::Copy qw(move);
use File::Path qw(make_path);
use TeXLive::TLPDB;
use TeXLive::TLUtils;

my ($dbroot, $from, $to, @roots) = @ARGV;
my $full = TeXLive::TLPDB->new();
$full->from_file("$dbroot/tlpkg/texlive.tlpdb", media => "local_uncompressed")
	or die "cannot read $dbroot/tlpkg/texlive.tlpdb\n";

my (%sel, @queue);
@queue = @roots;
while (defined(my $p = shift @queue)) {
	next if $sel{$p} || $p =~ /\.ARCH$/;
	my $o = $full->get_package($p) or die "no package $p in the tlpdb\n";
	$sel{$p} = $o;
	push @queue, $o->depends;
}

my $sub = TeXLive::TLPDB->new();
$sub->add_tlpobj($full->get_package("00texlive.config"));
my $n = 0;
for my $p (sort keys %sel) {
	my $o = $sel{$p};
	$sub->add_tlpobj($o);
	for my $f ($o->runfiles) {
		next unless $f =~ m{^texmf-dist/};
		-e "$from/$f" || -l "$from/$f" or die "$p names $f, which the texmf tarball does not hold\n";
		make_path(dirname("$to/$f"));
		unlink "$to/$f";
		move("$from/$f", "$to/$f") or die "move $f: $!\n";
		$n++;
	}
	for my $f ($o->docfiles) {
		my $dest;
		$dest = "$to/man/man$1/$2" if $f =~ m{^texmf-dist/doc/man/man(\d)/([^/]+\.\d)$};
		$dest = "$to/info/$1" if $f =~ m{^texmf-dist/doc/info/([^/]+\.info)$};
		next if !$dest || -e $dest;
		make_path(dirname($dest));
		move("$from/$f", $dest) or die "move $f: $!\n";
	}
}
printf "texmf: %d packages, %d files\n", scalar(keys %sel), $n;

sub config {
	my ($head, $dest, $cc, $post, @lines) = @_;
	open(my $h, '<', "$to/$head") or die "$head: $!\n";
	my @out = <$h>;
	close $h;
	make_path(dirname($dest));
	open(my $o, '>', $dest) or die "$dest: $!\n";
	print $o @out, @lines, @$post;
	close $o or die "$dest: $!\n";
}
my $cfg = "$to/texmf-var/tex/generic/config";
config("texmf-dist/web2c/fmtutil-hdr.cnf", "$to/texmf-dist/web2c/fmtutil.cnf",
	'#', [], $sub->fmtutil_cnf_lines);
config("texmf-dist/web2c/updmap-hdr.cfg", "$to/texmf-dist/web2c/updmap.cfg",
	'#', [], $sub->updmap_cfg_lines);
config("texmf-dist/tex/generic/config/language.us", "$cfg/language.dat",
	'%', [], $sub->language_dat_lines);
config("texmf-dist/tex/generic/config/language.us.def", "$cfg/language.def",
	'%', ["%%% No changes may be made beyond this point.\n", "\n",
	      "\\uselanguage {USenglish}             %%% This MUST be the last line of the file.\n"],
	$sub->language_def_lines);
config("texmf-dist/tex/generic/config/language.us.lua", "$cfg/language.dat.lua",
	'--', ["}\n"], $sub->language_lua_lines);

make_path("$to/tlpkg");
open(my $db, '>', "$to/tlpkg/texlive.tlpdb") or die "texlive.tlpdb: $!\n";
$sub->writeout($db);
close $db or die "texlive.tlpdb: $!\n";
PERL

rm -f "$PKG/usr/share/texmf-dist/scripts/texlive/tlmgr.pl" \
	"$PKG/usr/share/texmf-dist/scripts/texlive/tlmgrgui.pl"

find "$PKG/usr/share/texmf-dist" -name '*.jar' -printf '%h\n' | sort -u > "$SRC_ROOT/jardirs"
for l in "$PKG"/usr/bin/*; do
	[ -L "$l" ] || continue
	t=$(readlink -f "$l")
	grep -qxF "${t%/*}" "$SRC_ROOT/jardirs" && rm -f "$l"
done
find "$PKG/usr/share/texmf-dist" -name '*.jar' -delete

patch -p1 -d "$PKG/usr/share" -i "$PORT_SRC/texmfcnf.patch"

export PATH="$PKG/usr/bin:$PATH"
export LD_LIBRARY_PATH="$PKG/usr/lib"
export TEXMFROOT="$PKG/usr/share"
export TEXMFCNF="$PKG/usr/share/texmf-dist/web2c"
export FORCE_SOURCE_DATE=1

texlinks -f "$PKG/usr/share/texmf-dist/web2c/fmtutil.cnf" "$PKG/usr/bin"
mktexlsr "$PKG/usr/share/texmf-dist" "$PKG/usr/share/texmf-var"
updmap-sys --nohash
fmtutil-sys --all
rm -f "$PKG"/usr/share/texmf-var/web2c/*.log "$PKG"/usr/share/texmf-var/web2c/*/*.log
mktexlsr "$PKG/usr/share/texmf-dist" "$PKG/usr/share/texmf-var"

install -d "$PKG/usr/share/fontconfig/conf.avail"
cat > "$PKG/usr/share/fontconfig/conf.avail/09-texlive-fonts.conf" <<'XML'
<?xml version="1.0"?>
<!DOCTYPE fontconfig SYSTEM "urn:fontconfig:fonts.dtd">
<fontconfig>
	<dir>/usr/share/texmf-dist/fonts/opentype</dir>
	<dir>/usr/share/texmf-dist/fonts/truetype</dir>
</fontconfig>
XML

find "$PKG/usr/bin" -xtype l -delete
