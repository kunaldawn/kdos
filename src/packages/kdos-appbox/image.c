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
 * The appbox image, in and out of the repository. This was ports/appbox/pack,
 * ports/appbox/assemble and the python heredoc inside
 * script/06_packaging/01_appbox.sh.
 *
 *   kdos-appbox image pack <appbox.tar> <image-dir>
 *       Explode a docker-archive into per-member zstd files, so the 11 GB
 *       image fits git LFS's 2 GB/file limit and a Containerfile segment
 *       rebuild only rewrites that segment's layer blob.
 *
 *   kdos-appbox image assemble <image-dir>
 *       Stream the docker-archive back out to stdout for `podman load` — no
 *       11 GB temp file, no extra disk.
 *
 *   kdos-appbox image remap-uids <storage-root>
 *       Push a rootful podman store's ownership into the rootless layout.
 *
 * zstd is exec'd rather than linked: it is already a build dependency, and
 * linking libzstd would put a real -l on a program that has none.
 */

#include "kdos-appbox.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>

/* Split compressed output at 1.5 G, comfortably under LFS's 2 G. */
#define CHUNK (1536LL * 1024 * 1024)
#define IOBUF (4 * 1024 * 1024)

/* ──────────────────────────────────────────────────────────────────────── */
/* INDEX.json — written and read by this file alone, so it is generated in a
 * fixed shape and parsed by looking for that shape rather than by a JSON
 * parser we would then have to own. */

typedef struct {
	char name[272];
	long long size;
	char chunks[16][288];
	int nchunks;
} Member;

#define MAX_MEMBERS 512

static Member members[MAX_MEMBERS];
static int nmembers;

static void index_write(const char *dir)
{
	KbBuf b = {0};
	kb_buf_str(&b, "[\n");
	for (int i = 0; i < nmembers; i++) {
		kb_buf_printf(&b, " {\n  \"name\": \"%s\",\n  \"size\": %lld,\n"
				  "  \"chunks\": [\n", members[i].name,
			      members[i].size);
		for (int c = 0; c < members[i].nchunks; c++)
			kb_buf_printf(&b, "   \"%s\"%s\n", members[i].chunks[c],
				      c + 1 < members[i].nchunks ? "," : "");
		kb_buf_printf(&b, "  ]\n }%s\n", i + 1 < nmembers ? "," : "");
	}
	kb_buf_str(&b, "]");

	char *p = kb_path_join(dir, "INDEX.json");
	if (kb_write_all(p, b.p, b.n) < 0)
		kb_die("cannot write %s", p);
	free(p);
	kb_buf_free(&b);
}

/* Pulls "name", "size" and the "chunks" strings out in order. Anything else
 * in the file is ignored. */
static void index_read(const char *dir)
{
	char *p = kb_path_join(dir, "INDEX.json");
	size_t len = 0;
	char *d = kb_read_all(p, &len);
	if (!d)
		kb_die("cannot read %s", p);
	free(p);

	nmembers = 0;
	Member *m = NULL;
	for (char *s = d; *s;) {
		if (!strncmp(s, "\"name\":", 7)) {
			if (nmembers >= MAX_MEMBERS)
				kb_die("INDEX.json has more than %d members",
				       MAX_MEMBERS);
			m = &members[nmembers++];
			memset(m, 0, sizeof(*m));
			char *q = strchr(s + 7, '"');
			char *e = q ? strchr(q + 1, '"') : NULL;
			if (!e)
				kb_die("INDEX.json: malformed name");
			size_t n = (size_t)(e - q - 1);
			if (n >= sizeof(m->name))
				kb_die("INDEX.json: name too long");
			memcpy(m->name, q + 1, n);
			s = e + 1;
			continue;
		}
		if (m && !strncmp(s, "\"size\":", 7)) {
			m->size = strtoll(s + 7, &s, 10);
			continue;
		}
		if (m && !strncmp(s, "\"chunks\":", 9)) {
			s += 9;
			while (*s && *s != ']') {
				if (*s != '"') {
					s++;
					continue;
				}
				char *e = strchr(s + 1, '"');
				if (!e)
					kb_die("INDEX.json: malformed chunk");
				size_t n = (size_t)(e - s - 1);
				if (m->nchunks >= 16 || n >= sizeof(m->chunks[0]))
					kb_die("INDEX.json: too many chunks");
				memcpy(m->chunks[m->nchunks], s + 1, n);
				m->chunks[m->nchunks][n] = 0;
				m->nchunks++;
				s = e + 1;
			}
			continue;
		}
		s++;
	}
	free(d);
}

/* ──────────────────────────────────────────────────────────────────────── */

