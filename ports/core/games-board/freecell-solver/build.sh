# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

(
	cd "$SRC_ROOT/Template-Toolkit-$_tt"
	perl Makefile.PL TT_XS_ENABLE=n TT_XS_DEFAULT=n TT_ACCEPT=y TT_QUIET=y
	make
)
export PERL5LIB="$SRC_ROOT/Moo-$_moo/lib:$SRC_ROOT/Role-Tiny-$_roletiny/lib:$SRC_ROOT/Class-Method-Modifiers-$_cmm/lib:$SRC_ROOT/Sub-Quote-$_subquote/lib:$SRC_ROOT/Path-Tiny-$_pathtiny/lib:$SRC_ROOT/Template-Toolkit-$_tt/blib/lib"
export PYTHONPATH="$SRC_ROOT/pysol_cards-$_pysolcards"

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

rm -f "$PKG/usr/bin/fc_solve_find_index_s2ints.py" \
	"$PKG/usr/bin/find-freecell-deal-index.py" \
	"$PKG/usr/bin/gen-multiple-pysol-layouts" \
	"$PKG/usr/bin/make_pysol_freecell_board.py" \
	"$PKG/usr/share/man/man6/gen-multiple-pysol-layouts.6" \
	"$PKG/usr/share/man/man6/make_pysol_freecell_board.py.6"
