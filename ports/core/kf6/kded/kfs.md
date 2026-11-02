## About

KDED is `kded6`, the session daemon that hosts KDE's background modules: plugins in the
`kf6/kded` namespace that a desktop or a library installs and that `kded6` loads at login or the
first time something on the session bus asks for them. [kio](#port-kio)'s trash and `remote:/`
notifiers run in it, as do the Plasma modules for the status notifier watcher, the display
configuration and the keyboard layout. The daemon is D-Bus activatable as `org.kde.kded6`, and
[plasma-workspace](#port-plasma-workspace) starts it with the session.

## Notes

The install puts a systemd user unit, `plasma-kded6.service`, under `/usr/lib/systemd`;
`build.sh` removes the directory, since KDOS ships no systemd unit (see
[No systemd](../../../../docs/kdos/01-philosophy/principles.md#no-systemd-init-units-or-service-manager)). The D-Bus service file
it installs beside the unit starts `/usr/bin/kded6` directly, so activation works without it.

[kdoctools](#port-kdoctools), optional upstream and in `depends`, builds the manual page of
`kded6`. `KF_SKIP_PO_PROCESSING` is not passed: the port's only translations are those of that
manual page, which `kdoctools_install()` builds whatever the switch says, and kpkg's language
rule removes from the package (see
[Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
