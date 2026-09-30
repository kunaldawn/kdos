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
# yosys turns the design into an SMT problem, and yices2 or z3 answers it. All
# of them are ports, so a proof runs on the machine with no container and no
# network. An `smtbmc z3` engine runs the z3 binary, and sby imports click on
# every invocation.

make DESTDIR=$PKG PREFIX=/usr install
