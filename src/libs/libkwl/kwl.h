/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   libkwl — libktui's Wayland backend
 *
 * The same cell grid libktui already draws, painted into a wl_shm buffer with
 * fcft instead of written to a terminal as escapes. Every widget, every glyph
 * tier, every layout in libktui works here unchanged, because none of them ever
 * knew where the cells went.
 *
 * WHY THIS IS A SEPARATE ARCHIVE. libktui links nothing but musl and must keep
 * doing so: kinstall links it in PHASE 1, before any library exists to link
 * against. This one links wayland-client, fcft, pixman and xkbcommon. Keeping
 * them apart is what stops kinstall being dragged to phase 4 — so if you are
 * about to add a `-l` to libktui to save a file here, that is the trade you are
 * actually making.
 *
 * Surface roles, because a shell needs both and neither is xdg-shell: a panel
 * is a layer-shell surface with an exclusive zone, and a lock screen is an
 * ext-session-lock-v1 surface. A lock that used xdg-shell would be a window the
 * compositor could be persuaded to close.
 * ---------------------------------
 */

#ifndef KWL_H
#define KWL_H

#include <stdbool.h>
#include <stdint.h>

#include "ktui.h"
#include "kdisp.h"

/*
 * pixman_image_t without dragging <pixman.h> onto every consumer's include
 * path. It is a UNION, not a struct — declaring the wrong tag is a hard error
 * — and C11 allows a typedef to be repeated with the same type, so this and
 * pixman's own definition coexist in either order.
 */
union pixman_image;

/*
 * Connect, create the surface, and install this as libktui's backend. After
 * this returns 0, every ktui_* drawing call paints here.
 *
 * Returns -1 with nothing installed if there is no compositor, no font, or no
 * layer-shell when one was asked for — a caller that cannot draw should say so
 * and exit, not run blind.
 */
int kwl_init(const KDispConfig *cfg);

/* libkwl as a libkdisp implementation. A consumer hands the ADDRESS of this
 * to kdisp_init(); naming it is what links Wayland into that program, which
 * is why libkdisp itself never names it. */
extern const KDispImpl kwl_impl;
void kwl_shutdown(void);
/*
 * Overlay only: ask for a new size in cells. A toast stack is the case this
 * exists for — its surface is opaque for its whole height, so a daemon sized
 * once for the MAXIMUM number of toasts paints an empty dark rectangle on the
 * desktop for the whole session. The compositor answers with a configure and
 * the cell buffer follows; the caller redraws on the next frame.
 */
/*
 * Ask for a new size and WAIT for it. 0 when the surface really is that size,
 * -1 when the compositor said otherwise or said nothing — a caller that gets
 * -1 must draw at the size it already had, because a buffer that disagrees
 * with the last configure is a protocol error and those disconnect the client.
 */
int kwl_overlay_resize(int cols, int rows);

/*
 * Panel only: collapse to a one-cell strip with no exclusive zone, and back.
 *
 * Autohide is a PANEL property rather than a drawing one because the half that
 * matters is the exclusive zone — a panel that merely painted itself away
 * would still hold its strip of the screen against every maximised window.
 * Hidden the surface stays where it is, one cell thick, above the windows and
 * still answering the pointer, which is what makes hovering the edge able to
 * bring it back.
 *
 * The call blocks for one protocol roundtrip: the configure answering the
 * resize must be in hand before anything is painted, or the buffer committed
 * against it is the wrong size. It sets `ktui_resized`, so the caller's normal
 * resize path redraws.
 */
void kwl_layer_autohide(bool hidden);

/*
 * Overlay only: destroy the layer surface while idle and recreate it on
 * demand. The alternative — a NULL-buffer commit — resets the surface to
 * uninitialised in wlroots and the next toast is drawn nowhere; a one-cell
 * surface is the workaround this pair replaces. show() with a live surface
 * is just a resize; after a recreate it has completed the initial-commit
 * handshake and the surface is ready to draw on when it returns 0.
 */
