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

/*
 * ── the per-device rings ─────────────────────────────────────────────────
 *
 * A rate is a difference between two readings, so these are fed from
 * res_sample() rather than from a page's prepare(): prepare runs once per
 * FRAME and a frame is not an interval, and the offscreen dump draws exactly
 * once after two samples — a chart fed from prepare would be empty in every
 * golden and the arithmetic behind it would never be checked.
 *
 * A COUNTER THAT WENT BACKWARDS IS A GAP, NEVER A SPIKE. `eth0` in the
 * fixture wraps a 32-bit counter (4294967000 then 200) and the naive
 * subtraction is an enormous negative that becomes an enormous positive one
 * cast later. Both series of the pair skip that interval together, so the
 * mirror stays aligned: they are one measurement with a direction, and one
 * half advancing while the other did not would put received and sent a sample
 * out of step for the rest of the session.
 */
#define DEVH_MAX 16

struct devh {
	char name[32];
	KprHist a, b;			/* read/receive, write/send      */
	unsigned long long la, lb;
	int have;
	/* Busy lives here, keyed by NAME like everything else about a device.
	 * Held in a parallel array indexed by the listing's position it would
	 * be attached to whichever device happened to sort there next time. */
	unsigned long long ticks;
	int have_ticks;
	double busy;			/* -1 until an interval has passed */
};

static struct devh *devh_of(struct devh *tab, const char *name)
{
	for (int i = 0; i < DEVH_MAX; i++) {
		if (tab[i].name[0] && !strcmp(tab[i].name, name))
			return &tab[i];
		if (!tab[i].name[0]) {
			kb_strlcpy(tab[i].name, name, sizeof(tab[i].name));
			kpr_hist_init(&tab[i].a, 0);
			kpr_hist_init(&tab[i].b, 0);
			tab[i].busy = -1.0;
			return &tab[i];
		}
	}
	return NULL;		/* more devices than rings: no chart, no lie */
}

static void devh_push(struct devh *d, unsigned long long a,
		      unsigned long long b, double dt_s)
{
	if (!d)
		return;
	if (!d->have) {
		/* The first reading has no delta. Recording it and pushing
		 * nothing is what stops a machine's whole uptime being drawn
		 * as one enormous first sample. */
		d->la = a;
		d->lb = b;
		d->have = 1;
		return;
	}
	if (a < d->la || b < d->lb || dt_s <= 0.0) {
		d->la = a;
		d->lb = b;
		return;		/* the gap */
	}
	kpr_hist_push(&d->a, (double)(a - d->la) / dt_s);
	kpr_hist_push(&d->b, (double)(b - d->lb) / dt_s);
	d->la = a;
	d->lb = b;
}

/* ── Drives ──────────────────────────────────────────────────────────── */

static KprDisk *g_disk;
static int g_ndisk;
static int g_dhover = -1;	/* the row under the pointer */
/* How many rows the last draw actually laid down. The chart takes the bottom
 * third, so `n` is not the answer: a click down there would otherwise select a
 * row that is not on the screen. */
static int g_ddrawn;
static struct devh g_dh_disk[DEVH_MAX];
static int g_dsel;

void res_drive_prepare(void)
{
	/* The listing belongs to the sampler now. This is the first-frame
	 * guard for a face that drew before a sample ever ran. */
	if (!g_disk)
		g_ndisk = kpr_block_list(&g_disk);
}

