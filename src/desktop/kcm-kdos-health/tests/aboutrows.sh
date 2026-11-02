#!/bin/bash
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   aboutrows.sh — About this System's six rows, on a bare host
#
#   aboutrows.sh <scratch directory>
#
# Run by testing/selftest.sh from the repository root. Each about-* script is
# run under the conditions that make one print the wrong number of lines —
# no environment at all, the English pass, a root with no /run/kdos, no tree
# stamp and no package database, a read-only /var, and state files written
# to break it — and must exit 0 with exactly two lines in under 100 ms. Then
# each row's value is held to its fixture, and the shipped
# kcm-about-distrorc must name exactly the scripts this port installs.

set -u
A=src/desktop/kcm-kdos-health/about
S=${1:?usage: aboutrows.sh <scratch>}
RC_FILE=fs/usr/share/kdos/xdg/kcm-about-distrorc
LIMIT_MS=100
fail=0
worst=0

now_ms() {
	local e=${EPOCHREALTIME//[.,]/}
	echo "${e%???}"
}

# shape <script> <what> [env...]: two lines, exit 0, in time. The shape is
# held on each of three runs and the time is the fastest of them: a script
# that is slow is slow every time, while one run stalled by a loaded host
# would otherwise fail a script that is not.
shape() {
	local s=$1 what=$2 t0 ms best=-1 rc lines nonempty i
	shift 2
	for i in 1 2 3; do
		t0=$(now_ms)
		env -i "$@" "$A/$s" >"$S/out" 2>/dev/null
		rc=$?
		ms=$(($(now_ms) - t0))
		{ [ "$best" -lt 0 ] || [ "$ms" -lt "$best" ]; } && best=$ms
		lines=$(wc -l <"$S/out")
		nonempty=$(grep -c . "$S/out")
		if [ "$rc" -ne 0 ] || [ "$lines" -ne 2 ] || [ "$nonempty" -ne 2 ]; then
			echo "  FAIL $s ($what): exit $rc, $lines line(s), $nonempty non-empty"
			sed 's/^/    | /' "$S/out"
			fail=1
			return
		fi
	done
	ms=$best
	[ "$ms" -gt "$worst" ] && worst=$ms
	if [ "$ms" -ge "$LIMIT_MS" ]; then
		echo "  FAIL $s ($what): $ms ms, the limit is $LIMIT_MS"
		fail=1
	fi
}

# value <script> <expected value line> [env...]
value() {
	local s=$1 want=$2 got
	shift 2
	got=$(env -i "$@" "$A/$s" 2>/dev/null | sed -n 2p)
	if [ "$got" != "$want" ]; then
		echo "  FAIL $s: got \"$got\", want \"$want\""
		fail=1
	fi
}

rm -rf "$S"
mkdir -p "$S/empty" "$S/home-empty" "$S/r/run/kdos" "$S/r/etc/kdos" "$S/r/ports" \
	"$S/r/var/lib/kpkg/db" "$S/r/var/lib/kdos" "$S/r/usr/share/kdos/xdg" "$S/r/proc" \
	"$S/h/.config/kdos/boxes"

for s in about-slot about-accent about-tree about-libc about-boxes about-update; do
	[ -x "$A/$s" ] || { echo "  FAIL $s is not an executable in $A"; fail=1; continue; }
	sh -n "$A/$s" || { echo "  FAIL $s does not parse"; fail=1; continue; }
	shape "$s" "no environment"
	shape "$s" "English pass" LANGUAGE=en_US:C HOME="$S/home-empty"
	shape "$s" "empty root" KDOS_ABOUT_ROOT="$S/empty" HOME="$S/home-empty"
done

# ── the slot ─────────────────────────────────────────────────────────────
printf 'NAME="KDOS"\nVERSION_ID="0.3"\n' >"$S/r/etc/os-release"
printf '{\n  "slot": "a",\n  "version": "0.2.14",\n  "state": "confirmed",\n  "previous_slot": "b",\n  "previous_version": "0.2.13"\n}\n' >"$S/r/run/kdos/slot.json"
value about-slot "A · 0.2.14 · confirmed (previous: B · 0.2.13)" KDOS_ABOUT_ROOT="$S/r"
printf '{"slot":"b","version":"0.2.15","state":"trial","previous_slot":"a","previous_version":"0.2.14"}' >"$S/r/run/kdos/slot.json"
value about-slot "B · 0.2.15 · on trial (previous: A · 0.2.14)" KDOS_ABOUT_ROOT="$S/r"
printf '{"slot": "a", "state": "confirmed"}\n' >"$S/r/run/kdos/slot.json"
value about-slot "A · 0.3 · confirmed" KDOS_ABOUT_ROOT="$S/r"
printf '{"state": "single"}\n' >"$S/r/run/kdos/slot.json"
value about-slot "One root · 0.3" KDOS_ABOUT_ROOT="$S/r"
printf 'not json at all\n' >"$S/r/run/kdos/slot.json"
value about-slot "Unknown" KDOS_ABOUT_ROOT="$S/r"
printf '{"slot": "a\\nB", "state": "confirmed", "version": "$(reboot)"}\n' >"$S/r/run/kdos/slot.json"
value about-slot "Unknown" KDOS_ABOUT_ROOT="$S/r"
shape about-slot "hostile slot.json" KDOS_ABOUT_ROOT="$S/r"
value about-slot "Unknown" KDOS_ABOUT_ROOT="$S/empty"
# With no slot.json the boot entry's kdos_slot= names the slot, and only a
# or b does; a word that is a pattern stays a word.
printf 'BOOT_IMAGE=/EFI/kdos/b/vmlinuz root=UUID=9b2e kdos_slot=b quiet *\n' >"$S/r/proc/cmdline"
value about-slot "Unknown" KDOS_ABOUT_ROOT="$S/r"
rm "$S/r/run/kdos/slot.json"
value about-slot "B · 0.3" KDOS_ABOUT_ROOT="$S/r"
mkdir -p "$S/glob" && : >"$S/glob/kdos_slot=a"
here=$PWD
got=$(cd "$S/glob" && env -i KDOS_ABOUT_ROOT="$S/r" "$here/$A/about-slot" 2>/dev/null | sed -n 2p)
[ "$got" = "B · 0.3" ] || { echo "  FAIL about-slot: a word of the command line was expanded: \"$got\""; fail=1; }
printf 'root=/dev/vda1 kdos_slot=c kdos_slot=$(reboot)\n' >"$S/r/proc/cmdline"
value about-slot "Unknown" KDOS_ABOUT_ROOT="$S/r"
printf 'root=/dev/vda1 quiet\n' >"$S/r/proc/cmdline"
value about-slot "Unknown" KDOS_ABOUT_ROOT="$S/r"
shape about-slot "boot entry only" KDOS_ABOUT_ROOT="$S/r"

# ── the tree ─────────────────────────────────────────────────────────────
printf 'kdos-tree 1\ncommit 0123456789abcdef0123456789abcdef01234567\ntime 1735689600\n' >"$S/r/ports/TREE"
value about-tree "0123456789ab · 2025-01-01" KDOS_ABOUT_ROOT="$S/r"
printf 'kdos-tree 1\ncommit 0123456789abcdef\n' >"$S/r/ports/TREE"
value about-tree "0123456789ab" KDOS_ABOUT_ROOT="$S/r"
printf 'kdos-tree 1\ncommit $(reboot)\ntime 1\n' >"$S/r/ports/TREE"
value about-tree "Unknown" KDOS_ABOUT_ROOT="$S/r"
value about-tree "Unknown" KDOS_ABOUT_ROOT="$S/empty"

# ── the accent ───────────────────────────────────────────────────────────
printf 'amber' >"$S/h/.config/kdos/accent"
printf 'amber' >"$S/r/etc/kdos/accent"
value about-accent "Amber" KDOS_ABOUT_ROOT="$S/r" HOME="$S/h"
printf 'bone' >"$S/r/etc/kdos/accent"
value about-accent "Amber (login screen: Bone)" KDOS_ABOUT_ROOT="$S/r" HOME="$S/h"
mkdir -p "$S/xdg/kdos"
printf 'ice\n' >"$S/xdg/kdos/accent"
value about-accent "Ice (login screen: Bone)" KDOS_ABOUT_ROOT="$S/r" HOME="$S/h" XDG_CONFIG_HOME="$S/xdg"
rm "$S/h/.config/kdos/accent"
printf '[General]\nColorScheme=KDOSNorton\n' >"$S/h/.config/kdeglobals"
value about-accent "Norton (login screen: Bone)" KDOS_ABOUT_ROOT="$S/r" HOME="$S/h"
printf '[General]\nColorScheme=BreezeDark\n' >"$S/h/.config/kdeglobals"
printf '[General]\nColorScheme=KDOSPaper\n' >"$S/r/usr/share/kdos/xdg/kdeglobals"
value about-accent "Bone" KDOS_ABOUT_ROOT="$S/r" HOME="$S/h"
rm "$S/h/.config/kdeglobals"
value about-accent "Paper (login screen: Bone)" KDOS_ABOUT_ROOT="$S/r" HOME="$S/h"
printf 'bone; reboot' >"$S/r/etc/kdos/accent"
value about-accent "Paper" KDOS_ABOUT_ROOT="$S/r" HOME="$S/h"
value about-accent "Unknown" KDOS_ABOUT_ROOT="$S/empty"

# ── the C library ────────────────────────────────────────────────────────
printf '1.2.6 1\n./\n./lib/\n' >"$S/r/var/lib/kpkg/db/musl"
value about-libc "musl 1.2.6" KDOS_ABOUT_ROOT="$S/r"
chmod -R a-w "$S/r/var"
shape about-libc "read-only /var" KDOS_ABOUT_ROOT="$S/r"
value about-libc "musl 1.2.6" KDOS_ABOUT_ROOT="$S/r"
chmod -R u+w "$S/r/var"
value about-libc "musl" KDOS_ABOUT_ROOT="$S/empty"

# ── the boxes ────────────────────────────────────────────────────────────
value about-boxes "None" HOME="$S/h"
touch "$S/h/.config/kdos/boxes/www.conf"
value about-boxes "1 box" HOME="$S/h"
touch "$S/h/.config/kdos/boxes/dev.conf" "$S/h/.config/kdos/boxes/ham.conf" \
	"$S/h/.config/kdos/boxes/notes.txt"
value about-boxes "3 boxes" HOME="$S/h"
value about-boxes "Unknown"

# ── the update key ───────────────────────────────────────────────────────
U="$S/r/var/lib/kdos/update.json"
printf '{"behind":3,"orphans":0,"tree":"0123","version":"0.2.15","signed_by":{"key":"0f0a92d672dda877","name":"KDOS release, \\"main\\" {x}"},"verified":1735689600,"binhost":"https://x","channel":"stable","notes":"\\"signed_by\\":{\\"key\\":\\"ffffffffffffffff\\"}"}\n' >"$U"
value about-update "ed25519 0f0a…a877 · verified 2025-01-01" KDOS_ABOUT_ROOT="$S/r"
printf '{"behind":0,"signed_by":{"key":"0f0a92d672dda877","name":"k"},"verified":1735689600}' >"$U"
value about-update "ed25519 0f0a…a877 · verified 2025-01-01" KDOS_ABOUT_ROOT="$S/r"
printf '{"behind":0,"signed_by":null,"verified":null,"binhost":null}\n' >"$U"
value about-update "None verified yet" KDOS_ABOUT_ROOT="$S/r"
printf '{"signed_by":{"key":"$(reboot)aaaaaaa","name":"k"},"verified":1}\n' >"$U"
value about-update "Unknown" KDOS_ABOUT_ROOT="$S/r"
printf '{"signed_by":{"key":"0f0a92d672dda877","name":"k"},"verified":"soon"}\n' >"$U"
value about-update "ed25519 0f0a…a877" KDOS_ABOUT_ROOT="$S/r"
printf 'not json\n' >"$U"
value about-update "Unknown" KDOS_ABOUT_ROOT="$S/r"
chmod a-r "$U"
shape about-update "unreadable update.json" KDOS_ABOUT_ROOT="$S/r"
chmod u+r "$U"
value about-update "Unknown" KDOS_ABOUT_ROOT="$S/empty"

# ── the list ─────────────────────────────────────────────────────────────
# The shipped rc names what this port installs, by the path it installs to,
# separated by ';' — a ',' would make the whole value one missing program.
listed=$(sed -n 's/^ExtraSoftwareData=//p' "$RC_FILE")
case $listed in *,*) echo "  FAIL $RC_FILE separates ExtraSoftwareData with ','"; fail=1 ;; esac
names=$(printf '%s\n' "$listed" | tr ';' '\n' | sed -n 's#^/usr/libexec/kdos/##p' | sort | tr '\n' ' ')
ours=$(cd "$A" && ls about-* | sort | tr '\n' ' ')
if [ "$names" != "$ours" ]; then
	echo "  FAIL $RC_FILE names [$names], the port installs [$ours] in /usr/libexec/kdos"
	fail=1
fi

echo "  six rows, every condition, slowest row ${worst} ms (fastest of three runs)"
exit $fail
