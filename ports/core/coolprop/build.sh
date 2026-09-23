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
# EVERY CPMAddPackage IS GIVEN A LOCAL TREE, or configure downloads it. Eigen,
# fmt and nlohmann-json are ports and CPM is pointed at their installed
# headers; the upstreams pinned to bare commits, and valijson, are extra
# sources in the recipe and unpack beside $SRC. A package left out here is a
# network fetch at configure time, which fails in the build chroot.
pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr . \
	--config-settings=cmake.define.CPM_Eigen_SOURCE=/usr/include/eigen3 \
	--config-settings=cmake.define.CPM_fmt_SOURCE=/usr \
	--config-settings=cmake.define.CPM_nlohmann_json_SOURCE=/usr \
	--config-settings=cmake.define.CPM_msgpack-c_SOURCE=$SRC_ROOT/msgpack-c-$_msgpack \
	--config-settings=cmake.define.CPM_IF97_SOURCE=$SRC_ROOT/IF97-$_if97 \
	--config-settings=cmake.define.CPM_REFPROP_headers_SOURCE=$SRC_ROOT/REFPROP-headers-$_refprop \
	--config-settings=cmake.define.CPM_boost_headers_SOURCE=$SRC_ROOT/boost-headers-$_boost \
	--config-settings=cmake.define.CPM_multicomplex_SOURCE=$SRC_ROOT/multicomplex-$_multicomplex \
	--config-settings=cmake.define.CPM_valijson_SOURCE=$SRC_ROOT/valijson-$_valijson
