/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   KD's Homebrew Linux Distro
 * ---------------------------------
 */

/*
 * kb_trash.c — the freedesktop.org trash, once.
 *
 * `~/.local/share/Trash/files/NAME` is the file and
 * `~/.local/share/Trash/info/NAME.trashinfo` records where it came from and
 * when. BOTH are required: a file in `files/` with no `info/` entry cannot be
 * restored by anything, which makes "move to trash" a delete with extra steps
 * — and every other trash implementation on the machine, mc's included, reads
 * these.
 *
 * It lives here rather than in either consumer because `kdos-desk`'s Delete
 * key and `kdos trash` at a prompt must mean the same thing. Two
 * implementations would be two answers to what deleting is.
 */

#include "kbase.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

int kb_trash_dirs(char *files, size_t fn, char *info, size_t in)
{
	const char *home = kb_home_dir();
	char tmp[KB_TRASH_PATH];

	if (!home || !*home)
		return -1;
	if ((size_t)snprintf(tmp, sizeof(tmp), "%s/.local/share/Trash", home) >= sizeof(tmp))
		return -1;
	if ((size_t)snprintf(files, fn, "%s/files", tmp) >= fn)
		return -1;
	if ((size_t)snprintf(info, in, "%s/info", tmp) >= in)
		return -1;

	kb_mkdir_p(tmp);
	mkdir(files, 0700);
	mkdir(info, 0700);
	return 0;
}

/* Percent-encode for the Path= line. The spec says the value is a URI, so a
 * name with a space or a percent in it must be escaped or the record cannot be
 * parsed back. */
static void uri_escape(const char *in, char *out, size_t n)
{
	static const char *hex = "0123456789ABCDEF";
	size_t o = 0;

	for (const unsigned char *p = (const unsigned char *)in; *p && o + 4 < n; p++) {
		if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
		    (*p >= '0' && *p <= '9') || strchr("/-_.~", *p)) {
			out[o++] = (char)*p;
		} else {
			out[o++] = '%';
			out[o++] = hex[*p >> 4];
			out[o++] = hex[*p & 0xf];
		}
	}
	out[o] = '\0';
}

static int hexval(int c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
}

/* A truncated escape (`%4` at the end, `%zz`) is copied through verbatim
 * rather than dropped: the record was written by somebody else's trash and a
 * path that comes back one character short restores to the wrong place. */
static void uri_unescape(const char *in, char *out, size_t n)
{
	size_t o = 0;

	for (const char *p = in; *p && o + 1 < n; p++) {
		int hi, lo;
		if (*p == '%' && (hi = hexval((unsigned char)p[1])) >= 0 &&
		    (lo = hexval((unsigned char)p[2])) >= 0) {
			out[o++] = (char)((hi << 4) | lo);
			p += 2;
		} else {
			out[o++] = *p;
		}
	}
	out[o] = '\0';
}

int kb_trash_put(const char *path)
{
	char files[KB_TRASH_PATH], info[KB_TRASH_PATH];
	char dest[KB_TRASH_PATH * 2], meta[KB_TRASH_PATH * 2 + 16];
	char unique[KB_TRASH_NAME], escaped[KB_TRASH_PATH * 3];
	char abs[KB_TRASH_PATH];
	const char *name;
	struct tm tm;
	time_t now;
	FILE *f;

	if (kb_trash_dirs(files, sizeof(files), info, sizeof(info)) != 0)
		return -1;

	/*
	 * The record must name an ABSOLUTE path or nothing can restore it —
	 * `kdos trash notes.txt` is the ordinary invocation and the cwd is
	 * gone by the time anybody reads the record back.
	 */
	if (*path == '/') {
		kb_strlcpy(abs, path, sizeof(abs));
	} else {
		char cwd[KB_TRASH_PATH];
		if (!getcwd(cwd, sizeof(cwd)))
			return -1;
		if ((size_t)snprintf(abs, sizeof(abs), "%s/%s", cwd, path) >= sizeof(abs))
			return -1;
	}

	/* Trashing the trash empties it in the worst possible order. */
	if (!strncmp(abs, files, strlen(files)) || !strncmp(abs, info, strlen(info))) {
		errno = EINVAL;
		return -1;
	}

	name = kb_basename(abs);
	if (!*name || !strcmp(name, ".") || !strcmp(name, "..")) {
		errno = EINVAL;
		return -1;
	}

	/*
	 * The name is made unique before either half is written. Trashing two
	 * files called `notes.txt` from different directories is the ordinary
	 * case, and the second silently replacing the first is data loss.
	 */
	kb_strlcpy(unique, name, sizeof(unique));
	for (int n = 1; n < 1000; n++) {
		snprintf(dest, sizeof(dest), "%s/%s", files, unique);
		if (access(dest, F_OK) != 0)
			break;
		snprintf(unique, sizeof(unique), "%.180s.%d", name, n);
	}
	snprintf(dest, sizeof(dest), "%s/%s", files, unique);
	snprintf(meta, sizeof(meta), "%s/%s.trashinfo", info, unique);

	/*
	 * The info file is written FIRST. If the rename then fails there is a
	 * stale record and no file, which every trash implementation ignores;
	 * the other order leaves a file nothing can restore.
	 */
	f = fopen(meta, "w");
	if (!f)
		return -1;
	uri_escape(abs, escaped, sizeof(escaped));
	now = time(NULL);
	localtime_r(&now, &tm);
	fprintf(f, "[Trash Info]\nPath=%s\nDeletionDate=%04d-%02d-%02dT%02d:%02d:%02d\n",
		escaped, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
		tm.tm_hour, tm.tm_min, tm.tm_sec);
	fclose(f);

	if (rename(abs, dest) != 0) {
		/*
		 * Across a filesystem boundary rename() cannot work. The
		 * record is removed so it does not outlive the attempt, and
		 * errno is left as rename set it so the caller can say EXDEV
		 * rather than "failed".
		 */
		int e = errno;
		unlink(meta);
		errno = e;
		return -1;
	}
	return 0;
}

