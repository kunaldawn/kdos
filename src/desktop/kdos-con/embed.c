/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-con — a graphical application as WINDOWS
 *
 * A Wayland client's surface is pixels and this desktop composites characters.
 * The compositing that reconciles the two happens in a SEPARATE PROCESS —
 * kdos-cage --embed, one per APPLICATION — and this file is the parent half of
 * the private channel to it. kdos-con still links no wlroots, no mesa and no
 * pixel library at all: it moves a blob of bytes it never looks at.
 *
 * ONE CHANNEL, N WINDOWS. A guest is as many toplevels as it maps and each one
 * is a window on this desktop: its own rectangle, its own title, its own place
 * in the stack, its own mapping and its own blocks. They cannot be one window
 * — five toplevels composited into one framebuffer are five pictures nothing
 * downstream can separate — and they cannot be one cage each, because the
 * toplevels belong to ONE client on ONE display and a mapped surface cannot be
 * moved to another compositor. So `struct EmbedProc` is the process and
 * `struct EmbedWin` is the window, every op carries which window it is about,
 * and the only thing a fork still means is a new application.
 *
 * THE FRAME BECOMES SPRITES, not a rectangle of pixels drawn over the grid.
 * A sprite lives IN A CELL, so a window on top of an embedded one simply
 * overwrites those cells and the occlusion is the z-ordered copy that was
 * already there. A pixel region painted alongside the grid would cover
 * whatever was above it, and every window-model question — stacking, snapping,
 * workspaces — would need a second answer for one kind of window.
 *
 * A PICTURE IS SIXTEEN CELLS SQUARE AT MOST, because that is what the cell's
 * sprite encoding carries, so a window is a grid of blocks and damage is
 * rounded out to the blocks it touches. It is smaller than that wherever a
 * block of sixteen would not fit one message — see EM_TILE.
 *
 * THE DESCRIPTOR IS THE ONE PLACE ONE APPEARS. It is a socketpair created
 * before the fork and inherited — never a path anything can connect to — which
 * is what keeps the surface and view protocols descriptor-free and therefore
 * forwardable over ssh.
 * ---------------------------------
 */

#include <errno.h>
#include <fcntl.h>
#include <linux/input-event-codes.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "con.h"
#include "kbase.h"
#include "kembed.h"

/*
 * THE LARGEST BLOCK, AND THE REAL ONE IS CHOSEN PER WINDOW.
 *
 * Sixteen cells square is what the cell encoding carries — four bits per axis
 * — so a window can never be cut coarser than this. It is cut finer whenever
 * a block of this size would not fit one message: a block is
 * `tile * tile * cell_w * cell_h * 4` bytes against KCON_MAX_PAYLOAD, and the
 * side that chooses the pixel size is the side that has to keep it under the
 * cap. A block that exceeds it is refused by the encoder exactly as a full
 * queue is, so it stays owed for the life of the window and is re-cut and
 * re-refused on every turn — a permanent hole in that window, which the
 * desktop and every other window carry on around. `layout()` picks the
 * per-window `tile`; a smaller one is more blocks and no protocol change.
 */
#define EM_TILE 16
/*
 * ENOUGH BLOCKS FOR A GUEST THE SIZE OF THE WORK AREA ON THE BIGGEST SCREEN
 * THIS DRIVES. A 4K screen at an 8x15 cell is 480x144 cells, which is 30x9
 * blocks; the cell that forces the smallest tile is also the largest cell, so
 * a 64x64 cell cuts 4K into 60x33 cells and a tile of seven, which is 9x5.
 * The ceiling is what `layout()` refuses above, and a refusal there is a
 * resize the guest is never told about — a maximised window still drawing at
 * the size it had. The cost of the headroom is this array and nothing else.
 *
 * THE SLOTS THEMSELVES ARE THE SESSION'S AND ARE SHARED BY EVERY WINDOW AND
 * EVERY SURFACE. KCON_MAX_SPRITE_MAP is 4096 for the whole desktop; a window
 * claims one slot per block, so a maximised window on a 240x67 grid takes 75
 * and a half-screen one takes 24 — about fifty maximised windows, or two
 * hundred dialogs, before the rotation is empty. `layout()` takes what it can
 * get: a block that could not be given a slot is drawn as the fallback shade
 * and is never owed to a display, so the shortfall is a shaded patch in the
 * newest window rather than a hole onto the window underneath or, worse, two
 * windows drawing through one slot.
 */
#define EM_MAX_BLOCKS 1024

/*
 * HOW MANY WINDOWS ONE GUEST MAY PUT ON THE DESKTOP.
 *
 * A bound is needed because the slot rotation above is the session's: an
 * application that maps toplevels without bound would empty it and take the
 * pictures away from every other window. Sixteen is past what any application
 * a person uses opens at once — an editor with four docks, its image window
 * and a modal chooser is six — and a toplevel past it is dropped with a line
 * in the log rather than silently drawn over another window's pixels.
 */
#define EM_MAX_WINS 16

/*
 * A CELL SIZE FOR A SESSION THAT HAS NOT BEEN TOLD ONE. A view says how many
 * pixels its cells are; a terminal view has no answer, and a guest still has
 * to be given a size. Eight by sixteen is the console font's, so an embedded
 * application rendered for a terminal view has the aspect ratio the characters
 * that will represent it do.
 */
#define EM_CELL_W 8
#define EM_CELL_H 16

/*
 * ENOUGH BITS FOR EVERY KEY AND EVERY BUTTON A DEVICE CAN REPORT. Keys and
 * pointer buttons share one evdev number space, so the set that records what a
 * window is owed a release for is sized from the whole of it — a set sized for
 * the letters alone would index outside itself the first time a mouse with a
 * side button was used on a guest.
 */
#define EM_KEYSET ((KCON_KEYCODE_MAX + 32) / 32)

/*
 * HOW OFTEN A FRAME MAY BE SENT WHEN NOTHING CAN SHOW PIXELS. Every attached
 * view still receives the picture — a terminal view matches it to characters —
 * but a window of pixels at a compositor's frame rate down an ssh link is a
 * link that does nothing else.
 */
#define EM_SLOW_MS 250

/*
 * HOW OFTEN A GUEST MAY BE TOLD A NEW SIZE, AND HOW LONG ONE MAY GO
 * UNANSWERED BEFORE THE NEXT IS SENT.
 *
 * A KEMBED_SIZE is an output mode change on the far side: a swapchain, a fresh
 * memfd and a fresh mapping in both processes, and a full relayout in the
 * application — far heavier than the configure a toolkit answers on any other
 * compositor. A drag produces one per pointer motion, so a channel with no
 * gate buries the guest in reconfigures it answers seconds late and the window
 * trails the pointer for the whole gesture. The first number tracks the
 * session's own frame period — CON_FRAME_MS in main.c, which the compiler
 * cannot tie to this one — because a size sent more often than a frame is
 * composed cannot be seen; the two move together or the gate is a rate nothing
 * displays. The second bounds the wait for an answer: a guest whose mapping
 * never reaches the size it was told would otherwise hold the channel shut for
 * the life of the window.
 */
#define EM_SIZE_MS 16
#define EM_SIZE_LATE_MS 1000

/*
 * HOW LONG A GUEST HAS TO CLOSE ITSELF, and how long it has after being told
 * with a signal.
 *
 * Closing a window ASKS the application, which is what gives it the chance to
 * offer a save dialog — and a person answering that dialog is using the
 * window, not ignoring the close. Ten seconds is long enough for a container's
 * toolkit to draw one and short enough that a guest which is never going to
 * answer does not leave a window nothing can close.
 *
 * THE FIRST IS TWO DEADLINES ON THE SAME CLOCK, AND NEITHER RUNS AGAINST A
 * GUEST THAT ANSWERED. A WINDOW that has been asked and is still here is
 * dropped from the desktop, because the toplevel behind it answers nothing and
 * there is nobody left to ask; the PROCESS is signalled only when the ask
 * covered every window a person could reach, because a signal takes every
 * other window's unsaved work with it. An answer — the window retired, or a
 * question opened on it — stops both, since the failure they are measuring is
 * silence and neither one is silent.
 *
 * THE SECOND MUST EXCEED THE CAGE'S OWN ESCALATION, which is five: the signal
 * reaches the cage, and the cage then asks, signals and kills its guest before
 * exiting. Killing the cage inside that window leaves the application running
 * with nothing to draw on and nobody to reap it.
 */
#define EM_CLOSE_MS 10000
#define EM_KILL_MS 8000

/*
 * WHERE A TURN STOPS FILLING A DISPLAY, AND IT IS BELOW WHERE THE DISPLAY
 * WOULD REFUSE.
 *
 * KCON_VIEW_HIGH is the mark `kcon_view_sprite()` declines a block at, and it
 * is also the mark `kcon_view_ready()` stops calling a display ready at — and
 * a session whose displays are none of them ready composes no frame of its
 * own. Filling a queue right up to the refusal would therefore buy a guest's
 * blocks with the panel, the clock, the pointer and every other window, which
 * is a desktop that stops while one application draws. Half leaves room for a
 * composed frame — a 240x67 grid is under 130 kB — and for the block in
 * flight.
 */
#define EM_VIEW_SOFT (KCON_VIEW_HIGH / 2u)

/*
 * ONE GUEST PROCESS AND THE CHANNEL TO IT.
 *
 * Everything here is the PROCESS's and is shared by every window the guest
 * maps: one socket, one keyboard seat, one launch identity, one exit status.
 * A field that is about a rectangle of pixels belongs in EmbedWin below, and
 * the split is not cosmetic — a title, a size or a set of held keys kept here
 * would be a modal file dialog renaming the image window it opened over.
 */
struct EmbedProc {
	pid_t pid;
	int fd;
	/*
	 * THE GUEST'S STDERR, AND THE LAST LINE OF IT.
	 *
	 * A window is on screen from the moment the cage is forked, long before
	 * anything is known about whether the program behind it can start. When
	 * it cannot — a pack that will not mount, a box that will not compose,
	 * a binary that is not there — the cage's client exits, the cage exits
	 * with it and the window goes: a window that opened and closed itself,
	 * with the sentence explaining it in a log nobody is looking at. Every
	 * line still reaches the session's log; the last one is kept here so
	 * the person watching the window is told what happened to it.
	 */
	int errfd;
	char last[192];

	/*
	 * WHEN THE GUEST WAS ASKED TO GO AND HAS NOTHING LEFT TO ANSWER FOR, 0
	 * for never. The deadline that escalates to a signal is measured from
	 * it, and a second ask does not restart it: a window that is never
	 * going to answer would otherwise be kept alive by the clicking.
	 *
	 * IT IS THE PROCESS'S AND ONLY THE LAST WINDOW A PERSON CAN REACH ARMS
	 * IT. A guest signalled because one dialog of five ignored a close is
	 * every other window's unsaved work gone.
	 *
	 * AND IT IS DISARMED BY THE GUEST ANSWERING — retiring the window, or
	 * putting a question on it. It measures a guest that was asked and did
	 * nothing, and a signal delivered to one that is asking the person
	 * about the very same window loses exactly the work the ask exists to
	 * save.
	 */
	unsigned long long closing_ms;

	/*
	 * THE LAST SIGNAL SENT ON THAT CLOCK, so each one is sent once. The
	 * reap runs on every pump, and a kill() repeated at that rate is
	 * thousands of signals inside one close — a guest whose handler is
	 * re-entered that fast never reaches the end of its own shutdown. It
	 * goes back to 0 with the clock, because the next ask is a new
	 * escalation.
	 */
	int sigsent;

	int gone;			/* the guest exited */
	int status;			/* and what waitpid said about it */
	/*
	 * WHETHER THIS GUEST EVER MAPPED A TOPLEVEL. A cage publishes nothing
	 * until its client has a window, so until this is set there is one
	 * placeholder on the desktop saying what it is doing — and a guest that
	 * exits without it either failed to start or handed its document to an
	 * instance that is already running, which is the one difference the
	 * exit status can tell.
	 */
	int opened;

	/*
	 * THE LAUNCH IDENTITY EVERY WINDOW OF THIS GUEST INHERITS. It comes
	 * from the argument vector, which only the launch has: a second window
	 * is announced by a cage that carries no argv, so a window deriving its
	 * own would be a taskbar chip with no icon and a run-or-raise that
	 * matches nothing.
	 */
	char prog[64];
	char app_id[64];
	char title[128];

	/* THE WINDOWS OF THIS GUEST, newest first. `nwins` bounds what one
	 * application may take out of the session's slot rotation. */
	struct EmbedWin *wins;
	int nwins;

	/* WHICH LAYOUT THIS GUEST HOLDS, against the session's own count, so a
	 * keymap that changed reaches every live guest exactly once. ONE
	 * KEYBOARD PER CAGE: the seat is the process's, so the layout and the
	 * mask below are sent once for all of its windows. */
	unsigned km_gen;
	/*
	 * AND WHICH SELECTION IT HOLDS, against clip_gen(). ONE CLIPBOARD PER
	 * CAGE for the same reason: the seat is the process's, so every window
	 * of one guest pastes the same bytes and the mirror is per channel.
	 *
	 * THE SELECTION IS MIRRORED AND NOT FETCHED. A guest pastes out of the
	 * copy its own compositor is already holding, so a Ctrl+V costs no
	 * round trip to this process — a paste that had to wait for one is a
	 * paste that can hang the window doing it.
	 */
	unsigned long clip_gen;
	/*
	 * THE MODIFIER MASK THIS GUEST WAS LAST TOLD — depressed, latched,
	 * locked, group.
	 *
	 * KEMBED_MODS IS AN ASSERTION ABOUT A KEYBOARD and the guest acts on
	 * it, so it is sent only where it says something the key stream could
	 * not: a lock or a layout group the guest has no message for.
	 */
	unsigned mods_sent[4];
};

/*
 * ONE TOPLEVEL OF THAT GUEST, WHICH IS ONE WINDOW ON THIS DESKTOP.
 *
 * Every field is about this window's pixels or this window's place in the
 * window model, and every message about it carries `id` so the other end can
 * tell it from its siblings.
 */
struct EmbedWin {
	struct EmbedProc *proc;
	struct EmbedWin *next;		/* the guest's other windows */
	Win *win;

	/*
	 * ITS NAME ON THE WIRE.
	 *
	 * `id` is allocated by the CAGE, never reused, and 0 until the guest
	 * announces the toplevel — which is what a placeholder is: a window
	 * with a rectangle, a chip and a "starting…" sign that no toplevel has
	 * claimed yet. Zero is the channel on the wire, so a window with no id
	 * is addressed by nothing and answers nothing.
	 *
	 * THE OWNER AND THE ROLE ARE THE WINDOW MODEL'S AND ARE NOT KEPT HERE.
	 * KEMBED_OPEN names an owner in the cage's own numbering and the window
	 * model counts in the session's, so the owner is resolved once, at
	 * open, into `Win.owner`, and the role is spent there on `floating`,
	 * `no_task`, `modal` and `overlay`. A second copy in the cage's
	 * numbering is one that would eventually be read in the session's.
	 */
	uint32_t id;

	/*
	 * WHEN THIS WINDOW WAS ASKED TO GO, 0 for never. A second ask does not
	 * restart it, or a person's impatience would be the reason the
	 * deadline never fires; the GUEST clears it, by retiring the window or
	 * by opening a question about it.
	 *
	 * A WINDOW HAS ITS OWN DEADLINE BECAUSE A SIGNAL IS THE PROCESS'S. A
	 * guest holding five toplevels that answers none of them never lets
	 * `nwins` fall, so a process clock alone would never arm and nothing
	 * on the desktop could remove any of them — and a modal that ignores
	 * its close blocks its owner's, so there would be no route to the
	 * guest at all.
	 */
	unsigned long long close_ms;

	void *map;
	size_t map_len, slot_len;
	int pw, ph;			/* the mapping, in pixels */
	size_t stride;
	/*
	 * THE HALF THE CAGE LAST ANNOUNCED, AND THE HALF THIS WALK READS.
	 *
	 * They are separate because a walk spans turns: the cursor carries on
	 * where a turn ran out of display queue, and a walk that took its
	 * blocks from whichever half was newest at each turn would put squares
	 * of several guest frames into one picture by construction. A walk
	 * reads `pub_slot` from its first block to its last and adopts
	 * `latest` only when it wraps, so the picture a display assembles is
	 * one frame of the guest.
	 *
	 * IT NARROWS THE TEAR, IT DOES NOT CLOSE IT. KEMBED_SLOTS is two and
	 * the cage flips halves per rendered frame with nothing to ask
	 * permission of, so a guest that renders twice during one walk
	 * overwrites the half being read. Closing it needs a third half and a
	 * parent -> child release the cage waits on; with two, a walk that
	 * finishes inside one turn is coherent and a walk that spans several
	 * turns of a fast guest is not.
	 */
	int latest;
	int pub_slot;

	int cell_w, cell_h;
	int cols, rows;			/* the size the guest was asked for */
	int tile;			/* cells per block, at most EM_TILE */
	int bw, bh;			/* blocks across and down */
	/* THE SESSION'S SLOT ROTATION WAS EMPTY WHEN THIS WINDOW WAS CUT, so
	 * some of its blocks have nowhere to put a picture and are drawn as
	 * the fallback shade. Said once per window, because the shortfall is
	 * the desktop's and not this window's doing. */
	int short_slots;

	/*
	 * THE SIZE THE GUEST HAS NOT BEEN TOLD YET, AND THE LAST ONE IT WAS.
	 *
	 * `want` is 0 when there is nothing outstanding. `sent` is what the
	 * guest's mapping is expected to come back as, so a mapping that does
	 * not match it is a reconfigure still in flight and the next size
	 * waits — one at a time is what keeps a drag from queueing a
	 * reconfigure per pointer motion. PER WINDOW: the gate is about one
	 * output's mode, and a window that is not being dragged must not wait
	 * behind one that is.
	 */
	int want_pw, want_ph;
	int sent_pw, sent_ph;
	/*
	 * AND THE RECTANGLE THIS WINDOW HAS ALREADY BEEN CORRECTED TO.
	 *
	 * The cage gives its first output to the first toplevel that MAPS and
	 * this end gives the placeholder to the first ORDINARY one, so the two
	 * may name different toplevels and the window that claims the
	 * placeholder can come back mapped at its own natural size. take_buf()
	 * answers that with the rectangle the window model gave it — once per
	 * rectangle, because a guest that is told one and maps something else
	 * has answered, and re-asking on every mapping would be a size per
	 * frame for the life of the window.
	 */
	int corr_pw, corr_ph;
	unsigned long long size_ms;	/* when the last one went */
	int slots[EM_MAX_BLOCKS];	/* session sprite slots, -1 unassigned */

	uint32_t *scratch;		/* one block, contiguous */
	size_t scratch_px;

	/*
	 * WHICH BLOCKS ARE OWED TO WHICH DISPLAY, and it is per block rather
	 * than one rectangle because a cycle may only afford some of them: a
	 * bounding box has no way to say "these four went and those six did
	 * not", and a box that was partly sent is a window with stale squares
	 * in it that nothing ever repaints.
	 *
	 * A BIT PER VIEW, because displays do not keep pace with each other. A
	 * single flag cleared when any view took the block leaves the others
	 * stale for ever; one cleared only when every view took it makes a
	 * recorder attached at a tenth of the rate hold the screen back to its
	 * rate, because the block is re-cut and re-sent to the display that
	 * already has it on every cycle until the slowest one catches up. The
	 * bit is the view's index in the server's list, so the whole window is
	 * owed again whenever that list changes length — a cheap and
	 * self-correcting answer to a view that attached or went away.
	 */
	uint32_t dirty[EM_MAX_BLOCKS];
	int ndirty;
	int nviews;			/* the list length the bits are about */
	int cursor;			/* where the next cycle starts */

