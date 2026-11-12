#!/bin/bash
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   bus.sh DIR KDOS-NOTICE FAKEBUS — kdos-notice on a private session bus
#
# Re-runs itself under dbus-run-session. The notification server, Plasma's
# shell and io.kdos.Slots1 are fakebus; `kdos` and `kdos-bootctl` are stubs
# printing the fixture JSON a step puts in DIR/now; kdos-oomd is
# fakeoomd.py. The daemon then runs as the session would run it, its
# watches pointed at DIR, and every Notify it sends is counted twice — by
# the server's log and by dbus-monitor — so the claim "exactly one
# notification per change of state" is measured on the wire.
# ---------------------------------
set -u
if [ -z "${KN_BUS_INNER:-}" ]; then
	KN_BUS_INNER=1 exec dbus-run-session -- bash "$0" "$@"
fi
D=$1 KN=$2 FB=$3
HERE=$(cd "$(dirname "$0")" && pwd)
F=$HERE/fixtures
rm -rf "$D"
mkdir -p "$D/bin" "$D/now" "$D/ran" "$D/state" "$D/data/applications" \
	"$D/db" "$D/var" "$D/run"
PIDS=""
trap 'kill $PIDS 2>/dev/null' EXIT

# Milliseconds of the wall clock; bash's own, since busybox date has no %N.
ms() { local t=${EPOCHREALTIME/./}; echo $((10#$t / 1000)); }
fail() { echo "  FAIL  $*"; for f in "$D/log" "$D/notice.err"; do [ -f "$f" ] && sed 's/^/        /' "$f" | tail -20; done; exit 1; }
ok() { echo "  ok    $*"; }
waitfor() { # CMD... — true within five seconds
	local i=0
	until "$@"; do
		i=$((i + 1)); [ $i -gt 100 ] && return 1; sleep 0.05
	done
}
count() { grep -c "^$1" "$D/log" 2>/dev/null || true; }
nis() { [ "$(count "$1")" = "$2" ]; }
now() { cp "$F/$2" "$D/now/$1.json"; }

for c in kdos kdos-bootctl; do
	cat > "$D/bin/$c" <<EOF
#!/bin/sh
case "\$*" in
"restarts --json") cat "$D/now/restarts.json"; exit 1 ;;
"cve --json") cat "$D/now/cve.json"; exit 1 ;;
"status --json") cat "$D/now/boot.json" ;;
*) exit 2 ;;
esac
EOF
	chmod +x "$D/bin/$c"
done
for c in konsole systemsettings; do
	printf '#!/bin/sh\nprintf "%%s\\n" "$@" > "%s/ran/%s.tmp"\nprintf "%%s\\n" "$XDG_ACTIVATION_TOKEN" >> "%s/ran/%s.tmp"\nmv "%s/ran/%s.tmp" "%s/ran/%s"\n' \
		"$D" "$c" "$D" "$c" "$D" "$c" "$D" "$c" > "$D/bin/$c"
	chmod +x "$D/bin/$c"
done
printf '[Desktop Entry]\nType=Application\nName=Firefox ESR\nExec=firefox-esr %%u\n' \
	> "$D/data/applications/firefox-esr.desktop"
printf '[Desktop Entry]\nType=Application\nName=Konsole\nExec=konsole\n' \
	> "$D/data/applications/org.kde.konsole.desktop"

export PATH="$D/bin:$PATH" XDG_STATE_HOME=$D/state XDG_DATA_HOME=$D/data \
	XDG_DATA_DIRS=$D/none KDOS_NOTICE_BOOT_ID=boot-1 \
	KDOS_NOTICE_SETTLE_MS=0 KDOS_NOTICE_QUIET_MS=300 KDOS_NOTICE_OOM_MS=200 \
	KDOS_OOMD_SOCK=$D/oomd.sock KDOS_NOTICE_PKGDB=$D/db \
	KDOS_NOTICE_UPDATE_JSON=$D/var/update.json \
	KDOS_NOTICE_BOOTSTATE=$D/run/bootstate.json \
	KDOS_APPS_PENDING=$D/var/apps-pending

now restarts restarts-apps.json
now cve cve-3.json
now boot boot-trial.json
echo "ok trigger 'some 150000 1000000', kills 0" > "$D/oom"
python3 -I "$HERE/fakeoomd.py" "$D/oomd.sock" "$D/oom" & PIDS="$PIDS $!"
"$FB" notify "$D/log" & FBP=$!; PIDS="$PIDS $FBP"
dbus-monitor "type='method_call',interface='org.freedesktop.Notifications',member='Notify'" \
	> "$D/monitor" 2>/dev/null & PIDS="$PIDS $!"
