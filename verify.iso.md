Note: the harness blocked writing `/home/kunaldawn/.cache/kdos-portwork/isoverify/report.md`, so the report is returned here.

# ISO verify: build/iso-build/kdos.iso (2026-10-23 15:14, BUILD_ID 06f59201-dirty)

Seven areas were checked: boot, services, integrity, desktop, apps, install and cli. Evidence is in `/home/kunaldawn/.cache/kdos-portwork/isoverify/<area>/`. No repo edits.

**Totals:**
- 6 FAIL rows, which come down to 5 distinct defects.
- 3 not-testable rows.
- Every other row passes.
- 17 new defects after dedup: 3 medium, 12 low, 2 cosmetic.
- 4 corrections to the rig recipes and plan.

## 1. Checks

| id | area | verdict | evidence |
|---|---|---|---|
| H1-stdvga | boot | PASS | bochs, card0 only; WLR_RENDERER=pixman; 0 "Software rendering detected"; desktop shot |
| H1-virtio | boot | PASS | virtio-vga + renderD128; pixman; desktop up; SeaBIOS std-VGA also reaches the desktop |
| H1-retry-path | boot | NOT TESTABLE | no QEMU device triggers wlroots' software check; kms_swrast gives GLES2 on llvmpipe instead (D14) |
| L1 | boot | PASS | 0 udev "failed to execute"; by-uuid links present; initramfs helper scan clean |
| L2 | boot | PASS | devpts gid=5,mode=620,ptmxmode=666; pts node is kdos tty 620 |
| L3 | boot, services | PASS | 0 `kdos [N]: <` lines; ModemManager lines tagged; dbus `--syslog-only` |
| L6 | boot | **FAIL** | SeaBIOS USB boot still prints `/dev/sr0: Can't open blockdev`; empty sr0 size is 2097151, not 0 (D7) |
| boot-dmesg-diff | boot | PASS | only removals against r2 (7 udev lines, resolvconf x2) |
| F6-persistence | boot | PASS | partitioned and whole-disk sticks keep the marker; with no stick the upper is tmpfs |
| bios-legacy-boot | boot | PASS | SeaBIOS: media found, switch_root, runlevel 3, desktop |
| reboot | boot | PASS | runlevel 6 to restart in 8.1 s (was 31.4 s) |
| poweroff | boot | PASS | 5.9 s, qemu rc 0 |
| var-empty | all | PASS | root:root 0755, empty, live and installed |
| C1 | services | PASS | 0 epoll_ctl(ADD); hwsim wlan0 "disconnected"; supplicant Interfaces answers; association CONNECTED |
| H2 | services | PASS | start on a running cups or dbus gives `[SKIP] already supervised (pid N)`; no second instance |
| G1 | services | PASS | postgres smart shutdown and clean restart; 3.06 s TERM grace; KILL at 5.44 s; whole rcK stop 2.07 s |
| M1 | services | PASS | virtlogd and libvirtd pidfiles match the daemons; stop leaves nothing |
| M2 | services | PASS | babeld up; 0 "File exists" |
| L4 | services | PASS | 0 netdev; a wheel user can set the hostname; non-wheel gets Access denied |
| L8 | services | PASS | `service list`: one row each, with `(+N more)` |
| L16 | services, cli | **FAIL** | geoclue, nut and ngircd homes are still the old paths on the image and in build/fs (D8) |
| L7 | services | PASS | with NM stopped, a call gives ServiceUnknown (no activation file) |
| F2 / dbus-#9 / F11 | services, integrity | PASS | no NM/MM/polkit activation files; ksvc parents; pids match; 0 crash loops on 3 boots |
| F3 | services | PASS | failure states reported correctly; stop leaves only `.log` |
| F4 / dbus-#10 | services | PASS | `--nofork --nopidfile`; bus back 5.09 s after SIGTERM (but see D3) |
| F7 | services | PASS | syslogd `-F -8`; firewall ssh on/off returns promptly; sshd `PidFile=none` |
| F12 | services | PASS | /run/chrony is 750; makestep gives 200 OK |
| F14 / utmp-#11 | services | PASS | one runlevel row; `N 3` (telinit not run) |
| F15 / syslog-#12 | services | PASS | UTF-8 kept; rotation kept |
| F16 | services | PASS | 0 avahi Exited |
| F17 | services, cli | PASS | hint reads `service start energyd` |
| F18 | services | PASS | KDOS /etc/default/pcscd; unknown key skipped; args and env applied |
| ksvc-status-3min / crash-loops | services | PASS | all up at 221 s and 425 s; only smartd exits, with final status 17 |
| CLOEXEC-#6 | services | PASS | packd, oomd, mountd and powerd sockets are O_CLOEXEC |
| packd-#7 | services | PASS | install 1.0 then 2.0; rollback works both ways; old pack kept |
| polkit-#13 | services | PASS | no `--no-debug`; pkcheck rc 0 (logtest rule not run) |
| turtle / libpython-intl / ghc-numa | all | PASS | turtle pyc owned by python3-tkinter; libpython needs only libc; no libnuma; ghc runs |
| kpkg-check-all | integrity, install, cli | **FAIL** | `1 missing: gcc ./usr/share/info/gccinstall.info`, live and installed (D4) |
| ldd-scan | integrity | PASS | the 677 missing-NEEDED set is unchanged (all in private dirs); lxi and bcc are gone from the list |
| M3 | integrity | PASS | libbcc links libclang-cpp; `import bcc`, execsnoop and bpftrace work |
| H4 | integrity | PASS | liblxi needs libtirpc; lxi runs; discover rc 0 |
| L9 | integrity | PASS | lirc.client and lirc.config import; kpkg check clean |
| L10 | integrity | PASS | turtle and idlelib import; idle3 present |
| L11 | integrity | PASS | type.xml is empty; 207 fonts; label renders (D16) |
| L12 | integrity | PASS | unowned files 334 → 73 (fs/ or kernel only) |
| L13 / L14 / L15 / L17 | integrity | PASS | libmed owns mdump; Foomatic::PPD syntax OK; no git-svn or cvsserver; no android scripts |
| M4 | integrity | PASS | `std::locale("")` works under C.UTF-8 and en_US; gnucash-cli 5.17; no celestia abort |
| setuid-count | integrity | PASS | 19 setuid, 0 setgid; same set as before |
| F13 | integrity | PASS | /var/log and /var/cache dirs present and owned |
| M18 | desktop | PASS | calendar maps at once; agenda shows the khal event (P1) |
| M19 | desktop | PASS | toast body contrast 5.67:1 |
| L18-taskbar | desktop | PASS | every front-end plate has a picture; 21 NoDisplay entries |
| L18-appid | desktop, apps | **FAIL** | `kdos appid`: 0 matched, MISMATCH for settings, audio, devices, doc, find and pick; rc 1 (D5) |
| L19 | desktop | PASS | 9 tiles have pictures; blurb contrast 5.6–5.7:1 |
| L20 | desktop | PASS | MICROPHONES "none ? no card can record" in KT_MID |
| L21 | desktop | PASS | Devices maps before the 6.84 s scanimage probe ends |
| L22 / L23 | desktop | PASS | desk menu border sits above the hint row; overflow note whole |
| R2-header-icons | desktop | **FAIL** | headers of firewall, store, time, verify, update and print have no picture (D6) |
| R2-devices-scanimage | desktop, apps | PASS | no scanimage left after Devices closes |
| L5 | desktop | PASS | 6 boots, 0 segfaults; one of each portal; Settings and Screenshot answer |
| audio | desktop | PASS | PipeWire default sink; pw-play and aplay rc 0 |
| F20-desktop / F20-cli | desktop, cli | PASS | BT "no adapter"; packd and backup texts right; kfs docs present; `kdos why kpkg` correct (P3) |
| C2 | apps | PASS | no LOCAL libc symbols; vulkaninfo and eglinfo rc 0; LibreOffice maps; opencl rc 237 by design |
| M9 | apps, cli | PASS | 0 `important`; newsboat reloads |
| M10–M17 | apps | PASS | VICE ×10, GParted via pkexec, vlc/elisa, bCNC, tesseract, fldigi, goldendict and flamp all map with no crash |
| L24–L27 | apps | PASS | TUI entries NoDisplay; AdwaitaLegacy present; fontforge clean; catalogue taglines and sizes right |
| G2 | apps, install | PASS | create from a local image does not pull; a failed create's profile can be removed |
| FULL-SWEEP | apps | PASS | 385 entries: 358 map a window (was 198); 5 crashes, all old (Prusa, cura, mpv, dosbox-staging, moon-buggy) |
| L29 | install | PASS | unknown `packs` warned; installer writes `apps =` |
| M5 / F10 / oomd | install | PASS | zram swap stays on; rsync and kinstall at oom_score_adj -900; 0 oomd kills |
| M6 | install | PASS | target /var/log and /var/cache hold dirs only; wtmp starts at the installed boot |
| F9 | install | PASS | font.bin on the ESP; limine.conf points at it |
| F19 | install | PASS | "no BIOS boot partition" warning reaches kinstall.log |
| F1-layout / F1-seabios | install | PASS | BIOS boot partition present; limine bios-install; SeaBIOS reaches the installed login |
| L30 / AB-slots / persistence | install | PASS | root login works; slot a active and confirmed; marker survives a reboot |
| kdos-doctor / F5 / G4 | install, cli | PASS | no fail lines; podman store in $HOME; userdirs hint shown |
| H3 | cli | PASS | ping and tracepath work as root and as kdos; IDN off |
| M7 | cli | PASS | each of the 3 ports rc 0, nothing vulnerable (D11, P2) |
| M8 | cli | PASS | `kdos-shot --help` rc 0; `--geom` refused; no stray slurp |
| L28 | cli | PASS | a broken storage.conf or podman gives a warn (P4) |
| L31–L37 | cli | PASS | help, status and motd strings; RAPL and root skips; userdel clean; no mail spool; boxsock and mediad `--help`; 06_utmps doc; autologin refusal |
| G3 | cli | PASS | `update fetch` with no key refuses before any network access |
| L39 | cli | PASS | man pages present for kpkg, ksvc, service, kinstall and xdg-terminal-exec |
| old-audit-rerun-diff | cli | PASS | every diff is an intended fix except D1, D4 and D12 |
| round2 header-icons / scanimage | cli | NOT TESTABLE | graphical; the desktop area covered them |

