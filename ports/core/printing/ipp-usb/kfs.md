## About
ipp-usb serves a USB printer or scanner that speaks IPP-over-USB as if it were a network one: it
claims the device's IPP interfaces through [libusb](#port-libusb), relays them to a port on
localhost, and announces them with DNS-SD through [avahi](#port-avahi). [CUPS](#port-cups) then
finds the printer driverless, the same way it finds one on the network.
`/etc/init.d/82_ipp-usb.sh` runs `ipp-usb standalone`, which stays up and serves each such
device as it is plugged in.

## Flags
- `CGO_ENABLED=1` — Build with cgo, because ipp-usb's libusb and Avahi bindings are C. The Go
  default in this tree is cgo off; see
  [go](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#go).

## Notes
Upstream's `make install` also installs a udev rule and a systemd unit that start ipp-usb per
device. `build.sh` installs the program, `ipp-usb.conf`, the device quirk files and the manual
page itself and leaves both out: there is
[no systemd](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/01-philosophy/principles.md#no-systemd),
and the init script's `standalone` mode replaces the per-device start. Announcing on localhost
needs Avahi 0.8.0 or newer.
