#!/bin/bash
# kdos-sysupdated on a bus of its own, against fixture slots.
#
#   drive.sh OUT DAEMON CLIENT POLKITSTUB KDOS-TOOLS
#
# Run by testing/selftest.sh, which builds the four binaries; runnable alone
# with the same arguments. The daemon serves a private session bus with
# --session and keeps every file under OUT/root (--root), as Discover finds
# it with DISCOVER_TEST_SYSUPDATE=1. `kdos` is the stub beside this script;
# `kdos-bootctl` is the real one, on a fixture ESP.
#
# What is asserted is the conversation: Discover's call order and the
# replies it acts on (sysupclient), the API Discover's proxies are generated
# from (apidiff.py against the vendored XML), the polkit action each method
# names and whether it lets the agent ask, the AppStream file, the two-mark
# bless and the greeter's mark. Prints `FAIL` lines and exits 1 on the first
# failed group.
set -u
O=$1 DAEMON=$2 CLIENT=$3 PKSTUB=$4 KTOOLS=$5
HERE=$(cd "$(dirname "$0")" && pwd)
VENDORED=$HERE/../../../src/daemons/kdos-sysupdated/tests/org.freedesktop.sysupdate1.xml
SLOTS_XML=$HERE/../../../src/desktop/kcm-kdos-system/io.kdos.Slots1.xml
F=$O/fix R=$O/root E=$O/esp BIN=$O/bin
fail=0

rm -rf "$F" "$R" "$E" "$BIN"
mkdir -p "$F/proc" "$R/etc/sddm.conf.d" "$R/usr/bin" "$R/var/lib/kdos" \
         "$R/run/kdos/bless" "$E/EFI/kdos/a" "$E/EFI/kdos/b" "$BIN"
cp "$HERE/kdos" "$BIN/kdos"
chmod +x "$BIN/kdos"
ln -s "$KTOOLS" "$BIN/kdos-bootctl"

export SU_FIX=$F SU_ROOT=$R SU_ESP=$E
export PATH="$BIN:$PATH"
export KDOS_BOOTSTATE=$E/EFI/kdos/bootstate
export KDOS_CMDLINE=$F/cmdline KDOS_SYSROOT=$R KDOS_BOOT_ID=fixture-boot-1
export KDOS_REBOOT_ENTRY=$R/run/systemd/reboot-to-boot-loader-entry
unset KDOS_EFIVARS
# The mount table kdos-bootctl reads for the live medium: an installed root
# here, since a container's own `/` is an overlay too.
printf '/dev/vda2 / ext4 rw 0 0\n' > "$F/mounts"
export KDOS_MOUNTS=$F/mounts

printf 'kdos_slot=a root=UUID=AAAA-1111 rw\n' > "$F/cmdline"
printf 'NAME="KDOS"\nVERSION_ID="0.2"\nBUILD_ID="a81bec1c"\n' > "$R/etc/os-release"
printf 'VERSION="0.2.13"\n' > "$R/etc/kdos-release"
printf '1:2345:respawn:/usr/local/sbin/kdos-dm\n' > "$R/etc/inittab"
printf '#!/bin/sh\n' > "$R/usr/bin/sddm"
chmod +x "$R/usr/bin/sddm"
kdos-bootctl set-slot a AAAA-1111 >/dev/null
kdos-bootctl set-slot b BBBB-2222 >/dev/null
printf 'fetch ok %s tree current\n' "$(date +%s)" > "$R/var/lib/kdos/update.state"

doc() {   # <offer>; $INST is the version the writing system was
    printf '{"behind":3,"orphans":0,"tree":"29d742c14cc8aef873af7f1f28e267d6318facc2","version":"%s","installed":"'"${INST:-0.2.13}"'","problem":null,"detail":"","covered":3,"uncovered":0,"size":4096,"offer":"%s","lane":"binhost","signed_by":{"key":"0123456789abcdef","name":"KDOS release"},"verified":1761600000,"binhost":"https://example.invalid/binhost-%s","channel":"https://example.invalid/latest","notes":"Plasma 6.8.2.\\n\\nKonsole & Kate <fixed>.","checked":1761600000,"date":"2026-10-28","packages":[{"name":"aerc","have":"0.22.0","want":"0.23.0-1","binary":true},{"name":"curl","have":"8.11.0","want":"8.12.1-1","binary":true},{"name":"nftables","have":"1.1.1","want":"1.1.3-2","binary":true}]}\n' "$1" "$1" "$1"
}
put_doc() {   # <offer>: renamed into place, as `kdos update check --out` does
    doc "$1" > "$R/var/lib/kdos/update.json.new"
    mv "$R/var/lib/kdos/update.json.new" "$R/var/lib/kdos/update.json"
    printf '%s\n' "$1" > "$F/version"
}
put_doc 0.2.14