	/*
	 * WHAT THE GUEST DAMAGED WHILE A WALK WAS IN FLIGHT, in the same bits.
	 *
	 * A walk reads one half of the mapping from first block to last, so
	 * damage arriving mid-walk is about pixels that half does not carry.
	 * Owing it straight away would send the block from the pinned frame
	 * and clear the bit, and the pixels that caused the damage would never
	 * be sent at all. It is held here and merged when the walk wraps,
	 * which is also the moment the newer half is adopted — and it is why
	 * the owed count can reach zero under continuous damage instead of
	 * standing at the whole grid for ever.
	 */
	uint32_t pend[EM_MAX_BLOCKS];
	int walk_n;			/* blocks this walk has visited */

	/*
	 * HOW MUCH REPAIR EACH DISPLAY HAS BEEN PAID SINCE THE GUEST LAST
	 * DAMAGED THE WINDOW, in blocks, and when each was last offered a
	 * picture it cannot use as pixels. Both are indexed by the view's
	 * position in the server's list, the same bit position `dirty` uses.
	 */
	int repair[32];
	unsigned long long slow_ms[32];

	/*
	 * WHETHER A FRAME HAS EVER ARRIVED FOR THIS WINDOW. Until it has there
	 * is no picture to show and the window says what it is doing instead
	 * of standing black — which is what a container taking half a minute
	 * to come up looks like otherwise, and is indistinguishable from a
	 * dead one.
	 */
	int drew;
	/*
	 * WHAT THE GUEST HOLDS AND WHAT IT HAS BEEN TOLD, and they are two
	 * questions. `asleep` is this window's own answer to "can anybody see
	 * it" and gates the publish walk whatever the channel is doing;
	 * `sleep_told` is what actually crossed. A window that is not
	 * addressable yet — a placeholder, before its toplevel exists — can be
	 * put to sleep, raised and made fullscreen before it has a name on the
	 * wire, and a state latched as sent when nothing was sent is a state
	 * the guest is never told once it does.
	 */
	int focused, asleep, sleep_told;
	/*
	 * THE WINDOW STATE THE GUEST HAS BEEN TOLD. Fullscreen is the window
	 * model's answer and not the guest's request: `Super+f` and a dragged
	 * frame button put a window on the whole screen without the guest
	 * asking, and a guest that is never told draws the decorations of a
	 * windowed application across a screen that has none.
	 */
	int full_sent;
	/*
	 * THE GUEST SAYS THE SCREEN MUST STAY ON. It holds until the guest
	 * clears it or the window goes; a window nobody can see does not hold
	 * it, because an inhibitor honoured for bookkeeping is a battery spent
	 * on a video playing where nothing displays it.
	 */
	int inhibit;
	/*
	 * THE GUEST HOLDS THE POINTER, KEMBED_GRAB_*. While it does, the
	 * pointer reaches this window and nothing else: the desktop stops
	 * hovering, stops raising, and sends the delta with no position behind
	 * it. Moving the keyboard focus is the only way out, which is what
	 * keeps a captured pointer from being a machine nobody can escape.
	 */
	int grab;
	/* WHERE THE GUEST LAST SAW THE POINTER, its own pixels. A button sent
	 * while the pointer is held must carry the position the guest already
	 * has, or the cage warps the cursor the guest is holding still. */
	int last_px, last_py;
	/*
	 * EVERY KEY AND EVERY BUTTON THIS WINDOW IS OWED A RELEASE FOR.
	 *
	 * A press that crossed and a release that did not is a key held down
	 * for the life of the guest — which is what a window losing the
	 * keyboard mid-chord leaves behind, because the release is delivered
	 * to whatever has the focus when it arrives. The set is emptied into
	 * the channel before the focus goes, so the cage's own sweep on
	 * KEMBED_FOCUS a=0 finds nothing left to release and no key is
	 * released twice.
	 *
	 * TWO SETS AND NOT ONE, each sized from the whole of evdev's range
	 * because keys and pointer buttons share it. A release must go back as
	 * the op the press went out as: a button released as a key reaches the
	 * guest's keyboard and is resolved as a keysym, and a key released as
	 * a button is a click nobody made.
	 */
	uint32_t held[EM_KEYSET];
	uint32_t held_btn[EM_KEYSET];
};

/*
 * WHAT AN EMBEDDED WINDOW IS ACTUALLY ACHIEVING, once a second, when
 * KDOS_EMBED_STAT is set in the session's environment.
 *
 * There is otherwise no way to say what frame rate a guest reaches: the guest
 * renders at one rate, the session publishes at another and the display paints
 * at a third, and a change to any of them can be argued about for ever without
 * a number. The three counts here are those three rates, and `refused` is the
 * one that says which of them is the constraint — a display that is behind
 * refuses blocks, and blocks refused per second is the distance between what
 * the session had to send and what the screen could take.
 *
 * Counted always and printed only when asked, because a counter that is
 * compiled out is a counter nobody can ask for on the machine that has the
 * problem, and four increments per block are not measurable beside the copy
 * they sit next to.
 */
static struct {
	unsigned long frames;	/* KEMBED_FRAME from any cage            */
	unsigned long blocks;	/* blocks handed to at least one display */
	unsigned long refused;	/* blocks a display would not take       */
	unsigned long long bytes;
	unsigned long long when;
} em_stat;

static unsigned long long now_ms(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (unsigned long long)ts.tv_sec * 1000ull +
	       (unsigned long long)(ts.tv_nsec / 1000000);
}

/*
 * THE INPUT HALF IS AT THE END OF THIS FILE and the pump is near the start, so
 * the three things the pump needs from it are named here. Everything else in
 * that section is reached from the session and is declared in con.h.
 */
static void release_held(struct EmbedWin *e);
static void send_mods(struct EmbedProc *p);
static void send_keymap(struct EmbedProc *p);
static void send_clip(struct EmbedProc *p);

/* ── the wire ────────────────────────────────────────────────────────── */

/*
 * EVERY MESSAGE NAMES A WINDOW, AND ZERO NAMES THE CHANNEL.
 *
 * The two senders differ in that and in nothing else. A window op carries the
 * id the cage allocated, so an op about one toplevel cannot be answered on
 * another; a channel op — the keymap, the modifier mask, a close asked of the
 * whole guest — carries zero, which the far end reads as the channel itself
 * and never as a window.
 *
 * A WINDOW THE CAGE HAS NOT ANNOUNCED IS ADDRESSED BY NOTHING. Its id is 0
 * until KEMBED_OPEN arrives and 0 on a window op means the channel, so a
 * placeholder is sent no input, no size and no close — every one of which
 * would reach a toplevel the guest has not mapped.
 */
static int proc_send(struct EmbedProc *p, unsigned op, int a, int b, int c,
		     int d, unsigned f, uint32_t win)
{
	KembedMsg m = { .magic = KEMBED_MAGIC, .op = op, .a = a, .b = b,
			.c = c, .d = d, .e = f, .win = win };

	if (!p || p->fd < 0)
		return -1;
	while (send(p->fd, &m, sizeof(m), MSG_NOSIGNAL) < 0) {
		if (errno == EINTR)
			continue;
		return -1;
	}
	return 0;
}

static int send_msg(struct EmbedWin *e, unsigned op, int a, int b, int c,
		    int d, unsigned f)
{
	if (!e || !e->id)
		return -1;
	return proc_send(e->proc, op, a, b, c, d, f, e->id);
}

/*
 * THE SAME MESSAGE, CARRYING A DESCRIPTOR.
 *
 * This channel is the one place in the session a descriptor may cross, which
 * is why the keymap travels as one: a compiled xkb layout is tens of kilobytes
 * and chunking it through a 32-byte message would be thousands of datagrams
 * through the loop that also pumps frames. `fd` is the caller's and stays the
 * caller's — the kernel copies it, so the sender closes its own copy when it
 * is finished with it and not before.
 */
static int proc_send_fd(struct EmbedProc *p, unsigned op, int a, int b, int c,
			int d, unsigned f, int fd)
{
	KembedMsg m = { .magic = KEMBED_MAGIC, .op = op, .a = a, .b = b,
			.c = c, .d = d, .e = f };
	struct iovec iov = { .iov_base = &m, .iov_len = sizeof(m) };
	struct msghdr hdr = { .msg_iov = &iov, .msg_iovlen = 1 };
	union {
		char buf[CMSG_SPACE(sizeof(int))];
		struct cmsghdr align;
	} u;
	struct cmsghdr *c1;

	if (!p || p->fd < 0 || fd < 0)
		return -1;
	memset(&u, 0, sizeof(u));
	hdr.msg_control = u.buf;
	hdr.msg_controllen = sizeof(u.buf);
	c1 = CMSG_FIRSTHDR(&hdr);
	c1->cmsg_level = SOL_SOCKET;
	c1->cmsg_type = SCM_RIGHTS;
	c1->cmsg_len = CMSG_LEN(sizeof(int));
	memcpy(CMSG_DATA(c1), &fd, sizeof(fd));

	while (sendmsg(p->fd, &hdr, MSG_NOSIGNAL) < 0) {
		if (errno == EINTR)
			continue;
		return -1;
	}
	return 0;
}

/*
 * THE SIZE THE GUEST IS OWED, SENT WHEN IT MAY BE.
 *
 * Two gates, and they answer different failures. ONE RECONFIGURE AT A TIME,
 * because the cage answers a size with a new output mode, a new memfd, a new
 * mapping in both processes and a relayout inside the application — a second
 * size arriving mid-reallocation is a third one to do, and a drag that sent
 * one per pointer motion leaves the guest seconds behind the edge for the
 * whole gesture. AND AT MOST ONE A FRAME, because a size the screen has not
 * shown yet is a size nobody has seen the result of.
 *
 * A NEW MAPPING ENDS THE WAIT, whatever size it carries: take_buf() records
 * the pixels the guest came back with as the pixels it was told, because a
 * size the cage folded into an earlier one is a size no mapping will ever
 * report. The wait also stops being a reason after EM_SIZE_LATE_MS, which is
 * what covers a guest that never remaps at all — either bound missing holds
 * the channel shut for the life of the window.
 */
static void size_flush(struct EmbedWin *e)
{
	unsigned long long t;

	if (e->want_pw < 1 || e->want_ph < 1)
		return;
	if (e->want_pw == e->sent_pw && e->want_ph == e->sent_ph) {
		e->want_pw = e->want_ph = 0;	/* it is already that size */
		return;
	}

	t = now_ms();
	if (t - e->size_ms < EM_SIZE_MS)
		return;
	if (e->map && (e->pw != e->sent_pw || e->ph != e->sent_ph) &&
	    t - e->size_ms < EM_SIZE_LATE_MS)
		return;
	if (send_msg(e, KEMBED_SIZE, e->want_pw, e->want_ph, 0, 0, 0) != 0)
		return;
	e->sent_pw = e->want_pw;
	e->sent_ph = e->want_ph;
	e->want_pw = e->want_ph = 0;
	e->size_ms = t;
}

/*
 * ONE DATAGRAM: THE STRUCT, WHATEVER STRING FOLLOWS IT, AND THE DESCRIPTOR IT
 * MAY CARRY.
 *
 * A message SHORTER than the struct is a peer speaking something else; there
 * is exactly one peer and it is our own child, so the answer is to stop
 * talking to it rather than to guess. A LONGER one is not: the socket is
 * SOCK_SEQPACKET, so an op that carries a name carries it in the bytes after
 * the struct and the datagram boundary is its end.
 *
 * THE BUFFER IS KEMBED_TAIL_MAX AND NOT THE LONGEST TAIL THIS END READS. A
 * datagram larger than the buffer is truncated by the kernel with no error
 * anywhere, and a receiver that then measured the tail against the struct
 * would read a cut one as whole. `text` is always terminated and always cut
 * to `tcap` — a name past that is one this desktop could not draw anyway.
 */
static int recv_msg(struct EmbedProc *p, KembedMsg *m, int *fd, char *text,
		    size_t tcap)
{
	char buf[sizeof(*m) + KEMBED_TAIL_MAX];
	struct iovec iov = { .iov_base = buf, .iov_len = sizeof(buf) };
	struct msghdr hdr = { .msg_iov = &iov, .msg_iovlen = 1 };
	union {
		char buf[CMSG_SPACE(sizeof(int))];
		struct cmsghdr align;
	} u;
	ssize_t n;

	*fd = -1;
	if (text && tcap)
		text[0] = '\0';
	memset(&u, 0, sizeof(u));
	hdr.msg_control = u.buf;
	hdr.msg_controllen = sizeof(u.buf);

	/*
	 * CLOSE-ON-EXEC AS IT ARRIVES. Every mapping the cage passes lands
	 * here, and embed_open() forks between two of them: a descriptor
	 * without the flag is one the next guest inherits, which is one
	 * window's pixels readable from inside another application's box.
	 */
	do {
		n = recvmsg(p->fd, &hdr, MSG_DONTWAIT | MSG_CMSG_CLOEXEC);
	} while (n < 0 && errno == EINTR);

	if (n < 0)
		return errno == EAGAIN || errno == EWOULDBLOCK ? 0 : -1;
	if (n == 0)
		return -1;
	if (n < (ssize_t)sizeof(*m))
		return -1;
	memcpy(m, buf, sizeof(*m));
	if (m->magic != KEMBED_MAGIC)
		return -1;

	if (text && tcap) {
		size_t len = (size_t)n - sizeof(*m);

		if (len > tcap - 1)
			len = tcap - 1;
		memcpy(text, buf + sizeof(*m), len);
		text[len] = '\0';
	}

	/* THE LENGTH IS TESTED BEFORE THE PAYLOAD IS READ. A control message
	 * shorter than one descriptor is one this end would otherwise copy
	 * four bytes out of the header of. */
	for (struct cmsghdr *c = CMSG_FIRSTHDR(&hdr); c;
	     c = CMSG_NXTHDR(&hdr, c))
		if (c->cmsg_level == SOL_SOCKET &&
		    c->cmsg_type == SCM_RIGHTS &&
		    c->cmsg_len == CMSG_LEN(sizeof(int)))
			memcpy(fd, CMSG_DATA(c), sizeof(int));

	return 1;
}

/*
 * A NAME THE SESSION CAN DRAW. Two things make a guest's title unusable as it
 * arrives: a control byte, which the cell grid would take as a command rather
 * than as text, and a cut that landed in the middle of a UTF-8 sequence, which
 * the width measurement counts as a character the painter cannot produce. Both
 * are removed here, once, rather than at every place a title is drawn.
 */
static void sanitise_title(char *s)
{
	char *w = s;

	for (const char *r = s; *r; ) {
		uint32_t cp = 0;
		const char *next = ktui_utf8_next(r, &cp);

		if (next <= r)
			break;
		/* A byte that decoded to nothing is not a character at any
		 * width, and a control byte is a command to the grid rather
		 * than text in it. */
		if (cp < 0x20 || cp == 0x7f ||
		    (next == r + 1 && (unsigned char)*r >= 0x80)) {
			r = next;
			continue;
		}
		while (r < next)
			*w++ = *r++;
	}
	*w = '\0';
}

/* ── blocks ──────────────────────────────────────────────────────────── */

/*
 * The whole window is damaged. Used when the geometry changed, when a new
 * mapping arrived and when a view attaches: a view that has never been sent a
 * block draws the fallback mark where the picture should be.
 */
/*
 * EVERY VIEW OWES THE BLOCK AGAIN. A view that already had it is sent it
 * twice rather than left with a square of a frame that has moved on — a
 * resend costs bandwidth, a missed one is visibly wrong and nothing repaints
 * it.
 */
static uint32_t all_views(void)
{
	int n = kcon_server_view_count(S.server);

	if (n < 1)
		return 0;
	if (n > 32)
		n = 32;
	return n == 32 ? 0xffffffffu : (1u << n) - 1u;
}

static void mark(struct EmbedWin *e, int bx, int by)
{
	int i = by * e->bw + bx;
	uint32_t all = all_views();

	if (bx < 0 || by < 0 || bx >= e->bw || by >= e->bh ||
	    i >= EM_MAX_BLOCKS || !all)
		return;
	/*
	 * A WALK IN FLIGHT IS READING AN OLDER HALF, so the block is owed
	 * about the newer one and the walk must not answer it from the half it
	 * has pinned. The bit waits in `pend` until the walk wraps.
	 */
	if (e->walk_n > 0) {
		e->pend[i] = all;
		return;
	}
	if (!e->dirty[i])
		e->ndirty++;
	e->dirty[i] = all;
}

/*
 * THE GUEST DREW, SO EVERY DISPLAY IS WORTH PAYING AGAIN. The repair budget
 * exists for a display that lost a picture nothing else will resend; a window
 * the guest is still drawing resends those blocks by the ordinary path, so the
 * budget is restored here rather than spent on damage it duplicates.
 */
static void repair_reset(struct EmbedWin *e)
{
	memset(e->repair, 0, sizeof(e->repair));
}

static void damage_all(struct EmbedWin *e)
{
	/*
	 * AND ANY WALK IN FLIGHT IS OVER. The whole window is owed about
	 * whatever the guest publishes next, so a walk still pinned to an
	 * older half has nothing left worth finishing — and damage held back
	 * against that half is already covered by what is owed here.
	 */
	e->walk_n = 0;
	memset(e->pend, 0, sizeof(e->pend));
	repair_reset(e);
	for (int by = 0; by < e->bh; by++)
		for (int bx = 0; bx < e->bw; bx++)
			mark(e, bx, by);
}

static void damage_add(struct EmbedWin *e, int x, int y, int w, int h)
{
	if (w <= 0 || h <= 0 || e->cell_w < 1 || e->cell_h < 1 || e->tile < 1)
		return;

	repair_reset(e);

	int span_w = e->tile * e->cell_w, span_h = e->tile * e->cell_h;
	int bx0 = x / span_w, by0 = y / span_h;
	int bx1 = (x + w + span_w - 1) / span_w;
	int by1 = (y + h + span_h - 1) / span_h;

	if (bx0 < 0)
		bx0 = 0;
	if (by0 < 0)
		by0 = 0;
	for (int by = by0; by < by1; by++)
		for (int bx = bx0; bx < bx1; bx++)
			mark(e, bx, by);
}

/*
 * The blocks a window is cut into, and a session sprite slot for each. The
 * slots come from the server's own rotation, the same one surfaces draw from:
 * a session numbering its own pictures separately would eventually hand a view
 * a number a surface is already using.
 */
