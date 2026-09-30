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

# BUILT FROM db.txt, AND ACCEPTED ONLY IF UPSTREAM'S SIGNATURE COVERS IT.
#
# kdos.config sets CONFIG_CFG80211_REQUIRE_SIGNED_REGDB=y with the kernel's own
# regdb keys, so the kernel loads regulatory.db only when regulatory.db.p7s
# verifies against a certificate compiled into it. A database this build signs
# itself — which is what the tarball's Makefile does — is rejected in silence
# and the radio falls back to the world domain: no 5 GHz DFS, reduced TX power,
# no diagnostic anywhere.
#
# So the database is generated here with upstream's db2fw.py (standard library
# only), and upstream's detached signature is then verified against the
# generated file with the certificates the kernel trusts. A mismatch fails the
# build: the output is either byte-identical to what upstream signed or it does
# not ship. The Makefile is never run — its default target signs with a local
# key.
[ -f regulatory.db.p7s ] || { echo "regulatory.db.p7s missing — an unsigned db is the same as no db"; exit 1; }

python3 db2fw.py regulatory.db.built db.txt

# The kernel's net/wireless/certs holds sforshee's and wens's certificates;
# the tarball carries whichever of them currently signs.
ca=()
for cert in sforshee.x509.pem wens.x509.pem; do
	[ -f "$cert" ] && ca+=("$cert")
done
[ ${#ca[@]} -gt 0 ] || { echo "no upstream signing certificate in the tarball"; exit 1; }
cat "${ca[@]}" > regdb-ca.pem

openssl smime -verify -binary -inform DER \
	-in regulatory.db.p7s -content regulatory.db.built \
	-CAfile regdb-ca.pem -purpose any -out /dev/null ||
	{ echo "regulatory.db built from db.txt does not match upstream's signature"; exit 1; }

install -dm755 "$PKG/lib/firmware"
install -m644 regulatory.db.built "$PKG/lib/firmware/regulatory.db"
install -m644 regulatory.db.p7s   "$PKG/lib/firmware/regulatory.db.p7s"

install -Dm644 regulatory.bin.5 -t "$PKG/usr/share/man/man5"
ln -s regulatory.bin.5 "$PKG/usr/share/man/man5/regulatory.db.5"

install -dm755 "$PKG/usr/share/licenses/$name"
install -m644 LICENSE "$PKG/usr/share/licenses/$name/"
