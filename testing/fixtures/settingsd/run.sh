#!/bin/bash
# run.sh <bin> <tree> — kdos-settingsd on a private bus, every method driven.
#
#   <bin>   holds kdos-settingsd and settingsd-polkit (polkit.c), built by
#           testing/selftest.sh against whichever sd-bus the host has
#   <tree>  what tree.sh built: the daemon's /etc and /usr/share, and the
#           recorders it finds first on PATH
#
# THE BUS IS A PRIVATE dbus-daemon, and the daemon is pointed at it as its
# SYSTEM bus (DBUS_SYSTEM_BUS_ADDRESS). That matters: on a system bus sd-bus
# refuses every method not marked unprivileged unless the caller holds
# CAP_SYS_ADMIN, so a method missing the flag fails here exactly as it would
# for the Date & Time page.
#
# THE CLIENT IS dbus-send, the reference implementation's, which is on every
# host that has a dbus-daemon. A call's reply is its values, an error is the
# line `Error <name>: <message>`.
#
# POLKIT IS polkit.c, answering from $W/answers and logging each question, so
# every refusal is asserted twice: the caller is refused, and the file is as
# it was. Run as root, the daemon authorises its caller without asking —
# root may write each of these files by hand — so the refusal halves are
# skipped and said to be.
set -u
B=$1 W=$2
REPO=$(cd "$(dirname "$0")/../../.." && pwd)
failed=0
ROOT=0
[ "$(id -u)" = 0 ] && ROOT=1

ok()  { echo "  ok    $1"; }
bad() {
    echo "  FAIL  $1"
    shift
    for l in "$@"; do echo "        $l"; done
    failed=1
}
is() {   # <what> <expected> <got>
    if [ "$3" = "$2" ]; then ok "$1"; else bad "$1" "expected: $2" "got:      $3"; fi
}
has() {  # <what> <substring> <got>
    case "$3" in
    *"$2"*) ok "$1" ;;
    *) bad "$1" "expected: …$2…" "got:      $3" ;;
    esac
}
lacks() {  # <what> <substring> <got>
    case "$3" in
    *"$2"*) bad "$1" "did not expect: $2" "got:            $3" ;;
    *) ok "$1" ;;
    esac
}
unless_root() {  # <what> <command…> — a refusal half, which root never meets
    local what=$1
    shift
    if [ "$ROOT" = 1 ]; then
        echo "  skip  $what (root is authorised without polkit)"
    elif "$@"; then
        ok "$what"
    else
        bad "$what"
    fi
}

ADDR=$(dbus-daemon --session --print-address --fork --print-pid=3 \
    3>"$W/bus.pid") || { echo "  FAIL  no private dbus-daemon"; exit 1; }
BUSPID=$(cat "$W/bus.pid")
D="" PK="" MON=""
cleanup() {
    for p in $D $PK $MON $BUSPID; do kill "$p" 2>/dev/null; done
}
trap cleanup EXIT

: > "$W/answers"
: > "$W/polkit.log"
DBUS_SYSTEM_BUS_ADDRESS="$ADDR" "$B/settingsd-polkit" "$W/answers" \
    "$W/polkit.log" 2>"$W/polkit.err" &
PK=$!
dbus-monitor --address "$ADDR" \
    "type='signal',interface='org.freedesktop.DBus.Properties',member='PropertiesChanged'" \
    > "$W/signals.log" 2>&1 &
MON=$!

# exec'd from a subshell, so the pid `$!` names is the daemon itself and a
# signal sent to it reaches the daemon rather than a shell waiting on it.
daemon() {
    exec env PATH="$W/bin:$PATH" DBUS_SYSTEM_BUS_ADDRESS="$ADDR" \
        KDOS_SETTINGSD_ETC="$W/etc" KDOS_SETTINGSD_SHARE="$W/share" \
        KDOS_SETTINGSD_SYS="$W/sys" "$B/kdos-settingsd"
}
( daemon ) 2>"$W/daemon.err" &
D=$!

owned() {
    dbus-send --bus="$ADDR" --print-reply=literal --dest=org.freedesktop.DBus \
        /org/freedesktop/DBus org.freedesktop.DBus.NameHasOwner "string:$1" \
        2>/dev/null | grep -q true
}
for _ in $(seq 100); do
    owned org.freedesktop.PolicyKit1 && owned org.freedesktop.hostname1 &&
        owned org.freedesktop.timedate1 && owned org.freedesktop.locale1 && break
    sleep 0.1