static int layout(struct EmbedWin *e, int cols, int rows)
{
	/*
	 * THE BLOCK IS CUT TO FIT ONE MESSAGE, and the cell size is what
	 * decides whether sixteen cells square still does. The encoder refuses
	 * a block above KCON_MAX_PAYLOAD exactly as it refuses one to a full
	 * queue, so a block that can never be encoded stays owed and is a
	 * permanent hole in its own window. The headroom
	 * is the message the pixels travel under.
	 */
	size_t cell = (size_t)e->cell_w * e->cell_h * 4;
	int tile = EM_TILE;

	while (tile > 1 && (size_t)tile * tile * cell > KCON_MAX_PAYLOAD - 64)
		tile--;

	int bw = (cols + tile - 1) / tile;
	int bh = (rows + tile - 1) / tile;

	/*
	 * NOTHING MOVES UNTIL EVERYTHING CAN. The grid, the slot table, the
	 * scratch block and the owed set describe one size between them, and a
	 * refusal that had already written some of them leaves embed_draw()
	 * indexing a slot table sized for the other — past the end of it once
	 * the new grid is the larger — and send_block() cutting into a scratch
	 * buffer sized for a smaller cell. The caller's answer to a refusal is
	 * to leave the window at the size it has, which only works if this
	 * left it there.
	 */
	if (bw < 1 || bh < 1 || bw * bh > EM_MAX_BLOCKS)
		return -1;

	size_t px = (size_t)tile * e->cell_w * tile * e->cell_h;

	if (px > e->scratch_px) {
		uint32_t *p = realloc(e->scratch, px * 4);

		if (!p)
			return -1;
		e->scratch = p;
		e->scratch_px = px;
	}

	e->cols = cols;
	e->rows = rows;
	e->tile = tile;
	e->bw = bw;
	e->bh = bh;

	/*
	 * WHAT WAS OWED WAS OWED ABOUT A DIFFERENT GRID. An index is
	 * `by * bw + bx`, so a row count that changed makes every bit stale —
	 * and a bit outside the new grid can never be cleared, which would
	 * leave the count of owed blocks permanently above zero and the
	 * publisher walking the window on every cycle for ever. Every caller
	 * damages the whole window straight afterwards.
	 */
	memset(e->dirty, 0, sizeof(e->dirty));
	memset(e->pend, 0, sizeof(e->pend));
	e->ndirty = 0;
	e->cursor = 0;
	e->walk_n = 0;
	repair_reset(e);

	/*
	 * THE SLOTS ABOVE THE NEW GRID GO BACK NOW, not when the window
	 * closes. The rotation is the session's and every window and every
	 * surface draws from it, so a window shrinking from the whole screen to
	 * a quarter of it while holding fifty slots it cannot index is fifty
	 * pictures the next window cannot have — and with several windows per
	 * application that is the difference between a dialog with pixels in
	 * it and one drawn entirely in shade.
	 */
	for (int i = e->bw * e->bh; i < EM_MAX_BLOCKS; i++)
		if (e->slots[i] >= 0) {
			kcon_server_free_slot(S.server, e->slots[i]);
			e->slots[i] = -1;
		}

	/*
	 * AND WHAT IS LEFT IS TAKEN, BLOCK BY BLOCK, WITH NO ALL-OR-NOTHING.
	 *
	 * An empty rotation is a desktop with more pictures open than the
	 * protocol numbers, and refusing the whole window for it would leave
	 * the guest at a size nothing told it about. A block with no slot is
	 * drawn as the fallback shade and owed to no display — visibly
	 * unfinished, in ITS OWN window, which is the one failure here that
	 * cannot be mistaken for another window's pixels.
	 */
	for (int i = 0; i < e->bw * e->bh; i++) {
		if (e->slots[i] >= 0)
			continue;
		e->slots[i] = kcon_server_alloc_slot(S.server);
		if (e->slots[i] < 0 && !e->short_slots) {
			e->short_slots = 1;
			fprintf(stderr, "kdos-con: no session sprite slot left "
					"for '%s' — part of the window is "
					"drawn as shade\n",
				e->proc ? e->proc->prog : "a window");
		}
	}
	return 0;
}

/*
 * Cut one block out of the mapping and hand it to every attached view. The
 * rows are copied rather than pointed at: the mapping's stride is the whole
 * window's and a sprite's bytes have to be contiguous, and the half being read
 * is the one the child is not writing.
 */
/*
 * OFFERED ONLY TO THE DISPLAYS IN `*ready`, and a display that turns out to be
 * full loses its bit there for the rest of the turn. The mask is the turn's
 * record of who still has room: the caller tests it before cutting a block at
 * all, so a display that filled up stops costing block copies instead of
 * paying for one per remaining block and throwing every one of them away.
 *
 * A block still owed to a display keeps its bit in `*owed` and is offered
 * again on a later walk. Nothing about one display's refusal may stop the
 * block reaching another, or the fastest screen is paced by the slowest.
 *
 * A BLOCK OUTSIDE THE GUEST'S BUFFER IS NOT A REFUSAL. The window's cell grid
 * and the guest's pixels disagree for as long as it takes a resize to reach
 * the guest and a new mapping to come back, and during that window the blocks
 * past the old buffer's edge have no pixels to cut. Reading that as a display
 * being behind would stop the whole window publishing — including the blocks
 * that DO have pixels — until the guest caught up, which is a window frozen
 * mid-resize rather than one whose edge arrives late.
 */
static void send_block(struct EmbedWin *e, int bx, int by, uint32_t *owed,
		       uint32_t *ready)
{
	int cx = bx * e->tile, cy = by * e->tile;
	int cw = e->cols - cx, ch = e->rows - cy;

	if (cw > e->tile)
		cw = e->tile;
	if (ch > e->tile)
		ch = e->tile;
	if (cw < 1 || ch < 1) {
		*owed = 0;	/* not a block of this window at all */
		return;
	}

	int px = cx * e->cell_w, py = cy * e->cell_h;
	int pw = cw * e->cell_w, ph = ch * e->cell_h;

	if (px + pw > e->pw || py + ph > e->ph) {
		/*
		 * And it stops being owed. A bit nothing can ever clear holds
		 * the count of owed blocks above zero for the life of the
		 * window, which is the publisher walking the whole grid on
		 * every turn for ever; the mapping that gives these blocks
		 * pixels damages the whole window when it arrives, so
		 * forgetting them here loses nothing.
		 */
		*owed = 0;
		return;
	}

	int slot = e->slots[by * e->bw + bx];

	if (slot < 0) {
		*owed = 0;	/* nowhere to put a picture; never will be */
		return;
	}

	const uint8_t *base = (const uint8_t *)e->map +
			      (size_t)e->pub_slot * e->slot_len;

	for (int y = 0; y < ph; y++)
		memcpy(e->scratch + (size_t)y * pw,
		       base + (size_t)(py + y) * e->stride + (size_t)px * 4,
		       (size_t)pw * 4);

	/*
	 * WHAT A PICTURE LOOKS LIKE WHERE THERE ARE NO PIXELS. Something
	 * rather than nothing: a window that rendered as blank cells is
	 * indistinguishable from one that never drew.
	 */
	uint32_t fb = 0x2593u;
	int n = kcon_server_view_count(S.server);
	int took = 0;

	/*
	 * ONLY THE VIEWS THAT STILL OWE IT, and each one's bit is cleared on
	 * its own. A display that is behind refuses the piece and keeps its
	 * bit; the one beside it that took the block is not sent it again, so
	 * a recorder reading at a tenth of the rate costs its own bandwidth
	 * and not the screen's.
	 *
	 * Flushed here rather than once at the end of the loop, so the bytes
	 * go to the socket between blocks instead of piling up in the queue
	 * the watermark is measured against.
	 */
	if (n > 32)
		n = 32;
	for (int i = 0; i < n; i++) {
		KconSurface *v;

		if (!(*owed & *ready & (1u << i)))
			continue;
		v = kcon_server_view_at(S.server, i);
		/*
		 * MEASURED AGAIN HERE, because the mask was taken before the
		 * walk and this turn's own blocks are what filled the queue
		 * since. A display over the mark is out of the turn rather
		 * than out of this block: the blocks after it would be copied
		 * for a queue that refuses every one of them.
		 */
		if (kcon_view_pending(v) > EM_VIEW_SOFT) {
			*ready &= ~(1u << i);
			em_stat.refused++;
			continue;
		}
		if (!kcon_view_sprite(v, slot, cw, ch, fb, e->scratch, pw, ph)) {
			em_stat.refused++;
			continue;
		}
		kcon_view_flush(v);
		em_stat.bytes += (unsigned long long)pw * ph * 4;
		*owed &= ~(1u << i);
		took = 1;
	}
	em_stat.blocks += (unsigned)took;
}

/*
 * WHICH DISPLAYS HAVE ROOM THIS TURN, one bit per position in the server's
 * list — the same bit `dirty` owes a block with.
 *
 * TAKEN ONCE, ABOVE THE WALK, because it decides which blocks are worth
 * cutting: a block copied into the scratch buffer for a display that cannot
 * take it is a full block memcpy thrown away, and under whole-viewport damage
 * that is the whole grid's worth of copying per turn.
 *
 * A DISPLAY THAT CANNOT SHOW PIXELS IS RATE-LIMITED ON ITS OWN, not by
 * holding the turn back. A terminal view matches the picture to characters and
 * a window of them at a compositor's frame rate down an ssh link is a link
 * that does nothing else — but gating the whole turn on it stops the screen
 * beside it as well, which is the constraint the per-view bits exist to
 * remove.
 */
static uint32_t ready_views(struct EmbedWin *e, unsigned long long t)
{
	int n = kcon_server_view_count(S.server);
	uint32_t r = 0;

	if (n > 32)
		n = 32;
	for (int i = 0; i < n; i++) {
		KconSurface *v = kcon_server_view_at(S.server, i);

		if (kcon_view_pending(v) > EM_VIEW_SOFT)
			continue;
		if (!(kcon_view_caps(v) & KCON_VIEW_PIXELS)) {
			if (t - e->slow_ms[i] < EM_SLOW_MS)
				continue;
			e->slow_ms[i] = t;
		}
		r |= 1u << i;
	}
	return r;
}

/*
 * THE WALK WRAPPED: THE NEWER HALF BECOMES THE ONE TO READ AND THE DAMAGE HELD
 * AGAINST IT BECOMES OWED.
 *
 * Both at once, because they are the same decision. Adopting the half without
 * owing the damage would publish a newer frame's pixels for blocks nothing
 * asked to be re-sent; owing the damage without adopting the half would cut
 * those blocks from the frame the damage was not about.
 */
static void walk_end(struct EmbedWin *e, int nb)
{
	e->pub_slot = e->latest;
	e->walk_n = 0;
	for (int i = 0; i < nb; i++) {
		if (!e->pend[i])
			continue;
		if (!e->dirty[i])
			e->ndirty++;
		e->dirty[i] |= e->pend[i];
		e->pend[i] = 0;
	}
}

/*
 * HOW MUCH GOES OUT, AND WHERE IT STARTS.
 *
 * EACH DISPLAY'S OWN QUEUE IS THAT DISPLAY'S PACE, and no display's is
 * anybody else's. A turn offers every display blocks until ITS queue reaches
 * EM_VIEW_SOFT and then leaves it out of the rest of the turn; the queue can
 * never approach KCON_MAX_QUEUE, the mark that would declare the peer dead.
 * A turn ends early only when no display has room at all, because that is the
 * one state in which there is nothing to do.
 *
 * ONE WALK IS ONE GUEST FRAME. The walk pins the half of the mapping it
 * reads and holds it from its first block to its last, however many turns
 * that takes, so a display assembles a picture of one moment rather than a
 * band of squares from several. Damage arriving mid-walk waits in `pend` and
 * is owed when the walk wraps — see `pub_slot` for what two halves can and
 * cannot guarantee.
 *
 * THE CURSOR CARRIES ON WHERE THE LAST TURN STOPPED and advances past every
 * block the turn offered, taken or not, so a block one display could not take
 * does not hold the walk on it and no corner is starved by a guest that keeps
 * damaging the same place. Blocks that were not taken stay owed, which is the
 * only record that they are.
 */
static void publish(struct EmbedWin *e)
{
	/*
	 * A BIT IS A VIEW'S POSITION IN THE SERVER'S LIST, so a list that
	 * changed length is a set of bits about other displays. Owing the
	 * whole window again is the cheap answer and the only correct one: a
	 * view that went away took its position with it and every view after
	 * it moved down one.
	 */
	int nv = kcon_server_view_count(S.server);

	if (nv != e->nviews) {
		e->nviews = nv;
		damage_all(e);
	}

	/*
	 * A WALK THAT IS NOT IN FLIGHT READS THE NEWEST HALF THE CAGE HAS
	 * ANNOUNCED, and a mapping that has carried no frame yet has none.
	 */
	if (e->walk_n < 1)
		e->pub_slot = e->latest;
	if (!e->map || e->pub_slot < 0 || e->asleep)
		return;

	int nb = e->bw * e->bh;

	if (nb < 1 || nb > EM_MAX_BLOCKS)
		return;
	if (e->cursor < 0 || e->cursor >= nb)
		e->cursor = 0;

	/*
	 * A WALK WITH NOTHING LEFT OWED IS FINISHED WHEREVER IT STOPPED. It
	 * cannot be left in flight: damage waits in `pend` for as long as one
	 * is, so a walk that emptied the owed set short of the ring and was
	 * never ended would hold every later frame there and the window would
	 * stop for good.
	 */
	if (!e->ndirty) {
		if (e->walk_n > 0)
			walk_end(e, nb);
		if (!e->ndirty)
			return;
	}

	uint32_t ready = ready_views(e, now_ms());
	int n;

	if (!ready)
		return;

	/*
	 * THE RING POSITION IS FIXED FOR THE TURN. `n` is what walks it, so
	 * indexing off the cursor while the loop also advances the cursor
	 * moves the walk twice per block and visits a scattered subset of the
	 * ring — blocks that are never reached stay owed and the window never
	 * completes.
	 */
	int base = e->cursor;

	for (n = 0; n < nb && ready; n++) {
		int i = (base + n) % nb;

		/* Untouched, so uncopied: the block is owed to nobody with
		 * room and cutting it would be a copy for a queue that
		 * refuses it. */
		if (!(e->dirty[i] & ready))
			continue;
		send_block(e, i % e->bw, i / e->bw, &e->dirty[i], &ready);
		if (!e->dirty[i])
			e->ndirty--;
		e->cursor = (i + 1) % nb;
	}

	/*
	 * THE WALK IS OVER WHEN IT HAS VISITED THE WHOLE RING, counted across
	 * turns because a turn that ran out of display queue left the rest of
	 * the ring unvisited and the next one resumes at the cursor. A walk
	 * that adopted a newer half mid-ring would be the several-moments
	 * picture the pin exists to prevent.
	 */
	e->walk_n += n;
	if (e->walk_n >= nb)
		walk_end(e, nb);
}

/* ── which way a graphical application is shown ──────────────────────── */

/*
 * THE NAME A POLICY IS KEYED ON, AND THE LAUNCHER CARRIES IT RATHER THAN THIS
 * SIDE DERIVING IT.
 *
 * A generated launcher for an application that belongs to a pack runs
 * `kdos-appbox -b <pack> run <exec>` — `--box` is the same option — and <pack>
 * is the string kdos-appbox files the box profile under, so the console opens
 * the same file kdos-appbox will, with no second copy of appbox's exec-to-pack
 * rule to disagree with it. That rule is not a basename: it skips `env` and
 * `VAR=value` prefixes and matches the whole joined command, so a copy here
 * would answer differently for exactly the applications that need it most.
 *
 * `kdos box export` WRITES THE OTHER GENERATED SHAPE, `kdos-box run <box>
 * <app>`, where the box is spelled out and argv[0] is the second name on the
 * same binary. Both shapes must be read here or an exported application keys
 * no profile and shares one app id with every other one.
 *
 * A BARE `run <exec>` NAMES NO PACK and therefore no profile. The program's
 * own name is what is left; it keys no profile that exists, and the window is
 * still named, iconised and grouped by it. The pointer is into the caller's
 * own argv — the return value is never freed.
 */
static const char *guest_name(const char *const argv[])
{
	if (argv[0] && strstr(argv[0], "kdos-appbox")) {
		if (argv[1] && argv[2] && argv[3] && argv[4] &&
		    (!strcmp(argv[1], "-b") || !strcmp(argv[1], "--box")) &&
		    !strcmp(argv[3], "run"))
			return argv[2];
		if (argv[1] && argv[2] && !strcmp(argv[1], "run"))
			return kb_basename(argv[2]);
	}
	if (argv[0] && argv[1] && argv[2] && argv[3] &&
	    !strcmp(kb_basename(argv[0]), "kdos-box") &&
	    !strcmp(argv[1], "run"))
		return argv[2];

	return kb_basename(argv[0]);
}

/*
 * THE PROGRAM'S OWN NAME, WHICH IS THE DESKTOP ENTRY'S STEM AND NOT THE BOX'S.
 *
 * A generated launcher's entry is filed under the program — `firefox-esr` for
 * the pack `app.firefox-esr` — so the taskbar resolves a label, an icon and
 * the pin to merge with from this and never from the box the profile is filed
 * under. An entry `kdos box export` wrote carries the app as its
 * `StartupWMClass`, which is the same word, so both generated shapes resolve.
 * The basename and not the Exec's first word, because an upstream entry is
 * free to give an absolute path and every reader of an app id refuses one: a
 * path resolves no entry, so the chip carries a letter and no name.
 */
static const char *guest_prog(const char *const argv[])
{
	if (argv[0] && argv[1] && argv[2] && strstr(argv[0], "kdos-appbox")) {
		if (argv[3] && argv[4] &&
		    (!strcmp(argv[1], "-b") || !strcmp(argv[1], "--box")) &&
		    !strcmp(argv[3], "run"))
			return kb_basename(argv[4]);
		if (!strcmp(argv[1], "run"))
			return kb_basename(argv[2]);
	}
	if (argv[0] && argv[1] && argv[2] && argv[3] &&
	    !strcmp(kb_basename(argv[0]), "kdos-box") &&
	    !strcmp(argv[1], "run"))
		return kb_basename(argv[3]);

	return kb_basename(argv[0]);
}

/*
 * One key out of a box profile, or "". Keyed on the box, which is the name
 * `kdos-box profile` takes and the file the settings surface edits — a second
 * store for one key would be a second place to look when an application comes
 * up on the wrong thing. A name with a slash in it is no box's, and opening a
 * path built out of one is how a lookup reaches outside the profile
 * directory.
 *
 * THE FILE IS READ WHOLE HERE AND WRITTEN WHOLE THERE, so a key this side
 * reads is one kdos-appbox has to carry through a rewrite even though it means
 * nothing to a container flag. A writer that keeps only its own keys deletes
 * everybody else's, and the setting disappears the next time an unrelated one
 * is changed.
 */
static void profile_key(const char *name, const char *key, char *out,
			size_t cap)
{
	char path[512];
	const char *cfg = getenv("XDG_CONFIG_HOME");
	size_t klen = strlen(key);
	char *data;

	out[0] = '\0';
	if (!name || !*name || strchr(name, '/'))
		return;

	if (cfg && *cfg)
		snprintf(path, sizeof(path), "%.400s/kdos/boxes/%.63s.conf",
			 cfg, name);
	else
		snprintf(path, sizeof(path), "%.400s/.config/kdos/boxes/%.63s.conf",
			 kb_home_dir(), name);

	data = kb_read_all(path, NULL);
	if (!data)
		return;

	for (char *line = data, *next; line && *line; line = next) {
		char *nl = strchr(line, '\n');

		next = nl ? nl + 1 : line + strlen(line);
		if (nl)
			*nl = '\0';
		while (*line == ' ' || *line == '\t')
			line++;
		if (strncmp(line, key, klen))
			continue;

		char *eq = strchr(line, '=');

		if (!eq)
			continue;
		eq++;
		while (*eq == ' ' || *eq == '\t')
			eq++;
		snprintf(out, cap, "%s", eq);
		break;
	}
	free(data);
}

static void profile_display(const char *name, char *out, size_t cap)
{
	profile_key(name, "display", out, cap);
}

/*
 * EMBEDDING IS THE DEFAULT and a terminal of its own is the exception. An
 * embedded guest is composited by the software renderer unless its box profile
 * says `render = gpu`, and a terminal of its own is what remains for an
 * application that needs the card on its own terms — a full-screen mode set,
 * or a driver that will not run against a headless output. Every rule that
 * overrides the default says so, so that `kdos doctor` and a person reading a
 * log get the same sentence.
 */
