/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-settingsd — org.freedesktop.locale1
 *
 * THE SYSTEM LOCALE AND THE TWO KEYBOARDS. The locale is /etc/locale.conf,
 * which SDDM hands to the greeter and to every session it starts. The
 * console keymap is /etc/keymap, which kdos-getty loads on every console;
 * the graphical layout is /etc/kdos/xkb.conf, which reaches the login screen
 * through `kwin_wayland --locale1` and the session through startplasma.
 *
 * THE LOGIN SCREEN FOLLOWS A CHANGE AT ONCE. Its compositor listens for
 * PropertiesChanged on this object and reloads the layout when it hears one,
 * so every keyboard method announces all six keyboard properties: an empty
 * stored layout follows the console keymap, and a console change can
 * therefore be a layout change too.
 *
 * `convert` IS systemd's: SetX11Keyboard with it also sets the nearest
 * console keymap, SetVConsoleKeyboard with it the nearest layout, so a
 * settings page sets one and both follow. A layout with no console
 * equivalent leaves the console as it was.
 * ---------------------------------
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kbase.h"
#include "bus.h"

#define A_LOCALE   "org.freedesktop.locale1.set-locale"
#define A_KEYBOARD "org.freedesktop.locale1.set-keyboard"

static int bad_args(sd_bus_error *err, const char *what)
{
	return sd_bus_error_setf(err, SD_BUS_ERROR_INVALID_ARGS, "%s", what);
}

static void emit_keyboard(void)
{
	sd_bus_emit_properties_changed(ks_bus(), KS_LOCALE_PATH, KS_LOCALE_IF,
				       "VConsoleKeymap", "VConsoleKeymapToggle",
				       "X11Layout", "X11Model", "X11Variant",
				       "X11Options", NULL);
}

/* The console now, as well as at the next login on each one. A keymap that
 * fails to load here is still the setting: the file is what kdos-getty
 * reads, and the call has written it. */
static void load_keymap(const char *name)
{
	int r = *name ? ks_run("loadkeys", "-q", name, NULL)
		      : ks_run("loadkeys", "-q", "-d", NULL);

	if (r != 0)
		fprintf(stderr, "kdos-settingsd: loadkeys %s: %d\n",
			*name ? name : "-d", r);
}

/* ── properties ──────────────────────────────────────────────────────────── */

static int get_locale(sd_bus *bus, const char *path, const char *iface,
		      const char *prop, sd_bus_message *reply, void *userdata,
		      sd_bus_error *err)
{
	char vals[KS_LOCALE_KEYS][KS_ASSIGN_MAX];
	char *v[KS_LOCALE_KEYS + 1];
	int n = ks_locale_get(vals);

	(void)bus, (void)path, (void)iface, (void)prop, (void)userdata;
	(void)err;
	for (int i = 0; i < n; i++)
		v[i] = vals[i];
	v[n] = NULL;
	return sd_bus_message_append_strv(reply, v);
}

static int get_keyboard(sd_bus *bus, const char *path, const char *iface,
			const char *prop, sd_bus_message *reply,
			void *userdata, sd_bus_error *err)
{
	char km[KS_NAME_MAX];
	KsXkb x;

	(void)bus, (void)path, (void)iface, (void)userdata, (void)err;
	if (!strcmp(prop, "VConsoleKeymap")) {
		ks_keymap(km, sizeof(km));
		return sd_bus_message_append(reply, "s", km);
	}
	if (!strcmp(prop, "VConsoleKeymapToggle"))
		return sd_bus_message_append(reply, "s", "");
	ks_xkb_effective(&x);
	return sd_bus_message_append(reply, "s",
		!strcmp(prop, "X11Layout")  ? x.layout :
		!strcmp(prop, "X11Model")   ? x.model :
		!strcmp(prop, "X11Variant") ? x.variant : x.options);
}

/* ── SetLocale ───────────────────────────────────────────────────────────── */

/* The assignments as values by key, the shape ks_locale_set takes. */
static void locale_now(char vals[KS_LOCALE_KEYS][KS_NAME_MAX])
{
	char cur[KS_LOCALE_KEYS][KS_ASSIGN_MAX];
	int n = ks_locale_get(cur), k;
	bool empty;

	memset(vals, 0, sizeof(char[KS_LOCALE_KEYS][KS_NAME_MAX]));
	for (int i = 0; i < n; i++)
		if (ks_locale_assign_ok(cur[i], &k, &empty) && !empty)
			kb_strlcpy(vals[k], strchr(cur[i], '=') + 1,
				   KS_NAME_MAX);
}

