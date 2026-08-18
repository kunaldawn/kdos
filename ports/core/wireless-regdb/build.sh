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

# Install the prebuilt database, never regenerate it.
#
# kdos.config sets CONFIG_CFG80211_REQUIRE_SIGNED_REGDB=y, so the kernel loads
# regulatory.db only when regulatory.db.p7s verifies against a key compiled
# into it. Upstream ships both, signed with that key. The tarball's Makefile
# will rebuild the database from db.txt, and the result is signed with nothing:
# the kernel rejects it silently and falls back to the world domain, leaving a
# radio that works with no 5 GHz DFS and reduced TX power and no diagnostic
# anywhere.
#
# So: no `make`. Copy the two files upstream already signed.
[ -f regulatory.db ]     || { echo "regulatory.db missing from the tarball"; exit 1; }
[ -f regulatory.db.p7s ] || { echo "regulatory.db.p7s missing — an unsigned db is the same as no db"; exit 1; }

install -dm755 "$PKG/lib/firmware"
install -m644 regulatory.db     "$PKG/lib/firmware/regulatory.db"
install -m644 regulatory.db.p7s "$PKG/lib/firmware/regulatory.db.p7s"

install -dm755 "$PKG/usr/share/licenses/$name"
install -m644 LICENSE "$PKG/usr/share/licenses/$name/"
