/*
 * kicon_slot_png() — a tray item's `icon-data`, decoded from bytes.
 *
 * WHY A FIXTURE AND NOT A GOLDEN. Every surface that draws an icon turns its
 * pictures OFF for a dump, because a golden frame is the character grid and a
 * picture in one is a diff against whatever theme the machine carried. So the
 * one path that decodes a PNG out of a D-Bus message is the one path no
 * reference frame can reach, and without this it is compiled and never run.
 *
 * WHAT IT ASSERTS is the gate rather than the picture: a real PNG is accepted
 * and a slot comes back, a truncated one is refused, a blob over the cap is
 * refused without being looked at, and the same bytes twice are ONE slot —
 * which is what keeps a menu redrawing at frame rate from decoding a PNG per
 * row per frame.
 *
 * Not shipped. testing/selftest.sh compiles it and nothing else does.
 */
#include <stdio.h>
#include <string.h>

#include "kicon.h"
#include "ktui.h"

/*
 * A 2x2 RGBA PNG, and THE SAME BYTES testing/fixtures/traymenu/menustub.c
 * puts on the bus — so what this decodes is what that publishes.
 *
 * GENERATE IT, NEVER TYPE IT. A PNG carries a CRC per chunk and a zlib
 * stream, and bytes written out by hand satisfy neither; libpng refuses
 * them. Regenerate with a few lines of python's `zlib` and `struct` rather
 * than editing a value here.
 *
 * TWO PIXELS BY TWO, not one: a decoder that mishandles the row filter or
 * the stride is correct on a single pixel and wrong on everything else.
 */
static const unsigned char PNG1[] = {
	0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d,
	0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02,
	0x08, 0x06, 0x00, 0x00, 0x00, 0x72, 0xb6, 0x0d, 0x24, 0x00, 0x00, 0x00,
	0x13, 0x49, 0x44, 0x41, 0x54, 0x78, 0xda, 0x63, 0xf8, 0xcf, 0xc0, 0xf0,
	0x1f, 0x0c, 0x81, 0x34, 0x08, 0x34, 0x00, 0x00, 0x49, 0x49, 0x09, 0x78,
	0x9c, 0x51, 0x17, 0x92, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44,
	0xae, 0x42, 0x60, 0x82,
};

static int fails;

static void ck(int cond, const char *what)
{
	if (!cond) {
		printf("  FAIL %s\n", what);
		fails++;
	}
}

int main(void)
{
	/* Offscreen: the sprite table wants a frame to live beside, and this
	 * asks nothing of a terminal. */
	if (ktui_offscreen_init(40, 10) != 0) {
		printf("  iconpng: no offscreen buffer\n");
		return 1;
	}
	ktui_draw_init();
	kicon_init(8, 16, 1);

	if (!kicon_enabled()) {
		/* A host with no icon layer at all says so and stops: a pass
		 * here would be a pass that asserted nothing. */
		printf("  iconpng (skipped — no icon layer on this host)\n");
		return 0;
	}

	int a = kicon_slot_png(PNG1, sizeof(PNG1), 2, 1);

	ck(a >= 0, "a real PNG did not decode");

	/* THE SAME BYTES ARE ONE SLOT. The key is a hash of the blob, so a
	 * menu redrawing at frame rate costs a hash per row and not a decode
	 * — and two items publishing the same picture share it. */
	ck(kicon_slot_png(PNG1, sizeof(PNG1), 2, 1) == a,
	   "the same bytes came back as a different slot");

	/* A DIFFERENT CELL BOX IS A DIFFERENT PICTURE, because the fit is
	 * baked in: one slot for both would hand the second caller the
	 * first one's size. */
	ck(kicon_slot_png(PNG1, sizeof(PNG1), 4, 2) != a,
	   "a different cell box reused the same slot");

	/* Half a PNG decodes to nothing, and the answer must be -1 rather
	 * than a slot holding whatever libpng left behind. */
	ck(kicon_slot_png(PNG1, sizeof(PNG1) / 2, 2, 1) < 0,
	   "a truncated PNG was accepted");
	ck(kicon_slot_png(PNG1, 0, 2, 1) < 0, "an empty blob was accepted");
	ck(kicon_slot_png(NULL, sizeof(PNG1), 2, 1) < 0,
	   "a NULL blob was accepted");

	/* Past the cap it is refused WITHOUT being decoded: the bytes come
	 * from another process and a megabyte of them is an application that
	 * has confused an icon with a photograph. */
	ck(kicon_slot_png(PNG1, (size_t)KICON_PNG_MAX + 1, 2, 1) < 0,
	   "a blob over KICON_PNG_MAX was accepted");

	kicon_finish();
	if (fails)
		return 1;
	printf("  kicon_slot_png: decodes, memoises by bytes, refuses the rest\n");
	return 0;
}
