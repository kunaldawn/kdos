#!/bin/bash
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   run.sh DIR MATCHTEST — kdos-runner's matcher and its vectors, no bus
#
# Every source the palette and the library read, from tree.sh's fixtures;
# each executed vector reaches a stub that records it element by element,
# so a hostile word is seen arriving as one literal argument; the library's
# servers and the local model are fakeserve.py on loopback. Prints one `ok` line per check and
# exits non-zero at the first that fails.
# ---------------------------------
set -u
D=$1 MT=$2
HERE=$(cd "$(dirname "$0")" && pwd)
sh "$HERE/tree.sh" "$D" || exit 1
export HOME=$D/home KDOS_RUNNER_ROOT=$D/root PATH="$D/bin:$PATH"
unset XDG_CONFIG_HOME XDG_DATA_HOME XDG_CONFIG_DIRS XDG_DATA_DIRS XDG_CACHE_HOME QT_PLUGIN_PATH

# Milliseconds of the wall clock; bash's own, since busybox date has no %N.
ms() { local t=${EPOCHREALTIME/./}; echo $((10#$t / 1000)); }
fail() { echo "  FAIL  $*"; [ -n "${OUTF:-}" ] && sed 's/^/        /' "$OUTF" | head -40; exit 1; }
ok() { echo "  ok    $*"; }
q() { OUTF=$D/out.txt; "$MT" query "$1" > "$OUTF" 2>&1 || fail "matchtest query '$1' failed"; }
lib() { OUTF=$D/out.txt; "$MT" library "$1" "${2:-2000}" > "$OUTF" 2>&1 || fail "matchtest library '$1' failed"; }
ask() { OUTF=$D/out.txt; "$MT" ask "$1" "${2:-5000}" > "$OUTF" 2>&1 || fail "matchtest ask '$1' failed"; }
has() { grep -qF -- "$1" "$OUTF" || fail "$2: no '$1'"; }
hasnt() { ! grep -qF -- "$1" "$OUTF" || fail "$2: '$1' should not be there"; }
first() { [ "$(head -1 "$OUTF")" = "$1" ] || fail "$2: first row is '$(head -1 "$OUTF")', not '$1'"; }
# Wait for a stub to have recorded its vector; the watcher is detached.
ran() {
	local i=0
	while [ ! -f "$D/ran/$1" ] && [ $i -lt 100 ]; do sleep 0.05; i=$((i + 1)); done
	[ -f "$D/ran/$1" ] || fail "$1 was never run"
}
argv_is() { # STUB LINE...
	local s=$1; shift
	printf '%s\n' "$@" > "$D/want"
	diff -u "$D/want" "$D/ran/$s" > "$D/diff" || { OUTF=$D/diff; fail "$s ran with another vector"; }
}

# ── two-letter codes ───────────────────────────────────────────────────
q ff
first "Firefox ESR" "ff"
has "cat Favourites 100 1.00" "ff is the highest category"
has "  [kstart]" "ff launches through kstart"
has "  [firefox-esr]" "ff names its entry"
q FM
first "Dolphin" "a code is case-blind both ways"
q im; hasnt "GIMP" "a user's Hidden=true entry has no code"
q ni; hasnt "not-installed" "an uninstalled entry has no code"
q to; hasnt "Kate" "a code of the wrong length is no code"
ok "two-letter codes: case-blind, hidden and missing entries skipped"

# ── verbs ──────────────────────────────────────────────────────────────
q "accent amber"
first "Switch the accent to Amber" "accent amber"
has "  sub kdos theme amber" "the subtext is the command"
has "+term" "a background kdos verb offers Konsole"
q accent
[ "$(grep -c '^Switch the accent to' "$OUTF")" -ge 8 ] || fail "accent alone offers every accent"
q "acc bo"; first "Switch the accent to Bone" "a verb typed in part"
q "accent nosuch"; hasnt "Switch the accent" "an accent that is not one"
q "accent next"; first "Switch the accent to the next one" "accent next"; has "  sub kdos theme next" "accent next runs kdos theme next"
q "accent pr"; has "Switch the accent to the previous one" "accent prev in part"; has "  [prev]" "accent prev's vector"
q "phosphor on"; has "  [phosphor]" "phosphor on"; has "  [on]" "phosphor on"
q "tiling of"; first "Automatic tiling off" "a slot typed in part"
q "desk dock"; first "Lay the desktop out as the dock desk" "desk dock"
q "night light"; first "Turn Night Light on or off" "night light"; has "  [night-light]" "night light"
q "reduced motion"; first "Reduced motion on or off" "reduced motion"; has "  sub kdos toggle reduced-motion" "reduced motion's vector"
q "up ch"; first "Check for a system update" "update check in part"; has " term" "update check runs in Konsole"
q "doctor"; has "  [doctor]" "doctor"
q "box stop"; [ "$(grep -c '^Stop the box' "$OUTF")" = 3 ] || fail "box stop lists the three profiles"
q "box stop ww"; first "Stop the box www" "box stop www"; hasnt "not-a-profile" "a file that is not a profile"
q "box enter dev"; has "  [enter]" "box enter dev"; has "  [--window]" "box enter in its own accent window"
q "install gimp"; first "Find “gimp” in Discover" "install gimp"; has "  [--search]" "install searches Discover"
q "in"; hasnt "in Discover" "a free-text verb needs its whole word"
q "why /etc/kdos/menu.conf"; has "  [/etc/kdos/menu.conf]" "why takes the rest as one element"
q "lib desk"; first "Search the offline library alone for “desk”" "lib WORDS"
has " requery" "lib asks KRunner again"; has "  [kdos-library]" "lib names the library's runner"; has "  [desk]" "lib passes the words whole"
q "li desk"; hasnt "offline library alone" "lib needs its whole word"
ok "verbs, slots, partial words, free text"

# ── routes, chords; no settings pages ──────────────────────────────────
q "firewall"; hasnt "kcm_kdos_firewall" "System Settings' own runner has the KDOS pages"
q "previous version"; hasnt "kcm_kdos_system" "no second System Versions row"
q "net setup"; has "Set up network" "a route by its words in any order, titled from its name"
has "  sub route:setup.network" "a route row teaches its name"; hasnt "  sub systemsettings" "a route row hides its command"
q "route:setup.sound"; first "Set up sound" "route:NAME"; has "  [pavucontrol-qt]" "the user's route replaces the system's"
hasnt "kcm_pulseaudio" "the system's setup.sound is replaced"
q "toplevel"; hasnt "Network Sound" "an @ setting is not a route"
q "bad"; hasnt "should-not-appear" "a route name with a space"
q "keys tile"
has "Toggle Tiles Editor: Meta+T" "a chord from kglobalshortcutsrc"
q "keys toggle tiling"
has "Krohnkite: Toggle tiling: Meta+Shift+T, Meta+Alt+T" "alternatives joined"
q "keys krunner"; has "KRunner: Meta+Space, Alt+Space, Alt+F2, Search" "the krunner chord"
q "keys launch konsole"; has "Launch Konsole: Ctrl+Alt+T" "a service group's chord"
q "keys region"; has "Capture Rectangular Region: Meta+Shift+Print, Meta+Shift+S" "a default from /usr/share/kglobalaccel"
q "tile"; hasnt "Meta+T" "chords answer only after keys"
ok "routes merged by name and titled, chords from three places"

# ── install from the boot medium ───────────────────────────────────────
q gimp; hasnt "on medium" "a medium pack answers only after install"
q "install gimp"; has "app.gimp (on medium)" "install NAME finds a pack on the medium"; has "Find “gimp” in Discover" "beside the search"
has "  [--local-filename]" "Discover opens the file"; has "  [$D/root/mnt/iso/packs/app.gimp.kpack]" "the pack's path"
q "install office"; has "office (on medium)" "a .ktar set"; has "Install this set from the boot medium" "said to be a set"
q "install readme"; hasnt "on medium" "a file that is neither"
q "install bad"; hasnt "bad name" "a file name that is not a plain name"
KDOS_PACK_MEDIUM=$D/none q "install gimp"; hasnt "on medium" "\$KDOS_PACK_MEDIUM moves the medium, as it does Discover's"
ok "packs and sets on the boot medium open in Discover"


# ── execution: vectors, never a shell ──────────────────────────────────
"$MT" run "accent amber" 0 || fail "run accent amber"
ran kdos; argv_is kdos theme amber
rm -f "$D/ran/"*
"$MT" run "route:evil.semicolon" 0 || fail "run evil.semicolon"
ran kdos; argv_is kdos why ";touch\${IFS}$D/pwned"
rm -f "$D/ran/"*
"$MT" run "route:evil.subst" 0 || fail "run evil.subst"
ran kdos; argv_is kdos why "\$(touch" "$D/pwned)" "\`touch" "$D/pwned\`"
rm -f "$D/ran/"*
"$MT" run 'why $(touch '"$D"'/pwned);x' 0 || fail "run why hostile"
ran konsole; argv_is konsole --hold -e kdos why "\$(touch $D/pwned);x"
sleep 0.3
[ ! -e "$D/pwned" ] || fail "a hostile word was run by a shell"
rm -f "$D/ran/"*
"$MT" run "doctor" 0 term || fail "run doctor"
ran konsole; argv_is konsole --hold -e kdos doctor
rm -f "$D/ran/"*
"$MT" run "ff" 0 || fail "run ff"
ran kstart; argv_is kstart --application firefox-esr
[ "$(cat "$D/ran/kstart.token" 2>/dev/null)" = token=test-token-1 ] || fail "the activation token did not reach the child"
ok "every vector arrives element by element; nothing reaches a shell"

# ── the library ────────────────────────────────────────────────────────
lib "kdos-box"
has "kdos-box(1)" "a manual page"; has "  [man:/kdos-box(1)]" "a man: URL"
has "  sub KDOS from Scratch" "a KFS heading"; hasnt "ls(1)" "only kdos manual pages"
lib "photosynthesis"
has "3.1 Photosynthesis of slots & trees" "an h3 with its entities"
has "file://$D/root/usr/share/doc/kdos/02-user-guide/updates.html#photosynthesis-of-slots" "the heading's anchor"
has "cat Offline library 30" "a plain query ranks the library low"
lib "too deep"; hasnt "Too deep" "h4 is not indexed"
lib "?updating"; hasnt "Offline library 100" "a ? is no prefix of the library's"
lib "48.8584, 2.2945"; first "Show 48.85840, 2.29450 on the map" "a coordinate"; has "  [geo:48.858400,2.294500]" "a geo: link"
lib "geo:91,0"; hasnt "on the map" "a latitude past 90"
[ -s "$D/home/.cache/kdos/runner-books" ] || fail "the book index cache was not written"
mv "$D/root/usr/share/doc/kdos/02-user-guide/updates.html" "$D/root/usr/share/doc/kdos/02-user-guide/upd.html"
lib "photosynthesis"; has "02-user-guide/updates.html#" "the cache stands until the package changes"
mkdir -p "$D/root/var/lib/kpkg/db"; echo "kdos-doc 1" > "$D/root/var/lib/kpkg/db/kdos-doc"
lib "photosynthesis"; has "02-user-guide/upd.html#" "a changed book rebuilds the cache"
ok "books with anchors, manual pages, places, the index cache"

if [ "$("$MT" zeal)" = yes ] && [ -s "$D/home/.local/share/Zeal/Zeal/docsets/Python_3.docset/Contents/Resources/docSet.dsidx" ]; then
	lib "os.path"
	first "os.path" "the exact name first"
	has "  sub Python 3 · Module" "the docset's name and the entry's type"
	has "  [python:os.path.join]" "Zeal's own key"
	has "os.pathology" "a Core Data docset"
	lib "os_"
	has "os_path_like%" "a LIKE wildcard typed is a character"
	hasnt "os.path.join" "an underscore is not a wildcard"
	lib "zi"; hasnt "zipfile" "two letters ask no docset"
	ok "Zeal docsets, Dash and Core Data, by prefix"
else
	echo "  skip  Zeal docsets (this build has no SQLite, or python3 cannot write one)"
fi

python3 -I "$HERE/fakeserve.py" "$D/ports" &
FS=$!
trap 'kill $FS 2>/dev/null' EXIT
i=0; while [ ! -s "$D/ports" ] && [ $i -lt 100 ]; do sleep 0.05; i=$((i + 1)); done
read -r KW KO LL < "$D/ports" || fail "the fake servers did not start"
export KDOS_RUNNER_KIWIX_PORT=$KW KDOS_RUNNER_KOLIBRI_PORT=$KO KDOS_RUNNER_LLAMA_PORT=$LL
lib "photosynthesis"
has "Photosynthesis & light" "a kiwix suggestion, entities decoded"
has "  sub Wikipedia & friends" "the book's title"
has "url http://127.0.0.1:$KW/content/wikipedia_en_all_maxi_2024-01/A/Photosynthesis" "the article URL"
has "content/wiktionaryplusfr/A/Odd%20path/%C3%A9%3F%23" "a path escaped, + routed as plus"
hasnt "containing" "the full-text pattern is not a row"
has "Photosynthesis basics" "a Kolibri resource"
has "/en/learn/#/topics/c/a3f1c2" "a Kolibri resource URL"
has "/en/learn/#/topics/t/b7e9" "a Kolibri topic URL"
hasnt "Not an id" "a Kolibri id that is not hex"
grep -q "^kiwix /suggest?content=wikipedia_en_all_maxi_2024-01&term=photosynthesis&count=4$" "$D/ports.log" ||
	{ OUTF=$D/ports.log; fail "the kiwix request"; }
t0=$(ms)
lib "slowly" 300
t1=$(ms)
[ $((t1 - t0)) -lt 1500 ] || fail "a slow server held the answer past its deadline"
has "slowly in wikipedia" "the fast server's rows survive a slow one"

ask "ask what is   kdos"
first "The answer to “what is kdos” is 42." "the model's answer is the first row"
has "  sub small · Enter copies the answer" "the loaded model, named"
has "cat Local model 100" "an ask ranks first"
has "  [The answer to “what is kdos” is 42.]" "the answer is what Enter copies"
hasnt "think>" "a reasoning block is not part of the answer"
grep -q "^llama GET /v1/models$" "$D/ports.log" || { OUTF=$D/ports.log; fail "the models request"; }
grep -q "^llama POST /v1/chat/completions model=small stream=False q=what is kdos$" "$D/ports.log" ||
	{ OUTF=$D/ports.log; fail "the question, the loaded model, not streamed"; }
ask 'ask say "hi" \ there'
grep -qF 'q=say "hi" \ there' "$D/ports.log" || { OUTF=$D/ports.log; fail "quotes and a backslash reach the model as typed"; }
ask "ask broken"
first "The local model did not answer" "a server error"; has "the model failed to load" "the server's own message"
has "  [ask broken]" "Enter asks again"
ask "ask silent"; has "The model gave no answer" "an empty answer is no answer"
KDOS_RUNNER_ASK_TIMEOUT_MS=2000 ask "ask slowly" 6000
has "No answer in 2 seconds" "the exchange's timeout"
ask "ask slowly" 400
first "Still answering: slowly" "an answer still coming"; has "  [ask slowly]" "Enter asks again"
for t in "ask" "ask   " "asking about" "task list"; do
	ask "$t"; hasnt "Local model" "'$t' is no question"
done
ok "ask: a model chosen, the question sent once, failures and a timeout said"
kill $FS 2>/dev/null; wait $FS 2>/dev/null
lib "photosynthesis" 300
hasnt "Photosynthesis basics" "no server, no rows"; has "3.1 Photosynthesis" "and the local rows still"
ask "ask anything"; first "No local model is answering" "no llama-server"
ok "kiwix-serve and Kolibri over loopback, a deadline, no server at all"

# ── speed ──────────────────────────────────────────────────────────────
for t in ff "accent" "keys tile" "net"; do
	us=$("$MT" time "$t" 50)
	[ "$us" -lt 50000 ] || fail "'$t' took ${us}us; KRunner's budget is 50 ms"
done
ok "every palette match under 50 ms with warm caches"

# ── hostile input ──────────────────────────────────────────────────────
{
	printf '%s\n' '' '?' '??' 'route:' 'route:../../etc' 'keys' 'box stop ../../x'
	head -c 3000 /dev/zero | tr '\0' 'a'; echo
	printf 'accent %s\n' "$(head -c 600 /dev/zero | tr '\0' 'b')"
	printf '[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[\n'
	printf '&#xFFFFFFFF; &#0; &#55296; &amp &lt;b&gt;\n'
	printf '"\\u0000\\ud800\\udc00"\n'
} > "$D/fuzz"
"$MT" fuzz "$D/fuzz" > /dev/null 2>&1 || fail "the fuzz lines crashed the matcher"
ok "empty, oversized, nested and malformed input"