done
owned org.freedesktop.locale1 || {
    echo "  FAIL  the daemon never took its names"
    cat "$W/daemon.err" "$W/polkit.err"
    exit 1
}

# call <service> <Method> [dbus-send args…]
call() {
    local s=$1 m=$2
    shift 2
    dbus-send --bus="$ADDR" --print-reply=literal --reply-timeout=10000 \
        --dest="org.freedesktop.$s" "/org/freedesktop/$s" \
        "org.freedesktop.$s.$m" "$@" 2>&1
}
# prop <service> <Property> — the value, with dbus-send's variant prefix off
prop() {
    dbus-send --bus="$ADDR" --print-reply=literal \
        --dest="org.freedesktop.$1" "/org/freedesktop/$1" \
        org.freedesktop.DBus.Properties.Get "string:org.freedesktop.$1" \
        "string:$2" 2>&1 | sed -e 's/^ *variant *//' -e 's/^ *//'
}
getall() {
    dbus-send --bus="$ADDR" --print-reply --dest="org.freedesktop.$1" \
        "/org/freedesktop/$1" org.freedesktop.DBus.Properties.GetAll \
        "string:org.freedesktop.$1" 2>&1
}
# answer <action without org.freedesktop.> yes|no|challenge
answer() {
    grep -v "^org.freedesktop.$1 " "$W/answers" > "$W/answers.new"
    echo "org.freedesktop.$1 $2" >> "$W/answers.new"
    mv "$W/answers.new" "$W/answers"
}
asked() { wc -l < "$W/polkit.log" | tr -d ' '; }
last_asked() { tail -n1 "$W/polkit.log"; }
ran() { cat "$W/ran.log"; }
file() { cat "$W/etc/$1" 2>/dev/null || echo "<absent>"; }

# EVERY METHOD IS UNPRIVILEGED, read from the introspection data. The access
# check the flag lifts grants a caller with the daemon's own uid, which every
# caller here has, so a method missing it would still answer below; on the
# real system bus a user's call to it is refused before polkit is asked.
# sd-bus marks such a method `org.freedesktop.systemd1.Privileged`.
for s in hostname1 timedate1 locale1; do
    x=$(dbus-send --bus="$ADDR" --print-reply=literal --dest="org.freedesktop.$s" \
        "/org/freedesktop/$s" org.freedesktop.DBus.Introspectable.Introspect 2>&1)
    has "$s introspects" "<interface name=\"org.freedesktop.$s\">" "$x"
    lacks "and no method of it needs a privileged caller" \
        "org.freedesktop.systemd1.Privileged" "$x"
done

echo "  -- org.freedesktop.hostname1"
is "StaticHostname is /etc/hostname" "kdos" "$(prop hostname1 StaticHostname)"
is "DefaultHostname is the image's" "kdos" "$(prop hostname1 DefaultHostname)"
is "OperatingSystemPrettyName is os-release's, unquoted" \
    "KDOS — the fixture's \"own\" name" "$(prop hostname1 OperatingSystemPrettyName)"
has "GetAll answers for the whole interface" "KernelRelease" "$(getall hostname1)"
a=$(getall hostname1)
for p in OperatingSystemSupportEnd OperatingSystemImageID HardwareSKU \
    FirmwareDate MachineID BootID VSockCID ChassisAssetTag; do
    has "and carries systemd's $p" "\"$p\"" "$a"
done
is "HardwareVendor is DMI's sys_vendor" "LENOVO" "$(prop hostname1 HardwareVendor)"
is "HardwareVersion is DMI's, its padding off" "ThinkPad T14 Gen 3" \
    "$(prop hostname1 HardwareVersion)"
is "a Lenovo's HardwareModel is its product_version, as systemd reports it" \
    "ThinkPad T14 Gen 3" "$(prop hostname1 HardwareModel)"
is "a vendor's placeholder asset tag reads as none" "" \
    "$(prop hostname1 ChassisAssetTag)"
is "FirmwareDate is bios_date at midnight UTC, in µs" "uint64 1710374400000000" \
    "$(prop hostname1 FirmwareDate)"
