## About
[GNU Health]{.idx} is a hospital and clinic information system: patients, appointments,
laboratory, pharmacy, imaging, genetics and stock. It is a set of modules for the Tryton
application server, `trytond`, which keeps its data in [PostgreSQL](#port-postgresql) and
serves the Tryton desktop client on TCP port 8000. The port installs the server, the Tryton
modules GNU Health builds on, and GNU Health's own modules, in a prefix of their own,
`/usr/lib/gnuhealth`, and the service `85_gnuhealth`, which stays off until a database is
configured ([Services](../../../../docs/kdos/02-user-guide/administration.md#services)).

## Flags
- `--prefix=$_home` — Install every Python distribution of the bundle under
  `/usr/lib/gnuhealth` instead of `/usr`, scripts in its `bin/` and modules in its own
  `site-packages`. Nothing outside the port finds them there; the `launch` wrapper puts that
  prefix on `PYTHONPATH` for each command.

## Notes
- **The vendor bundle is the `pypackages` closure.** `pypackages` names every distribution the
  bundle carries, several pinned (`trytond:7.0.57` and the `trytond-*` modules, `proteus`,
  `gnuhealth-control`); `pyrequirements = no` and `pyruntime = no` keep `ports/fetch` from
  resolving more. Everything else GNU Health imports at run time (psycopg2, lxml, dateutil,
  pytz, Pillow, qrcode, matplotlib and the rest) is a port in `depends`. The `pip3 install` line
  must name the same set; a name missing there is in the bundle and never installed. `vobject` is
  pinned to 0.9.8 because 0.9.9's metadata imports dateutil, which the isolated fetch build
  lacks.
- **Three modules are left out.** The loop over `tryton/health*` installs every GNU Health module
  in the source except `health_federation`, `health_genetics_uniprot` and `health_orthanc`.
  `health_orthanc` talks to an Orthanc PACS through `pyorthanc`, which is neither a port nor in
  the bundle; `health_federation` talks to Thalamus, the GNU Health Federation message server,
  through `requests`, which `depends` does not name; `health_genetics_uniprot` is data only, the
  UniProtKB variant tables.
- **Translations are deleted by hand.** Tryton modules carry their translations as `.po` files
  under `trytond/modules/*/locale/`, which the tree-wide language prune does not match (it
  removes `.mo`, `.gmo` and `.qm` catalogs), so `build.sh` deletes them itself; see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy).
- **The commands are one wrapper.** `trytond`, `trytond-admin`, `trytond-cron`,
  `trytond-worker`, `trytond-console`, `trytond-stat` and `ghcontrol` in `/usr/bin` are symbolic
  links to `/usr/lib/gnuhealth/libexec/launch`, which computes the prefix's `purelib` and
  `platlib` directories with `sysconfig`, adds them to `PYTHONPATH`, sets `TRYTOND_CONFIG` to
  `/etc/gnuhealth/trytond.conf` unless it is already set, and runs the real script of the same
  name. The `test -x` before each link stops the build if pip did not install that script.
- **The service waits for configuration.** `85_gnuhealth` skips itself while the `gnuhealth`
  account is missing or `trytond.conf` has no `uri` line, and otherwise runs `trytond` under the
  supervisor as `gnuhealth` through `setpriv`. The shipped `trytond.conf` lists the commands that
  create the database role and the database and initialise it; its `uri` is commented out.
  The database URI names no user, because PostgreSQL's peer authentication over the local
  socket maps the `gnuhealth` account to the role of the same name.
- **The postinstall writes into the target root.** `postinstall.sh` creates the `gnuhealth`
  group and system account under `PKG_ROOT`, reads the ids back out of that root's
  `/etc/passwd` to `chown` `/var/lib/gnuhealth` and its `attach/` store, and sets both to `0750`,
  because `kpkgadd` creates every directory `0755`; see
  [postinstall.sh](../../../../docs/kdos/05-developer/writing-ports.md#postinstallsh).
