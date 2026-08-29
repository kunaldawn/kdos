// SPDX-License-Identifier: GPL-2.0-only
/*
 * KDOS: per-box grants on the sandbox allowlist.
 *
 * labwc's `allow_for_sandbox()` is a fixed list: a client tagged through
 * kdos-boxsock gets surfaces, seat, dmabuf, text-input, primary selection and
 * nothing else — so a boxed application can never bind screencopy,
 * data-control, or an input method. That is the right DEFAULT, and it was
 * also the only answer: a screen recorder in a box of its own could not be
 * given the screen back, however deliberately.
 *
 * `grant = screencopy, data-control` in `~/.config/kdos/boxes/<name>.conf`
 * names the globals a box may bind beyond the allowlist. The names are
 * SHORT — what a person would write — and map onto both generations of each
 * protocol, because granting the old screencopy and not the new one strands
 * every client written after 2024. `input-method` is deliberately the one
 * grant that has to be spelled out in full: it is a keylogger by design.
 *
 * Read ONCE PER CLIENT, at the first bind that reaches it, and cached on the
 * app_id: the filter runs on every global for every client, and a profile
 * parse per bind would be a file read in the compositor's hot path.
 * `kdos_grant_reload()` drops the cache on SIGHUP with the rest of the
 * config, so editing a profile takes effect on the next client — a running
 * one keeps what it bound.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kdos.h"

#define GRANT_MAX 16
#define NAME_MAX_ 64

struct grant_row {
	const char *name;		/* what the profile says */
	const char *globals[3];		/* what it unlocks */
};

/* The map is the whole policy: a name that is not here cannot be granted. */
static const struct grant_row grants[] = {
	{ "screencopy", { "zwlr_screencopy_manager_v1",
			  "ext_image_copy_capture_manager_v1",
			  "ext_output_image_capture_source_manager_v1" } },
	{ "toplevel-capture", { "ext_foreign_toplevel_image_capture_source_manager_v1",
				NULL, NULL } },
	{ "export-dmabuf", { "zwlr_export_dmabuf_manager_v1", NULL, NULL } },
	{ "data-control", { "zwlr_data_control_manager_v1",
			    "ext_data_control_manager_v1", NULL } },
	{ "foreign-toplevel", { "zwlr_foreign_toplevel_manager_v1",
				"ext_foreign_toplevel_list_v1", NULL } },
	{ "layer-shell", { "zwlr_layer_shell_v1", NULL, NULL } },
	{ "input-method", { "zwp_input_method_manager_v2",
			    "zwp_virtual_keyboard_manager_v1", NULL } },
	{ "output-power", { "zwlr_output_power_manager_v1", NULL, NULL } },
};

struct cached {
	char box[NAME_MAX_];
	char names[GRANT_MAX][32];
	int n;
	int loaded;
};

static struct cached cache[8];
static int ncache;

static void
parse_profile(struct cached *c)
{
	const char *home = getenv("HOME");
	char path[512], line[512];
	FILE *f;

	c->loaded = 1;
	c->n = 0;
	if (!home)
		return;
	snprintf(path, sizeof(path), "%s/.config/kdos/boxes/%s.conf", home, c->box);
	f = fopen(path, "r");
	if (!f)
		return;
	while (fgets(line, sizeof(line), f)) {
		char *k = line, *eq, *v, *tok, *save;
		while (*k == ' ' || *k == '\t')
			k++;
		if (*k == '#' || !(eq = strchr(k, '=')))
			continue;
		*eq = 0;
		k[strcspn(k, " \t")] = 0;
		if (strcmp(k, "grant"))
			continue;
		v = eq + 1;
		for (tok = strtok_r(v, " ,\t\r\n", &save); tok && c->n < GRANT_MAX;
		     tok = strtok_r(NULL, " ,\t\r\n", &save)) {
			snprintf(c->names[c->n], sizeof(c->names[c->n]), "%s", tok);
			c->n++;
		}
		break;
	}
	fclose(f);
}

static struct cached *
lookup(const char *box)
{
	for (int i = 0; i < ncache; i++)
		if (!strcmp(cache[i].box, box))
			return &cache[i];
	if (ncache == (int)(sizeof(cache) / sizeof(cache[0])))
		ncache = 0;	/* a ring; eight boxes at once is generous */
	struct cached *c = &cache[ncache++];
	snprintf(c->box, sizeof(c->box), "%s", box);
	parse_profile(c);
	return c;
}

bool
kdos_box_grant(const char *box, const char *iface)
{
	if (!box || !*box || !iface)
		return false;
	struct cached *c = lookup(box);
	for (int i = 0; i < c->n; i++) {
		for (size_t g = 0; g < sizeof(grants) / sizeof(grants[0]); g++) {
			if (strcmp(c->names[i], grants[g].name))
				continue;
			for (int k = 0; k < 3 && grants[g].globals[k]; k++)
				if (!strcmp(iface, grants[g].globals[k]))
					return true;
		}
	}
	return false;
}

void
kdos_grant_reload(void)
{
	ncache = 0;
}
