/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-audio — the devices the VOL applet cannot be
 *
 *   ╔═ audio ══════════════════════════════════════════════════╗
 *   ║ OUTPUTS                                     default: hw:0║
 *   ║ ▶ hw:0  HDA Intel PCH        ████████░░░░░░░░  62%       ║
 *   ║   hw:1  USB Audio            ██████████░░░░░░  71%       ║
 *   ║   pw    WH-1000XM4                            [pipewire] ║
 *   ║ ─────────────────────────────────────────────────────────║
 *   ║ BLUETOOTH                                       scanning ║
 *   ║ ▶ • WH-1000XM4              38:18:4C:..     connected    ║
 *   ║   · Keyboard K380           C4:2C:03:..     paired       ║
 *   ╚══════════════════════════════════════════════════════════╝
 *
 * The panel's VOL applet drives ONE ALSA mixer, which is the right thing for a
 * media key and is the whole of what a one-row indicator can be. Everything a
 * person actually asks of audio on a laptop is somewhere else: which device is
 * this coming out of, why is it coming out of the wrong one, and how do I get
 * the headphones to connect. That is this program.
 *
 * ALSA IS THE SUBSTRATE, PIPEWIRE IS OPTIONAL. Hardware volume lives in the
 * ALSA mixer whether or not a sound server is running — the same argument
 * kdos-osd is built on — so the card list is always there and always
 * switchable, and PipeWire's sinks are ADDED to it when a graph is reachable.
 * A desktop that could only switch outputs while pipewire was up would be a
 * desktop that cannot fix the case where pipewire is what went wrong.
 *
 * NOTHING HERE BLOCKS THE LOOP. Every bluez call is asynchronous — the device
 * list is a GetManagedObjects answered out of the loop's own sd_bus_process,
 * and Pair/Connect/Disconnect are fire-and-forget, their answer arriving as the
 * next refresh's property values. A pairing that waited for its reply would
 * freeze this window for the ten to thirty seconds a headset takes to think
 * about it, and a bounded 300 ms wait for the LIST would still be a surface
 * that stops drawing every two seconds whenever bluetoothd is wedged. The
 * ceiling is now what times the call out, not what the loop spends.
 */

#include <alsa/asoundlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <pipewire/pipewire.h>

/* KDOS ships basu; a development host usually has libsystemd, whose sd-bus is
 * the same API. The same selection tray.c and notifyd.c make. */
#if defined(__has_include)
#  if __has_include(<basu/sd-bus.h>)
#    include <basu/sd-bus.h>
#  else
#    include <systemd/sd-bus.h>
#  endif
#else
#  include <basu/sd-bus.h>
#endif

#include "kbase.h"
#include "kwl.h"
#include "shell.h"

#define AU_COLS		78
#define AU_ROWS		26
#define AU_MAX_DEV	32
#define AU_MAX_BT	32
#define AU_NAME		64

/* A bluez call that outlives this is a wedged daemon, not a slow bus — the
 * same ceiling and the same reasoning as the tray's property reads. */
#define BT_TIMEOUT_US	300000
#define BT_REFRESH_S	2

enum { AU_PANE_OUT = 0, AU_PANE_BT };

/* ── the output list ─────────────────────────────────────────────────────
 *
 * One row per ALSA card, then one per PipeWire sink. A card rather than a card
 * plus each of its PCM devices: `defaults.pcm.card` — the thing Enter writes —
 * selects a CARD, and offering a row that cannot be made the default would be
 * a control that does nothing.
 */
struct au_dev {
	int card;		/* ALSA card index, or -1 for a PipeWire sink */
	uint32_t pw_id;		/* PipeWire node id, when card < 0            */
	char name[AU_NAME];
	char detail[AU_NAME];
	int vol;		/* -1: no playback mixer on this card         */
	int muted;
};

static struct au_dev au_dev[AU_MAX_DEV];
static int au_ndev;
static int au_default_card;	/* what ~/.asoundrc says, or 0 */

/* ── ALSA ────────────────────────────────────────────────────────────────
 *
 * osd.c owns the mixer for the `default` PCM, which is the one the media keys
 * move. This one is per CARD — a different question, and the reason the element
 * search is here rather than shared: sh_volume_* has no card argument and must
 * not grow one, because the media keys are deliberately about whatever ALSA
 * calls default.
 */
static snd_mixer_elem_t *au_find_elem(snd_mixer_t *h)
{
	static const char *const NAMES[] = { "Master", "PCM", "Speaker",
					     "Headphone" };
	snd_mixer_selem_id_t *sid;
	snd_mixer_selem_id_alloca(&sid);

	for (size_t i = 0; i < sizeof(NAMES) / sizeof(NAMES[0]); i++) {
		snd_mixer_selem_id_set_index(sid, 0);
		snd_mixer_selem_id_set_name(sid, NAMES[i]);
		snd_mixer_elem_t *e = snd_mixer_find_selem(h, sid);
		if (e && snd_mixer_selem_has_playback_volume(e))
			return e;
	}
	return NULL;
}

static snd_mixer_t *au_mixer_open(int card, snd_mixer_elem_t **elem)
{
	char hw[16];
	snd_mixer_t *h = NULL;

	snprintf(hw, sizeof(hw), "hw:%d", card);
	if (snd_mixer_open(&h, 0) < 0)
		return NULL;
	if (snd_mixer_attach(h, hw) < 0 ||
	    snd_mixer_selem_register(h, NULL, NULL) < 0 ||
	    snd_mixer_load(h) < 0) {
		snd_mixer_close(h);
		return NULL;
	}
	*elem = au_find_elem(h);
	if (!*elem) {
		snd_mixer_close(h);
		return NULL;
	}
	return h;
}

/*
 * One mixer per card, kept open for the life of the process and re-read through
 * snd_mixer_handle_events. The panel's VOL applet learned this the expensive
 * way: open + attach + register + load per tick is a mixer rebuilt sixty times
 * a minute, and this list refreshes every two seconds plus once per volume
 * keystroke, per card. The ELEMENT is looked up again each time rather than
 * cached with the handle — handle_events can rebuild the element list when a
 * device goes away, and that lookup is a walk of an in-memory list.
 */
