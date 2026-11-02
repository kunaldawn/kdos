## About

`kdos-sysupdated` is the root daemon that lets Plasma's Discover install a KDOS system update. It
owns `org.freedesktop.sysupdate1` on the system bus and answers the calls Discover's stock
SystemdSysupdate backend makes: one target, `host`; its version; an AppStream file describing the
update; the newest version (`CheckNew`); then `Acquire`, which does all the long work and reports
its progress, and `Install`, which arms the filled slot for the next boot. It does none of that work
itself. Every check, download and install is a `kdos update` run (`check --fetch`,
`apply --acquire --mount`, `apply --arm --mount`) and every boot decision a `kdos-bootctl` run, each
exec'd with an argument vector and read through a progress pipe while the bus keeps answering. See
[Updating a machine](../../../docs/kdos/03-architecture/packaging.md#updating-a-machine) and
[A/B slot selection](../../../docs/kdos/03-architecture/boot-and-init.md#ab-slot-selection).

It also serves `io.kdos.Slots1`, whose contract is the System Versions page's
[`io.kdos.Slots1.xml`](../../desktop/kcm-kdos-system/io.kdos.Slots1.xml): the slots, the armed
trial, the update channel, `Try`, `CancelTry`, `SetChannel` and `Bless`. And it is the root half of
the session-confirmed boot: it runs `kdos-bootctl mark-good` once `/run/kdos/bless/` holds both the
mark the end of `rcS` writes and the one a Plasma session (through `Bless`) or a login greeter that
stayed up for a minute earns.

The port is KDOS's own C on `basu`, with no upstream source; `build.sh` compiles the daemon with
`libkbase` and installs it beside its bus policy, its two polkit action files and the packaged
AppStream file.

## Notes

- **It is tested against Discover's own XML.** `tests/org.freedesktop.sysupdate1.xml` is
  discover-6.8.2's `libdiscover/backends/SystemdSysupdateBackend/org.freedesktop.sysupdate1.xml`,
  byte for byte, the file Discover generates its proxies from. `testing/selftest.sh` runs the daemon
  on a private bus with `--session --root` over fixture slots and diffs its introspection against
  that file and against the Slots1 contract. The sysupdate1 API is not stable upstream, so every
  Plasma bump replaces the vendored copy with the new Discover's and reruns the selftest.
- **Acquire answers only after `kdos update` has said `verified` or `refused`.** Discover shows a
  failed call's message and a failed job as nothing but "failed", so a release index, binhost index
  or image manifest no trusted key signed, a package the binhost does not cover, or a slot that
  cannot be mounted reaches the person as the call's error, and only an update that can go ahead
  becomes a job.
- **`CheckNew` offers only what the release index named.** A fetch takes the release's signed
  `kdos-update.idx` first and the binhost index and slot image manifest only where it says, with
  the hash it gives; the offer in the update document is made from those alone. See
  [Packaging](../../../docs/kdos/03-architecture/packaging.md#publishing-a-release).
- **The update is a slot image or binhost packages, whichever `kdos update check` offered.** The
  daemon passes no lane to `kdos update apply`; the update document's `lane` only changes the
  AppStream text, which names the signed system image or the packages.
- **Nothing listens on localhost:3129.** Discover asks that address for an update's size and shows 0
  when nothing answers; the size still to download is in the update document, and the AppStream
  text Discover shows above the release notes states it.
- **A trial boot is offered nothing.** While this boot is the trial of the other slot, the slot an
  update would fill is the confirmed way back; `CheckNew` answers empty and `Acquire` is refused
  until the trial is confirmed or rolled back.
- **No D-Bus activation file.** `/etc/init.d/46_sysupdated.sh` supervises the daemon, and a bus that
  started a second, unsupervised copy for an early caller would keep the supervised one from its
  names.
- **Discover reaches it on the session bus with `DISCOVER_TEST_SYSUPDATE=1`,** which is where
  `kdos-sysupdated --session --root <dir>` serves it, so the whole flow can be driven without root.
- **The update document belongs to the root that wrote it.** `/var/lib/kdos/update.json` is in
  each slot, so the first boot of a slot, or the boot a rollback returns to, finds one written
  before that root was what it is now. A document whose `installed` is not the running version
  offers nothing, and the daemon rewrites it at start with `kdos update check --out`, which reads
  the tree and index already on the machine and downloads nothing.
- **The AppStream file lives on `/run`.** `/run/kdos/sysupdate/io.kdos.system.metainfo.xml` is
  regenerated whenever `/var/lib/kdos/update.json` changes; the copy in `/usr/share/kdos/update/` is
  the fallback that names the system and offers nothing.
