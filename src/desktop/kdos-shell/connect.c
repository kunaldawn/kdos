/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-connect — a share on another machine, and where it landed
 *
 *   ┌ connect to server ───────────────────────────────────┐
 *   │ 0  //files.example/team   /media/kdos/files-team     │
 *   ├──────────────────────────────────────────────────────┤
 *   │ Server   files.example                               │
 *   │ Share    team                                        │
 *   │ User     ada          Domain  -                      │
 *   │ Password ******                                      │
 *   │ Enter connect  Tab field  Esc cancel                 │
 *   └──────────────────────────────────────────────────────┘
 *
 * THIS BINARY MOUNTS NOTHING. It holds no capability, opens no device and
 * forks no helper: what it does is fill five fields and hand them to
 * `kdos-mountd`, which owns every privileged path on this system. A connect
 * window that needed root would be a setuid binary with a text editor's
 * attack surface.
 *
 * THE PASSWORD IS A SECOND FRAME AND NEVER A TOKEN. The daemon's request line
 * is split on spaces, so a password with one in it would become two tokens;
 * and a password in argv sits in `/proc/<pid>/cmdline` for every process on
 * the machine to read for as long as the child lives. It does not outlive the
 * request here either — the buffer is wiped when the answer comes back.
 *
 * THE FIELDS ARE NOT CHECKED HERE, THEY ARE CHECKED THERE. `mount.cifs`
 * assembles its option string by concatenation and escapes nothing but the
 * password, so a comma in any of these four is a mount option; the allowlist
 * that refuses one is the daemon's, because a check in a surface is a check
 * anything else talking to the socket does not get.
 *
 * THE LIST IS WHAT `/proc/mounts` SAYS, asked again after every verb. A row
 * number is true only of the list it came with.
 * ---------------------------------
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kbase.h"
#include "kchrome.h"
#include "kwl.h"
#include "shell.h"

#define CN_COLS 64
#define CN_ROWS 18
#define CN_MAX 16
#define CN_FIELD 256

enum { F_SERVER, F_SHARE, F_USER, F_DOMAIN, F_PASS, F_N };
enum { CB_CONNECT, CB_DISCONNECT, CB_CLOSE, CB_N };

static const char *const FLABEL[F_N] = {
	"Server", "Share", "User", "Domain", "Password"
};

static char field[F_N][CN_FIELD];
static int focus;			/* which field, when the form is up */
static int caret;
static int form;			/* 0 the list, 1 the form           */

static ShShareRow rows[CN_MAX];
static int nrows;
static const char *why;			/* why there are no rows            */
static char status[192];
static KtuiTable tbl;
static KtuiKeys keys;

static void refresh(void)
{
	static char no[160];

	why = NULL;
	nrows = sh_mountd_shares(rows, CN_MAX, no, sizeof(no));
	if (no[0])
		why = no;
	if (tbl.sel >= nrows)
		tbl.sel = nrows ? nrows - 1 : 0;
}

/*
 * THE REQUEST LINE AND THE SECRET IN ONE WRITE. The daemon reads a line and
 * then the exact byte count that line named, and two writes would leave it
 * blocked on a read the socket buffer had already satisfied.
 */
static void send_connect(void)
{
	char req[CN_FIELD * 4 + 64];
	char whole[sizeof(req) + CN_FIELD + 2];
	char buf[512];
	size_t n = strlen(field[F_PASS]);
	int len;

	if (!field[F_SERVER][0] || !field[F_SHARE][0] || !field[F_USER][0]) {
		snprintf(status, sizeof(status),
			 "a server, a share and a user are all needed");
		return;
	}
	if (!n) {
		snprintf(status, sizeof(status), "a password is needed");
		return;
	}
	/* `-` IS THE WHOLE OF "no domain": a token cannot be empty, because
	 * the line is split on spaces. */
	snprintf(req, sizeof(req), "cifs %s %s %s %s %zu", field[F_SERVER],
		 field[F_SHARE], field[F_USER],
		 field[F_DOMAIN][0] ? field[F_DOMAIN] : "-", n);
	len = snprintf(whole, sizeof(whole), "%s\n%s", req, field[F_PASS]);
	if (len < 0 || (size_t)len >= sizeof(whole)) {
		snprintf(status, sizeof(status), "that is longer than a "
						 "request can be");
		return;
	}
	if (sh_mountd_ask(whole, buf, sizeof(buf)) != 0)
		snprintf(status, sizeof(status), "kdos-mountd is not running");
	else {
		buf[strcspn(buf, "\r\n")] = '\0';
		snprintf(status, sizeof(status), "%.180s", buf);
		if (!strncmp(buf, "ok", 2))
			form = 0;
	}
	/* THE SECRET DOES NOT OUTLIVE THE REQUEST. One left in a surface's
	 * static buffer is one a core dump carries. */
	memset(whole, 0, sizeof(whole));
	memset(field[F_PASS], 0, sizeof(field[F_PASS]));
	refresh();
}