static struct {
	int card;
	snd_mixer_t *h;
} au_mix[AU_MAX_DEV];
static int au_nmix;

static snd_mixer_t *au_mixer(int card, snd_mixer_elem_t **elem)
{
	for (int i = 0; i < au_nmix; i++) {
		if (au_mix[i].card != card)
			continue;
		if (snd_mixer_handle_events(au_mix[i].h) < 0)
			return NULL;
		*elem = au_find_elem(au_mix[i].h);
		return *elem ? au_mix[i].h : NULL;
	}
	if (au_nmix >= AU_MAX_DEV)
		return NULL;
	snd_mixer_t *h = au_mixer_open(card, elem);
	if (!h)
		return NULL;
	au_mix[au_nmix].card = card;
	au_mix[au_nmix].h = h;
	au_nmix++;
	return h;
}

static void au_mixer_close_all(void)
{
	for (int i = 0; i < au_nmix; i++)
		snd_mixer_close(au_mix[i].h);
	au_nmix = 0;
}

static void au_card_volume(struct au_dev *d)
{
	snd_mixer_elem_t *e = NULL;
	long min = 0, max = 0, val = 0;

	d->vol = -1;
	d->muted = 0;
	if (d->card < 0 || !au_mixer(d->card, &e))
		return;
	if (snd_mixer_selem_get_playback_volume_range(e, &min, &max) == 0 &&
	    max > min &&
	    snd_mixer_selem_get_playback_volume(e, SND_MIXER_SCHN_FRONT_LEFT,
						&val) == 0)
		d->vol = (int)(((val - min) * 100 + (max - min) / 2) /
			       (max - min));
	if (snd_mixer_selem_has_playback_switch(e)) {
		int on = 1;
		snd_mixer_selem_get_playback_switch(e, SND_MIXER_SCHN_FRONT_LEFT,
						    &on);
		d->muted = !on;
	}
}

static void au_card_set_volume(struct au_dev *d, int pct)
{
	snd_mixer_elem_t *e = NULL;
	long min = 0, max = 0;

	if (d->card < 0 || !au_mixer(d->card, &e))
		return;
	if (pct < 0)
		pct = 0;
	if (pct > 100)
		pct = 100;
	if (snd_mixer_selem_get_playback_volume_range(e, &min, &max) == 0 &&
	    max > min) {
		snd_mixer_selem_set_playback_volume_all(e,
					min + (max - min) * pct / 100);
		/* Raising the volume unmutes — the same one line kdos-osd
		 * carries, and for the same complaint. */
		if (pct > 0 && snd_mixer_selem_has_playback_switch(e))
			snd_mixer_selem_set_playback_switch_all(e, 1);
	}
	au_card_volume(d);
}

static void au_card_toggle(struct au_dev *d)
{
	snd_mixer_elem_t *e = NULL;

	if (d->card < 0 || !au_mixer(d->card, &e))
		return;
	if (snd_mixer_selem_has_playback_switch(e))
		snd_mixer_selem_set_playback_switch_all(e, d->muted);
	au_card_volume(d);
}

/* Does this card have any playback PCM at all? A capture-only card is not an
 * output, and listing one gives the user a row that cannot be selected as the
 * place the sound comes out of. */
static int au_card_has_playback(snd_ctl_t *ctl)
{
	int dev = -1;

	while (snd_ctl_pcm_next_device(ctl, &dev) >= 0 && dev >= 0) {
		snd_pcm_info_t *pi;
		snd_pcm_info_alloca(&pi);
		snd_pcm_info_set_device(pi, (unsigned)dev);
		snd_pcm_info_set_subdevice(pi, 0);
		snd_pcm_info_set_stream(pi, SND_PCM_STREAM_PLAYBACK);
		if (snd_ctl_pcm_info(ctl, pi) >= 0)
			return 1;
	}
	return 0;
}

static void au_scan_alsa(void)
{
	int card = -1;

	while (snd_card_next(&card) >= 0 && card >= 0 && au_ndev < AU_MAX_DEV) {
		char hw[16];
		snd_ctl_t *ctl = NULL;
		snd_ctl_card_info_t *info;

		snprintf(hw, sizeof(hw), "hw:%d", card);
		if (snd_ctl_open(&ctl, hw, 0) < 0)
			continue;
		snd_ctl_card_info_alloca(&info);
		if (snd_ctl_card_info(ctl, info) < 0 ||
		    !au_card_has_playback(ctl)) {
			snd_ctl_close(ctl);
			continue;
		}

		struct au_dev *d = &au_dev[au_ndev++];
		memset(d, 0, sizeof(*d));
		d->card = card;
		snprintf(d->name, sizeof(d->name), "%s",
			 snd_ctl_card_info_get_name(info));
		snprintf(d->detail, sizeof(d->detail), "%s",
			 snd_ctl_card_info_get_mixername(info));
		snd_ctl_close(ctl);
		au_card_volume(d);
	}
}

/* ── ~/.asoundrc ─────────────────────────────────────────────────────────
 *
 * MERGED, never overwritten — the kdeglobals lesson on a smaller file. An
 * .asoundrc is a place people put real configuration (a dmix definition, a
 * software volume plugin), and a default-device switch that threw it away
 * would be a switch nobody uses twice. Only the two lines this program owns
 * are replaced.
 */
static int au_asoundrc(char *buf, size_t n)
{
	const char *home = getenv("HOME");

	if (!home || !*home)
		return -1;
	snprintf(buf, n, "%s/.asoundrc", home);
	return 0;
}

static int au_read_default_card(void)
{
	char path[512], line[256];
	FILE *f;
	int card = 0;

	if (au_asoundrc(path, sizeof(path)) != 0 || !(f = fopen(path, "r")))
		return 0;
	while (fgets(line, sizeof(line), f)) {
		char *p = line;
		while (*p == ' ' || *p == '\t')
			p++;
		if (!strncmp(p, "defaults.pcm.card", 17))
			card = atoi(p + 17);
	}
	fclose(f);
	return card;
}

/*
 * Temp file, fsync the FILE, fsync the DIRECTORY, rename. The directory fsync
 * is the step people leave out, and without it the rename can be lost while
 * the data survives — the same rule kdos-bootctl's state file is written under.
 */
