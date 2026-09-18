/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos remind — a toast, later
 *
 * ONE snooze PER REMINDER, AND NO SCHEDULER OF OURS. J.13's per-user table is
 * where the job lives, so a reminder survives a logout and comes back with the
 * session; `snooze` is what waits.
 *
 * IT IS ARMED TWICE AND FIRES ONCE, and both halves are needed. The per-user
 * table is read ONCE, at login — nothing watches the directory — so a file
 * written now would wait for the next login before anything looked at it;
 * `kdos remind` therefore starts its own `snooze` as well. The next login
 * starts a second one from the same file. They cannot both deliver, because
 * `kdos remind fire` REMOVES the reminder before it returns and the other one
 * then finds nothing: the file's presence is the reminder, and its absence is
 * the record that it has been given.
 *
 * THE TEXT IS A COMMENT LINE AND NOT AN ARGUMENT. Both timer parsers split a
 * row by leaving an expansion unquoted, with no `eval` and no quoting — so
 * `-- kdos notify "make tea"` reaches the command as `"make` and `tea"`. A
 * comment is the one field a parser that skips comments cannot mangle, and it
 * is the reminder's text.
 *
 * `-t` IS WHAT MAKES A MISSED ONE FIRE. `-s` alone does not: a freshly started
 * snooze begins looking one second from now, so a slot already past is simply
 * the same slot next year. With `-t` it starts looking from the timefile's
 * mtime — which for a reminder is when it was asked for — and the slack then
 * covers the gap. The timefile is the reminder's own file, so there is nothing
 * else to keep in step.
 * ---------------------------------
 */

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

#include "kdos-tools.h"

/*
 * A DAY OF SLACK. A reminder is about something a person meant to do at a
 * time, so one delivered a day late is still worth having and one delivered a
 * week late is an interruption about a moment that has gone. It is also the
 * bound on how far back a login re-arm reaches.
 */
#define REMIND_SLACK "1d"
#define REMIND_MAX   256
/* The spec holds the timefile's absolute path, so it is the path's size plus
 * the pattern. ONE constant for both the builder and the splitter: two figures
 * for one buffer is how a copy ends up shorter than what it is copying. */
#define REMIND_PATH  1024
#define REMIND_SPEC  (REMIND_PATH + 256)

static int usage(void)
{
	fprintf(stderr,
		"usage: kdos remind in 20m | at 15:30 | tomorrow 9 -- TEXT\n"
		"       kdos remind ls\n"
		"       kdos remind clear\n"
		"       kdos remind fire ID\n"
		"`--` is optional; everything after the time is the text.\n");
	return 2;
}

/* ~/.config/kdos/timers.d, made if it is not there: it is J.13's table and a
 * reminder is one of its rows. */
static char *timers_dir(void)
{
	const char *cfg = getenv("XDG_CONFIG_HOME");
	char *base = cfg && *cfg ? kb_strdup(cfg)
				 : kb_path_join(kb_home_dir(), ".config");
	char *kd = kb_path_join(base, "kdos");
	char *td = kb_path_join(kd, "timers.d");

	free(base);
	free(kd);
	kb_mkdir_p(td);
	return td;
}

/*
 * WHEN.
 *
 * Three shapes and no library: `in 20m`, `at 15:30`, `tomorrow 9`. Anything
 * else is refused rather than guessed at — a reminder that landed at a time
 * nobody meant is worse than one that was not set.
 *
 * `at` AND `tomorrow` ROLL FORWARD. `at 09:00` typed at ten in the morning
 * means tomorrow morning, because a time that has gone is not a time somebody
 * is asking to be reminded at.
 *
 * Returns the number of ARGUMENTS consumed, or 0.
 */
