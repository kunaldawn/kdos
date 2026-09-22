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
 * AND A KERBEROS SHARE HAS NO SECOND FRAME AT ALL. `Sign in` chooses between
 * a password and the ticket `kinit` already put in the credential cache; on
 * the ticket the password row is not asked for, because nothing about a
 * ticket crosses this socket — the kernel gets it from `cifs.upcall`, which
 * reads that cache directly. It is a different verb rather than an empty
 * password, so a surface cannot ask for a Kerberos mount by accident.
 *
 * THE FIELDS ARE NOT CHECKED HERE, THEY ARE CHECKED THERE. `mount.cifs`
 * assembles its option string by concatenation and escapes nothing but the
 * password, so a comma in any of these four is a mount option; the allowlist
 * that refuses one is the daemon's, because a check in a surface is a check
 * anything else talking to the socket does not get.
 *
 * THE LIST IS WHAT `/proc/mounts` SAYS, asked again after every verb. A row
 * number is true only of the list it came with.
 *
 * BROWSE IS A THIRD LIST AND IT IS NOT A DIRECTORY. There is no browse master
 * on this image to ask, so what `Browse` shows is whatever answered an mDNS
 * and a NetBIOS broadcast in the moment it was pressed — machines come and go
 * from it, and an empty one means nothing answered rather than that the verb
 * failed. It BLOCKS for as long as a broadcast takes, so the frame that says
 * so is drawn and flushed before the call rather than after it.
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
#define CN_SRV 32
#define CN_FIELD 256

enum { F_SERVER, F_SHARE, F_USER, F_DOMAIN, F_AUTH, F_PASS, F_N };
enum { CB_CONNECT, CB_BROWSE, CB_DISCONNECT, CB_CLOSE, CB_N };

/* The three things this window can be showing. They are exclusive: the form
 * takes every key, and a table under it would answer the same arrows. */
enum { M_LIST, M_FORM, M_BROWSE };

static const char *const FLABEL[F_N] = {
	"Server", "Share", "User", "Domain", "Sign in", "Password"
};

/* `Sign in` IS A CHOICE AND NOT A FIELD SOMEBODY TYPES INTO: two spellings of
 * one answer is a form that can be filled in wrongly. Space, Left and Right
 * move it and every other key is ignored. */
static const char *const FAUTH[2] = { "password", "Kerberos ticket" };
static int auth;

static char field[F_N][CN_FIELD];
static int focus;			/* which field, when the form is up */
static int caret;
static int mode = M_LIST;

static ShShareRow rows[CN_MAX];
static int nrows;
static const char *why;			/* why there are no rows            */
static ShServerRow srv[CN_SRV];
static int nsrv;
/* Set once a browse has been made, so an empty list can say "nothing
 * answered" rather than "press Browse", which is a different sentence. */
static int browsed;
static char status[192];
/* A dump renders one frame and prints it. The "asking the network…" frame
 * below is drawn and flushed for a person to see; flushed into a dump it would
 * be a second frame in a golden that holds one. */
static int dumping;
static KtuiTable tbl;
static KtuiTable stbl;
static KtuiKeys keys;

/* Forward: the browse runner below draws the frame that says it is asking
 * before it blocks, and the drawing lives past it. */
static void draw(void);

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

	if (!field[F_SERVER][0] || !field[F_SHARE][0]) {
		snprintf(status, sizeof(status),
			 "a server and a share are both needed");
		return;
	}
	/*
	 * A TICKET NAMES ITS OWN PRINCIPAL, so a username is what a password
	 * mount needs and what a Kerberos one may leave out. `-` is the whole
	 * of "no username" on the wire, because a token cannot be empty.
	 */
	if (auth == 0 && !field[F_USER][0]) {
		snprintf(status, sizeof(status), "a user is needed");
		return;
	}
	if (auth == 1) {
		char buf[512];

		snprintf(req, sizeof(req), "krb5 %s %s %s %s",
			 field[F_SERVER], field[F_SHARE],
			 field[F_USER][0] ? field[F_USER] : "-",
			 field[F_DOMAIN][0] ? field[F_DOMAIN] : "-");
		if (sh_mountd_ask(req, buf, sizeof(buf)) != 0) {
			snprintf(status, sizeof(status),
				 "kdos-mountd is not running");
		} else {
			buf[strcspn(buf, "\r\n")] = '\0';
			snprintf(status, sizeof(status), "%.180s", buf);
			if (!strncmp(buf, "ok", 2))
				mode = M_LIST;
		}
		refresh();
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
			mode = M_LIST;
	}
	/* THE SECRET DOES NOT OUTLIVE THE REQUEST. One left in a surface's
	 * static buffer is one a core dump carries. */
	memset(whole, 0, sizeof(whole));
	memset(field[F_PASS], 0, sizeof(field[F_PASS]));
	refresh();
}