static int au_write_default_card(int card)
{
	char path[512], tmp[544];
	char *old;
	KbBuf out = {0};
	FILE *f;
	int fd, rc = -1;

	if (au_asoundrc(path, sizeof(path)) != 0)
		return -1;
	old = kb_read_all(path, NULL);
	/* A file that EXISTS and did not read is not an empty one, and there is
	 * no line ceiling: a dmix plus a softvol plus a route definition is
	 * routinely longer than any fixed array, and truncating it here would
	 * throw away exactly what the merge exists to keep. */
	if (!old && access(path, F_OK) == 0)
		return -1;
	for (char *line = old, *next; line && *line; line = next) {
		char *nl = strchr(line, '\n');
		const char *p = line;

		next = nl ? nl + 1 : line + strlen(line);
		while (*p == ' ' || *p == '\t')
			p++;
		if (!strncmp(p, "defaults.pcm.card", 17) ||
		    !strncmp(p, "defaults.ctl.card", 17))
			continue;
		kb_buf_add(&out, line, (size_t)(next - line));
	}
	/* A file whose last line has no newline would otherwise get the first
	 * key glued onto the end of it. */
	if (out.n && out.p[out.n - 1] != '\n')
		kb_buf_add(&out, "\n", 1);
	/* Both keys: the PCM is where the sound goes and the CTL is which card
	 * `amixer` and every mixer applet then talks to. Writing one of them
	 * moves the audio and leaves the volume control on the old device. */
	kb_buf_printf(&out, "defaults.pcm.card %d\n", card);
	kb_buf_printf(&out, "defaults.ctl.card %d\n", card);
	free(old);

	snprintf(tmp, sizeof(tmp), "%s.kdos-tmp", path);
	fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
	if (fd < 0) {
		kb_buf_free(&out);
		return -1;
	}
	f = fdopen(fd, "w");
	if (!f) {
		close(fd);
		unlink(tmp);
		kb_buf_free(&out);
		return -1;
	}
	rc = fwrite(out.p, 1, out.n, f) == out.n ? 0 : -1;
	kb_buf_free(&out);
	if (rc != 0 || fflush(f) != 0 || fsync(fileno(f)) != 0) {
		fclose(f);
		unlink(tmp);
		return -1;
	}
	fclose(f);
	if (rename(tmp, path) != 0) {
		unlink(tmp);
		return -1;
	}

	char *slash = strrchr(path, '/');
	if (slash) {
		*slash = '\0';
		int dfd = open(path[0] ? path : "/", O_RDONLY | O_DIRECTORY |
						   O_CLOEXEC);
		if (dfd >= 0) {
			fsync(dfd);
			close(dfd);
		}
		*slash = '/';
	}
	return 0;
}

/* ── PipeWire sinks ──────────────────────────────────────────────────────
 *
 * The registry global carries media.class and node.description in its props,
 * so a sink is known without binding the node at all — one roundtrip instead
 * of one per device.
 */
struct au_pw {
	struct pw_loop *loop;
	struct pw_context *ctx;
	struct pw_core *core;
	struct pw_registry *registry;
	struct spa_hook registry_hook;
	struct spa_hook core_hook;
	int connected, broken, synced;
};

static struct au_pw au_pw;
/* Collected here rather than into au_dev directly: the registry callback fires
 * during a settle, and the device list is rebuilt from scratch by au_refresh. */
static struct { uint32_t id; char name[AU_NAME]; } au_sinks[AU_MAX_DEV];
static int au_nsinks;

static void au_reg_global(void *data, uint32_t id, uint32_t perms,
			  const char *type, uint32_t version,
			  const struct spa_dict *props)
{
	const char *cls, *name;
	(void)data; (void)perms; (void)version;

	if (strcmp(type, PW_TYPE_INTERFACE_Node) || !props ||
	    au_nsinks >= AU_MAX_DEV)
		return;
	cls = spa_dict_lookup(props, PW_KEY_MEDIA_CLASS);
	if (!cls || strcmp(cls, "Audio/Sink"))
		return;
	/* The description is the string the user has seen in every other
	 * mixer — "Built-in Audio Analog Stereo". node.name is
	 * `alsa_output.pci-0000_00_1f.3.analog-stereo`, which is an
	 * identifier and not a label. */
	name = spa_dict_lookup(props, PW_KEY_NODE_DESCRIPTION);
	if (!name)
		name = spa_dict_lookup(props, PW_KEY_NODE_NICK);
	if (!name)
		name = spa_dict_lookup(props, PW_KEY_NODE_NAME);

	for (int i = 0; i < au_nsinks; i++)
		if (au_sinks[i].id == id)
			return;
	au_sinks[au_nsinks].id = id;
	snprintf(au_sinks[au_nsinks].name, AU_NAME, "%s", name ? name : "sink");
	au_nsinks++;
}

static void au_reg_remove(void *data, uint32_t id)
{
	(void)data;
	for (int i = 0; i < au_nsinks; i++)
		if (au_sinks[i].id == id) {
			memmove(&au_sinks[i], &au_sinks[i + 1],
				(size_t)(au_nsinks - i - 1) * sizeof(au_sinks[0]));
			au_nsinks--;
			return;
		}
}

static const struct pw_registry_events au_registry_events = {
	PW_VERSION_REGISTRY_EVENTS,
	.global = au_reg_global,
	.global_remove = au_reg_remove,
};

static void au_core_error(void *data, uint32_t id, int seq, int res,
			  const char *msg)
{
	(void)data; (void)seq; (void)msg;
	if (id == PW_ID_CORE && res < 0)
		au_pw.broken = 1;
}

static void au_core_done(void *data, uint32_t id, int seq)
{
	(void)data; (void)seq;
	if (id == PW_ID_CORE)
		au_pw.synced = 1;
}

static const struct pw_core_events au_core_events = {
	PW_VERSION_CORE_EVENTS,
	.error = au_core_error,
	.done = au_core_done,
};

/* No PipeWire is not a failure: the ALSA half is the whole program without it,
 * and a machine on a bare TTY has exactly that. */
