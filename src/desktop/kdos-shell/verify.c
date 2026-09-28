/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-verify — are these files what the list says they are
 *
 *   ┌ Verify ────────────────────────────────────────────────────┐
 *   │ LIST  /home/kdos/Downloads/SHA256SUMS — sha256               │
 *   │ FILE                                         RESULT          │
 *   │ kdos-0.2.iso                                 OK              │
 *   │ kdos-0.2.iso.sig                             FAILED          │
 *   ├──────────────────────────────────────────────────────────────┤
 *   │ 1 matches, 1 does not        [ List ] [ Check ] [ Repair ]   │
 *   └──────────────────────────────────────────────────────────────┘
 *
 * THE CHECKER DECIDES; THIS DRAWS ITS ANSWER. `sha256sum -c`, `sha512sum -c`,
 * `b3sum -c` and `par2 verify` each already read a list and the files it
 * names, and a surface that hashed files itself would be a second answer to
 * whether a download is intact — the one that is not the tool a person would
 * type is the one nobody trusts.
 *
 * WHICH CHECKER IS THE LIST'S NAME, in any case. `.par2` is par2; a name
 * ending `.b3` or holding `b3sum` or `blake3` is b3sum; `sha512` is sha512sum;
 * anything else — SHA256SUMS, `.sha256`, `.sums` — is sha256sum, which is what
 * a download page publishes. A bare `b3` anywhere in the name is not enough:
 * it is in too many file names that have nothing to do with BLAKE3.
 *
 * IT RUNS WHERE THE LIST IS. Every one of these lists names its files relative
 * to itself, so the checker is started in the list's folder with the list's
 * own name; started anywhere else it reports every file missing.
 *
 * REPAIR IS par2's AND ONLY par2's. A checksum can say a file is wrong; a
 * par2 set carries the recovery blocks to put it right, and `par2 repair`
 * rewrites the damaged file in place, keeping the old one beside it as
 * `<name>.1`. Offered only after a verify has said a repair is possible.
 * ---------------------------------
 */

#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "kbase.h"
#include "kwl.h"
#include "shell.h"

#define VF_COLS 74
#define VF_ROWS 20
#define VF_MAX 4096
#define VF_PATH 1024

enum { VB_LIST, VB_CHECK, VB_REPAIR, VB_CLOSE, VB_N };
enum { TOOL_SHA256, TOOL_SHA512, TOOL_B3, TOOL_PAR2 };
static const char *const TOOL_NAME[] = { "sha256", "sha512", "blake3",
					 "par2" };

struct row {
	char name[256];
	char result[48];
	int bad;
};

static struct row *rows;
static int nrow, nbad;
static KtuiTable tbl;
static KtuiKeys keys;
static int icons_on = 1;

static char list[VF_PATH];
static int tool;
static char status[192];
static ShJob job;
static int repairing;
/* par2's exit status: 0 intact, 1 repairable, 2 not. */
static int par2_state = -1;

enum { PR_NONE, PR_LIST };
static int prompt;
static char answer[VF_PATH];

/* ── the check ─────────────────────────────────────────────────────────── */

static int pick_tool(const char *path)
{
	char b[256];
	size_t n;

	snprintf(b, sizeof(b), "%s", kb_basename(path));
	for (char *p = b; *p; p++)
		*p = (char)tolower((unsigned char)*p);
	n = strlen(b);
	if (n > 5 && !strcmp(b + n - 5, ".par2"))
		return TOOL_PAR2;
	if ((n > 3 && !strcmp(b + n - 3, ".b3")) || strstr(b, "b3sum") ||
	    strstr(b, "blake3"))
		return TOOL_B3;
	if (strstr(b, "sha512"))
		return TOOL_SHA512;
	return TOOL_SHA256;
}

static void add_row(const char *name, const char *result, int bad)
{
	struct row *r;

	if (!rows || nrow >= VF_MAX)
		return;
	r = &rows[nrow++];
	snprintf(r->name, sizeof(r->name), "%s", name);
	snprintf(r->result, sizeof(r->result), "%s", result);
	r->bad = bad;
	nbad += bad;
}

/*
 * One line from the checker. A `-c` line is `<name>: OK` or `<name>: FAILED`
 * with a reason after it, and the name may itself hold `: `, so the verdict is
 * found from the END. par2 says `Target: "<name>" - found.` or `- damaged.`
 * or `- missing.`. Everything else — warnings, par2's scanning chatter — is
 * not a row; the newest line is still the status while it runs.
 */
