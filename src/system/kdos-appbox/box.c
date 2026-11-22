/*
 * ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   KD's Homebrew Linux Distro
 * ---------------------------------
 *
 * Boxes and their sandbox profiles.
 *
 * A profile is a handful of keys in ~/.config/kdos/boxes/<name>.conf. The
 * namespace keys map onto flags of the container runtime — distrobox's for an
 * image box, `podman create --rootfs` for a pack box — which the kernel
 * enforces. KDOS deliberately does not offer confinement it cannot deliver:
 * there is no "no filesystem access" switch, because the box shares the host
 * root by design and pretending otherwise would be a lie printed in a
 * settings dialog.
 *
 * A profile is applied at CREATE time. Namespaces and volumes cannot be
 * re-flagged on a live container, so `kdos-box profile` says that a change
 * takes effect once the box is removed and created again, rather than
 * silently doing nothing, and its report carries a `!` line for as long as
 * the profile is newer than the box's container.
 *
 * THE PROTOCOL HALF IS NOT THIS FILE'S. KWin denies the globals that reach
 * past a client's own windows — the window list, fake input, screencast,
 * data control, layer shell, output management and the rest — to every
 * client of a tagged socket, and its sandbox policy (ports/core/kde/kwin's
 * sandbox-policy.patch) reads this profile's `grant` key to let one box bind
 * named ones beyond that. keeper.c turns the `secrets` grant into a bus rule.
 * This file parses and writes `grant` and owns the namespace half: the keys
 * applied at create time, and what the box's private runtime directory holds.
 *
 * A KEY THAT ENFORCES NOTHING IS PRINTED AS SUCH, never silently: a setting
 * that quietly does nothing is worse than an absent one. Which keys are in
 * that state depends on the box's LANE (box_lane()): a pack box and a store
 * box see their own runtime directory as /run/user/<uid>, so `wayland`,
 * `audio` and `x11` decide which sockets are in it, and `persistence` maps
 * onto `--rootfs …:O` and `--rm`; a box distrobox made from somebody else's
 * image shares the session's runtime directory, and every socket in it is
 * reachable whatever those keys say. `export` triggers nothing anywhere:
 * `kdos-appbox genlaunchers` covers every installed pack and every store box
 * at once and `kdos-box export` is the per-application route.
 *
 * The path is resolved from $HOME/.config and NOT from $XDG_CONFIG_HOME, and
 * KWin's grant check deliberately copies that rather than doing the more
 * correct thing, because the two resolving it differently is the one failure
 * this design must not have.
 */

#include "kdos-appbox.h"
#include "kcolor.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

void profile_defaults(Profile *p, const char *box)
{
	memset(p, 0, sizeof(*p));
	snprintf(p->name, sizeof(p->name), "%s", box);
	/* Everything shared: exactly what a plain `distrobox create` does, so a
	 * box with no profile file behaves identically to one with a default. */
	p->persist = PERSIST_PERSISTENT;
	p->wayland = 1;
	p->audio = 1;
	p->gpu = 1;
	/*
	 * THE CARD BY DEFAULT, AND THE FALL BACK IS AUTOMATIC. The device
	 * nodes are bound in, the drivers are in the base pack and both
	 * renderers are built: a box that draws with llvmpipe on a machine
	 * that has all three is paying for nothing. `auto` is what asks the
	 * machine — a render node that opens means hardware and its absence
	 * means software — and the two literal values are what overrule it.
	 */
	kb_strlcpy(p->render, "auto", sizeof(p->render));
	kb_strlcpy(p->x11, "yes", sizeof(p->x11));
}

/*
 * A box name is a container name, a profile file name, a socket path
 * component and a KWin app id at once. It starts with a letter or a digit: a
 * leading `-` is an option to every program it is handed to, and a leading
 * `.` is a hidden file and the parent directory's spelling.
 */
int box_name_ok(const char *s)
{
	if (!s || !*s || strlen(s) >= 64)
		return 0;
	if (*s == '.' || *s == '-')
		return 0;
	for (const char *c = s; *c; c++)
		if (!isalnum((unsigned char)*c) && *c != '.' && *c != '_' &&
		    *c != '-')
			return 0;
	return 1;
}

/*
 * THE GRANT VOCABULARY, and the one spelling of it in this program. KWin's
 * sandbox policy maps each name onto the globals it opens, and keeper.c maps
 * `secrets` onto the bus; a name in neither table opens nothing, which
 * profile_print() says rather than letting the word read as a permission.
 * The order is the one the Boxes page lists them in.
 */
static const struct {
	const char *name;
	const char *what;
} GRANTS[] = {
	{ "window-list", "lists and acts on every window (org_kde_plasma_window_management)" },
	{ "fake-input", "types and clicks into other windows (org_kde_kwin_fake_input)" },
	{ "screencast", "records the screen without asking (zkde_screencast_unstable_v1)" },
	{ "data-control", "reads and sets the clipboard at any time (ext_data_control_manager_v1)" },
	{ "layer-shell", "draws panels and overlays (zwlr_layer_shell_v1)" },
	{ "output-management", "changes the screens' modes and layout (kde_output_management_v2)" },
	{ "dpms", "switches the screens off and on (org_kde_kwin_dpms_manager)" },
	{ "plasma-shell", "places its windows as the shell does (org_kde_plasma_shell)" },
	{ "input-method", "passes KWin's filter for the input-method globals, which KWin still offers only to the input method it started" },
	{ "secrets", "reaches the password store (org.freedesktop.secrets on the bus)" },
};

int profile_granted(const Profile *p, const char *name)
{
	char copy[sizeof(p->grant)], *save = NULL;

	kb_strlcpy(copy, p->grant, sizeof(copy));
	for (char *t = strtok_r(copy, " ,\t", &save); t;
	     t = strtok_r(NULL, " ,\t", &save))
		if (!strcmp(t, name))
			return 1;
	return 0;
}

int box_lane(const Profile *p)
{
	if (!strncmp(p->base, "image:" STORE_IMG_PREFIX,
		     6 + sizeof(STORE_IMG_PREFIX) - 1))
		return BOX_LANE_STORE;
	if (!strncmp(p->base, "image:", 6) ||
	    (!p->base[0] && p->image[0]))
		return BOX_LANE_DISTROBOX;
	return BOX_LANE_PACK;
}

/*
 * WHETHER A BOX IS HANDED THE X11 DISPLAY. Every box is, unless its profile
 * says `x11 = no`. Tk, Java's AWT and Swing, Motif, FLTK, GTK 2 and Xt have no
 * Wayland backend, and nothing in a package says which toolkit a program
 * draws with: a box withheld the display by default starts such a program,
 * which finds no display and exits, while the box itself stays running.
 *
 * What it costs: Xwayland is one client to KWin, so an X11 window in a box is
 * drawn untagged and nothing it binds is filtered, and an X11 program can read
 * every other X11 program's windows and keys. A box handed the display is
 * therefore `tagged`, never `confined`; `x11 = no` is what confines one,
 * leaving it the tagged Wayland socket alone. A box that is not given the
 * session's cookie cannot connect — KWin's Xwayland requires one, and the
 * cookie lives in the session's runtime directory, which a private-layout box
 * does not see.
 */
int profile_x11(const Profile *p)
{
	return strcmp(p->x11, "no") != 0;
}

/*
 * A DRM RENDER NODE THIS USER CAN OPEN, which is the whole of "can anything
 * here draw on the card". OPENED AND NOT STAT()ED: the node is owned by the
 * `render` group, and a session outside that group reaches the same dead end
 * as a machine with no card at all — a name in /dev is not access to it.
 *
 * It is also the virgl test. virtio-gpu publishes a render node only when the
 * host offered VIRGL, so a plain `make run` has none and every answer below
 * resolves to software with nothing else asked.
 */
int box_render_node(char *out, size_t n)
{
	DIR *d = opendir("/dev/dri");
	struct dirent *e;
	int found = 0;

	if (!d)
		return 0;
	while (!found && (e = readdir(d)) != NULL) {
		/* "/dev/dri/" plus a NAME_MAX entry, so the compiler can see
		 * that nothing here is ever truncated. */
		char path[9 + 255 + 1];
		int fd;

		if (strncmp(e->d_name, "renderD", 7))
			continue;
		snprintf(path, sizeof(path), "/dev/dri/%s", e->d_name);
		fd = open(path, O_RDWR | O_CLOEXEC);
		if (fd < 0)
			continue;
		close(fd);
		found = 1;
		if (out)
			snprintf(out, n, "%s", path);
	}
	closedir(d);
	return found;
}

/*
 * WHAT THE `render` KEY RESOLVES TO ON THIS MACHINE, which is the only form of
 * it worth acting on: a profile states a wish and the hardware answers it.
 *
 * `software` is the one value that needs no machine — it is a refusal, and it
 * is honoured whatever is plugged in. Everything else, `gpu` and `auto` and an
 * absent key alike, is a REQUEST, so it resolves to hardware only where a
 * render node opens. Saying `gpu` on a machine with no card does not make one,
 * and a caller that believed the string would export a hardware promise into a
 * box that then draws every frame on the CPU.
 *
 * AND THE NODE IS PROBED FROM HERE, WHICH IS NOT WHERE THE BOX STANDS. This
 * process sees the host's /dev; a box with `devices = private` sees its own,
 * and `gpu = no` is what leaves /dev/dri out of it. Those two together are the
 * one case where the host's answer is not the box's, so they are refused here
 * rather than probed: every other box either shares /dev or has the nodes
 * bound back, and for those the open this makes IS the box's open.
 */
int profile_render_gpu(const Profile *p)
{
	if (!strcmp(p->render, "software") || !strcmp(p->render, "pixman") ||
	    !strcmp(p->render, "no") || !strcmp(p->render, "off") ||
	    !strcmp(p->render, "0") || !strcmp(p->render, "false"))
		return 0;
	if (p->devsys && !p->gpu)
		return 0;
	return box_render_node(NULL, 0);
}

char *profile_path(const char *box)
{
	char *dir = kb_calloc(1, MAX_LINE);
	snprintf(dir, MAX_LINE, "%s/.config/kdos/boxes", kb_home_dir());
	kb_mkdir_p(dir);
	snprintf(dir + strlen(dir), MAX_LINE - strlen(dir), "/%s.conf", box);
	return dir;
}

char *profile_home(const char *box)
{
	char *p = kb_calloc(1, MAX_LINE);
	const char *data = getenv("XDG_DATA_HOME");
	if (data && *data)
		snprintf(p, MAX_LINE, "%s/kdos/boxes/%s", data, box);
	else
		snprintf(p, MAX_LINE, "%s/.local/share/kdos/boxes/%s",
			 kb_home_dir(), box);
	return p;
}

static int truthy(const char *v)
{
	return !strcmp(v, "private") || !strcmp(v, "yes") || !strcmp(v, "on") ||
	       !strcmp(v, "1") || !strcmp(v, "true");
}

/* `30m`, `2h`, `90s`, or a bare number of seconds. Zero disables. */
static int duration(const char *v)
{
	char *end;
	long n = strtol(v, &end, 10);

	if (n <= 0)
		return 0;
	switch (*end) {
	case 'h': case 'H': return (int)(n * 3600);
	case 'm': case 'M': return (int)(n * 60);
	default:            return (int)n;
	}
}

