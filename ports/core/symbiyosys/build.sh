# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# "THE TESTBENCH PASSED" IS NOT "NO INPUT CAN MAKE THIS FAIL", and sby is what
# turns the first into the second: it hands a SystemVerilog assertion to yosys,
# yosys turns the design into an SMT problem, and yices2 answers it. All three
# are ports, so a proof runs on the machine with no container and no network.
#
# 0.47 IS THE LAST TAG sby PUBLISHES. The repository stopped tagging after
# yosys-0.47 while ports/core/yosys is 0.68; the interface sby drives yosys
# through is the `write_smt2` backend, which has not moved.
#
# PYTHON IS PASSED EXPLICITLY: the Makefile otherwise writes the path of
# whatever `python3` was on the builder's PATH into the launcher, which on this
# machine would be the chroot's and is right by accident rather than by
# statement.
make PREFIX=/usr PYTHON=/usr/bin/python3
make DESTDIR=$PKG PREFIX=/usr PYTHON=/usr/bin/python3 install
