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

fw_vendor() {
	case "$1" in
	i915/* | xe/* | intel/* | iwlwifi-* | qat_* | isci/*) vendor=intel ;;
	amdgpu/* | radeon/* | amd/* | amd-ucode/* | amdnpu/* | amdtee/* | r128/*) vendor=amd ;;
	nvidia/*) vendor=nvidia ;;
	rtl_bt/* | rtl_nic/* | rtlwifi/* | rtw88/* | rtw89/* | realtek/*) vendor=realtek ;;
	mediatek/* | rt[0-9]*) vendor=mediatek ;;
	brcm/* | cypress/* | bnx2/* | bnx2x/* | tigon/*) vendor=broadcom ;;
	ath[0-9]*/* | ath3k-* | ar3k/* | ar5523.* | ar7010* | ar9170-* | ar9271.* | carl9170-* | htc_* | qca/* | qcom/* | wil6210.*) vendor=qualcomm ;;
	*) vendor=misc ;;
	esac
}

fw="$SRC_ROOT/firmware"
rm -rf "$fw"
./copy-firmware.sh --zstd "$fw"

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
done < <(find "$fw" -type l)
if [ "$dangling" -ne 0 ]; then
	echo "ERROR: $dangling dangling firmware symlink(s)" >&2
	exit 1
fi

map="$SRC_ROOT/firmware-map"
own="$SRC_ROOT/firmware-own"
: > "$map"
: > "$own"
while IFS= read -r -d '' f; do
	rel=${f#"$fw"/}
	target=$rel
	[ -L "$f" ] && target=$(realpath --relative-to="$fw" "$f")
	fw_vendor "$target"
	printf 'firmware-%s %s\n' "$vendor" "${rel%.zst}" >> "$map"
	[ "firmware-$vendor" = "$name" ] && printf '%s\0' "$rel" >> "$own"
done < <(find "$fw" \( -type f -o -type l \) -print0)

if [ "$name" = firmware-map ]; then
	install -d "$PKG/usr/share/kdos"
	LC_ALL=C sort -k 2 -o "$PKG/usr/share/kdos/firmware-map" "$map"
	chmod 644 "$PKG/usr/share/kdos/firmware-map"
else
	[ -s "$own" ] || { echo "ERROR: no firmware file is $name's" >&2; exit 1; }
	install -d "$PKG/lib/firmware"
	tar -C "$fw" --null -T "$own" -cf - | tar -C "$PKG/lib/firmware" -xf -
	while IFS= read -r l; do
		[ -e "$l" ] || { echo "ERROR: $name links $l to another port's file" >&2; exit 1; }
	done < <(find "$PKG/lib/firmware" -type l)
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