## 2. FAILs and new defects (deduplicated, ranked)

`[REG]` marks a fix-regression: this round's change caused or exposed the defect.

### Medium

**D1. kdos-update counts bundled copies as known vulnerabilities.** Found in the cli area.
- **Symptom:** "6 with a known vulnerability" is shown while `kdos cve` reports nothing.
- **Cause:** `src/desktop/kdos-shell/update.c:232-249` `scan_cve()` keeps walking `{` past the end of `"findings": [ ]` and into `"bundled"`. The bug is older than this round; M7 emptying the findings made it visible.
- **Fix:**
  - Bound the loop at the first `]` after `"findings":`, and add a comment stating that rule.
  - Add a selftest fixture with empty findings and 2 bundled rows; the expected count is 0.
- **Rebuild:** kdos-shell (50_desktop).

**D2. Restarting the desktop from tty1 leaves the old kdos-netagent (and kdos-mediad) running.** Found in the boot area.
- **Symptom:** NM refuses the new netagent with AlreadyRegistered 6 times, then the "crashed repeatedly and was given up on" dialog appears.
- **Cause:** `src/desktop/kdos-comp/src/kdos-child.c:280-294` and `:503-505` call setsid and exec with nothing tying the child's life to the compositor's. Then `netagent.c:509-521` exits 0 on AlreadyRegistered, and kdos-child counts that as a death. The bug is older than this round.
- **Fix:**
  - At both sites, in the child after setsid: `prctl(PR_SET_PDEATHSIG, SIGTERM); if (getppid() != parent) _exit(0);`. Add a constraint comment.
  - Update docs/kdos/04-programs/kdos-comp.md "Supervised children" and docs/kdos/03-architecture/session.md "Supervised chrome".
