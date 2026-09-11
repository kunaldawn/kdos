/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-contacts — Sidekick's dialer, honestly: a name in, a number out
 *
 *   ┌ contacts ────────────────────────────────────┐
 *   │ ada                                          │
 *   ├──────────────────────────────────────────────┤
 *   │ Ada Lovelace   home   ada@example.org        │
 *   │ Ada Lovelace   cell   +44 7700 900123        │
 *   ├──────────────────────────────────────────────┤
 *   │ Enter copy  Esc close                        │
 *   └──────────────────────────────────────────────┘
 *
 * SIDEKICK'S FIFTH ACCESSORY DIALED A MODEM. Its descendant is a lookup: type
 * a name, get the address or the number, copy either. Nothing here dials
 * anything, and nothing here edits a card — `khard new` is a program that
 * already exists and does that better than a window could.
 *
 * THE STORE IS `khard` AND THIS PROGRAM HOLDS NONE. A second copy of a vCard
 * parser would be a second answer to what a contact is, and the one that is
 * not the store's is the one that goes stale. Two forks, once per query, never
 * from the draw.
 *
 * TWO THINGS khard DOES THAT COST A LINE EACH:
 *
 *   - `email --parsable` PRINTS A BANNER FIRST, results or not, unless it is
 *     told not to. Without `--remove-first-line` the first row of every search
 *     is `searching for '' ...`, which reads as a contact.
 *   - AN EMPTY BOOK EXITS NON-ZERO while printing nothing. A reader that gave
 *     up on the exit status would show "no contacts" for a book with contacts
 *     in it the moment one search matched nothing, so what is read is the
 *     OUTPUT and the status is not consulted.
 *
 * WHAT IS COPIED IS THE VALUE. A person who wanted the name had it on the
 * screen already, and an accessory exists to put something somewhere.
 * ---------------------------------
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kbase.h"
#include "kwl.h"
#include "shell.h"

#define CT_COLS 56
#define CT_ROWS 20
#define CT_QUERY 48
#define CT_MAX 512
#define CT_NAME 64
#define CT_VALUE 96
#define CT_LABEL 16

/* No layers: Esc closes the accessory, and the pool answers that with CLOSE. */
static KtuiKeys keys;

struct row {
	char name[CT_NAME];
	char label[CT_LABEL];
	char value[CT_VALUE];
};

static struct row rows[CT_MAX];
static int nrows;
static int truncated;
static const char *why;		/* why there are no rows, said on the surface */

static char query[CT_QUERY];
static int caret;
static KtuiTable tbl;
static char note[64];

/*
 * ONE `khard` VERB, PARSED. `--parsable` is tab-separated `value<TAB>name<TAB>
 * type`, which is the only shape this reads — a format string would be a
 * second place the columns are named.
 */
static void take(const char *text)
{
	for (const char *p = text; p && *p && nrows < CT_MAX;) {
		const char *nl = strchr(p, '\n');
		size_t len = nl ? (size_t)(nl - p) : strlen(p);
		char line[256];
		char *t1, *t2;

		if (len >= sizeof(line))
			len = sizeof(line) - 1;
		memcpy(line, p, len);
		line[len] = '\0';
		p = nl ? nl + 1 : p + strlen(p);

		if (!line[0])
			continue;
		t1 = strchr(line, '\t');
		if (!t1)
			continue;
		*t1++ = '\0';
		t2 = strchr(t1, '\t');
		if (t2)
			*t2++ = '\0';

		struct row *r = &rows[nrows++];

		kb_strlcpy(r->value, line, sizeof(r->value));
		kb_strlcpy(r->name, t1, sizeof(r->name));
		kb_strlcpy(r->label, t2 ? t2 : "", sizeof(r->label));
	}
	if (nrows == CT_MAX)
		truncated = 1;
}

/*
 * THE HARNESS'S SEAM. A dump cannot fork `khard`: neither $PATH nor an address
 * book is fixed for one, so a file stands in, in exactly the format khard is
 * asked to print. Never set on a login.
 */
