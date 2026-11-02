#!/bin/bash
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   run.sh DIR DECIDETEST — kdos-notice's decisions on fixture JSON, no bus
#
# The fixtures are shaped as `kdos restarts --json`, `kdos cve --json` and
# `kdos-bootctl status --json` print them. Each step is one look at the
# facts, and the state file carries what was posted from one step to the
# next, so "exactly one notification per change of state" is checked as a
# sequence: the same facts twice post once, changed facts replace, facts
# that end withdraw. Prints one `ok` line per group.
# ---------------------------------
set -u
D=$1 DT=$2
HERE=$(cd "$(dirname "$0")" && pwd)
F=$HERE/fixtures
rm -rf "$D"
mkdir -p "$D/state" "$D/data/applications"
export XDG_STATE_HOME=$D/state XDG_DATA_HOME=$D/data XDG_DATA_DIRS=$D/none
export KDOS_NOTICE_BOOT_ID=boot-1

fail() { echo "  FAIL  $*"; sed 's/^/        /' "$D/out" | head -30; exit 1; }
ok() { echo "  ok    $*"; }
step() { "$DT" "$@" > "$D/out" 2>&1 || fail "decidetest $*"; }
has() { grep -qF -- "$1" "$D/out" || fail "$2: no '$1'"; }
hasnt() { ! grep -qF -- "$1" "$D/out" || fail "$2: '$1' should not be there"; }

printf '[Desktop Entry]\nType=Application\nName=Firefox ESR\nExec=firefox-esr %%u\n' \
	> "$D/data/applications/firefox-esr.desktop"
printf '[Desktop Entry]\nType=Application\nName=Konsole\nExec=konsole\n' \
	> "$D/data/applications/org.kde.konsole.desktop"
printf '[Desktop Entry]\nType=Application\nName=Hidden\nExec=mpd\nHidden=true\n' \
	> "$D/data/applications/mpd.desktop"

# ── restarts ───────────────────────────────────────────────────────────
step restarts "$F/restarts-apps.json"
has "proc 2841 firefox-esr name=Firefox ESR" "an application by its exe"
has "proc 3300 mpd name= desktop=0" "a hidden entry names nothing"
has "post restarts replaces=0 persistent=1" "the first look posts, persistently"
has "summary Firefox ESR and Konsole were updated" "two applications, each once"
has "Some background programs" "the background program is mentioned"
has "action details Show Programs" "Show Programs"
step restarts "$F/restarts-apps.json"
has "none restarts" "the same processes again post nothing"
step restarts "$F/restarts-desktop.json"
has "post restarts replaces=1" "a changed set replaces the notification"
has "summary The desktop was updated" "the session's own programs"
has "Log out and back in" "the desktop asks for a new login"
has "action logout Log Out" "Log Out"
has "Konsole was updated too." "the applications are still named"
KDOS_NOTICE_BOOT_ID=boot-2 step restarts "$F/restarts-desktop.json"
has "post restarts" "the same pids on another boot are news"
step restarts "$F/restarts-background.json"
has "summary Background programs were updated" "only background programs"
step restarts "$F/restarts-none.json"
has "close restarts" "nothing left running old code withdraws it"
step restarts "$F/restarts-none.json"
has "none restarts" "and withdraws it once"
step restarts "$D/missing.json"
has "none restarts" "a command that did not answer changes nothing"
ok "restarts: once per set, the desktop asks for a login, withdrawn at the end"

# ── CVE ────────────────────────────────────────────────────────────────
step cve "$F/cve-3.json"
has "cves 3 in 2" "three ids in two packages"
has "summary 3 known vulnerabilities in installed packages" "the count"
has "body In curl and expat. An update fixes every one of them." "the packages"
has "persistent=1" "a CVE notice waits for a person"
step cve "$F/cve-3.json"
has "none cve" "the same findings at the next login"
step cve "$F/cve-2.json"
has "summary 2 known vulnerabilities" "fewer findings replace it"
step cve "$D/missing.json"
has "none cve" "a missing secdb withdraws nothing"
step cve "$F/cve-none.json"
has "close cve" "no findings withdraw it"
ok "cve: once per set of findings, replaced, withdrawn"

