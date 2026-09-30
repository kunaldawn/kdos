/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-chars — the character map, Sidekick's ASCII table with a search box
 *
 *   ┌ characters ──────────────────────────────────┐
 *   │ arrow                                        │
 *   ├──────────────────────────────────────────────┤
 *   │ ← U+2190  LEFTWARDS ARROW                    │
 *   │ ↑ U+2191  UPWARDS ARROW                      │
 *   │ → U+2192  RIGHTWARDS ARROW                   │
 *   ├──────────────────────────────────────────────┤
 *   │ Enter copy  Esc close                        │
 *   └──────────────────────────────────────────────┘
 *
 * THE INDEX IS MMAPPED AND NEVER READ. `mkcharidx` writes it at build time
 * against ICU; this maps it and searches it in place. Forty thousand names are
 * about a megabyte of text, and a read into a heap buffer would be a megabyte
 * of copying on every summon of an accessory whose whole point is that it
 * opens instantly — and a megabyte of dirty pages per surface rather than one
 * page cache every surface shares.
 *
 * A SEARCH PER KEYSTROKE, over the whole blob, with no index on the text. A
 * substring scan of a megabyte is well under a millisecond and a prefix tree
 * would answer a different question: somebody looking for an arrow types
 * `arrow`, and every useful name has it in the middle.
 *
 * THE BLOB IS ALREADY UPPER CASE, so only the query is folded. Folding the
 * names instead would be forty thousand copies per keystroke to avoid folding
 * eight characters once.
 *
 * WHAT IS COPIED IS THE CHARACTER, NOT ITS NAME OR ITS NUMBER. A character map
 * exists to put a character somewhere; a person who wanted `U+2192` would type
 * it.
 * ---------------------------------
 */

#define _POSIX_C_SOURCE 200809L
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "charidx.h"
#include "kbase.h"
#include "kwl.h"
#include "shell.h"

#define CH_COLS 56
#define CH_ROWS 20
#define CH_QUERY 48
#define CH_HITS 512

/* No layers: Esc closes the map, and the pool answers that with CLOSE. */
static KtuiKeys keys;

static const struct kchr_head *idx;
static const struct kchr_entry *ent;
static const char *blob;
static size_t mapped;
static const char *why;		/* why there is no index, said on the surface */

static char query[CH_QUERY];
static KtuiField qf = { query, sizeof(query), 0, 0, NULL };
static uint32_t hits[CH_HITS];
static int nhits;
static int truncated;
static KtuiTable tbl;
static char note[64];

static void idx_open(void)
{
	const char *path = getenv("KDOS_CHARIDX");
	struct stat st;
	int fd;

	if (!path || !*path)
		path = KCHR_PATH;
	fd = open(path, O_RDONLY | O_CLOEXEC);
	if (fd < 0) {
		why = "no character index on this image";
		return;
	}
	if (fstat(fd, &st) != 0 || (size_t)st.st_size < sizeof(*idx)) {
		close(fd);
		why = "the character index is truncated";
		return;
	}
	void *p = mmap(NULL, (size_t)st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);

	close(fd);		/* the mapping holds the file open */
	if (p == MAP_FAILED) {
		why = "the character index could not be mapped";
		return;
	}
	mapped = (size_t)st.st_size;
	idx = p;
	if (memcmp(idx->magic, "KCHR", 4) != 0 ||
	    idx->version != KCHR_VERSION) {
		why = "the character index is from another image";
		idx = NULL;
		return;
	}
	/*
	 * THE SIZES ARE CHECKED AGAINST THE FILE, not trusted. This is a
	 * mapped file with offsets in it: a header claiming more entries than
	 * the file holds would walk this process off the end of the mapping.
	 */
	size_t need = sizeof(*idx) + (size_t)idx->count * sizeof(*ent) +
		      idx->blob;

	if (need > mapped) {
		why = "the character index is truncated";
		idx = NULL;
		return;
	}
	ent = (const struct kchr_entry *)((const char *)p + sizeof(*idx));
	blob = (const char *)ent + (size_t)idx->count * sizeof(*ent);
}

/* The name of entry `i`, into a caller's buffer. Bounded by the blob, so an
 * entry pointing past it prints nothing rather than the next name. */