- **Rebuild:** kdos-comp (50_desktop).

**D3. A restarted system bus leaves NetworkManager running but off the bus.** Found in the services area.
- **Symptom:** Wi-Fi drops, wpa_supplicant exits, and ksvc still says `[OK]`.
- **Cause:** NM does not exit when it loses the bus, so its supervisor never restarts it. F4 exposed this, because the bus now comes back.
- **Fix:** choose one.
  - Add a ksvc `supervise --restart-with dbus` option and use it in `42_networkmanager.sh`. Rebuild kdos-tools (42_graphics) and fs/.
  - Or add a known-gaps.md row, and replace the bus-restart sentences in boot-and-init.md and daemons.md.

### Low: failing checks

**D4. `[REG]` FAIL kpkg-check-all: gcc's postinstall deletes gccinstall.info, a file gcc owns.** Found in the integrity, install and cli areas.
- **Cause:** `ports/core/toolchain/gcc/postinstall.sh:16-17` removes the path, meaning to clear the bootstrap compiler's unowned copy.
  - The package ships gccinstall.info only when makeinfo exists at build time.
  - The first 30_foundation build makes texinfo after gcc, so the package had no copy.
  - The M4 `--rebuild gcc` ran on a tree that already had texinfo, so the package now ships the file and the hook deletes it.
  - The defect was latent and older than this round; the M4 rebuild exposed it.
