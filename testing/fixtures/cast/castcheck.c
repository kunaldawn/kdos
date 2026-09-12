/*
 * castcheck — the consumer half of `kdos-view --cast`, as a test.
 *
 * A SECOND PROCESS for the reason embedcheck is one: a PipeWire node, a format
 * negotiation and a shared buffer are real daemon and library behaviours, and a
 * mock would only assert about itself. A stream that registers a node and never
 * produces a frame is the failure this exists to catch — the node alone proves
 * nothing, because nothing has to be rendered for one to appear.
 *
 *   castcheck --target 31 [--out frame.ppm] [--seconds 5]
 *
 * Prints the first frame's size and how many DISTINCT colours are in it, then
 * exits 0. The colour count is the assertion: a frame of one colour is a
 * cleared buffer, and a desktop rasterised through the cell painter is never
 * one colour.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pipewire/pipewire.h>
#include <spa/param/video/format-utils.h>

static struct pw_main_loop *loop;
static struct pw_stream *stream;
static struct spa_video_info_raw fmt;
static const char *out_path;
static int got_frame;

static void on_param_changed(void *data, uint32_t id, const struct spa_pod *param)
{
	uint32_t mt, ms;

	(void)data;
	if (!param || id != SPA_PARAM_Format)
		return;
	if (spa_format_parse(param, &mt, &ms) < 0)
		return;
	if (mt != SPA_MEDIA_TYPE_video || ms != SPA_MEDIA_SUBTYPE_raw)
		return;
	spa_format_video_raw_parse(param, &fmt);
}

/* Distinct colours, counted into a small open table. A cap of 64 is plenty:
 * the question is "more than one", and an exact count past that says nothing
 * extra. */
static int distinct(const uint32_t *px, long n)
{
	uint32_t seen[64];
	int ns = 0;

	for (long i = 0; i < n; i++) {
		int k;

		for (k = 0; k < ns; k++)
			if (seen[k] == (px[i] & 0xffffffu))
				break;
		if (k == ns) {
			if (ns == 64)
				return 64;
			seen[ns++] = px[i] & 0xffffffu;
		}
	}
	return ns;
}

static void on_process(void *data)
{
	struct pw_buffer *b;
	struct spa_buffer *sb;

	(void)data;
	b = pw_stream_dequeue_buffer(stream);
	if (!b)
		return;
	sb = b->buffer;

	if (sb->datas[0].data && sb->datas[0].chunk->size > 0 && !got_frame) {
		const uint32_t *px = sb->datas[0].data;
		int w = (int)fmt.size.width, h = (int)fmt.size.height;
		long n = (long)w * h;

		if (n > 0 &&
		    sb->datas[0].chunk->size >= (uint32_t)(n * 4)) {
			printf("castcheck: frame %dx%d stride %d, %d distinct colours\n",
			       w, h, sb->datas[0].chunk->stride,
			       distinct(px, n));

			if (out_path) {
				FILE *f = fopen(out_path, "wb");

				if (f) {
					fprintf(f, "P6\n%d %d\n255\n", w, h);
					for (long i = 0; i < n; i++) {
						unsigned char c[3] = {
							(px[i] >> 16) & 0xff,
							(px[i] >> 8) & 0xff,
							px[i] & 0xff
						};
						fwrite(c, 1, 3, f);
					}
					fclose(f);
				}
			}
			got_frame = 1;
			pw_main_loop_quit(loop);
		}
	}
	pw_stream_queue_buffer(stream, b);
}

static void on_state(void *data, enum pw_stream_state old,
		     enum pw_stream_state state, const char *error)
{
	(void)data;
	fprintf(stderr, "castcheck: %s -> %s%s%s\n",
		pw_stream_state_as_string(old), pw_stream_state_as_string(state),
		error ? ": " : "", error ? error : "");
}

static const struct pw_stream_events events = {
	PW_VERSION_STREAM_EVENTS,
	.state_changed = on_state,
	.param_changed = on_param_changed,
	.process = on_process,
};

