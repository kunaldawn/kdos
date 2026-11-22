# KDOS v0.2 acceptance findings

Working list for the v0.2 release round. Not a docs page.

The released ISO (`build/iso-build/kdos.iso`, VERSION `0.2+21b67721d`, sha256 `04f16a5c…563a9e`,
equal to the published `SHA256SUMS` line) was tested as an end user would meet it, in 15 lanes:
installed, variants, installer, updates, packages, boot, desktop, apps, settings, kdosdesk, cli,
media, security, release and docs. Thirteen lanes booted it in `kdos-qemu-py` under KVM through
`testing/vnc-shot.py` and small lane drivers built on its RFB, serial and monitor code: about 155
boots in all, UEFI (OVMF) and SeaBIOS, CD and USB, installs onto qcow2 disks with
`install-to-disk.sh` and with the graphical installer (plain, LUKS, one slot, OEM, BIOS), an
image-lane update from a signed mock 0.2.1 stick, and the real GitHub v0.2 release for update,
binhost and asset checks. The release and docs lanes worked on the host against the ISO's squashfs,
the GitHub API and the tree. Visual testing was screenshot analysis: every rig shot (raw PPM) was
converted, cropped and scaled 2–6×, pixel-sampled for contrast, and diffed across states (Tab-focus
sequences, panels across boots, before and after a click); verifiers re-viewed the shots they cite.
A second pass verified each finding against the tree, logs and shots and set the verdict and
severity used here. Rig limits: llvmpipe only (no GL), a host shared by several VMs (timings are
inflated), OVMF loaded with `-bios` (NvVars on the ESP), hard power-offs, and `--cmd` processes
outside the seat0 session.

**Counts:** 261 lane findings → 211 confirmed, merged into **162** (3 blocker, 12 major, 67 minor,
80 polish) · **8** uncertain (7 lane findings and 1 lead found while verifying) · **43** refuted.
44 fix units (35 code, 9 docs).

Path shorthand: `shots/` = `/home/kunaldawn/workspace/repos/kdos/build/shots/e2e/`, `logs/` =
`/home/kunaldawn/.cache/kdos-portwork/e2e/`. Fix units (`U..`, `D..`) are in the table after the
findings.

---

## Blocker

### F01 · Prebuilt software never installs: the image's `/ports/TREE` names no release
ids: packages-01, cli-01, installed-13, variants-01, installer-01, installer-02, updates-04, kdosdesk-01 (first wall), media-02, security-01

