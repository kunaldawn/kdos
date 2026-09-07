/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-disks — the surface for the verbs that were reachable from nothing
 *
 *   ┌ Disks ───────────────────────────────────────────────┐
 *   │ DEVICE     LABEL          TYPE     SIZE   MOUNTED    │
 *   │ sdb1       KDOSSTICK      vfat     14.4G  /media/…   │
 *   │ sdb2       -              crypto_L 1.0G   -          │
 *   │ kdos-sdb2  vault          ext4     1.0G   -          │
 *   ├──────────────────────────────────────────────────────┤
 *   │ [ Mount ] [ Unlock ] [ SMART ] [ Partition ] [ Erase ]│
 *   └──────────────────────────────────────────────────────┘
 *
 * EVERY PRIVILEGED OPERATION IS A kdos-mountd VERB and this program runs as
 * the user. It opens no block device, forks no `mkfs` and holds no capability:
 * what it does is draw a list the daemon published and send back a row number.
 * That is the whole design — a disks window that needed root would be a setuid
 * binary with a text editor's attack surface.
 *
 * PARTITIONING IS `cfdisk` IN A TERMINAL and is not reimplemented here. A
 * partition editor is a program in its own right, `cfdisk` is on the image and
 * is what somebody who partitions disks already knows; a second one drawn in
 * cells would be a worse copy that had to be kept correct forever. It is
 * launched through `sh_term_argv()` like every other terminal program, so it
 * opens in whichever terminal this desktop is.
 *
 * THERE IS NO `fsck`. A filesystem check on a mounted volume corrupts it and
 * on an unmounted one takes minutes with no progress anybody can read; the
 * honest place for it is a shell, where the person can see what it says and
 * answer it. A button that started one and could not be stopped would be the
 * most dangerous control on this desktop.
 *
 * ERASE ASKS FOR THE DEVICE'S OWN NAME, typed. That is the daemon's rule, not
 * this surface's decoration: `format` is refused unless the fourth token is
 * the kernel name THE DAEMON published, so a surface cannot confirm on
 * somebody's behalf and a mis-click cannot reach it.
 * ---------------------------------
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>

#include "kbase.h"
#include "kwl.h"
#include "shell.h"

#define DK_COLS 74
#define DK_ROWS 20
#define DK_MAX 64
#define DK_ANSWER 64

/* The buttons, most-useful-first: the bar drops from the right when the
 * window is narrow, and what must survive is the verb somebody opened this
 * window to reach. */
enum { DB_MOUNT, DB_UNLOCK, DB_SMART, DB_PART, DB_ERASE, DB_CLOSE, DB_N };

static KtuiKeys keys;
static ShMountRow rows[DK_MAX];
static int nrows;
static char why[96];
static char status[160];
static KtuiTable tbl;
static int icons_on = 1;

/* An open prompt: which verb is waiting for typed text, and what has been
 * typed. `unlock` wants a passphrase and `format` wants the device's name. */
enum { PR_NONE, PR_PASS, PR_NAME };
static int prompt;
static char answer[DK_ANSWER];
static int caret;

static void refresh(void)
{
	nrows = sh_mountd_list(rows, DK_MAX, why, sizeof(why));
	ktui_table_clamp(&tbl, nrows, ktui_h > 6 ? ktui_h - 6 : 1);
}

static const ShMountRow *sel_row(void)
{
	return tbl.sel >= 0 && tbl.sel < nrows ? &rows[tbl.sel] : NULL;
}

static int is_locked(const ShMountRow *r)
{
	return r && !strcmp(r->fstype, "crypto_LUKS");
}

/* ── the verbs ─────────────────────────────────────────────────────────── */

static void do_verb(const char *verb)
{
	const ShMountRow *r = sel_row();

	if (!r)
		return;
	sh_mountd_do(r->idx, verb, status, sizeof(status));
	refresh();
}

/*
 * A SECOND FRAME CARRIES THE SECRET, never a token on the request line. The
 * daemon's parser splits on spaces, so a passphrase with one in it would
 * become two tokens and a bad request; and a passphrase in argv would sit in
 * `/proc/<pid>/cmdline` for the life of the process.
 */
