# plan.iso — KDOS bootable ISO: fix plan

Source: six verified audit areas (boot, services, integrity, desktop, apps, install, cli) run against the snapshot ISO `isoaudit/kdos.iso` (`BUILD_ID=f6b0d161-dirty`), sorted against HEAD `06f59201`, merged with `isoaudit/fix-final.json`. Nothing here is committed. Hard rule 1 applies to every item: the docs named under each fix change in the same change.

Ids: `C` critical, `H` high, `M` medium, `L` low (open); `F` fixed at HEAD and waiting for a build; `G` documented gap; `X` refuted or a rig artefact (appendix).
Effort: S = under 1 h, M = up to 1 day.

---

## 1. Summary

| id | sev | area | title | status | effort |
|---|---|---|---|---|---|
| C1 | critical | services | wpa_supplicant built with `CONFIG_ELOOP_EPOLL`: its D-Bus interface never answers, so NM keeps every Wi-Fi device `unavailable` and GeoClue activation times out after 25 s | open | S |
| C2 | critical | apps | Mesa Rust code linked with crt-static: `libvulkan_nouveau.so` and `libRusticlOpenCL.so` carry private musl copies. Every Vulkan/zink/OpenCL client crashes (~130 Start entries, LibreOffice ×7, satdump, ffmpeg OpenCL) | open | M |
| H1 | high | boot | Desktop never starts on any non-virtio GPU that has no hardware GL (std VGA/bochs, simpledrm, ast, mgag200, cirrus, hyperv, vbox): wlroots refuses llvmpipe | open | S |
| H2 | high | services | `service start` on a running service forks a second supervisor and daemon; stop then orphans the first one | open | S |
| H3 | high | cli | `ping`/`tracepath` fail with `Invalid flags` for every destination (iputils built with IDN on musl) | open | S |
| H4 | high | integrity+apps | `liblxi.so.1` not linked to libtirpc: `lxi` and `lxi-gui` fail to relocate | open | S |
| M1 | medium | services | libvirtd/virtlogd overwrite the supervisor's pid file: stop does not stop them, and rcK leaves them running | open | S |
| M2 | medium | services | babeld never starts (`creat(/var/run/babeld.pid): File exists`) | open | S |
| M3 | medium | integrity | `libbcc.so.0` has 45 unresolved clang/LLVM 23 symbols: `import bcc` and all 139 bcc tools fail | open | S |
| M4 | medium | integrity+apps | libstdc++ generic locale throws for `C.UTF-8`: GnuCash, `gnucash-cli --quotes`, Celestia abort | open | M |
| M5 | medium | install | kinstall `swapoff -a` turns off the live session's zram for the rest of the session | open | S |
| M6 | medium | install | Live-session syslog, wtmp, caches and history are copied onto the installed system | open | S |
| M7 | medium | cli | `kdos cve` flags aerc, mbedtls and vorbis-tools on a clean image and exits 1 (fixes on other branches, bad secdb rows) | open | M |
| M8 | medium | cli | `kdos-shot --help` (any dash arg) enters region mode and hangs in slurp | open | S |
| M9 | medium | apps | newsboat cannot start: `kdos theme` writes `color important`, which newsboat 2.44 rejects | open | S |
| M10 | medium | apps | VICE SDL builds (10 entries) segfault on first launch: no `~/.local/state/vice` | open | S |
| M11 | medium | apps | GParted cannot open its window: no `xhost` port, so root is not admitted to Xwayland | open | M |
| M12 | medium | apps | Elisa segfaults: no VLC `plugins.dat`, so libvlc loads the Qt5 GUI plugin into Qt6 | open | S |
| M13 | medium | apps | bCNC (and LinuxCNC gcode_ripper) use the `trace variable` API that Tk 9 removed | open | S |
| M14 | medium | apps | tesseract has no compiled-in data path: no languages, so gImageReader is unusable | open | S |
| M15 | medium | apps | fldigi aborts: `Fl_ComboBox::add` indexes an empty string (caught by `_GLIBCXX_ASSERTIONS`) | open | S |
| M16 | medium | apps | GoldenDict-ng overflows musl's 128 KiB thread stack while loading dictionaries | open | S |
| M17 | medium | apps | flamp segfaults in libdecor-cairo on Wayland (0×0 frame) | open | M |
| M18 | medium | desktop | Clock click: the calendar maps only after khal returns (5.5 s cold) | open | M |
| M19 | medium | desktop | Notification toast body drawn in `KT_DIM` (1.65:1) | open | S |
| L1 | low | boot+services+integrity+desktop | Initramfs carries every udev rule but not their helpers: `failed to execute` on every boot; md/btrfs rules name `/usr/sbin/mdadm`, `/usr/sbin/blkid`, `/sbin/udevadm` | open | M |
| L2 | low | boot | `/dev/pts` mounted without `gid=5,mode=620,ptmxmode=666` | open | S |
| L3 | low | boot | ModemManager and dbus-daemon lines reach syslog twice; ModemManager's copy has an empty ident | open | S |
| L4 | low | boot+services | avahi's bus policy names group `netdev`, which does not exist: a dbus warning at every start, and the policy applies to no one | open | S |
| L5 | low | boot+integrity+apps | xdg-desktop-portal segfaults in libc at session start (1–2 of 4 boots) | open (diagnose) | M |
| L6 | low | boot | Initramfs pass 1 prints `Can't open blockdev` for an empty optical drive | open | S |
| L7 | low | services | `docs/kfs/16-wiki/services.md` says the bus can still start NetworkManager | open (docs) | S |
| L8 | low | services+cli | `service list` table broken: silent scripts join the next row, and multi-line statuses (utmps, timers, tlp) spill into it | open | S |
| L9 | low | integrity | `import lirc.client` fails (config.py misplaced, `import _client`) | open | S |
| L10 | low | integrity | turtle, turtledemo, idlelib and `idle3` not shipped although tkinter is | open | S |
| L11 | low | integrity | ImageMagick `type-dejavu.xml` lists 33 absent URW fonts | open | S |
| L12 | low | integrity | ~281 unowned bootstrap leftovers (translated man dirs, bash.mo/xz.mo, HTML, dlltool.1) | open | M |
| L13 | low | integrity | libmed's `mdump`/`xmdump` links are written outside `$PKG` | open | S |
| L14 | low | integrity | Foomatic::PPD does not compile on perl 5.44 (`defined(@array)`) | open | S |
| L15 | low | integrity | `git svn`/`git cvsserver` shipped but cannot run; not in known-gaps | open | S |
| L16 | low | integrity | System accounts whose home does not exist (avahi-autoipd, mosquitto, geoclue, nut, ngircd) | open | S |
| L17 | low | integrity | openconnect ships an Android helper with `#!/system/bin/sh` | open | S |
| L18 | low | desktop | 21 windowed kdos-shell front ends have no desktop entry: the taskbar shows a letter, and `kdos appid` reports a MISMATCH | open | S |
| L19 | low | desktop | Settings home: tile blurbs drawn in `KT_DIM`; the Panel and System tiles name icons the atlas does not have | open | S |
| L20 | low | desktop | kdos-devices: 'no sound card is present' when a playback-only card exists; empty notes drawn in `KT_DIM` | open | S |
| L21 | low | desktop | kdos-devices takes ~9 s to its first frame (blocking `scanimage -L`) | open | M |
| L22 | low | desktop | Desktop menu clamped to the bottom loses its bottom frame to the hint row | open | S |
| L23 | low | desktop | Overflow popup sentence clipped; kdos-devices `APPLICATION SETS ON A STICK` heading overdrawn | open | S |
| L24 | low | apps | Terminal=true Start entries close at once (ledger, tig, taskwarrior-tui, w3m, presenterm, rmpc) | open | S |
| L25 | low | apps | Adwaita inherits AdwaitaLegacy, which is not shipped | open | S |
| L26 | low | apps | FontForge needs `pkg_resources`; Gramps has no OsmGpsMap | open | S |
| L27 | low | apps | Catalogue metadata: wrong tagline on app.grace, missing sizes, 15 rows with only an id | open | S |
| L28 | low | apps | `kdos doctor` checks podman's store paths but never runs podman | open | S |
| L29 | low | install | `install-to-disk.sh` writes a `packs =` key kinstall does not have; conf.c drops unknown keys silently | open | S |
| L30 | low | install | `testing/vnc-shot.py` never answers `Password:`, so it cannot drive an installed disk | open | S |
| L31 | low | cli | `kdos help`/`kdos status` name `rollback`, `kdos app gimp`, 'installing is a mount' | open | S |
| L32 | low | cli | `kdos doctor` exits 1 on every VM (RAPL is `warn`); as root it says kdos-comp 'predates' the socket | open | S |
| L33 | low | cli | No 'Removing a user' procedure; the per-account ssh-agent blocks `userdel -r` | open (docs) | S |
| L34 | low | cli | `useradd`/`userdel` print mail-spool errors; kdos-users and kdos-shell.md name `adduser`/`deluser`, which are not shipped | open | S |
| L35 | low | cli | `kdos-boxsock --help` creates a socket named `--help`; `kdos-mediad` ignores its arguments | open | S |
| L36 | low | cli | administration.md's init-script table leaves out `06_utmps` | open (docs) | S |
| L37 | low | cli | `kdos-power autologin root` says 'no such account' | open | S |
| L38 | low | cli | kdos-cursors' `/etc/skel` copy disagrees with 050_theme.sh: 31 `config` rows in `kpkg check` | open (optional) | S |
| L39 | low | integrity+cli | No manual pages for kpkg*, ksvc/service, kinstall, xdg-terminal-exec | open (enhancement) | S |
| F1 | high | install | Wipe layout has no BIOS boot partition: the disk does not boot on SeaBIOS | fixed-at-HEAD | — |
| F2 | high | services+boot+integrity | D-Bus activation files for ModemManager/polkit/NM; ModemManager crash-loops | fixed-at-HEAD | — |
| F3 | high | services | ksvc reports `[ OK ]` for a crash-looping daemon | fixed-at-HEAD | — |
| F4 | high | services | System bus never restarts (stale `/run/dbus/pid`) | fixed-at-HEAD | — |
| F5 | high | apps+cli | Rootless podman: storage.conf pins root-owned paths | fixed-at-HEAD | — |
| F6 | medium | boot | Persistence store on a USB stick never found (single blkid races usb-storage) | fixed-at-HEAD | — |
| F7 | medium | services+cli | sshd/syslogd overwrite the supervisor's pid file; powerd/mountd accept without CLOEXEC (`firewall ssh on` hangs) | fixed-at-HEAD | — |
| F8 | medium | all | resolv.conf lacks openresolv's signature | fixed-at-HEAD | — |
| F9 | medium | install | Target mounted at `/mnt` hides `/mnt/iso`: no font.bin, empty plan dump | fixed-at-HEAD | — |
| F10 | medium | install | kinstall and rsync at oom_score_adj 0 | fixed-at-HEAD | — |
| F11 | medium | integrity | Stray unowned `org.freedesktop.NetworkManager.service` from fs/ | fixed-at-HEAD | — |
| F12 | low | boot+integrity | chronyd disables its command socket (`/run/chrony` 0755) | fixed-at-HEAD | — |
| F13 | low | integrity+install+cli | `kpkg check --all`: 10 missing `/var/log`, `/var/cache` dirs | fixed-at-HEAD | — |
| F14 | low | services | `who -r` empty; telinit appends run-level records | fixed-at-HEAD | — |
| F15 | low | services | syslogd mangles UTF-8 (no `-8`) | fixed-at-HEAD | — |
| F16 | low | services | avahi exits 255 before the bus socket exists | fixed-at-HEAD | — |
| F17 | low | services+cli | kdos-energy hints `service kdos-energyd start` | fixed-at-HEAD | — |
| F18 | low | services | `/etc/default/pcscd` is upstream's file | fixed-at-HEAD | — |
| F19 | low | install | Installer `W` warnings dropped | fixed-at-HEAD | — |
| F20 | low | desktop+cli | kdos-audio BT pane says 'not running'; root doctor theme hint; `kdos why` for fs/ files; kdos-backup silent exit; packd.conf `rollback` comment; `/usr/share/doc/kfs` | fixed-at-HEAD | — |
| G1 | low | boot | ksvc stop: fixed 1 s per daemon, then SIGKILL (postgresql loses its clean shutdown) | documented-gap (consequence undocumented) | M |
| G2 | medium | apps | `kdos-box create base=image:` always pulls; a failed create leaves a profile that `remove` keeps | documented-gap | S |
| G3 | low | cli | `kdos update fetch` 404s; no tree key is shipped | documented-gap | S (optional) |
| G4 | low | cli | A user added after boot sees mkdir/chmod errors until `service start userdirs` runs | documented-gap | S (optional) |

