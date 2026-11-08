## About

Baloo is KDE's file search. Its library, `KF6::Baloo`, answers queries by name, type, tag and
content from an index kept in an LMDB database ([lmdb](#port-lmdb)) under
`~/.local/share/baloo/`, and reads and writes the tags and ratings KDE applications attach to
files. The port also builds the per-user indexer that fills that index: `baloo_file`, which walks
the indexed folders, records every file's name and type and follows changes through inotify, and
`baloo_file_extractor`, which it starts to read each file's text and metadata through
[kfilemetadata](#port-kfilemetadata). Around them come `balooctl6`, `baloosearch6` and
`balooshow6`, the `baloosearch:`, `tags:` and `timeline:` KIO workers, the kded module that
refreshes an open search folder, and the QML modules `org.kde.baloo`. KRunner's file search in
[plasma-workspace](#port-plasma-workspace), [dolphin](#port-dolphin)'s search, information panel
and tags, and the music collection of [elisa](#port-elisa) are built on it.

## Flags

- `-D BUILD_INDEXER_SERVICE=ON` — Build `baloo_file`, `baloo_file_extractor`, `balooctl6` and the
  indexer's D-Bus interfaces, and install `/etc/xdg/autostart/baloo_file.desktop`, which starts the
  indexer with each Plasma session. Without them the library only queries an index nothing
  writes, and every search, tag view and timeline is empty. Upstream's default; it is named so the
  service cannot drop out unnoticed.
- `-D BUILD_EXPERIMENTAL=OFF` — Build no `baloodb`, a tool for inspecting the database by hand,
  which upstream marks experimental. The `org.kde.baloo.experimental` QML module, which the Plasma
  search settings page reads the indexer's progress from, is not behind this switch and is built.
- `-D KF_SKIP_PO_PROCESSING=ON` — `ki18n_install(po)` returns at once, so none of the message
  catalogues in `po/` is compiled or installed. kpkg removes every non-English catalogue from the
  package anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)),
  so the switch only saves build time.

## Notes

The indexer, the KIO workers and the command-line tools are built only when the configure finds
inotify. ECM's `FindInotify` reports the kernel interface found when `sys/inotify.h` exists, which
musl provides, and reports nothing at all when it does not, so not even
`CMAKE_REQUIRE_FIND_PACKAGE_Inotify` would stop a build without it. `build.sh` therefore checks
for `/usr/lib/libexec/kf6/baloo_file` after the install and stops the build when it is missing.

The install puts a systemd user unit, `kde-baloo.service`, under `/usr/lib/systemd`; `build.sh`
removes the directory, since KDOS has no systemd (see
[No systemd](../../../../docs/kdos/01-philosophy/principles.md#no-systemd-init-units-or-service-manager)).
The autostart entry starts the indexer instead, and only while `~/.config/baloofilerc` has
`[Basic Settings] Indexing-Enabled=true` (its `X-KDE-autostart-condition`; true when the key is
absent); `baloo_file` reads the same key when it starts and exits at once when it is false. The
unit's CPU and I/O weights and its memory ceiling do not apply without it. What bounds the indexer
is its own start-up: `baloo_file`, and each `baloo_file_extractor` it starts, drops to nice 19 and
the idle I/O class. Each also asks `sched_setscheduler()` for a lower scheduling class,
`SCHED_BATCH` for the indexer and `SCHED_IDLE` for the extractor; musl does not implement that
call and fails it with `ENOSYS`, so both keep the normal class and nice 19 is the whole of the CPU
bound. The indexer asks `org.freedesktop.PowerManagement` on the session bus whether to save power,
which PowerDevil answers true on battery, and does no content indexing and no clean-up of stale
entries while it should; with no answer on the bus it treats the machine as on mains power.

What is indexed is read from `baloofilerc`. The keys are `[General] only basic indexing` (names
and types, no file contents), `folders` (the home directory when unset), `exclude folders`,
`exclude filters`, `exclude mimetypes` and `index hidden folders` (off when unset, so `~/.cache`,
`~/.local` and every other dot-directory are skipped). Upstream's defaults already skip version
control directories, CMake and Ninja build trees, `node_modules`, Python environments, object
files and disk images. A configured `exclude filters` list has upstream's appended to it as long
as `exclude filters version` is older than upstream's, or absent, so a distribution default adds
patterns without dropping upstream's. KDOS's distribution defaults for this file live in the XDG
configuration cascade under `/usr/share/kdos/xdg`, not in this port. Folder lists are read as
paths, so `$HOME` in them is expanded.