static void drive_sample(double dt_s)
{
	kpr_block_free(g_disk);
	g_disk = NULL;
	g_ndisk = kpr_block_list(&g_disk);

	for (int i = 0; i < g_ndisk && i < DEVH_MAX; i++) {
		const KprDisk *d = &g_disk[i];
		struct devh *h = devh_of(g_dh_disk, d->name);

		if (!h)
			continue;
		/* A diskstats sector is 512 bytes BY DEFINITION of that
		 * interface, whatever the drive's own sector size. Reading
		 * queue/hw_sector_size instead is the classic way to be eight
		 * times wrong on a 4K disk. */
		devh_push(h, d->rd_sectors * 512ULL, d->wr_sectors * 512ULL,
			  dt_s);

		/*
		 * io_ticks is milliseconds the queue was busy, so over the
		 * interval it is a percentage — and it is the one figure that
		 * says a disk is the machine's bottleneck while its byte rate
		 * looks unremarkable.
		 */
		if (h->have_ticks && dt_s > 0.0 && d->io_ticks >= h->ticks) {
			double pc = (double)(d->io_ticks - h->ticks) /
				    (dt_s * 1000.0) * 100.0;
			h->busy = pc > 100.0 ? 100.0 : pc;
		}
		h->ticks = d->io_ticks;
		h->have_ticks = 1;
	}
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

/* The facts a detail view shows for one disk. Built here because this page is
 * the reader: a second one in detail.c would be a second answer. */
static void drive_facts(int i)
{
	static char l[6][128];
	const char *lines[6];
	const KprDisk *d = &g_disk[i];
	int n = 0;

	snprintf(l[n], sizeof(l[0]), "size        %s", res_size(d->size));
	lines[n] = l[n]; n++;
	snprintf(l[n], sizeof(l[0]), "media       %s%s",
		 d->rotational == 1 ? "rotational" :
		 d->rotational == 0 ? "solid state" : res_none(),
		 d->removable ? ", removable" : "");
	lines[n] = l[n]; n++;
	snprintf(l[n], sizeof(l[0]), "model       %s",
		 d->model[0] ? d->model : res_none());
	lines[n] = l[n]; n++;
	snprintf(l[n], sizeof(l[0]), "temperature %s", res_temp(d->temp_c));
	lines[n] = l[n]; n++;
	snprintf(l[n], sizeof(l[0]), "read        %s total",
		 res_size(d->rd_sectors * 512ULL));
	lines[n] = l[n]; n++;
	snprintf(l[n], sizeof(l[0]), "written     %s total",
		 res_size(d->wr_sectors * 512ULL));
	lines[n] = l[n]; n++;
	res_detail_open_facts(d->name, "block device", lines, n);
}

/* Which rows the table drew, so a click and a key agree about row order with
 * the virtual devices hidden. */
static int drive_rows(int *out, int cap)
{
	int n = 0;

	for (int i = 0; i < g_ndisk && n < cap; i++)
		if (RC.virtual_drives || !g_disk[i].virt)
			out[n++] = i;
	return n;
}

void res_draw_drives(int x, int y, int w, int h)
{
	const int bottom = y + h;
	int row = y;
	int idx[DEVH_MAX];
	int n = drive_rows(idx, DEVH_MAX);

	if (g_dsel >= n)
		g_dsel = n ? n - 1 : 0;
	g_ddrawn = 0;

	/*
	 * BUSY is io_ticks and it earns its column: a disk at 100% busy while
	 * its byte rate looks unremarkable is a queue that is the machine's
	 * bottleneck, and no other figure on this page says so.
	 */
	int c_size = 11, c_type = 21, c_busy = 27, c_temp = 34, c_model = 42;

	ktui_draw_fill(krect(x, row, w, 1), KT_SURFACE);
	ktui_draw_text(x, row, 10, "DEVICE", KT_MID, KT_SURFACE, 0);
	ktui_draw_text(x + c_size, row, 9, "SIZE", KT_MID, KT_SURFACE, 0);
	ktui_draw_text(x + c_type, row, 5, "TYPE", KT_MID, KT_SURFACE, 0);
	ktui_draw_text(x + c_busy, row, 6, "BUSY", KT_MID, KT_SURFACE, 0);
	ktui_draw_text(x + c_temp, row, 7, "TEMP", KT_MID, KT_SURFACE, 0);
	if (w >= c_model + 6)
		ktui_draw_text(x + c_model, row, w - c_model - 1, "MODEL",
			       KT_MID, KT_SURFACE, 0);
	row++;

	/* The chart takes the bottom third, and only when there is a third to
	 * take: below that the table is the page. */
	int chart_h = (bottom - 1 - row) / 3;
	if (chart_h < 3)
		chart_h = 0;
	int list_bottom = bottom - 1 - (chart_h ? chart_h + 1 : 0);

	for (int k = 0; k < n && row < list_bottom; k++) {
		const KprDisk *d = &g_disk[idx[k]];
		int sel = (k == g_dsel);
		int hov = !sel && k == g_dhover;
		int fg = sel ? KT_SURFACE : KT_TEXT;
		int bg = sel ? KT_ACCENT : hov ? KT_MID : KT_BG;
		char busy[16];

		if (sel || hov)
			ktui_draw_fill(krect(x, row, w, 1), bg);
		ktui_draw_text(x, row, 10, d->name, fg, bg, 0);
		ktui_draw_text(x + c_size, row, 9, res_size(d->size), fg, bg, 0);
		ktui_draw_text(x + c_type, row, 5,
			       d->rotational == 1 ? "HDD" :
			       d->rotational == 0 ? "SSD" : res_none(),
			       sel ? KT_SURFACE : KT_MID, bg, 0);
		const struct devh *bh = devh_of(g_dh_disk, d->name);

		if (bh && bh->busy >= 0.0)
			snprintf(busy, sizeof(busy), "%.0f%%", bh->busy);
		else
			snprintf(busy, sizeof(busy), "%s", res_none());
		ktui_draw_text(x + c_busy, row, 6, busy, fg, bg, 0);
		ktui_draw_text(x + c_temp, row, 7, res_temp(d->temp_c),
			       sel ? KT_SURFACE : KT_MID, bg, 0);
		if (w >= c_model + 6)
			ktui_draw_text(x + c_model, row, w - c_model - 1,
				       d->model[0] ? d->model : res_none(),
				       sel ? KT_SURFACE : KT_MID, bg, 0);
		row++;
		g_ddrawn = k + 1;
	}

	if (chart_h && n) {
		const KprDisk *d = &g_disk[idx[g_dsel]];
		struct devh *hh = devh_of(g_dh_disk, d->name);
		char label[64], reading[64];

		if (hh) {
			snprintf(label, sizeof(label), "%s  read / written",
				 d->name);
			snprintf(reading, sizeof(reading), "%s  %s",
				 hh->a.n ? res_size((unsigned long long)
					   kpr_hist_at(&hh->a, hh->a.n - 1))
					 : res_none(),
				 hh->b.n ? res_size((unsigned long long)
					   kpr_hist_at(&hh->b, hh->b.n - 1))
					 : res_none());
			res_graph2(910, krect(x, list_bottom + 1, w, chart_h),
				   &hh->a, &hh->b, label, reading);
		}
	}

	int hidden = 0;
	for (int i = 0; i < g_ndisk; i++)
		if (!RC.virtual_drives && g_disk[i].virt)
			hidden++;
	char foot[160];
	snprintf(foot, sizeof(foot),
		 "%d virtual hidden  %s  whole disks only  %s  Enter for "
		 "details", hidden, ktui_glyph[KT_G_DOT],
		 ktui_glyph[KT_G_DOT]);
	ktui_draw_text(x, bottom - 1, w, foot, KT_DIM, KT_BG, 0);
}

int res_drive_key(int k)
{
	int idx[DEVH_MAX];
	int n = drive_rows(idx, DEVH_MAX);

	if (k == KT_K_UP && g_dsel > 0) {
		g_dsel--;
		return 1;
	}
	if (k == KT_K_DOWN && g_dsel + 1 < n) {
		g_dsel++;
		return 1;
	}
	if ((k == '\n' || k == '\r') && n) {
		drive_facts(idx[g_dsel]);
		return 1;
	}
	return 0;
}

int res_drive_wheel(int up)
{
	int idx[DEVH_MAX];
	int n = drive_rows(idx, DEVH_MAX);

	if (up && g_dsel > 0)
		g_dsel--;
	else if (!up && g_dsel + 1 < n)
		g_dsel++;
	return 1;
}

/*
 * THE POINTER, the same contract the two tables keep: motion lights a row, a
 * press selects it, a press on the row that is ALREADY selected opens it.
 * Both device pages had verbs on keys and NOTHING at all on the pointer, so a
 * hand that reached this page could pick a drive and then not look at it.
 */
void res_drive_motion(int mx, int my)
{
	int idx[DEVH_MAX];
	int n = drive_rows(idx, DEVH_MAX);
	int k = my - 1;

	(void)mx;
	(void)n;
	g_dhover = (my >= 1 && k < g_ddrawn) ? k : -1;
}

int res_drive_click(int mx, int my, int btn)
{
	int idx[DEVH_MAX];
	int n = drive_rows(idx, DEVH_MAX);
	int k = my - 1;

	(void)mx;
	(void)btn;
	(void)n;
	if (my < 1 || k >= g_ddrawn)
		return 1;
	if (k == g_dsel)
		drive_facts(idx[k]);
	else
		g_dsel = k;
	return 1;
}

/* ── Network ─────────────────────────────────────────────────────────── */

static KprIface *g_if;
static int g_nif;

static struct devh g_dh_if[DEVH_MAX];
static int g_nsel;
static int g_nhover = -1;
static int g_ndrawn;

void res_net_prepare(void)
{
	if (!g_if)
		g_nif = kpr_net_list(&g_if);
}

static void net_sample(double dt_s)
{
	kpr_net_free(g_if);
	g_if = NULL;
	g_nif = kpr_net_list(&g_if);

	for (int i = 0; i < g_nif && i < DEVH_MAX; i++)
		devh_push(devh_of(g_dh_if, g_if[i].name), g_if[i].rx_bytes,
			  g_if[i].tx_bytes, dt_s);
}

/*
 * Only the visible page samples, which is this program's rule for io as much
 * as for /proc: a monitor with nine pages that read every device on every one
 * of them costs nine pages of io a second to show one.
 */
void res_dev_sample(void)
{
	static double last_s;
	static int have_last;
	/*
	 * THE INTERVAL IS THE SAMPLER'S OWN, not `R.sample.wall_ms`. That
	 * field is stamped inside res_sample()'s `if (want)` branch, and
	 * these two pages want no per-process file at all — so on Drives and
	 * Network it is never written and every rate here divided by zero
	 * elapsed, which renders as a chart that is simply empty with nothing
	 * saying why.
	 *
	 * A fixture's clock is its own: its snapshots are one declared
	 * interval apart by construction, and the monotonic clock would put
	 * them microseconds apart and make every rate thousands of times too
	 * large.
	 */
	double now_s = R.fixture
		       ? (double)R.tick * (double)RC.interval_ms / 1000.0
		       : kb_now_s();
	/* A FLAG, not `last_s != 0`: the fixture's first sample is legitimately
	 * at zero. */
	double dt_s = have_last && now_s > last_s ? now_s - last_s : 0.0;

	switch (res_page_current()) {
	case RP_DRIVES:
		drive_sample(dt_s);
		break;
	case RP_NETWORK:
		net_sample(dt_s);
		break;
	default:
		return;
	}
	last_s = now_s;
	have_last = 1;
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

static int net_rows(int *out, int cap)
{
	int n = 0;

	for (int i = 0; i < g_nif && n < cap; i++)
		if (RC.virtual_net || !g_if[i].virt)
			out[n++] = i;
	return n;
}

static void net_facts(int i)
{
	static char l[6][128];
	const char *lines[6];
	const KprIface *f = &g_if[i];
	int n = 0;

	snprintf(l[n], sizeof(l[0]), "state       %s, carrier %s",
		 f->up ? "up" : "down", f->carrier ? "yes" : "no");
	lines[n] = l[n]; n++;
	snprintf(l[n], sizeof(l[0]), "speed       %s",
		 f->speed_mbit > 0 ? "" : res_none());
	if (f->speed_mbit > 0)
		snprintf(l[n], sizeof(l[0]), "speed       %ld Mb/s",
			 f->speed_mbit);
	lines[n] = l[n]; n++;
	snprintf(l[n], sizeof(l[0]), "mtu         %d   driver %s", f->mtu,
		 f->driver[0] ? f->driver : res_none());
	lines[n] = l[n]; n++;
	snprintf(l[n], sizeof(l[0]), "address     %s",
		 f->mac[0] ? f->mac : res_none());
	lines[n] = l[n]; n++;
	snprintf(l[n], sizeof(l[0]),
		 "received    %s  %llu packets  %llu errors  %llu dropped",
		 res_size(f->rx_bytes), f->rx_pkts, f->rx_err, f->rx_drop);
	lines[n] = l[n]; n++;
	snprintf(l[n], sizeof(l[0]),
		 "sent        %s  %llu packets  %llu errors  %llu dropped",
		 res_size(f->tx_bytes), f->tx_pkts, f->tx_err, f->tx_drop);
	lines[n] = l[n]; n++;
	res_detail_open_facts(f->name, "network interface", lines, n);
}

void res_draw_net(int x, int y, int w, int h)
{
	const int bottom = y + h;
	int row = y;
	int idx[DEVH_MAX];
	int n = net_rows(idx, DEVH_MAX);

	if (g_nsel >= n)
		g_nsel = n ? n - 1 : 0;

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

	int chart_h = (bottom - 1 - row) / 3;
	if (chart_h < 3)
		chart_h = 0;
	int list_bottom = bottom - 1 - (chart_h ? chart_h + 1 : 0);

	for (int k = 0; k < n && row < list_bottom; k++) {
		const KprIface *f = &g_if[idx[k]];
		int sel = (k == g_nsel);
		int hov = !sel && k == g_nhover;
		int fg = sel ? KT_SURFACE : KT_TEXT;
		int bg = sel ? KT_ACCENT : hov ? KT_MID : KT_BG;
		char sp[24];

		if (sel || hov)
			ktui_draw_fill(krect(x, row, w, 1), bg);
		ktui_draw_text(x, row, 12, f->name, fg, bg, 0);
		ktui_draw_text(x + 13, row, 6, f->up ? "up" : "down",
			       sel ? KT_SURFACE : (f->up ? KT_ACCENT : KT_DIM),
			       bg, 0);
		/* No speed is a dash: a wireless driver publishes none, and
		 * 0 Mbit would be a claim about the link. */
		if (f->speed_mbit > 0)
			snprintf(sp, sizeof(sp), "%ld Mb", f->speed_mbit);
		else
			snprintf(sp, sizeof(sp), "%s", res_none());
		ktui_draw_text(x + 20, row, 9, sp, sel ? KT_SURFACE : KT_MID,
			       bg, 0);
		ktui_draw_text(x + 30, row, 10, res_size(f->rx_bytes), fg,
			       bg, 0);
		ktui_draw_text(x + 41, row, 10, res_size(f->tx_bytes), fg,
			       bg, 0);
		if (w >= 70)
			ktui_draw_text(x + 52, row, 18, f->mac,
				       sel ? KT_SURFACE : KT_DIM, bg, 0);
		row++;
	}

	if (chart_h && n) {
		const KprIface *f = &g_if[idx[g_nsel]];
		struct devh *hh = devh_of(g_dh_if, f->name);
		char label[64], reading[64];

		if (hh) {
			snprintf(label, sizeof(label), "%s  received / sent",
				 f->name);
			snprintf(reading, sizeof(reading), "%s  %s",
				 hh->a.n ? res_size((unsigned long long)
					   kpr_hist_at(&hh->a, hh->a.n - 1))
					 : res_none(),
				 hh->b.n ? res_size((unsigned long long)
					   kpr_hist_at(&hh->b, hh->b.n - 1))
					 : res_none());
			res_graph2(911, krect(x, list_bottom + 1, w, chart_h),
				   &hh->a, &hh->b, label, reading);
		}
	}

	/*
	 * SSID and connection state are NetworkManager's over sd-bus, and
	 * kdos-net is the program that owns them. Keeping basu out of this
	 * binary is what lets it run before the session bus exists — so the
	 * page NAMES the program rather than half-answering the question.
	 */
	char nfoot[160];
	snprintf(nfoot, sizeof(nfoot),
		 "totals since boot  %s  Enter for details  %s  n opens "
		 "kdos-net for Wi-Fi and VPN", ktui_glyph[KT_G_DOT],
		 ktui_glyph[KT_G_DOT]);
	ktui_draw_text(x, bottom - 1, w, nfoot, KT_DIM, KT_BG, 0);
}

int res_net_key(int k)
{
	int idx[DEVH_MAX];
	int n = net_rows(idx, DEVH_MAX);

	if (k == KT_K_UP && g_nsel > 0) {
		g_nsel--;
		return 1;
	}
	if (k == KT_K_DOWN && g_nsel + 1 < n) {
		g_nsel++;
		return 1;
	}
	if ((k == '\n' || k == '\r') && n) {
		net_facts(idx[g_nsel]);
		return 1;
	}
	if (k == 'n') {
		/* argv, never a command string, and detached: this program
		 * must not wait on another one to exit. */
		KbArgv a = { 0 };

		kb_argv_add(&a, "kdos-net");
		kb_argv_end(&a);
		kb_run_detach(&a);
		return 1;
	}
	return 0;
}

int res_net_wheel(int up)
{
	int idx[DEVH_MAX];
	int n = net_rows(idx, DEVH_MAX);

	if (up && g_nsel > 0)
		g_nsel--;
	else if (!up && g_nsel + 1 < n)
		g_nsel++;
	return 1;
}

void res_net_motion(int mx, int my)
{
	int idx[DEVH_MAX];
	int n = net_rows(idx, DEVH_MAX);
	int k = my - 1;

	(void)mx;
	(void)n;
	g_nhover = (my >= 1 && k < g_ndrawn) ? k : -1;
}

int res_net_click(int mx, int my, int btn)
{
	int idx[DEVH_MAX];
	int n = net_rows(idx, DEVH_MAX);
	int k = my - 1;

	(void)mx;
	(void)btn;
	(void)n;
	if (my < 1 || k >= g_ndrawn)
		return 1;
	if (k == g_nsel)
		net_facts(idx[k]);
	else
		g_nsel = k;
	return 1;
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
