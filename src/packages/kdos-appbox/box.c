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
	char *dir = xmalloc(MAX_LINE);
	snprintf(dir, MAX_LINE, "%s/.config/kdos/boxes", home_dir());
	mkdir_p(dir);
	snprintf(dir + strlen(dir), MAX_LINE - strlen(dir), "/%s.conf", box);
	return dir;
}

char *profile_home(const char *box)
{
	char *p = xmalloc(MAX_LINE);
	const char *data = getenv("XDG_DATA_HOME");
	if (data && *data)
		snprintf(p, MAX_LINE, "%s/kdos/boxes/%s", data, box);
	else
		snprintf(p, MAX_LINE, "%s/.local/share/kdos/boxes/%s",
			 home_dir(), box);
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
	found = read_file(path, buf, sizeof(buf)) >= 0;
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
	rc = write_file(path, buf);
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
	Argv a = {0};
	argv_add(&a, "podman");
	argv_add(&a, "image");
	argv_add(&a, "exists");
	argv_add(&a, image);
	argv_end(&a);
	return run_quiet(&a) == 0;
}

/*
 * Does the image carry the qgtk3 platform themes?
 *
 * QT_STYLE_OVERRIDE=Fusion only helps when something can supply Qt a palette;
 * with no platform theme Fusion falls back to Qt's built-in LIGHT palette,
 * which is worse than leaving the app alone. The Containerfile sets this label
 * in the same layer that installs qt{5,6}-gtk-platformtheme so the two cannot
 * drift. Cached for the boot: `podman image inspect` costs ~150ms against a
 * ~300ms warm launch, and the image cannot change without a reboot.
 */
int image_has_qt_gtk(const char *image)
{
	char *cache = path_join(runtime_dir(), "kdos-appbox.qtgtk");
	char buf[64] = {0};
	int yes;

	if (read_file(cache, buf, sizeof(buf)) < 0) {
		Argv a = {0};
		argv_add(&a, "podman");
		argv_add(&a, "image");
		argv_add(&a, "inspect");
		argv_add(&a, "--format");
		argv_add(&a, "{{index .Labels \"kdos.qt-gtk-theme\"}}");
		argv_add(&a, image);
		argv_end(&a);
		if (run_capture(&a, buf, sizeof(buf)) != 0)
			buf[0] = '\0';
		write_file(cache, buf);
	}
	yes = (buf[0] == '1');
	free(cache);
	return yes;
}

int box_exists(const char *box)
{
	Argv a = {0};
	argv_add(&a, "podman");
	argv_add(&a, "container");
	argv_add(&a, "exists");
	argv_add(&a, box);
	argv_end(&a);
	return run_quiet(&a) == 0;
}

int box_state(const char *box, char *buf, size_t n)
{
	Argv a = {0};
	argv_add(&a, "podman");
	argv_add(&a, "inspect");
	argv_add(&a, "--type");
	argv_add(&a, "container");
	argv_add(&a, "--format");
	argv_add(&a, "{{.State.Status}}");
	argv_add(&a, box);
	argv_end(&a);
	if (run_capture(&a, buf, n) != 0 || !buf[0])
		snprintf(buf, n, "absent");
	return 0;
}

int box_create(const Profile *p)
{
	Argv a = {0};
	char flags[MAX_LINE] = {0};

	if (!image_exists(p->image)) {
		warn("image %s is not present", p->image);
		return 1;
	}

	argv_add(&a, "distrobox");
	argv_add(&a, "create");
	argv_add(&a, "--name");
	argv_add(&a, p->name);
	argv_add(&a, "--image");
	argv_add(&a, p->image);
	argv_add(&a, "--yes");
	if (p->netns)
		argv_add(&a, "--unshare-netns");
	if (p->ipc)
		argv_add(&a, "--unshare-ipc");
	if (p->devsys)
		argv_add(&a, "--unshare-devsys");
	if (p->process)
		argv_add(&a, "--unshare-process");
	if (p->init)
		argv_add(&a, "--init");
	if (p->privhome) {
		char *h = profile_home(p->name);
		mkdir_p(h);
		argv_add(&a, "--home");
		argv_add(&a, h);
	}
	if (flags[0]) {
		argv_add(&a, "--additional-flags");
		argv_add(&a, flags);
	}
	argv_end(&a);
	return run_quiet(&a);
}

int box_remove(const char *box, int force)
{
	Argv a = {0};
	argv_add(&a, "distrobox");
	argv_add(&a, "rm");
	argv_add(&a, box);
	argv_add(&a, "--yes");
	if (force)
		argv_add(&a, "--force");
	argv_end(&a);
	return run_quiet(&a);
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
		die("the appbox image is not baked into this system — build the "
		    "ISO after 'make fetch-apps', or when online: kdos app <name>");

	lockpath = path_join(runtime_dir(), "kdos-appbox.create.lock");
	fd = lock_file(lockpath, 0);
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
	Argv a = {0};
	char *buf = xmalloc(1 << 16);
	int done;

	argv_add(&a, "podman");
	argv_add(&a, "logs");
	argv_add(&a, box);
	argv_end(&a);
	run_capture(&a, buf, 1 << 16);
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
	Argv a = {0};
	char *buf = xmalloc(1 << 16);
	char *line, *save;

	argv_add(&a, "podman");
	argv_add(&a, "ps");
	argv_add(&a, "--all");
	argv_add(&a, "--format");
	argv_add(&a, "{{.Names}}\t{{.Image}}\t{{.State}}");
	argv_end(&a);
	if (run_capture(&a, buf, 1 << 16) != 0) {
		free(buf);
		warn("podman is not usable — no boxes to list");
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
		       file_exists(pp) ? "custom" : "default");
		free(pp);
	}
	free(buf);
	return 0;
}