---

## 2. Open defects

### C1 — wpa_supplicant D-Bus dead (epoll eloop); Wi-Fi unusable; GeoClue 25 s timeout
- **Evidence:** hwsim `nmcli device` → `wlan0 wifi unavailable`. dbus log: `eloop_sock_queue: epoll_ctl(ADD) for fd=5 failed: File exists` / `dbus: Failed to set callback functions`. `Properties.Get` on `fi.w1.wpa_supplicant1` → `NoReply`. GeoClue `StartServiceByName` → `TimedOut (25000ms)`; with `[wifi] enable=false` it answers at once.
- **Root cause:** `ports/core/network/wpa_supplicant/build.sh:79` `CONFIG_ELOOP_EPOLL=y`. Upstream `dbus_common.c:111-120` registers one fd three times, and `eloop.c:218` `EPOLL_CTL_ADD` then fails with EEXIST.
- **Fix:** build.sh: replace the line with `CONFIG_ELOOP_POLL=y`. `kfs.md:52`: rewrite the bullet (the D-Bus interface registers one socket for read, write and exception; epoll refuses the second add, leaving NM's interface unanswered; poll has no FD_SETSIZE ceiling). `make book-stamp`. GeoClue needs nothing separate.
- **Rebuild:** `--phases 41_system,70_image --rebuild wpa_supplicant`
- **Rig check:** root: `modprobe mac80211_hwsim radios=2; sleep 10; nmcli device` → wlan0 `disconnected`; `grep -c 'epoll_ctl(ADD)' /run/kdos-svc.dbus.log` = 0; the `Properties.Get ... Interfaces` call returns an array; hostapd on wlan1 (ssid kdostest) + `nmcli device wifi connect kdostest ifname wlan0` succeeds; `time dbus-send ... StartServiceByName string:org.freedesktop.GeoClue2` → `uint32 1|2` in under 5 s.

### C2 — Mesa crt-static Rust: private musl in nouveau Vulkan and rusticl
- **Evidence:** `readelf -Ws libvulkan_nouveau.so` shows LOCAL `__libc_malloc_impl`, `__libc`, `__stdio_write`. `vulkaninfo`, `gnome-mines` and `eglinfo -B` → rc 139 (segfault in libvulkan_nouveau). Excluding the nouveau ICD → rc 0. rusticl: the gdb fault is a private auxv walk inside `libRusticlOpenCL.so.1`. LibreOffice headless convert rc=1 for every `RUSTICL_ENABLE` value; `OCL_ICD_VENDORS=/var/empty` or `SAL_DISABLE_OPENCL=1` → rc 0. `ffmpeg -init_hw_device opencl` → rc 139. The RUSTICL_ENABLE hypothesis is refuted.
- **Root cause:** `ports/core/gpu/mesa/build.sh:33` `RUSTFLAGS="-C prefer-dynamic"`. The musl target defaults to crt-static, so the Rust staticlibs (NAK, rusticl) embed `libc.a`. The same pattern is already fixed in `python3-cryptography/build.sh:13` and `atuin/build.sh:16`.
- **Fix:** build.sh: `export RUSTFLAGS="-C prefer-dynamic -C target-feature=-crt-static"`. Add a post-install command that fails on LOCAL `__libc`/`__libc_malloc_impl` in any `$PKG/usr/lib/*.so*` (no comment, per hard rule 6). Rewrite `kfs.md:34` (the RUSTFLAGS bullet) to explain both flags. Optionally add the same LOCAL-libc scan to `087_gates.sh` over build/fs (whitelist the `-static-pie` kdos-boxinit and kdos-splash). `make book-stamp`; `python3 testing/kfscheck.py`. Do not touch `fs/etc/profile.d/50-opencl.sh`; do not ship `SAL_DISABLE_OPENCL`.
- **Rebuild:** `--phases 42_graphics,70_image --rebuild mesa`
- **Rig check:** `readelf -Ws /usr/lib/libvulkan_nouveau.so /usr/lib/libRusticlOpenCL.so.1 | awk '$5=="LOCAL"' | grep -cE ' (__libc|__libc_malloc_impl|__stdio_write)$'` = 0. As kdos in the session env: `vulkaninfo --summary` → 0; `timeout 15 gnome-mines` → 124; `eglinfo -B` → 0; ffmpeg OpenCL → not 139; `libreoffice -env:UserInstallation=file:///tmp/lo1 --headless --convert-to pdf --outdir /tmp /etc/hostname` → 0 and a PDF exists; `timeout 20 satdump-ui` → 124. Re-run the 389-entry sweep (`isoaudit/apps/scripts/sweep*.sh`): `grep -c libvulkan_nouveau` in dmesg = 0.

### H1 — no desktop on a non-virtio GPU without hardware GL
- **Evidence:** `-vga std` (bochs-drm, only `card0`, no render node): kdos-comp.log `Could not initialize EGL` / `Software rendering detected`; tty1 sits at the restart prompt. Adding `export WLR_RENDERER=pixman` to `kdos-desktop` on the rig gave the full desktop (stdvga_after.png).
- **Root cause:** `fs/usr/local/bin/kdos-desktop:13-42` selects pixman only for `virtio:*|pci:v00001AF4*`. `kdos-desktop-start:148` runs kdos-comp once and then shows the prompt.
- **Fix:** (1) `kdos-desktop`: after the virtio case, `if ! ls /dev/dri/renderD* >/dev/null 2>&1; then export WLR_RENDERER=pixman; fi`, with a constraint comment. (2) `kdos-desktop-start` ~148: when kdos-comp exits non-zero, WLR_RENDERER is unset and its log has `Software rendering detected`, export pixman and retry once before the prompt (covers vmwgfx without 3D, and nouveau/amdgpu without firmware). Docs: `docs/kdos/03-architecture/session.md` step 2 (the no-render-node rule and the retry); `docs/kfs/16-wiki/virtual-machines.md:300-310` (std VGA, QXL and VirtualBox run in software, phosphor pass off). `python3 testing/kfscheck.py --repin` (the includes of kdos-desktop lines 13-17 and 37-39 move); `make book-stamp`.
- **Rebuild:** fs/ only.
- **Rig check:** UEFI `-vga std`, settle 25 s: `pgrep -x kdos-comp` rc 0; `grep -c 'Software rendering detected' /run/user/1000/kdos-comp.log` = 0; `pgrep -x kdos-shell` rc 0; the shot shows the panel. Repeat on plain virtio-vga to show it still comes up.

### H2 — ksvc double supervisor
- **Evidence:** `service start cups` on a running cups: supervisors 2→4, cupsd 1→2. `service stop cups` kills only the new one; `[DOWN] cupsd` while pid 1650 runs. fix-final.json `open` lists the same.
- **Root cause:** `src/system/kdos-tools/ksvc.c:357-479` `cmd_supervise` never checks `alive(read_pid(name))`, and at `:470-474` overwrites `/run/<name>.pid`. Only `fs/etc/init.d/40_dbus.sh:28-33` guards against this, in shell.
- **Fix:** in cmd_supervise, after `name_ok()` and before `drop_state()`: `if (alive(old)) { printf("[SKIP] (%s) already supervised (pid %d)\n", ...); return 0; }`, optionally checking that `/proc/<old>/cmdline` starts with `ksvc\0supervise`. Comment the rule (a second supervisor takes over the pid file and the first can never be stopped). Remove the 40_dbus.sh shell guard and keep its rule next to the C check. Docs: `docs/kdos/04-programs/kdos-command.md` ~1424 (ksvc supervise: a live name is skipped with `[SKIP]`); `docs/kfs/16-wiki/services.md` if it describes start. `testing/selftest.sh`. Note: fix-final iso_check 10 expects the text `[SKIP] dbus: already supervised`; make the message match one format, and update that check.
- **Rebuild:** kdos-tools (42_graphics) + fs/.
- **Rig check:** `service start cups; sleep 3; pgrep -f 'ksvc supervise cupsd' | wc -l; pgrep -x cupsd | wc -l` → 2, 1, and `[SKIP]` printed; `service stop cups; sleep 3; pgrep -x cupsd` → nothing; `service start dbus` → `[SKIP]`, still 2 dbus supervisor processes.

### H3 — ping/tracepath `Invalid flags`
- **Evidence:** root and kdos `ping -c1 127.0.0.1` → `Invalid flags` rc 2; `ping -V` → `IDN: yes`; arping works.
- **Root cause:** `ports/core/net-diag/iputils/build.sh:12-13` leaves `USE_IDN` at its default (true). iputils then passes `AI_IDN` (0x40), which musl's getaddrinfo rejects with EAI_BADFLAGS.
- **Fix:** add `-DUSE_IDN=false` to `meson setup`; kpkgbuild: drop `libidn2` from depends, release 2. New `kfs.md` (About; a Flags entry for `-DUSE_IDN=false`). `make book-stamp`; kfscheck. security-model.md:88-90 already states the intended behaviour.
- **Rebuild:** `--phases 41_system,70_image --rebuild iputils`
- **Rig check:** `ping -V` → `IDN: no`; ping 127.0.0.1 as root and as kdos → 1 received, rc 0; `tracepath -n 127.0.0.1` prints a hop; `ldd /usr/bin/ping | grep -c idn` = 0.

### H4 — liblxi not linked to libtirpc
- **Evidence:** `lxi --help` → `Error relocating /usr/lib/liblxi.so.1: xdr_u_char: symbol not found`; 20 `UND xdr_*` symbols; `LD_PRELOAD=libtirpc.so.3` makes it work.
- **Root cause:** liblxi-1.22 `src/meson.build:54-62` adds `tirpc_dep` only for cygwin, and `b_lundef=false` hides the gap. `ports/core/eda/liblxi/build.sh` does not compensate.
- **Fix:** `link-tirpc.patch` (`Subject: Link liblxi against libtirpc on every system that uses it`) moving `liblxi_deps += tirpc_dep` out of the cygwin branch; build.sh applies it and passes `-D b_lundef=true`. New `kfs.md` (About, Patches, Flags). `make book-stamp`. lxi-tools needs no rebuild.
- **Rebuild:** `--phases 41_system,70_image --rebuild liblxi`
- **Rig check:** `readelf -d /usr/lib/liblxi.so.1 | grep -c libtirpc` = 1; `lxi --help` rc 0; `timeout 15 lxi-gui` → 124.

### M1 — libvirtd/virtlogd pid file clash
- **Evidence:** `/run/libvirtd.pid` = the daemon's pid. `service stop libvirtd` → respawned 7 s later, and status `[ OK ]`. Both supervisors survive rcK.
- **Root cause:** `ports/core/virt/libvirt/build.sh:134` (62_virtlogd.sh) and `:155` (63_libvirtd.sh). As root the daemons default to `/run/<name>.pid`, which is ksvc's `pidfile()` (`ksvc.c:60-65`).
- **Fix:** `supervise "$NAME" "$DAEMON" --pid-file /run/libvirt/virtlogd.pid` and `... /run/libvirt/libvirtd.pid`. Remove the `# No --daemon ...` rationale comments (hard rule 6). kfs.md Notes: the default pid file is the supervisor's. `make book-stamp`.
- **Rebuild:** `--phases 43_toolkits,70_image --rebuild libvirt`
- **Rig check:** `cat /run/libvirtd.pid` = the first `pgrep -f 'ksvc supervise libvirtd'`; `service stop libvirtd; sleep 7; pgrep -x libvirtd` → nothing; `[DOWN]`. The same for virtlogd.

### M2 — babeld never starts
- **Evidence:** `creat(/var/run/babeld.pid): File exists`, restarting every 5 s.
- **Root cause:** `ports/core/network/babeld/build.sh:32` leaves babeld's default pid file (`babeld.c:74`, O_EXCL at `:366`), which is ksvc's.
- **Fix:** `supervise "$NAME" "$DAEMON" -I '' -c "$CONF" -S /var/lib/babeld/state`. New `kfs.md` with a Flags entry for `-I ''`. `make book-stamp`.
- **Rebuild:** `--phases 41_system,70_image --rebuild babeld`
- **Rig check:** `printf 'interface eth0\n' > /etc/babeld.conf; service start babeld; sleep 8; pgrep -x babeld` → a pid; `grep -c 'File exists' /run/kdos-svc.babeld.log` = 0; stop → no process.

### M3 — libbcc unresolved clang/LLVM 23 symbols
- **Evidence:** `python3 -c 'import bcc'` → `Error relocating ... llvm::abi::createX86_64TargetInfo`; `ldd libbcc.so.0 | grep -c 'symbol not found'` = 45. The providers are libclangAnalysisLifetimeSafety, clangScalableStaticAnalysis*, clangDependencyScanning, clangOptions and libLLVMABI. bpftrace works because it links libclang-cpp.
- **Root cause:** bcc-0.37.0 `cmake/clang_libs.cmake` static clang list (with `ENABLE_LLVM_SHARED=OFF` in `ports/core/devtools/bcc/build.sh`) does not name the libraries LLVM 23 split out.
- **Fix:** pre-set the cache: `-DlibclangAnalysis=/usr/lib/libclang-cpp.so` and the same for AST, Basic, CodeGen, Driver, Edit, Frontend, Lex, Parse, Rewrite, Sema, Serialization, ASTMatchers, Support, APINotes. Post-install guard: `ldd $PKG/usr/lib/libbcc.so.0 | grep -c 'symbol not found'` = 0. kfs.md: rewrite the ENABLE_LLVM_SHARED bullet. `make book-stamp`.
- **Rebuild:** `--phases 40_lang,70_image --rebuild bcc`
- **Rig check:** `ldd /usr/lib/libbcc.so.0 2>&1 | grep -c 'symbol not found'` = 0; `readelf -d ... | grep -c libclang-cpp` = 1; `python3 -c 'import bcc'`; `timeout 5 /usr/share/bcc/tools/execsnoop -h`; the bpftrace BEGIN probe still prints.

### M4 — libstdc++ `std::locale("")` throws under `LANG=C.UTF-8`
- **Evidence:** `gnucash` → rc 134; `gnucash-cli --quotes info` → `_S_create_c_locale name not valid` (gdb: `std::locale::locale(char const*)` ← `boost::this_process::path()` ← `GncFQQuoteSource`); `celestia-sdl` → the same exception. `LANG=C` works. A bare `std::locale("")` throws for `C.UTF-8` and `en_US.UTF-8`.
- **Root cause:** libstdc++ on musl uses the generic clocale model; `config/locale/generic/c_locale.cc` `_S_create_c_locale` accepts only `C`/`POSIX`. Boost.Process v1 `locale.hpp:72-85` calls `std::locale("")`. `fs/etc/profile:32` exports `LANG=C.UTF-8` (keep it).
- **Fix (primary):** `ports/core/toolchain/gcc/libstdcxx-generic-locale-names.patch` (`Subject: Accept any locale name the C library accepts in libstdc++'s generic locale model`): when the name is neither C nor POSIX, accept it if `newlocale(LC_ALL_MASK, name, 0)` succeeds (free it), keeping the C facet data. Apply it in build.sh; gcc `kfs.md` Patches. Prove it first in kdos-devdeps with a three-line `std::locale("")` test. **Fallback** if the gcc rebuild slips: `ports/core/devlibs/boost/musl-process-locale.patch` (adds `|| (defined(__linux__) && !defined(__GLIBC__))` to both branches of `boost/process/v1/locale.hpp`), then rebuild gnucash; Celestia stays broken, so add a known-gaps row. `make book-stamp`.
- **Rebuild:** `--phases 30_foundation --rebuild gcc` (gcc is also listed in 20_selfhost; select only 30_foundation; no `--snapshot`). Fallback: `--phases 41_system --rebuild boost`, then `--phases 44_apps,70_image --rebuild gnucash`.
- **Rig check:** as kdos with `LANG=C.UTF-8`: `timeout 15 gnucash` → 124; `gnucash-cli --quotes info` → no `terminate called` (expect `Can't locate JSON.pm`, see L-gap in §4); `timeout 10 celestia-sdl 2>&1 | grep -c _S_create_c_locale` = 0; `LANG=en_US.UTF-8 timeout 15 gnucash` → 124.

### M5 — kinstall `swapoff -a` kills the live zram
- **Evidence:** `/proc/swaps` has `/dev/zram0`; it is gone at t=66 s of the install and stays gone after a killed install.
- **Root cause:** `src/system/kdos-installer/install.c:1045-1046` (do_prepare).
- **Fix:** walk `/proc/swaps` (the same idiom as `unmount_disk()` at `:663`) and `swapoff` every entry except `/dev/zram*`. Docs: `docs/kdos/04-programs/kinstall.md:273,:289`; `docs/kdos/02-user-guide/installation.md` (where it says the installer stops swap).
- **Rebuild:** `rm build/mark/bootstrap/kinstall`; `--phases 10_bootstrap,70_image --steps 10_bootstrap:130_kinstall.sh`
- **Rig check:** live boot with `--disk`. Start `kinstall --config <wipe> --unattended &`; once `rsync` appears in kinstall.log, `grep -c zram0 /proc/swaps` = 1; kill kinstall; still 1.

### M6 — live-session state copied onto the installed system
- **Evidence:** on the installed boot, syslog has 4427 lines from host `kdos`; `last` starts at the live boot with a `crash`; atuin, starship and bat caches and root's caches are dated at the live boot.
- **Root cause:** `src/system/kdos-installer/install.c:1347-1386` (do_copy) excludes only kinstall.log, machine-id, ssh host keys and shadow backups.
- **Fix:** add `--include=/var/log/**/ --exclude=/var/log/** --include=/var/cache/**/ --exclude=/var/cache/**` (keeps the claimed directories, so F13 does not regress), plus `--exclude=` for `/root/.bash_history`, `/root/.cache/***`, `/home/kdos/.bash_history`, `/home/kdos/.cache/***` and `/home/kdos/.local/share/atuin/***`. Size the argv. Add a block comment (the rule). Docs: `kinstall.md:308-321`, `installation.md`. Add a selftest case if the rsync argv is tested.
- **Rebuild:** with M5 (130_kinstall).
- **Rig check:** install-to-disk.sh, then boot `--no-cdrom` (needs L30): `grep -c ' kdos ' /var/log/messages` = 0; `last -F | grep -c crash` = 0; no pre-install atuin/starship dirs; `kpkg check --all` → 0 missing.

### M7 — `kdos cve` false positives
- **Evidence:** aerc 0.22.0 flagged against 1.21.0; mbedtls 3.6.7 against the 4.1.1 row (every CVE is also in the 3.6.7 row); vorbis-tools 1.4.3 against 9.54; rc 1.
- **Root cause:** `src/system/kdos-tools/cve.c:157-230` `check_one()` counts any row above `have`, with no notion of a branch; `secdb/secdb.txt:12,:3747` hold mis-versioned rows that `vendor.py` cannot correct.
- **Fix:** decide per CVE: closed if some row naming it has a fix ≤ have on have's major.minor, or have ≥ every row; 'fixed in' is the lowest version above have on have's branch, else the lowest row above have. Rewrite the header comment and the `:190-202` comment. Add `secdb/corrections.txt` (`aerc 1.21.0-r0 drop`, `vorbis-tools 9.54-r1 1.4.3`), apply it in vendor.py, regenerate secdb.txt. Selftest fixture with two branches. Docs: `kdos-command.md#kdos-cve`.
- **Rebuild:** kdos-tools (42_graphics).
- **Rig check:** `su - kdos -c 'for p in mbedtls aerc vorbis-tools; do kdos cve $p; echo $p rc=$?; done'` → nothing vulnerable, rc 0 for each (one call with three names reports only the last name's verdict in its rc); `time kdos cve` finishes (record the time; it was over 20 s).

### M8 — kdos-shot option parsing
- **Root cause:** `src/system/kdos-tools/shot.c:137-179`: `argv[1][0] != '-'` makes any option mean region mode; unknown options are skipped; `--geom` is refused only after `kb_mkdir_p`; the usage still shows `[--geom X,Y,W,H]`.
- **Fix:** parse everything first: `-h/--help` → usage on stdout, rc 0; `--geom` → refusal before mkdir, rc 1; any other dash argument → usage on stderr, rc 1; drop `--geom` from the usage. Docs: `kdos-command.md#kdos-shot` (unknown option, `--help`); check the completion file.
- **Rebuild:** kdos-tools.
- **Rig check:** as kdos: `timeout 5 kdos-shot --help` → usage, rc 0; `--bogus` → rc 1; after `rm -rf ~/Pictures/Screenshots`, `--geom 0,0,1,1` → rc 1 and the directory is not created; `pgrep -x slurp` → none.

### M9 — newsboat `color important`
- **Root cause:** `src/system/kdos-tools/kdos.c:1245` `write_newsboat()`.
- **Fix:** delete that format line and its two arguments (`kcol_xterm256(sc->urgent), deep`). theming.md:247 and configuration.md:1136 do not name the element; grep to confirm.
- **Rebuild:** kdos-tools.
- **Rig check:** as kdos: `kdos theme <accent>`; `grep -c important ~/.config/newsboat/kdos-colors` = 0; `timeout 5 newsboat -x reload` → 0.

### M10 — VICE SDL state dir
- **Root cause:** vice-3.10 `src/arch/sdl/archdep.c:99-123` creates only the config dir (the GTK3 archdep creates cache, config and state); the log open fails and `log_helper` runs strlen(NULL).
- **Fix:** `sdl-create-state-dir.patch` (`Subject: Create the user state and cache directories in the SDL UI's archdep_init, as the GTK3 UI does`), applied in build.sh; kfs.md Patches. `make book-stamp`.
- **Rebuild:** `--phases 44_apps,70_image --rebuild vice`
- **Rig check:** after C2: `rm -rf ~/.local/state/vice; timeout 15 x64sc` → 124; `vice.log` exists; the same for vsid and xvic.

### M11 — GParted needs xhost
- **Root cause:** `ports/core/disk/gparted/build.sh --enable-xhost-root`; the wrapper (`/usr/bin/gparted:68`) skips the grant without xhost; no xhost port exists. ports-catalogue.md:1455 documents a working GParted.
- **Fix:** new port `ports/core/x11/xhost` (xhost-1.0.10; depends libX11 libXmu libXau), listed in `42_graphics/packages.d/x11.txt` (confirm with `python3 testing/phaseclosure.py`); `make fetch`, `ports/publish xhost`, commit its `sources.idx` line. Add `xhost` to gparted's depends. ports-catalogue.md; `make book-stamp`.
- **Rebuild:** `--phases 42_graphics --rebuild xhost`; `--phases 44_apps,70_image --rebuild gparted`
- **Rig check:** as kdos `xhost +SI:localuser:root`; as root `DISPLAY=:0 python3 -c 'import tkinter; tkinter.Tk(); print(1)'` → 1; GParted from Start → pkexec prompt, then `kdos hey list` shows it; take a shot.

### M12 — VLC plugin cache (Elisa)
- **Evidence:** no `plugins.dat`; Elisa segfaults in libQt5Gui. Running `vlc-cache-gen` makes Elisa run.
- **Root cause:** `src/system/kdos-kpkg/triggers.c:83-95` has no trigger for `usr/lib/vlc/plugins/`.
- **Fix:** add a trigger (the T_GIO pattern) that runs `/usr/lib/vlc/vlc-cache-gen /usr/lib/vlc/plugins` when the binary exists. Docs: the shared-indexes table in `writing-ports.md:1930-1943`, and packaging.md if it lists the triggers. selftest.
- **Rebuild:** `--phases 10_bootstrap --steps 10_bootstrap:120_kpkg.sh` (hash-marked), then `--phases 43_toolkits --rebuild vlc` so the trigger runs.
- **Rig check:** `ls -la /usr/lib/vlc/plugins/plugins.dat`; `timeout 15 elisa` → 124 with no libQt5Gui segfault; `vlc --version`.

### M13 — Tk 9 `trace variable`
- **Fix:** `ports/core/fabrication/bcnc/tk9-trace-add.patch` (`Subject: Use trace_add("write") instead of the trace("w") API Tk 9 removed`) for the 6 calls (CNCCanvas.py:2101, ProbePage.py:309/331/2302, ToolsPage.py:1938, lib/tkExtra.py:3033); the same kind of patch for linuxcnc `qtvcp/lib/ripper/gcode_ripper.py` (check `ncfiles/holecircle.py`). kfs.md Patches in both. `make book-stamp`.
- **Rebuild:** `--phases 44_apps,70_image --rebuild bcnc,linuxcnc`
- **Rig check:** `timeout 20 bcnc` → 124 with a window in `kdos hey list`; `grep -rlE '\.trace\("w"' /usr/lib/python3*/site-packages/{bCNC,qtvcp}` → nothing.

### M14 — tesseract data path
- **Root cause:** `ports/core/printing/tesseract/build.sh`: cmake never defines `TESSDATA_PREFIX`, so `ccutil.cpp:82-89` falls back to `"./"`.
- **Fix:** add `-DTESSDATA_PREFIX='"/usr/share"'` to CXXFLAGS (or CMAKE_CXX_FLAGS); guard with `strings` on the built library; kfs.md Flags. `make book-stamp`.
- **Rebuild:** `--phases 41_system,70_image --rebuild tesseract`
- **Rig check:** with TESSDATA_PREFIX unset, `tesseract --list-langs` → eng, osd; gImageReader opens without 'No languages available'.

### M15 — fldigi empty-string index
- **Fix:** `combo-empty-add.patch` (`Subject: Do not index past an empty string in Fl_ComboBox::add`) changing combo.cxx:524 to `!str.empty() && str.back() == '|'`. kfs.md Patches. Check the vendored combo.cxx in flrig, flmsg and flamp.
- **Rebuild:** `--phases 44_apps,70_image --rebuild fldigi` (plus any sibling that carries the same code)
- **Rig check:** `timeout 20 fldigi 2>&1 | grep -c basic_string.h` = 0; rc 124; a window is listed.

### M16 — GoldenDict thread stack
- **Root cause:** `ports/core/education/goldendict-ng/build.sh`: no `-z stack-size`, so pool threads get musl's 128 KiB (fault address and sp are in the same unmapped page).
- **Fix:** `LDFLAGS="$LDFLAGS -Wl,-z,stack-size=8388608"` (measure whether 1 MiB is enough); a kfs.md Flags entry modelled on nheko's. `make book-stamp`.
- **Rebuild:** `--phases 44_apps,70_image --rebuild goldendict-ng`
- **Rig check:** `readelf -lW /usr/bin/goldendict | grep GNU_STACK` shows a non-zero MemSiz; `timeout 20 goldendict` → 124 with no `Thread (pooled)` segfault.

### M17 — flamp / libdecor-cairo 0×0 frame
- **Root cause:** suspected: `size_range()` before the first size → a libdecor frame of 0×0 → `mmap` EINVAL → NULL deref in `draw_border_component`. `FLTK_BACKEND=x11` works.
- **Fix:** reproduce under ASan in kdos-devdeps first; then either a libdecor cairo patch refusing width or height ≤ 0 (`Subject:` line), or a flamp patch. Interim: the desktop entry gets `Exec=env FLTK_BACKEND=x11 flamp`, written by build.sh, with the reason in kfs.md.
- **Rebuild:** `--phases 44_apps,70_image --rebuild flamp` or `--phases 42_graphics --rebuild libdecor`
- **Rig check:** `timeout 15 flamp` → 124 on Wayland; no libdecor-cairo segfault.

### M18 — kdos-cal maps only after khal returns
- **Evidence:** at ≥2.5 s after the click nothing is mapped; at the later shot the calendar is drawn. khal takes 5.5 s cold, 1.3 s warm. The probe's 'never shows' is refuted.
- **Root cause:** `src/desktop/kdos-shell/cal.c:403` `cal_scan()` (blocking `khal list`) runs before `kdisp_init()` (`:431`).
- **Fix:** start khal as an `sh_job_start` job after kdisp_init; map at CAL_ROWS; poll every ~100 ms while the job runs; when it ends, parse into marks and the agenda and call `kdisp_overlay_resize` (the surface is bottom-anchored, so it grows upwards). Changing month starts a job too. The `KDOS_CAL_LIST` fixture stays synchronous (goldens unchanged). Rewrite the 'sized ONCE' comment (`:417-419`) and `kdos-shell.md:2381`.
- **Rebuild:** kdos-shell (50_desktop). For a quick look: `testing/quick.sh kdos-shell`.
- **Rig check:** fresh boot, `--wait 30 --keys esc --click 1840,1060 --shot a.png` with no sleep; ink present over (1480,640)-(1920,1040). After `su - kdos -c 'mkdir -p ~/.local/share/calendars/personal; khal new -a personal today 10:00 11:00 X'` (khal has no calendar to write to until the directory exists), click again: the agenda strip appears.

### M19 — toast body `KT_DIM`
- **Fix:** `src/desktop/kdos-shell/notifyd.c:870` `fg = KT_MID`. Regenerate the notifyd goldens in kdos-devdeps if they change.
- **Rebuild:** kdos-shell.
- **Rig check:** `notify-send -t 60000 T Bodytext`, shot with no sleep: body ink ≈ (143,139,126) on (14,14,13), a ratio ≥ 3.4.

### L1 — initramfs udev rules without helpers (four areas merged)
- **Evidence:** 8–9 `failed to execute` lines at 0.8–2.1 s (nfsrahead, tlp-usb-udev, mtp-probe, pktsetup). An offline scan of the initramfs shows the missing `/usr/sbin/{mdadm,blkid,nvme,...}`, `/sbin/udevadm`, `/usr/libexec/*` and `/usr/bin/*`. The helpers exist on the real root, and mdadm, blkid and udevadm exist in the initramfs as `bin/*`. No functional loss: the init runs `mdadm --assemble --scan`, and 01_udev replays the rules.
- **Root cause:** `script/phases/70_image/090_initramfs.sh:342-349` copies the rules and `/usr/lib/udev/*` wholesale; `:178-179` puts mdadm in `bin/`.
- **Fix:** (a) put mdadm at `usr/sbin/mdadm` with a `bin/mdadm` link (the lvm/dmsetup pattern at `:301-304`); add links `usr/sbin/blkid → ../../bin/blkid`, `sbin/udevadm → ../bin/udevadm` and `usr/lib/udev → ../../lib/udev`. (b) After the copy, drop every `lib/udev/rules.d` and `etc/udev/rules.d` rule whose RUN/PROGRAM/IMPORT{program} absolute path is absent from the tree, echoing each drop. (c) Fail the build if 10-dm, 11-dm-lvm, 13-dm-disk, 69-dm-lvm, 95-dm-notify, 63-md-raid-arrays, 64-md-raid-assembly or 64-btrfs is dropped (next to the existing lvm2 `_stray` gate). Constraint comment. Docs: `boot-and-init.md:435`; `docs/kfs/15-image/090-initramfs.md`; `kfscheck --repin` if includes move.
- **Rebuild:** 70_image.
- **Rig check:** `--no-session --root-cmd "dmesg | grep -c 'failed to execute'"` → 0; `/dev/disk/by-uuid` populated; offline: unpack the gzip segment after the microcode cpio and re-run the path scan → zero missing; the dm/md rules are present.

### L2 — devpts options
- **Fix:** `090_initramfs.sh:507` `mount -t devpts -o gid=5,mode=620,ptmxmode=666 devpts /dev/pts`. Docs: `docs/kfs/15-image/091-initramfs-init.md`.
- **Rebuild:** 70_image.
- **Rig check:** `grep devpts /proc/mounts` → `gid=5,mode=620,ptmxmode=666`; `ls -l /dev/pts/ptmx` → `crw-rw-rw-`; with foot open, `stat -c '%G %a' /dev/pts/0` → `tty 620`.

### L3 — ModemManager/dbus syslog doubling
- **Root cause:** `fs/etc/init.d/42_modemmanager.sh:29`: ModemManager opens syslog with a NULL ident and `LOG_PERROR`; `40_dbus.sh:40`: dbus-daemon writes to syslog and stderr.
- **Fix:** `supervise "$NAME" "$DAEMON" --log-file=/dev/stdout` (proven on the rig); replace the 'it logs to syslog' comment. dbus: add `--syslog-only` after checking `dbus-daemon --help` on the image. Docs: `docs/kfs/16-wiki/logs.md`, `mobile-broadband.md`.
- **Rebuild:** fs/.
- **Rig check:** `grep -c "kdos \[[0-9]*\]: <" /var/log/messages` = 0; ModemManager-tagged lines ≥ 1; `grep -c "kdos dbus: dbus-daemon"` = 0.

### L4 — avahi `netdev` group
- **Fix:** `ports/core/net-libs/avahi/build.sh`: `--with-avahi-priv-access-group=wheel`; kfs.md Flags; `docs/kfs/16-wiki/avahi-and-mdns.md` (wheel members may call SetHostName). `make book-stamp`. avahi is already in fix-final's rebuild list.
- **Rebuild:** `--phases 41_system,70_image --rebuild avahi`
- **Rig check:** `grep -c netdev /run/kdos-svc.dbus.log /usr/share/dbus-1/system.d/avahi-dbus.conf` → 0, 0; `policy group="wheel"`; as kdos, `avahi-set-host-name kdos` is not AccessDenied.

### L5 — xdg-desktop-portal segfault (diagnose before fixing)
- **Evidence:** `segfault ... error 6 in libc.so[6fadb,...]`, UID 1000, at 11.6–18.5 s, in 1–2 of 4 boots per area; the portal later runs under a new pid. The crashing pid is later than every surviving portal.
- **Hypotheses:** (a) `fs/usr/local/bin/kdos-desktop-start:68-73` sends SIGTERM to a D-Bus-activated main portal during its initialisation; (b) a second activated instance of portal-wlr (there is both an activation file and a session spawn).
- **Plan:** log MAIN's pid to syslog from kdos-desktop-start; enable cores (`ulimit -c unlimited` in the session, `kernel.core_pattern=/var/tmp/core.%e.%p`); boot 5–10 times; match the pid; gdb in devdeps. Then remove one of the two start paths rather than killing the process. Update the kdos-desktop-start comment block (`:44-51`) and session.md.
- **Rebuild:** fs/ for the instrumentation and for the fix.
- **Rig check:** 10 boots: `dmesg | grep -c 'xdg-desktop-por.*segfault'` = 0; each portal binary appears once in `pgrep -af xdg-desktop-portal`; `kdos doctor` portal line ok.

### L6 — empty optical drive noise
- **Fix:** in the 090_initramfs.sh live scan loop, skip a node that cannot be opened (an empty drive answers ENOMEDIUM; its sysfs size is a guessed 2097151, not 0). Detach `/mnt/system`, and `/mnt/overlay` without a persistent store, before the `/dev`, `/proc`, `/sys` moves, so switch_root can delete the initramfs's `/mnt`.
- **Rebuild:** 70_image.
- **Rig check:** SeaBIOS USB-stick boot with an empty CD drive: `grep -c "Can't open blockdev" serial.log` = 0 and `grep -c "failed to unlink mnt" serial.log` = 0, with and without a `KDOS_PERSIST` stick.

### L7 — services.md NetworkManager activation text
- **Fix:** replace `docs/kfs/16-wiki/services.md:187-190` with the ServiceUnknown rule, linking `configuration.md#etcservicedisabledname`. `docscheck --book docs/kfs`, `anchorcheck`, `make book-stamp` if kfscheck asks for it.
- **Check (host):** `grep -n 'can still start NetworkManager' docs/kfs/16-wiki/services.md` → nothing.

### L8 — `service list` table
- **Root cause:** `src/system/kdos-tools/ksvc.c:603-610` cmd_list sends the script's status output straight to the terminal.
- **Fix:** capture it with `kb_run_capture`; print the first non-empty line, trimmed to the width minus the 36-column prefix; add ` (+N more: service status <name>)` when there are more lines; print `—` when there is none; always end with a newline. Optionally make `fs/etc/init.d/55_tlp.sh` status print one `tlp:` line. Docs: `administration.md:40`, `:183-191` (replace '`02_modules` and `15_userdirs` print nothing'), `:187-188`; `docs/kfs/16-wiki/power-management.md:121`.
- **Rebuild:** kdos-tools (+ fs/ if 55_tlp changes).
- **Rig check:** `service list | awk 'NR>5' | grep -vc '^  [a-z]'` = 1 (the trailing blank line); the modules row shows `—`; utmps and timers show `+N more`; `grep -c 'System Info'` = 0.

### L9 — lirc.client
- **Fix:** `py3-client-import.patch` (`Subject: Import the _client extension from its package in lirc/client.py`); install `python-pkg/lirc/config.py` into purelib `lirc/` and byte-compile it; post-install `import lirc.client` check; kfs.md Patches and Notes. `make book-stamp`.
- **Rebuild:** `--phases 42_graphics,70_image --rebuild lirc`
- **Rig check:** `python3 -c 'import lirc.client, lirc.config; print(lirc.config.SYSCONFDIR)'` → `/etc`; `kpkg check lirc` clean.

### L10 — turtle/idlelib
- **Fix:** `ports/core/python/python3-tkinter/build.sh`: also copy `Lib/turtle.py`, `Lib/turtledemo` and `Lib/idlelib` (without `idle_test`), compileall, and install an `idle3` launcher. New python3-tkinter `kfs.md`; update `python3/kfs.md:15-17,:105`. `make book-stamp`.
- **Rebuild:** `--phases 42_graphics,70_image --rebuild python3-tkinter`
- **Rig check:** `python3 -c 'import turtle, turtledemo, idlelib.pyshell'`; `command -v idle3`.

### L11 — ImageMagick type-dejavu.xml
- **Fix:** drop `--with-dejavu-font-dir=/usr/share/fonts/TTF`; delete the kfs.md Flags bullet (it contradicts the shipped file); add a Notes line (the PostScript names fall back, because no URW base-35 port exists). `make book-stamp`.
- **Rebuild:** `--phases 43_toolkits,70_image --rebuild imagemagick`
- **Rig check:** no include in `/etc/ImageMagick-7/type.xml` names an absent file; `magick -list font | grep -c Font:` ≥ 200; `magick label:Hello /tmp/x.png` rc 0.

### L12 — unowned bootstrap leftovers
- **Root cause:** `script/phases/10_bootstrap/050_xz.sh`, `070_readline.sh` and `080_bash.sh` lack `--disable-nls`; they and 065_gawk/090_binutils install docs into `$SYSROOT`; `70_image/087_gates.sh:43` runs `unowned.py ... || true`. `writing-ports.md:1734` says only English ships.
- **Fix:** add `--disable-nls` (+ `--disable-doc` for xz) and remove the listed doc/man/info files after each bootstrap install (fresh trees). For the current tree, add a 70_image sweep step with the same list. Make 087_gates fail on unowned paths under `usr/share/{locale,man,doc,info}`. Docs: `build-system.md`, the docs/kfs 10_bootstrap pages; `kfscheck --repin`.
- **Rebuild:** 70_image (the sweep). The 10_bootstrap edits take effect only on a fresh tree; never re-run 10_bootstrap's other steps on this tree.
- **Rig check:** `ls /usr/share/man | grep -vc '^man'` = 0; `ls /usr/share/locale/*/LC_MESSAGES/{bash,xz}.mo | wc -l` = 0; host `python3 testing/unowned.py --root build/fs --fs fs` → no such path.

### L13 — libmed links
- **Fix:** after install, `ln -sf mdump4 "$PKG/usr/bin/mdump"` and the same for `xmdump`; kfs.md Notes.
- **Rebuild:** `--phases 41_system,70_image --rebuild libmed`
- **Rig check:** `grep -cx './usr/bin/mdump' /var/lib/kpkg/db/libmed` = 1 (and xmdump).

### L14 — Foomatic::PPD
- **Fix:** `perl-defined-array.patch` (`Subject: Test the array itself, not defined(@v), in Foomatic::PPD::_tag`); new kfs.md; post-install `perl -I... -e 'require Foomatic::PPD'`.
- **Rebuild:** `--phases 43_toolkits,70_image --rebuild foomatic-db-engine`
- **Rig check:** `perl -e 'require Foomatic::PPD; print "ok\n"'`.

### L15 — git svn/cvsserver
- **Fix (chosen):** drop them in `ports/core/vcs/git/build.sh` (the binaries, man pages and `Git/SVN*`), with the reason in git's kfs.md. Add a known-gaps row `git | git svn, git cvsserver | Subversion Perl bindings, DBI | one feature each`. `make book-stamp`.
- **Rebuild:** `--phases 30_foundation --rebuild git` (rebuild only; no `--snapshot`).
- **Rig check:** `git svn --version` → `'svn' is not a git command`; `git --version` unchanged.

### L16 — system accounts with no home
- **Fix:** avahi, mosquitto and ngircd build.sh: `install -d` their home in `$PKG`, plus `chown` in the existing postinstall; nut and geoclue: point `-d` at `/var/empty` or a directory they really use. kfs.md lines. avahi, mosquitto, ngircd and nut are already in the fix-final rebuild list.
- **Rebuild:** 41_system `--rebuild avahi,mosquitto,ngircd,nut,geoclue`
- **Rig check:** the passwd home loop prints only `polkitd /nonexistent` (and usbmux, whose daemon creates its own; see X-appendix).

### L17 — openconnect Android helper
- **Fix:** `rm "$PKG/usr/libexec/openconnect/hipreport-android.sh"`; kfs.md Notes.
- **Rebuild:** 41_system `--rebuild openconnect`
- **Rig check:** the file is absent; no `/system/bin/sh` shebang under /usr/libexec.

### L18 — front ends without desktop entries
- **Fix:** add `fs/usr/share/applications/<app_id>.desktop` (NoDisplay=true, Name, Icon from the atlas, Exec, StartupWMClass; shaped like kdos-peek.desktop) for kdos-audio, backup, bt, devices, disks, doc, find, firewall, net, netagent, openwith, pick, print, rec, settings, status, store, time, update, users, verify. Rewrite step 8 of `writing-desktop-software.md:1024-1027` (every toplevel ships a NoDisplay entry) and `kdos-shell.md:683` (the fallback is the first letter of the label). Optional preflight check: every app_id set beside `KDISP_ROLE_TOPLEVEL` has an entry.
- **Rebuild:** fs/.
- **Rig check:** open Super+I, F3, F6, `/` and Shift+F; `su - kdos -c 'kdos appid --quiet; echo rc=$?'` → rc 0; a 4× crop of the taskbar shows a picture on every plate.

### L19 — Settings tiles
- **Fix:** `settings.c:1980` `on ? fg : KT_MID`; CAT_TILE `:104` `panel` → `user-desktop`, and `:117` `computer-symbolic` → `drive-harddisk-system`. Regenerate the settings goldens in devdeps.
- **Rebuild:** kdos-shell.
- **Rig check:** Super+I, 2× crop: all 9 tiles carry a picture; blurb contrast ≥ 3.4.

### L20 — kdos-devices microphone sentence and dim notes
- **Fix:** record `npcm` in `scan_mics` (`devices.c:510-526`); at `:793` choose 'no card has a capture input' vs 'no sound card is present'; `:806-808` KT_DIM → KT_MID.
- **Rebuild:** kdos-shell.
- **Rig check:** `--audio`; `kdos-devices --dump | grep MICRO` → 'no card has a capture input'.

### L21 — kdos-devices first frame
- **Fix:** split `rescan()` (`devices.c:997`): the fast probes before kdisp_init; `scanimage -f` as an sh_job after it (`looking for scanners`); the `r` key starts a job too; the fixture path stays synchronous. Docs: `kdos-shell.md:1666-1668`.
- **Rebuild:** kdos-shell.
- **Rig check:** `--keys meta_l-f6 --shot d.png` with no sleep: the window is mapped and the SCANNERS row says 'looking for scanners'; no new 'did not respond to configure' line in kdos-comp.log.

### L22 — desk menu bottom frame
- **Fix (desk-local, no library change):** in `desk.c` after `ktui_menu_draw`, if `menu.y + menu.rows + 2 > h - 1`, set `menu.y = h - 1 - (menu.rows + 2)` and draw once more. Add a keydrive golden for desk at 80×24 (Shift+F10, selection on the last row). The libktui `floor` field is deferred: it would rebuild every port of ours.
- **Rebuild:** kdos-shell.
- **Rig check:** `--click 960,500,3 --sleep 2`; the pixel scan finds two border pairs, the bottom one above the hint row.

### L23 — clipped strings
- **Fix:** `status.c:565-567`: a sentence of ≤ 52 columns ('... in panel.conf go here.'); `devices.c:806`: `nx = max(20, 2 + ktui_utf8_width(head) + 2)`. Update `kdos-shell.md:1675` if the heading changes.
- **Rebuild:** kdos-shell.
- **Rig check:** `kdos-devices --dump | grep 'APPLICATION SETS ON A STICK'` is whole; the overflow chevron crop shows the whole sentence.

### L24 — Terminal=true entries
- **Fix:** add `NoDisplay=true` in the heredocs of tig, ledger, presenterm and w3m (keep their MIME/URL handlers); taskwarrior-tui and rmpc get `NoDisplay=true` (or a skel `.taskrc`, or the mpd opt-in). Docs: `docs/kdos/02-user-guide/applications.md`. `make book-stamp`.
- **Rebuild:** `--rebuild` tig (40_lang), presenterm and taskwarrior-tui (41_system), w3m (42_graphics), ledger and rmpc (44_apps).
- **Rig check:** `grep -L NoDisplay=true` over those entries lists only ones that stay open under `foot -e` for 5 s.

### L25 — AdwaitaLegacy
- **Fix:** new port `ports/core/themes/adwaita-icon-theme-legacy` (46.2, meson), published; a depends of adwaita-icon-theme; listed in `44_apps/packages.d/themes.txt`; ports-catalogue.md; `make book-stamp`.
- **Rebuild:** `--phases 44_apps --rebuild adwaita-icon-theme-legacy,adwaita-icon-theme`
- **Rig check:** `/usr/share/icons/AdwaitaLegacy/index.theme` exists; the inherits loop prints nothing; `timeout 10 kate 2>&1 | grep -c AdwaitaLegacy` = 0.

### L26 — FontForge pkg_resources; Gramps OsmGpsMap
- **Fix:** a fontforge `plugin.c` patch using `importlib.metadata.entry_points(group="fontforge_plugin")` (with a `Subject:` line) and a kfs.md entry. Gramps: a known-gaps row (`The Geography view | osm-gps-map | service (online tiles)`), or port osm-gps-map.
- **Rebuild:** `--phases 43_toolkits --rebuild fontforge`
- **Rig check:** `timeout 10 fontforge -quit 2>&1 | grep -c pkg_resources` = 0.

### L27 — catalogue metadata
- **Fix:** `src/system/kdos-appbox/catalogue`: grace tagline ('WYSIWYG 2D plotting of numerical data'); sizes for claws-mail, fityk and hugin; real meta for the 15 id-only rows, or delete those lines. `catalogue.c` is compiled into kinstall too; this file is data. Confirm whether 130_kinstall needs a re-run.
- **Rebuild:** kdos-appbox (44_apps, hash-driven).
- **Rig check:** `kdos-appbox catalogue | grep -E '^app\.(grace|fityk|hugin|claws-mail) '` shows a correct tagline and size.

### L28 — doctor podman check
- **Fix:** in the `kdos.c` Containers section (`:4330-4361`), when not root and podman exists, run `podman info --format {{.Store.GraphRoot}}` through the argv builder: warn with podman's first line on failure, ok with the path on success. Docs: the doctor check list in `kdos-command.md`.
- **Rebuild:** kdos-tools.
- **Rig check:** as kdos `kdos doctor | sed -n '/Containers/,/^$/p'` → ok with `/home/kdos/.local/share/containers/storage`; after `mkdir -p ~/.config/containers; printf '[storage\ndriver=\n' > ~/.config/containers/storage.conf` → `podman info` fails and doctor warns with its first line; remove the file afterwards (a `chmod 000` on the store does not break rootless podman, which runs in its own user namespace).

### L29 — install-to-disk.sh `packs` key
- **Fix:** `testing/install-to-disk.sh:31,50-51,78` → `APPS=${INSTALL_APPS:-essential}`, `apps = $APPS`; `testing.md:1571-1572`; `conf.c` `set_kv()` gets a final else printing `kinstall: <path>: unknown key '<k>' ignored`; the kinstall.md answer-file section. selftest.
- **Rebuild:** the script needs none; conf.c goes with M5 (130_kinstall).
- **Rig check:** `printf 'packs = x\napps = essential\n' > /tmp/a; kinstall --config /tmp/a --dump plan 2>&1` shows the unknown-key line and `apps essential`.

### L30 — vnc-shot.py root password
- **Fix:** in the `testing/vnc-shot.py:816-828` login loop, answer `assword` with `--root-password` (default `kdos`). Docs: the flag table in `testing.md` (~1167) and the install-to-disk.sh row (`:1579`).
- **Rebuild:** none (host).
- **Rig check:** after install-to-disk.sh, `$R --no-cdrom --disk disk.qcow2 --root-cmd 'hostname'` → `kdos-test`.

### L31 — kdos help/status strings
- **Fix:** `kdos.c:3257` ('built here from its recipe; installing needs a network'), `:3268` (`build and install one application — also remove`), `:3539` (`none — try: kdos app search <word>`). Grep docs/ for quoted help output.
- **Rebuild:** kdos-tools.
- **Rig check:** `kdos help | grep -c -e rollback -e 'is a mount'` = 0; the CONTAINERS line of `kdos status`.

### L32 — doctor RAPL warn / root 'predates'
- **Fix:** `kdos.c:4555` `warn_` → `skip_`; `:4596-4625`: warn 'predates' only when the running kdos-comp has the caller's uid; otherwise (and always for root) `skip_('kdos-comp runs in another account's session — run kdos doctor as that user')`, and the same for frame-timing. Docs: `docs/kfs/16-wiki/power-management.md:197`, `services.md:206`, `administration.md:1082-1083`.
- **Rebuild:** kdos-tools.
- **Rig check:** `[skip] no RAPL` for kdos and for root; no 'predates'; as root the 'another account' line; kdos doctor rc 0 on an installed VM.

### L33 — removing a user
- **Fix (docs):** `administration.md`: a 'Removing a user' section (`sudo pkill -u alice; sudo userdel -r alice`, why: the per-account ssh-agent from `50-ssh-agent.sh`, userdel exit 8); the users page in docs/kfs/16-wiki if there is one.
- **Check:** the documented sequence returns rc 0 and removes alice from passwd, subuid and subgid.

### L34 — mail spool / adduser names
- **Fix:** new `fs/etc/default/useradd` (`CREATE_MAIL_SPOOL=no`; GROUP/HOME/SHELL/SKEL as `useradd -D` shows today); `ports/core/auth/shadow/build.sh` `install -dm755 "$PKG/var/spool/mail"` with a kfs.md Notes line (login.defs is not edited); `users.c:226` and `:35` → `useradd`; `kdos-shell.md:2396` → `passwd, useradd, userdel, usermod`; administration.md and configuration.md `/etc` table.
- **Rebuild:** fs/; `--phases 30_foundation --rebuild shadow`; kdos-shell.
- **Rig check:** `useradd -D | grep MAIL` → `no`; `useradd -m bob; userdel -r bob` print nothing about mail, rc 0 twice; `kdos-users --dump | sed -n 3p` names `useradd`.

### L35 — boxsock/mediad arguments
- **Fix:** `src/desktop/kdos-boxsock/main.c:98-112`: refuse a non-alphanumeric first character; `-h/--help` → usage, rc 0. `src/desktop/kdos-shell/mediad.c:227-234`: when argc > 1, print usage (rc 0 for --help, 2 otherwise). Docs: kdos-appbox.md (boxsock), kdos-shell.md (mediad).
- **Rebuild:** kdos-boxsock, kdos-shell (50_desktop).
- **Rig check:** `kdos-boxsock --help` rc 0; `-x` → 'bad box name' rc 2; no `-help` socket; `kdos-mediad --help` exits; one mediad.

### L36 — `06_utmps` row
- **Fix (docs):** add the row between 05_hostname and 10_sysctl in `administration.md:80-145`, linking `boot-and-init.md#login-records`. docscheck, anchorcheck. kdos-doc picks it up through its hash.
- **Rig check:** `grep -c 06_utmps /usr/share/doc/kdos/02-user-guide/administration.html` ≥ 1.

### L37 — autologin message
- **Fix:** `src/daemons/kdos-powerd/main.c:540-551`: on a miss, `getpwnam()`; if the account exists, answer `err <who> is not an account that can log in (a system account or no login shell)`. Docs: daemons.md and kdos-command.md wherever they quote the reply.
- **Rebuild:** kdos-powerd (50_desktop).
- **Rig check:** `kdos-power autologin root` → 'not an account that can log in'; `nosuch` → 'no such account'.

### L38 — kdos-cursors skel copy (optional)
- **Fix:** drop `src/art/kdos-cursors/build.sh:14-15`; 050_theme.sh is the only writer (its `test -s` already guards it). Rewrite the `050_theme.sh:59-63` comment and `theming.md:663-664`. kdos-cursors is already in the fix-final list.
- **Rig check:** `kpkg check kdos-cursors` → 0 config rows.

### L39 — manual pages for kpkg*, ksvc/service, kinstall, xdg-terminal-exec (enhancement)
- **Fix:** widen `src/system/kdos-doc/mkbook.py:58` NAME_RE to these commands (they are the command-index.md rows at `:298-331`); update the header comment (`:9,:26`), `kdos-command.md:151` ('every command this repository ships') and kdos-doc's kfs.md.
- **Rebuild:** kdos-doc (41_system, hash-driven).
- **Rig check:** `man -w kpkg ksvc kinstall kpkgadd service xdg-terminal-exec` → paths for all of them.

---

## 3. Fixed at HEAD — confirm on the next ISO

Checks are numbered as in `fix-final.json` `iso_checks` (#0–#23); run those as written. Only checks that fix-final lacks are written out here.

| id | fixed by | confirm with |
|---|---|---|
| F1 | install.c BIOS boot partition, `bios_problem()` | #1, #2, #3, #4 |
| F2 | modemmanager/polkit build.sh `rm` activation files; fs/ deletes the NM file; `wait_bus_name` in 40/41/42 | #9; plus `grep -c 'Exited with code' /run/kdos-svc.{ModemManager,polkitd}.log` = 0 after 3 min, over 3 boots |
| F3 | ksvc.c state files, cmd_check | #14 |
| F4 | 40_dbus.sh `--nofork --nopidfile` | #10 (align the `[SKIP]` text with H2) |
| F5 | containers-common storage.conf | as kdos: `podman info --format '{{.Store.GraphRoot}}'` → `/home/kdos/.local/share/containers/storage`, rc 0; `kdos-appbox list` prints its header; #19 |
| F6 | 090_initramfs.sh `find_persist`/`usb_scan` | #8; plus a second boot with the same stick: `ls /root/m` (the marker written in boot 1) |
| F7 | 70_sshd.sh `PidFile=none`; 22_syslog.sh `-P /run/syslogd/...`; powerd/mountd `accept4(...CLOEXEC)` | #6; `timeout 20 kdos-power firewall ssh on` rc 0; `ssh off; sleep 7; pgrep -x sshd` → none; `service stop syslog; sleep 7; pgrep -x syslogd` → none |
| F8 | fs/etc/resolv.conf signature | `head -1 /etc/resolv.conf` = `# Generated by resolvconf` (or NM's block); `grep -c 'signature mismatch' /var/log/messages` = 0; no `/run/resolvconf/error` |
| F9 | `TARGET "/mnt/kinstall"`; dump.c partition names | #0, #1, #2 (font.bin, `term_font:`) |
| F10 | `KI_OOM_SCORE_ADJ -900` | #2, #5 |
| F11 | fs/ file deleted; 000_file_system.sh prunes manifest paths | #9 (activation files list empty) |
| F12 | 35_chrony.sh `install -d -m 0750` | `stat -c %a /run/chrony` = 750; `grep -c 'Wrong permissions' /run/kdos-svc.chronyd.log` = 0; `chronyc makestep` → `200 OK` |
| F13 | 110_iso.sh `sfs-var.pseudo` | #18; also on the installed first boot (with M6 applied) |
| F14 | 06_utmps.sh; utmps `runlvl-replace.patch` | #11 |
| F15 | 22_syslog.sh `-8` | #12 |
| F16 | 40_dbus.sh `wait_bus_name org.freedesktop.DBus` | `grep -a -c 'avahi-daemon.*Exited' /var/log/messages` = 0 |
| F17 | kdos-energyd main.c:351 | `su - kdos -c kdos-energy` → hint `service start energyd` |
| F18 | pcsc-lite build.sh; fs/etc/default/pcscd | #17 |
| F19 | install.c pump `case 'W'` | `grep -c '^\[..:..:..\] warning:' /var/log/kinstall.log` ≥ 1 when apps fall back to pending, with the reason |
| F20 | audio.c BT timeout; kdos.c hint; why.c; kdos-backup; packd.conf; kdos-kfs | #7 (packd.conf), #20 (backup); kdos-audio BT pane with bluetoothd running shows no 'not running'; `kdos why /usr/bin/kpkg` → `(none — provided by fs/ or 10_bootstrap, or not installed)`, since kpkg comes from 10_bootstrap and no package owns it; `ls /usr/share/doc/kfs` |
| — | the rest of fix-final (oomd rule, ALSA/BRLTTY, Samba, recording, text items, book data) | #5, #15, #16, #21, #22, #23 |

---

## 4. Documented gaps the audit touched, and docs that are wrong

### Documented gaps (behaviour matches the book)
- **G1 ksvc stop timing.** `ksvc.c:507-514` sleeps a fixed 1 s, then sends SIGKILL; `boot-and-init.md:1325-1327` and `daemons.md:93` state this mechanism. Missing from the book: the consequence (postgresql, samba and libvirtd lose a clean shutdown; 24 × 1.00 s at shutdown). Fix: poll `alive()` every 50 ms up to a grace period (default ~5 s; `supervise --stop-grace N` kept in the state file; postgresql, samba and libvirtd set longer ones). Also check the daemon, not only the supervisor (services unverified). Docs: those two pages plus `docs/kfs/16-wiki/writing-a-service.md`. Rebuild kdos-tools, and the ports that pass the option. Rig: time `reboot` on the serial log (expect < 10 s, no 1.00 s steps); the trap-sleep-3 test exits 0 on its own.
- **G2 `kdos-box create base=image:`** always pulls (`box_cmd.c:190-201`); a failed create leaves a profile (`cmd_box_remove`, rc==1 path). Documented at `kdos-appbox.md:273-280`. Fix: `podman image exists` first; remove the profile when there is no container; rewrite `:273-280` and the remove paragraph. Rebuild kdos-appbox. Rig: create from `localhost/t:1` → rc 0 with no pull; a failed create, then remove → rc 0, and the profile is gone from the list.
- **G3 update fetch.** `known-gaps.md:740-743`. `how-kdos-is-built.md:677-678` ('the image fetches no tree') is loose: `update.c:785-816` downloads before checking the key. Optional fix: load the keyring first and record `fetch no-key`; then update both pages.
- **G4 first login before userdirs.** `administration.md:342-343`. Optional: `10-wayland.sh` tests `[ -w /run/user ]` and prints a one-line hint.
- **L39 manual pages:** consistent with `kdos-command.md:151` (only `kdos-*` is promised). Listed as an enhancement.
- **Serial console asks for a password on an installed system** (`known-gaps.md:778-779`): correct; the harness does not handle it (L30).
- **Box base pack empties var/cache and var/log; packd `remove` keeps old kpacks; kdos-record 4:4:4; `kb_proc` pipes without CLOEXEC:** open per fix-final, not re-audited here.

### Docs that are wrong, or silent where they should not be (fixed in the listed items)
| page | problem | item |
|---|---|---|
| `docs/kfs/16-wiki/services.md:187-190` | says the bus can still start NetworkManager | L7 |
| `docs/kdos/03-architecture/session.md:152-158`, `docs/kfs/16-wiki/virtual-machines.md:300-310` | cover virtio only; non-virtio software GPUs not described | H1 |
| `docs/kdos/03-architecture/security-model.md:88-90` | 'every user can ping' — right as intent, false on the ISO | H3 |
| `ports-catalogue.md:1455` | GParted described as working | M11 |
| `ports/core/image-libs/imagemagick/kfs.md` | Flags bullet claims Sans/Serif/Mono get a font | L11 |
| `writing-ports.md:1734` | 'only English ships' — 281 leftovers say otherwise, and no gate enforces it | L12 |
| `kdos-shell.md:683` | fallback is 'one lowercase letter'; the tree draws the first letter of the label | L18 |
| `kdos-shell.md:2381`, `:1666-1668` | kdos-cal / kdos-devices first-frame behaviour | M18, L21 |
| `kdos-shell.md:2396`, `users.c:226` | name `adduser`/`deluser` | L34 |
| `kdos-command.md#kdos-shot` | an unknown mode exits 1, but `--help` and dash options hang | M8 |
| `administration.md:80-145` | no `06_utmps` row | L36 |
| `administration.md:40,183-191` | `service list` 'the script's own status line' | L8 |
| `administration.md:328-343` | no user removal procedure | L33 |
| `docs/kfs/16-wiki/power-management.md:197`, `services.md:206` | RAPL described as a warning | L32 |
| `kinstall.md:273,289,308-321` | swapoff -a; the copy excludes | M5, M6 |
| `testing.md:1571-1572` | `INSTALL_PACKS` does nothing | L29 |
| `testing.md` (~1167, 1579) | `--no-cdrom` second half of install-to-disk.sh cannot log in | L30 |
| `boot-and-init.md:435` | 'Always, with the udev rules from /usr/lib/udev and /etc/udev' | L1 |
| `theming.md:663-664` | skel cursors 'agree by construction' | L38 |
| `known-gaps.md` gnucash row (`:962`) | no 'Online price quotes — Finance::Quote, JSON, LWP, HTML::TreeBuilder, DateTime' | add the row; also gnucash kfs.md Notes |
| `known-gaps.md` | no git svn/cvsserver row; no Gramps Geography view row | L15, L26 |
| `writing-ports.md:1930-1943` | no VLC plugin-cache trigger row | M12 |
| `writing-desktop-software.md:1024-1027` | asks for desktop entries only for front ends that open files | L18 |
| memory note `buildfs-setuid-contamination.md` | says 18 setuid; the tree and security-model.md have 19 | not a repo file; update the memory |

---

## 5. Execution order

### Batches by rebuild cost
| batch | cost | items |
|---|---|---|
| A — host only | no build | L7, L29 (script), L30, L33, L36, the known-gaps rows; the gate scripts |
| B — fs/ only | 000_file_system.sh + 70_image | H1, H2 (40_dbus guard removal), L3, L5 (instrumentation, then the fix), L8 (55_tlp, optional), L18, L34 (`/etc/default/useradd`), G4 |
| C — initramfs / image scripts | 70_image | L1, L2, L6, L12 (sweep + gate) |
| D — bootstrap tools | 10_bootstrap steps | M12 (120_kpkg, hash-marked), M5, M6, L29 conf.c (130_kinstall; remove the mark) |
| E — toolchain / early phases | 30_foundation, 31_compilers `--rebuild` | M4 (gcc), L15 (git), L34 (shadow), fix-final utmps, ghc |
| F — third-party ports | one per port, per phase | C1, C2, H3, H4, M1, M2, M3, M10, M11, M13–M17, L4, L9–L11, L13, L14, L16, L17, L24–L26 + fix-final's port list |
| G — KDOS src ports (own hash each; rebuilt by selecting the phase) | seconds each | kdos-tools: H2, M7, M8, M9, L8, L28, L31, L32, G1 · kdos-shell: M18, M19, L19–L23, L34 (users.c), L35 (mediad) · kdos-boxsock: L35 · kdos-powerd: L37 · kdos-appbox: L27, G2 · kdos-doc: L39 + every docs/kdos edit · kdos-cursors: L38 |
| H — library | rebuilds every port of ours | none planned (the L22 libktui `floor` field is deferred) |

### Before building
1. Make the edits. For each new port (xhost, adwaita-icon-theme-legacy): `make fetch`, `ports/publish <port>`, commit the `sources.idx` line.
2. Prove M4's patch in kdos-devdeps (a `std::locale("")` test under `LANG=C.UTF-8`) and C2's flag (build mesa's Rust staticlib and `readelf` for LOCAL `__libc`) before spending the build.
3. Host gates: `testing/preflight.sh`; `testing/selftest.sh`; `CC="cc -fsanitize=address,undefined -g" testing/selftest.sh` (kinstall conf.c, cve.c); `python3 testing/kfscheck.py --repin` where includes moved; `make book-stamp`; `python3 testing/kfscheck.py`; `bash testing/docscheck.sh`; `python3 testing/anchorcheck.py`; `python3 testing/booknav.py`; `python3 testing/phaseclosure.py`; `make check`.

### Build commands, in order (one build carries every fix and fix-final's list; never pass `--snapshot`)
```sh
rm build/mark/bootstrap/kinstall
make build BUILD_ARGS="--phases 30_foundation --rebuild utmps,gcc,shadow,git"
make build BUILD_ARGS="--phases 31_compilers --rebuild ghc"
make build BUILD_ARGS="--phases 10_bootstrap --steps 10_bootstrap:120_kpkg.sh,10_bootstrap:130_kinstall.sh"
make build BUILD_ARGS="--phases 40_lang --rebuild bcc,tig"
make build BUILD_ARGS="--phases 41_system --rebuild wpa_supplicant,iputils,babeld,avahi,liblxi,tesseract,libmed,openconnect,mosquitto,ngircd,nut,geoclue,presenterm,taskwarrior-tui,modemmanager,polkit,pcsc-lite,containers-common,sysklogd,basu,brltty,postgresql,networkmanager-openvpn,maddy,prosody,radicale,samba"
make build BUILD_ARGS="--phases 42_graphics --rebuild mesa,xhost,lirc,python3-tkinter,w3m,minidlna"
make build BUILD_ARGS="--phases 43_toolkits --rebuild libvirt,vlc,imagemagick,foomatic-db-engine,fontforge"
make build BUILD_ARGS="--phases 44_apps --rebuild vice,fldigi,goldendict-ng,flamp,bcnc,linuxcnc,ledger,rmpc,gparted,adwaita-icon-theme-legacy,adwaita-icon-theme,mumble"
make build BUILD_ARGS="--phases 50_desktop --rebuild xdg-desktop-portal-wlr"
make build BUILD_ARGS="--phases 10_bootstrap --steps 10_bootstrap:000_file_system.sh"
make build BUILD_ARGS="--phases 70_image"
make book-data BUILDFS=build/fs
```
Notes:
- KDOS's own ports (kdos-doc, kdos-cursors, kdos-kfs, kdos-icons, kdos-theme, kdos-pack, kdos-splash, kdos-boxinit and kdos-bb in 41; kdos-tools in 42; kdos-appbox in 44; kdos-shell, kdos-boxsock, kdos-powerd and the rest of fix-final's 50_desktop list) rebuild through their changed hash when their phase is selected; they need no `--rebuild`.
- vlc is rebuilt after the kpkg step so the M12 trigger writes `plugins.dat`.
- The fs/ step runs after 41_system so pcsc-lite's upgrade sweep cannot unlink `fs/etc/default/pcscd` (fix-final ordering).
- gcc is selected only in 30_foundation. If the M4 gcc patch slips, drop `gcc` from line 2 and add `boost` to 41 and `gnucash` to 44 (the fallback).
- L15/L34: git and shadow use `--rebuild` only; nothing else in 30_foundation re-runs on this tree.
- If an interim is needed for M17, it lands with flamp; libdecor (42_graphics) only if the patch goes there.

### After building
Boot `build/iso-build/kdos.iso` (not the leftover disk images). Run §3 checks, then every §2 rig check. Order: the desktop comes up (virtio, then std VGA) → services (C1, H2, M1, M2, L3, L4, L8) → integrity (`kpkg check --all`, ELF scan with `ldd`) → apps sweep (C2 first, then the 389-entry sweep) → the install chain (install-to-disk.sh with L30, M5, M6, F1 on SeaBIOS) → 5–10 boots for L5.

---

## 6. Coverage

| area | exercised | not exercised |
|---|---|---|
| boot | 3 UEFI virtio boots with full log collection; UEFI std-VGA boot + poweroff; reboot with a timed serial log; SeaBIOS USB stick and El Torito CD; direct kernel boots (rescue, `single`); 2 persistence boots; offline: ESP, El Torito, Limine 12.9.0, unpacked initramfs (modules, microcode, udev scan) | installed-disk boot (A/B, LUKS, fsck, BIOS boot partition); the Limine 'clean session'/'verbose' entries; `emergency`; real hardware; `--gl`/virgl; the H1 retry path (only the pixman export was proven); the RTC on hardware |
| services | 4 boots: all 67 init scripts (list/status/logs), the pid files against live pids, the system bus (14 activatable names), polkit, nmcli, mmcli, avahi, cups, chrony, sshd, seatd, pcscd, syslog rotation, root daemon sockets and CLOEXEC, restart of 17 services, double start, stop against self-pidfile daemons, crash state, bus kill, rcK, hwsim Wi-Fi + hostapd | real BT/modem/printer/RAPL; any HEAD behaviour; `kpkg check --all` (timed out); full init shutdown; daemons with no config (snmpd, mosquitto, lircd, minidlna, nut, samba) and their pid files; whether the stop timing leaves a slow daemon running |
| integrity | 6 boots: `kpkg check --all` (2104 packages); ELF NEEDED + `ldd` over 16,539 files; dangling symlinks; setuid inventory (19, matches the book); world-writable files and ownership; pwck/grpck; caches (pixbuf, gio, mime, gschemas, fontconfig, icons); desktop-file-validate, Exec/TryExec, shebangs; 734 Python + 448 Perl modules; `perl -c` on 281 scripts; `man -w` for every command; /etc path references | installed system; GUI launches; box/pack contents; Python submodules and bytecode staleness; core_perl; pkg-config/.la; legacy BIOS |
| desktop | 5 boots at 1920×1080 with audio: panel, Start, palette, launcher, foot, snapping, workspaces, toasts and the centre, Settings, every panel popup, the desk menu, ~12 Super-key surfaces, lock (wrong password); rc.xml/menu.xml commands, favourites, Exec lines, session bus, portal Settings/Screenshot call, PipeWire, Xwayland (xkbcomp), clipboard, grim, doctor, appid | unlock (password unknown); real mic/BT/Wi-Fi/battery/brightness; the phosphor pass; ~17 smaller surfaces (store, pick, trash, peek, pix, rec, style, saver, ime, slit, update, firewall, backup, burn, verify, users, time); screenshot portal completion; logout, restore, crash-restart; an X11 client window |
| apps | 17 boots: all 389 visible entries launched (198 + 133 mapped; 58 classified); contact sheets; Icon/Exec resolution; gdb on representative crashes; readelf on the nouveau ICD and rusticl; podman/appbox on the live session; `kdos-appbox open` on ~15 types; mimeapps | real GPU (the nouveau/rusticl/GL results are VM-only); packs (the medium has none); a boxed app end to end; NoDisplay entries; interaction past the first window; mpv, cura, dosbox-staging, yamagi-quake2, sideband, moon-buggy left unresolved (no-GPU or no-data) |
| install | 1 full unattended install (1621 s) with an oom/PSI monitor; a live zram measurement; installed first boot (UEFI) and second boot (persistence, tty1 login, packd, a podman box); SeaBIOS boot of the disk + MBR dump | the interactive wizard; LVM, LUKS, reuse plan, swap file or partition; non-ext4; import route; pack boxes; NVRAM persistence (rig OVMF); oomd under real pressure; the stock harness against an installed disk |
| cli | 3 boots: ~190 commands located; `--help` for ~90 safe and the risky ones; every `kdos` subcommand in its read-only and error forms; `service` over 70 scripts; kpkg; packd/box/appbox; daemon clients (power, mount, energy, oomd, checkpass, resctl); the user lifecycle; hostname, timezone, keymap; backup; network/time/logs; `man -w` for 45 names; 379 documented paths | real-hardware media, smartctl, clone, persist create; `kdos march run`, update apply on slots, rebuild, a real `kdos app install`; installed-disk behaviour; reboot after hostname/timezone changes; `/etc/hosts` after a hostname change; the full `kpkg check --all` runtime |

Unverified leads (not filed, carried forward): kdos-update `--dump` '9 vulnerable' vs `kdos cve` 3; stray `(uint32 N,)` replies after `kdos remind`; the kdos-update `--dump` bottom-row overlap; `/etc/hosts` 127.0.1.1 after a hostname change; `/etc/hostname` with no trailing newline; deja-dup-monitor exiting; the ALSA Master vs PipeWire volume mapping; three stacked `login:` prompts on the first installed boot; depmod indexes unowned on an installed system; koreader's ssl RUNPATH; LinuxCNC's PyOpenGL EGL platform; under-linked libraries (libid3, libevent_extra/openssl, libefisec, libhpipp, libparted-fs-resize, libfzclient-private, libgrapheme, libwayland-*++); the supervisor log forwarder outliving a killed bus; `kdos-appbox open` glob vs xdg-mime disagreement.

---

## Appendix — refuted and rig artefacts (do not re-investigate)

| item | verdict | why |
|---|---|---|
| RTC interrupts / `hwclock -r` fails | rig artefact | QEMU HPET legacy routing of IRQ 8; `-machine hpet=off` fixes it. Optional one-line note in virtual-machines.md |
| fd0 I/O errors at session start and during install | rig artefact | QEMU i440fx exposes an empty floppy; kinstall excludes it (`disks 0`). Optional: `blacklist floppy`, or no FDC in vnc-shot |
| `/var/games/tetris-bsd.scores` 0666 | refuted | intended; bsd-games kfs.md:29-34 |
| PrusaSlicer G-code viewer segfault | rig artefact | no GLX in a GPU-less VM; re-measure with `--gl` after C2 |
| LibreOffice crash caused by `RUSTICL_ENABLE` | refuted | fails for every value; the cause is C2 |
| Clock popup 'never shows' | refuted | it shows late (M18) |
| udev helpers 'not on the image' | refuted | they are on the root, not in the initramfs (L1) |
| usbmux home missing | refuted | usbmuxd creates `/var/lib/lockdown` itself |
| nut `/var/lib/nut` | not used | state is `/run/nut`; L16 only repoints `-d` |
| `kpkgadd/kpkgdel --help` → unknown option; `kpkgdepends` echoes names | refuted | documented contract |
| libvirt nwfilter `config` rows in `kpkg check` | refuted | libvirtd rewrites them at start; `config` never fails |
| `kdos doctor` warnings as root (theme, foot, kdeglobals) | artefact of root's HOME/XDG_RUNTIME_DIR | the comp-socket part is L32 |
| smartd exit 17 `[DOWN]` | VM | no SMART devices |
| Two `ksvc supervise <name>` processes per service | by design | supervisor + log forwarder |
| Notification centre empty while a toast shows; taskbar keeps other-workspace windows | by design | documented |
| texstudio, qtcreator, gpxsee, spyder 'no window' | slow start | mapped by 60 s; flameshot, orca, kdeconnect-indicator are tray programs |
| NVRAM Boot entry gone on the next boot | rig | non-persistent OVMF vars |
