/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos privacy — who has the microphone, the camera and the screen
 *
 *   kdos privacy --json            one line, now
 *   kdos privacy --watch --json    that line, and again at every change
 *   kdos privacy                   the same, for a person
 *   kdos privacy mute|unmute <id>  a microphone stream
 *   kdos privacy stop <id>         a camera or screen capture
 *
 * THE ANSWER IS PIPEWIRE'S, plus the kernel's for a camera opened without it:
 *
 *   microphone  every running Stream/Input/Audio node, named by the process
 *               that owns it (application.process.id);
 *   camera      every camera Video/Source (device.api v4l2 or libcamera)
 *               with an active link, named by the stream at the other end;
 *               and every process holding a /dev/video* open itself, which
 *               PipeWire never sees;
 *   screen      every Stream/Output/Video whose application is kwin_wayland
 *               with an active link — KWin's screen casts, whether Spectacle or the portal
 *               asked for them — and every other Video/Source with one,
 *               named by the stream that reads it. Another program's video
 *               output stream is not listed.
 *
 * Each is named with the box it runs in, from its cgroup scope or the
 * KDOS_BOX its box's container sets (kdt_box_of_pid), because "Firefox
 * has the microphone" is a different sentence when Firefox is the box a
 * person keeps for one website.
 *
 * --watch FOLLOWS `pw-dump --monitor` RATHER THAN POLLING IT: a poll that
 * noticed a stop within the second the tray promises would run pw-dump twice
 * a second for as long as the session lasts. The camera's file-descriptor
 * scan runs on inotify instead: an open or close of a /dev/video* node, or
 * one appearing in /dev, rescans, and the scan is run again a quarter second
 * after each such event, for an opener whose descriptor was not yet in
 * /proc. Where inotify cannot be set up the scan falls back to every half
 * second. With no /dev/video* node at all the scan returns at once, so a
 * machine with no camera never walks /proc.
 *
 * Exit 0; 1 when PipeWire cannot be read or an action failed, with the reason
 * on stderr; 2 usage, which the tray takes as "stop retrying".
 * ---------------------------------
 */

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/wait.h>

#include "kbuild.h"
#include "kdos-tools.h"

/* ── the model ─────────────────────────────────────────────────────────── */

typedef struct {
	long id;
	KjNode *obj;		/* the whole pw-dump object */
} PwObj;

typedef struct {
	PwObj *o;
	int n, cap;
} PwMap;

typedef struct {
	char id[96];
	char app[128];
	char icon[96];
	char box[64];
	long pid;
	int muted, mutable_, stoppable;
	long target; /* the PipeWire object `stop` destroys */
} Entry;

typedef struct {
	Entry *e;
	int n, cap;
} List;

static void map_put(PwMap *m, long id, KjNode *obj)
{
	for (int i = 0; i < m->n; i++)
		if (m->o[i].id == id) {
			kj_free(m->o[i].obj);
			if (obj) {
				m->o[i].obj = obj;
			} else {
				m->o[i] = m->o[--m->n];
			}
			return;
		}
	if (!obj)
		return;
	if (m->n == m->cap) {
		m->cap = m->cap ? m->cap * 2 : 128;
		m->o = kb_realloc(m->o, (size_t)m->cap * sizeof(*m->o));
	}
	m->o[m->n].id = id;
	m->o[m->n].obj = obj;
	m->n++;
}

static void map_free(PwMap *m)
{
	for (int i = 0; i < m->n; i++)
		kj_free(m->o[i].obj);
	free(m->o);
	memset(m, 0, sizeof(*m));
}

/*
 * One pw-dump document — an array of objects — into the map. An object whose
 * `info` is null is one that went away. Each element is detached from the
 * parsed array so the map owns it.
 */
