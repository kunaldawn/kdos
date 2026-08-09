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

#include <ctype.h>
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
	ktui_draw_text(x, y, w, s, KT_DIM, KT_BG, 0);
}

/* Wrap a paragraph into `w` columns. Returns the number of lines drawn. */
int ktui_para(int x, int y, int w, const char *s, int fg)
{
	int lines = 0;
	while (*s) {
		int take = 0, last = 0;
		for (int i = 0; i <= w && s[i]; i++) {
			if (s[i] == ' ')
				last = i;
			take = i;
		}
		if ((int)strlen(s) <= w) {
			ktui_draw_text(x, y + lines, w, s, fg, KT_BG, 0);
			return lines + 1;
		}
		if (!last)
			last = take;
		char buf[512];
		int n = last < (int)sizeof(buf) - 1 ? last : (int)sizeof(buf) - 1;
		memcpy(buf, s, (size_t)n);
		buf[n] = 0;
		ktui_draw_text(x, y + lines, w, buf, fg, KT_BG, 0);
		lines++;
		s += last;
		while (*s == ' ')
			s++;
	}
	return lines;
}

/* ──────────────────────────────────────────────────────────────────────── */

int ktui_pw_score(const char *p)
{
	int len = (int)strlen(p);
	int lower = 0, upper = 0, digit = 0, other = 0;
	for (const char *c = p; *c; c++) {
		if (islower((unsigned char)*c))
			lower = 1;
		else if (isupper((unsigned char)*c))
			upper = 1;
		else if (isdigit((unsigned char)*c))
			digit = 1;
		else
			other = 1;
	}
	int classes = lower + upper + digit + other;
	int s = 0;
	if (len >= 8)
		s++;
	if (len >= 12)
		s++;
	if (len >= 16)
		s++;
	if (classes >= 2)
		s++;
	if (classes >= 3)
		s++;
	if (len < 4)
		s = 0;
	return s > 4 ? 4 : s;
}

void ktui_pw_meter(int x, int y, int w, const char *p)
{
	static const char *names[] = { "weak", "fair", "good", "strong",
				       "excellent" };
	if (!*p) {
		ktui_note(x, y, w, "empty");
		return;
	}
	int s = ktui_pw_score(p);
	int fg = s <= 0 ? KT_ERR : s <= 1 ? KT_WARN : KT_ACCENT;
	for (int i = 0; i < 12; i++)
		ktui_draw_text(x + i, y, 1,
			       i < (s + 1) * 3 ? ktui_glyph[KT_G_FULL]
					       : ktui_glyph[KT_G_SHADE],
			       i < (s + 1) * 3 ? fg : KT_DIM, KT_BG, 0);
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