const char *persist_name(Persistence p)
{
	switch (p) {
	case PERSIST_EPHEMERAL: return "ephemeral";
	case PERSIST_FROZEN:    return "frozen";
	default:                return "persistent";
	}
}

int profile_set(Profile *p, const char *kv)
{
	const char *eq = strchr(kv, '=');
	char key[64];
	size_t n;

	if (!eq)
		return -1;
	n = (size_t)(eq - kv);
	if (n >= sizeof(key))
		return -1;
	memcpy(key, kv, n);
	key[n] = '\0';
	while (n && (key[n - 1] == ' ' || key[n - 1] == '\t'))
		key[--n] = '\0';
	eq++;
	while (*eq == ' ' || *eq == '\t')
		eq++;

	if (!strcmp(key, "image"))
		snprintf(p->image, sizeof(p->image), "%s", eq);
	else if (!strcmp(key, "base"))
		snprintf(p->base, sizeof(p->base), "%s", eq);
	else if (!strcmp(key, "accent"))
		snprintf(p->accent, sizeof(p->accent), "%s", eq);
	else if (!strcmp(key, "grant"))
		snprintf(p->grant, sizeof(p->grant), "%s", eq);
	else if (!strcmp(key, "persistence")) {
		if (!strcmp(eq, "ephemeral"))
			p->persist = PERSIST_EPHEMERAL;
		else if (!strcmp(eq, "frozen"))
			p->persist = PERSIST_FROZEN;
		else
			p->persist = PERSIST_PERSISTENT;
	} else if (!strcmp(key, "network")) {
		/* host | private | none. `yes`, `on`, `1` and `true` read as
		 * private and any other word, `shared` among them, as host.
		 * `none` is private PLUS no interface at all, which is a
		 * different thing from a namespace of one's own with a bridge
		 * in it. */
		p->netnone = !strcmp(eq, "none");
		p->netns = p->netnone || truthy(eq);
	} else if (!strcmp(key, "ipc"))
		p->ipc = truthy(eq);
	else if (!strcmp(key, "devices"))
		p->devsys = truthy(eq);
	else if (!strcmp(key, "processes"))
		p->process = truthy(eq);
	else if (!strcmp(key, "home"))
		p->privhome = truthy(eq);
	else if (!strcmp(key, "init"))
		p->init = truthy(eq);
	else if (!strcmp(key, "wayland"))
		p->wayland = truthy(eq) || !strcmp(eq, "shared");
	else if (!strcmp(key, "audio"))
		p->audio = truthy(eq) || !strcmp(eq, "shared");
	else if (!strcmp(key, "gpu"))
		p->gpu = truthy(eq) || !strcmp(eq, "shared");
	else if (!strcmp(key, "x11")) {
		/* yes | no. Only a plain refusal is a no: every other word is
		 * the default, so a value nobody wrote on purpose leaves an
		 * X11-only application its display. */
		if (!strcmp(eq, "no") || !strcmp(eq, "off") ||
		    !strcmp(eq, "0") || !strcmp(eq, "false"))
			kb_strlcpy(p->x11, "no", sizeof(p->x11));
		else
			kb_strlcpy(p->x11, "yes", sizeof(p->x11));
	}
	else if (!strcmp(key, "export"))
		p->autoexport = !strcmp(eq, "auto");
	else if (!strcmp(key, "display"))
		snprintf(p->display, sizeof(p->display), "%s", eq);
	else if (!strcmp(key, "render"))
		snprintf(p->render, sizeof(p->render), "%s", eq);
	else if (!strcmp(key, "memory"))
		snprintf(p->memory, sizeof(p->memory), "%s", eq);
	else if (!strcmp(key, "cpus"))
		snprintf(p->cpus, sizeof(p->cpus), "%s", eq);
	else if (!strcmp(key, "pids"))
		p->pids = atoi(eq);
	else if (!strcmp(key, "autostop"))
		p->autostop_s = duration(eq);
	else {
		/*
		 * REPORTED BY NAME, never ignored: a typo that produces
		 * silence is indistinguishable from a setting that does
		 * nothing.
		 */
		if (p->nunknown < 8)
			snprintf(p->unknown[p->nunknown++], 64, "%s", key);
		return -1;
	}
	return 0;
}

const char *accent_shown(const char *accent)
{
	return !accent[0] || !strcmp(accent, "session") ? "none" : accent;
}

int accent_known(const char *accent)
{
	return !accent[0] || !strcmp(accent, "session") ||
	       kcol_find(accent) != NULL;
}

int profile_load(Profile *p, const char *box)
{
	char *path = profile_path(box);
	char buf[MAX_LINE];
	char *line, *save;
	int found;

	profile_defaults(p, box);
	found = kb_read_file(path, buf, sizeof(buf)) >= 0;
	free(path);
	if (!found)
		return 0;

	for (line = strtok_r(buf, "\n", &save); line;
	     line = strtok_r(NULL, "\n", &save)) {
		while (*line == ' ' || *line == '\t')
			line++;
		if (*line == '#' || !*line)
			continue;
		profile_set(p, line);
	}
	return 1;
}

/*
 * EVERY PROCESS OF A BOX, SIGNALLED BY THIS PROGRAM AND NOT BY `podman stop`.
 *
 * A box created with `--pid host` and no cgroup of its own — every store box,
 * and a pack box in a login with no delegated subtree — sits in the login's
 * own cgroup. podman stops a container that has no PID namespace of its own
 * through `crun kill --all`, and with no cgroup to list, that signals the
 * box's init and the init's descendants (crun's kill-all-without-cgroup.patch)
 * and leaves running every application the box's launches exec'd, so
 * kdos-box stop signals those itself; `podman kill` reaches the init alone.
 *
 * WHAT IS THE BOX'S. Each launch is a `podman exec`, which joins the box's
 * mount namespace and outlives the init when the PID namespace is the
 * host's, and a stopped box whose applications still run holds exec sessions
 * that `podman rm` refuses over, and a start refuses with EBUSY over the
 * storage they keep mounted. A process is the box's when it is in the init's
 * mount namespace; when the init is gone, when it is in a mount namespace
 * other than this program's and its environment carries the box's KDOS_BOX,
 * which every exec inherits from a container kdos-appbox created; and,
 * whatever the init's state, when its parent chain reaches one of the
 * container's conmons. A conmon is the container's when its comm is `conmon`
 * and its command line names the container's full ID (`-c <id>`, `--cid
 * <id>` or `--cid=<id>`): with `-e` or `--exec` it serves an exec session,
 * which is what reaches a `distrobox enter` or any other exec nobody here
 * started, carrying no KDOS_BOX and under `--pid host` no descendant of the
 * init; without, it is the container's main conmon.
 *
 * THE MAIN CONMON IS THE CONTAINER'S SUBREAPER, AND IS NEVER SIGNALLED. Every
 * process the entrypoint leaves behind as the init dies — distrobox's
 * `sleep 15`, the loop's sleep or a setup command the TERM caught it in — is
 * its child. As soon as the init has exited it runs podman's exit command
 * (`podman container cleanup`), then waits for that command and for every
 * child it still has, the dead init's orphans among them, and only then
 * exits. The box's storage stays busy, and a start fails with EBUSY on the
 * overlay, while any process in the container's mount namespace lives; a
 * SIGKILLed main conmon runs no cleanup at all. So every process under it but
 * the init is signalled, except a child that is its exit command or its OCI
 * runtime (argv[0] is its `--exit-command` or its first `-r`), and the main
 * conmon itself is counted while it lives and left to leave on its own, so
 * the stop returns once the orphans are gone. An orphan appears whenever the
 * init dies, which is after the pass that sent it SIGTERM, so every waiting
 * pass of a stop sends the pass's own signal (SIGTERM through the grace,
 * SIGKILL after it) to each process under a main conmon whose chain does not
 * go through the living init: counted and left alone, such an orphan holds
 * the stop until it ends by itself. A box that is `stopped`
 * with nothing of it left alive is one whose conmon died without that
 * cleanup, and the stop runs `podman container cleanup <box>` itself; on a
 * box whose cleanup ran, podman reports it `exited` and the call is not made.
 * An exec conmon is signalled only at SIGKILL, and counted while it lives; it
 * leaves on its own once its process has, and podman reads the exec's exit
 * from what it writes on the way. No conmon is ever matched by its mount
 * namespace or its environment.
 *
 * A DEAD INIT'S NAMESPACE OUTLIVES THE CHAIN TO THE CONMON. The main conmon
 * stops being a subreaper as it starts its exit command, so the child of an
 * orphan whose parent dies goes past it to the session's subreaper or PID 1,
 * where no parent chain reaches a conmon and, under distrobox, no KDOS_BOX
 * marks it. While the init is gone, the mount namespace of every process
 * found under one of the box's conmons — never this program's, nor the one
 * podman runs its conmons in — is remembered through the stop, and a process
 * in one of them is the box's in every later pass.
 *
 * A HANG-UP COMES FIRST TO AN EXEC SESSION. An interactive shell ignores
 * SIGTERM, so a `kdos-box enter` would hold the stop through the whole grace
 * and the SIGKILL pass. The SIGTERM pass sends every process under an exec
 * conmon SIGHUP before it: what a closed terminal sends, on which a shell
 * exits and hangs up its jobs. Neither the init nor the keeper is ever under
 * an exec conmon, so neither is sent one.
 *
 * AND THE LAUNCHES STILL WAITING FOR THE BOX. A launch — `kdos-box run`, a
 * generated launcher's `kdos-appbox -b <box> … run`, `kdos-box enter` — waits
 * up to 30 s for the box to say it is ready before it becomes its `podman
 * exec`, in this program's own mount namespace; left alone through a stop it
 * would go on to launch into the box once something starts it again. While
 * it waits it holds a mark, $XDG_RUNTIME_DIR/kdos/boxes/<box>.wait.<pid> with
 * its start time, and a mark whose pid and start time still name a process is
 * that process's: one that died without removing its mark is passed over and
 * the mark removed. This process is never one of them. Only an explicit stop
 * (`kdos-box stop`, a remove, a pack's removal) reaches the waiting launches;
 * box_unstick, which a launch runs on its own way in, does not, or two
 * launches into a stuck box would end each other. Only processes this user
 * may signal are reached: under keep-id the applications run as the user.
 */
static long box_init_pid(const char *box, char *id, size_t idn)
{
	KbArgv a = {0};
	char out[128] = "", *sp;

	id[0] = 0;
	kb_argv_add(&a, "podman");
	kb_argv_add(&a, "inspect");
	kb_argv_add(&a, "--type");
	kb_argv_add(&a, "container");
	kb_argv_add(&a, "--format");
	kb_argv_add(&a, "{{.State.Pid}} {{.Id}}");
	kb_argv_add(&a, box);
	kb_argv_end(&a);
	if (kb_run_capture(&a, out, sizeof(out)) != 0)
		return 0;
	if ((sp = strchr(out, ' ')))
		kb_strlcpy(id, sp + 1, idn);
	return strtol(out, NULL, 10);
}

/* The parent of `pid` (field 4 of its stat), or 0. */
static long proc_ppid(long pid)
{
	char path[64], buf[1024], *s;
	long pp = 0;

	snprintf(path, sizeof(path), "/proc/%ld/stat", pid);
	if (kb_read_file(path, buf, sizeof(buf)) < 0)
		return 0;
	s = strrchr(buf, ')');
	if (!s || sscanf(s + 1, " %*c %ld", &pp) != 1)
		return 0;
	return pp;
}