void kwl_overlay_hide(void);
int kwl_overlay_show(int cols, int rows);

/*
 * COPY. Puts `text` on the clipboard (primary = 0) or the primary selection
 * (primary = 1), by creating a data source this client answers `send` on.
 *
 * Returns -1 when there is no manager for that selection, when the allocation
 * fails, and — the one worth knowing about — when this surface has never seen
 * an input event: set_selection presents the SERIAL of the event that
 * justified it, and a compositor refuses one it has never issued. That is the
 * protocol saying a background client may not take the clipboard, not a bug.
 *
 * The text is copied; the caller keeps its own. The two selections are
 * separate stores, so copying into one never disturbs the other. A send
 * already in flight keeps the payload it started with, so replacing a
 * selection mid-send cannot splice or truncate what its receiver gets.
 *
 * Sends never block the frame: they are drained from kwl_pump, and the event
 * wait wakes on a receiver becoming writable. A send is abandoned only after
 * KWL_COPY_TIMEOUT_MS with no byte moving at all — a receiver that keeps
 * reading is never cut off, however large the selection.
 */
int kwl_copy(const char *text, size_t len, int primary);

/* Begin a drag carrying `data` as `mime`. The payload is copied. Needs a
 * recent input event: the compositor checks the serial a drag starts from. */
int kwl_drag_start(const char *mime, const char *data, size_t len);

/*
 * A DIFFERENT FONT ON THIS SURFACE, and on every surface in this process:
 * the face and its size are a process-global in libkcell. A program with one
 * window per process — kdos-term is the one — gets a per-window size out of
 * that; a program drawing several surfaces moves all of them at once.
 *
 * `step` moves the size in the name by that many units of whichever key
 * carries it, `:pixelsize=N` or `:size=N`, clamped at both ends. A step of 0
 * puts back the name kwl_init() was given, which is what a reset chord
 * means. A BITMAP FACE ANSWERS WITH THE NEAREST STRIKE IT CARRIES, so a
 * step that lands between two of them reloads a face of the same size and
 * the cell does not move at all — except where libkcell moves the size onto
 * the face's scalable twin (Terminus past its 32 strike: see "The cell's
 * size" in docs/kdos/05-developer/c-libraries.md), where every step moves the
 * cell. kwl_cell_w()/kwl_cell_h() are
 * how a caller tells: everything it cut for the old cell is still right when
 * they have not changed.
 *
 * The grid is derived from the cell and not stored, so a different cell is a
 * different number of columns and rows: this reloads, recuts the grid from the
 * pixels the surface already has, spoils every paint baseline and raises
 * `ktui_resized`. The CALLER calls ktui_draw_resize() and re-states everything
 * else it cut for the old cell — a sprite scaled to it is still scaled to it,
 * and nothing here knows what those are.
 *
 * 0 when the font in force is the one asked for. -1 with NOTHING MOVED when
 * the name does not fit or will not load: the old font is put back, because a
 * window with no glyphs at all is not a state a person can type their way out
 * of. -1 with the restore having failed too leaves nothing drawable, and a
 * caller that sees it should say so and exit.
 */
int kwl_font_step(int step);

/* Cell metrics, once the font is loaded, in the pixels the grid is drawn in:
 * logical on the integer path, device pixels on a fractional output scale,
 * where the font is loaded at the device size. A position handed to another
 * surface goes through kwl_px_logical(). */
int kwl_cell_w(void);
int kwl_cell_h(void);
/*
 * The offset a popup of THIS panel passes as its own margin from the same
 * screen edge: the surface's own height in logical pixels — the cell grid
 * plus the rule — plus the panel's gap.
 *
 * THE GAP IS PART OF IT. Height and offset are the same number only while the
 * bar is flush with the edge; with `margin_y` set they differ by exactly the
 * gap, and a popup placed with the bare height opens behind the bar it
 * belongs to.
 */
int kwl_popup_offset(void);
/* Which side of this surface faces away from the bar — see kwl.c. */
int kwl_edge_bottom(void);