waitfor test -S "$D/oomd.sock" || fail "the fake kdos-oomd did not start"
waitfor dbus-send --session --print-reply --dest=org.freedesktop.DBus / \
	org.freedesktop.DBus.GetNameOwner string:org.freedesktop.Notifications \
	> /dev/null 2>&1 || fail "the fake notification server did not start"

"$KN" 2> "$D/notice.err" & KNP=$!; PIDS="$PIDS $KNP"
waitfor nis notify 3 || fail "the first look did not post restarts, cve and the trial"
grep -q "^notify 1 0 0 Firefox ESR and Konsole were updated$" "$D/log" || fail "the restarts notice, persistent"
grep -q "^notify 2 0 0 3 known vulnerabilities in installed packages$" "$D/log" || fail "the cve notice, persistent"
grep -q "^notify 3 0 -1 Running KDOS 0.2.14 on trial$" "$D/log" || fail "the trial notice, timed"
"$KN" 2>> "$D/notice.err"; [ $? = 0 ] || fail "a second instance did not step aside"
kill -0 $KNP 2>/dev/null || fail "the first instance died"
ok "the first look posts the three that are news, once each"

dbus-send --session --print-reply --dest=io.kdos.Notice /io/kdos/Notice \
	io.kdos.Notice1.Rescan > /dev/null || fail "Rescan"
dbus-send --session --print-reply --dest=io.kdos.Notice /io/kdos/Notice \
	io.kdos.Notice1.Rescan > /dev/null || fail "Rescan"
[ "$(count notify)" = 3 ] || fail "looking again at the same facts posted again"
ok "the same facts, looked at twice more, post nothing"

now cve cve-2.json
for i in 1 2 3 4 5; do : > "$D/db/pkg$i"; sleep 0.05; done
waitfor nis notify 4 || fail "a package change did not lead to one look"
grep -q "^notify 2 2 0 2 known vulnerabilities" "$D/log" || fail "the new findings replace the old notice"
sleep 0.5
[ "$(count notify)" = 4 ] || fail "five package changes made more than one look"
ok "a burst of package changes is one look, and the CVE notice is replaced"

now restarts restarts-none.json
printf '{}\n' > "$D/var/update.json.tmp"; mv "$D/var/update.json.tmp" "$D/var/update.json"
waitfor nis close 1 || fail "the restarts notice was not withdrawn"
grep -q "^close 1$" "$D/log" || fail "the withdrawn one is the restarts notice"
ok "update.json written: restarts gone, its notice withdrawn"

now boot boot-kept.json
printf '{}\n' > "$D/run/bootstate.json"
waitfor nis close 2 || fail "the kept trial's notice was not withdrawn"
grep -q "^close 3$" "$D/log" || fail "the withdrawn one is the trial's"
ok "bootstate.json changed: the kept trial's notice withdrawn"

echo "ok trigger 'x', kills 1, last: gimp (appbox paint) (pid 9, 2048 MB)" > "$D/oom"
waitfor nis notify 5 || fail "a kill was not announced"
grep -q "^notify [0-9]* 0 -1 gimp in the box paint was closed to free memory$" "$D/log" || fail "the kill names the box"
sleep 0.6
[ "$(count notify)" = 5 ] || fail "one kill was announced twice"
ok "a kdos-oomd kill is announced once, naming the box"

"$FB" forge 2 details
sleep 0.4
[ ! -e "$D/ran/konsole" ] || fail "a forged ActionInvoked was obeyed"
dbus-send --session --print-reply --dest=org.freedesktop.Notifications \
	/org/freedesktop/Notifications io.kdos.Test.Invoke uint32:2 string:details > /dev/null ||
	fail "Invoke"
waitfor test -e "$D/ran/konsole" || fail "Details on the CVE notice ran nothing"
printf -- '--hold\n-e\nkdos\ncve\ntoken-from-server\n' | diff - "$D/ran/konsole" > /dev/null ||
	fail "Details ran $(tr '\n' ' ' < "$D/ran/konsole")"
ok "an action from the server runs its vector with the token; a forged one does not"

printf 'org.kde.kate\norg.gimp.GIMP\n' > "$D/var/apps-pending"
dbus-send --session --print-reply --dest=io.kdos.Notice /io/kdos/Notice \
	io.kdos.Notice1.Rescan > /dev/null || fail "Rescan"