static int apply_locale(KsAuth *a, sd_bus_error *err)
{
	char vals[KS_LOCALE_KEYS][KS_NAME_MAX];
	int k;
	bool empty;

	memset(vals, 0, sizeof(vals));
	for (char *l = a->text, *nl; l && *l; l = nl ? nl + 1 : NULL) {
		nl = strchr(l, '\n');
		if (nl)
			*nl = 0;
		if (ks_locale_assign_ok(l, &k, &empty) && !empty)
			kb_strlcpy(vals[k], strchr(l, '=') + 1, KS_NAME_MAX);
	}
	if (ks_locale_set(vals) != 0)
		return sd_bus_error_setf(err, SD_BUS_ERROR_FAILED,
					 "cannot write /etc/locale.conf: %s",
					 strerror(errno));
	sd_bus_emit_properties_changed(ks_bus(), KS_LOCALE_PATH, KS_LOCALE_IF,
				       "Locale", NULL);
	return 0;
}

/*
 * THE NEW LIST REPLACES THE OLD ONE WHOLE, as systemd-localed's does: a
 * variable the call leaves out is no longer set, and `NAME=` with nothing
 * after it says the same thing out loud.
 */
static int m_set_locale(sd_bus_message *m, void *u, sd_bus_error *err)
{
	char want[KS_LOCALE_KEYS][KS_NAME_MAX], now[KS_LOCALE_KEYS][KS_NAME_MAX];
	bool seen[KS_LOCALE_KEYS] = { false }, empty;
	char **v = NULL;
	KbBuf text = { 0 };
	int inter, r, k = 0;
	KsAuth *a;

	(void)u;
	r = sd_bus_message_read_strv(m, &v);
	if (r < 0)
		return r;
	r = sd_bus_message_read(m, "b", &inter);
	memset(want, 0, sizeof(want));
	for (char **s = v; r >= 0 && s && *s; s++) {
		if (!ks_locale_assign_ok(*s, &k, &empty))
			r = bad_args(err, "not a locale assignment this "
					  "interface carries");
		else if (seen[k])
			r = bad_args(err, "a variable is assigned twice");
		else if (!empty)
			kb_strlcpy(want[k], strchr(*s, '=') + 1, KS_NAME_MAX);
		if (r >= 0)
			seen[k] = true;
	}
	kb_strv_free(v);
	if (r < 0)
		return r;

	locale_now(now);
	if (!memcmp(want, now, sizeof(want)))
		return sd_bus_reply_method_return(m, NULL);
	for (int i = 0; i < KS_LOCALE_KEYS; i++)
		if (want[i][0])
			kb_buf_printf(&text, "%s=%s\n", ks_locale_key(i),
				      want[i]);
	a = ks_auth_new(m, apply_locale, inter);
	a->text = text.p;
	return ks_auth_run(a, A_LOCALE);
}

/* ── the console keymap ─────────────────────────────────────────────────── */

static int apply_vconsole(KsAuth *a, sd_bus_error *err)
{
	const char *km = a->s[0];
	KsXkb x;

	if (ks_set_keymap(km) != 0)
		return sd_bus_error_setf(err, SD_BUS_ERROR_FAILED,
					 "cannot write /etc/keymap: %s",
					 strerror(errno));
	load_keymap(km);
	if (a->b[0] && *km) {
		ks_xkb_stored(&x);
		if (ks_vt_to_xkb(km, x.layout, sizeof(x.layout), x.variant,
				 sizeof(x.variant)) &&
		    ks_set_xkb(&x) != 0) {
			emit_keyboard();
			return sd_bus_error_setf(err, SD_BUS_ERROR_FAILED,
						 "cannot write /etc/kdos/xkb.conf: %s",
						 strerror(errno));
		}
	}
	emit_keyboard();
	return 0;
}

static int m_set_vconsole(sd_bus_message *m, void *u, sd_bus_error *err)
{
	const char *km, *toggle;
	char cur[KS_NAME_MAX];
	int conv, inter, r;
	KsAuth *a;

	(void)u;
	r = sd_bus_message_read(m, "ssbb", &km, &toggle, &conv, &inter);
	if (r < 0)
		return r;
	/* kdos-getty loads one map; a toggle map would be stored and never
	 * loaded, which is a setting that does nothing. */
	if (*toggle)
		return sd_bus_error_setf(err, SD_BUS_ERROR_NOT_SUPPORTED,
					 "the console loads one keymap; a "
					 "toggle keymap is not supported");
	if (*km && !ks_keymap_name_ok(km))
		return bad_args(err, "not a console keymap name");
	if (*km && !ks_keymap_installed(km))
		return bad_args(err, "no such console keymap is installed");
	ks_keymap(cur, sizeof(cur));
	if (!strcmp(cur, km) && !conv)
		return sd_bus_reply_method_return(m, NULL);
	a = ks_auth_new(m, apply_vconsole, inter);
	kb_strlcpy(a->s[0], km, sizeof(a->s[0]));
	a->b[0] = !!conv;
	return ks_auth_run(a, A_KEYBOARD);
}