static void au_pw_init(void)
{
	pw_init(NULL, NULL);
	au_pw.loop = pw_loop_new(NULL);
	if (!au_pw.loop)
		return;
	au_pw.ctx = pw_context_new(au_pw.loop, NULL, 0);
	if (!au_pw.ctx)
		return;
	au_pw.core = pw_context_connect(au_pw.ctx, NULL, 0);
	if (!au_pw.core)
		return;
	pw_core_add_listener(au_pw.core, &au_pw.core_hook, &au_core_events, NULL);
	au_pw.registry = pw_core_get_registry(au_pw.core, PW_VERSION_REGISTRY, 0);
	if (!au_pw.registry)
		return;
	pw_registry_add_listener(au_pw.registry, &au_pw.registry_hook,
				 &au_registry_events, NULL);
	au_pw.connected = 1;
}

static void au_pw_settle(int ms)
{
	struct timespec t0, now;

	if (!au_pw.connected)
		return;
	clock_gettime(CLOCK_MONOTONIC, &t0);
	au_pw.synced = 0;
	pw_core_sync(au_pw.core, PW_ID_CORE, 0);
	while (!au_pw.synced && !au_pw.broken) {
		clock_gettime(CLOCK_MONOTONIC, &now);
		long el = (now.tv_sec - t0.tv_sec) * 1000 +
			  (now.tv_nsec - t0.tv_nsec) / 1000000;
		if (el >= ms)
			return;
		pw_loop_iterate(au_pw.loop, 20);
	}
}

static void au_pw_pump(void)
{
	if (au_pw.connected)
		for (int i = 0; i < 4; i++)
			pw_loop_iterate(au_pw.loop, 0);
}

static void au_pw_free(void)
{
	if (au_pw.registry)
		pw_proxy_destroy((struct pw_proxy *)au_pw.registry);
	if (au_pw.core)
		pw_core_disconnect(au_pw.core);
	if (au_pw.ctx)
		pw_context_destroy(au_pw.ctx);
	if (au_pw.loop)
		pw_loop_destroy(au_pw.loop);
	memset(&au_pw, 0, sizeof(au_pw));
}

/* ── wpctl ───────────────────────────────────────────────────────────────
 *
 * Switching PipeWire's default sink is a metadata write, and the program that
 * already knows how to do it is wireplumber's own. Spawned only when it is on
 * the PATH — an image with pipewire-media-session and no wireplumber has no
 * wpctl at all, and a control that silently did nothing there would be worse
 * than one that says what it can and cannot do.
 */
static int au_have_wpctl(void)
{
	const char *path = getenv("PATH");
	char buf[512];

	if (!path || !*path)
		path = "/usr/local/bin:/usr/bin:/bin";
	while (*path) {
		const char *end = strchr(path, ':');
		size_t n = end ? (size_t)(end - path) : strlen(path);
		if (n > 0 && n < sizeof(buf) - 8) {
			memcpy(buf, path, n);
			snprintf(buf + n, sizeof(buf) - n, "/wpctl");
			if (access(buf, X_OK) == 0)
				return 1;
		}
		if (!end)
			break;
		path = end + 1;
	}
	return 0;
}

/* ── bluez ───────────────────────────────────────────────────────────────
 *
 * READ-ONLY plus three actions. Everything shown is from one GetManagedObjects
 * on org.bluez, re-read on a timer: a property-change subscription would be
 * fewer bytes and one more thing to keep in agreement with what is on screen,
 * and the refresh IS how the result of a fire-and-forget Pair becomes visible.
 */
struct au_bt {
	char path[160];
	char alias[AU_NAME];
	char name[AU_NAME];
	char addr[24];
	int paired, connected, trusted;
};

static sd_bus *au_bus;
static struct au_bt au_bt[AU_MAX_BT];
static int au_nbt;
static char au_adapter[160];
static int au_bt_powered, au_bt_discovering;
/* Why the pane is empty, in the pane. An empty list with no explanation is
 * indistinguishable from a machine with no bluetooth at all. */
static char au_bt_why[96];
/* A GetManagedObjects is out and has not answered. */
static int au_bt_pending;

static void au_bt_read_props(sd_bus_message *m, struct au_bt *dev, int adapter)
{
	if (sd_bus_message_enter_container(m, 'a', "{sv}") <= 0)
		return;
	while (sd_bus_message_enter_container(m, 'e', "sv") > 0) {
		const char *key = NULL, *contents = NULL, *s = NULL;
		char t = 0;

		/* Leaving the dict ENTRY before giving up: the exit after this
		 * loop closes the a{sv}, and breaking from inside an entry
		 * makes it close the entry instead — every exit above it is
		 * then one level off and the rest of the reply is parsed
		 * against the wrong nesting. */
		if (sd_bus_message_read_basic(m, 's', &key) < 0 ||
		    sd_bus_message_peek_type(m, &t, &contents) < 0) {
			sd_bus_message_exit_container(m);
			break;
		}

		if (contents && !strcmp(contents, "s") &&
		    sd_bus_message_enter_container(m, 'v', "s") > 0) {
			sd_bus_message_read_basic(m, 's', &s);
			if (dev && s) {
				if (!strcmp(key, "Alias"))
					snprintf(dev->alias, AU_NAME, "%s", s);
				else if (!strcmp(key, "Name"))
					snprintf(dev->name, AU_NAME, "%s", s);
				else if (!strcmp(key, "Address"))
					snprintf(dev->addr, sizeof(dev->addr),
						 "%s", s);
			}
			sd_bus_message_exit_container(m);
		} else if (contents && !strcmp(contents, "b") &&
			   sd_bus_message_enter_container(m, 'v', "b") > 0) {
			int b = 0;
			sd_bus_message_read_basic(m, 'b', &b);
			if (dev) {
				if (!strcmp(key, "Paired"))
					dev->paired = b;
				else if (!strcmp(key, "Connected"))
					dev->connected = b;
				else if (!strcmp(key, "Trusted"))
					dev->trusted = b;
			} else if (adapter) {
				if (!strcmp(key, "Powered"))
					au_bt_powered = b;
				else if (!strcmp(key, "Discovering"))
					au_bt_discovering = b;
			}
			sd_bus_message_exit_container(m);
		} else {
			sd_bus_message_skip(m, "v");
		}
		sd_bus_message_exit_container(m);
	}
	sd_bus_message_exit_container(m);
}