static void scan(void)
{
	const char *fixture = getenv("KDOS_CONTACT_LIST");

	nrows = 0;
	truncated = 0;
	tbl.sel = 0;
	tbl.top = 0;
	note[0] = '\0';
	why = NULL;

	if (fixture && *fixture) {
		char *text = kb_read_all(fixture, NULL);

		if (!text) {
			why = "the fixture named by $KDOS_CONTACT_LIST is not readable";
			return;
		}
		take(text);
		free(text);
		if (!nrows && !query[0])
			why = "no contacts yet — `khard new` adds one";
		return;
	}

	if (!kb_have_prog("khard")) {
		why = "khard is not on this machine — it is the address book";
		return;
	}

	static const char *const verb[2] = { "email", "phone" };

	for (int v = 0; v < 2; v++) {
		KbArgv a = { 0 };
		KbBuf out = { 0 };

		kb_argv_add(&a, "khard");
		kb_argv_add(&a, verb[v]);
		kb_argv_add(&a, "--parsable");
		/*
		 * THE BANNER, ONLY WHERE IT IS PRINTED. `email` writes
		 * `searching for '' ...` as its first line whatever it finds;
		 * `phone` does not, and passing the flag to a verb that does
		 * not take it is an error rather than a no-op.
		 */
		if (!strcmp(verb[v], "email"))
			kb_argv_add(&a, "--remove-first-line");
		if (query[0])
			kb_argv_add(&a, query);
		kb_argv_end(&a);

		/*
		 * THE OUTPUT AND NOT THE STATUS. khard exits non-zero when
		 * nothing matched, and a reader that stopped there would
		 * report an empty book for a search that simply missed.
		 */
		kb_run_capture_buf(&a, &out);
		if (out.p)
			take(out.p);
		kb_buf_free(&out);
	}

	if (!nrows && !query[0])
		why = "no contacts yet — `khard new` adds one";
}

static const KtuiCol CT_COL[] = {
	{ NULL, 18 },		/* the name                                */
	{ NULL, 7 },		/* home, cell, work                        */
	{ NULL, 0 }		/* the address or the number               */
};
#define CT_NCOL 3

static void ct_cell(int row, int col, int x, int y, int w, int fg, int bg,
		    void *user)
{
	const struct row *r = &rows[row];

	(void)user;
	switch (col) {
	case 0:
		ktui_draw_text(x, y, w, r->name, fg, bg, KT_A_NONE);
		break;
	case 1:
		ktui_draw_text(x, y, w, r->label,
			       bg == KT_ACCENT ? KT_SURFACE : KT_DIM, bg,
			       KT_A_NONE);
		break;
	default:
		ktui_draw_text(x, y, w, r->value, fg, bg, KT_A_NONE);
		break;
	}
}

static void copy_selected(void)
{
	const struct row *r;

	if (!nrows || tbl.sel >= nrows)
		return;
	r = &rows[tbl.sel];
	/* kdisp_copy, not ktui_clip_copy: OSC 52 is a terminal's channel and
	 * this surface is a Wayland overlay as often as it is a console one.
	 * The seam picks whichever the session actually has. */
	if (kdisp_copy(r->value, strlen(r->value), 0))
		snprintf(note, sizeof(note), "copied %s", r->value);
	else
		snprintf(note, sizeof(note), "%s", "nothing to copy onto");
}

static void draw(void)
{
	int w = ktui_w, h = ktui_h;

	ktui_draw_fill(krect(0, 0, w, h), KT_SURFACE);
	ktui_draw_box(krect(0, 0, w, h), "contacts", KT_ACCENT, KT_SURFACE, 1);

	ktui_draw_text(2, 1, w - 4, query[0] ? query : "type a name",
		       query[0] ? KT_TEXT : KT_DIM, KT_SURFACE, KT_A_NONE);
	ktui_draw_hline(1, 2, w - 2, KT_G_HL, KT_DIM, KT_SURFACE);

	if (why) {
		ktui_draw_text(2, 4, w - 4, why, KT_MID, KT_SURFACE,
			       KT_A_NONE);
	} else if (!nrows) {
		ktui_draw_text(2, 4, w - 4, "nobody has that in their card",
			       KT_MID, KT_SURFACE, KT_A_NONE);
	} else {
		ktui_table_draw(krect(2, 3, w - 4, h - 6), &tbl, nrows, CT_COL,
				CT_NCOL, ct_cell, NULL, NULL, -1);
	}

	char stat[48];
	int sw;

	snprintf(stat, sizeof(stat), "%d%s", nrows, truncated ? "+" : "");
	ktui_draw_text(2, h - 2, w - 4, note[0] ? note : stat, KT_MID,
		       KT_SURFACE, KT_A_NONE);
	sw = (int)strlen(note[0] ? note : stat) + 3;
	ktui_hint("Enter", "copy");
	ktui_hint("Esc", ktui_esc_verb(&keys));
	ktui_hint_row(&keys, krect(2 + sw, h - 2, w - 4 - sw, 1), KT_SURFACE);
	ktui_term_caret(2 + caret, 1);
}

