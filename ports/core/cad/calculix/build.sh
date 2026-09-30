# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# The tarball is ./-prefixed (./CalculiX/ccx_<version>/src/...): the strip of
# one component removes the '.', and the tree lands under CalculiX/.
cd CalculiX
cd ccx_$version/src

# Upstream's link rule runs date.pl, which rewrites the sources with the build
# time, and links fixed ../../../ paths to static SPOOLES and ARPACK archives.
# The object and the solver archive are built through the Makefile, with the
# flags of its multithreaded variant (Makefile_MT), and linked here against
# the spooles, arpack-ng and openblas ports. The Fortran passes one array
# where another routine declares a scalar, which gfortran rejects without
# -fallow-argument-mismatch. The C side returns values from void functions
# and calls routines it never declares, which GCC rejects as errors unless
# -fpermissive turns them back into warnings. The Fortran flags start from
# CXXFLAGS, which carries no C-only -std.
_cflags="$CFLAGS -fopenmp -I/usr/include/spooles -DARCH=\"Linux\" -DSPOOLES -DARPACK -DMATRIXSTORAGE -DNETWORKOUT -DUSE_MT=1 -fpermissive"
_fflags="$CXXFLAGS -fopenmp -cpp -fallow-argument-mismatch"
make CC=gcc FC=gfortran CFLAGS="$_cflags" FFLAGS="$_fflags" ccx_$version.o ccx_$version.a
gfortran $LDFLAGS -fopenmp -o ccx ccx_$version.o ccx_$version.a \
	-lspooles -larpack -lopenblas -lpthread -lm

# FreeCAD's FEM workbench runs the solver as ccx from PATH.
install -Dm755 ccx "$PKG/usr/bin/ccx"
install -Dm644 gpl-2.0.txt "$PKG/usr/share/licenses/calculix/gpl-2.0.txt"