cat > "$F/bus.conf" <<'BUS'
<!DOCTYPE busconfig PUBLIC "-//freedesktop//DTD D-BUS Bus Configuration 1.0//EN"
 "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
<busconfig>
  <type>session</type>
  <listen>unix:tmpdir=/tmp</listen>
  <policy context="default">
    <allow send_destination="*" eavesdrop="true"/>
    <allow eavesdrop="true"/>
    <allow own="*"/>
  </policy>
</busconfig>
BUS
dbus-daemon --config-file="$F/bus.conf" --print-address=3 --fork \
    --print-pid=4 3>"$F/addr" 4>"$F/buspid"
A=$(cat "$F/addr")
export DBUS_SESSION_BUS_ADDRESS=$A
trap 'kill $(cat "$F/buspid") 2>/dev/null; [ -n "${DPID:-}" ] && kill $DPID 2>/dev/null; [ -n "${PKPID:-}" ] && kill $PKPID 2>/dev/null' EXIT

bc() { busctl --address="$A" "$@"; }
owner() {   # wait for a name, ten seconds at most
    for _i in $(seq 1 100); do
        dbus-send --session --print-reply --dest=org.freedesktop.DBus / \
            org.freedesktop.DBus.NameHasOwner "string:$1" 2>/dev/null |
            grep -q 'boolean true' && return 0
        sleep 0.1
    done
    return 1
}
wait_for() {   # <seconds> <command...>
    _t=$1; shift
    for _i in $(seq 1 $((_t * 10))); do
        "$@" && return 0
        sleep 0.1
    done
    return 1
}
start_daemon() {   # extra arguments; a later option overrides an earlier one
    "$DAEMON" --session --root "$R" --proc "$F/proc" --bless-after 2 \
        --tick 1 --refresh-after 0 "$@" >> "$F/daemon.log" 2>&1 &
    DPID=$!
    owner org.freedesktop.sysupdate1 && owner io.kdos.Slots1 ||
        { echo "  FAIL  the daemon did not take its names"; cat "$F/daemon.log"; exit 1; }
}
stop_daemon() {
    kill "$DPID" 2>/dev/null
    wait "$DPID" 2>/dev/null
    DPID=""
}
want() {   # <file> <line> — the line, exactly
    grep -qxF -- "$2" "$1" && return 0
    echo "  FAIL  expected in $(basename "$1"):"
    echo "          $2"
    echo "        got:"
    sed 's/^/          /' "$1"
    fail=1
    return 1
}
want_re() {   # <file> <regex>
    grep -qE -- "$2" "$1" && return 0
    echo "  FAIL  expected /$2/ in $(basename "$1"):"
    sed 's/^/          /' "$1"
    fail=1
    return 1
}
prop() {   # <property> of io.kdos.Slots1
    bc get-property io.kdos.Slots1 /io/kdos/Slots1 io.kdos.Slots1 "$1"
}

: > "$F/pk.rules"
"$PKSTUB" "$F/pk.rules" "$F/pk.log" 2>>"$F/daemon.log" &
PKPID=$!
owner org.freedesktop.PolicyKit1 || { echo "  FAIL  the polkit stub did not start"; exit 1; }
start_daemon

# ── Discover's flow ────────────────────────────────────────────────────────
timeout 60 "$CLIENT" discover > "$F/flow.txt" 2>&1
want "$F/flow.txt" "PING ok"
want "$F/flow.txt" "TARGET host kdos /org/freedesktop/sysupdate1/target/host"
want "$F/flow.txt" "APPSTREAM file://$R/run/kdos/sysupdate/io.kdos.system.metainfo.xml"
want "$F/flow.txt" "VERSION 0.2.13 (slot A)"
want "$F/flow.txt" "CHECKNEW 0.2.14"
want "$F/flow.txt" "ACQUIRE 0.2.14 1 /org/freedesktop/sysupdate1/job/_1"
want "$F/flow.txt" "REMOVED 1 /org/freedesktop/sysupdate1/job/_1 0"
want "$F/flow.txt" "INSTALL 0.2.14 2 /org/freedesktop/sysupdate1/job/_2"
want "$F/flow.txt" "REMOVED 2 /org/freedesktop/sysupdate1/job/_2 0"
# Progress climbs, never reaches 100 before the end, and ends at 100.
awk '/^PROGRESS/ { if ($2 < last) bad = 1; last = $2; n++ }
     END { exit !(n >= 2 && !bad && last == 100) }' "$F/flow.txt" ||
    { echo "  FAIL  Acquire's progress did not climb to 100:"; grep PROGRESS "$F/flow.txt"; fail=1; }