/*
 * THE BACKDROP — pixel chrome painted UNDER the cell grid, every frame.
 *
 * Called with the surface's own pixman image just before the cells go down,
 * so a caller can lay a body, a plate or a one-pixel rule where a 10x20 cell
 * of one colour cannot. `w`/`h` are the buffer in real pixels and `scale` is
 * the whole-number scale the grid is painted at: everything the callback
 * draws is in the pixels kwl_cell_w() counts and must be multiplied by it. On
 * a fractional output scale those pixels are the device's and `scale` is 1.
 *
 * Two consequences the caller does not get a choice about:
 *
 *  - Alone, it forces a FULL cell repaint and full damage on every commit.
 *    The row diff cannot know which cells the backdrop disturbed, and half a
 *    repaint over a fresh backdrop is a bar with last frame's text on it.
 *    kwl_set_backdrop_cache() below is what lifts that.
 *  - Any slot the caller cleared to alpha 0 is then LEFT ALONE by the cell
 *    painter rather than cleared, because the clear would erase what this
 *    just drew. Clear the slot the backdrop owns and no other.
 *
 * NULL removes it. Setting one drops any cache installed for the last.
 */
void kwl_set_backdrop(KDispBackdropFn fn);
/*
 * A BACKDROP THAT CAN BE REPAINTED IN PART, installed after kwl_set_backdrop.
 *
 * `key` names the whole picture the backdrop would paint now — everything it
 * depends on except the buffer's size and scale, which libkwl compares
 * itself — and is asked once per commit, before the paint. `band` paints the
 * rectangle (x, y, bw, bh), in buffer pixels, of exactly that picture into
 * `dst` and touches nothing outside it; it returns -1 when it cannot, and the
 * commit is then painted in full. `diff`, which may be NULL, sets `out` (an
 * initialised region) to every pixel of a w x h picture at `scale` where the
 * picture a previous `key` answer named and the current one can differ, and
 * returns -1 when it cannot say. `same`, which may be NULL, answers 1 when
 * the current picture's pixel rows [y, y + n) equal its rows [from, from + n)
 * across the whole width, and 0 when they differ or it cannot say.
 *
 * With `key` and `band`, a buffer whose last backdrop had the current key
 * keeps it: only the cells that differ from that buffer's own last paint are
 * repainted, each laid back on its band of the picture first, because the
 * painter leaves a backdrop-owned background alone and a glyph drawn on the
 * last glyph is both. With `diff` as well, a buffer wearing an OLDER picture
 * does the same, with every cell the moved pixels touch added to the ones it
 * repaints — a hover plate that moved costs the rows it left and the rows it
 * reached. A moved pixel outside the cell grid (the rule, the remainder past
 * the last cell) has no cell to repaint it, so that commit is painted in
 * full. With `same` as well, a grid that scrolled moves its pixels instead of
 * repainting them (kwl.c, scroll_blit), but only
 * the rows whose picture is the same at both ends of the move: a body with a
 * vertical gradient is different on every row, and a row moved across it
 * would carry the wrong shade with it.
 *
 * The damage is the changed cells plus, when the picture differs from the
 * one on the screen, what `diff` says moved between the two; without an
 * answer it is the whole surface. A key or a diff that misses an input leaves
 * stale pixels, so a caller derives both from the same state it draws from.
 * `KDOS_PAINT_FULL=1` in the environment paints and damages every commit in
 * full, for comparing the two; so does `KDOS_INSPECT=1`, which draws the rows
 * each commit changed over the surface (kwl_insp.h).
 *
 * NULL for `key` or `band` removes all four.
 */
struct pixman_region32;
typedef int (*KwlBackdropBandFn)(pixman_image_t *dst, int w, int h, int scale,
				 int x, int y, int bw, int bh);
typedef int (*KwlBackdropDiffFn)(uint64_t from, int w, int h, int scale,
				 struct pixman_region32 *out);
typedef int (*KwlBackdropSameFn)(int w, int h, int scale, int y, int from,
				 int n);
