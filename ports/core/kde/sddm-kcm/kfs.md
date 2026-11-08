## About

sddm-kcm is the Login Screen (SDDM) page of System Settings. It lists the greeter themes under
`/usr/share/sddm/themes` and sets a theme's background, the autologin user and session, the range
of user IDs the greeter shows and the halt and reboot commands, and its "Apply Plasma Settings"
button copies the signed-in user's `kdeglobals` (colours and fonts), `plasmarc`, `kcminputrc`
(cursor theme and size, Num Lock) and `kwinoutputconfig.json` (screen layout and scale) to the
greeter's own configuration. Every change is written as root by a KAuth helper,
`kcmsddm_authhelper`, into `/etc/sddm.conf.d/kde_settings.conf`, the greeter user's
configuration and the theme's `theme.conf.user`.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

The helper's five polkit actions, `org.kde.kcontrol.kcmsddm.save`, `.installtheme`,
`.uninstalltheme`, `.sync` and `.reset`, are all `auth_admin`. polkit's default administrator rule
names the `wheel` group, so a member of `wheel` confirms each change with their own password in
the agent's dialog, and nobody else can change the greeter.

The page decides whether to list itself by asking `org.freedesktop.systemd1` on the system bus
whether `sddm.service` is enabled. The system bus here has no such name, the call fails with
`ServiceUnknown`, and upstream treats that answer as "list the page", so it is always shown.

"Apply Plasma Settings" writes into `~/.config` of the account named `sddm`, which the helper
looks up by that name; the greeter runs as that account and reads its own configuration from
there. Without an `sddm` account with a home directory the helper refuses the sync.

The configuration paths are compiled in from cache variables whose defaults are the ones
[sddm](#port-sddm) reads: `/etc/sddm.conf`, `/etc/sddm.conf.d` and `/usr/lib/sddm/sddm.conf.d`,
with sessions from `/usr/share/wayland-sessions` and `/usr/share/xsessions`.

The "Get New…" action opens KNewStuff with `sddmtheme.knsrc`, whose install command is the
bundled `sddmthemeinstaller`. KDOS restricts KNewStuff with `ghns=false` under
`[KDE Action Restrictions]`, which turns the action off, so nothing is downloaded and the installer
is never run.

[sddm](#port-sddm) and [systemsettings](#port-systemsettings) are in `depends` for run time: the
page is a plugin that System Settings loads, and it configures SDDM. Its QML imports Kirigami
and QtMultimedia in the page's main file, where QtMultimedia previews a theme's video background,
and the KItemModels module on the Behavior page, so [kirigami](#port-kirigami),
[qt6-qtmultimedia](#port-qt6-qtmultimedia) and [kitemmodels](#port-kitemmodels) are there too.
A missing module in the main file leaves the whole page blank.
