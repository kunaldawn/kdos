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

make NOWERROR=TRUE
make NOWERROR=TRUE DESTDIR=$PKG install
rm -f "$PKG/usr/bin/acpiexamples"
install -d "$PKG/usr/share/licenses/$name"
sed -n '/^ \* 1\. Copyright Notice/,/\*\/$/p' source/include/acpi.h \
	> "$PKG/usr/share/licenses/$name/LICENSE"
grep -q 'GNU General Public License' "$PKG/usr/share/licenses/$name/LICENSE"