void kwl_set_backdrop_cache(uint64_t (*key)(void), KwlBackdropBandFn band,
			    KwlBackdropDiffFn diff, KwlBackdropSameFn same);
/*
 * EVERY PIXEL OF THE NEXT COMMIT IS OPAQUE. The surface then carries an
 * opaque region over its whole size, and the compositor copies it instead of
 * blending it and skips whatever lies underneath. A backdrop surface is ARGB
 * by construction — its body slot is alpha 0 — so this is the only way it is
 * ever treated as opaque.
 *
 * Read when the next commit is made and sent with it, so the claim describes
 * the pixels it travels with: a backdrop's key callback may make it. A claim
 * over a pixel that is not opaque shows whatever the compositor had under it,
 * so claim it only for a picture that covers the surface at alpha 255.
 */
void kwl_set_opaque(bool on);
/*
 * SOMETHING BELOW THE GRID CHANGED ITS PIXELS, asked at FLUSH TIME.
 *
 * The frame diff tracks cells, which is what the grid IS — but a surface with
 * a backdrop draws part of its picture underneath them, and a menu row
 * highlighted by a plate rather than by a cell attribute changes no text at
 * all. Without this the highlight stays where it is until something else
 * causes a frame.
 *
 * ASKED RATHER THAN ANNOUNCED, because a backdrop that describes its picture
 * as it draws cannot tell a change from a redescription: a latch set from
 * each piece makes every frame a commit, which is the "nothing changed: no
 * commit at all" gate gone for every surface that has a backdrop. The
 * callback is asked once per flush, after the picture is complete: it answers
 * non-zero only when what the backdrop would draw now differs from what it
 * last drew. A non-zero answer is a commit; what it repaints is the
 * backdrop's business (kwl_set_backdrop_cache). NULL removes it.
 */
void kwl_set_pixels_dirty_fn(int (*fn)(void));
/*
 * Whether a frame drawn NOW would only be stashed.
 *
 * The backend throttles to the compositor's frame callback: a flush while the
 * last commit is unanswered snapshots the cells and the callback commits the
 * newest snapshot, so every draw between two callbacks is thrown away. A
 * consumer whose draw is expensive — a terminal re-rendering a whole grid for
 * a program that writes as fast as it can be read — asks this first and skips
 * the draw entirely, which is the difference between drawing ten thousand
 * frames a second and drawing the sixty that are shown.
 *
 * Skipping is safe: the throttle opens on the frame callback or on its stall
 * timeout, and both wake the caller's loop. Do NOT gate a draw on
 * kwl_presented() instead — that stays true after a successful commit, and a
 * surface that stopped drawing then would never draw again.
 */
int kwl_frame_throttled(void);
/*
 * A LIST THAT GLIDES. `x`, `y`, `w`, `h` are the cells the list's rows occupy
 * — the rows alone, with no header and no scrollbar — and `top` is the item
 * drawn in its first row. Declared on every draw, beside kch_list_clamp();
 * a list not declared in a frame stops gliding.
 *
 * When `top` changes between two commits, the list's pixels slide from the
 * picture that was on the screen to the new one over KWL_GLIDE_MS (see
 * kwl_glide.h), eased out, with the commits in between driven by this
 * library's own frame callbacks. The cells never move by less than a row: the
 * fraction exists only in the pixels, the pointer hits the rows the cells
 * say, and the last commit is the cells' own picture. Nothing glides where
 * motion is off (comp.conf's `motion`), under KDOS_INSPECT, or when the move
 * is as long as the list — a page is a jump. Up to KWL_GLIDES lists a frame.
 */
void kwl_list_view(int x, int y, int w, int h, int top);
/*
 * 1 when the COMPOSITOR is drawing this window's frame, so the program must
 * not draw a second one round the outside of its own content. False on a
 * terminal, a panel and every popup — see kwl.c.
 */
