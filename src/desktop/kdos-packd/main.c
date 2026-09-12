/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-packd — the sixth root daemon
 *
 *     $ printf 'list\n' | nc -U /run/kdos-packd.sock
 *     base       1.0    base     mounted    620.0M  store
 *     rt-gtk     1.0    runtime  mounted    410.0M  store
 *     app.gimp   3.0.4  app      installed   92.0M  store
 *     app.krita  5.2.6  app      available  180.0M  medium
 *
 * kdos-powerd's shape exactly, for the fifth time: foreground under `ksvc`,
 * one socket in /run, gated by SO_PEERCRED to root and `wheel`, one line per
 * connection. It links libkbase, libksig and libkpack — every library it
 * links is code running as root, which is why libkpack was written to link
 * nothing else.
 *
 * THREE RULES, and each is a way a mounting daemon usually goes wrong:
 *
 *   - THE CLIENT NEVER NAMES A PATH. Every verb takes an ID out of a list this
 *     daemon itself published; `install` takes a FILENAME in a staging
 *     directory the daemon owns. kdos-mountd established this and the reason
 *     has not changed: a daemon reachable from `wheel` that takes a path is
 *     `mount /dev/sda2 /etc` from any shell.
 *   - VERIFICATION HAPPENS WHERE THE MOUNT HAPPENS. A client that verifies and
 *     then asks a daemon to mount has verified nothing.
 *   - A PACK IS READ-ONLY AND IS NEVER MODIFIED IN PLACE. Every write a box
 *     performs lands in its own upper directory; an update replaces the FILE.
 *     That is what makes rollback a rename and verification meaningful after
 *     installation rather than only before it.
 *
 * The mode on the socket is 0666 and the gate is the credential, which is
 * kdos-powerd's rule: a mode that LOOKED like the authorisation is a mode
 * somebody eventually loosens.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE	/* struct ucred */
#endif

#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include "kbase.h"
#include "ksig.h"
#include "packd.h"

int kd_fixture;
int kd_fixture_trust;
static int quiet;

/*
 * The format is GUARDED, as it is in libkbase's kb_warn. A null format is
 * undefined in vfprintf, and the sanitiser build's interprocedural pass cannot
 * prove one non-null across a whole program compiled in a single line — so
 * without the guard `-Wformat-overflow` refuses to build at all.
 */
void kd_log(const char *fmt, ...)
{
	va_list ap;

	if (quiet)
		return;
	fputs("kdos-packd: ", stderr);
	va_start(ap, fmt);
	if (fmt)
		vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
}

static const char *sock_path(void)
{
	const char *p = getenv("KDOS_PACKD_SOCKET");
	return p && *p ? p : KD_SOCKET;
}

static int uid_allowed(uid_t uid)
{
	struct passwd *pw;

	if (uid == 0)
		return 1;
	pw = getpwuid(uid);
	if (!pw)
		return 0;
	return kb_user_in_group(pw->pw_name, pw->pw_gid, KD_GROUP);
}

/* ── the protocol ──────────────────────────────────────────────────────── */

static const char *state_of(const KdPack *p)
{
	if (p->mnt[0])
		return "mounted";
	return p->origin == KD_ORIGIN_STORE ? "installed" : "available";
}

static void reply_list(int c)
{
	for (int i = 0; i < kd_npack; i++) {
		const KdPack *p = &kd_pack[i];
		dprintf(c, "%s\t%s\t%s\t%s\t%llu\t%s\n", p->meta.id,
			p->meta.version, kpk_kind_name(p->meta.kind), state_of(p),
			p->size, p->origin == KD_ORIGIN_STORE ? "store" : "medium");
	}
	(void)!write(c, "ok\n", 3);
}

static void reply_status(int c)
{
	int mounted = 0, installed = 0, available = 0;

	for (int i = 0; i < kd_npack; i++) {
		if (kd_pack[i].mnt[0])
			mounted++;
		if (kd_pack[i].origin == KD_ORIGIN_STORE)
			installed++;
		else
			available++;
	}
	dprintf(c, "store\t%s\n", kd_store_dir());
	dprintf(c, "medium\t%s\n", kd_medium_dir());
	/* Published rather than reconstructed by the client: a downloader that
	 * derived this path itself would be a second definition of where an
	 * unprivileged write is allowed. */
	dprintf(c, "staging\t%s\n", kd_staging_dir());
	dprintf(c, "retain\t%d\n", kd_retain());
	dprintf(c, "route\t%s\n", kd_route_name());
	dprintf(c, "packs\t%d installed, %d available, %d mounted\n", installed,
		available, mounted);
	dprintf(c, "boxes\t%d composed\n", kd_nbox);
	for (int i = 0; i < kd_nbox; i++)
		dprintf(c, "box\t%s\t%s\t%s\n", kd_box[i].name, kd_box[i].merged,
			kd_box[i].ephemeral ? "ephemeral" : "persistent");
	(void)!write(c, "ok\n", 3);
}