static void map_merge(PwMap *m, KjNode *doc)
{
	KjNode *prev = NULL, *el, *next;

	if (!doc || doc->type != KJ_ARR)
		return;
	for (el = doc->child; el; el = next) {
		const KjNode *info = kj_get(el, "info");
		long id = (long)kj_num(el, "id", -1);

		next = el->next;
		if (el->type != KJ_OBJ || id < 0) {
			prev = el;
			continue;
		}
		if (prev)
			prev->next = next;
		else
			doc->child = next;
		el->next = NULL;
		if (!info || info->type == KJ_NULL) {
			map_put(m, id, NULL);
			kj_free(el);
		} else {
			map_put(m, id, el);
		}
	}
}

static const char *type_of(const KjNode *o)
{
	return kj_str(o, "type", "");
}

static const KjNode *props(const KjNode *o)
{
	return kj_get(kj_get(o, "info"), "props");
}

static const char *prop(const KjNode *o, const char *key)
{
	const KjNode *p = props(o), *v = kj_get(p, key);

	if (!v)
		return NULL;
	if (v->type == KJ_STR)
		return v->str;
	return NULL;
}

static long prop_num(const KjNode *o, const char *key)
{
	const KjNode *v = kj_get(props(o), key);

	if (!v)
		return -1;
	if (v->type == KJ_NUM)
		return (long)v->num;
	if (v->type == KJ_STR && v->str && isdigit((unsigned char)v->str[0]))
		return atol(v->str);
	return -1;
}

static const KjNode *find(const PwMap *m, long id)
{
	for (int i = 0; i < m->n; i++)
		if (m->o[i].id == id)
			return m->o[i].obj;
	return NULL;
}

static int is_node(const KjNode *o)
{
	return !strcmp(type_of(o), "PipeWire:Interface:Node");
}

static const char *node_state(const KjNode *o)
{
	return kj_str(kj_get(o, "info"), "state", "");
}

/* The node's own process, or its client's. */
static long node_pid(const PwMap *m, const KjNode *o)
{
	long pid = prop_num(o, "application.process.id"), cid;
	const KjNode *c;

	if (pid > 0)
		return pid;
	cid = prop_num(o, "client.id");
	c = cid >= 0 ? find(m, cid) : NULL;
	return c ? prop_num(c, "application.process.id") : -1;
}

/* Props[0].mute, as the stream's own controls report it. */
static int node_muted(const KjNode *o)
{
	const KjNode *pa = kj_get(kj_get(kj_get(o, "info"), "params"), "Props");

	if (!pa || pa->type != KJ_ARR)
		return 0;
	for (const KjNode *p = pa->child; p; p = p->next)
		if (kj_get(p, "mute"))
			return kj_bool(p, "mute", 0);
	return 0;
}

/* ── naming ────────────────────────────────────────────────────────────── */

static void comm_of(long pid, char *out, size_t n)
{
	char path[64];

	snprintf(path, sizeof(path), "/proc/%ld/comm", pid);
	if (kb_read_line_file(path, out, n) <= 0)
		snprintf(out, n, "pid %ld", pid);
}

/* A node's application property, or its client's: PipeWire puts the
 * application's own name and icon on the client and copies them to a node
 * only sometimes. */
static const char *app_prop(const PwMap *m, const KjNode *o, const char *key)
{
	const char *v = o ? prop(o, key) : NULL;
	long cid;
	const KjNode *c;

	if (v && *v)
		return v;
	cid = o ? prop_num(o, "client.id") : -1;
	c = m && cid >= 0 ? find(m, cid) : NULL;
	return c ? prop(c, key) : NULL;
}

static void name_entry(Entry *e, const PwMap *m, const KjNode *o, long pid)
{
	const char *app = app_prop(m, o, "application.name");
	const char *icon = app_prop(m, o, "application.icon-name");

	e->pid = pid;
	if (app && *app)
		snprintf(e->app, sizeof(e->app), "%s", app);
	else if (pid > 0)
		comm_of(pid, e->app, sizeof(e->app));
	else
		snprintf(e->app, sizeof(e->app), "%s",
			 o && prop(o, "node.name") ? prop(o, "node.name")
						   : "unknown");
	snprintf(e->icon, sizeof(e->icon), "%s", icon ? icon : "");
	e->box[0] = '\0';
	if (pid > 0)
		kdt_box_of_pid(pid, e->box, sizeof(e->box));
}