int con_display_mode(const char *const argv[], const char **why)
{
	static char reason[160];
	char disp[64];

	*why = reason;

	if (!argv || !argv[0]) {
		snprintf(reason, sizeof(reason), "there is nothing to run");
		return CON_DISPLAY_VT;
	}

	profile_display(guest_name(argv), disp, sizeof(disp));

	if (!strcmp(disp, "vt")) {
		snprintf(reason, sizeof(reason),
			 "its box profile says display = vt");
		return CON_DISPLAY_VT;
	}
	if (!kcon_conf_bool("embed", 1)) {
		snprintf(reason, sizeof(reason),
			 "con.conf says embed = false");
		return CON_DISPLAY_VT;
	}

	snprintf(reason, sizeof(reason),
		 "embedding is what a graphical application gets");
	return CON_DISPLAY_EMBED;
}

/* ── the child ───────────────────────────────────────────────────────── */

static struct EmbedProc *procs[32];
static int nprocs;

/*
 * THE WINDOWS OF EVERY GUEST, FRONT OF THE STACK FIRST.
 *
 * Walked instead of the process list wherever the answer is about windows: a
 * turn's display queue is spent per WINDOW, a sleep is decided per window, and
 * a fixed order over processes lets a five-window application take the whole
 * turn before the process behind it is reached even once. The stack is the
 * order because it is the order the eye has, and a window nothing is drawing
 * costs one test.
 */
static int all_wins(struct EmbedWin **out, int max)
{
	int n = 0;

	for (Win *w = S.wins; w && n < max; w = w->next)
		if (w->kind == WIN_EMBED && w->em)
			out[n++] = w->em;
	return n;
}

/* The most bytes of windows there can be: every process at its own ceiling. */
#define EM_WIN_CAP ((int)(sizeof(procs) / sizeof(procs[0])) * EM_MAX_WINS)

/*
 * THIS GUEST'S FRONTMOST WINDOW, which is what `win` 0 on a window op means.
 *
 * A cage names the window every op is about, so nothing in step ever sends 0
 * here — but a message about the front window is what the protocol says 0 is,
 * and answering it on a window this guest actually has is the difference
 * between a cage that is one message behind and a session that invented a
 * window to hold a frame.
 */
static struct EmbedWin *proc_front(struct EmbedProc *p)
{
	for (Win *w = S.wins; w; w = w->next)
		if (w->kind == WIN_EMBED && w->em && w->em->proc == p)
			return w->em;
	return p ? p->wins : NULL;
}

/*
 * THE WINDOW A MESSAGE NAMES, or NULL — and NULL is dropped, never guessed at.
 * A window can go while a message about it is still in flight, and the answer
 * to that is to drop the message and close whatever descriptor it carried.
 */
static struct EmbedWin *win_of(struct EmbedProc *p, uint32_t id)
{
	if (!p)
		return NULL;
	if (!id)
		return proc_front(p);
	for (struct EmbedWin *e = p->wins; e; e = e->next)
		if (e->id == id)
			return e;
	return NULL;
}

/* The window of ANY guest that the cage numbered `id` — an owner named in a
 * KEMBED_OPEN is one of this channel's, and of no other. */
static Win *owner_win(struct EmbedProc *p, uint32_t id)
{
	struct EmbedWin *o = id ? win_of(p, id) : NULL;

	return o ? o->win : NULL;
}

/*
 * HOW MANY WINDOWS OF THIS GUEST A PERSON CAN GET RID OF, which is not how
 * many it has. A splash is drawn over everything with no frame, no taskbar row
 * and no place in the ring, so there is nothing on the desktop to close it by:
 * a guest left holding only splashes is a guest a person cannot reach, and a
 * count that included them would leave the escalation permanently unarmed
 * while a cage ran on with nothing on the screen.
 */
static int nreachable(const struct EmbedProc *p)
{
	int n = 0;

	for (const struct EmbedWin *q = p->wins; q; q = q->next)
		if (q->win && !q->win->overlay)
			n++;
	return n;
}

/*
 * THE ONE WINDOW OF THIS GUEST UNDER AN ASK, NULL when none is or more than one
 * is. It answers "which window is this question about" for a dialog that named
 * no parent, and only while the answer is not a guess.
 */
static struct EmbedWin *only_asked(struct EmbedProc *p)
{
	struct EmbedWin *one = NULL;

	for (struct EmbedWin *q = p->wins; q; q = q->next)
		if (q->close_ms) {
			if (one)
				return NULL;
			one = q;
		}
	return one;
}

/* The cell size a guest is rendered at: the primary view's, because that is
 * the display the person is looking at. A second view of a different font
 * rescales the sprite, which is what it already does for every other picture. */
static void cell_size(int *w, int *h)
{
	KconSurface *v = S.server ? kcon_server_view_at(S.server, 0) : NULL;

	*w = v ? kcon_view_cell_w(v) : 0;
	*h = v ? kcon_view_cell_h(v) : 0;
	if (*w < 2 || *h < 2 || *w > 64 || *h > 64) {
		*w = EM_CELL_W;
		*h = EM_CELL_H;
	}
}

/*
 * A WINDOW OF A GUEST THAT IS ALREADY RUNNING — no fork, no socket, no
 * process. Everything a window needs of its own and nothing the channel
 * already has.
 *
 * NOT LINKED INTO ANYTHING HERE. A window exists on the desktop when its Win
 * does, and the two are built in one place — embed_adopt() below — so there is
 * no state in which a mapping has arrived for a window the window list does
 * not have.
 */
static struct EmbedWin *win_new(struct EmbedProc *p)
{
	struct EmbedWin *e = calloc(1, sizeof(*e));

	if (!e)
		return NULL;
	for (int i = 0; i < EM_MAX_BLOCKS; i++)
		e->slots[i] = -1;
	e->proc = p;
	e->latest = -1;
	e->pub_slot = -1;
	cell_size(&e->cell_w, &e->cell_h);
	return e;
}

/*
 * A WINDOW THAT NEVER REACHED THE DESKTOP, UNDONE.
 *
 * THE SLOT ROTATION IS THE SESSION'S AND WHATEVER CLAIMS FROM IT RETURNS IT.
 * layout() takes one slot per block out of a finite map shared with every
 * window and every surface, so a launch abandoned after it — a socket that
 * could not be made, a fork that failed, a second window placed and then
 * refused — burns those slots for the life of the session with no error
 * anywhere, and a desktop that has run out of them draws the fallback shade
 * where every picture should be. embed_free() is the same sweep for a window
 * the desktop did have.
 */
static void win_free_partial(struct EmbedWin *e)
{
	if (!e)
		return;
	for (int i = 0; i < EM_MAX_BLOCKS; i++)
		if (e->slots[i] >= 0) {
			kcon_server_free_slot(S.server, e->slots[i]);
			e->slots[i] = -1;
		}
	free(e->scratch);
	free(e);
}

Win *embed_open(const char *const argv[], const char *title)
{
	int sv[2];

	if (!argv || !argv[0] || nprocs >= (int)(sizeof(procs) / sizeof(procs[0])))
		return NULL;

	struct EmbedProc *p = calloc(1, sizeof(*p));

	if (!p)
		return NULL;
	p->fd = -1;
	p->errfd = -1;

	/*
	 * THE APPLICATION'S OWN NAME, WHICH `argv[0]` IS NOT: every generated
	 * launcher runs kdos-appbox, so an app id taken from the first word is
	 * `kdos-appbox` for Firefox and for GIMP alike. A taskbar groups its
	 * chips by app id and looks the entry up by it, so one shared name
	 * merges every boxed window into a single chip with no icon and no
	 * label.
	 *
	 * HELD ON THE PROCESS, because only the launch has an argument vector:
	 * the second, third and fourth windows are announced over a channel
	 * that carries none, and a window deriving its own would carry no
	 * name at all.
	 */
	snprintf(p->app_id, sizeof(p->app_id), "%s", guest_prog(argv));
	snprintf(p->prog, sizeof(p->prog), "%s", guest_prog(argv));
	snprintf(p->title, sizeof(p->title), "%s",
		 title && *title ? title : argv[0]);

	struct EmbedWin *e = win_new(p);

	if (!e) {
		free(p);
		return NULL;
	}

	Win *w = calloc(1, sizeof(*w));

	if (!w) {
		free(e);
		free(p);
		return NULL;
	}

	w->kind = WIN_EMBED;
	w->id = ++S.next_id;
	w->workspace = S.workspace;
	w->em = e;
	e->win = w;
	snprintf(w->title, sizeof(w->title), "%s", p->title);
	snprintf(w->app_id, sizeof(w->app_id), "%s", p->app_id);
	/* THE ENTRY ID FOR A GENERATED LAUNCHER: the same name, written once
	 * and never rewritten by the guest, which is what run-or-raise and the
	 * geometry table match on — `title` is the guest's to change. */
	snprintf(w->prog, sizeof(w->prog), "%s", p->prog);

	/* Half the workarea, placed by the window model like anything else —
	 * an embedded application is a window and is given a window's size. */
	KwmRect area = win_workarea();

	win_place(w, area.w / 2, area.h / 2);

	if (layout(e, w->geom.w, w->geom.h) != 0) {
		free(w);
		win_free_partial(e);
		free(p);
		return NULL;
	}

	/*
	 * THE SIZE IT IS FORKED WITH IS A SIZE IT HAS BEEN TOLD, and it is the
	 * first toplevel's: the cage makes one output at `--embed WxH` before
	 * any client exists and the first window the guest maps claims it. So
	 * a one-window application comes up at exactly the rectangle this
	 * placement chose, having been sent no size at all.
	 */
	e->sent_pw = e->cols * e->cell_w;
	e->sent_ph = e->rows * e->cell_h;

	if (socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, sv) != 0) {
		free(w);
		win_free_partial(e);
		free(p);
		return NULL;
	}

	/* A pipe rather than the session's own descriptor 2, so what the guest
	 * says can be read by the window it belongs to. A failure to make one
	 * is not a failure to launch: the child falls back to the session's
	 * stderr, which is where every line went before. */
	int ep[2] = { -1, -1 };

	if (pipe(ep) != 0)
		ep[0] = ep[1] = -1;
	else {
		fcntl(ep[0], F_SETFD, FD_CLOEXEC);
		fcntl(ep[0], F_SETFL, O_NONBLOCK);
		fcntl(ep[1], F_SETFD, FD_CLOEXEC);
	}

	char geom[32];

	snprintf(geom, sizeof(geom), "%dx%d", e->cols * e->cell_w,
		 e->rows * e->cell_h);

	const char *av[KCON_MAX_ARGV + 4];
	int n = 0;

	av[n++] = "kdos-cage";
	av[n++] = "--embed";
	av[n++] = geom;
	av[n++] = "--";
	for (int i = 0; argv[i] && n < (int)(sizeof(av) / sizeof(av[0])) - 1; i++)
		av[n++] = argv[i];
	av[n] = NULL;

	pid_t pid = fork();

	if (pid < 0) {
		close(sv[0]);
		close(sv[1]);
		if (ep[0] >= 0) {
			close(ep[0]);
			close(ep[1]);
		}
		free(w);
		win_free_partial(e);
		free(p);
		return NULL;
	}

	if (pid == 0) {
		int null = open("/dev/null", O_RDWR);

		/*
		 * EVERY OTHER DESCRIPTOR IS PLACED AND CLOSED BEFORE THE
		 * CHANNEL IS INSTALLED, and the order is the whole of it.
		 *
		 * /dev/null and the pipe land on the lowest numbers the session
		 * is not using, either of which can be KEMBED_FD. A channel put
		 * there first is a channel the close that follows takes away —
		 * and the guest then has the socket on its standard streams,
		 * where the first thing it writes is a malformed message to its
		 * own parent.
		 */
		if (null >= 0) {
			dup2(null, 0);
			dup2(null, 1);
		}
		if (ep[1] >= 0)
			dup2(ep[1], 2);
		if (null > 2)
			close(null);
		if (ep[0] >= 0)
			close(ep[0]);
		if (ep[1] > 2)
			close(ep[1]);
		close(sv[0]);

		/* The inherited descriptor, at the number both halves name.
		 * dup2 clears close-on-exec, which is what makes it survive. */
		if (sv[1] != KEMBED_FD) {
			dup2(sv[1], KEMBED_FD);
			close(sv[1]);
		} else {
			fcntl(sv[1], F_SETFD, 0);
		}

		/*
		 * A US KEYMAP, AND IT IS THE FALLBACK AND NOT THE ANSWER.
		 *
		 * A view that reports a real keyboard sends the compiled
		 * layout it is running, and the session hands that to every
		 * guest with KEMBED_KEYMAP — so the person types their own
		 * letters. A view with no keyboard of its own, which is every
		 * view inside somebody else's terminal, sends none: what
		 * reaches the session from one is a CHARACTER, and the
		 * synthesised path turns it back into the key that produces
		 * that character on a US keyboard. The guest has to be reading
		 * one for that half of the trip to be true, and this is what
		 * makes it so until a keymap arrives.
		 */
		setenv("XKB_DEFAULT_RULES", "evdev", 1);
		setenv("XKB_DEFAULT_MODEL", "pc105", 1);
		setenv("XKB_DEFAULT_LAYOUT", "us", 1);
		setenv("XKB_DEFAULT_VARIANT", "", 1);
		setenv("XKB_DEFAULT_OPTIONS", "", 1);

		/* The guest is a client of the compositor we are starting, not
		 * of this session and not of anything outside it. */
		unsetenv("KDOS_CON");
		unsetenv("WAYLAND_DISPLAY");
		unsetenv("DISPLAY");

		/*
		 * WHICH RENDERER COMPOSITES THIS GUEST, out of its box profile's
		 * `render` key, where `gpu` asks for the card and anything else
		 * — including an absent key — is the software renderer. A key of
		 * its own and not the profile's `gpu`: that one says the render
		 * node is bound into the box, which every box gets by default,
		 * and answering the renderer question with it would composite
		 * the whole catalogue on the card. Opting in per application is
		 * the point, because the two renderers are not interchangeable:
		 * the software one draws into memory this process can read
		 * directly and works on any machine, and the hardware one needs
		 * a render node, a driver for it and a buffer that is both a
		 * DMA-BUF and mappable — which is worth it for a game and is a
		 * larger surface to fail on for a text editor. The cage falls
		 * back to software when any of that is missing, so the key is a
		 * request and not a promise.
		 */
		char render[32];

		profile_key(guest_name(argv), "render", render, sizeof(render));
		if (!strcmp(render, "gpu"))
			setenv("KDOS_EMBED_GPU", "1", 1);

		kb_child_reset_signals();
		execvp(av[0], (char *const *)av);
		_exit(127);
	}

	close(sv[1]);
	if (ep[1] >= 0)
		close(ep[1]);
	p->pid = pid;
	p->fd = sv[0];
	p->errfd = ep[0];
	p->wins = e;
	p->nwins = 1;

	w->next = S.wins;
	S.wins = w;
	S.focus = w->id;
	procs[nprocs++] = p;
	/*
	 * THE LAYOUT THE PERSON IS TYPING ON, BEFORE THE FIRST KEY. A guest
	 * given none reads the US positions its environment named, so a key
	 * that arrived before the keymap did would be resolved under the wrong
	 * layout and the letter would be wrong. There is nothing to send when
	 * no view has reported a keyboard — every such view sends characters,
	 * which the synthesised path resolves against exactly those US
	 * positions.
	 */
	send_keymap(p);
	/*
	 * AND THE SELECTION, BEFORE THE FIRST FRAME. A guest advertises what
	 * it can paste the moment its first window takes the keyboard, so a
	 * cage that had not been told yet is an application whose Paste item
	 * is grey for as long as it takes the next pump to come round.
	 */
	send_clip(p);
	return w;
}

/*
 * A TOPLEVEL MAPPED, SO A WINDOW OPENS — AND NOTHING FORKS.
 *
 * THE FIRST ORDINARY TOPLEVEL CLAIMS THE PLACEHOLDER, which is the window this
 * launch already put on the desktop and the rectangle the geometry memory
 * already chose for it. Not the first toplevel of ANY kind: an application
 * whose splash maps first would give the splash the rectangle the person keeps
 * the document window at, and would then have nowhere to put the document.
 * Owned windows never claim it either — a dialog is a question about a window
 * that has to exist first.
 *
 * EVERY OTHER TOPLEVEL IS A WINDOW OF ITS OWN, carrying the launch's name and
 * its owner's workspace: a dialog that opened on the desk the person has
 * switched away from is a dialog nothing can answer.
 */
