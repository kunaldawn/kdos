# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

_dir="$PKG/usr/lib/bwidget$version"
install -d "$_dir/images" "$_dir/lang"
install -m644 *.tcl "$_dir/"
install -m644 images/* "$_dir/images/"
install -m644 lang/en.rc "$_dir/lang/"
install -d "$PKG/usr/share/doc/bwidget"
install -m644 BWman/*.html "$PKG/usr/share/doc/bwidget/"
install -m644 LICENSE.txt "$PKG/usr/share/doc/bwidget/"
