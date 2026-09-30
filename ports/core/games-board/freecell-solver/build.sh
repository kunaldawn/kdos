# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------


# THE CONFIGURE RUNS PERL AND PYTHON GENERATORS, and each refuses to continue
# without its modules. Perl's are Moo (with Role::Tiny,
# Class::Method::Modifiers and Sub::Quote), Path::Tiny and the Template
# Toolkit; Python's is pysol_cards, which the configure imports whenever a
# python3 is on the path, and six, from the python3-six port. All are build
# tools, taken from their tarballs unpacked beside the source and never
# installed. The Template Toolkit is configured without its XS stash, so its
# pure-Perl modules run from the build directory with nothing compiled.
(
	cd "$SRC_ROOT/Template-Toolkit-$_tt"
	perl Makefile.PL TT_XS_ENABLE=n TT_XS_DEFAULT=n TT_ACCEPT=y TT_QUIET=y
	make
)
export PERL5LIB="$SRC_ROOT/Moo-$_moo/lib:$SRC_ROOT/Role-Tiny-$_roletiny/lib:$SRC_ROOT/Class-Method-Modifiers-$_cmm/lib:$SRC_ROOT/Sub-Quote-$_subquote/lib:$SRC_ROOT/Path-Tiny-$_pathtiny/lib:$SRC_ROOT/Template-Toolkit-$_tt/blib/lib"
export PYTHONPATH="$SRC_ROOT/pysol_cards-$_pysolcards"

# KPatience links the shared library for its Freecell and Simple Simon hints.
# The DBM solvers are built too, which is what gmp is for. Asciidoctor is kept
# out: the release carries the HTML documents it would regenerate into the
# source tree. The test suite needs Perl and Python modules this tree does not
# carry.
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D FCS_WITH_TEST_SUITE=OFF \
	-D BUILD_STATIC_LIBRARY=OFF \
	-D FCS_ENABLE_DBM_SOLVER=ON \
	-D FCS_AVOID_TCMALLOC=ON \
	-D DISABLE_APPLYING_RPATH=ON \
	-D CMAKE_DISABLE_FIND_PACKAGE_Asciidoc=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

# The Python deal generators import pysol_cards at run time, which is a build
# tool here and not installed, so each would stop on an ImportError. They and
# their manual pages are removed; pi-make-microsoft-freecell-board, compiled
# from C, generates the same deals.
rm -f "$PKG/usr/bin/fc_solve_find_index_s2ints.py" \
	"$PKG/usr/bin/find-freecell-deal-index.py" \
	"$PKG/usr/bin/gen-multiple-pysol-layouts" \
	"$PKG/usr/bin/make_pysol_freecell_board.py" \
	"$PKG/usr/share/man/man6/gen-multiple-pysol-layouts.6" \
	"$PKG/usr/share/man/man6/make_pysol_freecell_board.py.6"