/* Fork `zstd`, hand back the pipe end to talk to it on. */
static pid_t zstd_spawn(char *const argv[], int *pipefd, int to_child, int outfd)
{
	int fds[2];
	if (pipe(fds) < 0)
		kb_die("pipe: %s", strerror(errno));

	pid_t pid = fork();
	if (pid < 0)
		kb_die("fork: %s", strerror(errno));
	if (pid == 0) {
		if (to_child) {
			dup2(fds[0], STDIN_FILENO);
			if (outfd >= 0)
				dup2(outfd, STDOUT_FILENO);
		} else {
			dup2(fds[1], STDOUT_FILENO);
			if (outfd >= 0)
				dup2(outfd, STDIN_FILENO);
		}
		close(fds[0]);
		close(fds[1]);
		execvp(argv[0], argv);
		_exit(127);
	}
	close(to_child ? fds[0] : fds[1]);
	*pipefd = to_child ? fds[1] : fds[0];
	return pid;
}

static int reap(pid_t pid)
{
	int st;
	while (waitpid(pid, &st, 0) < 0)
		if (errno != EINTR)
			return -1;
	return WIFEXITED(st) ? WEXITSTATUS(st) : 128 + WTERMSIG(st);
}

/* ──────────────────────────────────────────────────────────────────────── */

static void mkparent(const char *path)
{
	char *copy = kb_strdup(path);
	char *slash = strrchr(copy, '/');
	if (slash) {
		*slash = 0;
		kb_mkdir_p(copy);
	}
	free(copy);
}

