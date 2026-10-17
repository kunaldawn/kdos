# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

cd CalculiX
cd ccx_$version/src

_cflags="$CFLAGS -fopenmp -I/usr/include/spooles -DARCH=\"Linux\" -DSPOOLES -DARPACK -DMATRIXSTORAGE -DNETWORKOUT -DUSE_MT=1 -fpermissive"
_fflags="$CXXFLAGS -fopenmp -cpp -fallow-argument-mismatch"
make CC=gcc FC=gfortran CFLAGS="$_cflags" FFLAGS="$_fflags" ccx_$version.o ccx_$version.a
gfortran $LDFLAGS -fopenmp -o ccx ccx_$version.o ccx_$version.a \
	-lspooles -larpack -lopenblas -lpthread -lm

install -Dm755 ccx "$PKG/usr/bin/ccx"
install -Dm644 gpl-2.0.txt "$PKG/usr/share/licenses/calculix/gpl-2.0.txt"