static Entry *list_add(List *l)
{
	if (l->n == l->cap) {
		l->cap = l->cap ? l->cap * 2 : 8;
		l->e = kb_realloc(l->e, (size_t)l->cap * sizeof(*l->e));
	}
	memset(&l->e[l->n], 0, sizeof(l->e[0]));
	return &l->e[l->n++];
}

/* ── the three lists ───────────────────────────────────────────────────── */

static void scan_pw(const PwMap *m, List *mic, List *cam, List *scr)
{
	for (int i = 0; i < m->n; i++) {
		const KjNode *o = m->o[i].obj;
		const char *cls;

		if (!is_node(o) || !(cls = prop(o, "media.class")))
			continue;
		if (!strcmp(cls, "Stream/Input/Audio") &&
		    !strcmp(node_state(o), "running")) {
			Entry *e = list_add(mic);

			snprintf(e->id, sizeof(e->id), "pw:%ld", m->o[i].id);
			name_entry(e, m, o, node_pid(m, o));
			e->muted = node_muted(o);
			e->mutable_ = 1;
			e->stoppable = 0;
			continue;
		}
		if (strcmp(cls, "Video/Source") &&
		    strcmp(cls, "Stream/Output/Video"))
			continue;
		/* Only KWin's video output streams are screen casts. Any other
		 * program's (a pipewiresink, a box's own video output) is
		 * neither a capture nor ours to end for every reader. */
		if (!strcmp(cls, "Stream/Output/Video")) {
			const char *bin = app_prop(m, o,
						   "application.process.binary");
			const char *an = app_prop(m, o, "application.name");

			if (!(bin && !strcmp(bin, "kwin_wayland")) &&
			    !(an && !strcmp(an, "kwin_wayland")))
				continue;
		}
		/* A source with readers: each active link out of it is one
		 * capture, named by the stream it feeds.
		 *
		 * Stopping a Stream/Output/Video destroys the node itself: it is
		 * the stream KWin made for this one screencast, and its end is
		 * what KWin turns into the screencast's closed event, so the
		 * recorder or the portal client learns the capture is over.
		 * Destroying only the link stops the frames and tells nobody:
		 * the recorder goes on saying it records. Any other source, a
		 * camera above all, is shared, so only its link is destroyed. */
		{
			const char *api = prop(o, "device.api");
			int camera = api && (!strcmp(api, "v4l2") ||
					     !strcmp(api, "libcamera"));
			int own = !strcmp(cls, "Stream/Output/Video");

			for (int j = 0; j < m->n; j++) {
				const KjNode *l = m->o[j].obj;
				const KjNode *info = kj_get(l, "info");
				const KjNode *peer;
				Entry *e;

				if (strcmp(type_of(l),
					   "PipeWire:Interface:Link") ||
				    (long)kj_num(info, "output-node-id", -1) !=
					    m->o[i].id ||
				    strcmp(kj_str(info, "state", ""), "active"))
					continue;
				peer = find(m, (long)kj_num(info,
							    "input-node-id",
							    -1));
				e = list_add(camera ? cam : scr);
				snprintf(e->id, sizeof(e->id), "pw:%ld:%ld",
					 m->o[i].id, m->o[j].id);
				name_entry(e, m, peer ? peer : o,
					   peer ? node_pid(m, peer) : -1);
				e->stoppable = 1;
				e->target = own ? m->o[i].id : m->o[j].id;
			}
		}
	}
}

/* Whether /dev holds any video* node. */
static int video_present(void)
{
	DIR *d = opendir("/dev");
	struct dirent *de;
	int any = 0;

	if (!d)
		return 1;
	while (!any && (de = readdir(d)))
		any = !strncmp(de->d_name, "video", 5);
	closedir(d);
	return any;
}

