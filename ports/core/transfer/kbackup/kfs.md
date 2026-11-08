## About

KBackup archives a chosen set of folders and files into tar archives, compressed or not, on a
local disk, a mounted drive or any KIO location such as `sftp:/`. A selection, its target and its
filters are saved as a profile (a `.kbp` file, whose MIME type the port installs), and a backup
can be full or take only what changed since the last full one. Restoring is unpacking the
archives with [ark](#port-ark) or `tar`.

## Flags

- `-D CMAKE_REQUIRE_FIND_PACKAGE_Qt6Core5Compat=ON` — Make [qt6-qt5compat](#port-qt6-qt5compat)
  a requirement. Upstream looks for it without `REQUIRED` but links `Qt6::Core5Compat`
  unconditionally, so without it the configure succeeds and the link fails.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON` — The handbook, through
  [kdoctools](#port-kdoctools), which upstream looks for as optional; required, a missing one
  stops the configure instead of leaving the handbook out.
- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

KBackup has no scheduler of its own. `kbackup --autobg <profile>` runs one profile without a
window, and a schedule is a user crontab line that runs it, which [cronie](#port-cronie) serves
and [kcron](#port-kcron) edits. KDOS's own backups are `kdos backup` over restic and are not this
program.

`update_xdg_mimetypes` in upstream's install does nothing under `DESTDIR`, so the package carries
only `kbackup.xml` and `kpkg` regenerates the shared MIME database on install (see
[Shared indexes](../../../../docs/kdos/05-developer/writing-ports.md#shared-indexes)).
