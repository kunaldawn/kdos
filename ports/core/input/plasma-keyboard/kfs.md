## About

plasma-keyboard is Plasma's on-screen keyboard. It is a Wayland input method built on
[qt6-qtvirtualkeyboard](#port-qt6-qtvirtualkeyboard): KWin starts it when its virtual-keyboard
setting is on and shows it whenever a text field takes focus on a touch screen or when it is asked
for. The same program is the login screen's input method, so a machine with no physical keyboard
can still type a password. Its settings page, `kcm_plasmakeyboard`, picks the layouts and the key
feedback.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-D PLASMA_KEYBOARD_SOUND_ENABLED=ON` — Compile in the key-press click. The option upstream
  declares is `PLASMA_KEYBOARD_SOUNDS_ENABLED`, but `src/CMakeLists.txt` and the generated
  `config-plasma-keyboard.h` read `PLASMA_KEYBOARD_SOUND_ENABLED`, which nothing sets; left unset,
  the sound file is not built in, the program turns the setting off each time it starts, and the
  Sound box on the settings page does nothing. The click stays off until somebody ticks that box.
- `-D PLASMA_KEYBOARD_VIBRATION_ENABLED=OFF` — No haptic feedback. Vibration is a request to the
  `org.sigxcpu.Feedback` service (feedbackd) on the session bus, which is not a port; built in,
  and on by default in the settings, every key press would make a D-Bus call to a name nobody
  owns.

## Notes

It speaks the `input-method-v1` protocol and Qt Wayland's private client API, so it needs
[qt6-qtwayland](#port-qt6-qtwayland), [wayland](#port-wayland) and
[wayland-protocols](#port-wayland-protocols) at build time, and links Qt's private headers: a Qt
release change means rebuilding it with the rest of the Qt set.

The click is played by Qt Virtual Keyboard's own sound effect, which exists only where
[qt6-qtvirtualkeyboard](#port-qt6-qtvirtualkeyboard) is built with Qt Multimedia; without it the
Sound box is accepted and the keyboard stays silent.

The settings page has a media-centre layout that imports the Plasma Bigscreen shell's QML; Bigscreen
is not ported, and the desktop layout of the page is the one loaded.