want_re "$F/calls" "^update apply --acquire --mount --progress 3$"
want_re "$F/calls" "^update apply --arm --mount --progress 3$"
want_re "$F/pk.log" "^CHECK org.freedesktop.sysupdate1.check flags=1 subject=system-bus-name:"
want_re "$F/pk.log" "^CHECK org.freedesktop.sysupdate1.update flags=1 subject=system-bus-name:"
[ "$fail" = 0 ] || exit 1
echo "  Discover's order: Ping, ListTargets, GetAppStream, GetVersion, CheckNew, Acquire with progress, Install"

# ── the slot is armed, and the session can see it ──────────────────────────
grep -q '^try      = b$' "$KDOS_BOOTSTATE" || { echo "  FAIL  Install did not arm slot b"; fail=1; }
[ "$(cat "$KDOS_REBOOT_ENTRY" 2>/dev/null)" = kdos-b ] ||
    { echo "  FAIL  the logout screen's boot entry is not kdos-b"; fail=1; }
kdos-bootctl list --json=short > "$F/list.json"
want_re "$F/list.json" '"id":"kdos-b","title":"KDOS 0.2.14 \(slot B\)","showTitle":"KDOS 0.2.14 \(slot B\)"'
bc call org.freedesktop.sysupdate1 /org/freedesktop/sysupdate1/target/host \
    org.freedesktop.sysupdate1.Target CheckNew > "$F/again.txt"
want "$F/again.txt" 's ""'
prop State > "$F/state.txt"
want_re "$F/state.txt" '"rebootRequired" b true'
want_re "$F/state.txt" '"onTrial" b false'
want_re "$F/state.txt" '"signer" s "KDOS release" "signerKey" s "0123456789abcdef" "verified" x 1761600000'
want_re "$F/state.txt" '"trying" s "b" "running" s "a"'
want_re "$F/state.txt" '"phase" s "armed" "version" s "0.2.14"'
prop TrySlot > "$F/tryslot.txt"
want "$F/tryslot.txt" 's "b"'
bc call io.kdos.Slots1 /io/kdos/Slots1 io.kdos.Slots1 ListSlots > "$F/slots.txt"
want_re "$F/slots.txt" '^aa\{sv\} 2 16 "id" s "a" "title" s "KDOS 0.2.13 \(slot A\)" "present" b true "version" s "0.2.13"'
want_re "$F/slots.txt" '"id" s "b" "title" s "KDOS 0.2.14 \(slot B\)" "present" b true "version" s "0.2.14"'
want_re "$F/slots.txt" '"confirmed" b true "trial" b false "failed" b false "bootable" b false "reason" s "This is already the version the computer starts." "state" s "confirmed"'
want_re "$F/slots.txt" '"trial" b true .* "state" s "trial"'
[ "$fail" = 0 ] || exit 1
echo "  after Install: try = b, the logout entry, CheckNew empty, Slots1 armed"

# ── the AppStream file ─────────────────────────────────────────────────────
AS=$R/run/kdos/sysupdate/io.kdos.system.metainfo.xml
python3 -I -c 'import sys, xml.etree.ElementTree as E; E.parse(sys.argv[1])' "$AS" ||
    { echo "  FAIL  the AppStream file is not well-formed XML"; fail=1; }
want_re "$AS" '<component type="operating-system">'
want_re "$AS" '<release version="0.2.14" type="stable" date="2026-10-28">'
want_re "$AS" 'Signed by KDOS release \(ed25519 0123…cdef\), verified on this machine\.'
want_re "$AS" 'Konsole &amp; Kate &lt;fixed&gt;\.'
want_re "$AS" '3 packages from the signed binhost: aerc, curl and nftables; 4\.0K to download\. They are installed into slot B'
# The same offer on the image lane says so, and names no packages.
doc 0.2.14 | sed 's/"lane":"binhost"/"lane":"image"/' > "$R/var/lib/kdos/update.json.new"
mv "$R/var/lib/kdos/update.json.new" "$R/var/lib/kdos/update.json"
wait_for 5 grep -q 'The signed system image of 0.2.14' "$AS" ||
    { echo "  FAIL  an image-lane document did not reach the AppStream file"; fail=1; }
want_re "$AS" 'The signed system image of 0\.2\.14, written whole; 4\.0K to download\. It is written into slot B'
put_doc 0.2.14
wait_for 5 grep -q '3 packages from the signed binhost' "$AS" ||
    { echo "  FAIL  the binhost-lane text did not come back"; fail=1; }