/* ── the XKB layout ─────────────────────────────────────────────────────── */

static void xkb_from(KsAuth *a, KsXkb *x)
{
	kb_strlcpy(x->layout, a->s[0], sizeof(x->layout));
	kb_strlcpy(x->model, a->s[1], sizeof(x->model));
	kb_strlcpy(x->variant, a->s[2], sizeof(x->variant));
	kb_strlcpy(x->options, a->s[3], sizeof(x->options));
}

static int apply_x11(KsAuth *a, sd_bus_error *err)
{
	char km[KS_NAME_MAX], cur[KS_NAME_MAX];
	KsXkb x;

	xkb_from(a, &x);
	if (ks_set_xkb(&x) != 0)
		return sd_bus_error_setf(err, SD_BUS_ERROR_FAILED,
					 "cannot write /etc/kdos/xkb.conf: %s",
					 strerror(errno));
	if (a->b[0] && ks_xkb_to_vt(x.layout, x.variant, km, sizeof(km))) {
		ks_keymap(cur, sizeof(cur));
		if (strcmp(cur, km)) {
			if (ks_set_keymap(km) != 0) {
				emit_keyboard();
				return sd_bus_error_setf(err, SD_BUS_ERROR_FAILED,
							 "cannot write /etc/keymap: %s",
							 strerror(errno));
			}
			load_keymap(km);
		}
	}
	emit_keyboard();
	return 0;
}

static int m_set_x11(sd_bus_message *m, void *u, sd_bus_error *err)
{
	const char *layout, *model, *variant, *options, *why;
	int conv, inter, r;
	KsXkb x, cur;
	KsAuth *a;

	(void)u;
	r = sd_bus_message_read(m, "ssssbb", &layout, &model, &variant,
				&options, &conv, &inter);
	if (r < 0)
		return r;
	if (strlen(layout) >= KS_XKB_MAX || strlen(model) >= KS_XKB_MAX ||
	    strlen(variant) >= KS_XKB_MAX || strlen(options) >= KS_XKB_MAX)
		return bad_args(err, "an XKB field is too long");
	kb_strlcpy(x.layout, layout, sizeof(x.layout));
	kb_strlcpy(x.model, model, sizeof(x.model));
	kb_strlcpy(x.variant, variant, sizeof(x.variant));
	kb_strlcpy(x.options, options, sizeof(x.options));
	why = ks_xkb_check(&x);
	if (why)
		return bad_args(err, why);
	ks_xkb_stored(&cur);
	if (!conv && !strcmp(x.layout, cur.layout) &&
	    !strcmp(x.model, cur.model) && !strcmp(x.variant, cur.variant) &&
	    !strcmp(x.options, cur.options))
		return sd_bus_reply_method_return(m, NULL);
	a = ks_auth_new(m, apply_x11, inter);
	kb_strlcpy(a->s[0], layout, sizeof(a->s[0]));
	kb_strlcpy(a->s[1], model, sizeof(a->s[1]));
	kb_strlcpy(a->s[2], variant, sizeof(a->s[2]));
	kb_strlcpy(a->s[3], options, sizeof(a->s[3]));
	a->b[0] = !!conv;
	return ks_auth_run(a, A_KEYBOARD);
}

#define M(name, sig, fn) \
	SD_BUS_METHOD(name, sig, NULL, fn, SD_BUS_VTABLE_UNPRIVILEGED)
#define KB(name) SD_BUS_PROPERTY(name, "s", get_keyboard, 0, \
				 SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE)

static const sd_bus_vtable locale_vtable[] = {
	SD_BUS_VTABLE_START(0),
	SD_BUS_PROPERTY("Locale", "as", get_locale, 0,
			SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
	KB("X11Layout"),
	KB("X11Model"),
	KB("X11Variant"),
	KB("X11Options"),
	KB("VConsoleKeymap"),
	KB("VConsoleKeymapToggle"),
	M("SetLocale", "asb", m_set_locale),
	M("SetVConsoleKeyboard", "ssbb", m_set_vconsole),
	M("SetX11Keyboard", "ssssbb", m_set_x11),
	SD_BUS_VTABLE_END
};

int ks_locale_add(sd_bus *bus)
{
	return sd_bus_add_object_vtable(bus, NULL, KS_LOCALE_PATH,
					KS_LOCALE_IF, locale_vtable, NULL);
}