#define CONMON_EXEC 1
#define CONMON_MAIN 2

/* A main conmon, with the argv[0] of the two children it starts itself: its
 * exit command and its OCI runtime. */
typedef struct {
	char exitcmd[256];
	char runtime[256];
} MainConmon;

/* CONMON_EXEC when `pid` is a conmon serving an exec session of the
 * container `id`, CONMON_MAIN when it is the container's main conmon (and
 * `m`, when given, is filled), and 0 otherwise. */
static int conmon_kind(long pid, const char *id, MainConmon *m)
{
	char path[64], comm[32], exitcmd[256] = "", runtime[256] = "";
	size_t len = 0;
	char *cl;
	int cid = 0, ex = 0;

	snprintf(path, sizeof(path), "/proc/%ld/comm", pid);
	if (kb_read_file(path, comm, sizeof(comm)) < 0)
		return 0;
	comm[strcspn(comm, "\n")] = 0;
	if (strcmp(comm, "conmon"))
		return 0;
	snprintf(path, sizeof(path), "/proc/%ld/cmdline", pid);
	cl = kb_read_all(path, &len);
	/* A word an option takes as its value is never read as an option:
	 * podman passes its own global options, `--runtime <path>` among
	 * them, as `--exit-command-arg` values after conmon's own `-r`. */
	for (size_t o = 0; cl && o < len; o += strlen(cl + o) + 1) {
		const char *w = cl + o;
		size_t wl = strlen(w);
		const char *next = o + wl + 1 < len ? cl + o + wl + 1 : "";
		int value = 1;

		if (!strcmp(w, "-c") || !strcmp(w, "--cid"))
			cid |= !strcmp(next, id);
		else if (!strcmp(w, "--exit-command"))
			kb_strlcpy(exitcmd, next, sizeof(exitcmd));
		else if (!strcmp(w, "-r") || !strcmp(w, "--runtime")) {
			if (!runtime[0])
				kb_strlcpy(runtime, next, sizeof(runtime));
		} else if (!strcmp(w, "--exit-command-arg") ||
			   !strcmp(w, "--runtime-arg") ||
			   !strcmp(w, "--runtime-opt"))
			;
		else {
			value = 0;
			if (wl > 6 && !strncmp(w, "--cid=", 6) &&
			    !strcmp(strchr(w, '=') + 1, id))
				cid = 1;
			else if (!strcmp(w, "-e") || !strcmp(w, "--exec"))
				ex = 1;
		}
		if (value && *next)
			o += wl + 1;
	}
	free(cl);
	if (!cid)
		return 0;
	if (ex)
		return CONMON_EXEC;
	if (m) {
		kb_strlcpy(m->exitcmd, exitcmd, sizeof(m->exitcmd));
		kb_strlcpy(m->runtime, runtime, sizeof(m->runtime));
	}
	return CONMON_MAIN;
}

/* Whether argv[0] of `pid` is `want`, which is never empty. */
static int argv0_is(long pid, const char *want)
{
	char path[64];
	size_t len = 0;
	char *cl;
	int hit;

	if (!*want)
		return 0;
	snprintf(path, sizeof(path), "/proc/%ld/cmdline", pid);
	cl = kb_read_all(path, &len);
	hit = cl && len && !strcmp(cl, want);
	free(cl);
	return hit;
}

static int ns_of(long pid, struct stat *st)
{
	char path[64];

	if (pid > 0)
		snprintf(path, sizeof(path), "/proc/%ld/ns/mnt", pid);
	else
		snprintf(path, sizeof(path), "/proc/self/ns/mnt");
	return stat(path, st);
}

static int environ_names_box(long pid, const char *box)
{
	char path[64], want[96];
	size_t len = 0;
	char *env;
	int hit = 0;

	snprintf(path, sizeof(path), "/proc/%ld/environ", pid);
	snprintf(want, sizeof(want), "KDOS_BOX=%s", box);
	env = kb_read_all(path, &len);
	for (size_t o = 0; env && o < len && !hit; o += strlen(env + o) + 1)
		hit = !strcmp(env + o, want);
	free(env);
	return hit;
}

/* The start time of `pid` in clock ticks since boot, field 22 of its stat, or
 * 0. A pid and its start time name one process: a pid reused since is
 * another. */
static unsigned long long proc_start(long pid)
{
	char path[64], buf[1024], *s;
	unsigned long long t = 0;

	snprintf(path, sizeof(path), "/proc/%ld/stat", pid);
	if (kb_read_file(path, buf, sizeof(buf)) < 0)
		return 0;
	/* The command name may hold ')' and spaces; the fields after the
	 * last ')' start at the state, field 3. */
	s = strrchr(buf, ')');
	if (!s || sscanf(s + 1, " %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u"
				" %*u %*u %*d %*d %*d %*d %*d %*d %llu", &t) != 1)
		return 0;
	return t;
}

/* $XDG_RUNTIME_DIR/kdos/boxes/<box>.wait.<pid>, holding the waiting
 * process's start time, for as long as box_wait_ready waits. */
static int wait_mark_path(const char *box, long pid, char *out, size_t n)
{
	char dir[MAX_LINE];
	int w;

	if (box_state_dir(dir, sizeof(dir)) != 0)
		return -1;
	w = snprintf(out, n, "%s/%s.wait.%ld", dir, box, pid);
	return w < 0 || (size_t)w >= n ? -1 : 0;
}

/* `sig` to every process with a wait mark for `box` whose start time is the
 * one it recorded; a mark whose process is gone, or whose pid now names
 * another process, is removed. */
static int signal_waits(const char *box, int sig)
{
	char dir[MAX_LINE], path[MAX_LINE], rec[32];
	size_t bl = strlen(box);
	char **names;
	int n = 0;

	if (box_state_dir(dir, sizeof(dir)) != 0)
		return 0;
	names = kb_listdir(dir, NULL);
	for (char **f = names; f && *f; f++) {
		unsigned long long t;
		char *end;
		long pid;

		if (strncmp(*f, box, bl) || strncmp(*f + bl, ".wait.", 6))
			continue;
		pid = strtol(*f + bl + 6, &end, 10);
		if (*end || pid <= 1)
			continue;
		if (pid == (long)getpid() ||
		    wait_mark_path(box, pid, path, sizeof(path)) != 0)
			continue;
		t = proc_start(pid);
		if (!t || kb_read_file(path, rec, sizeof(rec)) < 0 ||
		    strtoull(rec, NULL, 10) != t) {
			unlink(path);
			continue;
		}
		if (kill((pid_t)pid, sig) == 0)
			n++;
	}
	kb_strv_free(names);
	return n;
}

#define EXEC_MAX 64
#define MAIN_MAX 4

/* Which of the `n` pids in `list` the parent chain of `pid` reaches, which
 * is not itself counted, or -1; `top` is the process on the chain just below
 * it, `pid` itself when it is that pid's child. */
static int chain_reaches(long pid, const long *list, int n, long *top)
{
	long p = pid, below = pid;

	for (int depth = 0; n && depth < 64; depth++) {
		p = proc_ppid(p);
		if (p <= 1)
			return -1;
		for (int i = 0; i < n; i++)
			if (p == list[i]) {
				*top = below;
				return i;
			}
		below = p;
	}
	return -1;
}

static int signal_pid(long pid, int sig)
{
	return kill((pid_t)pid, sig) == 0;
}

/* The mount namespaces seen under a box's conmons once its init is gone,
 * kept from one pass of a stop to the next; stop_many empties it as it
 * starts, so a namespace number the kernel has since given to another
 * namespace is never taken for the box's. */
#define ORPHAN_NS_MAX 32
static struct {
	char box[96];
	dev_t dev;
	ino_t ino;
} orphan_ns[ORPHAN_NS_MAX];
static int n_orphan_ns;

static int orphan_ns_has(const char *box, const struct stat *st)
{
	for (int i = 0; i < n_orphan_ns; i++)
		if (orphan_ns[i].ino == st->st_ino &&
		    orphan_ns[i].dev == st->st_dev &&
		    !strcmp(orphan_ns[i].box, box))
			return 1;
	return 0;
}

/* Record the mount namespace of `pid`, found under one of the `nk` conmons
 * in `keep`, unless it is this program's or a conmon's own: podman runs its
 * conmons, and every podman command, in one namespace of its own. */
static void orphan_ns_add(const char *box, long pid, const struct stat *self,
			  const long *keep, int nk)
{
	struct stat st, cm;

	if (ns_of(pid, &st) != 0 || n_orphan_ns >= ORPHAN_NS_MAX ||
	    (st.st_ino == self->st_ino && st.st_dev == self->st_dev) ||
	    orphan_ns_has(box, &st))
		return;
	for (int i = 0; i < nk; i++)
		if (ns_of(keep[i], &cm) == 0 && cm.st_ino == st.st_ino &&
		    cm.st_dev == st.st_dev)
			return;
	kb_strlcpy(orphan_ns[n_orphan_ns].box, box,
		   sizeof(orphan_ns[n_orphan_ns].box));
	orphan_ns[n_orphan_ns].dev = st.st_dev;
	orphan_ns[n_orphan_ns].ino = st.st_ino;
	n_orphan_ns++;
}

/* What a stop reads from podman once, as it starts: the container's full ID,
 * and its init with the init's start time, so that every later pass tells
 * whether the init lives from /proc alone. */
typedef struct {
	char id[96];
	long ipid;
	unsigned long long istart;
} BoxIds;

static void box_ids(const char *box, BoxIds *b)
{
	b->ipid = box_init_pid(box, b->id, sizeof(b->id));
	b->istart = b->ipid > 0 ? proc_start(b->ipid) : 0;
}

/* `sig` to every process of the box (0 counts them), and `late`, when it is
 * not 0, to every process under a main conmon whose chain does not pass
 * through the living init: what the init left there as it died, or since. */
