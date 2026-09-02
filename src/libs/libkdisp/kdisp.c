/* libkdisp — pick a display server, then forward to it. See kdisp.h. */

#include "kdisp.h"

static const KDispImpl *cur;

int
kdisp_init(const KDispConfig *cfg, const KDispImpl *const *impls, int n)
{
	cur = NULL;

	/*
	 * No implementations offered is not a failure: it is a caller that
	 * wants the terminal backend libktui already has, which is what a
	 * --tty flag and the rig's offscreen mode both mean.
	 */
	if (!impls || n <= 0)
		return 0;

	for (int i = 0; i < n; i++) {
		const KDispImpl *im = impls[i];

		if (!im || !im->init)
			continue;
		/* Probed before init because init has side effects and this
		 * loop runs over implementations that will not be used. */
		if (im->probe && !im->probe())
			continue;
		if (im->init(cfg) != 0)
			continue;

		cur = im;
		return 0;
	}

	return -1;
}

const KDispImpl *
kdisp_current(void)
{
	return cur;
}

/*
 * A server that cannot answer leaves its entry NULL and gets the neutral
 * answer, not a crash: a console has no server-side decoration to report and
 * nothing to hand out in place of a Wayland handle. Every caller of these
 * already copes with the neutral value, because it is what a terminal returns.
 */
#define FWD_VOID(name, member)                 \
	void kdisp_##name(void)                \
	{                                      \
		if (cur && cur->member)        \
			cur->member();         \
	}

#define FWD_INT(name, member, dflt)            \
	int kdisp_##name(void)                 \
	{                                      \
		if (cur && cur->member)        \
			return cur->member();  \
		return (dflt);                 \
	}

FWD_VOID(shutdown, shutdown)
FWD_VOID(pump, pump)
FWD_VOID(overlay_hide, overlay_hide)
FWD_VOID(report_error, report_error)
FWD_VOID(unlock, unlock)

/* should_close is 0 on a backend with no window to close: a terminal program
 * ends because its own loop decided to, never because a server said so. */
FWD_INT(should_close, should_close, 0)
FWD_INT(fd, fd, -1)
FWD_INT(cell_w, cell_w, 1)
FWD_INT(cell_h, cell_h, 1)
FWD_INT(px_h, px_h, 0)
FWD_INT(scale, scale, 1)
FWD_INT(decorated, decorated, 0)
FWD_INT(popup_offset, popup_offset, 0)
FWD_INT(edge_bottom, edge_bottom, 0)
FWD_INT(lock_engaged, lock_engaged, 0)
FWD_INT(lock_finished, lock_finished, 0)

int
kdisp_overlay_resize(int cols, int rows)
{
	if (cur && cur->overlay_resize)
		return cur->overlay_resize(cols, rows);
	return -1;
}

int
kdisp_overlay_show(int cols, int rows)
{
	if (cur && cur->overlay_show)
		return cur->overlay_show(cols, rows);
	return -1;
}

void
kdisp_layer_autohide(bool hidden)
{
	if (cur && cur->layer_autohide)
		cur->layer_autohide(hidden);
}

int
kdisp_copy(const char *text, size_t len, int primary)
{
	if (cur && cur->copy)
		return cur->copy(text, len, primary);
	return -1;
}

int
kdisp_drag_start(const char *mime, const char *data, size_t len)
{
	if (cur && cur->drag_start)
		return cur->drag_start(mime, data, len);
	return -1;
}

void
kdisp_cursor_set(enum kdisp_cursor c)
{
	if (cur && cur->cursor_set)
		cur->cursor_set(c);
}

void
kdisp_set_backdrop(KDispBackdropFn fn)
{
	if (cur && cur->set_backdrop)
		cur->set_backdrop(fn);
}

void
kdisp_input_cells(const KRect *rects, int n)
{
	if (cur && cur->input_cells)
		cur->input_cells(rects, n);
}
