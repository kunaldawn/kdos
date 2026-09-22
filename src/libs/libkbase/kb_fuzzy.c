/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kb_fuzzy — one answer to "does this row match what was typed"
 *
 * THE POINT IS THAT THERE IS ONE OF THESE. Three surfaces search the same
 * applications — the palette, the launcher and the Start menu — and before this
 * they ranked the same query three different ways: the launcher scored a
 * subsequence with a prefix bonus and lower was better; the application index
 * did a case-insensitive SUBSTRING in three bands and higher was better; the
 * Start menu used the second through the first. A person who learns that `sm`
 * finds the system monitor in one of them and then finds nothing in another has
 * learned that the desktop's search is unreliable, which is worse than any
 * particular ranking being wrong.
 *
 * SUBSEQUENCE, NOT SUBSTRING, and that is the substantive change. `sm` has to
 * find `System Monitor`, which no substring search can do.
 *
 * HIGHER IS BETTER AND ZERO IS NO MATCH. Stated here because the two matchers
 * this replaces disagreed about that too, and a sort that kept the old
 * comparison would silently rank the list backwards while every entry in it
 * remained correct — the failure looks like bad ranking, not like a bug.
 * ---------------------------------
 */

#include <string.h>

#include "kbase.h"

static int lower(int c)
{
	return c >= 'A' && c <= 'Z' ? c + 32 : c;
}

/*
 * A WORD START IS WHERE A PERSON'S ACRONYM COMES FROM. The first character,
 * anything after a separator, and the capital in `SystemMonitor` — that last
 * one is why the test is on the pair and not on the character: without it
 * every camel-cased application id scores as one long word.
 */
static int word_start(const char *hay, int at)
{
	char prev, cur;

	if (at == 0)
		return 1;
	prev = hay[at - 1];
	cur = hay[at];
	if (prev == ' ' || prev == '-' || prev == '_' || prev == '.' ||
	    prev == '/' || prev == ':')
		return 1;
	return prev >= 'a' && prev <= 'z' && cur >= 'A' && cur <= 'Z';
}

/*
 * The ladder, in the order a person means it:
 *
 *   a prefix of the whole string          the strongest thing a query can be
 *   every character on a word start       an acronym: `sm` for System Monitor
 *   a character on a word start           each one, individually
 *   a character straight after the last   a run, rather than a scatter
 *   the first match, early                a small penalty for starting late
 *
 * The numbers matter only relative to one another, and the gaps are wide on
 * purpose: a close-run ranking that flips on a one-point change is a ranking
 * nobody can reason about from the outside.
 */
#define FZ_BASE		1
#define FZ_WORD		8
#define FZ_RUN		5
#define FZ_HEAD		12
#define FZ_ACRONYM	20
#define FZ_PREFIX	25
#define FZ_LATE_MAX	10

/*
 * One pass of the subsequence walk. With `prefer` set each needle character
 * takes the first word start that carries it; with it clear each takes the
 * leftmost occurrence. Returns 0 when the needle is not a subsequence of what
 * this pass was willing to consume.
 */
static int scan(const char *hay, const char *needle, int prefer)
{
	int score = 0, first = -1, last = -2, all_word = 1;
	int pos = 0;

	for (const char *n = needle; *n; n++) {
		int want = lower((unsigned char)*n);
		const char *h = hay + pos;
		const char *any = NULL;
		int at;

		for (; *h; h++) {
			if (lower((unsigned char)*h) != want)
				continue;
			if (!prefer || word_start(hay, (int)(h - hay)))
				break;
			if (!any)
				any = h;
		}
		if (!*h) {
			if (!any)
				return 0;	/* not a subsequence: no match */
			h = any;
		}

		at = (int)(h - hay);
		score += FZ_BASE;
		if (word_start(hay, at))
			score += FZ_WORD;
		else
			all_word = 0;
		if (at == last + 1)
			score += FZ_RUN;
		if (first < 0) {
			first = at;
			if (at == 0)
				score += FZ_HEAD;
		}
		last = at;
		pos = at + 1;
	}

	if (all_word)
		score += FZ_ACRONYM;
	if (!strncasecmp(hay, needle, strlen(needle)))
		score += FZ_PREFIX;

	/* Starting late costs, but only a little and only up to a point: past
	 * ten characters in, one position further along says nothing about
	 * whether this is the row somebody meant. */
	score -= first < FZ_LATE_MAX ? first : FZ_LATE_MAX;

	/* A match is never zero, because zero is the word for no match. */
	return score > 0 ? score : 1;
}

/*
 * A WORD START WINS OVER AN EARLIER OCCURRENCE INSIDE A WORD, BUT ONLY WHEN
 * THE WHOLE NEEDLE STILL FITS. The leftmost match is not the one a person
 * means: `sm` over `System Monitor` would take the `m` of `System` and the
 * acronym bonus — the whole reason the ladder exists — could never fire. But
 * the preference is greedy and commits, so reaching past an in-word character
 * to a later word start can put the rest of the needle out of reach: `tex`
 * over `Text Editor` would take the `E` of `Editor` and never find an `x`.
 * A preferring pass that fails is therefore redone leftmost, which is a plain
 * subsequence test — so anything that is a subsequence of the hay matches.
 */
int kb_fuzzy(const char *hay, const char *needle)
{
	int s;

	if (!hay || !needle)
		return 0;
	/*
	 * AN EMPTY QUERY MATCHES EVERYTHING, EQUALLY. A caller filtering a list
	 * as somebody types starts with nothing typed, and a zero there would
	 * empty the list before the first keystroke.
	 */
	if (!*needle)
		return 1;

	s = scan(hay, needle, 1);
	return s ? s : scan(hay, needle, 0);
}
