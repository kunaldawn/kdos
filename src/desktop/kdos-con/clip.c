/* kdos-con — the selection. See con.h.
 *
 * THE SESSION OWNS IT, because nothing else can. On Wayland the compositor
 * arbitrates between data devices; here kdos-con IS the server, so a copy is a
 * client handing over bytes and a paste is a client asking for them back.
 *
 * ONE CLIPBOARD AND ONE PRIMARY, and no ownership handshake. A Wayland source
 * stays alive to serve its own data, which is what makes a copy vanish when
 * the program that made it exits; a session that holds the bytes outlives the
 * program, which is what a person means by copying. The cost is that the
 * selection is text and only text — a picture on the clipboard would be a
 * payload this desktop has nowhere to put.
 */

#include <stdlib.h>
#include <string.h>

#include "con.h"

/*
 * CAPPED, AND WELL UNDER THE WIRE'S OWN LIMITS. libkcon refuses a payload
 * above a megabyte and drops a client whose queue passes four, so a selection
 * that reached either would be a copy that killed the window it came from.
 * Sixty-four kilobytes is a very long file name, a screenful of a terminal,
 * and a paragraph of prose several times over.
 */
#define CLIP_MAX (64u * 1024u)

static struct {
	char *text;
	size_t len;
} sel[2];			/* [0] clipboard, [1] primary */

static void clip_set(int primary, const char *text, size_t len)
{
	int i = primary ? 1 : 0;

	if (len > CLIP_MAX)
		len = CLIP_MAX;

	char *copy = NULL;

	if (text && len) {
		copy = malloc(len + 1);
		if (!copy)
			return;		/* the old selection stands */
		memcpy(copy, text, len);
		copy[len] = '\0';
	}

	free(sel[i].text);
	sel[i].text = copy;
	sel[i].len = copy ? len : 0;
}

/*
 * AND OUT TO EVERY VIEW THAT IS A TERMINAL. A view inside `foot`, or at the
 * far end of `ssh`, is a window on somebody's own desktop; a copy here that
 * did not reach that clipboard is one they cannot paste into their own editor.
 *
 * The CLIPBOARD only. The primary changes on every drag, and a desktop whose
 * clipboard was rewritten by every mouse gesture in a remote session is one
 * nobody would leave attached.
 */
static void clip_out(int primary)
{
	if (!primary && sel[0].text)
		kcon_view_clip(S.server, sel[0].text);
}

void clip_offer(KconSurface *f, const char *text, size_t len, int primary,
		void *user)
{
	(void)f;
	(void)user;
	clip_set(primary, text, len);
	clip_out(primary);
}

void clip_request(KconSurface *f, int primary, void *user)
{
	int i = primary ? 1 : 0;

	(void)user;
	if (!f)
		return;
	/*
	 * AN EMPTY SELECTION IS STILL AN ANSWER. A client that asked and heard
	 * nothing cannot tell a slow session from an empty clipboard, and
	 * would sit waiting for a paste that is never coming.
	 */
	kcon_surface_clip_data(f, sel[i].text ? sel[i].text : "");
}

/*
 * The session's own copy, for a terminal it runs itself: those windows are not
 * clients and have no socket to offer over.
 */
void clip_put(const char *text, size_t len, int primary)
{
	clip_set(primary, text, len);
	clip_out(primary);
}

const char *clip_get(int primary, size_t *len)
{
	int i = primary ? 1 : 0;

	if (len)
		*len = sel[i].len;
	return sel[i].text ? sel[i].text : "";
}

void clip_free(void)
{
	for (int i = 0; i < 2; i++) {
		free(sel[i].text);
		sel[i].text = NULL;
		sel[i].len = 0;
	}
}