/* Video4Linux devices are character major 81. A process with one open is a
 * process using a camera, through PipeWire or not; a pid PipeWire already
 * named is not named twice. */
static void scan_fds(List *cam)
{
	DIR *d;
	struct dirent *pe;

	if (!video_present() || !(d = opendir("/proc")))
		return;
	while ((pe = readdir(d))) {
		char fdp[64];
		DIR *fd;
		struct dirent *fe;
		long pid;

		if (!isdigit((unsigned char)pe->d_name[0]))
			continue;
		pid = atol(pe->d_name);
		snprintf(fdp, sizeof(fdp), "/proc/%ld/fd", pid);
		if (!(fd = opendir(fdp)))
			continue;
		while ((fe = readdir(fd))) {
			char lp[384], dev[64];
			struct stat st;
			int seen = 0;

			snprintf(lp, sizeof(lp), "%s/%s", fdp, fe->d_name);
			if (stat(lp, &st) != 0 || !S_ISCHR(st.st_mode) ||
			    major(st.st_rdev) != 81)
				continue;
			for (int i = 0; i < cam->n; i++)
				if (cam->e[i].pid == pid)
					seen = 1;
			if (seen)
				break;
			snprintf(dev, sizeof(dev), "video%u",
				 minor(st.st_rdev));
			{
				Entry *e = list_add(cam);

				snprintf(e->id, sizeof(e->id), "%s:%ld", dev,
					 pid);
				name_entry(e, NULL, NULL, pid);
				e->stoppable = 0;
			}
			break;
		}
		closedir(fd);
	}
	closedir(d);
}

static void emit_list(KbBuf *b, const char *key, const List *l, int mic)
{
	kb_buf_printf(b, ",\"%s\":[", key);
	for (int i = 0; i < l->n; i++) {
		const Entry *e = &l->e[i];

		kb_buf_str(b, i ? ",{\"id\":" : "{\"id\":");
		kb_json_str(b, e->id);
		kb_buf_str(b, ",\"app\":");
		kb_json_str(b, e->app);
		kb_buf_str(b, ",\"icon\":");
		kb_json_str(b, e->icon);
		kb_buf_str(b, ",\"box\":");
		if (e->box[0])
			kb_json_str(b, e->box);
		else
			kb_buf_str(b, "null");
		kb_buf_printf(b, ",\"pid\":%ld", e->pid > 0 ? e->pid : 0);
		if (mic)
			kb_buf_printf(b, ",\"muted\":%s",
				      e->muted ? "true" : "false");
		kb_buf_printf(b, ",\"mutable\":%s,\"stoppable\":%s}",
			      e->mutable_ ? "true" : "false",
			      e->stoppable ? "true" : "false");
	}
	kb_buf_str(b, "]");
}

static char *render(const PwMap *m)
{
	List mic = { 0 }, cam = { 0 }, scr = { 0 };
	KbBuf b = { 0 };

	scan_pw(m, &mic, &cam, &scr);
	scan_fds(&cam);
	kb_buf_str(&b, "{\"version\":1");
	emit_list(&b, "mic", &mic, 1);
	emit_list(&b, "camera", &cam, 0);
	emit_list(&b, "screen", &scr, 0);
	kb_buf_str(&b, "}");
	free(mic.e);
	free(cam.e);
	free(scr.e);
	return b.p;
}

/* ── reading PipeWire ──────────────────────────────────────────────────── */

static int load_once(PwMap *m, char *err, size_t n)
{
	KbArgv a = { 0 };
	KbBuf out = { 0 };
	KjNode *doc;

	kb_argv_add(&a, "pw-dump");
	kb_argv_end(&a);
	if (kb_run_capture_buf(&a, &out) != 0 || !out.p) {
		snprintf(err, n, "pw-dump could not read PipeWire");
		kb_buf_free(&out);
		return -1;
	}
	doc = kj_parse(out.p);
	kb_buf_free(&out);
	if (!doc) {
		snprintf(err, n, "pw-dump wrote something that is not JSON");
		return -1;
	}
	map_merge(m, doc);
	kj_free(doc);
	return 0;
}