static void embed_adopt(struct EmbedProc *p, const KembedMsg *m,
			const char *title)
{
	struct EmbedWin *e = NULL;
	Win *owner = owner_win(p, (uint32_t)m->c);
	unsigned role = (unsigned)m->d;
	int cw, ch;

	p->opened = 1;

	/*
	 * A QUESTION OPENED ON AN ASKED WINDOW IS THE ANSWER TO THE ASK, and
	 * both deadlines for it stop here. Mapping a child of the window that
	 * was asked is something only the guest's own event loop can do, which
	 * is what separates a guest thinking from a guest ignoring; without
	 * this the person reads "Save changes?" while the countdown to SIGTERM
	 * runs and the document goes with the process that was about to save
	 * it. The window is asked again by the next click, which is what a
	 * person whose dialog led nowhere does.
	 *
	 * AND A QUESTION THAT NAMED NO PARENT STILL ANSWERS, as long as there
	 * is one ask for it to be about. An Xwayland guest may mark a dialog by
	 * type and set no transient parent, and a save prompt that loses its
	 * work over that is the same loss by another road; the role bit is what
	 * separates it from the guest simply opening another document, and a
	 * second outstanding ask would leave nothing to say which of them it
	 * answered.
	 */
	struct EmbedWin *asked = owner && owner->em && owner->em->close_ms
					 ? owner->em
					 : NULL;

	if (!asked && !m->c &&
	    (role & (KEMBED_ROLE_DIALOG | KEMBED_ROLE_MODAL)))
		asked = only_asked(p);
	if (asked) {
		asked->close_ms = 0;
		p->closing_ms = 0;
		p->sigsent = 0;
	}

	/*
	 * The placeholder is the only window that can have no id, and it is
	 * this process's first. ANY ROLE BIT AT ALL MEANS THIS IS NOT AN
	 * ORDINARY TOPLEVEL — a splash, a tool palette, a dock, a dialog that
	 * named no parent — and the placeholder carries the application's
	 * remembered rectangle, a full frame and a taskbar row. Handing those
	 * to a dock writes the dock's rectangle back to the geometry table as
	 * the rectangle the document window opens at next time.
	 */
	if (!m->c && !role)
		for (struct EmbedWin *q = p->wins; q; q = q->next)
			if (!q->id && q->win)
				e = q;

	if (e) {
		e->id = (uint32_t)m->win;
		if (title && title[0])
			snprintf(e->win->title, sizeof(e->win->title), "%s",
				 title);
		/*
		 * THE SIZE IS SETTLED BY THE MAPPING AND NOT HERE. The cage
		 * gives its first output to the first toplevel that maps and
		 * this end gives the placeholder to the first ORDINARY one, so
		 * the two may name different toplevels; take_buf() compares
		 * what arrived against the rectangle the window model gave
		 * this window and asks for the difference.
		 */
		return;
	}

	if (p->nwins >= EM_MAX_WINS) {
		fprintf(stderr, "kdos-con: '%s' has %d windows open — the one "
				"it just opened is not shown\n",
			p->prog, p->nwins);
		return;
	}

	e = win_new(p);
	if (!e)
		return;

	Win *w = calloc(1, sizeof(*w));

	if (!w) {
		free(e);
		return;
	}

	e->id = (uint32_t)m->win;
	e->win = w;
	w->kind = WIN_EMBED;
	w->id = ++S.next_id;
	w->em = e;
	w->workspace = owner ? owner->workspace : S.workspace;
	w->owner = owner ? owner->id : 0;
	w->modal = owner && (role & KEMBED_ROLE_MODAL) ? 1 : 0;
	snprintf(w->title, sizeof(w->title), "%s",
		 title && title[0] ? title : p->title);
	snprintf(w->app_id, sizeof(w->app_id), "%s", p->app_id);
	snprintf(w->prog, sizeof(w->prog), "%s", p->prog);

	/*
	 * A WINDOW THAT BELONGS TO ANOTHER OPENS WHERE THE EYE IS, AT THE SIZE
	 * IT ASKED FOR. `floating` is that rule and it is already written
	 * down: centred rather than fitted into the overlap search, at its own
	 * size, and recorded in the geometry table nowhere — which is what
	 * stops a file chooser writing its rectangle as the rectangle the
	 * application's document window opens at next time.
	 *
	 * A TOOL PALETTE AND A SPLASH ARE THE SAME KIND OF THING AND A ROW IN
	 * THE TASKBAR IS FOR NEITHER. One application is one row; a splash is
	 * not even a window a person can switch to, so it carries no frame and
	 * is out of the ring entirely.
	 *
	 * ANY ROLE AT ALL IS ENOUGH FOR `floating`, owner or no owner. It is
	 * also what keeps a window out of the geometry table, and every window
	 * of one guest carries the same program name — so a file chooser that
	 * was remembered would write its rectangle as the rectangle the
	 * document window opens at next time, to disk, for every session after
	 * this one.
	 *
	 * A ROW IS WITHHELD ONLY FROM A WINDOW SOMETHING ELSE ANSWERS FOR. An
	 * owner that did not resolve — the owning window went while this
	 * KEMBED_OPEN was in flight — leaves a dialog nothing owns, and
	 * win_drop()'s repair only reaches windows that carry the id of the
	 * window that went. So the flag is taken from the RESOLVED owner, and
	 * a question with nobody to stand behind gets its own chip: a window
	 * with no row and no owner is one the taskbar cannot reach at all.
	 */
	if (owner || role)
		w->floating = 1;
	if (owner || (role & (KEMBED_ROLE_UTILITY | KEMBED_ROLE_SPLASH)))
		w->no_task = 1;
	if (role & KEMBED_ROLE_SPLASH)
		w->overlay = 1;

	/*
	 * THE SIZE THE GUEST CHOSE, IN CELLS, ROUNDED UP: a window one cell
	 * short of the pixels a dialog asked for is a dialog with its last row
	 * of buttons cut off. A guest that named no size gets half the work
	 * area, which is what a window with nothing to say about itself gets
	 * anywhere on this desktop.
	 */
	cell_size(&cw, &ch);
	KwmRect area = win_workarea();
	int want_w = m->a > 0 ? (m->a + cw - 1) / cw : area.w / 2;
	int want_h = m->b > 0 ? (m->b + ch - 1) / ch : area.h / 2;

	if (want_w > area.w - 2 * CON_FRAME)
		want_w = area.w - 2 * CON_FRAME;
	if (want_h > area.h - 2 * CON_FRAME)
		want_h = area.h - 2 * CON_FRAME;
	if (want_w < 1)
		want_w = 1;
	if (want_h < 1)
		want_h = 1;

	win_place(w, want_w, want_h);

	if (layout(e, w->geom.w, w->geom.h) != 0) {
		free(w);
		win_free_partial(e);
		return;
	}

	e->next = p->wins;
	p->wins = e;
	p->nwins++;

	/*
	 * IN FRONT, AND IT TAKES THE KEYBOARD ONLY IF THE PERSON IS ALREADY IN
	 * THIS APPLICATION.
	 *
	 * A window that opened behind the window it belongs to is one a person
	 * cannot find, so a new window goes to the front of the stack — and
	 * win_raise carries whatever it owns with it. But the keyboard is a
	 * different question: a toolbox or a reminder from an application
	 * nobody is looking at must not take the keys from the terminal
	 * somebody is typing in. So the focus moves only when the window that
	 * has it belongs to this same guest, or when nothing has it at all.
	 *
	 * A SPLASH NEVER TAKES IT. It is out of the ring and out of the
	 * taskbar; a splash a person can be left typing into is one they
	 * cannot get out of.
	 */
	Win *f = win_find(S.focus);
	int mine = !f || (f->kind == WIN_EMBED && f->em && f->em->proc == p);

	w->next = S.wins;
	S.wins = w;
	/*
	 * AND THE RAISE IS AIMED AT THE FAMILY, NOT AT THE NEW WINDOW. A
	 * second dialog mapped while a modal is already up would otherwise be
	 * raised on its own and land above the question — win_raise() walks up
	 * to the head of the family and brings the whole of it forward with
	 * the modal last, and it puts the focus on the question rather than on
	 * the window that just opened behind it.
	 */
	if (!w->overlay && mine)
		win_raise(owner ? owner->id : w->id);

	/*
	 * AND THE SIZE IT WAS PLACED AT GOES BACK AT ONCE. Its output is
	 * whatever natural size the guest chose; the rectangle the window
	 * model gave it is what it has to render into, and until that is sent
	 * the two disagree by however much the clamp above moved it.
	 */
	e->want_pw = e->cols * e->cell_w;
	e->want_ph = e->rows * e->cell_h;
	size_flush(e);
	damage_all(e);
}

/* ── the selection ───────────────────────────────────────────────────── */

/*
 * THE SESSION'S SELECTION, ON EVERY GUEST'S OWN SEAT.
 *
 * Each kdos-cage is a compositor with a seat of its own, so a copy left where
 * a guest put it is one clipboard per application: a copy in the browser the
 * terminal beside it cannot paste, and a copy in the terminal the browser
 * cannot see. Both halves cross this channel — a guest's copy arrives as
 * KEMBED_CLIP_OFFER and becomes the session's, and what the session holds is
 * pushed out as KEMBED_CLIP_SET and installed on the cage's seat.
 *
 * IT IS MIRRORED CONTINUOUSLY, NOT FETCHED AT THE PASTE. A guest reads the
 * clipboard through the offer it was sent on focus, so a cage told only when
 * somebody pressed Ctrl+V would have nothing to advertise at the moment the
 * guest decides whether its Paste item is usable at all — and a paste that
 * waits on a round trip through this process is a paste that can hang the
 * window doing it.
 *
 * PER CHANNEL AND NOT PER WINDOW. The seat is the process's, so every window
 * of one guest pastes the same bytes and the mirror is sent once for all of
 * them.
 */
static int clip_fd[2] = { -1, -1 };	/* [0] clipboard, [1] primary */
static size_t clip_fd_len[2];
static unsigned long clip_fd_gen;	/* what those descriptors are of */

/*
 * ONE SEALED DESCRIPTOR PER SELECTION, MADE ONCE PER CHANGE AND SHARED BY
 * EVERY GUEST.
 *
 * SEALED BEFORE IT CROSSES, because the far end maps it at the length it was
 * told: a descriptor that could still be shrunk is one whose reader can be
 * faulted after the check, and this end has no reason to write to it again.
 * That is also what lets one descriptor answer every channel — it cannot
 * change, so two guests reading it cannot see different bytes.
 */
static int clip_bytes(const char *text, size_t len)
{
	int fd = memfd_create("kdos-clip", MFD_CLOEXEC | MFD_ALLOW_SEALING);

	if (fd < 0)
		return -1;
	for (size_t off = 0; off < len;) {
		ssize_t n = write(fd, text + off, len - off);

		if (n > 0) {
			off += (size_t)n;
			continue;
		}
		if (n < 0 && errno == EINTR)
			continue;
		close(fd);
		return -1;
	}
	if (fcntl(fd, F_ADD_SEALS,
		  F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_WRITE) != 0) {
		close(fd);
		return -1;
	}
	return fd;
}

/*
 * BOTH SELECTIONS OR NEITHER, and the old pair is kept until the new one is
 * whole. A length of zero with no descriptor is what CLEARS the copy a cage
 * holds, so a half-built pair installed after a failure would empty a guest's
 * clipboard because this process could not open a file.
 */
static int clip_seal(unsigned long gen)
{
	int fd[2] = { -1, -1 };
	size_t len[2] = { 0, 0 };

	for (int i = 0; i < 2; i++) {
		size_t n = 0;
		const char *text = clip_get(i, &n);

		if (!text || !n)
			continue;
		fd[i] = clip_bytes(text, n);
		if (fd[i] < 0) {
			if (fd[0] >= 0)
				close(fd[0]);
			return -1;
		}
		len[i] = n;
	}

	for (int i = 0; i < 2; i++) {
		if (clip_fd[i] >= 0)
			close(clip_fd[i]);
		clip_fd[i] = fd[i];
		clip_fd_len[i] = len[i];
	}
	clip_fd_gen = gen;
	return 0;
}

/*
 * THIS GUEST IS BROUGHT LEVEL WITH THE SESSION, and only when it is behind.
 *
 * The generation is compared rather than the bytes: a selection is up to
 * KEMBED_CLIP_MAX and this runs once per channel per pump. A guest that has
 * just offered its own copy is one generation behind by construction, so the
 * mirror hands that copy straight back — the cage compares what it last
 * offered and does nothing with it, which is what keeps a copy from cancelling
 * itself a moment after it was made.
 */
static void send_clip(struct EmbedProc *p)
{
	unsigned long gen = clip_gen();

	if (!p || p->fd < 0 || p->clip_gen == gen)
		return;
	if (clip_fd_gen != gen && clip_seal(gen) != 0)
		return;		/* what the guest holds stands */

	for (int i = 0; i < 2; i++) {
		int r;

		if (clip_fd[i] < 0)
			r = proc_send(p, KEMBED_CLIP_SET, i, 0, 0, 0, 0, 0);
		else
			r = proc_send_fd(p, KEMBED_CLIP_SET, i,
					 (int)clip_fd_len[i], 0, 0, 0,
					 clip_fd[i]);
		if (r != 0)
			return;		/* the channel is going; try again */
	}
	p->clip_gen = gen;
}

/*
 * A GUEST COPIED, AND THE BYTES ARE MEASURED BEFORE THEY ARE MAPPED.
 *
 * THE LENGTH IN THE MESSAGE IS A CLAIM AND THE FILE IS THE FACT. A mapping
 * longer than the file faults on the byte past its end, and a file that can
 * still SHRINK faults after the check — so the seal is required as well as the
 * size. Anything that is not a plain file of the length it promised is
 * dropped: the descriptor came from another process, and the only thing this
 * end knows about it is what it can measure.
 *
 * `fd` STAYS THE CALLER'S. drain() closes every descriptor it received, so a
 * close here would land on whatever number was opened next.
 */
static void take_clip(const KembedMsg *m, int fd)
{
	struct stat st;
	int seals;
	void *map;
	size_t len;

	/* A CLEARED SELECTION IS NOT FORWARDED. A guest's source dies with the
	 * program that made it, and a session whose clipboard emptied because
	 * a window closed is one that threw away the copy a person made. */
	if (fd < 0 || m->b <= 0 || (size_t)m->b > KEMBED_CLIP_MAX)
		return;
	len = (size_t)m->b;

	if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) ||
	    (size_t)st.st_size < len)
		return;
	seals = fcntl(fd, F_GET_SEALS);
	if (seals < 0 || !(seals & F_SEAL_SHRINK))
		return;

	map = mmap(NULL, len, PROT_READ, MAP_PRIVATE, fd, 0);
	if (map == MAP_FAILED)
		return;
	clip_put(map, len, m->a != 0);
	munmap(map, len);
}

/* ── messages from the child ─────────────────────────────────────────── */

static void take_buf(struct EmbedWin *e, int fd, int w, int h, size_t stride,
		     size_t slot_len)
{
	if (fd < 0)
		return;
	if (w <= 0 || h <= 0 || stride < (size_t)w * 4 ||
	    slot_len < stride * (size_t)h) {
		close(fd);
		return;
	}

	size_t total = slot_len * KEMBED_SLOTS;
	void *map = mmap(NULL, total, PROT_READ, MAP_SHARED, fd, 0);

	close(fd);
	if (map == MAP_FAILED)
		return;

	if (e->map)
		munmap(e->map, e->map_len);
	e->map = map;
	e->map_len = total;
	e->slot_len = slot_len;
	e->stride = stride;
	e->pw = w;
	e->ph = h;
	/*
	 * THE MAPPING THAT CAME BACK IS THE SIZE THE GUEST IS, so it ends the
	 * wait whatever size it is. The cage remaps only when it publishes,
	 * and it drops a mode equal to the one it last mapped, so a size it
	 * skipped never arrives here: holding the gate open for pixels that
	 * are never coming leaves the window one full EM_SIZE_LATE_MS behind
	 * every later resize, which is a drag whose picture steps once a
	 * second. A want that still disagrees goes out on the flush below.
	 */
	e->sent_pw = w;
	e->sent_ph = h;
	/*
	 * AND A MAPPING THAT IS NOT THIS WINDOW'S RECTANGLE IS ANSWERED WITH
	 * THE RECTANGLE, ONCE.
	 *
	 * The two ends pick the first toplevel by different rules: the cage
	 * gives its first output to the first one that maps and this end gives
	 * the placeholder to the first ORDINARY one, so a guest whose splash
	 * maps first hands the document window an output of its own natural
	 * size while the window model frames it at the placeholder's
	 * remembered rectangle. Nothing else would ever close that gap —
	 * `want` is clear, so the flush below has nothing to send — and the
	 * window is drawn with a permanent blank margin or is silently
	 * cropped, with no error anywhere.
	 *
	 * ONCE PER RECTANGLE, which is what `corr` records. A guest that is
	 * told a rectangle and maps something else has answered — asking again
	 * on every mapping would be a size per frame for the life of the
	 * window — while a window the model later moves or resizes is a new
	 * rectangle and is asserted again.
	 */
	if (!e->want_pw && !e->want_ph &&
	    (w != e->cols * e->cell_w || h != e->rows * e->cell_h) &&
	    (e->corr_pw != e->cols * e->cell_w ||
	     e->corr_ph != e->rows * e->cell_h)) {
		e->corr_pw = e->want_pw = e->cols * e->cell_w;
		e->corr_ph = e->want_ph = e->rows * e->cell_h;
	}
	/* THE HALVES THIS NAMES ARE IN A MAPPING THAT NO LONGER EXISTS, so
	 * neither the walk's nor the newest may be read until the guest
	 * announces a frame in the new one. */
	e->latest = -1;
	e->pub_slot = -1;
	damage_all(e);
	size_flush(e);
}

/*
 * EVERYTHING THE CAGE HAS TO SAY, ROUTED BY WHICH WINDOW IT IS ABOUT.
 *
 * TWO KINDS OF MESSAGE AND THE FIELD SAYS WHICH. `win` 0 is the channel —
 * HELLO and GONE are about the process and about nothing narrower — and every
 * other op is about one toplevel and is answered on that toplevel alone. A
 * title, a fullscreen request, a mapping and a frame are the window's, so an
 * export dialog cannot rename the image window it opened over or take the
 * screen away from it.
 *
 * A WINDOW THIS END DOES NOT HAVE IS DROPPED AND ITS DESCRIPTOR IS CLOSED. A
 * window can go while a message about it is still in flight — the person
 * clicked the frame's X, the cage had already sent the next frame — and a
 * descriptor left open is one leaked per frame until the session runs out.
 */
static void drain(struct EmbedProc *p)
{
	for (;;) {
		KembedMsg m = { 0 };
		char text[KEMBED_TITLE_MAX];
		int fd = -1;
		int r = recv_msg(p, &m, &fd, text, sizeof(text));
		struct EmbedWin *e;

		if (r == 0)
			return;
		if (r < 0) {
			p->gone = 1;
			return;
		}

		/* The ops the channel answers for itself. */
		if (m.op == KEMBED_HELLO) {
			if (fd >= 0)
				close(fd);
			continue;
		}
		if (m.op == KEMBED_GONE) {
			if (fd >= 0)
				close(fd);
			p->gone = 1;
			continue;
		}
		/*
		 * THE SELECTION IS THE CHANNEL'S AND NOT A WINDOW'S: one cage
		 * is one seat, so a copy made in a dock and a copy made in the
		 * image window are the same clipboard and arrive under `win` 0.
		 */
		if (m.op == KEMBED_CLIP_OFFER) {
			take_clip(&m, fd);
			if (fd >= 0)
				close(fd);
			continue;
		}
		if (m.op == KEMBED_OPEN) {
			if (fd >= 0)
				close(fd);
			/*
			 * A TOPLEVEL WITH NO NAME IS NO TOPLEVEL, and one
			 * announced twice is a cage out of step: either would
			 * put a window on the desktop that nothing can address
			 * and nothing can close.
			 */
			if (!m.win || win_of(p, m.win))
				continue;
			sanitise_title(text);
			embed_adopt(p, &m, text);
			continue;
		}

		e = win_of(p, m.win);
		if (!e) {
			if (fd >= 0)
				close(fd);
			continue;
		}

		switch (m.op) {
		case KEMBED_CLOSE_WIN:
			if (fd >= 0)
				close(fd);
			/*
			 * THE TOPLEVEL IS ALREADY GONE, so the window is taken
			 * out rather than asked. win_close() would route back
			 * through embed_close() and ask a toplevel that no
			 * longer exists, and the window would stand on the
			 * desktop waiting for an answer nothing can send.
			 *
			 * AND THE PROCESS IS NOT TOUCHED. An application whose
			 * last window closed is an application with no window,
			 * which is what a session bus name is for: the next
			 * launch hands off into it and opens a window there.
			 *
			 * THE GUEST ANSWERED, SO THE CLOCK IS DISARMED. It
			 * measures a guest that was asked and did nothing, and
			 * a guest that retires the window it was asked about
			 * has done the thing it was asked for — one left
			 * running would otherwise be signalled and then killed
			 * ten seconds after honouring a close, which is
			 * precisely the shared-instance case the handoff
			 * depends on.
			 */
			p->closing_ms = 0;
			p->sigsent = 0;
			if (e->win)
				win_drop(e->win);
			break;
		case KEMBED_BUF:
			take_buf(e, fd, m.a, m.b, (size_t)m.c, (size_t)m.d);
			break;
		case KEMBED_FRAME:
			if (fd >= 0)
				close(fd);
			if (m.a < 0 || m.a >= KEMBED_SLOTS || !e->map)
				break;
			e->latest = m.a;
			em_stat.frames++;
			if (!e->drew) {
				e->drew = 1;
				damage_all(e);
			}
			damage_add(e, m.b, m.c, m.d, (int)m.e);
			break;
		case KEMBED_DAMAGE:
			/*
			 * Another box of the frame the last KEMBED_FRAME
			 * announced. It names no slot, so one arriving before
			 * any frame has is damage about a picture that does
			 * not exist yet and is dropped rather than rounded out
			 * against a grid that has none.
			 */
			if (fd >= 0)
				close(fd);
			if (!e->drew)
				break;
			damage_add(e, m.b, m.c, m.d, (int)m.e);
			break;
		case KEMBED_TITLE:
			/*
			 * THE NAME THE GUEST CHOSE, which is the document and
			 * not the launcher, and it is THIS WINDOW'S: the
			 * toplevel that spoke named itself, so an export
			 * dialog cannot rename the image window it opened
			 * over. The frame and the taskbar entry are drawn from
			 * `title`, and mgmt_publish re-announces a window
			 * whose title changed, so the panel follows from here
			 * with nothing else to call.
			 */
			if (fd >= 0)
				close(fd);
			sanitise_title(text);
			if (e->win && text[0])
				snprintf(e->win->title, sizeof(e->win->title),
					 "%s", text);
			break;
		case KEMBED_FULLSCREEN:
			/*
			 * THE GUEST ASKED FOR THE SCREEN. Its output is this
			 * window, so the request is only honoured by the
			 * window going fullscreen — a video that asked and got
			 * the rectangle it already had is the whole defect.
			 * win_fullscreen() is the toggle the chord uses, so
			 * the state is compared before it is called and a
			 * repeated request costs nothing.
			 */
			if (fd >= 0)
				close(fd);
			if (e->win && (m.a != 0) != (e->win->full != 0))
				win_fullscreen(e->win);
			break;
		case KEMBED_INHIBIT:
			/*
			 * THE GUEST IS PLAYING SOMETHING. An idle timer that
			 * blanked the screen under a film is the whole reason
			 * this op exists; the flag is read by the session's
			 * idle pass, which ignores it for a window nobody can
			 * see. It is cleared with the window, because a guest
			 * that sets an inhibitor and exits without clearing it
			 * would otherwise hold the screen on for the session.
			 */
			if (fd >= 0)
				close(fd);
			e->inhibit = m.a != 0;
			break;
		case KEMBED_GRAB:
			/*
			 * THE GUEST TOOK THE POINTER, or gave it back. A game
			 * and a three-dimensional editor read motion as a
			 * delta and want the cursor to stop moving; while this
			 * is held the session routes every pointer event to
			 * this window, hovers nothing, raises nothing and
			 * sends the delta alone.
			 *
			 * A VALUE THIS PROTOCOL DOES NOT NAME IS NO GRAB. The
			 * alternative is a pointer captured by a number
			 * neither end agrees the meaning of, which is a
			 * desktop that has stopped answering the mouse.
			 */
			if (fd >= 0)
				close(fd);
			if (m.a == KEMBED_GRAB_LOCKED ||
			    m.a == KEMBED_GRAB_CONFINED)
				e->grab = m.a;
			else
				e->grab = KEMBED_GRAB_NONE;
			break;
		default:
			if (fd >= 0)
				close(fd);
			break;
		}
	}
}