waitfor nis notify 6 || fail "the pending applications were not announced"
grep -q "^notify [0-9]* 0 0 2 applications chosen during install are not installed yet$" "$D/log" ||
	fail "the pending notice, persistent"
ok "the applications still to install are announced, persistent"

kill $FBP; wait $FBP 2>/dev/null
"$FB" notify "$D/log2" & FBP=$!; PIDS="$PIDS $FBP"
waitfor grep -qs "^notify" "$D/log2" || fail "a new notification server was not given the notices"
sleep 2.5
grep -q "^notify [0-9]* 0 0 2 known vulnerabilities in installed packages$" "$D/log2" ||
	fail "the CVE notice was not posted afresh to the new server"
grep -q "^notify [0-9]* 0 0 2 applications chosen during install are not installed yet$" "$D/log2" ||
	fail "the pending notice was not posted afresh to the new server"
[ "$(grep -c '^notify' "$D/log2")" = 2 ] || fail "the new server was given more than the notices still waiting"
grep -q "^close" "$D/log2" && fail "an id of the old server was closed on the new one"
ok "a restarted notification server is given the waiting notices again, under its own ids"

n_log=$(( $(count notify) + $(grep -c '^notify' "$D/log2") ))
n_mon=$(grep -c 'member=Notify' "$D/monitor")
[ "$n_log" = "$n_mon" ] || fail "the server saw $n_log Notify calls and dbus-monitor $n_mon"
ok "dbus-monitor counts the same $n_mon Notify calls"
kill $KNP 2>/dev/null; wait $KNP 2>/dev/null

# ── --bless ────────────────────────────────────────────────────────────
"$FB" slots "$D/bless" & SLP=$!; PIDS="$PIDS $SLP"
waitfor dbus-send --session --print-reply --dest=io.kdos.Slots1 /io/kdos/Slots1 \
	org.freedesktop.DBus.Peer.Ping > /dev/null 2>&1 || fail "the fake Slots1 did not start"
t0=$(ms)
KDOS_SLOTS1_BUS=session KDOS_NOTICE_BLESS_SECONDS=2 "$KN" --bless > "$D/bless.out" 2>&1 & BP=$!
sleep 1
[ ! -s "$D/bless" ] || fail "Bless before the shell existed"
"$FB" shell & SHP=$!; PIDS="$PIDS $SHP"
sleep 1.5
[ ! -s "$D/bless" ] || fail "Bless before the shell had answered for the interval"
wait $BP || fail "--bless did not finish"
t1=$(ms)
[ "$(grep -c '^bless' "$D/bless")" = 1 ] || fail "Bless was not called exactly once"
[ $((t1 - t0)) -ge 2900 ] || fail "Bless came $((t1 - t0)) ms after the start, before the shell's two seconds"
ok "--bless calls Bless once, only after the shell answered for the interval"

: > "$D/bless"
KDOS_SLOTS1_BUS=session KDOS_NOTICE_BLESS_SECONDS=2 "$KN" --bless > "$D/bless.out" 2>&1 & BP=$!
sleep 1.2
kill $SHP; wait $SHP 2>/dev/null
"$FB" shell & SHP=$!; PIDS="$PIDS $SHP"
sleep 1.2
[ ! -s "$D/bless" ] || fail "a shell that went away did not restart the interval"
wait $BP || fail "--bless did not finish after the restarted shell"
[ "$(grep -c '^bless' "$D/bless")" = 1 ] || fail "Bless after a restart"
ok "a shell that goes away starts the interval again"

kill $SHP; wait $SHP 2>/dev/null
: > "$D/bless"
KDOS_SLOTS1_BUS=session KDOS_NOTICE_BLESS_SECONDS=2 KDOS_NOTICE_BLESS_GIVEUP=1 \
	"$KN" --bless > "$D/bless.out" 2>&1 && fail "--bless with no shell succeeded"
[ ! -s "$D/bless" ] || fail "Bless with no shell"
kill $SLP; wait $SLP 2>/dev/null
"$FB" shell & SHP=$!; PIDS="$PIDS $SHP"
KDOS_SLOTS1_BUS=session KDOS_NOTICE_BLESS_SECONDS=0.5 "$KN" --bless > "$D/bless.out" 2>&1 ||
	fail "no Slots1 at all is not a failure"
ok "no shell gives up without blessing; no kdos-sysupdated is not an error"