/* `pw-dump --monitor` on a pipe; its pid in *pid. */
static int monitor_start(pid_t *pid)
{
	int p[2];

	if (pipe(p) != 0)
		return -1;
	*pid = fork();
	if (*pid < 0) {
		close(p[0]);
		close(p[1]);
		return -1;
	}
	if (*pid == 0) {
		int null = open("/dev/null", 0);

		dup2(p[1], STDOUT_FILENO);
		if (null >= 0)
			dup2(null, STDIN_FILENO);
		close(p[0]);
		close(p[1]);
		kb_child_reset_signals();
		execlp("pw-dump", "pw-dump", "--monitor", (char *)NULL);
		_exit(127);
	}
	close(p[1]);
	return p[0];
}

/*
 * The monitor writes one JSON array per change. They are cut out of the
 * stream at the bracket that closes the outermost one, counting only outside
 * strings — a node name may hold a bracket.
 */
typedef struct {
	KbBuf buf;
	size_t start;
	int depth, in_str, esc, open;
} Cutter;

static KjNode *cut_next(Cutter *c)
{
	for (size_t i = c->start; i < c->buf.n; i++) {
		char ch = c->buf.p[i];

		if (c->in_str) {
			if (c->esc)
				c->esc = 0;
			else if (ch == '\\')
				c->esc = 1;
			else if (ch == '"')
				c->in_str = 0;
			continue;
		}
		if (ch == '"') {
			c->in_str = 1;
		} else if (ch == '[' || ch == '{') {
			if (!c->depth)
				c->open = (int)i;
			c->depth++;
		} else if ((ch == ']' || ch == '}') && c->depth > 0 &&
			   --c->depth == 0) {
			char save = c->buf.p[i + 1];
			KjNode *doc;

			c->buf.p[i + 1] = '\0';
			doc = kj_parse(c->buf.p + c->open);
			c->buf.p[i + 1] = save;
			c->start = i + 1;
			/* Drop what has been consumed, now and then, so the
			 * buffer does not grow for the length of a login. */
			if (c->start > 1 << 16) {
				memmove(c->buf.p, c->buf.p + c->start,
					c->buf.n - c->start);
				c->buf.n -= c->start;
				c->start = 0;
				c->buf.p[c->buf.n] = '\0';
			}
			return doc;
		}
	}
	c->start = c->buf.n;
	return NULL;
}

/* ── the camera's inotify ──────────────────────────────────────────────── */

#define CAM_EVENTS (IN_OPEN | IN_CLOSE_WRITE | IN_CLOSE_NOWRITE)

/* An open/close watch on every /dev/video* node. A node the caller may not
 * read takes no watch, and none of the caller's processes can hold it. */
static void cam_watch_nodes(int in)
{
	DIR *d = opendir("/dev");
	struct dirent *de;

	if (!d)
		return;
	while ((de = readdir(d))) {
		char p[300];

		if (strncmp(de->d_name, "video", 5))
			continue;
		snprintf(p, sizeof(p), "/dev/%s", de->d_name);
		inotify_add_watch(in, p, CAM_EVENTS);
	}
	closedir(d);
}

/* An inotify descriptor watching /dev for video* nodes coming and going and
 * each one for opens and closes; -1 when there is none to be had. */
static int cam_watch_start(void)
{
	int in = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);

	if (in < 0)
		return -1;
	if (inotify_add_watch(in, "/dev", IN_CREATE | IN_DELETE) < 0) {
		close(in);
		return -1;
	}
	cam_watch_nodes(in);
	return in;
}

/* Read every queued event; 1 when any concerned a camera. A new video* node
 * gets its own watch. */
