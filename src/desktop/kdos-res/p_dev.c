/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   KD's Homebrew Linux Distro
 * ---------------------------------
 */

/*
 * Drives, Network and Batteries.
 *
 * Three pages that share a shape: enumerate devices, show what the driver
 * publishes, and say nothing where it publishes nothing. No page here invents
 * a number — a missing temperature is a dash, an unknown link speed is a dash,
 * and a counter that went backwards is a gap rather than a spike.
 */

#include <stdio.h>
#include <string.h>

#include "res.h"

/* ── Drives ──────────────────────────────────────────────────────────── */

static KprDisk *g_disk;
static int g_ndisk;

void res_drive_prepare(void)
{
	kpr_block_free(g_disk);
	g_disk = NULL;
	g_ndisk = kpr_block_list(&g_disk);
}

const char *res_drive_headline(void)
{
	static char s[64];
	int shown = 0;
	for (int i = 0; i < g_ndisk; i++)
		if (RC.virtual_drives || !g_disk[i].virt)
			shown++;
	snprintf(s, sizeof(s), "%d drive%s", shown, shown == 1 ? "" : "s");
	return s;
}

void res_draw_drives(int x, int y, int w, int h)
{
	const int bottom = y + h;
	int row = y;

	ktui_draw_fill(krect(x, row, w, 1), KT_SURFACE);
	ktui_draw_text(x, row, 10, "DEVICE", KT_MID, KT_SURFACE, 0);
	ktui_draw_text(x + 11, row, 9, "SIZE", KT_MID, KT_SURFACE, 0);
	ktui_draw_text(x + 21, row, 7, "TYPE", KT_MID, KT_SURFACE, 0);
	ktui_draw_text(x + 29, row, 7, "TEMP", KT_MID, KT_SURFACE, 0);
	if (w >= 56)
		ktui_draw_text(x + 37, row, w - 38, "MODEL", KT_MID,
			       KT_SURFACE, 0);
	row++;

	for (int i = 0; i < g_ndisk && row < bottom - 1; i++) {
		const KprDisk *d = &g_disk[i];
		/*
		 * Virtual devices are hidden by default and the footer says
		 * so. They are not dropped by the library — which ones matter
		 * is this page's decision, not libkproc's.
		 */
		if (!RC.virtual_drives && d->virt)
			continue;
		ktui_draw_text(x, row, 10, d->name, KT_TEXT, KT_BG, 0);
		ktui_draw_text(x + 11, row, 9, res_size(d->size), KT_TEXT,
			       KT_BG, 0);
		ktui_draw_text(x + 21, row, 7,
			       d->rotational == 1 ? "HDD" :
			       d->rotational == 0 ? "SSD" : "-",
			       KT_MID, KT_BG, 0);
		ktui_draw_text(x + 29, row, 7, res_temp(d->temp_c), KT_MID,
			       KT_BG, 0);
		if (w >= 56)
			ktui_draw_text(x + 37, row, w - 38,
				       d->model[0] ? d->model : "-",
				       KT_MID, KT_BG, 0);
		row++;
	}

	int hidden = 0;
	for (int i = 0; i < g_ndisk; i++)
		if (!RC.virtual_drives && g_disk[i].virt)
			hidden++;
	char foot[128];
	snprintf(foot, sizeof(foot),
		 "%d virtual device%s hidden  %s  whole disks only, never "
		 "partitions", hidden, hidden == 1 ? "" : "s",
		 ktui_glyph[KT_G_DOT]);
	ktui_draw_text(x, bottom - 1, w, foot, KT_DIM, KT_BG, 0);
}

/* ── Network ─────────────────────────────────────────────────────────── */

static KprIface *g_if;
static int g_nif;

void res_net_prepare(void)
{
	kpr_net_free(g_if);
	g_if = NULL;
	g_nif = kpr_net_list(&g_if);
}

const char *res_net_headline(void)
{
	static char s[64];
	int up = 0;
	for (int i = 0; i < g_nif; i++)
		if (g_if[i].up && !g_if[i].loopback)
			up++;
	snprintf(s, sizeof(s), "%d interface%s up", up, up == 1 ? "" : "s");
	return s;
}