- **Fix:** the areas proposed two fixes; take the integrity one.
  - In `gcc/build.sh`, after the info install block, add `rm -f "$PKG/usr/share/info/gccinstall.info"` with no comment. The package contents no longer depend on the tree's state, and the hook still clears the bootstrap copy.
  - The other proposal (drop the path from postinstall) leaves an unowned bootstrap copy on a fresh build, where gcc builds without makeinfo.
  - Write the rule in gcc `kfs.md` Notes. writing-ports.md:1906-1912 and docs/kfs/05-selfhost/intro.md:157-162 stay accurate.
  - Run `make book-stamp` and kfscheck.
- **Rebuild:** gcc, in 30_foundation only, with no `--snapshot`.

**D5. `[REG]` FAIL L18-appid: appid skips NoDisplay entries.** Found in the desktop and apps areas.
- **Cause:** `src/system/kdos-tools/appid.c:137-141` adds an entry only when NoDisplay is false. L18 made every front-end entry NoDisplay, so the L18 fix is incomplete.
- **Fix:**
  - Add every entry that loads; still skip `Hidden=true`.
  - Replace the comment with the rule: NoDisplay hides an entry from the menu, not from the taskbar.
  - Optionally count "not yet observed" over displayed entries only.
  - Add a selftest: a NoDisplay entry with an observed id gives ok and rc 0.
  - kdos-command.md:850-852 already matches.
- **Rebuild:** kdos-tools (42_graphics).

**D6. FAIL R2-header-icons: ten front ends never call kicon_init, so their headers have no picture.** Found in the desktop area.
- **Cause:** `kicon_slot()` returns -1 when kicon is not initialised (`kicon.c:662`). Nothing initialises it in:
  - `shell.c` `sh_run()` (:1351), which serves firewall, update, verify, print and users;
  - store.c:784, timezone.c:439, disks.c:649, burn.c:641 and rec.c.

  The bug is older than this round. The round-2 icon renames are correct but never reach the screen.
- **Fix:**
  - After `kdisp_init`, call `sh_pic_backend(); kicon_init(sh_pic_cell_w(), sh_pic_cell_h(), kdisp_scale());`, guarded by icons_on, as audio.c:1368-1369 does.
  - Extend the preflight icon check.
  - Update writing-desktop-software.md near :601.
  - Goldens are unaffected.
- **Rebuild:** kdos-shell (50_desktop). Quick look first: `testing/quick.sh kdos-shell`.

