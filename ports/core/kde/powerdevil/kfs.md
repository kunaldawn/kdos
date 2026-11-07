## About

PowerDevil is Plasma's power manager. Its daemon, `org_kde_powerdevil`, dims and blanks the screen
when the session is idle, suspends or powers off on the idle, lid, power-key and low-battery events
its profiles name, sets screen and keyboard brightness, and switches the power profile. It also
installs the battery and brightness applet, the Power Management settings module and a KRunner
plugin for the same controls. The suspend and power-off themselves, and the inhibitors that keep
the lid and the power key away from the system's own handling while a session runs, go through
`org.freedesktop.login1`, which [elogind](#port-elogind) serves.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Compile none of the interface translation catalogues in `po/`.
  kpkg removes every non-English catalogue from the package anyway (see
  [size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)),
  so this only saves build time.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_DDCUtil=ON` — External monitors' brightness over DDC/CI, through
  libddcutil from [ddcutil](#port-ddcutil). Upstream builds it in when the library is found and
  says nothing when it is not, and without it the brightness slider moves only a laptop's own
  panel.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON` — The handbook, through
  [kdoctools](#port-kdoctools), which upstream treats as optional.
- `-Wno-dev` — Silences CMake's warnings meant for the project's own developers.

## Notes

`plasma_session` starts the daemon from the XDG autostart entry
`/etc/xdg/autostart/powerdevil.desktop`, phase 0. The install also puts a systemd user unit under
`/usr/lib/systemd`, which the package lint refuses (see
[What the package lint refuses](../../../../docs/kdos/05-developer/writing-ports.md#what-the-package-lint-refuses));
`build.sh` removes the directory. The entry carries `X-systemd-skip=true`, which only a systemd
session reads.

The power profile switch, in the battery applet and in the settings, calls
`org.freedesktop.UPower.PowerProfiles` on the system bus and nothing else. [tlp-pd](#port-tlp-pd)
serves that name over TLP's own profiles. With no provider the applet shows no switch and tells the
user to install `power-profiles-daemon`, which is not a port. Battery levels and the AC state come
from [upower](#port-upower) through Solid. Both are in `depends` for run time, with
[elogind](#port-elogind).

Brightness on a laptop panel is written by `backlighthelper`, a KAuth helper that runs as root
under [polkit](#port-polkit), whose upstream policy lets the user at the active session use it
without a password. Three more helpers follow the same path: `chargethresholdhelper` for the
battery's charge limits, `discretegpuhelper` and `wakeupsourcehelper`. polkit is in `depends` for
run time, since without its daemon every helper call fails.

The Power settings show the battery's charge limits only when the helper's `getthreshold` action
answers, and hide them when it fails. [tlp](#port-tlp) writes the same sysfs files from
`tlp.conf` each time it applies a profile, so a limit the helper wrote would last only until TLP
next ran. KDOS's polkit rules (`/etc/polkit-1/rules.d/50-kdos.rules`) refuse the two writes,
`org.kde.powerdevil.chargethresholdhelper.setthreshold` and `.setconservationmode`, for everyone
and leave the two reads at upstream's defaults: the page shows the limits TLP set and cannot change
them.

libddcutil opens the `/dev/i2c-*` node of each monitor's DDC channel, so an external monitor's
brightness follows the slider only where the user can open that node. KDOS's udev rule
`/etc/udev/rules.d/70-kdos-i2c.rules` gives the `video` group read and write on the nodes whose
adapter belongs to a display controller, and on no other I²C bus; the desktop user is in `video`.
