## About

GVfs is GIO's virtual filesystem: a set of daemons, one per kind of location, that let any program
using GIO open `sftp://`, `smb://`, `dav://`, `ftp://`, `mtp://`, `gphoto2://`, `trash:///` and
`recent:///` locations as if they were local, and volume monitors that list disks, phones and
cameras. Through it a GTK application's file chooser can browse network shares and devices. The
daemons start on demand over D-Bus.

## Flags

- `XML_CATALOG_FILES=/etc/xml/catalog` — Names the XML catalogue libxml2 reads, so that
  `xsltproc` finds the installed [docbook-xsl](#port-docbook-xsl) manual-page stylesheet by its URL
  when `-Dman=true` builds the manual pages, instead of trying to fetch it.
- `--libexecdir=lib/gvfs` — Install `gvfsd`, each `gvfsd-<backend>` and the volume monitors in
  `/usr/lib/gvfs`. The D-Bus service files and mount descriptions that start them are generated
  with this path.
- `-Dsystemduserunitdir=no`, `-Dtmpfilesdir=no` — Install no systemd user units and no tmpfiles
  configuration. Left empty, either one makes the configure ask pkg-config for systemd's directory
  and stop when systemd is not found. KDOS has no systemd (see
  [No systemd](../../../../docs/kdos/01-philosophy/principles.md#no-systemd)).
- `-Dlogind=false` — Build without logind. With it, the udisks2 volume monitor asks
  `sd-login` which seat the session is on and lists only the drives of that seat; without it, it
  assumes one seat for the whole machine. Neither libsystemd nor libelogind is a port.
- `-Dbusy_processes_command=lsof` — Name the processes that keep a volume from unmounting with
  `lsof -t`, from [lsof](#port-lsof). It is upstream's default; the alternative is `fuser`.
- `-Dadmin=true` — Build the `admin://` backend, which opens files as root after a
  [polkit](#port-polkit) check, with [libcap](#port-libcap).
- `-Dafc=true` — iPhone and iPad file access over AFC, through
  [libimobiledevice](#port-libimobiledevice).
- `-Darchive=true` — Browse inside archives as folders, through [libarchive](#port-libarchive).
  Upstream marks the option deprecated and leaves it off.
- `-Dcdda=true` — Audio CDs as folders of WAV files, through [libcdio](#port-libcdio) and
  [libcdio-paranoia](#port-libcdio-paranoia).
- `-Ddnssd=true` — Find network services announced over DNS-SD, through [avahi](#port-avahi).
- `-Dgphoto2=true` — Cameras over PTP, through [libgphoto2](#port-libgphoto2).
- `-Dhttp=true` — The `http://` and WebDAV backends, through [libsoup3](#port-libsoup3).
- `-Dmtp=true` — Phones and players over MTP, through [libmtp](#port-libmtp).
- `-Dsftp=true` — SFTP, which runs the `ssh` of [openssh](#port-openssh).
- `-Dsmb=true` — Windows and Samba shares and network browsing, through [samba](#port-samba)'s
  client library.
- `-Dudisks2=true`, `-Dgudev=true` — The volume monitor for local disks and removable media,
  through [udisks2](#port-udisks2), which needs gudev ([libgudev](#port-libgudev)).
- `-Dbluray=true` — Read a Blu-ray disc's title and icon for its volume, through
  [libbluray](#port-libbluray).
- `-Dfuse=true` — Build `gvfsd-fuse`, which exposes every GVfs mount as a directory under
  `$XDG_RUNTIME_DIR/gvfs`, so a program that does not use GIO can open its files. It needs
  [fuse](#port-fuse) 3.
- `-Dkeyring=true` — Store and look up passwords for network locations through
  [libsecret](#port-libsecret).
- `-Dlibusb=true` — Let the MTP backend interrupt [libusb](#port-libusb)'s event handling when a
  device is unmounted, so the backend's event thread can finish.
- `-Dgcrypt=true` — libgcrypt support. Only the AFP backends use it, and they are off, but on it
  makes [libgcrypt](#port-libgcrypt) a requirement of the configure.
- `-Dafp=false`, `-Dgoogle=false` — No Apple Filing Protocol and no Google Drive backends. Both
  are deprecated upstream and off by default; Google Drive needs libgdata and GNOME Online
  Accounts, neither of which is a port.
- `-Dgoa=false`, `-Donedrive=false` — No GNOME Online Accounts volume monitor and no OneDrive
  backend, which needs GOA and msgraph. Neither GNOME Online Accounts nor msgraph is a port.
- `-Dgcr=false` — No certificate prompts through gcr 4, which is not a port.
- `-Dnfs=false` — Build no NFS backend, which would use libnfs.
- `-Dwsdd=false` — Build no WS-Discovery backend, which finds Windows machines on the network by
  running the external `wsdd` daemon; `wsdd` is not a port.
- `-Ddevel_utils=false`, `-Dunit_tests=false` — Build neither the development utilities nor the
  unit tests; both are upstream's defaults.
