## About

akonadi-calendar keeps calendars in [akonadi](#port-akonadi). It holds the calendar models
(`ETMCalendar`), the incidence changer that every calendar edit goes through, iTIP handling (the
invitations, replies and cancellations that travel by mail, sent through
[kmailtransport](#port-kmailtransport) and composed with [messagelib](#port-messagelib)), the
serializer plugins that store events, to-dos and journals as Akonadi items, a KCalendarCore
plugin that offers Akonadi calendars to any program using KCalendarCore, and `kalendarac`, the
reminder daemon that shows alarm notifications. KOrganizer, Merkuro,
[calendarsupport](#port-calendarsupport) and the Digital Clock's PIM events link or load it.

## Flags

- `-D BUILD_PYTHON_BINDINGS=OFF` — Build no Python bindings. Upstream's default is on, and the
  configure then requires Shiboken6 and PySide6, which are not ports.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6Mime=ON` — Take KMime from its Frameworks package. The
  configure asks for `KF6Mime` first and falls back to the PIM-era `KPim6Mime`; required, the
  fallback can never be taken. [kmime](#port-kmime) installs `KF6Mime`.
- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations, which kpkg's
  English-only rule would remove from the package anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

Signed and encrypted invitations go through GpgME: the configure requires
[gpgmepp](#port-gpgmepp), [qgpgme](#port-qgpgme) and [libkleo](#port-libkleo).

`kalendarac` installs an XDG autostart entry, `org.kde.kalendarac.desktop`, which Plasma starts at
login unless `kalendaracrc` sets `[General] Autostart=false`. It does not start the Akonadi server
on its own: until Akonadi has been set up, which happens the first time a PIM program runs, it
only waits for somebody else to start the server, so the default session, whose calendar is
Thunderbird, runs no Akonadi. Its D-Bus service file lets the Digital Clock and the PIM programs
start it on demand as well.
