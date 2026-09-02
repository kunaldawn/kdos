/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-view --cast — a view nobody looks at
 *
 * A SCREENCAST IS A VIEW. The session holds cells and a view is what turns them
 * into pixels, so recording the console desktop is a second view that
 * rasterises exactly as the KMS one does — through libkcell, at the same cell
 * size, with the same glyph cache — and writes the result into a PipeWire
 * stream instead of onto a screen. There is no second renderer and no capture
 * path beside the drawing path.
 *
 * IT IMPOSES NO GRID SIZE. A view that asked for one would resize the desktop
 * when somebody started recording it, which is the difference between taking a
 * picture of a room and rearranging it first.
 *
 * IT SENDS NO INPUT. A recording is not a seat.
 *
 * DAMAGE DRIVES THE FRAMES: a still desktop produces none. That is the rule the
 * rig already lives under, and it is why a recording of a terminal nobody is
 * typing in costs nothing.
 * ---------------------------------
 */

#include "cast.h"

#ifdef KDOS_VIEW_CAST

#include <stdio.h>
#include <string.h>

#include <pipewire/pipewire.h>
#include <spa/param/video/format-utils.h>

static struct pw_main_loop *loop;
static struct pw_stream *stream;
static int cast_w, cast_h;
static int negotiated;
static int hungry;
/* The frame waiting to go out, or NULL. Borrowed: the view owns one buffer for
 * the life of the stream and paints into it in place. */
static const uint32_t *pending;
static unsigned long frames;

static void on_state(void *data, enum pw_stream_state old,
		     enum pw_stream_state state, const char *error)
{
	(void)data;
	(void)old;
	if (state == PW_STREAM_STATE_ERROR)
		fprintf(stderr, "kdos-view: cast stream error: %s\n",
			error ? error : "(none)");
}

/*
 * The consumer agreed a format; now it has to be told how big a buffer is.
 * PipeWire will not allocate one for a size nobody stated, so a stream that
 * skips this negotiates a format and then never gets a buffer to fill.
 */
static void on_param_changed(void *data, uint32_t id,
			     const struct spa_pod *param)
{
	uint8_t buf[512];
	struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buf, sizeof(buf));
	const struct spa_pod *params[1];
	struct spa_video_info info;
	uint32_t mt, ms;

	(void)data;
	if (!param || id != SPA_PARAM_Format)
		return;
	if (spa_format_parse(param, &mt, &ms) < 0)
		return;
	if (mt != SPA_MEDIA_TYPE_video || ms != SPA_MEDIA_SUBTYPE_raw)
		return;

	memset(&info, 0, sizeof(info));
	if (spa_format_video_raw_parse(param, &info.info.raw) < 0)
		return;

	int stride = (int)info.info.raw.size.width * 4;
	int size = stride * (int)info.info.raw.size.height;

	if (stride <= 0 || size <= 0)
		return;

	cast_w = (int)info.info.raw.size.width;
	cast_h = (int)info.info.raw.size.height;

	params[0] = spa_pod_builder_add_object(&b,
		SPA_TYPE_OBJECT_ParamBuffers, SPA_PARAM_Buffers,
		SPA_PARAM_BUFFERS_buffers, SPA_POD_CHOICE_RANGE_Int(4, 2, 8),
		SPA_PARAM_BUFFERS_blocks, SPA_POD_Int(1),
		SPA_PARAM_BUFFERS_size, SPA_POD_Int(size),
		SPA_PARAM_BUFFERS_stride, SPA_POD_Int(stride),
		SPA_PARAM_BUFFERS_dataType,
		SPA_POD_CHOICE_FLAGS_Int(1 << SPA_DATA_MemPtr));

	pw_stream_update_params(stream, params, 1);
	negotiated = 1;
	hungry = 1;
}

/*
 * THE FRAME PIPEWIRE ASKS FOR, when it asks. The graph runs at the rate the
 * consumer negotiated and the desktop changes at its own; `pending` is the
 * bridge. A cycle with nothing new queues a buffer whose chunk is EMPTY, which
 * is PipeWire's way of saying "no new data" — so a still desktop costs a
 * scheduled cycle and not a copy, and a consumer sees a frame only when
 * something actually changed.
 */
static void on_process(void *data)
{
	struct pw_buffer *pb;
	struct spa_buffer *sb;
	uint8_t *dst;
	size_t stride, need;

	(void)data;
	pb = pw_stream_dequeue_buffer(stream);
	if (!pb)
		return;

	sb = pb->buffer;
	dst = sb->datas[0].data;
	stride = (size_t)cast_w * 4;
	need = stride * (size_t)cast_h;

	if (pending && dst && sb->datas[0].maxsize >= need) {
		memcpy(dst, pending, need);
		sb->datas[0].chunk->offset = 0;
		sb->datas[0].chunk->stride = (int32_t)stride;
		sb->datas[0].chunk->size = (uint32_t)need;
		pending = NULL;
		frames++;
	} else {
		sb->datas[0].chunk->offset = 0;
		sb->datas[0].chunk->stride = (int32_t)stride;
		sb->datas[0].chunk->size = 0;
	}

	pw_stream_queue_buffer(stream, pb);
}