[ "$fail" = 0 ] || exit 1
echo "  the AppStream file: one operating-system component, the release, its signer, its lane"

# ── the API Discover was built against ─────────────────────────────────────
dbus-send --session --print-reply=literal --dest=org.freedesktop.sysupdate1 \
    /org/freedesktop/sysupdate1 org.freedesktop.DBus.Introspectable.Introspect \
    > "$F/manager.xml"
dbus-send --session --print-reply=literal --dest=org.freedesktop.sysupdate1 \
    /org/freedesktop/sysupdate1/target/host \
    org.freedesktop.DBus.Introspectable.Introspect > "$F/target.xml"

# A slow acquire: its job is introspected while it exists, then cancelled.
put_doc 0.2.15
wait_for 5 grep -q 'version="0.2.15"' "$AS" ||
    { echo "  FAIL  a new update document did not reach the AppStream file"; exit 1; }
echo slow > "$F/acquire"
bc call org.freedesktop.sysupdate1 /org/freedesktop/sysupdate1/target/host \
    org.freedesktop.sysupdate1.Target Acquire st "" 0 > "$F/slow.txt"
want "$F/slow.txt" 'sto "0.2.15" 3 "/org/freedesktop/sysupdate1/job/_3"' || exit 1
dbus-send --session --print-reply=literal --dest=org.freedesktop.sysupdate1 \
    /org/freedesktop/sysupdate1/job/_3 \
    org.freedesktop.DBus.Introspectable.Introspect > "$F/job.xml"
python3 -I "$HERE/apidiff.py" "$VENDORED" "$F/manager.xml" "$F/target.xml" \
    "$F/job.xml" > "$F/apidiff.txt" 2>&1 ||
    { echo "  FAIL  the served API differs from Discover 6.8.2's:"; sed 's/^/          /' "$F/apidiff.txt"; fail=1; }
# io.kdos.Slots1 against the System Versions page's contract, where that
# page is in the tree.
if [ -f "$SLOTS_XML" ]; then
    dbus-send --session --print-reply=literal --dest=io.kdos.Slots1 \
        /io/kdos/Slots1 org.freedesktop.DBus.Introspectable.Introspect \
        > "$F/slots.xml"
    python3 -I "$HERE/apidiff.py" "$SLOTS_XML" "$F/slots.xml" \
        > "$F/slotsdiff.txt" 2>&1 ||
        { echo "  FAIL  io.kdos.Slots1 differs from $SLOTS_XML:"; sed 's/^/          /' "$F/slotsdiff.txt"; fail=1; }
fi
bc call org.freedesktop.sysupdate1 /org/freedesktop/sysupdate1 \
    org.freedesktop.sysupdate1.Manager ListJobs > "$F/jobs.txt"
want_re "$F/jobs.txt" '^a\(tsuo\) 1 3 "acquire" [0-9]+ "/org/freedesktop/sysupdate1/job/_3"$'
prop State > "$F/state.txt"
want_re "$F/state.txt" '"job" s "acquire"'
want_re "$F/state.txt" '"phase" s "acquiring"'
bc call org.freedesktop.sysupdate1 /org/freedesktop/sysupdate1/target/host \
    org.freedesktop.sysupdate1.Target Acquire st "" 0 > "$F/busy.txt" 2>&1
want_re "$F/busy.txt" "already running"
bc call org.freedesktop.sysupdate1 /org/freedesktop/sysupdate1/job/_3 \
    org.freedesktop.sysupdate1.Job Cancel > /dev/null ||
    { echo "  FAIL  Cancel was refused"; fail=1; }
wait_for 10 sh -c '! busctl --address="$0" call org.freedesktop.sysupdate1 /org/freedesktop/sysupdate1 org.freedesktop.sysupdate1.Manager ListJobs | grep -q acquire' "$A" ||
    { echo "  FAIL  the cancelled job is still listed"; fail=1; }
prop State > "$F/error.txt"
want_re "$F/error.txt" '"error" s "cancelled"'
[ "$fail" = 0 ] || exit 1
echo "  the served APIs match Discover 6.8.2's XML and the Slots1 contract; one job at a time; Cancel stops it"

