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
 * A profile is a handful of booleans in ~/.config/kdos/boxes/<name>.conf, and
 * every one of them maps onto a distrobox flag that the kernel actually
 * enforces. KDOS deliberately does not offer confinement it cannot deliver:
 * there is no "no filesystem access" switch, because distrobox shares the
 * host root at /run/host by design and pretending otherwise would be a lie
 * printed in a settings dialog.
 *
 * A profile is applied at CREATE time. Namespaces cannot be re-flagged on a
 * live container, so changing one marks the box as needing a recreate rather
 * than silently doing nothing — see cmd_security() in main.c.
 *
 * THE PROTOCOL HALF IS NOT THIS FILE'S ANY MORE. Before the labwc fork a
 * `wayland.*` key here could grant an individual global back to one box; the
 * fork's filter (allow_for_sandbox() in kdos-comp's server.c) is a fixed
 * allowlist, so a client is sandboxed or it is not — screencopy, data-control,
 * input-method and layer-shell are denied to every tagged client and no
 * profile key changes that. Nothing here writes or reads a `wayland.*` key,
 * which is the honest state: KDOS does not offer confinement it cannot
 * enforce, and it does not offer a knob that enforces nothing either. What
 * this file still owns is the namespace half — the keys above, applied at
 * create time.
 *
 * The path is resolved from $HOME/.config and NOT from $XDG_CONFIG_HOME, and
 * kdos-comp deliberately copies that rather than doing the more correct thing,
 * because the two resolving it differently is the one failure this design must
 * not have.
 */