static void reply_info(int c, const char *id)
{
	int i = kd_find(id);
	size_t len = 0;
	char *text;

	if (i < 0) {
		dprintf(c, "err no pack %s\n", id);
		return;
	}
	text = kpk_meta_render(&kd_pack[i].meta, &len);
	(void)!write(c, text, len);
	free(text);
	dprintf(c, "state       = %s\n", state_of(&kd_pack[i]));
	dprintf(c, "origin      = %s\n",
		kd_pack[i].origin == KD_ORIGIN_STORE ? "store" : "medium");
	if (kd_pack[i].mnt[0])
		dprintf(c, "mountpoint  = %s\n", kd_pack[i].mnt);
	(void)!write(c, "ok\n", 3);
}

/*
 * One word plus at most an id. Everything past the verb is split on spaces and
 * every token is checked against the id rule BEFORE it means anything —
 * an id that is not in the daemon's own list is `err` and nothing else.
 */
static void handle(int c, char *line, uid_t uid)
{
	char msg[512] = "";
	char *tok[KD_STACK + 2];
	int ntok = 0;
	char *save = NULL, *w;

	for (w = strtok_r(line, " \t", &save); w && ntok < KD_STACK + 2;
	     w = strtok_r(NULL, " \t", &save))
		tok[ntok++] = w;
	if (!ntok) {
		(void)!write(c, "err empty\n", 10);
		return;
	}

	/* The catalogue is re-read on EVERY request: a medium pulled out
	 * between two requests must not still be offered. */
	kd_scan();

	if (!strcmp(tok[0], "ping") && ntok == 1) {
		(void)!write(c, "ok\n", 3);
	} else if (!strcmp(tok[0], "list") && ntok == 1) {
		reply_list(c);
	} else if (!strcmp(tok[0], "status") && ntok == 1) {
		reply_status(c);
	} else if (!strcmp(tok[0], "info") && ntok == 2) {
		if (!kd_id_ok(tok[1]))
			(void)!write(c, "err not an id\n", 14);
		else
			reply_info(c, tok[1]);
	} else if (!strcmp(tok[0], "mount") && ntok == 2) {
		int i = kd_id_ok(tok[1]) ? kd_find(tok[1]) : -1;
		if (i < 0)
			dprintf(c, "err no pack %s\n", tok[1]);
		else if (kd_mount(i, msg, sizeof(msg)) == 0)
			dprintf(c, "ok %s\n", msg);
		else
			dprintf(c, "err %s\n", msg);
	} else if (!strcmp(tok[0], "unmount") && ntok == 2) {
		int i = kd_id_ok(tok[1]) ? kd_find(tok[1]) : -1;
		if (i < 0)
			dprintf(c, "err no pack %s\n", tok[1]);
		else if (kd_unmount(i, msg, sizeof(msg)) == 0)
			dprintf(c, "ok %s\n", msg);
		else
			dprintf(c, "err %s\n", msg);
	} else if (!strcmp(tok[0], "compose") && ntok >= 3) {
		for (int i = 2; i < ntok; i++)
			if (!kd_id_ok(tok[i])) {
				dprintf(c, "err %s is not an id\n", tok[i]);
				return;
			}
		if (kd_compose(tok[1], tok + 2, ntok - 2, uid, msg,
			       sizeof(msg)) == 0)
			dprintf(c, "ok %s\n", msg);
		else
			dprintf(c, "err %s\n", msg);
	} else if (!strcmp(tok[0], "decompose") && ntok == 2) {
		if (kd_decompose(tok[1], msg, sizeof(msg)) == 0)
			dprintf(c, "ok %s\n", msg);
		else
			dprintf(c, "err %s\n", msg);
	} else if (!strcmp(tok[0], "install") && ntok == 2) {
		if (kd_install(tok[1], msg, sizeof(msg)) == 0)
			dprintf(c, "ok %s\n", msg);
		else
			dprintf(c, "err %s\n", msg);
	} else if (!strcmp(tok[0], "remove") && ntok == 2) {
		if (kd_id_ok(tok[1]) && kd_remove(tok[1], msg, sizeof(msg)) == 0)
			dprintf(c, "ok %s\n", msg);
		else
			dprintf(c, "err %s\n", msg[0] ? msg : "not an id");
	} else if (!strcmp(tok[0], "rollback") && ntok == 2) {
		if (kd_id_ok(tok[1]) && kd_rollback(tok[1], msg, sizeof(msg)) == 0)
			dprintf(c, "ok %s\n", msg);
		else
			dprintf(c, "err %s\n", msg[0] ? msg : "not an id");
	} else if (!strcmp(tok[0], "graft") && ntok == 2) {
		int i = kd_id_ok(tok[1]) ? kd_find(tok[1]) : -1;
		if (i < 0)
			dprintf(c, "err no pack %s\n", tok[1]);
		else if (kd_graft(i, uid, msg, sizeof(msg)) == 0)
			dprintf(c, "ok %s\n", msg);
		else
			dprintf(c, "err %s\n", msg);
	} else if (!strcmp(tok[0], "ungraft") && ntok == 2) {
		if (kd_ungraft(tok[1], msg, sizeof(msg)) == 0)
			dprintf(c, "ok %s\n", msg);
		else
			dprintf(c, "err %s\n", msg);
	} else {
		(void)!write(c, "err unknown command\n", 20);
	}
}