/*
 * THE FRAME THAT SAYS SO IS FLUSHED FIRST. This call is a broadcast and a
 * wait, not a file read: without the flush the window sits on the frame before
 * the press for the whole of it, which reads as a surface that has hung.
 */
static void send_browse(void)
{
	static char no[160];

	snprintf(status, sizeof(status), "asking the network…");
	if (!dumping) {
		draw();
		ktui_draw_flush();
	}
	nsrv = sh_mountd_browse(srv, CN_SRV, no, sizeof(no));
	browsed = 1;
	mode = M_BROWSE;
	if (stbl.sel >= nsrv)
		stbl.sel = nsrv ? nsrv - 1 : 0;
	snprintf(status, sizeof(status), "%s",
		 no[0] ? no
		       : nsrv ? "Enter fills the server field"
			      : "nothing answered");
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

/* The address is a column and not a suffix on the name: two machines with the
 * same NetBIOS name on different subnets are one row apart otherwise. */
static const KtuiCol CN_SCOL[] = {
	{ NULL, 32 },		/* the server's name                       */
	{ NULL, 0 }		/* where it answered from                  */
};

static void sv_cell(int row, int col, int x, int y, int w, int fg, int bg,
		    void *user)
{
	(void)user;
	ktui_draw_text(x, y, w, col ? srv[row].addr : srv[row].name,
		       col ? (bg == KT_ACCENT ? KT_SURFACE : KT_DIM) : fg, bg,
		       KT_A_NONE);
}

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
		/* THE PASSWORD ROW IS DIMMED AND NOT HIDDEN on a ticket: a row
		 * that disappears moves every row under it, and the form is
		 * the same form either way. */
		int off = i == F_PASS && auth == 1;

		if (i == F_AUTH) {
			snprintf(shown, sizeof(shown), "%s   (Space changes)",
				 FAUTH[auth]);
		} else if (i == F_PASS) {
			size_t n = auth == 1 ? 0 : strlen(field[i]);

			if (n > sizeof(shown) - 1)
				n = sizeof(shown) - 1;
			memset(shown, '*', n);
			shown[n] = '\0';
			if (off)
				snprintf(shown, sizeof(shown),
					 "not asked for — the ticket answers");
		} else {
			snprintf(shown, sizeof(shown), "%s", field[i]);
		}
		ktui_draw_text(2, top + i, 9, FLABEL[i],
			       off ? KT_DIM : KT_MID, KT_SURFACE, KT_A_NONE);
		{
			int cfg, cbg;

			ktui_sel_slots(on, 1, KT_SURFACE, &cfg, &cbg);
			ktui_draw_fill(krect(11, top + i, w - 13, 1), cbg);
			ktui_draw_text(11, top + i, w - 13, shown,
				       off && !on ? KT_DIM : cfg, cbg,
				       KT_A_NONE);
		}
	}
	/* NO CARET WHERE THERE IS NOTHING TO TYPE. The choice row and the
	 * password row on a ticket both take no text, and a caret sitting in
	 * one is a promise the key handler does not keep. */
	if (focus == F_AUTH || (focus == F_PASS && auth == 1))
		ktui_term_caret(-1, -1);
	else
		ktui_term_caret(11 + caret, top + focus);
}

