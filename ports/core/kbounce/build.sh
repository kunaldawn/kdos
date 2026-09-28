# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------


# KF_SKIP_PO_PROCESSING leaves the interface catalogues out: bundled data is
# English only. kdoctools_install() builds every translated handbook as well,
# so Help opens a local English copy and the others are removed after the
# install.
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D BUILD_TESTING=OFF \
	-D KF_SKIP_PO_PROCESSING=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

find "$PKG/usr/share/doc/HTML" -mindepth 1 -maxdepth 1 ! -name en -exec rm -rf {} +

# UPSTREAM'S ENTRY IS REPLACED for StartupWMClass: KAboutData sets the desktop
# file name, so the Wayland app_id is org.kde.kbounce. The hicolor PNGs it names
# are upstream's, installed above.
cat > "$PKG/usr/share/applications/org.kde.kbounce.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=KBounce
GenericName=Ball Bouncing Game
Comment=Build walls to fill the field without being hit by a ball
TryExec=kbounce
Exec=kbounce -qwindowtitle %c
Icon=kbounce
X-DocPath=kbounce/index.html
Categories=Qt;KDE;Game;ArcadeGame;
Keywords=bounce;balls;walls;jezzball;arcade;kbounce;
Terminal=false
StartupWMClass=org.kde.kbounce
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.kde.kbounce.desktop"
