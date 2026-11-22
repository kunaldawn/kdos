/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   decidetest — kdos-notice's facts and decisions with no bus
 *
 *   decidetest restarts FILE      `kdos restarts --json` as FILE says
 *   decidetest cve FILE           `kdos cve --json`
 *   decidetest boot FILE          `kdos-bootctl status --json`, both the
 *                                 rollback and the trial
 *   decidetest oom LINE PREV      a kdos-oomd status line, PREV kills seen
 *   decidetest pending APPS [GROUPS [TITLES]]
 *                                 the installer's apps-pending file, and
 *                                 its groups-pending file read as
 *                                 kdos-notice reads both, with group
 *                                 titles from TITLES (none when absent)
 *   decidetest pending-cases DIR  the pending notice over both files and
 *                                 the tree's groups file: groups by title
 *                                 and by id, selections alone, both, the
 *                                 icon of each, and the withdrawal; `ok`
 *                                 or `FAIL` lines, exit 1 on a failure
 *   decidetest firmware FILE      `kdos firmware sync --dry-run --json`
 *   decidetest action KIND ACTION the vector an action runs
 *
 * $KDOS_NOTICE_ADMIN_GROUP stands for `wheel`; pending-cases sets it for
 * both an administrator and an account outside `wheel`.
 *
 * Each prints what kdos-notice would do — `post KIND`, `close KIND` or
 * `none KIND` — and records it in $XDG_STATE_HOME/kdos/notice as main.c
 * does after a server accepted it, ids counting up, so a run of calls is a
 * session seeing its facts change. $KDOS_NOTICE_BOOT_ID is the boot.
 * ---------------------------------
 */

#include <grp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "notice.h"

static KnState st;

static void apply(int got, int kind, const KnPost *p)
{
	if (!got) {
		printf("none %s\n", kn_kind_name(kind));
		return;
	}
	if (p->close) {
		printf("close %s %u\n", kn_kind_name(p->kind), st.id[p->kind]);
		st.id[p->kind] = 0;
		st.key[p->kind][0] = '\0';
	} else {
		unsigned id = 1;

		for (int k = 0; k < KN_COUNT; k++)
			if (st.id[k] >= id)
				id = st.id[k] + 1;
		printf("post %s replaces=%u persistent=%d urgency=%d\n",
		       kn_kind_name(p->kind), st.id[p->kind], p->persistent,
		       p->urgency);
		printf("  summary %s\n  body %s\n  icon %s\n", p->summary,
		       p->body, p->icon ? p->icon : "");
		for (int i = 0; p->actions[i]; i += 2)
			printf("  action %s %s\n", p->actions[i], p->actions[i + 1]);
		st.id[p->kind] = id;
		kb_strlcpy(st.key[p->kind], p->key, sizeof(st.key[0]));
	}
	if (kn_state_save(&st) != 0) {
		fprintf(stderr, "decidetest: the state was not saved\n");
		exit(1);
	}
}

static char *slurp(const char *path)
{
	char *t = kb_read_whole(path, NULL);

	/* An unreadable file is a command that printed nothing. */
	return t ? t : kb_strdup("");
}

/* ── the pending notice over both of the installer's records ───────── */

static int pc_fail;

static void pc_check(int cond, const char *what, const KnPost *p)
{
	printf("  %s  %s\n", cond ? "ok  " : "FAIL", what);
	if (!cond) {
		printf("        body %s\n", p->body);
		for (int i = 0; p->actions[i]; i += 2)
			printf("        action %s %s\n", p->actions[i],
			       p->actions[i + 1]);
		pc_fail = 1;
	}
}

/* One look: write both files and the tree's groups file, read them as
 * kdos-notice does, decide, and record the post in `s` as a server's
 * acceptance would. Every body posted is checked for a backtick, which
 * Plasma would draw as itself. */