static void on_line(const char *ln, void *user)
{
	(void)user;
	if (tool == TOOL_PAR2) {
		const char *q = strstr(ln, "Target: \""), *e, *v;
		char name[256];

		if (!q)
			return;
		q += 9;
		if (!(e = strstr(q, "\" - ")))
			return;
		snprintf(name, sizeof(name), "%.*s", (int)(e - q), q);
		v = e + 4;
		add_row(name, v, strncmp(v, "found", 5) != 0);
		return;
	}
	const char *ok = strstr(ln, ": OK");
	const char *bad = NULL;

	for (const char *p = strstr(ln, ": FAILED"); p;
	     p = strstr(p + 1, ": FAILED"))
		bad = p;
	if (ok && ok[4] == '\0') {
		char name[256];

		snprintf(name, sizeof(name), "%.*s", (int)(ok - ln), ln);
		add_row(name, "OK", 0);
	} else if (bad) {
		char name[256];

		snprintf(name, sizeof(name), "%.*s", (int)(bad - ln), ln);
		add_row(name, bad + 2, 1);
	}
}

static void summarise(void)
{
	int good = nrow - nbad;

	if (repairing) {
		snprintf(status, sizeof(status), job.status == 0
			 ? "repaired — the damaged files were rewritten, the "
			   "old ones kept as .1"
			 : "par2 could not repair: %.100s", job.last);
		repairing = 0;
		return;
	}
	if (tool == TOOL_PAR2)
		par2_state = job.status;
	if (!nrow)
		snprintf(status, sizeof(status), "%.150s",
			 job.last[0] ? job.last : "the list named nothing");
	else if (!nbad)
		snprintf(status, sizeof(status), "all %d match", nrow);
	else if (tool == TOOL_PAR2 && job.status == 1)
		snprintf(status, sizeof(status), "%d damaged or missing — "
						 "repair is possible", nbad);
	else
		snprintf(status, sizeof(status), "%d match%s, %d do%s not",
			 good, good == 1 ? "es" : "", nbad,
			 nbad == 1 ? "es" : "");
}

static void run(int repair)
{
	static char dir[VF_PATH], base[VF_PATH];
	const char *av[8];
	int n = 0;

	if (!list[0] || job.running)
		return;
	snprintf(dir, sizeof(dir), "%s", list);
	char *sl = strrchr(dir, '/');

	if (sl) {
		snprintf(base, sizeof(base), "%s", sl + 1);
		if (sl == dir)
			sl[1] = '\0';
		else
			*sl = '\0';
	} else {
		snprintf(base, sizeof(base), "%s", list);
		snprintf(dir, sizeof(dir), ".");
	}
	switch (tool) {
	case TOOL_PAR2:
		av[n++] = "par2";
		av[n++] = repair ? "repair" : "verify";
		av[n++] = "--";
		break;
	case TOOL_B3:
		av[n++] = "b3sum";
		av[n++] = "-c";
		break;
	case TOOL_SHA512:
		av[n++] = "sha512sum";
		av[n++] = "-c";
		break;
	default:
		av[n++] = "sha256sum";
		av[n++] = "-c";
		break;
	}
	av[n++] = base;
	av[n] = NULL;
	nrow = nbad = 0;
	tbl.sel = tbl.top = 0;
	repairing = repair;
	par2_state = -1;
	job.line = on_line;
	job.user = NULL;
	if (sh_job_start(&job, av, dir) != 0) {
		snprintf(status, sizeof(status), "%.150s",
			 job.last[0] ? job.last : "the checker will not start");
		return;
	}
	snprintf(status, sizeof(status), repair ? "repairing" : "checking");
}

static void set_list(const char *path)
{
	struct stat st;

	if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) {
		snprintf(status, sizeof(status), "%.150s is not a file", path);
		return;
	}
	snprintf(list, sizeof(list), "%s", path);
	tool = pick_tool(list);
	nrow = nbad = 0;
	run(0);
}

/* ── drawing ───────────────────────────────────────────────────────────── */

static const KtuiCol VF_COL[] = { { "FILE", 0 }, { "RESULT", 24 } };
#define VF_NCOL 2

static void vf_cell(int idx, int col, int x, int y, int w, int fg, int bg,
		    void *user)
{
	const struct row *r = &rows[idx];
	int on = bg == KT_ACCENT;

	(void)user;
	if (col == 0)
		ktui_draw_text(x, y, w, r->name, fg, bg, KT_A_NONE);
	else
		ktui_draw_text(x, y, w, r->result,
			       on ? KT_SURFACE : r->bad ? KT_ERR : KT_ACCENT,
			       bg, KT_A_NONE);
}