**D7. FAIL L6: the media scan still tries to mount an empty optical drive.** Found in the boot area.
- **Cause:** `script/phases/70_image/090_initramfs.sh:1081-1085` skips a node only when its sysfs size is 0. An empty sr drive reports the guessed size 2097151. The fix's premise was wrong, so this is not a regression.
- **Fix:**
  - Replace the size test with `{ : < "\$dev"; } 2>/dev/null || continue`, placed before the CHECKED test. Opening a drive with no medium fails with ENOMEDIUM.
  - Replace the comment.
  - Update docs/kfs/15-image/091-initramfs-init.md:281-283, and 090 if it mentions the size.
  - Run `kfscheck.py --repin` if needed, then `make book-stamp`.
- **Rebuild:** 70_image.

**D8. FAIL L16: service account homes are not applied on an existing tree.** Found in the services and cli areas.
- **Cause:** the geoclue, nut and ngircd `postinstall.sh` hooks call `useradd` only when the account is absent. build/fs already had the accounts, so it keeps the old homes. A fresh tree is correct.
- **Fix:** the areas proposed two fixes.
  - Recommended (services area): in each hook, after the guard, add `usermod -R "$root" -d /nonexistent <name>`. It is idempotent and makes the tree converge; it is not a migration path. Update each kfs.md, then `make book-stamp`. Rebuild those 3 ports (41_system).
  - Alternative (cli area): no repo change. From a container, run `usermod -R /kdos/build/fs -d /nonexistent <name>` once, then build 70_image.

### Low: new defects behind passing checks

**D9. openresolv calls `service <name> <cmd>`, so ksvc prints usage.** Every subscriber restart (nscd, dnsmasq, unbound, named) is skipped.
- **Fix:**
  - In `ports/core/network/openresolv/build.sh`, add to configure: `--restartcmd='service status $1 >/dev/null 2>&1 && service restart $1'`.
  - Use no `:`, and escape `&` as `\&`, because the Makefile substitutes the value with sed.
  - Check the installed resolvconf, line 33.
  - Add a kfs.md (Flags), then `make book-stamp`.
- **Rebuild:** openresolv (41_system).

**D10. SIGTERM to a ksvc supervisor orphans its daemon, and status then says [DOWN].** A later `service start` would then launch a second instance.
- **Fix (optional):** forward TERM to `-getpgrp()`, or have the H2 guard check the daemon pid. Update the selftest and kdos-command.md.
- **Rebuild:** kdos-tools (42_graphics).

**D11. `kdos cve a b c` checks only the last name.**
- **Cause:** `src/system/kdos-tools/cve.c:409-414` overwrites the name on each argument.
- **Fix:** refuse a second name or an unknown option with usage and rc 2. Add a selftest and a line in kdos-command.md#kdos-cve.
- **Rebuild:** kdos-tools (42_graphics).

**D12. `kdos-podman-api --help` starts the API service and then runs `exec --help`.**
- **Fix:** in the heredoc in `ports/core/containers/podman/build.sh`, add `case "$1" in -h|--help) echo "usage: kdos-podman-api COMMAND [ARG...]"; exit 0 ;; esac`. Then `make book-stamp`.
- **Rebuild:** podman (43_toolkits).

**D13. `kdos-appbox catalogue --selftest` segfaults against the shipped catalogue.** It is a test-only path and older than this round.
- **Cause:** `must()` in `catalogue.c:661-671` returns a zeroed struct whose `.packages` is NULL, and line :717 then calls strcmp on it.
- **Fix:** `static const CatPack none = { .packages = (char *)"" };`.
- **Rebuild:** kdos-appbox (44_apps).

**D14. With EGL on kms_swrast, wlroots runs GLES2 on llvmpipe.** The pixman retry never fires, and the CRT shader runs on the CPU. This is plausible on real hardware but not proven there.
- **Fix:**
  - In `kdos-crt.c:317-324`, decline the shader when `GL_RENDERER` contains llvmpipe, softpipe or swrast.
  - Replace the claim in the kdos-desktop-start comment (~171-178) and in session.md:162.
- **Rebuild:** kdos-comp (50_desktop).