static void send_secret(const char *verb, const char *text)
{
	const ShMountRow *r = sel_row();
	char req[128], buf[512];
	size_t n = strlen(text);

	if (!r)
		return;
	if (!strcmp(verb, "format"))
		snprintf(req, sizeof(req), "format %d ext4 %zu", r->idx, n);
	else
		snprintf(req, sizeof(req), "unlock %d %zu", r->idx, n);

	/*
	 * The request line and the secret go in ONE write, because the daemon
	 * reads the line and then the exact byte count that follows it — two
	 * writes would leave it blocked on a read the socket buffer had
	 * already satisfied.
	 */
	char whole[128 + DK_ANSWER + 2];
	int len = snprintf(whole, sizeof(whole), "%s\n%s", req, text);

	if (len < 0 || (size_t)len >= sizeof(whole))
		return;
	/* sh_mountd_ask appends the newline the request line needs, so the
	 * secret is passed as the tail of one "request". */
	whole[len] = '\0';
	if (sh_mountd_ask(whole, buf, sizeof(buf)) != 0)
		snprintf(status, sizeof(status), "kdos-mountd is not running");
	else {
		buf[strcspn(buf, "\r\n")] = '\0';
		snprintf(status, sizeof(status), "%.150s", buf);
	}
	refresh();
}

static void do_smart(void)
{
	const ShMountRow *r = sel_row();
	char buf[512];

	if (!r)
		return;
	if (sh_mountd_do(r->idx, "smart", buf, sizeof(buf)) != 0) {
		snprintf(status, sizeof(status), "%.150s", buf);
		return;
	}
	/* `ok model\tserial\thealth` — drawn as one line, because three
	 * facts about a drive are a sentence and not a page. */
	for (char *p = buf; *p; p++)
		if (*p == '\t')
			*p = ' ';
	snprintf(status, sizeof(status), "%.150s", buf + (strncmp(buf, "ok ", 3)
							  ? 0 : 3));
}

/*
 * `cfdisk` ON THE SELECTED ROW'S DISK, not on its partition. A partition
 * editor edits a partition TABLE, and pointing one at `/dev/sdb1` opens the
 * table inside a filesystem — which cfdisk reads as an empty disk and offers
 * to write.
 */
static void do_partition(void)
{
	const ShMountRow *r = sel_row();
	const char *av[12];
	char id[64], node[64];
	int n;

	if (!r)
		return;
	if (r->mnt[0]) {
		snprintf(status, sizeof(status),
			 "unmount %s first — the table is in use", r->kname);
		return;
	}
	/* The DISK behind the row: `sdb1` is on `sdb`, and a mapper name has
	 * no disk of its own to edit. */
	if (!strncmp(r->kname, "kdos-", 5)) {
		snprintf(status, sizeof(status),
			 "%s is a mapping, not a disk", r->kname);
		return;
	}
	size_t k = strlen(r->kname);

	while (k > 1 && r->kname[k - 1] >= '0' && r->kname[k - 1] <= '9')
		k--;
	snprintf(node, sizeof(node), "/dev/%.*s", (int)k, r->kname);

	n = sh_term_argv(av, 0, 8, "cfdisk", id, sizeof(id));
	av[n++] = "sudo";
	av[n++] = "cfdisk";
	av[n++] = node;
	av[n] = NULL;
	sh_spawn(av);
	snprintf(status, sizeof(status), "cfdisk on %s", node);
}

/* ── drawing ───────────────────────────────────────────────────────────── */

static const KtuiCol DK_COL[] = {
	{ "DEVICE", 12 }, { "LABEL", 0 },  { "TYPE", 12 },
	{ "SIZE", 7 },	  { "MOUNTED", 22 }
};
#define DK_NCOL 5

static void dk_cell(int idx, int col, int x, int y, int w, int fg, int bg,
		    void *user)
{
	const ShMountRow *r = &rows[idx];
	int on = bg == KT_ACCENT;

	(void)user;
	switch (col) {
	case 0:
		ktui_draw_text(x, y, w, r->kname, fg, bg, KT_A_NONE);
		break;
	case 1:
		ktui_draw_text(x, y, w, r->label[0] ? r->label : "-",
			       on ? KT_SURFACE : r->label[0] ? fg : KT_DIM, bg,
			       KT_A_NONE);
		break;
	case 2:
		ktui_draw_text(x, y, w, r->fstype,
			       on	       ? KT_SURFACE
			       : is_locked(r) ? KT_WARN
					      : KT_DIM,
			       bg, KT_A_NONE);
		break;
	case 3:
		ktui_draw_text(x, y, w, r->size, on ? KT_SURFACE : KT_DIM, bg,
			       KT_A_NONE);
		break;
	default:
		ktui_draw_text(x, y, w, r->mnt[0] ? r->mnt : "not mounted",
			       on	  ? KT_SURFACE
			       : r->mnt[0] ? KT_ACCENT
					   : KT_MID,
			       bg, KT_A_NONE);
		break;
	}
}

