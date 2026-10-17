# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D KDE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D BUILD_TESTING=OFF \
	-D KF_SKIP_PO_PROCESSING=ON \
	-D BUILD_DOC=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

cat > "$PKG/usr/share/applications/org.kde.kmahjongg.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=KMahjongg
GenericName=Mahjongg Solitaire
Comment=Match pairs of free tiles until the board is clear
TryExec=kmahjongg
Exec=kmahjongg -qwindowtitle %c
Icon=kmahjongg
X-DocPath=kmahjongg/index.html
Categories=Qt;KDE;Game;BoardGame;
Keywords=mahjong;mahjongg;tiles;solitaire;kmahjongg;
Terminal=false
StartupWMClass=org.kde.kmahjongg
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.kde.kmahjongg.desktop"