/*
 * The reply, parsed where it arrives. GetManagedObjects is asked with
 * sd_bus_call_async and answered here out of the loop's own sd_bus_process,
 * because a synchronous call is a bounded block and a bounded block is still a
 * block: with bluetoothd wedged, 300 ms out of every two seconds is a surface
 * that stops drawing and stops answering the compositor.
 */
static int au_bt_reply(sd_bus_message *reply, void *userdata, sd_bus_error *e)
{
	(void)userdata;
	(void)e;

	au_bt_pending = 0;
	/* Cleared HERE rather than when the call was sent: the pane keeps the
	 * list it is showing until there is a new one to show. */
	au_nbt = 0;
	au_adapter[0] = '\0';
	au_bt_powered = 0;
	au_bt_discovering = 0;

	if (sd_bus_message_is_method_error(reply, NULL)) {
		snprintf(au_bt_why, sizeof(au_bt_why),
			 "bluetoothd is not running (service start 60_bluetooth)");
		return 0;
	}
	au_bt_why[0] = '\0';

	if (sd_bus_message_enter_container(reply, 'a', "{oa{sa{sv}}}") <= 0)
		return 0;
	while (sd_bus_message_enter_container(reply, 'e', "oa{sa{sv}}") > 0) {
		const char *obj = NULL;

		if (sd_bus_message_read_basic(reply, 'o', &obj) < 0)
			break;
		if (sd_bus_message_enter_container(reply, 'a', "{sa{sv}}") > 0) {
			while (sd_bus_message_enter_container(reply, 'e',
							      "sa{sv}") > 0) {
				const char *iface = NULL;

				if (sd_bus_message_read_basic(reply, 's',
							      &iface) < 0)
					break;
				if (!strcmp(iface, "org.bluez.Adapter1")) {
					/* The FIRST adapter. A machine with two
					 * is rare enough that offering a choice
					 * would be more chrome than answer. */
					if (!au_adapter[0])
						snprintf(au_adapter,
							 sizeof(au_adapter),
							 "%s", obj);
					au_bt_read_props(reply, NULL, 1);
				} else if (!strcmp(iface, "org.bluez.Device1") &&
					   au_nbt < AU_MAX_BT) {
					struct au_bt *d = &au_bt[au_nbt];
					memset(d, 0, sizeof(*d));
					snprintf(d->path, sizeof(d->path), "%s",
						 obj);
					au_bt_read_props(reply, d, 0);
					au_nbt++;
				} else {
					sd_bus_message_skip(reply, "a{sv}");
				}
				sd_bus_message_exit_container(reply);
			}
			sd_bus_message_exit_container(reply);
		}
		sd_bus_message_exit_container(reply);
	}
	sd_bus_message_exit_container(reply);

	if (!au_adapter[0])
		snprintf(au_bt_why, sizeof(au_bt_why),
			 "no bluetooth adapter on this machine");
	return 0;
}

static void au_bt_refresh(void)
{
	sd_bus_message *m = NULL;

	if (!au_bus) {
		au_nbt = 0;
		au_adapter[0] = '\0';
		snprintf(au_bt_why, sizeof(au_bt_why),
			 "no system bus — bluetooth is not reachable");
		return;
	}
	/* One in flight at a time: the timer fires every two seconds and a
	 * daemon taking longer than that would otherwise collect a queue of
	 * identical questions. The timeout answers with an error message, which
	 * is what clears this. */
	if (au_bt_pending)
		return;
	if (sd_bus_message_new_method_call(au_bus, &m, "org.bluez", "/",
					   "org.freedesktop.DBus.ObjectManager",
					   "GetManagedObjects") < 0)
		return;
	if (sd_bus_call_async(au_bus, NULL, m, au_bt_reply, NULL,
			      BT_TIMEOUT_US) >= 0)
		au_bt_pending = 1;
	sd_bus_message_unref(m);
}

/*
 * Wait out the first answer, and only the first. A dump that drew before the
 * reply arrived would report a machine with no bluetooth because it asked three
 * milliseconds ago — the same settle, and the same reason, as au_pw_settle.
 */
static void au_bt_settle(int ms)
{
	struct timespec t0, now;

	if (!au_bus || !au_bt_pending)
		return;
	clock_gettime(CLOCK_MONOTONIC, &t0);
	while (au_bt_pending) {
		clock_gettime(CLOCK_MONOTONIC, &now);
		long el = (now.tv_sec - t0.tv_sec) * 1000 +
			  (now.tv_nsec - t0.tv_nsec) / 1000000;
		if (el >= ms)
			return;
		if (sd_bus_process(au_bus, NULL) > 0)
			continue;
		if (sd_bus_wait(au_bus, 20 * 1000) < 0)
			return;
	}
}

static const char *au_bt_label(const struct au_bt *d)
{
	if (d->alias[0])
		return d->alias;
	if (d->name[0])
		return d->name;
	return d->addr;
}

/*
 * Fire and forget, every one of them. A headset takes seconds to answer a Pair
 * and tens of seconds to refuse one; waiting for the reply would wedge this
 * window for exactly as long. The answer shows up in the next refresh.
 */
static void au_bt_call(const char *path, const char *method)
{
	if (!au_bus || !path || !*path)
		return;
	sd_bus_call_method_async(au_bus, NULL, "org.bluez", path,
				 !strcmp(method, "StartDiscovery") ||
				 !strcmp(method, "StopDiscovery")
					 ? "org.bluez.Adapter1"
					 : "org.bluez.Device1",
				 method, NULL, NULL, NULL);
	sd_bus_flush(au_bus);
}

/* ── the device list ─────────────────────────────────────────────────────── */

static void au_refresh(void)
{
	au_ndev = 0;
	au_scan_alsa();
	au_default_card = au_read_default_card();

	au_pw_pump();
	for (int i = 0; i < au_nsinks && au_ndev < AU_MAX_DEV; i++) {
		struct au_dev *d = &au_dev[au_ndev++];
		memset(d, 0, sizeof(*d));
		d->card = -1;
		d->pw_id = au_sinks[i].id;
		d->vol = -1;
		snprintf(d->name, sizeof(d->name), "%s", au_sinks[i].name);
		snprintf(d->detail, sizeof(d->detail), "pipewire");
	}
}