static int pc_look(const char *dir, const char *apps, const char *groups,
		   const char *titles, KnState *s, KnPost *p)
{
	char a[512], g[512], t[512];
	KnPending pd;
	int got;

	snprintf(a, sizeof(a), "%s/apps-pending", dir);
	snprintf(g, sizeof(g), "%s/groups-pending", dir);
	snprintf(t, sizeof(t), "%s/groups", dir);
	if (kb_write_all(a, apps, strlen(apps)) < 0 ||
	    kb_write_all(g, groups, strlen(groups)) < 0 ||
	    kb_write_all(t, titles, strlen(titles)) < 0) {
		fprintf(stderr, "decidetest: %s is not writable\n", dir);
		exit(1);
	}
	setenv("KDOS_APPS_PENDING", a, 1);
	setenv("KDOS_GROUPS_PENDING", g, 1);
	setenv("KDOS_NOTICE_GROUPS", t, 1);
	kn_read_pending(&pd);
	memset(p, 0, sizeof(*p));
	got = kn_decide_pending(&pd, s, p);
	if (got && p->close) {
		s->id[KN_PENDING] = 0;
		s->key[KN_PENDING][0] = '\0';
	} else if (got) {
		s->id[KN_PENDING]++;
		kb_strlcpy(s->key[KN_PENDING], p->key, sizeof(s->key[0]));
		if (strchr(p->body, '`')) {
			printf("  FAIL  a backtick in the body\n"
			       "        body %s\n", p->body);
			pc_fail = 1;
		}
	}
	return got;
}

static int pc_has(const KnPost *p, const char *text)
{
	return strstr(p->body, text) != NULL;
}

static int pc_icon(const KnPost *p, const char *icon)
{
	return p->icon && !strcmp(p->icon, icon);
}

static int pc_discover(const KnPost *p)
{
	return p->actions[0] && !strcmp(p->actions[0], KN_ACT_DETAILS) &&
	       !strcmp(p->actions[1], "Open Discover") && !p->actions[2];
}

/* Both kinds waiting: the body is the groups' text ending on the command
 * line, a newline, and Discover's line `apps` as the body's last line. */
static int pc_both(const KnPost *p, const char *apps)
{
	static const char cmd[] = "sudo kdos update install --pending\n";
	const char *c = strstr(p->body, cmd);
	size_t n = strlen(p->body), m = strlen(apps);

	return c && n > m && !strcmp(p->body + n - m, apps) &&
	       c + strlen(cmd) == p->body + n - m;
}

/* Lay the body out as Plasma's popup does, wrapping each body line at word
 * breaks to `width` characters (the 1920x1080 shot holds 38 to 41 a line) and
 * counting a UTF-8 character as one: the popup line the command stands on
 * alone, 0 without it, and the lines the whole body takes in `total`. */
static int pc_layout(const KnPost *p, int width, int *total)
{
	const char *b = p->body;
	int line = 0, cmd = 0;

	while (*b) {
		const char *e = strchr(b, '\n');
		size_t len = e ? (size_t)(e - b) : strlen(b);
		int col = 0;

		if (!cmd && len == 34 &&
		    !strncmp(b, "sudo kdos update install --pending", 34))
			cmd = line + 1;
		line++;
		for (const char *w = b; w < b + len;) {
			const char *we = w;
			int n = 0;

			while (we < b + len && *we != ' ') {
				if (((unsigned char)*we & 0xC0) != 0x80)
					n++;
				we++;
			}
			if (col && col + 1 + n > width) {
				line++;
				col = n;
			} else {
				col += (col ? 1 : 0) + n;
			}
			w = we < b + len ? we + 1 : we;
		}
		b = e ? e + 1 : b + len;
	}
	if (total)
		*total = line;
	return cmd;
}

static int pc_cmd_line(const KnPost *p, int width)
{
	return pc_layout(p, width, NULL);
}

/* Groups and boxed apps both waiting: the whole body inside the popup's
 * eight lines of 38, the command on a line of its own, the boxed line
 * `apps` last, and no line that counts Discover's selections as "more"
 * after a list of groups. */
static int pc_fits(const KnPost *p, const char *apps)
{
	int total = 0, cmd = pc_layout(p, 38, &total);

	return cmd > 0 && total <= 8 && pc_both(p, apps) &&
	       !strstr(p->body, "more selection");
}

/* The group kdos-notice takes for `wheel`: this account's primary group
 * for an administrator, a group no database holds for anyone else. */