**D15. moon-buggy segfaults when its terminal is killed.** It is older than this round and not yet diagnosed.
- **Next step:** get a gdb backtrace in kdos-devdeps, then ship a `.patch` with a `Subject:` line, plus a kfs.md.
- **Rebuild:** moon-buggy (41_system).

### Cosmetic

- **D16.** ImageMagick ships an unreferenced `type-dejavu.xml` naming 33 absent fonts. Optional fix: remove it in build.sh. Rebuild imagemagick (43_toolkits).
- **D17.** `useradd -D` prints `CREATE_MAIL_SPOOL=yes`. Optional fix: ship `fs/etc/default/useradd` and update configuration.md. Rebuild via the fs/ step, then 70_image.

### Corrections to plan and rig recipes (no product defect)

- **P1.** The M18 recipe needs `mkdir -p ~/.local/share/calendars/personal; khal new -a personal today ...`.
- **P2.** The M7 command proves only its last name. Loop over the names instead.
- **P3.** plan.iso.md:494 is wrong: no package owns kpkg (it comes from 10_bootstrap), so "(none …)" is the right answer.
- **P4.** L28's `chmod 000` test cannot break rootless podman. Use a malformed storage.conf.

## 3. Not testable

| item | why |
|---|---|
| H1 pixman retry, executed | no QEMU device triggers wlroots' "Software rendering detected"; vmware-svga's vmwgfx probe fails (-38); virtio falls back to GLES2 on llvmpipe (D14) |
| clinfo (C2) | no port |
| El Torito CD boot on BIOS; real hardware; `--gl`/virgl | not run; the rig is VM-only |
| telinit 2/3 (F14); polkit logtest rule (#13) | not run |
| energyd CLOEXEC | no RAPL, so energyd is down |
| hwsim DHCP lease (C1) | the test network has no DHCP server; association proven instead |
| Limine BIOS banner on the installed disk | nothing is printed to serial before getty; login proven instead |
| L21 spawn-to-first-frame in ms | WAYLAND_DEBUG shows no attach; judged from a no-sleep shot |
| `kdos doctor` rc 0, installed disk with a session | no session on the installed run; on live, the one warn (overlay $HOME) is correct |

## 4. Narrowed builds carrying every fix

Make the edits first, and do not edit a port while its rebuild runs.

```sh
make build BUILD_ARGS="--phases 30_foundation --rebuild gcc"                                    # D4 (30_foundation only, no --snapshot, not 20_selfhost)
make build BUILD_ARGS="--phases 41_system --rebuild geoclue,nut,ngircd,openresolv"              # D8, D9 (+moon-buggy after D15)
make build BUILD_ARGS="--phases 42_graphics --rebuild kdos-tools"                               # D5, D11 (+D10, D3 ksvc option)
make build BUILD_ARGS="--phases 43_toolkits --rebuild podman"                                   # D12 (+imagemagick for D16)
make build BUILD_ARGS="--phases 44_apps --rebuild kdos-appbox"                                  # D13
make build BUILD_ARGS="--phases 50_desktop --rebuild kdos-shell,kdos-comp"                      # D1, D2, D6, D14
make build BUILD_ARGS="--phases 10_bootstrap,70_image --steps 10_bootstrap:000_file_system.sh"  # only if fs/ changes (D3 init, D17)
make build BUILD_ARGS="--phases 70_image"                                                       # D7 initramfs; packages everything above
```

**Then:**
- Run `testing/preflight.sh`, `testing/selftest.sh`, `python3 testing/kfscheck.py` and `make book-stamp`.
- Re-check on the rig:
  - `kpkg check --all`: 0 missing.
  - `kdos appid --quiet`: rc 0.
  - Header shots via `testing/quick.sh kdos-shell`.
  - SeaBIOS USB boot: the serial log has no sr0 error.
  - `/etc/passwd`: the three accounts have home `/nonexistent`.
  - `kdos-update --dump`: 0 with a known vulnerability.
  - Restart the desktop from tty1: exactly one netagent, whose parent is the new kdos-comp.