# ── the trial and the rollback ─────────────────────────────────────────
step boot "$F/boot-trial.json"
has "boot ok=1 running=b active=a trying=b failed= a=0.2.13 b=0.2.14" "the status parsed"
has "none rollback" "a trial is not a rollback"
has "summary Running KDOS 0.2.14 on trial" "the trial"
has "goes back to KDOS 0.2.13" "where it goes back to"
has "persistent=0 urgency=0" "a trial notice is calm and expires"
step boot "$F/boot-trial.json"
has "none trial" "once per boot"
KDOS_NOTICE_BOOT_ID=boot-2 step boot "$F/boot-trial.json"
has "post trial" "a second trial boot says it again"
KDOS_NOTICE_BOOT_ID=boot-2 step boot "$F/boot-kept.json"
has "close trial" "the kept trial withdraws its notice"
KDOS_NOTICE_BOOT_ID=boot-3 step boot "$F/boot-kept.json"
has "none trial" "a kept slot says nothing"
step boot "$F/boot-rolledback.json"
has "summary Your last update didn't start" "the rollback"
has "body KDOS 0.2.14 did not start properly, so KDOS went back to 0.2.13." "versions named"
KDOS_NOTICE_BOOT_ID=boot-4 step boot "$F/boot-rolledback.json"
has "none rollback" "a rollback is said once, not every boot"
step boot "$F/boot-unconfigured.json"
has "boot ok=0" "one root, no slots"
has "none trial" "nothing about slots on a machine without them"
ok "trial once per boot and withdrawn when kept; rollback once per failed slot"

# ── memory kills ───────────────────────────────────────────────────────
step oom "ok trigger 'some 150000 1000000', kills 3, last: firefox-esr (appbox www) (pid 9, 812 MB)" 2
has "kills=3" "the count parsed"
has "summary firefox-esr in the box www was closed to free memory" "the box named"
has "using 812 MB" "its size"
step oom "ok trigger 'x', kills 3, last: firefox-esr (pid 9, 812 MB)" 3
has "none oom" "the same count is no kill"
step oom "ok trigger 'x', kills 5, last: gimp (pid 12, 2048 MB)" 3
has "summary 2 programs were closed to free memory" "two kills between looks"
has "The last was gimp." "the last named"
step oom "err not permitted" 0
has "none oom" "a refused socket says nothing"
ok "oom: per kill, the box named, a refusal is silence"

# ── the installer's pending applications ──────────────────────────────
printf '# chosen at install\ngroup office\napp.gimp\napp.kcalc\n' > "$D/pending"
step pending "$D/pending"
has "pending 2" "two ids; a group line and a comment are not counted"
has "post pending replaces=0 persistent=1" "the first look posts, persistently"
has "summary 2 applications chosen during install are not installed yet" "the count"
has "action details Open Discover" "Open Discover"
step pending "$D/pending"
has "none pending" "the same file at the next look"
printf 'app.kcalc\n' > "$D/pending"
step pending "$D/pending"
has "summary An application chosen during install is not installed yet" "one left replaces it"
: > "$D/pending"
step pending "$D/pending"
has "close pending" "an empty file withdraws it"
step pending "$D/missing"
has "none pending" "and withdraws it once"
step action pending details
[ "$(tr '\n' ' ' < "$D/out")" = "[plasma-discover] [--category] [Selected during install] " ] \
	|| fail "pending details"
ok "pending: said once per selection, withdrawn when Discover has installed it"

# ── actions ────────────────────────────────────────────────────────────
step action cve details
[ "$(tr '\n' ' ' < "$D/out")" = "[konsole] [--hold] [-e] [kdos] [cve] " ] || fail "cve details"
step action trial default
has "[kcm_kdos_system]" "a click on the trial notice"
step action cve '; rm -rf ~'
has "no action" "an action id that is not offered"
ok "actions: fixed vectors, nothing taken from the server"

# ── hostile documents ──────────────────────────────────────────────────
printf '{"processes": [%s' "$(head -c 20000 /dev/zero | tr '\0' '[')" > "$D/deep.json"
step restarts "$D/deep.json"; has "none restarts" "deep nesting"
printf '{"configured": true, "running": "../../x", "trying": "../../x", "slots": "a"}' > "$D/bad.json"
step boot "$D/bad.json"; has "running= " "a slot that is not a or b"; has "none trial" "no trial from a bad slot"
printf '{"findings": [{"port": "x", "cves": "%s"}]}' "$(head -c 5000 /dev/zero | tr '\0' ',')" > "$D/commas.json"
step cve "$D/commas.json"; has "cves 1 in 1" "an empty id list counts the finding once"
ok "nested, malformed and oversized documents"