/* What Enter does on the outputs pane, and what it reports back. */
static void au_set_default(struct au_dev *d, char *msg, size_t n)
{
	if (d->card >= 0) {
		if (au_write_default_card(d->card) == 0) {
			au_default_card = d->card;
			snprintf(msg, n, "default is hw:%d — new streams only",
				 d->card);
		} else {
			snprintf(msg, n, "could not write ~/.asoundrc");
		}
		return;
	}

	if (!au_have_wpctl()) {
		/*
		 * Said out loud rather than silently doing the ALSA thing: the
		 * user picked a PipeWire sink and is entitled to know the
		 * switch did not happen at that level.
		 */
		snprintf(msg, n, "no wpctl — the default is ALSA-level "
				 "(pick a card), and applies to new streams");
		return;
	}
	KbArgv a = {0};
	char id[16];
	snprintf(id, sizeof(id), "%u", d->pw_id);
	kb_argv_add(&a, "wpctl");
	kb_argv_add(&a, "set-default");
	kb_argv_add(&a, id);
	kb_argv_end(&a);
	if (kb_run(&a) == 0)
		snprintf(msg, n, "default sink is %s", d->name);
	else
		snprintf(msg, n, "wpctl refused node %u", d->pw_id);
}

/* ── drawing ─────────────────────────────────────────────────────────────── */

struct au_ui {
	int pane;
	int sel[2], top[2];
	char msg[96];
};

static void au_draw_outputs(struct au_ui *u, int y0, int rows, int w)
{
	for (int i = 0; i < rows && u->top[AU_PANE_OUT] + i < au_ndev; i++) {
		int idx = u->top[AU_PANE_OUT] + i;
		const struct au_dev *d = &au_dev[idx];
		int on = idx == u->sel[AU_PANE_OUT] && u->pane == AU_PANE_OUT;
		int y = y0 + i;
		/* "hw:" plus the widest int an %d can print, which is what
		 * -Wformat-truncation sizes against however small a real
		 * ALSA card index is. */
		char tag[16];
		/* Both names plus the parentheses: sized so the compiler can
		 * see it, because a -Wformat-truncation on a string that can
		 * never be that long is still a warning in the build log. */
		char label[AU_NAME * 2 + 8];

		/*
		 * A marker, not a filled row. The volume bar on the selected
		 * row would otherwise be drawn over an accent fill, and
		 * ktui_progress's own track would punch a hole through it —
		 * the same defect a heat strip has on a highlighted row.
		 */
		ktui_draw_text(2, y, 2, on ? ktui_glyph[KT_G_RIGHT] : " ",
			       KT_ACCENT, KT_SURFACE, KT_A_NONE);

		if (d->card >= 0)
			snprintf(tag, sizeof(tag), "hw:%d", d->card);
		else
			snprintf(tag, sizeof(tag), "pw");
		ktui_draw_text(4, y, 6, tag,
			       d->card >= 0 && d->card == au_default_card
				       ? KT_ACCENT : KT_DIM,
			       KT_SURFACE, KT_A_NONE);

		if (d->detail[0] && strcmp(d->detail, "pipewire"))
			snprintf(label, sizeof(label), "%s (%s)", d->name,
				 d->detail);
		else
			snprintf(label, sizeof(label), "%s", d->name);

		/* The bar takes the right of the row; the name gets what is
		 * left, and is clipped rather than allowed to run under it. */
		int bar_w = 16, pct_w = 5;
		int name_w = w - 10 - bar_w - pct_w - 3;
		if (name_w < 8) {
			/* Too narrow for a bar: the name gets the row rather
			 * than being squeezed under one. */
			name_w = w - 12;
			bar_w = 0;
		}
		ktui_draw_text(10, y, name_w, label,
			       on ? KT_TEXT : KT_MID, KT_SURFACE, KT_A_NONE);

		if (bar_w > 0 && d->vol >= 0) {
			int bx = w - 2 - pct_w - bar_w;
			char pct[8];
			ktui_progress_ex(krect(bx, y, bar_w, 1),
					 d->muted ? 0.0 : d->vol / 100.0, NULL,
					 KT_BAR_SOLID, KT_SURFACE);
			if (d->muted)
				snprintf(pct, sizeof(pct), "mute");
			else
				snprintf(pct, sizeof(pct), "%d%%", d->vol);
			ktui_draw_text_right(0, y, w - 2, pct,
					     d->muted ? KT_DIM : KT_ACCENT,
					     KT_SURFACE, KT_A_NONE);
		} else if (d->card < 0) {
			ktui_draw_text_right(0, y, w - 2, "[pipewire]", KT_DIM,
					     KT_SURFACE, KT_A_NONE);
		}
	}
	if (!au_ndev)
		ktui_draw_text(4, y0, w - 6, "no playback device", KT_DIM,
			       KT_SURFACE, KT_A_NONE);
}

static void au_draw_bt(struct au_ui *u, int y0, int rows, int w)
{
	if (au_bt_why[0]) {
		ktui_draw_text(4, y0, w - 6, au_bt_why, KT_DIM, KT_SURFACE,
			       KT_A_NONE);
		return;
	}
	if (!au_nbt) {
		ktui_draw_text(4, y0, w - 6,
			       au_bt_discovering ? "scanning"
						 : "no devices - press s to scan",
			       KT_DIM, KT_SURFACE, KT_A_NONE);
		return;
	}

	/*
	 * Three right-hand fields on one row, and the widths are subtracted
	 * from each other rather than guessed: a name allowed to run under the
	 * address is the geometry defect this toolkit ships most often. The
	 * address is dropped entirely below 56 columns instead of overlapping.
	 */
	const int status_w = 10, addr_w = 18;
	int show_addr = w >= 56;
	int name_w = w - 6 - status_w - (show_addr ? addr_w : 0) - 2;
	if (name_w < 6)
		name_w = 6;

	for (int i = 0; i < rows && u->top[AU_PANE_BT] + i < au_nbt; i++) {
		int idx = u->top[AU_PANE_BT] + i;
		const struct au_bt *d = &au_bt[idx];
		int on = idx == u->sel[AU_PANE_BT] && u->pane == AU_PANE_BT;
		int y = y0 + i;
		const char *mark = d->connected ? ktui_glyph[KT_G_BULLET]
				 : d->paired	? ktui_glyph[KT_G_DOT]
						: " ";
		int fg = d->connected ? KT_ACCENT : d->paired ? KT_MID : KT_DIM;

		ktui_draw_text(2, y, 2, on ? ktui_glyph[KT_G_RIGHT] : " ",
			       KT_ACCENT, KT_SURFACE, KT_A_NONE);
		ktui_draw_text(4, y, 2, mark, fg, KT_SURFACE, KT_A_NONE);
		ktui_draw_text(6, y, name_w, au_bt_label(d),
			       on ? KT_TEXT : KT_MID, KT_SURFACE, KT_A_NONE);
		if (show_addr)
			ktui_draw_text_right(0, y, w - 2 - status_w, d->addr,
					     KT_DIM, KT_SURFACE, KT_A_NONE);
		ktui_draw_text_right(0, y, w - 2,
				     d->connected ? "connected"
				     : d->paired  ? "paired"
						  : "new",
				     fg, KT_SURFACE, KT_A_NONE);
	}
}

