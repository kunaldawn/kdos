/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-burn — a folder or an image onto a CD, DVD or Blu-ray, and checked
 *
 *   ┌ Burn ────────────────────────────────────────────────────────┐
 *   │ SOURCE  /home/kdos/Downloads/kdos.iso — an image, 1.2G         │
 *   │ DRIVE      MODEL                                               │
 *   │ ▶ sr0      HL-DT-ST DVDRAM GP65NB60                            │
 *   ├────────────────────────────────────────────────────────────────┤
 *   │ [ Source ] [ Burn ] [ Verify ] [ Close ]                       │
 *   └────────────────────────────────────────────────────────────────┘
 *
 * xorriso DOES THE BURN. This picks what and where, runs it, and shows its
 * last line while it works; it builds no filesystem and speaks no SCSI,
 * because libburn already does both and a second implementation would be a
 * second answer to what a disc holds.
 *
 * NO DAEMON STANDS IN FRONT OF IT. An optical drive is `cdrom`'s, and the
 * desktop user is in `cdrom`, so xorriso opens the drive as the person — the
 * rule kdos-print keeps for the `lpadmin` group. A root daemon here would be
 * privilege nobody needs.
 *
 * TWO SOURCES, TWO COMMANDS, ONE VERIFY EACH:
 *
 *   - A FOLDER becomes a new ISO 9660 filesystem written in one session:
 *     `xorriso -outdev <drive> -blank as_needed -volid <name> -map <folder> /
 *     -commit -compare_r <folder> /`. After -commit xorriso loads the disc it
 *     just wrote, and -compare_r reads every file back and compares it with
 *     the folder, through libburn's own reads — so the verify is the same run.
 *     A difference is NOT an error event and leaves the exit status 0: xorriso
 *     prints it on its result channel and ends the comparison with
 *     `Differences detected.`, so that line is what fails the burn here.
 *
 *   - AN IMAGE is written as it is, the way cdrecord would: `xorriso -as
 *     cdrecord -v dev=<drive> blank=as_needed -eject padsize=300k <image>`.
 *     The kernel does not notice a disc libburn wrote until the tray has been
 *     opened, which is why the burn ejects; Verify, pressed once the disc is
 *     back in, reads it through the kernel and compares it with the image
 *     byte for byte (`kdos-burn --compare`, this program, so no `cmp` of
 *     unknown flavour decides it).
 *
 * `blank=as_needed` BLANKS A REWRITABLE DISC THAT HOLDS DATA. That is the
 * burn's one destructive step, and it is why Burn asks for a second Enter
 * naming the drive before anything runs.
 *
 * A BURN OUTLIVES ITS WINDOW. Closing this window does not stop xorriso —
 * half a write-once disc is a coaster — and the job's children ignore SIGPIPE
 * so the pipe that closed with the window cannot kill them.
 * ---------------------------------
 */

#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "kbase.h"
#include "kwl.h"
#include "shell.h"

#define BN_COLS 72
#define BN_ROWS 18
#define BN_DRIVES 8
#define BN_PATH 1024

enum { BB_SOURCE, BB_BURN, BB_VERIFY, BB_CLOSE, BB_N };

struct drive {
	char name[16];		/* sr0                                     */
	char model[64];		/* vendor and model, from /sys             */
};

static struct drive drives[BN_DRIVES];
static int ndrive;
static KtuiTable tbl;
static KtuiKeys keys;
static int icons_on = 1;
/* A recorded /sys, so a golden draws the same drives on every machine. */
static const char *fixture;

static char source[BN_PATH];
static int src_dir;		/* a folder, not an image                  */
static unsigned long long src_bytes;
static char status[192];

/* The source prompt and the burn confirmation — the two raised states. */
enum { PR_NONE, PR_SOURCE, PR_CONFIRM };
static int prompt;
static char answer[BN_PATH];

