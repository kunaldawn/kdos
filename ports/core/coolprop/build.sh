# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# THE MOST-USED LOOKUP IN MECHANICAL, PROCESS AND HVAC WORK, and it is a
# LOOKUP rather than a download: the equation-of-state coefficients for 122
# fluids, IAPWS-95 water and steam and the humid-air model are compiled into
# the library. A saturation temperature or an enthalpy is available with
# nothing else on the machine.
#
# 6.8.0 AND NOT 8.x, AND THE REASON IS THE SDIST RATHER THAN THE CODE. CoolProp
# 8 replaced its bundled `externals/` with CPM, which downloads TEN upstreams —
# Eigen, msgpack-c, nlohmann_json, valijson, IF97, REFPROP_headers,
# boost_headers, multicomplex, fmt and Catch2 — most pinned to bare git commits
# with no release tarball. Its sdist is 4 MB and cannot build without a
# network; 6.8.0's is 26 MB because every one of those trees is IN it, which is
# exactly what an offline build needs.
#
# -DCOOLPROP_NO_INCBIN, AND THE FILE IT AVOIDS IS NOT IN THE SDIST. The fluid
# database is embedded with the incbin library, whose `.incbin` assembler
# directive reads `all_fluids.json.z` at ASSEMBLY time — a file the release
# build generates and the sdist does not carry, so g++ fails with
# `{standard input}:10: Error: file not found`. The sdist ships the same bytes
# as a C array in include/all_fluids_JSON_z.h instead, and this macro is what
# selects it. Same data, same fluids; only the mechanism differs.
export CFLAGS="$CFLAGS -DCOOLPROP_NO_INCBIN"
export CXXFLAGS="$CXXFLAGS -DCOOLPROP_NO_INCBIN"

# --no-build-isolation because setuptools, Cython and cmake are installed
# ports; --no-deps because numpy is one too.
pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .
