/*
 * KK — the KDOS demo.  Forked from BB (C) 1997 AA-group; see LICENSE.notice.
 *
 * Upstream's main() opened with an interactive "Music?[Y/n]" prompt and then a
 * keyboard-driven mixer menu — 16-bit? stereo? sample rate? — before a single
 * frame was drawn. A demo that ships as a system command has to start when it
 * is started, so the questions are gone and the answers are compiled in:
 * 44.1 kHz, 16-bit stereo, software mixer, interpolation. `--no-sound` is the
 * one thing the prompt could actually decide.
 *
 * The other change is the loader set. Upstream registered load_s3m and nothing
 * else, which is fine when the only modules in existence are the three it
 * shipped. KK's music is vendored from ModArchive's public-domain shelf and is
 * .xm/.it/.mod far more often than .s3m, so all loaders are registered — with
 * the S3M loader alone every one of them fails at Player_Load with a message
 * nobody sees.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "timers.h"
#include "bb.h"
#include "kk.h"

void stop(void);

#ifdef HAVE_LIBMIKMOD
#include <mikmod.h>
MODULE *module;
int bbsound;
tl_timer *update_timer;

static void update_sound(void *data)
{
	if (Player_Active())
		MikMod_Update();
	else if (module)
		Player_SetPosition(0);
}
#endif

int load_song(char *name)
{
#ifdef HAVE_LIBMIKMOD
	finish_stuff = 0;
	if (bbsound) {
		char *name2;
		name2 = malloc(strlen(name) + strlen(SOUNDDIR) + 2);
		sprintf(name2, "%s/%s", SOUNDDIR, name);
		stop();
		if (module != NULL)
			Player_Free(module);
		module = Player_Load(name2, 64, 0);
		if (!module)
			module = Player_Load(name, 64, 0);
		free(name2);
		/* Upstream painted the failure over the demo and slept a
		 * second. A missing module is a packaging bug, not something
		 * the viewer can act on — say it on stderr and keep playing
		 * the pictures. */
		if (module == NULL)
			fprintf(stderr, "kk: %s: %s\n", name,
				MikMod_strerror(MikMod_errno));
	}
#endif
	return 0;
}

void play(void)
{
#ifdef HAVE_LIBMIKMOD
	if (module != NULL) {
		Player_Start(module);
		update_sound(NULL);
		if (!update_timer) {
			update_timer = tl_create_timer();
			tl_set_handler(update_timer, update_sound, NULL);
			tl_set_interval(update_timer, 10000);
			tl_add_timer(syncgroup, update_timer);
			tl_reset_timer(update_timer);
		}
	}
#endif
}

void stop(void)
{
#ifdef HAVE_LIBMIKMOD
	if (bbsound) {
		if (module)
			Player_Free(module);
		module = NULL;
		if (update_timer) {
			tl_free_timer(update_timer);
			update_timer = NULL;
		}
	}
#endif
}

#ifdef HAVE_LIBMIKMOD
static void sound_init(void)
{
	if (!kk.sound)
		return;

	MikMod_RegisterAllDrivers();
	MikMod_RegisterAllLoaders();

	md_mixfreq = 44100;
	md_mode |= DMODE_16BITS | DMODE_STEREO | DMODE_SOFT_MUSIC | DMODE_INTERP;

	bbsound = 1;
	if (MikMod_Init("")) {
		fprintf(stderr, "kk: sound off: %s\n",
			MikMod_strerror(MikMod_errno));
		bbsound = 0;
	}
}
#endif

int main(int argc, char *argv[])
{
	int retval;

	if (kk_parse_args(&argc, argv))
		return 0;

	bbinit(argc, argv);
#ifdef HAVE_LIBMIKMOD
	sound_init();
#endif
	aa_resize(context);
	retval = bb();
#ifdef HAVE_LIBMIKMOD
	if (bbsound)
		MikMod_Exit();
#endif
	if (kk.fps)
		kk_report();
	return retval;
}
