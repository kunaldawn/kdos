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
 * The pack lane: talking to kdos-packd, and running a box out of packs
 * instead of out of an image.
 *
 * ONE BOX PER APPLICATION, and this is the decision the rest of the file
 * follows from. The alternative — one box composing every installed app pack —
 * hits two walls at once. Adding a pack to a running box is not a thing
 * overlayfs can do, so installing an application would mean restarting a
 * container that other applications are running in; and a hundred lowerdirs
 * do not fit in the 4096 bytes mount(2) gives its option string. Per app, a
 * stack is three or four layers, installing one application disturbs nothing
 * else, and the pages of a shared runtime are shared between boxes because
 * the packs are mounted once.
 *
 * THE COST, STATED: one conmon per RUNNING application rather than one for all
 * of them. Two megabytes each, and only for applications that are open.
 *
 * WHAT THE BOX IS: `podman create --rootfs <merged>` over the overlay
 * kdos-packd composed, with the flag set distrobox uses for the same job —
 * keep-id so $HOME's files belong to somebody inside as well as outside,
 * host ipc/net/pid unless the profile says otherwise, /tmp and /run/user
 * shared because the session bus and the Wayland socket live there.
 *
 * KEEP-ID, AND PACKS THEREFORE PRESENT AS THE USER RATHER THAN AS ROOT. Both
 * routes were measured over a real three-pack stack:
 *
 *   no keep-id      container uid 0 IS host uid 1000, so /usr reads root:root
 *                   and $HOME's files read as root — tidy, and the box runs
 *                   everything as root
 *   --userns keep-id  container uid 1000 is host uid 1000, so /usr reads as
 *                   the user and the box runs as a real non-root account
 *
 * The second is taken because applications refuse to run as root and there is
 * no way to argue with them, which is also why distrobox chose it. The
 * ownership is cosmetic — the mode bits decide access, and packs are mounted
 * nosuid so ownership grants nothing either way.
 *
 * Building packs at uid 0 is not the third option it looks like: rootless
 * podman cannot map host root at all, so those files come back as the overflow
 * uid. Building them at the machine's first subuid WOULD present as root and
 * would tie every pack to one machine's subuid allocation.
 */

#include "kdos-appbox.h"

#include <errno.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#define PACKD_SOCKET "/run/kdos-packd.sock"

static const char *packd_socket(void)
{
	const char *p = getenv("KDOS_PACKD_SOCKET");
	return p && *p ? p : PACKD_SOCKET;
}

const char *pack_store(void)
{
	const char *p = getenv("KDOS_PACK_STORE");
	return p && *p ? p : "/var/lib/kdos/packs";
}

/*
 * One request, one reply, one connection — the protocol kdos-powerd,
 * kdos-mountd and kdos-energyd all use. Returns 0 when the daemon answered
 * `ok`, 1 when it answered `err`, and -1 when there is no daemon there, and
 * the caller must tell those apart: "the pack lane is not running" and "this
 * pack cannot be mounted" send a person to different places.
 */
int packd_ask(const char *req, char *out, size_t n)
{
	int fd;
	struct sockaddr_un addr = { .sun_family = AF_UNIX };
	KbBuf b = {0};
	char buf[4096];
	ssize_t got;
	int rc;

	if (out && n)
		out[0] = 0;
	fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (fd < 0)
		return -1;
	snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", packd_socket());
	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		close(fd);
		return -1;
	}
	dprintf(fd, "%s\n", req);
	while ((got = read(fd, buf, sizeof(buf))) > 0)
		kb_buf_add(&b, buf, (size_t)got);
	close(fd);
	if (!b.p) {
		kb_buf_free(&b);
		return -1;
	}

	/* `ok` and `err` are the LAST line; a listing comes before it. */
	{
		char *last = b.p;
		char *p = b.p;
		size_t i;
		for (i = 0; i < b.n; i++)
			if (b.p[i] == '\n' && i + 1 < b.n)
				last = b.p + i + 1;
		(void)p;
		if (!strncmp(last, "ok", 2)) {
			rc = 0;
			if (out && n) {
				const char *v = last[2] == ' ' ? last + 3 : "";
				kb_strlcpy(out, v, n);
				out[strcspn(out, "\r\n")] = 0;
			}
			/* A listing reply: hand back everything BEFORE the ok,
			 * because that is the answer and `ok` is only the
			 * punctuation. */
			if (out && n && last != b.p && !out[0]) {
				size_t pre = (size_t)(last - b.p);
				if (pre >= n)
					pre = n - 1;
				memcpy(out, b.p, pre);
				out[pre] = 0;
			}
		} else if (!strncmp(last, "err", 3)) {
			rc = 1;
			if (out && n) {
				kb_strlcpy(out, last[3] == ' ' ? last + 4 : last, n);
				out[strcspn(out, "\r\n")] = 0;
			}
		} else {
			rc = -1;
			if (out && n)
				kb_strlcpy(out, b.p, n);
		}
	}
	kb_buf_free(&b);
	return rc;
}