static int body_top;

static KRect list_rect(void)
{
	return krect(2, body_top + 2, ktui_w - 4, ktui_h - body_top - 6);
}

static void draw(void)
{
	int w = ktui_w, h = ktui_h;
	int busy = job.running;

	ktui_draw_fill(krect(0, 0, w, h), KT_BG);
	sh_frame(w, h, "Verify", KT_ACCENT, KT_BG, 1);
	body_top = kch_header(w, "security-high", "Verify",
			      "files against a checksum list or a par2 set",
			      icons_on);

	ktui_draw_text(2, body_top, 6, "LIST", KT_MID, KT_BG, KT_A_NONE);
	if (list[0])
		ktui_draw_textf(8, body_top, w - 10, KT_TEXT, KT_BG, KT_A_NONE,
				"%s — %s", list, TOOL_NAME[tool]);
	else
		ktui_draw_text(8, body_top, w - 10, "none chosen", KT_MID,
			       KT_BG, KT_A_NONE);

	if (nrow)
		ktui_table_draw(list_rect(), &tbl, nrow, VF_COL, VF_NCOL,
				vf_cell, NULL, NULL, -1);

	ktui_draw_hline(1, h - 4, w - 2, KT_G_HL, KT_DIM, KT_BG);

	struct kch_button b[VB_N];

	b[VB_LIST] = (struct kch_button){ "List", !busy };
	b[VB_CHECK] = (struct kch_button){ "Check", list[0] && !busy };
	b[VB_REPAIR] = (struct kch_button){ "Repair", tool == TOOL_PAR2 &&
					    par2_state == 1 && !busy };
	b[VB_CLOSE] = (struct kch_button){ "Close", 1 };

	int bx = kch_buttons(w, h - 2, b, VB_N, -1);

	if (prompt == PR_LIST)
		ktui_draw_textf(2, h - 3, w - 4, KT_TEXT, KT_BG, KT_A_NONE,
				"Checksum list or .par2 (a path, or drop it "
				"here): %s", answer);
	else if (status[0] && bx - 3 > 0)
		ktui_draw_text(2, h - 2, bx - 3, status,
			       nbad && !busy ? KT_WARN : KT_MID, KT_BG,
			       KT_A_NONE);

	ktui_hint_if(!prompt && !busy, "l", "list");
	ktui_hint_if(!prompt && list[0] && !busy, "c", "check");
	ktui_hint_if(!prompt && tool == TOOL_PAR2 && par2_state == 1 && !busy,
		     "p", "repair");
	ktui_hint_if(prompt != 0, "Enter", "use it");
	ktui_hint("Esc", ktui_esc_verb(&keys));
	ktui_hint_row(&keys, krect(2, h - 3 + (prompt ? 1 : 0), w - 4, 1),
		      KT_BG);
	if (prompt)
		ktui_term_caret(-1, -1);
}

/* ── keys ──────────────────────────────────────────────────────────────── */

static int prompt_up(void *user)
{
	(void)user;
	return prompt != PR_NONE;
}

static void prompt_down(void *user)
{
	(void)user;
	prompt = PR_NONE;
}

static void prompt_open(void)
{
	prompt = PR_LIST;
	status[0] = '\0';
	snprintf(answer, sizeof(answer), "%s", list);
}

static void on_key(int k)
{
	if (prompt == PR_LIST) {
		size_t n = strlen(answer);

		if (k == KT_K_ENTER) {
			prompt = PR_NONE;
			if (answer[0])
				set_list(answer);
		} else if (k == KT_K_BACKSPACE) {
			if (n)
				answer[n - 1] = '\0';
		} else if (k >= 0x20 && k < 0x7f && n + 1 < sizeof(answer)) {
			answer[n] = (char)k;
			answer[n + 1] = '\0';
		}
		return;
	}
	if (ktui_table_key(&tbl, nrow, ktui_h - body_top - 6, k, NULL, NULL))
		return;
	if (job.running)
		return;
	switch (k) {
	case 'l':
		prompt_open();
		break;
	case 'c':
		run(0);
		break;
	case 'p':
		if (tool == TOOL_PAR2 && par2_state == 1)
			run(1);
		break;
	}
}

static void on_button(int bi)
{
	if (job.running)
		return;
	switch (bi) {
	case VB_LIST:
		prompt_open();
		break;
	case VB_CHECK:
		run(0);
		break;
	case VB_REPAIR:
		if (tool == TOOL_PAR2 && par2_state == 1)
			run(1);
		break;
	}
}

