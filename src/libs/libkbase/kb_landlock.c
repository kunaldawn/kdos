/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   Landlock — unprivileged, inherited, irrevocable sandboxing
 *
 * Three syscalls and a prctl. No library, no capability, no namespace, no
 * root: any process can lock ITSELF down, and every child inherits it with
 * no way back out. That is why this belongs in libkbase, which links nothing
 * but musl — musl 1.2.5 already declares 444/445/446, so there is nothing to
 * link against and nothing to check for.
 *
 * The UAPI structs and constants are written out here rather than pulled from
 * <linux/landlock.h>. That header comes from the kernel headers package, and
 * libkbase is built in phase 1 against a sysroot that has no kernel tree —
 * including it would make the whole library conditional on a build order it
 * must not depend on. The values are ABI, so they cannot change.
 *
 * ── The trap this file exists to encode ────────────────────────────────
 *
 * `handled_access_fs` MUST NOT name a bit the running kernel does not know:
 * landlock_create_ruleset() answers EINVAL for the whole ruleset, so a binary
 * built against a newer ABI does not lose one restriction on an older kernel,
 * it fails to sandbox AT ALL. Every mask below is therefore assembled from
 * the ABI the kernel reports at runtime, never from what we compiled with.
 *
 * ── What this cannot do ────────────────────────────────────────────────
 *
 * A ruleset is not readable from outside the process. There is no
 * introspection API, and there is no /proc file. Anything reporting on a
 * sandboxed process can say what it was ASKED for and must say `unknown` for
 * everything else — never `denied`. See `kdos sandbox --explain`.
 * ---------------------------------
 */

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "kbase.h"

/* ── UAPI, verbatim ────────────────────────────────────────────────────── */

#define LL_CREATE_RULESET_VERSION (1U << 0)

#define LL_FS_EXECUTE		(1ULL << 0)
#define LL_FS_WRITE_FILE	(1ULL << 1)
#define LL_FS_READ_FILE		(1ULL << 2)
#define LL_FS_READ_DIR		(1ULL << 3)
#define LL_FS_REMOVE_DIR	(1ULL << 4)
#define LL_FS_REMOVE_FILE	(1ULL << 5)
#define LL_FS_MAKE_CHAR		(1ULL << 6)
#define LL_FS_MAKE_DIR		(1ULL << 7)
#define LL_FS_MAKE_REG		(1ULL << 8)
#define LL_FS_MAKE_SOCK		(1ULL << 9)
#define LL_FS_MAKE_FIFO		(1ULL << 10)
#define LL_FS_MAKE_BLOCK	(1ULL << 11)
#define LL_FS_MAKE_SYM		(1ULL << 12)
#define LL_FS_REFER		(1ULL << 13)	/* ABI 2 */
#define LL_FS_TRUNCATE		(1ULL << 14)	/* ABI 3 */
#define LL_FS_IOCTL_DEV		(1ULL << 15)	/* ABI 5 */

#define LL_NET_BIND_TCP		(1ULL << 0)	/* ABI 4 */
#define LL_NET_CONNECT_TCP	(1ULL << 1)	/* ABI 4 */

#define LL_SCOPE_ABSTRACT_UNIX	(1ULL << 0)	/* ABI 6 */
#define LL_SCOPE_SIGNAL		(1ULL << 1)	/* ABI 6 */

#define LL_RULE_PATH_BENEATH	1
#define LL_RULE_NET_PORT	2

struct ll_ruleset_attr {
	uint64_t handled_access_fs;
	uint64_t handled_access_net;	/* ABI 4 */
	uint64_t scoped;		/* ABI 6 */
};

struct ll_path_beneath_attr {
	uint64_t allowed_access;
	int32_t parent_fd;
} __attribute__((packed));

struct ll_net_port_attr {
	uint64_t allowed_access;
	uint64_t port;
};

/* The struct grew twice, and the kernel validates the size it is handed
 * against the sizes it knows. Send the prefix this ABI understands. */
static size_t attr_size(int abi)
{
	if (abi >= 6)
		return sizeof(struct ll_ruleset_attr);
	if (abi >= 4)
		return offsetof(struct ll_ruleset_attr, scoped);
	return offsetof(struct ll_ruleset_attr, handled_access_net);
}

/* Everything this ABI can police. Restricting an access the kernel does not
 * know about is the EINVAL above; NOT restricting one it does know is a hole,
 * so this tracks the ABI in both directions. */
static uint64_t fs_mask(int abi)
{
	uint64_t m = LL_FS_EXECUTE | LL_FS_WRITE_FILE | LL_FS_READ_FILE |
		     LL_FS_READ_DIR | LL_FS_REMOVE_DIR | LL_FS_REMOVE_FILE |
		     LL_FS_MAKE_CHAR | LL_FS_MAKE_DIR | LL_FS_MAKE_REG |
		     LL_FS_MAKE_SOCK | LL_FS_MAKE_FIFO | LL_FS_MAKE_BLOCK |
		     LL_FS_MAKE_SYM;
	if (abi >= 2)
		m |= LL_FS_REFER;
	if (abi >= 3)
		m |= LL_FS_TRUNCATE;
	if (abi >= 5)
		m |= LL_FS_IOCTL_DEV;
	return m;
}

/* Read access to a directory, and to everything under it. */
static uint64_t fs_read(int abi)
{
	uint64_t m = LL_FS_EXECUTE | LL_FS_READ_FILE | LL_FS_READ_DIR;
	if (abi >= 5)
		m |= LL_FS_IOCTL_DEV;
	return m;
}