static void au_draw(struct au_ui *u)
{
	int w = ktui_w, h = ktui_h;

	if (w < 40 || h < 12) {
		ktui_toosmall("kdos-audio", 40, 12);
		ktui_draw_flush();
		return;
	}

	ktui_draw_fill(krect(0, 0, w, h), KT_SURFACE);
	ktui_draw_box(krect(0, 0, w, h), " audio ", KT_ACCENT, KT_SURFACE, 1);

	int out_y = 2, out_rows = (h - 7) / 2;
	if (out_rows < 2)
		out_rows = 2;
	int sep_y = out_y + out_rows;
	int bt_hdr = sep_y + 1;
	int bt_y = bt_hdr + 1;
	int bt_rows = h - 2 - bt_y;
	if (bt_rows < 1)
		bt_rows = 1;

	ktui_draw_text(2, 1, w - 4, "OUTPUTS",
		       u->pane == AU_PANE_OUT ? KT_ACCENT : KT_DIM, KT_SURFACE,
		       KT_A_NONE);
	/* Only where there IS an ALSA card: `default hw:0` over an empty list is
	 * a claim about a device that is not there. */
	for (int i = 0; i < au_ndev; i++)
		if (au_dev[i].card >= 0) {
			char d[32];
			snprintf(d, sizeof(d), "default hw:%d",
				 au_default_card);
			ktui_draw_text_right(0, 1, w - 2, d, KT_DIM,
					     KT_SURFACE, KT_A_NONE);
			break;
		}
	au_draw_outputs(u, out_y, out_rows, w);

	ktui_draw_hline(1, sep_y, w - 2, KT_G_HL, KT_DIM, KT_SURFACE);

	ktui_draw_text(2, bt_hdr, w - 4, "BLUETOOTH",
		       u->pane == AU_PANE_BT ? KT_ACCENT : KT_DIM, KT_SURFACE,
		       KT_A_NONE);
	if (au_adapter[0])
		ktui_draw_text_right(0, bt_hdr, w - 2,
				     au_bt_discovering ? "scanning"
				     : au_bt_powered   ? "idle"
						       : "powered off",
				     au_bt_discovering ? KT_WARN : KT_DIM,
				     KT_SURFACE, KT_A_NONE);
	au_draw_bt(u, bt_y, bt_rows, w);

	/* The hint row is the message row: an action's answer belongs where the
	 * user's eyes already are, not in a toast they may not have on. */
	if (u->msg[0])
		ktui_draw_text(2, h - 2, w - 4, u->msg, KT_WARN, KT_SURFACE,
			       KT_A_NONE);
	else
		/* A hint chopped in half by the border tells the user about
		 * half the keys and lies about the rest of the sentence. */
		ktui_draw_text(2, h - 2, w - 4,
			       w >= 74 ? "Tab pane  <> volume  m mute  "
					 "Enter switch  s scan  p pair  Esc close"
				       : "Tab  <> vol  Enter  s scan  p pair",
			       KT_DIM, KT_SURFACE, KT_A_NONE);
	ktui_draw_flush();
}

/* ── main ─────────────────────────────────────────────────────────────────── */

static void au_clamp(struct au_ui *u, int rows_out, int rows_bt)
{
	int n[2] = { au_ndev, au_nbt };
	int rows[2] = { rows_out, rows_bt };

	for (int p = 0; p < 2; p++) {
		if (u->sel[p] >= n[p])
			u->sel[p] = n[p] ? n[p] - 1 : 0;
		if (u->sel[p] < 0)
			u->sel[p] = 0;
		if (rows[p] < 1)
			rows[p] = 1;
		if (u->sel[p] < u->top[p])
			u->top[p] = u->sel[p];
		if (u->sel[p] >= u->top[p] + rows[p])
			u->top[p] = u->sel[p] - rows[p] + 1;
	}
}

static int au_usage(void)
{
	fprintf(stderr, "usage: kdos-audio [--font NAME] [--dump]\n");
	return 2;
}