is "OperatingSystemSupportEnd is os-release's SUPPORT_END" \
    "uint64 1956441600000000" "$(prop hostname1 OperatingSystemSupportEnd)"
is "OperatingSystemImageVersion is os-release's" "0.2.0" \
    "$(prop hostname1 OperatingSystemImageVersion)"
has "MachineID is /etc/machine-id's sixteen bytes" \
    "01 23 45 67 89 ab cd ef 01 23 45 67 89 ab cd ef" "$(prop hostname1 MachineID)"
has "VSockCID is a uint32" "uint32 " "$(prop hostname1 VSockCID)"
is "Chassis is the firmware's until machine-info names one" "desktop" \
    "$(prop hostname1 Chassis)"
is "and the icon is named after it" "computer-desktop" "$(prop hostname1 IconName)"
printf 'HARDWARE_VENDOR="Kunal'"'"'s Workshop"\n' > "$W/etc/machine-info"
is "machine-info's HARDWARE_VENDOR corrects the firmware's" "Kunal's Workshop" \
    "$(prop hostname1 HardwareVendor)"
rm -f "$W/etc/machine-info"

answer hostname1.get-product-uuid no
r=$(call hostname1 GetProductUUID boolean:false)
unless_root "a refused GetProductUUID is AccessDenied" grep -q "AccessDenied" <<<"$r"
lacks "and gives no byte of the UUID" "4c 4c 45 44" "$( [ "$ROOT" = 1 ] || echo "$r")"
answer hostname1.get-product-uuid yes
has "an authorised GetProductUUID is DMI's product_uuid, as bytes" \
    "4c 4c 45 44 00 42 35 10 80 52 b4 c0 4f 4d 44 33" \
    "$(call hostname1 GetProductUUID boolean:false)"
unless_root "polkit was asked get-product-uuid" \
    grep -qx "org.freedesktop.hostname1.get-product-uuid 0 system-bus-name unique" \
    <<<"$(last_asked)"
answer hostname1.get-hardware-serial yes
is "an authorised GetHardwareSerial is DMI's product_serial" "PF3ABCDE" \
    "$(call hostname1 GetHardwareSerial | sed 's/^ *//')"
mv "$W/sys/class/dmi/id/product_serial" "$W/sys/class/dmi/id/product_serial.off"
has "with no serial anywhere it is NoHardwareSerial" \
    "org.freedesktop.hostname1.NoHardwareSerial" "$(call hostname1 GetHardwareSerial)"
mv "$W/sys/class/dmi/id/product_serial.off" "$W/sys/class/dmi/id/product_serial"

answer hostname1.set-static-hostname no
n=$(asked)
r=$(call hostname1 SetStaticHostname string:workbox boolean:false)
unless_root "a refused SetStaticHostname is AccessDenied" \
    grep -q "AccessDenied" <<<"$r"
unless_root "and leaves /etc/hostname alone" test "$(file hostname)" = kdos
unless_root "polkit was asked for set-static-hostname about the caller's bus name" \
    grep -qx "org.freedesktop.hostname1.set-static-hostname 0 system-bus-name unique" \
    <<<"$(last_asked)"
answer hostname1.set-static-hostname yes
r=$(call hostname1 SetStaticHostname string:workbox boolean:false)
lacks "an authorised SetStaticHostname succeeds" "Error" "$r"
is "and writes /etc/hostname" "workbox" "$(file hostname)"
is "and the kernel name follows it" "workbox" "$(prop hostname1 Hostname)"
is "and its source is the static name" "static" "$(prop hostname1 HostnameSource)"
has "and /etc/hosts resolves the new name" "127.0.1.1   workbox" "$(file hosts)"
lacks "and no longer the old one" "127.0.1.1   kdos" "$(file hosts)"

answer hostname1.set-hostname yes
call hostname1 SetHostname string:passing-through boolean:false >/dev/null
is "a transient name does not displace the static one" "workbox" \
    "$(prop hostname1 Hostname)"
call hostname1 SetStaticHostname string: boolean:false >/dev/null
is "an empty static name removes /etc/hostname" "<absent>" "$(file hostname)"
is "and the transient name takes the kernel" "passing-through" \
    "$(prop hostname1 Hostname)"
