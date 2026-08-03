/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   KDOS Installer
 * ---------------------------------
 */

#ifndef KINSTALL_H
#define KINSTALL_H

#include <poll.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#define KI_VERSION "4.0"

/* ────────────────────────────────────────────────────────────────────────
 * Geometry
 * ──────────────────────────────────────────────────────────────────────── */

typedef struct {
	int x, y, w, h;
} Rect;

static inline Rect rect(int x, int y, int w, int h)
{
	Rect r = { x, y, w, h };
	return r;
}

static inline int rect_hit(Rect r, int x, int y)
{
	return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

/* ────────────────────────────────────────────────────────────────────────
 * Colour
 *
 * Eight slots and no more. That is not minimalism for its own sake: the
 * console font is 512 glyphs, so the VT steals the foreground intensity bit
 * for the 9th glyph bit and colours 8-15 become unreachable as foreground.
 * Designing to eight means the TTY and a truecolor foot window render the
 * SAME picture, one with exact hex and one with the palette we install.
 * ──────────────────────────────────────────────────────────────────────── */

enum {
	CL_BG = 0,	/* backdrop                        */
	CL_ERR,		/* urgent                          */
	CL_ACCENT,	/* primary                         */
	CL_WARN,	/* secondary                       */
	CL_DIM,		/* inactive borders, disabled text */
	CL_MID,		/* secondary text, bar fill        */
	CL_SURFACE,	/* panel background                */
	CL_TEXT,	/* body text                       */
	CL_N
};

typedef struct {
	uint8_t r, g, b;
} Rgb;

typedef struct {
	const char *name;
	const char *label;
	Rgb slot[CL_N];
} Theme;

extern const Theme ki_themes[];
extern int ki_ntheme;
extern const Theme *ki_theme;

int theme_set(const char *name);

/* ────────────────────────────────────────────────────────────────────────
 * Terminal
 * ──────────────────────────────────────────────────────────────────────── */

enum {
	CAP_TRUECOLOR = 1 << 0,	/* 24-bit SGR                                */
	CAP_256 = 1 << 1,	/* indexed 256                               */
	CAP_LINUXVT = 1 << 2,	/* real VT: PIO_CMAP palette, no bold, no    */
				/* xterm mouse -> evdev backend instead      */
	CAP_UTF8 = 1 << 3,
	CAP_MOUSE = 1 << 4
};

extern int term_caps;
extern int term_w, term_h;
extern volatile sig_atomic_t term_resized;

int term_init(int want_mouse);
void term_shutdown(void);
void term_suspend(void);	/* drop back to the cooked terminal        */
void term_resume(void);		/* and take it over again                  */
void term_size_refresh(void);
void term_write(const char *s, size_t n);
void term_flush(void);
void term_printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void term_repalette(void);	/* after a live accent switch              */

/* ────────────────────────────────────────────────────────────────────────
 * Cell buffer
 * ──────────────────────────────────────────────────────────────────────── */

enum {
	A_NONE = 0,
	A_BOLD = 1 << 0,	/* suppressed on a VT: the bit is the font page */
	A_REVERSE = 1 << 1,
	A_UNDERLINE = 1 << 2
};

typedef struct {
	uint32_t ch;
	uint8_t fg, bg, attr;
} Cell;

/* Glyphs, resolved once against what the console font actually carries. */
enum {
	G_HL, G_VL, G_TL, G_TR, G_BL, G_BR,		/* single box    */
	G_TEE_L, G_TEE_R, G_TEE_T, G_TEE_B, G_CROSS,
	G_DHL, G_DVL, G_DTL, G_DTR, G_DBL, G_DBR,	/* double box    */
	G_FULL, G_SHADE, G_DOT, G_BULLET, G_SQUARE,
	G_UP, G_DOWN, G_LEFT, G_RIGHT, G_ELLIPSIS, G_DEG,
	G_N
};

extern const char *ki_glyph[G_N];

int draw_init(void);
void draw_resize(void);
void draw_clear(void);
void draw_flush(void);
void draw_invalidate(void);	/* force a full repaint next flush         */

void draw_cell(int x, int y, uint32_t ch, int fg, int bg, int attr);
void draw_fill(Rect r, int bg);
int draw_text(int x, int y, int maxw, const char *s, int fg, int bg, int attr);
int draw_textf(int x, int y, int maxw, int fg, int bg, int attr,
	       const char *fmt, ...) __attribute__((format(printf, 7, 8)));
void draw_hline(int x, int y, int w, int g, int fg, int bg);
void draw_vline(int x, int y, int h, int g, int fg, int bg);
void draw_box(Rect r, const char *title, int fg, int bg, int dbl);
void draw_shadow(Rect r);
void draw_cursor(int x, int y);	/* pointer overlay, evdev backend          */
void draw_hide_cursor(void);
void draw_clip(Rect r);		/* confine drawing to a pane               */
void draw_clip_none(void);
extern int draw_maxy;		/* lowest row any draw ASKED for this pass */

int utf8_width(const char *s);		/* display cells, ignores overlong */
const char *utf8_next(const char *s, uint32_t *cp);

/* ────────────────────────────────────────────────────────────────────────
 * Events
 * ──────────────────────────────────────────────────────────────────────── */

enum {
	EVT_NONE = 0,
	EVT_KEY,
	EVT_MOUSE,
	EVT_RESIZE,
	EVT_TICK
};

enum {
	K_ESC = 27,
	K_ENTER = 13,
	K_TAB = 9,
	K_BACKSPACE = 127,
	K_SPECIAL = 0x110000,
	K_UP, K_DOWN, K_LEFT, K_RIGHT,
	K_HOME, K_END, K_PGUP, K_PGDN, K_INS, K_DEL,
	K_BTAB,
	K_F1, K_F2, K_F3, K_F4, K_F5, K_F6,
	K_F7, K_F8, K_F9, K_F10, K_F11, K_F12
};

enum { MOD_SHIFT = 1, MOD_ALT = 2, MOD_CTRL = 4 };

enum {
	MB_LEFT = 0, MB_MIDDLE, MB_RIGHT,
	MB_WHEEL_UP, MB_WHEEL_DOWN,
	MB_MOVE
};

enum { MP_RELEASE = 0, MP_PRESS = 1, MP_DRAG = 2 };

typedef struct {
	int type;
	int key;		/* codepoint or K_*                        */
	int mods;
	int mx, my;
	int btn;
	int press;
} Event;

int input_init(int want_mouse);
void input_shutdown(void);
void input_suspend(void);
void input_resume(void);
int input_next(Event *ev, int timeout_ms);
int input_mouse_visible(int *x, int *y);

/* ────────────────────────────────────────────────────────────────────────
 * Immediate-mode UI
 *
 * Controls register a hit rect as they draw; the click that arrives on the
 * NEXT frame is matched against that list. Keeps every control a single
 * call with no retained tree to keep in sync with a resize.
 * ──────────────────────────────────────────────────────────────────────── */

typedef struct {
	int focus;		/* id of the focused control               */
	int nfocus;		/* focusables seen this frame              */
	int maxid;
	int clicked;		/* control id clicked this frame, -1 none  */
	int dblclick;
	int wheel;		/* -1 up, +1 down, 0 none                  */
	int wheel_id;
	Event ev;		/* event being dispatched this frame       */
	int consumed;
	int mx, my;		/* pointer position for hover              */
	int hover;
	Rect focus_rect;	/* where the focused control landed        */
	int focus_seen;
} Ui;

extern Ui ui;

/* Hit ids for mouse-only chrome. Far above anything ui_id() hands out, so
 * clicking them can be recognised without them joining the Tab ring. */
#define UI_ID_SIDEBAR 10000

void ui_frame_begin(Event *ev);
void ui_frame_end(void);
int ui_id(void);		/* claim the next focus id                 */
void ui_hit(Rect r, int id);
int ui_focused(int id);
int ui_activated(int id, Rect r);	/* Enter on focus, or a click      */
int ui_key(int k);		/* consume a key press this frame          */
void ui_focus_next(int dir);
void ui_focus_set(int id);

typedef struct {
	int sel;
	int off;
} ListState;

/* Row painter gets an already-cleared line; `sel` marks the current row and
 * `focus` whether the list itself owns the keyboard. */
typedef void (*ListRow)(int idx, int x, int y, int w, int sel, int focus,
			void *user);

int ui_list(Rect r, ListState *st, int count, ListRow row, void *user, int id);

int ui_button(Rect r, const char *label, int enabled, int primary);
int ui_check(int x, int y, int w, const char *label, int *val);
int ui_radio(int x, int y, int w, const char *label, int *val, int on);
int ui_input(Rect r, char *buf, size_t cap, int secret, const char *placeholder);
void ui_progress(Rect r, double frac, const char *label);
void ui_scrollbar(Rect r, int total, int shown, int off);

/* ────────────────────────────────────────────────────────────────────────
 * Probe
 * ──────────────────────────────────────────────────────────────────────── */

#define MAX_DISKS 32
#define MAX_PARTS 32

typedef struct {
	char name[32];		/* sda1                                    */
	char path[64];		/* /dev/sda1                               */
	unsigned long long start, sectors;
	char fstype[16];
	char label[40];
	char uuid[40];
	int is_esp;
	int mounted;
	char mountpoint[64];
} Part;

typedef struct {
	char name[32];
	char path[64];
	char model[48];
	char tran[16];		/* sata, nvme, usb, virtio                 */
	unsigned long long sectors;
	int sector_size;
	int rotational;
	int removable;
	int readonly;
	int is_boot_media;	/* the stick we are running from           */
	char table[8];		/* gpt, dos, -                             */
	Part part[MAX_PARTS];
	int nparts;
} Disk;

typedef struct {
	int uefi;
	int secure_boot;
	unsigned long long mem_kb;
	char cpu[64];
	int cores;
	int live;		/* booted from the squashfs overlay        */
	unsigned long long payload_kb;	/* what an install will copy       */
	unsigned long long appbox_kb;	/* of which, the alien app store   */
} SysInfo;

extern Disk ki_disk[MAX_DISKS];
extern int ki_ndisk;
extern SysInfo ki_sys;

void probe_system(void);
void probe_disks(void);
void probe_part(const char *path, Part *p);
const char *human_size(unsigned long long bytes);
Disk *disk_by_path(const char *path);

/* ────────────────────────────────────────────────────────────────────────
 * Configuration — everything the wizard collects, nothing written until
 * the install page runs.
 * ──────────────────────────────────────────────────────────────────────── */

enum { PLAN_WIPE = 0, PLAN_REUSE, PLAN_MANUAL };
enum { SWAP_NONE = 0, SWAP_FILE, SWAP_PART };

typedef struct {
	char keymap[64];
	char tz[80];
	char tz_label[64];

	char disk[64];
	int plan;
	char part_esp[64];
	char part_root[64];
	int format_esp;
	char fstype[16];
	int swap;
	long swap_mb;

	char hostname[64];
	char username[33];
	char fullname[64];
	char userpass[128];
	char userpass2[128];
	int user_wheel;
	char rootpass[128];
	char rootpass2[128];
	int root_locked;

	char theme[16];
	int with_appbox;
	unsigned svc_off;	/* bitmask over ki_services                */

	int reboot_after;
	int dry_run;
} Config;

extern Config cfg;

typedef struct {
	const char *name;	/* matches /etc/init.d/NN_<name>.sh         */
	const char *label;
	const char *note;
	int def_on;
} Service;

extern const Service ki_services[];
extern int ki_nservices;

void conf_defaults(void);
int conf_load(const char *path);
int conf_save(const char *path);

/* ────────────────────────────────────────────────────────────────────────
 * Install engine
 *
 * The work runs in a forked child that speaks a one-line protocol back over
 * a pipe, so the sequential shape of an install stays sequential code and
 * the UI stays a single-threaded poll loop.
 * ──────────────────────────────────────────────────────────────────────── */

#define MAX_STEPS 16
#define LOG_LINES 4096
#define LOG_COLS 200

enum { ST_PENDING = 0, ST_RUNNING, ST_DONE, ST_FAIL, ST_SKIP };

typedef struct {
	const char *title;
	const char *detail;
	int state;
	double t0, t1;
	double frac;		/* -1 when indeterminate                   */
	char note[96];
} StepUi;

typedef struct {
	StepUi step[MAX_STEPS];
	int nsteps;
	int cur;
	int running;
	int failed;
	int done;
	char failmsg[256];
	pid_t pid;
	int fd;
	double t0;
	char (*log)[LOG_COLS];
	int nlog, logtop;
	int logfd;
} Install;

extern Install inst;

void install_plan(void);	/* fill the step table from cfg            */
void install_start(int from_step);
void install_pump(void);
void install_abort(void);
int install_child_main(int wfd, int from_step);	/* runs in the child       */
void install_log(const char *line);

/* ────────────────────────────────────────────────────────────────────────
 * Pages
 * ──────────────────────────────────────────────────────────────────────── */

typedef struct Page {
	const char *id;
	const char *title;
	const char *subtitle;
	void (*enter)(void);
	void (*draw)(Rect body);
	/* Return 0 to allow Next, or fill err and return non-zero. */
	int (*validate)(char *err, size_t n);
	int (*event)(Event *ev);
	int hide_nav;
} Page;

extern Page *ki_pages[];
extern int ki_npages;
extern int ki_page;

void page_goto(int n);
void nav_next(void);
void nav_back(void);

/* modal helpers, driven from the main loop */
void modal_alert(const char *title, const char *msg);
void modal_confirm(const char *title, const char *msg,
		   const char *yes, const char *no, void (*on_yes)(void));
int modal_active(void);
void modal_draw(void);
int modal_event(Event *ev);

extern int ki_quit;
extern int ki_help;

/* ────────────────────────────────────────────────────────────────────────
 * Small shared helpers
 * ──────────────────────────────────────────────────────────────────────── */

double now_s(void);
char *xstrdup(const char *s);
void *xcalloc(size_t n, size_t sz);
int read_file(const char *path, char *buf, size_t cap);
int read_line_file(const char *path, char *buf, size_t cap);
int write_file(const char *path, const char *data);
int path_exists(const char *p);
int have_prog(const char *name);
void strlcpy_(char *d, const char *s, size_t n);
int str_ieq(const char *a, const char *b);
const char *basename_(const char *p);

#endif /* KINSTALL_H */