static void name_of(int i, char *out, size_t cap)
{
	uint32_t off = ent[i].off, len = ent[i].len;

	if (off > idx->blob || len > idx->blob - off)
		len = 0;
	if (len > cap - 1)
		len = (uint32_t)cap - 1;
	memcpy(out, blob + off, len);
	out[len] = '\0';
}

static void search(void)
{
	char up[CH_QUERY];
	size_t qn = strlen(query);

	nhits = 0;
	truncated = 0;
	tbl.sel = 0;
	tbl.top = 0;
	if (!idx)
		return;

	for (size_t i = 0; i < qn; i++)
		up[i] = query[i] >= 'a' && query[i] <= 'z'
				? (char)(query[i] - 'a' + 'A')
				: query[i];
	up[qn] = '\0';

	/*
	 * AN EMPTY QUERY IS THE FIRST PAGE, NOT EVERY CHARACTER. A map that
	 * opened on all of Unicode would put the reader at U+0020 with a
	 * hundred and fifty thousand rows below and nothing to look at.
	 */
	for (uint32_t i = 0; i < idx->count && nhits < CH_HITS; i++) {
		if (qn) {
			char nm[256];

			name_of((int)i, nm, sizeof(nm));
			if (!strstr(nm, up))
				continue;
		}
		hits[nhits++] = i;
	}
	if (nhits == CH_HITS)
		truncated = 1;
}

static const KtuiCol CH_COL[] = {
	{ NULL, 2, 0 },		/* the character itself                    */
	{ NULL, 8, 0 },		/* U+XXXX                                  */
	{ NULL, 0, 0 }		/* the name                                */
};
#define CH_NCOL 3

static void ch_cell(int row, int col, int x, int y, int w, int fg, int bg,
		    void *user)
{
	const struct kchr_entry *e = &ent[hits[row]];
	char buf[256];

	(void)user;
	switch (col) {
	case 0:
		buf[ktui_utf8_encode(e->cp, buf)] = '\0';
		ktui_draw_text(x, y, w, buf, fg, bg, KT_A_NONE);
		break;
	case 1:
		ktui_draw_textf(x, y, w, bg == KT_ACCENT ? KT_SURFACE : KT_DIM,
				bg, KT_A_NONE, "U+%04X", e->cp);
		break;
	default:
		name_of((int)hits[row], buf, sizeof(buf));
		ktui_draw_text(x, y, w, buf, fg, bg, KT_A_NONE);
		break;
	}
}

static void copy_selected(void)
{
	char buf[8];
	int n;

	if (!nhits || tbl.sel >= nhits)
		return;
	n = ktui_utf8_encode(ent[hits[tbl.sel]].cp, buf);
	buf[n] = '\0';
	/* kdisp_copy, not ktui_clip_copy: OSC 52 is a terminal's channel and
	 * this surface is an overlay and never a window.
	 * The seam picks whichever the session actually has. */
	if (kdisp_copy(buf, (size_t)n, 0))
		snprintf(note, sizeof(note), "copied %s", buf);
	else
		snprintf(note, sizeof(note), "%s", "nothing to copy onto");
}

static void draw(void)
{
	int w = ktui_w, h = ktui_h;

	ktui_draw_fill(krect(0, 0, w, h), KT_SURFACE);
	ktui_draw_box(krect(0, 0, w, h), "characters", KT_ACCENT, KT_SURFACE, 1);

	/* The query row, then a rule, then the hits. */
	ktui_draw_text(2, 1, w - 4, query[0] ? query : "type a name",
		       query[0] ? KT_TEXT : KT_DIM, KT_SURFACE, KT_A_NONE);
	ktui_draw_hline(1, 2, w - 2, KT_G_HL, KT_DIM, KT_SURFACE);

	if (why) {
		ktui_draw_text(2, 4, w - 4, why, KT_ERR, KT_SURFACE,
			       KT_A_NONE);
	} else if (!nhits) {
		ktui_draw_text(2, 4, w - 4, "no character has that in its name",
			       KT_MID, KT_SURFACE, KT_A_NONE);
	} else {
		ktui_table_draw(krect(2, 3, w - 4, h - 6), &tbl, nhits, CH_COL,
				CH_NCOL, ch_cell, NULL, NULL, -1);
	}

	char stat[48];
	int sw;

	snprintf(stat, sizeof(stat), "%d%s", nhits, truncated ? "+" : "");
	ktui_draw_text(2, h - 2, w - 4, note[0] ? note : stat, KT_MID,
		       KT_SURFACE, KT_A_NONE);
	sw = (int)strlen(note[0] ? note : stat) + 3;
	ktui_hint("Enter", "copy");
	ktui_hint("Esc", ktui_esc_verb(&keys));
	ktui_hint_row(&keys, krect(2 + sw, h - 2, w - 4 - sw, 1), KT_SURFACE);
	ktui_term_caret(2 + ktui_field_col(&qf), 1);
}

