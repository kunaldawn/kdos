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

`build.sh` deletes the XDG autostart entry `calindac` installs,
`/etc/xdg/autostart/org.kde.calindac.desktop`, whose condition key defaults to true and would
start the daemon in every session of every user; [akonadi-calendar](#port-akonadi-calendar)'s
`kalendarac` already fires the desktop's calendar reminders. The D-Bus service file,
`org.kde.calindac.service`, stays, so Calindori starts `calindac` when it opens, and its reminders
fire in a session where Calindori has run. A user who wants them at every login copies the entry
from the source's `calindac/` directory into `~/.config/autostart`.

To wake a suspended machine for a reminder, `calindac` asks `org.kde.Solid.PowerManagement` on
the session bus, which PowerDevil serves. PowerDevil is part of the Plasma session, and a 44_apps
port does not name a session port in `depends`, so it comes from the session's own meta port;
without it reminders still fire while the machine is awake.

Attendees are picked from [kpeople](#port-kpeople). The entry claims `text/calendar`; which
program opens a calendar file is set in `mimeapps.list`, where KDOS names Thunderbird.
[qqc2-desktop-style](#port-qqc2-desktop-style) is in `depends` because `main.cpp` selects its
`org.kde.desktop` style when no other is set. `KAboutData` gives the window the app_id
`org.kde.calindori`, the name of upstream's entry.