static int signal_all(const char *box, const BoxIds *b, int sig, int late,
		      int waits)
{
	struct stat self, init, st;
	const char *id = b->id;
	long ipid = b->ipid;
	int live = ipid > 0 && b->istart && proc_start(ipid) == b->istart;
	int have_init = live && ns_of(ipid, &init) == 0;
	long conmon[EXEC_MAX + MAIN_MAX], mainpid[MAIN_MAX], mychain[64];
	MainConmon mainc[MAIN_MAX];
	int nc = 0, nm = 0, nmine = 0;
	char **names;
	int n = waits ? signal_waits(box, sig) : 0;

	if (ns_of(0, &self) != 0)
		return n;
	if (have_init && init.st_ino == self.st_ino && init.st_dev == self.st_dev)
		have_init = 0;
	names = kb_listdir("/proc", NULL);
	/* This program and its ancestors are never signalled, even when it
	 * was itself started inside one of the box's exec sessions. */
	for (long p = getpid(); p > 1 && nmine < 64; p = proc_ppid(p))
		mychain[nmine++] = p;
	for (char **f = names; id[0] && f && *f; f++) {
		char *end;
		long pid = strtol(*f, &end, 10);
		MainConmon m;
		int k;

		if (*end || pid <= 1)
			continue;
		k = conmon_kind(pid, id, &m);
		if (k == CONMON_EXEC && nc < EXEC_MAX)
			conmon[nc++] = pid;
		else if (k == CONMON_MAIN && nm < MAIN_MAX) {
			mainc[nm] = m;
			mainpid[nm++] = pid;
		}
	}
	/* The exec conmons, then the main ones: every conmon of the box. */
	for (int i = 0; i < nm; i++)
		conmon[nc + i] = mainpid[i];
	for (char **f = names; f && *f; f++) {
		long pid, top;
		char *end;
		int mine, skip = 0, i;

		pid = strtol(*f, &end, 10);
		if (*end || pid <= 1 || (live && pid == ipid))
			continue;
		for (i = 0; i < nmine && !skip; i++)
			skip = pid == mychain[i];
		for (i = 0; i < nc && !skip; i++)
			skip = pid == conmon[i];
		for (i = 0; i < nm && !skip; i++)
			skip = pid == mainpid[i];
		if (skip)
			continue;
		if (chain_reaches(pid, conmon, nc, &top) >= 0) {
			if (!have_init)
				orphan_ns_add(box, pid, &self, conmon, nc + nm);
			if (sig == SIGTERM)
				signal_pid(pid, SIGHUP);
			n += signal_pid(pid, sig);
			continue;
		}
		if ((i = chain_reaches(pid, mainpid, nm, &top)) >= 0) {
			if (!argv0_is(top, mainc[i].exitcmd) &&
			    !argv0_is(top, mainc[i].runtime)) {
				if (!have_init)
					orphan_ns_add(box, pid, &self, conmon,
						      nc + nm);
				if (late && !(live && top == ipid))
					signal_pid(pid, late);
				n += signal_pid(pid, sig);
			}
			continue;
		}
		if (ns_of(pid, &st) != 0)
			continue;
		if (st.st_ino == self.st_ino && st.st_dev == self.st_dev)
			continue;
		mine = have_init ? st.st_ino == init.st_ino &&
				   st.st_dev == init.st_dev
				 : environ_names_box(pid, box) ||
				   orphan_ns_has(box, &st);
		if (mine)
			n += signal_pid(pid, sig);
	}
	for (int i = 0; (sig == 0 || sig == SIGKILL) && i < nc; i++) {
		int skip = 0;

		for (int k = 0; k < nmine && !skip; k++)
			skip = conmon[i] == mychain[k];
		if (!skip)
			n += signal_pid(conmon[i], sig);
	}
	for (int i = 0; sig == 0 && i < nm; i++)
		n += signal_pid(mainpid[i], 0);
	kb_strv_free(names);
	return n;
}

int box_signal_all(const char *box, int sig)
{
	BoxIds b;

	box_ids(box, &b);
	return signal_all(box, &b, sig, 0, 1);
}

static int podman_kill(const char *box, const char *sig)
{
	KbArgv k = {0};
	char err[512];

	kb_argv_add(&k, "podman");
	kb_argv_add(&k, "kill");
	kb_argv_add(&k, "--signal");
	kb_argv_add(&k, sig);
	kb_argv_add(&k, box);
	kb_argv_end(&k);
	return kb_run_feed_env(&k, NULL, 0, "", 0, err, sizeof(err));
}

/* `podman container cleanup <box>`: what the main conmon's exit command
 * does, for a box whose conmon died without running it. */
static void podman_cleanup(const char *box)
{
	KbArgv c = {0};
	char err[512];

	kb_argv_add(&c, "podman");
	kb_argv_add(&c, "container");
	kb_argv_add(&c, "cleanup");
	kb_argv_add(&c, box);
	kb_argv_end(&c);
	kb_run_feed_env(&c, NULL, 0, "", 0, err, sizeof(err));
}

static int box_down(const char *state)
{
	return strcmp(state, "running") && strcmp(state, "stopping") &&
	       strcmp(state, "paused");
}

/*
 * SIGTERM TO THE APPLICATIONS (SIGHUP first to the exec sessions), THE
 * WAITING LAUNCHES (when `waits`) AND THE INIT, `grace` seconds by the clock
 * for every box to leave `running` and `stopping` and for all of them, the
 * main conmon among them, to be gone, then SIGKILL to whatever is left but
 * the main conmon and three seconds more, the namespaces a dead init's
 * processes were found in remembered from the first pass to the last (the
 * comment above box_init_pid says why). Each waiting pass sends its own
 * signal again to every orphan under a main conmon, so one the init left as
 * it died is ended within a pass rather than waited out. Podman is asked for
 * the container's ID and its init once, as the stop starts; a waiting pass
 * reads /proc first and asks podman for the box's state only when nothing of
 * the box is alive there, since every `podman inspect` and `podman kill`
 * takes the container's lock, which the exit command holds while it cleans
 * the box, and each costs a podman start-up. The applications are signalled
 * whatever the init does, so a box that is down holds no exec session that a
 * later remove would refuse over, and no waiting launch is left to go into
 * it after the next start. A box left `stopped` with nothing of it alive is
 * given the `podman container cleanup` its conmon did not run. Returns how
 * many of the boxes are still up. box_stop_many is the explicit stop, with
 * the waiting launches; box_unstick runs it without them.
 */
static long long mono_ms(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000LL + ts.tv_nsec / 1000000;
}

static int stop_many(const char *const *boxes, int n, int grace, int waits)
{
	BoxIds *ids = kb_calloc(n > 0 ? n : 1, sizeof(*ids));
	char state[64];
	int up = 0;

	n_orphan_ns = 0;
	for (int i = 0; i < n; i++)
		box_ids(boxes[i], &ids[i]);
	for (int i = 0; i < n; i++) {
		signal_all(boxes[i], &ids[i], SIGTERM, 0, waits);
		box_state(boxes[i], state, sizeof(state));
		if (!box_down(state))
			podman_kill(boxes[i], "TERM");
	}
	for (int pass = 0; pass < 2; pass++) {
		int sig = pass ? SIGKILL : SIGTERM;
		long long end = mono_ms() + (pass ? 3 : grace) * 1000LL;

		for (;;) {
			up = 0;
			for (int i = 0; i < n; i++) {
				if (signal_all(boxes[i], &ids[i], 0, sig,
					       waits) > 0) {
					up++;
					continue;
				}
				box_state(boxes[i], state, sizeof(state));
				up += !box_down(state);
			}
			if (!up || mono_ms() >= end)
				break;
			usleep(200000);
		}
		if (!up || pass)
			break;
		for (int i = 0; i < n; i++) {
			signal_all(boxes[i], &ids[i], SIGKILL, 0, waits);
			box_state(boxes[i], state, sizeof(state));
			if (!box_down(state))
				podman_kill(boxes[i], "KILL");
		}
	}
	for (int i = 0; i < n; i++) {
		box_state(boxes[i], state, sizeof(state));
		if (!strcmp(state, "stopped") &&
		    signal_all(boxes[i], &ids[i], 0, 0, waits) == 0)
			podman_cleanup(boxes[i]);
	}
	free(ids);
	return up;
}

int box_stop_many(const char *const *boxes, int n, int grace)
{
	return stop_many(boxes, n, grace, 1);
}

/*
 * A CONTAINER IN `stopping` IS NOT STARTABLE, AND PODMAN SAYS SO IN A WAY
 * THAT NAMES NOTHING USEFUL: "must be in Created or Stopped state to be
 * started: container state improper". A `podman stop` that timed out
 * (box_signal_all says why) leaves a box there, and a hung app holding
 * D-state I/O can wedge one there for good.
 *
 * The stop sequence above runs, with a short grace and without the waiting
 * launches, the caller's own siblings among them. Only a PACK box is ever
 * removed past that: it keeps nothing in its container — its packs are
 * mounted read-only and its writable upper lives outside it, on disk or on
 * tmpfs in a live session — so recreating the container over the same stack
 * loses nothing, and a return of 3 tells the caller to create it again. Every
 * other box (a store box over a local image, or one with no recorded base)
 * keeps its writes inside the container, and removing it would delete them:
 * it is left stuck, a return of 4, with a warning naming `kdos-box remove
 * <box> --force`, which is the one place that data is given up and only when
 * somebody asks.
 */
int box_unstick(const char *box, char *state, size_t n)
{
	Profile p;

	box_state(box, state, n);
	if (strcmp(state, "stopping"))
		return 0;

	stop_many(&box, 1, 3, 0);
	box_state(box, state, n);
	if (strcmp(state, "stopping"))
		return 2;

	profile_load(&p, box);
	if (strncmp(p.base, "pack:", 5)) {
		kb_warn("%s is stuck stopping and its writes are inside its "
			"container, so it is left as it is — `kdos-box remove "
			"%s --force` discards the box and everything it wrote",
			box, box);
		return 4;
	}

	{
		KbArgv r = {0};
		kb_argv_add(&r, "podman");
		kb_argv_add(&r, "rm");
		kb_argv_add(&r, "-f");
		kb_argv_add(&r, "-t");
		kb_argv_add(&r, "0");
		kb_argv_add(&r, box);
		kb_argv_end(&r);
		kb_run(&r);
	}
	box_state(box, state, n);
	return 3;
}

static void profile_render(const Profile *p, char *buf, size_t cap)
{
	snprintf(buf, cap,
		 "# KDOS box profile. base, image, persistence, network, ipc,\n"
		 "# devices, processes, home, init, gpu, memory, cpus and pids\n"
		 "# are applied when the box is CREATED: a change to one takes\n"
		 "# effect once the box is removed (`kdos-box remove %s`) and\n"
		 "# created again. The other keys apply at its next launch.\n"
		 "base=%s\n"
		 "image=%s\n"
		 "accent=%s\n"
		 "grant=%s\n"
		 "persistence=%s\n"
		 "network=%s\n"
		 "ipc=%s\n"
		 "devices=%s\n"
		 "processes=%s\n"
		 "home=%s\n"
		 "init=%s\n"
		 "wayland=%s\n"
		 "audio=%s\n"
		 "gpu=%s\n"
		 "x11=%s\n"
		 "display=%s\n"
		 "render=%s\n"
		 "export=%s\n"
		 "memory=%s\n"
		 "cpus=%s\n"
		 "pids=%d\n"
		 "autostop=%ds\n",
		 p->name, p->base, p->image,
		 p->accent[0] ? p->accent : "session",
		 p->grant,
		 persist_name(p->persist),
		 p->netnone ? "none" : p->netns ? "private" : "host",
		 p->ipc ? "private" : "shared",
		 p->devsys ? "private" : "shared",
		 p->process ? "private" : "shared",
		 p->privhome ? "private" : "shared",
		 p->init ? "yes" : "no",
		 p->wayland ? "yes" : "no",
		 p->audio ? "yes" : "no",
		 p->gpu ? "yes" : "no",
		 p->x11[0] ? p->x11 : "yes",
		 p->display[0] ? p->display : "window",
		 p->render[0] ? p->render : "auto",
		 p->autoexport ? "auto" : "manual",
		 p->memory, p->cpus, p->pids, p->autostop_s);
}

int profile_save(const Profile *p)
{
	char *path = profile_path(p->name);
	char buf[MAX_LINE];
	int rc;

	profile_render(p, buf, sizeof(buf));
	/* ATOMIC: a profile that comes back empty has lost `base`, and a box
	 * whose base is unknown cannot be composed or started again. */
	rc = kb_write_file_atomic(path, buf);
	free(path);
	return rc;
}

int profile_is_kept(const char *box)
{
	char *pp = profile_path(box);
	char *t = kb_read_whole(pp, NULL);
	int kept = t && !strncmp(t, BOX_KEPT_MARK, strlen(BOX_KEPT_MARK));

	free(t);
	free(pp);
	return kept;
}

