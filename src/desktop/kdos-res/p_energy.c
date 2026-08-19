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
 * The Energy page — kdos-energyd's answer, rendered.
 *
 * It shows the DAEMON'S OWN REPORT rather than parsing its JSON and laying the
 * numbers out again. That is deliberate on two counts. The caveats are the
 * point of that report — shares are of ATTRIBUTABLE energy, the idle floor is
 * a measurement printed with the answer, GPU time is time and not joules, and
 * none of it is watt-hours — and a page that re-rendered the numbers would
 * eventually restate them loosely. And it needs no JSON parser here, so this
 * binary gains no dependency for one page.
 *
 * The socket is asked once per page visit, not once per tick: RAPL is a
 * free-running counter and the daemon's own sampling interval is fixed at 10 s.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "res.h"

#define EN_MAX 8192

static char g_text[EN_MAX];
static int g_have;
static int g_top;
static char g_why[160];

static const char *sock_path(void)
{
	const char *p = getenv("KDOS_ENERGYD_SOCKET");
	return p && *p ? p : "/run/kdos-energyd.sock";
}

static void fetch(void)
{
	g_text[0] = 0;
	g_why[0] = 0;

	int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (fd < 0) {
		kb_strlcpy(g_why, "no socket", sizeof(g_why));
		return;
	}
	struct sockaddr_un a;
	memset(&a, 0, sizeof(a));
	a.sun_family = AF_UNIX;
	kb_strlcpy(a.sun_path, sock_path(), sizeof(a.sun_path));

	if (connect(fd, (struct sockaddr *)&a, sizeof(a)) != 0) {
		/*
		 * DISTINGUISH THE TWO REASONS. "The daemon is not running" and
		 * "this machine has no RAPL domain" are different facts with
		 * different remedies, and 56_energyd.sh refuses to start the
		 * daemon at all on a machine without one — which is most VMs.
		 */
		if (kb_path_exists("/sys/class/powercap"))
			snprintf(g_why, sizeof(g_why),
				 "kdos-energyd is not running (the socket at "
				 "%s is not there)", sock_path());
		else
			snprintf(g_why, sizeof(g_why),
				 "this machine exposes no RAPL energy domain, "
				 "so per-application energy cannot be measured "
				 "here at all");
		close(fd);
		return;
	}

	if (write(fd, "report\n", 7) != 7) {
		kb_strlcpy(g_why, "the daemon accepted no request",
			   sizeof(g_why));
		close(fd);
		return;
	}
	size_t n = 0;
	for (;;) {
		ssize_t r = read(fd, g_text + n, sizeof(g_text) - n - 1);
		if (r <= 0)
			break;
		n += (size_t)r;
		if (n + 1 >= sizeof(g_text))
			break;
	}
	g_text[n] = 0;
	close(fd);
	if (!n)
		kb_strlcpy(g_why, "the daemon answered nothing",
			   sizeof(g_why));
}

void res_energy_prepare(void)
{
	if (!g_have) {
		g_have = 1;
		fetch();
	}
}

const char *res_energy_headline(void)
{
	return g_text[0] ? "per-application share of attributable energy"
			 : "unavailable";
}

void res_draw_energy(int x, int y, int w, int h)
{
	const int bottom = y + h;

	if (!g_text[0]) {
		ktui_draw_text(x + 1, y + 1, w - 2,
			       g_why[0] ? g_why : "no answer", KT_DIM,
			       KT_BG, 0);
		ktui_draw_text(x + 1, y + 3, w - 2,
			       "RAPL is root-only since Linux 5.10, so this "
			       "page is the daemon's answer or nothing.",
			       KT_DIM, KT_BG, 0);
		return;
	}

	/* The daemon's own lines, verbatim. */
	int row = y + 1;
	int line = 0;
	for (char *p = g_text; *p && row < bottom - 1; ) {
		char *nl = strchr(p, '\n');
		if (nl)
			*nl = 0;
		if (line++ >= g_top) {
			ktui_draw_text(x + 1, row, w - 2, p, KT_TEXT, KT_BG, 0);
			row++;
		}
		if (!nl)
			break;
		*nl = '\n';
		p = nl + 1;
	}
}

int res_energy_key(int k)
{
	if (k == KT_K_UP && g_top > 0) {
		g_top--;
		return 1;
	}
	if (k == KT_K_DOWN) {
		g_top++;
		return 1;
	}
	if (k == 'g') {			/* refresh */
		g_have = 0;
		return 1;
	}
	return 0;
}
