# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# THE DATABASE IS THE POINT, not the program: /usr/share/units/definitions.units
# is 3000 entries with their provenance written beside them, and it reads as a
# reference document with no network anywhere near it.
#
# units_cur IS DELIBERATELY NOT INSTALLED. It is a python script whose whole
# job is fetching today's exchange rates from a web service, which on this
# distro is a command that cannot work and should not exist.
./configure --prefix=/usr --datarootdir=/usr/share
make
make DESTDIR=$PKG install
rm -f $PKG/usr/bin/units_cur $PKG/usr/share/man/man1/units_cur.1