- **Sees:** every `kdos update install <port|group>` (CLI, Discover's KDOS-system lane, the
  installer's groups, `--pending`, `kdos firmware sync`) fails with "no signed binhost index is on
  this machine; `sudo kdos update fetch --binhost-only` fetches the release's signed index". That
  command exits 0 in under a second, prints nothing and fetches nothing, so the advice loops. A
  full `kdos update fetch` says "the ports tree is current (21b67721d)". The GUI installer's Done
  page says "the release's binary host could not be reached" on a working network, groups are
  deferred to a pending list that cannot install, and "no firmware beyond the medium's was
  installed". Discover offers GIMP only as a Debian box; Make It Yours shows no sizes; Orca, Haruna
  and CJK engines cannot be added. `KDOS_UPDATE_BINHOST=…/releases/download/v0.2` fetches the
  signed index (2440 packages, key 68d153e8bd271d30) in 2–5 s and installs work, so the release is
  fine and the image is not.
- **Repro:** boot the ISO online; as root `kdos update fetch --binhost-only; echo $?; cat
  /var/lib/kdos/update.state; cat /ports/TREE; kdos update install kcharselect`.
- **Evidence:** `logs/packages/run1.log` (36, 85, 124-200), `logs/packages/run2.log` (TREE, 50),
  `shots/packages/11-discover-search-gimp-noindex.view.png`, `shots/packages/12-discover-gimp-page-noindex.view.png`,
  `logs/cli/run2.clean.log` (155-160, 195-203, 217-219, 240-259), `logs/cli/run3.clean.log`,
  `logs/installed/run4.log`, `logs/installed/run2.log`, `logs/variants/run7.log`, `run9.log`,
  `run10.log`, `run14.log`, `run21.log`, `shots/variants/r9-00-kickoff-mines.png`, `r9-01-kmines.png`,
  `r21-00-kickoff-mines.png`, `shots/installer/63-apps-kdesystem.png`, `64-summary-final.png`,
  `73-done.png`, `73-done.crop-890-475-820-110-x2.png`, `85-discover-selected.png`,
  `165-small-apps-lang.png`, `113-luks-done.png`, `122-oem-done.png`,
  `logs/installer/kinstall-live.log` (113-116, 161), `logs/updates/run2h.log`,
  `shots/updates/96-github-refresh-28s.png`, `shots/updates/notes.md` (20:13),
  `logs/kdosdesk/run9.log`, `run10.log`, `run11.log`, `shots/kdosdesk/c-w3-miy.png`,
  `r9-01-miy.png`, `r4-07-disc-refreshed.view.png`, `shots/media/notes.md` (run4),
  `logs/security/run4.log`, `run5.log`, `run8.log`.
- **Cause:** (a) `script/phases/70_image/105_ports.sh:141-147` writes TREE with `kdos-tree`,
  `commit` and `time` only; only `ports/publish:1655` writes `version <tag>`. `tree_version()`
  (`update.c:367-395`) reports unnamed, `binhost_base()` (`update.c:1982-1998`) returns -1,
  `binhost_fetch()` (`update.c:2120-2126`) records `absent` and returns 0 silently.
  `tree_fetch()` (`update.c:1944-1952`, short-circuit `:1893-1901`) keeps the unnamed stamp
  because the published tree is the same commit (tag v0.2 = 21b67721d), so nothing repairs it. The
  install hint (`update.c:5426-5431`) names the command that just did nothing. (b) The graphical
  installer runs kinstall under pkexec, whose PATH has no `/usr/local/bin`; `kdos` lives only
  there, so `execvp("kdos")` in `run_full()` (`install.c:532`, from `fetch_index()` `:1740-1754`)
  exits 127 unlogged and is reported as an unreachable binhost (the terminal installer works:
  sudo's secure_path has `/usr/local/bin`).
- **Fix:** U01: pass `KDOS_GIT_TAG="$(git describe --tags --exact-match HEAD)"` from the Makefile,
  name it in `exec.sh`'s `env -i` list and header, write `version $KDOS_GIT_TAG` into TREE when set
  and the tree is clean; rewrite the 105_ports.sh header. U02: in `tree_fetch`, a same-commit
  fetched TREE that names a release replaces an unnamed one (and skip the signature short-circuit
  then); `fetch --binhost-only` prints why nothing is named and exits 1; the install refusal names
  `binhost =` / `sudo kdos update fetch` when the state is `absent`. U03: `ports/publish --release`
  refuses an ISO whose `/ports/TREE` lacks `version <tag>`. U08: kinstall sets a fixed PATH
  (`/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin`) in `kdos-installer/main.c`, and
  `run_full()`'s child writes `cannot run <argv0>: <strerror>` before `_exit(127)`. Docs D4, D5,
  D6, D8, D9 (packaging.md:1549-1553/1759, kdos-command.md:1255/1273, kinstall.md graphical
  section, how-kdos-is-built.md:747, developing.md release steps, build-system.md and
  filesystem-and-ipc.md env tables, configuration.md:595, known-gaps workaround, kfs 105-ports.md
  and updating.md:224). For v0.2 machines already out: `binhost =
  https://github.com/kunaldawn/kdos/releases/download/v0.2` in `/etc/kdos/update.conf` (put it in
  the v0.2 release notes and known-gaps); the next release, a new commit, replaces `/ports` and
  names itself. Optionally re-pack the v0.2 ISO from the tag and replace the asset (user's call).

### F02 · Every image-lane update rolls itself back: the new slot has no `/boot/efi`
ids: updates-01

- **Sees:** after Discover's Update All and a restart, slot B runs but its ESP is not mounted;
  `kdos-bootctl` says "this machine has one root and no rollback", System Versions says one
  version, the logout screen gives no warning. The next restart boots A and kdos-notice says "Your
  last update didn't start". Re-applying deletes `/boot/efi` again. Everything written to the root
  during the B session is lost.
- **Repro:** install A/B, apply the mock 0.2.1 image from Discover, restart, use B, restart.
- **Evidence:** `shots/updates/41-greeter-slotb.png`, `44-slotb-sysversions.png`,
  `46-slotb-konsole.png`, `48-slotb-logout.png`, `53-slota-after-rollback-6s.png`,
  `56-slota-sysversions-failed.png`, `logs/updates/run2c.log`, `run2d.log`, `run2e.log`.
- **Cause:** `image.c:2139` `prune(&F, ".")` deletes every path the manifest does not name, and
  the image root has no `boot/efi`; `make_dirs` (`:1722`) recreates only manifest directories;
  `kdt_image_carry` (`:2706`, `carry_fstab :2534`) writes the ESP fstab line but no mount point.
  rcS `mount -a` cannot mount `/boot/efi`; `kdos-bootctl mark-good` loads no state and returns 0
  (`bootctl.c:3000`), so `bless.c:324` logs "confirmed" while the ESP still says `try=b`.
- **Fix:** U02: `kdt_image_carry` makes every absolute fstab mount point in the slot (0755 root)
  at every arm; selftest carry case with a `/boot/efi` line; `cmd_mark_good` returns 1 when
  `/proc/cmdline` names `bootstate=` but no state loaded. Docs D4 (packaging.md:1897,
  boot-and-init.md:687-688 and "Counting and confirming"), D9 (ab-slots-and-rollback.md:235).

### F03 · No screen reader, braille or speech tools on the medium
ids: media-01, media-06, docs-01

- **Sees:** Meta+Alt+S and the Welcome tour's "Read the screen aloud" switch start nothing and say
  nothing; the switch shows on. System Settings says Orca is not installed. `service start brltty`
  says the service does not exist. A blind user cannot use the live session or reach the
  installer; the install copies the same base.
- **Repro:** boot the ISO, press Meta+Alt+S; `command -v orca brltty`.
- **Evidence:** `shots/media/11-orca-chord.png`, `33b-sr-switch-later.crop-636-380-648-120-x2.png`,
  `64-kcm-screenreader.png`, `logs/media/run1.log`, `run2.log`, `shots/media/notes.md` (run4),
  `shots/docs/image-packages.txt`, `shots/docs/notes.md`,
  `src/desktop/kdos-welcome/extra-pages/00-Accessibility.qml:145-150`.
- **Cause:** `ports/base` names no accessibility port, so `017_stage.sh` drops the built orca,
  brltty and liblouis; accessibility.md:11-28/55-61/104-131 and known-gaps.md:43-50 promise them.
- **Fix:** U20: add `orca brltty liblouis` to ports/base with a comment line (~6 MiB compressed,
  327 MiB under the gate). Docs D1/D2/D4: name the screen reader and BRLTTY in the base lists
  (getting-started.md:28-31, applications.md:26-28, packaging.md:368); Dasher, KMouth, piper and
  Kontrast become install commands in accessibility.md (with F58).

---

## Major

### F04 · The "join the parts and check" block empties the downloaded ISO
ids: release-02

- **Sees:** pasting README's / getting-started's two-line block on the single-file v0.2 download
  truncates `kdos-v0.2.iso` to 0 bytes before `cat` fails, then `sha256sum` prints FAILED. The
  user loses 1.6 GB and is told the release is corrupt.
- **Repro:** put `kdos-v0.2.iso` and `SHA256SUMS` in one directory; paste README.md:82-83.
- **Evidence:** `shots/release/join-parts-trap.log`, README.md:80-86, getting-started.md:65-72.
- **Cause:** the conditional `cat kdos-<v>.iso.part* > kdos-<v>.iso` sits in the same block as
  the check; the shell opens the target with O_TRUNC. developing.md:988-990 has the same block.
- **Fix:** D1, D8: the check block holds only `sha256sum -c SHA256SUMS --ignore-missing`; the join
  is its own sentence and block, for a release whose assets are `.part01`, `.part02` … only
  (`cat kdos-<v>.iso.part?? > kdos-<v>.iso`).

### F05 · On the live medium no graphical path ever fetches the binhost index
ids: packages-02, kdosdesk-01 (second wall)

- **Sees:** Discover → Updates → Refresh answers "Up to date" and fetches nothing; Make It Yours
  keeps saying "connect, open Discover and press Check for Updates, then Look Again", which can
  never work. A live user without a terminal gets no native ports and no group sizes, even with
  F01 fixed.
- **Repro:** live ISO with `binhost =` set; Discover from Kickoff, Updates, F5, wait 30 s; `ls
  /var/lib/kdos/binhost`.
- **Evidence:** `shots/packages/60-discover-updates.view.png`,
  `61-discover-after-f5.crop-480-140-960-760.png`, `logs/packages/run6.log` ("after Discover F5"),
  `logs/kdosdesk/run11.log`, `shots/kdosdesk/r4-07-disc-refreshed.view.png`.
- **Cause:** `src/daemons/kdos-sysupdated/bus.c:435-440` answers CheckNew with "" on a machine
  with no second slot before any fetch; `KdosBackend::checkForUpdates` only re-reads files.
- **Fix:** U04: in `cont_checknew`'s no-slot branch spawn `kdos update fetch --binhost-only`
  (a CH_CHECK child, at most once per refresh-after) and answer "" when it ends; update the
  no-slot and live cases in `testing/fixtures/sysupdated/drive.sh`. Docs D6 (discover.md:319-322),
  D7 (daemons.md:1080-1083, plasma-shell.md:431).

### F06 · The privacy lamp says the screen is captured whenever a taskbar thumbnail shows
ids: apps-01

- **Sees:** hovering any task-manager entry lights the red screen lamp; `kdos privacy --json`
  lists plasmashell as a screen capture; it goes out when the pointer leaves.
- **Repro:** open Dolphin, hover its task entry, look at the tray, run `kdos privacy --json`.
- **Evidence:** `shots/apps/92-hover-task.png`, `92-hover-task.crop-300-820-1620-260.png`,
  `93-after-hover.png`, `82-firefox-addons.png`, `82-firefox-addons.crop-1530-1037-390-40-x4.png`,
  `logs/apps/run9.log`.
- **Cause:** `src/system/kdos-tools/privacy.c:318-372` (scan_pw) counts every active link out of a
  KWin Stream/Output/Video node, including the shell's own thumbnail stream.
- **Fix:** U12: skip a link whose reader's `application.process.binary` is plasmashell when the
  stream is KWin's, with the rule in a comment; add a plasmashell reader to
  `testing/fixtures/kdos-command/pw-dump.json` (the assertion stays `[pw:60:61, pw:70:72]`).
  Docs D2 (desktop.md:512-515), D5 (kdos-command.md:1142), D7 (plasma-shell.md:121-123).

### F07 · `service list` and `ksvc check` as a user report every root daemon as FAIL
ids: cli-02

- **Sees:** as kdos, 15 daemons (dbus, elogind, polkitd, NetworkManager…) show `[FAIL]
  (supervisor gone; daemon pid N still running)`; `ksvc check dbus` exits 1. Root sees `[ OK ]`.
- **Repro:** `service list` as kdos, then as root.
- **Evidence:** `logs/cli/run1.clean.log` (1015-1056, 1076-1078), `logs/cli/run2.clean.log`
  (10-57), `shots/cli/38-tty2-misc.png`, `shots/cli/60-desktop-konsole-timezone.png`.
- **Cause:** `ksvc.c:291-294` `alive()` is `kill(pid, 0) == 0`, which fails with EPERM for a
  caller that does not own the supervisor.
- **Fix:** U14: `kill(pid, 0) == 0 || errno == EPERM`, with the constraint comment.

### F08 · A video opens no player; VLC on the base is unnamed and claims nothing
ids: media-03, docs-05, apps-06, packages-16

- **Sees:** Enter on an .mp4 in Dolphin opens a "Choose Application" grid with nothing
  preselected; VLC (on the base for Elisa) is in it but not offered. desktop.md says videos open in
  Haruna "on the image"; no base list names VLC. Office documents and mail links also ask.
- **Repro:** `~/Videos/clip-h264.mp4`, Enter in Dolphin.
- **Evidence:** `shots/media/13-open-video.png`, `53-chooser-vlc.png`, `54-vlc-playing.png`,
  `shots/docs/image-mimeinfo.cache`, `logs/apps/run1.log` (VIS lines), `shots/apps/kpair-0.png`,
  `kpair-3.png`, `shots/packages/03-kickoff-vlc.crop-0-520-650-300-x2.png`, `08-vlc.view.png`,
  `logs/packages/run1.log` (groups).
- **Cause:** `fs/usr/share/kdos/xdg/kde-mimeapps.list:180-200` names only
  `org.kde.haruna.desktop`, which is outside ports/base; `vlc.desktop` (vlc build.sh:169-190) has
  no MimeType by design, so KService has no fallback.
- **Fix:** U21: each Haruna row names `org.kde.haruna.desktop;vlc.desktop;`; vlc kfs.md:93-96 says
  the table names VLC as the fallback. Docs D1/D2: base lists name VLC ("which Elisa plays
  through"); desktop.md:415-419 says videos open in VLC until Haruna is installed and office
  documents and mail links ask until an application is installed; applications.md:377/614.

### F09 · First login sometimes comes up with Plasma's default panel instead of the KDOS desk
ids: desktop-01, boot-01, variants-04, apps-02, kdosdesk-02, media-05, security-09 (uncertain alone)

- **Sees:** about 1 first login in 7 (2/13, 1/16, 2/14, 2/12, 2/7, 2/4 installs, 1/7 across
  lanes): Start without its label, Plasma's stock launchers (System Settings, Discover, Dolphin,
  Firefox), meters in the text face, Kickoff favourites without Konsole; the desktop containment
  loses `plugin=` too. It persists for the account; `kdos desk status` reports `panels_edited:
  false`; `kdos desk kdos` repairs it.
- **Repro:** boot the ISO fresh repeatedly (or fresh installs) and look at the panel.
- **Evidence:** `shots/desktop/36-notify-root.crop-900-1030-1020-50-x2.png`,
  `40-krunner-app.view.png`, `114-start.crop-0-1030-1920-50.png`, `logs/desktop/run9.log`, run4-14,
  `shots/boot/ev-18-panel-race-stock-layout.png`, `ev-19-panel-race-meters-zoom.png`,
  `70-tray-checklist.png`, `72-back-to-tty1.png`, `73-clean2-desktop.png`, `74-race1..3-desktop.png`,
  `logs/boot/run17-{1,2,3}.log`, `shots/variants/panel-compare6-left.png`, `r5-10-session.png`,
  `r13-06-session95.png`, `r15-03-session.png`, `logs/variants/run16.log`,
  `shots/apps/panel-compare.png`, `100-spectacle-saved.png`,
  `100-spectacle-saved.crop-0-1035-700-45-x3.png`, `110-panel-at-start.png`,
  `logs/apps/run12.log`, `run13.log`, `shots/kdosdesk/c-r14-panel.png`, `r14-panel.png`,
  `c-r6-panel-l.png`, `c-r6-panel-r.png`, `c-r6-07-panel.png`, `logs/kdosdesk/run14.log`, `run15.log`,
  `run7.log`, `shots/media/93-lamp-middle.png`, `20-scale200-desktop.crop-640-1345-1300-95-x2.png`,
  `21-kickoff-200.png`, `logs/media/run6-appletsrc.txt`, `shots/desktop/02-kickoff.png`,
  `shots/security/10-konsole.view.png`, `panels-compare.png`, `panels-compare2.png`.
- **Cause (desktop-01 verifier, best supported):** plasma-workspace's kconf_update step
  `migrate-calendar-plugins` (`/usr/share/kconf_update/migrate-calendar-to-plugin-id.py:34-61`)
  rewrites `plasma-org.kde.plasma.desktop-appletsrc` in place (readlines, truncate(0), buffered
  write) on every first run. kded6 runs it in the same second as the first-login layout
  (`kconf_updaterc` mtime 0.8-1.1 s before kdosrc LaidOut in all 8 recorded runs). A plasmashell
  sync or reparse that lands in the empty window keeps only unsaved entries. Other verifiers'
  hypotheses (writeConfig without reloadConfig, applet init re-saving) are the fallback if the
  rerun still loses settings. Separately, `desk.c:254-265` (PANELS_JS) digests only panel
  geometry and plugins, so a desk that lost its settings reads as untouched.
- **Fix:** U10: append `[migrate-calendar-to-plugin-id.upd]` `done=migrate-calendar-plugins` to
  `fs/usr/share/kdos/xdg/kconf_updaterc` and rewrite its header for both steps; extend PANELS_JS to
  digest Kickoff's menuLabel, the task manager's launchers and the meters' chartFace so `kdos desk
  status` reports a lost desk. Docs D7 (plasma-shell.md "How a desk is laid out", kwin.md:202-207),
  D8 (configuration.md:386, :134), D5 (kdos desk status), D9 (repin
  shortcuts-and-window-rules.md:59). Proof: 20+ fresh boots, all with `menuLabel=Start`,
  `launchers=`, `chartFace=org.kdos.meters` and `[Containments][1] plugin=org.kde.plasma.folder`.

### F10 · A slot written from an image has `/` and `/ports` owned by uid 1000
ids: found while verifying updates-01; confirmed against `build/release/v0.2/image/kdos-image.manifest`

- **Sees:** nothing visible. The published v0.2 image manifest line 9 is `d 0755 1000 1000 … .`
  and line 786 `d 0755 1000 1000 … ports` (11,063 entries owned by 1000). `finish()`
  (`image.c:2045-2058`) lchowns the slot to the manifest, so after an image update the first
  account owns the root directory (can rename `/etc`) and the recipe tree root builds from.
  The image also carries `/kdos` and lacks `/mnt` and `/media`.
- **Repro:** `sed -n '9p;786p' build/release/v0.2/image/kdos-image.manifest`; apply an image and
  `stat -c '%u %n' /mnt/b /mnt/b/ports`.
- **Evidence:** `build/release/v0.2/image/kdos-image.manifest`, `build/release/mock/image/kdos-image.manifest`.
- **Cause:** `script/release-image.sh:111` overlays `build/image-ports` (builder-owned) as the top
  lowerdir, so the overlay root and `ports/` take the builder's uid; `kdos image make` records it.
  `110_iso.sh:131-141` adds `/mnt`, `/media` and excludes `/kdos`; release-image.sh does not.
- **Fix:** U02: release-image.sh mounts or copies the ports layer root-owned and applies the same
  include and exclude set as 110_iso.sh; `kdos image make` (and `kdos image verify`) refuse a
  manifest whose `.`, `etc`, `usr`, `ports` or anything beneath is not uid 0. The published
  image-v0.2 asset carries the fault: rebuild and replace it (or withdraw it) before any machine
  applies it. Docs D4 (packaging.md image section).

### F11 · After an image update, an account named `kdos` logs in with no password
ids: updates-02

- **Sees:** a machine installed without autologin boots slot B straight into the `kdos` session
  after the update, every boot.
- **Repro:** unattended install (account `kdos`), apply the 0.2.1 image, restart.
- **Evidence:** `shots/updates/01-greeter.png`, `41-greeter-slotb.png`, `46-slotb-konsole.png`,
  `logs/updates/run2c.log`.
- **Cause:** `image.c:2639-2646` (carry_etc) renames the slot's differing copy to
  `<file>.kdos-new` in the same directory; the image root ships the live
  `fs/etc/sddm.conf.d/kde_settings.conf` ([Autologin] User=kdos). SDDM 0.21 loads every non-hidden
  file of `sddm.conf.d`, so `kde_settings.conf.kdos-new` is read last and wins. Same hazard for any
  `*.d` directory read whole (cron.d, 00-kdos.conf edits).
- **Fix:** U02: inside a directory whose name ends `.d`, keep the slot's copy as the dotfile
  `.<file>.kdos-new`; selftest case. Docs D1 (installation.md:715), D3 (administration.md:1203),
  D4 (packaging.md:1895, boot-and-init.md:686-687), D8 (filesystem-and-ipc.md `.kdos-new`), D9
  (ab-slots-and-rollback.md:233), image.c header comment (:2203-2215).

### F12 · LibreOffice in Discover is an icon-less "LibreOffice Base"; Writer, Calc, Impress only as Debian boxes
ids: packages-09

- **Sees:** a search for libreoffice gives "LibreOffice Base" twice (native with a placeholder
  icon, plus a Debian box), then Calc, Draw, Impress and Math only as "built in a box".
- **Repro:** with the binhost index present, search Discover for libreoffice.
- **Evidence:** `shots/packages/74-discover-search-lo.crop-480-140-960-760.png`,
  `75-lo-page.view.png`.
- **Cause:** `kdos-discover/catalog/main.cpp:332-357`: a metainfo component without a desktop-id
  `<launchable>` (LibreOffice's legacy ids) gets no icon and its entry is emitted a second time
  (`:360-381`); `KdosBackend.cpp:413-417` keeps the first component per package. The Debian twins
  are F43.
- **Fix:** U07: treat `usr/share/applications/<id>` owned by the package as the launchable; per
  package prefer a desktop-application with an icon and the shortest name. Regenerate
  `kdos-binhost.xml` with the next binhost build. Docs D6 (which component a package is listed by).

### F13 · LibreOffice fails to install on a 4 GB live session unless it is the first install
ids: packages-04

- **Sees:** after any other install, `kdos update install libreoffice` fails after ~58 s with
  "Failed to extract … libreoffice-26.8.0.3-1.tar.xz"; Discover shows only "Please try again
  later". kdos-oomd killed xz at 589 MB.
- **Repro:** 4 GB live session, install gimp, then libreoffice.
- **Evidence:** `logs/packages/run2.log` (30346), `run7.log` (50), `run8.log` (55), `run9.log`,
  `shots/packages/76-lo-confirm.crop-480-140-960-760.png`, `79-lo-result.view.png`.
- **Cause:** `kdos-kpkg/add.c:552-557` runs `tar -xpvf`; GNU tar's `xz -d` decodes with -T0 by
  default, and a -9/32 MiB-block package needs ~100 MB a thread; `kb_proc.c:84-90` drops the
  child's stderr, so the reason is lost.
- **Fix:** U19: extract with `--use-compress-program=xz -T1` (~70 MB bound) and set
  `kb_proc_verbose` around the extraction so tar/xz errors reach the terminal and Discover. Docs D4
  (packaging.md compression rows near :691 and :1416).

### F14 · Default answers on a disk the size check accepts: the disk is erased, then the install fails with ENOSPC
ids: variants-02

- **Sees:** on a 5.6–9.2 GiB disk the GUI offers a 4096 MiB swap file with no warning; the install
  partitions and erases, `fallocate` fails, a 660 MiB partial swap is made, `/etc/skel` copy fails,
  then F at Theme, exit 1. swap=none installs.
- **Repro:** 5700 MiB disk, default answers, `kinstall --config A --unattended --events json`.
- **Evidence:** `logs/variants/run1.log` (DEFAULT-SWAP, 268-309), `shots/variants/r1-03-gui-disk.png`.
- **Cause:** `conf.c:791-809` (ki_disk_check) compares only install + 768 MiB; `ki_target_check`
  (`:919-957`) does not check the swap a one-slot erase plan puts on the root (defaults
  `conf.c:349-350`); the swapfile step (`install.c:2182-2227`) goes through `try_()` and mkswaps a
  short file.
- **Fix:** U08: on PLAN_WIPE with one slot and swap > 0, refuse before partitioning when the root
  minus the swap and 32 MiB metadata is below need + headroom ("a 4096 MiB swap does not fit beside
  the install on /dev/vda: make it at most N MiB, or none"), shown in `--dump plan` and on Summary;
  a failed fallocate/dd unlinks the partial file and warns, never mkswaps it. Add a 5700 MiB case
  under `testing/fixtures/kinstall/`. Docs D1 (installation.md:42, :269, :294-298, :844), D6
  (kinstall.md:222, :645).

### F15 · Graphical installer: the disk cannot be chosen with the keyboard, and radio and checkbox focus is invisible
ids: media-04

- **Sees:** on the Disk page Tab never reaches the disk row and arrows do not select it; Next stays
  disabled ("Choose the disk to install on."), so a keyboard-only install cannot proceed. Focus on
  the Erase radio and the Updates checkbox changes no pixel (diffed Tab sequence).
- **Repro:** installer with a target disk, keyboard only.
- **Evidence:** `shots/media/97-inst-disk.png`, `103-t1.png … 103-t12.png`,
  `105-x-arrows.crop-640-220-840-680.png`, `shots/media/notes.md` (run6).
- **Cause:** `DiskPage.qml:191` uses `QQC2.RadioDelegate` (focusPolicy Qt.NoFocus); qqc2-desktop-style
  6.31 `private/FocusRect.qml` never sets `hasFocus`, so Breeze draws no focus frame for any QML
  RadioButton or CheckBox.
- **Fix:** U09: `focusPolicy: Qt.StrongFocus` on the disk delegate. U26: a qqc2-desktop-style patch
  that sets `hasFocus: true` in FocusRect.qml, with a Subject: line and the upstream MR in kfs.md.

---
## Minor

### F16 · No release notes anywhere: `kdos update notes` fails and What's New is always empty
ids: installed-04, cli-07
- **Sees:** `kdos update notes` → "this slot carries no release notes", exit 1; plasma-welcome logs
  the failure at login; What's New can only say "No release notes came with this version".
- **Evidence:** `logs/installed/run6.log`, `run5.log` (session warnings, line 122), `logs/cli/run1.clean.log` (939-944), `logs/cli/run3.clean.log`.
- **Cause:** `tree_notes()` (`update.c:1329-1343`) reads `<tree>/RELEASE`; nothing writes it
  (no `ports/RELEASE`; `105_ports.sh:16-19, 64-67` and `ports/publish:1645-1660` copy none).
- **Fix:** U01: commit `ports/RELEASE` (Markdown notes of the version being built) and copy it in
  105_ports.sh. U03: add it to `release_tree()`'s archive list. Docs D5 (kdos-command.md:1259), D8
  (developing.md:914-918 and the release checklist, how-kdos-is-built.md:735-740).

### F17 · Installing applications raises a persistent "The desktop was updated — Log out" notice
ids: variants-10, packages-03
- **Sees:** after installing gimp, kde-games or any group in a running session: "The desktop was
  updated. Log out and back in…" with Log Out; after a Discover install "…Discover was updated too".
- **Evidence:** `shots/variants/r21-00-kickoff-mines.png`, `r21-01`, `r21-02-kpat.png`,
  `shots/packages/17-kcharselect.view.png`, `50-after-cli-gimp.view.png`, `53-progress2.view.png`,
  `logs/packages/run7.log` (14-17), `run5.log` (28).
- **Cause:** `restarts.c:115-150` (scan_proc) counts every deleted mapping under `/usr`, including
  the `icon-theme.cache`/mime cache an install's index trigger rewrites; kdos-notice then flags the
  desktop.
- **Fix:** U13: skip a mapping whose perms field has no `x`. Docs D5 (kdos-command.md:1107-1112).

### F18 · Prebuilt installs pull in build-only ports as run-time dependencies
ids: packages-07, cli-04
- **Sees:** kcharselect costs 17 packages / 31.7 MB including cmake, sphinx, pip and the Python
  build backends; kdos-kfs 389 MB including GHC and Rust; khelpcenter 315 MB including Rust,
  Node.js, CMake.
- **Evidence:** `logs/packages/run2.log` (105-147, 171-9465), `logs/cli/run3.clean.log` (125-194).
- **Cause:** `update.c:5460` resolves the prebuilt closure over `depends =` without skipping
  `ports/buildonly` (which `imageset.py:163/532` honours for the base).
- **Fix:** U02: the prebuilt route does not follow an edge into a buildonly port unless it was
  asked for or the route is `--source`; `087_gates.sh` runs `imageset.py --verify --all` so a
  package whose recorded needs are met only by a buildonly port fails the build; rewrite the
  ports/buildonly header. Docs D2 (applications.md dry-run text), D4 (packaging.md:63-66, :369,
  --prebuilt row :1938), D5 (kdos-command.md:1257).

### F19 · KDOS from Scratch and the Help Center are described as on the image
ids: cli-06, docs-04, release-09, desktop-11, apps-08, kdosdesk-05, kdosdesk-06, boot-09
- **Sees:** `/usr/share/doc/kfs` and khelpcenter are absent; README, kdos-command.md and the
  `kdos help` sheet say both books are on every image and in the Help Center; Dolphin's
  Library → Books holds one book; Welcome's Offline Library says "Both are installed, and open in
  the Help Center" and shows an empty "On this machine" heading; Help's "Open KDOS from Scratch"
  opens Firefox "File not found".
- **Evidence:** `logs/cli/run1.clean.log` (2182-2194), `logs/cli/run2.clean.log` (67-87),
  `shots/release/iso-contents.log`, `shots/desktop/116-library-books.half.png`,
  `102-library.crop-0-500-700-580.png`, `shots/apps/84-dolphin-books.png`,
  `shots/kdosdesk/sheet-w2-2.png`, `sheet-w2-3.png`, `w3-help-book.view.png`, `r11-06-kfs.png`,
  `sheet-r11.png`, `logs/kdosdesk/run3.log`, `shots/boot/ev-23-welcome-library-empty-section.png`.
- **Cause:** `ports/base:33-34` leaves out kdos-kfs on purpose and khelpcenter is in kde-system
  (`ports/groups:50-51`); the pages and `kdos.c:180` describe a full image; `50-Library.qml:49,
  52-61` and `80-Help.qml` do not check what is installed.
- **Fix:** U22: Library shows "On this machine" only when a tile's app exists and rewrites :49;
  Help's KFS button `actionEnabled: page.kfs.exists` (`kdos-library-kfs-book`). U15: `kdos.c:180`
  "the KDOS book, in the Help Center or the browser". Docs D1 (README.md:320-324, :339), D2
  (desktop.md:182, :382), D4 (packaging.md:75), D5 (kdos-command.md:165-166, :190-193), D7
  (plasma-shell.md:397, :400), D8 (filesystem-and-ipc.md:136, command-index.md kdos-doc row,
  repository-layout.md:261, ports-catalogue.md:5605).

### F20 · KDOS's own notifications vanish after 5 s and never reach the history
ids: desktop-02
- **Sees:** `kdos remind`, `kdos notify`, toggle notices and the runner's failure notice show for
  5 s and the bell stays "No unread notifications"; a reminder that fires while away is lost.
- **Evidence:** `shots/desktop/85-hist-during.half.png`, `38-notif-history.crop-1200-450-720-630.png`,
  `151-history-after-runner.crop-1200-450-720-630.png`, `161-history-after-remind.crop-1400-560-520-520.png`,
  `logs/desktop/run16.log`, `run7.log`.
- **Cause:** plasma-workspace's `/etc/xdg/plasmanotifyrc` sets `[Applications][@other]
  ShowInHistory=false`; KDOS posts with no resolvable desktop entry and no notifyrc
  (`kb_proc.c:444-465`, `kdos.c:2328`, `kdos-notice/main.c:160-170`, `kdos-runner/run.c:141-143`
  names `org.kde.krunner`, which is not an application entry).
- **Fix:** U15: ship `/usr/share/knotifications6/kdos.notifyrc` from kdos-tools ([Global]
  Name=KDOS, IconName=start-here-kdos, one Event); send `x-kde-appname=kdos` from kb_notify,
  notify_send, kdos-notice post() and run.c. Docs D2 (desktop.md:356-358), D5 (kdos-command.md:561-566,
  kdos remind), D7 (plasma-shell.md:258).

### F21 · Six of the seven run-or-raise keys launch programs that are not on the image
ids: desktop-04, kdosdesk-07, docs-02
- **Sees:** Meta+Shift+M/E/U/C/G/W open a Konsole titled after the program holding a bash prompt;
  Konsole's not-found line has scrolled above the greeting. Only lynx is present.
- **Evidence:** `shots/desktop/109-mc.half.png`, `109-mc.crop-448-195-1030-120-x2.png`,
  `logs/desktop/run9.log`, `logs/kdosdesk/run1.log`, `shots/kdosdesk/14-runraise-mc.view.png`,
  `r6-12-mc.png`, `w2-kc1.view.png`, `shots/docs/image-packages.txt`.
- **Cause:** kwin-script-kdos-runraise ships with the base; mc, aerc, rmpc, khal, iamb and micro
  are in no ports/base line and no group.
- **Fix:** docs (adding them to the base pulls in mpd, notmuch, w3m, mupdf and makes mpd a running
  user service; leave that to a product decision). D2: desktop.md:252-267 says lynx is on every
  machine, the other six install with `sudo kdos update install mc aerc rmpc khal iamb micro`, and
  until then the key opens Konsole with a shell and Konsole's "could not find" line above the
  greeting. D7: kwin.md:262-264. U24: kwin-script-kdos-runraise kfs.md Notes.

### F22 · Two slots need about 24.5 GiB, not "about 21 GiB"; the 40 GB example does not fit
ids: variants-06, release-04, docs-11
- **Sees:** a 20–24.5 GiB disk silently gets one slot and no rollback; probe says "25121 MB for two
  slots"; the GUI says "Two slots need 24.5 GiB". The release notes, installation.md:737,
  known-gaps.md:867 and developing.md:191 say 21 GiB. installation.md:315-316 gives 12/12/15 GiB
  for a 40 GB disk (the formula gives 10/10/12.7).
- **Evidence:** `logs/variants/run1.log`, `run10.log`, `shots/variants/r10-01-disk-20g.png`,
  `shots/variants/notes.md:16`, `shots/release/data/release-notes-body.md`,
  `src/system/kinstall-core/conf.c:165-205`, `kinstall-core.h:495-500`.
- **Cause:** the figure leaves out the default 4096 MiB swap that `ki_ab_need_mb()` counts.
- **Fix:** docs D1 (installation.md:42-44 "each at least 8 GiB, half again the install, or 30% of
  the disk past the swap"; :315-316 "40 GiB: two 11 GiB slots and a 13.5 GiB home; 256 GiB: two
  76 GiB slots and a 99 GiB home"; :735-737), D8 (known-gaps.md:866-867, developing.md:191): "about
  24.5 GiB with the default 4 GiB swap, 20.5 GiB with none, for the 4.7 GiB base". U03: the notes
  text at ports/publish:1990; patch the public v0.2 body by hand.

### F23 · A slot that was not deployed has no version in `kdos-bootctl status` and System Versions
ids: installed-05, updates-07
- **Sees:** fresh install: "slot b  KDOS (slot B), inactive"; after updating to B, slot A reads
  Version/Built/Tree "Unknown" and the rollback dialog says only "Start KDOS (slot A) Next Time?".
- **Evidence:** `shots/installed/42-system-versions.png`, `logs/installed/run2.log` (82),
  `run5.log` (ESP slot dirs), `shots/updates/62-slotb-confirmed-sysversions.png`,
  `63-rollback-confirm-dialog.png`, `66-rollback-logout.png`.
- **Cause:** kinstall copies the kernel pair into `EFI/kdos/a` and `b` (`install.c:2612-2637`)
  but writes no `release` record; only `kdos-bootctl deploy` (`bootctl.c:1612-1640`) does, and
  `slot_ident()` reads a non-running slot only from it.
- **Fix:** U08: after the kernel copies, write `EFI/kdos/a/release` and (with part_b)
  `EFI/kdos/b/release` from `TARGET/etc/kdos-release` in `ident_write_esp`'s format. U02: rewrite
  `bootctl.c:98-104` (installer writes the first records, deploy every later one). Docs D4
  (boot-and-init.md:849-853, including "EFI/kdos/b once an update has deployed it"), D6
  (kinstall.md:1040-1056 rows).

### F24 · The v0.2 release lacks `kfs.pdf` and `kfs-html.tar.zst`
ids: release-01, docs-03
- **Sees:** 9 assets; README.md:73, :323 and getting-started.md:53-58 list both books; the table
  also leaves out the ports tarball, the update index and sources.sha256.
- **Evidence:** `shots/release/release-page.png`, `shots/release/data/release-v0.2.json`,
  `run-44059474167.json`, `cr-21b67721d537c4291d6156414848372969d0836e.json`.
- **Cause:** origin had a branch `v0.2` beside the tag, so the draft was published with ref
  `untagged-183e…`; `.github/workflows/books.yml:115, :142` gate on `refs/tags/v`, so both the
  upload and the attach were skipped; the earlier tag-push run found no release yet.
- **Fix:** U33: gate on `startsWith(github.ref,'refs/tags/v') ||
  startsWith(github.event.release.tag_name,'v')`, TAG from `github.event.release.tag_name ||
  github.ref_name`, checkout that ref; rewrite the header :23-27. Operational, after U34 deletes
  the branch: `gh workflow run books.yml --ref v0.2`. Docs D1: add the missing assets to the table.

### F25 · README and `kdos help` describe a compiler toolchain and kpkg-from-source the image lacks
ids: release-03, docs-15
- **Sees:** README.md:47 "a compiler toolchain"; :234-236 "The installed system carries GCC,
  Clang, binutils, CMake…"; `kdos help`: "the host  kpkg — compiled here from source, against
  musl". The ISO has no gcc, cc, ld, make, cmake, meson or ninja.
- **Evidence:** `shots/release/iso-contents.log`, `shots/release/kdos-dev.png`,
  `src/system/kdos-tools/kdos.c:126-131`, `docs/kdos/04-programs/kdos-command.md:136-139`.
- **Fix:** U15: `kdos.c:126-127` → `"the host", "kdos update install — prebuilt, or built here"`.
  D1: README.md:47 "the box stack, Python and Perl, and the KDOS tools"; :234-236 a build first
  installs the build-tools group it lacks (as administration.md:1289-1293). D5:
  kdos-command.md:136-139.

### F26 · `wl-copy` and `wl-paste` are missing; `kdos shot` and `kdos clip` cannot reach the clipboard
ids: desktop-03, docs-10
- **Sees:** `wl-copy: command not found`; `kdos shot` only saves; `kdos shot qr` says nothing here
  can reach the clipboard; `kdos clip` put/take do nothing. desktop.md:432-433 and
  kdos-command.md:2251 promise wl-copy.
- **Evidence:** `logs/desktop/run3.log` (25), `run4.log`, `run9.log`, `shots/docs/image-packages.txt`.
- **Cause:** `src/system/kdos-tools/kpkgbuild:17` does not depend on wl-clipboard, which kdos-tools
  runs (`clip.c:32-60`, `shot.c:46-60, 278`).
- **Fix:** U20: add `wl-clipboard` to kdos-tools' depends (both in 42_graphics). Docs already
  describe it.

### F27 · The Help button on the KDOS settings pages does nothing
ids: settings-01
- **Sees:** Help on KDOS Look (and the other four KDOS pages) opens nothing; stderr "Could not find
  a suitable handler for help:/kdos/…". KDE's own pages fall back to docs.kde.org.
- **Evidence:** `shots/settings/80-look-help.png`, `110-kdos-doc.png`, `logs/settings/run5.log`
  (66), `run6.log`.
- **Cause:** kguiaddons 6.31 `kurlhandler_p.cpp:50-60` opens khelpcenter when present, else a
  docs.kde.org URL only for `/kcontrol` or app paths; `/kdos/…` is dropped. The book is at
  `/usr/share/doc/HTML/en/kdos`.
- **Fix:** U26: `ports/core/kf6/kguiaddons/help-local-page.patch` (Subject: "Open a help: page
  installed as HTML in the browser when KHelpCenter is absent"), applied in build.sh, Patches entry
  and upstream MR in kfs.md; `src/system/kdos-doc/kfs.md` sentence on the Help button. Docs D6
  (kdos-settings.md:11, :314-316).

### F28 · The release notes leave out Secure Boot off, 4 GB memory and the live login
ids: release-11
- **Sees:** the release page never says the stick will not boot with Secure Boot on (BOOTX64.EFI is
  unsigned), nor the memory floor or the `kdos`/`kdos` account; it calls the live ISO "the
  installer image".
- **Evidence:** `shots/release/release-page.png`, `shots/release/data/release-notes-body.md`.
- **Fix:** U03: notes text at ports/publish:1983-1990 ("The live ISO is kdos-<tag>.iso", one
  sentence of requirements and the live account); patch the v0.2 body by hand. D8: developing.md
  step 8 (~967).

### F29 · No documented way to check `SHA256SUMS.sig` before running KDOS
ids: release-05
- **Sees:** the only signature checks offered (`kdos verify`, `kpkg verify-pkg`) need a running
  KDOS; the `kdos-sig-1` / `kdos-pubkey-1` formats are not read by common tools.
- **Evidence:** `shots/release/verify.log`, `shots/release/data/SHA256SUMS.sig`,
  `kdos-release.pub`, `release-notes-body.md`.
- **Fix:** docs D1 (README after :88, getting-started after :74), D8 (developing.md:986-995) and
  U03 (notes :2001-2008): the OpenSSL 3 recipe the verifier ran — `{ printf
  302a300506032b6570032100; cut -d' ' -f2 kdos-release.pub; } | xxd -r -p > kdos-release.der;
  awk '$2=="ccb53bac7fdba3cd"{print $3}' SHA256SUMS.sig | xxd -r -p > SHA256SUMS.sig.bin; openssl
  pkeyutl -verify -pubin -keyform DER -inkey kdos-release.der -rawin -in SHA256SUMS -sigfile
  SHA256SUMS.sig.bin` — plus the key-id line `cut -d' ' -f2 kdos-release.pub | xxd -r -p |
  sha256sum | cut -c1-16`.

### F30 · The boot-splash and console-banner screenshots show the retired system
ids: release-07, docs-14
- **Sees:** `boot-splash.png` is green Phosphor (not Bone) with "musl . toybox . wlroots . no
  systemd" and POWERD/TLP; `tty-banner.png` shows Linux 7.0.10, 758 packages, the old tagline and a
  cut-off logo. Shipped: "musl · GNU · KDE Plasma · no systemd", 7.2.11.
- **Evidence:** `docs/screenshots/boot-splash.png`, `docs/screenshots/tty-banner.png`,
  `shots/release/crop-readme-bootsplash-tagline.png`, `shots/docs/doc-boot-splash-tagline.png`,
  `doc-tty-banner-tagline.png`, `doc-tty-banner-kernel.png`, `src/art/kdos-splash/genbanner.py:44`.
- **Fix:** U35: recapture both from the v0.2 ISO in Bone (splash during the service scripts; tty2
  login banner), after U11 lands; keep the file names.

### F31 · `sudo kdos update apply`, as documented, refuses on every fresh A/B install
ids: installed-03
- **Sees:** "slot b … is not mounted, so there is nowhere to install the candidate. Mount it, pass
  --mount, or pass --in-place". administration.md:1176 gives the bare command.
- **Evidence:** `logs/installed/run4.log` (73), administration.md "From a prompt",
  kdos-command.md:1331, :1377.
- **Cause:** `update.c:2891-2905` mounts the inactive slot only with `--mount`, and never under
  `--dry-run` (`ap->mount && !ap->dry`).
- **Fix:** D3: administration.md:1176 `sudo kdos update apply --mount`, and the :1185 table row
  explains it. U02 (optional): a dry run with `--mount` says it would mount slot `<x>` and stops;
  D5 kdos-command.md:1331.

### F32 · The Health page shows the normal live session as a red failure
ids: settings-07
- **Sees:** "1 check needs attention" and a red ✗ for "$HOME is on overlayfs (a live session), so a
  persistent box cannot exist…".
- **Evidence:** `shots/settings/05-health.png`.
- **Cause:** `kdos.c:1103-1107` emits `warn_()` for the medium's expected state; kdos-command.md:276-278
  names `kdos persist` as the remedy, which does not help (`kdos-packd/mount.c:503`), and
  `kdos-appbox/box.c:804-807` prints the same false remedy.
- **Fix:** U15: make it `skip_` with "a box's writes end with the session, and an installed system
  keeps them". U17: correct `box.c:804-807` to box_cmd.c:384-388's wording. D5:
  kdos-command.md:276-278.

### F33 · The base ships service scripts for programs it does not carry; the docs list them as shipped
ids: installed-11 (boot-05's verifier note; boot-11 and docs-18 user-service list)
- **Sees:** hostapd, thermald, kiwix-serve, kolibri, llama-server and mpd show `[ENABLED] [DOWN]`;
  the splash says "HOSTAPD … OK"; administration.md lists them with no install note, the thermald
  skip row says "not Intel", and the user-services list leaves out ssh-agent, which runs.
- **Evidence:** `logs/installed/run2.log` (services, init-logs-fail), `run6.log`,
  `shots/installed/09-boot-13.png`, `logs/boot/diagx/diag/user-services.txt`.
- **Fix:** docs. D3: administration.md:82, :128, :137, :172-174, :201 (rows say the program comes
  with `sudo kdos update install <port>` and skips until then; thermald row "not installed, or not
  Intel"; user services list `ssh-agent`, and mpd only once installed and configured). D4:
  boot-and-init.md:1387-1396 ("the local servers" skip because the program is absent).

### F34 · The root is mounted `relatime` although fstab says `defaults,noatime`
ids: installed-12
- **Evidence:** `logs/installed/run2.log` (mounts), `run1-install.log` (fstab).
- **Cause:** `090_initramfs.sh:954` mounts the root bare and rcS `mount -a` (`rcS:79`) never
  remounts a mounted filesystem; on btrfs `compress=zstd:3` would be lost too (inferred).
- **Fix:** U11: after `mount -a`, `findmnt -s -n / >/dev/null 2>&1 && mount -o remount /` with the
  rule in a comment. D4: boot-and-init.md:1276 (rcS step 4).

### F35 · The verbose entry runs the kernel console at 9600 baud
ids: boot-03
- **Sees:** black screen from 14 s to 45.6 s and a boot ~10 s slower; with `,115200` the serial gap
  is 4.7 s, not 13.3 s.
- **Evidence:** `shots/boot/ev-13-verbose-black-t27.6.png`, `ev-14-verbose-first-text-t45.6.png`,
  `logs/boot/caps/verbose/serial.txt`, `logs/boot/run12.log`, `run13.log`,
  `logs/boot/caps/verbose-115200/`.
- **Cause:** `110_iso.sh:349, :356, :363` pass `console=ttyS0` with no speed (9600n8).
- **Fix:** U11: `console=ttyS0,115200n8` on all three, with one sentence in the comment above;
  `kdos-splash.c:17-18` header comment. Docs D1 (getting-started.md:167-168), D4
  (boot-and-init.md:241-245), D9 (boot-menu-and-kernel-parameters.md:125, 110-iso.md:263-265,
  repin).

### F36 · Three wrong passphrases leave a bare root shell with no restart hint
ids: variants-07
- **Evidence:** `shots/variants/r6-04-bad3-3s.png`, `r6-06-shell.png`.
- **Cause:** `090_initramfs.sh:906-907` calls the generic `rescue()` (`:569-590`), which prints
  only the reason.
- **Fix:** U11: rescue "Wrong passphrase three times. Press Ctrl+Alt+Del to restart and try
  again."; the VT branch prints "Ctrl+Alt+Del restarts the computer." for every failure. Docs D1
  (installation.md:686-688), D4 (boot-and-init.md:541-543).

### F37 · System Versions on a one-slot install shows an empty slot B form
ids: variants-08
- **Evidence:** `shots/variants/r2-02-sysversions.png`, `r12-01-sysversions-scrolled.png`.
- **Cause:** `kcm-kdos-system/ui/main.qml:131-139` shows the one-version placeholder only when not
  configured; the Repeater (`:153-167`) draws a slot with `present=false`.
- **Fix:** U05: show the placeholder also when configured, not live and a slot is absent; give
  SlotForm `visible: modelData.present`; one-slot fixture and kcmuitest case. D6:
  kdos-settings.md:205-206 (the running slot's form stays beside the message).

### F38 · After a cancelled update, slot B is still offered as 0.2.1 and boots a half-written root
ids: updates-03
- **Sees:** System Versions lists slot B as "KDOS 0.2.1, Installed, not in use"; Start This Version
  Next Time boots a root that says 0.2+21b67721d into the plasma-setup first-run wizard.
- **Evidence:** `shots/updates/91-cancel-6s.png`, `92-sysversions-after-cancel.png`,
  `101-halfslot-logout.png`, `103-halfslot-boot-140s.png`, `logs/updates/run2h.log`.
- **Cause:** the acquire rewrites the slot in place (`update.c:3482-3494`) while
  `EFI/kdos/b/release` keeps the old label; `bootctl.c:3211-3245` `try` checks only a root and a
  kernel; `SlotForm.qml:96` offers the button for any inactive slot.
- **Fix:** U02: boot-state key `unfilled = <slot>` and verb `kdos-bootctl unfill`, set before the
  first write into the slot (every lane), cleared by deploy; `try` refuses; `status --json` reports
  `incomplete`; correct the `image.c:2083-2087` comment. U05: SlotForm/slotinfo show "Incomplete: an
  update stopped while writing it" and hide the button. Docs D4 (boot-and-init state keys, verb
  table; packaging acquire), D5 (kdos-command.md bootctl table ~2185-2200), D6
  (kdos-settings.md ~195-205, discover.md:287).

### F39 · The first Discover session that finds an update shows its size as "Unknown"
ids: updates-05
- **Evidence:** `shots/updates/12-notifier-click.png`, `13-update-details.png`,
  `14-update-moreinfo.png`, `15-discover-reopen-updates.png`, `16-moreinfo-reopen.png`.
- **Cause:** Discover 6.8.2 `SystemdSysupdateBackend.cpp:147-199` parses the AppStream before
  CheckNew rewrites it; `sysupdate-size.patch` then finds no matching release.
- **Fix:** U06: `ports/core/kde/discover/sysupdate-appstream-after-check.patch` (Subject:
  "SystemdSysupdateBackend: read a target's AppStream after CheckNew"), after
  sysupdate-errors.patch; kfs.md Patches with upstream MR. U04: `appstream.c:10-12` header. D6:
  discover.md "What the update page says".

### F40 · After "Keep KDOS 0.2 (slot A)", Discover and the tray still ask for a restart
ids: updates-06
- **Evidence:** `shots/updates/82-discover-after-cancel.png`,
  `84-tray-after-cancel-50s.png`.
- **Cause:** `KdosNotifier.cpp:249-257` emits needsRebootChanged only on false→true; upstream
  `DiscoverNotifier.cpp:77-85` latches it.
- **Fix:** U06: emit on both transitions (replace the :249-252 comment); `notifier-reboot-clears.patch`
  recomputing m_needsReboot; notifiertest case; kfs.md says an open Discover window keeps its page
  until restarted. D6: discover.md notifier row.

### F41 · "Update in Discover…" only flashes Discover's taskbar entry
ids: updates-08
- **Evidence:** `shots/updates/72-slota-discover-reoffer.png`, `73-slota-sysversions-after-manual-rollback.png`,
  `74-update-in-discover-click.png`, `76-discover-refresh.png`.
- **Cause:** `systemversions.cpp:544-549` uses `QProcess::startDetached` with no activation token.
- **Fix:** U05: `KIO::CommandLauncherJob` with desktop name `org.kde.discover` (also
  openInstaller :551-556); link KF6::KIOGui.

### F42 · Downloaded packages stay in the live session's RAM overlay
ids: packages-06
- **Sees:** `/var/lib/kdos/binhost` grows 5 → 479 MB over four installs, all in the 1.9 GB tmpfs,
  feeding F13.
- **Evidence:** `logs/packages/run2.log` (MEM blocks 169, 9465, 19211, 29014, 30352), `run8.log`.
- **Fix:** U02: on the live medium (bootctl_layout's `live`) unlink each package file and .sig once
  installed. Docs D4 (packaging.md:1632), D1 (getting-started.md:283-285).

### F43 · Discover offers a Debian box twin of a KDE app the binhost already has
ids: packages-08
- **Sees:** two identical Filelight rows, one "From KDOS system", one "Install from Debian, built
  in a box".
- **Evidence:** `shots/packages/33-search-filelight.crop-480-140-960-760.png`,
  `34-filelight-row1…`, `35-filelight-row2…`, `23-discover-filelight-page.view.png`.
- **Cause:** `KdosBackend.cpp:421-431` puts only installed ports' ids in m_hostIds;
  `readCatalogueFile` (`:413-417`) returns only the first component per package. (The appstream://
  link "No entry" was a wrong id, not a defect.)
- **Fix:** U07: readCatalogueFile returns every id; add the binhost catalogue's ids to m_hostIds.
  D6: discover.md:150.

### F44 · `kdos update install` prints a line for every file
ids: packages-10
- **Sees:** 29,119 `==> Installing usr/… -> //usr/…` lines for four installs.
- **Evidence:** `logs/packages/run2.log`.
- **Fix:** U19: drop the per-file `kp_msg` at `add.c:421` (or print it only under KPKG_VERBOSE).

### F45 · `kdos app info` gives a box app's size without its base and runtime
ids: packages-11
- **Sees:** "size 8.6M (an estimate)" while the first install grows the overlay by 670 MB and
  Discover says ~626 MiB.
- **Evidence:** `logs/packages/run6.log`, `run9.log` (26),
  `shots/packages/71-discover-zathura-page.crop-480-140-960-760.png`, `80-…`, `101-…`.
- **Fix:** U18: `app.c` cmd_info adds the `absent` rows of `kdos-appbox catalogue --chain` ("8.6M,
  626M with the base and runtime this computer has not built"). D2: applications.md:764, :778.

### F46 · Typing "Install KDOS" in Kickoff and pressing Enter opens Discover
ids: apps-03
- **Evidence:** `shots/apps/95-kickoff-install-results.png`, `94-install-kdos.png`.
- **Cause:** the palette's `install <rest>` verb (`match.c:125-126`, `:273-281`) ranks above
  Applications (krunnerrc favourites), so the installer's own name becomes a Discover search.
- **Fix:** U16: the verb does not answer when the rest is `kdos`; matchtest cases; header comment
  :21-22; runner kfs.md:13-17. D7: plasma-shell.md:181/184.

### F47 · nmtui is listed as "Network / Network Settings" beside the real network settings
ids: apps-05
- **Evidence:** `shots/apps/48-kickoff-network.png`, `kpair-2.png`, `kpair-4.png`,
  `logs/apps/run1.log` (15).
- **Fix:** U29: `ports/core/network/networkmanager/build.sh:71-83` Name=nmtui, GenericName=Network
  connections in a terminal, Categories=Network;ConsoleOnly;; kfs.md:20. D9:
  networkmanager.md:151-152 and the include pin.

### F48 · Partition Manager's New Partition dialog caps an 8 GiB disk at 7.00 GiB
ids: apps-17
- **Evidence:** `shots/apps/160-pm-new-dialog.png`, `161-pm-ext4-applied.png`.
- **Cause:** partitionmanager 26.08.2 `sizedialogbase.cpp:142-148` truncates double GiB capacities
  to qint64 before `setRange`.
- **Fix:** U28: `ports/core/disk/partitionmanager/size-dialog-ranges.patch` (Subject: "Keep
  fractional units in the size dialog's ranges"), applied in build.sh, upstream MR in kfs.md.

### F49 · At 200% scale the panel cuts off the clock; the guide says it keeps everything
ids: settings-02
- **Evidence:** `shots/settings/76-scale200-look.png`, `76-scale200-look.crop-700-990-1220-90-x2.png`,
  `75-scale150-look.crop-0-1005-960-75-x2.png`.
- **Fix:** D2: desktop.md:79-82 says the panel shrinks pins and charts first, then cuts what still
  does not fit at the right edge, the clock and Show Desktop first (1920 at 200% loses both), and
  `kdos desk kdos` lays it out again.

### F50 · The firewall authorisation is described wrongly
ids: settings-08
- **Sees:** administration.md says a wheel member "is let through" the firewall change; the
  Firewall page asks for a password at every Apply and again after reopening.
- **Evidence:** `shots/settings/13-fw-apply-polkit.png`, `m-fw-15-19.png`,
  `src/daemons/kdos-powerd/io.kdos.power1.policy:30-39`.
- **Fix:** D3: administration.md:742-743 (socket lets wheel through and refuses others; the page
  asks for a password at Apply, kept a few minutes for that window). U30: policy comment :9 and
  `kdos-powerd/kfs.md:33` ("kept for a few minutes for the process that gave it").

### F51 · The Offline Library page says "?" searches the library; the prefix is `lib`
ids: kdosdesk-04
- **Evidence:** `shots/kdosdesk/sheet-w2-2.png`, `r4-04-kr-help.view.png`.
- **Fix:** U22: `50-Library.qml:43` "Start with <b>lib</b> and your words…".

### F52 · `kpkg remove` leaves the port in the world set, so the next update brings it back
ids: cli-03
- **Evidence:** `logs/cli/run3.clean.log` (228-236).
- **Fix:** U19: after a successful `remove_one()` (`del.c:129`) drop the line from
  `<root>/var/lib/kdos/world` and `origin` (temp file + rename). Docs D2 (applications.md: `sudo
  kpkg remove <port>` uninstalls one and takes it out of the world set; replaces :652), D4
  (packaging.md kpkg remove row), D5 (kdos-command.md:1401-1406).

### F53 · Removing a box leaves a dead launcher in the menu
ids: cli-05
- **Sees:** "T3 — Terminal entering T3" stays in Kickoff after `kdos-box remove t3 --force`;
  kdos-fetch-app leaves `kdos-debian.desktop`.
- **Evidence:** `shots/cli/22-menu-search-t3.png`, `22-menu-search-t3.crop-0-520-646-130-x2.png`,
  `logs/cli/run3.clean.log` (47-49), `run2.clean.log` (392-452).
- **Cause:** `box.c:1121-1127` and `fetch.c:163-170` run `distrobox create` without `--no-entry`;
  removal uses `podman rm`.
- **Fix:** U17 (box.c) and U18 (fetch.c): add `--no-entry`. D5: kdos-appbox.md:612-615.

### F54 · `--help` is handled differently by almost every subcommand
ids: cli-09 (cli-08's `kdos settings --help` case)
- **Sees:** some print usage with 0, most with 2; `kdos why --help` looks up a port named --help,
  `explain --help` "nothing recorded", `oracle --help` prints the day's line, `toggle --help` "no
  such toggle", `settings --help` opens System Settings.
- **Evidence:** `logs/cli/run1.clean.log` (295-553, 499-509).
- **Fix:** U15: `kdos_main` answers `-h`/`--help` as a subcommand's first argument with that
  subcommand's synopsis and 0. D5: kdos-command.md:26 and the exit tables.

### F55 · kdos-fetch-app's own usage example fails after two minutes
ids: cli-11
- **Sees:** `kdos-fetch-app firefox` → "Package 'firefox' has no installation candidate" after 125 s;
  `--remove` prints distrobox errors and exits 0; `yay` is an AUR helper; '==> Installing' prints
  after the error.
- **Evidence:** `logs/cli/run7.clean.log`, `run2.clean.log` (454-485), `run1.clean.log` (1139-1151).
- **Fix:** U18: examples `firefox-esr`, `--box arch --image archlinux:latest neovim`,
  `--remove firefox-esr`; `setvbuf(stdout, NULL, _IOLBF, 0)`; `--no-entry`; -h/--help returns 0.

### F56 · `kdos update apply` exits 3 and 4, which the exit table does not list
ids: cli-12
- **Evidence:** `logs/cli/run2.clean.log` (273-278, 198-200), `run1.clean.log` (971-973).
- **Fix:** D5: kdos-command.md:1373-1377 adds 3 (refused before anything was written: live medium
  without --in-place, needs-source, unverified index or image, slot cannot take it) and 4 (stopped
  by SIGTERM, slot not booted).

### F57 · known-gaps says the update path has never run against a published release
ids: cli-13
- **Evidence:** known-gaps.md:890-905, `logs/cli/run2.clean.log` (250-259), `run3.clean.log`.
- **Fix:** D8: retitle "No update has been applied on an installed machine"; say what ran against
  v0.2 (tree fetch and signature, image manifest, binhost index with a named binhost, prebuilt
  install) and what did not (`apply` on an installed A/B machine).

### F58 · Piper, whisper.cpp, Dasher, KMouth and Kontrast are said to be on the image
ids: media-07, docs-07
- **Sees:** none is installed; `kdos speech` offers 63 MB Piper voices with no piper to use them.
- **Evidence:** `logs/media/run1.log` (bins), `shots/media/notes.md` (run7), kdos-command.md:1990,
  :2000, accessibility.md:27-28, :155, :317, applications.md:476.
- **Fix:** U18: `speech.c` `voice get` and `say` say piper is not installed and name the install
  command; header :8-29. D2 (accessibility.md rows, :155, :317; applications.md:476 base.en model is
  its own port), D5 (kdos-command.md:1990, :2000).

### F59 · The panel clock does not grow with the display scale or larger fonts
ids: media-08
- **Evidence:** `shots/media/71-2560-scale2.crop-2300-1350-260-90-x3.png`,
  `20-scale200-desktop.crop-1900-1345-660-95-x3.png`, `61-kickoff-16pt.crop-1540-1035-380-45-x4.png`.
- **Cause:** the desks set `autoFontAndSize=false`, `fontSize=10` (`layout.js:154-160`); upstream
  sizes a manual font by `Screen.pixelDensity`, which cancels the scale.
- **Fix:** U10: drop the clock's font keys and `fixedFamily()` in all three desks. D2
  (desktop.md:35), D7 (plasma-shell.md:37).

### F60 · In the dark accents, slider fills, the OSD bar and unselected radios are nearly invisible
ids: media-09
- **Sees:** fill vs groove 1.41:1, groove vs ground 1.29:1, unselected radio ~1.1:1 (Bone).
- **Evidence:** `shots/media/07-applet-capture.crop-1460-700-440-220-x3.png`,
  `10-osd-loop.crop-810-660-300-60-x4.png`, `45-vk-typed.crop-290-955-400-75-x3.png`.
- **Cause:** the KDOS Plasma style falls back to `default` for slider, bar meter and radio, which
  fill with ColorScheme-Highlight, the darkened selection.
- **Fix:** U10: `genlook.py` generates `widgets/slider.svgz`, `bar_meter_horizontal.svgz` and the
  radio/checkbox marks with ColorScheme-ButtonFocus fills; kdos-plasma-look kfs.md. D4:
  design-language.md:121-131.

### F61 · Keyboard focus on push buttons barely changes them
ids: media-10
- **Sees:** focused border (96,95,89) vs unfocused (68,66,61), 1.57:1, on every Breeze button.
- **Evidence:** `shots/media/28-29-quit-compare.png`, `95-tabs-grid.png`.
- **Cause:** breeze 6.8.2 `renderButtonFrame` draws focus in `highlightColor`, the darkened
  selection.
- **Fix:** U27: `ports/core/themes/breeze/button-focus-outline.patch` drawing the visualFocus pen
  in `focusColor(palette)`; kfs.md MR. D4: design-language.md (button focus reads the focus colour).

### F62 · At 16 pt fonts text clips and several surfaces do not update
ids: media-12
- **Evidence:** `shots/media/60-fonts16.png`, `61-kickoff-16pt.png`, `63-kcm-access.png`,
  `65-elisa-wav.png`.
- **Fix:** D2: accessibility.md "Larger text" says open windows keep their size and the panel,
  Kickoff and Welcome take the new size once reopened or at the next login. Upstream reports.

### F63 · Closing Partition Manager without changes leaves a disabled "Use partitions that already exist" checked
ids: media-13, installer-09
- **Evidence:** `shots/media/100-bc-compare.png`, `99b-disk-space.png`,
  `shots/installer/26-disk-after-partman.png`, `27-disk-wipe-again.png`.
- **Cause:** `DiskPage.qml:246-249` calls `page.useReuse()` before opening Partition Manager.
- **Fix:** U09: call `useReuse()` only when the rescan finds partitions, else keep `wipe`; header
  comment :12-14. D1 (installation.md:83), D6 (kinstall.md:1251).

### F64 · The input-method docs read as if Chinese, Japanese and Korean engines are installed
ids: media-14, docs-19 (e)
- **Evidence:** `shots/media/49-kcm-fcitx5.png`, `51-search-pinyin.crop-950-360-660-560.png`,
  `logs/media/run1.log` (fcitx5 profile).
- **Fix:** D3: administration.md ~1712-1726 says the engines and CJK fonts are the lang-zh/ja/ko
  groups, the shipped profile already names them, and replaces "The fonts cover every script". D2:
  accessibility.md:285.

### F65 · The docs say the live account has no password; root and kdos have the password `kdos`
ids: security-02
- **Sees:** security-model.md says an empty field unlocks the live lock screen; it fails ("Unlocking
  failed"); `kdos` unlocks.
- **Evidence:** `shots/security/43-lock-empty-enter.crop-660-380-600-360-x2.png`,
  `17-lock-empty.png`, `18-unlocked.png`, `logs/security/run2.log`, `run10.log`.
- **Fix:** D4 (security-model.md:190-191, :210-211; session.md:157), D8 (configuration.md:418), D9
  (screen-lock.md:40), U30 (xdg-live/kscreenlockerrc:10-12 and kwalletrc:10 comments).

### F66 · The signing table says `/etc/kdos/keys` ships with "No key"
ids: security-03
- **Evidence:** `logs/security/run1.log` (KEYRINGS), `run5.log`.
- **Fix:** D4: security-model.md:849 "`kdos-binhost.pub` and `kdos-release.pub`, the image's own
  build's, when the build found them; otherwise no key".

### F67 · `kdos-appbox status` tells a user outside wheel that packd is "not answering"
ids: security-04
- **Evidence:** `logs/security/run3.log`, `run4.log` (2727), `logs/security/verify/sockexp.py`.
- **Cause:** `kdos-appbox/main.c:1082-1085` prints "not answering" for any non-zero `packd_ask()`;
  the daemon's "err not permitted" arrives (the reset was the tester's client).
- **Fix:** U17: r==1 prints `packd : <its refusal>`. D5: kdos-appbox.md:457.

### F68 · An `http://` update channel is accepted and saved, then every fetch fails
ids: security-05
- **Evidence:** `logs/security/run4.log` (2704-2719).
- **Cause:** `kdos-sysupdated/state.c:488` accepts `http://`; `systemversions.cpp:299-300` offers
  it; curl runs `--proto =https,file`.
- **Fix:** U04: drop `http://` from channel_ok (comment :470); drive.sh case expecting "not an update
  channel". U05: page accepts only https and file, test rows (`systemversionstest.cpp:354`,
  `fakeslots.cpp:440`). D6: kdos-settings.md:236-238.

### F69 · securityfs is not mounted; the LSM description does not match the kernel
ids: security-06
- **Sees:** `/sys/kernel/security/lockdown` and `lsm` are absent; once mounted, `lsm` reads
  `capability,landlock,lockdown,yama` (no integrity).
- **Evidence:** `logs/security/run1.log` (LSM), `run2.log` (SECURITYFS).
- **Fix:** U11: `securityfs /sys/kernel/security securityfs nosuid,nodev,noexec 0 0` in
  `fs/etc/fstab` with its comment. D4 (boot-and-init.md:1599-1615, security-model.md:779-787 and
  the Integrity row), D8 (configuration.md:738), D9 (filesystems-and-fstab.md prose).

### F70 · A too-large install downloads until the disk is full before refusing
ids: security-08
- **Sees:** `kdos update install --prebuilt kde-all` on the live medium downloads 1.9 GB for 14 min,
  then "no room for binhost/konqueror…"; the cache is left filling the overlay.
- **Evidence:** `logs/security/run5.log` (62-64).
- **Fix:** U02: before the first download compare `bytes + dp.bytes` with `statvfs(cache_dir())`
  and refuse naming both; the dry run says the same and exits 1; comment :5241-5282; selftest with a
  small tmpfs cache. D5: kdos-command.md:1257.

### F71 · `kdos-box profile` prints distrobox flags for a pack box and shows a new `home=private` as in force
ids: security-10
- **Sees:** a pack box prints `--unshare-process`, `--home`…; after `profile home=private` and a
  restart the box still reads `~/.ssh` while the profile reads "home = private" with no note.
- **Evidence:** `logs/security/run6.log` (173-213), `run7.log`.
- **Fix:** U17: `box.c:811-822` prints the pack/store lane's mechanism; a `!` line when the
  profile is newer than the container's creation. D5: kdos-appbox.md "How the profile reports
  itself" and the network/ipc/processes rows (:1101).

### F72 · ISO Image Writer is described as present; the KRunner burn route starts nothing
ids: release-14, docs-09
- **Evidence:** `shots/release/iso-contents.log`, `/etc/kdos/menu.conf` (`system.burn =
  isoimagewriter`), getting-started.md:131-132, desktop.md:491.
- **Fix:** U20: add `isoimagewriter` to ports/base (148 KB + qgpgme 453 KB). D4: packaging.md:368
  summary. getting-started.md:131-132, desktop.md:491, the menu route and the kde-mimeapps `.iso`
  rows then hold; check in the rig that `.iso` opens it rather than Ark.

### F73 · plocate is not on the image while its index job runs at every login
ids: docs-06
- **Sees:** `locate` is absent; `kdos-updatedb --if-missing` execs a missing `/usr/sbin/updatedb`
  at every login and nightly, silently.
- **Evidence:** `fs/etc/kdos/user-services/30_timers.sh:36-38`,
  `fs/etc/skel/.config/kdos/timers.d/20-updatedb.timer`, `fs/usr/local/bin/kdos-updatedb:36-43`.
- **Fix:** U20: add `plocate` to ports/base (155 KB). D4: packaging.md:368.

### F74 · Dolphin is said to gain five KDOS actions; the image ships two
ids: docs-08
- **Fix:** D2: desktop.md:391-399 "up to five; two are always there"; the throwaway-box row needs
  a boxed application, Git Status Here needs lazygit.

### F75 · applications.md says the installer imports a set from a stick; it never does
ids: docs-13
- **Fix:** D2: applications.md:792-794 and :874-875 (the installer records groups only; on a live
  session a stick's set is offered to Discover; carry the .ktar to the installed machine and run
  `kdos app import`).

### F76 · Summary accepts a two-slot plan that kinstall then refuses; Finish leaves the VG active
ids: installer-03
- **Sees:** with an active `kdos` VG on another disk, Install fails at once with "…rename it with
  vgrename, or leave LVM off"; Back to Summary accepts the same plan.
- **Evidence:** `shots/installer/117-fail-done.png`, `118-back-to-summary.png`, `116-summary-oem.png`.
- **Cause:** `probe.c:551-571` `ki_vg_off_disk()` treats a failed unprivileged `lvm pvs` as no
  clash; `install.c:2916-2926` do_finish only unmounts.
- **Fix:** U08: fall back to `/sys/block/dm-*` (LVM- uuids, `slaves/`) when lvm cannot run; message
  "…or untick Two system slots" (`conf.c:946-948`); `release_disk()` after the final umount. Docs D1
  (installation.md:599-603), D6 (kinstall.md:645-654 and step table).

### F77 · The copy line's "about 0:00:NN left" is the elapsed time
ids: installer-04
- **Evidence:** `shots/installer/69-install-10s.png`, `70-install-copy.crop-880-280-840-250.png`,
  `71-close-during-install.png`, `72-install-later.crop-880-280-840-450.png`.
- **Cause:** `install.c:901-923` prints rsync's progress2 time field, which is elapsed on its
  end-of-file lines.
- **Fix:** U08: kinstall computes remaining = elapsed × (1−f)/f from a CLOCK_MONOTONIC start and
  prints its own rate with kb_human_size. D6: kinstall.md:421-423.

### F78 · The graphical installer never reports Secure Boot
ids: installer-05
- **Evidence:** `shots/installer/13-tour-click-5s.png`, `04-welcome.png`,
  `src/desktop/kdos-installer-gui/qml/WelcomePage.qml:139-143`.
- **Fix:** U09: ", Secure Boot on" on the Firmware row and a Warning InlineMessage when
  `Backend.probe.secure_boot`. Docs D1 (installation.md:82), D6 (kinstall.md:1249).

### F79 · A user name starting with "-" and a host name ending with "-" pass every check
ids: installer-06
- **Evidence:** `shots/installer/37-acct-user-dash.png`, `44-host-trailing-dash.png`,
  `acct-validation-montage.view.png`, `host-validation-montage.view.png`, `54-summary-oddnames.png`.
- **Fix:** U08: `conf.c:619-638` refuse a leading `-` and a trailing `-`; messages in `pages.c:1341,
  :1359`, `conf.c:711`. U09: `answers.cpp:110-121` patterns `^[a-z_][a-z0-9_-]{0,31}$` and
  `^[A-Za-z0-9]([A-Za-z0-9-]{0,61}[A-Za-z0-9])?$`, AccountPage.qml:33/51 messages, installertest
  cases. Docs D1 (installation.md:386, :390-391, ~792), D6 (kinstall.md:277-278).

### F80 · A disk attached after boot shows an empty root list for its LVM volumes
ids: installer-07
- **Evidence:** `shots/installer/96-reuse-root-list.crop-640-180-840-700.png`,
  `98-reuse-root-after-activate.crop-640-180-840-700.png`.
- **Fix:** U09: Look Again runs `pkexec /usr/bin/kinstall --dump probe --json` (activates VGs);
  placeholder for an empty root list. Docs D1 (installation.md Disk row, ~347), D6
  (kinstall.md:1251, ~1260-1263).

### F81 · Two disks of the same size cannot be told apart before erasing one
ids: installer-08
- **Evidence:** `shots/installer/94-kickoff-launch-disk.png`, `111-confirm2.crop-640-400-840-300.png`.
- **Fix:** U09: a second line per disk from `modelData.partitions` ("Holds: …" or "Empty"); the
  erase dialog names model and size (`Main.qml:140-141`). Docs D1 (installation.md:83), D6
  (kinstall.md:1251).

### F82 · A system keyboard layout never reaches an account that already has `kxkbrc`
ids: installed-08
- **Sees:** after `SetX11Keyboard us,de` and a reboot the session keeps `LayoutList=us` and shows
  no indicator; daemons.md:921 and kdos-settings.md:282 say startplasma copies the layout into each
  session.
- **Evidence:** `shots/installed/60-greeter-2layouts.crop-0-1030-700-50-x3.png`,
  `65-switch-user-greeter.png`, `71-greeter-layout-next.crop-0-1030-700-50-x3.png`,
  `73-session-2layouts.crop-1500-1036-420-44-x3.png`, `logs/installed/run8.log`.
- **Fix:** docs D7 (daemons.md:920-921), D6 (kdos-settings.md:282), D8 (configuration.md:814): the
  layout reaches a session only for an account whose kxkbrc names none. The greeter half (a
  running greeter shows no switcher until restarted) is undecided: at the greeter run
  SetX11Keyboard us,de as root, then as sddm `qdbus6 org.kde.keyboard /Layouts
  org.kde.KeyboardLayouts.getLayoutsList`; two entries mean the footer is stale, one means KWin
  `--locale1` did not follow.

---
## Polish

Each: what the user sees, evidence, cause where it is not obvious, fix and unit.

### F83 · `kdos update` hints are cut short or give advice that cannot work
ids: packages-15, cli-14, installed-13 (wording half)
- `groups` ends "…`kdos update fetch --binhost-only` takes this release's"; `check` prints
  "`kdos update apply --source` builds what is behind" with 0 behind and on the live medium, and
  "tree 21b67721d (21b67721d)". Evidence: `logs/packages/run1.log` (36, 60-62),
  `logs/cli/run1.clean.log` (893-910), `logs/installed/run4.log`, `run2.log`.
- Fix U02: finish the sentence at `update.c:5034-5035` and `:3627-3628`; at `:1676-1678` give the
  `--source` clause only when n > 0 and not live, else name `sudo kdos update fetch
  --binhost-only`; print `(<version>)` only when the tree is named.

### F84 · On an installed machine the boot messages are drawn over the splash
ids: installed-09 (variants-03 is the LUKS case, under Uncertain)
- "INIT: version 3.18 booting" and half-overwritten sysctl lines at the splash's top left on every
  installed boot; documented at known-gaps.md:844. Evidence: `shots/installed/09-boot-11.png`,
  `09-boot-13.png`, `09-boot-11.crop-0-150-300-40-x4.png`.
- Cause: `console=tty0` (install.c:2768) lets the initramfs's first echo end fbcon's deferred
  takeover. Fix U11: `kdos-splash.c` `console_claim()` puts the foreground VT in KD_GRAPHICS while
  the splash draws, `quit` returns KD_TEXT, `handoff` keeps KD_GRAPHICS; header :14-21. D4
  (boot-and-init.md:579-600), D8 (remove known-gaps.md:844-853).

### F85 · Splash text in the fill colour: tagline and failure reason at 1.1–1.7:1
ids: boot-06, variants-13
- Tagline (58,56,51) on (10,10,9) = 1.69:1; the WHY detail (15,15,13) on (2,2,2) during the 0.45 s
  fade, and rescue() quits right after `detail`. Evidence: `shots/boot/ev-02-splash-1080-48pct.png`,
  `ev-03-ksplash-1080-83pct.png`, `shots/variants/r6-03-bad3-now.png`, `r6-attempts.png`.
- Fix U11: `kdos-splash.c:785` and `:846` draw in C_PHOSDIM (KT_MID's slot), comment :794-797;
  `090_initramfs.sh` rescue() sleeps 4 s after `detail` before `quit`. D4 (boot-and-init.md:534,
  :638). Proof: `kdos-splash preview 1920x1080 9 out.ppm` ≥4.5:1.

### F86 · The greeter's user name is hard to read over the blurred penguin belly
ids: installed-06
- 2.6–3.4:1 for the name and "Login Failed"/"Unlocking failed". Evidence:
  `shots/installed/01-greeter.crop-800-540-320-110-x3.png`, `72-wrong-password.crop-860-550-200-50-x4.png`,
  `76-lock-wrong.crop-760-540-400-120-x3.png`.
- Fix U10: `kdos-theme greeter-globals --sddm` (`src/art/kdos-theme/kdecolors.c`) writes a darkened
  copy of the dark wallpaper as `background.png`, measured to 4.5:1 over the user block, and the
  lock screen points at it; kdos-theme kfs.md:15, kdos-plasma-look kfs.md:33-35, kdos-sddm-theme
  kfs.md. D4: design-language.md names the greeter floor.

### F87 · The greeter's virtual keyboard is light on the dark greeter
ids: installed-07
- Evidence: `shots/installed/32-greeter-vkbd.png`, `32-greeter-vkbd.crop-290-1000-400-75-x3.png`
  (the disabled globe key is normal with one language).
- Fix U10: after checking `/proc/$(pidof plasma-keyboard)/environ` at the greeter, add
  `QT_QPA_PLATFORMTHEME=kde` to GreeterEnvironment in `fs/etc/sddm.conf.d/00-kdos.conf`. D8
  (configuration.md:652), D4 (boot-and-init.md:1689).

### F88 · Kickoff's header avatar touches the user's name
ids: installed-14
- Evidence: `shots/installed/06-start-menu.crop-0-520-240-40-x4.png`. Cause: `genlook.py:851` dialog
  plate margins equal insets, so Kickoff's spacing is 0. Fix U10: dialog margins uniform(8), insets
  uniform(4); kfs.md note. Blue avatar is upstream.

### F89 · The lock screen and greeter clocks sit at different heights, contrary to the port note
ids: installed-19
- Evidence: `shots/installed/01-greeter.png`, `13-lock-prompt.png`. Fix U10:
  `src/art/kdos-sddm-theme/kfs.md` says they share user box, field and wallpaper; the clock sits
  higher on the lock screen and only the greeter has the credit.

### F90 · `service list` cells for lvm and tlp show a table header and a banner
ids: installed-20
- Evidence: `logs/installed/run2.log` (339, 373). Fix U30: `03_lvm.sh:61-62` and `55_tlp.sh:31-32`
  print a one-line summary first. D3: administration.md:226-229.

### F91 · Size units disagree: "holds 5.5G; needs at least 5.5G", GiB rows, MB/s copy line
ids: variants-11, installer-16
- Evidence: `logs/variants/run10.log`, `shots/variants/r10-01-disk-20g.png`,
  `r1-03-gui-disk.crop-653-265-500-125-x3.png`, `shots/installer/06-disk-4g.crop-650-260-700-160-x3.png`,
  `22-disk-lookagain.png`, `69-install-10s.png`.
- Fix U08: `conf.c:803-806` whole MiB with the unit spelled; `dump.c` probe JSON gains `need_bytes`;
  kinstall's own copy rate (with F77). U09: `DiskPage.qml` why() phrases a too-small disk with
  `Backend.formatSize`. D6: kinstall.md:690-700.

### F92 · The "this disk gets one slot" note reads as secondary text, not a warning
ids: variants-12
- (185,179,161) under (222,217,203) in Bone. Evidence: `shots/variants/r10-01-disk-20g.png`,
  `r10-01-disk-20g.crop-890-495-440-75-x3.png`. Fix U09: `DiskPage.qml:270-279` and
  `SummaryPage.qml:74` use a Warning InlineMessage. D1: installation.md:318 "say so in a warning".

### F93 · The passphrase prompt names the mapper "kdosroot"
ids: variants-14
- Evidence: `shots/variants/r5-01-luks-a.png`, `r13-01-bios-luks26.png`. Fix U11:
  `090_initramfs.sh:741-744` "DISK PASSPHRASE (attempt N of 3)". D4: boot-and-init.md:1076-1077.

### F94 · Kickoff and KRunner software results have no icons and loose matches
ids: variants-15, desktop-08
- "Get Mines…" twice, "Get PHASEX…" for 12*7+3, blank icons. Evidence:
  `shots/variants/r9-00-kickoff-mines.crop-0-580-650-140-x3.png`, `r9-01-kmines.png`,
  `shots/desktop/42-krunner-calc.crop-670-0-580-420.png`, `47-krunner-prevver…`, `41-krunner-palette…`.
- Cause: krunner_appstream reads Debian's DEP-11 pool, whose cached icons are not installed. Fix U29:
  `ports/core/xdg/debian-dep11` adds the snapshot's `icons-64x64.tar.gz` source (sha256 line,
  `ports/publish debian-dep11`, `ports/sources.idx`) and unpacks it; kfs.md About names KRunner as a
  reader. D6: discover.md:159-162 (KRunner offers Debian applications as "Get …"). Loose matching is
  libappstream's.

### F95 · `--dump plan` prints two contradicting swap lines; the log has LVM fd-leak warnings
ids: variants-16
- Evidence: `logs/variants/run14.log`. Fix U08: `dump.c:384-387` skips the generic line under the
  A/B branch; `install.c:211, :513-515` use `pipe2(…, O_CLOEXEC)`.

### F96 · The Keyboard page opens at Albanian, not the chosen layout
ids: variants-17, installer-15
- Evidence: `shots/variants/r1-02-gui-keyboard.png`, `shots/installer/05-keyboard.png`,
  `18-keyboard.png`. Fix U09: `KeyboardPage.qml` sets currentIndex and `positionViewAtIndex(…,
  ListView.Center)` for `Backend.answers.xkbLayout` (mapped through the proxy) on arrival and when
  the search empties.

### F97 · "Open the Log" asks which application to use and opens it behind the installer
ids: installer-10
- Evidence: `shots/installer/74-open-log.png`, `75-log-in-kate.png`. Fix U09: `backend.cpp:281-284`
  uses `KIO::OpenUrlJob` with a JobUiDelegate; KF6::KIOGui in CMakeLists.txt; `kio` in kpkgbuild.
  Docs D1 (installation.md:96-98), D6 (kinstall.md:1258).

### F98 · The password strength hint pushes the Account page down as you type
ids: installer-11
- Evidence: `shots/installer/39-acct-pw-mismatch.png`, `41-acct-layout.crop-880-280-840-700.png`,
  `57-account-final.crop-880-280-840-700.png`. Fix U09: `StrengthLabel.qml` keeps a two-line
  minimum height (header comment), `AccountPage.qml` `again` gets "Password, again".

### F99 · The Encryption switch looks the same on and off
ids: installer-13
- Tracks ~1.5:1. Evidence: `shots/installer/29-encryption.crop-885-312-200-25-x4.png`,
  `30-encryption-on…`. Fix U09: `EncryptionPage.qml:34` QQC2.CheckBox.

### F100 · A time-zone filter with no match hides the chosen zone
ids: installer-14
- Evidence: `shots/installer/16-tz-nomatch.crop-880-280-840-700.png`. Fix U09: `WelcomePage.qml:93-95`
  "%1 (no zone matches the filter)".

### F101 · Summary shows the group id `kde-system` instead of its title
ids: installer-17
- Evidence: `shots/installer/64-summary-final.png`. Fix U09: `SummaryPage.qml:160-161` maps ids to
  `Backend.apps.native` titles.

### F102 · Done-page warnings show raw backticks and split `--pending` across lines
ids: installer-18
- Evidence: `shots/installer/73-done.crop-890-475-820-110-x2.png`. Fix U09: `DonePage.qml:47-51`
  renders `…` spans as `<code>` with non-breaking hyphen and space, joined with `<br>`.

### F103 · The Look page shows desks as stock icons; Tiling's is a dim action glyph
ids: installer-19
- Evidence: `shots/installer/49-look.crop-895-655-580-160-x3.png`. Fix U09: `LookPage.qml:138`
  `preferences-system-windows-behavior`. D1: installation.md:86 (accents shown as they will look,
  desks named with a line each).

### F104 · An empty host name gets the character-rule message
ids: installer-20
- Evidence: `shots/installer/45-host-empty.png`, `host-validation-montage.view.png`. Fix U09:
  `AccountPage.qml:32-34` "Give the computer a name." first.

### F105 · KDOS application groups show no port count
ids: installer-21
- Evidence: `shots/installer/50-apps.png`, `51-apps-scroll1.crop-880-280-840-680.png`. Fix U09:
  `AppsPage.qml` subtitle `i18np("%1 port", "%1 ports", modelData.ports)`. D1 (installation.md:87),
  D6 (kinstall.md:1256).

### F106 · The pending-apps notice pairs a singular summary with a plural body
ids: installer-22
- Evidence: `shots/installer/84-session-now.crop-1500-800-420-230.png`, `85-discover-selected.png`.
  Fix U15: `kdos-notice/decide.c:483-492` follows `pd->n`; `tests/run.sh` case.

### F107 · The Done text for autologin into a locked session leaves out the lock
ids: installer-25
- Evidence: `shots/installer/113-luks-done.png`, `130-lock-prompt.png`. Fix U09: `DonePage.qml:41-42`
  "…the session starts locked; your password unlocks it."

### F108 · The update description says "to download" for a stick and has an empty Licenses field
ids: updates-12
- Evidence: `shots/updates/27-progress-60s.png`, `29-tray-after-apply.png`, `16-moreinfo-reopen.png`.
  Fix U04: `appstream.c:146-151` drops the verb (or "to copy" for a local path), emits
  `<project_license>MIT</project_license>`; header :16-25; test under `kdos-sysupdated/tests`. D6:
  discover.md:337-344. Upstream offline-update strings stay.

### F109 · The "Start this version next time?" sheet spans the whole page
ids: updates-14
- Evidence: `shots/updates/57-start-this-version-click.png`, `63-rollback-confirm-dialog.png`,
  `66-rollback-logout.png`, `94-github-check-25s.png`. Fix U05: `main.qml:330-357` startDialog
  `preferredWidth: Kirigami.Units.gridUnit * 24`.

### F110 · Discover install failures show only "Please try again later"
ids: packages-12
- Evidence: `shots/packages/113-offline-result.crop-480-140-960-760.png`, `114-offline-details…`,
  `79-lo-result.view.png`, `73-after-remove…`. Fix U07: `KdosTransaction::fail` hands the reason to
  the resource; `KdosResource.cpp:460-475` shows "<name> was not installed: <why>". D6: discover.md
  "Installing and removing".

### F111 · The install confirmation wraps "30.44 / MiB"
ids: packages-13
- Evidence: `shots/packages/44-install-confirm.crop-480-140-960-760.png`, `76-lo-confirm…`,
  `52-progress.view.png` (progress fill exists). Fix U07: `KdosPlan.cpp:68` joins number and unit
  with QChar::Nbsp.

### F112 · `kdos app remove` of an app that is not installed says "removed"
ids: packages-17
- Evidence: `logs/packages/run10.log` (post10), `shots/packages/101-remove-click.crop-480-140-960-760.png`.
  Fix U17: `store.c:690-741` prints "<id> is not installed" when nothing exists. D2: applications.md
  "Removing".

### F113 · KDOS Welcome pages inset content 8 px against an 18 px header; mixed headings on Look
ids: boot-08
- Evidence: `shots/boot/ev-22-welcome-look-accent-inset-zoom.png`, `10-welcome-1080-p07.png`, p12,
  p13, p16. Fix U22: `kdos/Flick.qml:27` gutter `Kirigami.Units.gridUnit`; `10-Look.qml` one heading
  style.

### F114 · The docs call Make It Yours the last tour page and leave out What's New
ids: boot-11 (tour half), docs-19 (b)
- The tour ends on Plasma's "Enjoy It!". Evidence: `shots/boot/10-welcome-1080-p18.png`,
  `src/desktop/kdos-welcome/extra-pages/90-WhatsNew.qml`. Fix U22: `kdos-welcome/kfs.md:11`. D1
  (getting-started.md:260-265), D7 (plasma-shell.md:402).

### F115 · The Getting Started popup runs flush to its frame and reads "4 things left to do of 5."
ids: desktop-05, kdosdesk-16
- Evidence: `shots/desktop/12-tray-check.crop-1455-590-250-90-x4.png`, `117-checklist-bottom…`,
  `shots/kdosdesk/c-r8-traycl.png`, `c-r4-checkbot.png`. Fix U23: `applet/qml/main.qml:55-63` side
  margins and a vertical ScrollBar; `ChecklistView.qml:51` "%1 of %2 left to do.".

### F116 · The tray's contents differ from the guide's list
ids: desktop-07
- Evidence: `shots/desktop/19-tray-expander.crop-1200-450-720-630.png`, `28-x-weather…`. Fix D2:
  desktop.md:116-117 lists what the tray holds (Vaults, weather, display configuration, input
  method; clipboard and printers when there is something to show).

### F117 · The Tiling desk's pager draws window outlines over the desktop names
ids: desktop-09
- Evidence: `shots/desktop/112-desk-tiling.crop-0-0-300-26-x4.png`. Fix U10: tiling `layout.js`
  `pager.writeConfig("showWindowOutlines", false)` and its header. D2: desktop.md:56.

### F118 · The key card orders an action's keys differently in every session
ids: desktop-10
- Evidence: `shots/desktop/80-keycard.view.png`, `121-meta-slash.view.png`. Fix U24: `keys.js:109-116`
  sorts each row's keys (fewest `+`, then localeCompare); `tests/tst_keys.qml.in` case. D7: kwin.md
  key card section.

### F119 · Toggle and runner notifications use command words, and a refusal reads as success
ids: desktop-12, kdosdesk-14
- "kdos / night-light on"; a refused `phosphor on` is titled "Phosphor mode on". Evidence:
  `shots/desktop/107-nightlight.half.png`, `150-phosphor-enter.crop-1200-450-720-630.png`,
  `shots/kdosdesk/c-r6-phos.png`. Fix U15: `toggle.c` label column ("Night Light on"); `run.c:113-116`
  summary "Failed: <row>". D5 (kdos-command.md:543-546), D7 (plasma-shell.md:199-201).

### F120 · VLC's first-run privacy dialog opens at the screen corner with a clipped title
ids: apps-09 (packages-18 under Uncertain)
- Evidence: `shots/apps/42-vlc.png`, `42-vlc.crop-90-0-445-90-x3.png`, `shots/packages/08-vlc.crop-90-0-445-70-x4.png`.
  Fix U21: ship `fs/etc/skel/.config/vlc/vlcrc` with `[qt] qt-privacy-ask=0` and `[core]
  metadata-network-access=0`. D8 (configuration.md skel table ~902-934), D2 (applications.md:379).

### F121 · Yakuake keeps Breeze's light skin under every accent
ids: apps-10
- Evidence: `shots/apps/13-yakuake-start.png`, `13-yakuake-start.crop-90-455-500-70-x3.png`. Fix D2:
  theming.md:15-17 states Yakuake's frame is its skin's images (optionally ship a yakuakerc with
  `Skin=plastik_dark` in a later fs change).

### F122 · The KDOS icon themes inherit Adwaita, which is not installed
ids: apps-13, kdosdesk-21
- Every KDE program logs `Icon theme "Adwaita" not found`; GTK programs lack the symbolic fallback.
  Evidence: `logs/apps/run2.log`, `logs/kdosdesk/run4.log`, `run8.log`, `run13.log`. Fix U20:
  `depends = adwaita-icon-theme` in `src/art/kdos-icons/kpkgbuild`; kdos-icons kfs.md:71.

### F123 · Elisa's empty playlist footer reads "0 tracks,  total"
ids: apps-14
- Evidence: `shots/apps/10-elisa.crop-1230-820-180-35-x4.png`, `105-elisa-tracks.png`. Fix U28:
  `ports/core/music-players/elisa/empty-playlist-footer.patch` (MediaPlayListView.qml:507-509 uses
  the -1 branch only when tracksCount > 0), applied in build.sh, upstream MR in kfs.md.

### F124 · Info Center → Firmware Security shows a red "package has not been validated" warning
ids: apps-15
- Evidence: `shots/apps/114-kinfo-firmware-security.png`. Fix U29: `ports/core/boot/fwupd/build.sh:42`
  `-Dsupported_build=enabled`; kfs.md:71-74.

### F125 · Firefox ESR logs two ignored policies at every start
ids: apps-16
- Evidence: `logs/apps/run3.log` (47-48). Fix U29: drop TranslateEnabled and GenerativeAI from
  `firefox-esr/build.sh:124-130`; kfs.md:104-110 (AIControls blocks and locks them).

### F126 · Health's Check Again tooltip reads "Trigger this action with 15"
ids: settings-03
- Evidence: `shots/settings/108-health-checkagain.crop-1120-240-320-70-x4.png`. Fix U25:
  `kcm-kdos-health/ui/main.qml:39` `shortcut: "F5"`. D6: kdos-settings.md:121 "Check Again (F5)".

### F127 · The Date & Time calendar draws Sunday dates in dark red at ~1.8:1
ids: settings-05
- Evidence: `shots/settings/104-clock-after.crop-840-390-300-200-x3.png`,
  `38-kcm_clock.crop-440-170-1040-700.png`. Fix U26:
  `ports/core/kf6/kwidgetsaddons/rest-day-contrast.patch` (red chosen by the cell ground's
  lightness, kdatetable.cpp:284/:372), build.sh, kfs.md Patches with MR.

### F128 · The Add Layout dialog lists languages Z→A
ids: settings-06
- Evidence: `shots/settings/67-kbd-add-dialog.png`, `m-kbd-67-70.png`. Fix U27:
  `ports/core/kde/plasma-desktop/layout-list-order.patch` (`KeyboardLayoutsModel.qml:15` ascending
  until searched), build.sh, kfs.md MR.

### F129 · A ninth scheme, "KDOS Box", sits among the accents on the Colors page
ids: settings-11, docs-19 (a)
- Evidence: `shots/settings/41-kcm_colors.crop-440-170-1040-700.png`. Fix D2: theming.md:50-51 and
  :93-97 name KDOS Box and say a non-accent scheme changes nothing below the desktop.

### F130 · The KDOS Look hint says the login screen and splash repaint "at once"
ids: settings-12
- Evidence: `shots/settings/02-look.crop-790-270-620-210-x3.png`. Fix U25:
  `kcm-kdos-look/ui/main.qml:204-205` (lock screen at once; login screen next time it shows; splash
  and boot menu at the next boot).

### F131 · Boxes page with no box: both columns headed "Boxes", Defaults enabled
ids: settings-15
- Evidence: `shots/settings/03-boxes.png`. Fix U25: `boxesmodule.cpp:46` `setRepresentsDefaults(true)`;
  `ui/main.qml:115` title "Box Settings"; kcmuitest case.

### F132 · Make It Yours says "press Check for Updates"; Discover's button is "Refresh"
ids: kdosdesk-10
- Evidence: `shots/kdosdesk/c-w3-miy.png`, `r4-07-disc-refreshed.view.png`. Fix U22:
  `95-MakeItYours.qml:149` and comment :27. D6 (discover.md:73, :244, :313), D7 (plasma-shell.md:431).
  Lands with F05.

### F133 · KRunner "accent" lists accents reversed and drops Amber and Bone
ids: kdosdesk-11
- Evidence: `shots/kdosdesk/06-kr-accent.view.png`. Fix U16: `match.c:296-300` relevance `0.6 - i *
  0.01`; header :14-16; matchtest order case.

### F134 · The `ask` row hides its advice in an elided subtext
ids: kdosdesk-12
- Evidence: `shots/kdosdesk/11-kr-ask.view.png`, `c-06-row1.png`, `07-kr-desk.view.png`,
  `09-kr-lib.view.png`. Fix U16: `ask.c:533-541` text "No local model is answering: install a
  language-model pack" (multiline), short subtext. D7: plasma-shell.md:235.

### F135 · KRunner install rows use icon `plasma-discover`, which no theme has
ids: kdosdesk-13
- Evidence: `shots/kdosdesk/c-08-icon.png`, `08-kr-install.view.png`, `logs/kdosdesk/run2.log`. Fix
  U16: `match.c:125` icon `plasmadiscover`.

### F136 · Desk rows say "the kdos desk"
ids: kdosdesk-15
- Evidence: `shots/kdosdesk/07-kr-desk.view.png`. Fix U16: `match.c:236` slot_shown maps
  kdos/dock/tiling to KDOS/Dock/Tiling; matchtest.

### F137 · Discover features a one-app "Essential set" first; some catalogue apps show "…" icons
ids: kdosdesk-17
- Evidence: `shots/kdosdesk/c-w3-disc-feat.png`, `r4-08-disc-search.view.png`, `r4-09-disc-app.view.png`,
  `r4-10-disc-set.view.png`. Fix U07: drop `set/essential` from `featured.json`. U17: catalogue meta
  categories TextEditor→Development, Biology→Science, Electronics→Development, Java→Development. D6:
  discover.md (a catalogue app shows its category's icon).

### F138 · Discover lists console tools and add-ons under Applications with unknown-file icons
ids: kdosdesk-18
- Evidence: `shots/kdosdesk/r4-11-disc-installed.view.png`, `c-r4-11-icon.png`. Fix U07: portEntry
  records the AppStream kind; `KdosResource::type()` maps addon→Addon, other→ApplicationSupport;
  `utilities-terminal` fallback; backendtest case. D6: discover.md host-ports paragraph.

### F139 · The key card names mc "Midnight Commander" while every other row is "<program> (<what>)"
ids: kdosdesk-19
- Evidence: `shots/kdosdesk/w2-kc1.view.png`, `sheet-r12-1.png`. Fix U24: runraise `main.qml:106` "Raise
  or Open mc (files)"; `org.kdos.runraise.mc.desktop` Name/Comment.

### F140 · kdos-bb ignores a terminal resize
ids: kdosdesk-20
- Evidence: `shots/kdosdesk/r5-06-bb-a.view.png`, `r5-07-bb-b.view.png`, `r5-08-bb-c.view.png`. Fix
  U28: `ports/core/image-libs/aalib/curses-resize.patch` (curses_getsize reads TIOCGWINSZ and calls
  resizeterm), build.sh, kfs.md. D5: kdos-bb.md (a resize is taken at the next scene).

### F141 · The docs say Discover's searches sort by name; they sort by relevance
ids: kdosdesk-23
- Evidence: `shots/kdosdesk/r4-08-disc-search.view.png`, `r4-11-disc-installed.view.png`. Fix D6
  (discover.md:40-42), U30 (`fs/usr/share/kdos/xdg/discoverrc:28-30` comment).

### F142 · kdos-notice notifications have a plain "KDOS" header with no mark or configure button
ids: kdosdesk-24
- Evidence: `shots/kdosdesk/c-r6-notice.png`, `c-r8-offer.png`. Fix U15: `main.c:146-169` sends
  `desktop-entry kdos-notice`; build.sh installs `kdos-notice.desktop` under applications with
  `X-GNOME-UsesNotifications=true`. D7: plasma-shell.md:264-266.

### F143 · `kpkg search` prints 60 lines of usage without saying the command is unknown
ids: cli-10
- Evidence: `logs/cli/run1.clean.log` (1706-1818, 1843-1884, 866-868). Fix U19: `front.c` prints
  "kpkg: unknown command '<cmd>'" before usage. U18 (optional): `app.c` no-match hint names
  `kdos update install --dry-run <name>`.

### F144 · bash completion misses six subcommands and two `update` verbs
ids: cli-15
- Evidence: `shots/cli/37-tty2-completion.png`, `logs/cli/run1.clean.log` (1330-1352). Fix U30:
  `fs/usr/share/bash-completion/completions/kdos` first words, update verbs, `app)` and `image)`
  cases. D5: kdos-command.md:101-110.

### F145 · The console's bright black is the fill colour, 1.69:1
ids: cli-17
- htop's figures barely visible on tty2. Evidence: `shots/cli/35-tty2-htop.png`,
  `35-tty2-htop.crop-780-30-1140-230-x2.png`. Fix U31: `kcolor.c:218-232` slot 8 = `kcol_muted(sc)`
  (as kcol_ansi), kcolor.h header, regenerate `fs/etc/vtrgb` (`kdos-bootctl palette`). D2:
  theming.md:283-288.

### F146 · Manual page NAME lines are table cells out of context and lose underscores and `--`
ids: cli-18
- "service - The same supervisor, under the conventional name"; "$XDGRUNTIMEDIR", "-json". Evidence:
  `logs/cli/run1.clean.log` (1281-1295, 1400-1461, 1558-1562, 1580-1620). Fix U32: `mkbook.py`
  escapes Markdown specials and turns off smart typography. D8: command-index.md:235, :279.

### F147 · Small slips: counts, a plural, a dangling distrobox line, a missing user service
ids: cli-22, docs-18
- command-index.md:78 says 41 subcommands (43); administration.md:1362 "Four more places" over 8
  rows; "1 bundled copies"; `kdos-box create` prints "To enter, run:" and nothing; administration.md
  omits ssh-agent and never says `/etc/kdos/update.conf` must be created. Evidence:
  `logs/cli/run2.clean.log` (292-297), `run1.clean.log` (255-256, 667-677).
- Fix D8 (command-index.md:78), D3 (administration.md:81-82, :1211, :1269, :1362), U18 (`cve.c:584`),
  U17 (`box_cmd.c`: print `kdos-box enter <name>` after creating).

### F148 · The application chooser lists the hidden "Accessibility" entry with a blank icon
ids: media-15
- Evidence: `shots/media/13-open-video.crop-630-410-420-140-x3.png`. Fix U27 (optional): patch in
  `ports/core/xdg/xdg-desktop-portal-kde` skipping NoDisplay services in
  `AppModel::loadApplications`, with the MR in kfs.md; otherwise report upstream.

### F149 · The RT-throttle sentence names a knob that throttles nothing on this kernel
ids: security-07
- Evidence: `logs/security/run2.log` (RT), `run1.log`. Fix D4 (security-model.md:176-178) and U30
  (`limits.d/30-kdos.conf:22-24` comment): the fair-class deadline server bounds priority 1.

### F150 · The Firewall page header starts "Everything else…" and omits what always gets in
ids: security-11
- Evidence: `shots/security/30-firewall-kcm.crop-766-250-660-110-x3.png`, `logs/security/run1.log`,
  `run12.log`. Fix U25: `kcm-kdos-firewall/ui/main.qml:65`. D6: kdos-settings.md Firewall.

### F151 · The front page shows a failing check from GitHub's branch Pages build
ids: release-06
- Evidence: `shots/release/crop-front-commit-status.png`, `front-page-light.png`,
  `data/cr-4b1db5b0b436edff6064b63e67210385ef0045fb.json`, `data/run-44091156915.json`. Fix U34
  (operational): Pages source "GitHub Actions" (`gh api -X PUT repos/kunaldawn/kdos/pages -f
  build_type=workflow`).

### F152 · The README hero screenshot is a full image with fastfetch scrolled and wrapped
ids: release-08
- Evidence: `shots/release/crop-readme-konsole-top.png`, `crop-readme-desktop-clock.png`,
  `crop-front-hero-light.png`, `iso-contents.log`. Fix U35: recapture `docs/screenshots/konsole.png`
  from the base ISO with the whole block.

### F153 · The release notes offer a DVD, and no page says how to burn one
ids: release-10
- Evidence: `shots/release/data/release-notes-body.md`, `iso-media.log`. Fix D1: one sentence after
  getting-started.md:130 and in README "burn the ISO as a disc image" (`growisofs -dvd-compat
  -Z /dev/sr0=kdos-<version>.iso`; Windows "Burn disc image").

### F154 · The QEMU try-it command hard-codes Debian's OVMF path
ids: release-18
- Evidence: README.md:98, getting-started.md:95. Fix D1: one sentence naming `/usr/share/edk2/`
  (Arch `x64/OVMF.4m.fd`) and the pflash form.

### F155 · The notes say "1.6GB" for a 1.54 GiB asset
ids: release-12
- Evidence: `shots/release/release-page.png`, `data/release-v0.2.json`. Fix U03: `ports/publish:321`
  `numfmt --to=iec-i --suffix=B`.

### F156 · The release notes name the guides as plain paths, not links
ids: release-13
- Fix U03: `ports/publish:1990` Markdown links to `blob/$tag/…/getting-started.md` and
  `installation.md`; patch the v0.2 body by hand.

### F157 · GitHub does not recognise the MIT licence
ids: release-15
- Evidence: `shots/release/front-page-light.png`, LICENSE:1-12. Fix U34: delete the banner block at
  LICENSE:3-12.

### F158 · Tag `v0.2` and branch `v0.2` point at different commits
ids: release-16
- This is why the release was published from an untagged ref (F24). Fix U34 (operational,
  destructive, the user runs it): `git push origin --delete refs/heads/v0.2`.

### F159 · known-gaps describes a 1,870 MiB ISO and says no ISO carried facets; both are past
ids: docs-12
- Fix D8: known-gaps.md:1250-1259 (1,573 MiB ISO, 1,517 MiB squashfs, 327 MiB headroom; the codec
  timing gap stays) and :1269-1279 (the released ISO carries FACETS=run; the unrun parts stay).

### F160 · getting-started says the boot menu and consoles take a new accent "with nothing restarted"
ids: docs-16
- Fix D1: getting-started.md:382-384 says what changes at once, at the next prompt and at the next
  boot, and that the live boot menu cannot be rewritten.

### F161 · Minor page mismatches: favourites file, brlapi group, "never a KDE application"
ids: docs-19 (c, d, f)
- Fix D2 (desktop.md:84-85: the favourites file is the boxes started at login, not the panel pins;
  applications.md:732-734: Okteta, KStars and Heaptrack are rows because they have no port), D1
  (getting-started.md:218 adds `brlapi`).

### F162 · The user guide explains build-tree internals where a user needs a step
ids: docs-20
- Fix D2: drop the chordcheck (desktop.md:229-231) and mimecheck (applications.md:636-637)
  sentences; shorten accessibility.md:63-68; drop the announcement-record section (:376-385),
  which c-libraries.md#announcements already documents (run booknav and anchorcheck).

---
## Fix units

One unit is one coherent change; no two units change the same file. `Sev` is the highest severity among its findings. "image" means `make build BUILD_ARGS="--phases 70_image"` after the port rebuilds; "rig" means a `vnc-shot.py` run on the rebuilt ISO.

| Unit | Change | Sev | Findings | Files | What proves it |
|---|---|---|---|---|---|
| U01 | The image names its release and carries its notes | blocker | F01, F16 | `Makefile`<br>`script/chroot/exec.sh`<br>`script/phases/70_image/105_ports.sh`<br>`ports/RELEASE` (new) | image from a detached, clean, tagged HEAD (`git switch --detach v0.2; make build BUILD_ARGS="--phases 70_image"`); then `cat /ports/TREE` shows `version v0.2` and `/ports/RELEASE` exists on the ISO; rig: `kdos update install --dry-run kcharselect` plans prebuilt packages with no update.conf |
| U02 | kdos update: fetch, install, apply and the image lane | blocker | F01, F02, F10, F11, F18, F23, F31, F38, F42, F70, F83 | `src/system/kdos-tools/update.c`<br>`src/system/kdos-tools/image.c`<br>`src/system/kdos-tools/bootctl.c`<br>`testing/selftest.sh`<br>`ports/buildonly`<br>`script/phases/70_image/087_gates.sh`<br>`script/release-image.sh` | selftest (carry cases for /boot/efi and a .d directory, same-commit named-tree fetch, small-tmpfs refusal, unfill); port kdos-tools (42_graphics) + image; rig: apply the 0.2.1 stick image, B mounts /boot/efi and logs `mark-good: slot b confirmed good` on its first boot and stays active; the greeter asks for a password; `ls -a /etc/sddm.conf.d` shows `.kde_settings.conf.kdos-new`; manifest `.` and `ports` are `0 0`; Cancel then `kdos-bootctl try b 1` refuses; `kdos update install --dry-run kcharselect` lists no cmake, ninja, sphinx or pip |
| U03 | Release script and the v0.2 release page | blocker | F01, F16, F22, F28, F29, F155, F156 | `ports/publish` | docs-only: `make release TAG=v0.2 PUBLISH_ARGS=--dry-run` and read notes.md; `ports/publish --release v0.2 --dry-run` lists RELEASE and refuses an ISO whose /ports/TREE has no `version`; operational: PATCH the public v0.2 body (figure, requirements, links, openssl recipe, `binhost =` workaround) |
| U04 | kdos-sysupdated: live fetch, channels, offer text | major | F05, F39, F68, F108 | `src/daemons/kdos-sysupdated/bus.c`<br>`src/daemons/kdos-sysupdated/state.c`<br>`src/daemons/kdos-sysupdated/appstream.c`<br>`src/daemons/kdos-sysupdated/tests/`<br>`testing/fixtures/sysupdated/drive.sh` | selftest (sysupdated fixture: no-slot and live cases call `fetch --binhost-only`; `SetChannel http://` refused); port kdos-sysupdated (50_desktop) + image; rig: live ISO, Discover from Kickoff, Updates, F5, `ls /var/lib/kdos/binhost` exists and a Discover search finds filelight |
| U05 | System Versions page | minor | F37, F38, F41, F68, F109 | `src/desktop/kcm-kdos-system/ui/main.qml`<br>`src/desktop/kcm-kdos-system/ui/SlotForm.qml`<br>`src/desktop/kcm-kdos-system/slotinfo.cpp`<br>`src/desktop/kcm-kdos-system/systemversions.cpp`<br>`src/desktop/kcm-kdos-system/CMakeLists.txt`<br>`src/desktop/kcm-kdos-system/tests/kcmuitest.cpp`<br>`src/desktop/kcm-kdos-system/tests/fakeslots.cpp`<br>`src/desktop/kcm-kdos-system/tests/systemversionstest.cpp` | make plasmatest (one-slot fixture, incomplete slot, http channel row); port kcm-kdos-system (50_desktop); testing/quick.sh kcm-kdos-system on the one-slot disk and with Discover open (Update in Discover raises it) |
| U06 | Discover patches: offer read after CheckNew, restart prompt clears | minor | F39, F40 | `ports/core/kde/discover/sysupdate-appstream-after-check.patch` (new)<br>`ports/core/kde/discover/notifier-reboot-clears.patch` (new)<br>`ports/core/kde/discover/build.sh`<br>`ports/core/kde/discover/kfs.md`<br>`src/desktop/kdos-discover/notifier/KdosNotifier.cpp`<br>`src/desktop/kdos-discover/tests/notifiertest.cpp` | make plasmatest (notifiertest); ports discover and kdos-discover (50_desktop) + image; rig: fresh install that never checked shows 1.5 GiB and the signed text in its first Discover session; Keep slot A clears the tray's restart tooltip |
| U07 | Discover's KDOS backend and catalogue writer | major | F12, F43, F110, F111, F137, F138 | `src/desktop/kdos-discover/backend/KdosBackend.cpp`<br>`src/desktop/kdos-discover/backend/KdosResource.cpp`<br>`src/desktop/kdos-discover/backend/KdosResource.h`<br>`src/desktop/kdos-discover/backend/KdosTransaction.cpp`<br>`src/desktop/kdos-discover/catalog/main.cpp`<br>`src/desktop/kdos-discover/model/KdosPlan.cpp`<br>`src/desktop/kdos-discover/data/featured.json`<br>`src/desktop/kdos-discover/tests/backendtest.cpp` | make plasmatest (backendtest: console-application and addon kinds); port kdos-discover (50_desktop) + image with KDOS_MAKE_BINHOST=1 (regenerates kdos-binhost.xml); rig: with the index, `libreoffice` gives one native LibreOffice with its icon and no Debian Writer/Calc/Impress, `filelight` one row, an offline install names its reason on the page |
| U08 | Installer core (kinstall-core, kinstall) | blocker | F01, F14, F23, F76, F77, F79, F91, F95 | `src/system/kinstall-core/conf.c`<br>`src/system/kinstall-core/install.c`<br>`src/system/kinstall-core/dump.c`<br>`src/system/kinstall-core/probe.c`<br>`src/system/kdos-installer/main.c`<br>`src/system/kdos-installer/pages.c`<br>`testing/fixtures/kinstall/` (new 5700 MiB probe case) | selftest (5700 MiB default-swap plan refused, swap=none passes; `-x` and `kdos-` refused; plan prints one swap line); `make build BUILD_ARGS="--phases 10_bootstrap,70_image --steps 10_bootstrap:130_kinstall.sh"`; rig: GUI install with kde-system ticked onto 48G: no binhost warning on Done, filelight installed, `kdos-bootctl status` shows slot b's version, no `leaked` in kinstall.log, time left shrinks |
| U09 | Graphical installer | major | F15, F63, F78, F79, F80, F81, F91, F92, F96, F97, F98, F99, F100, F101, F102, F103, F104, F105, F107 | `src/desktop/kdos-installer-gui/qml/DiskPage.qml`<br>`src/desktop/kdos-installer-gui/qml/SummaryPage.qml`<br>`src/desktop/kdos-installer-gui/qml/KeyboardPage.qml`<br>`src/desktop/kdos-installer-gui/qml/WelcomePage.qml`<br>`src/desktop/kdos-installer-gui/qml/AccountPage.qml`<br>`src/desktop/kdos-installer-gui/qml/StrengthLabel.qml`<br>`src/desktop/kdos-installer-gui/qml/EncryptionPage.qml`<br>`src/desktop/kdos-installer-gui/qml/DonePage.qml`<br>`src/desktop/kdos-installer-gui/qml/LookPage.qml`<br>`src/desktop/kdos-installer-gui/qml/AppsPage.qml`<br>`src/desktop/kdos-installer-gui/qml/Main.qml`<br>`src/desktop/kdos-installer-gui/answers.cpp`<br>`src/desktop/kdos-installer-gui/backend.cpp`<br>`src/desktop/kdos-installer-gui/backend.h`<br>`src/desktop/kdos-installer-gui/CMakeLists.txt`<br>`src/desktop/kdos-installer-gui/kpkgbuild`<br>`src/desktop/kdos-installer-gui/tests/installertest.cpp` | make plasmatest (qmllint, installertest); port kdos-installer-gui (50_desktop); testing/quick.sh kdos-installer-gui with a blank and an installed 48G disk: Tab reaches and Space selects the disk row, Keyboard page opens at English (US), Partition Manager close keeps Erase, Open the Log brings Kate to the front |
| U10 | First-login desk, panel look and greeter | major | F09, F59, F60, F86, F87, F88, F89, F117 | `fs/usr/share/kdos/xdg/kconf_updaterc`<br>`fs/etc/sddm.conf.d/00-kdos.conf`<br>`src/desktop/kdos-plasma-look/desks/org.kdos.desk.kdos/contents/layout.js`<br>`src/desktop/kdos-plasma-look/desks/org.kdos.desk.dock/contents/layout.js`<br>`src/desktop/kdos-plasma-look/desks/org.kdos.desk.tiling/contents/layout.js`<br>`src/desktop/kdos-plasma-look/genlook.py`<br>`src/desktop/kdos-plasma-look/kfs.md`<br>`src/system/kdos-tools/desk.c`<br>`src/art/kdos-theme/kdecolors.c`<br>`src/art/kdos-theme/kfs.md`<br>`src/art/kdos-sddm-theme/kfs.md` | fs; ports kdos-theme (41_system), kdos-tools (42_graphics), kdos-plasma-look (50_desktop) + image; python3 testing/lnfcheck.py; rig: 20+ fresh boots (default and clean-session) all with `menuLabel=Start`, the KDOS launchers, `chartFace=org.kdos.meters` and `[Containments][1] plugin=org.kde.plasma.folder`; greeter user block ≥4.5:1 by pixel scan; greeter Virtual Keyboard follows the scheme; clock doubles at 200%; slider fill ≥3:1; Kickoff header gap |
| U11 | Boot path: splash, initramfs, ISO menu, rcS, fstab | minor | F34, F35, F36, F69, F84, F85, F93 | `src/art/kdos-splash/kdos-splash.c`<br>`script/phases/70_image/090_initramfs.sh`<br>`script/phases/70_image/110_iso.sh`<br>`fs/etc/init.d/rcS`<br>`fs/etc/fstab` | selftest (unlock_root under PASS_TTY); port kdos-splash (41_system); fs; image; `kdos-splash preview 1920x1080 9 out.ppm` detail text ≥4.5:1; rig: installed plain and LUKS boot frame series show no console text over the splash (also decides variants-03), three wrong passphrases show the Ctrl+Alt+Del line and Ctrl+Alt+Del returns to the prompt, verbose entry serial gap <5 s, `findmnt -no OPTIONS /` has noatime, `cat /sys/kernel/security/lsm` works |
| U12 | Privacy lamp ignores the shell's thumbnails | major | F06 | `src/system/kdos-tools/privacy.c`<br>`testing/fixtures/kdos-command/pw-dump.json` | selftest (screen list stays [pw:60:61, pw:70:72] with a plasmashell reader added); port kdos-tools (42_graphics) + image; rig: hover a task entry, `kdos privacy --json` screen:[] and no lamp; Alt+Print recording still lights it |
| U13 | `kdos restarts` counts only executable mappings | minor | F17 | `src/system/kdos-tools/restarts.c` | selftest; port kdos-tools (42_graphics) + image; rig: install gimp in a session, `kdos restarts` says nothing is running deleted code and no notice appears |
| U14 | `ksvc` reads a root supervisor as alive for any caller | major | F07 | `src/system/kdos-tools/ksvc.c` | selftest; port kdos-tools (42_graphics) + image; rig: as kdos `service list` all [ OK ] and `ksvc check dbus; echo $?` prints 0 |
| U15 | KDOS notices and the `kdos` front end | minor | F19, F20, F25, F32, F54, F106, F119, F142 | `src/system/kdos-tools/kdos.c`<br>`src/system/kdos-tools/toggle.c`<br>`src/system/kdos-tools/kdos.notifyrc` (new)<br>`src/system/kdos-tools/build.sh`<br>`src/libs/libkbase/kb_proc.c`<br>`src/desktop/kdos-notice/main.c`<br>`src/desktop/kdos-notice/decide.c`<br>`src/desktop/kdos-notice/build.sh`<br>`src/desktop/kdos-notice/kdos-notice.desktop`<br>`src/desktop/kdos-notice/tests/run.sh`<br>`src/desktop/kdos-runner/run.c` | selftest (a libkbase edit rebuilds every port of ours); kdos-notice tests/run.sh; ports kdos-tools (42_graphics), kdos-notice and kdos-runner (50_desktop) + image; rig: `kdos remind in 1m tea` stays in the bell under KDOS; `kdos why --help` prints usage, exit 0; Health shows a grey live-session row; Night Light notice titled "Night Light on" |
| U16 | KRunner palette rows | minor | F46, F133, F134, F135, F136 | `src/desktop/kdos-runner/match.c`<br>`src/desktop/kdos-runner/ask.c`<br>`src/desktop/kdos-runner/tests/matchtest.c`<br>`src/desktop/kdos-runner/kfs.md` | selftest (kr-matchtest: `install kdos` gives no Discover row, accent order, desk names); port kdos-runner (50_desktop); testing/quick.sh kdos-runner: `Install KDOS` + Enter starts the installer, `accent` shows Amber and Bone |
| U17 | Boxes (kdos-appbox) and the catalogue | minor | F32, F53, F67, F71, F112, F137, F147 | `src/system/kdos-appbox/box.c`<br>`src/system/kdos-appbox/box_cmd.c`<br>`src/system/kdos-appbox/main.c`<br>`src/system/kdos-appbox/store.c`<br>`src/system/kdos-appbox/catalogue` | selftest; port kdos-appbox (43_toolkits) + image; rig: create and remove a box leaves no .desktop; `su - eve -c 'kdos-appbox status'` prints `packd : not permitted`; profile of a pack box names the lane's mechanism and the `!` line after `profile home=private`; second `kdos app remove` says not installed |
| U18 | Small `kdos` commands: fetch-app, app, cve, speech | minor | F45, F53, F55, F58, F143, F147 | `src/system/kdos-tools/fetch.c`<br>`src/system/kdos-tools/app.c`<br>`src/system/kdos-tools/cve.c`<br>`src/system/kdos-tools/speech.c` | selftest; port kdos-tools (42_graphics) + image; rig: `kdos-fetch-app firefox-esr` installs with ordered `==>` lines; `kdos app info app.zathura` shows the chain total (~626M); `kdos speech say` names the piper install |
| U19 | kpkg: one-thread extraction, quiet installs, world on remove | major | F13, F44, F52, F143 | `src/system/kdos-kpkg/add.c`<br>`src/system/kdos-kpkg/del.c`<br>`src/system/kdos-kpkg/front.c` | selftest and `CC="cc -fsanitize=address,undefined -g" testing/selftest.sh`; `make build BUILD_ARGS="--phases 10_bootstrap,70_image --steps 10_bootstrap:120_kpkg.sh"`; rig at -m 4G: install gimp then libreoffice succeeds with xz under 100 MB; `kdos update install kcharselect \| wc -l` in the tens; `kpkg remove smu` drops it from /var/lib/kdos/world |
| U20 | What the base image carries | blocker | F03, F26, F72, F73, F122 | `ports/base`<br>`src/system/kdos-tools/kpkgbuild`<br>`src/art/kdos-icons/kpkgbuild`<br>`src/art/kdos-icons/kfs.md` | testing/preflight.sh and python3 testing/phaseclosure.py; image; `python3 script/lib/imageset.py --why orca` (and brltty, liblouis, plocate, isoimagewriter, wl-clipboard, adwaita-icon-theme) all in the base; ISO under the 1,900 MiB gate; rig: Meta+Alt+S starts orca and speech reaches the sink monitor; `service start brltty` skips with no /etc/brltty.conf; `printf x \| wl-copy; wl-paste`; `locate` works after login; no `Adwaita not found` on stderr |
| U21 | Default applications and VLC | major | F08, F120 | `fs/usr/share/kdos/xdg/kde-mimeapps.list`<br>`fs/etc/skel/.config/vlc/vlcrc` (new)<br>`ports/core/video-players/vlc/kfs.md` | fs; image; python3 testing/mimecheck.py --root build/fs; rig: Enter on ~/Videos/clip-h264.mp4 in Dolphin plays in VLC; VLC from Kickoff shows no privacy dialog and keeps metadata-network-access=0 |
| U22 | Welcome pages | minor | F19, F51, F113, F114, F132 | `src/desktop/kdos-welcome/extra-pages/50-Library.qml`<br>`src/desktop/kdos-welcome/extra-pages/80-Help.qml`<br>`src/desktop/kdos-welcome/extra-pages/95-MakeItYours.qml`<br>`src/desktop/kdos-welcome/extra-pages/10-Look.qml`<br>`src/desktop/kdos-welcome/extra-pages/kdos/Flick.qml`<br>`src/desktop/kdos-welcome/kfs.md` | make plasmatest (qmllint); port kdos-welcome (50_desktop); testing/quick.sh kdos-welcome -- --cmd 'plasma-welcome --pages 50-Library.qml' (and 80-Help, 95-MakeItYours, 10-Look): no empty heading, the KFS button greyed, 18 px gutters |
| U23 | Getting Started plasmoid | polish | F115 | `src/desktop/plasmoid-kdos-checklist/applet/qml/main.qml`<br>`src/desktop/plasmoid-kdos-checklist/qml/ChecklistView.qml` | make plasmatest; port plasmoid-kdos-checklist (50_desktop); testing/quick.sh plasmoid-kdos-checklist: popup crop at 4× shows the inset |
| U24 | Key card order and run-or-raise labels | minor | F21, F118, F139 | `src/desktop/kwin-effect-kdos-keys/package/contents/ui/keys.js`<br>`src/desktop/kwin-effect-kdos-keys/tests/tst_keys.qml.in`<br>`src/desktop/kwin-script-kdos-runraise/package/contents/ui/main.qml`<br>`src/desktop/kwin-script-kdos-runraise/kglobalaccel/org.kdos.runraise.mc.desktop`<br>`src/desktop/kwin-script-kdos-runraise/kfs.md` | make plasmatest; ports kwin-effect-kdos-keys and kwin-script-kdos-runraise (50_desktop); testing/quick.sh: Meta+F1 on two boots gives the same chip order and "Raise or Open mc (files)" |
| U25 | KDOS settings pages: Health, Look, Boxes, Firewall | polish | F126, F130, F131, F150 | `src/desktop/kcm-kdos-health/ui/main.qml`<br>`src/desktop/kcm-kdos-look/ui/main.qml`<br>`src/desktop/kcm-kdos-boxes/boxesmodule.cpp`<br>`src/desktop/kcm-kdos-boxes/ui/main.qml`<br>`src/desktop/kcm-kdos-boxes/tests/kcmuitest.cpp`<br>`src/desktop/kcm-kdos-firewall/ui/main.qml` | make plasmatest; ports kcm-kdos-health, kcm-kdos-look, kcm-kdos-boxes, kcm-kdos-firewall (50_desktop); testing/quick.sh each page |
| U26 | KDE Frameworks patches: help pages, rest days, focus frame | major | F15, F27, F127 | `ports/core/kf6/kguiaddons/help-local-page.patch` (new)<br>`ports/core/kf6/kguiaddons/build.sh`<br>`ports/core/kf6/kguiaddons/kfs.md`<br>`ports/core/kf6/kwidgetsaddons/rest-day-contrast.patch` (new)<br>`ports/core/kf6/kwidgetsaddons/build.sh`<br>`ports/core/kf6/kwidgetsaddons/kfs.md`<br>`ports/core/kf6/qqc2-desktop-style/focusrect-hasfocus.patch` (new)<br>`ports/core/kf6/qqc2-desktop-style/build.sh`<br>`ports/core/kf6/qqc2-desktop-style/kfs.md`<br>`src/system/kdos-doc/kfs.md` | python3 testing/kfscheck.py (Subject: lines); ports kguiaddons, kwidgetsaddons, qqc2-desktop-style (43_toolkits) + image; make plasmatest; rig: Help on KDOS Look opens Firefox at file:///usr/share/doc/kdos/04-programs/kdos-settings.html#kdos-look; Sunday column ≥4.5:1 under Bone; Tab focus visible on a QML radio and checkbox |
| U27 | Plasma and theme patches: layout list, button focus, chooser | minor | F61, F128, F148 | `ports/core/kde/plasma-desktop/layout-list-order.patch` (new)<br>`ports/core/kde/plasma-desktop/build.sh`<br>`ports/core/kde/plasma-desktop/kfs.md`<br>`ports/core/themes/breeze/button-focus-outline.patch` (new)<br>`ports/core/themes/breeze/build.sh`<br>`ports/core/themes/breeze/kfs.md`<br>`ports/core/xdg/xdg-desktop-portal-kde/hidden-entries.patch` (new, optional)<br>`ports/core/xdg/xdg-desktop-portal-kde/build.sh`<br>`ports/core/xdg/xdg-desktop-portal-kde/kfs.md` | python3 testing/kfscheck.py; ports breeze (43_toolkits), plasma-desktop and xdg-desktop-portal-kde (50_desktop) + image; rig: Add Layout starts at Albanian; focused Quit border ≥3:1 against unfocused; Open With has no Accessibility cell |
| U28 | Application patches: Elisa, Partition Manager, aalib | minor | F48, F123, F140 | `ports/core/music-players/elisa/empty-playlist-footer.patch` (new)<br>`ports/core/music-players/elisa/build.sh`<br>`ports/core/music-players/elisa/kfs.md`<br>`ports/core/disk/partitionmanager/size-dialog-ranges.patch` (new)<br>`ports/core/disk/partitionmanager/build.sh`<br>`ports/core/disk/partitionmanager/kfs.md`<br>`ports/core/image-libs/aalib/curses-resize.patch` (new)<br>`ports/core/image-libs/aalib/build.sh`<br>`ports/core/image-libs/aalib/kfs.md` | python3 testing/kfscheck.py; ports aalib (41_system), elisa and partitionmanager (44_apps) + image; rig: Elisa footer '0 tracks'; New Partition on an 8 GiB disk reaches the maximum; kdos-bb fills a maximised Konsole at the next scene |
| U29 | Recipe fixes: nmtui entry, fwupd, Firefox policies, DEP-11 icons | minor | F47, F94, F124, F125 | `ports/core/network/networkmanager/build.sh`<br>`ports/core/network/networkmanager/kfs.md`<br>`ports/core/boot/fwupd/build.sh`<br>`ports/core/boot/fwupd/kfs.md`<br>`ports/core/browsers/firefox-esr/build.sh`<br>`ports/core/browsers/firefox-esr/kfs.md`<br>`ports/core/xdg/debian-dep11/kpkgbuild`<br>`ports/core/xdg/debian-dep11/build.sh`<br>`ports/core/xdg/debian-dep11/kfs.md`<br>`ports/sources.idx` | `make fetch` and `ports/publish debian-dep11` for the new source; python3 testing/kfscheck.py; ports networkmanager, debian-dep11 (41_system), fwupd (42_graphics), firefox-esr (44_apps) + image; rig: Kickoff 'Network' shows the KCM and 'nmtui — Network connections in a terminal'; no 'not validated' line; no 'Ignoring … policy'; Kickoff 'mines' rows have icons |
| U30 | Shipped config comments and service status lines | minor | F50, F65, F90, F141, F144, F149 | `fs/etc/init.d/03_lvm.sh`<br>`fs/etc/init.d/55_tlp.sh`<br>`fs/usr/share/bash-completion/completions/kdos`<br>`fs/usr/share/kdos/xdg-live/kscreenlockerrc`<br>`fs/usr/share/kdos/xdg-live/kwalletrc`<br>`fs/etc/security/limits.d/30-kdos.conf`<br>`fs/usr/share/kdos/xdg/discoverrc`<br>`src/daemons/kdos-powerd/io.kdos.power1.policy`<br>`src/daemons/kdos-powerd/kfs.md` | fs (`make build BUILD_ARGS="--phases 10_bootstrap,70_image --steps 10_bootstrap:000_file_system.sh"`); port kdos-powerd (50_desktop, comment only); rig: `service list` lvm and tlp cells are one-line summaries; tty2 `kdos update <TAB><TAB>` offers groups and theme |
| U31 | Console palette: bright black is muted text | polish | F145 | `src/libs/libkcolor/kcolor.c`<br>`src/libs/libkcolor/kcolor.h`<br>`fs/etc/vtrgb` | selftest (the vtrgb match check; a library edit rebuilds every port of ours); fs + image; rig: tty2 htop figures readable, slot 8 ≥4.5:1 |
| U32 | Manual page summaries | polish | F146 | `src/system/kdos-doc/mkbook.py` | port kdos-doc (41_system) + image; on the ISO `apropos kdos-session-wrapper` shows $XDG_RUNTIME_DIR and `man 8 service` has a self-contained NAME line |
| U33 | Books workflow attaches the release's books | minor | F24 | `.github/workflows/books.yml` | after U34 deletes the branch: `gh workflow run books.yml --ref v0.2`; `curl -s https://api.github.com/repos/kunaldawn/kdos/releases/tags/v0.2` lists kfs.pdf and kfs-html.tar.zst |
| U34 | Repository front page: licence, Pages source, branch | polish | F151, F157, F158 | `LICENSE` | operational, the user runs it: Pages source to GitHub Actions; `git push origin --delete refs/heads/v0.2` (destructive); after a push the API reports spdx MIT, no 'pages build and deployment' run, `git ls-remote origin v0.2` lists only the tag |
| U35 | Recapture the stale screenshots | minor | F30, F152 | `docs/screenshots/boot-splash.png`<br>`docs/screenshots/tty-banner.png`<br>`docs/screenshots/konsole.png` | rig rerun after U11 and U31: vnc-shot.py on build/iso-build/kdos.iso in Bone — splash during the service scripts, tty2 login banner, maximised Konsole with the whole fastfetch block; convert PPM to PNG |
| D1 | Docs: README, getting started, installation | blocker | F03, F04, F08, F11, F14, F19, F22, F24, F25, F29, F35, F36, F42, F63, F76, F78, F79, F80, F81, F92, F97, F103, F105, F114, F153, F154, F160, F161 | `README.md`<br>`docs/kdos/02-user-guide/getting-started.md`<br>`docs/kdos/02-user-guide/installation.md` | docs-only: bash testing/docscheck.sh; python3 testing/anchorcheck.py |
| D2 | Docs: desktop, applications, theming, accessibility | blocker | F03, F06, F08, F18, F19, F20, F21, F45, F49, F52, F58, F59, F62, F64, F74, F75, F112, F116, F117, F120, F121, F129, F145, F161, F162 | `docs/kdos/02-user-guide/desktop.md`<br>`docs/kdos/02-user-guide/applications.md`<br>`docs/kdos/02-user-guide/theming.md`<br>`docs/kdos/02-user-guide/accessibility.md` | docs-only: bash testing/docscheck.sh; python3 testing/anchorcheck.py; python3 testing/booknav.py |
| D3 | Docs: administration | major | F11, F31, F33, F50, F64, F90, F147 | `docs/kdos/02-user-guide/administration.md` | docs-only: bash testing/docscheck.sh; python3 testing/anchorcheck.py |
| D4 | Docs: architecture (boot, packaging, session, security, design language) | blocker | F01, F02, F03, F10, F11, F13, F18, F19, F23, F33, F34, F35, F36, F38, F42, F52, F60, F61, F65, F66, F69, F72, F73, F84, F85, F86, F87, F93, F149 | `docs/kdos/03-architecture/boot-and-init.md`<br>`docs/kdos/03-architecture/packaging.md`<br>`docs/kdos/03-architecture/session.md`<br>`docs/kdos/03-architecture/security-model.md`<br>`docs/kdos/03-architecture/design-language.md` | docs-only: bash testing/docscheck.sh; python3 testing/anchorcheck.py |
| D5 | Docs: the kdos command, boxes, the demo | blocker | F01, F06, F09, F16, F17, F18, F19, F20, F25, F31, F32, F38, F52, F53, F54, F56, F58, F67, F70, F71, F119, F140, F144 | `docs/kdos/04-programs/kdos-command.md`<br>`docs/kdos/04-programs/kdos-appbox.md`<br>`docs/kdos/04-programs/kdos-bb.md` | docs-only: bash testing/docscheck.sh; python3 testing/anchorcheck.py |
| D6 | Docs: installer, settings pages, Discover | blocker | F01, F05, F12, F14, F23, F27, F37, F38, F39, F40, F43, F63, F68, F76, F77, F78, F79, F80, F81, F82, F91, F94, F97, F105, F108, F110, F126, F132, F137, F138, F141, F150 | `docs/kdos/04-programs/kinstall.md`<br>`docs/kdos/04-programs/kdos-settings.md`<br>`docs/kdos/04-programs/discover.md` | docs-only: bash testing/docscheck.sh; python3 testing/anchorcheck.py |
| D7 | Docs: Plasma shell, KWin, daemons | major | F05, F06, F09, F19, F20, F21, F46, F59, F82, F114, F118, F119, F132, F134, F142 | `docs/kdos/04-programs/plasma-shell.md`<br>`docs/kdos/04-programs/kwin.md`<br>`docs/kdos/04-programs/daemons.md` | docs-only: bash testing/docscheck.sh; python3 testing/anchorcheck.py |
| D8 | Docs: developer guide and reference | blocker | F01, F04, F09, F11, F16, F19, F22, F28, F29, F57, F65, F69, F82, F84, F87, F120, F146, F147, F159 | `docs/kdos/05-developer/developing.md`<br>`docs/kdos/05-developer/how-kdos-is-built.md`<br>`docs/kdos/05-developer/build-system.md`<br>`docs/kdos/06-reference/known-gaps.md`<br>`docs/kdos/06-reference/configuration.md`<br>`docs/kdos/06-reference/filesystem-and-ipc.md`<br>`docs/kdos/06-reference/command-index.md`<br>`docs/kdos/06-reference/repository-layout.md`<br>`docs/kdos/06-reference/ports-catalogue.md` | docs-only: bash testing/docscheck.sh; python3 testing/anchorcheck.py; python3 testing/bookcounts.py if it checks the catalogue |
| D9 | KDOS from Scratch: wiki text, include pins, recipe stamp (runs last) | blocker | F01, F02, F09, F11, F35, F47, F65, F69 | `docs/kfs/15-image/105-ports.md`<br>`docs/kfs/15-image/110-iso.md`<br>`docs/kfs/16-wiki/updating.md`<br>`docs/kfs/16-wiki/signing-keys-and-trust.md`<br>`docs/kfs/16-wiki/ab-slots-and-rollback.md`<br>`docs/kfs/16-wiki/screen-lock.md`<br>`docs/kfs/16-wiki/networkmanager.md`<br>`docs/kfs/16-wiki/shortcuts-and-window-rules.md`<br>`docs/kfs/16-wiki/boot-menu-and-kernel-parameters.md`<br>`docs/kfs/16-wiki/filesystems-and-fstab.md`<br>`docs/kfs/data/recipes.stamp` | after every other unit: python3 testing/kfscheck.py --repin (105_ports.sh, 110_iso.sh, kconf_updaterc, networkmanager build.sh moved); make book-stamp; python3 testing/kfscheck.py; bash testing/docscheck.sh --book docs/kfs; python3 testing/anchorcheck.py |

Order and dependencies:

- Units change disjoint files and can be worked in parallel; a finding that spans units lists each.
  Docs units carry the page text their findings name and land in the same round as the code
  (hard rule 1).
- U01, U02, U03 and U08 together end F01 for the next release. v0.2 machines need the `binhost =`
  line in `/etc/kdos/update.conf` until then; the v0.2 release notes (U03) and known-gaps (D8) say so.
- U15 edits libkbase and U31 edits libkcolor: each rebuilds every port of ours. Land them before the
  full rebuild.
- U33 runs after U34's branch deletion. U35 is photographed after U11 and U31 are built.
- D9 runs last: `kfscheck --repin` and `make book-stamp` after every recipe, kfs.md, phase list and
  included file is final.

---

## Uncertain

Each needs the rerun named before it is fixed or dropped.

1. **variants-03 · After the LUKS passphrase, console text is drawn over the splash.** Unclear
   whether plain boots are clean (then LUKS is a specific defect and known-gaps.md:844 is stale) or
   this is F84's documented gap. Evidence: `shots/variants/r5-08-right-2s.png`,
   `r13-02-bios-unlocked.png`, `r13-03-bios-2s.png`, `r8-boot-montage.png`. Rerun: with
   `logs/variants/drive.py`, boot `games.qcow2` (plain) and shoot every 1 s from 6 s to the greeter;
   make a fresh `INSTALL_LUKS=1` disk, type the passphrase, shoot every 1 s to the greeter; count
   pixels with r+g+b>200 at x<400 per frame; `dmesg | grep -i fbcon` on both. U11's KD_GRAPHICS
   change covers either outcome.
2. **variants-05 · An installed machine's `limine.conf` became 0 bytes after hard power-offs.**
   Product (fsck -a of a dirty ESP) or rig (OVMF `-bios` NvVars writes)? Evidence:
   `shots/variants/r18-00-menu.png`, `r18-montage.png`, `logs/variants/run19.log`, `run20.log`.
   Rerun: a fresh `INSTALL_LUKS=1` disk booted with pflash OVMF (CODE read-only plus a writable VARS
   copy), 8 cycles alternating UEFI and SeaBIOS, each ended by a hard kill, then from the live ISO
   `wc -c /limine.conf; fsck.fat -n /dev/vda1`; repeat with `-bios OVMF.fd`. Corruption under pflash
   too means a product defect: mount `/boot/efi` read-only in kinstall's fstab and have
   kdos-bootctl remount it rw only around its writes.
3. **updates-10 · The rollback notice is not in the notification history.** Likely F20's mechanism
   (kdos-notice posts as "KDOS" with no desktop entry, filed under `@other`, ShowInHistory=false),
   or the Details click removed it. Evidence: `shots/updates/53-slota-after-rollback-6s.png`,
   `54-slota-details-sysversions.png`, `55-slota-notif-history.png`. Rerun after U15: an overlay
   `rb.qcow2` of `logs/updates/disk.qcow2`, boot to greeter, `rm -f
   /home/kdos/.local/state/kdos/notice`, log in, wait 60 s without clicking, open the Notifications
   applet and shoot it; then `systemsettings kcm_kdos_system` to see whether Details switches an
   open System Settings.
4. **updates-15 · After a manual rollback, the full Welcome tour opens again.** Likely session
   restore relaunching plasma-welcome without `--pages`. Evidence:
   `shots/updates/71-rollback-session.png`, `61-slotb-second-boot.png`. Rerun: on `rb.qcow2` log in,
   open `plasma-welcome --pages 90-WhatsNew.qml` from KRunner, log out with it open, log in, shoot,
   then `grep -n -A3 plasma-welcome /home/kdos/.config/ksmserverrc; ps -o args= -C plasma-welcome`.
   If confirmed: keep plasma-welcome out of session restore (`excludeApps` in a shipped ksmserverrc
   under `/usr/share/kdos/xdg`) and say so in session.md.
5. **kdosdesk-09 · One session had only the input-method icon in the tray.** Candidates: layout.js
   `addTray` (`:86-96`) racing `plasmoidregistry.cpp:126-133` on `extraItems`/`knownItems`, or F09's
   kconf_update truncation. Evidence: `shots/kdosdesk/sheet-tray-time.png`, `c-02-panel-right.png`,
   `14-runraise-mc.view.png`. Rerun after U10: 12 fresh boots with a root script printing the tray
   containment's `extraItems`, `knownItems`, `hiddenItems` from the appletsrc and live through
   `evaluateScript`; a keyboard-only tray with empty `extraItems` and full `knownItems` confirms the
   race.
6. **packages-18 · VLC's privacy dialog title is clipped ("…Polic").** KDOS's qt5ct font or upstream
   VLC 3 under Fusion? Evidence: `shots/packages/08-vlc.crop-90-0-445-70-x4.png`, `08-vlc.view.png`.
   Rerun: `--cmd 'vlc'` and `--cmd 'QT_QPA_PLATFORMTHEME= vlc'`, crop 90,0,445,70 at 4×. U21's vlcrc
   removes the dialog for new accounts, so this only decides whether qt5ct clips other Qt 5 group
   boxes.
7. **security-09 · One boot with the stock panel.** Folded into F09; F09's 20-boot proof decides it.
8. **Lead found while verifying updates-02 · Slot B's `/etc/inittab` carries the live medium's
   `S0 … --autologin root` line.** B's inittab equals `inittab.kdos-new`, A's has no such line, and
   the carry code as read should keep A's copy. Evidence: `logs/updates/run2c.log`. Rerun: apply the
   stick image, `diff /etc/inittab /mnt/b/etc/inittab; grep -n autologin /mnt/b/etc/inittab*`, boot B
   and see whether ttyS0 asks for a login. A root autologin on the serial line after an update is a
   major security defect in `carry_etc` (U02).

---

## Not tested

Deduplicated from every lane.

- **Real hardware:** suspend/resume, native GOP modes, Secure Boot on (refusal and the installer
  warning), a physical COM port (whether the 9600-baud console slows bare metal), USB sticks other
  than QEMU's xHCI, optical drives and burning a DVD, Bluetooth and HDMI audio, battery, multiple
  monitors, a braille display, audible output on a speaker, a microphone with a real signal, Wi-Fi.
- **GPU rendering:** Phosphor mode, the CRT pass, kdos-kwin-motion, Overview on a GPU, the boot
  splash's Phosphor motion. The host GPU runtime is broken (`/run/nvidia-persistenced/socket`
  missing; `egl-headless` unavailable); only llvmpipe refusals were checked.
- **Boot entries:** installed KDOS (verbose), (single user), (slot b) and memtest; the firmware's own
  KDOS entry (OVMF NVRAM not kept); the tty1 passphrase prompt of the verbose and single-user
  entries; BIOS boot of a GUI-installed disk.
- **Updates:** a real network `kdos update apply` (nothing is newer than v0.2), the delta, binhost
  and `--source` lanes of apply, a LUKS A/B update and the BIOS menu-led trial, one-slot `apply
  --in-place`, a genuinely broken update rolling back, security-update marking in the tray, the 24 h
  notification interval, a stick set up only through System Versions.
- **Installer:** the permission-prompt path, a reuse-plan install and Partition Manager creating
  partitions, swap partition, xfs and f2fs, an offline install, LUKS autologin from
  install-to-disk.sh, the terminal wizard's Disk page for a refused disk, a successful native group
  at install (blocked by F01), installed defaults with root locked (install-to-disk.sh unlocks root),
  `kdos firmware sync` on an installed system.
- **Packages and boxes:** `--source` builds, kdenlive and the games group beyond a dry run, a real
  catalogue app on a larger VM or installed disk, a Debian-lane box from Discover, the run7
  `kdos app install` failure, persistence-store boots and the clean-session entry with a store,
  `kdos persist create`, `kdos clone`, `kdos image make/verify/unpack`, `kdos rebuild`, `kdos backup
  run`, `kdos speech get/transcribe/say`, `kdos share`, `kdos march run`, `kdos-fetch-static`,
  distrobox exports, sshd and ssh into the machine, podman as root, a boxed GUI app launched through
  its launcher (tagged socket, KWin badge, filtered bus), a pack signed with `kdos-packs.pub`, kpkg
  binhost from a private directory, an unsigned kernel module.
- **Security:** a user outside wheel in an active graphical session, kdos-fwask and the knock
  counters (needs another host), Discover's install flow under the binhost failure.
- **Desktop and accessibility:** the hot corner (the tablet cannot pass the edge), volume and
  brightness OSD transients, the software-updates tray item, a lit privacy lamp's click-to-mute and
  tooltip, the Dock desk's floating dock, session restore, idle dimming and locking over a long soak,
  Orca on the login screen, CJK input, Dasher/KMouth/Kontrast, dwell click, bounce keys,
  colour-blindness correction, magnify-region, shake cursor, mouse-keys pointer movement (the cursor
  plane is not in the VNC frame), KDE Connect pairing, Kup running a backup, editing in Gwenview,
  Okular annotations, Kate LSP, Ark creating archives, Library sources Zeal/kiwix/Kolibri/llama, the
  transcribe action, the Make It Yours install queue, the Welcome Privacy Wallet button, kdos-addons
  pinpip/activescreen once enabled.
- **Settings:** Phosphor controls on a GPU, Boxes per-box controls (no box on the live medium),
  Health's Copy to Clipboard (no wl-paste), a time-zone change on the map, applying SDDM, Global
  Theme, splash, cursor, icon and font changes, Wi-Fi and Bluetooth pairing, Users delete, layout
  persistence across re-login.
- **Release:** downloading the full 1.6 GB ISO from GitHub, Rufus/Etcher and checksums on Windows or
  macOS, the README QEMU command on a non-Debian host, `gh` (not installed), hashing every binhost
  asset (digests and 27 range requests only), the 40 GB and 256 GB slot examples on real disks.
- **Timing:** boot and app start times on an idle host (other lanes' VMs ran at the same time).

---

## What passed, per lane

- **installed:** unattended plain install in 105 s with the documented layout; boot menu, splash,
  greeter, wrong and empty passwords; login in 8-9 s; first-login Welcome; lock/unlock, VT switch, no
  tty leak, logout two ways, Switch User; polkit dialog from KRunner; restart and shutdown clean with
  accent and wallpaper kept; `kdos update check --fetch` against the real release; groups, sudo,
  nft, NetworkManager, chrony, hostname, services, user services; idle 1.1-1.3 GB; no crashes.
- **variants:** size refusal consistent across probe, plan, events and GUI with nothing written; one
  slot on 20 GiB and 5700 MiB (swap=none) on UEFI and SeaBIOS; LUKS install, prompt, retry,
  three-strike shell, SeaBIOS unlock; plain boot splash clean in its accent; live ISO under SeaBIOS
  from USB and an install from it; with a binhost named, kde-games installs and runs.
- **installer:** three launch paths, single instance, Welcome probe, time-zone and keyboard search,
  4G refusal, Look Again, Partition Manager, every validation message, back navigation, Alt+Right and
  Cancel safety, pkexec without a prompt on the live medium, full, LUKS2+btrfs and OEM installs,
  close-during-install guard, Restart Now, installed systems match the answers, 1366x768 fits,
  language groups suggested.
- **updates:** Discover finds the stick's signed 0.2.1, polkit, progress, restart prompt, BootNext
  trial menu, automatic rollback and its banner, Start This Version Next Time, B confirmed on its
  second boot, rollback B→A, Keep slot A, Cancel mid-acquire, the default channel against v0.2 says
  up to date, the CheckNew throttle.
- **packages:** refusals for unknown, installed and offline; unprivileged dry runs; with a named
  binhost every signature verified and kcharselect, gimp, kde-games, libreoffice (fresh), filelight
  from Discover all install and launch with icons in Kickoff; boxed Zathura install, launch, removal
  and the out-of-room refusal.
- **boot:** UEFI and BIOS from CD and USB reach Plasma in ~32-39 s; the menu, countdown stop,
  wrap-around and memtest; verbose and clean-session entries; splash stages and artwork at four
  sizes; KSplash continuity; Welcome's 18 pages; panel adapts to width; tty2; no missing firmware,
  `kpkg check --all` clean, no broken links, every Exec resolves, 6,782 ELF files resolve, no flapping
  services, doctor clean but the documented warning.
- **desktop:** Kickoff, tray popups, clock, notify popups, Klipper, KRunner verbs and calculators,
  desktop menu and edit mode, window placement and snapping, decoration, Overview and the grid,
  Krohnkite, key card, logout screen, lock, Night Light, Meta+P, Spectacle, run chords, `kdos desk`
  for all three desks, Phosphor refusal, meters, Show Desktop, volume key.
- **apps:** all 34 visible entries resolve and open, with real file opens in Kate, Okular, Gwenview,
  Ark; Spectacle shots and recording; Elisa plays; Firefox, its portal file dialog and the KDOS book;
  System Monitor, Info Center, System Settings, Discover from Kickoff, Partition Manager making a GPT
  and btrfs, printing tools, Welcome and the installer; no crash in 17 boots.
- **settings:** About shows KDOS 0.2 at the tag; landing page and sidebar; KDOS Look repaints and
  persists and offers Repaint Everything; Firewall Apply through polkit, Reset, Defaults; Health
  filter; System Versions on the live medium; Colors page carried down by kded-kdos; scale with
  Keep/Revert; add a user; add and switch a layout; automatic time off; Sound; UEFI toggle; the KDOS
  global themes, splashes, icons and fonts.
- **kdosdesk:** Welcome's KDOS pages and buttons, key card, palette and library runner rows,
  kdos-notice pending notice, kded-kdos accent sync, stick offer and plasmashell respawn, checklist
  and privacy applets, library:/ shelves, window memory, run-or-raise for lynx, the three desks,
  KSplash, four accents, wallpapers, sounds, kdos-bb with audio, KDOS add-ons, Discover's KDOS
  backend, logout and autologin back.
- **cli:** `kdos help` and the sheet, version, status, doctor, why, explain, oracle, restarts, cve,
  sandbox (Landlock), timezone without a password and hostname through polkit, march, persist,
  clone listing, checklist, trash, places, speech, notify, remind, toggle, privacy, verify, shot,
  settings, app subcommands, update check/groups/fetch, bootctl on the live medium, kdos-doc, service
  as root, kdos-box lifecycle, podman, https/git/ssh, sudo, man pages, kpkg, fastfetch, tty2.
- **media:** PipeWire, WirePlumber and pipewire-pulse; output and input devices; volume key and
  OSD; playback, notification sounds and speech reach the sink; privacy lamp and mute for a capture;
  thumbnails; Gwenview, Okular, Elisa, VLC by hand; sticky and slow keys, zoom, invert, mouse-keys
  click, reduced motion, on-screen keyboard, fcitx5 Greek; keyboard-only Kickoff, System Settings
  and the installer's Welcome page; 200% at 2560x1440 fits; high-contrast accents.
- **security:** setuid set exactly as documented, shadow and sudoers modes, locked system accounts,
  sysctl and kernel config, mounts, nftables, SSH off by default and its round trip, root SSH
  refused, listeners, polkit matrix and agent dialogs, the narrow live rule, every privileged bus and
  socket method refused for a non-admin, closed firewall names, pack, tree, index and package
  tampering refused, box boundaries, Landlock sandbox, no shipped private key.
- **release:** releases/latest is v0.2; pre-releases below it; local ISO = SHA256SUMS = GitHub digest;
  `sha256sum -c` OK; all three signatures verify independently and with the shipped tools; key ids;
  every index entry and source digest resolves; hybrid ISO structure; unsigned loader matches the
  docs; boot menu matches the table; README facts match the ISO; every relative link and image
  resolves; kdos.dev serves both books.
- **docs:** the Limine menu, inittab, live account, kwinrc, power and lock defaults, shortcut tables,
  fcitx5 keys, key card groups, the kdos verbs and options, every `kdos settings` word, kinstall pages
  and answer keys, the GUI page list, the catalogue counts, Discover's backends, release checksums,
  links and anchors all match the image; every program missing from the ISO is in the binhost.

---

## Refuted

- installed-01 · elogind wedges after suspend/resume — rig: virtio_gpu has no freeze/restore, so the display blocks VT ioctls; same cause as installed-02.
- installed-02 · no screen after resume — documented gap (known-gaps.md:1472) and the rig's virtio-vga; optional: known-gaps and testing.md can name virtio_gpu's missing PM callbacks.
- installed-10 · shutdown shows raw rcK text — documented (boot-and-init.md:1499-1525); no shutdown splash is promised.
- installed-15 · Start penguin stays green in other accents — documented: the marks are raster (design-language.md:288).
- installed-16 · disabled Hibernate on the greeter — upstream Breeze `enabled: sddm.canHibernate`.
- installed-17 · Welcome "Simple by Default" shows a generic desktop — upstream page, generic by intent.
- installed-18 · black gap before the greeter, blank VT1 — documented handoff; llvmpipe start time; tty1 belongs to the display manager.
- variants-09 · 300 MB free on a 5700 MiB disk — the documented minimum; F14 covers the real fault in that band.
- installer-12 · no minimum password length — documented as advice, not a refusal.
- installer-23 · OEM wizard is upstream plasma-setup — documented as plasma-setup.
- installer-24 · Welcome allows Next with no usable disk — same as the terminal wizard; the Disk page refuses.
- installer-26 · confirm dialog does not dim — it dims ~30%; standard PromptDialog.
- updates-09 · About says 0.2 on slot B — mock kit artefact plus F02.
- updates-11 · no "updates available" notification — not promised; the tray icon is the documented signal.
- updates-13 · Update Issue over "Up to date" — rig: Discover started outside seat0.
- packages-05 · no way to remove a native port — documented: ports leave with the system version (applications.md:652); a known-gaps line on live memory is optional.
- packages-14 · placeholder icons in Discover and Kickoff — documented category fallback (discover.md:160-162); the real icon loss is F12/F94.
- boot-02 · black gap and KSplash at 83% — no unbroken picture promised; the mark stays in place; llvmpipe timing.
- boot-04 · `service list` shows skipped services as [DOWN] — documented meaning of [DOWN].
- boot-05 · splash marks skipped services OK — documented two verdicts; its doc note is in F33.
- boot-07 · faded logo behind Install KDOS — upstream plasma-welcome Live.qml backdrop.
- boot-10 · memtest86+ footer "8.10.unkno.x64" — upstream for every tarball build.
- desktop-06 · two Input Method tray entries — upstream fcitx5 SNI and Plasma's applet.
- apps-04 · Editor's Choice fallback icons — they are category icons by design.
- apps-07 · System Monitor Applications page empty — documented gap (known-gaps.md:94-103).
- apps-11 · Breeze's Desktop folder icon — inherited upstream art, as documented.
- apps-12 · Konsole opens with the banner scrolled — documented plain banner when the terminal is short.
- settings-04 · Default Applications shows "Other…" for video — documented fall-through (applications.md:634); F08 is the real fault.
- settings-09 · Time Zone tab banner for UTC — upstream behaviour for a zone off the map.
- settings-10 · Firewall status reads "Open" before Apply — the KCM pending-value convention.
- settings-13 · desktop icon label on Paper — upstream white label with shadow, readable.
- settings-14 · window under the panel after a scale change — upstream KWin geometry.
- kdosdesk-03 · Discover "Update Issue" on the live medium — rig: `su` session outside seat0.
- kdosdesk-08 · enabled Install button for a non-set .ktar — the test file was random bytes; nothing installs.
- kdosdesk-22 · tour advertises Activities — activities ship through kcm_activities; follow-up: desktop.md:275 and kglobalshortcutsrc:32-33 overstate "no activities interface".
- cli-08 · `kdos settings --help` opens System Settings — documented word fall-through; the `--help` case is in F54.
- cli-16 · fastfetch Host line wraps — QEMU's long DMI string.
- cli-19 · the oracle shows build-system reasons — documented corpus.
- cli-20 · GUI helpers block the terminal — documented exec behaviour; upstream warnings.
- cli-21 · `kdos app install` refused on a 4 GB VM — documented room check; rig memory size (the `info` size is F45).
- media-11 · desktop label on Paper — upstream white label with shadow.
- release-17 · planning files in the repository root — maintainer's choice; nothing promised.
- docs-17 · scanning listed but no scanner window — sane-backends and scanimage ship; no window promised.

## Re-test and second fix round

The ISO rebuilt after the first fix round was re-tested lane by lane. Each fix unit, the uncertain items and two visual smoke paths were checked, and a skeptic pass read every verdict against its shot or log: 179 fixed, 32 partial, 11 still, 11 untestable, and 80 new issues. The 32 partial or still items and the 80 new ones are keyed R001-R112. The second fix round below took them in 18 units; each unit was fixed, reviewed adversarially and refixed where the review raised an issue. Every unit is in the tree and not yet rebuilt into an ISO, so each status is "fixed in tree" until the next re-test.

| Unit | Severity | What | Items | Rebuild |
|---|---|---|---|---|
| W01 | major | kdos-shot qr: the decoded text goes to wl-copy on stdin, never in argv | R069 | kdos-tools |
| W02 | major | sysupdated fixture: drive.sh expects the 17 ListSlots keys bus.c sends | R014 |  |
| W03 | minor | kdos update and the release pipeline: notes, the N: line, cancel and mount, the hints, tags and the publish notes | R001, R009, R109, R091, R002, R004, R005, R006, R008, R010, R011, R012, R018, R070, R071, R072 | kdos-tools, kdos-kpkg, kdos-welcome |
| W04 | minor | Discover: release notes and licence on the update page, restart state after Keep, one job entry, plan text, catalogue listing, search and icons | R013, R015, R025, R022, R023, R024, R026, R016, R017, R027, R028, R029, R030, R031, R098, R032, R066 | discover, kdos-discover, kdos-sysupdated |
| W05 | minor | Installer: root-only VG clashes and whole-disk PVs, copy line units and time left, meta-group counts, Disk/Account/Keyboard/Look/Done page polish | R033, R039, R034, R035, R037, R103, R041, R095, R101, R104, R110, R036, R040, R042, R112, R081, R043, R044, R038, R108, R111 | kdos-installer, kdos-installer-gui |
| W06 | minor | Offline documentation: Help buttons open local pages with their anchor; man NAME lines are not cut | R080, R094, R083, R088 | qt6-qtbase, kdos-doc |
| W07 | minor | Boot path: the initramfs udevd does not survive switch_root, rescue prints its reason once, the splash tagline is readable on every accent | R052, R051, R050 | kdos-splash |
| W08 | minor | System Versions: an incomplete slot, the Start prompt width, one-slot help text, a refused channel | R007, R019, R020, R021 | kcm-kdos-system |
| W09 | minor | Services and session environment: nftables status for non-root, KDE_SESSION_VERSION in login shells, Welcome not relaunched at login | R053, R074, R089 |  |
| W10 | minor | kdos-appbox: a second remove says not installed, removing a box removes its exports, no dangling distrobox lines | R058, R061, R060, R059 | kdos-appbox |
| W11 | minor | kdos-fetch-app: export by the package's files, refuse images it cannot drive, apt-get, no dangling distrobox line | R062, R064, R065, R063, R067 | kdos-tools |
| W12 | minor | KRunner and run-or-raise: ask advice readable, bare 'accent' shows next/previous, settings titles beat the library, consistent labels | R056, R097, R057, R100, R077 | kdos-runner, kwin-script-kdos-runraise |
| W13 | minor | KDOS notifications: one sender name, reminders that wait the time asked | R054, R055 | kdos-tools (libkbase changed: every port of ours), kdos-notice, kdos-runner, kdos-appbox (if the util.c edit is made) |
| W14 | minor | Welcome: commands that run as shown, the chord the tour teaches | R075, R099, R076 | kdos-welcome |
| W15 | minor | Boxes settings page: Defaults disabled with no box chosen, through the KCModuleQml wrapper | R078, R079, R096 | kcm-kdos-boxes |
| W16 | minor | KDOS look kit: a visible switch on dark accents, a tinted greeter picture, an accurate SDDM note | R046, R047, R048 | kdos-theme, kdos-plasma-look, kdos-sddm-theme |
| W17 | polish | kpkg takes --root before the command | R068 | kpkg (front.c is built by 10_bootstrap; needs a 10_bootstrap rebuild and an image rebuild) |
| W18 | polish | Upstream port polish: ISO Image Writer status, the disabled calendar's Sundays, Elisa's footer, LibreWolf policies | R073, R082, R084, R086 | isoimagewriter, kwidgetsaddons, elisa, librewolf |

### Left open by the second round

- **R003**: The text of ports/RELEASE (what changed in this version) is release-notes content the user writes for a release.
- **R045**: F86 lock-screen half. Dimming the lock-screen wallpaper is a product decision the task names explicitly (kscreenlockerrc still names the undimmed wallpaper).
- **R102**: Same as R045, the lock-screen contrast seen from the installed lane. Dimming the lock screen is a product decision.
- **R049**: Seen once, after a scale change as well as a desk switch, with the cause unconfirmed (krohnkite or the desk switch). There is no fix site yet. Reproduce it first.
- **R085**: The kfs.md pages can name a merge request only after the user opens the upstream MRs, which is an outward action.
- **R087**: The grey penguin body is the known effect of vtrgb slot 8 following kcol_muted, and genlogo.py's comments match it. The evidence itself says whoever owns the logo must decide whether to keep it.
- **R090**: No fix line has been found. The clip happens in VLC 3's own privacy dialog under the KDE font, in both the qt5ct and empty-platform-theme runs. The shipped /etc/skel vlcrc (qt-privacy-ask=0) keeps the dialog from opening for every new account.
- **R092**: A read-only ESP that every writer remounts is a design decision. kdos-bootctl, the update image lane, kinstall and fwupd's capsule updates all write the ESP. The dirty bit has no observed effect: no file changed, and the mount succeeds.
- **R093**: F08 partial records only an untested step (no video opened from Dolphin). The measured defaults are correct, so there is nothing to fix.
- **R105**: F09 partial: both observed logins were correct. The item asks for the 20-boot proof, which is a rig verification task, not a fix.
- **R106**: QML RadioButton and CheckBox showing focus as an underline is Breeze's qqc2 style, and the U26 lane confirmed the patch draws it (R081). The disk-row half is in W05 (R042/R112).
- **R107**: distributor-logo-kdos is one fixed-colour brand mark (os-release LOGO, About page), installed once into hicolor and not per accent. Whether the brand mark follows the accent is a product decision.

### Waiting on an outward step

- Tag a clean commit (v0.2.1) and build from it, so `/ports/TREE` names its release (F01) and the binhost index carries `N:` lines (F18, R004).
- Edit the v0.2 release body and rebuild or withdraw the `image-v0.2` asset (F10, F22, F28, F29, F155, F156).
- Publish a `debian-dep11` source for the Debian catalogue's icons (F94).
- Offer the KDE, Qt and Elisa patches upstream and name each merge request in its port's `kfs.md` (R085): discover notifyrc-update-restart, sysupdate-licenses and notifier-reboot-clears; qtbase openurl-local-file; isoimagewriter status-wrap; kwidgetsaddons rest-day-contrast; elisa empty-playlist-footer.
- Decide whether to dim the lock-screen picture (F86, R045, R102) and whether more programs go on the base image (F21).

### Re-test of the second round

The ISO rebuilt with every round-2 unit was re-tested on a fresh install, one lane per unit and a visual smoke pass of the live medium and of an installed machine, each verdict checked by a skeptic against its shots and logs: of 130 verdicts, 112 fixed, 9 partial, 4 still and 5 untestable (a one-slot install, a rollback between two versions, a full GUI install and a republished binhost catalogue were out of reach), and 39 new issues. Those 57 items, keyed S001-S057, are the third fix round's input.

| Key | Lane | Item | Status or severity |
|---|---|---|---|
| S001 | W01 | kdos-shot qr: Spectacle stays in its region overlay after the drag until the selection is accepted, and nothing says to accept it | polish |
| S002 | W03 | R004 | partial |
| S003 | W03 | What's New and `kdos update notes` say nothing about a rollback when the failed slot's tree commit equals the running one | polish |
| S004 | W03 | A cancelled image apply says 'stopped between packages' while it was filling a slot from an image | polish |
| S005 | W03 | Live medium What's New points to System Versions 'shows the versions on this computer', but the live medium has no slots | polish |
| S006 | W03 | testing/selftest.sh stops at 'kdos rebuild refuses what it cannot finish' on a host with less than 25 GB free, and creates .selftest-rebuild-work in the repo root | polish |
| S007 | W04 | R023 | still |
| S008 | W04 | R029 | partial |
| S009 | W04 | R031 | partial |
| S010 | W04 | The 'Restart is required' notification cannot post: DiscoverNotifier sends it as a DefaultEvent, so it is looked up in plasma_workspace.notifyrc | minor |
| S011 | W04 | Updates page keeps a 'Updates could not be fetched' warning next to a valid image offer when the published tree is only older than this machine's | polish |
| S012 | W04 | LibreOffice's Discover page shows LibreOffice Base's description for the whole suite | minor |
| S013 | W04 | KRunner's Software Center still offers G Alternatives (galternatives), a Debian alternatives-system configurator | polish |
| S014 | W05 | R033 | partial |
| S015 | W05 | R035 | partial |
| S016 | W05 | R036 | partial |
| S017 | W05 | Copy time-left now overestimates 2-3x through most of the copy and climbs while progress advances | minor |
| S018 | W05 | Disk page swap check (unprivileged plan) allows 1 MiB more swap than the Summary's root plan accepts | minor |
| S019 | W05 | Tabbing to the root 'Again' field does not scroll it into view | polish |
| S020 | W05 | testing/selftest.sh stops at 'kdos rebuild refuses what it cannot finish' before the kinstall copy-line and whole-disk PV checks | minor |
| S021 | W06 | Firefox restores the previous Help tab after it is killed (session restore), so the next link opens beside a stale 'KDOS settings pages' tab | polish |
| S022 | W07 | Book data whatis.tsv still gives the old kdos-splash preview synopsis without [ACCENT] | polish |
| S023 | W07 | Splash failure detail lines are cut mid-word with no ellipsis | polish |
| S024 | W08 | R021 | partial |
| S025 | W08 | The one-slot Updates help may promise an update path the GUI does not offer | polish |
| S026 | W08 | An incomplete slot repeats the same fact in its State and reason lines | polish |
| S027 | W10 | `kdos-box export` exports a name that is not a command in the box, including an option such as --help | minor |
| S028 | W10 | An exported box launcher has a blank generic-file icon and no comment in Kickoff | polish |
| S029 | W10 | A second `kdos-box remove` deletes the profile that the first remove said was kept | polish |
| S030 | W11 | R065 | partial |
| S031 | W11 | A running rootless podman container (any distrobox) cannot be stopped or force-removed on KDOS; it stays 'Stopping' until reboot | major |
| S032 | W11 | Exported box launchers can come up with no icon (blank-file placeholder) in Kickoff, e.g. Debian foot | minor |
| S033 | W11 | kdos-fetch-app on Debian prints 'dpkg-preconfigure: unable to re-open stdin: No such file or directory' | polish |
| S034 | W13 | Once KDOS notifications share one 'KDOS Tools' group, Plasma's history hides a middle entry with no 'Show more' expander | minor |
| S035 | W13 | `kdos remind ls` lists the time a reminder was set, not when it fires | polish |
| S036 | W16 | Lock-screen user name is low contrast on phosphor: about 2.5:1 over the undimmed blurred belly (already tracked as R045/R102) | minor |
| S037 | W17 | kpkg remove into a nonexistent --root says 'run as root' while running as root | polish |
| S038 | W17 | kpkg list on a nonexistent --root prints nothing and exits 0 | polish |
| S039 | W17 | Host selftest stops at 'kdos rebuild refuses what it cannot finish' (not W17 code) | minor |
| S040 | W18 | Local binhost and mock release stick still carry the pre-fix librewolf package with TranslateEnabled/GenerativeAI | minor |
| S041 | W18 | LibreWolf prints a lockstore error on stderr at every start | polish |
| S042 | smoke-live | R098 / F137: placeholder icons in Discover Home Editor's Choice | untestable |
| S043 | smoke-live | Discover on the live medium raises 'Update Issue' at launch and on Refresh: 'Failed to check for a new version of "kdos". Error message: Interactive authentication required' | minor |
| S044 | smoke-live | Bare 'accent' in KRunner now shows next/previous but drops Perfect and Paper | polish |
| S045 | smoke-live | The Welcome page shows the green Phosphor KDOS logo under the Bone accent on the live medium too | polish |
| S046 | smoke-live | KRunner 'KDOS Look' lists the same title three times, plus 'The KDOS Book' | polish |
| S047 | smoke-installed | R020 One-slot install: Updates help promises a rollback | untestable |
| S048 | smoke-installed | R091 After a rollback, What's New says 'updated from <failed newer version>' | untestable |
| S049 | smoke-installed | R102 Lock-screen name contrast over the penguin belly (F86 lock half) | still |
| S050 | smoke-installed | R105 F09 desk intermittently not the KDOS desk | partial |
| S051 | smoke-installed | R107 Welcome Center page 1 shows the green Phosphor logo under a non-green accent | still |
| S052 | smoke-installed | R106 F15 QML radio/checkbox focus shows only as an underline | still |
| S053 | smoke-installed | R103 F91 kinstall copy line uses short units | untestable |
| S054 | smoke-installed | R108 Installer Done page: two warnings run together, lower case | untestable |
| S055 | smoke-installed | System Versions: an incomplete slot says the same thing twice | polish |
| S056 | smoke-installed | Start This Version prompt has a near-invisible frame and no dimmed backdrop | polish |
| S057 | smoke-installed | Installer Account page: password strength hint wraps at about half the field width | polish |

## Third fix round

The 57 open items of the second re-test, as eleven units, each fixed, reviewed and refixed.

| Unit | Severity | What | Items |
|---|---|---|---|
| X01 | major | A rootless `--pid host` box stops: crun's `kill-all-without-cgroup.patch` signals the init when the container has no cgroup; `kdos-box export` refuses an option or a command the box lacks, gives a bare command a terminal icon and a comment; a second `remove` keeps a kept profile, `--purge` deletes it (the KCM, Discover and an uninstall pass it for a box with no container) | S027–S032 |
| X02 | minor | Discover's restart notification drops `DefaultEvent`; a suite face describes every application in it; a machine ahead of the release is no warning; galternatives excluded; CheckNew from a caller polkit refuses is answered from the update document | S007, S008, S010–S013, S043 |
| X03 | minor | Copy time-left from two fitted costs; the Disk page's swap check matches the Summary's; the unprivileged plan sees an inactive VG | S014–S018, S053, S054 |
| X04 | minor | Update notes and cancel text, live What's New, the rebuild selftest's free-space skip and work directory, remind due time, fetch-app apt noise, `kdos-shot qr` wording | S001, S003–S006, S020, S033, S035, S039, S048 |
| X05 | minor | The notification history's group expander counts the hidden entries | S034 |
| X06 | polish | System Versions: one-slot help, an incomplete slot named once, the Start This Version prompt framed | S025, S026, S047, S055, S056 |
| X07 | polish | A KDOS page's whole title gives one *Open* row | S046 |
| X08 | polish | `kpkg --root` on a missing or read-only root says why | S037, S038 |
| X09 | polish | Splash failure lines end at a word with a marker | S023 |
| X10 | polish | Tab scrolls the focused control into view; the strength hint is the field's width | S019, S057 |
| X11 | polish | The book's whatis data carries the current `kdos-splash` synopsis | S022 |

Skipped: S002, S009, S040, S042 wait on a republished binhost; S036, S045, S049, S051, S052 are
product decisions (dimming the lock screen, an accent-following logo, Breeze's underline focus);
S021 is a rig artefact; S041 is LibreWolf's own stderr; S044 is KRunner's eight-row limit, as
documented; S050 is a verification with no defect.

Unproven until the rig re-test: S031/S030 (the crun patch), S032 (foot's icon, not diagnosed), S043
(the live session's polkit subject, not measured), S034 (no QML harness for the history applet).

### Re-test of the third round

A fresh two-slot base install, a one-slot graphical install and both smoke paths, with a skeptic
over every verdict: 81 verdicts, 65 fixed, 6 partial, 6 still, 4 untestable; 28 new, none above
minor. Fixed on the rig: a running distrobox stops (`podman stop` returns at once, `distrobox rm
--force` succeeds), `kdos-box export` refusals and icons, the kept profile, foot's icon. Still, by
decision: S044, S045, S049, S051, S052. Untestable without a release: S003, S042, S048. Open for
the fourth round:

| Item | From | What |
|---|---|---|
| T001 | X01 | A stopped distrobox cannot be restarted for up to 15 s: `podman start` / `distrobox enter` fail with an overlay 'device or resource busy' error while conmon still holds the container's orphaned `sleep 15` |
| T002 | X01 | `kdos-box run` wrapper process outlives `kdos-box stop` of its box |
| T003 | X02 | LibreOffice Discover page repeats the suite summary: same sentence as heading and as the first body paragraph |
| T004 | X02 | 'Restart is required' is sent to org.freedesktop.Notifications twice per arm |
| T005 | X02 | Discover Updates page says 'Up to date' while slot B is armed and a restart is required |
| T006 | X03 | S017 |
| T007 | X03 | S015 |
| T008 | X03 | Copy time-left counts rsync's hard-link names as bytes still to copy, so it reads 1.4-1.7x high and the line ends at 84% |
| T009 | X03 | The live medium activates an existing 'kdos' volume group at boot, which kinstall.md says it does not |
| T010 | X04 | A stop sent to `kdos update apply` after the image fill finishes is ignored silently: the slot is still armed and the run exits 0 |
| T011 | X04 | Two concurrent testing/selftest.sh runs from one checkout fail in the library suite's 'recent:' checks |
| T012 | X04 | `kdos remind ls` lists reminders in directory order, not by due time |
| T013 | X05 | The 'Night Light off' notification body says 'warm the screen (KWin's Night Light)' |
| T014 | X05 | The collapsed group hides a middle entry rather than the oldest |
| T015 | X06 | Start This Version prompt dims only the KCM page; the System Settings sidebar and footer stay bright and live, and clicking the sidebar silently discards the prompt |
| T016 | X06 | Arming a slot posts Discover's 'Restart is required … for the updates to take effect' with 'Update and Restart / Update and Shut Down', although no update is pending |
| T017 | X07 | KRunner: a partial KDOS settings title still lists 'KDOS Look' twice (System Settings and Offline library) |
| T018 | X08 | kpkg remove on a --root that is a regular file gives a lock error with a doubled slash instead of 'no such directory' |
| T019 | X10 | Encryption page passphrase hint wraps 'guess' onto a second line, and it sits under the Again field instead of under Passphrase |
| T020 | X10 | Text typed while a password field's reveal (eye) button has keyboard focus is added to the password |
| T021 | smoke-live | S034 Notification history hides a middle 'KDOS Tools' entry with no 'Show more' expander |
| T022 | smoke-live | LibreOffice Discover page repeats the suite summary as the first body paragraph |
| T023 | smoke-live | LibreWolf cannot be opened on the live medium: not on the image, and the published v0.2 binhost does not hold it |
| T024 | smoke-installed | S032: exported box launchers come up with no icon in Kickoff (Debian foot) |
| T025 | smoke-installed | Boxes KCM: box banner joins two sentences with a lower-case start; a fetch-app box reports no recorded confinement |
| T026 | smoke-installed | Installer 'Start the install?' confirmation has no dimmed backdrop |
| T027 | smoke-installed | Installer overall progress bar barely moves during the copy |
| T028 | smoke-installed | Release notes on a one-slot install describe a second slot and going back |
| T029 | smoke-installed | First-login pending notification says 'An application' for a chosen group |
| T030 | gui-install | S015 (R035) copy time-left accuracy |
| T031 | gui-install | S017 copy time-left overestimates 2-3x and climbs while progress advances |
| T032 | gui-install | Welcome tour 'Your System' page gives a one-slot update command that disagrees with System Versions (no fetch step, no sudo) |
| T033 | gui-install | Disk page keeps 'Two system slots' ticked while warning that the disk gets one |
| T034 | gui-install | Copy-line overshoot persists in the second half of a real copy (1.4-1.56x) |

## Fourth fix round

The 34 items of the third re-test, as eight units, each fixed, reviewed and refixed.

| Unit | Severity | What | Items |
|---|---|---|---|
| Y01 | minor | Copy time-left and the copy line discount rsync's hard-link names; the overall bar (graphical and terminal) weighs each step; Tab skips the reveal button; the Encryption hint sits under Passphrase; the erase and quit sheets dim; the Disk page drops the two-slot line on a one-slot disk; the live VG activation documented | T006–T009, T019, T020, T026, T027, T030, T031, T033, T034 |
| Y02 | minor | crun's no-cgroup kill reaches the init's descendants, so a stopped box restarts at once; `kdos-box stop` reaches the launches still waiting | T001, T002 |
| Y03 | minor | Discover's restart notice only for a staged update, once; the Updates page follows an outside arm; the suite page without the repeated summary | T003–T005, T016, T022 |
| Y04 | minor | A stop after the image fill is honoured or refused, never ignored; `kdos remind ls` by due time; Night Light wording; one-slot release notes; foot's task-manager grouping | T010, T012, T013, T024, T028 |
| Y05 | minor | The notification history hides a group's oldest entries and always shows the expander | T014, T021 |
| Y06 | minor | Welcome's one-slot update route; the pending-install notice names selections | T029, T032 |
| Y07 | polish | `kpkg --root` on a non-directory; a library row for a page's name is `<name> in <source>`; two selftests from one checkout | T011, T017, T018 |
| Y08 | polish | The System Versions prompt blocks the window; the Boxes banner and a distrobox box's confinement | T015, T025 |

Skipped: T023 (LibreWolf on the live medium needs a published binhost or a larger image). The
kdos-fwask test that posts an action after a held Notify reply now waits for the reply, as a real
server orders them.

### Re-test of the fourth round

The rebuilt ISO was re-tested lane by lane with screenshot analysis, and a skeptic checked each lane: 58 verdicts, 34 fixed, 14 partial, 3 still, 4 regressed, 3 untestable. The open and new items are numbered U001-U044.

| Key | Lane | Severity | Item |
|---|---|---|---|
| U001 | Y01 | open | T006 partial |
| U002 | Y01 | open | T007 partial |
| U003 | Y01 | open | T008 partial |
| U004 | Y01 | open | T030 still |
| U005 | Y01 | open | T031 regressed |
| U006 | Y01 | open | T034 still |
| U007 | Y01 | open | T027 partial |
| U008 | Y01 | open | T020 regressed |
| U009 | Y01 | open | T019 partial |
| U010 | Y01 | minor | First password field's reveal (eye) button cannot be reached by keyboard on the Account and Encryption pages, and the docs and comments say it can |
| U011 | Y01 | minor | Slot B copy time left climbs (0:15 -> 0:23) and reads 2.3x high near its end |
| U012 | Y01 | polish | Install page shows the firmware warning with raw backticks and a lowercase first letter |
| U013 | Y02 | major | A distrobox with an app running in it (a `distrobox enter` exec session) cannot be stopped cleanly or restarted: podman stop refuses, and kdos-box stop returns 0 but leaves the app and the box gives EBUSY on start |
| U014 | Y03 | polish | System Versions keeps the red 'needs an administrator's password, and none was given' banner after a later arm succeeds |
| U015 | Y04 | open | T028 partial |
| U016 | Y04 | minor | On a one-slot machine, `kdos update apply` takes the image lane and refuses, so the documented update route (fetch then apply) fails whenever the release publishes a slot image |
| U017 | Y04 | polish | `kdos-fetch-app --remove` leaves a pinned launcher behind as a blank-document icon in the task manager |
| U018 | Y06 | polish | A native group chosen in the installer that did not install (groups-pending) gets no first-login notice; only boxed selections (apps-pending) do |
| U019 | Y07 | polish | KRunner: for the partial query 'KDOS Lo' the top row is a weak fuzzy hit ('KDOS Help'), above the System Settings 'KDOS Look' page |
| U020 | Y08 | open | T015 partial |
| U021 | Y08 | open | T025 regressed |
| U022 | Y08 | major | kdos-box list loses every container on podman 6: `{{index .Labels "manager"}}` template errors, so every box reads 'not created' and container-only boxes vanish |
| U023 | Y08 | major | System Versions prompt hides behind System Settings after any click on the main window, leaving a dimmed page that ignores input |
| U024 | Y08 | polish | Start-version prompt window carries minimise and maximise buttons and a truncated caption, and its own taskbar entry |
| U025 | smoke-live | open | T023 LibreWolf cannot be opened on the live medium: not on the image, and the v0.2 binhost does not hold it still |
| U026 | smoke-live | polish | Installer Summary lists 'Applications: 1 group, about 8.6 MiB — recorded; the first login offers to build them' although no group was ticked, beside 'KDOS applications: None' |
| U027 | smoke-live | polish | Discover Editor's Choice shows generic placeholder icons for Scribus, Hugin and MeshLab |
| U028 | smoke-installed | open | T025 Boxes KCM banner sentence case; kdos-debian (fetch-app distrobox) confinement verdict regressed |
| U029 | smoke-installed | open | T015 Start This Version prompt dims only the KCM; sidebar stays live (extra check, Y08 open rig item) partial |
| U030 | smoke-installed | major | kdos-box list / Boxes KCM see no containers: the podman ps format `{{index .Labels "manager"}}` errors on podman 6.1.2 |
| U031 | smoke-installed | major | The kdos-fetch-app distrobox (kdos-debian) vanished between a logout and the next login; foot's launchers and pin remain pointing at it |
| U032 | smoke-installed | major | A login after logout intermittently bounces straight back to the greeter (sddm-helper exited with 63) |
| U033 | smoke-installed | minor | Start This Version prompt is a separate window that falls behind maximized System Settings, leaving the page dimmed and unusable |
| U034 | smoke-installed | polish | After a reboot the screen stays on the boot splash until the pointer moves, though the greeter has been up for minutes |
| U035 | gui-install | open | T006 (S017) copy time-left accuracy partial |
| U036 | gui-install | open | T007 (S015) copy time-left overshoot partial |
| U037 | gui-install | open | T030 (S015/R035) copy time-left accuracy on a 12 GiB GUI install partial |
| U038 | gui-install | open | T034 copy-line overshoot persists in the second half (1.4-1.56x) partial |
| U039 | gui-install | open | T027 overall progress bar barely moves during the copy partial |
| U040 | gui-install | open | T019 encryption passphrase hint wraps 'guess' and sits under Again partial |
| U041 | gui-install | minor | A password field's reveal (eye) button can no longer be reached by keyboard; kinstall.md says Shift+Tab reaches it |
| U042 | gui-install | polish | Overall bar is weighted toward the short steps after the copy on a one-slot disk: 63% when the copy ends at 95% of the install time |
| U043 | gui-install | polish | Slot B time-left starts low and goes up (11 s, 12 s, 11 s) while the copy advances |
| U044 | gui-install | polish | Summary step 2 says 'write the GPT layout: ESP, BIOS boot, root' when the third partition is an LVM physical volume |

## Fifth fix round

| Unit | Severity | Items | Fix |
|---|---|---|---|
| Z01 | major | U021, U022, U028, U030, U031, U013 | kdos-appbox: box list on podman 6, a profile-less distrobox removed at login, and distrobox exec sessions surviving kdos-box stop |
| Z02 | major | U020, U023, U029, U033, U024, U014 | System Versions start prompt: a real dialog transient of System Settings, and a stale error banner |
| Z03 | major | U032 | Login after logout bouncing back to the greeter (sddm-helper exit 63): pin the stage, then fix the teardown race |
| Z04 | minor | U015, U016, U017 | kdos update on a one-slot machine skips the slot image, and kdos-fetch-app --remove unpins the launcher |
| Z05 | polish | U018 | First-login notice also names native groups that did not install (groups-pending) |
| Z06 | minor | U001, U002, U003, U004, U005, U006, U011, U035, U036, U037, U038, U043, U007, U039, U042, U044, U008, U010, U041, U009, U040, U012 | Installer: copy time left that follows the slow tail, a bar weighted by time, the keyboard path to the reveal buttons, the hint on one line, and formatted warnings |

Skipped:

- U019: Pre-existing KRunner ranking across runners: Plasma's Applications (services) runner ranks 'KDOS Help', 'Install KDOS' and 'The KDOS Book' above System Settings' 'KDOS Look' on the shared word 'KDOS'. None of those desktop entries has a keyword matching 'Lo'. Changing it means patching plasma-worksp
- U025: A release-pipeline matter: LibreWolf is not on the base image (putting it there is a product decision), and the published v0.2 binhost does not hold its package while its catalogue still names it. Fixing it means republishing the binhost or its catalogue, which is a publish.
- U026: Wrong once the code is read, as the smoke-live skeptic refuted it. The 'Applications' row counts the boxed catalogue groups (plan.apps), and `essential` is preselected by design (probe.c ki_apps_enter; Backend::refreshApps seeds the ticks from it). It sits below the native groups the tester saw, whi
- U027: The three cards show the Graphics category fallback icon (KdosBackend.cpp categoryIcon: Graphics -> image-x-generic, a deliberate choice documented there), because the published binhost AppStream catalogue carries only stock icon names, which a live machine without those packages lacks. A real fix s
- U034: Probably a rig artefact: the splash stays on the virtio-gpu scanout after a warm reboot until input arrives, and e2e4 showed the same. The lane itself flags it as a likely virtio-gpu/rig artefact, and there is no evidence from real hardware or a cold boot.

Beside the units, the round makes these changes:
- **Base image:** it carries Haruna, KolourPaint, Skanpage, Filelight, KFind, KCharSelect, KClock and KWalletManager. That is the user's decision of 2026-11-18: GUI programs only, with the ISO kept under GitHub's 2 GiB asset limit.
- **texlive-doc:** 42_graphics builds it, and `ports/unbuilt` names nothing.
- **kdos-installer:** 41_system builds it as a package, so the binhost carries every recipe.
- **Stale binhost packages:** the 60 ports the fix rounds rebuilt without `KDOS_MAKE_BINHOST=1` are rebuilt with it.
- **LibreWolf (U025):** it is resolved by that binhost republish. Publishing is an outward step and waits for the user.
- **Boxes settings page:** it shows a listing that fails after a good one as an error banner above the rows it last listed.

Not proven on the rig by the fixers (the re-test does it):

- Z01: U013 rig proof (3): not run. Proof (3) is: `distrobox enter kdos-debian -- sleep 400 &`, `kdos-box stop kdos-debian`, then `podman start kdos-debian`, once with the init alive and once with it already exited. Whether podman drops the exec session after its con
- Z01: Rig proofs (1) and (2): not run. (1) is the `{{.Label "manager"}}` ps format and `{{.State.Pid}} {{.Id}}` inspect format on the shipped podman 6.1.2, plus what the Boxes KCM shows. (2) is kdos-debian surviving logout, login and reboot. The format strings are p
- Z02: Rig proof is still open for U020/U023/U029/U033/U024. None of these has been observed yet: KWin stackingOrder showing transient=true and transientFor=System Settings, the prompt staying on top after a click on 'Users', Dialog Parent dimming the whole window, o
- Z02: Minimise is kept on the prompt by design. KWin minimises and restores a modal dialog together with its main window (Workspace::updateMinimizedOfTransients), so it adds no window rule and no known-gaps entry.
- Z03: U032's root cause is still not pinned. That needs the 10-cycle rig reproduction, and VMs are not allowed in this unit. The wrapper's syslog lines now name the stage, and the status of a profile exit too.
- Z03: The zsh path (`zsh -l -f` plus the wrapper reading the zsh files) is unproven by a run: the host has no zsh. Only zsh 5.9.2's Doc/Zsh/options.yo and files.yo back it: -f is NO_RCS, -l is LOGIN, and zshenv is always read. The image's users are on bash.
- Z03: The settle second narrows the gap between the stop process ending and turnstiled's clear; it does not close it. The page and the header both say so.
- Z04: Rig proof not run (no VMs here). On a one-slot 16 GiB install with a newer signed image, `sudo kdos update apply` should print the one-slot pass-over line and take the binhost lane. A finished binhost install can only be proven where a binhost is published for
- Z04: Unproven on a real plasmashell: that Plasma 6 scripting accepts UNPIN_JS as written. That covers readConfig('launchers', []) returning a list, writeConfig taking a JS array, reloadConfig making the pin disappear, and the `[activities]\n` prefix format of activ
- Z04: An unpin changes the panel description, so `kdos desk status` shows that desk as edited, as it does after any pin change.
- Z04: A non-root `--dry-run` on a two-slot machine whose state is closed still shows 'no A/B slots' through apply_target. That code is older than this unit and not changed; a real apply runs as root, where the state is readable.
- Z05: Rig proof not run (no VMs allowed). Left for the re-test: unattended kinstall with groups = kde-artwork and no binhost, the first-login notice naming the group and `--pending`, and silence or withdrawal after `--pending`.
- Z05: docs/kfs/data/recipes.stamp is stale because kfs.md changed; the Book phase runs make book-stamp.
- Z05: plasma-shell.md, bus.sh and installation.md are not owned by this unit; their edits are in needs_other_files.
- Z06: No real `rsync -aHAX --info=progress2 --no-inc-recursive` trace of the live tree has been recorded; it needs a VM. U001-U006, U011, U035-U038 and U043 are proven against a model only, with costs fitted to the rig's byte trajectory. KI_COPY_OTHER_W and KI_COPY_
- Z06: It is unproven that rsync's to-chk count moves in the walker's order for -H later names; rsync may hold them back until the first name is done.
- Z06: If directories cost twice what files do (slot B's alternative fit), the time left still reads about 1.27x high in the middle of the copy.
- Z06: KI_PACKS_PORT_DS (1 s per port when a binhost is named) has never been measured.
- Z06: kfscheck pins for the 8 docs/kfs/16-wiki pages on install.c, and recipes.stamp, are left for the Book phase (repin and book-stamp are forbidden to this unit).
- Z06: The rig proofs (12 and 30 GiB GUI installs with 5 s shots, pixel-scanned bar, Summary step 2 text, keyboard reveal, one-line hint, Install page warning) have not been run.

### Re-test of the fifth round

The rebuilt ISO and a served copy of build/binhost were re-tested lane by lane with screenshot analysis, and a skeptic checked each lane: 78 verdicts, 44 fixed, 26 partial, 6 still, 1 not a defect, 1 regressed. The binhost lane installed LibreWolf, VLC, GIMP, kde-system and kdos-installer from it; every one of the 2,442 recipes has a current package there. The open and new items are numbered V001-V052 (V029, LibreWolf on the live medium, closes with the binhost republish).

| Key | Lane | Severity | Item |
|---|---|---|---|
| V001 | Z01 | minor | After a distrobox's init dies, kdos-box stop misses the dead init's orphan (the entrypoint's `sleep 15`), so the main conmon lingers and the next start or stop/start within ~15 s fails with overlay EBUSY |
| V002 | Z02 | open | U020 partial |
| V003 | Z02 | open | U029 partial |
| V004 | Z02 | open | U024 partial |
| V005 | Z02 | major | kdos-bootctl's boot-state and limine.conf rewrites are not durable on the vfat ESP despite fsync(file)+rename+fsync(dir): a power cut seconds after Try/Cancel loses the change, and a longer sequence left both files zero-length and the disk stuck in Limine |
| V006 | Z02 | polish | Start-version prompt decoration shows a ghost second × on first open |
| V007 | Z02 | minor | kdos-settings.md claims KWin's Dialog Parent dims the whole System Settings window, but the effect does not load under software rendering, so the sidebar and footer stay bright |
| V008 | Z03 | major | pam_turnstile's session socket leaks into every process of the session, so a box still running at Log Out keeps the user 'logged in': the boxes and user services are never stopped and the runtime directory is never cleared |
| V009 | Z04 | open | U015 partial |
| V010 | Z04 | open | U017 still |
| V011 | Z04 | polish | kdos update apply prints its lane explanation after the refusal it explains when output is not a terminal |
| V012 | Z04 | polish | `apply --in-place` on a two-slot machine says 'no A/B slots' |
| V013 | Z05 | polish | Pending notice shows literal backticks around the command and names the group by its id |
| V014 | Z05 | polish | A groups-only pending notice uses the Discover (plasmadiscover) icon although it offers no Discover action |
| V015 | Z06 | open | U001 partial |
| V016 | Z06 | open | U002 partial |
| V017 | Z06 | open | U003 partial |
| V018 | Z06 | open | U004 partial |
| V019 | Z06 | open | U005 partial |
| V020 | Z06 | open | U006 partial |
| V021 | Z06 | open | U011 partial |
| V022 | Z06 | open | U035 partial |
| V023 | Z06 | open | U036 partial |
| V024 | Z06 | open | U037 partial |
| V025 | Z06 | open | U038 partial |
| V026 | Z06 | open | U043 partial |
| V027 | Z06 | polish | Summary page switches to the one-column (label above value) layout on a two-slot disk, because step 2's new text is too long |
| V028 | Z06 | minor | The copy line's time left still holds flat for about 10 s in the slow tail, despite the planned countdown of a held estimate |
| V030 | smoke-live | open | U027 Discover Editor's Choice shows placeholder icons for Scribus, Hugin and MeshLab still |
| V031 | smoke-live | minor | Installer Summary falls into Kirigami's one-column layout at the default window size: every label sits above its value and the page needs about three screens of scrolling |
| V032 | smoke-installed | open | U029 (T015) Start This Version prompt dims only the KCM; sidebar stays live partial |
| V033 | smoke-installed | polish | Start Next Time? prompt opens with a stale, inert second close (X) button in its title bar |
| V034 | smoke-installed | minor | kdos-box stop on a box with a `kdos-box enter` shell takes 16.4 s |
| V035 | gui-install | open | U001 (T006) copy time left accuracy, root copy partial |
| V036 | gui-install | open | U002 (T007) root copy overshoot 1.28-1.85x partial |
| V037 | gui-install | open | U006 (T034) overshoot in the second half of the copy partial |
| V038 | gui-install | open | U035 (T006/S017) copy time-left accuracy partial |
| V039 | gui-install | open | U036 (T007/S015) overshoot partial |
| V040 | gui-install | open | U038 (T034) second-half overshoot partial |
| V041 | gui-install | open | U043 slot B time left starts low and goes up partial |
| V042 | gui-install | open | U044 Summary step 2 names root partition on an LVM layout regressed |
| V043 | gui-install | minor | Summary page falls to a one-column layout on the default two-slot install, because step 2's new text is too wide |
| V044 | gui-install | polish | First-login notice shows raw backticks and the group id instead of its name |
| V045 | gui-install | polish | Done page tells the user to run 'kdos update install --pending' without sudo, though it needs root |
| V046 | gui-install | polish | Summary promises the chosen native group is 'downloaded during the install' when the image names no binhost |
| V047 | binhost | open | U027: Discover's Editor's Choice shows generic placeholder icons for Scribus, Hugin and MeshLab still |
| V048 | binhost | major | KDE Help Center cannot show any application handbook after installing kde-system: docbook-xsl is in ports/buildonly, so neither the image nor the prebuilt install carries the stylesheets kdoctools reads at run time |
| V049 | binhost | minor | `kdos update fetch --binhost-only` fails with exit 1 and prints no reason when the release index cannot be downloaded (http:// URL, connection refused); `install` then gives circular advice |
| V050 | binhost | polish | Discover shows LibreWolf with a generic globe icon, 'Unknown author' and 'Licenses: Unknown' |
| V051 | binhost | polish | Discover install confirmation reads 'Downloads 1 package (0 bytes).' when the package file is already cached |
| V052 | binhost | polish | kinstall Welcome preflight reads '! running as root' in red when it is not running as root |

## Sixth fix round

| Unit | Severity | Items | Fix |
|---|---|---|---|
| R01 | major | V005, V010, V011, V012, V049 | kdos-tools: ESP writes durable on Linux vfat (bootctl), kdos-fetch-app --remove unpins the alias, kdos update apply/fetch messages |
| R02 | major | V008 | turnstile: pam_turnstile's session socket close-on-exec, so the last logout stops the boxes and user services |
| R03 | major | V048 | docbook-xsl and docbook-xml shipped at run time for kdoctools, so KDE handbooks render |
| R04 | minor | V015, V016, V017, V018, V019, V020, V021, V022, V023, V024, V025, V026, V028, V035, V036, V037, V038, V039, V040, V041, V027, V031, V042, V043, V045, V046, V052 | Installer: copy time-left fitted to a recorded rsync trace with a counted-down hold, Summary step text that keeps the two-column form, and the pending-group wording |
| R05 | minor | V001, V034 | kdos-box stop: reach the dead init's orphans under the main conmon, let its cleanup run, and end interactive exec shells at once |
| R06 | minor | V004, V006, V033, V002, V003, V007, V032 | System Versions start prompt: no ghost close button on first open, and a dim that does not promise a live sidebar |
| R07 | polish | V013, V014, V044 | kdos-notice pending notice: group named by its title, no literal backticks, an icon that fits a terminal-only remedy |
| R08 | polish | V051, V050 | Discover: install confirmation that counts cached packages truthfully, and the binhost catalogue's licence and homepage from the recipe |

Skipped:

- V009 (U015 partial): The image-lane half is fixed. Whether the documented one-slot route finishes end to end can be proven only against a tagged release with a published binhost, which needs a git tag and a publish.
- V030 (U027, smoke-live): Scribus, Hugin and MeshLab show Discover's Graphics category icon (KdosBackend categoryIcon maps Graphics to image-x-generic) because neither Breeze nor the image has their icons. debian-dep11 ships only Components-amd64.yml.gz. A real icon needs Debian's dep11 icons tarball, a new upstream source, or a republished pack catalogue that carries cached icons.
- V047 (U027, binhost): Same defect as V030. The icons come from the boxed-app catalogue's Debian AppStream data, which carries no icon files on the machine. Fixing it needs a new upstream source (dep11 icons-64x64) or a publish.
- V050 (icon half only): A not-installed port can show its own icon only through AppStream cached icons, which means a new icons artefact beside the binhost catalogue, listed in the signed release index and fetched by `kdos update fetch`. That is a change to the published release format and needs a republish. The licence and homepage half is in R08.

Beside the units:

- **docbook-xsl and docbook-xml:** they leave `ports/buildonly`. kdoctools names them at run time, so the base image and a prebuilt install carry the stylesheets the KDE handbooks need.
- **Installer copy time left (R04):** it is not refitted. kinstall writes a rig trace when `KINSTALL_COPY_TRACE` names a file, and the selftest replays it. Fitting the constants waits for traces recorded on the rebuilt live ISO (30 GiB two-slot and 12 GiB one-slot).

Not proven on the rig by the fixers (the re-test does it):

- R01: V005: nothing has shown durability on a real Linux vfat ESP yet. It rests on reading the linux-7.2.11 source (vfat_rename/vfat_sync_ipos only mark the inode dirty; fat_file_fsync -> sync_inode_metadata writes the entry) and needs the rig's offline read (try; try+cancel; run 8's GUI sequence plus two boots; fsck.fat -n clean).
- R01: A crash in the few milliseconds between rename() and esp_settle's fsync can still let background writeback flush the freed FAT before the entry is written. Only a flush or dirsync mount of the ESP closes that window, and that is outside these files and outside the plan.
- R02: V008 is not proven at run time. Proving it needs a rebuild of turnstile and the image, then the rig check: only sddm-helper holds the peer inode of the control.sock connection, ksvc stop runs, /run/user/1000 is cleared, t4 is stopped, sleep 600 sessions do not pile up over 5 logins, and a console login stays held until its shell exits. This unit may not run builds or VMs.
- R02: No test fails without the fix. turnstile has no test harness here, and selftest covers only the libk* libraries.
- R03: ports/core/doctools/docbook-xsl/kfs.md still says the desktop never reads the DocBook stylesheets. Its owner must edit it (see needs_other_files).
- R03: Not yet proven on a rebuilt tree or the rig: the kdoctools rebuild, imageset --verify on that tree, the ISO size (estimated at about +4 MiB, to roughly 1,695 of 1,900 MiB), and khelpcenter rendering help:/kinfocenter and help:/ksystemlog. All of these need make build and a VM.
- R04: V015-V026, V028, V035-V041 (the copy time left: 1.47x at 81-89% on 30 GiB, 10-16 s flat holds, Slot B rising) are not fixed. This is the reviewer's must. Plan steps (1) and (2) need a real rsync trace from the rig's live ISO, and this unit may not start VMs, so there is no trace fixture and no replay case that fails on today's constants. ki_copy_note, copy_hold, KI_COPY_HALF_S, KI_COPY_OTHER_W and the floors are unchanged. The first fix's model replay showed that the planned countdown hold makes things worse (1.29-1.35x rises, 1.60x worst), and that the excess comes from a per-file cost that depends on where the file sits in the tree. That has to be fitted against recorded traces, not against a model.
- R04: Reviewer should, rig check of SummaryPage at 1046 px: needs a VM, so it is in outward_actions. The QtTest guard is in a file this unit does not own; see needs_other_files.
- R04: Reviewer should, the full selftest stopping at 'kdos-fetch-app exports the package's files': that case belongs to another unit, and nothing in this unit's files changes it.
- R05: kdos-appbox.h comments (reviewer must) and docs/kfs/16-wiki/boxes.md (reviewer should) are owned by others; exact edits are in needs_other_files.
- R05: Needs the rig: that the real conmon's exit command and the end of the orphans release the overlay (no EBUSY across the 3-round kill-init / stop / start sequence), and that the Konsole of `kdos-box enter t4` closes within 3 s. The fake process trees prove only the parent-chain and namespace matching.
- R06: Not tested on the rig: I ran no VM. The proof named in the plan still has to be run: the 8x title-bar crop at once and after 3 s, the llvmpipe pixel averages, and the virgl dialogparent darkening. The claims that the hold arrives before the map and that the re-hold grows rest on the Qt 6.12 and KWin 6.8.2 sources and on offscreen sizes from Qt 6.11.1 in the devdeps image, not on Wayland.
- R06: A re-hold that shrinks a dimension still offers maximise for a moment on a mapped window, because KWin applies the minimum before the maximum. No ordering on the client side avoids this. kfs.md and the systemversions.h comment record it. On the measured paths the re-hold only grows.
- R07: V044: the notice staying on screen for 4+ minutes is the documented persistent=1, left as is per the plan.
- R07: Not proven on the rig: how Plasma draws the new body and the system-software-install icon.
- R08: V050 icon half (stock icons with no cached icon set): skipped per the unit plan

### Re-test of the sixth round

The rebuilt ISO and binhost were re-tested lane by lane with screenshot analysis, and a skeptic checked each lane: 90 verdicts, 44 fixed, 23 still, 22 partial, 1 untestable. The ESP writes survive a power cut (offline FAT reads, fsck clean), the last logout stops the boxes and user services, and KDE handbooks render. The R04 lane recorded the installer's copy traces on a 30 GiB two-slot and a 12 GiB one-slot install. The open and new items are numbered W001-W057.

| Key | Lane | Severity | Item |
|---|---|---|---|
| W001 | R04 | open | V015 still |
| W002 | R04 | open | V016 still |
| W003 | R04 | open | V017 still |
| W004 | R04 | open | V018 still |
| W005 | R04 | open | V019 still |
| W006 | R04 | open | V020 still |
| W007 | R04 | open | V021 still |
| W008 | R04 | open | V022 still |
| W009 | R04 | open | V023 still |
| W010 | R04 | open | V024 still |
| W011 | R04 | open | V025 still |
| W012 | R04 | open | V026 still |
| W013 | R04 | open | V028 still |
| W014 | R04 | open | V035 still |
| W015 | R04 | open | V036 still |
| W016 | R04 | open | V037 still |
| W017 | R04 | open | V038 still |
| W018 | R04 | open | V039 still |
| W019 | R04 | open | V040 still |
| W020 | R04 | open | V041 still |
| W021 | R04 | minor | 12 GiB GUI install: the copy's time left jumps from 0:00:41 to 0:01:26 thirteen seconds in, then overshoots 1.5-1.78x for about 16 s |
| W022 | R04 | minor | Root copy's time left now rises on the 30 GiB GUI install (0:00:36 to 0:00:45) |
| W023 | R04 | polish | Applications page still promises the groups are downloaded during the install on an image with no binhost |
| W024 | R05 | polish | `konsole -e kdos-box enter t4` leaves a second, empty 'kdos-box — Konsole' window behind the box Konsole |
| W025 | R06 | polish | main.qml comment line overruns the wrap width |
| W026 | R07 | polish | Pending notice's command wraps at the hyphen of '--pending', splitting the command across lines |
| W027 | R07 | polish | An untitled group id is hyphen-broken mid-name |
| W028 | R08 | open | V050 partial |
| W029 | R08 | polish | Discover's install banner still promises to say 'how many packages it downloads, and how much', but for a cached package the confirmation now says 'already downloaded' |
| W030 | R08 | polish | Discover shows 'Downloading' beside the Install button while the polkit password dialog is still waiting, before anything is fetched |
| W031 | R08 | polish | 25 catalogue components still have no project_license |
| W032 | smoke-installed | open | V001 (context, lane Z01) dead-init orphan / EBUSY after kdos-box stop partial |
| W033 | smoke-installed | minor | kdos-box stop of a distrobox started less than ~15 s earlier takes 15.2 s: the entrypoint's `sleep 15`, reparented to the main conmon after the init's TERM, is never signalled, even in the SIGKILL pass |
| W034 | gui-install | open | V015 U001 root copy time-left accuracy (12 and 30 GiB) partial |
| W035 | gui-install | open | V016 U002 root copy overshoot partial |
| W036 | gui-install | open | V017 U003 end point / overshoot partial |
| W037 | gui-install | open | V018 U004 12 GiB overshoot partial |
| W038 | gui-install | open | V019 U005 slot B accuracy partial |
| W039 | gui-install | open | V020 U006 second-half overshoot partial |
| W040 | gui-install | open | V021 U011 slot B rise / high near end partial |
| W041 | gui-install | open | V022 U035 root copy within 25% from 20% on partial |
| W042 | gui-install | open | V023 U036 overshoot partial |
| W043 | gui-install | open | V024 U037 12 GiB frames / flat hold partial |
| W044 | gui-install | open | V025 U038 overshoot at 60-80% partial |
| W045 | gui-install | open | V026 U043 slot B starts low then rises partial |
| W046 | gui-install | open | V028 copy time left holds flat ~10 s still |
| W047 | gui-install | open | V035 U001 (gui-install) root copy accuracy partial |
| W048 | gui-install | open | V036 U002 (gui-install) overshoot partial |
| W049 | gui-install | open | V037 U006 (gui-install) second-half overshoot partial |
| W050 | gui-install | open | V038 U035 (gui-install) accuracy + 16 s hold partial |
| W051 | gui-install | open | V039 U036 (gui-install) overshoot partial |
| W052 | gui-install | open | V040 U038 (gui-install) second-half overshoot on 30 GiB partial |
| W053 | gui-install | open | V041 U043 slot B late overshoot partial |
| W054 | gui-install | polish | Apps page header still promises 'downloaded prebuilt … during the install' on an image with no binhost |
| W055 | gui-install | polish | Welcome tour 'Make It Yours' tells the user to refresh in Discover on an image that names no binhost |
| W056 | binhost | open | V050 Discover shows LibreWolf with a generic globe icon, 'Unknown author' and 'Licenses: Unknown' partial |
| W057 | binhost | minor | After LibreWolf is installed from Discover and then removed with `kpkg remove`, Discover keeps showing it as installed (Launch, 'Part of the KDOS system') and cannot reinstall it; Launch fails with 'Update Issue' |

## Seventh fix round

| Unit | Severity | Items | Fix |
|---|---|---|---|
| S01 | minor | W001, W002, W003, W004, W005, W006, W007, W008, W009, W010, W011, W012, W013, W014, W015, W016, W017, W018, W019, W020, W021, W022, W034, W035, W036, W037, W038, W039, W040, W041, W042, W043, W044, W045, W046, W047, W048, W049, W050, W051, W052, W053 | Installer: copy time left fitted to the recorded 30 GiB and 12 GiB traces, a held time that counts down, bounded rises, and a selftest that replays both traces (root and Slot B, also slowed 2x/3x/5x) |
| S02 | minor | W033, W032, W024 | kdos-box stop signals the distrobox entrypoint's late orphans before any podman call that waits on the lock; `konsole -e kdos-box enter` runs in that terminal and titles its tab (OSC 30, OSC 0) |
| S03 | minor | W057, W030, W029, W031 | Discover: installed state read from kpkg, waiting status before the password, banner and confirmation wording, the recipe licence on every bare catalogue entry |
| S04 | polish | W023, W054, W055, W026, W027 | Group-install messages with no binhost: the installer Apps page asks the plan and says when nothing is downloaded, Make It Yours says no group is ready-made, and the pending command no longer breaks at its hyphen |
| S05 | polish | W025 | kcm-kdos-system main.qml comment reflowed |

Skipped:

- W028: The licence and homepage half is fixed. The remaining licence gap is the same defect as W031, planned in S03. What is still open is the icon and the author. The icon needs a cached-icons artefact listed in the signed release index, which is a release-format change and a binhost republish. 'Unknown author' is the documented outcome when no metainfo names a developer: catalog/main.cpp states it, and a recipe names no author. That is a product decision, not a defect.
- W056: Same as W028, seen from the binhost lane. The licence and homepage are fixed. The icon needs a new cached-icons release artefact and a republish, and 'Unknown author' is by design, since no developer is invented. The 26 components still without a licence are covered by W031 in S03.

Beside the units:

- **Book:** nine `kfs:include` ranges repinned after install.c moved; `docs/kfs/16-wiki/boxes.md` describes the in-place enter.
- **User guide:** applications.md says the confirmation counts what the install takes and what it downloads; getting-started.md says what Make It Yours shows when no binhost can be named.

Not proven on the rig by the fixers (the re-test does it):

- S01: With the fall rate at 0.6 of the clock, frames 1 s apart can show one figure three times on a disk 2-5x slower than the rig's. The replay bounds this at four frames, and 2 s frames show no three-frame hold. Each fall rate tried (0.75, 0.85, 1.0, or 1.0 only above the bound) breaks the live-tree mix case or the recorded-stream case.
- S01: The one whole rise to the raw estimate (cost==2 after KI_COPY_COST_S) and the unheld estimate on a stalled copy (!moved) never fire on the recorded traces at 0.5-3x. Only the synthetic ordered-tree cases cover them, so the rig re-test should watch for them.
- S01: KI_COPY_OTHER_W cannot be refit from the traces and stays 0.3.
- S01: Slot B can read about 0.6x real (3-4 s low) late in the copy. Nothing bounds the low side.
- S01: The test failing on the round-6 constants cannot be re-shown: that install.c is not on disk.
- S01: Rig proof not done (no VMs).
- S02: W033/W032: the 15 s cause is inferred from podman 6.1.2, conmon 2.2.1 and distrobox-init source. It is not measured on the rig, so a real stop under 3 s is unproven until the rig re-test.
- S02: W024: the OSC 30 tab title is taken from Konsole 26.08.2 source and not yet seen in a rig shot.
- S03: Not run here: the catalogue proof (no component of the rebuilt binhost catalogue missing project_license) needs a rebuilt binhost, and the rig proof (remove LibreWolf, then reinstall it; the status before the polkit password) needs a VM.
- S03: Left as it was: a failing QVERIFY in hostCatalogueListing returns before it restores m_backend, so pendingAndCategories and removeRuns fail after it too. The test was already written this way.
- S04: The QtTest for AppsPage with a plan map that has no binhost key is not added, because installertest.cpp is not owned by this unit. See needs_other_files.
- S04: The rig proofs need a rebuilt ISO and were not run. On a one-slot installed machine, Make It Yours shows the no-binhost message only after one Discover Refresh, or `sudo kdos update fetch --binhost-only`, has recorded that no binhost is named. Before that, a refresh might still name one, and a user cannot read the slot layout.

### Re-test of the seventh round

The rebuilt ISO (1,693 MiB) and binhost (2,442 of 2,442 ports at their recipe hash) were re-tested lane by lane with screenshot analysis, and a skeptic checked each lane: 113 verdicts, 95 fixed, 18 partial. The root copy's time left stays within 1.21x from 20% of the copy on, with no rise and no hold, on 12 GiB and 30 GiB GUI installs; Slot B no longer rises but one late frame read about 1.4x. A distrobox box stops in under 3 s in its first 15 s, `konsole -e kdos-box enter` runs in place, and Discover reinstalls a package removed with kpkg. The open and new items are numbered Q001-Q030.

| Key | Lane | Severity | Item |
|---|---|---|---|
| Q001 | S01 | open | W005 partial |
| Q002 | S01 | open | W007 partial |
| Q003 | S01 | open | W009 partial |
| Q004 | S01 | open | W012 partial |
| Q005 | S01 | open | W020 partial |
| Q006 | S01 | open | W038 partial |
| Q007 | S01 | open | W040 partial |
| Q008 | S01 | open | W053 partial |
| Q009 | S01 | minor | Copy time left reads low for the first third of the root copy (unbounded low side) |
| Q010 | S04 | open | W055 partial |
| Q011 | S04 | polish | Pending-groups notice says the groups 'could not be installed during the install' on an image where the install never tried |
| Q012 | S04 | polish | Installer Apps header wraps the command between 'install' and '--pending' |
| Q013 | smoke-live | minor | Discover lists LibreOffice as 'LibreOffice Base' (placeholder '…' icon, Base's description, no suite list) when the binhost catalogue is the published v0.2 one |
| Q014 | smoke-live | polish | Make It Yours page intro still says the groups are 'downloaded ready to run' above the new no-binhost message |
| Q015 | smoke-installed | polish | Typing `kdos-box enter <box>` in a Konsole prints a Qt warning into the calling terminal: 'qt.core.qobject.connect: QObject::disconnect: Unexpected nullptr parameter' |
| Q016 | gui-install | open | W005 (V019) slot B rise / 1.70x partial |
| Q017 | gui-install | open | W007 (V021) slot B rise / 1.70x at 63% partial |
| Q018 | gui-install | open | W008 (V022) frames over 1.25x from 20% on partial |
| Q019 | gui-install | open | W009 (V023) worst overshoot partial |
| Q020 | gui-install | open | W020 (V041) slot B 1.27-1.33x at 3.2-3.7 GiB partial |
| Q021 | gui-install | open | W038 (V019 U005) slot B accuracy partial |
| Q022 | gui-install | open | W040 (V021 U011) slot B high near end partial |
| Q023 | gui-install | open | W053 (V041 U043) slot B late overshoot partial |
| Q024 | gui-install | polish | Summary 'Steps' still says 'Packs — native groups installed' on an image with no binhost, while the rows above say the groups are only recorded |
| Q025 | gui-install | polish | Make It Yours header promises 'downloaded ready to run' right above the banner saying nothing can be installed ready-made |
| Q026 | gui-install | polish | Apps page header mixes 'binary host' and 'binhost' in one sentence |
| Q027 | gui-install | polish | Pending notice joins group titles with a comma, which is ambiguous when a title contains 'and' |
| Q028 | gui-install | polish | Root copy can still rise by 4 s mid-copy (12 GiB, 0:00:24 to 0:00:28 at 3.2 GiB) |
| Q029 | binhost | open | W056 (V050) Discover shows LibreWolf with a generic globe icon and 'Unknown author'; the licence half was fixed earlier partial |
| Q030 | binhost | polish | Discover started outside the logind session (rig --cmd via su -) cannot install: pkexec fails with 'Error creating textual authentication agent', and the user sees the generic 'Update Issue … Please try again later' dialog |

## Eighth fix round

| Unit | Severity | Items | Fix |
|---|---|---|---|
| T01 | minor | Q009, Q001, Q002, Q003, Q004, Q005, Q006, Q007, Q008, Q016, Q017, Q018, Q019, Q020, Q021, Q022, Q023, Q028, Q024, Q012, Q026 | Installer: copy time left fitted to a third recorded trace (two30b) with a low-side bound, a tighter near-end slack and no rise on the plain replays; the Packs step says when the groups are only recorded; "binary host" throughout, the pending command on its own line |
| T02 | minor | Q013 | Discover: a catalogue with no KDOS::BareEntry mark still lists LibreOffice as the suite |
| T03 | polish | Q011, Q027, Q014, Q025 | First-login group messages: the pending notice says the groups are not installed yet, one per line; Make It Yours does not promise downloads with no binary host |
| T04 | polish | Q015 | kdos-box enter: the separate box window's Konsole writes nothing into the calling terminal |

Skipped:

- Q010 (W055 partial: Make It Yours shows the Refresh advice on first view of a dirty-tree two-slot install): This is a product decision, not a code defect. On a two-slot machine a refresh can fetch a published tree that names a binhost, so binhost:true is correct until the first check has run (refresh_names_none in update.c ~5480). The loop already closes after one Discover visit (37-lookagain), and both the lane and its verifier name the first screen a product call.
- Q029 (W056 partial: LibreWolf's generic icon and 'Unknown author' in Discover): The icon before install needs a cached or remote icon artefact in the release's signed binhost index. That is a new published release artefact and a change to the signed-index format, which is a publish and a security-model decision. 'Unknown author' is kept on purpose. The shot after the install (24/29) was taken in the same Discover process that had already looked the icon up. The port does install hicolor/…/librewolf.png (build.sh ~200), so no shipped defect is shown there.
- Q030 (Discover started by su - outside the logind session cannot get a polkit agent): This is a rig artefact. The same flow started from KRunner inside the session installs (run5). The generic 'Update Issue' dialog is how upstream Discover handles any transaction error, and KdosTransaction::fail already puts the real reason on the page banner.

Not proven on the rig by the fixers (the re-test does it):

- T01: Q009, partly: the low bound reached is 0.72*left - 2 on the plain and re-timed replays, not 0.75; the e2e8 12 GiB re-timed case still reads 0.73-0.76x (see the first report).
- T01: Q028: the root rise from 24 to 28 at 3.2 GiB is still shown; the e2e8 re-timed case is held to the bounded-rise rule only.
- T01: Q001-Q008, Q016-Q023 (late Slot B high): fixed on the two30b replay only; unproven on the rig, since the failing GUI run has no trace.
- T02: I did not run the rig smoke-live check (live ISO with the v0.2 binhost, plasma-discover --search libreoffice) because no VMs are allowed. The test reproduces the published catalogue's shape (no marks, no start-centre licence, same-id bare duplicates) under the container's real libappstream. The published file shows Math only as a bare entry, so the expected page should read Base, Calc, Draw, Impress, Writer.
- T03: Rig proof not run (no VMs allowed). Two checks remain for the re-test: the notice on an installed no-binhost machine, and the Make It Yours header shots (01-welcome-p17, 20-tour-16).

### Re-test of the eighth round

The rebuilt ISO (1,693 MiB) and binhost (2,442 of 2,442 ports at their recipe hash) were re-tested lane by lane with screenshot analysis, and a skeptic checked each lane: 64 verdicts, 54 fixed, 7 partial, 2 still, 1 rig artefact. Slot B's late overshoot is down to 1.22x; the root copy still rose by 3 s once on a fresh 30 GiB install, Slot B held one figure for three 1 s frames once, and the early root figure reads about 0.75x. Discover lists LibreOffice as the suite with the published catalogue, and the no-binary-host wording holds on the installer, the notice and Make It Yours. The open and new items are numbered P001-P021.

| Key | Lane | Severity | Item |
|---|---|---|---|
| P001 | T01 | open | Q009 partial |
| P002 | T01 | open | Q028 still |
| P003 | T01 | minor | The copy-time constants are overfitted to the fixture traces: a fresh 30 GiB install's root copy rises on its plain replay, and slowed replays of both fresh traces break other selftest bounds |
| P004 | T01 | polish | Terminal installer's Applications paragraph splits the command across lines at 80 columns |
| P005 | T01 | polish | The Packs step text 'boxed for login' is cryptic |
| P006 | T03 | polish | Pending notice with both boxed selections and groups runs on in one paragraph and pushes the command below a scrollbar |
| P007 | T04 | polish | First `kdos-box enter` on a box that was never set up shows nothing for 10+ s, and stopping the box meanwhile prints 'Terminated' |
| P008 | smoke-live | polish | Haruna showed a black video area once (one row of coloured pixels) at 00:00:02 of a 5 s clip; it did not happen again |
| P009 | smoke-installed | polish | First `kdos-box enter` of a new image box shows a blank Konsole titled '~ : kdos-box' for about 15-30 s with no progress text while the box sets itself up |
| P010 | gui-install | open | Q002 W007 Slot B rise / ~1.4x near end partial |
| P011 | gui-install | open | Q006 W038 Slot B: no rise, no hold, near-end 1.4x partial |
| P012 | gui-install | open | Q009 copy time left low for the first third (unbounded low side) partial |
| P013 | gui-install | open | Q021 W038 (V019 U005) Slot B accuracy / holds partial |
| P014 | gui-install | open | Older: copy time left reaches 0:00:00 at the end partial |
| P015 | gui-install | open | Older: copy time left vs real time, root and Slot B partial |
| P016 | gui-install | minor | Slot B copy line holds 0:00:07 for three 1 s frames, then drops to 5 and rises to 6. A fresh trace from this ISO fails the selftest's own replay rules (flat on slot-b, slowed 3x low and high, slowed 5x never reaches 0) |
| P017 | gui-install | polish | Pending-groups notice with two groups overflows the popup: the command line is hidden below a scrollbar |
| P018 | gui-install | polish | Summary 'Applications: 1 group, about 8.6 MiB — recorded; the first login offers to build them' (singular count, plural pronoun), and the Packs step's 'boxed for login' is clipped wording |
| P019 | gui-install | polish | Welcome Center's Welcome page logo stays Phosphor green under any accent |
| P020 | binhost | open | Q029 (W056/V050): LibreWolf shows a generic globe icon and 'Unknown author' in Discover; the licence half was fixed earlier still |
| P021 | binhost | polish | `kdos update check` after an http:// binhost fetch reports only 'could not be downloaded' and leaves out the https-only reason that fetch printed |

## Ninth fix round

| Unit | Severity | Items | Fix |
|---|---|---|---|
| U01 | minor | P002, P003, P016, P011, P013, P010, P001, P012, P015, P014, P005 | Installer copy time left: one countdown rule (an estimate before the switch point, then a figure that never rises and counts down at 0.62 to 2 times the clock, ticked between rsync lines), replayed on six recorded traces; the Packs step in plain words |
| U02 | polish | P004, P018 | Installer front ends: the terminal Applications page puts the pending command on its own line, and the Summary agrees in number |
| U03 | polish | P006, P017 | kdos-notice: with boxed selections and native groups both waiting, the command stays inside the popup without scrolling |
| U04 | polish | P007, P009 | kdos-box enter on a box not yet set up: the window opens at once, a setting-up line, a 120 s wait, and a quiet end when the box is stopped |
| U05 | polish | P021 | kdos update check after an http:// binhost keeps the https-only reason |

Skipped:

- P008: Rig artefact. The black Haruna frame did not come back when the clip was re-run in the same image at 4, 11 and 15 s, and the lane itself judges it a frame caught mid-swap or during the first decode on llvmpipe. Nothing confirmed points at code.
- P019: Product decision by the user: the Welcome page logo is deliberately not tinted with the accent.
- P020: Needs a new published artefact. LibreWolf's icon before install only changes when the binhost's appstream catalogue is republished with a cached or remote icon, and the user ruled it out this round.

Not proven on the rig by the fixers (the re-test does it):

- U01: P001/P012/P015 low side: the 0.75x low bound is given up on slot B, as the brief allows. Plain floor is 0.6 (two30 slot-b reads 0.647, fresh30c slot-b 0.671, unthinned e2e9 slot-b 0.687); every root copy is at least 0.78. Slowed floor is 0.58 (lowest reading 0.62). A time shown that never rises keeps slot B's low early estimate (0.65 to 0.8 of the real time).
- U01: Remaining fragility: START works only from 0.1125 to 0.12. At 0.1, slowed fresh30 slot-b stands at 0:00:01 for 46 s; at 0.1375, the e2e7 slow start reads 25 s high. SLOW passes only at 0.62: 0.6 is 0.2 s over the high bound on fresh30 slot-b 2x, and 0.65 reads 0.41 on two30 slot-b 5x. The core cannot tell a low early estimate (slow disk) from a high one (slow start). Fold B's pick, fitted without e2e7 and two30, fails two30 slot-b 5x low at 0.426.
- U01: Synthetic trees whose second kind of file first appears mid-copy still read low through the tail. The new bounds cap how fast the reading falls and how long it stands at 0:00:01, but a rule that never rises cannot follow a cost that appears late.
- U01: P014: the last note shows 0:00:00 and every note before it is at least 1 s. Whether a 1 s GUI frame catches that last note depends on the GUI's frame timing, which the core does not control.
- U01: testing/selftest.sh stops at the kdos-notice pending-notice case (three FAILs on group titles), the same as for the first fixer. Those files belong to another agent. Every kinstall and copy section before it passes.
- U02: No rig proof was run (no VMs allowed). The 80x33 layout was checked only with the offscreen drawing of the real page code, not on tty2. The next rig pass should still photograph the tty2 Applications page with no binary host.
- U03: Not proven on the rig: the photographs in the plan still need a VM run.
- U03: Three groups of which two have titles that wrap (two lines each) need 5 title lines, so the command falls just below the scroll. Fixing that needs a shorter multi-group header ("N KDOS application groups chosen during the install are not installed yet:" takes 2 lines). /ports/groups has only two titles over about 43 characters, so it happens only when both are chosen together with a third group and a boxed selection.
- U04: The rig proofs were not run, because no VMs this round: the konsole -e shot within 3 s, a bare enter opening its window at once, and no 'Terminated' after kdos-box stop on the installed machine.
- U04: A SIGTERM that lands while `podman logs` is running is acted on only once that call returns. This is unmeasured on real podman 6.1.2.
- U05: The full testing/selftest.sh did not complete on either run, and both stops were outside this unit. Run 1 stopped at the session-wrapper case near line 4869 with a shell parse error ('w/log ... No such file or directory', rc 127). Run 2 stopped in 'kpkgadd lists what it extracted' with "ed: unrecognized option '--root'". Both look like other agents' edits in progress. On run 1 every update section passed before the stop, the extended http/HTTP hint case among them.
- U05: This host has no user namespaces, so the root treefetch section (the fetch, then the check assertion) is skipped here. It was proved by hand in kdos-devdeps in the first fix.

### Re-test of the ninth round

New ISO, a fresh base install, GUI installs onto 12 GiB and 30 GiB disks, the terminal installer at 80 columns, and the live and installed smoke paths. Result: 34 fixed, 4 partial, 3 still, 5 regressed. The Welcome logo and LibreWolf's icon before install stay out of scope, and the black Haruna frame did not come back.

- **O001** (U01, open) P003 still
- **O002** (U01, open) P016 partial
- **O003** (U01, open) P011 regressed
- **O004** (U01, open) P013 regressed
- **O005** (U01, open) P010 regressed
- **O006** (U01, open) P001 partial
- **O007** (U01, open) P015 regressed
- **O008** (U01, minor) Slot B copy time left reads 1.2-1.46x the real time through the second half of a 30 GiB GUI install, and a fresh trace fails the selftest replay's high bound
- **O009** (U01, minor) Root copy time left jumps 39 to 48 (+9 s) at about 12% of the copy after holding 39 for three frames
- **O010** (U02, polish) TUI Applications list: group names are cut with no gap and run into the count column at 80 columns
- **O011** (U02, minor) TUI pages cut values at the right edge at 80 columns (Summary 'root', Accounts login and root lines)
- **O012** (U02, polish) TUI Summary 'native groups' row shows the group id, not its title
- **O013** (U03, polish) In the combined pending notice, the trailing Discover line is cut at the popup's scroll edge or pushed below it, and '1 more selection' has no clear referent
- **O014** (U04, open) P007 regressed
- **O015** (U04, major) kdos-box enter can never enter a box whose `podman logs` output is over 64 KiB before container_setup_done: it waits 120 s and then wrongly says the box did not finish setting up
- **O016** (U04, minor) An unclean shutdown can corrupt a box's podman log, so the container_setup_done check never passes again
- **O017** (U04, polish) A bare enter whose window is closed by `kdos-box stop` leaves no line anywhere; the calling shell reports rc=0
- **O018** (U05, polish) `kdos update check` advises the same `fetch --binhost-only` that just failed for a directory binhost with no index, and reports no problem line
- **O019** (U05, polish) A binhost-fetch-failed problem line from an earlier binhost stays after update.conf names a different binhost or none
- **O020** (gui-install, open) P014 (copy time left reaches 0:00:00 at the end) partial
- **O021** (gui-install, minor) Terminal installer clips text at 80 columns on the Layout, Accounts and Summary pages
- **O022** (gui-install, polish) Terminal installer Applications list: long group titles run into the count column with no gap
- **O023** (gui-install, minor) Terminal installer Time page: tabbing through the zone list silently selects its first row (Europe/Andorra)
- **O024** (gui-install, polish) Root copy line: a visible early rise and a three-frame hold before 20% of the copy
- **O025** (gui-install, polish) Pending notice with three groups and a boxed selection still gets a scrollbar; the end of the Discover sentence is below it
- **O026** (gui-install, polish) First copy-line note reads 'about --:--:-- left'
- **O027** (binhost, polish) The binhost PACKAGES lists a superseded docbook-xsl-1.79.2-4 (empty E:, no N:) that kdos-update.idx does not publish

## Tenth fix round

- **U01** (major; O015, O014, O016, O017) kdos-box readiness: look for container_setup_done in this start's whole log, so a box with a long or damaged podman log can be entered, and report a stopped window enter in the calling shell.
- **U02** (minor; O008, O001, O002, O003, O004, O005, O007, O006, O009, O024, O026) kinstall copy time left: a countdown that falls faster than the clock while it is above the estimate, 'estimating' until the estimate settles, validated leave-one-out on eight traces.
- **U03** (minor; O011, O021, O023, O010, O022, O012) Terminal installer at 80 columns: wrap or fit every label and value, keep a gap before the Applications counts, name groups by title, and do not take a zone on focus.
- **U04** (polish; O013, O025) Pending notice with groups and boxed selections: a one-line header and a short boxed-app line that fit with the command inside the popup.
- **U05** (polish; O018, O019) kdos update check: use the last binhost result only for the binhost named now, and do not re-advise a fetch that found nothing there.
- **U06** (polish; O027) Binhost: move superseded package files out of build/binhost before the final index, so PACKAGES names only what kdos-update.idx publishes.

Not fully met: on the fresh 30 GiB trace, slot B reads up to 1.32x the real time left (the limit is 1.25x). The selftest allows up to 1.33x. The low floor stays at 0.6x. O020 (a 1 s frame missing the last 0.6 s of the copy) is accepted as frame cadence.

### Re-test of the tenth round

New ISO, a fresh base install, GUI installs onto 12 GiB and 30 GiB disks, the terminal installer at 80 columns, the live and installed smoke paths, and the binhost served to a guest. Result: 40 fixed, 15 partial, 2 still, 2 untestable.

The box readiness defect is fixed: boxes whose logs are 96-154 KiB enter in about 0.6 s, after a restart and after an unclean reboot. On the rig, the copy time left stays within the bounds: slot B reads at most 1.18x the real time, and root at least 0.70x, with no rise and no hold. One earlier trace still replays at up to 1.33x on slot B. The estimator is accepted as it is.

- **N001** (U01, polish) kdos-box enter prints 'setting up <box> for its first use; this can take a minute' on every restart and every enter after a reboot, not only on a first setup
- **N002** (U03, minor) TUI System page (7) cuts text at the right edge at 80 columns
- **N003** (U03, minor) TUI Time page (3) cuts the TZ string and local time values at 80 columns
- **N004** (U03, polish) TUI Welcome page cuts the processor name at 80 columns
- **N005** (U03, polish) TUI Summary says 'swap file' on the A/B layout, where Layout says 'Swap volume'
- **N006** (U03, polish) TUI scroll-bar column shows a stray cell at screen row 3
- **N007** (U04, minor) Groups-only pending notice with many groups puts the command below the popup's scroll; the stated reason for this behaviour is wrong
- **N008** (U05, minor) Discover's Updates warning for binhost-absent names the release channel URL, not the binhost that was fetched, so the sentence contradicts itself
- **N009** (U05, polish) The KdosBackend.cpp comment edited in this round leaves a line well past the file's wrap width
- **N010** (smoke-installed, polish) kdos-box enter of any stopped box says 'setting up <box> for its first use' on every start, not only the first
- **N011** (gui-install, minor) Terminal installer still cuts text at the right edge at 80 columns on the Welcome, Time and System pages
- **N012** (gui-install, minor) On a Linux VT, the terminal installer's advertised 'Alt+</> page' switches virtual console instead of paging
- **N013** (gui-install, polish) Password strength reads 'strong' in the terminal installer and 'Fair' in the GUI for the same password
- **N014** (binhost, minor) The Discover Updates warning for a binhost result names the release channel, not the binhost: 'No package index is published at https://github.com/kunaldawn/kdos/releases/latest/download. (no binhost is published at file:///root/bh)'
- **N015** (binhost, polish) On a tree newer than the published release, the hidden tree 'fetch-failed … older' problem masks binhost-absent and binhost-fetch-failed: `check` reports only fetch-failed, and Discover says 'Up to date' with no warning
- **N016** (binhost, polish) With a binhost index from another base still on disk, `check` reports nothing about the binhost that update.conf now names
- **N017** (binhost, polish) A port installed from the binhost through Discover then reads 'Part of the KDOS system: it is updated with the system and is not removed on its own', and has no Remove button

## Eleventh fix round

- **U01** (minor; N002, N003, N004, N011, N005, N006, N012, N013) Terminal installer: every page whole at 80 columns, paging keys that reach a VT, one strength rule, the A/B swap word.
- **U02** (minor; N014, N008, N009, N015, N016, N017) Binhost problems in Discover name the binhost, are not masked by an ahead-of-release tree, and a port installed on request can be removed.
- **U03** (minor; N007) Groups-only pending notice keeps its command above the popup's scroll.
- **U04** (polish; N001, N010) kdos-box enter says 'first use' only on a box's first setup.

### Re-test of the eleventh round

New ISO, a fresh base install, the terminal installer at 80x33 on tty2, a GUI install, the live and installed smoke paths, and the binhost served to a guest. Result: 39 fixed, 1 partial, 1 untestable, no regression.

- **M001** (U01, minor) Terminal installer: F1 (key help) does nothing on a Linux VT and types 'A' into a focused field
- **M002** (U01, minor) Terminal installer: a tab in an OUTPUT/LOG line throws text past column 80 and leaves garbage in the sidebar on a VT
- **M003** (U01, polish) Terminal installer: the status line cuts a refusal message at the Back button
- **M004** (U01, polish) Terminal installer System page: all eight accent swatches show the same three colours
- **M005** (U01, polish) Terminal installer Done page reads 'took 1 minutes 8 seconds'
- **M006** (U02, polish) Removing a host port in Discover asks no confirmation question: the Remove button goes straight to the polkit password prompt
- **M007** (U02, polish) After a Discover removal, the LibreWolf page labels its button 'Install from KDOS system' and drops the 'From KDOS system' source picker; before the first install it was a plain 'Install' with the picker
- **M008** (U02, polish) The binhost banner and check line each repeat the binhost URL ('…published at file:///root/bh. (no binhost is published at file:///root/bh)')
- **M009** (U03, polish) Groups-only pending header '7 KDOS application groups wait.' is terse beside the plan's fuller sentence
- **M010** (U04, polish) Unwrapped 120- and 126-column comment lines added by U04's re-fix in box.c and box_cmd.c
- **M011** (U04, polish) The enter timeout message's seconds understate the real wait (known, not fixed by U04)
- **M012** (smoke-live, polish) KWalletManager, which is now on the base image, opens on the live medium to a modal 'The KDE Wallet system is not enabled. Do you want me to enable it? If not, the KWalletManager will quit…' instead of its window
- **M013** (smoke-installed, polish) Two U04 comment lines were not reflowed and run to 120-126 columns in files wrapped at 80
- **M014** (gui-install, minor) N011 Terminal installer cuts text at the right edge at 80 co partial
- **M015** (gui-install, minor) TUI Install page OUTPUT pane cuts command lines at column 80 with no ellipsis
- **M016** (gui-install, polish) TUI Install OUTPUT shows 'mount3/dev/kdos/home': a stray '3' replaces a space, on every frame
- **M017** (gui-install, polish) TUI Done page says 'took 1 minutes 11 seconds'
- **M018** (gui-install, polish) TUI Install page keeps counting 'elapsed' and keeps the footer 'writing to disk' after the install has finished
- **M019** (gui-install, polish) TUI warnings are run-together lowercase paragraphs with backticks; the GUI shows the same warnings as capitalised bullets
- **M020** (gui-install, polish) TUI key-hint bar and inactive buttons are purple text on teal, hard to read
- **M021** (gui-install, polish) TUI System page accent swatches are the same three colours for all eight accents
- **M022** (gui-install, polish) System Versions Help on a one-slot machine opens the generic two-slot description
- **M023** (gui-install, polish) The tty login fetch panel is cut at column 80 with no ellipsis
- **M024** (binhost, polish) A group install records already-installed base-image members in world, so Discover then calls them 'Installed on request' and offers Remove
- **M025** (binhost, polish) The binhost-absent banner says the same thing twice
- **M026** (binhost, polish) `kdos update groups` with no index prints its 'no binhost index here, so no sizes:' line after the list, but kdos-command.md says it is printed first

## Twelfth fix round

- **U01** (minor; M001, M002, M014, M015, M016, M020) libktui on a Linux VT: console F-keys, control characters in drawn text, and the installer's palette under sudo.
- **U02** (polish; M024, M026, M022) kdos update: a group install records only what it installed, the groups line comes first, the absent-binhost check line says it once, System Versions help for one slot.
- **U03** (polish; M006, M007, M008, M025) Discover: ask before removing a port, keep a port's two sources while it is installed, say the absent binhost once.
- **U04** (polish; M003, M004, M021, M005, M017, M018, M019) Terminal installer pages: the status line fits, true accent swatches, one frozen duration, readable warnings, kinstall keeps tabs as spaces.
- **U05** (polish; M011, M010, M013) kdos-box enter: a clocked wait, and two rewrapped comments.
- M009 not fixed: The lane itself calls it a wording note and 'not a defect': the summary above already carries the meaning, and the terse sentence was kept on purpose so that the command stays on line 3.
- M012 not fixed: This is deliberate and documented.
- M023 not fixed: This is a rig artefact.

## Release audit

Fourteen end-user lanes, from scratch, on the ISO of the eleventh round: boot media (dd, Ventoy, Secure Boot), graphical and terminal installs, legacy BIOS and awkward disks, updates and slots, packages, building from source, boxes, the live and installed desktop, integration, accessibility and languages, documentation and release artefacts. Each lane's findings were checked against their evidence by a second agent.

- **A001** (L03-install-matrix, blocker) LUKS passphrase prompt at boot always uses the US keymap: a passphrase set on a German layout can never be typed and the machine cannot boot
- **A002** (L03-install-matrix, blocker) The installer's keyboard layout never reaches any account's Plasma session: the skel fcitx5 profile writes kxkbrc LayoutList=us at first login, so the lock screen rejects the real password
- **A003** (L03-install-matrix, blocker) Choosing a layout in the installer does not change the live session's layout, and the page has no test field, so everything typed afterwards is US-mapped
- **A004** (L02-media-ventoy, major) A Ventoy stick cannot start KDOS in any mode, under UEFI or BIOS: Limine and the kernel load, then the boot stops at a rescue shell with 'Failed to find KDOS installation media.' Nothing in the docs warns about this.
- **A005** (L03-install-matrix, major) Console keymap for German is kbd's 7-bit 'de' map: on a text console ü ö ä ß type @ [ ] \
- **A006** (L03-install-matrix, major) udisksd crash-loops (SIGSEGV) on a machine with a SATA disk, live and installed; Solid loses every device
- **A007** (L04-install-bios-disks, major) On an 8 GiB disk, taking the installer's own swap advice gives a system whose root is full at first login (12 MiB free, writes fail with ENOSPC); 12 GiB with the default swap leaves 1.9 GiB
- **A008** (L05-install-tui-rescue, major) Leaving the login screen for tty2 (the documented recovery console) kills the SDDM greeter; Ctrl+Alt+F1 / Alt+F1 back gives a permanent black screen
- **A009** (L06-updates-slots, major) An A/B `kdos update apply` unmounts /dev/pts, /dev/shm, /sys/fs/cgroup and efivarfs from the running system. New terminals open dead, and the one-boot BootNext trial turns into a three-boot trial led by the menu
- **A010** (L07-packages, major) GIMP crashes (SIGSEGV in its 'async' thread) whenever an image with an embedded colour profile is opened, whether the user picks Keep or Convert, and every open image is lost
- **A011** (L07-packages, major) Prebuilt Falkon and FreeCAD pull in the whole clang toolchain (369 MB download, 2 GB unpacked) through python3-pyside6, and the FreeCAD install then fails on a 30 GiB machine's slot
- **A012** (L08-build-source, major) A source install builds every missing port deep in the dependency closure (rust, clang, openmp, doxygen, spirv-*) that the dry run and Discover never mention; a small KDE app fills the root slot to 100%
- **A013** (L08-build-source, major) `kdos update install build-tools` installs no C compiler: gcc stays without its compiler, and libc and kernel headers are missing
- **A014** (L09-boxes, major) Discover deletes a development box (container and profile) with one click and asks nothing
- **A015** (L11-desktop-installed, major) Keyboard layouts never survive a login: fcitx5 rewrites ~/.config/kxkbrc to 'us' at every session start, so layouts added in System Settings → Keyboard are lost and a non-US system or installer layout never reaches the Plasma session
- **A016** (L11-desktop-installed, major) Two logged-in users (Switch User) start a ksystemstats/NetworkManager feedback loop: about 130 NM audit lines per second, syslog rotated every ~10 s, about 60% of a CPU burned
- **A017** (L11-desktop-installed, major) A discovered network printer cannot be added from System Settings → Printers: 'Unable to connect to <host>.local:8631: Name does not resolve' (.local names do not resolve through musl)
- **A018** (L11-desktop-installed, major) cups-browsed crash-loops (SIGSEGV every 5 s, a stack overflow in libcupsfilters) as soon as CUPS runs and an IPP printer is on the network, so no printer is ever added automatically
- **A019** (L13-a11y-i18n, major) Graphical installer Applications page: no group checkbox can be reached from the keyboard or a screen reader
- **A020** (L13-a11y-i18n, major) Hindi, Bengali, Thai, Tamil and other Indic/SE-Asian text shows as boxes; the docs say the base fonts cover alphabetic scripts
- **A021** (L14-docs-release, major) A v0.2.1 release would call itself "KDOS 0.2" everywhere: the version is hard-coded and nothing in the release procedure bumps or checks it
- **A022** (L14-docs-release, major) Machines installed from the public v0.2 ISO would roll back a v0.2.1 slot image (F02 is still in their kdos-tools, and the image has no /boot/efi), and nothing tells them
- **A023** (L01-media-dd, minor) ISO Image Writer offers the running live stick as a target, often selects it by default, and writing to it fails with a cryptic error
- **A024** (L02-media-ventoy, minor) A stick made by copying the ISO's files (Rufus's default 'ISO Image mode', or any copy to a FAT32 stick) boots Limine and the kernel, then fails at BOOT MEDIA, even though it is labelled KDOS_LIVE and holds system.sfs
- **A025** (L03-install-matrix, minor) Alt+Q (the Quit button's mnemonic) closes the installer at once, with no question, discarding every answer, even from inside a text field
- **A026** (L03-install-matrix, minor) The Done page tells a non-administrator to run `sudo kdos firmware sync`, which that account cannot do
- **A027** (L04-install-bios-disks, minor) The erase warnings never name what is on the disk: an NTFS 'Windows' partition and an ext4 'linuxhome' partition are erased behind a path-only banner
- **A028** (L04-install-bios-disks, minor) Reuse plan on a disk with no EFI system partition is a silent dead end: the ESP list opens empty and the page only says 'Choose an EFI system partition and a root.'
- **A029** (L04-install-bios-disks, minor) The book does not explain how to install KDOS beside another operating system (there is no resize or alongside option), and known-gaps does not list this
- **A030** (L05-install-tui-rescue, minor) Terminal installer: the Summary page puts the focus on BEGIN INSTALL, so one Enter on arrival erases the disk with no confirmation
- **A031** (L05-install-tui-rescue, minor) Terminal installer confirm dialogs default to the destructive answer: Ctrl+Q/Ctrl+C then Enter during an install stops it and leaves the target half written
- **A032** (L05-install-tui-rescue, minor) Following the Done page's 'Remove the boot medium before restarting' kills the live system: Reboot now dies with 'Bus error', sudo reboot segfaults, Ctrl+Alt+Del cannot run shutdown
- **A033** (L05-install-tui-rescue, minor) Terminal installer: Shift+Tab moves the focus FORWARD on a Linux virtual console
- **A034** (L05-install-tui-rescue, minor) Terminal installer: prefilled text fields put the caret at position 0, so typing a name gives 'xkdos' / 'ykdos'
- **A035** (L05-install-tui-rescue, minor) installation.md tells the user to switch to tty4 and log in there, but tty4 has no login prompt
- **A036** (L05-install-tui-rescue, minor) Rescue shell on an installed machine has no controlling terminal: no job control, Ctrl+C cannot stop a command
- **A037** (L06-updates-slots, minor) An A/B apply installs build-only dependencies into the new slot: 25 packages (91 MB) for a world set that `kdos update install` filled with 4
- **A038** (L06-updates-slots, minor) On a machine without autologin, the login screen confirms a trial a minute after boot, before anyone logs in, so a release with a broken desktop is kept
- **A039** (L06-updates-slots, minor) After going online again, Refresh on an A/B machine does not retry for an hour, and Discover keeps saying "no network was found"
- **A040** (L06-updates-slots, minor) Picking the confirmed slot in the boot menu, which the menu says abandons the update, is then reported as a failed update
- **A041** (L06-updates-slots, minor) Switching slots (Start This Version Next Time, `kdos-bootctl try`) boots a system without the apps installed since the last update, and nothing warns about it
- **A042** (L07-packages, minor) A prebuilt Chromium also installs the Rust toolchain, Crubit, esbuild, typescript-go and compiler-rt: 182 MB downloaded and about 830 MB unpacked that the browser never runs
- **A043** (L07-packages, minor) Opening Discover fetches from the network: the published tree from GitHub, the binhost index and the image manifest. The book says opening Discover reaches no server and the fetch happens only on Refresh
- **A044** (L07-packages, minor) gtk-doc does not work once installed: FACETS=run drops usr/share/gtk-doc/, which holds gtk-doc's own Python package
- **A045** (L07-packages, minor) GTK 4 programs show missing-icon placeholders: Celluloid's seek-back, seek-forward and sidebar buttons draw image-missing
- **A046** (L08-build-source, minor) A build that runs out of disk names no cause, leaves its work directory (0.9-1.7 GB) and keeps the root full; no build-space check; no documented cleanup
- **A047** (L08-build-source, minor) Cancelling a build in Discover leaves it compiling for up to a minute with the button greyed out on 'Downloading'; packages it built stay installed and cannot be removed with kdos update remove
- **A048** (L08-build-source, minor) No package installed after the image reaches the man database: makewhatis is run on //usr/share/man and refuses each page
- **A049** (L08-build-source, minor) `kdos update install --dev qt6-qtbase` is not enough to build a Qt6 Widgets program, and nothing restores a port's development closure
- **A050** (L09-boxes, minor) One kdos-fetch-app run without a network leaves kdos-debian without sudo, and every later run fails with 'sudo: not found'
- **A051** (L09-boxes, minor) A boxed app shim started in the background (`zathura file.pdf &`) stops on SIGTTOU and never opens a window
- **A052** (L09-boxes, minor) Discover's boxed install while offline runs about 2 min as 'Downloading', then shows a generic 'Update Issue' that does not mention the network
- **A053** (L09-boxes, minor) An app exported with `kdos-box export` has a blank icon in Kickoff, the task manager and the Boxes page
- **A054** (L09-boxes, minor) A box made with `kdos-box create` gets no title-bar colour rule, although its profile says it has one; rules for removed boxes stay behind
- **A055** (L09-boxes, minor) After Remove on the Boxes page, a dev box shows a false promise ('created again the next time its application starts') and a Start button that always fails
- **A056** (L09-boxes, minor) [out of lane] At 150% display scale on 1920x1080, the panel's task manager collapses to dots
- **A057** (L10-desktop-live, minor) Firefox ESR looks up the machine's region by IP on first start, though the tour says 'Nothing on KDOS phones home'
- **A058** (L10-desktop-live, minor) Typing 'terminal' in Kickoff/KRunner puts tmux ('Terminal Multiplexer') first, above Konsole
- **A059** (L11-desktop-installed, minor) A new account's first login silently re-themes the whole machine (boot menu, splash, console, login screen) back to the install-time accent
- **A060** (L11-desktop-installed, minor) Health page and `kdos doctor` show a false red failure ('this shell's bus is not the session's') whenever System Settings or Konsole was started from KRunner
- **A061** (L11-desktop-installed, minor) Printing is off on an installed machine and the Printers page dead-ends, while cups-browsed and ipp-usb run with no cupsd
- **A062** (L12-integration, minor) On the base, an office document (ODT, DOCX) opens in Ark as a zip archive, not in the chooser the docs promise
- **A063** (L12-integration, minor) A mailto: link gives an error dialog 'Could not read file mailto:…' instead of the chooser
- **A064** (L12-integration, minor) Hindi, Thai and CJK text shows as boxes on the base (including example.com in Firefox); no group installs noto-fonts-extra, and the docs say the fonts ship
- **A065** (L12-integration, minor) `.local` (mDNS) host names do not resolve for ping, ssh or browsers, and this is undocumented
- **A066** (L13-a11y-i18n, minor) After lang-ja, Kate draws kanji with Korean glyph forms (Han unification falls to Noto Sans CJK KR)
- **A067** (L13-a11y-i18n, minor) Region & Language offers formats for only 17 regions: no Japanese, Chinese, Korean, Arabic, Hebrew, Polish, Turkish or Greek
- **A068** (L13-a11y-i18n, minor) Applying any format in Region & Language shows a red error telling the user to generate locales from 'your distribution's manual'
- **A069** (L13-a11y-i18n, minor) KDOS Look settings: Orca announces the accent in use as 'not selected'
- **A070** (L13-a11y-i18n, minor) Welcome tour's Screen reader switch stays off after Meta+Alt+S starts Orca while the page is shown
- **A071** (L13-a11y-i18n, minor) Installer Look page: focus ring on accent cards and desk rows is about 2.5:1, below the 3:1 floor KDOS claims
- **A072** (L13-a11y-i18n, minor) Installer: why Next is disabled is never announced to a screen-reader user
- **A073** (L14-docs-release, minor) Release notes are never shown on the live medium or after a fresh install, but getting-started says What's New opens and describes both cases
- **A074** (L14-docs-release, minor) 78 shipped packages have no licence text, among them GPL/LGPL/MPL/Apache binaries and every KDOS package; KDOS's own MIT LICENSE is nowhere on the image
- **A075** (L14-docs-release, minor) README and book still show the retired system: stale boot-splash, tty-banner and hero screenshots (F30/F152 still open)
- **A076** (L01-media-dd, polish) ISO Image Writer names drives by raw D-Bus object path; two sticks look identical and the confirmation does not say which drive
- **A077** (L01-media-dd, polish) Writing KDOS's own ISO with KDOS's own image writer shows 'Could not find the key to verify the ISO image' with a prohibition icon, and the user guide does not warn
- **A078** (L01-media-dd, polish) `kdos persist` on a session started from a CD/DVD says a store can be made on the boot medium, which `kdos persist create` then refuses
- **A079** (L01-media-dd, polish) Installer Disk page calls the partitions of a KDOS live stick 'empty', including the 1.7 GiB that holds the system image
- **A080** (L02-media-ventoy, polish) With Secure Boot on, the firmware refuses the medium itself ('Access Denied'), but installation.md and known-gaps.md say the installer reports Secure Boot on its first page
- **A081** (L02-media-ventoy, polish) The rescue screen wraps its last line at 80 columns, leaving a lone '.', and points a laptop user at a 'serial console' they do not have
- **A082** (L02-media-ventoy, polish) When the medium is missing, the boot log never names a device it tried if the stick appears after the first 100 ms pass, and dmesg fills with repeated ISOFS errors
- **A083** (L02-media-ventoy, polish) The installer's Disk page calls the boot stick's ISO partition 'empty': 'Holds: empty 32.0 KiB, vfat 2.0 MiB, empty 1.7 GiB'
- **A084** (L02-media-ventoy, polish) The GUI installer's Welcome page lets you continue when no disk can take the install ('0 of 1'); it stops only when no disk exists at all
- **A085** (L02-media-ventoy, polish) `kinstall --dump probe` calls the logical CPU count 'cores', but both installer faces and the docs call it threads
- **A086** (L03-install-matrix, polish) Docs never say how a non-administrator account becomes one, and administration.md says such an account 'cannot administer the machine' although root's password works in every polkit prompt
- **A087** (L03-install-matrix, polish) Close-during-install dialog says 'Closing this window does not stop it' but offers no way to close; an install cannot be aborted from the window
- **A088** (L03-install-matrix, polish) Disk row shows the raw blkid type 'LVM2_member' for a partition
- **A089** (L03-install-matrix, polish) Install size is reported three ways: 5.1 GiB on Welcome, 'of 4.4 GiB' during the copy, '4.7 GiB base' in known-gaps
- **A090** (L03-install-matrix, polish) Hostname refusal says 'letters' but rejects ü
- **A091** (L03-install-matrix, polish) Summary does not say what kind of swap will be made
- **A092** (L03-install-matrix, polish) First-boot wizard (OEM) is generic KDE and partly confusing
- **A093** (L03-install-matrix, polish) Initramfs rescue shell shows shell job-control noise
- **A094** (L03-install-matrix, polish) With one disk, the Disk page selects nothing; on the reuse layout the only ESP is not preselected
- **A095** (L04-install-bios-disks, polish) Reuse plan's root list offers the Windows NTFS partition for formatting without its label
- **A096** (L04-install-bios-disks, polish) Booting slot B from the menu: System Versions says 'Running now · Installed, not in use', and kdos-bootctl status calls the running slot 'inactive' and 'confirmed good'
- **A097** (L04-install-bios-disks, polish) SATA disk models are cut to 16 characters in the installer ('Kingston-SA400S3', 'WDC-WD10EZEX-PHO') although udev has the full name
- **A098** (L04-install-bios-disks, polish) On a legacy-BIOS machine the Summary still promises 'the UEFI boot entry' in the Bootloader step
- **A099** (L04-install-bios-disks, polish) Book says no whole installed system has been booted on legacy BIOS; this audit booted one, both slots
- **A100** (L04-install-bios-disks, polish) Offline install from a medium that names a binhost: the Summary promises the groups are 'downloaded during the install', and the Done warning sends the user to a log instead of saying there is no network
- **A101** (L04-install-bios-disks, polish) `kdos update install --pending` with no binhost repeats the same two-line refusal for every member of the group
- **A102** (L04-install-bios-disks, polish) No CJK font on the base image: Chinese text on web pages renders as hex boxes in Firefox ESR (off-lane)
- **A103** (L04-install-bios-disks, polish) Disk page: the copy total differs from the stated install size (Welcome '5.1 GiB', progress 'of 4.4 GiB'), and the swap choice reads 'A file, or a volume with two slots' after slots are unticked
- **A104** (L05-install-tui-rescue, polish) A terminal --unattended run that fails or is refused prints no reason once it exits (and with the default reboot = 1 reboots)
- **A105** (L05-install-tui-rescue, polish) A failed preflight line keeps its positive wording, and the install is allowed to wipe the disk and then fail
- **A106** (L05-install-tui-rescue, polish) A new account's console uses bone until its first Plasma login, whatever accent the installer chose
- **A107** (L05-install-tui-rescue, polish) Developer wording on the console and in the installer
- **A108** (L05-install-tui-rescue, polish) TUI time-zone list is in country-code order, and empty filters say nothing
- **A109** (L05-install-tui-rescue, polish) On a 25-row console the login banner scrolls its own top off the screen
- **A110** (L05-install-tui-rescue, polish) kdos-bootctl status in the emergency shell says a two-slot machine 'has one root and no rollback'
- **A111** (L05-install-tui-rescue, polish) The failed-step Shell gives no hint how to get back to the installer
- **A112** (L06-updates-slots, polish) Version names change form after an update: "0.2+4dc1f7f82-dirty" for the installed slot, "4dc1f7f82" for the slot an apply filled
- **A113** (L06-updates-slots, polish) Boot menu: the other slot is a bare lowercase "KDOS (slot a)" with no version, and the comments use "root" and call a manual switch-back "the update"
- **A114** (L06-updates-slots, polish) Discover's no-network banner says "no network was found" when the network is up and only the server refused, and it repeats the URL in nested brackets (the M008/M025 repetition, here for the no-network codes)
- **A115** (L06-updates-slots, polish) With a key that does not verify, `check` and `install` advise running the same fetch that just failed for that reason
- **A116** (L06-updates-slots, polish) Every slot apply prints 79 "unowned ./usr/…" lines from the slot check, which read like errors
- **A117** (L06-updates-slots, polish) `kdos-bootctl status` as a user prints no status, and on a one-slot install its text and JSON differ from kdos-command.md
- **A118** (L06-updates-slots, polish) After an apply, the "Restart is required" notification offers "Update and Restart" and "Update and Shut Down" for an update that is already installed
- **A119** (L06-updates-slots, polish) RELEASE notes say `kdos update check` says whether a newer release is published, but check only reads what the last fetch left, and on a machine that never fetched it reports 0 behind
- **A120** (L06-updates-slots, polish) An interrupted update leaves the rollback slot "Incomplete" with "The next update writes it again", but nothing offers to refill it when no newer release exists
- **A121** (L07-packages, polish) Space used by installs never comes back: a removal leaves every dependency and every downloaded package, and nothing lists or removes them
- **A122** (L07-packages, polish) GIMP, Inkscape and every Qt 5 program open their own file dialog, not KDE's; the book says 'Open and Save dialogs are KDE's in nearly every application'
- **A123** (L07-packages, polish) Discover search ranks the wrong things first for the words users type: 'browser', 'chrome', 'vscode', 'mail'
- **A124** (L07-packages, polish) The terminal mail client aerc appears in Discover and Kickoff as 'Mail', with 'Unknown author' and nothing to say it is a terminal program
- **A125** (L07-packages, polish) Chinese, Japanese and Korean text shows as hex boxes in the browser on a stock install
- **A126** (L07-packages, polish) A cut download says only 'kdos update: binhost/<file> could not be downloaded'; the retry downloads the cut file again from byte 0
- **A127** (L07-packages, polish) Discover says 'Waiting' while a slow download is already running, until the first package file completes
- **A128** (L07-packages, polish) The native groups (office, graphics, internet …) cannot be installed from Discover; its 'Sets' are the boxed catalogue's sets with near-identical names
- **A129** (L07-packages, polish) The install confirmation dialog cuts the app name in its title: 'Install GNU Image Manipulatio…'
- **A130** (L07-packages, polish) Krita's welcome page shows a 'DEV BUILD' warning on the release
- **A131** (L08-build-source, polish) kpkg's own '==>' progress lines are block-buffered when stdout is not a terminal, so in a log or pipe they arrive after the output they announce
- **A132** (L08-build-source, polish) A source build's development-file restores trigger 'The desktop was updated — Log out and back in' although no version changed
- **A133** (L08-build-source, polish) Discover's build confirmation counts development-file restores of installed packages as 'installs 230 prebuilt'
- **A134** (L08-build-source, polish) The build-tools step of a source build installs the tools' build-only dependencies (python3-sphinx, poetry-core, hatchling, setuptools-scm, muon, scdoc, …) that the prebuilt build-tools group leaves out
- **A135** (L08-build-source, polish) The first source build of a 60 KB program downloads 305 MiB, 187 MiB of it the kernel package for its headers, and writes the kernel to the ESP again
- **A136** (L08-build-source, polish) A --dev restore of breeze-icons prints 116 'Skipping … (structure mismatch with host //usr/share/icons/…)' lines
- **A137** (L08-build-source, polish) With no binhost index, a fully-from-source install refuses without naming the step that fixes it
- **A138** (L09-boxes, polish) Removing boxes leaves their pulled distro images (about 1.4 GB here), and neither a command nor the docs say how to reclaim them; DISK shows 0B for every store box and dev box
- **A139** (L09-boxes, polish) `kdos-box export` on a box that does not exist writes a Start-menu entry and a command anyway
- **A140** (L09-boxes, polish) The Boxes page and `kdos-box list` keep every removed or never-created box as a 'Not created yet' row
- **A141** (L09-boxes, polish) Boxes page dialogs call a dev box by its exported app's name, and their wording is confusing
- **A142** (L09-boxes, polish) The 'profile was written after this box was created — applies once kdos-box remove' warning is wrong for keys that apply at the next launch, and it stays after a Recreate
- **A143** (L09-boxes, polish) kdos-box command-line rough edges
- **A144** (L09-boxes, polish) Boxed GTK apps print module-load warnings into the terminal, and Konsole's tab reads 'podman' while a boxed app runs
- **A145** (L09-boxes, polish) `kdos app remove` prints launcher-generator internals
- **A146** (L09-boxes, polish) No graphical way to open a box's shell except KRunner's 'box enter' verb
- **A147** (L10-desktop-live, polish) Welcome tour 'Keyboard Shortcuts' card says Meta+Shift+S toggles the screen reader; on KDOS it starts a region screenshot
- **A148** (L10-desktop-live, polish) Welcome tour Activities card says to switch activities with 'the Unassigned keyboard shortcut'
- **A149** (L10-desktop-live, polish) On the live medium, the Privacy page's wallet paragraph describes a wallet that asks for a password, but the wallet is switched off
- **A150** (L10-desktop-live, polish) Welcome Center reopened from the launcher on the live medium has no Install KDOS page and no Keep Your Changes page
- **A151** (L10-desktop-live, polish) The session splash restarts its progress bar: the boot splash reaches 97%, then KSplash shows 17–83%, and the text font changes
- **A152** (L10-desktop-live, polish) KRunner and Kickoff show three unrelated 'Software Center: Get …' Debian apps for almost any query, even arithmetic
- **A153** (L10-desktop-live, polish) First-launch windows are cramped on a 1920x1080 screen: Kate's status bar truncates and Dolphin's Places labels are cut
- **A154** (L10-desktop-live, polish) `kdos help` KEYS table: long chords run into the description, and one row shows a raw desktop id
- **A155** (L11-desktop-installed, polish) getting-started.md says What's New opens on a fresh install; the code opens nothing at an account's first login
- **A156** (L11-desktop-installed, polish) The login screen shows a greyed-out 'Hibernate' button; KDOS cannot hibernate, and no page says so
- **A157** (L11-desktop-installed, polish) KRunner's Offline library lists indistinguishable duplicates ('Firewall in KDOS book' twice, 'The firewall' twice)
- **A158** (L11-desktop-installed, polish) The product name has two expansions: 'KD's Homebrew Linux Distro' (boot and session splash) and 'Kool Desktop Operating System' (os-release, kdos help, terminal fetch)
- **A159** (L11-desktop-installed, polish) Default Applications shows 'Calendar: Kate' on the base image
- **A160** (L12-integration, polish) `service <Tab>` completes internal rc scripts (rcS, rcK, killprocs, killpower…) and `service start <Tab>` offers 'start stop'
- **A161** (L12-integration, polish) kpkg, kdos-box, kdos-appbox, kdos-bootctl and kinstall have no tab completion, and `kdos why <Tab>` completes nothing
- **A162** (L12-integration, polish) With CUPS off (the installer's default), cups-browsed and ipp-usb still start and run as root
- **A163** (L12-integration, polish) The docs and comments say xdg-desktop-portal-gtk serves the org.gnome.* Settings keys, but no KDOS machine has it (not on the base, in no group, no dependency)
- **A164** (L12-integration, polish) `man -k service` / `whatis` lists manual pages for systemd units on a system with no systemd
- **A165** (L13-a11y-i18n, polish) Region & Language labels the Czech locale 'ISO C Standard'
- **A166** (L13-a11y-i18n, polish) Light high-contrast accent (Paper): desktop icon labels are white on a near-white wallpaper
- **A167** (L13-a11y-i18n, polish) Input Method → Add Input Method hides Anthy, Pinyin and Hangul by default in KDOS's English session; the docs' steps omit this
- **A168** (L13-a11y-i18n, polish) speech-dispatcher and its eSpeak modules from the previous session keep running after logout
- **A169** (L13-a11y-i18n, polish) `kdos locale` accepts any locale name, e.g. LANG=xx_YY.UTF-8, and exits 0
- **A170** (L13-a11y-i18n, polish) Installer 'Start the install?' dialog opens with nothing focused; the first Tab lands on the destructive Install button
- **A171** (L13-a11y-i18n, polish) Installer Keyboard page: a focused layout list shows no focus indicator
- **A172** (L13-a11y-i18n, polish) Welcome tour: the first Tab on a page focuses something invisible
- **A173** (L14-docs-release, polish) The README's limits leave out gaps an ordinary user meets without warning, chiefly Wi-Fi and graphics on non-Intel/AMD/MediaTek/Realtek hardware in the live session
- **A174** (L14-docs-release, polish) installation.md says a whole installed system has never booted on legacy BIOS; it boots to the greeter under SeaBIOS
- **A175** (L14-docs-release, polish) Size figures in the book disagree with this ISO and with each other
- **A176** (L14-docs-release, polish) Nothing before the desktop says which release this is, and the surfaces that do give it in three forms; the splash tagline is a different name
- **A177** (L14-docs-release, polish) `kdos --version` and `kpkg --version` are refused, and `kdos help` does not list `kdos version`
- **A178** (L14-docs-release, polish) README download table: wrong 2 GiB wording, SHA256SUMS said to cover files it does not, and a `<version>` placeholder that does not match the asset names
- **A179** (L14-docs-release, polish) The generated release notes give the disk requirement twice
- **A180** (L14-docs-release, polish) No user-facing pointer to licences or to the source of shipped GPL software
- **A181** (L14-docs-release, polish) GitHub still does not recognise the MIT licence (F157 still open)
- **A182** (L14-docs-release, polish) Installed boot menu under a non-Bone accent: the accent's panel sits as a visible rectangle on the grey, accent-neutral backdrop