static const struct pw_stream_events cast_events = {
	PW_VERSION_STREAM_EVENTS,
	.state_changed = on_state,
	.param_changed = on_param_changed,
	.process = on_process,
};

int kcast_init(int w, int h, int fps)
{
	uint8_t buf[1024];
	struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buf, sizeof(buf));
	const struct spa_pod *params[1];
	struct spa_rectangle size = SPA_RECTANGLE((uint32_t)w, (uint32_t)h);
	struct spa_fraction rate = SPA_FRACTION((uint32_t)fps, 1);

	if (w <= 0 || h <= 0 || fps <= 0)
		return -1;

	cast_w = w;
	cast_h = h;

	pw_init(NULL, NULL);
	loop = pw_main_loop_new(NULL);
	if (!loop)
		return -1;

	/*
	 * A Video/Source node with the Screen role, which is what a screencast
	 * consumer looks for. The portal hands its node id to the application;
	 * everything else about the stream is ordinary PipeWire.
	 */
	struct pw_properties *props = pw_properties_new(
		PW_KEY_MEDIA_CLASS, "Video/Source",
		PW_KEY_MEDIA_ROLE, "Screen",
		PW_KEY_NODE_NAME, "kdos-console",
		PW_KEY_NODE_DESCRIPTION, "KDOS console session",
		NULL);

	stream = pw_stream_new_simple(pw_main_loop_get_loop(loop),
				      "kdos-console", props, &cast_events,
				      NULL);
	if (!stream)
		return -1;

	/*
	 * BGRx, because that is what the cell painter produces. pixman's
	 * a8r8g8b8 is 0xAARRGGBB in a word, whose bytes on a little-endian
	 * machine are B, G, R, A — so no conversion happens anywhere on this
	 * path, which is the point of choosing it.
	 */
	params[0] = spa_pod_builder_add_object(&b,
		SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat,
		SPA_FORMAT_mediaType, SPA_POD_Id(SPA_MEDIA_TYPE_video),
		SPA_FORMAT_mediaSubtype, SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw),
		SPA_FORMAT_VIDEO_format, SPA_POD_Id(SPA_VIDEO_FORMAT_BGRx),
		SPA_FORMAT_VIDEO_size, SPA_POD_Rectangle(&size),
		SPA_FORMAT_VIDEO_framerate, SPA_POD_Fraction(&rate));

	/*
	 * NOT A DRIVER. A driver node schedules the graph itself and has to be
	 * triggered by hand; a source that is scheduled by its consumer is the
	 * ordinary shape, and the rate is then the one the consumer asked for
	 * rather than one this program invented.
	 */
	if (pw_stream_connect(stream, PW_DIRECTION_OUTPUT, PW_ID_ANY,
			      PW_STREAM_FLAG_MAP_BUFFERS, params, 1) < 0) {
		fprintf(stderr, "kdos-view: cannot connect the cast stream — "
				"is PipeWire running?\n");
		return -1;
	}

	/*
	 * WAIT FOR THE NODE ID. It does not exist at connect — the server
	 * assigns it when it registers the node — so a caller that read it
	 * straight away would read SPA_ID_INVALID, and a portal handing that
	 * number to an application would be handing it nothing. Two seconds is
	 * far longer than a local registration and short enough that a PipeWire
	 * that is not answering is reported rather than waited on.
	 */
	for (int spin = 0; spin < 100; spin++) {
		if (pw_stream_get_node_id(stream) != SPA_ID_INVALID)
			return 0;
		pw_loop_iterate(pw_main_loop_get_loop(loop), 20);
	}

	fprintf(stderr, "kdos-view: PipeWire never registered the cast node\n");
	return -1;
}

int kcast_fd(void)
{
	return loop ? pw_loop_get_fd(pw_main_loop_get_loop(loop)) : -1;
}

void kcast_pump(void)
{
	if (loop)
		pw_loop_iterate(pw_main_loop_get_loop(loop), 0);
}

uint32_t kcast_node_id(void)
{
	return stream ? pw_stream_get_node_id(stream) : SPA_ID_INVALID;
}

int kcast_hungry(void)
{
	int h = hungry;

	hungry = 0;
	return h;
}

int kcast_push(const uint32_t *argb)
{
	if (!stream || !negotiated || !argb)
		return 0;
	pending = argb;
	return 1;
}

unsigned long kcast_frames(void)
{
	return frames;
}

void kcast_finish(void)
{
	if (stream) {
		pw_stream_destroy(stream);
		stream = NULL;
	}
	if (loop) {
		pw_main_loop_destroy(loop);
		loop = NULL;
	}
	pw_deinit();
}

#endif /* KDOS_VIEW_CAST */
