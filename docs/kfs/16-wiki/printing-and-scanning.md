# Printing and scanning

This page covers printing through [CUPS]{.idx}, adding and sharing [printers]{.idx term="printer"}, USB printers that
speak [IPP]{.idx}, and scanning through [SANE]{.idx}, both with a [scanner]{.idx} on the desk and one on the
network. Finding printers and scanners on the network depends on Avahi, which has a page of its
own, [Avahi and mDNS](avahi-and-mdns.md).

## Installation

Every port below is in a phase list, so the image carries all of them.

| Part | Ports |
|:--|:--|
| The print service | [cups](#port-cups), with [cups-filters](#port-cups-filters), [libcupsfilters](#port-libcupsfilters), [libppd](#port-libppd) and [ghostscript](#port-ghostscript) to turn a job into what a printer takes |
| Printers announced on the network | [cups-browsed](#port-cups-browsed) |
| USB printers and scanners that speak IPP | [ipp-usb](#port-ipp-usb) |
| Drivers for printers that need one | [gutenprint](#port-gutenprint), [hplip](#port-hplip), [brlaser](#port-brlaser), [splix](#port-splix), [foomatic-db](#port-foomatic-db) |
| Setting printers up | [kdos-shell](#port-kdos-shell) ([`kdos-print`]{.idx}), [system-config-printer](#port-system-config-printer) |
| Scanning | [sane-backends](#port-sane-backends), [sane-airscan](#port-sane-airscan) for network scanners, [skanlite](#port-skanlite) |
| Label printers | [ptouch-print](#port-ptouch-print) |

Three service scripts are part of the base filesystem, in `fs/etc/init.d`:

| Script | Service name | Supervised process | Starts |
|:--|:--|:--|:--|
| `80_cups.sh` | `cups` | [`cupsd`]{.idx} | The print service |
| `81_cups-browsed.sh` | [`cups-browsed`]{.idx} | `cups-browsed` | Adds printers shared on the network to CUPS as they appear |
| `82_ipp-usb.sh` | [`ipp-usb`]{.idx} | `ipp-usb` | Serves each IPP-over-USB device on localhost as it is plugged in |

The live medium starts all three. The installer turns CUPS off on an installed system unless you
turn it on: in its list of service choices, the last field of each entry is whether it starts
ticked:

<!-- kfs:include src/system/kdos-installer/conf.c lines=23-29 pin=9a2600b3e3b2 -->

The installer's choice writes [`/etc/service.disabled/cups`]{.idx} and nothing else, so `81_cups-browsed`
and `82_ipp-usb` still start at boot.

## Configuration

### The service scripts

Each runs its daemon in the foreground for the supervisor. `cupsd` takes `-f`; `cups-browsed` needs
no flag; `ipp-usb` runs in `standalone` mode, which keeps it running between devices:

<!-- kfs:include fs/etc/init.d/80_cups.sh -->

<!-- kfs:include fs/etc/init.d/82_ipp-usb.sh -->

### Who may manage printers

CUPS is configured with [`lpadmin`]{.idx} as its administrative group, runs filters as `lp`, and listens
on a local socket under [`/run/cups`]{.idx}:

<!-- kfs:include ports/core/printing/cups/build.sh lines=21-26 pin=61b8c5b3218f -->

The `kdos` account is in that group, in the shipped `/etc/group`:

<!-- kfs:include fs/etc/group lines=30-30 pin=7cbe296482c0 -->

A password asked for by the web interface is checked through PAM, with the file the cups port
writes:

<!-- kfs:include ports/core/printing/cups/build.sh lines=44-49 pin=06a021f5fcdc -->

### /etc/cups/

`cupsd.conf` and `cups-files.conf` are upstream's files; KDOS adds nothing there. Change
`cupsd.conf` with `cupsctl`, or edit it and restart the service. `printers.conf` holds the queues
and is written by `cupsd` itself as they are added; its first lines say not to edit it while
`cupsd` is running.

### /etc/ipp-usb/ipp-usb.conf

The ipp-usb port installs upstream's configuration unchanged, and upstream's quirks files for
particular printers under `/usr/share/ipp-usb/quirks`:

<!-- kfs:include ports/core/printing/ipp-usb/build.sh lines=16-19 pin=8b623148744b -->

Upstream's defaults serve each device on the loopback interface only, on a port from 60000 to
65535, and announce it over DNS-SD, which is how CUPS and SANE find a USB device as though it were
on the network. A quirks file of your own goes in [`/etc/ipp-usb/quirks`]{.idx}; `ipp-usb(8)` describes
the format.

### Scanners: /etc/sane.d/ and the udev rules

SANE's backends read their configuration from [`/etc/sane.d`]{.idx}; `dll.conf` there lists the backends
that are loaded, and sane-airscan adds itself through [`/etc/sane.d/dll.d/airscan`]{.idx}. The
sane-backends port writes the rules and the hardware-database file that mark each scanner a
backend supports:

<!-- kfs:include ports/core/printing/sane-backends/build.sh lines=22-25 pin=18964ba23fbe -->

The mark grants nothing by itself. KDOS's own rule turns it into the `dialout` group, and gives
the same group the USB devices of five scanner makers by vendor ID:

<!-- kfs:include fs/etc/udev/rules.d/70-kdos-scanner.rules -->

So a user scans only as a member of `dialout`. Brother P-touch label printers, which `ptouch-print`
drives over raw USB rather than through CUPS, go to `lpadmin` instead, in
[`/etc/udev/rules.d/70-kdos-ptouch.rules`]{.idx}.

## Usage

### Turning CUPS on (installed system)

1. Enable it for every boot, and start it:

   ```bash
   sudo service enable cups
   sudo service start cups
   ```

2. Check it:

   ```bash
   service status cups
   lpstat -r
   ```

### Adding a printer from the desktop

1. Open `kdos-print`: *Printers…* on the System page of Settings, or `kdos-print` at a prompt. Its
   *Printers* page lists the queues, each with its state, and marks the default.
2. Press `Right` for the *Discovered* page, which lists the device URIs `lpinfo -v` reports. URIs
   of the `file` and `serial` kinds are left out, and so are the bare backend names `lpinfo -v`
   prints for each backend that found no device (`network socket`, `direct hp`).
3. Select the printer and press `Enter`. The queue is named after its URI: the scheme is dropped,
   and each run of characters other than letters and digits becomes one underscore, so
   `ipp://printer.local/ipp/print` becomes `printer_local_ipp_print`.
4. Back on *Printers*, `d` makes the selected queue the default and `x` removes it. `r` reads both
   lists again.

`kdos-print` sets a printer up only as IPP Everywhere (`lpadmin -m everywhere`), the [driverless]{.idx term="driverless printing"}
protocol a printer advertises and CUPS configures with no PPD file. When `lpadmin` refuses, the
window shows its message. For a printer that needs a vendor driver, use the terminal procedure
below, or system-config-printer, *Print Settings* in the Start menu.

### Adding a printer from a terminal

1. List the device URIs CUPS can reach:

   ```bash
   lpinfo -v
   ```

   A line with a full URI, such as `network ipp://printer.local/ipp/print` or
   `direct usb://…`, is a device. A line carrying only a scheme, such as `network socket` or
   `direct hp`, is a backend that found nothing by itself; with no printer reachable those are the
   only lines.

2. Add and enable a driverless queue, with a name of your choice:

   ```bash
   lpadmin -p office -v 'ipp://printer.local/ipp/print' -m everywhere -E
   ```

   For a printer that needs a driver, find its model with `lpinfo -m` and give that name to `-m`
   in place of `everywhere`. `lpinfo -m` lists every model the drivers on the image cover, over
   seventeen thousand lines, so filter it: `lpinfo -m | grep -i laserjet`.

3. Make it the default and print a document:

   ```bash
   lpoptions -d office
   lp document.pdf
   ```

4. Follow the job:

   ```bash
   lpstat -p -d
   lpstat -o
   ```

   `cancel <job>` removes one.

`lpadmin`, `lpoptions`, `lp`, `lpstat` and `cancel` need no `sudo`: membership of `lpadmin` is
the authority CUPS checks. `lpoptions -d` sets your own default, which `lpstat -d` then reports.

### A USB printer

1. Plug it in. A printer that speaks IPP over USB is served by `ipp-usb` on localhost and announced
   there; it appears in `kdos-print` and in `lpinfo -v` like a network printer.
2. An older USB printer appears in `lpinfo -v` with a `usb://` URI. Add it with that URI and, where
   it is not driverless, a driver from `lpinfo -m`.

### Sharing a printer with other machines

1. Tell CUPS to share its queues, as yourself and not with `sudo`:

   ```bash
   cupsctl --share-printers
   lpadmin -p office -o printer-is-shared=true
   ```

   `cupsctl` rewrites `cupsd.conf`, which CUPS allows only after a password, so it asks
   `Password for <user> on localhost?` and takes your own login password. `cupsctl | grep share`
   then shows `_share_printers=1`.

2. Open the `ipp` service in the firewall, which needs membership of `wheel`:

   ```bash
   kdos-power firewall ipp on
   ```

   `ipp` opens TCP 631. The other named services, and how to close one again, are on
   [Firewall](firewall.md#opening-a-named-service).

### Scanning

1. Check that you are in `dialout`:

   ```bash
   id
   ```

2. List the scanners SANE can open. A network scanner that speaks eSCL or WSD is found by
   sane-airscan, through Avahi for eSCL and its own WS-Discovery for WSD:

   ```bash
   scanimage -L
   ```

   With none found it says `No scanners were identified.` and exits 0.

3. Scan, giving the device string `scanimage -L` printed:

   ```bash
   scanimage -d '<device>' > scan.pnm
   ```

4. For a preview and a selection, open Skanlite from the Start menu. `kdos-devices` (`Super+F6`)
   lists the same scanners, and does not scan.

## Tips

- CUPS's own web interface is at `http://localhost:631`, for every setting `kdos-print` does not
  offer.
- `lpstat -t` prints everything CUPS knows at once: the scheduler, the default, every queue and
  every job.
- An application in a box prints through the host's CUPS socket, `/run/cups`, which is shared into
  the box when the box is created. A box created while CUPS was not running cannot print until it
  is created again; see
  [Profile changes and re-creating a box](../../kdos/02-user-guide/applications.md#profile-changes-and-re-creating-a-box).
- `airscan-discover` lists the eSCL and WSD scanners on the network, with the lines that would
  name them in [`/etc/sane.d/airscan.conf`]{.idx}; with none it prints only its `[devices]`
  header.
- Programs for scanned documents: gImageReader (`gimagereader-qt6`) reads text out of a scan with
  Tesseract, and `ocrmypdf` adds a text layer to a scanned PDF.

## Troubleshooting

**`lpstat` says the scheduler is not running.** On an installed system CUPS is off unless it was
chosen at install time. See [Turning CUPS on](#turning-cups-on-installed-system). Otherwise:

```bash
cat /run/kdos-init.cups.log
sudo tail -n 40 /run/kdos-svc.cupsd.log
```

A running service reports `[ OK ] cupsd  (pid N)` to `service status cups`.

**`kdos-print` says `cupsd is not running (service start cups)`.** The scheduler is down; the
procedure above starts it. `lpstat -r` says `scheduler is running` or `scheduler is not running`.

**`lpadmin -m everywhere` fails.** `IPP Everywhere driver requires an IPP connection.` means the
URI is not `ipp://` or `ipps://`: the printer is on another backend and needs a driver from
`lpinfo -m`. `Unable to create PPD: Printer does not support required IPP attributes or document
formats.` means CUPS reached the address and found no driverless printer there.

**`lpadmin` is refused.** Check `id` for `lpadmin`. A user added with `useradd` is not in it unless
the `-G` list named it; add it with `sudo usermod -aG lpadmin <user>` and log in again.

**`cupsctl` says `Unauthorized` or `Forbidden`.** `Unauthorized` means no password reached CUPS:
`cupsctl` asks for one only on a terminal, so run it at a prompt rather than from a script, and
answer with your login password. `Forbidden` is what `sudo cupsctl` gets: CUPS's system group is
`lpadmin` alone, and root is not in it. Run it as a member of `lpadmin`, or as root with
`cupsctl -h localhost:631 -U <member> …` and that member's password.

**A job stays in the queue.** `lpstat -t` shows the queue's state and the printer's last message.
CUPS's log is `/var/log/cups/error_log`; for more detail, run `cupsctl --debug-logging`, print
again, and read it.

**A USB printer is not found.** Ask `ipp-usb` what it sees:

```bash
sudo ipp-usb check
sudo tail -n 40 /var/log/ipp-usb/main.log
```

`ipp-usb check` prints `Configuration files: OK` and then each device it serves, or
`No IPP over USB devices found`.

A printer that does not speak IPP over USB is not one `ipp-usb` serves; look for it in
`lpinfo -v` as a `usb://` URI.

**A network printer is not discovered.** Discovery is mDNS. Check `service status avahi` and
`avahi-browse -rt _ipp._tcp`; see [Avahi and mDNS](avahi-and-mdns.md#troubleshooting).

**`scanimage -L` lists nothing for a scanner that is plugged in.** Check the group first: a
backend cannot open a device node it has no permission for. Compare with `sudo scanimage -L`; if
root sees the scanner, add the user to `dialout` and log in again. If root does not see it either,
the backend may not support the model; `lsusb` shows its USB ID.

## Known gaps

- [Broad hardware support is not a goal](../../kdos/06-reference/known-gaps.md#broad-hardware-support-is-not-a-goal):
  no range of printers and scanners has been tested
- [Some command-line programs build after Qt](../../kdos/06-reference/known-gaps.md#some-command-line-programs-build-after-qt):
  sane-backends is among them, so a build that stops before `43_toolkits` has no scanning

## See also

- [Printing](../../kdos/02-user-guide/administration.md#printing): the user guide's section
- [kdos-print](../../kdos/04-programs/kdos-shell.md#kdos-print): why the window drives `lpadmin` and runs as you
- [Scanning, OCR and printing](../../kdos/02-user-guide/applications.md#scanning-ocr-and-printing): the applications and drivers
- [The device groups and the udev rules](../../kdos/02-user-guide/administration.md#the-device-groups-and-the-udev-rules): the scanner and label-printer rules
- [Avahi and mDNS](avahi-and-mdns.md), [udev and device groups](udev-and-device-groups.md),
  [Users and groups](users-and-groups.md), [Firewall](firewall.md), [Services](services.md)

<!-- book-nav -->
---

*Part XVI. System administration, chapter 397.* Previous: [396. Bluetooth](bluetooth.md) · [Contents](../README.md) · Next: [398. Audio](audio.md)