static int image_pack(const char *tarpath, const char *outdir)
{
	kb_rmtree(outdir);
	kb_mkdir_p(outdir);

	KbTarIn t;
	if (kb_tar_open(&t, tarpath) < 0)
		kb_die("cannot read %s", tarpath);

	KbTarEntry e;
	nmembers = 0;
	char *buf = kb_calloc(1, IOBUF);

	int rc;
	while ((rc = kb_tar_next(&t, &e)) == 1) {
		if (e.typeflag != '0' && e.typeflag != 0)
			continue;

		if (nmembers >= MAX_MEMBERS)
			kb_die("more than %d members in %s", MAX_MEMBERS, tarpath);
		/* Guarded rather than truncated: a silently shortened member
		 * name would produce an archive podman cannot load. */
		if (strlen(e.name) + 16 >= sizeof(members[0].chunks[0]))
			kb_die("member name too long for the index: %s", e.name);

		Member *m = &members[nmembers++];
		memset(m, 0, sizeof(*m));
		kb_strlcpy(m->name, e.name, sizeof(m->name));
		m->size = e.size;

		char zpath[1024];
		snprintf(zpath, sizeof(zpath), "%s/%s.zst", outdir, m->name);
		mkparent(zpath);

		int zfd = open(zpath, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
		if (zfd < 0)
			kb_die("cannot create %s", zpath);

		char *const argv[] = { (char *)"zstd", (char *)"-q", (char *)"-T0",
				       (char *)"-8", (char *)"-c", NULL };
		int pin;
		pid_t pid = zstd_spawn(argv, &pin, 1, zfd);
		close(zfd);

		int n;
		while ((n = kb_tar_read(&t, buf, IOBUF)) > 0) {
			ssize_t off = 0;
			while (off < n) {
				ssize_t w = write(pin, buf + off, (size_t)(n - off));
				if (w <= 0) {
					if (w < 0 && errno == EINTR)
						continue;
					kb_die("zstd: write failed on %s", m->name);
				}
				off += w;
			}
		}
		close(pin);
		if (reap(pid) != 0)
			kb_die("zstd failed on %s", m->name);

		struct stat st;
		if (stat(zpath, &st) < 0)
			kb_die("cannot stat %s", zpath);

		/* Via a local: the name and the chunk list live in the same
		 * struct, and the compiler cannot see that they do not
		 * overlap. */
		char nm[sizeof(m->name)];
		kb_strlcpy(nm, m->name, sizeof(nm));

		if (st.st_size <= CHUNK) {
			snprintf(m->chunks[0], sizeof(m->chunks[0]), "%s.zst", nm);
			m->nchunks = 1;
		} else {
			/* Split in place, then drop the whole file. */
			int in = open(zpath, O_RDONLY | O_CLOEXEC);
			if (in < 0)
				kb_die("cannot reopen %s", zpath);
			long long left = st.st_size;
			int part = 0;
			while (left > 0) {
				char ppath[1024];
				snprintf(m->chunks[part], sizeof(m->chunks[0]),
					 "%s.p%02d.zst", nm, part);
				snprintf(ppath, sizeof(ppath), "%s/%s", outdir,
					 m->chunks[part]);
				int out = open(ppath, O_WRONLY | O_CREAT | O_TRUNC |
							      O_CLOEXEC, 0644);
				if (out < 0)
					kb_die("cannot create %s", ppath);
				long long want = left < CHUNK ? left : CHUNK;
				while (want > 0) {
					size_t take = want < IOBUF ? (size_t)want
								   : IOBUF;
					ssize_t r = read(in, buf, take);
					if (r <= 0)
						break;
					if (write(out, buf, (size_t)r) != r)
						kb_die("short write to %s", ppath);
					want -= r;
					left -= r;
				}
				close(out);
				part++;
				if (part >= 16)
					kb_die("%s needs more than 16 chunks",
					       m->name);
			}
			close(in);
			unlink(zpath);
			m->nchunks = part;
		}

		printf("  %-72s %lld -> %d chunk%s\n", m->name, m->size,
		       m->nchunks, m->nchunks == 1 ? "" : "s");
	}
	free(buf);
	kb_tar_close(&t);
	if (rc < 0)
		kb_die("%s is not a readable tar archive", tarpath);

	index_write(outdir);
	printf("packed %d members\n", nmembers);
	return 0;
}

/* ──────────────────────────────────────────────────────────────────────── */

static int image_assemble(const char *dir)
{
	index_read(dir);

	char *buf = kb_calloc(1, IOBUF);
	for (int i = 0; i < nmembers; i++) {
		Member *m = &members[i];
		if (kb_tar_put_header(STDOUT_FILENO, m->name, m->size) < 0)
			kb_die("cannot write the header for %s", m->name);

		long long written = 0;
		for (int c = 0; c < m->nchunks; c++) {
			char *p = kb_path_join(dir, m->chunks[c]);
			int in = open(p, O_RDONLY | O_CLOEXEC);
			if (in < 0)
				kb_die("cannot read %s", p);
			free(p);

			char *const argv[] = { (char *)"zstd", (char *)"-q",
					       (char *)"-dc", NULL };
			int pout;
			pid_t pid = zstd_spawn(argv, &pout, 0, in);
			close(in);

			ssize_t n;
			while ((n = read(pout, buf, IOBUF)) > 0) {
				ssize_t off = 0;
				while (off < n) {
					ssize_t w = write(STDOUT_FILENO, buf + off,
							  (size_t)(n - off));
					if (w <= 0) {
						if (w < 0 && errno == EINTR)
							continue;
						kb_die("short write on %s", m->name);
					}
					off += w;
				}
				written += n;
			}
			close(pout);
			if (reap(pid) != 0)
				kb_die("zstd failed on %s", m->chunks[c]);
		}

		if (written != m->size)
			kb_die("%s: INDEX.json says %lld bytes, the chunks hold "
			       "%lld", m->name, m->size, written);
		if (kb_tar_pad(STDOUT_FILENO, m->size) < 0)
			kb_die("cannot pad %s", m->name);
	}
	free(buf);
	return kb_tar_finish(STDOUT_FILENO) < 0 ? 1 : 0;
}

/* ──────────────────────────────────────────────────────────────────────── */

#define BASE_UID  1000		/* kdos                                    */
#define SUB_BASE  100000	/* fs/etc/subuid: kdos:100000:65536        */
#define SUB_COUNT 65536

/* Idempotent: an id already in the rootless layout stays put, so a second
 * pass over the same store cannot corrupt it. Re-baking onto an existing
 * store used to clamp already-remapped uids at 165535 and every
 * `distrobox enter` then died with "crun: readlink ''". */
static unsigned remap(unsigned n)
{
	if (n == BASE_UID || (n >= SUB_BASE && n < SUB_BASE + SUB_COUNT))
		return n;
	if (n == 0)
		return BASE_UID;
	return SUB_BASE + (n < SUB_COUNT ? n : SUB_COUNT) - 1;
}

static long long remap_walk(const char *path)
{
	struct stat st;
	if (lstat(path, &st) < 0)
		return 0;

	if (lchown(path, remap(st.st_uid), remap(st.st_gid)) < 0)
		kb_warn("cannot chown %s", path);
	/* chown strips setuid/setgid from a regular file; put the mode back. */
	if (!S_ISLNK(st.st_mode) && (st.st_mode & (S_ISUID | S_ISGID)))
		chmod(path, st.st_mode & 07777);

	long long n = 1;
	if (S_ISDIR(st.st_mode) && !S_ISLNK(st.st_mode)) {
		char **names = kb_listdir(path, NULL);
		for (char **p = names; p && *p; p++) {
			char *child = kb_path_join(path, *p);
			n += remap_walk(child);
			free(child);
		}
		kb_strv_free(names);
	}
	return n;
}

static int image_remap(const char *root)
{
	if (!kb_is_dir(root))
		kb_die("no such storage root: %s", root);
	long long n = remap_walk(root);
	printf("remapped %lld entries\n", n);
	return 0;
}

/* ──────────────────────────────────────────────────────────────────────── */

int cmd_image(int argc, char **argv)
{
	if (argc < 1)
		kb_die("usage: kdos-appbox image pack|assemble|remap-uids ...");

	if (!strcmp(argv[0], "pack")) {
		if (argc < 3)
			kb_die("usage: kdos-appbox image pack <tar> <image-dir>");
		return image_pack(argv[1], argv[2]);
	}
	if (!strcmp(argv[0], "assemble")) {
		if (argc < 2)
			kb_die("usage: kdos-appbox image assemble <image-dir>");
		return image_assemble(argv[1]);
	}
	if (!strcmp(argv[0], "remap-uids")) {
		if (argc < 2)
			kb_die("usage: kdos-appbox image remap-uids <storage>");
		return image_remap(argv[1]);
	}
	kb_die("unknown image subcommand '%s'", argv[0]);
}
