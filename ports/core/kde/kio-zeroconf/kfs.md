## About

kio-zeroconf is the `zeroconf:/` KIO worker, which lists the services machines on the local
network advertise over DNS-SD (mDNS): file shares, printers, SSH and web servers. Dolphin's Network
place shows it, and a kded module watches for services coming and going so an open folder
refreshes.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations in `po/`, which kpkg's
  English-only rule would remove from the package anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-Wno-dev` — Silences CMake's warnings meant for the project's own developers.

## Notes

Discovery goes through [kdnssd](#port-kdnssd), which is built against [avahi](#port-avahi), so
the worker sees nothing unless `avahi-daemon` is running; KDOS starts it at boot. The
`dnssdwatcher` module runs inside `kded6` from [kded](#port-kded), which the Plasma session
starts. Answers to mDNS queries arrive on UDP port 5353, which the shipped firewall admits (see
[The firewall](../../../../docs/kdos/02-user-guide/administration.md#the-firewall)).