/* Read plus everything that changes the tree. REFER is deliberately included:
 * without it, rename() and link() ACROSS two allowed directories fail, which
 * looks like a bug in the sandboxed program rather than policy. */
static uint64_t fs_write(int abi)
{
	uint64_t m = fs_read(abi) | LL_FS_WRITE_FILE | LL_FS_REMOVE_DIR |
		     LL_FS_REMOVE_FILE | LL_FS_MAKE_CHAR | LL_FS_MAKE_DIR |
		     LL_FS_MAKE_REG | LL_FS_MAKE_SOCK | LL_FS_MAKE_FIFO |
		     LL_FS_MAKE_BLOCK | LL_FS_MAKE_SYM;
	if (abi >= 2)
		m |= LL_FS_REFER;
	if (abi >= 3)
		m |= LL_FS_TRUNCATE;
	return m;
}

/* ── API ───────────────────────────────────────────────────────────────── */

int kb_landlock_abi(void)
{
	long v = syscall(__NR_landlock_create_ruleset, NULL, 0,
			 (unsigned long)LL_CREATE_RULESET_VERSION);
	return v < 0 ? -errno : (int)v;
}

int kb_landlock_new(KbLandlock *ll, int net_off)
{
	memset(ll, 0, sizeof(*ll));
	ll->abi = kb_landlock_abi();
	ll->fd = -1;
	if (ll->abi <= 0)
		return ll->abi < 0 ? ll->abi : -ENOSYS;

	struct ll_ruleset_attr attr = { .handled_access_fs = fs_mask(ll->abi) };

	/* Only claim the network if we intend to deny it. Handling
	 * bind/connect and then adding no port rule is a total TCP block,
	 * which is the point — but claiming it while allowing everything
	 * would need a rule per port, and there is no wildcard. */
	if (net_off && ll->abi >= 4)
		attr.handled_access_net = LL_NET_BIND_TCP | LL_NET_CONNECT_TCP;
	ll->net_handled = attr.handled_access_net != 0;

	long fd = syscall(__NR_landlock_create_ruleset, &attr,
			  attr_size(ll->abi), 0UL);
	if (fd < 0)
		return -errno;
	ll->fd = (int)fd;
	return 0;
}

int kb_landlock_allow(KbLandlock *ll, const char *path, int write)
{
	if (ll->fd < 0)
		return -EINVAL;

	/* O_PATH so this works for a directory we may not be allowed to read,
	 * and O_CLOEXEC so the fd never survives into the sandboxed program. */
	int fd = open(path, O_PATH | O_CLOEXEC);
	if (fd < 0)
		return -errno;	/* a path that is not there is the caller's problem */

	/* A rule on a NON-DIRECTORY may only name accesses that can apply to a
	 * file. Hand landlock_add_rule a mask containing MAKE_DIR or
	 * REMOVE_FILE for, say, /dev/null and it answers EINVAL and adds
	 * nothing — and since the usual shape is to ignore the return for
	 * best-effort base paths, the rule just quietly is not there. That
	 * presents as the sandboxed program failing on `>/dev/null`, which
	 * looks like a broken program rather than a missing rule. */
	struct stat st;
	int isdir = fstat(fd, &st) == 0 && S_ISDIR(st.st_mode);
	uint64_t acc = write ? fs_write(ll->abi) : fs_read(ll->abi);
	if (!isdir)
		acc &= LL_FS_EXECUTE | LL_FS_READ_FILE | LL_FS_WRITE_FILE |
		       (ll->abi >= 3 ? LL_FS_TRUNCATE : 0) |
		       (ll->abi >= 5 ? LL_FS_IOCTL_DEV : 0);

	struct ll_path_beneath_attr pb = {
		.allowed_access = acc,
		.parent_fd = fd,
	};
	long r = syscall(__NR_landlock_add_rule, ll->fd, LL_RULE_PATH_BENEATH,
			 &pb, 0UL);
	int err = r < 0 ? -errno : 0;
	close(fd);
	if (!err)
		ll->nrules++;
	return err;
}

int kb_landlock_allow_tcp(KbLandlock *ll, uint16_t port, int connect)
{
	if (ll->fd < 0)
		return -EINVAL;
	if (!ll->net_handled)
		return 0;	/* network was never restricted; nothing to allow */

	struct ll_net_port_attr np = {
		.allowed_access = connect ? LL_NET_CONNECT_TCP : LL_NET_BIND_TCP,
		.port = port,
	};
	long r = syscall(__NR_landlock_add_rule, ll->fd, LL_RULE_NET_PORT,
			 &np, 0UL);
	return r < 0 ? -errno : 0;
}

int kb_landlock_enforce(KbLandlock *ll)
{
	if (ll->fd < 0)
		return -EINVAL;

	/* Mandatory, and the ordering matters: without NO_NEW_PRIVS a
	 * sandboxed process could exec a setuid binary and climb out, so the
	 * kernel refuses restrict_self without it. */
	if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0))
		return -errno;

	long r = syscall(__NR_landlock_restrict_self, ll->fd, 0UL);
	int err = r < 0 ? -errno : 0;
	close(ll->fd);
	ll->fd = -1;
	return err;
}

void kb_landlock_free(KbLandlock *ll)
{
	if (ll->fd >= 0)
		close(ll->fd);
	ll->fd = -1;
}
