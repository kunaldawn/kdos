# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

ctg="$SRC_ROOT/ctypesgen"
SETUPTOOLS_SCM_PRETEND_VERSION=0 pip3 install --no-deps --no-index \
	--no-build-isolation --prefix="$ctg" "$SRC_ROOT/ctypesgen-$_ctypesgen"
export PATH="$ctg/bin:$PATH"
export PYTHONPATH="$(python3 -c 'import sysconfig, sys; print(sysconfig.get_path("purelib", vars={"base": sys.argv[1]}))' "$ctg")"

pdfium_ver=$(pkg-config --modversion libpdfium)
pdfium_build=${pdfium_ver#*.*.}
export GIVEN_FULLVER="$pdfium_ver"
export PDFIUM_PLATFORM="system-search:${pdfium_build%%.*}"
export PDFIUM_BINARY=/usr/lib/libpdfium.so
export PDFIUM_HEADERS=/usr/include/pdfium
pip3 install --ignore-installed --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .
