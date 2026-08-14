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
	eq++;

	if (!strcmp(key, "image"))
		snprintf(p->image, sizeof(p->image), "%s", eq);
	else if (!strcmp(key, "network"))
		p->netns = truthy(eq);
	else if (!strcmp(key, "ipc"))
		p->ipc = truthy(eq);
	else if (!strcmp(key, "devices"))
		p->devsys = truthy(eq);
	else if (!strcmp(key, "processes"))
		p->process = truthy(eq);
	else if (!strcmp(key, "home"))
		p->privhome = truthy(eq);
	else if (!strcmp(key, "init"))
		p->init = truthy(eq);
	else
		return -1;
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

int profile_save(const Profile *p)
{
	char *path = profile_path(p->name);
	char buf[MAX_LINE];
	int rc;

	snprintf(buf, sizeof(buf),
		 "# KDOS appbox profile — applied when the box is CREATED.\n"
		 "# Namespaces cannot be re-flagged on a live container, so a\n"
		 "# change here takes effect after `kdos-appbox recreate %s`.\n"
		 "image=%s\n"
		 "network=%s\n"
		 "ipc=%s\n"
		 "devices=%s\n"
		 "processes=%s\n"
		 "home=%s\n"
		 "init=%s\n",
		 p->name, p->image,
		 p->netns ? "private" : "shared",
		 p->ipc ? "private" : "shared",
		 p->devsys ? "private" : "shared",
		 p->process ? "private" : "shared",
		 p->privhome ? "private" : "shared",
		 p->init ? "yes" : "no");
	rc = kb_write_file(path, buf);
	free(path);
	return rc;
}

void profile_print(const Profile *p)
{
	printf("box       = %s\n", p->name);
	printf("image     = %s\n", p->image);
	printf("network   = %s\n", p->netns ? "private" : "shared");
	printf("ipc       = %s\n", p->ipc ? "private" : "shared");
	printf("devices   = %s\n", p->devsys ? "private" : "shared");
	printf("processes = %s\n", p->process ? "private" : "shared");
	printf("home      = %s\n", p->privhome ? "private" : "shared");
	printf("init      = %s\n", p->init ? "yes" : "no");
	if (p->privhome) {
		char *h = profile_home(p->name);
		printf("home-path = %s\n", h);
		free(h);
	}
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