is "with the transient source" "transient" "$(prop hostname1 HostnameSource)"
has "and /etc/hosts follows the kernel's name" "127.0.1.1   passing-through" \
    "$(file hosts)"
call hostname1 SetStaticHostname string:kdos boolean:false >/dev/null

n=$(asked)
for bad_name in 'bad_name!' 'under_score' '-lead' 'a..b' \
    'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa'; do
    has "SetStaticHostname '$bad_name' is InvalidArgs" "InvalidArgs" \
        "$(call hostname1 SetStaticHostname "string:$bad_name" boolean:false)"
done
is "and polkit was asked nothing about any of them" "$n" "$(asked)"

answer hostname1.set-machine-info yes
call hostname1 SetPrettyHostname "string:My \"box\" costs \$5" boolean:false >/dev/null
is "a pretty name round-trips its quotes and dollar" "My \"box\" costs \$5" \
    "$(prop hostname1 PrettyHostname)"
has "and is in machine-info, escaped" 'PRETTY_HOSTNAME="My \"box\" costs \$5"' \
    "$(file machine-info)"
has "a pretty name with a newline is refused" "InvalidArgs" \
    "$(call hostname1 SetPrettyHostname "string:two
lines" boolean:false)"
call hostname1 SetChassis string:laptop boolean:false >/dev/null
is "SetChassis writes the chassis" "laptop" "$(prop hostname1 Chassis)"
is "and the icon follows it until one is set" "computer-laptop" \
    "$(prop hostname1 IconName)"
has "an unknown chassis is refused" "InvalidArgs" \
    "$(call hostname1 SetChassis string:spaceship boolean:false)"
has "an icon name with a slash is refused" "InvalidArgs" \
    "$(call hostname1 SetIconName string:../evil boolean:false)"
call hostname1 SetDeployment string:production boolean:false >/dev/null
is "SetDeployment writes the deployment" "production" "$(prop hostname1 Deployment)"

# A string that is not UTF-8 cannot be sent at all, so one hand-edited field
# that is not would fail GetAll for every property beside it.
printf 'LOCATION="\377 not UTF-8"\n' >> "$W/etc/machine-info"
has "a hand-written field that is not UTF-8 leaves GetAll answering" \
    "PrettyHostname" "$(getall hostname1)"
is "and reads as no value" "" "$(prop hostname1 Location)"

answer hostname1.set-machine-info challenge
r=$(call hostname1 SetLocation string:Desk boolean:false)
unless_root "a challenge without interactive is InteractiveAuthorizationRequired" \
    grep -q "InteractiveAuthorizationRequired" <<<"$r"
r=$(call hostname1 SetLocation string:Desk boolean:true)
lacks "and with interactive set it is answered" "Error" "$r"
unless_root "the interactive call asked polkit with AllowUserInteraction" \
    grep -qx "org.freedesktop.hostname1.set-machine-info 1 system-bus-name unique" \
    <<<"$(last_asked)"
is "SetLocation wrote the location" "Desk" "$(prop hostname1 Location)"
n=$(asked)
call hostname1 SetLocation string:Desk boolean:false >/dev/null
is "setting the value already set asks polkit nothing" "$n" "$(asked)"
has "PropertiesChanged announced the chassis and the icon" "IconName" \
    "$(cat "$W/signals.log")"

echo "  -- org.freedesktop.timedate1"
is "no /etc/localtime is UTC" "UTC" "$(prop timedate1 Timezone)"
is "CanNTP: chrony's script and chronyd are there" "boolean true" \
    "$(prop timedate1 CanNTP)"
is "NTP: chrony is not disabled" "boolean true" "$(prop timedate1 NTP)"
is "LocalRTC is false: the RTC is UTC" "boolean false" "$(prop timedate1 LocalRTC)"
has "GetAll answers for the whole interface" "RTCTimeUSec" "$(getall timedate1)"

answer timedate1.set-timezone no
r=$(call timedate1 SetTimezone string:Europe/Berlin boolean:true)
unless_root "a refused SetTimezone is AccessDenied" grep -q "AccessDenied" <<<"$r"
unless_root "and no link was written" test ! -L "$W/etc/localtime"
answer timedate1.set-timezone yes
r=$(call timedate1 SetTimezone string:Europe/Berlin boolean:true)
lacks "an authorised SetTimezone succeeds" "Error" "$r"
is "and /etc/localtime points into the zone tree" \
    "$W/share/zoneinfo/Europe/Berlin" "$(readlink "$W/etc/localtime")"
