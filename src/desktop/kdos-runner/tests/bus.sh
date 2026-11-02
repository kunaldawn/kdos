#!/bin/bash
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   bus.sh DIR KDOS-RUNNER — org.kde.krunner1 on a private session bus
#
# A bus of its own whose service directory holds the shipped
# io.kdos.Runner.service with Exec pointed at the binary under test, so the
# first Match starts the runner the way KRunner's first query does. Then
# KRunner's calls in KRunner's order (krunner 6.31 dbusrunner.cpp): Config,
# Actions, Match, SetActivationToken, Run, Teardown, on both paths. A Run
# sent after Teardown, as KRunner on Wayland can, still runs within the
# grace (0.6 s here). A library Match is held open on a slow server while
# the palette keeps answering: the deadline is a second and a half here, so
# a runner that blocked on it could not answer the palette in time. A
# failing kdos verb's error arrives as a Notify on the wire (dbus-monitor;
# nothing serves notifications here). An ask waits out its quiet (0.3 s
# here) before it reaches the fake llama-server, so a question typed on
# goes there once; its answer's row copies through Klipper's
# setClipboardContents, and a question past the reply limit (1.5 s here)
# answers "still answering", whose Run opens KRunner on it again through
# querySingleRunner — both seen on the wire, since neither serves here.
# ---------------------------------
set -u
D=$1 KR=$2
HERE=$(cd "$(dirname "$0")" && pwd)
PORT=$HERE/..
if [ -z "${KR_BUS_INNER:-}" ]; then
	sh "$HERE/tree.sh" "$D" || exit 1
	mkdir -p "$D/services"
	sed "s|^Exec=.*|Exec=$D/kdos-runner-env|" "$PORT/io.kdos.Runner.service" \
		> "$D/services/io.kdos.Runner.service"
	# The bus starts the service with its own environment; this carries
	# the fixture's into it.
	cat > "$D/kdos-runner-env" <<EOF
#!/bin/sh
export HOME=$D/home KDOS_RUNNER_ROOT=$D/root PATH="$D/bin:\$PATH"
export KDOS_RUNNER_KIWIX_PORT=\$(cut -d' ' -f1 "$D/ports" 2>/dev/null)
export KDOS_RUNNER_KOLIBRI_PORT=\$(cut -d' ' -f2 "$D/ports" 2>/dev/null)
export KDOS_RUNNER_LLAMA_PORT=\$(cut -d' ' -f3 "$D/ports" 2>/dev/null)
export KDOS_RUNNER_DEADLINE_MS=1500 KDOS_RUNNER_TORN_MS=600
export KDOS_RUNNER_ASK_QUIET_MS=300 KDOS_RUNNER_ASK_REPLY_MS=1500
exec "$KR" 2> "$D/runner.err"
EOF
	chmod +x "$D/kdos-runner-env"
	cat > "$D/session.conf" <<EOF
<!DOCTYPE busconfig PUBLIC "-//freedesktop//DTD D-Bus Bus Configuration 1.0//EN"
 "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
<busconfig>
  <type>session</type>
  <listen>unix:tmpdir=$D</listen>
  <servicedir>$D/services</servicedir>
  <policy context="default">
    <allow send_destination="*" eavesdrop="true"/>
    <allow eavesdrop="true"/>
    <allow own="*"/>
  </policy>
</busconfig>
EOF
	KR_BUS_INNER=1 exec dbus-run-session --config-file="$D/session.conf" -- bash "$0" "$@"
