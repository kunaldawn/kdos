/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   The Sensors page — what the board is willing to say about itself
 *
 * THE KERNEL'S OWN NAMES, and no libsensors. That library exists to parse a
 * configuration file renaming and scaling these for a human; a name it
 * invented is one nothing else on this machine agrees with, and `k10temp
 * Tctl` is what every other tool a person might cross-check with will print.
 *
 * GROUPED BY CHIP, because a channel's name is only unique within one. Two
 * chips both publishing `temp1` is the normal case and a flat list of
 * `temp1 temp1 temp1` names nothing.
 *
 * A BAR ONLY WHERE THERE IS A SCALE TO DRAW IT AGAINST. A temperature has a
 * critical point the chip itself publishes, so it has a full scale and a bar
 * is honest. A fan's RPM has no ceiling the kernel knows and a voltage's
 * nominal is not published either, so those are numbers — a bar drawn against
 * a scale nobody chose is a picture that means nothing.
 * ---------------------------------
 */

#include <stdio.h>
#include <string.h>

#include "res.h"

static KprSensor *g_sen;
static int g_nsen;

void res_sensor_prepare(void)
{
	kpr_sensors_free(g_sen);
	g_sen = NULL;
	g_nsen = kpr_sensors_list(&g_sen);
}

const char *res_sensor_headline(void)
{
	static char s[64];
	double hot = -1.0;
	int ntemp = 0, nfan = 0;

	for (int i = 0; i < g_nsen; i++) {
		if (g_sen[i].kind == KPR_SENSOR_TEMP) {
			ntemp++;
			if (g_sen[i].value > hot)
				hot = g_sen[i].value;
		} else if (g_sen[i].kind == KPR_SENSOR_FAN) {
			nfan++;
		}
	}
	if (!g_nsen)
		return "no sensor on this machine";
	/* THE HOTTEST, because that is the one question a person opens this
	 * page with. An em dash where nothing answered: a machine with no
	 * temperature sensor is not one running at zero. */
	if (hot < 0)
		snprintf(s, sizeof(s), "%d reading%s, no temperature",
			 g_nsen, g_nsen == 1 ? "" : "s");
	else
		snprintf(s, sizeof(s), "%.0f%sC hottest of %d, %d fan%s", hot,
			 ktui_glyph[KT_G_DEG], ntemp, nfan,
			 nfan == 1 ? "" : "s");
	return s;
}

/*
 * THE DEGREE SIGN COMES FROM THE GLYPH TABLE, not from this source file. The
 * console font is 512 glyphs and a character it lacks renders BLANK — so a
 * literal `°` here is a unit that silently disappears on a tty, and the tiers
 * exist precisely so a caller never has to know which font it landed on.
 */
static const char *unit_of(int kind)
{
	static char deg[8];

	switch (kind) {
	case KPR_SENSOR_TEMP:
		snprintf(deg, sizeof(deg), "%sC", ktui_glyph[KT_G_DEG]);
		return deg;
	case KPR_SENSOR_FAN:   return "rpm";
	case KPR_SENSOR_VOLT:  return "V";
	default:               return "W";
	}
}

/*
 * The slot a temperature is drawn in. The chip's own critical point is the
 * only threshold worth colouring against — a number this program invented
 * would be wrong on somebody's hardware, and a warning nobody trusts is one
 * everybody learns to ignore.
 */
static int temp_slot(const KprSensor *s)
{
	if (s->kind != KPR_SENSOR_TEMP || s->crit <= 0)
		return KT_TEXT;
	if (s->value >= s->crit)
		return KT_ERR;
	if (s->value >= s->crit * 0.9)
		return KT_WARN;
	return KT_TEXT;
}

void res_draw_sensors(int x, int y, int w, int h)
{
	char last[48] = "";
	int row = y;

	if (!g_nsen) {
		ktui_draw_text(x + 2, y + 1, w - 4,
			       "This machine publishes no hwmon or thermal "
			       "sensor.", KT_DIM, KT_BG, 0);
		return;
	}

	for (int i = 0; i < g_nsen && row < y + h; i++) {
		const KprSensor *s = &g_sen[i];
		char val[48];

		if (strcmp(s->chip, last)) {
			if (row > y)
				row++;
			if (row >= y + h)
				break;
			ktui_section(x, row, w, s->chip);
			snprintf(last, sizeof(last), "%s", s->chip);
			row += 2;
			if (row >= y + h)
				break;
		}

		if (s->kind == KPR_SENSOR_FAN)
			snprintf(val, sizeof(val), "%.0f %s", s->value,
				 unit_of(s->kind));
		else
			snprintf(val, sizeof(val), "%.1f %s", s->value,
				 unit_of(s->kind));

		ktui_draw_text(x + 2, row, 22, s->label, KT_MID, KT_BG, 0);
		ktui_draw_text(x + 25, row, 14, val, temp_slot(s), KT_BG, 0);

		/* The bar, only where the chip published a ceiling. */
		if (s->kind == KPR_SENSOR_TEMP && s->crit > 0 && w > 56) {
			double frac = s->value / s->crit;

			if (frac < 0)
				frac = 0;
			if (frac > 1)
				frac = 1;
			ktui_gauge(x + 40, row, w - 52, frac, temp_slot(s),
				   KT_BG);
			ktui_draw_textf(x + w - 11, row, 10, KT_DIM, KT_BG, 0,
					"crit %.0f", s->crit);
		}
		row++;
	}
}
