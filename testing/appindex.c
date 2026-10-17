/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   appindex — the application index loses no entry
 *
 * `apps.c` loads one more entry than its first allocation holds, and every one
 * of them must come back: an index that stops at a fixed size drops the rest
 * from the Start menu with nothing on screen to say so. The fixture directory
 * is written here, `SH_APPS_CHUNK + 1` entries named `app0000`…, so the count
 * follows the constant.
 *
 * A BINARY OF ITS OWN for boxcheck's reason: `apps.c` lives in `kdos-shell`,
 * and linking the shell to reach it would drag a Wayland client in. The stubs
 * are the launch path, which nothing here calls, compiled against `shell.h` so
 * a drifted signature is a compile error.
 * ---------------------------------
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "launch.h"
#include "shell.h"

void sh_spawn(const char *const argv[]) { (void)argv; }
void sh_strip_field_codes(char *s) { (void)s; }
int sh_term_argv_in(const char *want, int floating, const char *size,
		    const char *argv[], int n, int max, const char *cmd,
		    char *id, size_t idsz)
{
	(void)want; (void)floating; (void)size; (void)argv; (void)max;
	(void)cmd; (void)id; (void)idsz;
	return n;
}
int sh_desktop_entry(const char *id, struct sh_entry *out)
{
	(void)id; (void)out;
	return -1;
}

int main(void)
{
	const char *root = getenv("XDG_DATA_HOME");
	const int want = SH_APPS_CHUNK + 1;
	char path[1024];
	int got, missing = 0;

	if (!root) {
		fprintf(stderr, "appindex: XDG_DATA_HOME names the fixture\n");
		return 2;
	}
	snprintf(path, sizeof(path), "%s/applications", root);
	mkdir(root, 0755);
	mkdir(path, 0755);
	for (int i = 0; i < want; i++) {
		FILE *f;

		snprintf(path, sizeof(path), "%s/applications/app%04d.desktop",
			 root, i);
		f = fopen(path, "w");
		if (!f) {
			perror(path);
			return 2;
		}
		fprintf(f, "[Desktop Entry]\nType=Application\n"
			   "Name=App %04d\nExec=app%04d\n", i, i);
		fclose(f);
	}

	got = sh_apps_load();
	for (int i = 0; i < want; i++) {
		char id[32];

		snprintf(id, sizeof(id), "app%04d", i);
		if (!sh_apps_find(id))
			missing++;
	}
	printf("  %d entries written, %d indexed, %d missing\n", want, got,
	       missing);
	return got == want && !missing ? 0 : 1;
}
