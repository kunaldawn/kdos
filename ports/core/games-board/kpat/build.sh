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
	-D WITH_BH_SOLVER=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

find "$PKG/usr/share/man" -mindepth 1 -maxdepth 1 ! -name 'man[0-9]*' -exec rm -rf {} +

cat > "$PKG/usr/share/applications/org.kde.kpat.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=KPatience
GenericName=Patience Card Game
Comment=Solitaire card game
TryExec=kpat
Exec=kpat -qwindowtitle %c %u
Icon=kpat
X-DocPath=kpat/index.html
MimeType=application/vnd.kde.kpatience.savedstate;
Categories=Qt;KDE;Game;CardGame;
Keywords=solitaire;patience;cards;klondike;spider;freecell;yukon;golf;kpat;
Terminal=false
StartupWMClass=org.kde.kpat
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.kde.kpat.desktop"