/* What the job is doing, so its end says the right thing. */
enum { PH_IDLE, PH_BURN, PH_VERIFY };
static int phase;
static ShJob job;
static char job_drive[16];
/* xorriso's -compare_r said `Differences detected.` during this burn. */
static int differ;

/* ── the machine ───────────────────────────────────────────────────────── */

static const char *sysroot(void)
{
	return fixture ? fixture : "/sys";
}

static void read_line(const char *path, char *out, size_t n)
{
	char *s = kb_read_whole(path, NULL);

	out[0] = '\0';
	if (!s)
		return;
	s[strcspn(s, "\n")] = '\0';
	/* SCSI inquiry strings are space-padded to their field width. */
	for (int i = (int)strlen(s) - 1; i >= 0 && s[i] == ' '; i--)
		s[i] = '\0';
	snprintf(out, n, "%s", s);
	free(s);
}

/* Every optical drive is an `sr` block device; its vendor and model are the
 * SCSI inquiry's, which is what the drive says it is. */
static void scan_drives(void)
{
	char path[512];
	char **names;
	int n = 0;

	ndrive = 0;
	snprintf(path, sizeof(path), "%s/block", sysroot());
	names = kb_listdir(path, &n);
	for (int i = 0; names && i < n && ndrive < BN_DRIVES; i++) {
		struct drive *d = &drives[ndrive];
		char vendor[32], model[48];

		if (strncmp(names[i], "sr", 2) || !isdigit((unsigned char)
							   names[i][2]))
			continue;
		snprintf(d->name, sizeof(d->name), "%.15s", names[i]);
		snprintf(path, sizeof(path), "%s/block/%s/device/vendor",
			 sysroot(), names[i]);
		read_line(path, vendor, sizeof(vendor));
		snprintf(path, sizeof(path), "%s/block/%s/device/model",
			 sysroot(), names[i]);
		read_line(path, model, sizeof(model));
		snprintf(d->model, sizeof(d->model), "%s%s%s", vendor,
			 vendor[0] && model[0] ? " " : "", model);
		ndrive++;
	}
	kb_strv_free(names);
	ktui_table_clamp(&tbl, ndrive, 4);
}

/* What the source is. A folder is measured by nothing — walking it for a size
 * would be a second `du` — and an image by its length. */
static void set_source(const char *path)
{
	struct stat st;

	snprintf(source, sizeof(source), "%s", path);
	src_dir = 0;
	src_bytes = 0;
	if (stat(source, &st) != 0) {
		snprintf(status, sizeof(status), "%.120s is not there", source);
		source[0] = '\0';
		return;
	}
	if (S_ISDIR(st.st_mode)) {
		src_dir = 1;
	} else if (S_ISREG(st.st_mode)) {
		src_bytes = (unsigned long long)st.st_size;
	} else {
		snprintf(status, sizeof(status), "%.120s is neither a folder "
						 "nor a file", source);
		source[0] = '\0';
	}
}

/* The volume name a folder's disc gets: the folder's own name, in the
 * characters every reader of ISO 9660 shows, at most 32 of them. */
static void volid_of(const char *path, char *out, size_t n)
{
	const char *b = kb_basename(path);
	size_t k = 0;

	for (; *b && k + 1 < n && k < 32; b++) {
		unsigned char c = (unsigned char)*b;

		out[k++] = isalnum(c) ? (char)toupper(c) : '_';
	}
	out[k] = '\0';
	if (!k)
		snprintf(out, n, "KDOS");
}

static const struct drive *sel_drive(void)
{
	return tbl.sel >= 0 && tbl.sel < ndrive ? &drives[tbl.sel] : NULL;
}

/* ── the burn ──────────────────────────────────────────────────────────── */

static void on_line(const char *ln, void *user)
{
	(void)user;
	if (!strncmp(ln, "Differences detected", 20))
		differ = 1;
}

