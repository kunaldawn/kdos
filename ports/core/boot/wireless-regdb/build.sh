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

[ -f regulatory.db.p7s ] || { echo "regulatory.db.p7s missing — an unsigned db is the same as no db"; exit 1; }

python3 db2fw.py regulatory.db.built db.txt

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
