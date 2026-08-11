/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-fetch-app / kdos-fetch-static
 *
 * kdos-fetch-app installs an alien app into an ad-hoc distrobox and exports it
 * to the host. kdos-fetch-static drops a checksummed upstream binary into
 * /usr/local/bin.
 *
 * The app name reaching kdos-fetch-app is the reason this is C. The shell
 * version built the in-box command by INTERPOLATING it into a `bash -c`
 * string — `sudo apt install -y '$APP'` — which the outer shell expanded
 * before the inner shell ever parsed it, so an app name containing a quote
 * broke straight out of the quoting and ran as the box's root. Here the
 * package manager fallback is still shell, because that is what it is, but
 * the name arrives as a POSITIONAL PARAMETER and is never part of the script
 * text.
 * ---------------------------------
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "kdos-tools.h"

#define BOX_DEFAULT   "kdos-debian"
#define IMAGE_DEFAULT "debian:stable"
#define STATIC_DEST   "/usr/local/bin"

/* ──────────────────────────────────────────────────────────────────────── */

/*
 * Fixed script text, app name passed as $1. Nothing here is built from the
 * user's argument, so there is nothing to escape.
 */
static const char INSTALL_SH[] =
	"set -e\n"
	"if command -v apt >/dev/null; then\n"
	"  sudo apt update\n"
	"  sudo apt install -y -- \"$1\"\n"
	"elif command -v dnf >/dev/null; then\n"
	"  sudo dnf install -y -- \"$1\"\n"
	"elif command -v pacman >/dev/null; then\n"
	"  sudo pacman -Sy --noconfirm -- \"$1\"\n"
	"else\n"
	"  echo 'no known package manager in box' >&2; exit 1\n"
	"fi\n";

static const char REMOVE_SH[] =
	"if command -v apt >/dev/null; then sudo apt remove -y -- \"$1\"\n"
	"elif command -v dnf >/dev/null; then sudo dnf remove -y -- \"$1\"\n"
	"elif command -v pacman >/dev/null; then sudo pacman -Rns --noconfirm -- \"$1\"\n"
	"else echo 'no known package manager' >&2; exit 1; fi\n";

static const char WHICH_SH[] = "command -v -- \"$1\" 2>/dev/null\n";

static void box_sh(KbArgv *a, const char *box, const char *script,
		   const char *arg)
{
	kb_argv_add(a, "distrobox");
	kb_argv_add(a, "enter");
	kb_argv_add(a, box);
	kb_argv_add(a, "--");
	kb_argv_add(a, "sh");
	kb_argv_add(a, "-c");
	kb_argv_add(a, script);
	kb_argv_add(a, "sh");	/* $0 */
	kb_argv_add(a, arg);	/* $1 — the app, as data                   */
	kb_argv_end(a);
}

/* The app's binary inside the box, or "" when it is GUI-only. */
static void app_binary(const char *box, const char *app, char *out, size_t cap)
{
	KbArgv a = {0};
	box_sh(&a, box, WHICH_SH, app);
	if (kb_run_capture(&a, out, cap) != 0)
		out[0] = 0;

	/* command -v can print more than one line if the box's profile is
	 * noisy; the last one is the answer, as the `tail -1` did. */
	char *nl = strrchr(out, '\n');
	if (nl)
		memmove(out, nl + 1, strlen(nl + 1) + 1);
	size_t n = strlen(out);
	while (n && (out[n - 1] == '\r' || out[n - 1] == ' '))
		out[--n] = 0;
}

static int box_exists_named(const char *box)
{
	KbArgv a = {0};
	char buf[1 << 15];
	kb_argv_add(&a, "podman");
	kb_argv_add(&a, "ps");
	kb_argv_add(&a, "--all");
	kb_argv_add(&a, "--format");
	kb_argv_add(&a, "{{.Names}}");
	kb_argv_end(&a);
	if (kb_run_capture(&a, buf, sizeof(buf)) != 0)
		return 0;

	for (char *line = strtok(buf, "\n"); line; line = strtok(NULL, "\n"))
		if (!strcmp(line, box))
			return 1;
	return 0;
}

static void fetch_app_usage(void)
{
	printf("Usage: kdos-fetch-app [--box <name>] [--image <image>] <app>\n"
	       "       kdos-fetch-app --remove [--box <name>] <app>\n"
	       "\n"
	       "Defaults:\n"
	       "  --box    %s\n"
	       "  --image  %s  (only used if the box doesn't exist yet)\n"
	       "\n"
	       "Examples:\n"
	       "  kdos-fetch-app firefox\n"
	       "  kdos-fetch-app --box arch --image archlinux:latest yay\n"
	       "  kdos-fetch-app --remove firefox\n",
	       BOX_DEFAULT, IMAGE_DEFAULT);
}

