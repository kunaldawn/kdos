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
# USE_PYTHON=OFF drops the boost::python binding; boost is still needed for
# regex, filesystem and date_time, which the parser itself uses.
#
# THE LINE EDITOR IS PROBED, NOT AN OPTION, so the generated system.hh is
# checked: CMakeLists takes libedit when it finds it and readline otherwise,
# and the REPL a bare `ledger` opens would follow build order.
#
# BOOST_REGEX_UNICODE_RUNS IS SET, NOT PROBED. The probe that decides whether
# ledger uses Boost.Regex's ICU side assigns a narrow string literal to a
# std::basic_string<uint32_t>, which GCC 15 refuses to compile, so it reports
# no and case-folded matching of non-ASCII account and payee names falls back
# to bytes. Boost here is built with ICU; with the result set, `bal активы`
# matches `Активы:Банк`, and a missing ICU fails the link instead.
#
# USE_GPGME reads a journal that gpg encrypted, through gpgmepp.
#
# BUILD_DOCS IS THE INFO MANUAL, and `ninja doc` has to run before the install:
# the target is not in ALL, and the install rule names the .info file whether
# or not it was built. BUILD_WEB_DOCS stays off.

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