/*
 * THE DUAL-MODE SEAM, and it has an end: W7-5 deletes it once the packs are
 * what ships. The pack lane is in use when the store has a `base` pack and the
 * daemon answers; anything else is the monolithic image, unchanged.
 */
int pack_mode(void)
{
	static int known = -1;
	char buf[64];

	if (known >= 0)
		return known;
	if (getenv("KDOS_FORCE_IMAGE")) {
		known = 0;
		return known;
	}
	{
		char *p = kb_path_join(pack_store(), "base.kpack");
		int have = kb_path_exists(p);
		free(p);
		if (!have) {
			known = 0;
			return known;
		}
	}
	known = packd_ask("ping", buf, sizeof(buf)) == 0;
	return known;
}

/* Every pack the daemon knows, one `id\tversion\tkind\tstate\tsize\torigin`
 * line each. */
char *pack_list(void)
{
	char *buf = kb_calloc(1, 1 << 16);

	if (packd_ask("list", buf, 1 << 16) != 0) {
		free(buf);
		return NULL;
	}
	return buf;
}

/*
 * The pack that provides an application, by its shim name. The `command =`
 * keys are what genlaunchers wrote the shims from, so this is the same table
 * read from the other end.
 */
int pack_of_command(const char *cmd, char *id, size_t n)
{
	char *list = pack_list();
	char *line, *save;
	int found = 0;

	if (!list)
		return -1;
	for (line = strtok_r(list, "\n", &save); line && !found;
	     line = strtok_r(NULL, "\n", &save)) {
		char *tab = strchr(line, '\t');
		char info[8192];
		char req[256];
		char *l2, *s2;

		if (!tab)
			continue;
		*tab = 0;
		snprintf(req, sizeof(req), "info %s", line);
		if (packd_ask(req, info, sizeof(info)) != 0)
			continue;
		for (l2 = strtok_r(info, "\n", &s2); l2;
		     l2 = strtok_r(NULL, "\n", &s2)) {
			if (strncmp(l2, "command", 7))
				continue;
			char *eq = strchr(l2, '=');
			if (!eq)
				continue;
			eq++;
			while (*eq == ' ')
				eq++;
			if (!strcmp(eq, cmd)) {
				kb_strlcpy(id, line, n);
				found = 1;
				break;
			}
		}
	}
	free(list);
	return found ? 0 : -1;
}

/*
 * Every `env =` line the pack's own stack declares, walked from the app
 * outwards along `requires`.
 *
 * THE NEAREST PACK WINS, which is what makes an override possible: an
 * application that needs a different platform theme from the runtime under it
 * says so in its own metadata and is not argued with. Collected by NAME, so a
 * value from further out is dropped rather than appended — two assignments to
 * one variable in an exec environment is a coin toss.
 *
 * This replaces the image-label question for pack boxes, and it has to: a pack
 * box is `podman --rootfs`, so `podman image inspect` has no image to inspect
 * and answers no to every label.
 */
int pack_env(const char *id, char out[][256], int max)
{
	char todo[8][128];
	int ntodo = 0, n = 0, i;

	kb_strlcpy(todo[ntodo++], id, sizeof(todo[0]));
	for (i = 0; i < ntodo && i < 8; i++) {
		char info[8192], req[256];
		char *line, *save;

		/* The precision is the element bound: with a variable index
		 * the compiler reads todo[i] as the whole 2-D array. */
		snprintf(req, sizeof(req), "info %.*s",
			 (int)sizeof(todo[0]) - 1, todo[i]);
		if (packd_ask(req, info, sizeof(info)) != 0)
			continue;
		for (line = strtok_r(info, "\n", &save); line;
		     line = strtok_r(NULL, "\n", &save)) {
			char *eq = strchr(line, '=');
			const char *val;

			if (!eq)
				continue;
			val = eq + 1;
			while (*val == ' ')
				val++;
			if (!strncmp(line, "env ", 4) || !strncmp(line, "env=", 4)) {
				const char *name_end = strchr(val, '=');
				size_t len;
				int dup = 0;

				if (!name_end)
					continue;
				len = (size_t)(name_end - val) + 1;
				for (int k = 0; k < n; k++)
					if (!strncmp(out[k], val, len)) {
						dup = 1;
						break;
					}
				if (!dup && n < max)
					kb_strlcpy(out[n++], val, 256);
			} else if (!strncmp(line, "requires", 8) && ntodo < 8) {
				char name[128];
				size_t j = 0;

				while (val[j] && val[j] != ' ' && j + 1 < sizeof(name)) {
					name[j] = val[j];
					j++;
				}
				name[j] = '\0';
				if (name[0])
					kb_strlcpy(todo[ntodo++], name,
						   sizeof(todo[0]));
			}
		}
	}
	return n;
}

