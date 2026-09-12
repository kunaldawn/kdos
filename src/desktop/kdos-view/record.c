/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-view --record / --replay — a recording is a view that writes a file
 *
 * A view holds no state: a whole frame and every sprite are resent when one
 * attaches, and `--cast` already proves a view nobody looks at works. So a
 * recorder is a view that writes to a file, and a player is a view whose
 * messages come from one.
 *
 * WHAT IS RECORDED IS THE SESSION'S OWN MESSAGES, with a timestamp each: one
 * ndjson line per message, the payload base64'd, the whole stream under zstd.
 * Not the cells and not the pixels — the messages. A recorder that re-encoded
 * cells would be a second definition of a frame, and it would lose an op added
 * to the protocol after it was written; this one carries whatever arrived.
 *
 * THE HEADER NAMES THE PROTOCOL VERSION, and a replay refuses a version it
 * does not speak rather than drawing it wrong. The bytes are that protocol's,
 * so a recording is only as portable as the protocol is — which is the honest
 * shape for something written by a desktop and played by the same desktop.
 *
 * IT IS KDOS'S FORMAT AND IT IS NOT AN ASCIICAST. An asciicast is a terminal's
 * byte stream; this is a desktop's cell frames, its sprites and its window
 * chrome. No asciinema player will open it, and calling it asciicast would buy
 * a broken expectation. Nothing here needs a player port either: a view that
 * draws cells is already the player.
 * ---------------------------------
 */

#include "record.h"

#ifdef KDOS_VIEW_RECORD

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <zstd.h>

#include "kbase.h"
#include "kcon.h"

/* The one place the format's name and version appear. */
#define KREC_MAGIC "kdos-view-record"
#define KREC_VERSION 1

static FILE *out;
static ZSTD_CStream *cs;
static char *cbuf;
static size_t cbuf_cap;
static double t0;

