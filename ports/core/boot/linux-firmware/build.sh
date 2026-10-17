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

./copy-firmware.sh --zstd "$PKG/lib/firmware"

fw="$PKG/lib/firmware"
find "$fw/qcom" -mindepth 1 -maxdepth 1 \
	! -name aic100 ! -name aic200 ! -name qdu100 ! -name 'sdx*' ! -name 'prog_firehose_*' \
	-exec rm -rf {} +
find "$fw/mellanox" -name 'mlxsw_spectrum*.mfa2*' \
	! -name 'mlxsw_spectrum-13.2010.1006.mfa2*' \
	! -name 'mlxsw_spectrum2-29.2010.1006.mfa2*' \
	! -name 'mlxsw_spectrum3-30.2010.1006.mfa2*' \
	-exec rm -f {} +
rm -f "$fw"/mrvl/prestera/mvsw_prestera_fw-v2.0.img* "$fw"/mrvl/prestera/mvsw_prestera_fw-v3.0.img*
while IFS= read -r l; do
	case "$(readlink "$l")" in
	qcom/*) [ -e "$l" ] || rm -f "$l" ;;
	esac
done < <(find "$fw" -maxdepth 1 -type l)

dangling=0
while IFS= read -r l; do
	[ -e "$l" ] || { echo "dangling firmware symlink: $l" >&2; dangling=$((dangling + 1)); }
done < <(find "$PKG/lib/firmware" -type l)
if [ "$dangling" -ne 0 ]; then
	echo "ERROR: $dangling dangling firmware symlink(s)" >&2
	exit 1
fi

install -d "$PKG/usr/share/licenses/$name"
install -m644 WHENCE "$PKG/usr/share/licenses/$name/"
find . -maxdepth 1 -type f -name 'LICEN[CS]E*' -exec install -m644 -t "$PKG/usr/share/licenses/$name/" {} +
cp -r LICENSES "$PKG/usr/share/licenses/$name/"
missing=0
for lic in $(grep -oE 'See LICEN[CS]E[._][A-Za-z0-9._+-]*' WHENCE | sed 's/^See //; s/\.$//' | sort -u); do
	[ -e "$PKG/usr/share/licenses/$name/LICENSES/$lic" ] || { echo "WHENCE names a missing licence: $lic" >&2; missing=1; }
done
[ "$missing" -eq 0 ]
