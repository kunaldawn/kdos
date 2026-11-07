/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   fwcore — what kdos-fwask decides, with no bus in it
 */

#include "fwcore.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "kbase.h"

/*
 * The services a session program serves. `rdp` and `vnc` are krdp's and
 * krfb's defaults (3389, 5900); `kiwix` is kiwix-serve asked to share its
 * library beyond this machine. The port each name opens is kdos-powerd's
 * table and is not repeated here.
 */
const FaAsk fa_asks[] = {
	{ "kdeconnect", "kdeconnectd", "KDE Connect",
	  "KDE Connect can't be reached by your phone" },
	{ "rdp", "krdpserver", "Remote Desktop",
	  "Remote Desktop can't be reached from other computers" },
	{ "vnc", "krfb", "Desktop Sharing",
	  "Desktop Sharing can't be reached from other computers" },
	{ "kiwix", "kiwix-serve", "Kiwix",
	  "Kiwix can't share your library with other devices" },
};
const int fa_nasks = (int)(sizeof(fa_asks) / sizeof(fa_asks[0]));

const FaAsk *fa_find(const char *service)
{
	for (int i = 0; i < fa_nasks; i++)
		if (!strcmp(fa_asks[i].service, service))
			return &fa_asks[i];
	return NULL;
}

int fa_token_ok(const char *s, size_t max)
{
	size_t n = 0;

	if (!s || !*s)
		return 0;
	for (; s[n]; n++) {
		unsigned char c = (unsigned char)s[n];

		if (n + 1 >= max)
			return 0;
		if (!(islower(c) || isdigit(c) || c == '-'))
			return 0;
	}
	return 1;
}

int fa_knocks_rose(FaKnocks *prev, const FaKnocks *now, char rose[][FA_NAME],
		   int max)
{
	int nrose = 0;

	for (int i = 0; i < now->n; i++) {
		unsigned long long before = 0;
		int seen = 0;

		for (int j = 0; j < prev->n; j++)
			if (!strcmp(prev->k[j].name, now->k[i].name)) {
				before = prev->k[j].packets;
				seen = 1;
				break;
			}
		unsigned long long fresh = !seen || now->k[i].packets < before
						   ? now->k[i].packets
						   : now->k[i].packets - before;
		if (fresh > 0 && nrose < max)
			kb_strlcpy(rose[nrose++], now->k[i].name, FA_NAME);
	}
	*prev = *now;
	return nrose;
}

int fa_running(const char *proc_root, const char *comm, uid_t uid)
{
	DIR *d = opendir(proc_root);
	struct dirent *e;
	int found = 0;

	if (!d)
		return 0;
	while (!found && (e = readdir(d))) {
		char path[512], name[32];
		struct stat st;
		const char *p = e->d_name;

		while (*p && isdigit((unsigned char)*p))
			p++;
		if (*p || p == e->d_name)
			continue;
		snprintf(path, sizeof(path), "%s/%s", proc_root, e->d_name);
		/* The directory's owner is the process's real owner; a process
		 * that exited between readdir and stat is simply not counted. */
		if (stat(path, &st) != 0 || st.st_uid != uid)
			continue;
		snprintf(path, sizeof(path), "%s/%s/comm", proc_root, e->d_name);
		if (kb_read_line_file(path, name, sizeof(name)) <= 0)
			continue;
		if (!strcmp(name, comm))
			found = 1;
	}
	closedir(d);
	return found;
}

/* ── the answers ───────────────────────────────────────────────────────── */

static int memory_push(FaMemory *m, const char *net, const char *name,
		       const char *answer)
{
	if (m->n == m->cap) {
		int cap = m->cap ? m->cap * 2 : 32;

		m->a = kb_realloc(m->a, (size_t)cap * sizeof(*m->a));
		m->cap = cap;
	}
	kb_strlcpy(m->a[m->n].net, net, FA_NET);
	kb_strlcpy(m->a[m->n].name, name, FA_NAME);
	kb_strlcpy(m->a[m->n].answer, answer, FA_ANSWER);
	m->n++;
	return 0;
}

