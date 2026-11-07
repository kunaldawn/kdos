## About

KRunner is the KDE Frameworks library that answers a typed query. A caller hands a string to
`KRunner::RunnerManager`, which passes it to every enabled runner at once; each runner replies with
matches (`KRunner::QueryMatch`: a title, an icon, a relevance and the actions that run it), and the
manager ranks them into one result list, `KRunner::ResultsModel`, which a Qt Quick view shows as it
fills. It ships no runner and no program of its own. The search window that opens on Alt+Space is
`krunner` from [plasma-workspace](#port-plasma-workspace), which also carries most of the stock
runners (applications, files, the calculator, shell commands, sessions); the search fields of the
application launcher in [plasma-desktop](#port-plasma-desktop), of the overview in
[kwin](#port-kwin) and of [milou](#port-milou) are clients of the same library, and
[powerdevil](#port-powerdevil), [systemsettings](#port-systemsettings),
[kdeplasma-addons](#port-kdeplasma-addons), [akonadi-search](#port-akonadi-search),
[kweather](#port-kweather) and [zanshin](#port-zanshin) each add a runner. Most of the Plasma
session and several applications therefore require it at configure time.

A runner reaches the manager in one of two ways. A Qt plugin is installed under
`/usr/lib/qt6/plugins/kf6/krunner/` and found through its JSON metadata. A D-Bus runner is any
program, in any language, that implements the `org.kde.krunner1` interface (`Match`, `Run`,
`Actions`, `Config`, `Teardown` and `SetActivationToken`), whose definition the package installs as
`/usr/share/dbus-1/interfaces/kf6_org.kde.krunner1.xml`, and is declared by a desktop file in
`/usr/share/krunner/dbusplugins/` naming its service and object path
(`X-Plasma-DBusRunner-Service`, `X-Plasma-DBusRunner-Path`) and the query forms it answers
(`X-Plasma-Runner-Syntaxes`). A named service may be D-Bus activated, so the runner's program need
not be running before the first query. With `X-Plasma-API=DBus2` the manager asks the runner for
its configuration (`Config`) before it matches, which `X-Plasma-API=DBus` skips; either way
`Teardown` closes every match session in which the runner was asked to match. Under Wayland, when
the user runs a match, the manager obtains an XDG activation token for its own focused window and
passes it in `SetActivationToken` immediately before `Run`. A runner that starts a program hands that token on
as `XDG_ACTIVATION_TOKEN`. Without it, [kwin](#port-kwin) still raises the new window at its
default focus-stealing prevention level, Low, but opens it behind the active window at Medium and
above. KDOS's own runners, in [kdos-runner](#port-kdos-runner), are D-Bus runners written in C on
basu, so they link no Qt.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — The tarball carries no `po/` directory, so there is no
  catalogue to skip and the switch changes nothing in this build. Every KDE recipe in the tree
  passes it, so a later release that ships translations builds none of them without a recipe
  change; the image is English only (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

[kitemmodels](#port-kitemmodels) is a hard dependency although the configure looks for it without
`REQUIRED`: the library links `KF6::ItemModels` for its result model whatever the search found,
so without it generation stops on a target that does not exist rather than building a narrower
library.

Which runners run is the user's choice, kept in `~/.config/krunnerrc`: `[Plugins]` holds one
`<runner id>Enabled=` key per runner, and `[Runners][<runner id>]` that runner's own settings. A
distribution default for that file belongs in the XDG configuration cascade, not in this port.
The manager reads the runner list from disk when it loads its runners: when it starts, and again
whenever `[Plugins]` changes. A runner installed into a running session is therefore offered only
after one of the two. A Qt plugin and a D-Bus runner with the same id do not both load; the plugin
wins.

`/usr/share/kdevappwizard/templates/` receives two project templates, `runner6` (a C++ plugin) and
`runner6python` (a D-Bus runner in Python), which KAppTemplate and KDevelop offer as starting
points for a new runner. The API documentation is built only by its own `prepare_docs` and
`generate_docs` targets, which need Qt's `qdoc`, and the recipe builds neither.
