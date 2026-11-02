## About

Calindori is the Plasma Mobile calendar: events and to-dos with reminders, kept in local
iCalendar files through [kcalendarcore](#port-kcalendarcore) rather than in Akonadi. Its
companion `calindac` is the reminder daemon, which fires the notifications for every calendar
Calindori manages.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

`calindac` installs an XDG autostart entry, `/etc/xdg/autostart/org.kde.calindac.desktop`, with
`X-KDE-autostart-condition=calindacrc:General:Autostart:true`. The key's default is true, so
Plasma starts the daemon in every session of every user once the package is installed; a user
turns it off with `Autostart=false` under `[General]` in `~/.config/calindacrc`. Its D-Bus
service file, `org.kde.calindac.service`, lets Calindori start it on demand as well.

To wake a suspended machine for a reminder, `calindac` asks `org.kde.Solid.PowerManagement` on
the session bus, which PowerDevil serves. PowerDevil is part of the Plasma session, and a 44_apps
port does not name a session port in `depends`, so it comes from the session's own meta port;
without it reminders still fire while the machine is awake.

Attendees are picked from [kpeople](#port-kpeople). The entry claims `text/calendar`; which
program opens a calendar file is set in `mimeapps.list`, where KDOS names Thunderbird.
[qqc2-desktop-style](#port-qqc2-desktop-style) is in `depends` because `main.cpp` selects its
`org.kde.desktop` style when no other is set. `KAboutData` gives the window the app_id
`org.kde.calindori`, the name of upstream's entry.