static int cam_watch_drain(int in)
{
	char buf[4096] __attribute__((aligned(__alignof__(struct inotify_event))));
	ssize_t got;
	int any = 0, added = 0;

	while ((got = read(in, buf, sizeof(buf))) > 0) {
		for (char *p = buf; p < buf + got;) {
			const struct inotify_event *ev = (const void *)p;

			if (!ev->len || !strncmp(ev->name, "video", 5)) {
				any = 1;
				added |= (ev->mask & IN_CREATE) != 0;
			}
			p += sizeof(*ev) + ev->len;
		}
	}
	if (added)
		cam_watch_nodes(in);
	return any;
}

static volatile sig_atomic_t stop_watch;

static void on_term(int sig)
{
	(void)sig;
	stop_watch = 1;
}

static int say(const char *line)
{
	if (fputs(line, stdout) == EOF || fputc('\n', stdout) == EOF ||
	    fflush(stdout) == EOF)
		return -1;
	return 0;
}

static int watch(void)
{
	PwMap m = { 0 };
	Cutter c = { 0 };
	char *last = NULL, chunk[8192];
	pid_t pid;
	int fd = monitor_start(&pid), rc = 0, cam = -1, again = 0;

	if (fd < 0) {
		fprintf(stderr, "kdos privacy: pw-dump could not be started\n");
		return 1;
	}
	signal(SIGPIPE, SIG_IGN);
	signal(SIGTERM, on_term);
	signal(SIGINT, on_term);
	cam = cam_watch_start();
	{
		/* The state before the monitor's first document arrives is
		 * an empty one, said at once so the tray has a line. */
		char *now = render(&m);

		if (say(now) != 0) {
			free(now);
			goto out;
		}
		last = now;
	}
	while (!stop_watch) {
		struct pollfd p[2] = { { .fd = fd, .events = POLLIN },
				       { .fd = cam, .events = POLLIN } };
		int r = poll(p, cam >= 0 ? 2 : 1,
			     cam < 0 ? 500 : again ? 250 : -1);
		KjNode *doc;
		char *now;

		if (r < 0 && errno != EINTR)
			break;
		if (r < 0)
			continue;
		if (r == 0)
			again = 0;
		if (cam >= 0 && (p[1].revents & POLLIN)) {
			if (cam_watch_drain(cam))
				again = 1;
			else if (!(p[0].revents & (POLLIN | POLLHUP)))
				continue;
		}
		if (p[0].revents & (POLLIN | POLLHUP)) {
			ssize_t got = read(fd, chunk, sizeof(chunk));

			if (got <= 0) {
				fprintf(stderr, "kdos privacy: PipeWire went "
						"away\n");
				rc = 1;
				break;
			}
			kb_buf_add(&c.buf, chunk, (size_t)got);
			while ((doc = cut_next(&c))) {
				map_merge(&m, doc);
				kj_free(doc);
			}
		}
		now = render(&m);
		if (!last || strcmp(now, last)) {
			if (say(now) != 0) {
				free(now);
				break;
			}
			free(last);
			last = now;
		} else {
			free(now);
		}
	}
out:
	kill(pid, SIGTERM);
	waitpid(pid, NULL, 0);
	close(fd);
	if (cam >= 0)
		close(cam);
	free(last);
	kb_buf_free(&c.buf);
	map_free(&m);
	return rc;
}

/* ── acting ────────────────────────────────────────────────────────────── */

