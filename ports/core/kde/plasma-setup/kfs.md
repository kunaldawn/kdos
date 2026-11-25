## About

Plasma Setup is the first-boot wizard for a machine handed over without an account (KDOS's
"Let the first person set it up" install). On that boot the login manager signs in a special
`plasma-setup` user with no password, whose session runs only the wizard: it creates the first
real account in the groups KDOS gives its own account, and sets the keyboard layout, time zone, host name and
Wi-Fi network, then writes `/etc/plasma-setup-done` so it never runs again and logs the new user
in.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

- `-D KDE_INSTALL_CONFDIR=/usr/share/kdos/xdg` — Install `plasmasetuprc` into
  `/usr/share/kdos/xdg`, where every KDOS default lives, instead of `/etc/xdg`, which is the
  machine owner's and which no package ships into (see
  [Session](../../../../docs/kdos/03-architecture/session.md)). Plasma Setup reads the file by the
  absolute path this directory compiles into `PLASMA_SETUP_CONFIG_PATH`, not through
  `XDG_CONFIG_DIRS`, so the setting moves the file and the path together. Nothing else of this port
  installs into the configuration directory.

## Patches

`plasmasetuprc-kdos-groups.patch` sets `[Accounts] UserGroups` to the groups the image gives its own
account in `fs/etc/group` (`wheel,users,audio,video,render,input,kvm,cdrom,dialout,lpadmin,tty`)
instead of `wheel` alone. An OEM install's first account is made by Plasma Setup's helper with
`useradd -m -U`, which gives it `/etc/default/useradd`'s `GROUPS=audio`, and then `usermod -a -G`
with this list; an account made by the installer keeps the live account's memberships, so the two
would otherwise differ: without `kvm` a box gets no hardware virtualisation, without `lpadmin`
the printer pages ask for an administrator. Every group named must exist in `/etc/group`, or
`useradd` refuses the account; the list follows `fs/etc/group`'s `kdos` lines. It is not proposed
upstream: the file is upstream's documented place for a distribution's choice.

