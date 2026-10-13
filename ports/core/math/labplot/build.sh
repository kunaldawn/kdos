#!/bin/bash
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# musl-execinfo leaves out print_callstack, a debugging aid nothing calls,
# and the <execinfo.h> it needs, which musl does not have, on any C library
# but glibc.
#
# elapsed-timer-include gives the Fourier filter the <QElapsedTimer> it uses:
# no Qt header it includes brings it in.
patch -p1 -i "$PORT_SRC/musl-execinfo.patch"
patch -p1 -i "$PORT_SRC/elapsed-timer-include.patch"

# Every importer and feature with a library on this system is required
# rather than dropped when absent: FFTW, HDF5, NetCDF, FITS, Poppler's Qt 6
# frontend (LaTeX labels), Discount (Markdown notes), Eigen, the serial-port
# live data source, SVG export, Cantor worksheets, libcerf's fit models,
# Matio (MATLAB files), Orcus with Ixion (ODS files), the MQTT live data
# source, and the Origin, ROOT, MCAP and XLSX readers, whose code is bundled
# in src/3rdparty. The docking framework is the bundled copy
# (LOCAL_QT_ADVANCED_DOCK_COPY). ReadStat and the Vector BLF reader are off:
# each is a download at configure time. KUserFeedback is never looked for,
# so no usage statistics are collected. The SDK library and its headers are
# not built. KF_SKIP_PO_PROCESSING leaves the translation catalogues out:
# bundled data is English only.
#
# Cantor and NetCDF are checked after the configure instead of required: their
# find modules pass <Name>_FIND_REQUIRED to a config-package search as a bare
# argument, and a requirement sets it to 1, which that search reads as a
# version (netCDF 4 is then incompatible) or rejects outright (Cantor).
#
# QT_FIND_PRIVATE_MODULES has finding Qt6Gui load Qt6GuiPrivate as well: the
# docking framework and the XLSX reader are built against Qt's private API,
# and without the target the configure stops.
#
# Eigen's config file defines the Eigen3::Eigen target and none of the
# EIGEN3_FOUND, EIGEN3_INCLUDE_DIR and EIGEN3_VERSION_STRING variables LabPlot
# reads, so the three are given here from eigen3.pc; the requirement on Eigen3
# still stops the configure when the library is missing.
# The numeric library calls exp10, a GNU extension musl declares only under
# _GNU_SOURCE. LabPlot defines it for Clang alone, and the C99 KDE's settings
# impose defines no feature macro.
export CFLAGS="$CFLAGS -D_GNU_SOURCE"

cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D BUILD_TESTING=OFF \
	-D KF_SKIP_PO_PROCESSING=ON \
	-D ENABLE_TESTS=OFF \
	-D ENABLE_SDK=OFF \
	-D ENABLE_REPRODUCIBLE=ON \
	-D ENABLE_CANTOR=ON \
	-D ENABLE_FFTW=ON \
	-D ENABLE_HDF5=ON \
	-D ENABLE_NETCDF=ON \
	-D ENABLE_FITS=ON \
	-D ENABLE_LIBCERF=ON \
	-D ENABLE_LIBORIGIN=ON \
	-D ENABLE_ROOT=ON \
	-D ENABLE_READSTAT=OFF \
	-D ENABLE_MATIO=ON \
	-D ENABLE_MQTT=ON \
	-D ENABLE_QTSERIALPORT=ON \
	-D ENABLE_QTSVG=ON \
	-D ENABLE_DISCOUNT=ON \
	-D ENABLE_XLSX=ON \
	-D ENABLE_MCAP=ON \
	-D LOCAL_MCAP_DOWNLOAD=OFF \
	-D ENABLE_ORCUS=ON \
	-D ENABLE_VECTOR_BLF=OFF \
	-D ENABLE_EIGEN3=ON \
	-D LOCAL_QT_ADVANCED_DOCK_DOWNLOAD=OFF \
	-D LOCAL_QT_ADVANCED_DOCK_COPY=ON \
	-D QT_FIND_PRIVATE_MODULES=ON \
	-D EIGEN3_FOUND=ON \
	-D EIGEN3_INCLUDE_DIR="$(pkg-config --variable=prefix eigen3)/include/eigen3" \
	-D EIGEN3_VERSION_STRING="$(pkg-config --modversion eigen3)" \
	-D CMAKE_DISABLE_FIND_PACKAGE_KF6UserFeedback=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_FFTW3=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_HDF5=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_CFitsio=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Poppler=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Discount=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Eigen3=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Qt6SerialPort=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Qt6Svg=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KF6SyntaxHighlighting=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KF6Purpose=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_LIBCERF=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Matio=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Orcus=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Qt6Mqtt=ON \
	-Wno-dev
grep -q -- -DHAVE_CANTOR_LIBS build/build.ninja
grep -q -- -DHAVE_NETCDF build/build.ninja
cmake --build build
DESTDIR=$PKG cmake --install build
test -x "$PKG/usr/bin/labplot"

# kdoctools_install() builds every translated handbook and manual page; only
# the English ones are kept.
for d in "$PKG"/usr/share/doc/HTML/*/ "$PKG"/usr/share/man/*/; do
	case ${d%/} in
	*/HTML/en | */man/man[0-9]*) ;;
	*) rm -rf "$d" ;;
	esac
done

# UPSTREAM'S ENTRY IS REPLACED: the Wayland app_id is org.kde.labplot, the
# PNG icons are named labplot (the entry's org.kde.labplot is an SVG, which
# the panel never reads), and of the three types it claims only
# application/x-labplot is defined, by the labplot.xml the port installs.
cat > "$PKG/usr/share/applications/org.kde.labplot.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=LabPlot
GenericName=Data Plotting and Analysis
Comment=Plot, fit, filter and analyse scientific data
TryExec=labplot
Exec=labplot %f
Icon=labplot
Terminal=false
StartupWMClass=org.kde.labplot
X-DocPath=labplot/index.html
MimeType=application/x-labplot;
Categories=Qt;KDE;Education;Science;Physics;Math;
Keywords=plot;graph;chart;fit;statistics;data;analysis;spreadsheet;labplot;
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.kde.labplot.desktop"
test -e "$PKG"/usr/share/icons/hicolor/48x48/apps/labplot.png
