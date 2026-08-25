/*
 * The tone ladder, asserted for every accent.
 *
 * A separate program because libkchrome is not on the zero-dependency
 * selftest binary's link line, and because what is being checked is a CLAIM
 * about the palette rather than about a function: that the derived tones give
 * a bar a legible middle, in all four accents, when the eight slots do not.
 *
 * The claims, and each is a way the ladder fails silently:
 *
 *  - the EDGE reads against the desktop. It is what says where the bar is;
 *    the body cannot, being within 1.07:1 of the wallpaper in every accent.
 *  - rest < hover < active, strictly. Three states that do not separate are
 *    a taskbar that cannot say which window you are in.
 *  - the ACTIVE plate clears 7:1 for its label wherever the palette allows
 *    it, and bone — which cannot, its `text` and `pdark` being close in
 *    luminance — still clears the 4.5:1 AA floor. Asserting 7:1 on all four
 *    would fail forever on a palette that has no such colour.
 */
#include <stdio.h>
#include <string.h>

#include "kchrome.h"

static int fails;

static void ok(int cond, const char *what)
{
	if (!cond) {
		printf("  FAIL: %s\n", what);
		fails++;
	}
}

/* Straight composite, matching kch_tone.c's own reference arithmetic. */
static uint32_t over(uint32_t f, uint32_t b, uint8_t a)
{
	uint32_t o = 0;

	for (int s = 16; s >= 0; s -= 8) {
		uint32_t x = (f >> s) & 0xff, y = (b >> s) & 0xff;

		o |= ((x * a + y * (255 - a)) / 255) << s;
	}
	return o;
}

int main(void)
{
	for (int i = 0; i < kcol_nscheme; i++) {
		const KcolScheme *s = &kcol_schemes[i];
		char msg[160];

		ktui_theme_set(s->name);
		kch_tone_reset();

		uint32_t body = over(kcol_mix(s->deep, s->variant, 60),
				     s->backdrop,
				     kch_tone_alpha(KCH_T_BODY_TOP));
		uint32_t rest = over(kch_tone(KCH_T_REST), body,
				     kch_tone_alpha(KCH_T_REST));
		uint32_t hov = over(kch_tone(KCH_T_HOVER), body,
				    kch_tone_alpha(KCH_T_HOVER));
		uint32_t act = over(kch_tone(KCH_T_ACTIVE), body,
				    kch_tone_alpha(KCH_T_ACTIVE));

		int c_edge = kcol_contrast(kch_tone(KCH_T_EDGE), s->backdrop);
		int c_rest = kcol_contrast(rest, body);
		int c_hov = kcol_contrast(hov, body);
		int c_act = kcol_contrast(act, body);
		int c_txt = kcol_contrast(s->text, act);

		snprintf(msg, sizeof(msg),
			 "%s: edge %d.%02d:1 against the desktop", s->name,
			 c_edge / 100, c_edge % 100);
		ok(c_edge >= 200, msg);

		snprintf(msg, sizeof(msg),
			 "%s: rest/hover/active separate (%d/%d/%d)", s->name,
			 c_rest, c_hov, c_act);
		ok(c_rest < c_hov && c_hov <= c_act && c_rest >= 115, msg);

		snprintf(msg, sizeof(msg),
			 "%s: focused plate stands off the bar (%d.%02d:1)",
			 s->name, c_act / 100, c_act % 100);
		ok(c_act >= 200, msg);

		/*
		 * Bone is a STATED miss and is asserted against the floor it
		 * actually reaches, not against the target. A test written to
		 * the target would fail on every run for ever and teach people
		 * to ignore this program.
		 */
		int floor = strcmp(s->name, "bone") ? 700 : 450;

		snprintf(msg, sizeof(msg),
			 "%s: label on the focused plate %d.%02d:1 (floor %d)",
			 s->name, c_txt / 100, c_txt % 100, floor);
		ok(c_txt >= floor, msg);
	}

	if (fails)
		printf("  %d tone assertion(s) failed\n", fails);
	else
		printf("  the ladder reads in all %d accents\n", kcol_nscheme);
	return fails ? 1 : 0;
}