/* Ask the daemon for a root filesystem. The client names an id; the daemon
 * decides the stack, the mount points and where the upper goes. */
int pack_compose(const char *box, const char *id, char *merged, size_t n)
{
	char req[512];

	snprintf(req, sizeof(req), "compose %s %s", box, id);
	return packd_ask(req, merged, n);
}

int pack_decompose(const char *box)
{
	char req[256], msg[512];

	snprintf(req, sizeof(req), "decompose %s", box);
	return packd_ask(req, msg, sizeof(msg));
}

/*
 * `podman create --rootfs`, with the flags distrobox passes for the same job.
 * Each group is here because something breaks without it:
 *
 *   keep-id            $HOME is the host's and its files are uid 1000; without
 *                      it the container user is a subuid and cannot read them
 *   host ipc/net/pid   the session bus, the Wayland socket and pipewire all
 *                      live in /run/user, and a private netns would cut the
 *                      box off from a portal it reaches over that socket
 *   /tmp and /run/user shared, because that is where the bus and the sockets
 *                      are — and /tmp must be 1777 on the host or every GTK
 *                      app fails to make its lock file
 *   /dev and /sys      a GPU, a camera and an audio device are all in there
 *   label/apparmor off the host runs neither, and a profile that is not loaded
 *                      is a denial rather than a policy
 *   tmpfs on /run      a pack is read-only, and /run must be writable
 */