fi
PIDS=""
trap 'kill $PIDS 2>/dev/null' EXIT
# Milliseconds of the wall clock; bash's own, since busybox date has no %N.
ms() { local t=${EPOCHREALTIME/./}; echo $((10#$t / 1000)); }
fail() { echo "  FAIL  $*"; [ -n "${OUTF:-}" ] && sed 's/^/        /' "$OUTF" | head -30; [ -s "$D/runner.err" ] && sed 's/^/        err: /' "$D/runner.err"; exit 1; }
ok() { echo "  ok    $*"; }
call() { # PATH METHOD ARGS...
	local p=$1 m=$2; shift 2
	OUTF=$D/out
	dbus-send --session --print-reply --dest=io.kdos.Runner "$p" \
		"org.kde.krunner1.$m" "$@" > "$OUTF" 2>&1 || fail "$p $m $*"
}
has() { grep -qF -- "$1" "$OUTF" || fail "$2: no '$1'"; }
hasnt() { ! grep -qF -- "$1" "$OUTF" || fail "$2: '$1' should not be there"; }
waitfile() {
	local i=0
	while [ ! -f "$1" ] && [ $i -lt 100 ]; do sleep 0.05; i=$((i + 1)); done
	[ -f "$1" ]
}
# The id of the row whose text is $1 in the last Match reply.
idof() {
	awk -v t="$1" '/string "k[0-9]+"/ { gsub(/.*string "|"/, ""); id = $0; next }
		index($0, "string \"" t "\"") { print id; exit }' "$D/out"
}

python3 -I "$HERE/fakeserve.py" "$D/ports" & PIDS="$PIDS $!"
i=0; while [ ! -s "$D/ports" ] && [ $i -lt 100 ]; do sleep 0.05; i=$((i + 1)); done
dbus-monitor "type='method_call',interface='org.freedesktop.Notifications',member='Notify'" \
	> "$D/monitor" 2>/dev/null & PIDS="$PIDS $!"
dbus-monitor "type='method_call',member='setClipboardContents'" \
	"type='method_call',member='querySingleRunner'" \
	> "$D/plasma" 2>/dev/null & PIDS="$PIDS $!"

call /runner Config
has 'string "MinLetterCount"' "Config names MinLetterCount"; has "int32 2" "two letters for the palette"
grep -q 'kdos-runner' "$D/runner.err" 2>/dev/null && fail "the runner complained at start"
call /library Config; has "int32 3" "three for the library"
call /runner Actions; has 'string "terminal"' "the Konsole action"
call /library Actions; hasnt 'string "terminal"' "the library offers no action"
ok "activated by the bus; Config and Actions on both paths"

call /runner Match string:ff
has 'string "Firefox ESR"' "ff"; has "int32 100" "the highest category"
has 'string "subtext"' "a subtext property"; has 'string "actions"' "an actions property"
FF=$(idof "Firefox ESR"); [ -n "$FF" ] || fail "no id for Firefox ESR"
call /runner SetActivationToken string:tok-42
call /runner Run string:"$FF" string:
waitfile "$D/ran/kstart" || fail "Run started nothing"
[ "$(tr '\n' ' ' < "$D/ran/kstart")" = "--application firefox-esr " ] || fail "kstart ran with $(cat "$D/ran/kstart")"
[ "$(cat "$D/ran/kstart.token" 2>/dev/null)" = token=tok-42 ] || fail "the activation token"
ok "Match ff, then Run with the activation token: kstart --application firefox-esr"

call /runner Match string:"accent amber"
AM=$(idof "Switch the accent to Amber"); [ -n "$AM" ] || fail "no accent row"
has 'string "terminal"' "a kdos verb offers Konsole"
call /runner Run string:"$AM" string:terminal
waitfile "$D/ran/konsole" || fail "the Konsole action ran nothing"
[ "$(tr '\n' ' ' < "$D/ran/konsole")" = "--hold -e kdos theme amber " ] || fail "konsole ran $(cat "$D/ran/konsole")"
rm -f "$D/ran/"*
call /runner Run string:k999999 string:
call /runner Run string:"$AM" string:"; rm -rf ~"
waitfile "$D/ran/kdos" || fail "an unknown action did not fall back to the default"
[ "$(tr '\n' ' ' < "$D/ran/kdos")" = "theme amber " ] || fail "kdos ran $(cat "$D/ran/kdos")"
[ ! -e "$D/ran/konsole" ] && [ ! -e "$D/ran/kstart" ] || fail "an unknown id ran something"
ok "the Konsole action; an unknown id runs nothing; an unknown action is the default"

call /runner Teardown
rm -f "$D/ran/"*
call /runner Run string:"$AM" string:
waitfile "$D/ran/kdos" || fail "a Run just after Teardown ran nothing"
rm -f "$D/ran/"*
call /runner Teardown
sleep 0.8
call /runner Match string:ff
call /runner Run string:"$AM" string:
sleep 0.3
[ ! -e "$D/ran/kdos" ] || fail "a row outlived Teardown's grace"
ok "a Run that arrives just after Teardown runs; a row past the grace does not"

call /runner Match string:route:fail.theme
FT=$(idof "fail.theme"); [ -n "$FT" ] || fail "no fail.theme row"
call /runner Run string:"$FT" string:
i=0; while ! grep -q 'kdos theme: broken is not an accent' "$D/monitor" && [ $i -lt 100 ]; do sleep 0.05; i=$((i + 1)); done
grep -q 'string "kdos theme: broken is not an accent"' "$D/monitor" || { OUTF=$D/monitor; fail "a failing verb's error was not posted"; }
ok "a failing kdos verb posts its error as a notification"

( dbus-send --session --print-reply --reply-timeout=5000 --dest=io.kdos.Runner /library \
	org.kde.krunner1.Match string:slowly > "$D/slow" 2>&1; ms > "$D/slow.t.tmp"; mv "$D/slow.t.tmp" "$D/slow.t" ) &
sleep 0.05
t0=$(ms)
call /runner Match string:ff
t1=$(ms)
has 'string "Firefox ESR"' "the palette while the library waits"
[ $((t1 - t0)) -lt 200 ] || fail "the palette waited $((t1 - t0)) ms on the library"
waitfile "$D/slow.t" || fail "the slow library Match never came back"
[ $(( $(cat "$D/slow.t") - t0 )) -ge 1300 ] || fail "the slow Match came back before its deadline"
grep -q 'string "slowly in wikipedia' "$D/slow" || { OUTF=$D/slow; fail "the deadline answer lost the fast server's rows"; }
ok "a library Match held on a slow server does not hold the palette"

# The first library Match started the book index in a child.
waitfile "$D/home/.cache/kdos/runner-books" || fail "the book index was never built"
call /library Match string:?photosynthesis
has 'string "Photosynthesis & light"' "a kiwix row on the bus"
has 'string "urls"' "a URL property"
has 'string "Photosynthesis basics"' "a Kolibri row"
has 'string "3.1 Photosynthesis of slots & trees"' "a book heading"
call /runner Match string:?photosynthesis
hasnt 'string "Photosynthesis' "the palette leaves ? to the library"
call /library Match string:ff
hasnt 'Firefox' "the library has no favourites"
ok "library rows from every source on the bus; each path keeps to its side"

( dbus-send --session --print-reply --reply-timeout=5000 --dest=io.kdos.Runner /runner \
	org.kde.krunner1.Match string:"ask wh" > "$D/typed" 2>&1; ms > "$D/typed.t.tmp"; mv "$D/typed.t.tmp" "$D/typed.t" ) &
sleep 0.1
t0=$(ms)
call /runner Match string:"ask what is kdos"
has 'string "The answer to “what is kdos” is 42."' "the model's answer on the bus"
has 'string "multiline"' "an answer shows on several lines"
has 'string "copy"' "an answer offers Copy"
waitfile "$D/typed.t" || fail "the Match of a question typed on never came back"
[ $(( $(cat "$D/typed.t") - t0 )) -lt 250 ] || fail "a question typed on held its Match"
! grep -q 'string "Still answering\|string "The answer' "$D/typed" || { OUTF=$D/typed; fail "a question typed on was answered"; }
! grep -q "q=wh$" "$D/ports.log" || { OUTF=$D/ports.log; fail "a question typed on reached the model"; }
AN=$(idof "The answer to “what is kdos” is 42."); [ -n "$AN" ] || fail "no id for the answer"
call /runner Run string:"$AN" string:copy
i=0; while ! grep -q 'setClipboardContents' "$D/plasma" && [ $i -lt 100 ]; do sleep 0.05; i=$((i + 1)); done
grep -A2 'setClipboardContents' "$D/plasma" | grep -q 'string "The answer to “what is kdos” is 42."' ||
	{ OUTF=$D/plasma; fail "the answer did not reach Klipper"; }
ok "ask: one question after the typing stops, its answer copied through Klipper"

t0=$(ms)
call /runner Match string:"ask slowly"
t1=$(ms)
has 'string "Still answering: slowly"' "an answer past the reply limit"
[ $((t1 - t0)) -ge 1300 ] && [ $((t1 - t0)) -lt 2500 ] || fail "the still-answering reply took $((t1 - t0)) ms"
SL=$(idof "Still answering: slowly"); [ -n "$SL" ] || fail "no id for the still-answering row"
call /runner Run string:"$SL" string:
i=0; while ! grep -q 'querySingleRunner' "$D/plasma" && [ $i -lt 100 ]; do sleep 0.05; i=$((i + 1)); done
grep -A3 'querySingleRunner' "$D/plasma" | grep -q 'string "ask slowly"' ||
	{ OUTF=$D/plasma; fail "KRunner was not asked again"; }
sleep 2.5
t0=$(ms)
call /runner Match string:"ask slowly"
t1=$(ms)
has 'string "The answer to “slowly” is 42."' "the answer that came after its Match"
[ $((t1 - t0)) -lt 250 ] || fail "a kept answer took $((t1 - t0)) ms"
[ "$(grep -c "q=slowly$" "$D/ports.log")" = 1 ] || { OUTF=$D/ports.log; fail "the question was sent twice"; }
ok "ask: still answering by the reply limit, KRunner asked again, the late answer kept"
