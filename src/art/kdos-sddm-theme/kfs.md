## About

`kdos-sddm-theme` is the KDOS login screen, `/usr/share/sddm/themes/kdos`. It is not written from
scratch: the build copies the Breeze SDDM theme that [plasma-desktop](#port-plasma-desktop)
installs and applies one patch to it, so the login screen and the lock screen are drawn by the
same Breeze components and share one layout. Colours, the Plasma style and the cursor come from the
`sddm` user's `kdeglobals`, `plasmarc` and `kcminputrc`, which `kdos-powerd` copies from
[kdos-plasma-look](#port-kdos-plasma-look)'s `/usr/share/kdos/greeter/<accent>/` for the machine's
accent. The one colour the patch names is the black of a screen with nothing on it; Breeze's
`color=` stays in `theme.conf` and is read only by a `type=color` background, which this theme
does not set.

## Flags

- `cp -a /usr/share/sddm/themes/breeze/. .` — The installed Breeze theme, from the plasma-desktop
  in the tree. Deriving from the installed copy is what makes an upstream change to the Breeze
  greeter a build failure here instead of a broken login screen.
- `rm -f preview.png default-logo.svg Messages.sh` — Breeze's screenshot, which the login-screen
  settings page would show as this theme's; the Plasma logo, which the patched `theme.conf` does
  not name; and a translation-extraction script that does nothing on an installed system.
- `install -m644 "$PORT_SRC/sddm-theme.desktop" metadata.desktop` — The theme's own name and id,
  `kdos`, in place of Breeze's. It is a whole file rather than a hunk of the patch because the
  upstream file is mostly translations of the word Breeze, which change every release. Beside the
  recipe it is not called `metadata.desktop`, which would make the port directory itself look
  like an installable theme with no `Main.qml` to every check that looks for one.

## Notes

`kdos-sddm.patch` makes the Breeze greeter the KDOS one. `theme.conf` shows the KDOS mark
(`distributor-logo-kdos` from [kdos-icons](#port-kdos-icons)) and takes its background from
`/var/lib/sddm/kdos-background.png`, the accent's dark wallpaper that `kdos-powerd` writes; the
scheme's Window ground is drawn under it, so a greeter that starts before that file exists shows
the accent's ground instead of an empty window. The clock is set in the fixed-width face (a new
`MonoClock.qml`, Breeze's clock with the family changed), the footer carries "KDOS · Plasma by
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

The greeter always shows a dark accent: SDDM reads no light, dark or time-of-day setting, so for
the light accent the greeter files are its dark partner's. Which theme SDDM uses is `[Theme]
Current=` in `/etc/sddm.conf.d/00-kdos.conf`, not this package. The Breeze theme this copies is
installed by plasma-desktop's `INSTALL_SDDM_THEME`, on by default; a plasma-desktop built with it
off fails this port at its first line.

Meta+Alt+S needs a session bus and the AT-SPI bus in the greeter, or Orca starts and hears
nothing. The command is the fixed string `orca --replace`, run through Plasma5Support's
executable engine, whose KProcess splits a string with no shell syntax into a program and its
arguments and starts it with no shell; nothing typed at the greeter reaches it.