is "and Timezone reads it back" "Europe/Berlin" "$(prop timedate1 Timezone)"
has "TZ in the profile names the link" "export TZ=':/etc/localtime'" \
    "$(file profile.d/20-timezone.sh)"
has "the zone's country is the regulatory domain" "ieee80211_regdom=DE" \
    "$(file modprobe.d/kdos-regdom.conf)"
has "and iw was told now, by argument vector" "iw reg set DE" "$(ran)"
has "PropertiesChanged announced the zone" "Timezone" "$(cat "$W/signals.log")"
n=$(asked)
has "'../../etc/shadow' is not a zone name" "not a time zone name" \
    "$(call timedate1 SetTimezone string:../../etc/shadow boolean:false)"
has "'Mars/Olympus' is no such zone" "no such time zone" \
    "$(call timedate1 SetTimezone string:Mars/Olympus boolean:false)"
has "a directory of the tree is no zone" "no such time zone" \
    "$(call timedate1 SetTimezone string:Europe boolean:false)"
has "a file without the TZif magic is no zone" "no such time zone" \
    "$(call timedate1 SetTimezone string:Europe/Notazone boolean:false)"
is "and polkit was asked nothing about any of them" "$n" "$(asked)"
is "a refused zone left the working one" "Europe/Berlin" "$(prop timedate1 Timezone)"
call timedate1 SetTimezone string:UTC boolean:false >/dev/null
is "a zone with no country removes the old country" "<absent>" \
    "$(file modprobe.d/kdos-regdom.conf)"
has "and returns the running radio to the world domain" "iw reg set 00" "$(ran)"

z=$(dbus-send --bus="$ADDR" --print-reply --dest=org.freedesktop.timedate1 \
    /org/freedesktop/timedate1 org.freedesktop.timedate1.ListTimezones 2>&1)
has "ListTimezones carries the zones" '"Europe/Berlin"' "$z"
has "and the old names tzdata.zi links" '"US/Eastern"' "$z"
has "and UTC" '"UTC"' "$z"

has "SetTime while NTP is on is AutomaticTimeSyncEnabled" \
    "org.freedesktop.timedate1.AutomaticTimeSyncEnabled" \
    "$(call timedate1 SetTime int64:1000000 boolean:true boolean:false)"
answer timedate1.set-ntp yes
: > "$W/ran.log"
r=$(call timedate1 SetNTP boolean:false boolean:false)
lacks "SetNTP false succeeds" "Error" "$r"
if [ -e "$W/etc/service.disabled/chrony" ]; then
    ok "and writes the marker rcS obeys"
else
    bad "and writes the marker rcS obeys"
fi
has "and stops chrony now, through service" "service stop chrony" "$(ran)"
is "NTP reads false at once, before SetTime is called" "boolean false" \
    "$(prop timedate1 NTP)"
answer timedate1.set-time no
r=$(call timedate1 SetTime int64:1000000 boolean:true boolean:false)
unless_root "a refused SetTime is AccessDenied" grep -q "AccessDenied" <<<"$r"
answer timedate1.set-time yes
lacks "an authorised relative SetTime succeeds (a test tree moves no clock)" \
    "Error" "$(call timedate1 SetTime int64:1000000 boolean:true boolean:false)"
has "an absolute time of zero is refused" "InvalidArgs" \
    "$(call timedate1 SetTime int64:0 boolean:false boolean:false)"
has "a relative time that overflows is refused" "overflow" \
    "$(call timedate1 SetTime int64:9223372036854775807 boolean:true boolean:false)"
: > "$W/ran.log"
call timedate1 SetNTP boolean:true boolean:false >/dev/null
if [ ! -e "$W/etc/service.disabled/chrony" ]; then
    ok "SetNTP true removes the marker"
else
    bad "SetNTP true removes the marker"
fi
has "and starts chrony now" "service start chrony" "$(ran)"
has "SetLocalRTC true is NotSupported" "NotSupported" \
    "$(call timedate1 SetLocalRTC boolean:true boolean:false boolean:false)"
