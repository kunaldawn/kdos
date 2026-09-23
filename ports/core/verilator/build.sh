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
#
# --with-solver names the SMT solver that SystemVerilog randomize() with
# constraints hands its problem to; left to configure, the default is whichever
# of z3, cvc5 or cvc4 happens to be on PATH in the chroot. OBJCACHE is the
# compiler wrapper written into verilated.mk that every user's model build runs
# through; configure otherwise takes ccache only when it finds it. jemalloc and
# tcmalloc are probed for and are not ports.
autoconf
./configure --prefix=/usr \
	--with-solver='z3 --in' \
	--disable-jemalloc \
	--disable-tcmalloc \
	OBJCACHE=ccache
make
make DESTDIR=$PKG install
