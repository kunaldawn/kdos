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

# IT IS THE HALF taskwarrior DOES NOT DO. taskwarrior records what should
# happen and whether it did; this records the interval it took, and the two
# integrate through a hook rather than being one program. Its storage is
# `~/.local/share/timewarrior/data/*.data` — one line per interval, plain
# text, the calcurse and ledger property again.
mkdir -p build && cd build
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DASCIIDOCTOR_EXECUTABLE=/usr/bin/asciidoctor
ninja
DESTDIR=$PKG ninja install

# THE MANUAL PAGES ARE RENDERED FROM THEIR .adoc SOURCES by asciidoctor.
# Naming the executable makes it required: CMake's own search, finding
# nothing, installs the tarball's prebuilt pages instead, so which pages ship
# would depend on build order. The rendering branch installs only what it
# renders, so the three `.so` redirects with no .adoc source — day, week and
# month, each pointing at timew-chart(1) — are installed from the tarball.
install -Dm644 ../doc/man1/timew-day.1 ../doc/man1/timew-week.1 ../doc/man1/timew-month.1 \
	-t "$PKG/usr/share/man/man1"