static double now_s(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

/* One buffer through the compressor and out. Everything written by this file
 * goes through here, so there is one place that knows the stream is
 * compressed at all. */
static int push(const void *data, size_t len, ZSTD_EndDirective mode)
{
	ZSTD_inBuffer in = { data, len, 0 };

	do {
		ZSTD_outBuffer ob = { cbuf, cbuf_cap, 0 };
		size_t left = ZSTD_compressStream2(cs, &ob, &in, mode);

		if (ZSTD_isError(left))
			return -1;
		if (ob.pos && fwrite(cbuf, 1, ob.pos, out) != ob.pos)
			return -1;
		if (mode == ZSTD_e_end && !left)
			break;
		if (mode != ZSTD_e_end && in.pos == in.size)
			break;
	} while (1);
	return 0;
}

int krec_open(const char *path, int cols, int rows, int cell_w, int cell_h)
{
	char head[256];

	if (!path || !*path)
		return -1;
	out = fopen(path, "wb");
	if (!out) {
		perror(path);
		return -1;
	}
	cs = ZSTD_createCStream();
	cbuf_cap = ZSTD_CStreamOutSize();
	cbuf = malloc(cbuf_cap);
	if (!cs || !cbuf) {
		fprintf(stderr, "kdos-view: no memory to record\n");
		krec_close();
		return -1;
	}
	ZSTD_initCStream(cs, 3);

	snprintf(head, sizeof(head),
		 "{\"%s\":%d,\"proto\":%d,\"cols\":%d,\"rows\":%d,"
		 "\"cell_w\":%d,\"cell_h\":%d}\n",
		 KREC_MAGIC, KREC_VERSION, KCON_VERSION, cols, rows,
		 cell_w, cell_h);
	if (push(head, strlen(head), ZSTD_e_continue) != 0) {
		fprintf(stderr, "kdos-view: cannot write %s\n", path);
		krec_close();
		return -1;
	}
	t0 = now_s();
	return 0;
}

void krec_msg(unsigned op, const char *payload, size_t len)
{
	char line[128];
	size_t b64cap;
	char *b64;

	if (!out || !cs)
		return;
	if (len > KCON_MAX_PAYLOAD)
		return;

	b64cap = (len + 2) / 3 * 4 + 1;
	b64 = malloc(b64cap);
	if (!b64)
		return;
	if (kb_b64_encode(payload, len, b64, b64cap) < 0) {
		free(b64);
		return;
	}

	snprintf(line, sizeof(line), "{\"t\":%.3f,\"op\":%u,\"p\":\"",
		 now_s() - t0, op);
	if (push(line, strlen(line), ZSTD_e_continue) == 0 &&
	    push(b64, strlen(b64), ZSTD_e_continue) == 0)
		push("\"}\n", 3, ZSTD_e_continue);
	free(b64);
}

void krec_close(void)
{
	if (cs && out)
		push("", 0, ZSTD_e_end);
	if (cs)
		ZSTD_freeCStream(cs);
	if (out)
		fclose(out);
	free(cbuf);
	cs = NULL;
	out = NULL;
	cbuf = NULL;
	cbuf_cap = 0;
}

/* ── the player ────────────────────────────────────────────────────────── */

/* One JSON number or string out of a line. The format is this file's own and
 * every line it writes has the same six fields, so this reads what this writes
 * rather than being a JSON parser: a recording is not a document somebody
 * hand-edits, and a parser for one would be a parser to keep safe. */
static int field_num(const char *line, const char *key, double *out_v)
{
	const char *at = strstr(line, key);

	if (!at)
		return 0;
	*out_v = atof(at + strlen(key));
	return 1;
}

int krec_replay(const char *path, int *cols, int *rows, int *cell_w,
		int *cell_h, KrecFn fn, void *user)
{
	FILE *f = fopen(path, "rb");
	ZSTD_DStream *ds = ZSTD_createDStream();
	size_t incap = ZSTD_DStreamInSize(), outcap = ZSTD_DStreamOutSize();
	char *inb = malloc(incap), *outb = malloc(outcap);
	char *acc = NULL;
	size_t acc_len = 0, acc_cap = 0;
	int rc = -1, first = 1;
	double base = -1;

	if (!f || !ds || !inb || !outb) {
		fprintf(stderr, "kdos-view: cannot read %s\n", path);
		goto done;
	}
	ZSTD_initDStream(ds);

	for (;;) {
		size_t n = fread(inb, 1, incap, f);
		ZSTD_inBuffer in = { inb, n, 0 };

		if (!n)
			break;
		while (in.pos < in.size) {
			ZSTD_outBuffer ob = { outb, outcap, 0 };
			size_t r = ZSTD_decompressStream(ds, &ob, &in);

			if (ZSTD_isError(r)) {
				fprintf(stderr,
					"kdos-view: %s is not a recording\n",
					path);
				goto done;
			}
			if (acc_len + ob.pos + 1 > acc_cap) {
				size_t want = acc_cap ? acc_cap * 2 : 65536;

				while (want < acc_len + ob.pos + 1)
					want *= 2;

				char *bigger = realloc(acc, want);

				if (!bigger)
					goto done;
				acc = bigger;
				acc_cap = want;
			}
			memcpy(acc + acc_len, outb, ob.pos);
			acc_len += ob.pos;
			acc[acc_len] = '\0';

			/* Whole lines only: a compressed block ends wherever
			 * it ends, and half a line is not a message. */
			for (;;) {
				char *nl = memchr(acc, '\n', acc_len);
				size_t used;
				double t = 0, op = 0;

				if (!nl)
					break;
				*nl = '\0';
				used = (size_t)(nl - acc) + 1;

				if (first) {
					double v = 0;

					first = 0;
					if (!strstr(acc, KREC_MAGIC)) {
						fprintf(stderr,
							"kdos-view: %s is not a kdos-view recording\n",
							path);
						goto done;
					}
					/* A RECORDING IS THIS PROTOCOL'S OWN
					 * BYTES. Played by a build that speaks
					 * a different version they would be
					 * parsed as something else, so the
					 * mismatch is refused here. */
					if (field_num(acc, "\"proto\":", &v) &&
					    (int)v != KCON_VERSION) {
						fprintf(stderr,
							"kdos-view: %s is protocol %d, this is %d\n",
							path, (int)v,
							KCON_VERSION);
						goto done;
					}
					if (cols && field_num(acc, "\"cols\":", &v))
						*cols = (int)v;
					if (rows && field_num(acc, "\"rows\":", &v))
						*rows = (int)v;
					if (cell_w && field_num(acc, "\"cell_w\":", &v))
						*cell_w = (int)v;
					if (cell_h && field_num(acc, "\"cell_h\":", &v))
						*cell_h = (int)v;
				} else if (field_num(acc, "\"t\":", &t) &&
					   field_num(acc, "\"op\":", &op)) {
					const char *p = strstr(acc, "\"p\":\"");
					char *raw;
					size_t rawlen = 0;

					if (p) {
						p += 5;

						size_t b64len = strcspn(p, "\"");

						raw = malloc(b64len + 4);
						if (raw &&
						    kb_b64_decode(p, b64len,
								  raw,
								  b64len + 4,
								  &rawlen) >= 0) {
							/*
							 * THE CLOCK IS THE
							 * RECORDING'S. A player
							 * that drew everything
							 * at once would answer
							 * "what did this look
							 * like" and never
							 * "what happened".
							 */
							if (base < 0)
								base = now_s() - t;
							double wait = base + t - now_s();

							if (wait > 0 && wait < 10) {
								struct timespec ts = {
									(time_t)wait,
									(long)((wait - (double)(time_t)wait) * 1e9)
								};

								nanosleep(&ts, NULL);
							}
							if (fn)
								fn((unsigned)op,
								   raw, rawlen,
								   user);
						}
						free(raw);
					}
				}
				memmove(acc, acc + used, acc_len - used);
				acc_len -= used;
				acc[acc_len] = '\0';
			}
		}
	}
	rc = 0;
done:
	if (ds)
		ZSTD_freeDStream(ds);
	if (f)
		fclose(f);
	free(inb);
	free(outb);
	free(acc);
	return rc;
}

#else /* !KDOS_VIEW_RECORD */

#include <stdio.h>

int krec_open(const char *path, int cols, int rows, int cell_w, int cell_h)
{
	(void)path; (void)cols; (void)rows; (void)cell_w; (void)cell_h;
	fprintf(stderr, "kdos-view: this build has no record mode "
			"(built without zstd)\n");
	return -1;
}

void krec_msg(unsigned op, const char *payload, size_t len)
{
	(void)op; (void)payload; (void)len;
}

void krec_close(void)
{
}

int krec_replay(const char *path, int *cols, int *rows, int *cell_w,
		int *cell_h, KrecFn fn, void *user)
{
	(void)path; (void)cols; (void)rows; (void)cell_w; (void)cell_h;
	(void)fn; (void)user;
	fprintf(stderr, "kdos-view: this build has no replay mode "
			"(built without zstd)\n");
	return -1;
}

#endif /* KDOS_VIEW_RECORD */
