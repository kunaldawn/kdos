#!/bin/sh
# tree.sh <dir> — the /etc and /usr/share kdos-settingsd is driven against.
#
# Built rather than committed, because most of it is empty files whose NAMES
# are the fixture: a zone file is anything that starts with the TZif magic, a
# console keymap is any file called <name>.map.gz, an XKB layout is any file
# under symbols/. The rules list is the one real excerpt, copied from
# evdev.lst beside this script.
#
#   <dir>/etc     KDOS_SETTINGSD_ETC   — what the image ships
#   <dir>/share   KDOS_SETTINGSD_SHARE — zoneinfo, keymaps, xkb
#   <dir>/sys     KDOS_SETTINGSD_SYS   — one laptop's firmware, as sysfs
#                                        shows it
#   <dir>/bin     first on PATH        — service, loadkeys, iw and chronyd,
#                                        each recording its argv in ran.log
set -eu
D=$1
HERE=$(cd "$(dirname "$0")" && pwd)
REPO=$(cd "$HERE/../../.." && pwd)

rm -rf "$D"
mkdir -p "$D/etc/init.d" "$D/etc/kdos" "$D/etc/profile.d" \
    "$D/share/zoneinfo/Europe" "$D/share/zoneinfo/America" \
    "$D/share/X11/xkb/rules" "$D/share/X11/xkb/symbols" \
    "$D/share/keymaps/i386/qwerty" "$D/share/keymaps/i386/qwertz" \
    "$D/share/keymaps/i386/azerty" "$D/share/keymaps/i386/dvorak" "$D/bin" \
    "$D/sys/class/dmi/id" "$D/sys/firmware/acpi" \
    "$D/sys/firmware/devicetree/base"

# ── /etc as the image ships it ───────────────────────────────────────────
printf 'kdos' > "$D/etc/hostname"
cp "$REPO/fs/etc/hosts" "$D/etc/hosts"
printf 'us\n' > "$D/etc/keymap"
cp "$REPO/fs/etc/kdos/xkb.conf" "$D/etc/kdos/xkb.conf"
cp "$REPO/fs/etc/locale.conf" "$D/etc/locale.conf"
printf '0123456789abcdef0123456789abcdef\n' > "$D/etc/machine-id"
cat > "$D/etc/os-release" <<'EOF'
NAME="KDOS"
PRETTY_NAME="KDOS — the fixture's \"own\" name"
ID=kdos
HOME_URL="https://github.com/kunaldawn/kdos"
IMAGE_ID=kdos-live
IMAGE_VERSION=0.2.0
SUPPORT_END=2031-12-31
EOF

# ── the firmware ─────────────────────────────────────────────────────────
# One field per file with the kernel's trailing newline, one padded with the
# spaces firmware leaves, and one holding a vendor's placeholder for "unset".
# The chassis has all three witnesses, so the unit can take them away in
# systemd's order: DMI's type 3 (desktop), ACPI's profile 2 (mobile) and a
# device tree's word.
dmi() { printf '%s\n' "$2" > "$D/sys/class/dmi/id/$1"; }
dmi sys_vendor LENOVO
dmi product_name 21AH00ABUS
dmi product_sku LENOVO_MT_21AH_BU_Think_FM_ThinkPad
dmi product_version 'ThinkPad T14 Gen 3   '
dmi bios_vendor LENOVO
dmi bios_version 'N3MET18W (1.17 )'
dmi bios_date 03/14/2024
dmi chassis_asset_tag 'TO BE FILLED BY O.E.M.'
dmi chassis_type 3
dmi product_uuid 4c4c4544-0042-3510-8052-b4c04f4d4433
dmi product_serial PF3ABCDE
printf '2\n' > "$D/sys/firmware/acpi/pm_profile"
printf 'tablet' > "$D/sys/firmware/devicetree/base/chassis-type"
# The chrony service, as rcS and `service` find it: by its name, under a
# number the boot order chose.
printf '#!/bin/sh\nexit 0\n' > "$D/etc/init.d/35_chrony.sh"
chmod 755 "$D/etc/init.d/35_chrony.sh"

# ── the zone tree ────────────────────────────────────────────────────────
for z in UTC Europe/Berlin Europe/London America/New_York; do
    printf 'TZif2 fixture zone\n' > "$D/share/zoneinfo/$z"
done
# A name that exists and is not a zone file.
printf 'not a zone\n' > "$D/share/zoneinfo/Europe/Notazone"
printf '# comment\nDE\t+5230+01322\tEurope/Berlin\nGB\t+513030-0000731\tEurope/London\nUS\t+404251-0740023\tAmerica/New_York\n' \
    > "$D/share/zoneinfo/zone.tab"
cat > "$D/share/zoneinfo/tzdata.zi" <<'EOF'
# version fixture
Z Europe/Berlin 0:53:28 - LMT 1893 Ap
Z Europe/London -0:1:15 - LMT 1847 D
Z America/New_York -4:56:2 - LMT 1883 N 18 17u
L America/New_York US/Eastern
L Europe/London GB
EOF

# ── xkeyboard-config ─────────────────────────────────────────────────────
cp "$HERE/evdev.lst" "$D/share/X11/xkb/rules/evdev.lst"
for l in us gb de ch fr; do : > "$D/share/X11/xkb/symbols/$l"; done

# ── kbd's keymaps ────────────────────────────────────────────────────────
for m in qwerty/us qwerty/uk qwertz/de-latin1 qwertz/de-latin1-nodeadkeys \
         qwertz/sg qwertz/fr_CH azerty/fr dvorak/dvorak; do
    : > "$D/share/keymaps/i386/$m.map.gz"
done

# ── the programs, each a recorder ────────────────────────────────────────
for p in service loadkeys iw chronyd; do
    cat > "$D/bin/$p" <<EOF
#!/bin/sh
printf '%s\n' "$p \$*" >> "$D/ran.log"
exit 0
EOF
    chmod 755 "$D/bin/$p"
done
: > "$D/ran.log"