#include "kdos-appbox.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void profile_defaults(Profile *p, const char *box)
{
	memset(p, 0, sizeof(*p));
	snprintf(p->name, sizeof(p->name), "%s", box);
	snprintf(p->image, sizeof(p->image), "%s", DEFAULT_IMAGE);
	/* Everything shared: exactly what a plain `distrobox create` does, so a
	 * box with no profile file behaves identically to one with a default. */
	p->persist = PERSIST_PERSISTENT;
	p->wayland = 1;
	p->audio = 1;
	p->gpu = 1;
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
	else if (!strcmp(key, "persistence")) {
		if (!strcmp(eq, "ephemeral"))
			p->persist = PERSIST_EPHEMERAL;
		else if (!strcmp(eq, "frozen"))
			p->persist = PERSIST_FROZEN;
		else
			p->persist = PERSIST_PERSISTENT;
	} else if (!strcmp(key, "network")) {
		/* host | private | none, and the legacy shared/private the
		 * image lane already wrote. `none` is private PLUS no
		 * interface at all, which is a different thing from a
		 * namespace of one's own with a bridge in it. */
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
	else if (!strcmp(key, "export"))
		p->autoexport = !strcmp(eq, "auto");
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
		 * REPORTED BY NAME, never ignored. The promise comp.conf's
		 * reload already makes, for the same reason: a typo that
		 * produces silence is indistinguishable from a setting that
		 * does nothing.
		 */
		if (p->nunknown < 8)
			snprintf(p->unknown[p->nunknown++], 64, "%s", key);
		return -1;
	}
	return 0;
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
 * A CONTAINER IN `stopping` IS NOT STARTABLE, AND PODMAN SAYS SO IN A WAY
 * THAT NAMES NOTHING USEFUL: "must be in Created or Stopped state to be
 * started: container state improper". `podman stop` sends SIGTERM and waits
 * out its timeout, and kdos-boxinit stays alive reaping, so a stop followed
 * promptly by a start lands exactly here — as does a hung app holding D-state
 * I/O, which can wedge a box in `stopping` for good.
 *
 * Wait it out, then force the way back. A box is stateless — its packs are
 * mounted read-only and its writable upper is on disk — so nothing is lost by
 * recreating the container over the same stack.
 */
int box_unstick(const char *box, char *state, size_t n)
{
	int i;

	box_state(box, state, n);
	if (strcmp(state, "stopping"))
		return 0;

	for (i = 0; i < 15 && !strcmp(state, "stopping"); i++) {
		sleep(1);
		box_state(box, state, n);
	}
	if (strcmp(state, "stopping"))
		return 1;

	{
		KbArgv k = {0};
		kb_argv_add(&k, "podman");
		kb_argv_add(&k, "kill");
		kb_argv_add(&k, box);
		kb_argv_end(&k);
		kb_run(&k);
	}
	sleep(2);
	box_state(box, state, n);
	if (strcmp(state, "stopping"))
		return 2;

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

int profile_save(const Profile *p)
{
	char *path = profile_path(p->name);
	char buf[MAX_LINE];
	int rc;

	snprintf(buf, sizeof(buf),
		 "# KDOS box profile — applied when the box is CREATED.\n"
		 "# Namespaces and volumes cannot be re-flagged on a live\n"
		 "# container, so a change here takes effect after\n"
		 "# `kdos-box recreate %s`.\n"
		 "base=%s\n"
		 "image=%s\n"
		 "accent=%s\n"
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
		 "export=%s\n"
		 "memory=%s\n"
		 "cpus=%s\n"
		 "pids=%d\n"
		 "autostop=%ds\n",
		 p->name, p->base, p->image,
		 p->accent[0] ? p->accent : "session",
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
		 p->autoexport ? "auto" : "manual",
		 p->memory, p->cpus, p->pids, p->autostop_s);
	/* ATOMIC: a profile that comes back empty has lost `base`, and a box
	 * whose base is unknown cannot be composed or started again. */
	rc = kb_write_file_atomic(path, buf);
	free(path);
	return rc;
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
	else
		printf("image       = %s\n", p->image);
	printf("accent      = %s\n", p->accent[0] ? p->accent : "the session's");
	printf("persistence = %-11s %s\n", persist_name(p->persist),
	       p->persist == PERSIST_FROZEN	? "(writes discarded)"
	       : p->persist == PERSIST_EPHEMERAL ? "(upper on tmpfs)"
						 : "(upper on disk)");
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
	printf("wayland     = %-11s %s\n", p->wayland ? "yes" : "no",
	       p->wayland ? "tagged through kdos-boxsock"
			  : "no display socket reaches it");
	printf("memory      = %-11s %s\n", p->memory[0] ? p->memory : "unlimited",
	       p->memory[0] ? "--memory" : "");
	printf("cpus        = %-11s %s\n", p->cpus[0] ? p->cpus : "all",
	       p->cpus[0] ? "--cpus" : "");
	if (p->pids)
		printf("pids        = %-11d --pids-limit\n", p->pids);
	if (p->autostop_s)
		printf("autostop    = %-11d seconds idle, enforced by `kdos-box gc`\n",
		       p->autostop_s);
	printf("export      = %-11s %s\n", p->autoexport ? "auto" : "manual",
	       p->autoexport ? "its apps become host launchers"
			     : "`kdos-box export` is how an app gets a launcher");
	if (p->privhome) {
		char *h = profile_home(p->name);
		printf("home-path   = %s\n", h);
		free(h);
	}
	/*
	 * `audio` and `gpu` are NOT printed as enforced, because they are not.
	 * Both ride on /dev and /run/user being shared, which is the `devices`
	 * key — there is no podman flag that grants a box a speaker and denies
	 * it a camera. Saying so is the rule; a key that reported "yes" here
	 * while changing nothing would be exactly the lie this format avoids.
	 */
	if (!p->devsys && (!p->audio || !p->gpu))
		printf("            ! audio=%s gpu=%s cannot be enforced separately"
		       " — both follow `devices`\n",
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
	 * box created before cups was up keeps missing it until
	 * `kdos-appbox recreate`; the matching CUPS_SERVER export in box_env()
	 * is per-launch and probes the socket again. */
	if (kb_path_exists("/run/cups/cups.sock")) {
		kb_argv_add(&a, "--volume");
		kb_argv_add(&a, "/run/cups:/run/cups");
	}
	if (flags[0]) {
		kb_argv_add(&a, "--additional-flags");
		kb_argv_add(&a, flags);
	}
	kb_argv_end(&a);
	return kb_run(&a);
}

int box_remove(const char *box, int force)
{
	KbArgv a = {0};
	kb_argv_add(&a, "distrobox");
	kb_argv_add(&a, "rm");
	kb_argv_add(&a, box);
	kb_argv_add(&a, "--yes");
	if (force)
		kb_argv_add(&a, "--force");
	kb_argv_end(&a);
	return kb_run(&a);
}

/*
 * Create the box if it is missing, serialized against the login-time warmup so
 * two creates can never race.
 */
int box_ensure(const char *box)
{
	Profile p;
	char *lockpath;
	int fd, rc = 0;

	if (box_exists(box))
		return 0;

	profile_load(&p, box);
	if (!image_exists(p.image))
		kb_die("the appbox image is not baked into this system — build the "
		    "ISO after 'make fetch-apps', or when online: kdos app <name>");

	lockpath = kb_path_join(kb_runtime_dir(), "kdos-appbox.create.lock");
	fd = kb_lock_file(lockpath, 0);
	free(lockpath);
	if (box_exists(box)) {          /* someone else won the race */
		if (fd >= 0)
			close(fd);
		return 0;
	}
	fprintf(stderr, "==> First launch: creating distrobox '%s' from the "
			"baked image...\n", box);
	rc = box_create(&p);
	if (fd >= 0)
		close(fd);
	return rc;
}

/*
 * distrobox-init announces completion with `container_setup_done` on the
 * container's stdout, which podman keeps in its logs. That marker is the only
 * honest readiness signal: distrobox-enter waits for it ONLY when it started
 * the container itself, so entering a container someone else started (the
 * login warmup) skips the wait and can exec into a half-built user.
 */
int box_setup_done(const char *box)
{
	KbArgv a = {0};
	char *buf = kb_calloc(1, 1 << 16);
	int done;

	kb_argv_add(&a, "podman");
	kb_argv_add(&a, "logs");
	kb_argv_add(&a, box);
	kb_argv_end(&a);
	kb_run_capture(&a, buf, 1 << 16);
	done = strstr(buf, "container_setup_done") != NULL;
	free(buf);
	return done;
}

int box_wait_ready(const char *box, int seconds)
{
	int i;
	for (i = 0; i < seconds * 5; i++) {
		if (box_setup_done(box))
			return 0;
		usleep(200000);
	}
	return -1;
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