static void timeout_cb(void *data, uint64_t expirations)
{
	(void)data;
	(void)expirations;
	pw_main_loop_quit(loop);
}

int main(int argc, char **argv)
{
	uint32_t target = PW_ID_ANY;
	int seconds = 5;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--target") && i + 1 < argc)
			target = (uint32_t)strtoul(argv[++i], NULL, 10);
		else if (!strcmp(argv[i], "--out") && i + 1 < argc)
			out_path = argv[++i];
		else if (!strcmp(argv[i], "--seconds") && i + 1 < argc)
			seconds = atoi(argv[++i]);
		else {
			fprintf(stderr, "castcheck: unknown option '%s'\n",
				argv[i]);
			return 2;
		}
	}

	pw_init(&argc, &argv);
	loop = pw_main_loop_new(NULL);
	if (!loop)
		return 1;

	/*
	 * THE TARGET IS A PROPERTY, not the id argument below. Passing a node
	 * id to pw_stream_connect is the old way and a modern PipeWire ignores
	 * it — the stream then connects to nothing, sits in `paused`, and looks
	 * exactly like a producer that never rendered.
	 */
	char tgt[32];
	struct pw_properties *props =
		pw_properties_new(PW_KEY_MEDIA_TYPE, "Video",
				  PW_KEY_MEDIA_CATEGORY, "Capture",
				  PW_KEY_MEDIA_ROLE, "Screen", NULL);

	if (target != PW_ID_ANY) {
		snprintf(tgt, sizeof(tgt), "%u", target);
		pw_properties_set(props, PW_KEY_TARGET_OBJECT, tgt);
	}

	stream = pw_stream_new_simple(pw_main_loop_get_loop(loop), "castcheck",
				      props, &events, NULL);
	if (!stream)
		return 1;

	uint8_t buf[1024];
	struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buf, sizeof(buf));
	const struct spa_pod *params[1];
	struct spa_rectangle rmin = SPA_RECTANGLE(1, 1);
	struct spa_rectangle rdef = SPA_RECTANGLE(640, 480);
	struct spa_rectangle rmax = SPA_RECTANGLE(8192, 8192);
	struct spa_fraction fmin = SPA_FRACTION(0, 1);
	struct spa_fraction fdef = SPA_FRACTION(30, 1);
	struct spa_fraction fmax = SPA_FRACTION(240, 1);

	params[0] = spa_pod_builder_add_object(&b,
		SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat,
		SPA_FORMAT_mediaType, SPA_POD_Id(SPA_MEDIA_TYPE_video),
		SPA_FORMAT_mediaSubtype, SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw),
		SPA_FORMAT_VIDEO_format, SPA_POD_Id(SPA_VIDEO_FORMAT_BGRx),
		SPA_FORMAT_VIDEO_size,
		SPA_POD_CHOICE_RANGE_Rectangle(&rdef, &rmin, &rmax),
		SPA_FORMAT_VIDEO_framerate,
		SPA_POD_CHOICE_RANGE_Fraction(&fdef, &fmin, &fmax));

	if (pw_stream_connect(stream, PW_DIRECTION_INPUT, PW_ID_ANY,
			      PW_STREAM_FLAG_AUTOCONNECT |
				      PW_STREAM_FLAG_MAP_BUFFERS,
			      params, 1) < 0) {
		fprintf(stderr, "castcheck: cannot connect\n");
		return 1;
	}

	struct spa_source *t = pw_loop_add_timer(pw_main_loop_get_loop(loop),
						 timeout_cb, NULL);
	struct timespec ts = { .tv_sec = seconds, .tv_nsec = 0 };

	pw_loop_update_timer(pw_main_loop_get_loop(loop), t, &ts, NULL, false);

	pw_main_loop_run(loop);

	if (!got_frame) {
		fprintf(stderr, "castcheck: no frame arrived\n");
		return 1;
	}
	return 0;
}