static int serve(void)
{
	const char *path = sock_path();
	int srv;
	struct sockaddr_un addr = { .sun_family = AF_UNIX };

	if (geteuid() != 0 && !strcmp(path, KD_SOCKET)) {
		fprintf(stderr, "kdos-packd: must run as root\n");
		return 1;
	}

	kb_mkdir_p(kd_store_dir());
	{
		char *st = kb_path_join(kd_store_dir(), "staging");
		char *mn = kb_path_join(kd_store_dir(), "mnt");
		kb_mkdir_p(st);
		kb_mkdir_p(mn);
		/*
		 * The staging directory is the ONE place an unprivileged
		 * download may land, so `wheel` may write to it and nothing
		 * else in the store is writable. A download still never runs
		 * as root.
		 */
		chmod(st, 01777);
		free(st);
		free(mn);
	}

	kd_scan();
	kd_adopt();

	srv = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (srv < 0) {
		fprintf(stderr, "kdos-packd: socket: %s\n", strerror(errno));
		return 1;
	}
	/*
	 * A sun_path is 108 bytes and snprintf would TRUNCATE — which binds a
	 * socket at a path nobody asked for, leaves the unlink above aiming at
	 * the untruncated name, and answers the second start with "address
	 * already in use" for a file that appears not to exist. Refused
	 * instead, and named.
	 */
	if (strlen(path) >= sizeof(addr.sun_path)) {
		fprintf(stderr, "kdos-packd: socket path is longer than %zu "
				"bytes: %s\n", sizeof(addr.sun_path) - 1, path);
		close(srv);
		return 1;
	}
	snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path);
	unlink(path);
	if (bind(srv, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		fprintf(stderr, "kdos-packd: bind %s: %s\n", path, strerror(errno));
		close(srv);
		return 1;
	}
	chmod(path, 0666);
	if (listen(srv, 8) < 0) {
		close(srv);
		return 1;
	}
	kd_log("listening on %s, %d pack(s)", path, kd_npack);

	for (;;) {
		int c = accept(srv, NULL, NULL);
		struct ucred cred = {0};
		socklen_t len = sizeof(cred);
		char buf[1024] = {0};
		ssize_t n;

		if (c < 0) {
			if (errno == EINTR)
				continue;
			break;
		}
		if (getsockopt(c, SOL_SOCKET, SO_PEERCRED, &cred, &len) < 0 ||
		    !uid_allowed(cred.uid)) {
			kd_log("refused uid %u (not root and not in %s)",
			       (unsigned)cred.uid, KD_GROUP);
			(void)!write(c, "err not permitted\n", 18);
			close(c);
			continue;
		}
		n = read(c, buf, sizeof(buf) - 1);
		if (n <= 0) {
			close(c);
			continue;
		}
		buf[n] = 0;
		buf[strcspn(buf, "\r\n")] = 0;
		handle(c, buf, cred.uid);
		close(c);
	}
	close(srv);
	unlink(path);
	return 1;
}

