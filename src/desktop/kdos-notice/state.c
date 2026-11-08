/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   state.c — what kdos-notice last said, kept across logins
 *
 * $XDG_STATE_HOME/kdos/notice, one line per kind that has said something:
 *
 *   <kind> TAB <notification id> TAB <key>
 *
 * The key is decide.c's reduction of the state the notice was about; the id
 * is what the notification server returned, used to replace or withdraw
 * that notification. An id is good only while the server that issued it
 * runs, so main.c forgets every id it loads and every id it holds when
 * org.freedesktop.Notifications changes owner: Plasma takes a replaces_id
 * it never issued as the new notification's own number, and would replace
 * or close another program's notification of that number.
 *
 * Deleting the file announces everything once more. A line that does not
 * parse is dropped, which can at worst repeat a notice.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "notice.h"

static int path(char *out, size_t n)
{
	return kb_state_path("kdos/notice", out, n);
}

void kn_state_load(KnState *s)
{
	char p[1024], *text, *save = NULL;

	memset(s, 0, sizeof(*s));
	if (!path(p, sizeof(p)) || !(text = kb_read_whole(p, NULL)))
		return;
	for (char *l = strtok_r(text, "\n", &save); l;
	     l = strtok_r(NULL, "\n", &save)) {
		char *t1 = strchr(l, '\t'), *t2;
		int kind;

		if (!t1 || !(t2 = strchr(t1 + 1, '\t')))
			continue;
		*t1 = *t2 = '\0';
		kind = kn_kind_of(l);
		if (kind < 0 || strlen(t2 + 1) >= sizeof(s->key[0]))
			continue;
		s->id[kind] = (unsigned)strtoul(t1 + 1, NULL, 10);
		kb_strlcpy(s->key[kind], t2 + 1, sizeof(s->key[0]));
	}
	free(text);
}

int kn_state_save(const KnState *s)
{
	char p[1024], dir[1024];
	KbBuf b = { 0 };
	int r;

	if (!path(p, sizeof(p)))
		return -1;
	kb_strlcpy(dir, p, sizeof(dir));
	*strrchr(dir, '/') = '\0';
	if (kb_mkdir_p(dir) != 0)
		return -1;
	for (int k = 0; k < KN_COUNT; k++)
		if (s->key[k][0] && !strpbrk(s->key[k], "\t\n"))
			kb_buf_printf(&b, "%s\t%u\t%s\n", kn_kind_name(k),
				      s->id[k], s->key[k]);
	r = kb_write_file_atomic(p, b.p ? b.p : "");
	kb_buf_free(&b);
	return r;
}