static int parse_when(int argc, char **argv, time_t *out)
{
	time_t now = time(NULL);
	struct tm tm;

	if (argc < 2)
		return 0;
	localtime_r(&now, &tm);

	if (!strcmp(argv[0], "in")) {
		char *end = NULL;
		long n = strtol(argv[1], &end, 10);
		long mul;

		if (n <= 0 || !end || !*end || end[1])
			return 0;
		switch (*end) {
		case 's': mul = 1; break;
		case 'm': mul = 60; break;
		case 'h': mul = 3600; break;
		case 'd': mul = 86400; break;
		default: return 0;
		}
		/* A year is the ceiling because the snooze pattern carries a
		 * month and a day and no year: past twelve months the same
		 * pattern is a different reminder. */
		if (n * mul > 300L * 86400L)
			return 0;
		*out = now + n * mul;
		return 2;
	}

	int h = -1, m = 0, day = 0;

	if (!strcmp(argv[0], "at")) {
		if (sscanf(argv[1], "%d:%d", &h, &m) != 2 &&
		    sscanf(argv[1], "%d", &h) != 1)
			return 0;
	} else if (!strcmp(argv[0], "tomorrow")) {
		day = 1;
		if (sscanf(argv[1], "%d:%d", &h, &m) != 2 &&
		    sscanf(argv[1], "%d", &h) != 1)
			return 0;
	} else {
		return 0;
	}
	if (h < 0 || h > 23 || m < 0 || m > 59)
		return 0;

	tm.tm_hour = h;
	tm.tm_min = m;
	tm.tm_sec = 0;
	tm.tm_mday += day;
	tm.tm_isdst = -1;

	time_t at = mktime(&tm);

	if (at == (time_t)-1)
		return 0;
	if (at <= now) {
		localtime_r(&at, &tm);
		tm.tm_mday += 1;
		tm.tm_isdst = -1;
		at = mktime(&tm);
		if (at == (time_t)-1)
			return 0;
	}
	*out = at;
	return 2;
}

/*
 * An id from the clock and the pid: short enough to type after `fire`, and
 * unique enough that two reminders set in one second do not share a file. It
 * is not a secret and does not need to be one — it names a file in this
 * person's own configuration directory.
 */
static void make_id(char *out, size_t n)
{
	snprintf(out, n, "%08lx%04x", (unsigned long)time(NULL),
		 (unsigned)getpid() & 0xffff);
}

/* The text of a reminder: its file's first comment line, without the mark. */
static int read_text(const char *path, char *out, size_t n)
{
	FILE *f = fopen(path, "re");
	char line[1024];

	out[0] = '\0';
	if (!f)
		return 0;
	while (fgets(line, sizeof(line), f)) {
		char *p = line;

		if (*p != '#')
			continue;
		p++;
		while (*p == ' ')
			p++;
		size_t len = strlen(p);

		while (len && (p[len - 1] == '\n' || p[len - 1] == '\r'))
			p[--len] = '\0';
		if (!len)
			continue;
		snprintf(out, n, "%s", p);
		fclose(f);
		return 1;
	}
	fclose(f);
	return 0;
}

/* `-m M -d D -H h -M m -s SLACK -t PATH`, the pattern that matches the one
 * minute this reminder is for. There is no year in a snooze pattern, which is
 * why `in` refuses anything further out than ten months. */
static void spec_of(const time_t *at, const char *path, char *out, size_t n)
{
	struct tm tm;

	localtime_r(at, &tm);
	snprintf(out, n, "-m %d -d %d -H %d -M %d -S 0 -s %s -t %s",
		 tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min,
		 REMIND_SLACK, path);
}

/*
 * ARM IT NOW, as well as writing the row. Nothing re-reads the per-user table
 * between logins, so a reminder that only wrote a file would wait for the next
 * one. Detached, because this command returns and a reminder does not.
 */
static void arm(const char *spec, const char *id)
{
	KbArgv a = { 0 };
	char buf[REMIND_SPEC];
	char *p, *save;

	if (!kb_have_prog("snooze"))
		return;
	kb_argv_add(&a, "snooze");
	snprintf(buf, sizeof(buf), "%s", spec);
	/* The spec is words this program built, so splitting it here is
	 * splitting what was just joined — not parsing somebody's input. */
	for (p = strtok_r(buf, " ", &save); p; p = strtok_r(NULL, " ", &save))
		kb_argv_add(&a, p);
	kb_argv_add(&a, "kdos");
	kb_argv_add(&a, "remind");
	kb_argv_add(&a, "fire");
	kb_argv_add(&a, id);
	kb_argv_end(&a);
	kb_run_detach(&a);
}

