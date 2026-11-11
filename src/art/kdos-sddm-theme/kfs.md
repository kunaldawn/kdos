## About

`kdos-sddm-theme` is the KDOS login screen, `/usr/share/sddm/themes/kdos`. It is not written from
scratch: the build copies the Breeze SDDM theme that [plasma-desktop](#port-plasma-desktop)
installs and applies one patch to it, so the login screen and the lock screen are drawn by the
same Breeze components and share one layout. Colours, the Plasma style and the cursor come from the
`sddm` user's `kdeglobals`, `plasmarc` and `kcminputrc`, copied from
[kdos-plasma-look](#port-kdos-plasma-look)'s `/usr/share/kdos/greeter/<accent>/` for the machine's
accent: by the image's `050_theme.sh` for the default accent, by the installer, and by
`kdos-powerd` when `kdos theme` runs. The one colour the patch names is the black of a screen with
nothing on it. Breeze's `color=` line is removed from `theme.conf`, and the background's colour is
the scheme's Window ground: the Login Screen page's Clear Image writes `type=color` to
`theme.conf.user`, and the greeter then shows the accent's ground rather than a colour no KDOS
palette made.

## Flags

- `cp -a /usr/share/sddm/themes/breeze/. .` — The installed Breeze theme, from the plasma-desktop
  in the tree. Deriving from the installed copy is what makes an upstream change to the Breeze
  greeter a build failure here instead of a broken login screen.
- `rm -f preview.png default-logo.svg Messages.sh KeyboardButton.qml` — Breeze's screenshot, which
  the login-screen settings page would show as this theme's; the Plasma logo, which the patched
  `theme.conf` does not name; a translation-extraction script that does nothing on an installed
  system; and Breeze's layout button, which the patched footer replaces.
- `python3 "$PORT_SRC/genpreview.py"` — The theme's `preview.png`, drawn for the default accent
  (`kdos-theme accents --default`), the picture System Settings → Login Screen shows for
  it (`Screenshot=preview.png` in the metadata). `genpreview.py` draws a schematic of the greeter
  with [python3-pillow](#port-python3-pillow): the default accent's greeter background, cropped and
  blurred as the greeter shows it, the clock and date in the set's fixed-width face (resolved by
  `fc-match`), the user's circle, the password field and the credit, every colour read from the
  set's `kdeglobals`. The time and date are a fixed sample, so the file is the same at every
  build. The script is beside the recipe and not installed.
- `install -m644 "$PORT_SRC/sddm-theme.desktop" metadata.desktop` — The theme's own name and id,
  `kdos`, in place of Breeze's. It is a whole file rather than a hunk of the patch because the
  upstream file is mostly translations of the word Breeze, which change every release. Beside the
  recipe it is not called `metadata.desktop`, which would make the port directory itself look
  like an installable theme with no `Main.qml` to every check that looks for one.

## Notes

`kdos-sddm.patch` makes the Breeze greeter the KDOS one. `theme.conf` shows the KDOS mark
(`distributor-logo-kdos` from [kdos-icons](#port-kdos-icons)) and takes its background from
`/var/lib/sddm/kdos-background.png`, the accent's dark wallpaper copied with the rest of the set;
the scheme's Window ground is drawn under it, so a greeter that finds no such file shows the
accent's ground instead of an empty window. The clock is set in the fixed-width face (a new
`MonoClock.qml`, Breeze's clock with the family changed, and the time's no-break spaces made plain
ones: `en_US` and other locales put U+202F before AM or PM, JetBrains Mono has no glyph for it,
and the fallback face's taller line pushes the date down), the footer carries "KDOS · Plasma by
KDE", Meta+Alt+S starts Orca as it does in a session (the greeter's KWin runs without global
shortcuts, so the greeter binds it), and the screen fades in out of black and back to black on a
login, so the mode set either side of the greeter falls inside a black frame. It is KDOS's own and
not for upstream.

**The lock screen as the login.** On a machine that logs in automatically into a locked session,
the stock Breeze lock screen is the first thing shown, and it is drawn from the same
`org.kde.breeze.components` as this greeter: the same clock position, user box and footer, on the
same accent's wallpaper (`kscreenlockerrc` names the accent's wallpaper package, whose dark image
is the greeter's background). Changing the layout here and not there would make the two screens a
person types the same password into look different, so the patch changes only the mark, the
clock's face, the footer and the background, never the layout.

**The footer's two keyboard buttons work over the greeter's session bus.** The Virtual Keyboard
button is Breeze's: `Keyboards.KWinVirtualKeyboard.forceActivate()`, a D-Bus call to the
greeter's KWin, which shows plasma-keyboard (KWin otherwise shows it only after touch or pen
input). The layout button is the lock screen's (Plasma's `loginlockscreen/Footer.qml`): a
`ToolButton` over `org.kde.plasma.workspace.components`' `KeyboardLayoutSwitcher`, which reads and
switches KWin's layouts through `org.kde.keyboard`, shown when locale1 names more than one. It
replaces Breeze's `KeyboardButton.qml`, whose list comes from SDDM's own keyboard model, which
SDDM's Wayland keyboard backend leaves empty, so that button never shows on a Wayland greeter.
The bus is the one user service turnstile's `ksvc` backend starts for the `sddm` account.

The greeter always shows a dark accent: SDDM reads no light, dark or time-of-day setting, so for
the light accent the greeter files are its dark partner's. Which theme SDDM uses is `[Theme]
Current=` in `/etc/sddm.conf.d/00-kdos.conf`, not this package. The Breeze theme this copies is
installed by plasma-desktop's `INSTALL_SDDM_THEME`, on by default; a plasma-desktop built with it
off fails this port at its first line.

Meta+Alt+S starts Orca, and the greeter's session bus brings up the AT-SPI bus and speech-dispatcher
with it, but the `sddm` account runs no PipeWire and ALSA's default device is PipeWire's, so Orca
says nothing at the greeter. The command is the fixed string `orca --replace`, run through
Plasma5Support's executable engine, whose KProcess splits a string with no shell syntax into a
program and its arguments and starts it with no shell; nothing typed at the greeter reaches it.