# ── Discover's cancel, and a job that fails ────────────────────────────────
timeout 60 "$CLIENT" cancel > "$F/cancel.txt" 2>&1
want_re "$F/cancel.txt" "^ACQUIRE 0.2.15 4 /org/freedesktop/sysupdate1/job/_4$"
want "$F/cancel.txt" "CANCEL ok"
want "$F/cancel.txt" "REMOVED 4 /org/freedesktop/sysupdate1/job/_4 -125"
echo fail > "$F/acquire"
timeout 60 "$CLIENT" discover > "$F/fail.txt" 2>&1
want "$F/fail.txt" "REMOVED 5 /org/freedesktop/sysupdate1/job/_5 1"
grep -q '^INSTALL' "$F/fail.txt" && { echo "  FAIL  Install followed a failed Acquire"; fail=1; }
prop State > "$F/error.txt"
want_re "$F/error.txt" '"error" s "the binhost package of zlib did not install"'
[ "$fail" = 0 ] || exit 1
echo "  a cancelled job ends -ECANCELED, a failed one with its status and the reason in Slots1"

# ── refusals reach Discover's banner as the call's error ───────────────────
echo refuse > "$F/acquire"
timeout 60 "$CLIENT" discover > "$F/refuse.txt" 2>&1
want "$F/refuse.txt" "ERROR io.kdos.Sysupdate.Error.Refused 2 packages need a source build (gimp, krita): \`kdos update apply --source\`"
grep -q '^REMOVED' "$F/refuse.txt" && { echo "  FAIL  a refused Acquire sent JobRemoved"; fail=1; }
echo ok > "$F/acquire"
echo "org.freedesktop.sysupdate1.update no" > "$F/pk.rules"
timeout 60 "$CLIENT" discover > "$F/denied.txt" 2>&1
want_re "$F/denied.txt" "^ERROR org.freedesktop.DBus.Error.AccessDenied "
echo "org.freedesktop.sysupdate1.update challenge" > "$F/pk.rules"
bc --allow-interactive-authorization=no call org.freedesktop.sysupdate1 \
    /org/freedesktop/sysupdate1/target/host org.freedesktop.sysupdate1.Target \
    Acquire st "" 0 > "$F/nointeract.txt" 2>&1
want_re "$F/nointeract.txt" "Interactive authentication required"
want_re "$F/pk.log" "^CHECK org.freedesktop.sysupdate1.update flags=0 "
bc call org.freedesktop.sysupdate1 /org/freedesktop/sysupdate1/target/host \
    org.freedesktop.sysupdate1.Target Install st "9.9" 0 > "$F/badver.txt" 2>&1
want_re "$F/badver.txt" "nothing has been acquired|is not available"
: > "$F/pk.rules"
[ "$fail" = 0 ] || exit 1
echo "  a refusal, a denial and a challenge without interaction are each the call's error"

# ── Try and Bless ──────────────────────────────────────────────────────────
bc call io.kdos.Slots1 /io/kdos/Slots1 io.kdos.Slots1 Try s z > "$F/try.txt" 2>&1
want_re "$F/try.txt" "no slot named 'z'"
bc call io.kdos.Slots1 /io/kdos/Slots1 io.kdos.Slots1 Try s a > "$F/try.txt" 2>&1
want_re "$F/try.txt" "bootctl: slot a is already active"
want_re "$F/pk.log" "^CHECK io.kdos.slots.try flags=1 "
# CancelTry forgets the armed trial, and Try arms it again.
bc call io.kdos.Slots1 /io/kdos/Slots1 io.kdos.Slots1 CancelTry > /dev/null ||
    { echo "  FAIL  CancelTry was refused"; fail=1; }
grep -q '^try      = $' "$KDOS_BOOTSTATE" || { echo "  FAIL  CancelTry left the trial armed"; fail=1; }
[ -e "$KDOS_REBOOT_ENTRY" ] && { echo "  FAIL  CancelTry left the logout entry"; fail=1; }
bc call io.kdos.Slots1 /io/kdos/Slots1 io.kdos.Slots1 Try s b > /dev/null ||
    { echo "  FAIL  Try b was refused"; fail=1; }
grep -q '^try      = b$' "$KDOS_BOOTSTATE" || { echo "  FAIL  Try did not arm slot b"; fail=1; }
grep -q '^attempts = 1$' "$KDOS_BOOTSTATE" || { echo "  FAIL  Try gave more than one boot"; fail=1; }
# SetChannel rewrites the tree = line and nothing else.
mkdir -p "$R/etc/kdos"
printf '# kept\nbinhost = /srv/binhost\ntree = /old\n' > "$R/etc/kdos/update.conf"
bc call io.kdos.Slots1 /io/kdos/Slots1 io.kdos.Slots1 SetChannel s "https://example.invalid/kdos" > /dev/null ||
    { echo "  FAIL  SetChannel was refused"; fail=1; }
printf '# kept\nbinhost = /srv/binhost\ntree = https://example.invalid/kdos\n' | cmp -s - "$R/etc/kdos/update.conf" ||
    { echo "  FAIL  SetChannel wrote:"; cat "$R/etc/kdos/update.conf"; fail=1; }