static void take_drop(void)
{
	size_t len = 0;
	const char *uris = ktui_drop_take(&len);
	char line[VF_PATH], path[VF_PATH];

	for (const char *p = uris; p && p < uris + len && *p;) {
		size_t k = strcspn(p, "\r\n");

		if (k && *p != '#' && k < sizeof(line)) {
			memcpy(line, p, k);
			line[k] = '\0';
			if (kb_uri_path(line, path, sizeof(path))) {
				prompt = PR_NONE;
				set_list(path);
				return;
			}
		}
		p += k;
		while (*p == '\r' || *p == '\n')
			p++;
	}
}

int verify_main(int argc, char **argv)
{
	const char *font = NULL;
	const char *first = NULL;
	int dump = 0;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--font") && i + 1 < argc)
			font = argv[++i];
		else if (!strcmp(argv[i], "--dump"))
			dump = 1;
		else if (!strcmp(argv[i], "--no-icons"))
			icons_on = 0;
		else if (argv[i][0] != '-' && !first)
			first = argv[i];
		else {
			fprintf(stderr, "usage: kdos-verify [--font NAME] "
					"[--no-icons] [--dump] [LIST]\n");
			return 2;
		}
	}
	rows = calloc(VF_MAX, sizeof(*rows));
	ktui_keys_layer(&keys, "Cancel", prompt_up, prompt_down, NULL);
	if (first)
		set_list(first);

	KDispConfig cfg = {
		.role = KDISP_ROLE_TOPLEVEL,
		.cols = VF_COLS,
		.rows = VF_ROWS,
		.min_cols = 52,
		.min_rows = 12,
		.title = "Verify",
		.app_id = "kdos-verify",
		.font = font,
		.keyboard = 1,
	};

	sh_theme_from_cache();
	if (dump) {
		/* The finished check, not the first instant of it: a golden
		 * of "checking" would hold nothing the surface is for. */
		if (job.running) {
			sh_job_wait(&job);
			summarise();
		}
		ktui_offscreen_init(VF_COLS, VF_ROWS);
		ktui_draw_init();
		draw();
		ktui_draw_dump();
		return 0;
	}
	if (kdisp_init(&cfg, kdos_disp, kdos_disp_n) != 0) {
		fprintf(stderr, "kdos-verify: no display server\n");
		return 1;
	}
	ktui_draw_init();

	while (!kdisp_should_close()) {
		if (job.running && sh_job_pump(&job)) {
			if (job.running)
				snprintf(status, sizeof(status), "%.150s",
					 job.last);
			else
				summarise();
		}
		draw();
		ktui_draw_flush();

		KtuiEvent ev;

		if (!ktui_backend()->poll_event(&ev, job.running ? 250 : 1000)) {
			if (ktui_resized) {
				ktui_resized = 0;
				ktui_draw_resize();
				ktui_draw_invalidate();
			}
			continue;
		}
		if (ev.type == KT_EVT_DROP) {
			if (!job.running)
				take_drop();
			continue;
		}
		if (ev.type == KT_EVT_MOUSE) {
			if (ev.press == KT_MP_DRAG) {
				kch_hover(ev.mx, ev.my);
				continue;
			}
			if (ev.press == KT_MP_PRESS) {
				int bi = kch_button_at(ev.mx, ev.my);

				if (bi == VB_CLOSE)
					break;
				if (bi >= 0) {
					prompt = PR_NONE;
					on_button(bi);
					continue;
				}
			}
			if (ktui_table_event(list_rect(), &tbl, nrow,
					     ktui_h - body_top - 6, VF_NCOL,
					     VF_COL, &ev, NULL, NULL) ==
			    KTUI_TABLE_CLOSE)
				break;
			continue;
		}
		if (ev.type != KT_EVT_KEY)
			continue;
		if (prompt != PR_LIST || ev.key == KT_K_ESC) {
			int r = ktui_keys(&keys, &ev);

			if (r == KTUI_KEY_CLOSE)
				break;
			if (r == KTUI_KEY_TAKEN)
				continue;
		}
		/* An unhandled Ctrl or Alt chord arrives as its letter, and
		 * must not type that letter into the path. */
		if (prompt == PR_LIST && ev.key >= 0x20 && ev.key < 0x7f &&
		    (ev.mods & (KT_MOD_CTRL | KT_MOD_ALT)))
			continue;
		on_key(ev.key);
	}
	kdisp_shutdown();
	free(rows);
	return 0;
}
