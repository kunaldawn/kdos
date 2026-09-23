# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# THE SOLVER BEHIND A FORMAL PROOF, not a simulation. SymbiYosys turns a
# SystemVerilog assertion into an SMT problem and this is what answers it —
# which is the difference between "the testbench passed" and "no input can make
# this fail".
autoconf
./configure --prefix=/usr
make
make DESTDIR=$PKG install
install -Dm644 doc/yices.1 doc/yices-sat.1 doc/yices-smt.1 doc/yices-smt2.1 -t "$PKG/usr/share/man/man1"
