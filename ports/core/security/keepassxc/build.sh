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

patch -p1 -i "$PORT_SRC/app-id.patch"

# NETWORKING and UPDATECHECK are off: the first downloads site favicons and
# queries breach lists, the second polls for releases, and neither works on a
# machine with no network. Everything kept is local. FDOSECRETS makes KeePassXC
# the org.freedesktop.secrets provider that libsecret, QtKeychain and KWallet's
# API all reach. BROWSER is native messaging over a local socket to
# keepassxc-proxy. SSHAGENT hands keys to a running ssh-agent, KEESHARE syncs
# groups through files, and YUBIKEY is challenge-response over USB and PC/SC.
# AUTOTYPE types into X11 windows under Xwayland only: Wayland gives a client
# no way to inject keys into another. KEEPASSXC_BUILD_TYPE=Release drops the
# "development build" warning shown on every start. keepassxc-cli drops line
# editing without a word when Readline is not found, so it is required. The
# patch names the Wayland app_id, org.keepassxc.KeePassXC, in StartupWMClass.
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D KEEPASSXC_BUILD_TYPE=Release \
	-D WITH_TESTS=OFF \
	-D WITH_GUI_TESTS=OFF \
	-D WITH_XC_DOCS=ON \
	-D WITH_XC_UPDATECHECK=OFF \
	-D WITH_XC_NETWORKING=OFF \
	-D WITH_XC_FDOSECRETS=ON \
	-D WITH_XC_AUTOTYPE=ON \
	-D WITH_XC_X11=ON \
	-D WITH_XC_BROWSER=ON \
	-D WITH_XC_BROWSER_PASSKEYS=ON \
	-D WITH_XC_SSHAGENT=ON \
	-D WITH_XC_KEESHARE=ON \
	-D WITH_XC_YUBIKEY=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Readline=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

# Bundled data is English only: every other interface catalogue goes.
find "$PKG/usr/share/keepassxc/translations" -name 'keepassxc_*.qm' \
	! -name 'keepassxc_en.qm' ! -name 'keepassxc_en_US.qm' ! -name 'keepassxc_en_GB.qm' \
	-delete

# The Firefox-family browsers look for the proxy's manifest here, system-wide,
# so keepassxc-browser connects without KeePassXC writing into each profile.
install -d "$PKG/usr/lib/mozilla/native-messaging-hosts"
cat > "$PKG/usr/lib/mozilla/native-messaging-hosts/org.keepassxc.keepassxc_browser.json" <<'JSON'
{
    "name": "org.keepassxc.keepassxc_browser",
    "description": "KeePassXC integration with native messaging support",
    "path": "/usr/bin/keepassxc-proxy",
    "type": "stdio",
    "allowed_extensions": [
        "keepassxc-browser@keepassxc.org"
    ]
}
JSON
chmod 644 "$PKG/usr/lib/mozilla/native-messaging-hosts/org.keepassxc.keepassxc_browser.json"