static void burn(void)
{
	static char node[32], dev[48], volid[40];
	const struct drive *d = sel_drive();
	const char *av[24];
	int n = 0;

	if (!d || !source[0] || job.running)
		return;
	snprintf(node, sizeof(node), "/dev/%s", d->name);
	snprintf(job_drive, sizeof(job_drive), "%s", d->name);
	av[n++] = "xorriso";
	if (src_dir) {
		volid_of(source, volid, sizeof(volid));
		av[n++] = "-abort_on";
		av[n++] = "FAILURE";
		/* A file that cannot be read or written is a SORRY, not a
		 * FAILURE: the run goes on past it, and this makes the exit
		 * status say so. */
		av[n++] = "-return_with";
		av[n++] = "SORRY";
		av[n++] = "32";
		av[n++] = "-outdev";
		av[n++] = node;
		av[n++] = "-blank";
		av[n++] = "as_needed";
		av[n++] = "-volid";
		av[n++] = volid;
		av[n++] = "-map";
		av[n++] = source;
		av[n++] = "/";
		av[n++] = "-commit";
		av[n++] = "-compare_r";
		av[n++] = source;
		av[n++] = "/";
	} else {
		snprintf(dev, sizeof(dev), "dev=%s", node);
		av[n++] = "-as";
		av[n++] = "cdrecord";
		av[n++] = "-v";
		av[n++] = dev;
		av[n++] = "blank=as_needed";
		av[n++] = "-eject";
		av[n++] = "padsize=300k";
		av[n++] = source;
	}
	av[n] = NULL;
	differ = 0;
	job.line = on_line;
	job.user = NULL;
	if (sh_job_start(&job, av, NULL) != 0) {
		snprintf(status, sizeof(status), "%.150s",
			 job.last[0] ? job.last : "xorriso will not start");
		return;
	}
	phase = PH_BURN;
	snprintf(status, sizeof(status), "burning to %s", d->name);
}

/* Read the disc back and compare it with the image — this program again, as
 * `--compare`, so the window keeps drawing while a DVD is read. */
static void verify(void)
{
	static char node[32];
	const struct drive *d = sel_drive();
	const char *av[6];

	if (!d || !source[0] || src_dir || job.running)
		return;
	snprintf(node, sizeof(node), "/dev/%s", d->name);
	snprintf(job_drive, sizeof(job_drive), "%s", d->name);
	av[0] = "kdos-burn";
	av[1] = "--compare";
	av[2] = source;
	av[3] = node;
	av[4] = NULL;
	job.line = NULL;
	if (sh_job_start(&job, av, NULL) != 0) {
		snprintf(status, sizeof(status), "%.150s", job.last);
		return;
	}
	phase = PH_VERIFY;
	snprintf(status, sizeof(status), "reading %s back", d->name);
}

static void job_finished(void)
{
	if (phase == PH_BURN && job.status == 0 && differ)
		snprintf(status, sizeof(status), "burnt to %s, but the disc "
						 "differs from the folder",
			 job_drive);
	else if (phase == PH_BURN && job.status == 0)
		snprintf(status, sizeof(status), src_dir
			 ? "burnt to %s, and every file compares"
			 : "burnt to %s — put the disc back and Verify",
			 job_drive);
	else if (phase == PH_VERIFY && job.status == 0)
		snprintf(status, sizeof(status), "the disc in %s matches the "
						 "image", job_drive);
	else
		snprintf(status, sizeof(status), "%s: %.150s",
			 phase == PH_BURN ? "xorriso stopped" : "no match",
			 job.last[0] ? job.last : "no reason given");
	phase = PH_IDLE;
}

/*
 * `kdos-burn --compare IMAGE NODE`: the image's length read from both and
 * compared, a line per step of about one per cent. Exit 0 when they match.
 * The disc holds padding past the image's end, which is why only the image's
 * own length is compared.
 */