void profile_mark_kept(const char *box)
{
	char *pp = profile_path(box);
	char *t = kb_read_whole(pp, NULL);

	if (t && strncmp(t, BOX_KEPT_MARK, strlen(BOX_KEPT_MARK))) {
		KbBuf b = {0};

		kb_buf_printf(&b, "%s%s", BOX_KEPT_MARK, t);
		if (kb_write_file_atomic(pp, b.p) != 0)
			kb_warn("%s: %s", pp, strerror(errno));
		kb_buf_free(&b);
	}
	free(t);
	free(pp);
}

/* The create-time keys, in the digest's order, by the names the profile file
 * spells them with. */
static const char *const PROFILE_CREATE_KEYS[] = {
	"base", "image", "persistence", "network", "ipc", "devices",
	"processes", "home", "init", "gpu", "memory", "cpus", "pids", NULL
};

/* Each key's value as profile_render writes it. `image` is derived from an
 * `image:` base at create, so it counts only for a profile with no base. */
static void create_key_value(const Profile *p, int k, char *out, size_t n)
{
	switch (k) {
	case 0:  snprintf(out, n, "%s", p->base); break;
	case 1:  snprintf(out, n, "%s", p->base[0] ? "" : p->image); break;
	case 2:  snprintf(out, n, "%s", persist_name(p->persist)); break;
	case 3:  snprintf(out, n, "%s", p->netnone ? "none"
				  : p->netns ? "private" : "host"); break;
	case 4:  snprintf(out, n, "%d", p->ipc); break;
	case 5:  snprintf(out, n, "%d", p->devsys); break;
	case 6:  snprintf(out, n, "%d", p->process); break;
	case 7:  snprintf(out, n, "%d", p->privhome); break;
	case 8:  snprintf(out, n, "%d", p->init); break;
	case 9:  snprintf(out, n, "%d", p->gpu); break;
	case 10: snprintf(out, n, "%s", p->memory); break;
	case 11: snprintf(out, n, "%s", p->cpus); break;
	default: snprintf(out, n, "%d", p->pids); break;
	}
}

void profile_create_digest(const Profile *p, char *out, size_t n)
{
	size_t at = (size_t)snprintf(out, n, "v1.");

	for (int k = 0; PROFILE_CREATE_KEYS[k] && at + 9 <= n; k++) {
		char val[320], hex[65];
		KbSha256 h;

		create_key_value(p, k, val, sizeof(val));
		kb_sha256_init(&h);
		kb_sha256_update(&h, PROFILE_CREATE_KEYS[k],
				 strlen(PROFILE_CREATE_KEYS[k]));
		kb_sha256_update(&h, "=", 1);
		kb_sha256_update(&h, val, strlen(val));
		kb_sha256_final(&h, hex);
		memcpy(out + at, hex, 8);
		at += 8;
		out[at] = '\0';
	}
}

int profile_digest_diff(const char *a, const char *b, char *out, size_t n)
{
	int nk = 0, changed = 0, whole;

	while (PROFILE_CREATE_KEYS[nk])
		nk++;
	whole = strncmp(a, "v1.", 3) || strncmp(b, "v1.", 3) ||
		strlen(a) != 3 + 8 * (size_t)nk || strlen(b) != strlen(a);
	if (n)
		out[0] = '\0';
	for (int k = 0; k < nk; k++) {
		size_t l;

		if (!whole && !memcmp(a + 3 + 8 * k, b + 3 + 8 * k, 8))
			continue;
		l = strlen(out);
		snprintf(out + l, n - l, "%s%s", changed ? ", " : "",
			 PROFILE_CREATE_KEYS[k]);
		changed++;
	}
	return changed;
}

int tty_foreground(int fd)
{
	pid_t fg;

	if (!isatty(fd))
		return 0;
	fg = tcgetpgrp(fd);
	return fg >= 0 && fg == getpgrp();
}

/* The digest the box's container was created with, "" when it has none or
 * podman could not say. */
static void box_profile_label(const char *box, char *out, size_t n)
{
	KbArgv a = {0};

	kb_argv_add(&a, "podman");
	kb_argv_add(&a, "inspect");
	kb_argv_add(&a, "--type");
	kb_argv_add(&a, "container");
	kb_argv_add(&a, "--format");
	kb_argv_add(&a, "{{index .Config.Labels \"" BOX_PROFILE_LABEL "\"}}");
	kb_argv_add(&a, box);
	kb_argv_end(&a);
	if (kb_run_capture(&a, out, n) != 0)
		out[0] = '\0';
	out[strcspn(out, "\r\n")] = '\0';
	if (!strcmp(out, "<no value>"))
		out[0] = '\0';
}

/*
 * WHAT IT ENFORCED, not what the file said. Every line below names the podman
 * flag or the KDOS mechanism behind it, and anything this build cannot deliver
 * is printed as such — KDOS does not offer confinement it cannot enforce, and
 * a settings dialog that lists one is a lie with a checkbox next to it.
 */
