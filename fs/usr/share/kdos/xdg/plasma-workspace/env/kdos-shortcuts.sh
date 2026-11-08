# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   Give a person the KDOS global shortcuts, once each.
#
# Sourced by startplasma, with every other plasma-workspace/env/*.sh along
# XDG_CONFIG_DIRS, before KWin starts. kglobalaccel lives inside KWin and
# reads ~/.config/kglobalshortcutsrc alone: a kglobalshortcutsrc in an XDG
# config directory is never consulted. So the shipped layers are copied in
# here, ahead of the first read:
#
#   each kglobalshortcutsrc along XDG_CONFIG_DIRS is a layer, the earlier
#   directory winning a key — /etc/xdg's over KDOS's /usr/share/kdos/xdg;
#
#   an entry is copied only when the person's file has no value of its own
#   for it, and only if it has never been copied before. Both are recorded in
#   $XDG_STATE_HOME/kdos/kglobalshortcutsrc.seeded, one `[group]<TAB>key` a
#   line. Without that record, a desktop entry's shortcut reset to KDE's
#   default — which kglobalaccel stores by deleting the entry — would come
#   back at the next login;
#
#   the person's file is rewritten whole through a temporary file and a
#   rename, so KWin never reads half of it.
#
# Everything runs in a subshell: startplasma imports the environment this
# leaves behind into the session, and this sets nothing a session needs.
# A failure here costs the KDOS chords for one login and nothing else.
# ---------------------------------

(
	dst=${XDG_CONFIG_HOME:-$HOME/.config}/kglobalshortcutsrc
	state=${XDG_STATE_HOME:-$HOME/.local/state}/kdos/kglobalshortcutsrc.seeded

	set -f --
	_ifs=$IFS
	IFS=:
	for _d in ${XDG_CONFIG_DIRS:-/etc/xdg}; do
		[ -n "$_d" ] && [ -f "$_d/kglobalshortcutsrc" ] &&
			set -- "$@" "$_d/kglobalshortcutsrc"
	done
	IFS=$_ifs
	set +f
	[ "$#" -gt 0 ] || exit 0

	mkdir -p "${dst%/*}" "${state%/*}" || exit 0
	tmp=$dst.kdos-seed.$$
	stmp=$state.$$
	trap 'rm -f "$tmp" "$stmp"' EXIT

	awk -v dst="$dst" -v state="$state" -v tmp="$tmp" -v stmp="$stmp" '
	function trim(s) {
		sub(/^[ \t]+/, "", s)
		sub(/[ \t\r]+$/, "", s)
		return s
	}
	function header(s) {
		return s ~ /^\[.*\]$/
	}
	function keyof(s) {
		sub(/=.*/, "", s)
		return trim(s)
	}
	BEGIN {
		ns = 0
		while ((getline l < state) > 0) {
			if (l != "") {
				seen[l] = 1
				oldstate[++ns] = l
			}
		}
		close(state)

		no = 0
		for (i = 1; i < ARGC; i++) {
			g = ""
			while ((getline l < ARGV[i]) > 0) {
				s = trim(l)
				if (s == "" || substr(s, 1, 1) == "#")
					continue
				if (header(s)) {
					g = s
					continue
				}
				if (g == "" || index(s, "=") == 0)
					continue
				id = g "\t" keyof(s)
				if (id in layer)
					continue
				layer[id] = s
				grpof[id] = g
				order[++no] = id
			}
			close(ARGV[i])
		}

		nd = 0
		g = ""
		while ((getline l < dst) > 0) {
			d[++nd] = l
			s = trim(l)
			if (header(s)) {
				g = s
				ingroup[g] = 1
				last[g] = nd
			} else if (g != "" && s != "" && substr(s, 1, 1) != "#") {
				last[g] = nd
				if (index(s, "=") > 0)
					has[g "\t" keyof(s)] = 1
			}
		}
		close(dst)

		nn = 0
		npend = 0
		for (i = 1; i <= no; i++) {
			id = order[i]
			if (id in seen)
				continue
			newstate[++nn] = id
			if (id in has)
				continue
			g = grpof[id]
			pend[g] = pend[g] layer[id] "\n"
			if (!(g in queued)) {
				queued[g] = 1
				gorder[++ng] = g
			}
			npend++
		}
		if (nn == 0)
			exit 3

		if (npend > 0) {
			for (g in pend)
				if (g in ingroup)
					at[last[g]] = g
			for (i = 1; i <= nd; i++) {
				print d[i] > tmp
				if (i in at)
					printf "%s", pend[at[i]] > tmp
			}
			for (j = 1; j <= ng; j++) {
				g = gorder[j]
				if (g in ingroup)
					continue
				if (nd > 0 || j > 1)
					print "" > tmp
				print g > tmp
				printf "%s", pend[g] > tmp
			}
			close(tmp)
		}

		for (i = 1; i <= ns; i++)
			print oldstate[i] > stmp
		for (i = 1; i <= nn; i++)
			print newstate[i] > stmp
		close(stmp)
		exit (npend > 0 ? 0 : 4)
	}' "$@"

	case $? in
	0) mv -f "$tmp" "$dst" && mv -f "$stmp" "$state" ;;
	4) mv -f "$stmp" "$state" ;;
	esac
) 2>/dev/null || :