static int compare_main(const char *img, const char *node)
{
	enum { CH = 1 << 20 };
	char *a = malloc(CH), *b = malloc(CH);
	int fa = open(img, O_RDONLY | O_CLOEXEC);
	int fb = open(node, O_RDONLY | O_CLOEXEC);
	struct stat st;
	unsigned long long size, off = 0, last = 0, step;

	if (!a || !b || fa < 0 || fb < 0 || fstat(fa, &st) != 0) {
		printf("%s will not open: %s\n", fa < 0 ? img : node,
		       strerror(errno));
		return 1;
	}
	size = (unsigned long long)st.st_size;
	step = size / 100 > CH ? size / 100 : CH;
	while (off < size) {
		size_t want = size - off < CH ? (size_t)(size - off) : CH;
		ssize_t r = pread(fa, a, want, (off_t)off);
		ssize_t q = pread(fb, b, want, (off_t)off);

		if (r != (ssize_t)want) {
			printf("the image could not be read at byte %llu\n", off);
			return 1;
		}
		if (q != (ssize_t)want) {
			printf("the disc ends at byte %llu of %llu\n",
			       off + (q > 0 ? (unsigned long long)q : 0), size);
			return 1;
		}
		if (memcmp(a, b, want) != 0) {
			printf("the disc differs from the image near byte %llu\n",
			       off);
			return 1;
		}
		off += want;
		if (off - last >= step || off == size) {
			last = off;
			printf("compared %s of %s\n", kb_human_size(off),
			       kb_human_size(size));
			fflush(stdout);
		}
	}
	printf("the disc matches the image\n");
	return 0;
}

/* ── drawing ───────────────────────────────────────────────────────────── */

static const KtuiCol BN_COL[] = { { "DRIVE", 10 }, { "MODEL", 0 } };
#define BN_NCOL 2

static void bn_cell(int idx, int col, int x, int y, int w, int fg, int bg,
		    void *user)
{
	const struct drive *d = &drives[idx];
	int on = bg == KT_ACCENT;

	(void)user;
	if (col == 0)
		ktui_draw_text(x, y, w, d->name, fg, bg, KT_A_NONE);
	else
		ktui_draw_text(x, y, w, d->model[0] ? d->model : "-",
			       on ? KT_SURFACE : KT_MID, bg, KT_A_NONE);
}

static int body_top;

static KRect list_rect(void)
{
	return krect(2, body_top + 2, ktui_w - 4, ktui_h - body_top - 6);
}