int audio_main(int argc, char **argv)
{
	const char *font = NULL;
	int dump = 0;
	struct au_ui u = {0};

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--font") && i + 1 < argc)
			font = argv[++i];
		else if (!strcmp(argv[i], "--dump"))
			dump = 1;
		else
			return au_usage();
	}

	sh_theme_from_cache();
	au_pw_init();
	/* The graph takes a connect, a registry and a global per sink to become
	 * known; asking once and drawing would report a machine with no sinks
	 * because it asked too early. Same settle kdos-shell --dump does. */
	au_pw_settle(600);
	if (sd_bus_open_system(&au_bus) < 0)
		au_bus = NULL;
	au_bt_refresh();
	au_bt_settle(BT_TIMEOUT_US / 1000);
	au_refresh();

	if (dump) {
		ktui_offscreen_init(AU_COLS, AU_ROWS);
		au_draw(&u);
		ktui_draw_dump();
		if (au_bus)
			sd_bus_unref(au_bus);
		au_pw_free();
		au_mixer_close_all();
		return 0;
	}

	KwlConfig cfg = {
		.role = KWL_ROLE_OVERLAY,
		.cols = AU_COLS,
		.rows = AU_ROWS,
		.app_id = "kdos-audio",
		.font = font,
		.keyboard = 1,
		/* A dialog, not a menu: people click back to whatever is
		 * playing to hear whether the switch worked. */
	};
	if (kwl_init(&cfg) != 0) {
		fprintf(stderr, "kdos-audio: no compositor or no layer-shell\n");
		if (au_bus)
			sd_bus_unref(au_bus);
		au_pw_free();
		au_mixer_close_all();
		return 1;
	}
	ktui_draw_init();

	time_t last_bt = time(NULL), last_dev = last_bt;

	while (!kwl_should_close()) {
		int out_rows = (ktui_h - 7) / 2;
		if (out_rows < 2)
			out_rows = 2;
		int bt_y = 2 + out_rows + 2;
		int bt_rows = ktui_h - 2 - bt_y;

		au_clamp(&u, out_rows, bt_rows);
		au_draw(&u);

		KtuiEvent ev;
		int got = ktui_backend()->poll_event(&ev, 500);

		/* Both timers first: an event arriving does not make the device
		 * list stale, and a refresh per keystroke would open a mixer
		 * per card per keypress. */
		time_t now = time(NULL);
		if (au_bus)
			while (sd_bus_process(au_bus, NULL) > 0)
				;
		if (now - last_bt >= BT_REFRESH_S) {
			last_bt = now;
			au_bt_refresh();
		}
		if (now - last_dev >= 2) {
			last_dev = now;
			au_refresh();
		}
		if (ktui_resized) {
			ktui_resized = 0;
			ktui_draw_resize();
			ktui_draw_invalidate();
		}
		if (!got)
			continue;

		int *sel = &u.sel[u.pane];
		int n = u.pane == AU_PANE_OUT ? au_ndev : au_nbt;

		if (ev.type == KT_EVT_MOUSE) {
			int row = -1, pane = u.pane;
			if (ev.my >= 2 && ev.my < 2 + out_rows) {
				pane = AU_PANE_OUT;
				row = u.top[AU_PANE_OUT] + ev.my - 2;
			} else if (ev.my >= bt_y && ev.my < bt_y + bt_rows) {
				pane = AU_PANE_BT;
				row = u.top[AU_PANE_BT] + ev.my - bt_y;
			}
			int limit = pane == AU_PANE_OUT ? au_ndev : au_nbt;
			if (row < 0 || row >= limit)
				row = -1;

			/* Plain movement arrives as KT_MP_DRAG — libkwl's
			 * spelling, the same contract every other front end
			 * keeps. */
			if (ev.press == KT_MP_DRAG) {
				if (row >= 0) {
					u.pane = pane;
					u.sel[pane] = row;
				}
				continue;
			}
			if (ev.press != KT_MP_PRESS)
				continue;
			if (ev.btn == KT_MB_WHEEL_UP) {
				u.sel[u.pane]--;
				continue;
			}
			if (ev.btn == KT_MB_WHEEL_DOWN) {
				u.sel[u.pane]++;
				continue;
			}
			if (ev.btn == KT_MB_RIGHT)
				break;
			if (ev.btn != KT_MB_LEFT || row < 0)
				continue;
			/* A click on the row that is already selected activates
			 * it — pick.c's rule, so one hand learns one thing.
			 * Motion selects, so a plain first click would switch
			 * the output or drop a headset the pointer was merely
			 * crossing. */
			if (pane == u.pane && row == u.sel[pane]) {
				if (pane == AU_PANE_OUT)
					au_set_default(&au_dev[row], u.msg,
						       sizeof(u.msg));
				else
					au_bt_call(au_bt[row].path,
						   au_bt[row].connected
							   ? "Disconnect"
							   : "Connect");
			}
			u.pane = pane;
			u.sel[pane] = row;
			continue;
		}
		if (ev.type != KT_EVT_KEY)
			continue;

		switch (ev.key) {
		case KT_K_ESC:
			goto done;
		case KT_K_TAB:
			u.pane = u.pane == AU_PANE_OUT ? AU_PANE_BT
						       : AU_PANE_OUT;
			break;
		case KT_K_UP:
			(*sel)--;
			break;
		case KT_K_DOWN:
			(*sel)++;
			break;
		case KT_K_HOME:
			*sel = 0;
			break;
		case KT_K_END:
			*sel = n ? n - 1 : 0;
			break;
		case KT_K_LEFT:
		case KT_K_RIGHT:
			if (u.pane == AU_PANE_OUT && *sel < au_ndev &&
			    au_dev[*sel].vol >= 0)
				au_card_set_volume(&au_dev[*sel],
						   au_dev[*sel].vol +
						   (ev.key == KT_K_RIGHT ? 5
									 : -5));
			break;
		case KT_K_ENTER:
			if (u.pane == AU_PANE_OUT && *sel < au_ndev)
				au_set_default(&au_dev[*sel], u.msg,
					       sizeof(u.msg));
			else if (u.pane == AU_PANE_BT && *sel < au_nbt)
				au_bt_call(au_bt[*sel].path,
					   au_bt[*sel].connected ? "Disconnect"
								 : "Connect");
			break;
		case 'm':
			if (u.pane == AU_PANE_OUT && *sel < au_ndev)
				au_card_toggle(&au_dev[*sel]);
			break;
		case 's':
			if (!au_adapter[0])
				break;
			au_bt_call(au_adapter, au_bt_discovering
						       ? "StopDiscovery"
						       : "StartDiscovery");
			snprintf(u.msg, sizeof(u.msg), "%s",
				 au_bt_discovering ? "stopping scan"
						   : "scanning");
			break;
		case 'p':
			if (u.pane == AU_PANE_BT && *sel < au_nbt) {
				au_bt_call(au_bt[*sel].path, "Pair");
				/* Fire and forget: the answer arrives as a
				 * property change the next refresh reads. */
				snprintf(u.msg, sizeof(u.msg),
					 "pairing %s — check the device",
					 au_bt_label(&au_bt[*sel]));
			}
			break;
		default:
			break;
		}
	}

done:
	kwl_shutdown();
	if (au_bus)
		sd_bus_unref(au_bus);
	au_pw_free();
	au_mixer_close_all();
	return 0;
}
