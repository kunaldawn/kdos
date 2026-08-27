# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# THE LINT IS WHY THIS IS HERE AS MUCH AS THE SIMULATOR. `verilator --lint-only`
# catches the width mismatches and inferred latches that iverilog will happily
# simulate and a real part will not do.
#
# help2man IS A DEPEND, not an optional nicety. The default target builds a man
# page per program by piping `$(PROG) --help` through it, and there is no
# configure switch to turn that off — verilator's configure has no --disable-docs
# and silently ignores one. Without help2man the build exits 127 at
# `verilator_gantt.1`, long after everything that matters has compiled.
autoconf
./configure --prefix=/usr
make
make DESTDIR=$PKG install
