# Mobile broadband

This page covers connecting through a cellular modem: a built-in WWAN card, a USB modem, or a
phone that offers dial-up networking over [Bluetooth]{.idx}. ModemManager finds and drives the modem,
NetworkManager makes the connection, and [`mmcli`]{.idx} shows the modem's state, unlocks its SIM and reads
its text messages. Mobile broadband is built and
[has not been connected on hardware](../../kdos/06-reference/known-gaps.md#a-usb-modem-that-first-appears-as-a-storage-device-is-not-switched-into-a-modem),
so nothing here has been seen to unlock a SIM, register or connect. What has been run, on a
machine with no modem, is the service and which ModemManager holds the bus name, `mmcli`'s
answers when there is no modem, and the creation of each profile below.

## Installation

| Port | Provides |
|---|---|
| [modemmanager](#port-modemmanager) | `ModemManager`, `mmcli` and `libmm-glib` |
| [libmbim](#port-libmbim), [libqmi](#port-libqmi), [libqrtr-glib](#port-libqrtr-glib) | The MBIM, QMI and QRTR protocols ModemManager speaks to a modem; a modem that speaks none of them is driven with AT commands |
| [mobile-broadband-provider-info](#port-mobile-broadband-provider-info) | The carriers' APN database, `/usr/share/mobile-broadband-provider-info/serviceproviders.xml` |
| [ppp](#port-ppp) | `pppd`, for a modem that connects over a serial line |
| [networkmanager](#port-networkmanager) | The `gsm` connection type, built with `-Dmodem_manager=true` and, for a phone, `-Dbluez5_dun=true` |

All of them are in the `network` shelf's list of `41_system`, so every image ships them. The kernel
drivers for MBIM and QMI USB modems and for PCIe WWAN cards are modules of the
[linux](#port-linux) port, loaded when the device appears.

[`/etc/init.d/42_modemmanager.sh`]{.idx} starts ModemManager at boot on every machine, enabled on the
live medium and on an installed system alike. It sorts after `41_polkitd` and before
`42_networkmanager`.

## Configuration

### The service script

<!-- kfs:include fs/etc/init.d/42_modemmanager.sh -->

ModemManager is started here rather than by the bus, for the reason given in
[How polkit and ModemManager are started](../../kdos/03-architecture/security-model.md#how-polkit-and-modemmanager-are-started).
It logs to the system log, `/var/log/messages`; the script's own output is in
`/run/kdos-init.modemmanager.log`. Its name for `service` is `modemmanager`, and
`service status modemmanager` reports the process as `ModemManager`.

On the booted image the ModemManager that serves is not the supervised one. The image also
ships `/usr/share/dbus-1/system-services/org.freedesktop.ModemManager1.service`, NetworkManager
asks for that name as it starts (the system log records *Activating service
name='org.freedesktop.ModemManager1' requested by ... comm="/usr/sbin/NetworkManager
--no-daemon"*), and the bus starts a ModemManager of its own. The supervised one then cannot take
the name: it logs *could not acquire the 'org.freedesktop.ModemManager1' service name*, exits,
and is started again every five seconds, for as long as the machine runs.
`service status modemmanager` still reports `[ OK ]`, because it reports the supervisor.
`ps -eo pid,ppid,args | grep '[M]odemManager'` tells the two apart: the one serving has parent 1,
and the supervised attempts appear in `/run/kdos-svc.ModemManager.log`.

### Who may drive the modem

<!-- kfs:include fs/etc/polkit-1/rules.d/50-kdos.rules lines=88-97 pin=9cc511da443e -->

ModemManager is built with `-Dpolkit=strict`, so without this rule only root may unlock a SIM,
enable a modem or read its messages. With it, members of `wheel` may do all three with `mmcli` and
no `sudo`. NetworkManager itself needs no rule, because it runs as root.

### The connection profile

A mobile broadband connection is a NetworkManager profile of type `gsm`, saved under
`/etc/NetworkManager/system-connections/` like every other; see
[NetworkManager](networkmanager.md#connection-profiles). Its settings, `gsm.apn`,
`gsm.username`, `gsm.password`, `gsm.pin` and the rest, are listed in `nm-settings-nmcli(5)`.

## Usage

### Finding the modem

1. List the modems ModemManager has found:

   ```bash
   mmcli -L
   ```

2. Show one in full. `any` stands for the only modem, or the first:

   ```bash
   mmcli -m any
   ```

   The output gives the modem's state (`locked`, `disabled`, `registered`, `connected` and so on),
   its SIM, its signal and the operator it has registered with.

With none found, `mmcli -L` prints `No modems were found` and `mmcli -m any` prints `error:
couldn't find modem`. A modem missing from `mmcli -L` is either absent from the kernel too (see
[Troubleshooting](#troubleshooting)) or a USB stick still showing its storage side.

### Unlocking the SIM

When `mmcli -m any` reports the modem `locked`, give the SIM its PIN:

```bash
mmcli -i any --pin=<PIN>
```

If you skip this, NetworkManager asks for the PIN when it first connects, through
`kdos-netagent` in a desktop session.

### Connecting

1. Create the profile with your carrier's APN:

   ```bash
   nmcli connection add type gsm ifname '*' con-name mobile apn <apn>
   ```

   `ifname '*'` lets the profile use whichever modem is present. Add `gsm.username` and
   `gsm.password` when the carrier asks for them. `nmtui`'s *Mobile Broadband* entry, under *Edit
   a connection*, creates the same profile.

2. Bring it up, and down:

   ```bash
   nmcli connection up mobile
   nmcli connection down mobile
   ```

### Connecting through a phone over Bluetooth

A phone that offers only dial-up networking over Bluetooth is a modem to ModemManager.

1. Pair the phone in `kdos-bt`.
2. Create a Bluetooth profile of the `dun` kind, with the phone's address and the carrier's APN:

   ```bash
   nmcli connection add type bluetooth bt-type dun bluetooth.bdaddr <phone> gsm.apn <apn>
   ```

3. Bring it up with `nmcli connection up` and the profile's name, which `nmcli connection show`
   lists.

A phone that shares its connection as a network (PAN) needs no modem at all; that is the `panu`
kind, on the [NetworkManager](networkmanager.md#other-connection-types) page.

### Text messages

```bash
mmcli -m any --messaging-list-sms        # the messages, each with a path or an index
mmcli -s <index>                          # one message in full
```

A carrier's data-limit warnings arrive this way.

### On the live medium and on an installed system

The two behave the same. On the live medium a profile lasts until the next reboot unless the medium
has a persistence store.

## Tips

- If you do not know the APN, set `gsm.auto-config yes` on the profile:
  `nmcli connection modify mobile gsm.auto-config yes`. NetworkManager then looks the APN up in
  the provider database by the network the modem has registered with.
- `mmcli -m any --3gpp-scan --timeout=300` lists the operators in range, which shows whether the
  modem can see a network at all. A scan can take minutes; `--timeout` keeps `mmcli` waiting for
  it.
- `mmcli -m any --enable` and `--disable` switch the modem's radio on and off without
  NetworkManager.
- Each option of `mmcli` is in `mmcli(1)`; `mmcli --help-all` prints them grouped.

## Troubleshooting

**`mmcli -L` lists no modem.**
Check that the kernel sees one: `dmesg | grep -iE 'mbim|qmi|wwan'` shows the driver binding to
it, and `ls /sys/class/net` then lists its network interface. A USB stick that shows up as a small read-only disk is in
its storage mode, and nothing on KDOS switches it; see [Known gaps](#known-gaps). If the kernel has
the device and ModemManager does not, check that a ModemManager holds the bus name
(`pgrep -a ModemManager`; see [The service script](#the-service-script)) and read the system log.

**`mmcli` refuses to unlock or enable the modem, or to list its messages.**
Your account is not in `wheel`, the group the polkit rule grants. `id` lists your groups; `sudo`
works either way.

**The modem is `registered` and the connection fails.**
The APN is the usual cause. Compare the profile's with the carrier's (`nmcli -f gsm connection show
mobile`), or turn on `gsm.auto-config`. NetworkManager's reason is in the system log:
`sudo grep -a NetworkManager /var/log/messages`.

**More detail is needed.**
`sudo mmcli -G DEBUG` raises ModemManager's log level (it answers `Successfully set logging
level`), and `sudo tail -f /var/log/messages` then
shows what it says to the modem. The level holds until the ModemManager that serves exits.
`sudo service restart modemmanager` does not reset it, because it restarts only the supervised
copy, which never holds the bus name (see [The service script](#the-service-script)).

## Known gaps

- [A USB modem that first appears as a storage device is not switched into a modem](../../kdos/06-reference/known-gaps.md#a-usb-modem-that-first-appears-as-a-storage-device-is-not-switched-into-a-modem)

## See also

- [Networking](../../kdos/02-user-guide/administration.md#networking): mobile broadband and the
  other connection types
- [How polkit and ModemManager are started](../../kdos/03-architecture/security-model.md#how-polkit-and-modemmanager-are-started)
- [What is granted instead: 50-kdos.rules](../../kdos/03-architecture/security-model.md#what-is-granted-instead-50-kdosrules)
- [NetworkManager](networkmanager.md), [DNS](dns.md)
- [Bluetooth](bluetooth.md), [sudo, PAM and polkit](sudo-pam-polkit.md)

<!-- book-nav -->
---

*Part XVI. System administration, chapter 390.* Previous: [389. VPN](vpn.md) · [Contents](../README.md) · Next: [391. Access point](access-point.md)
