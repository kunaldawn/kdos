/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-theme — Konsole colour schemes and profiles
 *
 *   konsole <outdir> [accent]   KDOS-<Title>.colorscheme and
 *                               KDOS-<Title>.profile, per accent
 *
 * The sixteen colours are libkcolor's kcol_ansi(), the derivation `kdos
 * theme` writes into tmux, btop and the other terminal programs'
 * configurations too: a tmux status line drawn in one blue inside a terminal
 * whose blue is another reads as two palettes.
 *
 * A PROFILE IS FOUND BY ITS `Name`, and the light/dark keys in konsolerc
 * name profiles, not files — `DarkThemeProfile=KDOS Bone` and not
 * `KDOS-Bone.profile`. Each profile therefore carries the reading name and
 * the scheme it uses carries the file name, and neither is ever written from
 * the other by hand.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kdos-theme.h"

/* The monospace face of KDOS Modern, and its size in a terminal. The global
 * themes set the same face as kdeglobals `fixed`; a terminal reads one point
 * larger than a text field because a person reads it for longer. */
#define KTH_MONO_FONT	"JetBrains Mono"
#define KTH_TERM_PT	11

/* How far a faint (SGR 2) colour sits towards the background. Faint is
 * DECREASED INTENSITY by definition, so it is the one tone here that is not
 * held to a text floor. */
#define FAINT_MIX	40

/* ── the files ────────────────────────────────────────────────────────── */

static void colour(KbBuf *b, const char *group, uint32_t c)
{
	kb_buf_printf(b, "[%s]\nColor=", group);
	kth_rgb(b, c);
	kb_buf_str(b, "\n\n");
}

static void scheme(const KcolScheme *sc, const char *outdir)
{
	KcolAnsi a;
	KbBuf b = {0};
	char file[80], group[32];

	kcol_ansi(sc, &a);

	/* Groups in the byte order of their names, as Konsole writes a scheme
	 * it saves. */
	colour(&b, "Background", sc->deep);
	colour(&b, "BackgroundFaint", sc->deep);
	colour(&b, "BackgroundIntense", sc->variant);
	for (int i = 0; i < 8; i++) {
		snprintf(group, sizeof(group), "Color%d", i);
		colour(&b, group, a.normal[i]);
		snprintf(group, sizeof(group), "Color%dFaint", i);
		colour(&b, group, kcol_mix(a.normal[i], sc->deep, FAINT_MIX));
		snprintf(group, sizeof(group), "Color%dIntense", i);
		colour(&b, group, a.bright[i]);
	}
	colour(&b, "Foreground", sc->text);
	colour(&b, "ForegroundFaint", kcol_muted(sc));
	colour(&b, "ForegroundIntense", a.bright[7]);
	kb_buf_printf(&b,
		      "[General]\n"
		      "Blur=false\n"
		      "ColorRandomization=false\n"
		      "Description=KDOS %s\n"
		      "Opacity=1\n"
		      "Wallpaper=\n",
		      kth_title(sc));

	snprintf(file, sizeof(file), "%s.colorscheme", sc->theme_name);
	kth_emit(outdir, file, &b);
	kb_buf_free(&b);
}

/*
 * A block cursor in the accent with deep under it; a VISUAL bell, because a
 * terminal is where a bell rings forty times while a build fails and a
 * notification per ring is noise; ten thousand lines of scrollback.
 * BoldIntense draws bold in the bright half, which is what the bright ramp is
 * derived for.
 *
 * LIGATURES ARE OFF, and WordModeCoding is the switch. Konsole draws a run of
 * the characters programming ligatures are made of (`->`, `!=`, `<=`) in one
 * text call while it is on, its default, and JetBrains Mono then joins the
 * run into one glyph: `!=` reads as `≠` in a terminal, where what was typed is
 * what has to be seen. Off, every cell is drawn on its own.
 */
static void profile(const KcolScheme *sc, const char *outdir)
{
	KbBuf b = {0};
	char file[80];

	kb_buf_printf(&b,
		      "[Appearance]\n"
		      "BoldIntense=true\n"
		      "ColorScheme=%s\n"
		      "Font=%s,%d\n"
		      "WordModeCoding=false\n"
		      "\n"
		      "[Cursor Options]\n"
		      "CursorShape=0\n"
		      "CustomCursorColor=",
		      sc->theme_name, KTH_MONO_FONT, KTH_TERM_PT);
	kth_rgb(&b, sc->primary);
	kb_buf_str(&b, "\nCustomCursorTextColor=");
	kth_rgb(&b, sc->deep);
	kb_buf_printf(&b,
		      "\nUseCustomCursorColor=true\n"
		      "\n"
		      "[General]\n"
		      "Name=KDOS %s\n"
		      "Parent=FALLBACK/\n"
		      "\n"
		      "[Scrolling]\n"
		      "HistoryMode=1\n"
		      "HistorySize=10000\n"
		      "\n"
		      "[Terminal Features]\n"
		      "BellMode=2\n",
		      kth_title(sc));

	snprintf(file, sizeof(file), "%s.profile", sc->theme_name);
	kth_emit(outdir, file, &b);
	kb_buf_free(&b);
}

int kth_konsole(const char *outdir, const char *accent)
{
	const KcolScheme *pick[32];
	int n = kth_pick(accent, pick, 32);

	for (int i = 0; i < n; i++) {
		scheme(pick[i], outdir);
		profile(pick[i], outdir);
	}
	return 0;
}
