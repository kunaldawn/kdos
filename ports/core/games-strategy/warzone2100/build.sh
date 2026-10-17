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

patch -p1 -i "$PORT_SRC/no-update-check.patch"

patch -p1 -i "$PORT_SRC/vulkan-colorspace.patch"

export LDFLAGS="$LDFLAGS -Wl,--strip-debug"
cmake -S . -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DWZ_DISTRIBUTOR=KDOS \
	-DWZ_ENABLE_WARNINGS=OFF \
	-DWZ_ENABLE_WARNINGS_AS_ERRORS=OFF \
	-DWZ_ENABLE_BASIS_UNIVERSAL=ON \
	-DWZ_INCLUDE_TERRAIN_HIGH=ON \
	-DWZ_DOWNLOAD_PREBUILT_PACKAGES=OFF \
	-DWZ_DISABLE_FETCHCONTENT_GIT_CLONE=ON \
	-DWZ_ENABLE_BACKEND_VULKAN=ON \
	-DWZ_USE_SYSTEM_LIBJPEG_TURBO=ON \
	-DWZ_FORCE_MINIMAL_OPUSFILE=ON \
	-DWZ_DEBUG_GFX_API_LEAKS=OFF \
	-DENABLE_GNS_NETWORK_BACKEND=ON \
	-DENABLE_DISCORD=OFF \
	-DWZ_BUILD_SENTRY=OFF \
	-DWZ_SKIP_ELF_SEPARATE_DEBUG=ON \
	-DENABLE_NLS=OFF \
	-DENABLE_DOCS=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_VulkanHeaders=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_Fribidi=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_Miniupnpc=ON
cmake --build build
DESTDIR=$PKG cmake --install build
rm -rf "$PKG/usr/share/doc/warzone2100/hosting/linux_scripts"

rm -f "$PKG/usr/share/icons/net.wz2100.warzone2100.png"
install -Dm644 icons/warzone2100.large.png \
	"$PKG/usr/share/icons/hicolor/256x256/apps/net.wz2100.warzone2100.png"
cat > "$PKG/usr/share/applications/net.wz2100.warzone2100.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Warzone 2100
GenericName=Strategy Game
Comment=Post-apocalyptic real-time strategy
Exec=warzone2100
Icon=net.wz2100.warzone2100
Terminal=false
StartupWMClass=warzone2100
Categories=Game;StrategyGame;
Keywords=warzone2100;wz2100;strategy;rts;real-time;
DESKTOP
chmod 644 "$PKG/usr/share/applications/net.wz2100.warzone2100.desktop"