n=$(asked)
lacks "SetLocalRTC false, with nothing to fix, succeeds" "Error" \
    "$(call timedate1 SetLocalRTC boolean:false boolean:false boolean:false)"
is "and asks polkit nothing" "$n" "$(asked)"

echo "  -- org.freedesktop.locale1"
l=$(dbus-send --bus="$ADDR" --print-reply --dest=org.freedesktop.locale1 \
    /org/freedesktop/locale1 org.freedesktop.DBus.Properties.Get \
    string:org.freedesktop.locale1 string:Locale 2>&1)
has "the shipped locale.conf is LANG=en_US.UTF-8, its comment skipped" \
    'string "LANG=en_US.UTF-8"' "$l"
lacks "and the comment is no variable" 'string "#' "$l"
is "VConsoleKeymap is /etc/keymap" "us" "$(prop locale1 VConsoleKeymap)"
is "the shipped xkb.conf follows it: X11Layout" "us" "$(prop locale1 X11Layout)"
has "GetAll answers for the whole interface" "X11Options" "$(getall locale1)"

n=$(asked)
call locale1 SetLocale array:string:LANG=en_US.UTF-8 boolean:false >/dev/null
is "setting the locale already set asks polkit nothing" "$n" "$(asked)"
answer locale1.set-locale no
r=$(call locale1 SetLocale array:string:LANG=en_GB.UTF-8 boolean:false)
unless_root "a refused SetLocale is AccessDenied" grep -q "AccessDenied" <<<"$r"
unless_root "and leaves the shipped file as it was" \
    cmp -s "$W/etc/locale.conf" "$REPO/fs/etc/locale.conf"
answer locale1.set-locale yes
r=$(call locale1 SetLocale array:string:LANG=en_GB.UTF-8,LC_TIME=en_DK.UTF-8 boolean:false)
lacks "an authorised SetLocale succeeds" "Error" "$r"
is "locale.conf is bare NAME=value lines, the shape SDDM reads" \
    "LANG=en_GB.UTF-8
LC_TIME=en_DK.UTF-8" "$(file locale.conf)"
l=$(dbus-send --bus="$ADDR" --print-reply --dest=org.freedesktop.locale1 \
    /org/freedesktop/locale1 org.freedesktop.DBus.Properties.Get \
    string:org.freedesktop.locale1 string:Locale 2>&1)
has "and Locale reads it back" '"LC_TIME=en_DK.UTF-8"' "$l"
n=$(asked)
has "a variable given twice is refused" "InvalidArgs" \
    "$(call locale1 SetLocale array:string:LANG=en_GB.UTF-8,LANG=de_DE.UTF-8 boolean:false)"
has "an unknown variable is refused" "InvalidArgs" \
    "$(call locale1 SetLocale array:string:LD_PRELOAD=/tmp/x.so boolean:false)"
has "a quote in a value is refused" "InvalidArgs" \
    "$(call locale1 SetLocale 'array:string:LANG=en"GB' boolean:false)"
is "and polkit was asked nothing about any of them" "$n" "$(asked)"
call locale1 SetLocale array:string:LANG=C.UTF-8 boolean:false >/dev/null
is "a new list replaces the old one whole" "LANG=C.UTF-8" "$(file locale.conf)"

answer locale1.set-keyboard yes
s0=$(wc -l < "$W/signals.log")
: > "$W/ran.log"
r=$(call locale1 SetVConsoleKeyboard string:de-latin1 string: boolean:false boolean:false)
lacks "SetVConsoleKeyboard succeeds" "Error" "$r"
is "and writes /etc/keymap" "de-latin1" "$(file keymap)"
has "and loads it on the console, by argument vector" "loadkeys -q de-latin1" "$(ran)"
is "an empty stored layout follows the console: X11Layout" "de" \
    "$(prop locale1 X11Layout)"
sleep 0.2
has "and the login screen is told: PropertiesChanged carries X11Layout" \
    "X11Layout" "$(tail -n +"$((s0 + 1))" "$W/signals.log")"
has "a keymap that is not installed is refused" "InvalidArgs" \
    "$(call locale1 SetVConsoleKeyboard string:nosuchmap string: boolean:false boolean:false)"
