## About

KUnifiedPush is KDE's implementation of UnifiedPush, the open standard for push notifications
without Google's servers. It has three parts: the client library an application links to register
for pushes (NeoChat, Tokodon and Itinerary do), `kunifiedpush-distributor`, the per-user D-Bus
service that keeps one connection to a push server (ntfy, Gotify, NextPush or Mozilla's Autopush)
and hands each message to the application it is for, and the Push Notifications page in System
Settings, which chooses the server and lists the registered applications.

## Flags

- `-D KUNIFIEDPUSH_CLIENT_ONLY=OFF` — Builds the distributor and the System Settings page as well
  as the client library. Upstream's default is the same; the switch is pinned because with it on
  the package would hold only the library, and an application that registers would then find no
  distributor on the bus.
- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the translation catalogues under `po/`, which
  the package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

The install puts a systemd user unit, `kunifiedpush-distributor.service`, and its
`graphical-session.target.wants` link under `/usr/lib/systemd`; `build.sh` removes the directory,
since KDOS has no systemd (see
[No systemd](../../../../docs/kdos/01-philosophy/principles.md#no-systemd)) and the package lint
refuses it. The distributor starts from the XDG autostart entry
`/etc/xdg/autostart/org.kde.kunifiedpush-distributor.desktop` instead, which the Plasma session
runs at login; the entry's `X-systemd-skip=true` only matters where systemd starts the unit.

The distributor opens no connection while no application has registered with it: it stays idle
on the session bus, and only a registration makes it connect to the push server. The server is
`https://unifiedpush.kde.org/`, an ntfy instance, from the shipped default
`/etc/xdg/KDE/kunifiedpush-distributor.conf`; System Settings changes it per user.

Messages are encrypted end to end with Web Push message encryption (RFC 8291), which is what
[openssl](#port-openssl) is for. The System Settings page is a QML page that imports Kirigami, so [kirigami](#port-kirigami)
is in `depends` for run time.

The System Settings page also installs `kcm_push_notifications.desktop`, which KCMUtils generates
with `NoDisplay=true`: it lets `kcmshell6` and a search open the page and adds no row to the
application menu.
