/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   libktui — text furniture and the console escape hatch
 * ---------------------------------
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include "kbase.h"
#include "ktui.h"

void ktui_section(int x, int y, int w, const char *title)
{
	ktui_draw_text(x, y, w, title, KT_ACCENT, KT_BG, 0);
	int tw = ktui_utf8_width(title);
	if (w > tw + 2)
		ktui_draw_hline(x + tw + 1, y, w - tw - 1, KT_G_HL, KT_DIM, KT_BG);
}

void ktui_kv(int x, int y, int w, const char *k, const char *v, int fg)
{
	ktui_draw_text(x, y, 16, k, KT_MID, KT_BG, 0);
	ktui_draw_text(x + 17, y, w - 17, v, fg, KT_BG, 0);
}

void ktui_note(int x, int y, int w, const char *s)
{
	ktui_draw_text(x, y, w, s, KT_MID, KT_BG, 0);
}

/*
 * Wrap a paragraph into `w` COLUMNS. Returns the number of lines drawn, which
 * is what every caller adds to its own y.
 *
 * Measured in columns and cut on codepoint boundaries, because `w` is a pane
 * width: counting bytes wraps a line of em dashes a third short and leaves
 * half a sequence at each end of the cut, which draws as a replacement glyph
 * and restarts the next line mid-character.
 *
 * A LINE BREAKS AT ITS LAST SPACE, OR AFTER ITS LAST '/' WHEN IT HAS NO SPACE.
 * A word longer than the line is then a path or a zone name, and cutting it
 * after a separator keeps each piece readable: "America/Argentina/" over
 * "Buenos_Aires", not "America/Argentina/Buenos" over "_Aires". A slash never
 * beats a space, so a path inside a sentence still starts its own line whole.
 * Only a run with neither is cut at the column budget.
 *
 * EVERY LINE ADVANCES `s` BY AT LEAST ONE CODEPOINT. A width that fits nothing
 * — a one-column pane under a double-width character — otherwise copies
 * nothing, advances nothing and spins here for ever.
 */
int ktui_para(int x, int y, int w, const char *s, int fg)
{
	int lines = 0;

	if (!s || w <= 0)
		return 0;
	while (*s) {
		const char *p = s, *brk = NULL, *slash = NULL;
		int cols = 0;

		while (*p) {
			uint32_t cp;
			const char *nx = ktui_utf8_next(p, &cp);
			int cw = ktui_wcwidth(cp);

			if (cols + cw > w)
				break;
			if (cp == ' ' && p > s)
				brk = p;
			else if (cp == '/' && *nx)
				slash = nx;
			cols += cw;
			p = nx;
		}
		if (!*p) {
			ktui_draw_text(x, y + lines, w, s, fg, KT_BG, 0);
			return lines + 1;
		}
		/* A line may end exactly ON the column budget when the next
		 * character is the space that separates the words: the space
		 * is dropped, not drawn, so it costs the line nothing. */
		if (*p == ' ')
			brk = p;

		const char *end = brk ? brk : slash ? slash : p;
		uint32_t cp;

		if (end == s)
			end = ktui_utf8_next(s, &cp);

		char buf[512];
		size_t n = (size_t)(end - s);

		if (n >= sizeof(buf)) {
			/* Clamped to the last WHOLE sequence that fits: a byte
			 * clamp would put the split back in at a different
			 * place. What is left of the line wraps to the next. */
			const char *q = s;

			while (q < end) {
				const char *nx = ktui_utf8_next(q, &cp);

				if ((size_t)(nx - s) >= sizeof(buf))
					break;
				q = nx;
			}
			n = (size_t)(q - s);
		}
		memcpy(buf, s, n);
		buf[n] = 0;
		ktui_draw_text(x, y + lines, w, buf, fg, KT_BG, 0);
		lines++;
		s += n;
		while (*s == ' ')
			s++;
	}
	return lines;
}

/* ──────────────────────────────────────────────────────────────────────── */

/*
 * ONE RULE FOR BOTH INSTALLERS: by length alone, in characters, as the
 * graphical installer's strength hint rates it. Under 8 is short, 8 to 15
 * fair, 16 and over long. A second rule here would rate the same password
 * two ways on one machine, depending on which installer asked.
 */
int ktui_pw_score(const char *p)
{
	int len = 0;

	while (*p) {
		uint32_t cp;

		p = ktui_utf8_next(p, &cp);
		len++;
	}
	return len < 8 ? 0 : len < 16 ? 1 : 2;
}

void ktui_pw_meter(int x, int y, int w, const char *p)
{
	/* THE BAR IS THE PART READ AT A GLANCE, so every score moves it: the
	 * lit count is derived from the number of scores rather than from a
	 * step size, or the top score saturates the bar early and two scores
	 * paint the same picture. */
	enum { PW_CELLS = 12, PW_STEPS = 3 };	/* names[] has PW_STEPS entries */
	static const char *names[PW_STEPS] = { "short", "fair", "long, good" };
	if (!*p) {
		ktui_note(x, y, w, "empty");
		return;
	}
	int s = ktui_pw_score(p);
	int fg = s <= 0 ? KT_WARN : KT_ACCENT;
	int lit = (s + 1) * PW_CELLS / PW_STEPS;	/* 4, 8, 12 */

	ktui_draw_hline(x, y, lit, KT_G_FULL, fg, KT_BG);
	ktui_draw_hline(x + lit, y, PW_CELLS - lit, KT_G_SHADE, KT_DIM, KT_BG);
	ktui_draw_text(x + 14, y, w - 14, names[s], fg, KT_BG, 0);
}

/* ──────────────────────────────────────────────────────────────────────── */

void ktui_toosmall(const char *title, int min_w, int min_h)
{
	ktui_draw_clear();
	char m[80];
	snprintf(m, sizeof(m), "terminal is %dx%d, need at least %dx%d", ktui_w,
		 ktui_h, min_w, min_h);
	ktui_draw_text(1, ktui_h / 2 - 1, ktui_w - 2, title, KT_ACCENT, KT_BG, 0);
	ktui_draw_text(1, ktui_h / 2, ktui_w - 2, m, KT_ERR, KT_BG, 0);
	ktui_draw_text(1, ktui_h / 2 + 1, ktui_w - 2,
		       "resize the window and it will come back", KT_MID, KT_BG, 0);
	ktui_draw_flush();
}

int ktui_run_console(char *const argv[])
{
	ktui_term_suspend();
	ktui_input_suspend();
	pid_t pid = fork();
	if (pid == 0) {
		execvp(argv[0], argv);
		_exit(127);
	}
	int st = 0;
	if (pid > 0)
		waitpid(pid, &st, 0);
	fputs("\n\033[0m-- press Enter to return --", stdout);
	fflush(stdout);
	int c;
	while ((c = getchar()) != '\n' && c != EOF)
		;
	ktui_input_resume();
	ktui_term_resume();
	return st;
}
