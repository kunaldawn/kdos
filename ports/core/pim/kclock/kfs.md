## About

KClock is KDE's clock application, a Kirigami program from the convergent set: alarms, timers, a
stopwatch and a list of world clocks. Alarms and timers belong to `kclockd`, a small daemon
started from the session's autostart directory and by D-Bus activation, so an alarm rings with the
window closed. The port also installs a panel widget showing the next alarm and a KRunner plugin
for setting timers.

## Flags

- `-D BUILD_PLASMOID=ON` — Build the panel widget, which links [libplasma](#port-libplasma);
  upstream's default, stated so that a missing libplasma stops the configure instead of being a
  reason to turn the widget off.
- `-D KCLOCK_BUILD_SHELL_OVERLAY=ON` — Build the over-the-lock-screen popup for a ringing alarm or
  a finished timer, over the `kde_lockscreen_overlay_v1` protocol from
  [plasma-wayland-protocols](#port-plasma-wayland-protocols); upstream's default, stated for the
  same reason. The desktop entry names the interface in `X-KDE-Wayland-Interfaces`, which is how
  KWin grants it.
- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

`kclockd` wakes the machine for an alarm through PowerDevil's `org.kde.Solid.PowerManagement`
interface, which belongs to the Plasma session, not to `depends`: a Gear port never names a
session port. Without PowerDevil the daemon counts down with an ordinary timer, which rings only
while the machine is awake.

The default alarm sound is from [ocean-sound-theme](#port-ocean-sound-theme), with the
freedesktop theme as the fallback, so the theme is in `depends` for run time. The plasmoid's
configuration page imports `org.kde.kcmutils` from [kcmutils](#port-kcmutils) and the clock face
draws with [ksvg](#port-ksvg).

The daemon's D-Bus activation file names `kclockd` by path and carries no `SystemdService=`
line, so the session bus starts it directly.

The lock-screen overlay links Qt's private `GuiPrivate` and `WaylandClientPrivate` modules from
[qt6-qtbase](#port-qt6-qtbase) and [qt6-qtwayland](#port-qt6-qtwayland), so the port is rebuilt
with every Qt release, as every port that uses a private Qt header is.
