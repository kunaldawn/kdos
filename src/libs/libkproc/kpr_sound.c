/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kpr_sound — the PCM devices, and which way each one points
 *
 * A CARD IS NOT A MICROPHONE. `/proc/asound/cards` lists controllers; an HDMI
 * codec is a card with four playback PCMs and no capture stream at all. A
 * picker built from the card list offers a monitor's audio output as an input,
 * and the recorder that accepts it fails to open with a message about the
 * device rather than about the choice.
 *
 * `/proc/asound/pcm` is the file that answers it, one line per PCM:
 *
 *     01-00: ALC623 Analog : ALC623 Analog : playback 1 : capture 1
 *
 * card-device, the PCM's id, its name, and then one field per stream. The
 * capture flag is the presence of the `capture` field and nothing else.
 *
 * THE NAME COMES FROM THE LINE. Joining the card index to
 * `/proc/asound/cards` would cost a second read and answer `HD-Audio Generic`
 * where the line itself says `ALC623 Analog` — the worse label, and one that
 * would carry the host's card names into a fixture whose PCM list is recorded.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kproc.h"

/* One `field : field : field` component, trimmed. Returns the start of the
 * next component or NULL at the end of the line. */
static char *field(char *p, char **out)
{
	char *sep = strstr(p, " : ");

	if (sep)
		*sep = '\0';
	while (*p == ' ')
		p++;
	size_t n = strlen(p);
	while (n && p[n - 1] == ' ')
		p[--n] = '\0';
	*out = p;
	return sep ? sep + 3 : NULL;
}

KprSoundPcm *kpr_sound_pcms(int *n)
{
	char *buf = kpr_slurp_proc("asound/pcm");
	KprSoundPcm *v = NULL;
	int cnt = 0, cap = 0;

	*n = 0;
	if (!buf)
		return NULL;
	for (char *line = buf, *nl; line && *line; line = nl) {
		nl = strchr(line, '\n');
		if (nl)
			*nl++ = '\0';

		int card = -1, dev = -1;
		char *rest = strchr(line, ':');

		if (!rest || sscanf(line, "%d-%d", &card, &dev) != 2)
			continue;
		rest++;

		char *id, *name;

		rest = field(rest, &id);
		if (!rest)
			continue;
		rest = field(rest, &name);

		int cap_stream = 0, play_stream = 0;

		while (rest) {
			char *f;

			rest = field(rest, &f);
			if (!strncmp(f, "capture", 7))
				cap_stream = 1;
			else if (!strncmp(f, "playback", 8))
				play_stream = 1;
		}
		if (cnt == cap) {
			cap = cap ? cap * 2 : 8;
			KprSoundPcm *g = realloc(v, (size_t)cap * sizeof(*v));

			if (!g)
				break;
			v = g;
		}
		KprSoundPcm *p = &v[cnt++];

		p->card = card;
		p->device = dev;
		p->capture = cap_stream;
		p->playback = play_stream;
		kb_strlcpy(p->name, name && *name ? name : id, sizeof(p->name));
		snprintf(p->id, sizeof(p->id), "hw:%d,%d", card, dev);
	}
	free(buf);
	*n = cnt;
	return v;
}

void kpr_sound_free(KprSoundPcm *v)
{
	free(v);
}
