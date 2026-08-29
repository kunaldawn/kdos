# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# THE HIGHEST-RISK PORT IN THE CATALOGUE, and what makes it so is that it is a
# Fortran build as well as a C one: scipy carries MINPACK, QUADPACK, ODEPACK
# and ARPACK as Fortran and links them against OpenBLAS. gfortran comes from
# the gcc port — this tree builds `--enable-languages=c,c++,fortran` — and
# openblas is what LAPACK resolves to; without it meson finds the reference
# implementation or nothing at all.
#
# scipy.constants IS THE CODATA TABLE, which is half the reason this is a core
# row: on a machine with no network it is the only authoritative source of a
# physical constant.
#
# NO WHEELS EXIST FOR THIS TARGET — python3 here is 3.14 on musl and PyPI's
# manylinux wheels are glibc — so this is a from-source build through
# meson-python.
#
# NO VENDOR BUNDLE and --no-build-isolation: meson-python, meson, ninja and
# Cython are all PORTS, so the isolated environment pip would otherwise build
# has nothing to add and everything to fetch. Vendoring instead means pip
# resolving the backend's own chain, which ends at the PyPI `ninja` wrapper
# whose sdist compiles CMake from source.
# -Duse-pythran=false, AND IT IS A DELIBERATE TRADE. pythran compiles a
# handful of scipy's pure-python inner loops ahead of time; it is another
# build-time python compiler with its own chain, and meson's find_program for
# it is unconditional when the option is on, so the build stops at
# `Program 'pythran' not found` before anything is compiled. What it costs is
# speed in parts of scipy.signal and scipy.optimize, not correctness or an API.
#
# blas and lapack are NAMED rather than left to their default: both options are
# strings that scipy resolves through pkg-config, and openblas is the port this
# tree builds. The reference kernels would be an order of magnitude slower and
# nothing would say so.
pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr \
	--config-settings=setup-args=-Duse-pythran=false \
	--config-settings=setup-args=-Dblas=openblas \
	--config-settings=setup-args=-Dlapack=openblas \
	.