static int cmd_set(int argc, char **argv)
{
	time_t at = 0;
	int used = parse_when(argc, argv, &at);
	char id[32], *dir, path[REMIND_PATH], spec[REMIND_SPEC];
	char text[512] = { 0 };
	FILE *f;

	if (!used)
		return usage();
	argc -= used;
	argv += used;
	if (argc && !strcmp(argv[0], "--")) {
		argc--;
		argv++;
	}
	if (!argc)
		return usage();

	for (int i = 0; i < argc; i++) {
		size_t at_end = strlen(text);

		snprintf(text + at_end, sizeof(text) - at_end, "%s%s",
			 i ? " " : "", argv[i]);
	}
	/* A NEWLINE WOULD BE A SECOND LINE OF THE FILE, and the second line is
	 * the job. Nothing else in the text can reach the parser, because a
	 * comment is all a comment ever is. */
	for (char *p = text; *p; p++)
		if (*p == '\n' || *p == '\r')
			*p = ' ';

	make_id(id, sizeof(id));
	dir = timers_dir();
	snprintf(path, sizeof(path), "%s/remind-%s.timer", dir, id);
	free(dir);
	spec_of(&at, path, spec, sizeof(spec));

	f = fopen(path, "we");
	if (!f)
		kb_die("%s: %s", path, strerror(errno));
	/* THE FIRST COMMENT IS THE TEXT. Both timer parsers skip a comment, so
	 * it is the one field this file can carry that a row cannot. */
	fprintf(f, "# %s\n", text);
	fprintf(f, "remind-%s %s -- kdos remind fire %s\n", id, spec, id);
	fclose(f);
	if (chmod(path, 0600) != 0)
		kb_die("%s: %s", path, strerror(errno));

	arm(spec, id);

	struct tm tm;
	char when[64];

	localtime_r(&at, &tm);
	strftime(when, sizeof(when), "%a %e %b %H:%M", &tm);
	printf("%s at %s\n", text, when);
	return 0;
}

/* Every reminder still waiting, oldest file first — which is the order the
 * directory gives and is good enough for a handful of rows. */
static int each(int (*fn)(const char *path, const char *id, void *u), void *u)
{
	char *dir = timers_dir();
	DIR *d = opendir(dir);
	struct dirent *e;
	int n = 0;

	if (!d) {
		free(dir);
		return 0;
	}
	while ((e = readdir(d))) {
		char path[1024], id[64];
		size_t len = strlen(e->d_name);

		if (strncmp(e->d_name, "remind-", 7) != 0 || len < 14)
			continue;
		if (strcmp(e->d_name + len - 6, ".timer") != 0)
			continue;
		snprintf(id, sizeof(id), "%.*s", (int)(len - 13),
			 e->d_name + 7);
		snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);
		n += fn(path, id, u) ? 1 : 0;
	}
	closedir(d);
	free(dir);
	return n;
}

struct listing {
	char line[REMIND_MAX][160];
	int n;
};

static int ls_one(const char *path, const char *id, void *u)
{
	struct listing *l = u;
	char text[512];
	struct stat st;
	char when[64] = "?";

	if (!read_text(path, text, sizeof(text)))
		snprintf(text, sizeof(text), "(no text)");
	/* WHEN IT WAS ASKED FOR, and not the slot. The slot is in the row, in
	 * snooze's syntax; the file's mtime is the moment a person typed it,
	 * which is what they recognise a reminder by. */
	if (stat(path, &st) == 0) {
		struct tm tm;

		localtime_r(&st.st_mtime, &tm);
		strftime(when, sizeof(when), "%a %e %b %H:%M", &tm);
	}
	/* AN EXPLICIT PRECISION, not a wide field trimmed by luck: a listing row
	 * is one line and a long reminder truncates ON PURPOSE. */
	if (l->n < REMIND_MAX)
		snprintf(l->line[l->n++], sizeof(l->line[0]), "%s  %s  %.100s",
			 id, when, text);
	return 1;
}

static int cmd_ls(void)
{
	static struct listing l;
	char body[512] = { 0 };
	int cnt;

	l.n = 0;
	each(ls_one, &l);
	cnt = l.n;
	if (isatty(STDOUT_FILENO) || !cnt) {
		if (!cnt)
			printf("no reminders\n");
		for (int i = 0; i < cnt; i++)
			printf("%s\n", l.line[i]);
		return 0;
	}
	/*
	 * NO TERMINAL, SO A TOAST. The chord that lists reminders has nowhere
	 * to print, and a command that wrote to a stdout nobody is reading
	 * would look like a chord that does nothing.
	 */
	for (int i = 0; i < cnt && i < 4; i++) {
		size_t o = strlen(body);

		snprintf(body + o, sizeof(body) - o, "%s%s", o ? "\n" : "",
			 l.line[i]);
	}
	kb_notify("kdos", cnt == 1 ? "1 reminder" : "Reminders", body);
	return 0;
}