static int ch_arg(int argc, char **argv, int *i)
{
	(void)argc;
	if (argv[*i][0] == '-')
		return 0;
	snprintf(query, sizeof(query), "%s", argv[*i]);
	return 1;
}

static void ch_start(int dump)
{
	(void)dump;
	qf.caret = (int)strlen(query);
	idx_open();
	search();
}

/* A paste is a queue and not an event: offered on every wake. */
static void ch_wake(void)
{
	if (ktui_field_key(&qf, NULL) & KTUI_FIELD_CHANGED)
		search();
}

/*
 * THE POINTER PICKS A CHARACTER AND COPIES IT. A character map is the surface
 * somebody opens BECAUSE they do not know how to type the thing they want, so
 * the one glyph they came for is taken by a press as well as by Enter.
 */
static int ch_mouse(const KtuiEvent *ev)
{
	KRect tr = krect(2, 3, ktui_w - 4, ktui_h - 6);
	int i;

	if (ev->btn == KT_MB_WHEEL_UP || ev->btn == KT_MB_WHEEL_DOWN) {
		ktui_table_key(&tbl, nhits, ktui_h - 6,
			       ev->btn == KT_MB_WHEEL_UP ? KT_K_UP : KT_K_DOWN,
			       NULL, NULL);
		return SH_EV_TAKEN;
	}
	if (ev->press != KT_MP_PRESS)
		return SH_EV_PASS;
	if (ev->btn == KT_MB_RIGHT)
		return SH_EV_CLOSE;
	if (ev->btn != KT_MB_LEFT)
		return SH_EV_PASS;
	i = ktui_table_hit(tr, &tbl, nhits, CH_NCOL, CH_COL, ev->mx, ev->my);
	if (i < 0)
		return SH_EV_PASS;
	if (i == tbl.sel)
		copy_selected();
	else
		ktui_table_pick(&tbl, nhits, i, NULL, NULL);
	return SH_EV_TAKEN;
}

static int ch_event(KtuiEvent *ev)
{
	if (ev->type == KT_EVT_MOUSE)
		return ch_mouse(ev);
	if (ev->type != KT_EVT_KEY)
		return SH_EV_PASS;

	note[0] = '\0';
	if (ev->key == KT_K_ENTER) {
		copy_selected();
		return SH_EV_TAKEN;
	}
	/*
	 * THE FOUR LIST KEYS BELONG TO THE LIST AND HOME AND END DO NOT. A
	 * character map is read far more than it is retyped, so Up, Down and
	 * the pages mean the list here as they do on every other surface — but
	 * there is a text field on this one, and Home and End mean its caret.
	 * The table answers all six, so the four are named rather than the
	 * call being trusted to take only what this surface meant to give it.
	 */
	if (ev->key == KT_K_UP || ev->key == KT_K_DOWN ||
	    ev->key == KT_K_PGUP || ev->key == KT_K_PGDN) {
		ktui_table_key(&tbl, nhits, ktui_h - 6, ev->key, NULL, NULL);
		return SH_EV_TAKEN;
	}

	/* Everything else is the query's, the toolkit's field. */
	if (ktui_field_key(&qf, ev) & KTUI_FIELD_CHANGED)
		search();
	return SH_EV_TAKEN;
}

int chars_main(int argc, char **argv)
{
	static const ShSurface s = {
		.cfg = {
			.role = KDISP_ROLE_OVERLAY,
			.cols = CH_COLS,
			.rows = CH_ROWS,
			.app_id = "kdos-chars",
			.keyboard = 1,
		},
		.usage = "[--font NAME] [--dump] [QUERY]",
		.keys = &keys,
		.popup = 1,
		.popup_bg = KT_SURFACE,
		.arg = ch_arg,
		.start = ch_start,
		.draw = draw,
		.event = ch_event,
		.wake = ch_wake,
	};
	int r = sh_run(&s, argc, argv);

	if (mapped)
		munmap((void *)idx, mapped);
	return r;
}