static void admin_group(int admin)
{
	struct group *g = getgrgid(getgid());

	setenv("KDOS_NOTICE_ADMIN_GROUP",
	       admin && g ? g->gr_name : "kdos-notice-nosuch-group", 1);
}

/* pc_fits for an account outside `wheel`: the su -c command alone on its
 * line, followed by the boxed line `apps`, inside eight popup lines. */
static int pc_su_fits(const KnPost *p, const char *apps)
{
	static const char cmd[] = "\nsu -c 'kdos update install --pending'\n";
	const char *c = strstr(p->body, cmd);
	size_t n = strlen(p->body), m = strlen(apps);
	int total = 0;

	pc_layout(p, 38, &total);
	return c && n > m && !strcmp(p->body + n - m, apps) &&
	       c + strlen(cmd) == p->body + n - m && total <= 8 &&
	       !strstr(p->body, "sudo");
}

static int pending_cases(const char *dir)
{
	static const char run[] = "Once a binary host answers, run:\n"
				  "sudo kdos update install --pending";
	static const char hdr[] = "# Native groups chosen during the install "
				  "and not yet installed.\n";
	static const char tree[] =
		"# group <id> <title>\n"
		"group kde-games KDE games\n"
		"  kde-gear-games\n"
		"group kde-network KDE torrents, remote desktop and file sharing\n"
		"  ktorrent krdc\n"
		"group kde-system KDE disk usage, search, logs, wallets, "
		"characters and handbooks\n"
		"  filelight\n"
		"group kde-online KDE torrents, remote desktop, file sharing and "
		"online accounts\n"
		"  ktorrent\n"
		"group kde-artwork KDE wallpapers and the Oxygen theme # art\n"
		"  plasma-workspace-wallpapers oxygen\n"
		"group bare\n"
		"group odd Tools <b>&</b> more\n"
		"group huge Enormous title that wraps to four popup lines "
		"because it names every program the group holds, one after "
		"another, without stopping for breath\n";
	static const char artwork[] = "KDE wallpapers and the Oxygen theme "
				      "was chosen during the install and is "
				      "not installed yet. ";
	char want[512];
	KnState s;
	KnPost p;
	int got;

	/* The cases below are an administrator's: the account's own primary
	 * group stands for `wheel`, whatever groups the host gives it. */
	admin_group(1);
	memset(&s, 0, sizeof(s));
	got = pc_look(dir, "", "kde-artwork\n", tree, &s, &p);
	pc_check(got && !p.close && p.persistent, "groups alone post", &p);
	snprintf(want, sizeof(want), "%s%s", artwork, run);
	pc_check(!strcmp(p.body, want),
		 "groups alone: the group by its title, and the command", &p);
	pc_check(!p.actions[0], "groups alone: no Open Discover", &p);
	pc_check(pc_icon(&p, "system-software-install"),
		 "groups alone: the software-install icon", &p);
	got = pc_look(dir, "", "kde-artwork\n", tree, &s, &p);
	pc_check(!got, "the same groups again post nothing", &p);
	got = pc_look(dir, "", "kde-artwork\n",
		      "group kde-artwork KDE artwork\n", &s, &p);
	pc_check(!got, "a changed title posts nothing", &p);

	got = pc_look(dir, "essential\n", "kde-artwork\n", tree, &s, &p);
	pc_check(got && !p.close, "a selection beside the group posts", &p);
	snprintf(want, sizeof(want), "%s%s\n1 boxed app also waits in "
		 "Discover.", artwork, run);
	pc_check(!strcmp(p.body, want),
		 "both: the group's sentence and command, then Discover's "
		 "line", &p);
	pc_check(pc_fits(&p, "1 boxed app also waits in Discover."),
		 "one group and one app: inside eight lines of 38", &p);
	pc_check(pc_discover(&p), "both: Open Discover", &p);
	pc_check(pc_icon(&p, "plasmadiscover"), "both: Discover's icon", &p);

	got = pc_look(dir, "essential\n", hdr, tree, &s, &p);
	pc_check(got && !p.close, "the groups installed: the notice changes",
		 &p);
	pc_check(!pc_has(&p, "group") && !pc_has(&p, "--pending") &&
		 pc_has(&p, "1 selection is waiting"),
		 "selections alone: no group sentence", &p);
	pc_check(pc_discover(&p), "selections alone: Open Discover", &p);
	pc_check(pc_icon(&p, "plasmadiscover"),
		 "selections alone: Discover's icon", &p);

	got = pc_look(dir, "essential\n", "kde-artwork\nkde-games\n", tree,
		      &s, &p);
	pc_check(got && pc_has(&p, "2 KDOS application groups:\nKDE "
			       "wallpapers and the Oxygen theme\nKDE games\n"
			       "Once a binary host answers, run:"),
		 "two groups: counted, each by its title on its own line", &p);

	got = pc_look(dir, "essential\n", "kde-utilities\n", tree, &s, &p);
	pc_check(got && pc_fits(&p, "1 boxed app also waits in Discover."),
		 "one untitled group and one app: command, newline, the "
		 "boxed line", &p);

	got = pc_look(dir, "org.kde.kate\napp.kcalc\n",
		      "kde-games\nnosuch-group\nkde-artwork\n", tree, &s, &p);
	pc_check(got && pc_has(&p, "3 KDOS application groups:\nKDE "
			       "games\n“nosuch\u2011group”\nKDE wallpapers "
			       "and the Oxygen theme\nOnce a binary host answers, run:") &&
		 pc_fits(&p, "2 boxed apps also wait in Discover."),
		 "three groups, one untitled, and two selections: the titles "
		 "and the command, then Discover", &p);
	pc_check(pc_discover(&p) && pc_icon(&p, "plasmadiscover"),
		 "three groups and selections: Open Discover", &p);

	got = pc_look(dir, "org.kde.kate\n", "kde-games\nnosuch-group\n"
		      "kde-artwork\n", tree, &s, &p);
	pc_check(got && pc_fits(&p, "1 boxed app also waits in Discover.") &&
		 pc_has(&p, "3 KDOS application groups:\nKDE games\n"
			"“nosuch\u2011group”\nKDE wallpapers and the Oxygen "
			"theme\nOnce a binary host answers, run:\n"),
		 "three groups, one untitled, and one app: a one-line header, "
		 "every title, the command and the boxed line in eight lines",
		 &p);
	got = pc_look(dir, "essential\n", "kde-games\nkde-artwork\n"
		      "kde-network\n", tree, &s, &p);
	pc_check(got && pc_fits(&p, "1 boxed app also waits in Discover."),
		 "three titled groups and one app: inside eight lines", &p);

	/* /ports/groups' two longest titles each wrap to two popup lines:
	 * with them the whole body still takes eight lines of 38 or fewer,
	 * and at 36 characters a line the command still stands on the
	 * eighth line or above, the last one the popup shows before it
	 * scrolls. */
	got = pc_look(dir, "essential\n", "kde-games\nkde-system\n"
		      "kde-artwork\n", tree, &s, &p);
	pc_check(got && pc_fits(&p, "1 boxed app also waits in Discover.") &&
		 !pc_has(&p, "more\n") &&
		 pc_cmd_line(&p, 36) > 0 && pc_cmd_line(&p, 36) <= 8,
		 "three groups, one long title, and one app: every title, "
		 "inside eight lines", &p);
	got = pc_look(dir, "org.kde.kate\napp.kcalc\n",
		      "kde-system\nkde-online\n", tree, &s, &p);
	pc_check(got && pc_fits(&p, "2 boxed apps also wait in Discover.") &&
		 !pc_has(&p, "more\n") &&
		 pc_cmd_line(&p, 36) > 0 && pc_cmd_line(&p, 36) <= 8,
		 "two long titles and two apps: both titles, inside eight "
		 "lines", &p);
	got = pc_look(dir, "org.kde.kate\n", "kde-system\nkde-online\n"
		      "kde-network\nkde-games\nkde-artwork\n", tree, &s, &p);
	pc_check(got && pc_fits(&p, "1 boxed app also waits in Discover.") &&
		 pc_has(&p, "5 KDOS application groups:\nKDE disk usage") &&
		 pc_has(&p, "\nand 4 more\nOnce a binary host answers, run:"),
		 "more groups than the popup holds: the titles that fit, "
		 "then and N more, the command and the boxed line", &p);
	got = pc_look(dir, "org.kde.kate\n", "huge\nkde-games\n", tree, &s,
		      &p);
	pc_check(got && pc_fits(&p, "1 boxed app also waits in Discover.") &&
		 pc_has(&p, "2 KDOS application groups chosen during the "
			"install are not installed yet. Once") &&
		 !pc_has(&p, "Enormous"),
		 "a first title too long to fit: the count alone", &p);

	/* Several groups alone: the count and the command first, then every
	 * title with no budget, since the notice is the one place that names
	 * them; the popup shows the top of the body, so a scroll hides only
	 * titles. */
	got = pc_look(dir, "", "kde-system\nkde-online\nkde-network\n"
		      "kde-games\nkde-artwork\n", tree, &s, &p);
	snprintf(want, sizeof(want), "5 KDOS application groups wait.\n%s\n"
		 "They are:\nKDE disk "
		 "usage, search, logs, wallets, characters and handbooks\n"
		 "KDE torrents, remote desktop, file sharing and online "
		 "accounts\nKDE torrents, remote desktop and file sharing\n"
		 "KDE games\nKDE wallpapers and the Oxygen theme", run);
	pc_check(got && !strcmp(p.body, want),
		 "five groups alone: the command, then every title, no and N "
		 "more", &p);
	got = pc_look(dir, "", "huge\nkde-games\n", tree, &s, &p);
	pc_check(got && pc_has(&p, "2 KDOS application groups wait.\nOnce a "
			       "binary host answers, run:\nsudo kdos update "
			       "install --pending\nThey are:\nEnormous") &&
		 pc_has(&p, "breath\nKDE games") &&
		 pc_cmd_line(&p, 38) > 0 && pc_cmd_line(&p, 38) <= 8,
		 "a long title among groups alone: still listed, below the "
		 "command", &p);

	/* The seven KDE groups of /ports/groups alone: the command stands on
	 * the third popup line, far above the scroll, and every title is
	 * there below it. */
	{
		static const char seven[] =
			"group kde-games KDE games\n"
			"group kde-education KDE education\n"
			"group kde-pim KDE mail, calendar and contacts\n"
			"group kde-multimedia KDE audio and video\n"
			"group kde-graphics KDE graphics\n"
			"group kde-utilities KDE utilities\n"
			"group kde-system KDE disk usage, search, logs, "
			"wallets, characters and handbooks\n";
		static const char *const titles[] = {
			"\nKDE games\n", "\nKDE education\n",
			"\nKDE mail, calendar and contacts\n",
			"\nKDE audio and video\n", "\nKDE graphics\n",
			"\nKDE utilities\n",
			"\nKDE disk usage, search, logs, wallets, characters "
			"and handbooks",
		};
		int all = 1;

		got = pc_look(dir, "", "kde-games\nkde-education\nkde-pim\n"
			      "kde-multimedia\nkde-graphics\nkde-utilities\n"
			      "kde-system\n", seven, &s, &p);
		for (size_t i = 0; i < sizeof(titles) / sizeof(*titles); i++)
			all = all && pc_has(&p, titles[i]);
		pc_check(got && all && !pc_has(&p, "more\n") &&
			 pc_cmd_line(&p, 38) > 0 && pc_cmd_line(&p, 38) <= 3 &&
			 pc_cmd_line(&p, 36) > 0 && pc_cmd_line(&p, 36) <= 8,
			 "seven groups alone: the command on the third popup "
			 "line or above, every title below it", &p);
	}

	got = pc_look(dir, "", "kde-games\nkde-network\n", tree, &s, &p);
	snprintf(want, sizeof(want), "2 KDOS application groups wait.\n%s\n"
		 "They are:\nKDE games\n"
		 "KDE torrents, remote desktop and file sharing", run);
	{
		int total = 0;

		pc_check(got && !strcmp(p.body, want) &&
			 pc_layout(&p, 38, &total) > 0 && total <= 8,
			 "two groups alone, a title with a comma: one title a "
			 "line, the whole body inside eight lines of 38", &p);
	}
	pc_check(pc_icon(&p, "system-software-install"),
		 "two groups alone: the software-install icon", &p);

	got = pc_look(dir, "", "kde-utilities\n", tree, &s, &p);
	snprintf(want, sizeof(want), "The KDOS application group "
		 "“kde\u2011utilities” was chosen during the install and is "
		 "not installed yet. %s", run);
	pc_check(got && !strcmp(p.body, want),
		 "a group the tree has no title for: named by its id", &p);
	got = pc_look(dir, "", "bare\n", tree, &s, &p);
	pc_check(got && pc_has(&p, "The KDOS application group “bare” was "),
		 "a group line with no title: named by its id", &p);
	got = pc_look(dir, "", "kde-artwork\nkde-utilities\n", tree, &s, &p);
	pc_check(got && pc_has(&p, "They are:\nKDE wallpapers and the Oxygen "
			       "theme\n“kde\u2011utilities”"),
		 "a titled group beside an untitled one", &p);
	pc_check(got && !strstr(p.body, "kde-utilities") &&
		 pc_has(&p, "\nsudo kdos update install --pending\nThey are:"),
		 "the command alone on its line, the id with no breaking "
		 "hyphen", &p);
	got = pc_look(dir, "", "kde-artwork\n", "", &s, &p);
	pc_check(got && pc_has(&p, "The KDOS application group "
			       "“kde\u2011artwork” was chosen"),
		 "no groups file: named by its id", &p);

	got = pc_look(dir, "", "odd\n", tree, &s, &p);
	pc_check(got && pc_has(&p, "Tools &lt;b&gt;&amp;&lt;/b&gt; more was "
			       "chosen"),
		 "a title is markup-escaped", &p);

	got = pc_look(dir, "", "kde-artwork # chosen at install\n"
		      "bad/name\n", tree, &s, &p);
	pc_check(got && pc_has(&p, artwork) && !pc_has(&p, "bad/name"),
		 "groups read as --pending reads them: cut at #, names only",
		 &p);

	{
		char many[2048], ids[512];
		int o = 0, d = 0;

		for (int i = 0; i < 20; i++) {
			o += snprintf(many + o, sizeof(many) - o, "group g%02d "
				      "Long group title number %02d of twenty\n",
				      i, i);
			d += snprintf(ids + d, sizeof(ids) - d, "g%02d\n", i);
		}
		got = pc_look(dir, "", ids, many, &s, &p);
		snprintf(want, sizeof(want), "20 KDOS application groups "
			 "chosen during the install are not installed yet. %s",
			 run);
		pc_check(got && !strcmp(p.body, want),
			 "names past the buffer: the counted form", &p);
		pc_check(got && !pc_has(&p, "Long group title"),
			 "the counted form names no title", &p);
	}

	got = pc_look(dir, "", "", tree, &s, &p);
	pc_check(got && p.close, "both empty withdraw the notice", &p);
	got = pc_look(dir, "# nothing\n", hdr, tree, &s, &p);
	pc_check(!got, "and nothing more while both name nothing", &p);

	/* An account outside `wheel`, which sudo refuses: every layout names
	 * su -c on a line of its own, and the boxed line still ends the body
	 * inside the popup. */
	admin_group(0);
	got = pc_look(dir, "", "kde-artwork\n", tree, &s, &p);
	pc_check(got && pc_has(&p, "\nsu -c 'kdos update install --pending'") &&
		 !pc_has(&p, "sudo"),
		 "outside wheel: one group names su -c, never sudo", &p);
	got = pc_look(dir, "", "kde-artwork\nkde-games\n", tree, &s, &p);
	pc_check(got && pc_has(&p, "wait.\nOnce a binary host answers, run:\n"
			       "su -c 'kdos update install --pending'\n"
			       "They are:") && !pc_has(&p, "sudo"),
		 "outside wheel: groups alone name su -c", &p);
	got = pc_look(dir, "org.kde.kate\n", "kde-system\nkde-online\n", tree,
		      &s, &p);
	pc_check(got && pc_su_fits(&p, "1 boxed app also waits in Discover."),
		 "outside wheel: groups and an app fit the popup with su -c",
		 &p);
	admin_group(1);
	return pc_fail;
}

