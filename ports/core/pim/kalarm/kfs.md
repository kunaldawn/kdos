## About

KAlarm is KDE's personal alarm scheduler. An alarm shows a message, plays a sound, speaks its
text, runs a command or sends an email, once or on a recurrence, with reminders before it and
snoozing after it. Alarms live in KAlarm's own calendar files; its Akonadi plugin adds birthday
alarms from the address book, email alarms sent through [kmailtransport](#port-kmailtransport),
and the address book for email recipients. It can keep an icon in the system tray, through
[kstatusnotifieritem](#port-kstatusnotifieritem), to show and pause the pending alarms.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — compile none of the translation catalogues in `po/`. kpkg
  removes every non-English catalogue from the package anyway (see
  [size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)),
  so this only saves build time.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6Auth=ON` — wake from suspend for an alarm through the
  real-time clock. `ENABLE_RTC_WAKE_FROM_SUSPEND` is on by default, but upstream turns it off by
  itself when it does not find [kauth](#port-kauth). Required, the RTC helper is always built.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6GlobalAccel=ON` — global shortcuts for a new alarm and for
  spreading the alarm windows, through [kglobalaccel](#port-kglobalaccel), which upstream looks
  for without `REQUIRED`.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6IconThemes=ON` — `KIconTheme::initTheme()` at start-up, which
  makes the icons follow the desktop's icon theme and colour scheme. Upstream finds
  [kiconthemes](#port-kiconthemes) without `REQUIRED` and links it only when it is found.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON` — the handbook is built only when
  [kdoctools](#port-kdoctools) is found; required, a missing kdoctools stops the configure instead
  of quietly dropping the handbook.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6TextEditTextToSpeech=ON` — speaking an alarm's text, through
  the `TextEditTextToSpeech` library of [ktextaddons](#port-ktextaddons), which exists only when
  ktextaddons found [qt6-qtspeech](#port-qt6-qtspeech). Upstream looks for it quietly; required,
  a ktextaddons without speech stops the configure instead of dropping the choice.
- `-D WITHOUT_X11=ON` — no X11 code. Upstream finds the X11 libraries whenever they are there and
  then uses them to keep alarm windows off other virtual desktops and to raise an edit dialog, all
  of it only when KAlarm runs as an X11 client. KAlarm runs as a Wayland client on KDOS.

## Notes

Sound alarms play through a plugin, and upstream builds two: one on [vlc](#port-vlc) and one on
[mpv](#port-mpv). `ENABLE_LIBVLC` and `ENABLE_LIBMPV` are both on by default, each finds its
library with `REQUIRED`, and the configure stops if both are off, so both libraries are in
`depends` and the build has no optional sound backend.

The RTC helper is a KAuth helper, `kalarm_helper`, run as root by D-Bus activation for the action
`org.kde.kalarm.rtcwake.settimer`. The action's polkit default is `auth_admin`, so setting a
wake-up time asks for an administrator's password.

Global shortcuts are registered with the session's `kglobalacceld`, a Plasma session port that
this port does not name (a port in 41 to 44 never depends on a session port); the shortcuts do
nothing in a session without it.

Upstream installs `kalarm.autostart.desktop` into `/etc/xdg/autostart` with
`X-KDE-autostart-condition=kalarmrc:General:AutoStart:false`, so KAlarm starts at login only once
the user has turned on its own start-at-login setting.

[kdepim-runtime](#port-kdepim-runtime) is in `depends` for run time only: the Akonadi plugin reads
birthdays from its address-book resources and sends email alarms through its mail dispatcher
agent.

The Wayland app id is `org.kde.kalarm`, which `main.cpp` sets as the desktop file name; it is the
name of the entry upstream installs, which carries no `StartupWMClass`, so the task manager
matches the window to it with no change.

`KF_SKIP_PO_PROCESSING` stops only `ki18n_install()`: `kdoctools_install()` still builds the
handbook in every translated language, and kpkg's language rule removes
`/usr/share/doc/HTML/<lang>/` from the package.
