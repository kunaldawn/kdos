# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# Pure Tcl: the package is its source files. /usr/lib is on Tcl's auto_path,
# so package require finds the pkgIndex.tcl there. English messages only.
_dir="$PKG/usr/lib/bwidget$version"
install -d "$_dir/images" "$_dir/lang"
install -m644 *.tcl "$_dir/"
install -m644 images/* "$_dir/images/"
install -m644 lang/en.rc "$_dir/lang/"
install -d "$PKG/usr/share/doc/bwidget"
install -m644 BWman/*.html "$PKG/usr/share/doc/bwidget/"
install -m644 LICENSE.txt "$PKG/usr/share/doc/bwidget/"