int main(int argc, char **argv)
{
	char boot[64];
	KnPost p;

	kb_set_progname("decidetest");
	if (argc < 3) {
		fprintf(stderr, "usage: decidetest restarts|cve|boot|oom|pending|pending-cases|firmware|action ...\n");
		return 2;
	}
	kn_state_load(&st);
	kn_boot_id(boot, sizeof(boot));
	if (!strcmp(argv[1], "restarts")) {
		KnRestarts *r = kb_calloc(1, sizeof(*r));
		char *j = slurp(argv[2]);

		if (kn_parse_restarts(j, r))
			kn_name_procs(r);
		for (int i = 0; i < r->n; i++)
			printf("proc %d %s name=%s desktop=%d\n", r->p[i].pid,
			       r->p[i].comm, r->p[i].name, r->p[i].desktop);
		apply(kn_decide_restarts(r, boot, &st, &p), KN_RESTARTS, &p);
		free(j);
		free(r);
	} else if (!strcmp(argv[1], "cve")) {
		KnCve *c = kb_calloc(1, sizeof(*c));
		char *j = slurp(argv[2]);

		kn_parse_cve(j, c);
		printf("cves %d in %d\n", c->ncve, c->n);
		apply(kn_decide_cve(c, &st, &p), KN_CVE, &p);
		free(j);
		free(c);
	} else if (!strcmp(argv[1], "boot")) {
		KnBoot b;
		char *j = slurp(argv[2]);

		kn_parse_boot(j, &b);
		printf("boot ok=%d running=%s active=%s trying=%s failed=%s "
		       "a=%s b=%s\n", b.ok, b.running, b.active, b.trying,
		       b.failed, b.ver[0], b.ver[1]);
		apply(kn_decide_rollback(&b, &st, &p), KN_ROLLBACK, &p);
		apply(kn_decide_trial(&b, boot, &st, &p), KN_TRIAL, &p);
		free(j);
	} else if (!strcmp(argv[1], "oom") && argc > 3) {
		KnOom o;

		kn_parse_oom(argv[2], &o);
		printf("oom ok=%d kills=%d last=%s\n", o.ok, o.kills, o.last);
		apply(kn_decide_oom(&o, atoi(argv[3]), &p), KN_OOM, &p);
	} else if (!strcmp(argv[1], "pending")) {
		KnPending pd;

		if (argc > 3) {
			setenv("KDOS_APPS_PENDING", argv[2], 1);
			setenv("KDOS_GROUPS_PENDING", argv[3], 1);
			/* A missing titles file, never the host's /ports/groups:
			 * the drive reads only the files it is given. */
			setenv("KDOS_NOTICE_GROUPS",
			       argc > 4 ? argv[4] : "/nonexistent/kdos-groups", 1);
			kn_read_pending(&pd);
		} else {
			char *t = slurp(argv[2]);

			kn_parse_pending(t, NULL, NULL, &pd);
			free(t);
		}
		printf("pending %d groups %d %s\n", pd.n, pd.ngroups,
		       pd.groups);
		apply(kn_decide_pending(&pd, &st, &p), KN_PENDING, &p);
	} else if (!strcmp(argv[1], "pending-cases")) {
		return pending_cases(argv[2]);
	} else if (!strcmp(argv[1], "firmware")) {
		KnFirmware f;
		char *j = slurp(argv[2]);

		kn_parse_firmware(j, &f);
		printf("firmware ok=%d ports=%d\n", f.ok, f.n);
		apply(kn_decide_firmware(&f, &st, &p), KN_FIRMWARE, &p);
		free(j);
	} else if (!strcmp(argv[1], "action") && argc > 3) {
		KbArgv a;
		int k = kn_kind_of(argv[2]);

		if (k < 0 || !kn_action_argv(k, argv[3], &a)) {
			printf("no action\n");
			return 0;
		}
		for (int i = 0; i < a.n && a.v[i]; i++)
			printf("[%s]\n", a.v[i]);
	} else {
		fprintf(stderr, "decidetest: unknown step %s\n", argv[1]);
		return 2;
	}
	return 0;
}