static void draw(void)
{
	int w = ktui_w, h = ktui_h;
	const struct drive *d = sel_drive();
	int busy = job.running;
	char what[64];

	ktui_draw_fill(krect(0, 0, w, h), KT_BG);
	sh_frame(w, h, "Burn", KT_ACCENT, KT_BG, 1);
	body_top = kch_header(w, "media-optical-data", "Burn",
			      "a folder or an image onto a CD, DVD or Blu-ray",
			      icons_on);

	if (!source[0])
		snprintf(what, sizeof(what), "nothing chosen");
	else if (src_dir)
		snprintf(what, sizeof(what), "a folder");
	else
		snprintf(what, sizeof(what), "an image, %s",
			 kb_human_size(src_bytes));
	ktui_draw_text(2, body_top, 8, "SOURCE", KT_MID, KT_BG, KT_A_NONE);
	ktui_draw_textf(10, body_top, w - 12, source[0] ? KT_TEXT : KT_MID,
			KT_BG, KT_A_NONE, "%s%s%s", source,
			source[0] ? " — " : "", what);

	if (!ndrive)
		ktui_draw_text(2, body_top + 2, w - 4,
			       "no optical drive is attached", KT_MID, KT_BG,
			       KT_A_NONE);
	else
		ktui_table_draw(list_rect(), &tbl, ndrive, BN_COL, BN_NCOL,
				bn_cell, NULL, NULL, -1);

	ktui_draw_hline(1, h - 4, w - 2, KT_G_HL, KT_DIM, KT_BG);

	struct kch_button b[BB_N];

	b[BB_SOURCE] = (struct kch_button){ "Source", !busy };
	b[BB_BURN] = (struct kch_button){ "Burn", d && source[0] && !busy };
	b[BB_VERIFY] = (struct kch_button){ "Verify",
					    d && source[0] && !src_dir &&
					    !busy };
	b[BB_CLOSE] = (struct kch_button){ "Close", 1 };

	int bx = kch_buttons(w, h - 2, b, BB_N, -1);

	if (prompt == PR_SOURCE)
		ktui_draw_textf(2, h - 3, w - 4, KT_TEXT, KT_BG, KT_A_NONE,
				"Folder or image (a path, or drop it here): %s",
				answer);
	else if (prompt == PR_CONFIRM)
		ktui_draw_textf(2, h - 3, w - 4, KT_WARN, KT_BG, KT_A_NONE,
				"Burn to %s? A rewritable disc there is blanked "
				"first. Enter burns.", d ? d->name : "?");
	else if (status[0] && bx - 3 > 0)
		ktui_draw_text(2, h - 2, bx - 3, status, busy ? KT_TEXT
				: KT_MID, KT_BG, KT_A_NONE);

	ktui_hint_if(!prompt && !busy, "s", "source");
	ktui_hint_if(!prompt && d && source[0] && !busy, "b", "burn");
	ktui_hint_if(!prompt && d && source[0] && !src_dir && !busy, "v",
		     "verify");
	ktui_hint_if(prompt != 0, "Enter",
		     prompt == PR_CONFIRM ? "burn" : "use it");
	ktui_hint("Esc", ktui_esc_verb(&keys));
	ktui_hint_row(&keys, krect(2, h - 3 + (prompt ? 1 : 0), w - 4, 1),
		      KT_BG);
	if (prompt == PR_SOURCE)
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

static void prompt_open(int which)
{
	prompt = which;
	status[0] = '\0';
	if (which == PR_SOURCE)
		snprintf(answer, sizeof(answer), "%s", source);
}

static void ask_burn(void)
{
	if (sel_drive() && source[0] && !job.running)
		prompt_open(PR_CONFIRM);
}

static void on_key(int k)
{
	if (prompt == PR_CONFIRM) {
		prompt = PR_NONE;
		if (k == KT_K_ENTER)
			burn();
		return;
	}
	if (prompt == PR_SOURCE) {
		size_t n = strlen(answer);

		if (k == KT_K_ENTER) {
			prompt = PR_NONE;
			if (answer[0])
				set_source(answer);
		} else if (k == KT_K_BACKSPACE) {
			if (n)
				answer[n - 1] = '\0';
		} else if (k >= 0x20 && k < 0x7f && n + 1 < sizeof(answer)) {
			answer[n] = (char)k;
			answer[n + 1] = '\0';
		}
		return;
	}
	if (ktui_table_key(&tbl, ndrive, 4, k, NULL, NULL))
		return;
	if (job.running)
		return;
	switch (k) {
	case 's':
		prompt_open(PR_SOURCE);
		break;
	case 'b':
	case KT_K_ENTER:
		ask_burn();
		break;
	case 'v':
		verify();
		break;
	case 'r':
		scan_drives();
		break;
	}
}

static void on_button(int bi)
{
	switch (bi) {
	case BB_SOURCE:
		if (!job.running)
			prompt_open(PR_SOURCE);
		break;
	case BB_BURN:
		ask_burn();
		break;
	case BB_VERIFY:
		verify();
		break;
	}
}

/* A dropped folder or file becomes the source: the first `file://` URI. */
static void take_drop(void)
{
	size_t len = 0;
	const char *uris = ktui_drop_take(&len);
	char line[BN_PATH], path[BN_PATH];

	for (const char *p = uris; p && p < uris + len && *p;) {
		size_t k = strcspn(p, "\r\n");

		if (k && *p != '#' && k < sizeof(line)) {
			memcpy(line, p, k);
			line[k] = '\0';
			if (kb_uri_path(line, path, sizeof(path))) {
				prompt = PR_NONE;
				status[0] = '\0';
				set_source(path);
				return;
			}
		}
		p += k;
		while (*p == '\r' || *p == '\n')
			p++;
	}
}

int burn_main(int argc, char **argv)
{
	const char *font = NULL;
	int dump = 0;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--compare") && i + 2 < argc)
			return compare_main(argv[i + 1], argv[i + 2]);
		else if (!strcmp(argv[i], "--font") && i + 1 < argc)
			font = argv[++i];
		else if (!strcmp(argv[i], "--fixture") && i + 1 < argc)
			fixture = argv[++i];
		else if (!strcmp(argv[i], "--dump"))
			dump = 1;
		else if (!strcmp(argv[i], "--no-icons"))
			icons_on = 0;
		else if (argv[i][0] != '-' && !source[0])
			set_source(argv[i]);
		else {
			fprintf(stderr, "usage: kdos-burn [--font NAME] "
					"[--no-icons] [--fixture SYS] [--dump] "
					"[FOLDER|IMAGE]\n"
					"       kdos-burn --compare IMAGE DEVICE\n");
			return 2;
		}
	}

	ktui_keys_layer(&keys, "Cancel", prompt_up, prompt_down, NULL);
	scan_drives();

	KDispConfig cfg = {
		.role = KDISP_ROLE_TOPLEVEL,
		.cols = BN_COLS,
		.rows = BN_ROWS,
		.min_cols = 52,
		.min_rows = 12,
		.title = "Burn",
		.app_id = "kdos-burn",
		.font = font,
		.keyboard = 1,
	};

	sh_theme_from_cache();
	if (dump) {
		ktui_offscreen_init(BN_COLS, BN_ROWS);
		ktui_draw_init();
		draw();
		ktui_draw_dump();
		return 0;
	}
	if (kdisp_init(&cfg, kdos_disp, kdos_disp_n) != 0) {
		fprintf(stderr, "kdos-burn: no display server\n");
		return 1;
	}
	ktui_draw_init();

	while (!kdisp_should_close()) {
		if (job.running && sh_job_pump(&job)) {
			if (job.running)
				snprintf(status, sizeof(status), "%.150s",
					 job.last);
			else
				job_finished();
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

				if (bi == BB_CLOSE)
					break;
				if (bi >= 0) {
					prompt = PR_NONE;
					on_button(bi);
					continue;
				}
			}
			int r = ktui_table_event(list_rect(), &tbl, ndrive, 4,
						 BN_NCOL, BN_COL, &ev, NULL,
						 NULL);

			if (r == KTUI_TABLE_PICKED)
				ask_burn();
			else if (r == KTUI_TABLE_CLOSE)
				break;
			continue;
		}
		if (ev.type != KT_EVT_KEY)
			continue;
		/* FIRST, except while the source prompt is up: typed text
		 * owns every printable key there, and the ladder still
		 * answers Escape through the layer. */
		if (prompt != PR_SOURCE || ev.key == KT_K_ESC) {
			int r = ktui_keys(&keys, &ev);

			if (r == KTUI_KEY_CLOSE)
				break;
			if (r == KTUI_KEY_TAKEN)
				continue;
		}
		/* An unhandled Ctrl or Alt chord arrives as its letter, and
		 * must not type that letter into the path. */
		if (prompt == PR_SOURCE && ev.key >= 0x20 && ev.key < 0x7f &&
		    (ev.mods & (KT_MOD_CTRL | KT_MOD_ALT)))
			continue;
		on_key(ev.key);
	}
	kdisp_shutdown();
	return 0;
}
