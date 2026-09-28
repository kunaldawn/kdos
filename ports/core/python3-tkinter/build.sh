# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# python3 is built before Tk exists and ships without tkinter. This builds
# the one extension module from the same source, against the installed Tcl
# and Tk, and packages it with the pure-Python package beside it. The version
# is python3's: the module is compiled for that interpreter's ABI.
./configure \
	--prefix=/usr \
	--enable-shared \
	--with-system-expat \
	--without-system-libmpdec \
	--with-ensurepip=no \
	--disable-test-modules

_ext=$(python3 -c 'import sysconfig; print(sysconfig.get_config_var("EXT_SUFFIX"))')
make "Modules/_tkinter$_ext"

_lib=/usr/lib/python$_version
install -Dm755 "Modules/_tkinter$_ext" "$PKG$_lib/lib-dynload/_tkinter$_ext"
install -d "$PKG$_lib"
cp -r Lib/tkinter "$PKG$_lib/tkinter"
rm -rf "$PKG$_lib/tkinter/__pycache__"
python3 -m compileall -q -o 0 -o 1 -o 2 -d "$_lib/tkinter" "$PKG$_lib/tkinter"