prop Channel > "$F/channel.txt"
want "$F/channel.txt" 's "https://example.invalid/kdos"'
bc call io.kdos.Slots1 /io/kdos/Slots1 io.kdos.Slots1 SetChannel s "ftp://x y" > "$F/channel.txt" 2>&1
want_re "$F/channel.txt" "not an update channel"
want_re "$F/pk.log" "^CHECK io.kdos.slots.channel flags=1 "
bc call io.kdos.Slots1 /io/kdos/Slots1 io.kdos.Slots1 SetChannel s "" > /dev/null
printf '# kept\nbinhost = /srv/binhost\n' | cmp -s - "$R/etc/kdos/update.conf" ||
    { echo "  FAIL  SetChannel \"\" did not remove the line"; fail=1; }
# The first mark alone, on a machine that starts a display manager, confirms
# nothing.
: > "$R/run/kdos/bless/userspace"
sleep 2
[ -e "$R/run/kdos/bless/confirmed" ] && { echo "  FAIL  one mark confirmed the boot"; fail=1; }
echo "io.kdos.slots.bless no" > "$F/pk.rules"
bc call io.kdos.Slots1 /io/kdos/Slots1 io.kdos.Slots1 Bless > "$F/bless.txt" 2>&1
want_re "$F/bless.txt" "Access denied"
[ -e "$R/run/kdos/bless/graphical" ] && { echo "  FAIL  a refused Bless wrote the mark"; fail=1; }
: > "$F/pk.rules"
bc call io.kdos.Slots1 /io/kdos/Slots1 io.kdos.Slots1 Bless > /dev/null ||
    { echo "  FAIL  Bless was refused"; fail=1; }
want_re "$F/pk.log" "^CHECK io.kdos.slots.bless flags=0 "
wait_for 5 test -e "$R/run/kdos/bless/confirmed" ||
    { echo "  FAIL  both marks did not confirm the boot"; cat "$F/daemon.log"; fail=1; }
prop State > "$F/blessed.txt"
want_re "$F/blessed.txt" '"blessUserspace" b true "blessGraphical" b true'
want_re "$F/blessed.txt" '"blessed" b true'
# This boot armed slot b, so its mark-good leaves the trial alone.
grep -q '^try      = b$' "$KDOS_BOOTSTATE" ||
    { echo "  FAIL  the bless of the boot that armed the trial cancelled it"; fail=1; }
stop_daemon
[ "$fail" = 0 ] || exit 1
echo "  Try and CancelTry through kdos-bootctl, SetChannel keeps the rest of update.conf; Bless writes the second mark"

# ── a machine nobody logs in to: the greeter earns the mark ────────────────
# Only processes of SDDM's account count: first a pair with the greeter's
# names run by an ordinary account, which must earn nothing.
rm -f "$R/run/kdos/bless/"*
printf 'root:x:0:0::/root:/bin/sh\nsddm:x:982:982:SDDM Greeter Account:/var/lib/sddm:/sbin/nologin\nkdos:x:1000:1000::/home/kdos:/bin/bash\n' > "$R/etc/passwd"
mkdir -p "$F/proc/700" "$F/proc/701" "$F/proc/702"
printf 'sddm-greeter-qt6\0' > "$F/proc/700/cmdline"
printf 'Name:\tsddm-greeter-qt\nUid:\t1000\t1000\t1000\t1000\n' > "$F/proc/700/status"
printf '/usr/bin/kwin_wayland\0' > "$F/proc/701/cmdline"
printf 'Name:\tkwin_wayland\nUid:\t1000\t1000\t1000\t1000\n' > "$F/proc/701/status"
rm -f "$F/proc/702/cmdline" "$F/proc/702/status"
: > "$R/run/kdos/bless/userspace"
start_daemon
sleep 4
[ -e "$R/run/kdos/bless/graphical" ] &&
    { echo "  FAIL  a greeter run by an ordinary account earned the mark"; fail=1; }
stop_daemon
rm -f "$R/run/kdos/bless/"*
printf 'sddm-greeter-qt6\0--socket\0/tmp/sddm\0' > "$F/proc/700/cmdline"
printf 'Name:\tsddm-greeter-qt\nUid:\t982\t982\t982\t982\n' > "$F/proc/700/status"
printf '/usr/bin/kwin_wayland\0--no-lockscreen\0' > "$F/proc/701/cmdline"
printf 'Name:\tkwin_wayland\nUid:\t982\t982\t982\t982\n' > "$F/proc/701/status"
printf '/usr/bin/kwin_wayland\0' > "$F/proc/702/cmdline"
printf 'Name:\tkwin_wayland\nUid:\t1000\t1000\t1000\t1000\n' > "$F/proc/702/status"
: > "$R/run/kdos/bless/userspace"
start_daemon
wait_for 8 test -e "$R/run/kdos/bless/graphical" ||
    { echo "  FAIL  the greeter did not earn the graphical mark"; cat "$F/daemon.log"; fail=1; }