/* ── the fixture ───────────────────────────────────────────────────────── */

/*
 * What WOULD be mounted, and nothing is. The seam `kdos stutter --fixture`,
 * `kdos-oomd --fixture` and `kdos-mountd --fixture` all use, and the only way
 * selection logic this consequential gets tested — on a host with no root, no
 * erofs and no loop device.
 */
static int fixture(int argc, char **argv)
{
	char msg[512];

	kd_fixture = 1;
	setenv("KDOS_PACK_STORE", argv[0], 1);
	if (argc > 1)
		setenv("KDOS_PACK_MEDIUM", argv[1], 1);
	else
		setenv("KDOS_PACK_MEDIUM", "/nonexistent", 1);

	kd_scan();
	printf("packs\n");
	for (int i = 0; i < kd_npack; i++) {
		KsigRing ring = {0};
		char who[KSIG_ID_HEX];
		KpkPack p;
		const char *sig = "unreadable";

		if (kpk_open(kd_pack[i].file, &p) == 0) {
			const char *kd = getenv("KDOS_KEYS");
			ksig_ring_load(&ring, kd && *kd ? kd : KD_KEYS);
			sig = kpk_sig_state_name(kpk_verify(&p, &ring, who));
		}
		printf("  %-16s %-8s %-8s %-9s %s\n", kd_pack[i].meta.id,
		       kd_pack[i].meta.version, kpk_kind_name(kd_pack[i].meta.kind),
		       kd_pack[i].origin == KD_ORIGIN_STORE ? "store" : "medium",
		       sig);
	}

	printf("compose\n");
	for (int i = 0; i < kd_npack; i++) {
		char *ids[1];
		char box[80];

		if (kd_pack[i].meta.kind != KPK_KIND_APP)
			continue;
		ids[0] = kd_pack[i].meta.id;
		snprintf(box, sizeof(box), "fx-%d", i);
		if (kd_compose(box, ids, 1, getuid(), msg, sizeof(msg)) == 0) {
			char *sp = strchr(msg, ' ');
			printf("  %-16s %s\n", kd_pack[i].meta.id,
			       sp ? sp + 1 : msg);
		} else {
			printf("  %-16s REFUSED %s\n", kd_pack[i].meta.id, msg);
		}
	}

	printf("graft\n");
	for (int i = 0; i < kd_npack; i++) {
		const KpkMeta *m = &kd_pack[i].meta;

		if (m->kind != KPK_KIND_DATA)
			continue;
		/* The DESTINATIONS are the interesting half — /usr/share for a
		 * host consumer, ~/.local/share/kdos/packs for a box, which
		 * shares $HOME and nothing else. */
		for (int g = 0; g < m->ngraft; g++)
			printf("  %-16s graft    %s -> %s/%s\n", m->id,
			       m->graft[g].from, KD_SHARE, m->graft[g].to);
		for (int g = 0; g < m->nboxgraft; g++)
			printf("  %-16s boxgraft %s -> ~/.local/share/kdos/packs/%s\n",
			       m->id, m->boxgraft[g].from, m->boxgraft[g].to);
		if (kd_graft(i, getuid(), msg, sizeof(msg)) == 0)
			printf("  %s\n", msg);
		else
			printf("  REFUSED %s\n", msg);
	}
	printf("%d pack(s)\n", kd_npack);
	return 0;
}

int main(int argc, char **argv)
{
	kb_set_progname("kdos-packd");

	if (argc > 2 && !strcmp(argv[1], "--fixture")) {
		quiet = getenv("KDOS_PACKD_VERBOSE") ? 0 : 1;
		return fixture(argc - 2, argv + 2);
	}
	if (argc > 1) {
		fprintf(stderr, "usage: kdos-packd [--fixture STORE [MEDIUM]]\n"
				"\nThe daemon takes no other argument. Talk to it\n"
				"on %s; every verb takes an id out of the list it\n"
				"publishes, and none takes a path.\n", KD_SOCKET);
		return 2;
	}
	signal(SIGPIPE, SIG_IGN);
	return serve();
}