int pack_box_create(const Profile *p, const char *merged)
{
	KbArgv a = {0};
	struct passwd *pw = getpwuid(getuid());
	char hostname[128] = "kdos";
	char vol[MAX_LINE];
	const char *home = kb_home_dir();

	kb_argv_add(&a, "podman");
	kb_argv_add(&a, "create");
	kb_argv_add(&a, "--name");
	kb_argv_add(&a, p->name);
	kb_argv_add(&a, "--rootfs");
	kb_argv_add(&a, merged);

	gethostname(hostname, sizeof(hostname) - 1);
	kb_argv_addf(&a, "--hostname=%s.%s", p->name, hostname);
	kb_argv_add(&a, "--label");
	kb_argv_add(&a, "manager=kdos-box");
	kb_argv_add(&a, "--security-opt");
	kb_argv_add(&a, "label=disable");
	kb_argv_add(&a, "--security-opt");
	kb_argv_add(&a, "apparmor=unconfined");
	kb_argv_add(&a, "--user");
	kb_argv_add(&a, "root:root");
	kb_argv_add(&a, "--userns");
	kb_argv_add(&a, "keep-id");
	kb_argv_add(&a, "--annotation");
	kb_argv_add(&a, "run.oci.keep_original_groups=1");
	kb_argv_add(&a, "--ulimit");
	kb_argv_add(&a, "host");

	/* The profile's namespaces, the 1:1 mapping the Profile struct exists
	 * to keep: a key that cannot be enforced is not offered, and each of
	 * these is exactly one podman flag. */
	if (!p->netns) {
		kb_argv_add(&a, "--network");
		kb_argv_add(&a, "host");
	}
	if (!p->ipc) {
		kb_argv_add(&a, "--ipc");
		kb_argv_add(&a, "host");
	}
	if (!p->process) {
		kb_argv_add(&a, "--pid");
		kb_argv_add(&a, "host");
	}
	if (p->pids > 0)
		kb_argv_addf(&a, "--pids-limit=%d", p->pids);
	else
		kb_argv_add(&a, "--pids-limit=-1");
	if (p->memory[0]) {
		kb_argv_add(&a, "--memory");
		kb_argv_add(&a, p->memory);
	}
	if (p->cpus[0]) {
		kb_argv_add(&a, "--cpus");
		kb_argv_add(&a, p->cpus);
	}

	kb_argv_add(&a, "--mount");
	kb_argv_add(&a, "type=tmpfs,destination=/run");
	kb_argv_add(&a, "--mount");
	kb_argv_add(&a, "type=tmpfs,destination=/run/lock");

	snprintf(vol, sizeof(vol), "/tmp:/tmp:rslave");
	kb_argv_add(&a, "--volume");
	kb_argv_add(&a, vol);

	if (p->privhome) {
		char *h = profile_home(p->name);
		kb_mkdir_p(h);
		snprintf(vol, sizeof(vol), "%s:%s:rslave", h, h);
		free(h);
	} else {
		snprintf(vol, sizeof(vol), "%s:%s:rslave", home, home);
	}
	kb_argv_add(&a, "--volume");
	kb_argv_add(&a, vol);

	if (!p->devsys) {
		kb_argv_add(&a, "--volume");
		kb_argv_add(&a, "/dev:/dev:rslave");
		kb_argv_add(&a, "--volume");
		kb_argv_add(&a, "/sys:/sys:rslave");
	}
	snprintf(vol, sizeof(vol), "/run/user/%u:/run/user/%u:rslave",
		 (unsigned)getuid(), (unsigned)getuid());
	kb_argv_add(&a, "--volume");
	kb_argv_add(&a, vol);
	kb_argv_add(&a, "--volume");
	kb_argv_add(&a, "/dev/shm:/dev/shm");
	/* The host's cups socket, so a boxed app's own print dialog works.
	 * A volume is a CREATE-time property and cannot be added later. */
	if (kb_path_exists("/run/cups/cups.sock")) {
		kb_argv_add(&a, "--volume");
		kb_argv_add(&a, "/run/cups:/run/cups");
	}

	/*
	 * kdos-boxinit is STATIC and is bind-mounted in from the host: the
	 * box's /usr is Debian's, and a musl-linked host binary would look for
	 * its loader there and not find it.
	 */
	kb_argv_add(&a, "--volume");
	kb_argv_add(&a, "/usr/libexec/kdos/kdos-boxinit:/usr/libexec/kdos-boxinit:ro");

	kb_argv_addf(&a, "--env=KDOS_BOX=%s", p->name);
	kb_argv_addf(&a, "--env=KDOS_BOX_USER=%s", pw ? pw->pw_name : "kdos");
	kb_argv_addf(&a, "--env=KDOS_BOX_UID=%u", (unsigned)getuid());
	kb_argv_addf(&a, "--env=KDOS_BOX_GID=%u", (unsigned)getgid());
	kb_argv_addf(&a, "--env=KDOS_BOX_HOME=%s", home);
	kb_argv_add(&a, "--env=container=podman");

	kb_argv_add(&a, "--entrypoint");
	kb_argv_add(&a, "/usr/libexec/kdos-boxinit");
	kb_argv_end(&a);
	return kb_run(&a);
}

/*
 * Create the box if it is missing, serialized against the login warmup exactly
 * as the image lane is: two creates racing produce one failure and one
 * container nobody expected.
 */
int pack_box_ensure(const char *box, const char *id)
{
	Profile p;
	char merged[MAX_LINE], msg[512];
	char *lockpath;
	int fd, rc;

	if (box_exists(box))
		return 0;

	profile_load(&p, box);
	lockpath = kb_path_join(kb_runtime_dir(), "kdos-appbox.create.lock");
	fd = kb_lock_file(lockpath, 0);
	free(lockpath);
	if (box_exists(box)) {		/* somebody else won the race */
		if (fd >= 0)
			close(fd);
		return 0;
	}

	rc = pack_compose(box, id, merged, sizeof(merged));
	if (rc != 0) {
		if (fd >= 0)
			close(fd);
		if (rc < 0)
			kb_warn("kdos-packd is not running — no pack can be mounted");
		else
			kb_warn("%s: %s", id, merged);
		return 1;
	}
	tracef("composed %s", merged);

	fprintf(stderr, "==> First launch: composing '%s' from packs...\n", box);
	rc = pack_box_create(&p, merged);
	if (rc != 0)
		pack_decompose(box);
	if (fd >= 0)
		close(fd);
	(void)msg;
	return rc;
}

/*
 * Start the box if it is not running, then wait for kdos-boxinit's marker.
 * The wait is the same contract the image lane keeps — `container_setup_done`
 * in podman's logs — so `box_wait_ready()` is shared rather than reimplemented.
 */
int pack_box_start(const char *box)
{
	KbArgv a = {0};
	char state[64];

	box_state(box, state, sizeof(state));
	if (!strcmp(state, "running"))
		return 0;
	kb_argv_add(&a, "podman");
	kb_argv_add(&a, "start");
	kb_argv_add(&a, box);
	kb_argv_end(&a);
	if (kb_run(&a) != 0)
		return 1;
	return 0;
}
