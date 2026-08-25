/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   privacy.c — which app is using your microphone, by name
 *
 * Every phone OS answers this and no Linux desktop does. The reason is not
 * difficulty: PipeWire knows the capture node, the node knows the client, the
 * client knows its own name, and the panel knows how to draw. It is four owners
 * for one indicator, and nobody owns all four. KDOS does.
 *
 * TWO SOURCES, because there are two ways to record and only one of them goes
 * through PipeWire:
 *
 *   microphone — a PipeWire node with media.class Stream/Input/Audio, counted
 *                only while its state is RUNNING. A node that exists but is
 *                idle is an app that has OPENED the mic, not one that is
 *                listening, and an indicator that cannot tell those apart is
 *                an indicator nobody believes twice.
 *   camera     — a process holding an open fd on /dev/video*, found by walking
 *                /proc. Almost no app takes the camera through the portal; they
 *                open the device. PipeWire would report nothing at all here.
 *                Plus a Stream/Input/Video node whose props say media.role
 *                Camera — the libcamera/portal route, deduped against the walk
 *                by application.process.id where the app published one, and
 *                counted twice (`+1`) where it did not.
 *   screen     — every other Stream/Input/Video node: a portal ScreenCast
 *                consumer is exactly that shape, and it is somebody watching
 *                the SCREEN, which deserves its own lamp rather than the
 *                camera's.
 *
 * The name comes from `application.name` where PipeWire has it (which is the
 * app's own idea of what it is called — "Google Chrome", not "chrome"), from
 * node.name where it does not, and from /proc/<pid>/comm for the camera.
 *
 * What this file will NOT do: claim a device is in use because a process has it
 * open. An open fd on a camera IS use — there is no other reason to hold one —
 * but an open PipeWire node is not, which is why the two halves are counted
 * differently rather than uniformly.
 */

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <pipewire/pipewire.h>

#include "kproc.h"
#include "shell.h"

/* The camera walk is /proc, not an event source: it costs a readdir per process
 * and there is nothing to subscribe to. Twice a second would be waste; every
 * two seconds is faster than a human notices a light. */
#define CAM_SCAN_INTERVAL 2

struct priv_node {
	struct spa_list link;
	struct sh_priv *priv;
	uint32_t id;
	struct pw_proxy *proxy;
	struct spa_hook proxy_hook;
	struct spa_hook node_hook;
	int kind;			/* SH_PRIV_MIC, _CAM or _SCR */
	int running;
	int pid;			/* application.process.id, or 0 */
	char name[SH_PRIV_NAME];
};

struct sh_priv {
	struct pw_loop *loop;
	struct pw_context *ctx;
	struct pw_core *core;
	struct pw_registry *registry;
	struct spa_hook registry_hook;
	struct spa_hook core_hook;
	int connected;
	int broken;
	int synced;
	struct spa_list nodes;

	/* The camera half, refreshed on its own clock. */
	struct sh_priv_use cam[SH_MAX_PRIV];
	int ncam;
	time_t cam_scanned;
};

/* ── the camera: /proc, because the portal is not how anyone uses one ───── */

static const char *proc_root(void)
{
	/* The same trick `kdos stutter --fixture` uses: the walk reads from a
	 * directory that defaults to /proc and does not have to be one, which is
	 * what makes this testable without a camera. */
	const char *p = getenv("KDOS_PRIVACY_PROC");
	return (p && *p) ? p : "/proc";
}

static void read_comm(const char *root, const char *pid, char *out, size_t len)
{
	char path[512];
	snprintf(path, sizeof(path), "%s/%s/comm", root, pid);
	if (sh_read_line(path, out, len) != 0)
		snprintf(out, len, "pid %.16s", pid);
}

/*
 * Is this pid already counted as a CAMERA by the graph? Then the /proc walk
 * must not count the same app a second time — one recording, one lamp.
 *
 * Only against a node that is actually being counted, which is what the dedupe
 * MEANS: any video node used to match, so OBS sharing the screen (a SCR node
 * carrying the same pid) hid the webcam it was also holding — dark CAM lamp
 * with the camera live, and dark in both lamps when that node was idle. The
 * dedupe works only for an app that published `application.process.id`; one
 * that did not is counted by both halves and shows as `+1`.
 */
static int pw_video_pid(struct sh_priv *p, int pid)
{
	struct priv_node *n;
	if (pid <= 0)
		return 0;
	spa_list_for_each(n, &p->nodes, link)
		if (n->pid == pid && n->kind == SH_PRIV_CAM && n->running)
			return 1;
	return 0;
}

static void scan_cameras(struct sh_priv *p)
{
	const char *root = proc_root();
	/* Sized for the longest path this builds — the compiler is right that a
	 * 512-byte root plus a pid plus an fd name does not fit in 512. */
	char path[1024], target[512];
	DIR *d = opendir(root);
	struct dirent *e;

	p->ncam = 0;
	if (!d)
		return;
	while ((e = readdir(d)) && p->ncam < SH_MAX_PRIV) {
		if (e->d_name[0] < '0' || e->d_name[0] > '9')
			continue;
		snprintf(path, sizeof(path), "%s/%s/fd", root, e->d_name);
		DIR *fds = opendir(path);
		struct dirent *f;
		if (!fds)
			continue;	/* another user's process: not ours to see */
		while ((f = readdir(fds))) {
			if (f->d_name[0] == '.')
				continue;
			snprintf(path, sizeof(path), "%s/%s/fd/%s", root,
				 e->d_name, f->d_name);
			ssize_t n = readlink(path, target, sizeof(target) - 1);
			if (n <= 0)
				continue;
			target[n] = 0;
			if (strncmp(target, "/dev/video", 10))
				continue;
			if (pw_video_pid(p, atoi(e->d_name)))
				break;	/* the graph already names this app */

			struct sh_priv_use *u = &p->cam[p->ncam];
			u->pid = atoi(e->d_name);
			read_comm(root, e->d_name, u->name, sizeof(u->name));
			/* One entry per app, not per fd: a webcam app opens the
			 * device several times and would otherwise fill the
			 * panel with its own name. */
			int dup = 0;
			for (int i = 0; i < p->ncam; i++)
				if (p->cam[i].pid == u->pid)
					dup = 1;
			if (!dup)
				p->ncam++;
			break;
		}
		closedir(fds);
	}
	closedir(d);
}

/* ── the microphone: the PipeWire graph ────────────────────────────────── */

static void node_info(void *data, const struct pw_node_info *info)
{
	struct priv_node *n = data;
	const char *name;

	if (info->change_mask & PW_NODE_CHANGE_MASK_STATE)
		n->running = info->state == PW_NODE_STATE_RUNNING;
	if (!(info->change_mask & PW_NODE_CHANGE_MASK_PROPS) || !info->props)
		return;
	/* The app's own idea of its name first: "Google Chrome" rather than
	 * "chrome". node.name is the fallback and is what a plain
	 * `pw-cat --record` has. */
	name = spa_dict_lookup(info->props, PW_KEY_APP_NAME);
	if (!name)
		name = spa_dict_lookup(info->props, PW_KEY_NODE_NAME);
	if (name)
		snprintf(n->name, sizeof(n->name), "%s", name);
	name = spa_dict_lookup(info->props, PW_KEY_APP_PROCESS_ID);
	if (name)
		n->pid = atoi(name);
}

static const struct pw_node_events node_events = {
	PW_VERSION_NODE_EVENTS,
	.info = node_info,
};

static void node_proxy_removed(void *data)
{
	struct priv_node *n = data;
	pw_proxy_destroy(n->proxy);
}

/*
 * Unlinked, NOT freed. The node record lives in the proxy's own user data
 * (`pw_registry_bind` allocates it there), so freeing it here would hand
 * PipeWire's allocator a pointer into the middle of its own object.
 */
static void node_proxy_destroy(void *data)
{
	struct priv_node *n = data;
	spa_list_remove(&n->link);
}

static const struct pw_proxy_events proxy_events = {
	PW_VERSION_PROXY_EVENTS,
	.removed = node_proxy_removed,
	.destroy = node_proxy_destroy,
};

static void registry_global(void *data, uint32_t id, uint32_t permissions,
			    const char *type, uint32_t version,
			    const struct spa_dict *props)
{
	struct sh_priv *p = data;
	const char *cls;
	int kind;
	(void)permissions;
	(void)version;

	if (strcmp(type, PW_TYPE_INTERFACE_Node) || !props)
		return;
	cls = spa_dict_lookup(props, PW_KEY_MEDIA_CLASS);
	if (!cls)
		return;
	/*
	 * A STREAM, not a device. `Audio/Source` is the microphone itself and is
	 * always there; `Stream/Input/Audio` is an application connected to one,
	 * which is the only thing worth lighting a lamp for.
	 *
	 * A video stream is only the CAMERA when its props say so: a portal
	 * ScreenCast consumer (OBS sharing the screen) is exactly a
	 * `Stream/Input/Video` node too, and lighting the CAM lamp for it is
	 * how an indicator teaches people not to believe it. Everything not
	 * declaring media.role=Camera is SCR — screen capture, its own warning.
	 */
	if (!strcmp(cls, "Stream/Input/Audio")) {
		kind = SH_PRIV_MIC;
	} else if (!strcmp(cls, "Stream/Input/Video")) {
		const char *role = spa_dict_lookup(props, PW_KEY_MEDIA_ROLE);
		kind = (role && !strcasecmp(role, "Camera")) ? SH_PRIV_CAM
							     : SH_PRIV_SCR;
	} else {
		return;
	}

	struct pw_proxy *proxy = pw_registry_bind(p->registry, id, type,
						  PW_VERSION_NODE,
						  sizeof(struct priv_node));
	if (!proxy)
		return;

	struct priv_node *n = pw_proxy_get_user_data(proxy);
	memset(n, 0, sizeof(*n));
	n->priv = p;
	n->id = id;
	n->proxy = proxy;
	n->kind = kind;
	const char *name = spa_dict_lookup(props, PW_KEY_APP_NAME);
	if (!name)
		name = spa_dict_lookup(props, PW_KEY_NODE_NAME);
	snprintf(n->name, sizeof(n->name), "%s", name ? name : "an app");
	/* The pid, when the app published one — what lets the /proc camera walk
	 * skip processes this graph already names, instead of counting them
	 * twice. */
	const char *pid = spa_dict_lookup(props, PW_KEY_APP_PROCESS_ID);
	if (pid)
		n->pid = atoi(pid);
	spa_list_append(&p->nodes, &n->link);

	pw_proxy_add_listener(proxy, &n->proxy_hook, &proxy_events, n);
	pw_node_add_listener((struct pw_node *)proxy, &n->node_hook,
			     &node_events, n);
}

static void registry_global_remove(void *data, uint32_t id)
{
	struct sh_priv *p = data;
	struct priv_node *n;
	spa_list_for_each(n, &p->nodes, link)
		if (n->id == id) {
			pw_proxy_destroy(n->proxy);
			return;
		}
}

static const struct pw_registry_events registry_events = {
	PW_VERSION_REGISTRY_EVENTS,
	.global = registry_global,
	.global_remove = registry_global_remove,
};

/*
 * A dead PipeWire must not take the panel with it. The error is recorded and
 * the connection dropped on the next dispatch; the indicator goes dark, which
 * is the truthful state — with no graph to read, nothing is known to be
 * recording.
 */
static void core_error(void *data, uint32_t id, int seq, int res,
		       const char *message)
{
	struct sh_priv *p = data;
	(void)seq; (void)message;
	if (id == PW_ID_CORE && res < 0)
		p->broken = 1;
}

/* The other half of a roundtrip: `done` for the sequence `pw_core_sync` asked
 * for. Only `--dump` waits on it — the live panel learns the graph a frame or
 * two later and nobody can see the difference. */
static void core_done(void *data, uint32_t id, int seq)
{
	struct sh_priv *p = data;
	(void)seq;
	if (id == PW_ID_CORE)
		p->synced = 1;
}

static const struct pw_core_events core_events = {
	PW_VERSION_CORE_EVENTS,
	.error = core_error,
	.done = core_done,
};

/* ── the public half ───────────────────────────────────────────────────── */

static void priv_disconnect(struct sh_priv *p)
{
	struct priv_node *n, *tmp;
	spa_list_for_each_safe(n, tmp, &p->nodes, link)
		pw_proxy_destroy(n->proxy);
	if (p->registry)
		pw_proxy_destroy((struct pw_proxy *)p->registry);
	if (p->core)
		pw_core_disconnect(p->core);
	if (p->ctx)
		pw_context_destroy(p->ctx);
	p->registry = NULL;
	p->core = NULL;
	p->ctx = NULL;
	p->connected = 0;
	p->broken = 0;
}

static void priv_connect(struct sh_priv *p)
{
	p->ctx = pw_context_new(p->loop, NULL, 0);
	if (!p->ctx)
		return;
	p->core = pw_context_connect(p->ctx, NULL, 0);
	if (!p->core) {
		pw_context_destroy(p->ctx);
		p->ctx = NULL;
		return;
	}
	pw_core_add_listener(p->core, &p->core_hook, &core_events, p);
	p->registry = pw_core_get_registry(p->core, PW_VERSION_REGISTRY, 0);
	if (!p->registry) {
		priv_disconnect(p);
		return;
	}
	pw_registry_add_listener(p->registry, &p->registry_hook,
				 &registry_events, p);
	p->connected = 1;
}

int sh_priv_init(struct sh_state *sh)
{
	struct sh_priv *p = calloc(1, sizeof(*p));
	if (!p)
		return -1;
	sh->priv = p;
	spa_list_init(&p->nodes);

	pw_init(NULL, NULL);
	p->loop = pw_loop_new(NULL);
	if (!p->loop) {
		sh_priv_free(sh);
		return -1;
	}
	priv_connect(p);
	/* Not an error if it failed: a session without PipeWire is a session
	 * where nothing can be recording through it, and the camera half works
	 * regardless. */
	scan_cameras(p);
	p->cam_scanned = time(NULL);
	return 0;
}

void sh_priv_dispatch(struct sh_state *sh)
{
	struct sh_priv *p = sh->priv;
	time_t now;

	if (!p)
		return;
	if (p->connected) {
		/*
		 * Non-blocking, and four times rather than once: a stream that
		 * has just appeared takes a bind and an info event to become
		 * known, and one pass per panel tick would put a RECORDING
		 * indicator two seconds behind the recording. Four empty polls
		 * cost nothing on an idle graph. Measured: one tick instead of
		 * two.
		 */
		for (int i = 0; i < 4; i++)
			pw_loop_iterate(p->loop, 0);
		if (p->broken)
			priv_disconnect(p);
	}
	now = time(NULL);
	if (now - p->cam_scanned >= CAM_SCAN_INTERVAL) {
		p->cam_scanned = now;
		scan_cameras(p);
		/* One reconnect attempt per camera scan, so a PipeWire that
		 * came up late is picked up without a retry timer of its own. */
		if (!p->connected)
			priv_connect(p);
	}
}

/*
 * Pump until the graph has actually arrived, or the budget runs out.
 *
 * A single non-blocking iterate is not enough to KNOW anything: connecting,
 * receiving the registry, binding a node and receiving its info are four
 * roundtrips. `kdos-shell --dump` renders exactly one frame, so it has to wait
 * for them — a dump that reports "nothing is recording" because it asked too
 * early is worse than no dump at all. Three rounds: globals, binds, infos.
 */
void sh_priv_settle(struct sh_state *sh, int ms)
{
	struct sh_priv *p = sh->priv;
	struct timespec t0, now;

	if (!p || !p->connected)
		return;
	clock_gettime(CLOCK_MONOTONIC, &t0);
	for (int round = 0; round < 3; round++) {
		p->synced = 0;
		pw_core_sync(p->core, PW_ID_CORE, 0);
		while (!p->synced) {
			clock_gettime(CLOCK_MONOTONIC, &now);
			long el = (now.tv_sec - t0.tv_sec) * 1000 +
				  (now.tv_nsec - t0.tv_nsec) / 1000000;
			if (el >= ms || p->broken)
				return;
			pw_loop_iterate(p->loop, 20);
		}
	}
}

int sh_priv_count(const struct sh_state *sh, int kind)
{
	struct sh_priv *p = sh->priv;
	int n = 0;

	if (!p)
		return 0;
	if (kind == SH_PRIV_CAM)
		n += p->ncam;
	struct priv_node *node;
	spa_list_for_each(node, &p->nodes, link)
		if (node->kind == kind && node->running)
			n++;
	return n;
}

const char *sh_priv_name(const struct sh_state *sh, int kind)
{
	struct sh_priv *p = sh->priv;

	if (!p)
		return NULL;
	struct priv_node *node;
	spa_list_for_each(node, &p->nodes, link)
		if (node->kind == kind && node->running && node->name[0])
			return node->name;
	if (kind == SH_PRIV_CAM && p->ncam > 0)
		return p->cam[0].name;
	return NULL;
}

/*
 * WHICH BOX IS RECORDING, for the tooltip and not for the bar.
 *
 * On a machine where every fat application is a container, "firefox-esr is
 * recording" leaves out the half that says WHICH firefox — and with the pack
 * lane there can legitimately be two. The name is PipeWire's, chosen by the
 * app; the box is libkproc's conmon walk over the pid the app published, which
 * is the same identity kdos stutter, kdos-energyd and kdos-oomd use, so a user
 * box needs nothing added here to be named.
 *
 * It is the TOOLTIP's, never the panel row's: the applet is three cells and a
 * box suffix there would push the meters strip off an eighty-column bar.
 */
int sh_priv_box(const struct sh_state *sh, int kind, char *out, size_t n)
{
	struct sh_priv *p = sh->priv;
	struct priv_node *node;
	int pid = 0;

	if (out && n)
		*out = '\0';
	if (!p || !out || !n)
		return 0;
	spa_list_for_each(node, &p->nodes, link)
		if (node->kind == kind && node->running && node->pid > 0) {
			pid = node->pid;
			break;
		}
	if (!pid && kind == SH_PRIV_CAM && p->ncam > 0)
		pid = p->cam[0].pid;
	if (pid <= 0)
		return 0;
	return kpr_box_of_pid(pid, out, n) && *out;
}

void sh_priv_free(struct sh_state *sh)
{
	struct sh_priv *p = sh->priv;

	if (!p)
		return;
	priv_disconnect(p);
	if (p->loop)
		pw_loop_destroy(p->loop);
	free(p);
	sh->priv = NULL;
}
