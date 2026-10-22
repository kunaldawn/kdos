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

export PYTHONDONTWRITEBYTECODE=1
root=$PORT_SRC/../../..
for d in ports/core script/phases docs/kfs; do
	[ -d "$root/$d" ] || { echo "kdos-kfs: $root/$d is missing: the book is rendered from a KDOS checkout" >&2; exit 1; }
done

python3 "$PORT_SRC/book/kfsgen.py" --root "$root" --out "$SRC/kfs-src"
python3 "$PORT_SRC/book/kfsrender.py" --pandoc pandoc --typst typst \
	--src "$SRC/kfs-src" --out "$SRC/kfs" --buildfs /

install -d "$PKG/usr/share/doc"
cp -R "$SRC/kfs/html" "$PKG/usr/share/doc/kfs"
install -m644 "$SRC/kfs/kfs.pdf" "$PKG/usr/share/doc/kfs/kfs.pdf"
