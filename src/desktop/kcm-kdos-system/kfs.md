## About

System Versions is the System Settings page for KDOS's two root slots, under System. It lists slot A
and slot B with the version, build date, tree commit and kernel each holds and the state each is
in (running, confirmed, on trial, failed), and its one button, "Start This Version Next Time",
arms a one-boot trial of the other slot: the previous version after an update, or the new one
again after a rollback. Its settings are where `kdos update` takes a newer tree from (the KDOS
releases, a folder or removable medium, or another address) and, under "Installing software",
the route `kdos update install` takes with no flag and the route of what a build lacks: the
`install =` and `build-deps =` lines of `/etc/kdos/update.conf`, each prebuilt or built on this
computer, both prebuilt by default. Beside the settings it shows what the
update service reports — an update that is available, one prepared in the other slot that Discover
has not yet installed, and why the last one stopped — and opens Discover to act on it.

The page holds no privilege and touches no file of the boot state. Every change is a call to
`io.kdos.Slots1` on the system bus, which [kdos-sysupdated](#port-kdos-sysupdated) serves and
authorises through polkit (`io.kdos.slots.try`, `io.kdos.slots.channel`,
`io.kdos.slots.install-route`) before it runs
`kdos-bootctl try <slot> 1`; a refusal from kdos-bootctl is shown in its own words. The interface
is `io.kdos.Slots1.xml` beside the recipe, and it is the contract both sides are held to: the
page's test fixture generates its adaptor from it, and kdos-sysupdated serves the same members and
keys.

## Flags

- `-D BUILD_TESTING=OFF` — Leave the QtTest suites out of the package. They start a fixture
  `io.kdos.Slots1` server on a private session bus and load the page offscreen, which
  `testing/plasmatest.sh` does in the development container; the package build has no session bus.

## Notes

The QML and the C++ share the translation domain `kcm_kdos_system`, the plugin id, because
`KQuickConfigModule::mainUi()` sets the QML engine's domain to the plugin id; a second name would
split the catalogue in two.

`X-DocPath` in `kcm_kdos_system.json` is `kdos/04-programs/kdos-settings.html#system-versions`,
the page's section of the KDOS book under `help:/`, which `testing/anchorcheck.py` checks against
the book's headings. System Settings' Help button opens `help:/` followed by the key's text, and
KIO's `help:` worker finds the page through `/usr/share/doc/HTML/en/kdos`, the link
[kdos-doc](#port-kdos-doc) installs (see
[Help](../../../docs/kdos/04-programs/kdos-settings.md#help)).

Applying a different update location or install route raises an administrator prompt, which needs the polkit
agent of the Plasma session; a page opened outside a session gets a refusal and says so. A
location is written whole into one `tree =` line of `/etc/kdos/update.conf`, so a path or an
address containing a space or a control character is refused before it is sent.