void profile_print(const Profile *p)
{
	printf("box         = %s\n", p->name);
	if (p->base[0])
		printf("base        = %s\n", p->base);
	else if (p->image[0])
		printf("image       = %s\n", p->image);
	else
		printf("base        = (none yet — the first launch records the pack)\n");
	/*
	 * A PROFILE WHOSE CREATE-TIME KEYS DIFFER FROM ITS CONTAINER'S LABEL IS
	 * NOT WHAT THE BOX RUNS: the lines below are the file's, and the
	 * container was made from what the label recorded (BOX_PROFILE_LABEL).
	 * The keys that differ are named. A box with no container prints
	 * nothing, because its next create reads the file as it is now, and
	 * nor does a container with no `v1.` label — one distrobox made by
	 * itself — since nothing records what it was made from.
	 */
	{
		char label[PROFILE_DIGEST_MAX + 32], now[PROFILE_DIGEST_MAX];
		char keys[256];
		int nk;

		box_profile_label(p->name, label, sizeof(label));
		profile_create_digest(p, now, sizeof(now));
		if (!strncmp(label, "v1.", 3) &&
		    (nk = profile_digest_diff(label, now, keys, sizeof(keys))) > 0)
			printf("            ! the profile was written after this box"
			       " was created: %s %s — %s once `kdos-box remove"
			       " %s` has run and the box is made again\n", keys,
			       nk == 1 ? "differs" : "differ",
			       nk == 1 ? "it applies" : "they apply", p->name);
	}
	/*
	 * THE TITLE BAR'S COLOUR IS A KWin RULE, written by rules.c for every
	 * box the user has, and it is named here by the scheme it forces. No
	 * accent is printed `none`: the file's token for it is `session`, which
	 * reads as "the session's accent" and is the neutral KDOSBox scheme. A
	 * name that is no libkcolor accent is said to be one, because the
	 * rule falls back to the neutral scheme without a word.
	 */
	{
		char scheme[64];

		rules_scheme(p->accent, scheme, sizeof(scheme));
		printf("accent      = %-11s title bars in %s (KWin rule kdos-box-%s)\n",
		       accent_shown(p->accent), scheme, p->name);
		if (!accent_known(p->accent))
			printf("            ! accent '%s' is no KDOS accent — title"
			       " bars stay neutral (KDOSBox)\n", p->accent);
	}
	/*
	 * EVERY GRANT IS NAMED WITH WHAT IT OPENS, and a name that opens nothing
	 * is said to: a misspelt grant would otherwise read as a permission.
	 */
	if (!p->grant[0]) {
		printf("grant       = none        KWin denies its windows the desktop-wide globals\n");
	} else {
		char copy[sizeof(p->grant)], *save = NULL;

		printf("grant       = %s\n", p->grant);
		kb_strlcpy(copy, p->grant, sizeof(copy));
		for (char *t = strtok_r(copy, " ,\t", &save); t;
		     t = strtok_r(NULL, " ,\t", &save)) {
			int known = 0;

			for (size_t i = 0; i < sizeof(GRANTS) / sizeof(*GRANTS); i++)
				if (!strcmp(GRANTS[i].name, t)) {
					printf("              %-17s %s\n", t,
					       GRANTS[i].what);
					known = 1;
				}
			if (!known)
				printf("            ! grant '%s' is no name KWin's sandbox "
				       "policy or the bus proxy knows — it opens nothing\n",
				       t);
		}
	}
	/*
	 * WHAT A BOX WRITES, BY LANE. A pack box's root is kdos-packd's overlay,
	 * and `--rootfs …:O` lays a second, throwaway one over it, whose writes
	 * podman destroys when the container stops; `--rm` then removes the
	 * container itself. A store box keeps its writes in its container, so
	 * `ephemeral` is that container removed at stop and `frozen` has nothing
	 * to discard them with. A distrobox box keeps everything.
	 */
	{
		int lane = box_lane(p);
		const char *how = "writes kept";

		if (lane == BOX_LANE_PACK)
			how = p->persist == PERSIST_EPHEMERAL ? "--rootfs …:O --rm"
			    : p->persist == PERSIST_FROZEN    ? "--rootfs …:O"
			    : "kdos-packd's upper, kept";
		else if (lane == BOX_LANE_STORE && p->persist == PERSIST_EPHEMERAL)
			how = "--rm";
		printf("persistence = %-11s %s\n", persist_name(p->persist), how);
		if (p->persist != PERSIST_PERSISTENT &&
		    (lane == BOX_LANE_DISTROBOX ||
		     (lane == BOX_LANE_STORE && p->persist == PERSIST_FROZEN)))
			printf("            ! %s is not enforced — this box keeps its"
			       " writes inside its container, and nothing discards"
			       " them short of `kdos-box remove`\n",
			       persist_name(p->persist));
		/*
		 * KEPT IS ONLY AS LONG AS THE FILESYSTEM UNDER THEM, and the
		 * remedy differs by lane. A pack box's upper is kdos-packd's,
		 * and kdos-packd puts it in the runtime directory's tmpfs
		 * whenever $HOME is an overlay — the kernel stacks no upper on
		 * overlayfs — so on a live session it ends with the session
		 * whether or not a `kdos persist` store is in use, and only an
		 * installed system keeps it; a $HOME on tmpfs holds it in
		 * memory as well. A store or distrobox box writes
		 * into podman's storage under $HOME, which ends with the
		 * session when the overlay's upper is RAM and outlasts it when
		 * the upper is a persistence store.
		 */
		if (!(lane == BOX_LANE_PACK && p->persist != PERSIST_PERSISTENT) &&
		    !(lane == BOX_LANE_STORE && p->persist == PERSIST_EPHEMERAL)) {
			char where[MAX_LINE];

			if (lane == BOX_LANE_PACK) {
				char *h = profile_home(p->name);

				kb_strlcpy(where, h, sizeof(where));
				free(h);
				if (fs_is_overlay(where) || fs_in_memory(where))
					printf("            ! the writes are in this"
					       " live session's memory and end with"
					       " it — a pack box keeps them only on"
					       " an installed system\n");
			} else {
				podman_storage_dir(where, sizeof(where));
				if (fs_in_memory(where))
					printf("            ! the writes are on this"
					       " live session's memory overlay and"
					       " end with it — a `kdos persist`"
					       " store keeps them from its next"
					       " boot\n");
			}
		}
	}
	/*
	 * THE NAMESPACES BY LANE, each named by what made it. distrobox is
	 * told with its own `--unshare-*` and `--home` flags and shares the
	 * host's namespaces otherwise. A pack or store box is a `podman
	 * create` of this program's (pack_box_create), whose private states
	 * are podman's rootless defaults — no flag at all — and whose shared
	 * ones are `--network host`, `--ipc host`, `--pid host` and binds;
	 * naming distrobox's flags for those would describe a command that
	 * never ran.
	 */
	if (box_lane(p) == BOX_LANE_DISTROBOX) {
		printf("network     = %-11s %s\n",
		       p->netnone ? "none" : p->netns ? "private" : "host",
		       p->netnone ? "--network none" :
		       p->netns   ? "--unshare-netns" : "--network host");
		printf("ipc         = %-11s %s\n", p->ipc ? "private" : "shared",
		       p->ipc ? "--unshare-ipc" : "--ipc host");
		printf("devices     = %-11s %s\n", p->devsys ? "private" : "shared",
		       p->devsys ? "--unshare-devsys" : "/dev and /sys bind-mounted");
		printf("processes   = %-11s %s\n", p->process ? "private" : "shared",
		       p->process ? "--unshare-process" : "--pid host");
		printf("home        = %-11s %s\n", p->privhome ? "private" : "shared",
		       p->privhome ? "--home" : "the user's own $HOME");
	} else {
		printf("network     = %-11s %s\n",
		       p->netnone ? "none" : p->netns ? "private" : "host",
		       p->netnone ? "--network none" :
		       p->netns   ? "a network namespace of its own (podman's rootless default)"
				  : "--network host");
		printf("ipc         = %-11s %s\n", p->ipc ? "private" : "shared",
		       p->ipc ? "an IPC namespace of its own (podman's default)"
			      : "--ipc host");
		printf("devices     = %-11s %s\n", p->devsys ? "private" : "shared",
		       p->devsys ? "podman's own /dev and /sys"
				 : "/dev and /sys bind-mounted");
		printf("processes   = %-11s %s\n", p->process ? "private" : "shared",
		       p->process ? "a PID namespace of its own (podman's default)"
				  : "--pid host");
		printf("home        = %-11s %s\n", p->privhome ? "private" : "shared",
		       p->privhome ? "its layer directory's home, bound at $HOME"
				   : "the user's own $HOME, bound in");
	}
	/*
	 * A SHARED HOME IS A WAY OUT, and the profile says so wherever it is
	 * shared: a program that can write ~/.config/autostart or a shell's
	 * profile decides what the next login runs, outside every box. The box
	 * profiles themselves are bound read-only into a box, so a box cannot
	 * grant itself what its profile withholds.
	 */
	if (!p->privhome)
		printf("            ! home=shared lets the box change what the next"
		       " login runs (~/.config/autostart, ~/.bashrc); only"
		       " home=private keeps its writes inside the box\n");
	/*
	 * THE SOCKETS ARE THE RUNTIME DIRECTORY'S, AND ONLY A PRIVATE ONE CAN
	 * WITHHOLD THEM. A pack box or a store box sees
	 * $XDG_RUNTIME_DIR/kdos/box/<box> as its /run/user/<uid>, and what a
	 * launch puts in it is all there is: the tagged Wayland socket KWin
	 * filters on, the audio sockets, the X11 cookie. A distrobox box sees the
	 * session's own directory, KWin's `wayland-1`, PipeWire and the cookie
	 * included, whatever these keys say — which is why a box made that way
	 * is at most `tagged`.
	 */
	{
		int priv = box_lane(p) != BOX_LANE_DISTROBOX;
		int x11 = profile_x11(p);

		printf("wayland     = %-11s %s\n", p->wayland ? "yes" : "no",
		       p->wayland ? "a tagged socket from kdos-boxsock in its runtime directory"
		       : priv ? "no Wayland socket in its runtime directory"
		       : "the session's socket is still in reach");
		if (!p->wayland && !priv)
			printf("            ! wayland=no cannot be enforced — this box"
			       " shares the session's runtime directory, and the"
			       " session's socket is in it\n");
		printf("audio       = %-11s %s\n", p->audio ? "yes" : "no",
		       p->audio ? "PipeWire and PulseAudio sockets linked into its runtime directory"
		       : priv ? "no audio socket in its runtime directory"
		       : "the session's audio sockets are still in reach");
		if (!p->audio && !priv)
			printf("            ! audio=no cannot be enforced — this box"
			       " shares the session's runtime directory, PipeWire's"
			       " socket included\n");
		printf("x11         = %-11s %s\n", p->x11[0] ? p->x11 : "yes",
		       x11 ? "DISPLAY and the session's cookie: its X11 windows reach KWin untagged"
		       : "no X11 cookie: Xwayland refuses it");
		if (!x11 && !priv)
			printf("            ! x11=no cannot be enforced — the session's"
			       " X11 cookie is in the runtime directory this box"
			       " shares\n");
		else if (!x11 && x11_cookie_exposed(p))
			printf("            ! x11=no cannot be enforced — the session's"
			       " X11 cookie is where this box can read it ($XAUTHORITY"
			       " under /tmp or a home it shares)\n");
	}
	/*
	 * THE RENDERER, AND THE MACHINE'S ANSWER BESIDE IT. The key is a
	 * request and the render node is what grants it, so printing the word
	 * from the file alone would report hardware on a machine that has none
	 * — the exact lie this format exists to avoid.
	 *
	 * THE NODE IS NAMED ONLY WHERE THE BOX CAN REACH IT. A private /dev
	 * with `gpu = no` has no /dev/dri in it, so the host path this process
	 * just opened is a path that does not exist inside the box, and
	 * printing it would answer the question with another machine's /dev.
	 *
	 * The software answer names the variable it exports and calls it
	 * advisory, because that is what it is: an application may unset
	 * LIBGL_ALWAYS_SOFTWARE, and nothing here stops one.
	 */
	{
		char node[256] = "";
		int gpu = profile_render_gpu(p);
		int reach = !(p->devsys && !p->gpu);

		if (reach)
			box_render_node(node, sizeof(node));
		printf("render      = %-11s %s\n",
		       p->render[0] ? p->render : "auto",
		       gpu	   ? node
		       : !reach	   ? "LIBGL_ALWAYS_SOFTWARE=1 (advisory) — no /dev/dri inside this box"
		       : node[0]   ? "LIBGL_ALWAYS_SOFTWARE=1 (advisory) — the profile refuses the card"
				   : "LIBGL_ALWAYS_SOFTWARE=1 (advisory) — no render node on this machine");
	}
	printf("memory      = %-11s %s\n", p->memory[0] ? p->memory : "unlimited",
	       p->memory[0] ? "--memory" : "");
	printf("cpus        = %-11s %s\n", p->cpus[0] ? p->cpus : "all",
	       p->cpus[0] ? "--cpus" : "");
	if (p->pids)
		printf("pids        = %-11d --pids-limit\n", p->pids);
	/*
	 * A PACK BOX'S CGROUP IS ITS LOGIN SESSION'S, the one place a rootless
	 * podman may apply `memory`, `cpus` and `pids`. A store box takes no
	 * session's cgroup, because its writes are in its container and a
	 * container whose cgroup parent went with an old session can never
	 * start again; it has the flags accepted and enforced by nothing but
	 * kdos-oomd's budget check, which reads `memory` alone: `cpus` and
	 * `pids` are held by nothing, and the note names only the limits the
	 * profile sets. Nor does a pack box created in a login whose cgroup
	 * cannot take it (box_cgroup_parent says why).
	 */
	if (p->memory[0] || p->cpus[0] || p->pids > 0) {
		char scope[1024];

		if (box_lane(p) != BOX_LANE_PACK ||
		    !box_cgroup_parent(p->name, scope, sizeof(scope))) {
			const char *loose = p->cpus[0] && p->pids > 0
					    ? "`cpus` or `pids`"
					    : p->cpus[0] ? "`cpus`"
					    : p->pids > 0 ? "`pids`" : NULL;

			printf("            ! the limits are podman flags this box"
			       " gets no cgroup of its own for —");
			if (p->memory[0])
				printf(" kdos-oomd's budget check is what holds"
				       " it to `memory`%s", loose ? "," : "");
			if (loose)
				printf(" nothing holds it to %s", loose);
			printf("\n");
		}
	}
	if (p->autostop_s)
		printf("autostop    = %-11d seconds after start with no window,"
		       " enforced by `kdos-box gc`\n", p->autostop_s);
	/*
	 * ONE ROUTE, WHICHEVER WORD IS IN THE FILE. `kdos-appbox genlaunchers`
	 * writes the entries, the shims and the mime cache for every installed
	 * pack and every store box at once, and `kdos-box export` is the
	 * per-application route for a box that is neither. Neither of them
	 * consults this key.
	 */
	printf("export      = %-11s %s\n", p->autoexport ? "auto" : "manual",
	       "`kdos-box export` is how one app gets a launcher");
	if (p->autoexport)
		printf("            ! export=auto is not a trigger — nothing"
		       " exports by itself; `kdos-appbox genlaunchers` does"
		       " every pack and store box\n");
	if (p->privhome) {
		char *h = profile_home(p->name);
		/* A pack or store box has it at the user's home path, from the
		 * layer directory's `home`; distrobox's --home is the path. */
		if (box_lane(p) != BOX_LANE_DISTROBOX)
			printf("home-path   = %s/home (seen in the box as %s)\n", h,
			       kb_home_dir());
		else
			printf("home-path   = %s\n", h);
		free(h);
	}
	/*
	 * THE CARD'S DEVICE NODES, WHICH IS A DIFFERENT KEY FROM `render`:
	 * this one says whether /dev/dri reaches the box at all — hardware GL,
	 * VA-API decode and Vulkan all need it — and `render` says which
	 * renderer composites the guest's windows.
	 *
	 * It is enforceable in exactly one direction. A box with `devices =
	 * shared` has the host's whole /dev bind-mounted and nothing can be
	 * subtracted from it; a box with `devices = private` has none of it,
	 * and this key is what binds the render nodes back in by themselves.
	 * Both states are printed as what they are.
	 */
	printf("gpu         = %-11s %s\n", p->gpu ? "yes" : "no",
	       !p->devsys	 ? "follows `devices` — the host's /dev is shared whole"
	       : p->gpu		 ? "--volume /dev/dri"
				 : "no device nodes reach it");
	/*
	 * A SHARED /dev CANNOT HAVE A HOLE CUT IN IT, and a key that reported
	 * "no" here while changing nothing would be exactly the lie this format
	 * avoids. There is no podman flag that grants a box a speaker and
	 * denies it a camera, which is why this is printed only for the shared
	 * case: with `devices = private` the box starts with no /dev at all and
	 * `gpu` is the flag above that binds the card back. `audio = no` still
	 * takes the audio SOCKETS away, above; the sound card is a device.
	 */
	if (!p->devsys && (!p->audio || !p->gpu))
		printf("            ! audio=%s gpu=%s cannot take a device away"
		       " — /dev is shared whole, so the sound card and the"
		       " graphics card follow `devices`\n",
		       p->audio ? "yes" : "no", p->gpu ? "yes" : "no");
	for (int i = 0; i < p->nunknown; i++)
		printf("            ! boxes/%s.conf: unknown key '%s'\n", p->name,
		       p->unknown[i]);
}