int contacts_main(int argc, char **argv)
{
	const char *font = NULL;
	int dump = 0;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--font") && i + 1 < argc)
			font = argv[++i];
		else if (!strcmp(argv[i], "--dump"))
			dump = 1;
		else if (argv[i][0] != '-')
			snprintf(query, sizeof(query), "%s", argv[i]);
		else {
			fprintf(stderr, "usage: kdos-contacts [--font NAME] "
					"[--dump] [QUERY]\n");
			return 2;
		}
	}
	caret = (int)strlen(query);

	scan();

	KDispConfig cfg = {
		.role = KDISP_ROLE_OVERLAY,
		.cols = CT_COLS,
		.rows = CT_ROWS,
		.app_id = "kdos-contacts",
		.font = font,
		.keyboard = 1,
	};

	sh_theme_from_cache();
	if (dump) {
		ktui_offscreen_init(CT_COLS, CT_ROWS);
		ktui_draw_init();
		draw();
		ktui_draw_dump();
		return 0;
	}
	if (kdisp_init(&cfg, kdos_disp, kdos_disp_n) != 0) {
		fprintf(stderr, "kdos-contacts: no display server\n");
		return 1;
	}
	ktui_draw_init();
	kch_px_popup(KT_SURFACE);

	while (!kdisp_should_close()) {
		draw();
		ktui_draw_flush();

		KtuiEvent ev;

		if (!ktui_backend()->poll_event(&ev, 1000)) {
			if (ktui_resized) {
				ktui_resized = 0;
				ktui_draw_resize();
				ktui_draw_invalidate();
			}
			continue;
		}
		if (ev.type != KT_EVT_KEY)
			continue;

		/* FIRST, above this surface's own switch. */
		if (ktui_keys(&keys, &ev) == KTUI_KEY_CLOSE)
			break;

		note[0] = '\0';
		if (ev.key == KT_K_ENTER) {
			copy_selected();
			continue;
		}
		/*
		 * THE FOUR LIST KEYS BELONG TO THE LIST AND HOME AND END DO
		 * NOT. There is a text field on this surface and Home and End
		 * mean its caret; the table answers all six, so the four are
		 * named rather than the call being trusted to take only what
		 * this surface meant to give it.
		 */
		if (ev.key == KT_K_UP || ev.key == KT_K_DOWN ||
		    ev.key == KT_K_PGUP || ev.key == KT_K_PGDN) {
			ktui_table_key(&tbl, nrows, ktui_h - 6, ev.key, NULL,
				       NULL);
			continue;
		}

		int qn = (int)strlen(query);

		if (ev.key == KT_K_BACKSPACE) {
			if (caret > 0) {
				memmove(query + caret - 1, query + caret,
					(size_t)(qn - caret) + 1);
				caret--;
				scan();
			}
		} else if (ev.key == KT_K_LEFT) {
			if (caret > 0)
				caret--;
		} else if (ev.key == KT_K_RIGHT) {
			if (caret < qn)
				caret++;
		} else if (ev.key == KT_K_HOME) {
			caret = 0;
		} else if (ev.key == KT_K_END) {
			caret = qn;
		} else if (ev.key >= 0x20 && ev.key < 0x7f &&
			   qn + 1 < CT_QUERY) {
			memmove(query + caret + 1, query + caret,
				(size_t)(qn - caret) + 1);
			query[caret++] = (char)ev.key;
			scan();
		}
	}

	kdisp_shutdown();
	return 0;
}
