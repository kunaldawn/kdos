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

pip3 install --ignore-installed --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .

site=$(echo "$PKG"/usr/lib/python3*/site-packages)
rm -rf "$site/kolibri/dist/cext"
find "$site/kolibri" -name '*.so' -delete
if find "$site/kolibri" -name '*.so*' | grep -q .; then
	echo 'a prebuilt shared object is left in the package' >&2
	exit 1
fi

cat > "$PKG/usr/bin/kolibri" <<'KDOS_SH'
#!/bin/sh
export KOLIBRI_DISABLE_PING="${KOLIBRI_DISABLE_PING:-true}"
exec python3 -m kolibri "$@"
KDOS_SH
chmod 755 "$PKG/usr/bin/kolibri"