wait_for 5 test -e "$R/run/kdos/bless/confirmed" ||
    { echo "  FAIL  the greeter's mark did not confirm the boot"; fail=1; }
stop_daemon
# Autologin configured: the session confirms, the greeter does not.
rm -f "$R/run/kdos/bless/"*
printf '[Autologin]\nUser=kdos\nSession=plasma\n' > "$R/etc/sddm.conf.d/kde_settings.conf"
: > "$R/run/kdos/bless/userspace"
start_daemon
sleep 4
[ -e "$R/run/kdos/bless/graphical" ] &&
    { echo "  FAIL  the greeter confirmed a machine that logs in by itself"; fail=1; }
stop_daemon
rm -f "$R/etc/sddm.conf.d/kde_settings.conf"
# A user session existed this boot: the greeter no longer counts.
rm -f "$R/run/kdos/bless/"*
mkdir -p "$R/run/systemd/sessions"
printf 'UID=1000\nUSER=kdos\nCLASS=user\nTYPE=wayland\n' > "$R/run/systemd/sessions/2"
: > "$R/run/kdos/bless/userspace"
start_daemon
sleep 4
[ -e "$R/run/kdos/bless/graphical" ] &&
    { echo "  FAIL  the greeter confirmed a boot whose session had come and gone"; fail=1; }
stop_daemon
rm -rf "$R/run/systemd/sessions"
[ "$fail" = 0 ] || exit 1
echo "  the greeter earns the mark after its minute, unless the machine logs in or a session existed"

# ── a document another state of this root wrote ────────────────────────────
# The first boot of a slot reads the document the slot's system wrote before
# it was filled: it offers nothing, and a check of this root, with no
# network, rewrites it. A CheckNew while that check runs waits for it.
INST=0.2.12 put_doc 0.2.16
doc 0.2.16 > "$F/next.json"
echo 3 > "$F/checkdelay"
: > "$F/calls"
start_daemon
prop State > "$F/stale.txt"
want_re "$F/stale.txt" '"offer" s ""'
bc call org.freedesktop.sysupdate1 /org/freedesktop/sysupdate1/target/host \
    org.freedesktop.sysupdate1.Target CheckNew > "$F/stale.txt"
want "$F/stale.txt" 's "0.2.16"'
want "$F/calls" "update check --out $R/var/lib/kdos/update.json"
grep -q -- '--fetch' "$F/calls" && { echo "  FAIL  the check of a stale document asked the network"; fail=1; }
stop_daemon
rm -f "$F/next.json" "$F/checkdelay"
[ "$fail" = 0 ] || exit 1
echo "  a document another state of this root wrote offers nothing until a local check rewrites it"

# ── "Check for updates": the network at most once an hour ──────────────────
# A CheckNew more than --refresh-after seconds after the last fetch fetches
# the signed tree and index first, and answers from what arrived; the next
# CheckNew inside the hour is answered from the document.
{ grep -v '^fetch ' "$R/var/lib/kdos/update.state"
  printf 'fetch ok 1000 tree current\n'; } > "$F/state.new"
mv "$F/state.new" "$R/var/lib/kdos/update.state"
doc 0.2.17 > "$F/next.json"
: > "$F/calls"
start_daemon --refresh-after 3600
for _n in 1 2; do
    bc call org.freedesktop.sysupdate1 /org/freedesktop/sysupdate1/target/host \
        org.freedesktop.sysupdate1.Target CheckNew > "$F/fetch.txt"
    want "$F/fetch.txt" 's "0.2.17"'
done
want "$F/calls" "update check --fetch --out $R/var/lib/kdos/update.json"
[ "$(grep -c -- '--fetch' "$F/calls")" = 1 ] ||
    { echo "  FAIL  two CheckNew calls inside the hour fetched twice"; cat "$F/calls"; fail=1; }
stop_daemon
rm -f "$F/next.json"
[ "$fail" = 0 ] || exit 1
echo "  CheckNew fetches the signed tree and index once an hour at most, and answers from them"