/* ── what the guest said ─────────────────────────────────────────────── */

/*
 * EVERY LINE TO THE SESSION'S LOG, THE LAST ONE KEPT.
 *
 * The log is where a whole failure is read afterwards; `last` is one sentence,
 * and it is the one shown to somebody whose window has just closed itself. A
 * chunk may carry several lines or half of one, so what is kept is the last
 * run of text in it that is not blank.
 */
static void drain_err(struct EmbedProc *e)
{
	char buf[513];
	ssize_t n;

	if (e->errfd < 0)
		return;

	for (;;) {
		n = read(e->errfd, buf, sizeof(buf) - 1);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			if (errno != EAGAIN && errno != EWOULDBLOCK) {
				close(e->errfd);
				e->errfd = -1;
			}
			return;
		}
		if (n == 0) {
			close(e->errfd);
			e->errfd = -1;
			return;
		}

		buf[n] = '\0';
		fputs(buf, stderr);

		for (ssize_t i = 0; i < n; i++)
			if (buf[i] == '\n' || buf[i] == '\r' || buf[i] == '\t')
				buf[i] = '\0';
		for (ssize_t i = n - 1; i >= 0; i--) {
			ssize_t s = i;

			if (!buf[i])
				continue;
			while (s > 0 && buf[s - 1])
				s--;
			snprintf(e->last, sizeof(e->last), "%s", buf + s);
			break;
		}
	}
}

/* ── the session's side of the loop ──────────────────────────────────── */

int embed_fds(int *fds, int max)
{
	int n = 0;

	for (int i = 0; i < nprocs && n < max; i++) {
		if (procs[i]->fd >= 0 && n < max)
			fds[n++] = procs[i]->fd;
		if (procs[i]->errfd >= 0 && n < max)
			fds[n++] = procs[i]->errfd;
	}
	return n;
}

/*
 * AND IT HAS TO BE ASKABLE FROM THE CONFIGURATION, not only the environment.
 * The session is started by init on a tty nobody has logged into yet, so there
 * is no shell in which to export a variable — a measurement only reachable by
 * `VAR=1 kdos-con` is one nobody can take on the machine that has the problem.
 * The variable still works, for a session started by hand.
 */
static int stat_wanted(void)
{
	static int cached = -1;

	if (cached < 0)
		cached = getenv("KDOS_EMBED_STAT") != NULL ||
			 kcon_conf_bool("embed_stat", 0);
	return cached;
}

static void stat_tick(void)
{
	unsigned long long t;

	if (!nprocs || !stat_wanted())
		return;
	t = now_ms();
	if (!em_stat.when) {
		em_stat.when = t;
		return;
	}
	if (t - em_stat.when < 1000)
		return;
	struct EmbedWin *ws[EM_WIN_CAP];

	fprintf(stderr,
		"embed-stat: %lu guest frames, %lu blocks (%llu kB), "
		"%lu refused, over %llu ms, %d window(s) in %d guest(s)\n",
		em_stat.frames, em_stat.blocks, em_stat.bytes >> 10,
		em_stat.refused, t - em_stat.when,
		all_wins(ws, EM_WIN_CAP), nprocs);
	em_stat.frames = em_stat.blocks = em_stat.refused = 0;
	em_stat.bytes = 0;
	em_stat.when = t;
}

/*
 * WHERE A TURN STARTS SPENDING THE DISPLAY QUEUE, AND IT ROTATES OVER WINDOWS.
 *
 * A turn's room is finite and the window served first takes it, so a fixed
 * order lets one window lock another out for as long as both are drawing —
 * and over PROCESSES rather than windows it lets one application with five
 * windows spend the whole turn before the panel's guest is reached even once.
 * The focused window is the one somebody is looking at and goes first; with no
 * embedded window focused the start rotates, so every window reaches the front
 * within as many turns as there are windows.
 */
static int pump_start(struct EmbedWin **ws, int n)
{
	static unsigned rr;

	if (n < 1)
		return 0;
	for (int i = 0; i < n; i++)
		if (ws[i]->win && ws[i]->win->id == S.focus)
			return i;
	return (int)(rr++ % (unsigned)n);
}

/*
 * THE KEYBOARD MOVES IN TWO PASSES, AND THE ORDER IS THE WHOLE OF IT.
 *
 * KEMBED_FOCUS with a=0 takes the keyboard from whatever holds it on that
 * channel, so a gain sent before the loss it replaces is a gain the loss
 * immediately undoes — which for two windows of one guest is a dialog that
 * takes the focus and loses it in the same turn, for ever. Every window that
 * is losing it is told first, then every window that is gaining it.
 *
 * AND EVERY KEY GOES BEFORE THE KEYBOARD DOES. The release for a key still
 * down arrives at whatever has the focus next, so a window that lost it
 * mid-chord would hold that key for the life of the guest — Ctrl stuck down in
 * an editor, W stuck down in a game.
 */
static void focus_pass(struct EmbedWin **ws, int n)
{
	for (int i = 0; i < n; i++) {
		struct EmbedWin *e = ws[i];

		if (!e->focused || (e->win->id == S.focus && !S.locked))
			continue;
		e->focused = 0;
		release_held(e);
		/*
		 * AND THE POINTER GOES WITH IT. A constraint is only active
		 * for the surface that has the keyboard, so the cage drops the
		 * grab as focus clears and says so; the flag is cleared here
		 * as well because a session that waited for that message would
		 * route every pointer event to a window nobody is using until
		 * it arrived.
		 */
		e->grab = KEMBED_GRAB_NONE;
		send_msg(e, KEMBED_FOCUS, 0, 0, 0, 0, 0);
	}

	for (int i = 0; i < n; i++) {
		struct EmbedWin *e = ws[i];

		if (e->focused || e->win->id != S.focus || S.locked)
			continue;
		/*
		 * THE LOCKS AND THE LAYOUT GROUP, WHICH NO KEY CAN ESTABLISH.
		 * Caps Lock set before this guest existed is in no key stream
		 * it will ever receive. Sent here, on a keymap change and
		 * behind a key that moved either of them — and nowhere else,
		 * because a mask applied beside the key stream disagrees with
		 * the keys the guest believes are down and the next key then
		 * resolves under the wrong one. A session with no mask of its
		 * own sends none: see km_mods_valid.
		 */
		send_mods(e->proc);
		/*
		 * AND THE KEYBOARD IS THIS WINDOW'S ONLY ONCE IT HAS BEEN
		 * TOLD. A window the guest has not announced has no name on
		 * the wire and cannot be given anything; latching it as
		 * focused would mean the toplevel that claims it is never
		 * handed the keyboard at all.
		 */
		if (send_msg(e, KEMBED_FOCUS, 1, 0, 0, 0, 0) == 0)
			e->focused = 1;
	}
}

void embed_pump(void)
{
	struct EmbedWin *ws[EM_WIN_CAP];
	int n, start;

	stat_tick();

	/* EVERY CHANNEL IS DRAINED BEFORE ANY WINDOW IS SERVED, because a
	 * datagram may open a window, close one or move a mapping, and a walk
	 * begun over the list as it was is a walk over windows some of which
	 * no longer exist. */
	for (int i = 0; i < nprocs; i++) {
		if (procs[i]->fd >= 0)
			drain(procs[i]);
		drain_err(procs[i]);
		/*
		 * AND THE SELECTION GOES OUT AFTER THE DRAIN, so a copy this
		 * turn brought in from one guest reaches the others in the
		 * same turn rather than a pump later. It costs a generation
		 * compare per channel and sends nothing while nobody has
		 * copied.
		 */
		if (!procs[i]->gone)
			send_clip(procs[i]);
	}

	n = all_wins(ws, EM_WIN_CAP);
	focus_pass(ws, n);
	start = pump_start(ws, n);

	for (int k = 0; k < n; k++) {
		struct EmbedWin *e = ws[(start + k) % n];
		Win *w = e->win;

		/*
		 * THE GUEST IS TOLD WHAT THE WINDOW MODEL DECIDED. Sleep so an
		 * application nobody can see stops rendering frames nobody
		 * composites, which on a battery is the whole difference
		 * between a window and a wasted process — and with a window
		 * per toplevel it is also the whole difference between an
		 * application with four docks and one: a dock nobody is
		 * looking at has its own output and produces no frames at all.
		 *
		 * NOBODY CAN SEE IT IS MORE THAN MINIMISED — another
		 * workspace, hidden, the lock, the saver — and win_is_on_screen
		 * is the one place that list is written, because a window this
		 * side kept awake is one whose blocks are cut, copied and sent
		 * down the same queue the window being looked at is waiting
		 * for.
		 */
		int asleep = !win_is_on_screen(w);
		int full = w->full != 0;

		/*
		 * AND WHETHER IT IS FULLSCREEN, which is the window model's
		 * answer and not the guest's request. `Super+f` and the frame's
		 * own button put a window on the whole screen without the guest
		 * asking for it, and a guest that is never told draws the
		 * chrome of a windowed application across a screen that has
		 * none.
		 */
		if (full != e->full_sent &&
		    send_msg(e, KEMBED_FULLSCREEN_SET, full, 0, 0, 0, 0) == 0)
			e->full_sent = full;
		if (asleep != e->asleep) {
			e->asleep = asleep;
			if (!asleep)
				damage_all(e);
		}
		if (e->asleep != e->sleep_told &&
		    send_msg(e, KEMBED_SLEEP, e->asleep, 0, 0, 0, 0) == 0)
			e->sleep_told = e->asleep;

		/* A SIZE HELD BACK BY EITHER GATE GOES FROM HERE. The gesture
		 * that asked for it may have stopped, and a size nothing
		 * flushes is a guest left at the size before the last one. */
		size_flush(e);
		publish(e);
	}
}

/*
 * The window changed size in cells, or a cell changed size in pixels. A resize
 * IS an output resize on the far side, which is how the application
 * reconfigures the way it would on any compositor.
 *
 * THE GUEST'S OUTPUT IS MEASURED IN PIXELS, so a font step moves it while the
 * window keeps its rectangle: the cell size is read before the guard and the
 * guard tests both. A guard on the cell count alone leaves such a guest
 * rendering at the old pixels-per-cell for the rest of its life, cutting every
 * block from a size nothing on the screen has any more — and because
 * send_block() derives its pixels from the same stale cell, there is no error
 * anywhere, only a picture at the wrong resolution.
 *
 * The grid and the owed set move at once because they are local and cheap;
 * only the message to the guest is gated, so a live drag stays live.
 */
void embed_resized(Win *w)
{
	struct EmbedWin *e = w ? w->em : NULL;
	int cw, chh;

	if (!e)
		return;

	cell_size(&cw, &chh);
	if (w->geom.w == e->cols && w->geom.h == e->rows &&
	    cw == e->cell_w && chh == e->cell_h)
		return;

	e->cell_w = cw;
	e->cell_h = chh;

	if (layout(e, w->geom.w, w->geom.h) != 0)
		return;
	e->want_pw = e->cols * e->cell_w;
	e->want_ph = e->rows * e->cell_h;
	size_flush(e);
	damage_all(e);
}

/*
 * A VIEW ATTACHED, so every block has to go out again: a view holds the
 * pictures it was sent and a new one was sent none.
 */
/*
 * A DISPLAY COULD NOT KEEP A PICTURE, so the block is owed to that display and
 * to nobody else. kcon_view_sprite() answers for the wire: the bytes crossed,
 * the bit was cleared, and without this the block is a hole in the window for
 * as long as the window lives because nothing re-sends a picture nobody knows
 * was lost.
 *
 * THE REPAIR IS BOUNDED BY THE WINDOW, NOT BY TIME. A display whose sprite
 * table is simply too small for this window refuses the replacement exactly as
 * it refused the picture, and an unbounded re-owe is then the same bytes for
 * ever — the hole, plus the queue the rest of the desktop needs. One window's
 * worth of blocks per display is paid between one guest damage and the next:
 * a display that lost a few pictures has them all back on the next walk, and
 * one that can keep none of them stops being paid until the guest draws again,
 * by which time the ordinary damage path is sending those blocks anyway.
 *
 * THE VIEW'S POSITION IS ONLY THIS PUMP'S. It is the bit position `dirty`
 * uses, and a list that changes length owes the whole window again — which is
 * what makes a position taken here safe to write and never safe to keep.
 */
void embed_sprite_lost(KconSurface *v, int slot)
{
	int n = kcon_server_view_count(S.server);
	int vi = -1;

	if (n > 32)
		n = 32;
	for (int i = 0; i < n; i++)
		if (kcon_server_view_at(S.server, i) == v) {
			vi = i;
			break;
		}
	if (vi < 0)
		return;

	struct EmbedWin *ws[EM_WIN_CAP];
	int nw = all_wins(ws, EM_WIN_CAP);

	for (int k = 0; k < nw; k++) {
		struct EmbedWin *e = ws[k];
		int nb = e->bw * e->bh;

		if (nb > EM_MAX_BLOCKS)
			nb = EM_MAX_BLOCKS;
		for (int i = 0; i < nb; i++) {
			if (e->slots[i] != slot)
				continue;
			if (e->repair[vi] >= nb)
				return;
			e->repair[vi]++;
			if (!e->dirty[i])
				e->ndirty++;
			e->dirty[i] |= 1u << vi;
			return;
		}
	}
}

void embed_view_attached(void)
{
	struct EmbedWin *ws[EM_WIN_CAP];
	int n = all_wins(ws, EM_WIN_CAP);

	for (int i = 0; i < n; i++) {
		damage_all(ws[i]);
		memset(ws[i]->slow_ms, 0, sizeof(ws[i]->slow_ms));
	}
}

/*
 * THE GUEST IS ASKED FIRST, AND ONLY THEN MADE TO GO.
 *
 * A signal to the cage takes the application down with it, so an unsaved
 * document is lost to the close button with no dialog and no way back —
 * whereas an ask is the request the application already handles, and the
 * window stays until it has actually gone. The escalation exists because an
 * ask alone leaves a window nothing can close when the guest ignores it, and
 * it is measured from the first ask: clicking close again does not restart the
 * clock, which would make a person's impatience the reason it never fires.
 *
 * THE ASK NAMES ONE WINDOW AND THE SIGNAL NAMES THE PROCESS, which is why the
 * process clock is armed only when the ask covers the last window a person can
 * reach. A person closing one dialog of five is not asking the application to
 * quit, and a process signalled for that dialog's silence loses every other
 * window's unsaved work.
 *
 * AND A GUEST THAT ANSWERS IS ON NO CLOCK AT ALL. Both deadlines measure
 * silence, and a guest that maps a question on the window it was asked about
 * has broken it: embed_adopt() stops them there. The evidence has to be
 * something only the guest's own event loop can produce — a frame is not,
 * since the cage composites its scene whether or not its client is still
 * reading the socket — or the countdown runs while the person reads "Save
 * changes?" and the answer they give is delivered to a process already killed.
 *
 * SO EVERY WINDOW CARRIES A DEADLINE OF ITS OWN as well, and it is the only
 * thing that can remove one from the desktop when the guest answers nothing:
 * `nwins` never falls for a guest that ignores every ask, so a process clock
 * alone would never arm and a modal that ignores its close also blocks its
 * owner's — leaving two windows nothing can shift and no route to the guest at
 * all. embed_reap() drops such a window locally; the toplevel behind it is
 * beyond this desktop's reach either way, and the drop of the LAST one is
 * where embed_reap() arms the process clock, so the guest left with nothing on
 * the desktop is still signalled and killed rather than running on unreachable
 * for the life of the session.
 *
 * AND ASKING THE OWNER OF A MODAL ASKS NOTHING. An application that has put a
 * question on the screen is not answering about the window behind it, and a
 * close routed there would be a close nothing ever replies to.
 */
void embed_close(Win *w)
{
	struct EmbedWin *e = w ? w->em : NULL;

	if (!e || win_modal_for(w->id))
		return;
	/*
	 * A PLACEHOLDER IS ASKED FOR AS LONG AS IT IS THE ONLY HANDLE ON THE
	 * GUEST. Its ask goes out as zero, which on this op means every
	 * toplevel the guest has — the right ask while the guest is starting
	 * up and has none, and the right ask while everything it has opened is
	 * a splash, because a splash is not a window a person can close.
	 *
	 * IT IS DROPPED INSTEAD ONCE THE GUEST HAS A WINDOW OF ITS OWN TO
	 * CLOSE. The placeholder is then a leftover no toplevel claimed, and
	 * asking the application to quit because somebody dismissed it would
	 * take the windows it did open with it.
	 */
	if (!e->id) {
		for (struct EmbedWin *q = e->proc->wins; q; q = q->next)
			if (q->id && q->win && !q->win->overlay) {
				win_drop(w);
				return;
			}
	}
	proc_send(e->proc, KEMBED_CLOSE, 0, 0, 0, 0, 0, e->id);
	if (!e->close_ms)
		e->close_ms = now_ms();
	if (nreachable(e->proc) <= 1 && !e->proc->closing_ms)
		e->proc->closing_ms = now_ms();
}

/*
 * THE SESSION ITSELF IS GOING, so this is the signal and not the ask: there is
 * no screen left to draw a save dialog on and nothing left to answer it with.
 */
void embed_close_all(void)
{
	for (int i = 0; i < nprocs; i++)
		if (procs[i]->pid > 0)
			kill(procs[i]->pid, SIGTERM);
}

/*
 * WHAT IS SAID WHEN A GUEST GOES WITHOUT EVER HAVING OPENED A WINDOW.
 *
 * The guest's own last line first: it names the step that failed, which is the
 * only thing that tells a pack that will not mount apart from a box that will
 * not compose. Failing that, the exit status, which at least separates a
 * program that is not on the machine (127) from one that ran and refused.
 */
