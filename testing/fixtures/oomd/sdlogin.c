/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   sdlogin.c — libelogind's sd_uid_get_state(), answered from the environment
 *
 *     cc -shared -fPIC -o <dir>/libelogind.so.0 sdlogin.c
 *     LD_LIBRARY_PATH=<dir> KDOS_TEST_UID_STATE="65534=active" \
 *         kdos-oomd --explain nobody
 *
 * kdos-oomd opens libelogind.so.0 by name at run time, so a directory first on
 * LD_LIBRARY_PATH holding this one is the session tracker it asks, and the
 * daemon under test is the shipped binary unchanged. KDOS_TEST_UID_STATE holds
 * `uid=state` words; a uid it does not name is `offline`, which is what
 * elogind answers for a user with no login at all.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

int sd_uid_get_state(uid_t uid, char **state)
{
	const char *all = getenv("KDOS_TEST_UID_STATE");
	char want[32];
	size_t wl;

	snprintf(want, sizeof(want), "%u=", (unsigned)uid);
	wl = strlen(want);
	for (const char *p = all ? all : ""; *p;) {
		size_t len = strcspn(p, " ");

		if (len > wl && !strncmp(p, want, wl)) {
			*state = strndup(p + wl, len - wl);
			return *state ? 0 : -12;
		}
		p += len;
		while (*p == ' ')
			p++;
	}
	*state = strdup("offline");
	return *state ? 0 : -12;
}