# ── a trial boot: the slot an update would fill is the way back ────────────
# Slot b is armed; booting it makes this boot its trial. Nothing is offered
# and Acquire is refused before `kdos update` runs, until the trial ends.
rm -f "$R/run/kdos/bless/"* "$F/proc/"*/cmdline
printf 'kdos_slot=b root=UUID=BBBB-2222 rw\n' > "$F/cmdline"
doc 0.2.17 > "$R/var/lib/kdos/update.json.new"
mv "$R/var/lib/kdos/update.json.new" "$R/var/lib/kdos/update.json"
: > "$F/calls"
start_daemon
prop State > "$F/trial.txt"
want_re "$F/trial.txt" '"onTrial" b true'
want_re "$F/trial.txt" '"offer" s ""'
bc call org.freedesktop.sysupdate1 /org/freedesktop/sysupdate1/target/host \
    org.freedesktop.sysupdate1.Target CheckNew > "$F/trial.txt"
want "$F/trial.txt" 's ""'
bc call org.freedesktop.sysupdate1 /org/freedesktop/sysupdate1/target/host \
    org.freedesktop.sysupdate1.Target Acquire st "" 0 > "$F/trial.txt" 2>&1
want_re "$F/trial.txt" "trial of slot B"
grep -q '^update apply' "$F/calls" && { echo "  FAIL  an Acquire on a trial boot ran kdos update"; fail=1; }
stop_daemon
printf 'kdos_slot=a root=UUID=AAAA-1111 rw\n' > "$F/cmdline"
[ "$fail" = 0 ] || exit 1
echo "  a trial boot is offered nothing and refuses Acquire"

# ── a machine with no display manager, and one with no second slot ─────────
rm -f "$R/run/kdos/bless/"*
printf '1:2345:respawn:/sbin/agetty tty1 38400 linux\n' > "$R/etc/inittab"
mv "$KDOS_BOOTSTATE" "$F/bootstate.saved"
: > "$R/run/kdos/bless/userspace"
start_daemon
wait_for 5 test -e "$R/run/kdos/bless/confirmed" ||
    { echo "  FAIL  a console machine waited for a graphical mark"; fail=1; }
bc call org.freedesktop.sysupdate1 /org/freedesktop/sysupdate1/target/host \
    org.freedesktop.sysupdate1.Target CheckNew > "$F/noslots.txt"
want "$F/noslots.txt" 's ""'
bc call org.freedesktop.sysupdate1 /org/freedesktop/sysupdate1/target/host \
    org.freedesktop.sysupdate1.Target GetVersion > "$F/noslots.txt"
want "$F/noslots.txt" 's "0.2.13"'
bc call org.freedesktop.sysupdate1 /org/freedesktop/sysupdate1/target/host \
    org.freedesktop.sysupdate1.Target Acquire st "" 0 > "$F/noslots.txt" 2>&1
want_re "$F/noslots.txt" "no second root slot"
prop Live > "$F/noslots.txt"
want "$F/noslots.txt" "b false"
stop_daemon
[ "$fail" = 0 ] || exit 1
echo "  a console machine is confirmed by rcS's mark; a single-root machine is offered nothing"

# ── the live medium ────────────────────────────────────────────────────────
# No boot state and `/` an overlay, as kdos-bootctl reads /proc/mounts: Live
# is true, nothing is offered, Acquire says to install KDOS rather than to
# update in place, and the AppStream file names the live session.
printf 'overlay / overlay rw,lowerdir=/mnt/system,upperdir=/mnt/overlay/upper 0 0\n' > "$F/mounts"
start_daemon
prop Live > "$F/live.txt"
want "$F/live.txt" "b true"
prop Configured > "$F/live.txt"
want "$F/live.txt" "b false"
bc call org.freedesktop.sysupdate1 /org/freedesktop/sysupdate1/target/host \
    org.freedesktop.sysupdate1.Target CheckNew > "$F/live.txt"
want "$F/live.txt" 's ""'
bc call org.freedesktop.sysupdate1 /org/freedesktop/sysupdate1/target/host \
    org.freedesktop.sysupdate1.Target Acquire st "" 0 > "$F/live.txt" 2>&1
want_re "$F/live.txt" "this is the live medium.*install KDOS to update it"
want_re "$R/run/kdos/sysupdate/io.kdos.system.metainfo.xml" \
    '<summary>The KDOS live session, which system updates do not reach</summary>'
grep -q '<release ' "$R/run/kdos/sysupdate/io.kdos.system.metainfo.xml" &&
    { echo "  FAIL  the live medium's AppStream file offers a release"; fail=1; }
stop_daemon
printf '/dev/vda2 / ext4 rw 0 0\n' > "$F/mounts"
mv "$F/bootstate.saved" "$KDOS_BOOTSTATE"
[ "$fail" = 0 ] || exit 1
echo "  the live medium is named as such, offered nothing and told to install KDOS"
exit 0