int kwl_decorated(void);
/* The whole-number scale the grid is painted at. Anything a consumer
 * rasterises for itself has to be produced at kwl_cell_w() * scale — libkicon
 * is the caller. 1 on a fractional output scale, where the cell is the font's
 * at the device size instead. */
int kwl_scale(void);
/* A pixel counted in kwl_cell_w()'s units, as the compositor counts it:
 * floored to the logical pixel it falls in, and the same number on the
 * integer path. See KDispImpl.px_logical. */
int kwl_px_logical(int px);
/* Call `fn` with the new scale whenever the surface's pixel cell changes —
 * the output scale, or the device cell on a fractional one. At most
 * KWL_SCALE_FNS functions; one already registered is not added twice. See
 * KDispImpl.on_scale. */
enum { KWL_SCALE_FNS = 4 };
void kwl_on_scale(KDispScaleFn fn);

/*
 * The pointer's shape over this surface, via cursor-shape-v1. Sticky: it is
 * re-sent on every pointer enter, so a consumer sets it when its hover target
 * changes, not per frame. A compositor without the protocol ignores it, which
 * leaves the arrow — the shape a surface that never asks for one keeps.
 */
void kwl_cursor_set(enum kdisp_cursor c);

/*
 * Which cells of this surface answer the pointer at all.
 *
 * `n` rectangles in CELLS; n == 0 means the surface takes no pointer input and
 * every click falls through to whatever is behind it. Pass n < 0 to go back to
 * the default, which is the whole surface.
 *
 * THE DESKTOP IS WHY THIS EXISTS. kdos-desk covers the entire output, so with
 * the default region it eats every click on the root window — and the
 * compositor's own root-menu mousebind (right-press -> ShowMenu, menu.xml)
 * then never fires for as long as desktop icons are on, which is the shipped
 * default. Claiming only the cells its icons occupy leaves a click on bare
 * wallpaper reaching the compositor exactly as it does with the icons switched
 * off.
 */
void kwl_input_cells(const KRect *rects, int n);
/* Rename this window while it runs — the compositor draws its frame and its
 * taskbar row. See KDispImpl.set_title. */
void kwl_set_title(const char *title);

/*
 * The connection, for a consumer that needs to bind protocols of its own —
 * kdos-shell binds foreign-toplevel and ext-workspace on exactly this
 * display. Deliberately shared rather than opened a second time: two
 * wl_displays would be two clients, with two seats and two sets of globals,
 * which would then have to be kept agreeing with each other about what one
 * panel is showing. NULL before kwl_init() succeeds.
 *
 * It is a `void *` because kwl.h is included by files that do not otherwise
 * pull in wayland-client.h; cast at the use site.
 */
void *kwl_display(void);

/*
 * The connection's fd, and one turn of the protocol, for a consumer that has
 * OTHER fds to wait on — kdos-notifyd waits on the session bus at the same
 * time. A program with only the surface to service should use the backend's
 * poll_event instead; this pair exists so a second event source does not have
 * to be serviced by polling on a timer.
 */
int kwl_fd(void);
void kwl_pump(void);	/* dispatch what has arrived, flush what is queued */

/* Non-zero once the compositor or the user has asked the surface to go away. */
int kwl_should_close(void);

/*
 * Session lock only.
 *
 * `kwl_lock_engaged()` is non-zero once the COMPOSITOR has confirmed the
 * session is locked. A lock screen must not accept a password before that:
 * until `locked` arrives the screen may still be showing the session, and the
 * user would be typing into a machine that is not yet secured.
 *
 * `kwl_lock_finished()` is the compositor refusing the lock — it is already
 * locked by someone else, or the request came too late. A client that sees it
 * must exit WITHOUT unlocking anything.
 *
 * `kwl_unlock()` sends unlock_and_destroy and is the only way out. It must be
 * called before the process exits, or the compositor keeps the screen locked
 * exactly as if the client had crashed.
 */
int kwl_lock_engaged(void);
int kwl_lock_finished(void);
void kwl_unlock(void);

#endif /* KWL_H */