static int rm_one(const char *path, const char *id, void *u)
{
	(void)id;
	(void)u;
	return unlink(path) == 0;
}

static int cmd_clear(void)
{
	int n = each(rm_one, NULL);
	char body[64];

	/*
	 * THE ARMED snooze IS LEFT ALONE. It will wake at its slot, find no
	 * file and deliver nothing — which is the same test `fire` makes for
	 * the reminder that has already been given, and one rule is better
	 * than a second one that hunts processes.
	 */
	snprintf(body, sizeof(body), "%d removed", n);
	if (isatty(STDOUT_FILENO))
		printf("%s\n", body);
	else
		kb_notify("kdos", "Reminders cleared", body);
	return 0;
}

struct fire {
	const char *want;
	int done;
};

static int fire_one(const char *path, const char *id, void *u)
{
	struct fire *f = u;
	char text[512];

	if (strcmp(id, f->want))
		return 0;
	if (!read_text(path, text, sizeof(text)))
		snprintf(text, sizeof(text), "(no text)");
	/*
	 * NOWHERE TO APPEAR IS NOT DELIVERED. A reminder that came due while
	 * nothing was listening must stay a reminder, or a machine that
	 * happened to have no session at the wrong minute silently eats it.
	 */
	if (!kb_have_prog("gdbus") ||
	    (!getenv("DBUS_SESSION_BUS_ADDRESS") && !getenv("KDOS_CON")))
		return 0;
	kb_notify("kdos", "Reminder", text);
	/* REMOVED BEFORE RETURNING, and that is what makes it fire once: the
	 * other snooze armed against the same file finds nothing. */
	unlink(path);
	f->done = 1;
	return 1;
}

static int cmd_fire(int argc, char **argv)
{
	struct fire f = { NULL, 0 };

	if (argc < 1)
		return usage();
	f.want = argv[0];
	each(fire_one, &f);
	return 0;
}

/*
 * THE ONE-ROW PROMPT, AND WHY IT IS A VERB.
 *
 * A chord runs ONE command and there is no shell anywhere on that path, so
 * `kdos-prompt --input | kdos remind` cannot be a chord. This is the pipe,
 * inside the program that would have been on the right of it.
 *
 * THE WHOLE LINE IS THE ARGUMENT. A person types `in 20m tea`, which is what
 * they would have typed at a prompt — a dialog with a field for the time and a
 * field for the text would be two boxes for one sentence.
 */
static int cmd_ask(void)
{
	KbArgv a = { 0 };
	char line[512];
	char *word[64];
	int n = 0;
	char *p, *save;

	if (!kb_have_prog("kdos-prompt"))
		kb_die("kdos-prompt is not installed");
	kb_argv_add(&a, "kdos-prompt");
	kb_argv_add(&a, "--input");
	kb_argv_add(&a, "--message");
	kb_argv_add(&a, "Remind me to…");
	kb_argv_add(&a, "--placeholder");
	kb_argv_add(&a, "in 20m tea");
	kb_argv_end(&a);
	/* 254 is Escape and 0 with nothing is an empty box; neither is an
	 * error and neither says anything. */
	if (kb_run_capture(&a, line, sizeof(line)) != 0 || !line[0])
		return 0;

	for (p = strtok_r(line, " \t", &save); p && n < 63;
	     p = strtok_r(NULL, " \t", &save))
		word[n++] = p;
	word[n] = NULL;
	if (n < 3)
		return usage();
	return cmd_set(n, word);
}

int remind_main(int argc, char **argv)
{
	if (argc < 2)
		return usage();
	if (!strcmp(argv[1], "--ask"))
		return cmd_ask();
	if (!strcmp(argv[1], "ls"))
		return cmd_ls();
	if (!strcmp(argv[1], "clear"))
		return cmd_clear();
	if (!strcmp(argv[1], "fire"))
		return cmd_fire(argc - 2, argv + 2);
	if (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help"))
		return usage();
	return cmd_set(argc - 1, argv + 1);
}