int image_exists(const char *image)
{
	KbArgv a = {0};
	kb_argv_add(&a, "podman");
	kb_argv_add(&a, "image");
	kb_argv_add(&a, "exists");
	kb_argv_add(&a, image);
	kb_argv_end(&a);
	return kb_run(&a) == 0;
}

/*
 * Does the image declare a label?
 *
 * Two of them decide how Qt apps are themed — `kdos.qt-gtk-theme` (the qgtk3
 * platform themes are installed) and `kdos.qt-kde-theme` (the kde one is, along
 * with the KDE segment). Each is set by the Containerfile in the SAME layer that
 * installs what it promises, so the label and the image cannot drift; asking the
 * image is what stops kdos-appbox exporting an environment the image cannot
 * honour, and an unhonoured QT_STYLE_OVERRIDE=Fusion is worse than nothing (Qt
 * falls back to its built-in LIGHT palette).
 *
 * Cached per label for the boot: `podman image inspect` costs ~150 ms against a
 * ~300 ms warm launch, and the image cannot change without a reboot.
 */
int image_has_label(const char *image, const char *label)
{
	char name[128], buf[64] = {0};
	size_t n = 0;
	int yes;

	/* The cache file is named after the label, so a label with a slash or a
	 * space in it cannot name a path of its own choosing. */
	n += (size_t)snprintf(name, sizeof(name), "kdos-appbox.label.");
	for (const char *c = label; *c && n + 1 < sizeof(name); c++)
		name[n++] = (*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') ||
				    (*c >= '0' && *c <= '9')
				    ? *c
				    : '-';
	name[n] = '\0';

	char *cache = kb_path_join(kb_runtime_dir(), name);
	if (kb_read_file(cache, buf, sizeof(buf)) < 0) {
		KbArgv a = {0};
		char fmt[128];
		snprintf(fmt, sizeof(fmt), "{{index .Labels \"%s\"}}", label);
		kb_argv_add(&a, "podman");
		kb_argv_add(&a, "image");
		kb_argv_add(&a, "inspect");
		kb_argv_add(&a, "--format");
		kb_argv_add(&a, fmt);
		kb_argv_add(&a, image);
		kb_argv_end(&a);
		if (kb_run_capture(&a, buf, sizeof(buf)) != 0)
			buf[0] = '\0';
		kb_write_file(cache, buf);
	}
	yes = (buf[0] == '1');
	free(cache);
	return yes;
}

int box_exists(const char *box)
{
	KbArgv a = {0};
	kb_argv_add(&a, "podman");
	kb_argv_add(&a, "container");
	kb_argv_add(&a, "exists");
	kb_argv_add(&a, box);
	kb_argv_end(&a);
	return kb_run(&a) == 0;
}

int box_state(const char *box, char *buf, size_t n)
{
	KbArgv a = {0};
	kb_argv_add(&a, "podman");
	kb_argv_add(&a, "inspect");
	kb_argv_add(&a, "--type");
	kb_argv_add(&a, "container");
	kb_argv_add(&a, "--format");
	kb_argv_add(&a, "{{.State.Status}}");
	kb_argv_add(&a, box);
	kb_argv_end(&a);
	if (kb_run_capture(&a, buf, n) != 0 || !buf[0])
		snprintf(buf, n, "absent");
	return 0;
}

/* Letters, digits and dots: a size (`4G`), a count or a fraction (`1.5`),
 * and nothing a shell would split, expand or quote. */
static int plain_word(const char *s)
{
	if (!*s)
		return 0;
	for (; *s; s++)
		if (!isalnum((unsigned char)*s) && *s != '.')
			return 0;
	return 1;
}

static void add_flag(char *buf, size_t n, const char *flag, const char *val)
{
	size_t l = strlen(buf);
	snprintf(buf + l, n - l, "%s%s %s", l ? " " : "", flag, val);
}

/*
 * DISTROBOX'S CLOSING INSTRUCTION IS DROPPED, AND NOTHING ELSE. A successful
 * `distrobox create` ends with "Distrobox '<n>' successfully created.", "To
 * enter, run:" and a blank line on its stderr and `distrobox enter <n>` on its
 * stdout, and the command that enters a KDOS box is `kdos-box enter`, which
 * cmd_box_create prints. Its stdout goes to /dev/null, as every kb_run
 * child's does: it carries that command and a pull's image id. Its stderr is
 * read here with the terminal's colour sequences taken out (the reset that
 * closes distrobox's green `[ OK ]` opens the dropped line), and every other
 * line, an error above all, reaches stderr. "Creating '<n>' using image <i>"
 * ends in a tab and no newline until the container exists, so a partial line
 * that opens with "Creating '" is written as it arrives: held back, the
 * create would show nothing while it runs.
 */
static void strip_csi(char *s)
{
	char *w = s;

	for (char *r = s; *r; r++) {
		if (r[0] == '\033' && r[1] == '[') {
			r += 2;
			while (*r && (*r < 0x40 || *r > 0x7e))
				r++;
			if (!*r)
				break;
			continue;
		}
		*w++ = *r;
	}
	*w = '\0';
}

static int distrobox_tail(const char *line, const char *name, int *tail)
{
	char want[160];

	snprintf(want, sizeof(want), "Distrobox '%s' successfully created.",
		 name);
	if (!strcmp(line, want) || !strcmp(line, "To enter, run:")) {
		*tail = 1;
		return 1;
	}
	if (*tail && !*line)
		return 1;
	*tail = 0;
	return 0;
}

static int run_distrobox_create(const KbArgv *a, const char *name)
{
	char buf[4096];
	size_t len = 0;
	int fd[2], st, tail = 0, open_line = 0;
	pid_t pid;

	if (!kb_proc_verbose || pipe(fd) != 0)
		return kb_run(a);
	pid = fork();
	if (pid < 0) {
		close(fd[0]);
		close(fd[1]);
		return kb_run(a);
	}
	if (pid == 0) {
		int null = open("/dev/null", O_RDWR);

		if (null >= 0) {
			dup2(null, STDIN_FILENO);
			dup2(null, STDOUT_FILENO);
			close(null);
		}
		dup2(fd[1], STDERR_FILENO);
		close(fd[0]);
		close(fd[1]);
		kb_child_reset_signals();
		execvp(a->v[0], (char *const *)a->v);
		_exit(127);
	}
	close(fd[1]);
	for (;;) {
		ssize_t n = read(fd[0], buf + len, sizeof(buf) - 1 - len);
		char *nl;

		if (n < 0 && errno == EINTR)
			continue;
		if (n > 0)
			len += (size_t)n;
		buf[len] = '\0';
		while ((nl = memchr(buf, '\n', len))) {
			size_t used = (size_t)(nl - buf) + 1;

			*nl = '\0';
			strip_csi(buf);
			if (open_line || !distrobox_tail(buf, name, &tail))
				fprintf(stderr, "%s\n", buf);
			open_line = 0;
			len -= used;
			memmove(buf, buf + used, len);
			buf[len] = '\0';
		}
		/* A partial line: the end of the stream, a line too long for
		 * the buffer, or the "Creating" line that waits on the
		 * container. None of them is a tail line. */
		if (len && (n <= 0 || len == sizeof(buf) - 1 || open_line ||
			    !strncmp(buf, "Creating '", 10))) {
			strip_csi(buf);
			fputs(buf, stderr);
			fflush(stderr);
			open_line = 1;
			len = 0;
		}
		if (n <= 0)
			break;
	}
	if (open_line)
		fputc('\n', stderr);
	close(fd[0]);
	while (waitpid(pid, &st, 0) < 0)
		if (errno != EINTR)
			return -1;
	return WIFEXITED(st) ? WEXITSTATUS(st) : 128 + WTERMSIG(st);
}

int box_create(const Profile *p)
{
	KbArgv a = {0};
	char flags[MAX_LINE] = {0};

	if (!image_exists(p->image)) {
		kb_warn("image %s is not present", p->image);
		return 1;
	}

	kb_argv_add(&a, "distrobox");
	kb_argv_add(&a, "create");
	kb_argv_add(&a, "--name");
	kb_argv_add(&a, p->name);
	kb_argv_add(&a, "--image");
	kb_argv_add(&a, p->image);
	kb_argv_add(&a, "--yes");
	/* No menu entry of distrobox's: `kdos-box remove` is a `podman rm`,
	 * which leaves the entry distrobox would write naming a box that is
	 * gone, and a box's launchers are `kdos-box export`'s. */
	kb_argv_add(&a, "--no-entry");
	if (p->netns)
		kb_argv_add(&a, "--unshare-netns");
	if (p->ipc)
		kb_argv_add(&a, "--unshare-ipc");
	if (p->devsys)
		kb_argv_add(&a, "--unshare-devsys");
	if (p->process)
		kb_argv_add(&a, "--unshare-process");
	if (p->init)
		kb_argv_add(&a, "--init");
	if (p->privhome) {
		char *h = profile_home(p->name);
		kb_mkdir_p(h);
		kb_argv_add(&a, "--home");
		kb_argv_add(&a, h);
	}
	/* The host's cups socket, so every boxed app's own print dialog works
	 * (no portal Print backend exists or is needed). A volume is a
	 * CREATE-time property — it cannot be added to a live container — so a
	 * box created before cups was up keeps missing it until it is removed
	 * and created again; the matching CUPS_SERVER export in box_env()
	 * is per-launch and probes the socket again. */
	if (kb_path_exists("/run/cups/cups.sock")) {
		kb_argv_add(&a, "--volume");
		kb_argv_add(&a, "/run/cups:/run/cups");
	}
	/* KDOS's distribution defaults, read-only at the host's path, for the
	 * XDG_CONFIG_DIRS every launch sets (pack_box_create says why). */
	if (kb_is_dir(BOX_KDOS_XDG)) {
		kb_argv_add(&a, "--volume");
		kb_argv_add(&a, BOX_KDOS_XDG ":" BOX_KDOS_XDG ":ro");
	}
	/*
	 * THE RENDER NODES, FOR A BOX THAT HAS NO /dev OF THE HOST'S.
	 * `--unshare-devsys` is all of /dev or none of it, so a box created
	 * with private devices has no /dev/dri: no hardware GL, no VA-API and
	 * no Vulkan, whatever the profile's `render` key says and whatever the
	 * compositor offers. This binds back the card alone — which is what
	 * `gpu = yes` means and the only direction it can be enforced in.
	 */
	if (p->devsys && p->gpu && kb_path_exists("/dev/dri")) {
		kb_argv_add(&a, "--volume");
		kb_argv_add(&a, "/dev/dri:/dev/dri");
	}
	/*
	 * THE LIMITS, `network = none` AND THE PROFILE LABEL, which distrobox
	 * has no flags of its own for, go to podman through --additional-flags. distrobox-create
	 * `eval`s the command it builds, so a value is passed only when it is
	 * made of characters a shell reads as one plain word; anything else is
	 * refused by name rather than handed to that eval. `--unshare-netns`
	 * above is what keeps distrobox from adding `--network host`, so
	 * `--network none` here is the only network flag podman sees.
	 */
	if (p->memory[0]) {
		if (plain_word(p->memory))
			add_flag(flags, sizeof(flags), "--memory", p->memory);
		else
			kb_warn("%s: memory=%s is not a size — not applied",
				p->name, p->memory);
	}
	if (p->cpus[0]) {
		if (plain_word(p->cpus))
			add_flag(flags, sizeof(flags), "--cpus", p->cpus);
		else
			kb_warn("%s: cpus=%s is not a number — not applied",
				p->name, p->cpus);
	}
	if (p->pids > 0) {
		char n[16];
		snprintf(n, sizeof(n), "%d", p->pids);
		add_flag(flags, sizeof(flags), "--pids-limit", n);
	}
	if (p->netnone)
		add_flag(flags, sizeof(flags), "--network", "none");
	/* What the container is made from, for profile_print to compare. */
	{
		char digest[PROFILE_DIGEST_MAX], label[PROFILE_DIGEST_MAX + 32];

		profile_create_digest(p, digest, sizeof(digest));
		snprintf(label, sizeof(label), BOX_PROFILE_LABEL "=%s", digest);
		add_flag(flags, sizeof(flags), "--label", label);
	}
	kb_argv_add(&a, "--additional-flags");
	kb_argv_add(&a, flags);
	kb_argv_end(&a);
	return run_distrobox_create(&a, p->name);
}

/*
 * The box's init announces completion with `container_setup_done` on the
 * container's stdout, which podman keeps in its logs: distrobox-init for an
 * image: box, kdos-boxinit for a pack box and a store box. That marker is the
 * only honest readiness signal: distrobox-enter waits for it ONLY when it
 * started the container itself, so entering a container someone else started
 * (the login warmup) skips the wait and can exec into a half-built user.
 *
 * THIS START'S LINES ONLY, READ WHOLE. A box that is not running is not
 * ready. The log is asked `--since` the later of two times: the end of the
 * previous run (FinishedAt, which podman sets only when a run exits and
 * leaves alone on a start; zero before the first) and two seconds before
 * this start's StartedAt. StartedAt alone is too late a bound: podman takes
 * it after `crun start` returns, when the init is already running, and
 * conmon stamps each line as it reads it, so kdos-boxinit's marker, printed
 * within milliseconds of its exec, can carry an earlier stamp than StartedAt
 * and would be dropped. FinishedAt keeps an earlier run's marker from
 * answering for a restart whose init has not finished; the margin below
 * StartedAt does that after a reboot, where the last run never reached a
 * FinishedAt. The log is read unbounded, since a box stopped mid-setup and
 * started again holds two inits' output and passes 64 KiB before its marker,
 * and searched as bytes, since a NUL in a line would end a string search.
 * `--tail` is not used: podman's k8s-file tail reader gives up on the first
 * line it cannot parse, and the NULs an unclean shutdown leaves in the file
 * are such a line, where the forward reader skips the line and goes on.
 *
 * A marker in the output counts whatever the exit status. When `podman logs`
 * fails without one (a log driver it cannot read back), the answer is
 * /.containersetupdone, which distrobox-init creates once its first package
 * install is done and never removes: that proves an earlier init got that
 * far, not that this start's init has finished. kdos-boxinit creates no such
 * file, so a pack or store box whose log podman cannot read is never
 * reported ready.
 */
#define SETUP_SINCE_MARGIN 2

int box_setup_done(const char *box)
{
	KbArgv a = {0};
	KbBuf out = {0};
	char st[160] = "", run[8] = "", since[48];
	long long ss, fs;
	long sn, fn;
	struct tm tm;
	time_t t;
	int rc, done;

	kb_argv_add(&a, "podman");
	kb_argv_add(&a, "inspect");
	kb_argv_add(&a, "--type");
	kb_argv_add(&a, "container");
	kb_argv_add(&a, "--format");
	kb_argv_add(&a, "{{.State.Running}} "
			"{{.State.StartedAt.Unix}} {{.State.StartedAt.Nanosecond}} "
			"{{.State.FinishedAt.Unix}} {{.State.FinishedAt.Nanosecond}}");
	kb_argv_add(&a, box);
	kb_argv_end(&a);
	if (kb_run_capture(&a, st, sizeof(st)) != 0 ||
	    sscanf(st, "%7s %lld %ld %lld %ld", run, &ss, &sn, &fs, &fn) != 5 ||
	    strcmp(run, "true") || sn < 0 || sn > 999999999 ||
	    fn < 0 || fn > 999999999)
		return 0;
	ss -= SETUP_SINCE_MARGIN;
	if (fs > ss || (fs == ss && fn > sn)) {
		ss = fs;
		sn = fn;
	}
	t = (time_t)ss;
	if (!gmtime_r(&t, &tm))
		return 0;
	snprintf(since, sizeof(since), "%04d-%02d-%02dT%02d:%02d:%02d.%09ldZ",
		 tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
		 tm.tm_hour, tm.tm_min, tm.tm_sec, sn);

	memset(&a, 0, sizeof(a));
	kb_argv_add(&a, "podman");
	kb_argv_add(&a, "logs");
	kb_argv_add(&a, "--since");
	kb_argv_add(&a, since);
	kb_argv_add(&a, box);
	kb_argv_end(&a);
	rc = kb_run_capture_buf(&a, &out);
	done = out.p && memmem(out.p, out.n, "container_setup_done", 20) != NULL;
	kb_buf_free(&out);
	if (done || rc == 0)
		return done;

	memset(&a, 0, sizeof(a));
	kb_argv_add(&a, "podman");
	kb_argv_add(&a, "exec");
	kb_argv_add(&a, box);
	kb_argv_add(&a, "test");
	kb_argv_add(&a, "-e");
	kb_argv_add(&a, "/.containersetupdone");
	kb_argv_end(&a);
	return kb_run(&a) == 0;
}

/* Whether the box has EVER finished a setup, on any start: the marker
 * anywhere in its whole log, no --since, read unbounded and searched as
 * bytes for the reasons above. /.containersetupdone answers only when
 * `podman logs` fails without a marker: distrobox-init creates the file
 * after its first package install and prints the marker only after the
 * user setup, so a log that reads back without one is a first setup still
 * running, whatever the file says. It says nothing about this start:
 * box_setup_done answers that. A pack or store box whose log holds no
 * marker reads as never set up. */
int box_setup_ever(const char *box)
{
	KbArgv a = {0};
	KbBuf out = {0};
	int rc, done;

	kb_argv_add(&a, "podman");
	kb_argv_add(&a, "logs");
	kb_argv_add(&a, box);
	kb_argv_end(&a);
	rc = kb_run_capture_buf(&a, &out);
	done = out.p && memmem(out.p, out.n, "container_setup_done", 20) != NULL;
	kb_buf_free(&out);
	if (done || rc == 0)
		return done;

	memset(&a, 0, sizeof(a));
	kb_argv_add(&a, "podman");
	kb_argv_add(&a, "exec");
	kb_argv_add(&a, box);
	kb_argv_add(&a, "test");
	kb_argv_add(&a, "-e");
	kb_argv_add(&a, "/.containersetupdone");
	kb_argv_end(&a);
	return kb_run(&a) == 0;
}

static volatile sig_atomic_t wait_stopped;

static void on_wait_term(int sig)
{
	(void)sig;
	wait_stopped = 1;
}

/* Up to `seconds` for container_setup_done, holding this process's wait mark
 * (`<box>.wait.<pid>`) the whole time, so an explicit stop of the box ends the
 * wait (EVERY PROCESS OF A BOX). The stop's SIGTERM is caught while the wait
 * lasts, without SA_RESTART, so the sleep between polls ends on it and the
 * mark is removed before anything else: the stop counts a waiting launch as
 * alive while its mark names it. The SIGTERM disposition found on the way in
 * is put back on the way out and the signal handed to it: re-raised, so a
 * launch that leaves SIGTERM at its default dies of it as it would have
 * without the wait and never goes on into the box; a caller that ignores
 * SIGTERM around the wait (`kdos-box enter`, which says what happened and
 * exits 1, so its shell prints no "Terminated") is returned -2. 0 when the
 * box is ready, -2 when a SIGTERM ended the wait, -1 when the time ran out.
 *
 * box_wait_ready_note calls `note` once, inside the same wait and under the
 * same mark, when the box is still not ready `quiet` seconds of
 * CLOCK_MONOTONIC after the wait began, or at the last poll when that comes
 * sooner; never when the box is ready first or a SIGTERM ends the wait. The
 * wait and the grace are both clocked, not counted in polls, because each
 * poll runs podman twice besides its sleep: a count of polls would stretch
 * both well past the seconds named on a rootless podman. One
 * wait rather than two keeps the mark up throughout: between two, a stop
 * would find no mark, signal nothing, and leave the second wait polling a
 * stopped box until its time ran out. */
int box_wait_ready(const char *box, int seconds)
{
	return box_wait_ready_note(box, seconds, 0, NULL);
}

int box_wait_ready_note(const char *box, int seconds, int quiet,
			void (*note)(const char *box))
{
	struct sigaction sa, old;
	char mark[MAX_LINE], rec[32];
	struct timespec t0, now;
	long long ms;
	int have, noted = 0, rc = -1;

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = on_wait_term;
	sigemptyset(&sa.sa_mask);
	wait_stopped = 0;
	sigaction(SIGTERM, &sa, &old);
	have = wait_mark_path(box, (long)getpid(), mark, sizeof(mark)) == 0;
	if (have) {
		snprintf(rec, sizeof(rec), "%llu\n", proc_start((long)getpid()));
		have = kb_write_file(mark, rec) == 0;
	}
	clock_gettime(CLOCK_MONOTONIC, &t0);
	while (!wait_stopped) {
		clock_gettime(CLOCK_MONOTONIC, &now);
		ms = (now.tv_sec - t0.tv_sec) * 1000LL +
		     (now.tv_nsec - t0.tv_nsec) / 1000000;
		if (ms >= seconds * 1000LL)
			break;
		if (box_setup_done(box)) {
			rc = 0;
			break;
		}
		if (note && !noted && !wait_stopped &&
		    (ms >= quiet * 1000LL || ms + 200 >= seconds * 1000LL)) {
			noted = 1;
			note(box);
		}
		if (!wait_stopped)
			usleep(200000);
	}
	if (have)
		unlink(mark);
	if (wait_stopped)
		rc = -2;
	sigaction(SIGTERM, &old, NULL);
	if (rc == -2 && old.sa_handler != SIG_IGN)
		raise(SIGTERM);
	return rc;
}

int box_list(void)
{
	KbArgv a = {0};
	char *buf = kb_calloc(1, 1 << 16);
	char *line, *save;

	kb_argv_add(&a, "podman");
	kb_argv_add(&a, "ps");
	kb_argv_add(&a, "--all");
	kb_argv_add(&a, "--format");
	kb_argv_add(&a, "{{.Names}}\t{{.Image}}\t{{.State}}");
	kb_argv_end(&a);
	if (kb_run_capture(&a, buf, 1 << 16) != 0) {
		free(buf);
		kb_warn("podman is not usable — no boxes to list");
		return 1;
	}

	printf("%-16s %-34s %-10s %s\n", "BOX", "IMAGE", "STATE", "PROFILE");
	for (line = strtok_r(buf, "\n", &save); line;
	     line = strtok_r(NULL, "\n", &save)) {
		char *name = line;
		char *image = strchr(line, '\t');
		char *state;
		Profile p;
		char *pp;

		if (!image)
			continue;
		*image++ = '\0';
		state = strchr(image, '\t');
		if (!state)
			continue;
		*state++ = '\0';

		pp = profile_path(name);
		profile_load(&p, name);
		printf("%-16s %-34s %-10s %s\n", name, image, state,
		       kb_path_exists(pp) ? "custom" : "default");
		free(pp);
	}
	free(buf);
	return 0;
}
