# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# THE BINDINGS ARE GENERATED FROM THE INSTALLED HEADERS, NOT SHIPPED. The sdist
# carries reference bindings written from the headers of pdfium-binaries'
# build — its own pdfium release, with V8, XFA and Skia defined — and setup
# falls back to them whenever no `ctypesgen` is on PATH. Those import cleanly
# against this library and then describe structs and enums of another
# release; pypdfium2 itself calls that ABI-unsafe. Generated here, every
# declaration comes from /usr/include/pdfium and the library is loaded by its
# path.
#
# ctypesgen is pypdfium2's own fork (setup asserts PYPDFIUM2_SPECIFIC), pinned
# by the commit its pyproject.toml names. It is needed only for this, so it
# goes into a private prefix under $SRC_ROOT and never into the package.
ctg="$SRC_ROOT/ctypesgen"
SETUPTOOLS_SCM_PRETEND_VERSION=0 pip3 install --no-deps --no-index \
	--no-build-isolation --prefix="$ctg" "$SRC_ROOT/ctypesgen-$_ctypesgen"
export PATH="$ctg/bin:$PATH"
export PYTHONPATH="$(python3 -c 'import sysconfig, sys; print(sysconfig.get_path("purelib", vars={"base": sys.argv[1]}))' "$ctg")"

# system-search takes the library and the headers from what the pdfium port
# installed. The version is handed over explicitly: pypdfium2's own
# pkg-config probe reads a stream it never captured and dies on it, and
# without GIVEN_FULLVER it resolves the build number by listing Chromium's
# tags over the network. libpdfium.pc carries the full version.
pdfium_ver=$(pkg-config --modversion libpdfium)
pdfium_build=${pdfium_ver#*.*.}
export GIVEN_FULLVER="$pdfium_ver"
export PDFIUM_PLATFORM="system-search:${pdfium_build%%.*}"
export PDFIUM_BINARY=/usr/lib/libpdfium.so
export PDFIUM_HEADERS=/usr/include/pdfium
pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .
