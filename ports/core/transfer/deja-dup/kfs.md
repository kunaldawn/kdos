## About
[Déjà Dup]{.idx} is a GTK 4 and libadwaita front end for scheduled, encrypted backups. It stores
nothing itself: [restic](#port-restic) writes and reads the backups, [rclone](#port-rclone)
reaches the cloud locations, and the setuid `fusermount3` from [fuse](#port-fuse) mounts a
snapshot for browsing. Each is found by name at run time, which is why all three are in `depends`
though none is linked. The passphrase is kept through libsecret, by whatever answers
`org.freedesktop.secrets`.

## Flags
- `-Dpackagekit=disabled` — Build without PackageKit, through which Déjà Dup offers to install a
  missing backup tool from the distribution's repositories when it is needed. The option is
  `auto` upstream; PackageKit is not a port, and kpkg installs packages here.

## Notes
- **Duplicity is not a port.** Upstream lists it as a run-time dependency for reading and writing
  existing Duplicity backups, and makes new backups with restic. Here every backup is a restic
  one, and a Duplicity backup made elsewhere cannot be restored; it is a row of
  [Features whose library has no port](../../../../docs/kdos/06-reference/known-gaps.md#features-whose-library-has-no-port).
- **The scheduler is not started by the session.** `build.sh` deletes upstream's
  `/etc/xdg/autostart/org.gnome.DejaDup.Monitor.desktop`: it names no `OnlyShowIn`, and Plasma
  would start `/usr/libexec/deja-dup/deja-dup-monitor` at every login, which activates dconf and
  four gvfs volume monitors even with "Back up automatically" off. Kup, the Backups page in System
  Settings, is the desktop's scheduled backup. "Back up automatically" backs up only while
  `deja-dup-monitor` runs, started by hand or from an autostart entry the user adds in System
  Settings' Autostart page; "Back Up Now" and restores work without it.
- **The menu icon.** Upstream installs the application icon as `org.gnome.DejaDup.svg` only, and
  the panel reads hicolor PNGs, so `build.sh` renders 48, 64 and 128 pixel PNGs with `rsvg-convert`.
- **The menu entry is replaced.** The GtkApplication id, `org.gnome.DejaDup`, is the Wayland
  app_id and the `StartupWMClass`. Upstream's `MimeType` line, which claims the two OAuth
  callback URL schemes a Google Drive or OneDrive location signs in through, is left out, and so
  is `DBusActivatable`, so the launcher runs the program named in `Exec`. Those two locations
  need a browser sign-in and a network; a local disk, a folder and a restic repository do not.
- **English only.** The help keeps its `C` pages and every translated one is removed.