`plasma-setup-kdos-firstboot.patch` makes ten changes, and its code is to be offered upstream. The
keyboard page's two lists sort through a new `LayoutVariantSortModel` over upstream's filters. The
layout list, while a search is applied, sorts by the fuzzy-match score descending, best match first,
and otherwise, and between equal scores, by description, A to Z, as the keyboard settings' Add
Layout dialog does. Every score is 0 before a search (an empty pattern matches everything at score
0), so the score gives no order until a search is applied; and the layout model's own order is the
XKB rules files', which is not alphabetical (*Armenian* before *Arabic*, the extra layouts after
*Wolof*). Upstream sorts that list by the score ascending, so a search for *German* lists plain
*German* last. The variant list puts the layout's own row without a variant first, then the variants
by description, and while a search is applied by its match score, best first, with equal scores by
description. Upstream sorts that list by the fuzzy-match score, ascending, so a search puts the best
match last: after a search for *German* the list opens on *Romanian (Germany, no dead keys)* and
plain *German* is last, where a click on the first row picks the wrong layout. The model's
`searching` property follows the search string the layout search model holds, which the field sets
when Enter is pressed, and its `plainFirst` property is set for the variant list alone. After any
re-sort or re-filter of either list, and after a click, both lists highlight the layout and variant
in use (the system's current layout, which a click replaces), scrolled into view, and nothing when a
search hides that layout, because a ListView keeps its row number across a re-sort or re-filter and
would highlight whichever row moved into that row number while another stays applied; a click
re-filters the variant list, so without that the plain *German* row stays unhighlighted after
*German* is clicked. Arrow keys still move the highlight without applying it, and the variant list
of a layout reached that way highlights nothing. The keyboard page's subtitle reads *Select your
keyboard layout.*, since the page has no language control, and its search field shows no *Ctrl+F*
hover tooltip, since the page focuses the field itself and the tooltip opens over the subtitle. The
landing page takes its background from the wallpaper package named by `[Appearance] Wallpaper` in
`plasmasetuprc` (default `Next`): of the images named `WxH` in the screen's orientation, the one
with the longest short side, from `images_dark` first when the active colour scheme's window
background is dark by Kirigami's `ColorUtils.brightnessForColor`. Upstream asks the Dark Theme
page's `PrepareUtil.usingDarkTheme`, which is true only for the scheme named `BreezeDark`, so every
KDOS scheme would show the light image behind the wizard's headline; and it names `Next`'s own
`5120x2880.png` and `1440x2960.png`, which no KDOS wallpaper carries. And the landing page's
headline is `[Appearance] Welcome` when that key is set, instead of *Welcome to Plasma Desktop*. The
headline and the *Begin Setup* button sit on a scrim, a rounded panel of the colour scheme's window
background at 85% opacity padded by twice `Kirigami.Units.largeSpacing`, and the headline takes the
scheme's text colour instead of white: upstream's white text over a wallpaper reads only where the
image is dark, and the KDOS mascot's light belly runs through the letters. The Account page's avatar
is a plain kirigami-addons `Avatar`, a display element, in the colour scheme's highlight colour, the
accent, instead of upstream's disabled `AvatarButton` in a colour hashed from the typed name that
has nothing to do with the scheme. The colour has to be resolved outside a disabled button:
`AvatarButton` takes the `Button` colour set, and its disabled group's highlight on a KDOS scheme is
the window background, so the initials vanish once a name is typed. And the Time page preselects the
machine's time zone only when the selector's map shows it. A zone with no place on the map, such as
`UTC`, raises the selector's *The currently selected time zone is not available for visualization on
this map* banner, and its location box then looks up a region the zone has none of and throws; left
unset, the region box reads *Choose…* and the machine keeps its zone until one is picked.

The same patch sets KDOS's values in `plasmasetuprc`: `Wallpaper=KDOS-Bone`, the default accent's
wallpaper (an install in another accent still shows Bone's here), and `Welcome=Welcome to KDOS` in
the upstream headline's two-line form. Those two lines are KDOS's choice, not part of the offer.

## Notes

`build.sh` removes two of the wizard's pages, which are KPackages the wizard finds by listing
`/usr/share/plasma/packages`: `org.kde.plasmasetup.language`, whose list holds the one language
the image ships, and `org.kde.plasmasetup.prepare` (*Before we get started*), whose only control is
a Dark Theme switch that applies the Breeze or Breeze Dark global theme over the KDOS accent. The
`prepareutil` QML module that page used stays installed and is loaded by nothing. The wizard's pages are Keyboard,
Account, Host name, Time and Wi-Fi, then Finished.

Upstream arms the wizard with three systemd pieces, and `build.sh` deletes all three:
`plasma-setup.service` under `/usr/lib/systemd/system`, the `plasma-setup` user in
`/usr/lib/sysusers.d`, and the user's home in `/usr/lib/tmpfiles.d`. KDOS has no systemd and kpkg's
package lint refuses `usr/lib/systemd` (see
[No systemd](../../../../docs/kdos/01-philosophy/principles.md#no-systemd-init-units-or-service-manager)).
Their work is done by a boot script instead, before the login manager starts and only while
`/etc/plasma-setup-done` is absent and the system is not the live medium: it provides the
`plasma-setup` account, builds its home in `/run/plasma-setup` (the autostart entry for the wizard,
the global-shortcut allow-list from `/usr/share/plasma-setup/kglobalaccelrc`, and the settings that
turn off the screen locker, the wallet, the welcome tour and the default panel), and runs
`plasma-setup-bootutil`, which writes the autologin drop-in
`/etc/sddm.conf.d/99-plasma-setup.conf`. The script is `/etc/init.d/62_plasma-setup.sh`; once
`/etc/plasma-setup-done` exists it also deletes a leftover `99-plasma-setup.conf`, so a wizard that
finished without removing its autologin cannot log the `plasma-setup` account in again.

`plasma-setup-bootutil` asks systemd over the system bus whether Plasma Login is enabled; with no
systemd the call fails and it writes SDDM's drop-in, which is the login manager KDOS runs
([sddm](#port-sddm)).

The wizard's privileged steps go through its KAuth helper, `plasma-setup-auth-helper`, which runs
`useradd`, `chpasswd` and `usermod` from [shadow](#port-shadow), found in `/usr/sbin`. Its pages
change the system through the `hostname1`, `locale1` and `timedate1` interfaces KDOS's
`kdos-settingsd` serves, and through NetworkManager. The polkit rule upstream installs,
`/usr/share/polkit-1/rules.d/plasma-setup-polkit.rules`, lets the `plasma-setup` user take exactly
those actions without a password, and nobody else. Accounts it creates join `audio` through `useradd`'s default and the groups in
`/usr/share/kdos/xdg/plasmasetuprc` through `usermod -a`, and it reads the first ordinary UID from
`/etc/login.defs`.

The keyboard page needs `org.kde.plasma.private.kcm_keyboard` from
[plasma-desktop](#port-plasma-desktop) at build time (`ecm_find_qmlmodule(... REQUIRED)`), and the
Wi-Fi page loads [plasma-nm](#port-plasma-nm)'s QML at run time; the wizard's session shows
[plasma-keyboard](#port-plasma-keyboard) on a touch screen.