int fetch_app_main(int argc, char **argv)
{
	const char *box = BOX_DEFAULT, *image = IMAGE_DEFAULT, *app = NULL;
	int remove = 0;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--box") && i + 1 < argc)
			box = argv[++i];
		else if (!strcmp(argv[i], "--image") && i + 1 < argc)
			image = argv[++i];
		else if (!strcmp(argv[i], "--remove"))
			remove = 1;
		else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
			fetch_app_usage();
			return 1;
		} else if (argv[i][0] == '-')
			kb_die("unknown flag: %s", argv[i]);
		else
			app = argv[i];
	}

	if (!app) {
		fetch_app_usage();
		return 1;
	}
	if (!kb_have_prog("distrobox"))
		kb_die("distrobox not installed");
	if (!kb_have_prog("podman"))
		kb_die("podman not installed");

	if (!box_exists_named(box)) {
		if (remove)
			kb_die("box '%s' does not exist", box);
		printf("==> Creating distrobox '%s' (image: %s)\n", box, image);
		KbArgv a = {0};
		kb_argv_add(&a, "distrobox");
		kb_argv_add(&a, "create");
		kb_argv_add(&a, "--name");
		kb_argv_add(&a, box);
		kb_argv_add(&a, "--image");
		kb_argv_add(&a, image);
		kb_argv_add(&a, "--yes");
		kb_argv_end(&a);
		kb_proc_verbose = 1;
		if (kb_run(&a) != 0)
			kb_die("could not create the box");
	}
	kb_proc_verbose = 1;

	char bin[512] = {0};

	if (remove) {
		printf("==> Removing exported launcher for '%s'\n", app);
		KbArgv a = {0};
		kb_argv_add(&a, "distrobox");
		kb_argv_add(&a, "enter");
		kb_argv_add(&a, box);
		kb_argv_add(&a, "--");
		kb_argv_add(&a, "distrobox-export");
		kb_argv_add(&a, "--app");
		kb_argv_add(&a, app);
		kb_argv_add(&a, "--delete");
		kb_argv_end(&a);
		if (kb_run(&a) != 0)
			printf("    (no .desktop export to remove)\n");

		app_binary(box, app, bin, sizeof(bin));
		if (bin[0]) {
			printf("==> Removing CLI wrapper for '%s'\n", app);
			KbArgv b = {0};
			kb_argv_add(&b, "distrobox");
			kb_argv_add(&b, "enter");
			kb_argv_add(&b, box);
			kb_argv_add(&b, "--");
			kb_argv_add(&b, "distrobox-export");
			kb_argv_add(&b, "--bin");
			kb_argv_add(&b, bin);
			kb_argv_add(&b, "--delete");
			kb_argv_end(&b);
			if (kb_run(&b) != 0)
				printf("    (no CLI wrapper to remove)\n");
		}

		printf("==> Uninstalling '%s' inside '%s'\n", app, box);
		KbArgv c = {0};
		box_sh(&c, box, REMOVE_SH, app);
		return kb_run(&c);
	}

	printf("==> Installing '%s' inside '%s'\n", app, box);
	KbArgv ins = {0};
	box_sh(&ins, box, INSTALL_SH, app);
	if (kb_run(&ins) != 0)
		kb_die("install failed inside '%s'", box);

	/* Two exports, and neither is fatal on its own: a GUI app has a
	 * .desktop but often no useful CLI name, a CLI tool has the reverse,
	 * and plenty have both. */
	int exported = 0;

	printf("==> Exporting '%s' as a host launcher\n", app);
	KbArgv e = {0};
	kb_argv_add(&e, "distrobox");
	kb_argv_add(&e, "enter");
	kb_argv_add(&e, box);
	kb_argv_add(&e, "--");
	kb_argv_add(&e, "distrobox-export");
	kb_argv_add(&e, "--app");
	kb_argv_add(&e, app);
	kb_argv_end(&e);
	if (kb_run(&e) == 0)
		exported = 1;
	else
		printf("    no .desktop file found for '%s' — skipping the "
		       "launcher entry\n", app);

	app_binary(box, app, bin, sizeof(bin));
	if (bin[0]) {
		printf("==> Exporting '%s' as a command (%s)\n", app, bin);
		char *dir = kb_path_join(kb_home_dir(), ".local/bin");
		kb_mkdir_p(dir);

		KbArgv b = {0};
		kb_argv_add(&b, "distrobox");
		kb_argv_add(&b, "enter");
		kb_argv_add(&b, box);
		kb_argv_add(&b, "--");
		kb_argv_add(&b, "distrobox-export");
		kb_argv_add(&b, "--bin");
		kb_argv_add(&b, bin);
		kb_argv_add(&b, "--export-path");
		kb_argv_add(&b, dir);
		kb_argv_end(&b);
		if (kb_run(&b) == 0)
			exported = 1;
		else
			printf("    could not export a CLI wrapper for '%s'\n",
			       app);
		free(dir);
	}

	if (!exported)
		kb_die("nothing to export for '%s' — is it really installed?",
		       app);

	printf("\nDone.\n"
	       "  launcher : ~/.local/share/applications/  "
	       "(Super opens the KDOS launcher)\n");
	if (bin[0])
		printf("  command  : just run '%s' — ~/.local/bin is on PATH\n",
		       app);
	return 0;
}

