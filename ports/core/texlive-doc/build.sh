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


# THE DOCUMENTATION OF THE TEXLIVE PORT'S CUT. The same texmf tarball and
# texlive.tlpdb as texlive, and the same closure: TL_ROOTS must equal the
# texlive recipe's, or texdoc lists packages whose manuals are missing and
# this port ships manuals for packages that are not installed. A document
# file is kept where the tlpdb marks no language or English. The manual pages
# and info files under doc/ are texlive's, installed into /usr/share/man and
# /usr/share/info, and are skipped here, as is the Java bytecode a few
# packages' examples carry: nothing here compiled it.

TL_ROOTS="collection-basic collection-latex collection-latexrecommended
	collection-latexextra collection-fontsrecommended collection-fontutils
	collection-luatex collection-xetex collection-metapost
	collection-plaingeneric collection-mathscience collection-pictures
	collection-pstricks collection-bibtexextra collection-binextra
	collection-langenglish latexmk"

ITL_SRC="$SRC_ROOT/install-tl-$version"

perl -I"$ITL_SRC/tlpkg" - "$SRC_ROOT" "$SRC" "$PKG/usr/share" $TL_ROOTS <<'PERL'
use strict; use warnings;
use File::Basename qw(dirname);
use File::Copy qw(move);
use File::Path qw(make_path);
use TeXLive::TLPDB;

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

my $n = 0;
for my $p (sort keys %sel) {
	my $o = $sel{$p};
	my $data = $o->docfiledata || {};
	for my $f ($o->docfiles) {
		next unless $f =~ m{^texmf-dist/doc/} && $f !~ m{^texmf-dist/doc/(man|info)/};
		next if $f =~ /\.(jar|class)$/;
		my $lang = $data->{$f}{'language'};
		next if $lang && $lang !~ /(^|,)en\b/;
		-e "$from/$f" || -l "$from/$f" or die "$p names $f, which the texmf tarball does not hold\n";
		make_path(dirname("$to/$f"));
		move("$from/$f", "$to/$f") or die "move $f: $!\n";
		$n++;
	}
}
printf "texmf documentation: %d packages, %d files\n", scalar(keys %sel), $n;
PERL