int fa_memory_load(FaMemory *m, const char *path)
{
	size_t len = 0;
	char *data;

	m->n = 0;
	data = kb_read_all(path, &len);
	if (!data)
		return errno == ENOENT ? 0 : -1;
	for (char *line = data, *next; line && *line; line = next) {
		char net[FA_NET + 1], name[FA_NAME + 1], answer[FA_ANSWER + 1];
		char rest;

		next = strchr(line, '\n');
		if (next)
			*next++ = '\0';
		/* One field more than the format allows is a line someone
		 * edited into another shape, and is dropped whole. */
		if (sscanf(line, "%40s %32s %12s %c", net, name, answer, &rest) != 3)
			continue;
		if (!fa_token_ok(net, FA_NET) || !fa_token_ok(name, FA_NAME) ||
		    !fa_token_ok(answer, FA_ANSWER))
			continue;
		memory_push(m, net, name, answer);
	}
	free(data);
	return m->n;
}

int fa_memory_asked(const FaMemory *m, const char *net, const char *name)
{
	for (int i = 0; i < m->n; i++)
		if (!strcmp(m->a[i].net, net) && !strcmp(m->a[i].name, name))
			return 1;
	return 0;
}

static int memory_rewrite(const FaMemory *m, const char *path)
{
	KbBuf b = { 0 };
	int from = m->n > FA_MEMORY_KEEP ? m->n - FA_MEMORY_KEEP : 0;
	int rc;

	for (int i = from; i < m->n; i++)
		kb_buf_printf(&b, "%s %s %s\n", m->a[i].net, m->a[i].name,
			      m->a[i].answer);
	rc = kb_write_file_atomic(path, b.p ? b.p : "");
	kb_buf_free(&b);
	return rc;
}

int fa_memory_record(FaMemory *m, const char *path, const char *net,
		     const char *name, const char *answer)
{
	char line[FA_NET + FA_NAME + FA_ANSWER + 4];
	int fd, len;

	if (!fa_token_ok(net, FA_NET) || !fa_token_ok(name, FA_NAME) ||
	    !fa_token_ok(answer, FA_ANSWER))
		return -1;
	memory_push(m, net, name, answer);

	if (m->n > FA_MEMORY_KEEP) {
		int rc = memory_rewrite(m, path);
		int drop = m->n - FA_MEMORY_KEEP;

		memmove(m->a, m->a + drop, (size_t)FA_MEMORY_KEEP * sizeof(*m->a));
		m->n = FA_MEMORY_KEEP;
		return rc;
	}

	{
		char *dir = kb_strdup(path);
		char *slash = strrchr(dir, '/');

		if (slash) {
			*slash = '\0';
			kb_mkdir_p(dir);
		}
		free(dir);
	}
	/* One write of a whole line with O_APPEND: two sessions of the same
	 * user answering at once interleave lines, never bytes. */
	len = snprintf(line, sizeof(line), "%s %s %s\n", net, name, answer);
	fd = open(path, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0600);
	if (fd < 0)
		return -1;
	if (write(fd, line, (size_t)len) != len) {
		close(fd);
		return -1;
	}
	return close(fd);
}

void fa_memory_free(FaMemory *m)
{
	free(m->a);
	m->a = NULL;
	m->n = m->cap = 0;
}

/* ── the words ─────────────────────────────────────────────────────────── */

void fa_escape(char *out, size_t n, const char *s, size_t cut)
{
	size_t lim = strlen(s), o = 0;

	/* The cut moves back to the start of the character it would split. */
	if (cut && lim > cut) {
		lim = cut;
		while (lim > 0 && ((unsigned char)s[lim] & 0xC0) == 0x80)
			lim--;
	}
	for (size_t in = 0; in < lim; in++) {
		char one[2] = { s[in], '\0' };
		const char *rep = one;

		if (s[in] == '&')
			rep = "&amp;";
		else if (s[in] == '<')
			rep = "&lt;";
		else if (s[in] == '>')
			rep = "&gt;";
		else if ((unsigned char)s[in] < 0x20)
			rep = " ";
		size_t l = strlen(rep);

		if (o + l >= n)
			break;
		memcpy(out + o, rep, l);
		o += l;
	}
	out[o] = '\0';
}

void fa_body(char *out, size_t n, const FaAsk *ask, const char *netname,
	     int may)
{
	char where[400], name[340];

	if (netname && *netname) {
		fa_escape(name, sizeof(name), netname, 64);
		snprintf(where, sizeof(where), "on the network “%s”",
			 name);
	} else {
		snprintf(where, sizeof(where), "on this network");
	}

	if (may)
		snprintf(out, n,
			 "The firewall is turning it away %s. Allow opens %s "
			 "to every network until you close it in Firewall "
			 "settings; Not on This Network stops this question "
			 "here.",
			 where, ask->app);
	else
		snprintf(out, n,
			 "The firewall is turning it away %s, and only an "
			 "administrator can open %s in Firewall settings.",
			 where, ask->app);
}