/* ──────────────────────────────────────────────────────────────────────── */

static int ends_with(const char *s, const char *suffix)
{
	size_t n = strlen(s), m = strlen(suffix);
	return n >= m && !strcmp(s + n - m, suffix);
}

int fetch_static_main(int argc, char **argv)
{
	if (argc != 4) {
		printf("Usage: kdos-fetch-static <name> <url> <sha256>\n"
		       "\n"
		       "Downloads <url>, checks its sha256, and installs the "
		       "binary\n(or extracts <name> from a tarball) into %s/"
		       "<name>.\n"
		       "\n"
		       "Tarball detection by extension: .tar, .tar.gz, .tgz, "
		       ".tar.xz, .tar.bz2, .tar.zst\nPlain binary: anything "
		       "else.\n", STATIC_DEST);
		return 1;
	}

	const char *name = argv[1], *url = argv[2], *want = argv[3];
	if (geteuid() != 0)
		kb_die("must run as root (writes to %s)", STATIC_DEST);

	char tmp[] = "/tmp/kdos-fetch-static.XXXXXX";
	if (!mkdtemp(tmp))
		kb_die("cannot create a work directory: %s", strerror(errno));

	char *archive = kb_path_join(tmp, kb_basename(url));

	printf("==> Downloading %s\n", url);
	kb_proc_verbose = 1;
	KbArgv c = {0};
	kb_argv_add(&c, "curl");
	kb_argv_add(&c, "-fL");
	kb_argv_add(&c, "-o");
	kb_argv_add(&c, archive);
	kb_argv_add(&c, "--");
	kb_argv_add(&c, url);
	kb_argv_end(&c);
	if (kb_run(&c) != 0)
		kb_die("download failed");

	printf("==> Verifying sha256\n");
	char sum[256] = {0};
	KbArgv s = {0};
	kb_argv_add(&s, "sha256sum");
	kb_argv_add(&s, "--");
	kb_argv_add(&s, archive);
	kb_argv_end(&s);
	if (kb_run_capture(&s, sum, sizeof(sum)) != 0)
		kb_die("sha256sum failed");
	char *sp = strchr(sum, ' ');
	if (sp)
		*sp = 0;
	if (strcmp(sum, want))
		kb_die("sha256 mismatch\n  expected: %s\n  actual:   %s", want,
		       sum);

	char *dest = kb_path_join(STATIC_DEST, name);

	const char *taropt = NULL;
	if (ends_with(archive, ".tar.gz") || ends_with(archive, ".tgz"))
		taropt = "-xzf";
	else if (ends_with(archive, ".tar.xz"))
		taropt = "-xJf";
	else if (ends_with(archive, ".tar.bz2"))
		taropt = "-xjf";
	else if (ends_with(archive, ".tar.zst"))
		taropt = "--zstd";
	else if (ends_with(archive, ".tar"))
		taropt = "-xf";

	if (!taropt) {
		KbArgv i = {0};
		kb_argv_add(&i, "install");
		kb_argv_add(&i, "-Dm755");
		kb_argv_add(&i, archive);
		kb_argv_add(&i, dest);
		kb_argv_end(&i);
		if (kb_run(&i) != 0)
			kb_die("could not install %s", dest);
		printf("==> Installed %s\n", dest);
		return 0;
	}

	KbArgv t = {0};
	kb_argv_add(&t, "tar");
	if (!strcmp(taropt, "--zstd")) {
		kb_argv_add(&t, "--zstd");
		kb_argv_add(&t, "-xf");
	} else {
		kb_argv_add(&t, taropt);
	}
	kb_argv_add(&t, archive);
	kb_argv_add(&t, "-C");
	kb_argv_add(&t, tmp);
	kb_argv_end(&t);
	if (kb_run(&t) != 0)
		kb_die("could not extract %s", archive);

	/* Locate <name> anywhere under the extraction dir. */
	char found[1024] = {0};
	KbArgv f = {0};
	kb_argv_add(&f, "find");
	kb_argv_add(&f, tmp);
	kb_argv_add(&f, "-type");
	kb_argv_add(&f, "f");
	kb_argv_add(&f, "-name");
	kb_argv_add(&f, name);
	kb_argv_add(&f, "-perm");
	kb_argv_add(&f, "-u+x");
	kb_argv_end(&f);
	kb_proc_verbose = 0;
	kb_run_capture(&f, found, sizeof(found));
	kb_proc_verbose = 1;
	char *nl = strchr(found, '\n');
	if (nl)
		*nl = 0;
	if (!found[0])
		kb_die("could not find binary '%s' inside the tarball", name);

	KbArgv i = {0};
	kb_argv_add(&i, "install");
	kb_argv_add(&i, "-Dm755");
	kb_argv_add(&i, found);
	kb_argv_add(&i, dest);
	kb_argv_end(&i);
	if (kb_run(&i) != 0)
		kb_die("could not install %s", dest);

	printf("==> Installed %s\n", dest);
	free(archive);
	free(dest);
	return 0;
}