/* Read one record. A `.trashinfo` that does not parse is absent, never
 * partial: a half-read Path= restores a file to the wrong place. */
static int read_info(const char *info, const char *name, KbTrashItem *it)
{
	char meta[KB_TRASH_PATH * 2 + 16], *text, *line, *save;
	size_t len;
	int have_path = 0;

	snprintf(meta, sizeof(meta), "%s/%s.trashinfo", info, name);
	text = kb_read_all(meta, &len);
	if (!text)
		return -1;

	it->orig[0] = it->when[0] = '\0';
	for (line = strtok_r(text, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
		if (!strncmp(line, "Path=", 5)) {
			uri_unescape(line + 5, it->orig, sizeof(it->orig));
			have_path = 1;
		} else if (!strncmp(line, "DeletionDate=", 13)) {
			kb_strlcpy(it->when, line + 13, sizeof(it->when));
		}
	}
	free(text);
	return have_path ? 0 : -1;
}

int kb_trash_list(KbTrashItem **out)
{
	char files[KB_TRASH_PATH], info[KB_TRASH_PATH], p[KB_TRASH_PATH * 2];
	KbTrashItem *v = NULL;
	int n = 0, cap = 0;
	struct dirent *e;
	DIR *d;

	*out = NULL;
	if (kb_trash_dirs(files, sizeof(files), info, sizeof(info)) != 0)
		return -1;
	d = opendir(files);
	if (!d)
		return 0;

	while ((e = readdir(d))) {
		struct stat st;
		KbTrashItem it;

		if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
			continue;
		memset(&it, 0, sizeof(it));
		kb_strlcpy(it.name, e->d_name, sizeof(it.name));
		/* A file with no record cannot be restored, so it is listed
		 * with an empty origin rather than hidden — hiding it means
		 * the only way to reclaim the space is by hand. */
		read_info(info, e->d_name, &it);

		snprintf(p, sizeof(p), "%s/%s", files, e->d_name);
		if (lstat(p, &st) == 0) {
			it.isdir = S_ISDIR(st.st_mode);
			it.bytes = (unsigned long long)st.st_size;
		}

		if (n == cap) {
			cap = cap ? cap * 2 : 32;
			v = realloc(v, (size_t)cap * sizeof(*v));
			if (!v) {
				closedir(d);
				return -1;
			}
		}
		v[n++] = it;
	}
	closedir(d);
	*out = v;
	return n;
}

int kb_trash_restore(const char *name, char *to, size_t tn)
{
	char files[KB_TRASH_PATH], info[KB_TRASH_PATH];
	char src[KB_TRASH_PATH * 2], meta[KB_TRASH_PATH * 2 + 16];
	KbTrashItem it;
	char *slash;

	if (kb_trash_dirs(files, sizeof(files), info, sizeof(info)) != 0)
		return -1;
	memset(&it, 0, sizeof(it));
	if (read_info(info, name, &it) != 0) {
		errno = ENOENT;
		return -1;
	}
	snprintf(src, sizeof(src), "%s/%s", files, name);
	snprintf(meta, sizeof(meta), "%s/%s.trashinfo", info, name);

	/* Restoring onto something that is there now would destroy it. */
	if (access(it.orig, F_OK) == 0) {
		errno = EEXIST;
		return -1;
	}
	/* The directory a file came from can have been removed since. */
	slash = strrchr(it.orig, '/');
	if (slash && slash != it.orig) {
		*slash = '\0';
		kb_mkdir_p(it.orig);
		*slash = '/';
	}
	if (rename(src, it.orig) != 0)
		return -1;
	unlink(meta);
	if (to)
		kb_strlcpy(to, it.orig, tn);
	return 0;
}

int kb_trash_remove(const char *name)
{
	char files[KB_TRASH_PATH], info[KB_TRASH_PATH];
	char p[KB_TRASH_PATH * 2], meta[KB_TRASH_PATH * 2 + 16];

	if (kb_trash_dirs(files, sizeof(files), info, sizeof(info)) != 0)
		return -1;
	snprintf(p, sizeof(p), "%s/%s", files, name);
	snprintf(meta, sizeof(meta), "%s/%s.trashinfo", info, name);

	/* The file goes first: an `info/` entry outliving its file is the
	 * state every implementation ignores, the other way round is a file
	 * nothing can name. */
	if (kb_is_dir(p) && !kb_is_link(p)) {
		if (kb_rmtree(p) != 0)
			return -1;
	} else if (unlink(p) != 0 && errno != ENOENT) {
		return -1;
	}
	unlink(meta);
	return 0;
}

int kb_trash_empty(void)
{
	KbTrashItem *v;
	int n, done = 0;

	n = kb_trash_list(&v);
	if (n < 0)
		return -1;
	for (int i = 0; i < n; i++)
		if (kb_trash_remove(v[i].name) == 0)
			done++;
	free(v);
	return done;
}
