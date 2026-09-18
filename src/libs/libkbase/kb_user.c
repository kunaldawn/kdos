/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   Who the human accounts on this machine are
 *
 * ONE ANSWER TO "WHO MAY LOG IN", because two would disagree and the
 * disagreement would be invisible: a greeter offering an account the user
 * manager does not list, or the other way round, looks like a bug in
 * whichever one the person happened to open second.
 *
 * uid >= 1000 AND A SHELL THAT IS NOT A REFUSAL. Those are the two tests every
 * login screen makes, and together they are why `nobody` and the service
 * accounts are not offered. 65534 and above is `nobody` and the overflow ids,
 * which are not people either.
 *
 * THE GECOS FIELD IS A COMMA-SEPARATED RECORD and only its first field is a
 * name. Printing the whole thing puts an office number and a phone extension
 * on the login screen.
 * ---------------------------------
 */

#define _POSIX_C_SOURCE 200809L
#include <pwd.h>
#include <stdio.h>
#include <string.h>

#include "kbase.h"

int kb_users(KbUser *out, int max)
{
	struct passwd *pw;
	int n = 0;

	setpwent();
	while (n < max && (pw = getpwent()) != NULL) {
		if (pw->pw_uid < 1000 || pw->pw_uid >= 65534)
			continue;
		if (!pw->pw_shell || strstr(pw->pw_shell, "nologin") ||
		    strstr(pw->pw_shell, "/false"))
			continue;

		KbUser *u = &out[n++];

		memset(u, 0, sizeof(*u));
		snprintf(u->name, sizeof(u->name), "%s", pw->pw_name);
		snprintf(u->home, sizeof(u->home), "%s",
			 pw->pw_dir ? pw->pw_dir : "");
		snprintf(u->shell, sizeof(u->shell), "%s", pw->pw_shell);
		u->uid = pw->pw_uid;
		u->gid = pw->pw_gid;
		snprintf(u->gecos, sizeof(u->gecos), "%s",
			 pw->pw_gecos ? pw->pw_gecos : "");

		char *comma = strchr(u->gecos, ',');

		if (comma)
			*comma = '\0';
	}
	endpwent();
	return n;
}