static void say_stillborn(const struct EmbedProc *p)
{
	const char *name = p->prog[0]  ? p->prog
			   : p->title[0] ? p->title
					 : "the application";
	char body[256];

	if (p->last[0])
		snprintf(body, sizeof(body), "%s", p->last);
	else if (WIFSIGNALED(p->status))
		snprintf(body, sizeof(body),
			 "killed by signal %d before it opened a window",
			 WTERMSIG(p->status));
	else
		snprintf(body, sizeof(body),
			 "exited with status %d before it opened a window",
			 WEXITSTATUS(p->status));

	fprintf(stderr, "kdos-con: '%s' did not start: %s\n", name, body);
	kb_notify(name, "Did not start", body);
}

/*
 * A CHANNEL WITH NO WINDOW LEFT AND NO GUEST BEHIND IT.
 *
 * THE ONE PLACE A PROCESS IS FREED, and it is reached from the reap and from
 * nowhere else — never from a window going, because a window goes while a
 * datagram from that same channel is being read and the loop reading it would
 * be walking freed memory.
 *
 * A PROCESS WITH NO WINDOW AND A LIVE GUEST IS NOT THIS. An application whose
 * last window closed is what a shared session bus is for: the next launch
 * hands off into it and opens a window there, and the backstop against one
 * forgotten is the box collector, which stops a container with nothing open in
 * it.
 */
static void proc_gc(void)
{
	for (int i = 0; i < nprocs; i++) {
		struct EmbedProc *p = procs[i];

		if (p->wins || p->pid > 0 || !p->gone)
			continue;
		if (p->fd >= 0)
			close(p->fd);
		if (p->errfd >= 0)
			close(p->errfd);
		procs[i--] = procs[--nprocs];
		free(p);
	}
}

/*
 * A guest that exited closes its windows. Polled rather than driven by
 * SIGCHLD, for the reason vt_reap is: the session already wakes on a timer,
 * and a handler would be a signal racing the window list.
 */
void embed_reap(void)
{
	/*
	 * THE DEADLINE ON A WINDOW THAT WAS ASKED TO GO, before any process is
	 * looked at. A window whose ask went out EM_CLOSE_MS ago and is still
	 * here is a window nothing can close, which is the failure the deadline
	 * exists for; it is taken out locally, because the toplevel behind it
	 * answers nothing and there is nobody left to ask.
	 *
	 * AND THE DROP OF THE LAST ONE ARMS THE PROCESS CLOCK HERE. The ask
	 * arms it only when it already covers every window a person can reach,
	 * so a guest asked to close two windows at once is armed by neither
	 * ask: both windows leave the desktop on their own deadlines and the
	 * cage is left running for the life of the session with nothing on the
	 * screen and no taskbar chip to reach it by. A guest whose last window
	 * had to be taken from it gets the same grace after the drop that being
	 * asked would have bought it.
	 */
	struct EmbedWin *ws[EM_WIN_CAP];
	int nw = all_wins(ws, EM_WIN_CAP);
	unsigned long long t = now_ms();

	for (int k = 0; k < nw; k++) {
		struct EmbedProc *p;

		if (!ws[k]->close_ms || !ws[k]->win ||
		    t - ws[k]->close_ms <= EM_CLOSE_MS)
			continue;
		p = ws[k]->proc;
		win_drop(ws[k]->win);
		if (!nreachable(p) && p->pid > 0 && !p->closing_ms)
			p->closing_ms = t;
	}

	for (int i = 0; i < nprocs; i++) {
		struct EmbedProc *p = procs[i];
		int status = 0;

		/*
		 * THE DEADLINE ON A GUEST THAT WAS ASKED TO GO. SIGTERM first,
		 * because the cage's own shutdown asks its client and waits;
		 * SIGKILL after, because a cage blocked on a container runtime
		 * that will not unwind is a window on the desktop that nothing
		 * can close.
		 */
		if (p->pid > 0 && p->closing_ms) {
			unsigned long long waited = t - p->closing_ms;

			/*
			 * EACH SIGNAL GOES ONCE, which is what `sigsent`
			 * records. This runs on every pump: a kill() left
			 * ungated is thousands of signals across one close,
			 * and a guest whose SIGTERM handler is re-entered at
			 * that rate never reaches the end of the shutdown it
			 * was asked for.
			 */
			if (waited > EM_CLOSE_MS + EM_KILL_MS) {
				if (p->sigsent != SIGKILL) {
					kill(p->pid, SIGKILL);
					p->sigsent = SIGKILL;
				}
			} else if (waited > EM_CLOSE_MS && !p->sigsent) {
				kill(p->pid, SIGTERM);
				p->sigsent = SIGTERM;
			}
		}
		if (p->pid > 0 && waitpid(p->pid, &status, WNOHANG) == p->pid) {
			p->pid = 0;
			p->gone = 1;
			p->status = status;
		}
		if (!p->gone || p->pid)
			continue;

		/*
		 * A GUEST THAT NEVER OPENED A WINDOW EITHER FAILED TO START OR
		 * HANDED ITS DOCUMENT TO AN INSTANCE THAT WAS ALREADY RUNNING,
		 * and the exit status is the only thing that tells them apart.
		 *
		 * A CLEAN EXIT WITH NO WINDOW IS THE HANDOFF, and it is SILENT.
		 * Every boxed application shares one session bus, which is
		 * exactly what makes a second launch open a second document in
		 * the first cage — so a notice here would fire every time
		 * somebody opened a second file. The window the person asked
		 * for arrives in that first cage as another KEMBED_OPEN.
		 *
		 * ANYTHING ELSE IS A FAILURE, said once, here, because this is
		 * the one place that knows both that no window ever opened and
		 * what the program wrote on its way out.
		 */
		if (!p->opened && !(WIFEXITED(p->status) &&
				    WEXITSTATUS(p->status) == 0)) {
			drain_err(p);
			say_stillborn(p);
		}

		/*
		 * AND EVERY WINDOW OF IT GOES, IN ONE PASS. No window may
		 * outlive its channel: an embedded window with no process
		 * behind it has no pixels, no input and no way to be closed.
		 * win_drop() is what takes one out — the toplevels are already
		 * gone with the process, so there is nobody left to ask.
		 */
		while (p->wins && p->wins->win)
			win_drop(p->wins->win);
	}
	proc_gc();
}

/*
 * ONE WINDOW GOES, called from win_drop once the window is going for good. The
 * channel behind it is left alone here, whether or not this was its last
 * window: freeing it is proc_gc's, which runs where no datagram from it is
 * being read.
 */
void embed_free(Win *w)
{
	struct EmbedWin *e = w ? w->em : NULL;
	struct EmbedProc *p;

	if (!e)
		return;
	w->em = NULL;
	e->win = NULL;
	p = e->proc;

	for (struct EmbedWin **pp = &p->wins; *pp; pp = &(*pp)->next)
		if (*pp == e) {
			*pp = e->next;
			p->nwins--;
			break;
		}

	if (e->map)
		munmap(e->map, e->map_len);
	/*
	 * THE SESSION'S SPRITE SLOTS GO BACK. They are a finite map, not a
	 * counter that wraps: a guest opened and closed enough times without
	 * this exhausts it and no window on the desktop can show a picture
	 * again. The whole table is walked rather than bw*bh, because a guest
	 * that was once larger still holds the slots above its current size.
	 */
	for (int i = 0; i < EM_MAX_BLOCKS; i++)
		if (e->slots[i] >= 0) {
			kcon_server_free_slot(S.server, e->slots[i]);
			e->slots[i] = -1;
		}
	free(e->scratch);
	free(e);
}

int embed_alive(const Win *w)
{
	return w && w->em && w->em->proc->pid > 0 && !w->em->proc->gone;
}

/* ── drawing ─────────────────────────────────────────────────────────── */

/*
 * The window's cells ARE the picture: each one names the block covering it and
 * which cell of that block it is. Nothing here touches a pixel — the bytes
 * went to the views as sprites and this is the reference to them.
 */
void embed_draw(const Win *w)
{
	const struct EmbedWin *e = w ? w->em : NULL;

	if (!e)
		return;

	/*
	 * A WINDOW WITH NO FRAME YET SAYS SO. Sprite cells naming slots no
	 * display has a picture for come out as the fallback mark, which is a
	 * window full of shade blocks and reads as a broken application rather
	 * than as one that has not started drawing.
	 */
	if (!e->drew) {
		static const char *msg = "starting…";
		int lw = ktui_utf8_width(msg);
		int x = w->geom.x + (w->geom.w - lw) / 2;
		int y = w->geom.y + w->geom.h / 2;

		if (lw <= w->geom.w && w->geom.h > 0)
			ktui_draw_text(x, y, lw, msg, KT_MID, KT_BG,
				       KT_A_NONE);
		return;
	}

	for (int y = 0; y < w->geom.h && y < e->rows; y++)
		for (int x = 0; x < w->geom.w && x < e->cols; x++) {
			int slot = e->slots[(y / e->tile) * e->bw +
					    (x / e->tile)];
			uint32_t ch;

			/*
			 * A BLOCK THE SESSION HAD NO SLOT LEFT FOR IS SHADE,
			 * NOT NOTHING. A cell left unwritten shows whatever is
			 * behind this window, so the picture would be a hole
			 * onto another program's text — which reads as a
			 * compositing defect rather than as this desktop
			 * having run out of the numbers a picture is carried
			 * under. It is the same mark a display that lost a
			 * picture draws, for the same reason.
			 */
			if (slot < 0) {
				ktui_draw_cell(w->geom.x + x, w->geom.y + y,
					       0x2593u, KT_DIM, KT_BG, 0);
				continue;
			}
			ch = KTUI_SPRITE_BASE |
			     ((uint32_t)slot << 8) |
			     ((uint32_t)(y % e->tile) << 4) |
			     (uint32_t)(x % e->tile);
			ktui_draw_cell(w->geom.x + x, w->geom.y + y, ch,
				       KT_TEXT, KT_BG, 0);
		}
}

/* ── input ───────────────────────────────────────────────────────────── */

/*
 * A CHARACTER BACK TO THE KEY THAT PRODUCES IT, on the US keymap a guest is
 * started with.
 *
 * THIS IS THE PATH FOR A VIEW THAT HAS NO KEYBOARD — one inside somebody
 * else's terminal, which is what a session reached over ssh is driven by.
 * Such a view reports a character, because a character is all its terminal
 * gave it, and this is the other half of that trip. It is a table rather than
 * a keymap because the session links no xkb and must not.
 *
 * WHAT IT CANNOT DO is what the raw path below exists for: a character has no
 * release, so a key sent this way is pressed and released in the same breath
 * and nothing can be HELD. A view that reports a real device drives a guest
 * through embed_key_raw() instead and none of this runs for it.
 */
struct KeyCode {
	int key;
	uint16_t code;
	uint8_t shift;
};

static const struct KeyCode keymap[] = {
	{ 'a', KEY_A, 0 }, { 'b', KEY_B, 0 }, { 'c', KEY_C, 0 },
	{ 'd', KEY_D, 0 }, { 'e', KEY_E, 0 }, { 'f', KEY_F, 0 },
	{ 'g', KEY_G, 0 }, { 'h', KEY_H, 0 }, { 'i', KEY_I, 0 },
	{ 'j', KEY_J, 0 }, { 'k', KEY_K, 0 }, { 'l', KEY_L, 0 },
	{ 'm', KEY_M, 0 }, { 'n', KEY_N, 0 }, { 'o', KEY_O, 0 },
	{ 'p', KEY_P, 0 }, { 'q', KEY_Q, 0 }, { 'r', KEY_R, 0 },
	{ 's', KEY_S, 0 }, { 't', KEY_T, 0 }, { 'u', KEY_U, 0 },
	{ 'v', KEY_V, 0 }, { 'w', KEY_W, 0 }, { 'x', KEY_X, 0 },
	{ 'y', KEY_Y, 0 }, { 'z', KEY_Z, 0 },
	{ 'A', KEY_A, 1 }, { 'B', KEY_B, 1 }, { 'C', KEY_C, 1 },
	{ 'D', KEY_D, 1 }, { 'E', KEY_E, 1 }, { 'F', KEY_F, 1 },
	{ 'G', KEY_G, 1 }, { 'H', KEY_H, 1 }, { 'I', KEY_I, 1 },
	{ 'J', KEY_J, 1 }, { 'K', KEY_K, 1 }, { 'L', KEY_L, 1 },
	{ 'M', KEY_M, 1 }, { 'N', KEY_N, 1 }, { 'O', KEY_O, 1 },
	{ 'P', KEY_P, 1 }, { 'Q', KEY_Q, 1 }, { 'R', KEY_R, 1 },
	{ 'S', KEY_S, 1 }, { 'T', KEY_T, 1 }, { 'U', KEY_U, 1 },
	{ 'V', KEY_V, 1 }, { 'W', KEY_W, 1 }, { 'X', KEY_X, 1 },
	{ 'Y', KEY_Y, 1 }, { 'Z', KEY_Z, 1 },
	{ '1', KEY_1, 0 }, { '2', KEY_2, 0 }, { '3', KEY_3, 0 },
	{ '4', KEY_4, 0 }, { '5', KEY_5, 0 }, { '6', KEY_6, 0 },
	{ '7', KEY_7, 0 }, { '8', KEY_8, 0 }, { '9', KEY_9, 0 },
	{ '0', KEY_0, 0 },
	{ '!', KEY_1, 1 }, { '@', KEY_2, 1 }, { '#', KEY_3, 1 },
	{ '$', KEY_4, 1 }, { '%', KEY_5, 1 }, { '^', KEY_6, 1 },
	{ '&', KEY_7, 1 }, { '*', KEY_8, 1 }, { '(', KEY_9, 1 },
	{ ')', KEY_0, 1 },
	{ ' ', KEY_SPACE, 0 },
	{ '-', KEY_MINUS, 0 }, { '_', KEY_MINUS, 1 },
	{ '=', KEY_EQUAL, 0 }, { '+', KEY_EQUAL, 1 },
	{ '[', KEY_LEFTBRACE, 0 }, { '{', KEY_LEFTBRACE, 1 },
	{ ']', KEY_RIGHTBRACE, 0 }, { '}', KEY_RIGHTBRACE, 1 },
	{ ';', KEY_SEMICOLON, 0 }, { ':', KEY_SEMICOLON, 1 },
	{ '\'', KEY_APOSTROPHE, 0 }, { '"', KEY_APOSTROPHE, 1 },
	{ '`', KEY_GRAVE, 0 }, { '~', KEY_GRAVE, 1 },
	{ '\\', KEY_BACKSLASH, 0 }, { '|', KEY_BACKSLASH, 1 },
	{ ',', KEY_COMMA, 0 }, { '<', KEY_COMMA, 1 },
	{ '.', KEY_DOT, 0 }, { '>', KEY_DOT, 1 },
	{ '/', KEY_SLASH, 0 }, { '?', KEY_SLASH, 1 },
	{ KT_K_ESC, KEY_ESC, 0 },
	{ KT_K_ENTER, KEY_ENTER, 0 },
	{ KT_K_TAB, KEY_TAB, 0 },
	{ KT_K_BTAB, KEY_TAB, 1 },
	{ KT_K_BACKSPACE, KEY_BACKSPACE, 0 },
	{ KT_K_UP, KEY_UP, 0 }, { KT_K_DOWN, KEY_DOWN, 0 },
	{ KT_K_LEFT, KEY_LEFT, 0 }, { KT_K_RIGHT, KEY_RIGHT, 0 },
	{ KT_K_HOME, KEY_HOME, 0 }, { KT_K_END, KEY_END, 0 },
	{ KT_K_PGUP, KEY_PAGEUP, 0 }, { KT_K_PGDN, KEY_PAGEDOWN, 0 },
	{ KT_K_INS, KEY_INSERT, 0 }, { KT_K_DEL, KEY_DELETE, 0 },
	{ KT_K_F1, KEY_F1, 0 }, { KT_K_F2, KEY_F2, 0 },
	{ KT_K_F3, KEY_F3, 0 }, { KT_K_F4, KEY_F4, 0 },
	{ KT_K_F5, KEY_F5, 0 }, { KT_K_F6, KEY_F6, 0 },
	{ KT_K_F7, KEY_F7, 0 }, { KT_K_F8, KEY_F8, 0 },
	{ KT_K_F9, KEY_F9, 0 }, { KT_K_F10, KEY_F10, 0 },
	{ KT_K_F11, KEY_F11, 0 }, { KT_K_F12, KEY_F12, 0 },
};

/*
 * A KEY DOWN OR UP, AND THE SET REMEMBERS IT. Everything that sends a key goes
 * through here, so there is one place where what a window is owed a release
 * for is written — a second sender would be a key this side believes is up and
 * the guest is holding down.
 */
static void tap(struct EmbedWin *e, int code, int down, unsigned ms)
{
	if (code < 0 || code > KCON_KEYCODE_MAX)
		return;
	if (send_msg(e, KEMBED_KEY, code, down ? 1 : 0, 0, 0, ms) != 0)
		return;
	if (down)
		e->held[code / 32] |= 1u << (code % 32);
	else
		e->held[code / 32] &= ~(1u << (code % 32));
}

/*
 * A POINTER BUTTON DOWN OR UP, AND THE SAME SET REMEMBERS IT.
 *
 * A BUTTON IS HELD EXACTLY AS A KEY IS: a drag in an editor, a selection in a
 * browser, an aim button in a game. Every one of them survives the session
 * moving the keyboard away, and the release the person makes a moment later is
 * delivered to whatever has the focus then — so a button whose press crossed
 * and whose release did not is a button the guest holds for the life of the
 * application. Everything that sends a button goes through here, for the
 * reason everything that sends a key goes through tap().
 */
static void btn_tap(struct EmbedWin *e, int px, int py, int btn, int down,
		    unsigned ms)
{
	if (btn < 0 || btn > KCON_KEYCODE_MAX)
		return;
	if (send_msg(e, KEMBED_BUTTON, px, py, btn, down ? 1 : 0, ms) != 0)
		return;
	if (down)
		e->held_btn[btn / 32] |= 1u << (btn % 32);
	else
		e->held_btn[btn / 32] &= ~(1u << (btn % 32));
}

/*
 * EVERY KEY AND EVERY BUTTON THIS WINDOW IS HOLDING, RELEASED.
 *
 * Called before the keyboard leaves the window, and wherever the raw stream
 * stops — a release travels on that stream and on nothing else. A press whose
 * release was delivered somewhere else is a key held for the life of the
 * application — W in a game, Ctrl in an editor, the left button mid-drag — and
 * the release cannot be delivered here because the session never saw one: the
 * person let go while another window had the focus.
 *
 * EACH GOES BACK AS THE OP IT CAME AS. A button released as a key reaches the
 * guest's keyboard, is resolved as a keysym and leaves the button down.
 */
static void release_held(struct EmbedWin *e)
{
	unsigned ms = (unsigned)now_ms();

	for (int i = 0; i < EM_KEYSET; i++) {
		if (!e->held[i])
			continue;
		for (int b = 0; b < 32; b++)
			if (e->held[i] & (1u << b))
				tap(e, i * 32 + b, 0, ms);
	}
	for (int i = 0; i < EM_KEYSET; i++) {
		if (!e->held_btn[i])
			continue;
		for (int b = 0; b < 32; b++)
			if (e->held_btn[i] & (1u << b))
				btn_tap(e, e->last_px, e->last_py,
					i * 32 + b, 0, ms);
	}
}