static int act(const char *verb, const char *id)
{
	PwMap m = { 0 };
	List mic = { 0 }, cam = { 0 }, scr = { 0 };
	char err[256] = "", num[24];
	const Entry *hit = NULL;
	int rc = 1;

	if (load_once(&m, err, sizeof(err)) != 0) {
		fprintf(stderr, "kdos privacy: %s\n", err);
		return 1;
	}
	scan_pw(&m, &mic, &cam, &scr);
	scan_fds(&cam);
	for (int k = 0; k < 3 && !hit; k++) {
		const List *l = k == 0 ? &mic : k == 1 ? &cam : &scr;

		for (int i = 0; i < l->n; i++)
			if (!strcmp(l->e[i].id, id))
				hit = &l->e[i];
	}
	if (!hit) {
		fprintf(stderr, "kdos privacy: nothing is capturing as %s\n",
			id);
		goto out;
	}
	if (!strcmp(verb, "stop")) {
		KbArgv a = { 0 };

		if (!hit->stoppable) {
			fprintf(stderr, "kdos privacy: %s cannot be stopped "
					"from here — close %s\n", id, hit->app);
			goto out;
		}
		snprintf(num, sizeof(num), "%ld", hit->target);
		kb_argv_add(&a, "pw-cli");
		kb_argv_add(&a, "destroy");
		kb_argv_add(&a, num);
		kb_argv_end(&a);
		rc = kb_run(&a) == 0 ? 0 : 1;
		if (rc)
			fprintf(stderr, "kdos privacy: PipeWire refused to end "
					"%s\n", id);
	} else {
		long node;
		KbArgv a = { 0 };

		if (!hit->mutable_ || sscanf(hit->id, "pw:%ld", &node) != 1) {
			fprintf(stderr, "kdos privacy: %s is not a microphone "
					"stream\n", id);
			goto out;
		}
		snprintf(num, sizeof(num), "%ld", node);
		kb_argv_add(&a, "wpctl");
		kb_argv_add(&a, "set-mute");
		kb_argv_add(&a, num);
		kb_argv_add(&a, !strcmp(verb, "mute") ? "1" : "0");
		kb_argv_end(&a);
		rc = kb_run(&a) == 0 ? 0 : 1;
		if (rc)
			fprintf(stderr, "kdos privacy: wpctl could not %s %s\n",
				verb, id);
	}
out:
	free(mic.e);
	free(cam.e);
	free(scr.e);
	map_free(&m);
	return rc;
}

static int usage(void)
{
	fprintf(stderr, "usage: kdos privacy [--json] [--watch]\n"
			"       kdos privacy mute|unmute|stop <id>\n");
	return 2;
}

int kdt_privacy_main(int argc, char **argv)
{
	int json = 0, follow = 0;

	if (argc == 2 && (!strcmp(argv[0], "mute") ||
			  !strcmp(argv[0], "unmute") ||
			  !strcmp(argv[0], "stop"))) {
		if (!*argv[1] || argv[1][0] == '-' || strlen(argv[1]) > 512)
			return usage();
		return act(argv[0], argv[1]);
	}
	for (int i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "--json"))
			json = 1;
		else if (!strcmp(argv[i], "--watch"))
			follow = 1;
		else
			return usage();
	}
	if (follow)
		return watch();
	{
		PwMap m = { 0 };
		char err[256] = "", *line;

		if (load_once(&m, err, sizeof(err)) != 0) {
			fprintf(stderr, "kdos privacy: %s\n", err);
			return 1;
		}
		line = render(&m);
		map_free(&m);
		if (json) {
			printf("%s\n", line);
		} else {
			KjNode *doc = kj_parse(line);
			static const char *const K[] = { "mic", "camera",
							 "screen" };
			static const char *const L[] = { "microphone",
							 "camera", "screen" };
			int any = 0;

			for (int k = 0; k < 3; k++) {
				const KjNode *arr = kj_get(doc, K[k]);

				for (const KjNode *e = arr ? arr->child : NULL;
				     e; e = e->next) {
					const char *box = kj_str(e, "box", NULL);

					printf("%-10s %s%s%s%s  (%s)\n", L[k],
					       kj_str(e, "app", "?"),
					       box ? " in box " : "",
					       box ? box : "",
					       kj_bool(e, "muted", 0) ? ", muted"
								      : "",
					       kj_str(e, "id", ""));
					any = 1;
				}
			}
			if (!any)
				printf("nothing is using the microphone, the "
				       "camera or the screen\n");
			kj_free(doc);
		}
		free(line);
	}
	return 0;
}