static void draw(void)
{
	int w = ktui_w, h = ktui_h;
	int top = 1, body = h - 5;

	ktui_draw_fill(krect(0, 0, w, h), KT_SURFACE);
	ktui_draw_box(krect(0, 0, w, h), "connect to server", KT_ACCENT,
		      KT_SURFACE, 1);

	if (mode == M_FORM) {
		draw_form(w, top, body);
	} else if (mode == M_BROWSE) {
		if (!nsrv)
			ktui_draw_text(2, top + 1, w - 4,
				       browsed ? "no machine on this network "
						 "answered — type a name instead"
					       : "Browse asks the network",
				       KT_MID, KT_SURFACE, KT_A_NONE);
		else
			ktui_table_draw(krect(2, top, w - 4, body), &stbl,
					nsrv, CN_SCOL, CN_NCOL, sv_cell, NULL,
					NULL, -1);
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

	b[CB_CONNECT] = (struct kch_button){
		mode == M_FORM ? "Connect" : "Connect…", mode != M_BROWSE
	};
	b[CB_BROWSE] = (struct kch_button){ "Browse", mode != M_FORM };
	b[CB_DISCONNECT] = (struct kch_button){ "Disconnect",
						mode == M_LIST && nrows };
	b[CB_CLOSE] = (struct kch_button){ "Close", 1 };

	int bx = kch_buttons(w, h - 2, b, CB_N, -1);
	int room = bx - 3;

	if (status[0] && room > 0)
		ktui_draw_text(2, h - 2, room,
			       status, strncmp(status, "err", 3) ? KT_MID
								: KT_ERR,
			       KT_SURFACE, KT_A_NONE);

	ktui_hint("Enter", mode == M_FORM	 ? "connect"
			  : mode == M_BROWSE ? "use this server"
					     : "connect…");
	ktui_hint_if(mode == M_FORM, "Tab", "field");
	ktui_hint_if(mode == M_LIST, "b", "browse");
	ktui_hint_if(mode == M_LIST && nrows > 0, "d", "disconnect");
	ktui_hint("Esc", mode == M_LIST ? ktui_esc_verb(&keys) : "back");
	ktui_hint_row(&keys, krect(2, h - 3, w - 4, 1), KT_SURFACE);
	if (mode != M_FORM)
		ktui_term_caret(-1, -1);
}

/* ── keys ──────────────────────────────────────────────────────────────── */

static void form_open(void)
{
	mode = M_FORM;
	/* THE FIRST ROW THAT STILL WANTS AN ANSWER. A server already filled in
	 * — by a browse row, or by the last attempt — means the password is
	 * what is left, and on a ticket there is no password either. */
	focus = !field[F_SERVER][0] ? F_SERVER : auth == 1 ? F_SHARE : F_PASS;
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
	/*
	 * THE CHOICE ROW ANSWERS BEFORE THE TEXT HANDLING BELOW, and it takes
	 * Left and Right away from the caret because there is no caret on it.
	 * Every other key falls through to the moves above and below, so Tab
	 * still leaves the row.
	 */
	if (focus == F_AUTH) {
		if (ev->key == ' ' || ev->key == KT_K_LEFT ||
		    ev->key == KT_K_RIGHT) {
			auth = !auth;
			/* A PASSWORD TYPED AND THEN NOT USED IS STILL A
			 * PASSWORD IN THIS PROCESS'S MEMORY. */
			if (auth)
				memset(field[F_PASS], 0,
				       sizeof(field[F_PASS]));
		}
		if (ev->key != KT_K_TAB && ev->key != KT_K_BTAB &&
		    ev->key != KT_K_UP && ev->key != KT_K_DOWN)
			return 1;
	}
	/* AND THE PASSWORD ROW TAKES NO TEXT WHEN IT IS NOT ASKED FOR. */
	if (focus == F_PASS && auth == 1 && ev->key != KT_K_TAB &&
	    ev->key != KT_K_BTAB && ev->key != KT_K_UP &&
	    ev->key != KT_K_DOWN)
		return 1;
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
	if (ev->key >= 0x20 && ev->key < 0x7f &&
	    !(ev->mods & (KT_MOD_CTRL | KT_MOD_ALT)) && n + 1 < CN_FIELD) {
		memmove(f + caret + 1, f + caret, (size_t)(n - caret) + 1);
		f[caret++] = (char)ev->key;
		return 1;
	}
	return 1;			/* the form takes every key */
}

int connect_main(int argc, char **argv)
{
	const char *font = NULL;
	int dump = 0, want_browse = 0;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--font") && i + 1 < argc)
			font = argv[++i];
		else if (!strcmp(argv[i], "--dump"))
			dump = 1;
		/* The window opens on the connected list; `--browse` is what
		 * a dump needs to reach the other one, because a dump takes
		 * no keys. */
		else if (!strcmp(argv[i], "--browse"))
			want_browse = 1;
		else {
			fprintf(stderr, "usage: kdos-connect [--font NAME] "
					"[--dump] [--browse]\n");
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
		dumping = 1;
		ktui_offscreen_init(CN_COLS, CN_ROWS);
		ktui_draw_init();
		refresh();
		if (want_browse)
			send_browse();
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
	if (want_browse)
		send_browse();

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
			/*
			 * A DETENT SCROLLS THE TABLE. It is answered before
			 * anything below, because a wheel tick arrives as a
			 * press with no release and would otherwise fall into
			 * the button arm and run whichever row it passed over.
			 */
			if (ev.btn == KT_MB_WHEEL_UP ||
			    ev.btn == KT_MB_WHEEL_DOWN) {
				int k = ev.btn == KT_MB_WHEEL_UP ? KT_K_UP
								: KT_K_DOWN;

				if (mode == M_BROWSE)
					ktui_table_key(&stbl, nsrv,
						       ktui_h - 5, k, NULL,
						       NULL);
				else
					ktui_table_key(&tbl, nrows,
						       ktui_h - 5, k, NULL,
						       NULL);
				continue;
			}
			int bi = kch_button_at(ev.mx, ev.my);

			kch_hover(ev.mx, ev.my);
			if (ev.btn != KT_MB_LEFT)
				continue;
			if (bi == CB_CLOSE)
				break;
			if (bi == CB_CONNECT) {
				if (mode == M_FORM)
					send_connect();
				else if (mode == M_LIST)
					form_open();
				continue;
			}
			if (bi == CB_BROWSE) {
				if (mode != M_FORM)
					send_browse();
				continue;
			}
			if (bi == CB_DISCONNECT) {
				if (mode == M_LIST)
					send_disconnect();
				continue;
			}
			if (mode == M_BROWSE && ev.my - 1 >= 0 &&
			    ev.my - 1 < nsrv)
				stbl.sel = ev.my - 1;
			else if (mode == M_LIST && ev.my - 1 >= 0 &&
				 ev.my - 1 < nrows)
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
		if (mode == M_FORM) {
			if (ev.key == KT_K_ESC) {
				mode = M_LIST;
				memset(field[F_PASS], 0,
				       sizeof(field[F_PASS]));
				continue;
			}
			form_key(&ev);
			continue;
		}

		/*
		 * THE BROWSE LIST ANSWERS ESCAPE ITSELF, before the rung
		 * pool: the pool's Escape closes the window, and a person
		 * backing out of a list they opened by accident means the
		 * list and not the session.
		 */
		if (mode == M_BROWSE) {
			if (ev.key == KT_K_ESC) {
				mode = M_LIST;
				status[0] = '\0';
				continue;
			}
			if (ev.key == KT_K_ENTER) {
				if (nsrv) {
					snprintf(field[F_SERVER],
						 sizeof(field[F_SERVER]), "%s",
						 srv[stbl.sel].name);
					form_open();
				}
				continue;
			}
			if (ev.key == 'b' || ev.key == 'B') {
				send_browse();
				continue;
			}
			if (ktui_keys(&keys, &ev) == KTUI_KEY_CLOSE)
				break;
			ktui_table_key(&stbl, nsrv, ktui_h - 5, ev.key, NULL,
				       NULL);
			continue;
		}

		if (ktui_keys(&keys, &ev) == KTUI_KEY_CLOSE)
			break;

		if (ev.key == KT_K_ENTER) {
			form_open();
			continue;
		}
		if (ev.key == 'b' || ev.key == 'B') {
			send_browse();
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
