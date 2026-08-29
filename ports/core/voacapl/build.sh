# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# THE COEFFICIENTS SHIP AND THE SOLAR FLUX DOES NOT, which is the exact shape
# of this program's offline limit. VOACAP's ionospheric coefficient set is a
# fixed table that installs with it; the smoothed sunspot number for today is a
# live measurement, so offline the prediction is only as good as the number
# somebody types in. It is still the difference between a guess and a model.
#
# gfortran comes from the gcc port — this tree builds the compiler with
# `--enable-languages=c,c++,fortran`, so there is no separate package.
# configure ships in the tag archive, so autoreconf is not needed.
./configure --prefix=/usr

# -j1, AND IT IS NOT CAUTION. gfortran writes a `.mod` file when it compiles a
# module and every user of that module must be compiled AFTER it; this
# Makefile.am declares no such dependency, so a parallel build reaches
# anttyp99.f90 before cant99.f90 has written cant99.mod and stops on a missing
# file that the next serial run produces. The whole build is seconds.
make -j1
make DESTDIR=$PKG install

# itshfbc is the runtime data directory VOACAP looks for; `makeitshfbc` is what
# creates it in a user's home, and it is a shell script the port installs.