static void draw(void)
{
	int w = ktui_w, h = ktui_h;
	const ShMountRow *r = sel_row();
	int body;

	ktui_draw_fill(krect(0, 0, w, h), KT_BG);
	sh_frame(w, h, "Disks", KT_ACCENT, KT_BG, 1);

	/* The subtitle says what this window IS, in both states: the failure
	 * is said once, in the body, where the rows would have been. Saying it
	 * twice reads as two different problems. */
	int top = kch_header(w, "drive-harddisk", "Disks",
			     "removable volumes and what is on them", icons_on);

	body = h - top - 4;
	if (body < 1)
		body = 1;

	if (why[0]) {
		ktui_draw_text(2, top + 1, w - 4, why, KT_ERR, KT_BG,
			       KT_A_NONE);
	} else if (!nrows) {
		ktui_draw_text(2, top + 1, w - 4, "nothing removable is plugged in",
			       KT_MID, KT_BG, KT_A_NONE);
	} else {
		ktui_table_draw(krect(2, top, w - 4, body), &tbl, nrows, DK_COL,
				DK_NCOL, dk_cell, NULL, NULL, -1);
	}

	ktui_draw_hline(1, h - 4, w - 2, KT_G_HL, KT_DIM, KT_BG);

	struct kch_button b[DB_N];

	b[DB_MOUNT] = (struct kch_button){ r && r->mnt[0] ? "Unmount" : "Mount",
					   r && !is_locked(r) };
	b[DB_UNLOCK] = (struct kch_button){ "Unlock", is_locked(r) };
	b[DB_SMART] = (struct kch_button){ "SMART", r != NULL };
	b[DB_PART] = (struct kch_button){ "Partition", r && !r->mnt[0] };
	b[DB_ERASE] = (struct kch_button){ "Erase", r && !r->mnt[0] };
	b[DB_CLOSE] = (struct kch_button){ "Close", 1 };

	int bx = kch_buttons(w, h - 2, b, DB_N, -1);
	int room = bx - 3;

	if (prompt) {
		char shown[DK_ANSWER + 8];

		if (prompt == PR_PASS) {
			size_t n = strlen(answer);

			if (n > sizeof(shown) - 2)
				n = sizeof(shown) - 2;
			memset(shown, '*', n);
			shown[n] = '\0';
		} else {
			snprintf(shown, sizeof(shown), "%s", answer);
		}
		ktui_draw_textf(2, h - 3, w - 4, KT_TEXT, KT_BG, KT_A_NONE,
				"%s %s",
				prompt == PR_PASS
					? "Passphrase:"
					: "Erase as ext4 — type the device name:",
				shown);
	} else if (status[0] && room > 0) {
		ktui_draw_text(2, h - 2, room, status, KT_MID, KT_BG,
			       KT_A_NONE);
	}

	ktui_hint_if(!prompt && r && !is_locked(r), "Enter",
		     r && r->mnt[0] ? "unmount" : "mount");
	ktui_hint_if(!prompt && is_locked(r), "u", "unlock");
	ktui_hint_if(!prompt && r, "s", "SMART");
	ktui_hint_if(!prompt && r && !r->mnt[0], "p", "partition");
	ktui_hint_if(!prompt, "r", "rescan");
	ktui_hint_if(prompt != 0, "Enter", "confirm");
	ktui_hint("Esc", prompt ? "cancel" : ktui_esc_verb(&keys));
	ktui_hint_row(&keys, krect(2, h - 3 + (prompt ? 1 : 0), w - 4, 1),
		      KT_BG);
	if (prompt)
		ktui_term_caret(-1, -1);
}

/* ── keys ──────────────────────────────────────────────────────────────── */

static void prompt_open(int which)
{
	prompt = which;
	answer[0] = '\0';
	caret = 0;
	status[0] = '\0';
}

static void prompt_take(void)
{
	int was = prompt;

	prompt = PR_NONE;
	if (!answer[0])
		return;
	send_secret(was == PR_PASS ? "unlock" : "format", answer);
	/* THE SECRET DOES NOT OUTLIVE THE PROMPT. A passphrase left in a
	 * surface's static buffer is one a core dump carries. */
	memset(answer, 0, sizeof(answer));
	caret = 0;
}

