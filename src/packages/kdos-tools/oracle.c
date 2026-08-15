/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos oracle — one recorded lesson, at every login
 *
 * The corpus is the SAME one `kdos why` and `kdos explain` read: every file in
 * it is a debug cycle somebody already paid for, and preflight fails the build
 * when one of them names a port or a path that no longer exists. So this is a
 * fortune cookie that cannot go stale — which is the only kind worth having on
 * a machine whose whole argument is that it records its own reasons.
 *
 * The pick is (day XOR boot), not random: the same aphorism all day, so it can
 * be quoted at somebody an hour later, and a different one after a reboot, so
 * a machine that is up for a month is not showing one line for a month. NOT
 * the pid — that varies per PROCESS, so every login and every `kdos oracle`
 * drew a fresh line and neither half of the claim above held.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "kdos-tools.h"

/* The boot as a number. /proc/sys/kernel/random/boot_id is a fresh uuid per
 * boot and is world-readable; a machine without it (a chroot, a fixture) gets
 * 0, which costs the per-boot variation and nothing else. */
static unsigned long boot_seed(void)
{
	char *id = kb_read_all("/proc/sys/kernel/random/boot_id", NULL);
	unsigned long h = 1469598103934665603UL;	/* FNV-1a */

	if (!id)
		return 0;
	for (const char *p = id; *p; p++) {
		h ^= (unsigned char)*p;
		h *= 1099511628211UL;
	}
	free(id);
	return h;
}

/* Sorted (kb_listdir sorts), so the same day and boot select the same file on
 * every machine carrying the same corpus. */
static char *pick_file(void)
{
	int n = 0;
	char **names = kb_listdir(kdt_reason_dir(), &n);
	if (!names || n <= 0) {
		kb_strv_free(names);
		return NULL;
	}

	/* Day since the epoch, not the hour: an aphorism that changed while you
	 * were reading it would be a different kind of joke. */
	unsigned long day = (unsigned long)(time(NULL) / 86400);
	unsigned long seed = day ^ boot_seed();

	char *p = kb_path_join(kdt_reason_dir(), names[seed % (unsigned long)n]);
	kb_strv_free(names);
	return p;
}

/*
 * "the aphorism · the scar". `cite:` is the file's own attribution and the
 * first `port:` is the fallback — a reason always claims something, so there
 * is always an attribution to print.
 */
char *kdt_oracle_line(void)
{
	char *path = pick_file();
	if (!path)
		return NULL;
	char *text = kb_read_all(path, NULL);
	free(path);
	if (!text)
		return NULL;

	char *title = kdt_reason_header(text, "title", 0);
	char *cite = kdt_reason_header(text, "cite", 0);
	if (!cite)
		cite = kdt_reason_header(text, "port", 0);
	if (!cite)
		cite = kdt_reason_header(text, "path", 0);

	char *out = NULL;
	if (title) {
		KbBuf b = {0};
		if (cite)
			kb_buf_printf(&b, "%s · %s", title, cite);
		else
			kb_buf_str(&b, title);
		out = b.p;
	}
	free(title);
	free(cite);
	free(text);
	return out;
}

int oracle_main(int argc, char **argv)
{
	int plain = argc > 1 && !strcmp(argv[1], "--plain");

	char *line = kdt_oracle_line();
	if (!line) {
		fprintf(stderr, "kdos oracle: no reasons installed (%s)\n",
			kdt_reason_dir());
		return 1;
	}

	/* SGR 33 is the secondary slot: /etc/vtrgb maps the console's yellow
	 * onto the scheme's secondary, and the generated foot palette does the
	 * same, so one escape is the accent's secondary on both. Never bold —
	 * on a 512-glyph VT the intensity bit is the 9th glyph bit, so bold
	 * changes the FONT PAGE rather than the weight. */
	if (!plain && isatty(STDOUT_FILENO))
		printf("\033[33m%s\033[0m\n", line);
	else
		printf("%s\n", line);
	free(line);
	return 0;
}
