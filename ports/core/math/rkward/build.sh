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

# RKWard runs R in a backend process that dlopen()s libR.so (DLOPEN_RLIB), so
# the configure runs the installed R to find R_HOME, its include directory and
# its library. The output, help and preview windows are QtWebEngine pages;
# FORCE_WITH_QWEBENGINE makes a missing QtWebEngine a configure error rather
# than a build with no HTML renderer. KDSingleApplication and KCrash are
# optional upstream and required here, so the system library is used and not
# the bundled copy. The rkward and rkwardtests R packages are built into
# tarballs and installed into the user's R library on first start.
# KF_SKIP_PO_PROCESSING leaves the translation catalogues out: bundled data is
# English only.
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D BUILD_TESTING=OFF \
	-D KF_SKIP_PO_PROCESSING=ON \
	-D FORCE_WITH_QWEBENGINE=ON \
	-D FORCE_WITH_QWEBVIEW=OFF \
	-D DLOPEN_RLIB=ON \
	-D R_EXECUTABLE=/usr/bin/R \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KDSingleApplication-qt6=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KF6Crash=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build
test -x "$PKG/usr/bin/rkward"
test -e "$PKG"/usr/share/rkward/rpackages/rkward.tgz

# kdoctools_install() builds every translated handbook and manual page; only
# the English ones are kept.
for d in "$PKG"/usr/share/doc/HTML/*/ "$PKG"/usr/share/man/*/; do
	case ${d%/} in
	*/HTML/en | */man/man[0-9]*) ;;
	*) rm -rf "$d" ;;
	esac
done

# UPSTREAM'S ENTRY IS REPLACED for StartupWMClass: KAboutData makes the
# Wayland app_id org.kde.rkward. Every MIME type it claims is defined by the
# XML files the port installs.
cat > "$PKG/usr/share/applications/org.kde.rkward.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=RKWard
GenericName=Statistics with R
Comment=Data analysis, plots and reports with the R language
TryExec=rkward
Exec=rkward --autoreuse %F
Icon=rkward
Terminal=false
StartupWMClass=org.kde.rkward
X-DocPath=rkward/index.html
Categories=Qt;KDE;Science;Math;NumericalAnalysis;
MimeType=text/r;application/rdata;text/vnd.kde.rmarkdown;application/vnd.kde.rkward-output;
Keywords=statistics;data;r;regression;analysis;plot;rkward;
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.kde.rkward.desktop"
test -e "$PKG"/usr/share/icons/hicolor/48x48/apps/rkward.png