has "a keymap given as a path is refused" "InvalidArgs" \
    "$(call locale1 SetVConsoleKeyboard string:/etc/passwd string: boolean:false boolean:false)"
has "a toggle keymap is NotSupported" "NotSupported" \
    "$(call locale1 SetVConsoleKeyboard string:us string:de-latin1 boolean:false boolean:false)"

: > "$W/ran.log"
r=$(call locale1 SetX11Keyboard string:gb string:pc105 string: string: boolean:true boolean:false)
lacks "SetX11Keyboard with convert succeeds" "Error" "$r"
has "and writes xkb.conf" "XKBLAYOUT=gb" "$(file kdos/xkb.conf)"
has "keeping its explanation" "AN EMPTY XKBLAYOUT FOLLOWS THE CONSOLE" \
    "$(file kdos/xkb.conf)"
is "X11Model reads back" "pc105" "$(prop locale1 X11Model)"
is "convert set the nearest console map" "uk" "$(file keymap)"
has "and loaded it" "loadkeys -q uk" "$(ran)"
call locale1 SetVConsoleKeyboard string:us string: boolean:false boolean:false >/dev/null
is "an explicit layout is kept when the console changes" "gb" \
    "$(prop locale1 X11Layout)"
call locale1 SetX11Keyboard string:ch string: string:fr string:ctrl:nocaps \
    boolean:true boolean:false >/dev/null
is "a layout with a variant converts to the console map for both" "fr_CH" \
    "$(file keymap)"
is "and the options read back" "ctrl:nocaps" "$(prop locale1 X11Options)"
n=$(asked)
has "a layout xkeyboard-config does not list is refused" "InvalidArgs" \
    "$(call locale1 SetX11Keyboard string:xx string: string: string: boolean:false boolean:false)"
has "a variant under the wrong layout is refused" "InvalidArgs" \
    "$(call locale1 SetX11Keyboard string:de string: string:dvorak string: boolean:false boolean:false)"
has "a variant with no layout is refused" "InvalidArgs" \
    "$(call locale1 SetX11Keyboard string: string: string:dvorak string: boolean:false boolean:false)"
has "an option with a semicolon is refused" "InvalidArgs" \
    "$(call locale1 SetX11Keyboard string:us string: string: 'string:x;y' boolean:false boolean:false)"
is "and polkit was asked nothing about any of them" "$n" "$(asked)"
answer locale1.set-keyboard no
r=$(call locale1 SetX11Keyboard string:us string: string: string: boolean:false boolean:false)
unless_root "a refused SetX11Keyboard is AccessDenied" grep -q "AccessDenied" <<<"$r"
unless_root "and xkb.conf keeps the layout it had" \
    grep -qx "XKBLAYOUT=ch" "$W/etc/kdos/xkb.conf"

if [ "$ROOT" = 1 ]; then
    is "root's calls were answered without asking polkit once" "0" "$(asked)"
fi

echo "  -- the daemon"
( daemon ) >/dev/null 2>"$W/second.err"
is "a second copy on the same bus exits 3, the status supervise stops on" \
    "3" "$?"
if [ "$ROOT" = 0 ]; then
    env -u KDOS_SETTINGSD_ETC "$B/kdos-settingsd" >/dev/null 2>"$W/noroot.err"
    is "outside a test tree it refuses to run as non-root" "1" "$?"
else
    echo "  skip  the non-root refusal (running as root)"
fi
chmod 644 "$W/etc/init.d/35_chrony.sh"
is "without chrony's script CanNTP is false" "boolean false" "$(prop timedate1 CanNTP)"
has "and SetNTP true is NoNTPSupport" "org.freedesktop.timedate1.NoNTPSupport" \
    "$(call timedate1 SetNTP boolean:true boolean:false)"
lacks "while SetNTP false, sent by every Apply of the Date & Time page, succeeds" \
    "Error" "$(call timedate1 SetNTP boolean:false boolean:false)"
kill -TERM "$D"
wait "$D"
is "SIGTERM ends it cleanly" "0" "$?"
D=""
if [ "$failed" != 0 ] && [ -s "$W/daemon.err" ]; then
    echo "  daemon stderr:"
    sed 's/^/    /' "$W/daemon.err"
fi
exit "$failed"
