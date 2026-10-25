# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

./configure \
	--prefix=/usr \
	--enable-shared \
	--with-system-expat \
	--with-system-libmpdec \
	--with-ensurepip=no \
	--disable-test-modules \
	OPT="-O3 -Wall"

_ext=$(python3 -c 'import sysconfig; print(sysconfig.get_config_var("EXT_SUFFIX"))')
make "Modules/_tkinter$_ext" "build/scripts-$_version/idle$_version"

_lib=/usr/lib/python$_version
install -Dm755 "Modules/_tkinter$_ext" "$PKG$_lib/lib-dynload/_tkinter$_ext"
install -d "$PKG$_lib"
cp -r Lib/tkinter Lib/turtledemo Lib/idlelib Lib/turtle.py "$PKG$_lib/"
rm -rf "$PKG$_lib/idlelib/idle_test" "$PKG$_lib"/{tkinter,turtledemo,idlelib}/__pycache__
python3 -m compileall -q -o 0 -o 1 -o 2 -s "$PKG" -p / \
	"$PKG$_lib"/{tkinter,turtledemo,idlelib,turtle.py}

install -Dm755 "build/scripts-$_version/idle$_version" "$PKG/usr/bin/idle$_version"
ln -s "idle$_version" "$PKG/usr/bin/idle3"
