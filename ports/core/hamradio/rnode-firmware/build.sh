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

_dest=$PKG/usr/share/rnode-firmware/$version
install -dm755 "$_dest"
install -m644 "$name-$version.json" "$_dest/release.json"
install -m644 "$name-console-$version.bin" "$_dest/console_image.bin"
for _zip in "$name"-*-"$version".zip; do
	_board=${_zip#"$name"-}
	_board=${_board%-"$version".zip}
	install -m644 "$_zip" "$_dest/rnode_firmware_$_board.zip"
done
python3 - "$_dest" <<'PY'
import json, os, sys
dest = sys.argv[1]
with open(os.path.join(dest, "release.json")) as f:
    release = json.load(f)
for fw, info in release.items():
    if not os.path.exists(os.path.join(dest, fw)):
        continue
    with open(os.path.join(dest, fw + ".version"), "w") as out:
        out.write(f"{info['version']} {info['hash']}")
PY
printf '%s\n' "$version" > "$PKG/usr/share/rnode-firmware/version"

install -d "$PKG/usr/bin"
cat > "$PKG/usr/bin/rnodeconf-local" <<'KDOS_SH'
#!/bin/sh
share=/usr/share/rnode-firmware
read -r ver < "$share/version" || exit 1
cache=$HOME/.config/rnodeconf/update/$ver
mkdir -p "$cache" || exit 1
for f in "$share/$ver"/*; do
	t=$cache/${f##*/}
	case ${f##*/} in
	console_image.bin)
		[ -L "$t" ] && rm -f "$t"
		[ -e "$t" ] || cp "$f" "$t" || exit 1
		;;
	*)
		[ -e "$t" ] || ln -s "$f" "$t"
		;;
	esac
done
exec rnodeconf --nocheck --fw-version "$ver" "$@"
KDOS_SH
chmod 755 "$PKG/usr/bin/rnodeconf-local"