int embed_key(Win *w, const KtuiEvent *ev)
{
	struct EmbedWin *e = w ? w->em : NULL;

	if (!e || !e->id || e->proc->fd < 0)
		return 0;

	const struct KeyCode *k = NULL;

	for (size_t i = 0; i < sizeof(keymap) / sizeof(keymap[0]); i++)
		if (keymap[i].key == ev->key) {
			k = &keymap[i];
			break;
		}
	if (!k)
		return 0;

	/*
	 * PRESS AND RELEASE, both here. This view reports a key as one event —
	 * it is a character, not a switch — so there is no second event to
	 * send the release from, and a guest given only the press would hold
	 * every key it was ever given down. It is also why nothing typed this
	 * way can be held: see the table's heading.
	 */
	int shift = k->shift || (ev->mods & KT_MOD_SHIFT);
	unsigned ms = (unsigned)now_ms();

	if (shift)
		tap(e, KEY_LEFTSHIFT, 1, ms);
	if (ev->mods & KT_MOD_CTRL)
		tap(e, KEY_LEFTCTRL, 1, ms);
	if (ev->mods & KT_MOD_ALT)
		tap(e, KEY_LEFTALT, 1, ms);

	tap(e, k->code, 1, ms);
	tap(e, k->code, 0, ms);

	if (ev->mods & KT_MOD_ALT)
		tap(e, KEY_LEFTALT, 0, ms);
	if (ev->mods & KT_MOD_CTRL)
		tap(e, KEY_LEFTCTRL, 0, ms);
	if (shift)
		tap(e, KEY_LEFTSHIFT, 0, ms);
	return 1;
}

int embed_ptr(Win *w, const KtuiEvent *ev)
{
	struct EmbedWin *e = w ? w->em : NULL;

	if (!e || !e->id || e->proc->fd < 0)
		return 0;

	/*
	 * THE CELL, PLUS WHERE IN IT. A cell is several pixels wide and a
	 * guest's buttons are smaller than one, so a view that knows its own
	 * pixel geometry says where inside the cell the pointer was; one that
	 * does not means its centre.
	 */
	int cx = ev->mx - w->geom.x, cy = ev->my - w->geom.y;
	int px = cx * e->cell_w + e->cell_w / 2 +
		 ev->subx * e->cell_w / 256;
	int py = cy * e->cell_h + e->cell_h / 2 +
		 ev->suby * e->cell_h / 256;

	if (px < 0)
		px = 0;
	if (py < 0)
		py = 0;
	if (px >= e->pw && e->pw > 0)
		px = e->pw - 1;
	if (py >= e->ph && e->ph > 0)
		py = e->ph - 1;

	unsigned ms = (unsigned)now_ms();

	switch (ev->btn) {
	case KT_MB_LEFT:
	case KT_MB_MIDDLE:
	case KT_MB_RIGHT: {
		static const int btn[] = { BTN_LEFT, BTN_MIDDLE, BTN_RIGHT };

		if (ev->press == KT_MP_DRAG) {
			e->last_px = px;
			e->last_py = py;
			send_msg(e, KEMBED_MOTION, px, py, 0, 0, ms);
		} else {
			e->last_px = px;
			e->last_py = py;
			btn_tap(e, px, py, btn[ev->btn],
				ev->press == KT_MP_PRESS, ms);
		}
		break;
	}
	case KT_MB_WHEEL_UP:
	case KT_MB_WHEEL_DOWN: {
		/*
		 * ONE DETENT, AND THE POSITION FIRST. KEMBED_AXIS carries no
		 * place of its own, so a scroll over a window the pointer has
		 * just moved into would otherwise be spent where the pointer
		 * was. A cell view has no resolution finer than a detent and
		 * no second axis, which is exactly what the raw path below
		 * adds.
		 */
		int up = ev->btn == KT_MB_WHEEL_UP;

		send_msg(e, KEMBED_MOTION, px, py, 0, 0, ms);
		e->last_px = px;
		e->last_py = py;
		send_msg(e, KEMBED_AXIS, (up ? -1 : 1) * 15 * 256,
			 up ? -120 : 120, 0, KEMBED_AXIS_WHEEL, ms);
		break;
	}
	default:
		send_msg(e, KEMBED_MOTION, px, py, 0, 0, ms);
		e->last_px = px;
		e->last_py = py;
		break;
	}
	return 1;
}

/* ── the same input, as the device reported it ───────────────────────────
 *
 * A CELL DESKTOP IS POINTED AT A CELL AND TYPED AT WITH CHARACTERS, and an
 * embedded guest is the one thing on it that is neither. It holds keys down,
 * repeats from its own keymap, reads a modifier that produces no character,
 * aims at a scrollbar two pixels wide, scrolls sideways and takes the pointer
 * away from the desktop altogether — none of which survives a round trip
 * through a codepoint and a cell.
 *
 * THE COOKED EVENT FOR THE SAME PHYSICAL INPUT HAS ALREADY BEEN ROUTED when
 * anything here is called. That single ordering rule is what lets this arm
 * deliver and nothing else: the session has already decided whether a chord
 * ate the key, which window the pointer is over and where its own cursor is,
 * so there is exactly one answer to where a click landed and it is not made
 * here.
 * ──────────────────────────────────────────────────────────────────────── */

/*
 * THE LAYOUT EVERY GUEST IS GIVEN, and the count that says which guests have
 * it.
 *
 * ONE SESSION IS ONE KEYBOARD, so the last view to say what it is running
 * wins. It is held as a sealed descriptor rather than as bytes because that is
 * how it crosses: sealed so the guest may map it and trust the length, and
 * kept open for the life of the session because a guest may open at any time
 * and every one of them needs the same one.
 */
static int km_fd = -1;
static size_t km_len;
static int km_format;
/*
 * COUNTED FROM ONE, so that a guest calloc'd at zero is behind whatever the
 * session holds and is sent the layout on its first pump. Zero would read as
 * "this guest already has it" for every guest that had never been told
 * anything.
 */
static unsigned km_gen = 1;
/*
 * THE MODIFIER STATE THE KEYBOARD IS IN — depressed, latched, locked, group —
 * and whether the session is being told it at all.
 *
 * IT ARRIVES ON THE RAW STREAM AND NOWHERE ELSE, and the raw stream runs only
 * while an embedded window holds the keyboard. So the mask is knowledge with a
 * lifetime: Caps Lock pressed into a terminal while no guest had the focus
 * reaches the session as nothing at all, and a mask carried across that gap
 * describes a keyboard the session stopped watching. `km_mods_valid` is
 * cleared wherever the stream stops, and an invalid mask is sent to no guest —
 * KEMBED_MODS is acted on, so a session that asserts the locks are clear when
 * it does not know is one that puts every letter in the wrong case.
 *
 * IT IS A RESYNC AND NOT A PER-KEY EVENT: the key stream drives the guest's
 * own xkb state, and a mask applied beside it disagrees with the keys the
 * guest believes are down. Only a lock and a layout group are sent, and only
 * when they change — they are what no key a guest receives can establish.
 */
static unsigned km_mods[4];
static int km_mods_valid;

void embed_mods_note(const KconKeyRaw *k)
{
	if (!k)
		return;
	km_mods[0] = k->depressed;
	km_mods[1] = k->latched;
	km_mods[2] = k->locked;
	km_mods[3] = k->group;
	km_mods_valid = 1;
}

/*
 * HAND THIS GUEST THE LAYOUT AND THE STATE IT CANNOT DERIVE, in that order: a
 * mask means nothing until the keymap it indexes into has been compiled.
 */
static void send_mods(struct EmbedProc *p)
{
	if (!p || p->fd < 0 || !km_mods_valid)
		return;
	memcpy(p->mods_sent, km_mods, sizeof(p->mods_sent));
	proc_send(p, KEMBED_MODS, (int)km_mods[0], (int)km_mods[1],
		  (int)km_mods[2], (int)km_mods[3], 0, 0);
}

/*
 * THE LOCKS AND THE GROUP AGAIN, WHEN THEY ARE NOT WHAT THIS GUEST BELIEVES.
 *
 * A GUEST RESOLVES ITS OWN KEYS and reaches the right answer for everything it
 * was sent — but it was sent nothing while it did not have the keyboard, and
 * nothing for the presses a chord ate. Caps Lock and a layout group are the
 * two states that survive that gap, so they are compared against what this
 * window was last told and re-sent when they differ. Depressed and latched are
 * carried along because they are the same keyboard's, not because a guest
 * needs them: it holds the keys it was given.
 */
static void mods_resync(struct EmbedProc *p)
{
	if (!km_mods_valid)
		return;
	if (p->mods_sent[2] == km_mods[2] && p->mods_sent[3] == km_mods[3])
		return;
	send_mods(p);
}

/*
 * THE RAW STREAM STARTED, OR STOPPED.
 *
 * ONE CALL FOR BOTH EDGES, from the one place that knows the stream changed
 * state. A RELEASE TRAVELS ON THAT STREAM AND ON NOTHING ELSE: a key still
 * down when it stops is one every guest goes on holding — W in a game, Ctrl in
 * an editor — because the release the person makes a moment later is sent by
 * nobody. And the modifier mask travels on it too, so what the session holds
 * stops being true the moment it stops arriving.
 *
 * EVERY GUEST AND NOT ONLY THE FOCUSED ONE. Focus leaving a window empties it
 * on its way out, so the window still holding keys is the one the gate closed
 * under — and the gate also closes for the lock, the saver and a view that
 * went away, none of which move the focus.
 */
void embed_raw_reset(void)
{
	km_mods_valid = 0;
	struct EmbedWin *ws[EM_WIN_CAP];
	int n = all_wins(ws, EM_WIN_CAP);

	for (int i = 0; i < n; i++)
		release_held(ws[i]);
}

/*
 * THE LAYOUT, AND ONLY TO A GUEST THAT DOES NOT ALREADY HOLD IT. A resend
 * makes the guest recompile a keymap it is already running, and a recompile
 * resets the modifier state the resync beside it then has to restore — so a
 * layout that reaches a guest twice is a lock that flickers off and on.
 */
static void send_keymap(struct EmbedProc *p)
{
	if (km_fd < 0 || !p || p->fd < 0 || p->km_gen == km_gen)
		return;
	if (proc_send_fd(p, KEMBED_KEYMAP, (int)km_len, km_format, 0, 0, 0,
			 km_fd) != 0)
		return;
	p->km_gen = km_gen;
	/* A COMPILED KEYMAP IS A KEYBOARD WITH NOTHING HELD AND NOTHING
	 * LOCKED. The guest starts the new layout from there, so that is what
	 * it has been told, and the resync beside it restores whatever the
	 * session actually knows. */
	memset(p->mods_sent, 0, sizeof(p->mods_sent));
	send_mods(p);
}

void embed_keymap(int format, const char *text, size_t len)
{
	int fd;

	if (!text || len < 2 || len > KCON_KEYMAP_MAX)
		return;
	/*
	 * THE SAME LAYOUT AGAIN IS NOT A CHANGE. A view re-announcing what the
	 * session already holds would otherwise reseal a descriptor and make
	 * every guest recompile a keymap it is already running — and a
	 * recompile resets the modifier state the resync beside it then has to
	 * restore.
	 */
	if (km_fd >= 0 && km_len == len && km_format == format) {
		char have[256];
		ssize_t n = pread(km_fd, have, sizeof(have), 0);
		size_t cmp = len < sizeof(have) ? len : sizeof(have);

		if (n == (ssize_t)cmp && !memcmp(have, text, cmp))
			return;
	}

	fd = memfd_create("kdos-keymap", MFD_CLOEXEC | MFD_ALLOW_SEALING);
	if (fd < 0)
		return;
	for (size_t off = 0; off < len;) {
		ssize_t n = write(fd, text + off, len - off);

		if (n > 0) {
			off += (size_t)n;
			continue;
		}
		if (n < 0 && errno == EINTR)
			continue;
		close(fd);
		return;
	}
	/*
	 * SEALED BEFORE IT CROSSES. The guest maps it and compiles from the
	 * length it was told; a descriptor that could still be shrunk is one
	 * whose reader can be faulted after the fact, and this end has no
	 * reason to write to it again.
	 */
	if (fcntl(fd, F_ADD_SEALS,
		  F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_WRITE) != 0) {
		close(fd);
		return;
	}

	if (km_fd >= 0)
		close(km_fd);
	km_fd = fd;
	km_len = len;
	km_format = format;
	km_gen++;

	for (int i = 0; i < nprocs; i++)
		send_keymap(procs[i]);
}

/*
 * A VIEW PIXEL TO A GUEST PIXEL.
 *
 * The position arrives in the view's own pixels and in the cell those pixels
 * were measured in, so the window's origin converts with no rounding: it is
 * exactly `geom.x` cells along, and the subtraction is exact at both edges of
 * the window. A VIEW WHOSE CELL IS NOT THE ONE THE GUEST IS RENDERED AT is
 * scaled by the ratio — the mapping is `cols * cell_w` pixels wide and nothing
 * else — which is what a second display of a different font does, and what the
 * primary does for the one or two events between a font step and the resize it
 * causes.
 */
static void raw_px(const struct EmbedWin *e, const Win *w, int vx, int vy,
		   int cw, int ch, int *px, int *py)
{
	int x = vx - w->geom.x * cw;
	int y = vy - w->geom.y * ch;

	if (cw != e->cell_w)
		x = x * e->cell_w / cw;
	if (ch != e->cell_h)
		y = y * e->cell_h / ch;
	if (x < 0)
		x = 0;
	if (y < 0)
		y = 0;
	if (x >= e->pw && e->pw > 0)
		x = e->pw - 1;
	if (y >= e->ph && e->ph > 0)
		y = e->ph - 1;
	*px = x;
	*py = y;
}

int embed_key_raw(Win *w, const KconKeyRaw *k)
{
	struct EmbedWin *e = w ? w->em : NULL;

	if (!e || !e->id || e->proc->fd < 0 || !k)
		return 0;
	tap(e, k->code, k->state, k->ms);
	/*
	 * AND THE STATE THE KEY LEFT THE KEYBOARD IN, AFTER THE KEY.
	 *
	 * The mask this message carries is the one the view reached by
	 * applying this key. Ahead of the key, a guest applies the key on top
	 * and toggles a lock straight back off; behind it, both ends hold the
	 * same state and every key after it is resolved under that state. A
	 * guest given the keyboard while Caps Lock is on is told so by the
	 * first key it receives, and that key is the only one it resolves
	 * without it.
	 */
	mods_resync(e->proc);
	return 1;
}

int embed_ptr_raw(Win *w, const KconPtrRaw *p)
{
	struct EmbedWin *e = w ? w->em : NULL;
	int px, py;

	if (!e || !e->id || e->proc->fd < 0 || !p ||
	    p->cell_w < 1 || p->cell_h < 1)
		return 0;

	raw_px(e, w, p->x, p->y, p->cell_w, p->cell_h, &px, &py);

	/*
	 * THE DELTA FIRST, THEN THE PLACE. A guest holding the pointer reads
	 * the delta and nothing else; one that is not spends it on the motion
	 * that follows. Sending them in this order is what makes a locked
	 * pointer the same code path as a free one with the position left off.
	 */
	if (p->dx || p->dy || p->dx_un || p->dy_un)
		send_msg(e, KEMBED_REL, p->dx, p->dy, p->dx_un, p->dy_un,
			 p->ms);

	if (p->button) {
		/*
		 * A BUTTON CARRIES A POSITION BECAUSE MOTION IS COALESCED, and
		 * a click after a coalesced motion must land where the pointer
		 * actually is. WHILE THE POINTER IS HELD it carries the place
		 * the guest last saw instead: a guest that asked for the
		 * cursor to stop moving must not have it warped by its own
		 * mouse button.
		 */
		if (e->grab != KEMBED_GRAB_NONE)
			btn_tap(e, e->last_px, e->last_py, p->button,
				p->state, p->ms);
		else {
			e->last_px = px;
			e->last_py = py;
			btn_tap(e, px, py, p->button, p->state, p->ms);
		}
		return 1;
	}

	if (e->grab != KEMBED_GRAB_NONE)
		return 1;	/* the delta above is the whole event */

	/*
	 * AND THE MOTION EVEN WHERE THE PIXEL IS UNCHANGED, because the cage
	 * holds the delta above until a motion spends it: a position dropped
	 * as redundant is a delta that lands on the next one instead, which a
	 * guest reading relative motion sees as the pointer jumping. The
	 * session's own queue coalesces consecutive motions into one per turn,
	 * so this is a message a turn and not a message a device event.
	 */
	e->last_px = px;
	e->last_py = py;
	send_msg(e, KEMBED_MOTION, px, py, 0, 0, p->ms);
	return 1;
}

int embed_axis_raw(Win *w, const KconAxisRaw *a)
{
	struct EmbedWin *e = w ? w->em : NULL;
	int src;

	if (!e || !e->id || e->proc->fd < 0 || !a)
		return 0;

	/*
	 * MAPPED IN A SWITCH AND NOT ASSIGNED ACROSS. The two enumerations are
	 * two protocols' own and neither may be made to depend on the numbers
	 * of the other; a pass-through that happened to be right today is a
	 * coupling neither end could see when one of them changed.
	 */
	switch (a->source) {
	case KCON_AXIS_SRC_FINGER:
		src = KEMBED_AXIS_FINGER;
		break;
	case KCON_AXIS_SRC_CONTINUOUS:
		src = KEMBED_AXIS_CONTINUOUS;
		break;
	case KCON_AXIS_SRC_WHEEL_TILT:
		src = KEMBED_AXIS_WHEEL_TILT;
		break;
	default:
		src = KEMBED_AXIS_WHEEL;
		break;
	}
	if (a->flags & KCON_AXIS_INVERTED)
		src |= (int)KEMBED_AXIS_INVERTED;

	send_msg(e, KEMBED_AXIS, a->value, a->value120,
		 a->axis == KCON_AXIS_HORIZ ? 1 : 0, src, a->ms);
	return 1;
}

/*
 * THE POINTER LEFT. Sent rather than a motion to a position outside the
 * window: the cage clamps a warp back onto its output and would deliver the
 * clamped one as an arrival, so a guest told to leave that way stays hovered
 * on whatever the edge is.
 */
void embed_leave(Win *w)
{
	struct EmbedWin *e = w ? w->em : NULL;

	if (!e || !e->id || e->proc->fd < 0)
		return;
	send_msg(e, KEMBED_LEAVE, 0, 0, 0, 0, 0);
}

/*
 * WHICH WINDOW IS HOLDING THE POINTER, or none. While one is, the session
 * routes every pointer event to it and hovers, raises and warps nothing — and
 * moving the keyboard focus is the way out, because the constraint the cage
 * created is only active for the surface that has the keyboard.
 */
Win *embed_grab_win(void)
{
	struct EmbedWin *ws[EM_WIN_CAP];
	int n = all_wins(ws, EM_WIN_CAP);

	for (int i = 0; i < n; i++)
		if (ws[i]->grab != KEMBED_GRAB_NONE && ws[i]->win &&
		    ws[i]->proc->fd >= 0)
			return ws[i]->win;
	return NULL;
}

/*
 * IS A GUEST ASKING FOR THE SCREEN TO STAY ON. Only one somebody can see: an
 * inhibitor held by a window on another workspace, minimised or under the
 * lock is bookkeeping, and a machine kept awake by bookkeeping is one whose
 * battery goes while it sits closed.
 */
int embed_inhibited(void)
{
	struct EmbedWin *ws[EM_WIN_CAP];
	int n = all_wins(ws, EM_WIN_CAP);

	for (int i = 0; i < n; i++)
		if (ws[i]->inhibit && ws[i]->win &&
		    win_is_on_screen(ws[i]->win))
			return 1;
	return 0;
}
