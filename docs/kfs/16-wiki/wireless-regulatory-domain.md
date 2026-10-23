# Wireless regulatory domain

A Wi-Fi radio may use only the channels and the transmit power the country it is in allows. The
kernel learns the rules from the signed [wireless regulatory database]{.idx} and the country from
its *regulatory domain*. Until something names a country, it stays in the world domain `00`,
which keeps every 5 GHz channel that needs radar detection (DFS) closed and caps transmit power.
This page covers where the database and the country come from on KDOS, how to see and change the
country, and how to tell when the radio is stuck on the world rules.

## Installation

| Port | Provides |
|---|---|
| [wireless-regdb](#port-wireless-regdb) | `/lib/firmware/regulatory.db` and its signature, `regulatory.db.p7s` |
| [iw](#port-iw) | `iw reg get` and `iw reg set` |
| [linux](#port-linux) | `cfg80211`, the kernel's wireless layer, built as a module |
| [kdos-powerd](#port-kdos-powerd) | The `timezone` verb, which also sets the country |
| [kdos-tools](#port-kdos-tools) | `kdos doctor`, which checks the database and the domain |

`wireless-regdb` is on the `boot` shelf of `41_system`; the rest are on every image too. No
service script is involved: `cfg80211` reads the country from its module options when it loads,
and loads the database itself.

The kernel accepts `regulatory.db` only with a valid `regulatory.db.p7s` beside it, signed by a key
it trusts. The port builds the database from upstream's `db.txt`, checks the result against
upstream's signature, and refuses to package it when the two disagree, so an edited database never
ships; see [Firmware](../../kdos/02-user-guide/administration.md#firmware).

## Configuration

### The database

<!-- kfs:include ports/core/boot/wireless-regdb/build.sh lines=28-30 pin=296d0b3fbe48 -->

Both files must stay as the package installed them. A database regenerated locally fails the
signature check in silence and leaves the radio exactly where it was, on the world domain.

### The country: `/etc/modprobe.d/kdos-regdom.conf`

One setting, `options cfg80211 ieee80211_regdom=<CC>`, where `<CC>` is a two-letter country code;
the file `kdos-powerd` writes puts a two-line comment naming the zone above it. The image ships
no such file; two programs write it from the [time zone]{.idx}, looking the zone up in
tzdata's `zone.tab`, which gives each zone one country.

The installer writes it from the zone chosen on its *Time* page:

<!-- kfs:include src/system/kdos-installer/install.c lines=1746-1754 pin=5a0dbdc98892 -->

`kdos-powerd` writes it whenever the time zone is changed later, and runs `iw reg set` so the
running radio follows at once. A zone with no country, such as `UTC`, removes the file:

<!-- kfs:include src/daemons/kdos-powerd/main.c lines=808-819 pin=7a5ecb4a3526 -->

The file is read when `cfg80211` loads, which is during boot, as the first wireless driver loads,
or, on a machine with no wireless driver, when a program such as `iw` first asks for the wireless
layer. A change to it takes effect at the next boot. Removing it changes nothing in the running
kernel either: after a change to `UTC` the radio keeps the country it had until the next boot. The key is described in
[/etc/modprobe.d/kdos-regdom.conf](../../kdos/06-reference/configuration.md#etcmodprobedkdos-regdomconf),
and the verb in [timezone](../../kdos/04-programs/daemons.md#timezone).

## Usage

### Seeing the country in force

```bash
iw reg get
```

The `global` entry is the country the kernel applies; an entry per wireless device (`phy#0`)
follows for a driver that keeps its own. `country 00: DFS-UNSET` is the world domain; a country
names its radar-detection region, as in `country DE: DFS-ETSI`, and each line after it is a
frequency range with its limits and flags.

### Setting the country

1. Set the time zone; the country follows it. From the desktop, choose the zone in `kdos-time`.
   From a terminal, as a member of `wheel`:

   ```bash
   kdos-power timezone Europe/Berlin
   ```

   This rewrites `/etc/modprobe.d/kdos-regdom.conf` and runs `iw reg set` for the running boot.

2. Check the result with `iw reg get`, and later with `kdos doctor`.

### Setting the country for this boot only

When the machine is somewhere other than its time zone says, set the country by hand. It lasts
until the next boot:

```bash
sudo iw reg set DE
```

The file the time zone writes is rewritten, or removed, at every change of zone, so it is not the
place for a country that differs from the zone's.

### Checking with `kdos doctor`

```bash
kdos doctor
```

The *Hardware* section has one line for the database. It reads both files and the country `cfg80211` was
loaded with, and reports `ok` with that country when the files are in place and it is not `00`;
`warn` when either file is missing, or when the country is `00` on a machine with a radio; and
`skip` when the country is `00` and there is no wireless device, as in a virtual machine, where the
line reads `[skip] regulatory.db and signature present; no wireless device here to load them`.
The country it reads is the module's load-time value, so a later `iw reg set` does not change
the line.

### On the live medium and on an installed system

The live medium ships no `kdos-regdom.conf`, so it boots on the world domain. Setting the time zone
in the live session writes the file and sets the running radio's country, and the file lasts until
the next reboot unless the medium has a persistence store. An installed system carries the file
the installer wrote from the zone you chose, so it boots with a country from the start, unless the
zone has none, such as `UTC`.

## Tips

- `iw list` shows, per band, which channels are open and which are `disabled` or `no IR` (no
  initiating radiation: the radio may answer on them but not start a transmission). The same
  machine shows more 5 GHz channels open once a country is set.
- A `hostapd` [access point]{.idx} carries its own `country_code` key; set it to the same country, see
  [Access point](access-point.md).
- Some drivers adopt the country an access point announces, others never do; the module option is
  what makes the country independent of the network you join.

## Troubleshooting

**`kdos doctor` warns that the domain is `00`.**
No country was set at boot. Set the time zone as above; the warning names the same two remedies
it does, `kdos-power timezone <Area/City>` for every boot and `iw reg set <CC>` for this one.

**`kdos doctor` warns that `regulatory.db` or `regulatory.db.p7s` is missing.**
The package's files are gone or damaged. `sudo kpkg check wireless-regdb` compares them with the
package database.

**No 5 GHz networks show, or a 5 GHz access point will not start.**
Run `iw reg get`. On `country 00` the DFS channels are closed; set the country. With the right
country set, `iw list` shows whether the card itself supports the band.

**The country is set and `iw reg get` still says `00`.**
Check that the file holds a line `options cfg80211 ieee80211_regdom=<CC>` with a two-letter code,
and that it is read: `cat /sys/module/cfg80211/parameters/ieee80211_regdom` prints the value the
module was loaded with. `dmesg | grep -i regulatory` shows whether the kernel loaded the database
and accepted its signature; its first line is `cfg80211: Loading compiled-in X.509 certificates
for regulatory database`.

## Known gaps

- [kdos doctor cannot check hardware a machine does not have](../../kdos/06-reference/known-gaps.md#kdos-doctor-cannot-check-hardware-a-machine-does-not-have):
  the database check is a `skip` with no wireless device.

## See also

- [Firmware](../../kdos/02-user-guide/administration.md#firmware): the database, its signature and
  the country
- [3. Time](../../kdos/02-user-guide/installation.md#3-time): the installer's zone, and the country
  it sets
- [timezone](../../kdos/04-programs/daemons.md#timezone): the `kdos-powerd` verb
- [kdos doctor](../../kdos/04-programs/kdos-command.md#kdos-doctor)
- [NetworkManager](networkmanager.md), [Access point](access-point.md)
- [Time and NTP](time-and-ntp.md), [Microcode and firmware](microcode-and-firmware.md)

<!-- book-nav -->
---

*Part XVI. System administration, chapter 392.* Previous: [391. Access point](access-point.md) · [Contents](../README.md) · Next: [393. File sharing](file-sharing.md)