void res_draw_net(int x, int y, int w, int h)
{
	const int bottom = y + h;
	int row = y;

	ktui_draw_fill(krect(x, row, w, 1), KT_SURFACE);
	ktui_draw_text(x, row, 12, "INTERFACE", KT_MID, KT_SURFACE, 0);
	ktui_draw_text(x + 13, row, 6, "STATE", KT_MID, KT_SURFACE, 0);
	ktui_draw_text(x + 20, row, 9, "SPEED", KT_MID, KT_SURFACE, 0);
	ktui_draw_text(x + 30, row, 10, "RX", KT_MID, KT_SURFACE, 0);
	ktui_draw_text(x + 41, row, 10, "TX", KT_MID, KT_SURFACE, 0);
	if (w >= 70)
		ktui_draw_text(x + 52, row, 18, "ADDRESS", KT_MID,
			       KT_SURFACE, 0);
	row++;

	for (int i = 0; i < g_nif && row < bottom - 1; i++) {
		const KprIface *n = &g_if[i];
		if (!RC.virtual_net && n->virt)
			continue;
		ktui_draw_text(x, row, 12, n->name, KT_TEXT, KT_BG, 0);
		ktui_draw_text(x + 13, row, 6, n->up ? "up" : "down",
			       n->up ? KT_ACCENT : KT_DIM, KT_BG, 0);
		char sp[24];
		/* No speed is a dash: a wireless driver publishes none, and
		 * 0 Mbit would be a claim about the link. */
		if (n->speed_mbit > 0)
			snprintf(sp, sizeof(sp), "%ld Mb", n->speed_mbit);
		else
			snprintf(sp, sizeof(sp), "%s", res_none());
		ktui_draw_text(x + 20, row, 9, sp, KT_MID, KT_BG, 0);
		ktui_draw_text(x + 30, row, 10, res_size(n->rx_bytes),
			       KT_TEXT, KT_BG, 0);
		ktui_draw_text(x + 41, row, 10, res_size(n->tx_bytes),
			       KT_TEXT, KT_BG, 0);
		if (w >= 70)
			ktui_draw_text(x + 52, row, 18, n->mac, KT_DIM,
				       KT_BG, 0);
		row++;
	}

	/*
	 * SSID and connection state are NetworkManager's over sd-bus, and
	 * kdos-net is the program that owns them. Keeping basu out of this
	 * binary is what lets it run before the session bus exists.
	 */
	char nfoot[128];
	snprintf(nfoot, sizeof(nfoot),
		 "totals are since boot  %s  kdos-net owns Wi-Fi and VPN",
		 ktui_glyph[KT_G_DOT]);
	ktui_draw_text(x, bottom - 1, w, nfoot, KT_DIM, KT_BG, 0);
}

/* ── Batteries ───────────────────────────────────────────────────────── */

static KprBattery *g_bat;
static int g_nbat;

void res_batt_prepare(void)
{
	kpr_power_free(g_bat);
	g_bat = NULL;
	g_nbat = kpr_power_list(&g_bat);
}

const char *res_batt_headline(void)
{
	static char s[64];
	for (int i = 0; i < g_nbat; i++)
		if (g_bat[i].is_battery) {
			snprintf(s, sizeof(s), "%d%% %s", g_bat[i].capacity,
				 g_bat[i].state);
			return s;
		}
	return "no battery";
}

void res_draw_batt(int x, int y, int w, int h)
{
	const int bottom = y + h;
	int row = y + 1;
	int any = 0;

	for (int i = 0; i < g_nbat && row < bottom - 2; i++) {
		const KprBattery *b = &g_bat[i];
		if (!b->is_battery)
			continue;
		any = 1;
		kch_group(x, row, w, b->name);
		row++;

		char line[160];
		snprintf(line, sizeof(line), "charge      %d%%  (%s, %.1f Wh)",
			 b->capacity, b->state[0] ? b->state : "unknown",
			 (double)b->energy_now / 1e6);
		ktui_draw_text(x + 1, row++, w - 2, line, KT_TEXT, KT_BG, 0);

		/*
		 * HEALTH IS WEAR, and it is a different number from charge. A
		 * battery reporting 90% charge can hold 70% of what it held
		 * new, and a monitor that prints only capacity says nothing
		 * about whether it needs replacing.
		 */
		if (b->health >= 0.0) {
			/*
			 * These are µWh (or µAh where the driver publishes the
			 * charge_* triple instead), NOT bytes. Formatting them
			 * with the size helper prints "48M" for 50 Wh — a
			 * number in the wrong unit, which is a wrong claim
			 * rather than an ugly one.
			 */
			char now[32], des[32];
			snprintf(now, sizeof(now), "%.1f Wh",
				 (double)b->energy_full / 1e6);
			snprintf(des, sizeof(des), "%.1f Wh",
				 (double)b->energy_full_design / 1e6);
			snprintf(line, sizeof(line),
				 "health      %.1f%%  (%s of %s by design)",
				 b->health * 100.0, now, des);
		}
		else
			snprintf(line, sizeof(line),
				 "health      %s  (the driver publishes no "
				 "design capacity)", res_none());
		ktui_draw_text(x + 1, row++, w - 2, line, KT_TEXT, KT_BG, 0);

		if (b->cycles >= 0) {
			snprintf(line, sizeof(line), "cycles      %d", b->cycles);
			ktui_draw_text(x + 1, row++, w - 2, line, KT_MID,
				       KT_BG, 0);
		}
		if (b->power_uw) {
			snprintf(line, sizeof(line), "draw        %.1f W",
				 (double)b->power_uw / 1e6);
			ktui_draw_text(x + 1, row++, w - 2, line, KT_MID,
				       KT_BG, 0);
		}
		if (b->tech[0]) {
			snprintf(line, sizeof(line), "technology  %s", b->tech);
			ktui_draw_text(x + 1, row++, w - 2, line, KT_DIM,
				       KT_BG, 0);
		}
		row++;
	}

	for (int i = 0; i < g_nbat && row < bottom - 1; i++) {
		const KprBattery *b = &g_bat[i];
		if (b->is_battery)
			continue;
		char line[128];
		snprintf(line, sizeof(line), "%-12s %s", b->name,
			 b->online == 1 ? "connected" :
			 b->online == 0 ? "not connected" : res_none());
		ktui_draw_text(x + 1, row++, w - 2, line, KT_MID, KT_BG, 0);
	}

	/* A machine with no battery gets a page that SAYS so. An empty page
	 * reads as a page that failed to load. */
	if (!any)
		ktui_draw_text(x + 1, y + 1, w - 2,
			       "This machine has no battery.", KT_DIM,
			       KT_BG, 0);
}
