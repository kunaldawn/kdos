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

export CXXFLAGS="$CXXFLAGS -I$SRC/lib/utfcpp/v4/source"

mkdir -p build && cd build
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DCMAKE_SKIP_INSTALL_RPATH=ON \
	-DBUILD_DOCS=ON \
	-DBUILD_WEB_DOCS=OFF \
	-DUSE_PYTHON=OFF \
	-DUSE_GPGME=ON \
	-DBUILD_LIBRARY=ON \
	-DBOOST_REGEX_UNICODE_RUNS=1
grep -q '^#define HAVE_EDIT 1' system.hh \
	|| { echo "ledger: libedit missing" >&2; exit 1; }
ninja
ninja doc
DESTDIR=$PKG ninja install
install -Dm644 "$PKG/usr/share/contrib/ledger-completion.bash" \
	"$PKG/usr/share/bash-completion/completions/ledger"
rm -f "$PKG/usr/share/contrib/ledger-completion.bash"
rm -rf "$PKG/usr/share/contrib/non-profit-audit-reports/tests"
install -d "$PKG/usr/share/ledger"
mv "$PKG/usr/share/contrib" "$PKG/usr/share/ledger/contrib"

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/ledger.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Accounting
GenericName=Accounting
Comment=Double-entry accounting from a plain-text ledger
Exec=ledger
Icon=office-chart-line
Terminal=true
Categories=Office;Finance;
Keywords=account;ledger;finance;money;
EOF
chmod 644 "$PKG/usr/share/applications/ledger.desktop"
