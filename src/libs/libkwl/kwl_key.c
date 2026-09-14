/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   xkb keysyms -> libktui keys
 *
 * libktui's key vocabulary was defined by what a TERMINAL sends: a codepoint,
 * or one of a couple of dozen KT_K_* specials that an escape sequence decodes
 * to. Here the source is xkb, and the job is to arrive at exactly the same
 * values so that every widget written against the tty behaves identically —
 * a Tab is 9 in both, and Ctrl+C is the letter `c` with KT_MOD_CTRL in both.
 */

#include <xkbcommon/xkbcommon.h>

#include "kwl_priv.h"

int kwl_keysym_to_ktui(xkb_keysym_t sym, struct xkb_state *state,
		       xkb_keycode_t code)
{
	switch (sym) {
	case XKB_KEY_Escape:	return KT_K_ESC;
	case XKB_KEY_Return:
	case XKB_KEY_KP_Enter:	return KT_K_ENTER;
	case XKB_KEY_Tab:	return KT_K_TAB;
	/* Shift+Tab is its own keysym, and libktui has its own key for it —
	 * a terminal sends CSI Z for this, not a modified Tab. */
	case XKB_KEY_ISO_Left_Tab: return KT_K_BTAB;
	case XKB_KEY_BackSpace:	return KT_K_BACKSPACE;
	case XKB_KEY_Up:	return KT_K_UP;
	case XKB_KEY_Down:	return KT_K_DOWN;
	case XKB_KEY_Left:	return KT_K_LEFT;
	case XKB_KEY_Right:	return KT_K_RIGHT;
	case XKB_KEY_Home:	return KT_K_HOME;
	case XKB_KEY_End:	return KT_K_END;
	case XKB_KEY_Page_Up:	return KT_K_PGUP;
	case XKB_KEY_Page_Down:	return KT_K_PGDN;
	case XKB_KEY_Insert:	return KT_K_INS;
	case XKB_KEY_Delete:	return KT_K_DEL;
	/*
	 * The keypad with NumLock off. xkb hands these out as their own
	 * keysyms — with NumLock on the digits come through get_utf32 below —
	 * and without the mapping half the keyboard's navigation keys did
	 * nothing at all. (KP_Prior/KP_Next ARE KP_Page_Up/KP_Page_Down: same
	 * keysym value, so only one spelling can appear as a case label.)
	 */
	case XKB_KEY_KP_Up:	return KT_K_UP;
	case XKB_KEY_KP_Down:	return KT_K_DOWN;
	case XKB_KEY_KP_Left:	return KT_K_LEFT;
	case XKB_KEY_KP_Right:	return KT_K_RIGHT;
	case XKB_KEY_KP_Home:	return KT_K_HOME;
	case XKB_KEY_KP_End:	return KT_K_END;
	case XKB_KEY_KP_Prior:	return KT_K_PGUP;
	case XKB_KEY_KP_Next:	return KT_K_PGDN;
	case XKB_KEY_KP_Insert:	return KT_K_INS;
	case XKB_KEY_KP_Delete:	return KT_K_DEL;
	case XKB_KEY_F1:	return KT_K_F1;
	case XKB_KEY_F2:	return KT_K_F2;
	case XKB_KEY_F3:	return KT_K_F3;
	case XKB_KEY_F4:	return KT_K_F4;
	case XKB_KEY_F5:	return KT_K_F5;
	case XKB_KEY_F6:	return KT_K_F6;
	case XKB_KEY_F7:	return KT_K_F7;
	case XKB_KEY_F8:	return KT_K_F8;
	case XKB_KEY_F9:	return KT_K_F9;
	case XKB_KEY_F10:	return KT_K_F10;
	case XKB_KEY_F11:	return KT_K_F11;
	case XKB_KEY_F12:	return KT_K_F12;
	default:		break;
	}

	/*
	 * A COMPOSED KEYSYM IS TRANSLATED BY KEYSYM, not by keycode.
	 *
	 * The compose machine replaces `sym` with what the sequence produced —
	 * `'` then `e` becomes XKB_KEY_eacute — and the key that was actually
	 * pressed is still `e`. Asking xkb what the KEYCODE means answers `e`,
	 * so every accented character typed through a dead key came out as its
	 * base letter and the compose table did nothing at all.
	 */
	if (sym != xkb_state_key_get_one_sym(state, code)) {
		uint32_t composed = xkb_keysym_to_utf32(sym);

		return composed ? (int)composed : 0;
	}

	uint32_t cp = xkb_state_key_get_utf32(state, code);
	if (!cp)
		return 0;	/* a bare modifier, or a key with no text */

	/*
	 * ONE VOCABULARY FOR A CTRL CHORD: the letter, with KT_MOD_CTRL in
	 * `mods`. xkb folds Ctrl into a control code here and neither of the
	 * other two backends does — libktui's terminal decoder unfolds 0x03
	 * back to `c`, and libkkms reads the keysym, which xkb never folds —
	 * so a chord written against either of those misses every Ctrl chord
	 * under Wayland. The fold is undone rather than propagated, because
	 * the event already carries the modifier.
	 */
	if (cp < 0x20) {
		uint32_t plain = xkb_keysym_to_utf32(sym);

		if (plain)
			return (int)plain;
	}

	return (int)cp;
}