static int on_key(int k)
{
	if (prompt) {
		int n = (int)strlen(answer);

		if (k == KT_K_ESC) {
			prompt = PR_NONE;
			memset(answer, 0, sizeof(answer));
			caret = 0;
		} else if (k == KT_K_ENTER) {
			prompt_take();
		} else if (k == KT_K_BACKSPACE) {
			if (caret > 0) {
				memmove(answer + caret - 1, answer + caret,
					(size_t)(n - caret) + 1);
				caret--;
			}
		} else if (k >= 0x20 && k < 0x7f && n + 1 < DK_ANSWER) {
			memmove(answer + caret + 1, answer + caret,
				(size_t)(n - caret) + 1);
			answer[caret++] = (char)k;
		}
		return 0;
	}

	if (ktui_table_key(&tbl, nrows, ktui_h - 6, k, NULL, NULL))
		return 0;

	const ShMountRow *r = sel_row();

	switch (k) {
	case KT_K_ENTER:
		if (r && !is_locked(r))
			do_verb(r->mnt[0] ? "unmount" : "mount");
		break;
	case 'u':
		if (is_locked(r))
			prompt_open(PR_PASS);
		break;
	case 'c':
		/* Close the MAPPING, which is the other half of unlock and
		 * has no button: it is the rare verb, and a bar that carried
		 * every verb would drop the common ones on a narrow window. */
		if (r && !strncmp(r->kname, "kdos-", 5))
			do_verb("close");
		break;
	case 's':
		do_smart();
		break;
	case 'p':
		do_partition();
		break;
	case 'e':
		if (r && !r->mnt[0])
			prompt_open(PR_NAME);
		break;
	case 'r':
		refresh();
		break;
	case 'q':
		return 1;
	}
	return 0;
}

static void on_button(int bi)
{
	const ShMountRow *r = sel_row();

	switch (bi) {
	case DB_MOUNT:
		if (r && !is_locked(r))
			do_verb(r->mnt[0] ? "unmount" : "mount");
		break;
	case DB_UNLOCK:
		if (is_locked(r))
			prompt_open(PR_PASS);
		break;
	case DB_SMART:
		do_smart();
		break;
	case DB_PART:
		do_partition();
		break;
	case DB_ERASE:
		if (r && !r->mnt[0])
			prompt_open(PR_NAME);
		break;
	}
}

int disks_main(int argc, char **argv)
{
	const char *font = NULL;
	int dump = 0;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--font") && i + 1 < argc)
			font = argv[++i];
		else if (!strcmp(argv[i], "--dump"))
			dump = 1;
		else if (!strcmp(argv[i], "--no-icons"))
			icons_on = 0;
		else {
			fprintf(stderr, "usage: kdos-disks [--font NAME] "
					"[--no-icons] [--dump]\n");
			return 2;
		}
	}

	KDispConfig cfg = {
		.role = KDISP_ROLE_TOPLEVEL,
		.cols = DK_COLS,
		.rows = DK_ROWS,
		.min_cols = 56,
		.min_rows = 12,
		.title = "Disks",
		.app_id = "kdos-disks",
		.font = font,
		.keyboard = 1,
	};

	sh_theme_from_cache();
	if (dump) {
		ktui_offscreen_init(DK_COLS, DK_ROWS);
		ktui_draw_init();
		refresh();
		draw();
		ktui_draw_dump();
		return 0;
	}
	if (kdisp_init(&cfg, kdos_disp, kdos_disp_n) != 0) {
		fprintf(stderr, "kdos-disks: no display server\n");
		return 1;
	}
	ktui_draw_init();
	refresh();

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
		if (ev.type == KT_EVT_MOUSE) {
			if (ev.press == KT_MP_DRAG) {
				kch_hover(ev.mx, ev.my);
				continue;
			}
			if (ev.press != KT_MP_PRESS)
				continue;

			int bi = kch_button_at(ev.mx, ev.my);

			if (bi == DB_CLOSE)
				break;
			if (bi >= 0) {
				on_button(bi);
				continue;
			}
			int idx = ktui_table_hit(krect(2, 4, ktui_w - 4,
						       ktui_h - 8),
						 &tbl, nrows, DK_NCOL, DK_COL,
						 ev.mx, ev.my);
			ktui_table_pick(&tbl, nrows, idx, NULL, NULL);
			continue;
		}
		if (ev.type != KT_EVT_KEY)
			continue;

		/* FIRST, above this surface's own switch — except while a
		 * prompt is up, which owns Escape: the ladder would close the
		 * window on the key that must abandon the passphrase. */
		if (!prompt && ktui_keys(&keys, &ev) == KTUI_KEY_CLOSE)
			break;
		if (on_key(ev.key))
			break;
	}

	memset(answer, 0, sizeof(answer));
	kdisp_shutdown();
	return 0;
}