static void send_disconnect(void)
{
	char buf[256];

	if (!nrows)
		return;
	if (sh_mountd_do(rows[tbl.sel].idx, "disconnect", buf,
			 sizeof(buf)) != 0)
		snprintf(status, sizeof(status), "%.180s", buf);
	else
		snprintf(status, sizeof(status), "%.180s", buf);
	refresh();
}

/* ── drawing ───────────────────────────────────────────────────────────── */

static const KtuiCol CN_COL[] = {
	{ NULL, 30 },		/* the UNC                                 */
	{ NULL, 0 }		/* where it is                             */
};
#define CN_NCOL 2

static void cn_cell(int row, int col, int x, int y, int w, int fg, int bg,
		    void *user)
{
	(void)user;
	ktui_draw_text(x, y, w, col ? rows[row].mnt : rows[row].unc,
		       col ? (bg == KT_ACCENT ? KT_SURFACE : KT_DIM) : fg, bg,
		       KT_A_NONE);
}

static void draw_form(int w, int top, int body)
{
	for (int i = 0; i < F_N && i < body; i++) {
		char shown[CN_FIELD];
		int on = i == focus;

		if (i == F_PASS) {
			size_t n = strlen(field[i]);

			if (n > sizeof(shown) - 1)
				n = sizeof(shown) - 1;
			memset(shown, '*', n);
			shown[n] = '\0';
		} else {
			snprintf(shown, sizeof(shown), "%s", field[i]);
		}
		ktui_draw_text(2, top + i, 9, FLABEL[i], KT_MID, KT_SURFACE,
			       KT_A_NONE);
		ktui_draw_fill(krect(11, top + i, w - 13, 1),
			       on ? KT_ACCENT : KT_SURFACE);
		ktui_draw_text(11, top + i, w - 13, shown,
			       on ? KT_SURFACE : KT_TEXT,
			       on ? KT_ACCENT : KT_SURFACE, KT_A_NONE);
	}
	ktui_term_caret(11 + caret, top + focus);
}

static void draw(void)
{
	int w = ktui_w, h = ktui_h;
	int top = 1, body = h - 5;

	ktui_draw_fill(krect(0, 0, w, h), KT_SURFACE);
	ktui_draw_box(krect(0, 0, w, h), "connect to server", KT_ACCENT,
		      KT_SURFACE, 1);

	if (form) {
		draw_form(w, top, body);
	} else if (why) {
		ktui_draw_text(2, top + 1, w - 4, why, KT_MID, KT_SURFACE,
			       KT_A_NONE);
	} else if (!nrows) {
		ktui_draw_text(2, top + 1, w - 4,
			       "nothing connected — Connect names a server",
			       KT_MID, KT_SURFACE, KT_A_NONE);
	} else {
		ktui_table_draw(krect(2, top, w - 4, body), &tbl, nrows,
				CN_COL, CN_NCOL, cn_cell, NULL, NULL, -1);
	}

	ktui_draw_hline(1, h - 4, w - 2, KT_G_HL, KT_DIM, KT_SURFACE);

	struct kch_button b[CB_N];

	b[CB_CONNECT] = (struct kch_button){ form ? "Connect" : "Connect…", 1 };
	b[CB_DISCONNECT] = (struct kch_button){ "Disconnect", !form && nrows };
	b[CB_CLOSE] = (struct kch_button){ "Close", 1 };

	int bx = kch_buttons(w, h - 2, b, CB_N, -1);
	int room = bx - 3;

	if (status[0] && room > 0)
		ktui_draw_text(2, h - 2, room,
			       status, strncmp(status, "err", 3) ? KT_MID
								: KT_ERR,
			       KT_SURFACE, KT_A_NONE);

	ktui_hint("Enter", form ? "connect" : "connect…");
	ktui_hint_if(form, "Tab", "field");
	ktui_hint_if(!form && nrows > 0, "d", "disconnect");
	ktui_hint("Esc", form ? "cancel" : ktui_esc_verb(&keys));
	ktui_hint_row(&keys, krect(2, h - 3, w - 4, 1), KT_SURFACE);
	if (!form)
		ktui_term_caret(-1, -1);
}

/* ── keys ──────────────────────────────────────────────────────────────── */

static void form_open(void)
{
	form = 1;
	focus = field[F_SERVER][0] ? F_PASS : F_SERVER;
	caret = (int)strlen(field[focus]);
	status[0] = '\0';
}

