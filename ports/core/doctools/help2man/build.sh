# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# A BUILD DEPENDENCY THAT LOOKS OPTIONAL AND IS NOT. A Makefile rule that pipes
# `$(PROG) --help` into help2man does not degrade when help2man is absent — it
# exits 127 and takes the build with it, usually after everything that matters
# has already compiled. verilator is the worked example.
#
# --disable-nls: the message catalogues need a `po4a` chain and translate the
# help2man program's own output, not the man pages it writes.
./configure --prefix=/usr --disable-nls
make
make DESTDIR=$PKG install
