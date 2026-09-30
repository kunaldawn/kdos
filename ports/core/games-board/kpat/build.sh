# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------


# The Freecell and Simple Simon hints come from freecell-solver and Golf's
# from black-hole-solver; both are required, so a missing library fails the
# configure instead of leaving those games without a solver. The card decks
# are libkdegames', under /usr/share/carddecks.

# KF_SKIP_PO_PROCESSING leaves the interface catalogues out: bundled data is
# English only. kdoctools_install() builds every translated handbook and
# translated kpat(6) page as well, so Help and man open the English copies and
# the others are removed after the install.
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
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

find "$PKG/usr/share/doc/HTML" -mindepth 1 -maxdepth 1 ! -name en -exec rm -rf {} +
find "$PKG/usr/share/man" -mindepth 1 -maxdepth 1 ! -name 'man[0-9]*' -exec rm -rf {} +

# UPSTREAM'S ENTRY IS REPLACED for StartupWMClass: KAboutData sets the desktop
# file name, so the Wayland app_id is org.kde.kpat. The hicolor PNGs it names
# are upstream's, installed above.
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
