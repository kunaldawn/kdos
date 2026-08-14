/*
 * privacycheck — the camera half of the recording indicator, against a /proc
 * that is a directory in this repo rather than the kernel's.
 *
 * `testing/fixtures/privacy/proc` is three processes: one holding /dev/video0
 * twice (a webcam app opens the device more than once, and the panel must name
 * it once), one holding /dev/video1, and one holding an ALSA capture device —
 * which must NOT count, because on a PipeWire system that process is PipeWire
 * itself and naming it would be the exact non-answer this feature exists to
 * replace.
 *
 * The microphone half needs a live PipeWire graph and is proved by hand against
 * a real session; what is asserted here is that its absence is harmless.
 */
#include <stdio.h>
#include <string.h>

#include "shell.h"

/*
 * privacy.c's one dependency on the rest of the shell, reimplemented here so
 * the check does not have to link shell.c and, through it, Wayland.
 */
int sh_read_line(const char *path, char *buf, size_t len)
{
	FILE *f = fopen(path, "r");
	if (!f)
		return -1;
	if (!fgets(buf, (int)len, f)) {
		fclose(f);
		return -1;
	}
	fclose(f);
	buf[strcspn(buf, "\n")] = 0;
	return 0;
}

static int fail(const char *msg)
{
	fprintf(stderr, "privacycheck: %s\n", msg);
	return 1;
}

int main(void)
{
	struct sh_state sh = {0};
	int rc = 0;

	if (sh_priv_init(&sh) != 0)
		return fail("could not start");

	int cams = sh_priv_count(&sh, SH_PRIV_CAM);
	const char *who = sh_priv_name(&sh, SH_PRIV_CAM);

	if (cams != 2)
		rc = fail("the camera holders were not counted once each");
	else if (!who || (strcmp(who, "cheese") && strcmp(who, "firefox")))
		rc = fail("the camera holder was not named from /proc");

	/* PIPEWIRE_RUNTIME_DIR points nowhere for this run, so the microphone
	 * half has no graph to read. Zero, not a crash and not a guess. */
	if (sh_priv_count(&sh, SH_PRIV_MIC) != 0)
		rc = fail("a microphone was reported with no PipeWire at all");

	sh_priv_free(&sh);
	if (!rc)
		printf("  privacy: two camera holders named, the ALSA holder "
		       "ignored\n");
	return rc;
}