static void form_move(int by)
{
	focus = (focus + by + F_N) % F_N;
	caret = (int)strlen(field[focus]);
}

static int form_key(const KtuiEvent *ev)
{
	char *f = field[focus];
	int n = (int)strlen(f);

	if (ev->key == KT_K_ENTER) {
		send_connect();
		return 1;
	}
	if (ev->key == KT_K_TAB || ev->key == KT_K_DOWN) {
		form_move(1);
		return 1;
	}
	if (ev->key == KT_K_BTAB || ev->key == KT_K_UP) {
		form_move(-1);
		return 1;
	}
	if (ev->key == KT_K_BACKSPACE) {
		if (caret > 0) {
			memmove(f + caret - 1, f + caret,
				(size_t)(n - caret) + 1);
			caret--;
		}
		return 1;
	}
	if (ev->key == KT_K_LEFT) {
		if (caret > 0)
			caret--;
		return 1;
	}
	if (ev->key == KT_K_RIGHT) {
		if (caret < n)
			caret++;
		return 1;
	}
	if (ev->key == KT_K_HOME) {
		caret = 0;
		return 1;
	}
	if (ev->key == KT_K_END) {
		caret = n;
		return 1;
	}
	if (ev->key >= 0x20 && ev->key < 0x7f && n + 1 < CN_FIELD) {
		memmove(f + caret + 1, f + caret, (size_t)(n - caret) + 1);
		f[caret++] = (char)ev->key;
		return 1;
	}
	return 1;			/* the form takes every key */
}

int connect_main(int argc, char **argv)
{
	const char *font = NULL;
	int dump = 0;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--font") && i + 1 < argc)
			font = argv[++i];
		else if (!strcmp(argv[i], "--dump"))
			dump = 1;
		else {
			fprintf(stderr,
				"usage: kdos-connect [--font NAME] [--dump]\n");
			return 2;
		}
	}

	KDispConfig cfg = {
		.role = KDISP_ROLE_OVERLAY,
		.cols = CN_COLS,
		.rows = CN_ROWS,
		.app_id = "kdos-connect",
		.font = font,
		.keyboard = 1,
	};

	sh_theme_from_cache();
	if (dump) {
		ktui_offscreen_init(CN_COLS, CN_ROWS);
		ktui_draw_init();
		refresh();
		draw();
		ktui_draw_dump();
		return 0;
	}
	if (kdisp_init(&cfg, kdos_disp, kdos_disp_n) != 0) {
		fprintf(stderr, "kdos-connect: no display server\n");
		return 1;
	}
	ktui_draw_init();
	kch_px_popup(KT_SURFACE);
	/* The page in /usr/share/kdos/doc that F1 opens; a name with no file
	 * there is refused by testing/preflight.sh. */
	keys.doc = "connect";
	keys.help = sh_help;
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
			int bi = kch_button_at(ev.mx, ev.my);

			kch_hover(ev.mx, ev.my);
			if (ev.btn != KT_MB_LEFT)
				continue;
			if (bi == CB_CLOSE)
				break;
			if (bi == CB_CONNECT) {
				if (form)
					send_connect();
				else
					form_open();
				continue;
			}
			if (bi == CB_DISCONNECT) {
				if (!form)
					send_disconnect();
				continue;
			}
			if (!form && ev.my - 1 >= 0 && ev.my - 1 < nrows)
				tbl.sel = ev.my - 1;
			continue;
		}
		if (ev.type != KT_EVT_KEY)
			continue;

		/*
		 * THE FORM TAKES EVERY KEY BEFORE THE RUNG POOL, and Esc is
		 * the exception it hands back. A surface whose text field sat
		 * under the pool would close the window the first time
		 * somebody typed a letter the pool had a meaning for.
		 */
		if (form) {
			if (ev.key == KT_K_ESC) {
				form = 0;
				memset(field[F_PASS], 0,
				       sizeof(field[F_PASS]));
				continue;
			}
			form_key(&ev);
			continue;
		}

		if (ktui_keys(&keys, &ev) == KTUI_KEY_CLOSE)
			break;

		if (ev.key == KT_K_ENTER) {
			form_open();
			continue;
		}
		if (ev.key == 'd' || ev.key == 'D') {
			send_disconnect();
			continue;
		}
		if (ev.key == 'r' || ev.key == 'R') {
			refresh();
			continue;
		}
		ktui_table_key(&tbl, nrows, ktui_h - 5, ev.key, NULL, NULL);
	}

	kdisp_shutdown();
	memset(field, 0, sizeof(field));
	return 0;
}
