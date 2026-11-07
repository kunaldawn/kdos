## About

iio-sensor-proxy reads a machine's accelerometer, ambient-light sensor, compass and proximity
sensor through the kernel's IIO and input interfaces and publishes them on the system bus as
`net.hadess.SensorProxy`. KWin asks it for the device orientation to rotate a convertible's or
tablet's screen, and PowerDevil for the ambient light to drive automatic brightness. A machine
with no such sensor gains nothing from it, and the daemon then exits.

The daemon is `/usr/libexec/iio-sensor-proxy`, run as root under `ksvc` from an init script in
the tree's `fs/` overlay; `monitor-sensor` prints what it reports.

## Flags

- `-Dudevrulesdir=/usr/lib/udev/rules.d` — Install `80-iio-sensor-proxy.rules` where
  [eudev](#port-eudev) reads rules. The rule tags each sensor with `IIO_SENSOR_PROXY_TYPE`, which
  the daemon reads to know what a device is. Upstream's `auto` asks `udev.pc`, which names
  `/lib/udev`, the same directory under the merged `/usr`, by a path the package would then
  carry outside `usr`.
- `-Dsystemdsystemunitdir=` — Install no systemd unit. An empty directory is meson's switch
  for leaving a configured file uninstalled.
- `-Dssc-support=disabled` — No Qualcomm Sensor Core support, which needs `libssc` and serves
  only phones with a Qualcomm sensor DSP.
- `-Dtests=false` — With `-Dgtk-tests=false` and `-Dgtk_doc=false`: no test programs, no GTK
  test window and no API reference.

## Notes

The D-Bus policy lets only root own `net.hadess.SensorProxy` and lets any client read it. Nothing
installs a D-Bus activation file for the name: upstream starts the daemon only through its systemd
unit (`Type=dbus`) and the rule's `SYSTEMD_WANTS`, which eudev ignores. So the daemon must be
running before KWin and PowerDevil ask for it, and on KDOS an init script starts it at boot under
`ksvc`.

At start-up the daemon logs "No sensors or missing kernel drivers for the sensors. Exiting" and
exits with status 0 when no device carries `IIO_SENSOR_PROXY_TYPE`, and it exits the same way
when the last sensor is removed. A supervisor that restarts it on every exit would restart it every
few seconds on a desktop, so the init script starts it only when udev reports a tagged device.
