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

# THE LEDGER IS A TEXT FILE AND THE PROGRAM IS A REPORT GENERATOR. Nothing is
# stored anywhere else: transactions are lines somebody typed, in a file under
# version control, and `ledger` only ever reads them. That is why it belongs on
# a machine meant to outlive its software — the data survives the tool, which
# is not true of any accounting application with a database.
#
# --disable-python drops the boost::python binding; boost is still needed for
# regex, filesystem and date_time, which the parser itself uses.
# THE UNIT TESTS DO NOT INHERIT THE VENDORED utfcpp INCLUDE. ledger carries
# utfcpp in lib/utfcpp/v4/source and declares it PRIVATE on libledger, while
# test/unit only adds src/ and links the library — so every test translation
# unit fails on `'utf8' has not been declared` after the library itself has
# compiled cleanly. Adding the directory to CXXFLAGS reaches every target and
# costs the library nothing.
export CXXFLAGS="$CXXFLAGS -I$SRC/lib/utfcpp/v4/source"

mkdir -p build && cd build
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_DOCS=OFF \
	-DBUILD_WEB_DOCS=OFF \
	-DUSE_PYTHON=OFF \
	-DBUILD_LIBRARY=ON
ninja
DESTDIR=$PKG ninja install

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
