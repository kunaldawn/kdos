# kde.port.md — KDE Plasma replaces the KDOS desktop

KDOS deletes every cell-drawn desktop and ships KDE Plasma 6.8.x and all of KDE Gear as its one
desktop. KDE is customised and integrated so that it reads as KDOS from firmware to logout:
KDOS Modern look with an opt-in Phosphor mode, an SDDM login in the same style, Discover and
System Settings speaking A/B slots, packs and boxes, a native Kirigami installer, and an offline
machine that stays complete with no network.

Status: **plan, nothing implemented.** Decisions D1–D12 were made by the user on 2026-10-28;
the "Defaults the plan assumes" table (§1.3) was confirmed by the user the same day, with five rows
changed from the first proposal (A1, A5, B17, B25 and two D8 rows).
Evidence tags: `[measured: …]` was read in the tree or in KDE's sources, `[source: URL]` is
upstream, `[inferred]` is not yet measured and is a task to measure. Research data:
`~/.cache/kdos-portwork/kde-replace/` (`research.json`, `prior/`).

## Contents

1. [Decisions, rule changes and version set](#1-decisions-rule-changes-and-version-set)
2. [What is deleted, what survives](#2-what-is-deleted-what-survives)
3. [Platform: init, seat, session, login, boot](#3-platform-init-seat-session-login-boot)
4. [The KDE port set and how it is built](#4-the-kde-port-set-and-how-it-is-built)
5. [Identity: KDOS Modern and Phosphor mode](#5-identity-kdos-modern-and-phosphor-mode)
6. [KDOS-native integration](#6-kdos-native-integration)
7. [Innovations](#7-innovations)
8. [Documentation, tests, milestones and risks](#8-documentation-tests-milestones-and-risks)


## 1. Decisions, rule changes and version set

Section names in the "Implemented in" columns refer to the sections of this plan by title (Deletion, Session and login, Look, Integration, Software and updates, Installer, Ports and phases, Testing, Documentation), because the editor assigns the final numbers. HEAD at measurement: `a81bec1c` (branch `v0.2`, clean).

### 1.1 The goal

1. KDOS boots to an SDDM login and a KDE Plasma 6.8 Wayland desktop with all of KDE Gear. It is KDE-grade polished and visibly KDOS: the "KDOS Modern" look in eight accents (bone by default), plus an opt-in Phosphor mode.
2. Every KDOS concept is reached through native KDE surfaces. Boxes, packs, A/B slots, updates and firmware appear in Discover, System Settings KCMs, a tray plasmoid and KRunner. There is no second desktop and no cell desktop.
3. The system stays offline and from source. sysvinit, rcS and ksvc remain, with elogind added. Nothing downloads at run time. App sources are KDOS-only: no Flathub and no KNewStuff.
4. An OS update is a signed, atomic write into the inactive A/B slot with a "Restart to update" step. The stepping stone is a signed binhost. Building from source stays available.
5. Installation runs in a native Kirigami installer over kinstall's single backend, with an optional OEM first-boot hand-off to plasma-setup. The TUI stays for serial and unattended installs.

### 1.2 The twelve user decisions (final, 2026-10-28)

| # | Decision | What it means concretely | Cost / trap and its mitigation | Implemented in |
|---|---|---|---|---|
| D1 | Delete every cell desktop | **Delete** 7 of 8 `src/desktop` ports: kdos-comp, kdos-shell (55 command names, 54 surfaces), kdos-term, kdos-res (+ setuid `kdos-resctl`), kdos-lock (+ `kdos-checkpass`), kdos-record, xdg-desktop-portal-kdos [measured: inv-code]. **Delete** 8 libraries (58 files, 37,205 lines): libkwl, libkcell, libkchrome, libkicon, libkdisp, libkimg, libkvt, libkwm. **Delete** the art ports kdos-icons, kdos-cursors and kdos-gtk-theme. **Delete** the ports wlroots, seatd, xdg-desktop-portal-wlr, grim, slurp, wf-recorder, wl-mirror, wayvnc, wvkbd, fcft and tllist. **Delete** the console residue: `script/phases/70_image/070_user.sh:79`, `docs/kfs/15-image/070-user.md:53` and one more line [measured: inv-code verdict, "three residues"]. **Survive:** libktui, libkcolor, libkxdg, libkbase, libkproc, libkpkg, libksig and libkpack; kdos-bb. **Move:** kdos-boxsock goes to `src/system` | (a) xwayland's only declared reverse dependency is wlroots. The kwin recipe must add `xwayland` to `depends` (kwin `CMakeLists.txt:281`) [measured: inv-code verdict]. (b) kdos-appbox needs kdos-boxsock for each box's tagged socket (`src/system/kdos-appbox/main.c:60-130`), so boxsock moves before anything is deleted. (c) KWin rewrites a boxed window's app_id to the security-context id `app.<id>`. Every box then loses its icon and pinning unless genlaunchers also writes `app.<id>.desktop` [measured: res-innovation verdict I9]. (d) `fs/etc/skel` favorites must stay, or appbox warm-up must be re-sourced [measured: inv-code verdict]. (e) 24 reason files fail preflight once their cited paths are deleted, and about 400-470 links need retargeting, so they change in the same commit [measured: inv-code, inv-docs verdicts]. (f) kdos-oomd's protected list (`src/daemons/kdos-oomd/main.c:237-243`) must name kwin_wayland, plasmashell, ksmserver, kded6, sddm, elogind-daemon, ksecretd and xdg-desktop-portal-kde | Deletion; Ports and phases; Documentation |
| D2 | "KDOS Modern" + opt-in "Phosphor mode" | Breeze-grade base in KDOS colours. One global theme per accent, `org.kdos.<accent>.desktop`; bone is the default. A KDOS Plasma style, a KDOS window decoration, wallpapers, a login, splash and lock set, and monospace touches. Phosphor mode is a CRT KWin effect plus retro accents. It is off by default, controlled by one toggle, and offered on the Welcome tour | (a) The CRT effect is C++ against KWin-internal API and must be rebuilt, and often fixed, on every Plasma minor release. It ships as a built-in effect added by `ports/core/kde/kwin/kdos-crt.patch` (05 §5.4.2), because an out-of-tree effect's IID embeds KWIN_PLUGIN_VERSION_STRING and its loader refuses it at every 6.8.x point release [measured: effect.h:1134; effectloader.cpp:298-303]; the patch costs a rebase per release, and its shader lands at M5, before M6 deletes kdos-comp. (b) The plain rig runs KWin on llvmpipe, where the effect's `supported()` refuses to load. Photograph it with `vnc-shot.py --gl` or `make run-hw` [measured: res-innovation verdict 3]. (c) The breeze port is 6.7.5 with `WITH_DECORATIONS=OFF`, `WITH_WALLPAPERS=OFF` and `BUILD_CURSOR=OFF`. It must move to 6.8.2 with all three ON, or a misnamed wallpaper leaves the desktop blank and the cursor theme is missing [measured: res-branding verdict]. (d) A live accent switch does not change the wallpaper until plasmashell restarts, so `kdos theme` must set the wallpaper explicitly [inferred from code: res-branding verdict]. (e) Lock and logout folders in a global theme are ignored. The lock screen follows the Plasma shell package [measured: res-branding] | Look |
| D3 | Rule changes approved | Rule 3: elogind is allowed; there is no systemd init, no unit files and no supervisor. Rule 4: SDDM is allowed. Rule 5: KDOS code may link Qt 6/KF6 (KCMs, plasmoids, KWin effects, Discover backends, the installer). `.patch` files against KDE modules are allowed where no flag exists, each upstreamed in parallel. Exact text in 1.4 | (a) KDE modules install user units under `KDE_INSTALL_SYSTEMDUSERUNITDIR`, and `kpkg` lint refuses `usr/lib/systemd` (`src/system/kdos-kpkg/lint.c:143-144`). Keep the lint; each recipe drops its units [measured: res-session item]. (b) The preflight "desktop ELF links no toolkit" guard (`testing/preflight.sh:1302-1340`) fails the first KCM. It is rewritten in the same change (1.4.4). (c) About 226k lines of systemd-derived C enter the host through elogind [measured: res-session] | Session and login; Ports and phases; Documentation |
| D4 | Native integration everywhere, staged | Discover backend(s), KCMs (Boxes, Slots/Updates, Accent, Backups, Firewall, Users), an update tray plasmoid, a KRunner D-Bus runner, a ksystemstats plugin for boxes and energy | (a) libdiscover and the notifier plugin API are internal, so a KDOS Discover backend is version-locked to Discover [measured: res-innovation verdict I16]. (b) plasma-firewall 6.8.2 has only firewalld and ufw backends, uses SystemdJob/systemctl, and is gated `firewall? ( systemd )` in Gentoo. KDOS's nftables firewall needs a KDOS KCM, not plasma-firewall [measured: res-session missed] | Integration; Software and updates |
| D5 | KDOS layout + first-run "pick your desk" | Three global-theme layouts (`contents/layouts/org.kde.plasma.desktop-layout.js`): KDOS (default), Dock and Tiling, chosen on the first Welcome page and changeable later in Global Theme | Applying a layout later replaces the user's panels. The picker and the KCM must say so before applying [inferred] | Look; Integration |
| D6 | Floating windows + smart snap; auto-tiling bundled but disabled | KWin quick tiling, edge and quarter snap, and custom tiles (6.4 per-desktop layouts) are on. Krohnkite (MIT) ships disabled behind one toggle [source: res-innovation Q8] | Krohnkite is a third-party KWin script. Pin a release and re-test it on every Plasma minor release [inferred] | Look; Integration |
| D7 | Noto Sans UI + one vector KDOS monospace; Terminus only in Phosphor mode | `kdeglobals [General] font=Noto Sans,10`, `fixed=JetBrains Mono,10` (B17). Terminus TTF is set only by Phosphor mode | Neither Hack nor JetBrains Mono is ported. `ports/core/fonts` holds noto-fonts, noto-fonts-extra, terminus-ttf, ttf-dejavu and nerd-fonts-symbols [measured: `ls ports/core/fonts`]. A new font port is required. Pick it by measuring the 'M' advance, box-drawing and Powerline coverage, and hinting at 10 pt and 1.25 scale ("cell width is not max advance") | Look; Ports and phases |
| D8 | Best-of-breed default apps | KDE apps by default unless an already-ported app is clearly better. The one list is the D8 table in 1.3.2, confirmed; Look (Default applications) derives `/usr/share/kdos/xdg/kde-mimeapps.list` from it and holds no second list | `fs/etc/xdg/kdos-mimeapps.list` is replaced by `/usr/share/kdos/xdg/kde-mimeapps.list`; each change to it rewrites `02-user-guide/applications.md` and `desktop.md`. A desktop id in the list that the image does not install falls back silently, so `testing/mimecheck.py` (Look) gates it | Look; Documentation |
| D9 | SDDM + KDOS QML theme, Wayland greeter on kwin_wayland | SDDM built `-DNO_SYSTEMD=ON -DUSE_ELOGIND=ON -DENABLE_JOURNALD=OFF` (Alpine's flags). plasma-workspace built `-DINSTALL_SDDM_WAYLAND_SESSION=ON`; `sddm.conf.d` sets `DisplayServer=wayland` with the plasma-wayland.conf `CompositorCommand`. Plus sddm-kcm and a KDOS theme designed together with the splash and lock screen | (a) Upstream marks the greeter beta: plasma-workspace `CMakeLists.txt:14` defaults the session OFF, with "BETA, do not deploy to final users yet" at `:391`; SDDM `Configuration.h:41-42` says "Wayland support is experimental". It must pass a rig gate: boot, login, logout, user switch, lock, suspend/resume [measured: res-session missed]. (b) SDDM has had no tag since 0.21.0 (2024-02-26) and KDE is moving to PLM, which needs systemd (`plasma-login-manager CMakeLists.txt:57-71`) [measured]. KDOS may end up carrying sddm-kcm and the Breeze SDDM theme. (c) The greeter's `--inputmethod plasma-keyboard` needs qt6-qtvirtualkeyboard, which is missing. (d) Without locale1 the greeter uses the US keyboard layout, so kdos-settingsd lands with SDDM. (e) The KDOS splash quits at the end of rcS, so SDDM must start after it or DRM master races to a black screen [measured: res-session risks]. (f) KWin's CAP_SYS_NICE does not survive kpkg's tar, so RT priority needs `pam_limits` rtprio in the SDDM stacks [measured: prior] | Session and login; Look |
| D10 | KDOS-only, offline app sources | Discover shows ports, packs, boxes, A/B system updates and fwupd firmware. No Flatpak, no Flathub. KNewStuff is disabled: `kdeglobals [KDE Action Restrictions] ghns=false`, and the Discover KNS backend is not built. A curated offline add-on pack ships instead | (a) Discover needs qcoro (missing) and appstream rebuilt with `-Dqt=true`; appstream is 1.2.0 today [measured: `ports/core/*/appstream/kpkgbuild`]. (b) Without AppStream metadata for ports, packs and boxes Discover looks empty even when it works [measured: res-session risks]. Generating that metadata is a P0 task. (c) Some KDE modules still show "Get New…" buttons that the restriction greys out. Audit them per module [inferred]. (d) Two shipped timers download today: `10-update-check.timer` (`--fetch`, nightly) and fwupd's `30-fwupd-refresh.timer` (daily). A16 removes both downloads | Software and updates |
| D11 | Signed whole-slot images with deltas into the inactive slot; binhost as stepping stone; `kdos update apply --source` stays | `kdos-sysupdated` serves `org.freedesktop.sysupdate1`, and Discover is built `-DBUILD_SystemdSysupdateBackend=ON` (default OFF, `backends/CMakeLists.txt:73`) [measured: res-session verdict]. `kdos-bootctl mark-good` moves from the end of rcS (`fs/etc/init.d/rcS:186-188`) to after a healthy Plasma session, as KDE Linux's three-step bless does | (a) Only `Acquire` shows progress in Discover, so all download or build work happens in Acquire and Install is the quick deploy plus `kdos-bootctl try`. (b) The tray notifier needs a version-locked BackendNotifierModule or a KDOS plasmoid. (c) Reboot goes through login1 (elogind) [measured: res-innovation verdict I16]. (d) Reverses `decisions.md` "Updates build a signed tree from source" (:705) and `packaging.md` "no binhost takes part". (e) systemd 260 broke sysupdate for Discover once, so pin the API and test it on every Discover bump [measured: res-innovation Q2] | Software and updates; Session and login |
| D12 | Native Kirigami installer over kinstall's backend; Partition Manager for manual layouts; "Set up now" or OEM | The backend is `install.c`, `probe.c`, `conf.c` and `dump.c`: 0, 0, 0 and 1 libktui references against 274 in `pages.c`. It gains a JSON event stream (`--events`/`--progress json`). Partition Manager plus kpmcore (both missing) handles manual layouts. OEM writes a marker that arms plasma-setup through `/etc/sddm.conf.d/99-plasma-setup.conf` | (a) The backend links libkcolor (`install.c:2277-2279`, `kinstall.h:25-26`), so libkcolor survives [measured: res-session verdict]. (b) plasma-setup needs timedate1, hostname1, locale1 and qt6-qtvirtualkeyboard. It already falls back to SDDM autologin when systemd is absent (`bootutil.cpp:23-46`) [measured]. (c) The live-image installer entry moves to `/usr/share/applications` with `LiveEnvironment=true` and `LiveInstaller=` in plasma-welcomerc. The installer removes it from the installed system, and its `foot --app-id` exec changes [measured: inv-features verdict]. (d) kpmcore, `org.kde.powerdevil.*`, timedate1 and sddm-kcm need polkit grants that today's wheel rules do not give [measured: inv-features verdict] | Installer; Session and login |

Scope carried from earlier on 2026-10-28: full Plasma at the latest 6.8.x (73 modules), all of KDE Gear (252 modules at 26.08.2) and the frameworks they need. Of the Gear modules, 55 are present at 26.08.1 and 197 are new (196 inventory records plus kcolorchooser, which the records missed). Four are skipped (B25) and 193 are ported, seven of them late (B26) [measured: prior/gear-inventory.json `merged.summary`, `merged.skip`; Ports and phases]. Of the 73 Plasma modules, 13 are skipped (B24). The tree has 2,083 recipes under `ports/core` and 26 under `src`, 2,109 in all [measured: `find … -name kpkgbuild | wc -l`]. "All of Gear" in one meta port would overflow `KP_MAX_DEPS=128` and silently truncate its tail. Split the meta port, or raise the limit and add a check [measured: inv-code verdict].

### 1.3 Defaults the plan assumes

Every row was confirmed by the user on 2026-10-28 and is now binding on the sections that depend on it. Every default another section relies on has a row here, and the rulings list in Documentation does not repeat a choice that has one (kdos-mountd is B2, the `kdos` verbs are B27).

#### 1.3.1 System, session and process defaults

| # | Default | Recommendation | Reason | Evidence | Status |
|---|---|---|---|---|---|
| A1 | Translations | English only. Translations stay skipped: the 109 `-DKF_SKIP_PO_PROCESSING=ON` recipes keep the flag, every new KDE port passes it, and kpkg prune keeps deleting non-English catalogues and handbooks. Region & Language sets formats (dates, numbers, currency, paper) only | One language keeps build time, image size and the slot small, and kpkg prune, the 70_image locale gates and `writing-ports.md`'s Languages rule stay as they are. Cost: KDE's localisation is not offered; `known-gaps.md` keeps the English-only gap | user ruling 2026-10-28; `writing-ports.md:1760-1770`; `src/system/kdos-kpkg/prune.c` [measured] | confirmed |
| A2 | Per-user services (bus at `$XDG_RUNTIME_DIR/bus`, PipeWire, WirePlumber, pipewire-pulse) | turnstile 0.1.11 with a ksvc backend script (about 50-100 lines of sh) and `pam_turnstile` in the session stacks. During bring-up only, a session wrapper starts the bus and audio, then runs `startplasma-wayland` | It gives systemd --user semantics (supervised restart, lifetime equal to the user's logins), and Chimera proves it with Plasma 6.8. It replaces the unsupervised `&` launches in `fs/usr/local/lib/kdos/session-common.sh:172-181`. Trap: elogind and turnstile both want `/run/user`, so one owner must be chosen (Chimera's elogind no-runtime-dir patch plus turnstile `manage_rundir=true`, or the reverse). kdos-appbox hard-codes `/run/user/<uid>` and the bus path (`main.c:276-283`), so that path must not change | res-session Q4 and items [measured] | confirmed |
| A3 | polkit | Hybrid. Keep `fs/etc/polkit-1/rules.d/50-kdos.rules` (181 lines [measured: `wc -l`], wheel-group grants) for the KDOS daemons. Let elogind sessions enable upstream `allow_active` defaults. polkit-kde-agent-1 6.8.2 is the agent for everything else. The `seat` group tier (`fs/etc/group:29`, used by kdos-mountd, kdos-powerd and kdos-oomd, and kept by kinstall at `install.c:917-921`) becomes `allow_active` on the active login1 session, changed in one commit together with `security-model.md` | Session tracking wakes dormant `allow_active` defaults. Leaving the rules unchanged would silently widen grants, and removing the `seat` group without a replacement would drop a tier | res-session risks; res-session missed [measured] | confirmed |
| A4 | hostname1, timedate1 and locale1 | **kdos-settingsd**: new, at `src/daemons/kdos-settingsd`, C on basu, about 1,000-1,500 lines [estimate]. Rejected: openrc-settingsd 1.5.0 (4,136 lines, glib) | openrc-settingsd's localed keeps the layout in `/etc/X11/xorg.conf.d/30-keyboard.conf` (`src/localed.c:268`), which rule 4 forbids and `lint.c:145` (`etc/X11`) refuses. The Date & Time KCM saves only through timedate1, plasma-setup uses all three, and the greeter runs `kwin_wayland --locale1`. It must land together with SDDM | res-session item; verdict "partly-true" (the daemon need holds) [measured] | confirmed |
| A5 | Crash handler | None. KCrash with no handler; no DrKonqi and no drkonqi-legacy. A crashed app exits; KWin and plasmashell restart themselves; core dumps are the record | DrKonqi 6.8.2 requires systemd-coredump (`CMakeLists.txt:99-100`), and a downstream snapshot that may stop building on any KF or Qt bump is not worth carrying | user ruling 2026-10-28; res-session Q8 [measured] | confirmed |
| A6 | Power profiles | tlp-pd. Rebuild TLP 1.10.2 with `TLP_WITH_ELOGIND=1` and `make install-pd` (`ports/core/hardware/tlp/build.sh:12,16`), and run it supervised by ksvc. elogind owns suspend, power-off and the lid. kdos-powerd loses its power verbs | PowerDevil uses only `org.freedesktop.UPower.PowerProfiles` (`powerprofile.cpp:32-33`), and TLP keeps its tuning. Trap: charge thresholds have two possible owners, so PowerDevil's are disabled. tlp-pd is Python (gi). Its D-Bus service file is `SystemdService=`-only, so KDOS needs its own activation file or supervision | res-session Q9 and verdict [measured] | confirmed |
| A7 | Secret store | ksecretd + kwallet-pam. kwallet `build.sh:19` becomes `-D BUILD_KSECRETD=ON` and drops the `[KSecretD] Enabled=false` write at `:27-31`. `pam_kwallet5` goes into the sddm stacks. KeePassXC stays an app | Passwords then work for KDE, libsecret and QtKeychain apps with nothing else running. Trap: with autologin there is no password to unlock the wallet, so it prompts on first use. Document this in `security-model.md` | res-session Q10 and verdict [measured] | confirmed |
| A8 | Telemetry | Do not port kuserfeedback. Build Discover, plasma-welcome and the rest with `-DCMAKE_DISABLE_FIND_PACKAGE_KF6UserFeedback=ON`; the tree already does this in 3 recipes (dolphin, kate, labplot) | It is optional in Discover (`CMakeLists.txt:37`) and plasma-welcome (`:59-64`). Uploading usage data contradicts "offline by construction" and D10. This departs from res-session's recommendation A, because D10 postdates it | prior verdict ("keep it disabled"); `grep -rl DISABLE_FIND_PACKAGE_KF6UserFeedback ports/core --include=build.sh` = 3 [measured] | confirmed |
| A9 | Where KDOS Qt code lives | Reuse `src/desktop` for KCMs, plasmoids, the runner's Qt parts, the KWin effect and decoration, Discover backends, the SDDM theme and the installer front end | `bookcounts.py` `SRC_AREAS`, the 50_desktop `PORT_REPO` and CLAUDE.md's "four areas" stay valid. Only the kfsgen `AREA_DESC` text (`kfsgen.py:829`) and `decisions.md` "KDOS's own code is divided…" (:577) change | inv-code Q6; inv-docs Q3 [measured] | confirmed |
| A10 | Shortcuts | KDE defaults plus a thin KDOS layer. Meta+Space opens KRunner, which also keeps Alt+Space, Alt+F2 and Search. Ship `/etc/xdg/fcitx5/config` moving `EnumerateGroupForwardKeys` off Super+Space | Meta+Space is free among KDE defaults but is fcitx5's default group switch. KWin's filter runs first, so fcitx5 group switching would silently stop. Gate: a script greps every KDOS-added chord against the shipped `kglobalshortcutsrc` and fcitx5 config | res-innovation verdict 13 [measured]; inv-features Q7 | confirmed |
| A11 | Qt5 theming | Keep qt5ct 1.9 on its `kde` key (`ports/core/themes/qt5ct/kde-key.patch`). `kdos theme` writes the qt5ct colours from the KDE `.colors` file, and the qt5ct settings entry is hidden. Delete qt6ct | qt5ct is the only theming path for 28 Qt5 ports [measured: `depends` naming `qt5-qtbase`, grep over `ports/core/*/*/kpkgbuild`] (vlc, keepassxc, lmms, quassel, mumble, smplayer…), because plasma-integration is `BUILD_QT5=OFF` and there is no KF5. Trap: qt5ct and `write_qtct` must be kept or deleted together, or Qt5 apps paint qt5ct defaults | res-branding verdict; inv-code verdict [measured] | confirmed |
| A12 | Regression net for Qt code | Split `testing/selftest.sh` (C libraries and tools; runs on a bare host) from a new `testing/plasmatest.sh` (QtTest, qmllint, `kpackagetool6` installs into a temporary prefix, devdeps image). AT-SPI tree text goldens via KDE's selenium-webdriver-at-spi, which double as an accessibility gate. Pixel goldens only for the Phosphor shader. The 223 cell goldens are deleted, except `testing/fixtures/view`, which stays for the libktui view test | Text diffs are deterministic. Pixel goldens drift across Qt and Mesa versions | inv-docs Q4 and Q8 and verdict [measured] | confirmed |
| A13 | System updates in Discover | `kdos-sysupdated` (`src/daemons`, C on basu) implements `org.freedesktop.sysupdate1` (Manager.ListTargets and JobRemoved; Target.List, Describe, CheckNew, Acquire, Install, Vacuum, GetAppStream, GetVersion) over `kdos update` and `kdos-bootctl`. Discover's stock SystemdSysupdate backend drives it. No other systemd-named API is served on the user bus for now | It needs no Discover patch, because `isValid` only Pings the name (`SystemdSysupdateBackend.cpp:48-53`). The user-bus `org.freedesktop.systemd1` scope shim is deferred until KIO's scope path is measured; `_KDE_APPLICATIONS_AS_SCOPE=1` forces scope mode without faking a Version | res-session verdict; res-innovation Q2 and verdict 8 [measured] | confirmed |
| A14 | elogind configuration | `logind.conf`: `DelegateSessionCgroups=yes` (rootless podman box limits), `HandleLidSwitch=suspend` (PowerDevil takes the inhibitor in-session) and `KillUserProcesses=no`. seatd, `45_seatd.sh` and the `seat` group are deleted. `15_userdirs.sh` is reduced to the user.slice delegation, or removed | Sessions move to `/sys/fs/cgroup/<sid>`, and `/run/user` gets a per-login lifetime. Warm boxes and podman limits must be re-proven on the rig | res-session items and risks [measured] | confirmed |
| A15 | Lock screen as login on single-user LUKS machines | Opt-in. When the root is LUKS-encrypted and there is one account, kinstall offers SDDM autologin into a session that starts locked. Otherwise SDDM is always shown | The passphrase at boot already authenticates the user. Trap: the wallet has no PAM password to unlock with (A7) | res-innovation Q5 [source] | confirmed |
| A16 | Network timers | No shipped timer touches the network. fwupd's `build.sh` stops writing `/etc/kdos/timers.d/30-fwupd-refresh.timer` (the heredoc at `ports/core/boot/fwupd/build.sh:55-62`); a signed LVFS metadata snapshot carried by each release serves as a fwupd local remote, so Discover lists firmware offline (Software and updates, "Firmware in Discover"). `fs/etc/kdos/timers.d/10-update-check.timer` stays nightly but runs `kdos update check --out /var/lib/kdos/update.json` without `--fetch`, against the tree, cache or medium already present. The networked actions are the ones the user starts: Discover's "Check for updates" (which runs `CheckNew` with a fetch), `kdos update fetch`, `kdos update apply` and `fwupdmgr refresh` | D10 keeps "nothing downloads at run time", and both timers break it today: the update check runs `--fetch` nightly [measured: `fs/etc/kdos/timers.d/10-update-check.timer:18`] and fwupd refreshes from LVFS daily at 13:43 [measured: `ports/core/boot/fwupd/build.sh:61`]. Cost: firmware metadata is only as fresh as the last release or the last manual refresh | administration.md:216-224, :254; configuration.md:252, :453; filesystem-and-ipc.md:45, :67 [measured] | confirmed |

#### 1.3.2 Desktop components, apps and look defaults

| # | Default | Recommendation | Reason | Evidence | Status |
|---|---|---|---|---|---|
| B1 | kdos-res | Delete it with `kdos-resctl` and libkdisp (D1 lists it). plasma-systemmonitor with libksysguard 6.8.2 (`ENABLE_KAUTH_HELPER=ON`, libpcap off) is the GUI monitor, and btop 1.4.7 serves the console. A KDOS ksystemstats plugin restores per-box and energy (kdos-energyd) attribution at P1 | One fewer setuid helper, and fully KDE-native. inv-code recommended keeping a TTY version first; D1 overrides that | inv-code Q1 [measured] | confirmed |
| B2 | kdos-mountd and `kdos-mount` | Delete them (3,327 lines, 489 selftest lines, 42 fixtures). udisks2 2.11.2, Solid, the Device Notifier and ISO Image Writer replace them | Its only real consumer was kdos-shell. Dolphin and udisks2 calls already succeed for wheel users with no agent. Non-wheel users depend on A3 | inv-code Q7; inv-features verdict [measured] | confirmed |
| B3 | foot | Delete it with fcft and tllist. Konsole 26.08.x is the only terminal, plus Yakuake, which must be ported. gnuradio switches to `-DGRC_XTERM_EXE=/usr/bin/konsole` (`ports/core/sdr/gnuradio/build.sh:37`, check at `:89`), with `depends` and kfs.md Flags updated. Comments in 12 `fs/` files are rewritten | One terminal to theme. Konsole renders sixel, iTerm2 OSC 1337 and kitty APC images, so the four `X-KDOS-Term=kdos-term` recipes (yazi, rmpc, timg, presenterm) work unchanged | inv-code Q2 and verdict (OSC 1337 refuted) [measured] | confirmed |
| B4 | kdos-bb | Keep it as a terminal demo and change its kdos-term references to Konsole. Gate: measure Konsole's DEC mode 2026 synchronised output with the demo's timing harness | It links only aalib, libmikmod and ncurses, so it is not desktop code | inv-docs Q2 [measured]; mode 2026 at Konsole `:2354-2355` [inferred] | confirmed |
| B5 | Screenshots and recording | Port Spectacle 26.08.x (needs kquickimageeditor 0.7.0.1). Delete kdos-shot, kdos-record, grim, slurp, wf-recorder and wl-mirror. krdp replaces wayvnc. Rewrite the `decisions.md` narrowing "No compositor-private protocols" (:908), which exists to exclude Spectacle | KWin's ScreenShot2 and the portal are the capture paths. Spectacle 6.8 also captures audio | inv-code Q8; inv-features verdict [measured] | confirmed |
| B6 | Calendar, contacts and PIM | "All of Gear" brings in the Akonadi PIM stack (44 modules, 10 tiers deep, about 45 MB of source) on the SQLite backend, which is Akonadi's default; there is no MariaDB port and sqlite 3.53.4 already has `UNLOCK_NOTIFY`. Merkuro, KOrganizer, KAddressBook and KMail are installed; the default handler for mail, calendar and contacts is Thunderbird (D8 table), so no Akonadi server starts in the default session. The Digital Clock shows events through kdepim-addons. khal and khard stay as CLIs. kdos-cal and kdos-contacts are deleted with kdos-shell | Decided scope ("all of Gear"). It needs ktextaddons 2.2.0, kdiagram 3.0.1, KF6 kdav and krunner, and libkleo, kmbox and mimetreeparser bumped to 26.08.2, because messagelib pins KPim exactly | prior/gear-inventory `merged.pim` [measured] | confirmed |
| B7 | Backups | Kup, which ships in Plasma 6.8, with a KDOS restic backend (job, scheduler, KIO worker for browsing snapshots) carried as a `.patch` and upstreamed. restic 0.19.1 stays the engine and `kdos backup` stays the CLI | Kup supports only bup and rsync. Switching KDOS backups to bup would abandon existing restic repositories' tooling | inv-features Q12 [measured]; Kup in 6.8 [source: res-innovation] | confirmed |
| B8 | Union | Build Union, because plasma-workspace requires it, but leave the default `widgetStyle` on Breeze. Adopt a KDOS Union style only after KDE makes Union the default | Union is a 6.8 beta. Its style format changed between 6.7 and 6.8, and KDE's "Ocean" goal will extend it to Plasma, decorations and the login manager. Avoid investing in Breeze-specific SVG forks before then | res-branding Q8; prior (Union required, needs cxx-rust-cssparser) [measured] | confirmed |
| B9 | Icons and cursors | Breeze icons plus KDOS marks (logo, `start-here`, distributor logo) in a theme with `Inherits=breeze` and `FollowsColorScheme=true` in its `index.theme`; breeze_cursors. kdos-icons, kdos-cursors and kdos-gtk-theme are deleted. GTK apps get Breeze-GTK through kde-gtk-config, which needs sassc and xsettingsd | Without `FollowsColorScheme=true`, even inherited Breeze icons stop following the accent | res-branding Q6 and verdict [measured] | confirmed |
| B10 | Distribution defaults location | `/usr/share/kdos/xdg`, appended after `/etc/xdg` in `XDG_CONFIG_DIRS`, as Fedora and Kubuntu do. `/etc/xdg/kdeglobals` carries only `[KDE] LookAndFeelPackage=org.kdos.bone.desktop` | `/etc` stays the machine owner's | res-branding Q7 [measured] | confirmed |
| B11 | Accent and light mode | bone dark, with automatic day/night switching off, offered on a Welcome page. SDDM shows `LookAndFeelPackage` only, so the default must suit both day and night | The greeter never reads the day/night keys | res-branding Q3 and verdict [measured] | confirmed |
| B12 | Lock and logout screens | The stock Breeze lock and logout QML in KDOS colours, wallpaper and fonts. The SDDM theme is designed to match them. A KDOS shell-package override of `lockscreen/` and `logout/` is revisited after Union/Ocean lands | Global-theme `lockscreen/` and `logout/` folders are ignored in 6.8.2. A forked shell package would need a patch refresh on every release | res-branding Q5 [measured] | confirmed |
| B13 | CRT power-cycle motion (login unfold, lock collapse) | Part of Phosphor mode, so off by default. It is a KWin JS/QML effect, and the whole-screen collapse needs the C++ paintScreen effect | This keeps D2's "off by default, one toggle" unambiguous. res-innovation proposed it on by default | res-innovation Q1 and verdict 4 [measured] | confirmed |
| B14 | Default terminal profile | `/etc/xdg/konsolerc` with `[Desktop Entry] DefaultProfile=KDOS.profile` and `[LightDarkTheme] SyncProfileWithSystemTheme=true`, `LightThemeProfile=<Name=>`, `DarkThemeProfile=<Name=>`. The values are profile names, not file names | Without the sync key the light and dark keys do nothing | res-branding verdict [measured] | confirmed |
| B15 | Installer accounts and first boot | "Set up now" (the installer creates the account) is the default for self-installs. "Let the first person set it up (OEM)" arms plasma-setup | D12 | res-session Q3 [measured] | follows D12 |
| B16 | Cursor | One neutral cursor, `breeze_cursors`, for every accent; kdos-cursors deleted | KWin renders SVG cursors with a plain QSvgRenderer (`src/utils/svgcursorreader.cpp:126`), so an accent cursor costs eight themes of about 4.4 MB each and a cursor-theme switch per accent | res-branding item 16; Look (Cursor) [measured] | confirmed |
| B17 | Monospace face and its touches | JetBrains Mono, set explicitly as `fixed` and first for `monospace` in fontconfig. `testing/monofont.py` (box drawing, blocks, braille, Latin Extended, Greek, Cyrillic; cell width on `M`; 0/O and 1/l/I; Konsole shots at 1, 1.25, 2) gates coverage and records the cell metrics; ligatures off in the KDOS Konsole profile. Monospace touches: Konsole, Kate's editor, the panel clock, KSplash text, the SDDM clock and System Monitor numerals. Window titles stay Noto Sans | Tall x-height and clear 0/O and 1/l/I. plasma-integration's declared default is Hack (`CMakeLists.txt:83-87`), so the `fixed` key must be set or an unset key falls back to Hack's name and fontconfig's pick; `FontHack` stays off | user ruling 2026-10-28; Look (Fonts) [measured] | confirmed |
| B18 | Animation speed | `kdeglobals [KDE] AnimationDurationFactor=0.5` (KDE default 1) | KDOS motion is short; CachyOS ships 0.25 | Look (Polish defaults) [source: res-innovation reviews] | confirmed |
| B19 | Phosphor accent pairing | Turning Phosphor mode on with the bone or paper accent switches to the phosphor accent and remembers the previous one; amber, ice, norton, borland, perfect and phosphor are kept as chosen. Off restores the previous accent | Bone and paper are the two accents that do not read as a CRT | Look (Phosphor mode) [inferred] | confirmed |
| B20 | KDOS shortcut changes to KDE defaults | Two rebinds: Meta+Q closes the window (KDE: activities, which ship with no UI) and Meta+F1 opens the KDOS key card (KDE: "Switch to Desktop 1", which keeps Ctrl+F1). Additions on chords KDE leaves free: Meta+/ (the key card too), Meta+Return (Konsole), Meta+Shift+T (auto-tiling), Shift+Print and Alt+Print (Spectacle region and record), Meta+Ctrl+Shift+N (Night Light). Meta+Space is A10. Phosphor mode has no chord | KDOS heritage on chords KDE leaves free; the two rebinds replace a KDE binding, so they are the ones to veto. Every chord passes `testing/chordcheck.py` before it ships | `virtualdesktops.cpp:884`; Look (Shortcuts); Integration ("Key card overlay") [measured] | confirmed |
| B21 | Baloo | File-name indexing only (`baloofilerc` basic indexing; key name to verify on 6.31.0), with content indexing offered on a Welcome page; baloo rebuilt `BUILD_INDEXER_SERVICE=ON`; boxes, packs, caches and build trees excluded; indexing off on the live medium | Content indexing at first login is a CPU storm on a fresh install [inferred; measure on the rig] | Look (Polish defaults); Ports and phases (baloo row) [measured: `build.sh:19`] | confirmed |
| B22 | Linger for warm boxes | Warm boxes and user services stop at the user's last logout unless the user has a linger file (`loginctl enable-linger <user>`), read by turnstile's `linger = maybe`. `KillUserProcesses=no` and `RemoveIPC=no` (A14) keep a lingering user's boxes intact | A box that outlives every login holds memory and a session bus no one sees; linger makes that an explicit choice. Trap: the linger path elogind writes and the one turnstile reads must be the same; measure both | Session and login (Per-user services) [inferred; measure] | confirmed |
| B23 | KeePassXC and the Secret Service | KeePassXC stays an app, with its own Secret Service integration off by default; ksecretd owns `org.freedesktop.secrets` (A7) and starts first | Two providers of one bus name race at login, and the loser's clients see the other's empty store | Session and login (Session save and restore, crash handling, secrets, input method) [measured: `known-gaps.md:429-436`] | confirmed |
| B24 | Plasma modules not ported | 13 of 73: breeze-grub (Limine; `lint.c:146` refuses `etc/grub.d`), breeze-plymouth and plymouth-kcm (no Plymouth; kdos-splash owns boot), drkonqi (needs systemd-coredump; A5), flatpak-kcm (Flatpak REQUIRED; D10), kwayland-integration (a KF5 plugin; no KF5), plasma-firewall (firewalld and ufw only, SystemdJob; a KDOS nftables KCM replaces it), plasma-login-manager (needs systemd at build, greeter and KCM), plasma-bigscreen (TV), plasma-mobile, plasma-dialer and spacebar (phone), plasma-nano (a minimal shell for mobile and bigscreen; grep every port for `org.kde.plasma.nano` before closing) | "Full Plasma" means every module that can run on a Wayland-only, non-systemd, Limine-booted desktop | Ports and phases (Plasma 6.8.2) [measured] | confirmed |
| B25 | Gear modules not ported | 4 of 252: kjournald (libsystemd REQUIRED), kmag (`QScreen::grabWindow(0)` returns nothing on Wayland), kmousetool (X11 and XTest REQUIRED), plasma-settings (the Plasma Mobile settings shell). kcron is ported, with cronie as its cron: cronie runs as an rcS service under ksvc for user crontabs, and snooze keeps the system timers in `/etc/kdos/timers.d` | None of the four can work on KDOS. Two schedulers then coexist: snooze for what the system ships, cron for what users write | prior/gear-inventory `merged.skip` [measured]; kcron: user ruling 2026-10-28 | confirmed |
| B26 | Late Gear modules (W7) | Seven ship after v1 and do not block it: kaccounts-integration, kaccounts-providers and signon-kwallet-extension (accounts-sso stack, Qt6 qmake builds unmeasured on musl), colord-kde (needs colord and libgusb), audiotube (needs qt6-qtmultimedia's GStreamer backend and python3-ytmusicapi), kajongg (six Twisted ports), kigo (gnugo) | Each needs a third-party stack no other v1 module needs. Ports and phases counts 8 in W7 against these seven names; recount there | Ports and phases (Gear 26.08.2; W7) [measured: prior] | confirmed |
| B27 | `kdos` verbs whose data source goes | Kept and re-sourced: `notify` (`org.freedesktop.Notifications.Notify`; dismiss, raise, count and list dropped, exit 2), `doctor` (checks kwin_wayland, plasmashell, the KDE portal, login1, timedate1, the polkit agent, ksecretd, udisks2, kdos-sysupdated), `hey` and `appid` (an `org_kde_plasma_window_management` client; `kdos-box gc` needs it to find windowless boxes), `menu` (KRunner `querySingleRunner`), `panel toggle` (`evaluateScript`), `thumb` (cache only); `kdos-shot` becomes a wrapper over `spectacle -b -n`. Dropped: `stutter` (no frame socket under KWin; a KWin plugin is a P2 idea) | `hey` cannot be dropped while idle-box autostop uses it; everything else follows its KDE replacement | Integration (The `kdos` command) [measured: 06 table] | confirmed |

D8 default-application proposal: the one list the user confirms. Today's handlers are measured from `fs/etc/xdg/kdos-mimeapps.list`. Look (Default applications) turns this table into `/usr/share/kdos/xdg/kde-mimeapps.list` and must match it row for row.

| Role | Proposed default | Also installed | Today | Reason | Status |
|---|---|---|---|---|---|
| Web | Firefox ESR 153.5.0 | Falkon 26.08.x | firefox-esr | Site compatibility and extensions. Plasma Browser Integration's extension is preinstalled from the medium (D10) | confirmed |
| Mail, calendar, contacts | Thunderbird 153.5.0 (`mailto:`, `text/calendar`) | KMail, Kontact, Merkuro, KOrganizer, KAddressBook (B6) | thunderbird; kdos-cal, kdos-contacts | One app for all three, and no Akonadi server in the default session. khal and khard stay as CLIs | confirmed |
| Office | LibreOffice 26.8.0.3 | Calligra | libreoffice-* | Format fidelity, OOXML above all. Its `--disable-kf6` (`ports/core/office/libreoffice/build.sh:79`) becomes the KF6 VCL plugin so it shows KDE dialogs | confirmed |
| Files, archives | Dolphin, Ark | — | the same | Already KDE | confirmed |
| Text and code | Kate | — (KWrite is built off) | kate | One editor | confirmed |
| Images, PDF/PS/DjVu | Gwenview, Okular | — | the same | Already KDE | confirmed |
| E-books | Calibre viewer | Arianna | calibre-ebook-viewer | Format coverage; today's default | confirmed |
| Video | Haruna 1.8.1 | — | haruna | KDE and mpv | confirmed |
| Music | Elisa 26.08.x | Strawberry 1.2.30 | strawberry (19 types) | Kirigami, Baloo-fed, matches the look. Strawberry has the stronger tag and collection tools; the user may keep it | confirmed |
| Terminal | Konsole (+ Yakuake) | — | foot | B3 | confirmed |
| Screenshots, recording | Spectacle | — | kdos-shot, kdos-record | B5 | confirmed |
| Calculator | KCalc | Qalculate-Qt (stays installed) | kdos-shell surface | KDE native; KRunner keeps libqalculate for units and currencies | confirmed |
| Remote desktop (`rdp:`, `vnc:`, `spice:`) | Remmina 1.4.43 | KRDC | remmina | RDP, VNC, SPICE and SSH in one; KRDC lacks SPICE [inferred] | confirmed |
| Torrents | qBittorrent 5.2.3 | KTorrent (installed) | none set | Already ported; search plugins, RSS and sequential download in one | confirmed |
| Disk images (`application/x-cd-image` write) | ISO Image Writer | — | kdos-burn | Replaces `kdos-burn.desktop` | confirmed |
| `.zim` | Kiwix Desktop | — | org.kiwix.desktop | Offline library | confirmed |
| `geo:` | Organic Maps | Marble | — | Offline World.mwm | confirmed |
| Passwords | KeePassXC (Secret Service off, B23) | KeepSecret | — | Mature, browser integration | confirmed |
| `tel:` | KDE Connect | — | — | Upstream default | confirmed |
| Raster, vector, photo | GIMP 3.2.6 and KolourPaint (simple formats), Inkscape 1.4.4, darktable | — | the same | Clearly better than any KDE alternative | confirmed |

#### 1.3.3 Work items for the defaults

- [ ] A16, fwupd: drop the `timers.d` heredoc from `ports/core/boot/fwupd/build.sh:55-62` and rewrite its `kfs.md` Notes entry (`:96-99`) to state that the metadata comes from the release's local remote and `fwupdmgr refresh` is run by hand. Gate: after `--rebuild fwupd`, `ls build/fs/etc/kdos/timers.d` lists no `30-fwupd-refresh.timer`; `python3 testing/kfscheck.py` passes after `make book-stamp`.
- [ ] A16, fwupd local remote: carry a signed LVFS metadata snapshot in each release under `/usr/share/fwupd/remotes.d/` (Software and updates, "Firmware in Discover"), at milestone M5 with the Discover fwupd backend, not later. Gate: `fwupdmgr get-remotes` lists the KDOS snapshot remote; with the network down, `fwupdmgr get-updates` answers from it on the rig.
- [ ] A16, update check: `fs/etc/kdos/timers.d/10-update-check.timer` runs `kdos update check --out /var/lib/kdos/update.json` without `--fetch`, and its comment is rewritten: it names the update tray plasmoid and `kdos-sysupdated` as the readers of `update.json` (not the panel's badge or kdos-update) and states that the job reads only the tree already in place. Gate: a rig boot with no network shows no failed-fetch notice in the tray or Discover, and `kdos update check --json` records no fetch failure.
- [ ] Rule 1 pages for A16: `02-user-guide/administration.md` (`:216-224` timer example, `:254` timer table, firmware section), `06-reference/configuration.md` (`:252`, `:453`), `06-reference/filesystem-and-ipc.md` (`:45`, `:67`), `04-programs/kdos-command.md` (`:1094`), and `01-philosophy/principles.md:230` (1.4.3). Gate: `grep -rn 'check --fetch\|30-fwupd-refresh' docs/` returns nothing; `bash testing/docscheck.sh` passes.

### 1.4 Rule, convention and principle changes

#### 1.4.1 CLAUDE.md hard rules 3, 4 and 5

Current text [measured: `CLAUDE.md:88-105`]:

```
88  3. **No systemd.** No `systemd-*` on the host. See
89     [principles](docs/kdos/01-philosophy/principles.md#no-systemd) for the
90     replacements and what the choice costs.
92  4. **No Xorg server.** No `xorg-server`, no display manager, nothing X on the
93     login path, and `fs/etc/X11/` must never exist. Xwayland is the one X
94     server: it serves boxed applications and host applications that have no
95     Wayland path. X client libraries may be linked by any application that
96     needs them.
98  5. **Toolkits are for applications, never for the desktop.** Applications may
99     be ported natively with GTK 3/4, libadwaita, Qt 5/6, KDE Frameworks,
100    wxWidgets, FLTK or Tk, built with their Wayland backend as the run-time
101    default. The desktop itself (`kdos-comp`, `kdos-shell`, the daemons and
102    portals) draws cells and links no toolkit, and there is no Plasma or GNOME
103    shell. Why, and what is ported, is in
104    [`principles.md`](docs/kdos/01-philosophy/principles.md#toolkits-are-for-applications-not-the-desktop)
105    and [`applications.md`](docs/kdos/02-user-guide/applications.md).
```

Proposed text. The anchors change with the principle headings in 1.4.3:

```
3. **No systemd init, unit files or service manager.** PID 1 is sysvinit;
   services are `rcS` scripts supervised by `ksvc`, and per-user services run
   under turnstile's `ksvc` backend. `elogind` is the one logind provider and
   `basu` the one sd-bus library. A KDOS daemon may serve a documented subset
   of a freedesktop D-Bus API that systemd also serves (`sysupdate1`,
   `hostname1`, `timedate1`, `locale1`); `kpkg` refuses `usr/lib/systemd`, so
   no recipe installs a unit. See
   [principles](docs/kdos/01-philosophy/principles.md#no-systemd-init-units-or-service-manager)
   for each replacement and what the choice costs.

4. **No Xorg server.** No `xorg-server` and nothing X on the login path. SDDM
   is the display manager, built against `elogind`, and runs its greeter on
   `kwin_wayland` (`DisplayServer=wayland`); there is no X11 session.
   `fs/etc/X11/` must never exist and `kpkg` refuses `etc/X11`. Xwayland is
   the one X server: KWin starts it for boxed applications and host
   applications that have no Wayland path. X client libraries may be linked
   by any application that needs them.

5. **The desktop is KDE Plasma; KDOS's desktop code is Qt 6 and KDE
   Frameworks.** Plasma, KWin and KDE Gear run Wayland-only. KDOS code that
   is part of the desktop (KCMs, plasmoids, the KRunner runner, KWin effects
   and scripts, the window decoration, Discover backends, the SDDM theme and
   the graphical installer front end) lives under `src/desktop/` and may link
   Qt 6, KDE Frameworks 6 and Plasma's libraries, and no other toolkit. Code
   under `src/system/`, `src/daemons/` and `src/art/`, every `libk*` library
   and every program that runs as root or before the login links no toolkit.
   Applications may be ported natively with GTK 3/4, libadwaita, Qt 5/6, KDE
   Frameworks, wxWidgets, FLTK or Tk, built with their Wayland backend as the
   run-time default. A `.patch` against a KDE module is allowed under rule 9
   when no flag exists; it is offered upstream at the same time, and its
   `kfs.md` names the merge request. Why, and what is ported, is in
   [`principles.md`](docs/kdos/01-philosophy/principles.md#the-desktop-is-kde-plasma)
   and [`applications.md`](docs/kdos/02-user-guide/applications.md).
```

The rule 3 text names turnstile (A2) and the served APIs (A4, A13).

#### 1.4.2 Other CLAUDE.md lines that change

| Line(s) [measured] | Current | Proposed |
|---|---|---|
| `:20` | `The session bus, portals, audio, supervised chrome, the box environment` → session.md | `The login, elogind, per-user services (bus, PipeWire), portals, the box environment` → session.md |
| `:24` | `Where a window goes, tiling, snapping, workspaces` → window-model.md | Unchanged text; the page is rewritten for KWin quick tiling, custom tiles and the bundled auto-tiler (D6) |
| `:25` | `**Drawing anything** — colour, chrome, the pointer contract, glyph tiers` → design-language.md | `**Drawing anything** — the KDOS look kit: accents and colour schemes, Plasma style, decoration, fonts, Phosphor mode` → design-language.md (filename kept, so 28 bare links survive [measured: inv-docs verdict]) |
| `:26-29` | Rows for kdos-comp.md, kdos-shell.md, kdos-res.md, kdos-term.md | Four rows in slots 21-24 (inv-docs Q1 option A, which keeps the numbering of chapters 25-45). `KWin, its effects and the KDOS decoration` → `04-programs/kwin.md`; `The Plasma shell, layouts, KDOS plasmoids and the runner` → `04-programs/plasma-shell.md`; `KDOS settings pages (KCMs)` → `04-programs/kdos-settings.md`; `Discover, updates and the A/B slots` → `04-programs/discover.md` (08 §8.1.1) |
| `:41` | `**Writing a new surface**` → writing-desktop-software.md | `**Writing KDOS desktop code** — a KCM, plasmoid, runner or KWin effect` → writing-desktop-software.md (filename kept) |
| `:42` | `Tests, fixtures, goldens, the rig` | `Tests, fixtures, AT-SPI and QtTest, the rig` |
| `:168-169` | `- **Colour comes from a slot, never a literal.** `KT_MID` for labels; `KT_DIM` is a fill.` | `- **Colour comes from a slot or a role, never a literal.** In `libktui` code `KT_MID` is for labels and `KT_DIM` is a fill. In Qt code QML reads `Kirigami.Theme` roles and C++ reads `KColorScheme`; the KDOS accents reach Plasma only as the `.colors` files generated from `libkcolor`'s palette, and a colour literal in `src/desktop` QML or C++ fails preflight.` (A new preflight check; see 1.4.4) |
| `:187-188` | `make run # plain graphics: no phosphor pass` / `make run-hw # accelerated: the pass is on` | `make run # plain graphics: KWin on llvmpipe, Phosphor mode cannot load` / `make run-hw # virgl: Phosphor mode and the motion effects run` |
| `:209` | `A desktop program` → `--phases 50_desktop --rebuild <port>` | `KDOS desktop code (src/desktop)` → same command. Add a row `A KDE module` → `--phases <its phase>,70_image --rebuild <port>`, plus a note that a Plasma, KF or Gear bump rebuilds the whole release set (1.5.3) |
| `:264-267` | `$R --wait 24 --cmd 'kdos-res' …` | `$R --wait <measured> --cmd 'plasma-systemmonitor' …`. The wait is re-measured once SDDM autologin on the live image exists; `vnc-shot.py:139-145` and `:850-866` assume a compositor on tty1 and are rewritten first [measured: res-session missed] |
| `:274-275` | `testing/quick.sh kdos-shell -- --keys meta_l-ctrl-q …` | An example on a KDOS KCM, for example `testing/quick.sh kdos-kcm-boxes -- --cmd 'systemsettings kcm_kdos_boxes' …`, once quick.sh can restart plasmashell or kwin_wayland. Until then the example is removed [inferred: quick.sh rework is a Testing task] |
| `:279` | `cannot carry anything under fs/, a new port, or anything owning a D-Bus name` | Unchanged, plus: `nor a plasmoid or KWin plugin that the running plasmashell or kwin_wayland already loaded` [inferred] |
| `:290` | `The welcome card is open on first login. Dismiss it before any other shot.` | `plasma-welcome opens on first login. Close it before any other shot.` |
| `:303-304` | `A plain virtual display puts the compositor on software rendering, so the phosphor pass is never in a rig photograph.` | `A plain virtual display puts KWin on llvmpipe, where Phosphor mode and the CRT motion refuse to load; photograph them with vnc-shot.py --gl.` [measured: res-innovation verdict 3] |
| `:307-308` | `Goldens regenerate only where the Wayland dependencies exist` | `AT-SPI goldens and plasmatest.sh run only in the development image (testing/devdeps-image.sh), not on a bare host.` |
| `:343-346` | "four areas that hold ports (`system`, `art`, `desktop`, `daemons`)" | Unchanged (A9), with one clause added: "`desktop` holds the Qt code of rule 5" |
| New trap | — | `A Plasma, KF or Gear port at a version other than the release set's fails preflight; bump the whole set in one change.` (1.5.3) |
| New trap | — | `An XDG autostart entry any port installs runs in the only session; Plasma processes /etc/xdg/autostart and D-Bus activation that KDOS never ran.` [measured: res-session risks; res-innovation risks] |

#### 1.4.3 Philosophy pages to rewrite

Each rewrite replaces the old text and records no history (rule 2). The mapping from old surfaces to their replacements lives only in this plan and in commit messages. The book carries a present-tense task index [inv-docs Q5].

| Page:line [measured] | Heading | Intent of the rewrite |
|---|---|---|
| `principles.md:32` | `## No systemd` → `## No systemd init, units or service manager` | The function table gains rows for login and seats (elogind, not systemd-logind; replaces the `seatd` row at `:42`), per-user services (turnstile with a ksvc backend, not `systemd --user`), the settings APIs (kdos-settingsd, not systemd-hostnamed, -timedated or -localed) and OS update orchestration (kdos-sysupdated, not systemd-sysupdate). State what elogind brings (about 226k lines, musl patches) and what it buys (SDDM, user switching, PowerDevil, lock-before-sleep). The cgroup paragraph is rewritten for `/sys/fs/cgroup/<sid>` and `DelegateSessionCgroups` |
| `principles.md:80` | `## No Xorg server, and one carve-out` | It keeps the carve-out (Xwayland, started by KWin). It now states that SDDM is the display manager with a Wayland greeter and that no X session exists. "no display manager" is removed. It names the lint rule (`etc/X11`) as the enforcement |
| `principles.md:116` | `## Toolkits are for applications, not the desktop` → `## The desktop is KDE Plasma` | Plasma is the one desktop. KDOS desktop code is Qt 6/KF6 under `src/desktop`, and root, early-boot and library code links no toolkit; the reason is that anything running before the login or as root must not pull a GUI stack. Patches against KDE are carried only without a flag and are upstreamed in parallel. The cell-grid and libktui paragraphs shrink to the TUI programs that survive (kinstall TUI, kdosbuild picker, kdos-appbox) |
| `principles.md:173` | `## Everything that runs on the host is built from source` | Still true, but it now says where the build happens. Every binary is built from pinned source by the KDOS release (binhost, then slot images) and verified by signature. `kdos update apply --source` rebuilds on the machine. Remove any claim that updates compile locally by default |
| `principles.md:230` | `## Offline by construction` | Add the run-time half. KNewStuff is disabled (`ghns=false`), there is no Flatpak, no telemetry (A8), and the curated add-on pack is on the medium. No shipped timer touches the network (A16): the nightly update check reads only what is present, and fwupd lists firmware from the release's LVFS snapshot. The networked actions are the ones the user starts: Discover's "Check for updates", `kdos update fetch`, `kdos update apply` and `fwupdmgr refresh` |
| `decisions.md:44` | `## The compositor is a frozen fork of labwc` | Delete. Replacement entry: `## KWin is the compositor`, covering why Plasma and not a fork, and the cost of version-locked KDOS plugins |
| `decisions.md:222` | `## sysvinit as PID 1` | Keep, and add why elogind and not systemd 262 on musl. Upstream calls musl support "incomplete … without a promise of continued support" (NEWS:2778-2791), and SysV scripts have been removed since v260 [measured: res-session verdict]. Also add why not dinit |
| `decisions.md:242` | `## No KDE, GNOME or any existing desktop on the host` | Replace with `## KDE Plasma and all of Gear as the desktop`: why Plasma (polish, a Wayland-only 6.8, upstream non-systemd precedent in Alpine, Chimera, Gentoo and Void), why all of Gear, and the version-set policy (1.5) |
| `decisions.md:577` | `## KDOS's own code is divided by what each program is` | The `desktop` row becomes the Qt code of rule 5 |
| `decisions.md:705` | `## Updates build a signed tree from source` | Replace with `## Updates are signed slot images, with a binhost on the way`: the D11 end state, the binhost stage, the source fallback and the move of `mark-good` to after a healthy session |
| `decisions.md:758`, `:790` | `## Forking libtsm…`, `## One file chooser at one width` | Delete: libkvt and kdos-pick go, and xdg-desktop-portal-kde serves the file chooser |
| `decisions.md:816-908` | Narrowings: `Drops reach only the trash`, `Screens are ordered, not placed`, `Tabs stack; they do not tile`, `One font for every output`, `Braille and speech do not read the desktop`, `A tray menu is read once`, `Invitations are read, not answered`, `No compositor-private protocols` | Delete each narrowing that KWin and Plasma lift (Orca reads the desktop over AT-SPI; Spectacle works). Measure each before deleting it and keep any that still holds. `No ReGIS or Tektronix` goes with kdos-term |
| `decisions.md:915` | `### Nothing downloads at run time` | Keep, and add KNewStuff disabled plus the offline add-on pack (D10) |
| `why-kdos.md:11`, `:38` | `## What KDOS is`, `## The shape of the system` | KDOS is a from-source musl system with a KDE Plasma desktop, boxes for glibc apps and A/B image updates. The cell desktop description is removed |
| `why-kdos.md:73`, `:292` | `## Built from source, with named exceptions`, `## What is not built from source` | The release's signed binaries are built from this tree. No new exception is added unless binary updates bring one. Measure what the binhost and images carry |
| `why-kdos.md:188` | `## Native applications, and boxes for the rest` | Discover is the front door to ports, packs and boxes. No Flathub |
| `why-kdos.md:253`, `:269` | `## What is deliberately absent`, `## The system in numbers` | Absent now: systemd init, Flatpak, KNewStuff, telemetry, X sessions. The numbers are regenerated from the tree after the port |
| `how-kdos-differs.md:39-40` | At-a-glance rows `Init`, `Display` | `Display`: "Wayland only, KWin; rootless Xwayland for X11 clients; SDDM with a Wayland greeter". `Init` gains "elogind for seats and sessions" |
| `how-kdos-differs.md:104` (body at `:110`, "KDOS uses no systemd component") | `## Init and service supervision` | Rewrite `:110`, which becomes false. Name elogind as the one systemd-derived component. Rewrite `:130-133`: there is now a per-user service manager (turnstile), and socket activation is still absent |
| `how-kdos-differs.md:139`, `:162`, `:185` | `## The display stack`, `## Toolkits`, `## The desktop` | KWin plus SDDM. KDOS's own desktop code is Qt and KF6. The desktop is Plasma with the KDOS Modern look and Phosphor mode, set against Fedora KDE, KDE Linux, Alpine and Chimera |
| `how-kdos-differs.md:339`, `:386`, `:424` | `## Applications`, `## Updates`, `## The security model` | Discover with KDOS-only sources. Updates follow the KDE Linux model (signed slot images, deltas, "Restart to update"). The security model covers the polkit hybrid (A3), session tracking, ksecretd and box confinement under KWin's security-context |
| `how-kdos-differs.md:465` | `## What you give up` | Add Flathub, KNewStuff, DrKonqi's coredumpd flow and Plasma Login Manager. Remove the cell desktop's trade-offs |

Every row also rewrites the dependent pages these sections link into (`boot-and-init.md`, `session.md`, `security-model.md`, `packaging.md`, `design-language.md`, `window-model.md`, `known-gaps.md`, `docs/kfs` Part XIII). Gate for all of them: `bash testing/docscheck.sh`, `python3 testing/anchorcheck.py` and `python3 testing/booknav.py --write`, then a clean `make check`.

#### 1.4.4 The preflight toolkit guard

Current guard [measured: `testing/preflight.sh:1302-1340`]: for every `src/*/*/` package installed in `build/fs`, any ELF whose `NEEDED` matches

```
^(libgtk-|libgdk-|libadwaita-|libwebkit|libjavascriptcoregtk|libQt[0-9]|libKF[0-9]|libwx_|libfltk|libtk[0-9]|libX[A-Za-z0-9_-]*\.so)
```

fails `desktop ELF links`. The first KDOS KCM fails this guard, and so do the graphical installer front end and every plasmoid with a C++ plugin.

Replacement: `==> KDOS code links only the toolkit its area allows`.

| Area | Allowed toolkit `NEEDED` | Always refused |
|---|---|---|
| `src/desktop/*` | `libQt6*`, `libKF6*`, `libPlasma*`, `libplasma*`, `libkwin*`, `libKDecoration3*`, `libKirigami*`, `libDiscoverCommon*` (exact names measured on the first build) | `libQt5*`; GTK, GDK, libadwaita, WebKit, JavaScriptCore, wx, FLTK, Tk; Xlib (`libX*`) |
| `src/system/*`, `src/daemons/*`, `src/art/*` | none | the full current regex, including Qt and KF |
| Any setuid file, any binary under `sbin`, any program `rcS` starts | none | the full current regex, whatever the area |

Work items:

- [ ] Replace the block at `testing/preflight.sh:1302-1340` with the area-keyed check above, keeping its skip conditions (no build tree, no readelf). Its comment states the rule and its consequence: root, early-boot and library code that pulls in a GUI stack makes the login depend on that stack. Gate: on a tree with one `src/desktop` KCM installed, the check passes. Then add `libgtk-3.so.0` to that KCM's NEEDED (a test fixture ELF) and to a `src/daemons` package; both must fail. Prove the check fails before trusting it.
- [ ] Add a `src/desktop` colour-literal check (CLAUDE.md `:168` proposal). It greps `*.qml` and `*.cpp` under `src/desktop` for `#[0-9a-fA-F]{6}` and `Qt.rgba(`/`QColor(` with literals, with a named allowlist for the generated `.colors` writer. Gate: a fixture QML with `color: "#00ff00"` fails it.
- [ ] Rewrite the kdos-checkpass rationale at `testing/preflight.sh:1352-1366`. The shadow-mode check stays, but its stated reason becomes PAM (`pam_unix`) for SDDM and kscreenlocker [measured: inv-docs verdict]. Gate: `testing/preflight.sh` passes, and grepping for `checkpass` in `testing/` and `script/` returns nothing.
- [ ] Keep `src/system/kdos-kpkg/lint.c:143-145` (`usr/lib/systemd`, `lib/systemd`, `etc/X11`) and rewrite its messages to state the rule ("KDOS runs no systemd units", "KDOS has no X server configuration"). Gate: selftest's lint fixtures pass, and a fixture package with `usr/lib/systemd/user/x.service` is refused.
- [ ] Rule 1 pages for these four items: `05-developer/testing.md` (preflight section), `03-architecture/security-model.md`, `05-developer/writing-desktop-software.md`, `05-developer/writing-ports.md` (the systemd-unit and KDE-patch paragraphs).

#### 1.4.5 Work items for the rule changes

- [ ] Apply the 1.4.1 and 1.4.2 text to `CLAUDE.md` in the commit that lands the first Plasma port, not before: a rule that names a missing program misleads the reader. Gate: `python3 testing/anchorcheck.py` resolves the three new principle anchors, and grepping `CLAUDE.md` for `kdos-comp`, `kdos-shell`, `kdos-res`, `kdos-term` and `phosphor pass` returns nothing.
- [ ] Rewrite the 1.4.3 sections. Gate: `bash testing/docscheck.sh --book docs/kdos` passes, and grepping `docs/kdos/01-philosophy` for `seatd`, `no display manager`, `uses no systemd component` and `character-cell grid` returns nothing.
- [ ] Rewrite `kdos-powerd/main.c:23` ("logind does not exist on KDOS and is not coming") and the `seat`-group comments in kdos-powerd and kdos-oomd together with A3 [measured: res-session rule_change; inv-code verdict]. Gate: grepping for `seatd\|logind does not exist` in `src/` returns nothing.
- [ ] Extend `anchorcheck.py` and `docscheck.sh` to cover `CLAUDE.md` and README `<img src>` before the mass deletion [inv-docs Q7]. Gate: a deliberately broken anchor in `CLAUDE.md` fails the check.

### 1.5 Version set

#### 1.5.1 Pinned set and what the tree has

| Component | In tree [measured] | Target | Upstream state | Notes |
|---|---|---|---|---|
| Plasma | 4 of 73 modules at 6.7.5: breeze, kwayland, libksysguard, plasma-integration (`ports/core/kde/*`, `themes/breeze`) | 6.8.2 (2026-10-27, 73 tarballs) | 6.8.3 on 2026-11-10, 6.8.4 on 2026-12-01, 6.8.5 on 2027-01-05, 6.8.6 on 2027-03-16 (the last). 6.9 freezes 2027-01-05 and 6.9.0 ships 2027-02-23 [source: community.kde.org/Schedules/Plasma_6, via prior] | 6.8 is Wayland-only; kwin-x11 is gone. Every 6.8.2 module sets `QT_MIN 6.11.0` and `KF6_MIN 6.30.0` [measured: prior] |
| KDE Frameworks | 67 of 74 at 6.30.0 (`ports/core/kf6`; ECM in 41_system, the rest in 43_toolkits) | 6.31.0 (2026-10-08) | Monthly releases. Plasma 6.9 needs KF 6.34 [source: prior] | Missing: krunner and ksvg (hard requirements for KWin and plasma-workspace), syndication and kdav (PIM), kuserfeedback (not ported, A8), kapidox and oxygen-icons (skipped) |
| Qt | 6.12.0 (`qt6-qtbase`, `qtdeclarative`, `qtwayland`); `qt6-qtwebengine` 6.11.2 | 6.12.0; qtwebengine stays 6.11.2 until a 6.12 build is measured | Arch ships KWin 6.8.1 on Qt 6.12.0, and Alpine builds 6.8.1 on musl with Qt 6.11.1 [measured: prior] | Missing: `qt6-qtvirtualkeyboard` 6.12.0 (plasma-keyboard, greeter, plasma-setup), `qt6-qthttpserver` 6.12.0, `qt6-qtwebview` pinned to 6.11.2 to match qtwebengine [measured: prior/gear-inventory] |
| KDE Gear | 55 ports at 26.08.1 (`grep -rl '^version *= *26\.08'` = 55) | 26.08.2 (2026-10-06) | 26.08.3 [inferred: early November 2026]; 26.12.0 [inferred: December 2026] | Every present port's top-level CMake deps are identical between .1 and .2 except kdenlive (`BUILD_RUN_QMLLINT`; its patch becomes a flag) [measured: prior/gear-inventory] |
| plasma-wayland-protocols | 1.22.0 (`ports/core/kde/plasma-wayland-protocols/kpkgbuild`) | 1.23.0 | KWin 6.8.2 requires it | The only KWin minimum the tree misses [measured: prior]. Others hold: wayland 1.26.0, wayland-protocols 1.49, libinput 1.32.0, Xwayland 24.1.14, pipewire 1.6.9, mesa 26.2.3 [measured] |
| elogind | absent | **255.24 with Alpine's musl patch set** (10 patches, 385 lines) | Latest 257.16. Chimera runs 255.17 with 15 patches (862 lines), including no-runtime-dir [measured: prior verdict 7; res-session item] | 255.24 has a patch set proven on musl with Plasma 6.8. 257.16 is a P2 task: forward-port the patches and re-run the session gate |
| SDDM | absent | **0.21.0 plus Alpine's 0.21.0-r10 patches**; fallback: a pinned master snapshot (`633c1bf5`, 2026-10-20) published as a sha256-named tarball | No tag since 2024-02-26 [measured: res-session item] | First task: list what r10's patches carry, then build against Qt 6.12. If the Wayland greeter needs fixes that exist only in master, switch to the snapshot |
| turnstile | absent | 0.1.11 | — | A2 |
| polkit / polkit-kde-agent-1 | 128 / absent | 128 / 6.8.2 | — | — |
| accountsservice | absent | latest release; version to measure | Its meson build needs libelogind (`meson.build:241-245`) | Only the client library links logind [measured: res-session verdict] |
| Other prerequisites | basu 0.2.1, dbus 1.16.2, appstream 1.2.0, kirigami-addons 1.14.2, fwupd 2.1.7, TLP 1.10.2, fcitx5 5.1.22 | qcoro 0.13.0, kquickimageeditor 0.7.0.1, cxx-rust-cssparser 1.1.0 (Union), libqaccessibilityclient 0.6.0, kio-fuse 5.1.1, kpmcore and partitionmanager (Gear), ktextaddons 2.2.0, kdiagram 3.0.1, KDSoap and kdsoap-ws-discovery-client (SMB), sassc, xsettingsd; appstream rebuilt `-Dqt=true`; kirigami-addons 1.15.0 if a 6.8 module needs it | — | [measured: prior; inv-features verdict] |

#### 1.5.2 Bump policy

1. **One release set per family.** All Plasma ports share one version, all KF6 ports share one version and all Gear ports share one version. A family bump is one change that rebuilds the family. A mixed set fails preflight.
2. **Plasma.** Track the latest 6.8.x bugfix release and take each point release within its week. Plasma 6.9: start a branch at 6.9.0 beta (after the 2027-01-05 freeze), take KF 6.34 in the same change, and ship 6.9 on main at 6.9.1 [inferred: .1 follows .0 by about one week]. A move to 6.9 requires every version-locked KDOS plugin (Phosphor effect, decoration, Discover backend or notifier, KWin plugins) to rebuild and pass its gate.
3. **Frameworks.** Bump KF only when Plasma requires it or when a fix is needed, and always as the full 74. Recommendation: move to 6.31.0 at the start of the port, while the missing frameworks are added, because every Plasma and Gear port is new or rebuilt anyway and a later KF bump would rebuild them all again.
4. **Gear.** Take 26.08.2 now (55 bumps), then each 26.08.x. Take 26.12 after its .1, as one family change.
5. **Qt.** Take 6.12.x patch releases. qtwebengine and qtwebview move together and may lag qtbase by one minor version, as they do now (6.11.2 against 6.12.0).
6. **elogind and SDDM.** Keep them pinned. Bump only together with a re-proven patch set and the D9 rig gate (boot, login, logout, switch user, lock, suspend/resume, OEM first boot).
7. **Carried patches.** Each `.patch` against a KDE module has a `Subject:` line and an upstream merge-request link in its `kfs.md` Patches section, and is refreshed or dropped on every bump of that module.

#### 1.5.3 Work items

- [ ] Add a preflight check `the KDE release set is one version` covering `ports/core/kde/*`, the Plasma ports on other shelves (breeze and any others, found by a `group = plasma` marker or a list file), `ports/core/kf6/*` and the Gear list. Gate: bumping one Plasma port alone fails the check, and the full set passes.
- [ ] Bump plasma-wayland-protocols to 1.23.0. Gate: `make build BUILD_ARGS="--phases <its phase>,70_image --rebuild plasma-wayland-protocols"` succeeds, and `ports/publish plasma-wayland-protocols` adds its index line.
- [ ] Bump KF6 to 6.31.0 (67 ports) and add krunner, ksvg, syndication and kdav at 6.31.0. Gate: every `ports/core/kf6/*/kpkgbuild` reads `6.31.0`, `python3 testing/phaseclosure.py` passes, and the 43_toolkits phase builds.
- [ ] Bump Gear to 26.08.2 (55 ports) and convert kdenlive's patch to `-DBUILD_RUN_QMLLINT=OFF`. Gate: the release-set check passes, and `python3 testing/kfscheck.py` passes after `make book-stamp`.
- [ ] Measure the SDDM choice. Read Alpine's `community/sddm` r10 patch list, build 0.21.0 with `-DBUILD_WITH_QT6=ON -DNO_SYSTEMD=ON -DUSE_ELOGIND=ON -DENABLE_JOURNALD=OFF` in kdos-devdeps, and record in its kfs.md which patches apply. Gate: `sddm --test-mode` starts the greeter on `kwin_wayland` in the rig with `--gl`.
- [ ] Measure the elogind choice. Build 255.24 with Alpine's patches on musl 1.2.6 in kdos-devdeps. Gate: `loginctl session-status` reports `Type=wayland` and `Active=yes` for the SDDM session in the rig. Record 257.16's patch delta as a follow-up.
- [ ] Record the version set in `03-architecture/packaging.md` (a release-set section) and in `docs/kfs` Part XIII, replacing the cell-desktop text (rule 1).


## 2. What is deleted, what survives

Measured at HEAD `a81bec1c` (branch `v0.2`, clean) unless a row says otherwise. Reverse dependencies come from a parser over all 2,109 `kpkgbuild` files [measured: ~/.cache/kdos-portwork/kde-replace/verify-inv-code/deps.json]. D1 fixes the scope: every cell desktop goes, including `kdos-res` (no TTY mode is kept), the KDOS icon, cursor and GTK art, and every console/VT desktop residue. Nothing in this section is deleted before its KDE replacement is built, wired and rig-proven (2.7).

### 2.1 Totals

| What | Deleted | Kept / rewritten | Evidence |
|---|---|---|---|
| `src/desktop` programs | 7 ports: kdos-comp, kdos-shell, kdos-term, kdos-lock, kdos-record, kdos-res, xdg-desktop-portal-kdos — 376 files, 138,340 text lines | kdos-boxsock (4 files, 387 lines) re-homed to `src/system` | [measured: `git ls-files src/desktop/<p> \| xargs cat \| wc -l`] |
| `src/libs` | 8 of 17 libraries: libkwl, libkcell, libkchrome, libkicon, libkdisp, libkimg, libkvt, libkwm — 58 files, 37,205 lines | 9 survive: libkbase, libkbuild, libkcolor, libkpack, libkpkg, libkproc, libksig, libktui, libkxdg (minus `kxdg_verbs.c`, 235 lines) | [measured; file count 58, not 54: verify-inv-code verdict 1] |
| `src/art` | 2 ports: kdos-cursors (38 files, 4.5 MB), kdos-gtk-theme (48 files, 936 KB); plus the Papirus art inside kdos-icons (`art/`, `atlas/atlas.kia` 2.6 MB, `vendor.py`; most of its 23,280 files, 48 MB) and kdos-theme's `gtk\|icons\|cursors` subcommands | kdos-theme REWRITE (build-time generator of the Plasma identity kit, 05 §5.2.0); kdos-icons REWRITE (the KDOS/KDOS-Light marks theme, 05 §5.2.5); kdos-splash, kdos-bb | [measured: `du -sh`, `src/art/kdos-theme/kpkgbuild` description "GTK stylesheet, icons, cursors"; 05 §5.2.0] |
| `src/daemons` | kdos-mountd (4 files, 3,327 lines) + `fs/etc/init.d/58_mountd.sh` | kdos-powerd, kdos-oomd (rewritten lists), kdos-energyd, kdos-packd | [measured] |
| `src/system/kdos-tools` | hey.c 531, stutter.c 674, appid.c 273, thumb.c 495, speech.c 361 (2,334 lines); ~900 lines of `kdos.c` (5,071) | 29 other `.c` files, shot.c (272) rewritten as a Spectacle wrapper; reasons: 21 of 78 deleted, ~12 rewritten, 57 remain (2.6) | [measured: `wc -l`] |
| Selftest C | `src/libs/selftest.c` ~2,190 of 8,648 lines (`test_wm`, `test_vt_*`, `test_kvt_*`, `test_kimg`, `test_term_render`, `test_osc52`, `test_bracketed_paste`, the kxdg_verb call sites) | rest | [measured: inv-docs selftest.c row; selftest.c:37-40 includes] |
| Ports total | 28: 10 of ours (7 `src/desktop`, kdos-mountd, kdos-cursors, kdos-gtk-theme) + 18 third-party (2.4) | 2,081 of 2,109, before KDE additions | [measured: deps.json] |
| `ports/sources.idx` | 18 index lines across the 18 third-party ports (22 for the 20 measured, less terminus-ttf's 3 and whisper-model-base-en's 1) | — | [measured: sha256 grep against `ports/sources.idx`] |
| `fs/` (221 tracked files) | ~50 files (2.5) | ~40 rewritten, `favorites` kept | [measured: `git ls-files fs \| wc -l`] |
| Goldens | all 223 files of `testing/goldens/` (222 + README, 1.2 MB) | none | [measured; every one is a kdos-shell/kdos-res/kdos-term/libkvt surface: inv-docs verdict 7] |
| Fixtures | 303 files in 37 dirs: the 36 desktop dirs of inv-docs less `view` and `backup`, plus `stutter` (49) and `mountd` (42) | `backup` (3 files, the `kdos backup run` selftest, 2.3), `box catalogue clone cve energy oomd pack places polkit portup recent view`, and the libkproc part of `res` (282 files; subset to measure) | [measured: per-dir `git ls-files` counts] |
| Harnesses | `testing/barcheck.c`, `boxcheck.c`, `appindex.c` (332 lines) | — | [measured: boxcheck.c:11 "these live in kdos-shell"] |
| `testing/selftest.sh` | ~4,300 of 9,208 lines (desktop blocks ~3,765 + mountd 489 + stutter 6724-6755 + checkpass 247-268) | library, kpkg, services, A/B, LUKS, kinstall, daemons, cve, libktui views, theme audit (rewritten) | [measured: section headers; inv-code testing row] |
| `testing/preflight.sh` | ~400 lines deleted, ~1,000 of 3,477 touched | ELF guard, mimeapps, toggles, man-page check rewritten | [inv-docs preflight row] |
| Book | 4 chapters wholly about deleted programs (kdos-comp.md 1,398, kdos-shell.md 2,547, kdos-res.md 812, kdos-term.md 747 = 5,504 lines); `window-model.md` (565) rewritten; 20 of 22 screenshots | `boot-splash.png`, `tty-banner.png` | [measured: `wc -l`; `ls docs/screenshots`] |
| Book link debt | ~400-470 inbound links, 95 `#port-` links to deleted recipes, 83 `kfs:include` pins into deleted or changed files, 13 `docs/kfs/data/tier-a.txt` rows | — | [inv-docs verdicts 3, 5, 10] |

The `docs/` rewrite itself is section 9's inventory; this section names the pages each deletion forces.

### 2.2 `src/` programs and libraries

Actions: **DELETE** (no successor needed), **REPLACE-BY-KDE** (deleted once the named KDE module ships), **REWRITE** (kept, edited), **RE-HOME** (moved), **KEEP**.

#### src/desktop

| Port | Action | Size | Consumers (depends / runtime) | KDE replacement |
|---|---|---|---|---|
| kdos-comp (labwc 0.20.0 fork + 22 grafts) | REPLACE-BY-KDE | 246 files, 58,489 lines | depends: none. Runtime: `kdos hey` (kdos-cmd.sock), `kdos stutter` (kdos-frames.sock), `kdos appid` ledger, `kdos-box gc`, `kdos-session-save`, `kdos theme` SIGHUP list, kdos-powerd lid policy | kwin_wayland 6.8.x; crt pass → Phosphor KWin effect (D2); idle/lid → PowerDevil + kscreenlocker; ScreenSaver → ksmserver/PowerDevil; a11y → KWin plugins (stickykeys, slowkeys, bouncekeys, mousekeys, dwellclicker, magnifier, zoom, colorblindnesscorrection) [measured: kwin/src/plugins]; winpos → window rules; stack&tile/teams/`hey` (Haiku residue) → none |
| kdos-shell (55 basenames) | REPLACE-BY-KDE | 83 files, 66,791 lines | depends: xdg-desktop-portal-kdos. Runtime: `kdos-doc` is a kdos-shell symlink [measured: kdos-shell/build.sh:140]; `kdos-backup --once` from the backup timer; `kdos-prompt`, `kdos-pick`, `kdos-openwith`, `kdos-keys` behind `kb_have_prog` guards in kept tools | plasmashell, KRunner, System Settings KCMs, plasma-nm, plasma-pa, bluedevil, kscreen, Klipper, print-manager, kinfocenter, plasma-welcome; KDOS-only surfaces (update, store, backup, firewall, users, timezone, verify) → KDOS KCMs/Discover backends (section 6) |
| kdos-term | REPLACE-BY-KDE | 9 files, 2,816 lines | none; 4 recipes set `X-KDOS-Term=kdos-term` (yazi/build.sh:41, rmpc:51, presenterm:35, timg:43) | Konsole 26.08.1 (already ported). Konsole renders sixel, iTerm2 OSC 1337 `File=` and kitty APC images [measured: konsole Vt102Emulation.cpp:476, 642, 650, 690-707, 986], so no picture path is lost |
| kdos-lock + kdos-checkpass (setuid) | REPLACE-BY-KDE | 5 files, 869 lines | kdos-powerd main.c:1114 matches comm `kdos-lock`; selftest.sh:247-268; preflight.sh:1352-1366; kdosbuild.sh:50-51; 000_file_system.sh:146; kdos.c:4476-4490 | kscreenlocker over PAM (`/etc/pam.d/kde`, `unix_chkpwd` setuid from ports/core/auth/pam/build.sh:32). KWin compiles no ext-session-lock [measured: kwin/src/wayland/CMakeLists.txt] |
| kdos-res + kdos-resctl (setuid) | REPLACE-BY-KDE (D1: no TTY mode) | 23 files, 7,074 lines; 37 goldens; `fixtures/res` 282 files | none by depends; `testing/packlane.sh:229-232` dumps `--page boxes` | plasma-systemmonitor + ksystemstats; box and energy attribution → a KDOS ksystemstats plugin or the Boxes KCM (section 6); btop stays for the console |
| kdos-record | REPLACE-BY-KDE | 4 files, 735 lines | sole rdep of slurp and gst-plugins-ugly; one of xdg-desktop-portal-wlr | Spectacle (KPipeWire), OBS |
| xdg-desktop-portal-kdos | REPLACE-BY-KDE | 6 files, 1,566 lines | depends on kdos-shell (charnames.idx); selftest.sh:6788-6962 | xdg-desktop-portal-kde |
| kdos-boxsock | RE-HOME → `src/system/kdos-boxsock` | 4 files, 387 lines | `kdos-appbox.h:30` KDOS_BOXSOCK | n/a — KWin implements wp_security_context_v1 [measured: kwin securitycontext_v1.cpp; clientconnection.cpp:194] |

Trap carried with the re-home: KWin denies a sandboxed client only 7 globals [measured: kwin wayland_server.cpp:132-140], against kdos-comp's allowlist plus `kdos-grant.c` per-box grants, and `org.kde.KWin.ScreenShot2` checks only `calledFromDBus()` [measured: screenshotdbusinterface2.cpp:231]. Boxes gain globals they lacked. Mitigation belongs to section 7 (security): a carried KWin patch extending `restrictedInterfaces`, upstreamed in parallel (D3).

#### src/libs

| Library | Action | Size | Consumers | Note |
|---|---|---|---|---|
| libkwl | DELETE | 9 / 9,378 | kdos-lock, kdos-res, kdos-shell, kdos-term, libkchrome | Qt/KF6 for any new KDOS UI |
| libkcell | DELETE | 8 / 5,317 | kdos-shell, kdos-term, libkchrome, libkwl | not used by kdos-tools [measured: kdos-tools/build.sh:14-23] |
| libkchrome | DELETE | 6 / 2,996 | kdos-res, kdos-shell; testing/barcheck.c | |
| libkicon | DELETE | 5 / 1,683 | kdos-res, kdos-shell, libkchrome; reads `atlas.kia` | |
| libkdisp | DELETE | 2 / 875 | kdos-res, kdos-shell, kdos-term, libkchrome, libkwl; selftest.sh:24, 123-129 | D1 removes kdos-res, its last reason to stay |
| libkimg | DELETE | 2 / 1,121 | kdos-shell, kdos-term; selftest.c:40; selftest.sh:110-153 (KIMG_SRC, kimgfuzz) | |
| libkvt | DELETE | 21 / 15,084 | **kdos-term only** (+ selftest.c:38, fixtures/vt) | kdos-bb links `-laa -lmikmod -lncurses` only; kdos-tools does not link it [measured] |
| libkwm | DELETE | 5 / 751 | kdos-comp, kdos-shell; selftest.c:37 `test_wm` | |
| libktui | KEEP + REWRITE | 17 / 11,229 | kdos-installer, kdos-appbox, `script/kdosbuild.sh:29`, `script/phases/10_bootstrap/130_kinstall.sh:55-60` | `ktui_term.c:249` TERM detection names foot; add konsole. Includes only kbase.h/kcolor.h, so no deleted dependency |
| libkcolor | KEEP | 2 / 1,161 | kdos-tools, kdos-splash, kdos-powerd, kdos-installer, 70_image/050_theme.sh, 110_iso.sh, kdos-theme (the generator; `kcolor.h` `KCOL_SCHEMES` :60) | the accent numbers for console, splash and the generated Plasma schemes |
| libkbase | KEEP + REWRITE | 13 / 3,522 | every src port | only `kb_terminal()` needs code: `kb_fs.c:406-420` returns `"foot"` when `WAYLAND_DISPLAY` is set → konsole. `kb_desktop_prefix()` (kb_fs.c:386-403) already yields `kde` for `XDG_CURRENT_DESKTOP=KDE`, selecting `kde-mimeapps.list` [measured: verdict 13]. Comment `kb_proc.c:141` names kdos-lock |
| libkxdg | KEEP; DELETE `kxdg_verbs.c` | 8 / 2,585 | kept users kdos-appbox, kdos-tools use entry/exec/handlers/mime/places/recent | `kxdg_verb*` is called only from kdos-shell desk.c and pick.c **and** ~14 call sites in `src/libs/selftest.c`; `kxdg.h` declares `KxdgVerb` and the four functions — all three edits in one change. Successor: Dolphin service menus |
| libkproc, libkpkg, libkbuild, libksig, libkpack | KEEP | 39 files | — | no chrome identity list exists in libkproc (`kpr_ident` does not exist) [measured: verdict 12]; the comm lists to rewrite are in kdos-oomd and kdos.c |
| `selftest.c` | REWRITE | ~2,190 of 8,648 deleted | — | spans: `test_wm` 4876-5112, `test_vt_*` 5724-7308, `test_kvt_term` 7309, `test_kimg` 7505/7697, `test_kvt_drive` 7796-8072, `test_term_render` 8073-8189, `test_osc52` 8190, `test_bracketed_paste` 8258; `:2494 put_recipe(ldir, "src/art/kdos-theme")` stays valid (kdos-theme survives as the generator); `:5869-5873` links kdos-shell.md#settings |

#### src/system, src/daemons, src/art, src/devtools

| Port | Action | What changes | Evidence |
|---|---|---|---|
| kdos-tools | REWRITE | 2.3 | |
| kdos-appbox (15 files, 8,712 lines) | REWRITE | `main.c:327` `XDG_CURRENT_DESKTOP=KDOS`→`KDE`; `box_cmd.c:387` foot→konsole; `launchers.c:100` `SKIP_NEEDS_KWIN` emptied (Spectacle now runs); `launchers.c:67-77` RESERVED list drops foot, kdos-desktop*, kdos-pix, kdos-rec, kdos-theme (`kdos-shot` stays: the Spectacle wrapper is a host program); stale-launcher filtering (`sh_box_missing` lived in kdos-shell) moves into genlaunchers; `open.c:68,210,282` Open-With chooser `kdos-openwith` → `kioclient`/portal AppChooser; **`cmd_warmup` (main.c:649-680) reads `~/.config/kdos/favorites`** — keep the skel file as the box-warmup list, or re-source from Task Manager `launchers=` in `plasma-org.kde.plasma.desktop-appletsrc`; recommendation: keep `favorites` as a KDOS file (no parser of Plasma's applet config), rename its documented meaning to "boxes pre-started at login" | [measured: verdict 6; missed 1-2] |
| kdos-installer (10 files, 7,715) | REWRITE (TUI stays per D12) | `install.c:83,96` theme step writes Plasma defaults; `install.c:1765-1833` autologin edits `login.conf` → SDDM `[Autologin]` in `/etc/sddm.conf.d/`; `pages.c:1437` names foot; `pages.c:2093` "Back to the live desktop"; `install.c:917-921` (adds the user to `seat`) and `conf.c:22` go with the `seat` tier (03 §3.5, 01 A16) | [measured: inv-code] |
| kdos-doc (7 files, 466) | REWRITE | `fs/usr/share/applications/kdos-doc.desktop` `Exec=kdos-doc` runs the kdos-shell viewer; re-point to `xdg-open /usr/share/doc/kdos/index.html` (or khelpcenter) and ship a real `kdos-doc` wrapper in this port; `mkbook.py:22-29` makes a man page per `kdos-*` command-index row, so deleted commands drop out with the docs; preflight.sh:3441-3468 fails on an installed `kdos-*` without a page | [measured: verdict 6 of inv-docs] |
| kdos-pack, kdos-boxinit, kdos-kfs, kdos-kpkg, kdosbuild, kdos-portup | KEEP | comments only (`kdosbuild.sh:50-51` names checkpass/resctl as the setuid bits chown strips → name `unix_chkpwd`) | [measured] |
| kdos-powerd | REWRITE | `suspend`/`poweroff`/`reboot`/`autologin`/`timezone` verbs and `lock_before_suspend()` (`main.c:1090-1150`, waits for comm `kdos-lock`) deleted (03 §3.7): kscreenlocker locks on elogind's `PrepareForSleep`, PowerDevil owns the lid in a session and elogind `Handle*` outside one; the `kdos-power` client's `suspend`/`poweroff`/`reboot` map to `loginctl`; firewall table `main.c:325-328` row `vnc` "this desktop over VNC (wayvnc)" → re-describe for krfb (tcp 5900) and add `rdp` (tcp 3389) for krdp; the `KP_SEAT` tier (`main.c:29-30, 74, 958`) moves per 03 §3.5 | [measured] |
| kdos-oomd | REWRITE | `ko_protected[]` (main.c:240-252) becomes the list of 03 §3.7, every entry the 15-character `comm` that `is_protected()` compares with `strcmp` (main.c:259-260): `kwin_wayland`, `kwin_wayland_wr`, `plasmashell`, `ksmserver`, `kded6`, `plasma_session`, `kglobalacceld`, `kscreenlocker_g`, `Xwayland`, `xdg-desktop-por`, `polkit-kde-auth`, `ksecretd`, `sddm`, `sddm-helper`, `sddm-greeter-qt`, `elogind`, `turnstiled`, `dbus-daemon`, `ksvc`, `kdos-powerd`, `kdos-energyd`, `kdos-oomd`, `kdos-settingsd`, `accounts-daemon`, `wireplumber` (+ the pipewire prefix rule, unchanged); drop `kdos-comp`, `kdos-shell`, `kdos-desk`, `kdos-notifyd`, `seatd`. The elogind executable is `elogind` [source: elogind src/login/meson.build:77]; each comm is confirmed on the rig with `ps -o comm=` (03 §3.7); fixtures/oomd (80 files) follow; comment 225-238 rewritten; the `KO_SEAT` read tier (main.c:29, 62, 587-591) moves per 03 §3.5 | [measured: main.c:225-260] |
| kdos-mountd + kdos-mount | REPLACE-BY-KDE | udisks2 2.11.2 (already in 43_toolkits) + Solid + Device Notifier + Dolphin; TTY: `udisksctl`; wheel grants move into `50-kdos.rules` (2.5) | consumers: kdos-shell and kdos-comp kdos-child.c only [measured] |
| kdos-energyd, kdos-packd | KEEP | energyd's front ends (kdos-res pages, kdos-shell status) go; new front end = ksystemstats plugin / Boxes KCM (section 6) | |
| kdos-splash | KEEP | the fb0 boot splash in the initramfs (`090_initramfs.sh:146-560`); the KDOS ksplash/SDDM look is section 5 | |
| kdos-bb | KEEP | `src/bb.c:143` and two more lines name kdos-term/libkvt (DECSET 2026 via `kvt_term_sync_hold`) — reword; Konsole 26.08 implements DECSET 2026 [measured: inv-features verdict, konsole src/Vt102Emulation.cpp:2354-2355]. Only the timing is a rig task: the demo's timing harness shows no torn frame in Konsole on `--gl` | [measured: missed 8 of inv-docs] |
| kdos-theme | REWRITE (build-time generator) | delete the `gtk`, `icons`, `cursors` subcommands (`gtk.c`, `icons.c`, `cursors.c`) and `kdos theme`'s calls to them (`kdos.c:373, 398, 564-601`); add 05 §5.2.0's `kde-colors`, `konsole`, `qt5ct-colors`, `wallpaper`, `greeter-globals`, `contrast`, all from libkcolor `KCOL_SCHEMES` (`kcolor.h:60`); C, no Qt; selftest.sh:231-233 keeps compiling it; its `kfs.md` is rewritten for the generator (08 §8.1.5) | [measured; 05 §5.2.0] |
| kdos-icons | REWRITE (05 §5.2.5) | delete `art/` (Papirus, 23,254 SVGs), the atlas and `vendor.py`; keep `genmarks.py` and ship two `index.theme` (`KDOS`, `KDOS-Light`: `Inherits=breeze…`, `FollowsColorScheme=true`) with `start-here-kdos`, `distributor-logo-kdos` and `kdos` — the name `fs/etc/os-release:7` `LOGO=kdos` and the global themes' `[Icons] Theme=KDOS` need | [measured: fs/etc/os-release:7; 05 §5.2.5] |
| kdos-cursors, kdos-gtk-theme | DELETE (D1) | Breeze cursors (`breeze` is built `BUILD_CURSOR=OFF` → flip), breeze-gtk + kde-gtk-config (not ported); the KDOS identity kit is section 5. 070_user.sh's `rm -rf .icons .themes` loses its reason | [measured: verify-inv-code missed 9] |

#### Work items — survivors

- [ ] Rewrite `kb_terminal()` (`src/libs/libkbase/kb_fs.c:406-420`) to return `konsole`; fix `kb_proc.c:141`. Gate: `testing/selftest.sh` green; `grep -n '"foot"' src/libs` empty. Docs: `05-developer/c-libraries.md` (libkbase).
- [ ] `ktui_term.c:249` recognises Konsole's `TERM`/`KONSOLE_VERSION`. Gate: kinstall in Konsole on the rig renders the same as on tty2 (rig shot). Docs: `c-libraries.md` (libktui).
- [ ] Delete `kxdg_verbs.c`, the `KxdgVerb` declarations in `kxdg.h`, and the selftest.c call sites together. Gate: `grep -rn kxdg_verb src testing` empty; selftest green. Docs: `c-libraries.md` (libkxdg verbs section removed).
- [ ] kdos-appbox: the edits in the table; `favorites` kept and documented as the warmup list. Gate: boot ISO, log in, `pgrep -f 'conmon.*<favorite box>'` shows the box pre-started; `kdos-box gc` dry-run lists no box as "has a window" falsely (see 2.3 `hey`). Docs: `04-programs/kdos-appbox.md` (548-605, 606-759, 826-853), `06-reference/configuration.md` (favorites).
- [ ] kdos-oomd protected list as above, with the 15-byte comm rule. Gate: selftest oomd block with a fixture `/proc` carrying `kwin_wayland_wr`; `kdos doctor` reports oomd protected names. Docs: `04-programs/daemons.md#how-it-decides`, `03-architecture/overview.md:405`.
- [ ] `seat` group: owned by 03 §3.5 (decision 01 A16). The tier moves to the active elogind session — polkit `allow_active` for login1, `sd_uid_get_state()` for kdos-oomd's read tier — and the group is deleted from `fs/etc/group:29` and `install.c:917-921` last, in the change after kdos-powerd and kdos-oomd stop reading it (kdos-mountd is deleted here). Trap: openssh is built without PAM (03 §3.2), so an SSH login registers no elogind session; a non-wheel SSH user can no longer suspend or reboot, and wheel stays the SSH path. This section only keeps the deletion order: the group outlives seatd (step 7d) until 03 §3.5 lands. Gate: 03 §3.5's (`grep -rn '"seat"\|KO_SEAT\|KP_SEAT\|KM_SEAT' src` empty; selftest.sh:5584 rewritten to the new tier). Docs: 03 §3.5's list plus `security-model.md:287, 315-325`, `daemons.md:70-78`, `overview.md:403-409`, `administration.md:316-326, 439`, `installation.md:310`, `getting-started.md:260`, `04-programs/README.md:102`, `kinstall.md`, `filesystem-and-ipc.md` [measured: inv-docs missed 3].
- [ ] kdos-powerd: delete the sleep verbs and `lock_before_suspend()` (03 §3.7), map the client's `suspend`/`poweroff`/`reboot` to `loginctl`, firewall `vnc`→krfb and `+rdp` rows. Gate: `kdos-power suspend` on a laptop resumes onto kscreenlocker; `kdos-power firewall list` shows `rdp`. Docs: `daemons.md:248-303`, `administration.md` firewall.
- [ ] kdos-doc wrapper and `.desktop` re-point. Gate: `kdos-doc` on the ISO opens the book; preflight man-page check green. Docs: `04-programs/kdos-command.md` (kdos-doc row).
- [ ] kdos-installer TUI edits. Gate: `testing/install-to-disk.sh` run boots the installed disk into SDDM autologin when chosen. Docs: `04-programs/kinstall.md:43, 245, 290-292, 377-382, 428, 464+`, `02-user-guide/installation.md:317-341`.
- [ ] Move `src/desktop/kdos-boxsock` → `src/system/kdos-boxsock`; list it in `script/phases/42_graphics/packages.d/src-system.txt`, drop it from `50_desktop/packages.txt`; reword `main.c:11, 96`; fix the compile check path at `testing/selftest.sh:1207` (keep 1192-1212 outside the deleted span). Gate: `python3 testing/phaseclosure.py`; selftest boxsock check; `bash testing/docscheck.sh` (bookcounts places it under `system`). Docs: `repository-layout.md`, `ports-catalogue.md`, `packs-and-boxes.md`.

### 2.3 kdos-tools, subcommand by subcommand

Basenames from `main.c` TOOLS (`kdos`, `ksvc`, `service`, `kdos-getty`, `kdos-bootctl`, `kdos-shot`, `kdos-banner`, `kdos-fetch-app`, `kdos-fetch-static`, `kdos-sfx`, `kdos-mpctl`, `kdos-share`) and `kdos <cmd>` dispatch at `kdos.c:4999-5066` [measured]. kdos-tools links libkbase, libkcolor, libkpkg, libkxdg, libkproc, libksig, libkpack — **not** libktui or libkcell [measured: build.sh:14-23].

| Command | File | Action | Under Plasma |
|---|---|---|---|
| `kdos hey` | hey.c (531) | DELETE | kdos-cmd.sock dies with kdos-comp. Callers rewritten: `kdos-session-save`, `testing/appsweep.sh:223`, `packlane.sh:269`, `kdos-box gc`. `gc` autostop asks "has this box a window" — re-implemented over a KWin script (`workspace.windowList()` → `resourceClass`/`pid`) loaded through `org.kde.KWin /Scripting loadScript`; until then gc's documented behaviour is "leaves every box running" [measured: kdos-appbox.md:1150-1160] |
| `kdos stutter` | stutter.c (674) | DELETE | kdos-frames.sock has no KWin equivalent; fixtures/stutter (49) and selftest.sh:6724-6755 go; reason `blocked-before-busy.txt` goes |
| `kdos appid` | appid.c (273) | DELETE | the ledger was fed by kdos-comp; no successor verb (06 §6.7 drops its row and `io.kdos.Runner.RecordAppId`). The app_id patches (goxel, keepassxc, urh) still matter because Task Manager matches `desktopFileName` the same way; genlaunchers' rig gate checks them. selftest.sh's `kdos appid` block (from :9036) goes |
| `kdos-shot` | shot.c (272) + symlink | REWRITE (06 §6.7) | a thin wrapper over `spectacle -b -n -{f,m,r,a} -o`; `qr` = region capture + `zbarimg` (`ports/core/image-libs/zbar`, ported); grim needs wlr-screencopy/ext-image-copy-capture, KWin compiles neither [measured]. Gate: `kdos-shot qr` decodes a QR on screen (06 §6.7) |
| `kdos thumb` | thumb.c (495) | DELETE | ffmpegthumbs + kdegraphics-thumbnailers (ported) write the same `~/.cache/thumbnails`; ffmpegthumbnailer goes with it (2.4); 06 drops its `thumb` row; selftest.sh:3118-3152 goes |
| `kdos speech` | speech.c (361) | DELETE | only caller kdos-shell rec.c. whisper-model-base-en stays: the Dolphin "Transcribe" service menu (06 §6.1, 07 §7.4.5) calls `whisper-cli` with it directly, not through a `kdos` verb (2.4) |
| `kdos panel`, `kdos menu` | kdos.c:200-209, 4786-4861 | DELETE | KRunner; `menu.conf` routes → KDOS KRunner runner (section 6); 06 §6.7 drops its `panel`/`menu` rows (no `evaluateScript` verb) |
| `kdos settings` | kdos.c:4757-4785 | REWRITE | `systemsettings <kcm>` / `kcmshell6 <kcm>` |
| `kdos notify` | kdos.c:4862-4990 | REWRITE | keep summary/body, `--time`, `--battery` over `org.freedesktop.Notifications`; drop `--dismiss`, `--dismiss-all`, `--raise`, `--dnd` (kdos-notify.sock, kdos.c:4949) |
| `kdos toggle` | toggle.c (126) | REWRITE | night-light → KWin `org.kde.KWin.NightLight` / kglobalaccel "Toggle Night Color"; dnd → Plasma notification inhibition; stay-awake → `org.freedesktop.ScreenSaver.Inhibit`; preflight.sh:2752-2779 toggle checks rewritten |
| `kdos theme` (+ `--audit`, `style`) | kdos.c:301-3236; themeaudit.c (397) | REWRITE (05 §5.3) | stops writing what 05 §5.3 lists: `write_themerc` (1465-1651), `write_foot` (710-799), `write_kdosterm` (800-893), the qt6ct half of `write_qtct` (2558-2667), `style_write_comp` crt keys (2999-3148), the `kdos-theme gtk\|icons\|cursors` calls (373-601), `kde_palette()` and the `[Colors:*]`/`[WM]` merge into `~/.config/kdeglobals` (2182-2253, 2477-2528) with `TerminalApplication=foot`, the wallpaper retint into `~/.cache/kdos/wallpaper.png`, the SIGHUP list, `--preview`. Plasma schemes, Konsole schemes and qt5ct colour files are generated at build time by kdos-theme (2.2); `kdos theme` applies the global theme / colour scheme of the chosen accent under 05 §5.3's KDOS-owned guard; **keep the qt5ct half** — qt5ct carries `kde-key.patch` and answers platform theme `kde` for Qt 5 apps, so dropping its writer while keeping qt5ct paints qt5ct defaults [measured: verdict 7]; `plasma-apply-colorscheme` ignores a scheme whose name equals the current one, so a same-accent re-apply must switch away and back or write `kdeglobals` and send `org.kde.KGlobalSettings.notifyChange` [prior report 6]; keep the CLI app writers (bat, micro, helix, nvim, delta, newsboat, aerc, fzf, tmux, btop, mc, yazi, starship, lscolors); `kdt_reload_session` who[] (kdos.c:249-250) is replaced by the KDE notify path; themeaudit.c:179 foot row removed |
| `kdos help` | kdos.c:3237-3419 | REWRITE | chords from `kglobalshortcutsrc` + KDOS `/usr/share/kglobalaccel/*.desktop`, not rc.xml (3310-3340); drop the `kdos-keys` lookup (3326) and the `kdos-desktop`/`kdos-res` rows (3300-3301) |
| `kdos doctor` | kdos.c:4245-4739 | REWRITE | check kwin_wayland, plasmashell, xdg-desktop-portal-kde, kscreenlocker PAM stack, `unix_chkpwd` setuid, elogind session Active; drop kdos-comp, kdos-shell, wlr portal, foot theme, checkpass/resctl, kdos-frames.sock/kdos-cmd.sock probes (4304-4672, 4687) |
| `kdos status` | kdos.c:3487-3562 | REWRITE | Session row from elogind (`loginctl show-session`), not `running("kdos-comp")` (3520, 3694, 4332, 4337, 4640) |
| `kdos-share` | share.c (446) | REWRITE | terminal via `kb_terminal` (konsole); no-file chooser `kdos-pick` (79-81) → `kdialog --getopenfilename`; plus a Dolphin service menu |
| `kdos places` | places.c (86) | REWRITE | also write `~/.local/share/user-places.xbel` (Dolphin/KIO places) |
| `kdos-mpctl` | mpctl.c (588) | REWRITE | drop `watch`/`now` (panel file); keep transport verbs bound through a KDOS kglobalaccel `.desktop`, since mpd has no MPRIS — or ship `mpd-mpris` and delete the verbs [inferred: no mpd-mpris port; measure] |
| `kdos-sfx` | sfx.c (282) | KEEP | login sound started from a Plasma autostart entry, not `kdos_session_once` |
| `kdos remind` | remind.c | REWRITE | `ask` dies "kdos-prompt is not installed" (500-502) → `kdialog --inputbox` |
| `kdos-getty` | getty.c | REWRITE | stays the agetty wrapper on tty2-6; tty1 no longer runs the desktop. **Trap:** getty.c sets `RLIMIT_RTPRIO 95` (getty.c:251-274) and places the login in delegated `user.slice/user-<uid>/session` (getty.c:379-418) that podman's cgroupfs manager needs for box memory limits; an SDDM session never passes through it. Mitigation: `pam_limits` `rtprio` line in `/etc/security/limits.d/` for the SDDM PAM stack, and the cgroup placement moved to a `pam_exec` KDOS helper or to the elogind cgroup layout — owned by section 3 (session); the deletion of the tty1 chain waits for it |
| `kdos why`/`explain` | why.c + reasons/ | KEEP; corpus rewritten | 2.6 |
| `ksvc`/`service`, `kdos-bootctl`, `kdos-banner`, `kdos-fetch-app`/`-static`, `oracle`, `cve`, `update`, `version`, `app`, `trash`, `restarts`, `sandbox`, `march`, `rebuild`, `clone`, `persist`, clip helpers | ~12,500 lines | KEEP | comments in restarts.c, banner.c, oracle.c, sandbox.c name the old desktop; clip.c uses wl-copy/wl-paste, which work on KWin (wl-clipboard 2.3.0 builds ext-data-control against wayland-protocols 1.49; KWin compiles ext-data-control-v1) [measured: verdict 3] |
| `kdos backup` (new) | — | NEW | `fs/etc/skel/.config/kdos/timers.d/10-backup.timer` runs `kdos-backup --once`, a kdos-shell surface; port its restic logic (kdos-shell backup.c) to a kdos-tools verb **before** kdos-shell goes, or scheduled backups stop silently [measured: inv-code risk 7] |

- [ ] Port `kdos-backup --once` into a CLI (`kdos backup run`), repoint the timer. Gate: `kdos backup run` against `fixtures/backup` in selftest; timer fires in the rig (`/var/log` line). Docs: `kdos-command.md`, `administration.md` backup, `configuration.md` backup.conf.
- [ ] `kdos-shot` Spectacle wrapper. Gate: `kdos-shot qr` decodes a QR on screen on the rig; `kdos-shot -o x.png` writes a PNG. Docs: `kdos-command.md` (kdos-shot), `command-index.md`.
- [ ] `kdos theme` rewrite. Gate: `kdos theme amber` on the rig changes Plasma accent without logout (shot before/after); `kdos theme --audit` green; selftest 9108-9208 rewritten to audit `.colors`, Konsole scheme, qt5ct; 050_theme.sh tests (2.7). Docs: `02-user-guide/theming.md` (whole page), `kdos-command.md:274-724`.
- [ ] help/doctor/status/settings/notify/toggle/share/places/remind/mpctl edits. Gate: `testing/selftest.sh` kdos-tools blocks; `kdos doctor` on the ISO prints no FAIL. Docs: `kdos-command.md` (each verb), `command-index.md`.
- [ ] Delete hey.c, stutter.c, appid.c, thumb.c, speech.c and the `kdos panel/menu/hey/stutter/appid/thumb/speech` dispatch lines; keep the `kdos-shot` TOOLS row and symlink. Gate: preflight man-page check (the `kdos-shot` page describes the wrapper); `grep -n 'kdos-frames\|kdos-cmd.sock' src` empty. Docs: `kdos-command.md:849-875, 955-997, 1572-1598`, `filesystem-and-ipc.md:496-575`.

### 2.4 Third-party ports

#### Removed

| Port | Version | Shelf / phase list | rdeps today | Why it goes | Order constraint |
|---|---|---|---|---|---|
| wlroots | 0.20.2 | wl / `50_desktop/packages.txt` | kdos-comp, kdos-lock, kdos-res, kdos-shell, kdos-term | KWin is the compositor; carries `idle-capture-frame.patch` | after all 5 rdeps |
| xdg-desktop-portal-wlr | 0.8.4 | xdg / 50_desktop | kdos-record | portal-kde | after kdos-record |
| seatd | 0.9.3 | base / `41_system/packages.d/base.txt` | wlroots | KWin has only Logind, ConsoleKit, Noop session backends, no libseat [measured: kwin src/core/session.cpp:19-23]; elogind (D3) | after wlroots; `fs/etc/init.d/45_seatd.sh` with it |
| grim | 1.5.0 | wl / `42_graphics/wl.txt` | — | needs wlr-screencopy/ext-image-copy-capture (not in KWin) | leaf |
| wtype | 0.4 | wl / 42 wl.txt | — | needs zwp_virtual_keyboard_v1 (not in KWin) | leaf |
| wl-mirror | 0.18.5 | wl / 42 wl.txt | — | backends export-dmabuf, screencopy, extcopy only [measured: README]; KScreen mirror | leaf |
| wvkbd | 0.20 | input / 42 input.txt | — | needs virtual-keyboard + input-method-v2; KWin has input-method-v1 [measured: main.c:45,50]; plasma-keyboard | leaf |
| wl-kbptr | 0.4.1 | wl / `44_apps/wl.txt` | — | binds zwlr_virtual_pointer_manager_v1 [measured: src/main.c:512-515]; KWin mousekeys | leaf |
| wf-recorder | 0.6.0 | video-tools / 44 video-tools.txt | — | wlr-screencopy; Spectacle/OBS | leaf |
| wayvnc | 0.10.1 | virt / 42 virt.txt | — | wlr-screencopy; krfb/krdp | before neatvnc |
| neatvnc | 1.0.1 | virt / 42 virt.txt | wayvnc | only wayvnc; carries `nettle4.patch` | after wayvnc; `aml` stays (rdep wlvncc) |
| slurp | 1.5.0 | wl / 42 wl.txt | kdos-record | no consumer left | after kdos-record |
| fuzzel | 1.15.0 | wl / 42 wl.txt | — | KRunner; only used by the wlr portal chooser config | before fcft |
| foot | 1.28.0 | shells / 42 shells.txt | gnuradio | Konsole is the one terminal (D1 deletes default-terminal duties; one terminal to brand) | after gnuradio moves to konsole |
| fcft | 3.3.3 | wl / 42 wl.txt | foot, fuzzel, kdos-term, kdos-lock, kdos-shell, kdos-res | no consumer once foot/fuzzel/src-desktop go | after those |
| tllist | 1.1.0 | devlibs / `41_system/devlibs.txt` | fcft, foot, fuzzel | header lib of fcft | after fcft |
| qt6ct | 0.11 | themes / 44 themes.txt | — | plasma-integration's `kde` platform theme (Qt 6) | leaf; `050_theme.sh:72-75` tests skel qt6ct files |
| ffmpegthumbnailer | 2.3.1 | media-frameworks / 42 media-frameworks.txt | — | only `kdos thumb` (media-frameworks.txt:36-39); ffmpegthumbs replaces | after kdos-tools thumb.c |

`ports/sources.idx`: 22 lines name these 20 ports. `make publish-orphans` only lists them; removal is `ports/publish --orphans --prune=yes-delete` (networked, deletes archive assets), and any hash a `v0.1` recipe names is protected [measured: verdict 11; Makefile:99-100]. Recommendation: leave the archive assets and index lines until the release that drops them, run the prune once after v0.2 is tagged.

#### Kept, with edits forced by the deletion

| Port | Edit | Evidence |
|---|---|---|
| terminus-ttf 4.49.3 | KEEP (fonts / 44 fonts.txt): the Phosphor-mode Terminus option (D7, 05 §5.2.11) needs a TTF/OTF Qt can load, which this port is; its `fonts.txt` comment "labwc's titlebars, root menu and window-switcher OSD" is rewritten to that consumer | [measured: fonts.txt; 05 §5.2.11] |
| whisper-model-base-en 20241029 | KEEP (ai / 41 ai.txt): the model of the Dolphin "Transcribe" service menu (06 §6.1, 07 §7.4.5); the recipe that installs that service menu lists `whisper-model-base-en` and `whisper.cpp` in `depends`, replacing the deleted rdeps (kdos-shell rec.c, `kdos speech`) | [measured: ai.txt; 06 §6.1] |
| xwayland 24.1.14 | its only declared rdep is wlroots; **the kwin recipe must list `xwayland` in depends** — `find_package(Xwayland 23.1.0)` is optional (kwin/CMakeLists.txt:281), so without it Xwayland support drops silently at configure | [measured: verdict 4; missed 6] |
| qt5ct 1.9 | KEEP (Default A11): answers `kde` for the Qt 5 apps (qt5-qtbase has 28 rdeps: keepassxc, vlc, smplayer, mumble, quassel, lmms, …); `kdos theme` keeps its qt5ct writer. Deleting it would leave Qt 5 apps on Qt's built-in `QKdeTheme` (kdeglobals colours/icons/fonts, Fusion widgets, Qt file dialog); full Breeze for Qt 5 needs KF5, which the tree lacks | [measured: verdict 7] |
| gnuradio | `-DGRC_XTERM_EXE=/usr/bin/konsole`, depends `foot` → `konsole` (both resolve within 43_toolkits), kfs.md Flags | [measured: build.sh:37,89; kpkgbuild depends] |
| yazi, rmpc, timg, presenterm | drop `X-KDOS-Term=kdos-term` from build.sh (Konsole is `TerminalApplication` in kdeglobals) | [measured: build.sh line numbers above] |
| breeze, plasma-integration | 6.7.5 → 6.8.x; breeze `WITH_DECORATIONS=ON`, `BUILD_CURSOR=ON` (cursors replace kdos-cursors) | [measured: verify-inv-code missed 9] |
| pipewire, wireplumber, polkit, fwupd, gvfs, gnome-disk-utility, libpulse | logind/elogind flags flipped for elogind (pipewire `-Dlogind=disabled` :66, wireplumber `-Delogind=disabled` :16, libpulse `-Delogind=disabled` :36, gvfs `-Dlogind=false` :44, gnome-disk-utility `-Dlogind=none` :14, polkit `-Dsession_tracking=ConsoleKit` :19 → `elogind`); pam and shadow flags unchanged (section 3.2) | [measured: grep of build.sh] — section 3 owns the flags; listed here because they rebuild 41/42 phases in the same pass |
| konsole, wl-clipboard, basu (cmus, fprintd, libei), udisks2, libinput, libdisplay-info, lcms2, glslang, libdecor, wayland-utils, wev, waypipe, waylandpp, xdg-desktop-portal, xdg-desktop-portal-gtk, libei, xcb-util-wm, aml, gst-plugins-ugly, terminus-font (kdos-splash, getty, installer), mpv-mpris, fcitx5 family | KEEP | fcitx5: KWin starts it (`kwinrc [Wayland] InputMethod=`), so `kdos-ime` and `fcitx5 -d` in kdos-desktop-start go; add fcitx5-qt, fcitx5-gtk, fcitx5-configtool (section 4) |
| Stale text in kept recipes | `xcb-util-wm kpkgbuild:16` "wlroots' Xwayland xwm", `libnsgif kpkgbuild:16` "libkimg and imv", `43_toolkits disk.txt:28-29` kdos-mountd, `pam kfs.md:4,34` (wayvnc), `icu kfs.md` (charnames.idx), `ports/shelves:80` `wl` shelf "wlroots" | a `kpkgbuild` description edit changes that port's hash and rebuilds it; kfs.md and list comments do not [measured: verify-inv-code missed 8] |

- [ ] gnuradio and the four `X-KDOS-Term` recipes. Gate: `python3 testing/phaseclosure.py`; `python3 testing/kfscheck.py` (flag explained in kfs.md); `make book-stamp`.
- [ ] kwin recipe depends includes `xwayland`. Gate: kwin's configure log shows `Xwayland` found; `xeyes` from a box shows on the rig.
- [ ] Remove each third-party port in the 2.7 order. Gate per step: phaseclosure, preflight, `bash testing/docscheck.sh` (bookcounts: the catalogue row goes in the same edit), `python3 testing/anchorcheck.py` (`#port-<name>` links: 95 to deleted recipes, 28 kdos-shell, 23 kdos-comp, 13 wlroots, 15 to fcft/slurp/grim/wvkbd/wl-kbptr/foot/fuzzel [measured: inv-docs verdict 3]). Docs: `06-reference/ports-catalogue.md`, `status.md`, `packaging.md`, `docs/kfs/data/tier-a.txt` (13 rows: 9 desktop, 3 art, seatd).

### 2.5 `fs/`

#### Delete

| Path | Lines | Replaced by |
|---|---|---|
| `fs/etc/skel/.config/kdos-comp/rc.xml`, `menu.xml` | 589, 151 | KDOS shortcuts as `/usr/share/kglobalaccel/*.desktop` (`X-KDE-Shortcuts`) + `kwinrc`/`kglobalshortcutsrc` defaults in the look-and-feel package (section 4); preflight.sh:1516-1918 rc.xml checks replaced |
| `fs/etc/skel/.config/kdos/comp.conf`, `panel.conf`, `launcher.conf` | 359, 89, 15 | `layout.js` (D5), Task Manager launchers, `kwinrc`; `kdos.c style_write_comp` (2997-3001) and libktui/kdos-appbox comment mentions (ktui.h:647, 719; box.c:275) go with it |
| `fs/etc/skel/.config/xdg-desktop-portal-wlr/config` | 22 | portal-kde |
| `fs/etc/skel/.config/foot/foot.ini`, `foot/themes/kdos` | 94, 1 | Konsole profile + colorscheme written by `kdos theme` |
| `fs/usr/local/bin/kdos-desktop`, `kdos-desktop-start` | 61, 230 | SDDM → `startplasma-wayland`; KDOS bring-up in `/etc/xdg/plasma-workspace/env/*.sh` + autostart (section 3) |
| `fs/usr/share/xdg-desktop-portal/kdos-portals.conf` | 44 | `kde-portals.conf` shipped by portal-kde |
| `fs/etc/kdos/menu.conf` | 114 | KDOS KRunner runner data, if the runner is built (section 6) |
| `fs/etc/xdg/menus/kdos-applications.menu` | 62 | `plasma-applications.menu` + `/etc/xdg/menus/applications-merged/kdos.menu` for the `[box]` category |
| `fs/usr/share/applications/kdos-{audio,backup,bt,burn,devices,disks,find,firewall,net,netagent,openwith,peek,pick,pix,print,rec,settings,status,store,time,update,users,verify}.desktop` | 23 files | KCMs / KDE apps; all `NoDisplay=true`, all exec kdos-shell symlinks [measured]. `kdos-doc.desktop` REWRITE; `kdos-openarchive.desktop` KEEP |
| `fs/usr/share/kdos/doc/{backup,bt,connect,devices,display,index,net,rec,res,settings}.txt` | 10 files | F1 pages of deleted surfaces |
| `fs/usr/share/kdos/screensaver.txt` | 6 | — |
| `fs/etc/init.d/58_mountd.sh` | 35 | udisks2 |
| `fs/etc/init.d/45_seatd.sh` | 31 | elogind init script (section 3) |

#### Rewrite

| Path | Lines | Change | Evidence |
|---|---|---|---|
| `fs/etc/inittab:34`, new `fs/usr/local/sbin/kdos-dm`, `fs/usr/local/sbin/kdos-login` (28), `fs/etc/kdos/login.conf` (15), `fs/etc/skel/.bash_profile` + `.zprofile` (26 each, desktop trigger :20-26) | 158 | `inittab:34` becomes `1:2345:respawn:/usr/local/sbin/kdos-dm` (`exec sddm`; falls back to a password getty on tty1); rcS never starts SDDM, because rcS quits the splash synchronously at its end and an earlier SDDM would race kdos-splash for DRM master (03 §3.1); autologin moves to `/etc/sddm.conf.d/kdos.conf` (`[Autologin] User=`, `Session=plasma`); the `.bash_profile` `kdos-desktop` trigger is removed. `kdos-login`/`login.conf` DELETE once kinstall writes SDDM autologin | [measured: .bash_profile:20-26; `grep -n respawn fs/etc/inittab` → 34] |
| `fs/usr/local/lib/kdos/session-common.sh` | 436 | split: env (runtime dir, fixed bus `$XDG_RUNTIME_DIR/bus`, keymap → `kxkbrc`) → `/etc/xdg/plasma-workspace/env/kdos.sh`; `kdos_session_boxes` (warmup, gc) and `kdos_session_once` timers and reminders → the `30_timers` per-user service (section 3.3), its login sound, apps-pending toast and box restore → `/etc/xdg/autostart/kdos-session.desktop`; `kdos_session_audio` → PipeWire per-user service (section 3); first-run keys card → plasma-welcome page. Functions at :24, 29, 34, 85, 118, 172, 197 | [measured] |
| `fs/usr/local/bin/kdos-session-save` | 105 | called from `/etc/xdg/plasma-workspace/shutdown/kdos-session-save.sh`; drop `kdos hey`; record box windows from the KWin script ledger (2.3) | header :3-6 |
| `fs/etc/profile.d/10-wayland.sh` | 103 | final content (the same in 03 §3.1 and 05 §5.2.0): delete `XDG_CURRENT_DESKTOP=KDOS` (:41), `XDG_MENU_PREFIX=kdos-` (:59), **`XCURSOR_THEME=KDOS-cursors`/`XCURSOR_SIZE` (:75-76), which shadow KDE's `kcminputrc cursorTheme` for every process started from a login shell** [measured: kwin src/cursor.cpp:115-126], and the runtime-dir creation (elogind owns `/run/user/<uid>`); keep `QT_QPA_PLATFORMTHEME=kde` (:103), because login shells outside Plasma need it for qt5ct (A11); add `XDG_CONFIG_DIRS=/etc/xdg:/usr/share/kdos/xdg` (05 §5.2.19); keep `QT_QPA_PLATFORM="wayland;xcb"` only if a measurement shows a non-session shell needs it | [measured: line numbers; startplasma.cpp:412-417 forces `KDE`/`plasma-` inside the session] |
| `fs/usr/local/bin/xdg-terminal-exec` | 23 | konsole | |
| `fs/etc/xdg/kdos-mimeapps.list` (246), `fs/etc/xdg/mimeapps.list` (31), `fs/etc/skel/.config/mimeapps.list` (11) | 288 | one `fs/etc/xdg/kde-mimeapps.list` with the D8 defaults; `mimeapps.list` keeps only cross-desktop fallbacks; drop kdos-* handlers; preflight.sh:2365-2386 rewritten | `kb_desktop_prefix` yields `kde` [measured] |
| `fs/usr/share/glib-2.0/schemas/90_kdos.gschema.override` | 31 | match what kde-gtk-config writes (Breeze GTK, Breeze icons, cursor, fonts per D7); comment :17 ("matches what xdg-desktop-portal-kdos answers") rewritten | [measured] |
| `fs/etc/skel/.config/gtk-{3,4}.0/settings.ini` | 6 each | DELETE once kde-gtk-config ships (it writes them on login); until then Breeze names | [inferred] |
| `fs/etc/polkit-1/rules.d/50-kdos.rules` | 181 | its header premise (no session tracker, every check lands on `allow_any`, so no agent) is false under elogind. The "ONE MECHANISM, NEVER TWO" contract (:15-17) is rewritten, not dropped: rules decide, the agent only answers the challenges the rules leave — polkit-kde-agent-1 is the one prompt path, and a rule that grants wheel without a prompt exists only for an action whose shipped policy has no `allow_active` (NetworkManager `enable-disable-*`, `wifi.share.*`; ModemManager; udisks2 removable mounts formerly via kdos-mountd). Every kdos-net/kdos-shell grant (:60-102) re-pointed at the actions plasma-nm, powerdevil, KAuth helpers and kinfocenter call. Docs: `security-model.md:456-614` | [measured: :1-42; memory nm-no-session-tracking] |
| `fs/etc/init.d/15_userdirs.sh` | 57 | elogind owns `/run/user/<uid>`; the delegated `user.slice/user-<uid>` creation (:10-40) stays only if section 3 keeps the cgroupfs layout for podman | [measured] |
| `fs/etc/skel/.config/kdos/timers.d/10-backup.timer` | 23 | `kdos backup run` (2.3) | |
| `fs/etc/kdos/timers.d/10-update-check.timer` | 18 | comment :1-3 names "the panel's badge and kdos-update" → Discover's update notifier and `kdos update`; rewritten in the same change as the D10 update-check timer decision (06) | [measured: :1-3] |
| `fs/etc/udev/rules.d/70-kdos-backlight.rules` | | DELETE: its consumer is the panel's brightness (`kdos-osd`); PowerDevil 6.8.2 writes backlight through the root KAuth helper `org.kde.powerdevil.backlighthelper.setbrightness`, not through logind SetBrightness and not as the user [measured: powerdevil daemon/controllers/backlightbrightness.cpp:32, 245], and keyboard backlight through UPower. Gate: on a laptop, the brightness keys and the battery applet's slider change `/sys/class/backlight/*/brightness` with the rule gone. Docs: `administration.md:774` row, `docs/kfs/16-wiki/laptops.md:53-55` (kfs:include pin removed), `udev-and-device-groups.md:209` | [measured: rule header :1; grep of src] |
| `fs/etc/skel/.config/mc/ini` (:24), `mc.ext.ini` (:11, 26, 249) | | comment-only: `kdos-appbox open` stays the open path (kdos-appbox REWRITE keeps `open`; only its chooser changes, 2.2), so behaviour is unchanged; reword any mention of the kdos-openwith chooser | [measured: grep] |
| `fs/etc/skel/.config/kdos/favorites` | 42 | KEEP; header text redefined as the box-warmup list | [measured: verdict 6] |
| `fs/etc/skel/.local/share/applications/kdos-install.desktop` | | `Exec=foot …` → the Kirigami KDOS Installer (D12); serial path unchanged | |
| Comment-only: `fs/etc/{bash.bashrc,profile,motd,nftables.conf,alsa/conf.d/99-kdos-pipewire.conf,modules-load.d/kdos-hwmon.conf,init.d/53_xfs_healer.sh}`, `fs/etc/skel/{.bashrc,.config/tmux/tmux.conf,.config/mc/menu,.config/fastfetch/config.jsonc,.config/aerc/aerc.conf,.config/kdos/backup.conf}`, `fs/root/.bashrc` | ~15 files | foot → konsole; kdos-term sixel case adds konsole; mc menu kdos-peek/find/term → dolphin/konsole; fastfetch "wlroots" → "KDE Plasma" | [measured: `git grep -l` over fs/] |
| `fs/etc/group:29` `seat:x:994:kdos` | 1 | DELETE last, per 03 §3.5 / 01 A16 (2.2 `seat` item) | [measured: fs/etc/group:29] |

- [ ] Each delete/rewrite above in the step 2.7 names. Gate: `make build BUILD_ARGS="--phases 10_bootstrap,70_image --steps 10_bootstrap:000_file_system.sh"` then rig ISO boot to SDDM; `python3 testing/kfscheck.py` (83 pins: rc.xml 19, comp.conf 15, kdos-desktop 7, 10-wayland.sh 6, session-common.sh 5, kdos-desktop-start 5, …) passes after `--repin` or pin removal. Docs: `03-architecture/session.md` (whole), `boot-and-init.md:1117-1295, 1457-1548`, `06-reference/configuration.md:79-688`, `filesystem-and-ipc.md:189-283`, `docs/kfs/16-wiki/{session-and-environment,compositor-keybindings,panel-and-launcher,fonts-and-theming,login-and-autologin,portals-and-screen-sharing,screen-lock}.md`.

### 2.6 The `kdos why` reasons corpus

78 files in `src/system/kdos-tools/reasons/`, installed to `/usr/share/kdos/reasons` (build.sh:40-41). preflight.sh:1001-1009 fails a reason whose `port:` names a missing port; preflight.sh:1011-1020 accepts any `path:` when a `port:` is also named, so a stale path is silent [measured: verdict 5]. 24 fail preflight, 1 more cites a deleted path, 8 more name deleted things in their text.

| File | Cites | Action |
|---|---|---|
| a-server-that-blocks-while-it-thinks-is-down | portal-kdos | DELETE |
| default-first-in-rc-xml | kdos-comp, rc.xml | DELETE |
| a-setuid-helper-takes-no-arguments | kdos-lock / kdos-checkpass | DELETE (no KDOS setuid helper remains) |
| emphasis-is-a-fill-not-reverse | kdos-shell | DELETE |
| a-hit-map-must-not-outlive-what-it-describes | kdos-shell | DELETE |
| properties-are-never-read-inside-a-bus-callback | kdos-shell tray | DELETE |
| the-compositor-owns-locked | kdos-lock | DELETE |
| scanout-hands-you-a-clients-buffer | kdos-comp crt | DELETE |
| the-event-queue-is-a-ring | kdos-shell / libkwl | DELETE |
| a-mock-of-either-side-would-have-passed | kdos-shell traycheck | DELETE |
| wallpaper-is-the-compositors-not-a-clients | kdos-comp | DELETE |
| a-node-that-exists-is-not-a-node-that-is-recording | kdos-shell privacy | DELETE |
| a-view-elsewhere-is-reported-minimized | kdos-comp | DELETE |
| claim-only-the-cells-the-icons-occupy | kdos-shell desk | DELETE |
| an-unhandled-sighup-kills-a-panel | kdos-shell / kdos theme | DELETE |
| the-portal-daemon-snapshots-its-backends | xdg-desktop-portal-wlr / kdos-desktop-start | DELETE (startplasma orders portal start) |
| screencast-success-with-no-streams | portal-kdos | DELETE |
| bind-layer-shell-at-4 | kdos-shell / libkwl | DELETE |
| openuri-is-kdos-appbox-open | portal-kdos | DELETE |
| one-per-output-means-one-process-per-output | kdos-comp kdos-child.c | DELETE |
| a-resize-is-not-applied-until-the-consumer-applies-it | cite libktui, port kdos-shell | REWRITE: `port: kdos-installer` (libktui consumer) |
| skip-if-installed-compares-the-recipe | kpkg, port wlroots | REWRITE: `port: kdos-kpkg` |
| the-vt-tier-exists-because-the-font-is-512-glyphs | terminus-font + kdos-shell | REWRITE: drop `port: kdos-shell`, cite libktui ramp tiers |
| a-supervised-child-gets-ignored-dispositions-back | kdos-comp kdos-child.c | REWRITE to `port: kdos-tools` (ksvc) if ksvc resets dispositions before exec, else DELETE — measure: ksvc.c:485-486, 558 set `SIG_IGN` |
| session-bus-fixed-path | `path: /usr/local/bin/kdos-desktop`, port dbus | REWRITE: path → `/etc/xdg/plasma-workspace/env/kdos.sh` |
| blocked-before-busy | cite `kdos stutter` | DELETE |
| the-state-file-is-written-before-the-signal | kdos theme; text kdos-comp, kdos-shell | REWRITE text (Plasma notify path) |
| never-emit-a-bold-attribute-on-a-vt | kinstall; text foot | REWRITE text |
| launcher-filename-is-upstreams-desktop-id | kdos-appbox; text kdos-shell | REWRITE text (Kickoff, Task Manager) |
| a-library-does-not-own-the-exit-path | libkbase; text libkwl | REWRITE text |
| gtk-im-module-must-stay-unset, x11-forcing-execs-kill-alien-apps, energy-is-relative-never-watt-hours | text "compositor"/"panel" | REVIEW: true under KWin as written? rewrite the noun only |

Result: 21 deleted, 57 kept. `kdos-command.md:885` states "78" → the new count, measured with `ls reasons | wc -l` in the same change.

- [ ] Apply the table in the same change as 2.7 step 6. Gate: `testing/preflight.sh` reasons check green; `grep -lE 'kdos-comp|kdos-shell|libkwl|kdos-desktop|foot\b' src/system/kdos-tools/reasons/*.txt` empty; `kdos why <each kept>` renders. Docs: `04-programs/kdos-command.md:885`.

### 2.7 Ordered deletion sequence

Constraints the order obeys:

- **The tree always has a desktop.** A cell piece is deleted only after its KDE replacement is built, wired into a phase list and seen working on the ISO in the rig. The login switch (step 3, milestone M3) happens while the cell desktop still builds; the deletion (step 6, M6) follows. 08 §8.3 maps every step onto a milestone.
- **phaseclosure refuses a list that drops a port a listed port still depends on**, so consumers go first: xdg-desktop-portal-kdos → kdos-shell → fcft and wlroots; foot and fuzzel → fcft → tllist; wayvnc → neatvnc; kdos-record → slurp and xdg-desktop-portal-wlr; gnuradio → konsole before foot [measured: inv-code ordering].
- **Deleting anything under `src/libs` (and editing `selftest.c` or any library) changes the hash of every source-less src port**: `kp_hash.c:259-285` hashes the port dir plus `<portdir>/../../libs` [measured: verdict 10]. Every library edit and deletion (libkbase, libktui, libkxdg, selftest.c, the 8 deleted libraries) is built in **one** build, so the full src rebuild is paid once.
- **No early phase is re-run on a tree ahead of it.** Steps that edit 41_system/42_graphics lists (tllist, seatd, src-art, src-system, the logind flag flips) are built with `BUILD_ARGS="--continue-from 41_system"`.
- `040_orphans.sh` removes packages without a recipe only at 70_image; a 50_desktop-only rig run still has stale `kdos-*` binaries in `build/fs`.
- KDE recipes staged before a phase list installs them need `ports/unbuilt` entries (bookcounts.py:36-39).
- Work in a git worktree if a build is running from the main checkout.

| Step | What lands | KDE replacement that must already be in | Gate |
|---|---|---|---|
| 0 | Rule rewrite (CLAUDE.md 3/4/5, `principles.md`, `decisions.md`, `how-kdos-differs.md`); preflight ELF guard (`preflight.sh:1302-1342`) narrowed: `libQt6*`/`libKF6*` allowed for `src/desktop/*` only, GTK/wx/FLTK/Tk/Xlib and `libQt5*` still refused everywhere | — | `testing/preflight.sh`; a deliberately bad fixture (a src/system ELF linking libQt6Core) still fails [memory: prove a check fails before trusting it] |
| 1 | Plasma 6.8.x + frameworks + Gear core + elogind + SDDM built and listed (43_toolkits kde/kf6 lists, 50_desktop Plasma block); kwin depends `xwayland`; KDOS integration ports in `src/desktop` (look-and-feel, colour schemes); logind flag flips; breeze/plasma-integration to 6.8.x. Cell desktop still the login path | — | `make build BUILD_ARGS="--continue-from 41_system"`; phaseclosure; rig: from a `--root-script`, `su - kdos -c startplasma-wayland` on tty2 reaches plasmashell (shot) — a test path, no shipped switch |
| 2 | Rig harness rewritten for Plasma: `vnc-shot.py:852-875` fixed sleep → a real gate (poll `org.kde.plasmashell` on the session bus); `quickpatch.sh:100-172` restarts kwin/plasmashell; `quick.sh` default example; `testing/plasma-probe.sh` | step 1 | a probe run on a step-1 ISO returns the D-Bus names; `testing/quick.sh plasma-workspace -- --shot …` produces a shot |
| 3 | **Login switch**: `inittab:34` becomes `1:2345:respawn:/usr/local/sbin/kdos-dm` (`exec sddm`; falls back to a password getty on tty1); rcS never starts SDDM (03 §3.1). SDDM with the KDOS theme, Wayland greeter on kwin_wayland (D9 gate); tty1 trigger removed from `.bash_profile`/`.zprofile`; session-common.sh split into env + autostart, `10-wayland.sh` cleaned, `kde-mimeapps.list`, polkit rules + polkit-kde-agent-1, kscreenlocker PAM stack, RT limits + cgroup placement for the SDDM session (2.3 getty trap), kdos-powerd sleep verbs deleted (3.7), kdos-oomd list, kdos-appbox desktop name/terminal/RESERVED | kwin, plasma-workspace, kscreenlocker, xdg-desktop-portal-kde, polkit-kde-agent-1, sddm, elogind, konsole | ISO boots to SDDM, autologin to Plasma (shot); lock + unlock with the user's password (shot); `loginctl show-session` Active=yes Type=wayland; a box app opens from Kickoff; `chrt -p` of pipewire shows SCHED_FIFO; a box's `memory.max` is set; preflight, selftest |
| 4 | Kept consumers rewritten away from what goes: kdos-tools verbs (2.3) incl. `kdos backup run`; libkbase `kb_terminal`, libktui TERM; kdos-installer TUI autologin → SDDM; kdos-doc wrapper; kdos-share/remind/open fallbacks to kdialog/portal; gnuradio and X-KDOS-Term recipes; fcitx5 started by KWin | step 3 + Spectacle, kdialog, ffmpegthumbs (ported), plasma-welcome | each change: phaseclosure + preflight + selftest; library edits wait for step 6's build |
| 5 | kdos-boxsock re-homed to `src/system`, listed in 42_graphics `src-system.txt` | — | phaseclosure; selftest boxsock check; bookcounts |
| 6 | **One change deletes the cell desktop** (its pieces cross-reference): the 7 `src/desktop` programs and their 50_desktop list lines; kdos-mountd + `58_mountd.sh`; kdos-cursors, kdos-gtk-theme and their `41_system/src-art.txt` lines; kdos-icons' Papirus art and kdos-theme's gtk/icons/cursors subcommands (both ports rewritten, 2.2); the kdos-tools files (hey, stutter, appid, thumb, speech) and panel/menu dispatch; the 8 libraries and `kxdg_verbs.c`; `selftest.c` tests, includes and kxdg_verb sites; `selftest.sh` (`:24` INC, `:110-153` kimg, `:123-129` compile, `:247-268` checkpass, the desktop blocks ending at 1191, mountd 5951-6439, stutter 6724-6755, `9036-9059` (appid), `9074-9087` kept (libktui view)); goldens; 303 fixture files + the non-libkproc part of `fixtures/res`; barcheck/appindex/boxcheck; preflight desktop checks; the fs/ deletions of 2.5; the reasons of 2.6; `Dockerfile.devdeps` drops wlroots/fcft/libseat; docs pages and screenshots (section 9) | plasma-systemmonitor + ksystemstats (kdos-res), Spectacle (kdos-record, kdos-shot), krfb/krdp (before step 7 drops wayvnc), plasma-keyboard (wvkbd), Breeze cursors `BUILD_CURSOR=ON`, breeze-gtk + kde-gtk-config (GTK theme), udisks2 + Solid + Device Notifier (kdos-mountd) | `make build BUILD_ARGS="--continue-from 41_system"` — the one full src rebuild; `make check`; `CC="cc -fsanitize=address,undefined -g" testing/selftest.sh`; `python3 testing/kfscheck.py`; `bash testing/docscheck.sh`; `python3 testing/anchorcheck.py`; `python3 testing/booknav.py --write`; `make book-stamp`; `grep -rlE 'kdos-comp\|kdos-shell\|libkwl\|libkcell' src script testing fs` empty |
| 7a | Leaf third-party ports: grim, wtype, wl-mirror, wvkbd, wl-kbptr, wf-recorder, fuzzel, qt6ct, ffmpegthumbnailer, foot | step 4 gnuradio | phaseclosure after each removal |
| 7b | slurp and xdg-desktop-portal-wlr; wayvnc then neatvnc | step 6 (kdos-record gone) | phaseclosure |
| 7c | fcft, then tllist (41_system devlibs.txt) | 7a (foot, fuzzel), step 6 | phaseclosure |
| 7d | wlroots, then seatd + `45_seatd.sh` (41_system base.txt) | step 6 | phaseclosure; `grep -rn seatd src fs script` empty |
| 8 | Image and tooling text: `050_theme.sh:16-18, 37, 57-75` (themerc-override, foot, qt6ct tests → Plasma scheme + look-and-feel tests; qt5ct test kept); `070_user.sh:73, 79-90` (`.icons`/`.themes`, cosmic/kdos-con, kdos-comp rm lines); `50_desktop/phase.env` `KDOS_PHASE_DESC`; `ports/shelves:80`; Makefile `:24-33, 204-241, 355-369`; `kdosbuild.sh:50-51`; `000_file_system.sh:146`; `kfsgen.py:829-835` AREA_DESC (`desktop` = KDOS's Plasma integration; `art` drops icons/cursors); residues `docs/kdos/05-developer/build-system.md:745`, `docs/kfs/15-image/070-user.md:53` | — | `make build BUILD_ARGS="--phases 70_image"`; rig ISO boot straight into SDDM/Plasma; `ls build/fs/usr/bin/kdos-shell` absent (040_orphans swept); `make check` |

Not touched by any step, because `src/desktop` is reused for KDOS Qt code (Default A9): `PORT_REPO` in `50_desktop/phase.env` and `preflight.sh:55`, `testing/bookcounts.py:56` and `anchorcheck.py:123,143` `SRC_AREAS`, `040_orphans.sh:34` REPOS. The area is never empty: step 1 puts the KDOS integration ports there before step 6 removes the cell programs.

- [ ] Step 0 rule and guard rewrite. Gate in table. Docs: `CLAUDE.md`, `01-philosophy/{principles,decisions,how-kdos-differs,why-kdos}.md`, `05-developer/testing.md` (ELF guard).
- [ ] Steps 1-2. Gate in table. Docs: `05-developer/{developing,testing}.md` (rig, quick loop), `how-kdos-is-built.md:545-560`.
- [ ] Step 3 login switch. Gate in table. Docs: `03-architecture/{session,boot-and-init,security-model}.md`, `02-user-guide/{getting-started,desktop,administration}.md`, `docs/kfs/16-wiki/login-and-autologin.md`, `screen-lock.md`, `session-and-environment.md`.
- [ ] Step 4 consumer rewrites. Gate in table. Docs: per row of 2.2/2.3.
- [ ] Step 5 boxsock move. Gate in table.
- [ ] Step 6 the deletion change. Gate in table. Docs: delete `04-programs/{kdos-comp,kdos-shell,kdos-res,kdos-term}.md` with slots 21-24 refilled so chapter numbers 25-45 and "Forty-five chapters" hold [measured: inv-docs verdict 2]; rewrite `window-model.md`, `design-language.md`, `writing-desktop-software.md`, `04-programs/README.md`, `c-libraries.md` (~1,100 of 2,206 lines go); retarget the ~400-470 inbound links before the targets go; `known-gaps.md` Desktop entries; `command-index.md` (58 kdos-shell links; man pages follow).
- [ ] Steps 7a-7d. Gate per row. Docs: `ports-catalogue.md`, `status.md`, `packaging.md`, `tier-a.txt`, the deleted ports' `kfs.md` go with them.
- [ ] Step 8. Gate in table. Docs: `05-developer/build-system.md`, `docs/kfs/15-image/*.md`, `docs/kfs/13-desktop/intro.md` (pins `50_desktop/packages.txt` 13-18, `phase.env` 25-28).


## 3. Platform: init, seat, session, login, boot

This section replaces everything between `rcS` and the first Plasma frame. PID 1 stays sysvinit, services stay `rcS` scripts under `ksvc`. elogind becomes the logind provider, SDDM the display manager, turnstile plus a ksvc backend the per-user service manager, and kdos-settingsd the provider of hostname1, timedate1 and locale1. The defaults it relies on are 1.3 rows A2, A3, A4, A5, A6, A7, A13, A14 and A15. Where a verdict corrected a research report, this section follows the verdict.

Two verdicts shape the whole section:

- **The `seat` group outlives seatd.** It is the non-admin authorisation tier of kdos-powerd (`src/daemons/kdos-powerd/main.c:29-36`), kdos-oomd (`main.c:62`, `:587-591`) and kdos-mountd (`main.c:91-94`), and kinstall keeps the user in it (`install.c:917-921`) [measured: res-session verdict 1]. It is removed only in the change that moves those checks to polkit `allow_active` (3.5).
- **The fixed bus path and the boot-made `/run/user/<uid>` exist for boxes and rootless podman.** They have nothing to do with two desktops. kdos-appbox hard-codes `XDG_RUNTIME_DIR=/run/user/<uid>` and `DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/<uid>/bus` (`src/system/kdos-appbox/main.c:276-283`) and binds `/run/user/<uid>` into every box (`pack.c:401`) [measured]. Both stay requirements (3.3).

### 3.1 Boot-to-desktop chain

```
firmware ─ Limine (accent menu, GOP mode) ─ kernel (simpledrm on the firmware framebuffer)
  └─ initramfs /init ─ kdos-splash run (fb0, CRT power-on) ─ switch_root (util-linux)
       └─ sysvinit: si::sysinit:/etc/init.d/rcS
            ├─ 01_udev … 35_chrony                                  (unchanged)
            ├─ 40_dbus            system bus
            ├─ 40_elogind         NEW  elogind (supervised, login1 on the system bus)
            ├─ 41_polkitd         polkit 128, -Dsession_tracking=elogind
            ├─ 42_networkmanager  NM 1.58.1, session_tracking=elogind, suspend_resume=elogind
            ├─ 43_settingsd       NEW  kdos-settingsd: hostname1 / timedate1 / locale1
            ├─ 44_accounts        NEW  accounts-daemon (org.freedesktop.Accounts)
            ├─ 45_seatd           DELETED
            ├─ 55_powerd          kdos-powerd, firewall + accent only (3.7)
            ├─ 55_tlp, 55_tlppd   NEW tlp-pd: org.freedesktop.UPower.PowerProfiles
            ├─ 56_energyd, 57_oomd (new protected list), 59_packd …
            ├─ touch /run/kdos/bless-userspace                       (was: kdos-bootctl mark-good)
            └─ kdos-splash quit   synchronous power-off to black
       └─ inittab 1:2345:respawn:/usr/local/sbin/kdos-dm   (was: kdos-getty tty1 kdos-login tty1)
            └─ exec /usr/bin/sddm (VT1, DisplayServer=wayland)
                 ├─ sddm-helper ─ PAM sddm-greeter (pam_elogind: class=greeter;
                 │                     pam_turnstile: /run/user/<sddm uid>, backend starts nothing)
                 │    └─ kwin_wayland --no-global-shortcuts --no-lockscreen --locale1   (no --xwayland)
                 │         └─ sddm-greeter-qt6  KDOS QML theme (section 5)
                 └─ on login / autologin: sddm-helper ─ PAM sddm | sddm-autologin
                      ├─ pam_unix / pam_kwallet5 (auth: password captured)
                      ├─ pam_limits   ← /etc/security/limits.d/25-pw-rlimits.conf (RT budget)
                      ├─ pam_elogind  ← session c<N>, seat0, VT, /sys/fs/cgroup/<sid>
                      ├─ pam_turnstile ← /run/user/<uid> (tmpfs), user services (3.3):
                      │      dbus-daemon @ $XDG_RUNTIME_DIR/bus, pipewire, wireplumber, pipewire-pulse,
                      │      the per-user snooze timer runner
                      │      DBUS_SESSION_BUS_ADDRESS exported into the PAM environment
                      ├─ pam_kwallet5 (session, auto_start) → ksecretd --pam-login
                      └─ [Wayland] SessionCommand=/usr/local/bin/kdos-session-wrapper (00-kdos.conf)
                           │  argument: the upstream Exec of /usr/share/wayland-sessions/plasma.desktop
                           │  login shell (/etc/profile, profile.d), bus check, box warm-up
                           └─ exec plasma-dbus-run-session-if-needed startplasma-wayland
                                     (bus already set: no private bus)
                                     startkderc systemdBoot=false → "Using classic boot"
                                     └─ plasma_session: kwin_wayland_wrapper --xwayland, ksmserver,
                                        kded6, XDG autostart phase 0/1/2 (plasmashell,
                                        polkit-kde-agent-1, pam_kwallet_init, kdos-session-good)
```

Evidence for the chain:

- startplasma takes the classic path when `org.freedesktop.systemd1` is absent, or always with `systemdBoot=false` (`startplasma.cpp:608-630`, `:725-739`). Its only hard environment check is `DBUS_SESSION_BUS_ADDRESS` (`:848-851`) [measured: prior verdict 3].
- `kwin_wayland_wrapper` additionally needs `XDG_RUNTIME_DIR` (`wl-socket.c:132-134`). Nothing in startplasma, plasma_session or KWin starts PipeWire [measured: prior verdict 3 correction]. This holds for the greeter's KWin too: with elogind's runtime-dir code patched out (3.2), pam_turnstile in `sddm-greeter` (3.4) is the only creator of the `sddm` user's runtime dir, and without it the greeter is a black screen [inferred: from the two measured facts].
- KWin's session backends are Logind, ConsoleKit and Noop, in that order. There is no libseat backend (`src/core/session.cpp:19-33`) [measured: prior verdict 1], so seatd gives KWin nothing and elogind is the provider.
- KWin starts Xwayland only with `--xwayland` (`src/main_wayland.cpp:348-349`, `:169-178`) [measured]. The greeter command line has no such option, so no X server runs on the login path.
- `rcS` quits the splash synchronously before init reaches the inittab respawn entries. SDDM is therefore started from inittab, never from an rcS script, so it cannot race the splash for the DRM master [measured: `fs/etc/init.d/rcS` tail; res-session risk "flicker-free handoff"].

Files that change:

| File | Today [measured] | Becomes |
|---|---|---|
| `fs/etc/inittab:22-34` | `1:2345:respawn:/usr/local/sbin/kdos-getty tty1 /usr/local/sbin/kdos-login tty1` | `1:2345:respawn:/usr/local/sbin/kdos-dm`. The comment block is rewritten to the new constraint: a missing or failing `sddm` still yields a console |
| `fs/etc/inittab:36-40` | tty2 `kdos-getty tty2 /sbin/agetty …` | unchanged; this is still the recovery console. `/bin/login` there becomes util-linux's PAM login (3.2) |
| `fs/usr/local/sbin/kdos-login` (28 lines) | reads `login.conf`, execs `agetty --autologin` | **deleted** |
| new `fs/usr/local/sbin/kdos-dm` | — | `exec /usr/bin/sddm` when it is executable. Otherwise `exec kdos-getty tty1 /sbin/agetty --noclear tty1 38400 linux`, so a broken display manager still leaves a password prompt on tty1 (never an autologin) |
| `fs/etc/kdos/login.conf` (15 lines, `autologin = kdos`) | single autologin key | **deleted**. `/etc/sddm.conf.d/kde_settings.conf [Autologin]` is the one source, written by kinstall and by sddm-kcm |
| `src/system/kdos-tools/getty.c:211-281` `raise_rt_limits()` | RTPRIO 95, NICE 39, MEMLOCK 4 MiB, inherited by the tty1 session; comment says "nothing reads limits.d" | **removed**. pam_limits applies the same numbers from limits.d to every PAM login (3.2, 3.4), so there is one mechanism |
| `src/system/kdos-tools/getty.c:378-430` | moves the tty1 autologin into `user.slice/user-<uid>/session` | **removed**. elogind places sessions (3.2). Font, `/etc/vtrgb` and keymap loading stay for tty2 |
| `fs/etc/skel/.bash_profile:18-26`, `.zprofile` | start `kdos-desktop` on `/dev/tty1` | trigger **deleted**. `kdos-session-wrapper` (3.3) re-executes itself in the user's login shell, so these files are still read, but nothing is started from them |
| `fs/usr/local/bin/kdos-desktop`, `kdos-desktop-start` (291 lines) | the cell session | **deleted** (D1). The surviving pieces move to `kdos-session-wrapper`, the per-user services and the plasma-workspace env and shutdown scripts (3.3) |
| `fs/etc/profile.d/10-wayland.sh` | forces `XDG_MENU_PREFIX=kdos-` and `XCURSOR_THEME=KDOS-cursors`; defaults `XDG_CURRENT_DESKTOP` to `KDOS`; creates `$XDG_RUNTIME_DIR` | `kdos-session-wrapper` runs the login shell, which sources `/etc/profile`, so every line here reaches Plasma. **Delete** the `XDG_MENU_PREFIX`, `XCURSOR_*` and `XDG_CURRENT_DESKTOP` lines (SDDM sets `XDG_CURRENT_DESKTOP=KDE` from `DesktopNames`) and the runtime-dir creation (turnstile owns it). Keep the toolkit Wayland variables; `QT_QPA_PLATFORMTHEME=kde` stays per 1.3 A11 |
| `fs/etc/init.d/rcS:176-189` | `kdos-bootctl mark-good`, "LAST, deliberately" | writes `/run/kdos/bless-userspace`. mark-good runs only when both that marker and a session marker exist (3.1 tasks; detail in the Software and updates section) [measured: res-innovation verdict I18; res-session verdict: KDE Linux's `.path` unit blesses on either marker, so copy the intent, not the unit] |
| `fs/etc/init.d/06_utmps.sh`, `ports/core/auth/utmps` | utmps daemons | **kept**. SDDM writes utmpx records (`HelperApp.cpp:303-336`) through utmps, as Alpine builds it with `utmps-dev` [measured: res-session item] |
| `testing/vnc-shot.py:139-145`, `:850-866` | types on tty1 and waits for `kdos-comp` there | rewritten in the Testing section. Every rig gate in this section depends on it [measured: res-session missed 3] |

Tasks:

- [ ] Write `fs/usr/local/sbin/kdos-dm`, then replace `inittab:34` and rewrite its comment. Gate: the rig boots the ISO to the SDDM greeter. After `chmod -x /usr/bin/sddm; kill <sddm pid>` on a booted rig, tty1 shows a login prompt within one respawn.
- [ ] Delete `kdos-login`, `login.conf` and the skel triggers. Gate: `grep -rn 'login.conf\|kdos-login\|kdos-desktop' fs src script testing docs` finds no live reference.
- [ ] Delete `raise_rt_limits()` and the cgroup move from `getty.c`, in the same change as the limits.d task in 3.2. Gate: `selftest.sh` passes. On the rig, after an SDDM login, `grep -E 'realtime priority|nice priority|locked memory' /proc/$(pidof pipewire)/limits` reads 95, 39 and 4 MiB.
- [ ] Rewrite `10-wayland.sh` as above. Gate: in a Plasma session on the rig, `env | grep -E 'XDG_MENU_PREFIX|XCURSOR_THEME|XDG_CURRENT_DESKTOP'` shows no `kdos-` prefix and `XDG_CURRENT_DESKTOP=KDE`.
- [ ] Replace rcS's mark-good with the userspace marker, add the session-good autostart (`/etc/xdg/autostart/kdos-session-good.desktop`, phase 2, which touches `$XDG_RUNTIME_DIR/kdos/bless-session` after plasmashell has answered on `org.kde.plasmashell` for 60 s), and add the root side that requires both markers (owned by kdos-sysupdated in the Software and updates section). Gate: on a trial slot with `plasmashell` replaced by `/bin/false`, three boots roll back to the other slot (`kdos-bootctl status`).
- [ ] Docs, same change: `docs/kdos/03-architecture/boot-and-init.md` (the boot table row 16 and the steps at `:1153-1158`; tty1 login; mark-good; the "desktop depends on it twice over" paragraph), `docs/kdos/03-architecture/session.md` (the whole session start), `docs/kfs/16-wiki/login-and-autologin.md`, `docs/kfs/16-wiki/session-and-environment.md`, and the comment headers of `inittab`, `getty.c` and `rcS`.

### 3.2 elogind

**Version.** elogind **255.24** with Alpine's musl patch set: 10 patches, 385 lines [measured: res-session item, prior verdict 8]. Both 255.24 and 257.16 have `DelegateSessionCgroups`; 255.17 (Chimera) and 252.39 (Void) do not [measured: prior verdict 8]. 255.24 is what Alpine edge and Gentoo stable ship against Plasma 6.8. 257.16 is the newest tag but is in Gentoo testing only. Re-evaluate 257.x when Alpine moves.

**The /run/user owner.** One of elogind and turnstile must own `/run/user/<uid>` [measured: prior verdict 8 correction]. Unpatched elogind mounts a tmpfs at the user's first login and deletes it with `rm -rf` at the last logout (`user-runtime-dir.c:104`, `:139-157`). That would delete the runtime dir under a lingering box. The plan follows Chimera: carry Chimera's 2-hunk `no-runtime-dir.patch` on elogind and build turnstile with `manage_rundir=true` (3.3) [measured: prior verdict 8: Chimera main/turnstile/template.py:6]. The patch gets a `Subject:` line and is offered upstream as a configuration option.

**Port**: `ports/core/auth/elogind/` on the `auth` shelf, beside polkit, pam and utmps. Its phase is the one that holds polkit and NetworkManager; find it with `grep -rlx polkit script/phases/*/packages*` and check with `phaseclosure.py`. Proposed meson flags, each to be confirmed against `meson_options.txt` at 255.24:

```
-Dmode=release -Dcgroup-controller=elogind -Ddefault-hierarchy=unified
-Dhalt-path=/sbin/halt -Dpoweroff-path=/sbin/poweroff -Dreboot-path=/sbin/reboot
-Dkexec-path=/usr/sbin/kexec -Dnologin-path=/usr/sbin/nologin
-Dpamlibdir=/usr/lib/security -Dpamconfdir=no -Dutmp=false
-Dpolkit=enabled -Dacl=enabled -Daudit=disabled -Dselinux=disabled -Dsmack=false
-Dman=disabled -Dhtml=disabled -Dtests=false -Defi=false
-Ddbuspolicydir=/usr/share/dbus-1/system.d -Ddefault-kill-user-processes=false
```

- `-Dutmp=false`: SDDM and util-linux login write utmpx through utmps. A second writer through musl's stub utmp would be a no-op at best [inferred].
- `-Dpoweroff-path` and `-Dreboot-path` point at sysvinit, so a Plasma "Shut Down" still goes through runlevel 0 and `rcK` (`inittab:51-63`) [measured: inittab].
- `-Dpolkit=enabled` is required. Without it elogind authorises only root for `PowerOff` and `Suspend`, and PowerDevil cannot suspend [inferred: systemd-logind semantics]. Polkit is built against libelogind, which is a build-order cycle. Measure whether elogind's polkit feature needs `polkit-gobject-1` at build time or only installs policy. If it needs the library, build elogind before polkit with the policy directory passed explicitly.

**Configuration**: `/etc/elogind/logind.conf.d/50-kdos.conf`, shipped in the elogind recipe:

```
[Login]
KillUserProcesses=no        # boxes and lingering user services survive logout
RemoveIPC=no                # POSIX shm of a lingering box is not swept at logout
DelegateSessionCgroups=yes  # rootless podman needs a subtree it owns
HandleLidSwitch=suspend     # applies at the greeter; PowerDevil holds handle-* in a session
HandleLidSwitchExternalPower=suspend
HandleLidSwitchDocked=ignore
HandlePowerKey=poweroff
InhibitDelayMaxSec=5        # kscreenlocker's lock and KWin's blank fit; measure
[Sleep]
AllowHibernation=no
AllowSuspendThenHibernate=no
AllowHybridSleep=no
```

The `#` notes above are for this plan only. Rule 6 keeps them out of the recipe: the reasons go in `elogind/kfs.md`, and the comment in the shipped `.conf` states the constraint.

- The `[Sleep]` keys answer `CanHibernate` and the related calls with `no`, so PowerDevil and the Leave menu hide those actions. KDOS swap is zram (`fs/etc/init.d/12_zram.sh`), so there is no disk to resume from [inferred].
- PowerDevil owns lid and power-key policy inside a session through block inhibitors. logind's `Handle*` keys apply only where no session holds them: the greeter, or a console login [measured: res-session Q9 option A].

**The init script** is `fs/etc/init.d/40_elogind.sh`. It runs `supervise elogind /usr/lib/elogind/elogind` in the foreground, then `wait_bus_name org.freedesktop.login1`, so the scripts after it (polkitd, NM, settingsd) find the name owned. It ships no D-Bus activation file, following service_helper's rule [measured: `fs/etc/init.d/service_helper:60-63`].

**cgroups.** Sessions move from `/sys/fs/cgroup/user.slice/user-<uid>/session` to `/sys/fs/cgroup/<sid>`. With `DelegateSessionCgroups=yes` the user owns `/sys/fs/cgroup/<sid>/{session,delegated}`, which requires pure cgroup2 (`logind-session.c:144`, `:252-307`) [measured: prior verdict 8]. `fs/etc/fstab:11` mounts only cgroup2 [measured: prior verdict 8]. Whether elogind enables `+cpu +memory +pids` at the root is unmeasured. So `15_userdirs.sh` shrinks to `15_cgroups.sh`, which keeps only the root `subtree_control` write (`15_userdirs.sh:31-32`). The `/run/user` creation and the `user.slice` tree are deleted.

**Rebuilds that depend on elogind** [measured: flags as listed]:

| Port | Today | Change |
|---|---|---|
| `ports/core/auth/polkit/build.sh:19` | `-Dsession_tracking=ConsoleKit`, `ck-printerr-newline.patch` at `:12` | `-Dsession_tracking=elogind` (choices `logind`, `elogind`, `ConsoleKit`) [measured: polkit-128 `meson_options.txt:1`]. Delete the patch once it is measured to touch only the ConsoleKit monitor. `depends` gains `elogind` |
| `ports/core/network/networkmanager/build.sh:22-26` | `-Dsession_tracking=no -Dsession_tracking_consolekit=false -Dsuspend_resume=consolekit` | `-Dsession_tracking=elogind -Dsuspend_resume=elogind`. Same change: kdos-powerd's NM `Sleep()` call (`main.c:141-176`) is deleted, or NM sleeps twice [measured: res-session item]. `user:` connection permissions start working (`docs/kfs` note on settings.modify.system) |
| `ports/core/disk/udisks2/build.sh` | no logind flag; configure auto-detects | rebuild after elogind. Gate: `ldd /usr/lib/udisks2/udisksd \| grep elogind` [inferred: auto-detect] |
| `ports/core/hardware/tlp/build.sh:12,16` | `TLP_WITH_ELOGIND=0`, `install-tlp install-rdw` | `TLP_WITH_ELOGIND=1` (installs TLP's elogind system-sleep hook) plus `install-pd`. Set `TLP_SYSD` to a staging path and delete it, because `install-pd` installs `tlp-pd.service` there unconditionally (`Makefile:253-262`, `_SYSD` default `/usr/lib/systemd/system` at `:28`) and `lint.c:143` refuses it. `tlp-pd.dbus.service` is `Exec=/bin/false` + `SystemdService=` and is deleted. tlp-pd runs under ksvc (3.7) [measured: res-session verdict TLP] |
| `ports/core/audio-io/wireplumber/build.sh:16` | `-Delogind=disabled` | `-Delogind=enabled`. Optional (P1); it only matters for seat-aware Bluetooth profiles [measured: res-session item] |
| `ports/core/audio-io/pipewire/build.sh:66` | `-Dlogind=disabled`, `-Drlimits-install=false` (`:78`) | `-Dlogind=enabled` (P1). The rlimits flags change in this section (below) |
| `ports/core/boot/fwupd/build.sh:22` | `-Dlogind=disabled` | `-Dlogind=enabled`: fwupd inhibits shutdown during a flash (P1) [inferred] |
| `ports/core/audio-io/libpulse/build.sh:36` | `-Delogind=disabled` | `-Delogind=enabled` (a `feature` option) [measured: libpulse 17.0 `meson_options.txt:108-110`] |
| `ports/core/gtk/gvfs/build.sh:44` | `-Dlogind=false` | `-Dlogind=true` (a boolean) [measured: gvfs `meson_options.txt:30`] |
| `ports/core/disk/gnome-disk-utility/build.sh:14` | `-Dlogind=none` | `-Dlogind=libelogind` (choices `libsystemd`, `libelogind`, `none`) [measured: gnome-disk-utility `meson_options.txt:1`] |
| `ports/core/auth/pam/build.sh:19-20`, `:35-57` | `-D logind=disabled -D elogind=disabled`; `system-auth`, `system-session` with pam_limits; no elogind | **flags unchanged**: elogind depends on pam, so pam built against elogind is a cycle, and `pam_elogind.so` ships with elogind itself [inferred: both recipes]. The recipe adds `/etc/pam.d/system-login`, the one stack every login includes: `auth/account/password include system-*`, `session include system-session`, `session optional pam_elogind.so`, `-session optional pam_turnstile.so`. Order follows Chimera's `system-login`; measure it at port time |
| `ports/core/base/util-linux/build.sh:16` | `--disable-login` | `--enable-login` (PAM). It installs `/etc/pam.d/login` (`include system-login`), so a tty2 login registers with elogind and gets `allow_active` [measured: res-session item "Console login with PAM"] |
| `ports/core/auth/shadow/build.sh:20`, `:28` | `--disable-logind`, `--without-libpam` | **both unchanged**: shadow's `login` is deleted, so nothing in shadow registers a session. Delete shadow's `/usr/bin/login` so util-linux owns `login`. shadow's other tools stay PAM-free, which avoids the trap of `pam.d/other` being deny-all (`pam/build.sh:65-76`) for every shadow tool that would lack its own stack |
| `ports/core/servers/openssh` | no PAM [measured: grep] | unchanged (P2). ssh sessions stay unregistered with elogind |
| `ports/core/base/seatd`, `fs/etc/init.d/45_seatd.sh` | seatd 0.9.3 | **deleted**, with wlroots (its only dependent, `wlroots/kpkgbuild:19`) and `testing/Dockerfile.devdeps:67` `libseat-dev` [measured: res-session missed] |

**RT budget through PAM.** pam_limits is already in `system-auth` and `system-session` (`pam/build.sh:40,54`), but no limits file ships. Build pipewire with `-Drlimits-install=true -Drlimits-match=@audio -Drlimits-memlock=4096`. That installs `/etc/security/limits.d/25-pw-rlimits.conf` with the numbers `getty.c:251-253` grants today. The memlock option is in kB, and its default of 4194304 kB is the 4 GiB ceiling the getty comment rejects [measured: pipewire 1.6.9 `meson_options.txt:326-359`; `getty.c:236-240`]. The live user is in `audio` (`fs/etc/group`) [measured], and every new account joins `audio` too, however it is made (kinstall, plasma-setup's OEM account, System Settings' Users page through accountsservice, `useradd` through the skel defaults), so no account's PipeWire or KWin runs without the RT limits (ruling 1, §8.5).

KWin needs this too. `gainRealTime()` requests `SCHED_RR` at the minimum priority, and logs "Failed to gain real time thread priority" without `RLIMIT_RTPRIO >= 1` or `CAP_SYS_NICE` (`kwin src/utils/realtime.cpp:17-26`) [measured]. The setcap does not survive kpkg's tar [measured: prior], so the limit is the only route.

Tasks:

- [ ] Port elogind 255.24 with Alpine's patches, each given a `Subject:` header, plus Chimera's `no-runtime-dir.patch`. Write `kfs.md` (patches, flags, the `/run/user` ownership, the polkit cycle). Publish the source. Gate: `kfscheck.py` passes, and `make build BUILD_ARGS="--phases <phase>,70_image --rebuild elogind"` succeeds.
- [ ] Measure the polkit build cycle. Gate: a fresh chroot build of `elogind` then `polkit` then `elogind` produces identical elogind packages, or the recipe passes the policy dir explicitly and builds once.
- [ ] Add `40_elogind.sh` and `50-kdos.conf`. Gate: on the rig, `loginctl list-sessions` shows the SDDM greeter session (class greeter) and, after login, a user session on seat0 with `Active=yes`. `loginctl show-session <id> -p VTNr` is 1.
- [ ] Rebuild polkit, NM, udisks2, TLP, util-linux, wireplumber, pipewire, fwupd, libpulse, gvfs and gnome-disk-utility as tabled, ship pam's `system-login`, and delete seatd and wlroots. pam's and shadow's configure flags do not change. Gate: for each flipped flag, `readelf -d` of the port's main binary or library names `libelogind.so.0`; `phaseclosure.py` passes. On the rig, `pkcheck --action-id org.freedesktop.login1.suspend --process $$` from a Konsole in the active session succeeds with no prompt (upstream `allow_active=yes`).
- [ ] Ship `25-pw-rlimits.conf` through the pipewire flags in the same change as the `getty.c` deletion (3.1). Gate: `chrt -p $(pgrep -u kdos -x pipewire)` reports `SCHED_FIFO`. `/proc/$(pidof kwin_wayland)/task/*/sched` shows policy 2 (RR) on its input and commit threads, and KWin's log has no "Failed to gain real time" line.
- [ ] Reduce `15_userdirs.sh` to `15_cgroups.sh`. Gate: inside a boxed app started from Plasma, `cat /sys/fs/cgroup/$(cut -d: -f3 /proc/self/cgroup)/memory.max` shows the box profile's limit, not `max` (re-prove the rule that `15_userdirs.sh:22-27` states).
- [ ] Prove that a warm box survives logout and re-login with `KillUserProcesses=no`, `RemoveIPC=no` and turnstile linger (3.3). Gate: `podman ps` as the user after SDDM logout and login lists the same container IDs, and the box's app reconnects to `$XDG_RUNTIME_DIR/bus`.
- [ ] Docs, same change: `principles.md:42` (the seatd row becomes elogind), `principles.md#no-systemd` (renamed per 1.4.3), `how-kdos-differs.md:110` ("KDOS uses no systemd component" becomes the elogind exception), `security-model.md` (System accounts, Root daemons), `daemons.md` (seatd), `boot-and-init.md` (init order table), `docs/kdos/06-reference/glossary.md` and `ports-catalogue.md`, `docs/kfs/16-wiki/users-and-groups.md` and `containers.md`, the kfs.md files of polkit, NetworkManager, TLP, wireplumber, pipewire, fwupd, libpulse, gvfs, gnome-disk-utility, wlroots and util-linux, `src/daemons/kdos-powerd/main.c:23` ("logind does not exist on KDOS and is not coming"), and every other site in res-session missed 9's list of 34 places. Gate: `grep -rn -i 'seatd\|logind does not\|no logind' docs src fs` returns only intended text. `docscheck.sh` passes.

### 3.3 Per-user services without systemd --user

**Choice: turnstile 0.1.11 with a ksvc backend** (1.3 A2), not a native `ksvc --user` mode:

| | turnstile + ksvc backend | ksvc --user from a PAM module |
|---|---|---|
| Login counting across SDDM, tty2 and ssh; start at the first login, stop at the last; linger | turnstiled does it already | must be written: a root tracker per uid, which is turnstile again |
| `/run/user/<uid>` creation and removal | `manage_rundir=true` | must be written |
| Exporting `DBUS_SESSION_BUS_ADDRESS` through PAM | built in (Chimera uses it) [inferred: turnstiled.conf; measure the key name] | must be written |
| Supervision, restart and restart-with | ksvc (exists: `src/system/kdos-tools/ksvc.c`, 1,197 lines, `--restart-with`) | ksvc |
| Precedent with elogind + Plasma 6.8 on musl | Chimera [measured: prior verdict 8] | none |
| New code | backend script (~50-100 lines of sh) + a ksvc run-dir override (~30 lines of C) [estimate] | ~600-1,000 lines of C [estimate] |

**Ports and files:**

- `ports/core/auth/turnstile/` (0.1.11, meson): `-Dmanage_rundir=true`, the dinit backend off and the KDOS backend installed. The config `/etc/turnstile/turnstiled.conf` sets `backend = ksvc`, `manage_rundir = yes`, `export_dbus_address = yes`, `linger = maybe` and `login_timeout = 15`. Key names come from turnstiled.conf at 0.1.11 [inferred; measure].
- **Backend** `/usr/lib/turnstile/backend/ksvc`, shipped in the turnstile recipe or in kdos-tools. `run` exports `KSVC_RUN_DIR=$XDG_RUNTIME_DIR/ksvc` and runs `/etc/kdos/user-services/NN_*.sh start` in order, using the same `service_helper` as `/etc/init.d`. It signals readiness on the fifo turnstile hands it once the bus socket answers and `pipewire-0` exists. `stop` runs the scripts' `stop` in reverse. For a uid below 1000 (the `sddm` greeter) `run` signals readiness at once and starts nothing (3.4). The backend protocol is turnstile's documented `run`/`stop` contract [source: turnstile README "A backend is a very trivial shell script"; measure the argument order].
- **ksvc change:** honour `KSVC_RUN_DIR` (pid, state and log files) when the caller is not root. Today `pidfile()` joins the fixed `RUN_DIR` (`ksvc.c:86-91`) and logs go to `/run/kdos-svc.<name>.log` (`:376`) [measured]. The facility is `user`.
- **User services**, in `/etc/kdos/user-services/`, in this order:

| Script | Command | Readiness | Notes |
|---|---|---|---|
| `10_dbus` | `dbus-daemon --session --nofork --address=unix:path=$XDG_RUNTIME_DIR/bus` | `dbus-send --session … GetId` answers | The fixed path boxes bind (`kdos-appbox/main.c:282`). It replaces `kdos_session_bus` and its flock dance (`session-common.sh:118-151`) |
| `20_pipewire` | `pipewire` | `$XDG_RUNTIME_DIR/pipewire-0` exists | — |
| `21_wireplumber` | `wireplumber`, `--restart-with pipewire` | — | starts after pipewire and before pipewire-pulse, as `session-common.sh:169-171` requires |
| `22_pipewire-pulse` | `pipewire-pulse`, `--restart-with pipewire` | `$XDG_RUNTIME_DIR/pulse/native` exists | plasma-pa talks PulseAudio, so it must answer before plasmashell starts |
| `30_timers` | `/usr/libexec/kdos/user-timers`: the per-user snooze runner moved out of `kdos_session_once` (`session-common.sh:309-404`). It starts one `snooze` per `~/.config/kdos/timers.d/*.timer` (skel ships `10-backup.timer`, `20-updatedb.timer`) and the `kdos remind` loops. Shell, or a kdos-tools verb | its pidfile under `$XDG_RUNTIME_DIR/kdos` | runs for every login, graphical or tty, for turnstile's lifetime of the user; without it scheduled backups, updatedb and reminders stop silently at the login switch (M3) [measured: `session-common.sh:309-404`, `fs/etc/skel/.config/kdos/timers.d`] |

turnstile holds the PAM session open until the backend reports ready, within `login_timeout` [inferred; measure]. So pipewire-pulse is up before `startplasma-wayland` runs, and plasma-pa does not race it. This replaces the unsupervised `&` launches of `kdos_session_audio` (`session-common.sh:172-181`) [measured].

**The bus.** The upstream `Exec=` of `plasma.desktop` runs `plasma-dbus-run-session-if-needed`, which starts a private `dbus-run-session` only when `DBUS_SESSION_BUS_ADDRESS` is empty [measured: prior verdict 3]. turnstile exports the fixed address into the PAM environment, so Plasma and every box share `$XDG_RUNTIME_DIR/bus`. If the export ever fails, Plasma silently gets a random bus and boxes lose theirs. `kdos-session-wrapper` therefore refuses to start, with a visible error, when the variable is unset or does not name `$XDG_RUNTIME_DIR/bus`.

**`/etc/xdg/startkderc`**: `[General] systemdBoot=false`. This skips the `ListUnitFilesByPatterns` probe (`startplasma.cpp:613-615`). With `force` the session would exit with code 4 [measured: prior verdict 3 correction]. It ships in the plasma-workspace recipe.

**The KDOS session wrapper, `fs/usr/local/bin/kdos-session-wrapper`.** `/usr/share/wayland-sessions/plasma.desktop` stays exactly as plasma-workspace installs it (`Exec=…/plasma-dbus-run-session-if-needed …/startplasma-wayland`, `login-sessions/plasmawayland.desktop.cmake:2-3`), so no recipe edits an installed upstream file (hard rule 9). SDDM runs `[Wayland] SessionCommand` with that `Exec=` string as its argument (`sddm-0.21.0 src/helper/UserSession.cpp:122-124`; default `data/scripts/wayland-session`, `CMakeLists.txt:181`) [measured]. `00-kdos.conf` (3.4) points the key at the KDOS wrapper, which takes over `wayland-session`'s job:

1. re-executes itself in the user's login shell, as `wayland-session` does for bash and zsh, so `/etc/profile` and `profile.d` are read;
2. checks the bus address, as above;
3. runs the box warm-up (`kdos_session_boxes`, `session-common.sh:85-116`);
4. `exec $@`, unquoted: SDDM passes the whole `Exec=` as one argument, and upstream's script word-splits it the same way [measured: `wayland-session` last line].

The rest of `session-common.sh` moves:

- `BROWSER`, `open` and the other environment go to `/etc/xdg/plasma-workspace/env/kdos.sh`. startplasma sources `<config dirs>/plasma-workspace/env/*.sh` (`startplasma.cpp:308-345`) [measured].
- Session-end work (`kdos-box gc --session-end`) goes to `/etc/xdg/plasma-workspace/shutdown/kdos.sh`. plasma-shutdown runs every `plasma-workspace/shutdown` directory found by `QStandardPaths::locateAll` (`startkde/plasma-shutdown/shutdown.cpp:190`) [measured].
- The keymap, XKB and IME code (`session-common.sh:34-84`) goes into kdos-settingsd (3.6) and KWin (3.8).

`kdos_session_once`'s timers and reminders become the `30_timers` user service above. Its remaining items (login sound, apps-pending toast) are re-homed by the Integration section.

Tasks:

- [ ] Port turnstile 0.1.11 and write the ksvc backend and the `KSVC_RUN_DIR` change. Gate: `selftest.sh` covers the ksvc run-dir override. On the rig, `ksvc check pipewire` run as the user reports `up`, and `kill -9` of pipewire is followed within `RESPAWN_DELAY` (5 s) by a new pid, with wireplumber and pipewire-pulse restarted with it.
- [ ] **M1, single owners under the cell desktop.** In the same change that ships the user services, `kdos_session_bus` and `kdos_session_audio` (`session-common.sh:118-181`, called from `kdos-desktop-start:43-44`) are deleted, or become no-ops when `$XDG_RUNTIME_DIR/bus` and `pipewire-0` already answer, and `15_userdirs.sh` stops creating `/run/user` (`:16-17`), so turnstile (`manage_rundir`) is its one creator. Otherwise two daemons race for the same sockets and the M1 gate measures nothing. Gate: under the cell desktop at M1, `pgrep -c -u kdos -x pipewire` is 1, `pgrep -c -u kdos -x dbus-daemon` is 1, and `ss -xl | grep -c /run/user/1000/bus` is 1. Referenced from M1 in the Milestones section.
- [ ] Ship the `30_timers` user service and delete the timer loop from `session-common.sh`. Gate: after SDDM login, `pgrep -u kdos -a snooze` lists one snooze per `~/.config/kdos/timers.d` entry; a 1-minute fixture timer fires and writes its log line; after logout of the last session no snooze remains, unless the user lingers. Docs: `administration.md` (timers), `configuration.md` (`timers.d`), `session.md`.
- [ ] Ship the user-service scripts and delete `kdos_session_bus` and `kdos_session_audio`. Gate: after SDDM login, `ss -xl | grep "/run/user/1000/bus"` shows one listener. `echo $DBUS_SESSION_BUS_ADDRESS` in Konsole is `unix:path=/run/user/1000/bus`. `pactl info` answers before plasmashell's first frame (compare `ksvc` state timestamps with KWin's log).
- [ ] Ship `startkderc`, `kdos-session-wrapper`, the `SessionCommand` key, `env/kdos.sh` and `shutdown/kdos.sh`. Gate: `grep -c Exec=.*startplasma-wayland /usr/share/wayland-sessions/plasma.desktop` is 1, unchanged from upstream (`diff` against the build tree's generated `login-sessions/plasmawayland.desktop`); `~/.local/share/sddm/wayland-session.log` (or the session log path, measured) contains the wrapper's bus-check line and "Using classic boot". Logout runs `kdos-box gc --session-end` (its log line appears).
- [ ] Linger policy: decide whether warm boxes keep running after the last logout. Gate: a recorded decision in `session.md`, and the 3.2 box-survival test passing or failing as the decision expects.
- [ ] Docs, same change: `session.md` (the session bus, audio, timers, the session wrapper, the start-up order), `docs/kdos/06-reference` (paths: `/etc/kdos/user-services`, `$XDG_RUNTIME_DIR/ksvc`, `/usr/local/bin/kdos-session-wrapper`), `kdos-command.md` (`service` under a user), `daemons.md` (ksvc user mode), `docs/kfs/16-wiki/session-and-environment.md`, and the turnstile and pipewire `kfs.md` files.

### 3.4 SDDM

**Version.** SDDM **0.21.0**, the latest tag (2024-02-26). Master `633c1bf5` (2026-10-20) is active [measured: res-session item]. Build 0.21.0 with `-DCMAKE_POLICY_VERSION_MINIMUM=3.5`, which is a flag where Alpine carries a patch [measured: res-session/distro/chimera-sddm.py]. Alpine's other two patches (pam path; "disable automatic portal launching") are each measured for need before being carried. Move to a pinned master snapshot only if 0.21.0's Wayland greeter fails its rig gates. That would be a recipe change, not a patch.

**Recipe** (`ports/core/kde/sddm/`, `kde` shelf; phase from `phaseclosure.py`):

```
-DBUILD_WITH_QT6=ON -DNO_SYSTEMD=ON -DUSE_ELOGIND=ON -DENABLE_JOURNALD=OFF
-DINSTALL_PAM_CONFIGURATION=OFF -DBUILD_MAN_PAGES=ON -DUID_MIN=1000 -DUID_MAX=60000
-DRUNTIME_DIR=/run/sddm -DDBUS_CONFIG_FILENAME=sddm_org.freedesktop.DisplayManager.conf
CPPFLAGS=-I/usr/include/utmps LIBS="-lutmps -lskarnet"
```

- `USE_ELOGIND` only sets halt and reboot to `loginctl`. SDDM reaches logind over D-Bus (`PowerManager.cpp:117-126`) [measured: res-session verdict 4].
- `SDDM_INITIAL_VT` keeps its upstream default of 1 (`CMakeLists.txt:192`) [measured]. Alpine's 7 is not copied, because tty1 is the graphical VT on KDOS and tty2 is the recovery console.
- The utmps flags are the ones shadow uses (`shadow/build.sh:34`) [measured].
- `depends`: elogind, pam, dbus, utmps, qt6-qtdeclarative, kwin (greeter compositor), libxcb. SDDM requires XCB (`CMakeLists.txt:71`) [measured: res-session item]. A system account `sddm` (home `/var/lib/sddm`, shell `/sbin/nologin`) is added to `fs/etc/passwd` and `fs/etc/group`.

**The Wayland greeter.** plasma-workspace installs `/usr/lib/sddm/sddm.conf.d/plasma-wayland.conf` only with `-DINSTALL_SDDM_WAYLAND_SESSION=ON`; the option defaults OFF (`CMakeLists.txt:14`, `:388-391`). Upstream marks this path "BETA, do not deploy to final users yet", and SDDM's `Configuration.h:41-42` calls Wayland support experimental [measured: res-session verdict 3]. Rule 4 forces KDOS onto this path, so it is a known risk with a rig gate, not a stock configuration. Alpine ships exactly this [measured: res-session verdict 4].

The plasma-workspace recipe passes the flag. KDOS adds `/etc/sddm.conf.d/00-kdos.conf`, which overrides `/usr/lib/sddm/sddm.conf.d` (read first):

```
[General]
DisplayServer=wayland
GreeterEnvironment=QT_WAYLAND_SHELL_INTEGRATION=layer-shell
InputMethod=
Numlock=none
[Wayland]
CompositorCommand=kwin_wayland --no-global-shortcuts --no-lockscreen --locale1
SessionCommand=/usr/local/bin/kdos-session-wrapper
SessionDir=/usr/share/wayland-sessions
[X11]
SessionDir=
[Users]
MinimumUid=1000
MaximumUid=60000
RememberLastUser=true
RememberLastSession=true
[Theme]
Current=breeze
```

- `--inputmethod plasma-keyboard` is dropped from upstream's command until `qt6-qtvirtualkeyboard` and `plasma-keyboard` are ported (`plasma-keyboard CMakeLists:37-43`). Gentoo does the same for PLM [measured: res-session item]. It returns with those ports.
- `Current=breeze` until section 5's `kdos` theme lands. plasma-desktop installs the Breeze SDDM theme (`INSTALL_SDDM_THEME TRUE`, `CMakeLists.txt:19`, `:259-274`) [measured: prior verdict 7].
- `SessionCommand` replaces `/usr/share/sddm/scripts/wayland-session` with the KDOS wrapper (3.3).
- An empty `[X11] SessionDir` and no `/usr/share/xsessions` means no X session can be offered. plasma-workspace 6.8.2 still builds the X11 session by default (`option(WITH_X11_SESSION … ON)`, `CMakeLists.txt:13`) [measured]; the recipe's `-DWITH_X11_SESSION=OFF` (Ports section) is what leaves only `startplasma-wayland`. Gate: `ls /usr/share/xsessions /usr/bin/startplasma-x11` fails on a built tree.

**PAM stacks**, written by the sddm recipe (and the kscreenlocker recipe for `kde*`). They follow Chimera's files, adapted to `system-login` [measured: res-session item "PAM stacks"]:

| File | Content |
|---|---|
| `/etc/pam.d/sddm` | `auth include system-login`; `-auth optional pam_kwallet5.so`; `account include system-login`; `password include system-login`; `session include system-login`; `-session optional pam_kwallet5.so auto_start` |
| `/etc/pam.d/sddm-autologin` | `auth required pam_succeed_if.so uid >= 1000 quiet`; `auth required pam_permit.so`; `account include system-login`; `password required pam_deny.so`; `session include system-login`; `-session optional pam_kwallet5.so auto_start` |
| `/etc/pam.d/sddm-greeter` | `auth required pam_permit.so`; `account required pam_permit.so`; `password required pam_deny.so`; `session required pam_unix.so`; `session optional pam_elogind.so`; `session required pam_turnstile.so`. Required, not optional: turnstile is the only creator of `/run/user/<sddm uid>` (3.2), and the greeter's KWin refuses to start without `XDG_RUNTIME_DIR` (3.1) |
| `/etc/pam.d/kde`, `kde-fingerprint`, `kde-smartcard` | kscreenlocker's services (CMake defines them) [measured: prior]. `kde`: `auth include system-auth`, `account include system-account`. `kde-fingerprint`: `auth sufficient pam_fprintd.so` (fprintd 1.94.5 is in the tree) [measured: res-session item]. `kde-smartcard`: off until a pam_pkcs11 port exists |

**The greeter's turnstile session.** `turnstiled.conf` has one global `backend` key (`none` disables spawning for everyone) and `root_session` applies only to uid 0, so there is no per-user way to start no services [measured: Chimera `turnstiled.conf.in:22-31`, `:104-110`]. The ksvc backend (3.3) therefore checks the uid: below 1000 (the `sddm` account) its `run` signals readiness at once and starts nothing, and its `stop` is empty. The greeter gets a runtime dir and no bus, PipeWire or timers. Whether SDDM's greeter needs a session bus of its own is measured by the gate below.

`system-auth` carries `nullok` (`pam/build.sh:38`). That lets the live medium's password-less `kdos` account unlock its own lock screen. An installed machine always has a password set by kinstall or plasma-setup. Removing `nullok` would lock out the live user, so it stays, and `security-model.md` states the consequence.

**Autologin policy**, by kinstall answer. The answer file already has `autologin` and `luks` keys (`conf.c:189-190`, `:216-217`) [measured]:

| Case | `/etc/sddm.conf.d/kde_settings.conf` | `kscreenlockerrc` |
|---|---|---|
| live medium | `[Autologin] User=kdos Session=plasma Relogin=true` (KDE Linux live-setup) [measured: res-session item] | `[Daemon] Autolock=false` |
| `autologin=no` (default for installs) | no `[Autologin]` | upstream |
| `autologin=yes`, one account, LUKS root (1.3 A15) | `[Autologin] User=<u> Session=plasma Relogin=false` | `[Daemon] LockOnStart=true`: the lock screen is the login. kinstall offers an extra "no password after the disk passphrase" choice, which sets `LockOnStart=false` |
| `autologin=yes`, otherwise | not offered. kinstall refuses and explains, or asks for the A15 change | — |

- `LockOnStart` (kcfg `:36-39`) makes ksld lock immediately (`ksldapp.cpp:282-285`) [measured: res-innovation verdict I8]. Not measured: whether a desktop frame shows before the lock surface. That is a rig gate below.
- The trap: with autologin, pam_kwallet5 gets no password, so the wallet asks on first use (1.3 A7). The lock-screen-as-login mode does not fix this, because kscreenlocker's `kde` stack runs only `auth` [inferred]. It is a measured task below.
- kinstall's writer (`install.c:1765-1833`) is rewritten to produce `kde_settings.conf` instead of editing `login.conf`. kinstall's Theme step and the Kirigami installer are in the Installer section.
- plasma-setup's OEM path writes `/etc/sddm.conf.d/99-plasma-setup.conf`, because its `GetUnitFileState(plasmalogin.service)` probe falls back to SDDM when systemd1 is absent. This is in both `bootutil.cpp:23-46` and `authhelper.cpp:705-718` [measured: res-session verdict]. Needs no patch.

**sddm-kcm 6.8.2** (`ports/core/kde/sddm-kcm/`): ECM, Qt6 and KF6 only, plus a KAuth helper [measured: res-session item]. It writes `kde_settings.conf` and syncs theme settings to `/var/lib/sddm`. It is the GUI owner of `[Autologin]` and `[Theme]` from then on. kdos-powerd's `autologin` verb is deleted (3.7).

**Fast user switching.** SDDM exports `org.freedesktop.DisplayManager`, and libkworkspace's `canSwitchUser()` is hard-coded true (`login1_manager_interface.cpp`, prior verdict 2) [measured]. "Switch User" asks SDDM for a new greeter on a free VT, and elogind activates it.

**Greeter keyboard layout.** With `--locale1`, KWin follows locale1's X11 layout (`main_wayland.cpp:414`, `:536-539`). When locale1 returns empty properties, it falls back to `XKB_DEFAULT_LAYOUT`/`VARIANT`/`MODEL`/`OPTIONS` (`xkb.cpp:531-578`) [measured: res-session verdict 6]. So kdos-settingsd (3.6) is the primary source. Whether `GreeterEnvironment` reaches the compositor process, or only the greeter, is unmeasured. The XKB fallback is only trusted after that is measured.

**Rule 4 wording.** SDDM links libxcb. The 1.4.1 rule text allows X client libraries and names SDDM with `DisplayServer=wayland`. The gate is that no X server process ever exists before a user session starts Xwayland.

**Theme.** Section 5 (Look) owns the KDOS QML theme `/usr/share/sddm/themes/kdos`, designed with the lock screen and the KSplash. This section owns only `[Theme] Current=`, the faces path (`/var/lib/AccountsService/icons`, served by accountsservice in 3.6) and the gates below. Known trap: a third-party SDDM theme broke on 6.8 over a virtual-keyboard QML type [source: forum.manjaro.org/t/190649].

Tasks:

- [ ] Greeter runtime dir. Gate: on the rig, `ls -ld /run/user/$(id -u sddm)` exists while the greeter shows, and `grep -z XDG_RUNTIME_DIR /proc/$(pgrep -u sddm kwin_wayland)/environ` names it; `pgrep -u sddm -a 'dbus-daemon|pipewire|snooze'` is empty. With `pam_turnstile` removed from `sddm-greeter`, the greeter fails to draw (prove the gate fails).
- [ ] Port SDDM 0.21.0 with the flags above, the `sddm` account and the PAM files; write `kfs.md`. Gate: `kfscheck.py` passes. `readelf -d /usr/bin/sddm` shows `libutmps` and no libsystemd. `lint.c` accepts the package (no `usr/lib/systemd`).
- [ ] Rebuild plasma-workspace with `-DINSTALL_SDDM_WAYLAND_SESSION=ON` and ship `00-kdos.conf`. Gate: the rig shot shows the Breeze greeter at 1920x1080. During the greeter, `pgrep -a Xwayland` and `pgrep -a Xorg` are empty, and `ls /tmp/.X11-unix` is empty or absent.
- [ ] Greeter soak, the beta-path gate: 20 boots on the rig plus one Intel and one AMD machine. At each, log in with a password, log out, and confirm the greeter returns. Gate: 20/20 greeters drawn, and no `sddm-greeter` crash in `/var/log/messages`.
- [ ] Autologin matrix (live, off, LockOnStart, LockOnStart=false). Gate: a rig shot per case. In the LockOnStart case, a shot with no sleep after login (rig-shot-cost rule) shows the lock surface and no desktop frame. If a desktop frame shows, record the gap in `known-gaps.md`.
- [ ] Measure the wallet in each autologin case. Gate: after autologin plus first unlock, `secret-tool store` either succeeds without a KWallet prompt or the prompt is documented in `security-model.md`.
- [ ] Port sddm-kcm 6.8.2. Gate: System Settings > Login Screen changes autologin, `kde_settings.conf` changes, and the next boot follows it.
- [ ] Fast user switching. Gate: on the rig with two accounts, Switch User shows a greeter on VT2+. Logging in as the second user, then switching back, returns to the first session unlocked by password, and `loginctl list-sessions` shows two user sessions and one active.
- [ ] Greeter layout: set `/etc/keymap` to `de-latin1`. Gate: the greeter's password field types `z` for the physical Y key (rig `--type`), and the layout indicator reads `de`.
- [ ] Docs, same change: `boot-and-init.md` (the login), `session.md` (greeter runtime dir; the session wrapper), `security-model.md` (PAM stacks; greeter runtime dir; nullok; autologin and the wallet), `docs/kdos/04-programs/kinstall.md` (autologin writer), `docs/kdos/02-user-guide` page on logging in, `docs/kfs/16-wiki/login-and-autologin.md`, `known-gaps.md` (greeter beta status), CLAUDE.md rule 4 (1.4.1).

### 3.5 polkit

**Model: hybrid** (1.3 A3). With `-Dsession_tracking=elogind`, every subject in an active local session matches upstream `allow_active` defaults. polkit-kde-agent-1 answers `auth_admin*` challenges. `50-kdos.rules` shrinks to the grants upstream defaults do not give and that the plan wants without a prompt.

Turning on session tracking wakes dormant `allow_active` defaults, so the rules file and `security-model.md` change in the same commit as the polkit rebuild [measured: res-session risk]. The current header ("WHY THERE IS NO AUTHENTICATION AGENT", "ONE MECHANISM, NEVER TWO", `50-kdos.rules:4-17`) states a constraint that stops being true at that commit [measured].

**polkit-kde-agent-1 6.8.2**: installs `polkit-kde-authentication-agent-1.desktop` in autostart with `X-KDE-autostart-phase=0` (`CMakeLists.txt:100`), so plasma_session starts it on classic boot. Its `main.cpp:69-78` exits when its session does not resolve, which is why elogind must be up first [measured: res-session item]. The recipe deletes the user unit (`CMakeLists.txt:103`).

**`50-kdos.rules` rewritten.** Each rule tests `subject.local && subject.active` in addition to the group:

| Block | Actions | Result | Why |
|---|---|---|---|
| wheel, local and active | `org.freedesktop.NetworkManager.settings.modify.system` and the NM wifi and network toggles that today's file grants | YES | plasma-nm edits system connections without a prompt for an admin. Without session tracking these were `allow_any` refusals; now upstream gives some of them to `allow_active` already. Keep only those that upstream still sends to `auth_admin` (measure per action) |
| wheel, local and active | the udisks2 `*-system`, `*-others`, `modify-device*`, `open-device*`, `loop-*` and `encrypted-*` actions in today's list | YES | Dolphin and the Device Notifier mount internal disks and unlock LUKS without a password, as today |
| wheel, local and active | `org.debian.pcsc-lite.access_pcsc`, `access_card`; `ModemManager1.Device.Control`, `Messaging` | YES, or delete if upstream `allow_active` covers them (measure) | — |
| wheel | `org.gnome.gparted`, `org.kde.k3b.updatepermissions`, `org.kde.ktexteditor6.katetextbuffer.savefile`, `org.libvirt.unix.manage`, `net.nokyan.Resources.kill` | `AUTH_ADMIN_KEEP` | Once an agent exists, a prompt is the right answer for raw-disk and root-file writes. Today they are YES only because no prompt could be shown |
| `plasma-setup` user | the action list of `files/plasma-setup-polkit.rules` | YES | shipped by the plasma-setup recipe as its own file, not merged into 50-kdos [measured: res-session item] |
| explicit keeps | `org.freedesktop.fwupd.update-internal`, bolt and gvfs admin actions | `AUTH_ADMIN_KEEP` | named so that a future broad rule cannot widen them [measured: res-session item "50-kdos.rules"] |

The `seat` tier moves to polkit in the same change. kdos-powerd's `suspend`, `poweroff` and `reboot` verbs are deleted, because Plasma calls login1 and polkit's `org.freedesktop.login1.*` `allow_active=yes` covers the person at the machine. kdos-oomd's `seat` read tier (`main.c:62`, `:587-591`) becomes "the uid owns an active elogind session" (`sd_uid_get_state()` through libelogind, or `loginctl`). kdos-mountd follows whatever the Deletion section decides for it [measured: inv-features: DELETE proposed]. Only after all three, delete `seat` from `fs/etc/group:29` and from `install.c:917-921`.

**KAuth actions** Plasma will raise, and the result each gets:

| Action id | Owner | Policy |
|---|---|---|
| `org.kde.powerdevil.backlighthelper.setbrightness`, `.brightness`, `.brightnessmax`, `.syspath` | powerdevil | upstream `allow_active=yes` [measured: ids from source] |
| `org.kde.powerdevil.chargethresholdhelper.setthreshold`, `.getthreshold`, `.setconservationmode`, `.getconservationmode` | powerdevil | upstream; charge thresholds are owned by TLP (1.3 A6), so PowerDevil's battery-threshold page is hidden. Measure whether a kcfg key hides it, else document |
| `org.kde.powerdevil.discretegpuhelper.hasdualgpu`, `.wakeupsourcehelper.setwakeupcount` | powerdevil | upstream |
| `org.kde.kcontrol.kcmsddm.save`, `.sync`, `.reset` | sddm-kcm | upstream `auth_admin_keep` [inferred: ids follow the kcm's `.actions`; measure] |
| `org.kde.kpmcore.externalcommand.init` | kpmcore (Partition Manager) | upstream `auth_admin` [inferred; measure] |
| kinfocenter helpers (dmidecode and the like) | kinfocenter | upstream [inferred; measure the ids] |
| `org.freedesktop.timedate1.set-timezone`, `.set-time`, `.set-ntp`, `.set-local-rtc` | kdos-settingsd | timezone and ntp `allow_active=yes`; time and local-rtc `auth_admin_keep`, the systemd-timedated defaults [inferred] |
| `org.freedesktop.hostname1.set-hostname`, `.set-static-hostname`, `.set-machine-info` | kdos-settingsd | `auth_admin_keep` |
| `org.freedesktop.locale1.set-locale`, `.set-keyboard` | kdos-settingsd | `auth_admin_keep` |
| `org.freedesktop.accounts.*` | accountsservice | upstream (own user: `allow_active`; other users: `auth_admin`) |
| `io.kdos.firewall.set`, `io.kdos.accent.set` | kdos-powerd's D-Bus face (3.7) | `auth_admin_keep` for firewall, `allow_active=yes` for accent |

Tasks:

- [ ] Port polkit-kde-agent-1 6.8.2 without its user unit. Gate: on the rig, `pkexec true` in Konsole raises the KDE dialog, and after the correct password the exit status is 0.
- [ ] Rewrite `50-kdos.rules` in the same commit as the polkit rebuild, deciding each row by measuring upstream defaults (`pkaction --verbose --action-id <id>`). Gate: the existing `testing/fixtures/polkit` checks are rewritten for the new table and pass under `selftest.sh`. On the rig, mounting a second virtio disk from Dolphin as `kdos` (wheel) raises no prompt, and as a non-wheel second user it raises an admin prompt.
- [ ] Move the `seat` tier and delete the group, as above. Gate: `grep -rn '"seat"\|KO_SEAT\|KP_SEAT\|KM_SEAT' src` returns nothing, `selftest.sh` passes, and `kdos doctor` run as a non-admin active user still reads kdos-oomd's state.
- [ ] Measure the four `[inferred]` KAuth ids. Gate: a table in `security-model.md` generated from `grep -h '^\[' /usr/share/kf6/kauth/*.actions` (or the polkit `.policy` files) on a built tree.
- [ ] Docs, same change: `security-model.md:456-614` (the whole "polkit, and why the desktop has no authentication agent" chapter is replaced by "polkit: the agent and the KDOS grants"), `security-model.md` Root daemons "Who may talk to them" (`:306-343`, the `seat` tier), `daemons.md` (kdos-powerd and kdos-oomd authorisation), `docs/kfs/16-wiki/users-and-groups.md` (`seat`), the polkit `kfs.md`, and the new `50-kdos.rules` header stating the hybrid rule.

### 3.6 systemd-shaped D-Bus services

**hostname1, timedate1, locale1: kdos-settingsd** (1.3 A4). The verdict reframed this as a choice: the greeter layout has an XKB fallback, and the `etc/X11` lint applies to payloads [measured: res-session verdict 6]. The choice falls on a KDOS daemon for reasons measured in openrc-settingsd's source:

- Its storage paths are other distributions' layouts: `/etc/env.d/02locale`, `/etc/conf.d/keymaps`, `/etc/conf.d/loadkeys`, `/etc/X11/xorg.conf.d/30-keyboard.conf` and `00-keyboard.conf` (`localed.c:1290-1315`), and `/etc/timezone`, `/etc/hardwareclock` and `/etc/conf.d/hwclock` (`timedated.c:824-828`). KDOS keeps `/etc/hostname`, `/etc/keymap`, `/etc/localtime` + `/etc/profile.d/20-timezone.sh` and `/etc/modprobe.d/kdos-regdom.conf` [measured].
- Its NTP control shells out: `system("/etc/rc.d/rc.ntpd start")` (`timedated.c:268-402`). That breaks the "no system() and no shell" convention and knows nothing of chrony under ksvc [measured].
- Adapting it means patching most of its path table plus its NTP backend, on 4,136 lines of glib [measured: res-session item]. kdos-settingsd reuses code KDOS already has: kdos-powerd's timezone writer (`main.c:808-900`: `/etc/localtime`, `20-timezone.sh`, the cfg80211 regdomain) and the VT-to-XKB table (`session-common.sh:34-84`).

**kdos-settingsd**, `src/daemons/kdos-settingsd/`, C on basu and libkbase, ~1,000-1,500 lines [estimate], init script `43_settingsd.sh` (after polkitd):

| Interface | Properties served | Methods | Storage |
|---|---|---|---|
| `org.freedesktop.hostname1` | Hostname, StaticHostname, PrettyHostname, IconName, Chassis, KernelName/Release/Version, OperatingSystemPrettyName (from `/etc/os-release`) | SetHostname, SetStaticHostname, SetPrettyHostname, SetIconName, SetChassis | `/etc/hostname` (read by `05_hostname.sh`), `/etc/machine-info` |
| `org.freedesktop.timedate1` | Timezone, LocalRTC, CanNTP, NTP, NTPSynchronized, TimeUSec, RTCTimeUSec | SetTimezone, SetTime, SetLocalRTC, SetNTP, ListTimezones | `/etc/localtime`, `/etc/profile.d/20-timezone.sh`, regdomain; NTP through `service start/stop chrony` via the argv builder; NTPSynchronized from `adjtimex()` |
| `org.freedesktop.locale1` | Locale, VConsoleKeymap, VConsoleKeymapToggle, X11Layout, X11Model, X11Variant, X11Options | SetLocale, SetVConsoleKeyboard, SetX11Keyboard (with `convert`) | `/etc/locale.conf` (new: KDOS sets no `LANG` anywhere today [measured: grep of `fs/`]), `/etc/keymap` (+ `loadkeys` on the text VTs), `/etc/kdos/xkb.conf` (new; never `/etc/X11`) |

Every method checks polkit with interactive authorisation allowed, using the exact action ids in 3.5. Those are the ids plasma-setup's rule names (`plasma-setup-polkit.rules`), so its OEM flow works unmodified [measured: res-session verdict 6 correction]. Consumers: the Date & Time KCM saves only through timedate1 (`kcms/dateandtime/main.cpp:43-97`); plasma-setup uses all three (`timeutil.cpp:17`, `hostnameutil.cpp:119`, `keyboardutil.cpp:104-136`, `languageutil.cpp:71`); KWin `--locale1` reads locale1; startplasma's locale1 query only warns on failure (`startplasma.cpp:815-845`) [measured: res-session item, prior verdict 3].

**accountsservice 26.27.3** (`ports/core/auth/accountsservice/`): `-Delogind=true -Dadmin_group=wheel -Dwtmpfile=/var/log/wtmp -Dsystemdsystemunitdir=no`, following Alpine [measured: res-session item]. Only `libaccountsservice` links sd-login; `accounts-daemon` does not. KDE consumers use D-Bus only: the Users KCM (`kcms/users` ships its own XML), the portal's Account dialog (`userinfodialog.cpp:32-33`), and SDDM faces read as files from `/var/lib/AccountsService/icons/<user>` [measured: res-session verdict 5]. The reason to port it is the Users KCM and avatars, not a link-time need. Init script `44_accounts.sh` runs `supervise accounts-daemon /usr/lib/accounts-daemon` and then `wait_bus_name org.freedesktop.Accounts`; there is no D-Bus activation file.

**sysupdate1: kdos-sysupdated.** Specified in the Software and updates section (1.3 A13). From this section it needs only that it starts after kdos-settingsd (`46_sysupdated.sh`) and that it is the root side of the two-marker boot blessing (3.1).

**KIO scopes and a user-bus `org.freedesktop.systemd1`. Decision: not served in this plan.** KIO launches without cgroup scopes when `systemd1` does not answer. `_KDE_APPLICATIONS_AS_SCOPE=1` would force scope mode, but a provider must then own `systemd1` on the session bus and answer `StartTransientUnit` quickly, because `ScopedProcessRunner` holds the child on an eventfd until the reply (`scopedprocessrunner.cpp:50-63`). It must also fail `ListUnitFilesByPatterns` so that startplasma stays on classic boot [measured: res-innovation verdict 8].

The gain is System Monitor's Applications page and per-app portal ids. The cell tree is not `user-UID.slice/user@UID.service`, and podman places box processes in `libpod-<id>` children that libksysguard never lists, so the gain is partial even with a shim [measured: res-innovation verdict I13].

- [ ] Spike, P2, owned by the Integration section: measure launch latency through a 200-line scope shim. Gate for adoption: p99 added latency under 5 ms over 100 launches, `startplasma` still logs "Using classic boot", and the user approves a `systemd1` bus name under rule 3. Without all three it is not built, and `known-gaps.md` records the empty Applications page.

Tasks:

- [ ] Build kdos-settingsd, with a `selftest.sh` fixture driving each method against a temporary `/etc` (the `KDOS_POWERD_ETC` pattern, `kdos-powerd/main.c` `fw_etc()`). Under ASan: `CC="cc -fsanitize=address,undefined -g" testing/selftest.sh`. Gate: the fixture passes. On the rig, the Date & Time KCM changes the zone to `Europe/Berlin` (`readlink /etc/localtime`, and `date` in a new Konsole) and toggles NTP (`service status chrony`).
- [ ] Move the timezone code out of kdos-powerd and the keymap table out of `session-common.sh` in the same change. Gate: `grep -n 'timezone' src/daemons/kdos-powerd/main.c` and `grep -n XKB_DEFAULT fs/usr/local/lib/kdos/session-common.sh` return nothing, and `kdos timezone <zone>` (if the verb stays) calls timedate1.
- [ ] Port accountsservice. Gate: the Users KCM lists `kdos`, and a set avatar appears at the SDDM greeter after logout.
- [ ] Docs, same change: `daemons.md` (new kdos-settingsd and accounts-daemon entries; kdos-powerd loses timezone), `docs/kdos/06-reference` (D-Bus names and config paths: `/etc/locale.conf`, `/etc/kdos/xkb.conf`, `/etc/machine-info`), `kdos-command.md` (timezone, hostname), `known-gaps.md` (Applications page), `security-model.md` (the new root daemon's input surface).

### 3.7 Power, lid, suspend, lock

**Ownership after the change:**

| Function | Owner | Path |
|---|---|---|
| Suspend, power-off, reboot | elogind | Plasma (libkworkspace, PowerDevil) → `org.freedesktop.login1.Manager` → polkit `login1.*` → system-sleep hooks → `/sys/power/state`, or `/sbin/poweroff` / `/sbin/reboot` (sysvinit runlevel 0/6) |
| Lid, power key, idle in a session | PowerDevil 6.8.2 | holds `handle-lid-switch`/`handle-power-key`/`handle-suspend-key` block inhibitors; policy in `/etc/xdg/powerdevilrc` (Look/Integration sections set the defaults) |
| Lid and keys at the greeter or a console | elogind `Handle*` (3.2) | — |
| Lock before sleep | kscreenlocker | takes a `sleep` delay inhibitor and locks on `PrepareForSleep(true)`, which elogind emits; `InhibitDelayMaxSec=5` bounds the wait [measured: res-session item "kdos-powerd"] |
| Power profiles | tlp-pd (TLP 1.10.2) | serves `org.freedesktop.UPower.PowerProfiles` and `net.hadess.PowerProfiles` (`tlp-pd.in:34-39`). PowerDevil uses only the former (`powerprofile.cpp:32-33`) [measured: res-session verdict TLP]. Supervised by ksvc from `55_tlppd.sh`, because its D-Bus file cannot activate it |
| TLP suspend and resume hooks | elogind system-sleep | `TLP_WITH_ELOGIND=1` installs the hook. The radio-restore work kdos-powerd does in `tlp_hook()` (`main.c:178-211`) moves there [measured: res-session missed 10] |
| NetworkManager sleep | NM itself | `-Dsuspend_resume=elogind` (3.2) |
| Battery charge thresholds | TLP | PowerDevil's helper is left unused (3.5) |

**kdos-powerd, reduced** (`src/daemons/kdos-powerd/main.c`, 1,438 lines today [measured]):

- **Deleted verbs and code:** `ping` stays as a liveness check. Deleted: `suspend`, `poweroff`, `reboot` (`:1017-1080`), `lock_before_suspend()` (`:1135-1190`, which looked for a `kdos-lock` process), the NM `Sleep()` call (`:141-176`), `tlp_hook()` and the `/sys/power/state` write (`:178-211`), the `autologin` verb (`:633`; sddm-kcm owns it) and the `timezone` verb (`:808-900`; kdos-settingsd owns it). The "WHY NOT THE OBVIOUS ALTERNATIVES" header (`:20-36`) is rewritten, because elogind now exists. The `kdos-power` client keeps `suspend`, `poweroff` and `reboot` as a client-side mapping to `loginctl suspend|poweroff|reboot`, so the lock before sleep is kscreenlocker's (above) and no daemon verb is involved.
- **Kept verbs:** `firewall` (the named-service table, `:269-330`, written to `/etc/nftables.d/50-kdos-services.nft`) and `accent` (`/etc/kdos/accent`, `/etc/vtrgb`, `limine.conf`). Both gain a D-Bus face on the system bus, `io.kdos.Power1` with `SetFirewallService(s name, b open)`, `ListFirewallServices()` and `SetAccent(s)`. It is authorised through polkit (`io.kdos.firewall.set`, `io.kdos.accent.set`, 3.5), so the native KCMs from the Integration section can call it without a socket client. The SO_PEERCRED socket stays for the `kdos` CLI.
- **Firewall rows for KDE remote-desktop.** Keep `kdeconnect` (`meta l4proto { tcp, udp } th dport 1714-1764 accept`, `:322-323`). The `vnc` row's description ("this desktop over VNC (wayvnc)", `:327`) becomes krfb, because wayvnc is deleted with wlroots. Add `rdp` (`tcp dport 3389 accept`, "this desktop over RDP (krdp)"). Ports come from krfb and krdp defaults [inferred; measure from their sources]. plasma-firewall has only firewalld and ufw backends and drives systemd jobs (`firewalldclient.cpp:19,63,579`), so it is not ported. The KDOS firewall KCM belongs to the Integration section [measured: res-session missed 5].

**kdos-energyd**: unchanged as a daemon. It keeps per-app and per-box energy accounting and sets no power policy. Its consumer moves from kdos-res to a ksystemstats plugin (Integration section) [measured: inv-code KEEP].

**kdos-oomd protected list** (`main.c:240-252`). Every name is a 15-character `comm` [measured: inv-features verdict]. The list becomes:

```
"kwin_wayland", "kwin_wayland_wr", "plasmashell", "ksmserver", "kded6",
"plasma_session", "kglobalacceld", "kscreenlocker_g", "Xwayland",
"xdg-desktop-por", "polkit-kde-auth", "ksecretd",
"sddm", "sddm-helper", "sddm-greeter-qt", "elogind", "turnstiled",
"dbus-daemon", "ksvc", "kdos-powerd", "kdos-energyd", "kdos-oomd",
"kdos-settingsd", "accounts-daemon", "wireplumber", NULL   (+ the pipewire prefix rule)
```

`seatd` and the cell names go. Plasma's OOM notifier listens only to systemd1 (`oom_notifier/module.cpp:47-122`), so kdos-oomd sends its own per-user notification naming the box. That work is in the Integration section [measured: inv-features verdict]. Each comm is measured on the rig before it is trusted, because a 15-character truncation error silently protects nothing (`ps -o comm= -p <pid>`). The elogind daemon's comm in particular is unmeasured.

Tasks:

- [ ] Port PowerDevil 6.8.2 (if not ported by the Ports section) and tlp-pd under ksvc. Gate: the battery applet's profile switcher on the rig (virtio battery or a laptop) changes `tlp-stat -m` between `power-saver`, `balanced` and `performance`.
- [ ] Strip kdos-powerd as listed and add `io.kdos.Power1`. Gate: `selftest.sh` firewall and accent fixtures pass over the socket and over D-Bus (`gdbus call --system --dest io.kdos.Power1 …`). `wc -l main.c` falls by the deleted blocks.
- [ ] Suspend path. Gate: on a laptop (the rig cannot suspend meaningfully), closing the lid locks before sleep, and resume shows the lock screen; `kdos-power suspend` from Konsole also resumes onto kscreenlocker; `kdos-power firewall list` shows `rdp`. NetworkManager reconnects within 10 s (`nmcli -t g`). `tlp-stat -s` after resume shows the AC/BAT profile reapplied. `/var/log/messages` shows exactly one NM sleep and one wake.
- [ ] Greeter lid. Gate: lid closed at the SDDM greeter suspends (elogind `HandleLidSwitch`), and the machine resumes to the greeter.
- [ ] Rewrite `ko_protected` and its comment. Gate: the `testing/fixtures/oomd` cases are updated and pass. On the rig, every name in the list matches a running process's `comm` or is documented as conditional (greeter-only, box-only).
- [ ] Docs, same change: `daemons.md` (kdos-powerd's verbs and D-Bus face, kdos-oomd's list, kdos-energyd's consumer), `kdos-command.md` (`kdos-power` client: `suspend` and `poweroff` map to `loginctl`), `security-model.md` "The firewall" (`:414-455`, the krfb and krdp rows), `docs/kfs/16-wiki` power pages, and the TLP and PowerDevil `kfs.md` files.

### 3.8 Session save and restore, crash handling, secrets, input method

**Session restore.** ksmserver and kded6 come up under plasma_session on classic boot [measured: prior verdict 3]. KWin 6.8.2 implements staging `xdg-session-management-v1` (`src/wayland/CMakeLists.txt:64`) [measured: res-session item]. Plasma's default, "restore previous session", is kept. `kdos-session-save` and `kdos_session_once`'s restore are deleted with the cell desktop.

- [ ] Gate: open Konsole and Kate, log out, log in. Both windows return (rig shot). Record any app that does not in `known-gaps.md`.

**Crash handling** (1.3 A5). DrKonqi 6.8.2 needs systemd-coredump (`CMakeLists.txt:99-100`) [measured]. The first release ships KCrash with no handler. A crashed app exits, and plasmashell and KWin restart themselves: kwin_wayland_wrapper relaunches KWin, and plasmashell uses KCrash auto-restart [inferred; measure both]. There is no crash dialog: DrKonqi needs systemd-coredump and no downstream snapshot is carried. `known-gaps.md` states it.

- [ ] Gate (first release): `kill -SEGV $(pidof kwin_wayland)` keeps the session's clients alive and KWin back within 5 s. `kill -SEGV $(pidof plasmashell)` brings the panel back.

**KWallet and ksecretd** (1.3 A7):

- `ports/core/kf6/kwallet/build.sh:19` becomes `-D BUILD_KSECRETD=ON`, and the `[KSecretD] Enabled=false` heredoc (`:27-31`) is deleted. Without it, kwallet-pam would exec `/usr/bin/ksecretd`, which does not exist: `KF6WalletConfig.cmake:38` already points `KWALLETD_BIN_PATH` at `bin/ksecretd`, and `build/fs/usr/bin` has only `kwalletd6` [measured: res-session verdict 11]. So this is a prerequisite of kwallet-pam, not polish.
- Port kwallet-pam 6.8.2 (`pam_kwallet.c:412-413` execs `ksecretd --pam-login`; needs socat 1.8.1.3, present; installs the `pam_kwallet_init` autostart) without its user unit (`CMakeLists.txt:106`).
- `known-gaps.md:429-436` ("a saved login needs KeePassXC running") is rewritten. KeePassXC remains an app. Both claim `org.freedesktop.secrets`, and ksecretd is started first. KeePassXC's own Secret Service integration therefore stays off by default (its config), and the conflict is documented.
- [ ] Gate: after a password login, `secret-tool store --label t k v` and `secret-tool lookup k` succeed with no prompt, and `pgrep -a ksecretd` shows `--pam-login`. The autologin case is the 3.4 measurement.

**Input method.** fcitx5 is started by KWin: `/etc/xdg/kwinrc [Wayland] InputMethod[$e]=/usr/share/applications/org.fcitx.Fcitx5.desktop`, the setting the Virtual Keyboard KCM writes [measured: inv-code item; inv-features item]. `fcitx5 -d` and kdos-ime (part of kdos-shell, `ime.c`) are deleted with the cell desktop. Port fcitx5-qt, fcitx5-gtk and fcitx5-configtool; all three are in the `kde-plasma` meta's `depends` beside fcitx5 and its engines (4.4). fcitx5-gtk 5.1.7 (the latest release, older than fcitx5 5.1.22) is built with the GTK 3 and GTK 4 modules and no GTK 2 module, on the `input` shelf, and writes GTK 3's `immodules.cache`, without which GTK 3 never loads a module that is not compiled into it [measured: porttest build, 2026-11-02].

`QT_IM_MODULE` and `GTK_IM_MODULE` stay unset. Qt 6 and GTK 4 then use text-input v3 (GTK 4's Wayland module registers at priority 100, fcitx5-gtk's at 10) and Qt 5 apps text-input v2; GTK 3 chooses by locale, where its Wayland module claims none and fcitx5-gtk's claims `*`, so a GTK 3 app reaches fcitx5 over D-Bus through the module [measured: gtk 3.24.52 and 4.24.0 gtkimmodule.c, imwayland.c, gtkimcontextwayland.c; fcitx5-gtk fcitxim.c]. fcitx5 is built `ENABLE_X11=Off` [measured: inv-code item], so of the Xwayland apps only GTK 3 and GTK 4 ones get an IME, through fcitx5-gtk's module. Whether to turn X11 on and set `XMODIFIERS=@im=fcitx` is decided by measuring one X11-only app (Integration section).

The Meta+Space collision with fcitx5's `EnumerateGroupForwardKeys` is 1.3 A10. `/etc/xdg/fcitx5/config` moves that chord, and `fs/etc/skel/.config/fcitx5/profile` stays [measured: inv-code "kept fs"].

The greeter gets no fcitx5 (`InputMethod=` is empty in 3.4). Its on-screen keyboard is plasma-keyboard, once ported.

- [ ] Gate: in Kate under Plasma, Ctrl+Space toggles Pinyin (fcitx5-chinese-addons), and the candidate window appears at the caret (rig shot). `pgrep -a fcitx5` shows a single instance whose parent is kwin_wayland.
- [ ] Gate (fcitx5-gtk): `fcitx5-gtk3-immodule-probing` prints `fcitx` and `fcitx5-gtk4-immodule-probing` prints `wayland` in Konsole; Ctrl+Space toggles Pinyin in a GTK 3 and a GTK 4 program on the host and in a GTK 3 program run with `GDK_BACKEND=x11` (rig shots).

Docs, same change: `session.md` (input methods: the section at `:941-995`; secrets; restore), `security-model.md` (the secret store), `known-gaps.md`, and the kwallet, kwallet-pam and fcitx5 `kfs.md` files.

### 3.9 Seamless boot: kdos-splash → SDDM → KSplash

**Design rule** [source: res-innovation I7]: every stage ends on full black and the next starts from black. The modeset KWin always performs on its first atomic commit with `ALLOW_MODESET` (`drm_commit.cpp:164,182`) then falls inside a black frame [measured: res-innovation I7 evidence]. Stages:

1. Limine in accent colours.
2. kdos-splash on `/dev/fb0`.
3. `kdos-splash quit` collapses to black, synchronously (`rcS` tail; `boot-and-init.md:581`) [measured].
4. SDDM's greeter KWin modesets on black, and the theme fades in from black.
5. After login, the greeter KWin exits. The session KWin modesets again, and KSplash fades in from black and continues kdos-splash's layout (section 5 draws it).
6. KWin's login effect, or section 5's replacement, reveals the desktop.

In the A15 autologin mode, stage 4 does not happen, so there is one KWin start and one modeset.

**What is measured and what is not:**

- The kernel has `CONFIG_SYSFB_SIMPLEFB=y` and `CONFIG_DRM_SIMPLEDRM=y` beside `CONFIG_FB=y`, `CONFIG_FB_VESA=y` and `CONFIG_FB_EFI=y` (`ports/core/boot/linux/kdos.config:826,3478,3502-3504`). It also has `FRAMEBUFFER_CONSOLE_DEFERRED_TAKEOVER=y` (`:3553`). No `DRM_FBDEV_EMULATION` line is in the 7.2.11 config [measured: grep].
- KDE's packaging recommendations ask for simpledrm with every `CONFIG_FB_*` driver off [source: community.kde.org/Distributions/Packaging_Recommendations, res-branding item]. kdos-splash draws on `/dev/fb0`, so what provides fb0 decides whether that is possible. **Unmeasured:** whether fb0 is simpledrm's fbdev emulation or efifb/vesafb, on the rig (virtio-gpu, bochs) and on real Intel/AMD hardware.
- GPU drivers are modules (`DRM_I915=m`, `DRM_AMDGPU=m`, `DRM_XE=m`, `:3444-3500`) [measured]. They load during rcS's `01_udev`, while the splash is drawing. The splash survives that driver handover today only on the rig's built-in `DRM_VIRTIO_GPU=y` and `DRM_BOCHS=y` [inferred]. **Unmeasured on hardware.**
- On an installed machine (`console=tty0`), fbcon takes the framebuffer over about 1 ms after `/init` starts and draws boot messages over the splash [measured: `boot-and-init.md:561-566`; known-gaps]. An SDDM greeter on VT1 inherits a VT fbcon has drawn on until KWin sets `KD_GRAPHICS` through elogind's `TakeControl`.
- KWin startup can show 2-4 s of black, attributed to a bit-depth change needing a full modeset [source: bugs.kde.org/show_bug.cgi?id=514351]. `KWIN_DRM_PREFER_COLOR_DEPTH` is read at `drm_output.cpp:388` [measured: res-innovation I7 evidence].

Tasks (P1, after 3.4's greeter gates pass):

- [ ] Measure fb0's owner. Gate: `cat /proc/fb` and `dmesg | grep -iE 'simpledrm|efifb|vesafb|fbcon|i915|amdgpu'` recorded for the rig, one Intel machine and one AMD machine, in `boot-and-init.md`.
- [ ] If simpledrm provides fb0 everywhere, turn off `CONFIG_FB_EFI` and `CONFIG_FB_VESA`, as KDE recommends. Gate: kdos-splash still draws on all three machines (photograph or video) and `/proc/fb` lists the DRM fbdev.
- [ ] Driver-load handover. Gate: a 60 fps video of the Intel and AMD boots shows the splash continuing through i915/amdgpu probe with at most one black blink. If not, `rcS` loads the GPU module before the splash's first frame (in the initramfs), measured for boot-time cost.
- [ ] fbcon over the splash on installed machines. Gate: with `quiet loglevel=3 vt.global_cursor_default=0` on the installed command line (kinstall's Limine entry), no text is visible between Limine and the greeter on the Intel machine. Measure whether `fbcon=nodefer` or `fbcon=map:1` (moving fbcon off VT1) is needed instead. `known-gaps.md`'s entry is removed or rewritten.
- [ ] Splash-to-greeter. Gate: the video shows black between `kdos-splash quit` and the greeter's first frame, with no fbcon text and no cursor. The KWin greeter log has one `ALLOW_MODESET` commit per output.
- [ ] Greeter-to-session. Gate: with a password login, the video shows the greeter fading to black, at most one black interval of no more than 1 s, then KSplash from black. Try matching the greeter's and the session's KScreen configuration and bpc (same mode via `/var/lib/sddm/.config/kwinoutputconfig.json` written by sddm-kcm's sync) and `KWIN_DRM_PREFER_COLOR_DEPTH=24` in both environments, set in `00-kdos.conf` and in `/etc/xdg/plasma-workspace/env/kdos-kwin.sh`. Keep each only if it shortens the blink, measured.
- [ ] Autologin (A15). Gate: the video shows splash, black, then KSplash (or the lock surface in LockOnStart mode), with no greeter frame.
- [ ] Rig gate for every task above: a no-sleep shot (rig-shot-cost rule) taken immediately after `kdos-splash quit` shows a black frame. A plain rig is software-rendered, so modeset timing gates run on hardware only. The rig proves ordering, not blink length.
- [ ] Docs, same change: `boot-and-init.md` (the splash section `:530-600` gains the handover to SDDM; the "Three facts constrain the splash" list gains fb0's measured owner; the quit paragraph names SDDM instead of the tty1 getty and the compositor), `known-gaps.md` (splash overdraw), `kinstall.md` (kernel command line), and the linux and kdos-splash `kfs.md` files.


## 4. The KDE port set and how it is built

This section lists every port the KDE desktop adds, bumps, rebuilds or skips. It says where each one goes (shelf and phase) and how the set is built, published and tested. Versions follow 1.5. Defaults A1, A5, A8 and A9 from 1.3 apply here: English only, no DrKonqi, no kuserfeedback, and KDOS's Qt code in `src/desktop`. Session-stack rebuilds (elogind, polkit, PAM, NetworkManager) are owned by section 3. They appear in 4.2 only as port rows, so the build order is complete in one place.

Baseline [measured]: 2,083 recipes (`find ports/core -name kpkgbuild | wc -l`); 1,984 packages installed in `build/fs`; 2,631 `ports/sources.idx` lines; 67 KF6 ports at 6.30.0; four Plasma ports at 6.7.5 (breeze, kwayland, libksysguard, plasma-integration); plasma-wayland-protocols 1.22.0; 55 Gear ports at 26.08.1; Qt 6.12.0, with qtwebengine at 6.11.2.

### 4.1 Module inventory

#### 4.1.1 KDE Frameworks: 7 missing of 74

The 6.31 release lists 74 frameworks [measured: `~/.cache/kdos-portwork/kde-research/kf631.list`]. The tree has 67 of them. All 74 move to 6.31.0 in one family change (1.5.2 rule 3). Shelf `kf6` (shelf rule 3). Phase `43_toolkits`, except ECM in `41_system`.

| Framework | Action | Needed by | Notes |
|---|---|---|---|
| krunner | PORT | kwin (overview, runners), plasma-workspace, plasma-desktop, milou, powerdevil, systemsettings, akonadi-search, kweather, zanshin (REQUIRED) | Its reverse dependencies exceed 20, so it needs a `kfs.md` |
| ksvg | PORT | kwin, libplasma, kscreenlocker, aurorae, kclock, knights | Its reverse dependencies exceed 20, so it needs a `kfs.md` [prior] |
| syndication | PORT | akregator, alligator, kasts, ktorrent (RSS plugin) | — |
| kdav | PORT | kdepim-runtime (DAV resource, REQUIRED) | — |
| kuserfeedback | **SKIP** (A8) | Optional everywhere | Not ported. Every new KDE recipe that probes it still passes `-DCMAKE_DISABLE_FIND_PACKAGE_KF6UserFeedback=ON`, because a later port of it would otherwise be found with no error [measured: dolphin `build.sh:24`, kate `:23`]. Telemetry code then never links, even if someone ports it |
| kapidox | **SKIP** | No consumer | It is an API-documentation generator (Python). No port builds QCH or API docs |
| oxygen-icons | **SKIP** | No REQUIRED consumer [prior] | Breeze icons are the default (B9). Revisit if an app's icon fallback shows missing glyphs |

#### 4.1.2 Plasma 6.8.2: 73 modules

The list comes from the 6.8.2 tarball list [measured: `~/.cache/kdos-portwork/kde-research/p682s.list`]. The counts are 56 PORT, 4 BUMP and 13 SKIP.

Shelves follow the shelf rules. Desktop-platform pieces go to `kde` (rule 6). User-facing tools are filed by function (rule 5). Themes and sounds go to `themes` (rule 4).

Phase `43` means a library that a Gear port or another 43 port links. `50` means the session. `41` means data with no toolkit in its closure (4.3).

| Module | Action | Shelf | Phase | Key flags and notes |
|---|---|---|---|---|
| aurorae | PORT | kde | 50 | KWin's SVG decoration engine; KWin runtime |
| bluedevil | PORT | kde | 50 | BlueZ policy is open, so no polkit grant is needed [prior] |
| breeze | BUMP 6.7.5 → 6.8.2 | themes | 43 | 4.2 |
| breeze-grub | SKIP | — | — | KDOS boots Limine, and kpkg lint refuses `etc/grub.d` [measured: `src/system/kdos-kpkg/lint.c:146`] |
| breeze-gtk | PORT | themes | phaseclosure decides (41 or 42) | Replaces kdos-gtk-theme (D1, B9). Needs sassc. Whether it needs pycairo for asset rendering is to be measured |
| breeze-plymouth | SKIP | — | — | No Plymouth. kdos-splash owns boot (3.9) |
| discover | PORT | kde | 50 | 4.2 lists the backend flags (D10) |
| drkonqi | SKIP (A5) | — | — | `find_package(Systemd 255) TYPE REQUIRED` [measured: res-session `drkonqi-CMakeLists.txt:99-100`]. No drkonqi-legacy |
| flatpak-kcm | SKIP (D10) | — | — | `pkg_check_modules(Flatpak REQUIRED)` [measured: res-innovation, flatpak-kcm `CMakeLists.txt:22`] |
| kactivitymanagerd | PORT | kde | 50 | Activities daemon. Remove its systemd user unit |
| kde-cli-tools | PORT | kde | 50 | kde-open, kioclient, keditfiletype, the kdesu front end and plasma-open-settings |
| kde-gtk-config | PORT | kde | 50 | Needs sassc at build time and xsettingsd at run time [prior]. It becomes the one GTK settings writer. `kdos theme` must stop writing `gtk.css` in the same change (7.2.3) |
| kdecoration | PORT | kde | 43 | KDecoration3. breeze links it |
| kdeplasma-addons | PORT | kde | 50 | `find_package(Corrosion REQUIRED)` at `CMakeLists.txt:114`, for the kameleon QMK helper with 101 crates [measured: prior]. Use `vendoring = rust`, not a patch: a vendor bundle is a supported shape, and kameleon gives accent-coloured keyboard LEDs (D2) |
| kglobalacceld | PORT | kde | 50 | KWin REQUIRED |
| kinfocenter | PORT | sysmon | 50 | Its runtime tools are missing: aha and clinfo (4.1.4). Whether eglinfo, vulkaninfo and wayland-info are present is to be measured. Chimera's `fix-output.patch` fixes libc++ only, and KDOS builds with GCC and libstdc++, so it is not carried [measured: cports patch header] |
| kmenuedit | PORT | kde | 50 | — |
| knighttime | PORT | kde | 50 | KWin and plasma-workspace REQUIRED. Night light |
| kpipewire | PORT | kde | 43 | krfb REQUIRED; Spectacle and portal screencast |
| krdp | PORT | virt | 43 | The library behind kdeconnect's virtual-monitor plugin. Links freerdp 3.32.0 (present) |
| kscreen | PORT | kde | 50 | Display KCM and OSD |
| kscreenlocker | PORT | kde | 50 | PAM services `kde`, `kde-fingerprint` and `kde-smartcard` (3.4) |
| ksshaskpass | PORT | security | 50 | — |
| ksystemstats | PORT | sysmon | 50 | The CAP_PERFMON GPU helper's file capability is lost in kpkg's tar (accepted; recorded in its `kfs.md`) [prior] |
| kup | PORT | transfer | 50 | B7: a restic backend carried as a `.patch`. Its dependencies (libgit2, rsync) are to be measured |
| kwallet-pam | PORT | auth | 50 | Execs `KWALLETD_BIN_PATH` from KF6Wallet, so kwallet needs `BUILD_KSECRETD=ON` (4.2). socat 1.8.1.3 is present |
| kwayland | BUMP 6.7.5 → 6.8.2 | kde | 43 | — |
| kwayland-integration | SKIP | — | — | A KF5 KWindowSystem plugin, and the tree has no KF5 (no `ports/core/kf5`) [measured] |
| kwin | PORT | kde | 50 | 4.2 lists its flags |
| kwrited | PORT | kde | 50 | `write`/`wall` messages as notifications. Reads utmpx through utmps |
| layer-shell-qt | PORT | kde | 43 | Used by plasma-workspace, kscreenlocker and Spectacle |
| libkscreen | PORT | kde | 43 | Grouped with the Plasma libraries in 43 so that one wave builds them all |
| libksysguard | BUMP 6.7.5 → 6.8.2 | kde | 43 | 4.2 |
| libplasma | PORT | kde | 43 | Its reverse dependencies exceed 20, so it needs a `kfs.md`. kclock, knights, kweather and merkuro link it |
| milou | PORT | kde | 50 | KWin loads `org.kde.milou` |
| ocean-sound-theme | PORT | themes | 41 | Data only. kclock's default alarm |
| oxygen | PORT | themes | 43 | Style and decoration, opt-in. `-DBUILD_QT5=OFF` |
| oxygen-sounds | PORT | themes | 41 | Data only |
| plasma-activities | PORT | kde | 43 | gwenview, kio-extras, konqueror and 9 PIM modules use it |
| plasma-activities-stats | PORT | kde | 43 | plasma-workspace and sweeper REQUIRED; kio-extras `recentlyused:/` |
| plasma-bigscreen | SKIP | — | — | TV form factor |
| plasma-browser-integration | PORT | kde | 50 | Native-messaging host |
| plasma-desktop | PORT | kde | 50 | The Date & Time KCM writes through timedate1 from kdos-settingsd (A4). The Users KCM needs accountsservice |
| plasma-dialer | SKIP | — | — | Phone |
| plasma-disks | PORT | disk | 50 | smartmontools 7.5 is present. KAuth helper (A3) |
| plasma-firewall | SKIP | — | — | It has firewalld and ufw backends only, uses SystemdJob, and Gentoo sets `REQUIRED_USE firewall? ( systemd )` [measured: res-session missed]. A KDOS nftables KCM replaces it (section 6) |
| plasma-integration | BUMP 6.7.5 → 6.8.2 | kde | 43 | 4.2 |
| plasma-keyboard | PORT | input | 50 | Needs qt6-qtvirtualkeyboard. It is the greeter's `--inputmethod` (3.4) |
| plasma-login-manager | SKIP | — | — | Needs systemd at build, greeter and KCM level [measured: res-session verdict 2] |
| plasma-mobile | SKIP | — | — | Phone shell |
| plasma-nano | SKIP | — | — | A minimal shell for the mobile and bigscreen shells [inferred]. Task: grep every ported module for `org.kde.plasma.nano` before closing this |
| plasma-nm | PORT | kde | 50 | — |
| plasma-pa | PORT | kde | 50 | Runs over pipewire-pulse, started by turnstile (A2) |
| plasma-sdk | PORT | devtools | 44 | plasmoidviewer, lookandfeelexplorer and iconexplorer, for KDOS theme work. Nothing depends on it |
| plasma-setup | PORT | kde | 50 | D12 OEM. Remove its systemd unit, sysusers and tmpfiles files and ship an rcS hook instead (section 6) |
| plasma-systemmonitor | PORT | sysmon | 50 | No patch. Ship `/usr/share/kdos/xdg/plasma-systemmonitorrc` with a `pageOrder` that omits `applications.page` (it needs per-app cgroups, which elogind does not create). This does by configuration what Chimera's patch does to the kcfg default [measured: cports patch; inferred: the kcfg default can be overridden from XDG config] |
| plasma-thunderbolt | PORT | hardware | 50 | bolt 0.9.11 is present |
| plasma-vault | PORT | filesystem | 50 | gocryptfs 2.6.1 is present. CryFS and EncFS are not ported, so the vault offers gocryptfs only |
| plasma-welcome | PORT | kde | 50 | KDOS pages (7.7.1). Build with KF6UserFeedback disabled (A8) |
| plasma-workspace | PORT | kde | 50 | 4.2 and 4.7 |
| plasma-workspace-wallpapers | PORT | themes | 41 | 294.8 MB of images [prior]. It is built because the medium carries the whole tree (4.6) |
| plasma5support | PORT | kde | 43 | plasma-desktop REQUIRED; kdeplasma-addons data engines |
| plymouth-kcm | SKIP | — | — | No Plymouth |
| polkit-kde-agent-1 | PORT | kde | 50 | Remove its systemd user unit (`CMakeLists.txt:103`) [measured: res-session] |
| powerdevil | PORT | kde | 50 | Uses libddcutil (present). Power profiles come through tlp-pd (A6) |
| print-manager | PORT | printing | 50 | `-DSCP_INSTALL=OFF` [prior]. cups is in 41 |
| qqc2-breeze-style | PORT | kde | 43 | Optional for tokodon; the QtQuick style |
| sddm-kcm | PORT | kde | 50 | Writes `/etc/sddm.conf.d/kde_settings.conf` through KAuth |
| spacebar | SKIP | — | — | SMS, phone |
| spectacle | PORT | graphics | 50 | Needs kquickimageeditor, kpipewire and layer-shell-qt |
| systemsettings | PORT | kde | 50 | Hosts the KDOS KCMs (section 6) |
| union | PORT | kde | 43 | plasma-workspace REQUIRED (`CMakeLists.txt:362`) [measured]. `widgetStyle` stays Breeze (B8). Needs cxx-rust-cssparser |
| xdg-desktop-portal-kde | PORT | xdg | 50 | Replaces xdg-desktop-portal-kdos and -wlr. Remove its systemd user unit (`data/CMakeLists.txt:9`) |

`kwin-x11` is not in the 6.8.2 list [measured]; Xwayland stays through `KWIN_BUILD_X11=ON`.

#### 4.1.3 KDE Gear 26.08.2: all of it

Gear 26.08.2 has 252 modules [measured: prior `gear-inventory.json` merged.summary; `gear/sizes.txt` has 252 rows]. 55 are in the tree and are bumped (4.2). 197 are new: 196 records plus kcolorchooser, which the records missed. Of the new ones, 4 cannot work and are skipped [measured: prior merged.skip]. That ports **193**. A veto ports kcron and cronie, its one missing dependency [measured: prior records kcron.missing_deps], for 193 Gear ports plus one third-party port.

| Category | New ports | Modules (shelf in brackets where it is not the obvious one) |
|---|---|---|
| Applications | 54 | arianna, audex, calligra, dragon, francis, ghostwriter, isoimagewriter, itinerary, juk, kalk, kalm, kasts, kbackup, kcalc, kcharselect [i18n], kclock [pim], kcolorchooser [graphics], kcron [kde], kdf, kdialog [xdg], keditbookmarks [kde], keepsecret, keysmith, kfind, kget, kgpg, kgraphviewer, khelpcenter [kde], kimagemapeditor, kmix [audio-io], kmouth [accessibility], koko, kongress, konqueror, kontrast, krdc, krfb, kruler, ksystemlog, kteatime, ktimer, ktorrent, kwalletmanager, kweather [gis], lokalize, neochat, partitionmanager [disk], plasmatube, qrca, skanpage, sweeper, telly-skout, tokodon, yakuake [shells] |
| Mobile-first, convergent | 9 | alligator, angelfish, audiotube, calindori, krecorder, ktrip, plasma-camera, plasma-phonebook, qmlkonsole |
| Games | 34 | bomber, bovo, granatier, kajongg, kapman, kblackbox, kbreakout, kdiamond, kdominate, kfourinline, kgoldrunner, kigo, killbots, kiriki, kjumpingcube, klickety, klines, knavalbattle, knetwalk, knights, kolf, kollision, konquest, kshisen, ksirk, ksnakeduel, kspaceduel, ksquares, ktuberling, kubrick, lskat, palapeli, picmi, skladnik |
| Education | 12 + 1 data | artikulate, blinken, kanagram, kbruch, khangman, kig, kiten, klettres, kmplot, minuet, rocs, step; kdeedu-data |
| PIM applications | 19 | akonadi-calendar-tools, akonadi-import-wizard, akregator, grantlee-editor, kaddressbook, kalarm, kdepim-runtime, kmail, kmail-account-wizard, kmailtransport, kontact, kontactinterface, korganizer, kpimtextedit, mbox-importer, merkuro, pim-data-exporter, pim-sieve-editor, zanshin |
| Libraries | 34 | akonadi, akonadi-calendar, akonadi-contacts, akonadi-mime, akonadi-search, calendarsupport, eventviews, grantleetheme, incidenceeditor, kaccounts-integration, kcalutils, khealthcertificate, kidentitymanagement, kimap, kitinerary, kldap, kopeninghours, kosmindoormap, kpkpass, kpmcore, kpublictransport, ksmtp, ktnef, kunifiedpush, kweathercore, libgravatar, libkdepim, libkgapi, libksieve, libktorrent, mailcommon, mailimporter, messagelib, pimcommon |
| Developer tools | 12 | accessibility-inspector, akonadiconsole, kapptemplate, kcachegrind, kdebugsettings, kde-dev-scripts, kde-dev-utils, kirigami-gallery, kompare, massif-visualizer, poxml, umbrello |
| KIO workers | 6 | audiocd-kio, kamera, kdesdk-kio, kio-admin, kio-gdrive, kio-zeroconf |
| Plugins and parts | 11 | colord-kde, dolphin-plugins, kaccounts-providers, kde-inotify-survey, kdenetwork-filesharing, kdepim-addons, kdev-php, kdev-python, markdownpart, signon-kwallet-extension, svgpart |
| Thumbnailers | 1 | kdesdk-thumbnailers |
| **Total** | **193** | |

Each module's shelf comes from its record [measured: `prior/gear-inventory.json` records[].shelf]. All shelves named there are in `ports/shelves`. Libraries go to 43 and everything nothing depends on goes to 44 (4.3).

**Skipped: 4 that cannot work.**
- kjournald: libsystemd REQUIRED.
- kmag: `QScreen::grabWindow(0)` returns nothing on Wayland.
- kmousetool: X11 and XTest REQUIRED.
- plasma-settings: the Plasma Mobile settings shell.

All four are measured [prior merged.skip] and scope-neutral: none can run on KDOS.

**kcron and cronie (B25).** kcron edits crontabs through the `crontab` binary, and its KAuth helper writes `/etc/crontab` [measured: prior records kcron.concerns]. KDOS ships no crond today; snooze (`ports/core/base/snooze`, `fs/etc/init.d/18_timers.sh`) keeps the system timers, and cron is added for user crontabs only.
- [ ] Port cronie (shelf `base`, phase 41, no systemd unit installed, supervised by an rcS script under ksvc) and kcron (shelf `kde`, phase 44). Gate: on the rig, System Settings → Task Scheduler adds a user job, `crontab -l` lists it and it runs. Docs: `03-architecture/boot-and-init.md` (the crond service), `06-reference/ports-catalogue.md`, and every count of Gear ports in 01, 04 and 08.

**Late (wave W7, not v1-blocking).**
- kaccounts-integration, kaccounts-providers and signon-kwallet-extension: no longer late. They and the accounts-sso stack build (porttest), with two new ports, signon-ui and libproxy; libaccounts-glib builds from upstream commit 69cebb86.
- colord-kde: needs colord and libgusb.
- audiotube: needs the GStreamer backend in qt6-qtmultimedia and python3-ytmusicapi, which needs frequent bumps.
- kajongg: the Twisted chain (six python ports).
- kigo: gnugo at run time.

**Optional enrichments deferred.** python3-translate-toolkit (lokalize ODF), recastnavigation (kosmindoormap routing), maplibre-native-qt (itinerary maps), libetebase (EteSync, Rust), subversion and mercurial (dolphin-plugins VCS) and libmms (dead since 2014; build kget with the plugin off) [prior]. Every recipe pins such a dependency off with `CMAKE_DISABLE_FIND_PACKAGE_*`, so a later port does not change the build without notice.

**Build flags common to every Gear recipe.**
- `-DCMAKE_DISABLE_FIND_PACKAGE_KF6UserFeedback=ON`.
- `OPTION_ADD_AUTOGENERATETEXT=OFF` in libksieve, kmail and pim-sieve-editor. This turns off ktextaddons' LLM client.
- `-DDATABASE_BACKEND=SQLITE`, `-DINSTALL_APPARMOR=OFF` and `-DBUILD_PYTHON_BINDINGS=OFF` in akonadi (B6).
- `rm -rf $PKG/usr/lib/systemd` wherever a module installs user units: akonadi, kunifiedpush, and any other the lint finds.
- kmix ships its autostart entry with `X-KDE-autostart-condition` off, so plasma-pa stays the only tray mixer [prior question 14].

#### 4.1.4 Third-party and non-Gear ports that are missing

Versions are measured in prior research or in section 1.5 unless marked. Anything marked "measure" gets its version from `ports/update` at recipe time.

| Port | Shelf | Phase | Version | Needed by | Key flags and notes |
|---|---|---|---|---|---|
| qcoro | qt-extra | 43 | 0.13.0 | plasma-workspace, powerdevil, plasma-nm and Discover (REQUIRED); kio-extras sftp:/; 9 Gear modules | Core, DBus, Network and Quick components. C++20 coroutines (GCC 16.2) |
| cxx-rust-cssparser | kde | 41 (no toolkit in its closure; phaseclosure confirms) | 1.1.0 | union | `vendoring = rust`, 62 crates. Built through corrosion 0.6.1 (present, 40_lang). Needs a `kfs.md` (vendor bundle) |
| kquickimageeditor | qt-extra | 43 | 0.7.0.1 | Spectacle (REQUIRED), koko, neochat, skanpage | highway and qt6-qtshadertools are present |
| libqaccessibilityclient | qt-extra | 43 | 0.6.0 | kwin (zoom focus tracking, `KWIN_BUILD_QACCESSIBILITYCLIENT`), accessibility-inspector | Built for Qt6 (QAccessibilityClient6) |
| kdsoap | qt-extra | 43 | latest; measure | kdsoap-ws-discovery-client | KDAB. Qt6 build (`-DKDSoap_QT6=ON`; the option name is to be measured) |
| kdsoap-ws-discovery-client | qt-extra | 43 | latest; measure | kio-extras smb:/ (`smb/CMakeLists.txt:6`, REQUIRED) [measured: inv-features verdict] | WS-Discovery on UDP 3702. Its unicast replies probably need a scoped nftables rule, like the NetBIOS 137 one [inferred]. That rule is section 6's |
| kio-fuse | kde | 43 | 5.1.1 | xdg-desktop-portal-kde and plasma-workspace at run time | Built over fuse 3.18.3 (present) |
| kirigami-app-components | qt-extra | 43 | 1.1.1 | keepsecret, koko | — |
| futuresql | qt-extra | 43 | 0.1.1 | angelfish, audiotube, kontrast | Needs qcoro |
| ktextaddons | qt-extra | 43 | 2.2.0 | the whole PIM stack | TextAutoGenerateText off |
| kdiagram | qt-extra | 43 | 3.0.1 | eventviews, incidenceeditor, massif-visualizer, calligra | Take it from the `3.0.1/` directory. The bare 2.7.90 tarball beside it is an old beta |
| kpeoplevcard | pim | 43 | 0.1 | plasma-phonebook | — |
| libquotient | chat | 43 | 0.9.6 | neochat (REQUIRED), itinerary | olm is present and deprecated upstream |
| libphonenumber | i18n | 41 | 9.0.42 | kitinerary | C++ only, with the Java and geocoder tooling off |
| qt6-qtvirtualkeyboard | qt6 | 43 | 6.12.0 | plasma-keyboard, the greeter, plasma-setup | `group = qt6` |
| qt6-qtwebview | qt6 | 43 | **6.11.2** (pinned to qtwebengine) | neochat (REQUIRED), tokodon, Discover (optional) | `group = qt6`. Pinned below the 6.12.0 that exists, because QtWebView's Linux backend is QtWebEngine |
| qt6-qthttpserver | qt6 | 43 | 6.12.0 | arianna | `group = qt6` |
| elogind | auth | 41 | 255.24 + Alpine patches | sddm, polkit, accountsservice, powerdevil, kscreenlocker and others | Section 3.2 |
| turnstile | auth | 41 | 0.1.11 | per-user services (A2) | Section 3.3 |
| accountsservice | auth | 41 | latest; measure | the Users KCM, the Account portal, SDDM avatars | `-Delogind=true -Dadmin_group=wheel -Dsystemdsystemunitdir=no` (Alpine) |
| sddm | auth | 50 | 0.21.0 + Alpine r10 patches (1.5) | the login | `-DBUILD_WITH_QT6=ON -DNO_SYSTEMD=ON -DUSE_ELOGIND=ON -DENABLE_JOURNALD=OFF`; utmps (present, 30_foundation) |
| kpmcore | disk | 43 | 26.08.2 (Gear) | partitionmanager, the KDOS installer (D12) | KAuth helper (A3) |
| xdg-user-dirs | xdg | 41 | latest; measure (0.19 per prior) | plasma-desktop, Places | — |
| libsass | devlibs | 40/41 (phaseclosure) | latest; measure | sassc | — |
| sassc | buildtools | 40/41 (phaseclosure) | latest; measure | breeze-gtk, kde-gtk-config | — |
| xsettingsd | x11 | 42 (libX11 in closure) | latest; measure | kde-gtk-config at run time | Serves XSETTINGS to Xwayland GTK clients |
| iio-sensor-proxy | hardware | 41 | latest; measure | auto-rotation and ambient light (KWin, powerdevil) | `-Dsystemdsystemunitdir=` empty; supervised by an rcS script |
| tlp-pd | hardware | 42 (python3-gobject is 42_graphics) | 1.10.2, `group = tlp` | powerdevil power profiles (A6) | A second build of TLP's source (4.3 trap 2). `make install-pd TLP_SYSD=$STAGE` and the staged unit deleted, because `install-pd` installs `tlp-pd.service` unconditionally and lint refuses `usr/lib/systemd`. The D-Bus service has `Exec=/bin/false`, so tlp-pd is ksvc-supervised [measured: res-session verdict] |
| aha | cli | 41 | latest; measure | kinfocenter | — |
| clinfo | gpu | phaseclosure decides | latest; measure | kinfocenter | — |
| krohnkite | kde | 50 | 0.9.10.0 (2026-10-25), the maintained anametologin fork [source: codeberg.org/anametologin/krohnkite; MIT]. The original esjeon/krohnkite was last pushed 2023-08 | the Tiling desk and `kdos tiling on\|off` (D6; 05 §5.5.3, 07 §7.5.4) | Data only: a KWin/Script package installed to `/usr/share/kwin/scripts/krohnkite/`, shipped disabled (`kwinrc [Plugins] krohnkiteEnabled=false`). The source is the release's packaged `.kwinscript`, because a build from TypeScript needs npm, which the tree does not vendor [inferred; measure that the release asset exists for 0.9.10.0]. kpkg wants the file as `krohnkite-0.9.10.0.<ext>` (port tarball naming), so the recipe renames it. Phase 50 because it is meaningful only under KWin and the `kde-plasma` meta names it (4.3 rule 4) |
| KDOS monospace font | fonts | 41 | the D7 pick | the UI monospace | `ttf-jetbrains-mono` (B17). It is a `ttf-*` name, so the `fonts` shelf (shelf rule 2). plasma-integration's `FontHack` find stays off |
| cdparanoia | optical | 41 | 10.2 | audiocd-kio (REQUIRED) | Carries Alpine's musl and build patches, each with a `Subject:` line |
| libodfgen, libwps | office | 41 | 0.1.8, 0.4.14 | calligra import filters | Over librevenge (present) |
| libetonyek, liblangtag | office, i18n | 41 | 0.1.13, 0.6.8 | calligra's Keynote filter | — |
| colord, libgusb | graphics-libs, hardware | 41 | 1.4.8, 0.4.9 | colord-kde (W7) | `-Dsystemd=false`. colord's `allow_active` actions under A3 |
| libaccounts-glib, libaccounts-qt, signond, signon-plugin-oauth2 | auth | 41 / 43 | 1.27, 1.17, 8.61, 0.25 | the kaccounts stack (W7) | qmake against Qt6, unmeasured on musl |
| gnugo | games-board | 41 | 3.8 | kigo (W7) | — |
| python3-twisted chain (6 ports) | python-net, python-libs, python | 41 | twisted 26.4.0 | kajongg (W7) | — |
| python3-ytmusicapi | python-net | 41 | 1.12.3 | audiotube (W7) | — |

Present and sufficient, so **not** ported: polkit-qt6 0.201.1, pulseaudio-qt 1.9.0, qtkeychain 0.17.0, qca 2.3.12, kirigami-addons 1.14.2 (Discover needs ≥ 1.10, `CMakeLists.txt:44`), libqalculate 5.12.0, ddcutil 3.0.2, libei 1.6.0, libevdev 1.13.7, hwdata 0.411, lcms2 2.19.1, libdisplay-info 0.4.0, libinput 1.32.0, libcanberra 0.30, smartmontools, bolt, gocryptfs, freerdp, fwupd 2.1.7, gpgme and qgpgme 2.2.0, socat, python3-psutil, gdb and lldb [measured: each `find ports/core -name <p>`]. greetd, consolekit2, power-profiles-daemon, openrc-settingsd, flatpak, packagekit, rtkit and Calamares stay unported (sections 1.3 and 3).

#### 4.1.5 KDOS's own ports

This is the canonical list of KDOS's own ports; every other section uses these names. The section in the last-but-one column defines each port's content, and the milestone is section 8.3's. Phase follows 4.3: `src/art` data and `src/system` code stay where their closure puts them, and everything that links Qt, KF6 or a Plasma module, or is meaningful only in the session, is in 50_desktop.

| Port | Area | Phase | Links | Content | Designed in | M |
|---|---|---|---|---|---|---|
| kdos-theme (REWRITE) | src/art | 41 (`src-art.txt`, unchanged) | C, libkcolor + libkbase; build-time tool | Generators: `kde-colors`, `konsole`, `qt5ct-colors`, `wallpaper`, `greeter-globals`, `contrast` | 05 §5.2.0, §5.3 | M4 |
| kdos-icons (REWRITE) | src/art | 41 (unchanged) | data | KDOS marks (`start-here-kdos`, `distributor-logo-kdos`, …) and the `KDOS`/`KDOS-Light` index themes over Breeze | 05 §5.2.5 | M4 |
| kdos-wallpapers | src/art | 41 [inferred: closure is kdos-theme] | data | `/usr/share/wallpapers/KDOS-<Accent>/` ×8 | 05 §5.2.7 | M4 |
| kdos-sounds | src/art | 42 [inferred: renders with `kdos-sfx` from kdos-tools, which is listed in 42 (`42_graphics/packages.d/src-system.txt`)] | data | `/usr/share/sounds/kdos/`, `Inherits=ocean,freedesktop` | 05 §5.2.14, 07 §7.2.5 | M4 |
| kdos-sddm-theme | src/art | 50; `depends = plasma-desktop` | data + `kdos-sddm.patch` | `/usr/share/sddm/themes/kdos/`, a copy of plasma-desktop's installed `breeze` SDDM theme with the patch applied | 05 §5.2.10 | M4 |
| kdos-plasma-look | src/desktop | 50 | data | The 8 colour schemes, the 8 global themes `org.kdos.<accent>.desktop`, the `kdos` desktoptheme, the three desk templates (`layout.js`: KDOS, Dock, Tiling), KSplash, the Konsole profile and schemes, the qt5ct palettes, the `/usr/share/kdos/xdg` defaults | 05 §5.2.1-5.2.3, §5.2.8, §5.2.12, §5.2.16, §5.2.19, §5.5 | M4 |
| kdos-welcome | src/desktop | 50 | QML only | plasma-welcome's `intro-customization.desktop` and the KDOS extra pages, including the desk picker and the Phosphor toggle | 05 §5.2.18, 07 §7.7.1 | M4 (identity page), M5 (P0 pages), M7 (P1 pages) |
| kded-kdos | src/desktop | 50 | KF6 kded, KConfig | Accent sync (a KConfigWatcher on kdeglobals) and the `.ktar` stick import | 05 §5.3, 06 §6.8 | M4 (accent), M5 (`.ktar`) |
| kdos-kwin-motion | src/desktop | 50 | KWin JS scripted effects, no ABI tie | `kdos_{unfold,login,quit}` | 05 §5.4.3, 07 §7.2.2 | M7 |
| kwin-effect-kdos-keys | src/desktop | 50 | KWin declarative script effect | The Meta+F1 key card | 07 §7.5.2 | M7 |
| kwin-script-kdos-runraise | src/desktop | 50 | KWin JS | Run-or-raise, one key per terminal program | 07 §7.5.5 | M9 |
| kwin-script-kdos-window-memory | src/desktop | 50 | KWin JS | Window position memory until Plasma 6.9's per-arrangement memory | 06 §6.1 (Windows) | M7 |
| kcm-kdos-system | src/desktop | 50 | KCMUtils, Kirigami | System Versions: slots, trial, rollback, update channel (`io.kdos.Slots1`) | 06 §6.4, 07 §7.3.6 | M5 |
| kcm-kdos-boxes | src/desktop | 50 | KCMUtils, Kirigami | Per-box grants and lifecycle | 06 §6.4 | M5 |
| kcm-kdos-firewall | src/desktop | 50 | KCMUtils, Kirigami | Named services, through kdos-powerd's `firewall` verbs | 06 §6.4 | M7 |
| kcm-kdos-look | src/desktop | 50 | KCMUtils, Kirigami | The Phosphor toggle and preset, accent-sync status and the one-action accent picker (07 §7.2.3); accents themselves stay on the Colors and Global Theme pages | 06 §6.4, 07 §7.2.3 | M5 (accent), M7 (Phosphor) |
| kcm-kdos-health | src/desktop | 50 | KCMUtils (kinfocenter CommandOutputContext) | `kdos doctor` output | 06 §6.4, 07 §7.6.2 | M7 |
| kdos-discover | src/desktop | 50; `depends = discover` | libdiscover internal API, version-locked: rebuilt and re-tested every Plasma minor | KdosBackend (ports, packs, boxes) and the `BackendNotifierModule` notifier plugin (07's update tray) | 06 §6.2.2, §6.2.5, 07 §7.6.3 | M5 |
| ksystemstats-kdos | src/desktop | 50 | libksysguard, version-locked | Sensors `kdos/box/<name>/*`, `kdos/energy/*`, `kdos/slot/*` | 06 §6.8, 07 §7.6.1 | M9 |
| kio-library | src/desktop | 50 | KF6 KIO worker API | `library:/` | 07 §7.4.2 | M9 |
| plasmoid-kdos-privacy | src/desktop | 50 | QML plasmoid over `kdos privacy --json` (kdos-tools) | Microphone, camera and screen lamps that name the box | 07 §7.3.4 | M7 |
| kdos-installer-gui | src/desktop | 50 | Kirigami | The graphical installer; execs `kinstall --events json` through pkexec | 06 §6.6 | M5 |
| kdos-runner | src/desktop | 50 | C on basu, no Qt; `org.kde.krunner1` | The KDOS KRunner verbs | 06 §6.5.1, 07 §7.5.1 | M5 |
| kdos-library-runner | src/desktop | 50 | C on basu, no Qt; `org.kde.krunner1` | The Library runner. It may merge into kdos-runner as a second `dbusplugins` entry; decide when kdos-runner exists | 07 §7.4.1 | M7 |
| kdos-notice | src/desktop | 50 | C on basu, no Qt; `org.freedesktop.Notifications` client | Restart, CVE, trial, OOM-kill and firewall notices; 07 §7.6.3's "notify agent" is this port | 06 §6.5.2, 07 §7.6.3 | M7 (the bless notice M5) |
| kdos-addons | src/art | 50 | data | The curated offline add-on pack in place of "Get New…": wallpapers, colour schemes, Konsole schemes, KWin scripts, no third-party QML plasmoid without a per-Plasma rig test. Each item's licence is recorded in its `kfs.md` | 07 §7.4.4, 06 §6.2.1 | M7 |
| kdos-sysupdated | src/daemons | 50 | C on basu, no Qt | `org.freedesktop.sysupdate1` over the A/B slots | 06 §6.2.2 | M5 |
| kdos-settingsd | src/daemons | 50 | C on basu, no Qt | `org.freedesktop.hostname1`, `timedate1`, `locale1` | 03 (A4) | M1 |

**Not KDOS ports.** kinstall-core is a directory, `src/system/kinstall-core`, not a port: the kdos-installer recipe compiles it without `libktui` on the include path, archives it as `libkinstall-core.a` and links it with the terminal wizard into the one binary `/usr/bin/kinstall`; kdos-installer-gui drives that binary and links nothing of it (06 §6.6). The window decoration is Breeze, configured (`breezerc`, square), not a KDOS plugin; a C++ KDecoration3 plugin is P2 and has no port until then (05 §5.2.4). The CRT effect `kdos_crt` is `ports/core/<kde shelf>/kwin/kdos-crt.patch`, a carried patch against kwin (05 §5.4.2, 07 §7.2.1). The auto-tiler is the third-party krohnkite port (4.1.4). kdos-boxsock is an existing port re-homed to `src/system` (06 §6.3.3), not a new one.

**Count.** 28 rows: 2 rewritten (kdos-theme, kdos-icons) and 26 new.

### 4.2 Existing ports to bump or rebuild with different flags

The flags are measured from each `build.sh` unless marked. Each row's rebuild is gated by `make build BUILD_ARGS="--phases <phase>,70_image --rebuild <port>"`. Each row rewrites that port's `kfs.md` Flags section in the same change (rule 1), and `kfscheck` fails while a note explains a flag that is gone.

| Port (shelf, phase) | Today | Change | Why |
|---|---|---|---|
| breeze (themes, 43) | 6.7.5; `BUILD_CURSOR=OFF`, `WITH_DECORATIONS=OFF`, `WITH_WALLPAPERS=OFF` (`build.sh:22-24`) | 6.8.2; all three `ON`; add `kdecoration` to depends | The Breeze decoration, configured square in `/usr/share/kdos/xdg/breezerc`, is the KDOS window decoration (05 §5.2.4). `breeze_cursors` is the Breeze global theme's `cursorTheme` and kde-gtk-config's default (B9). The `Next` wallpaper is `defaultwallpaper.cpp:43-46`'s last fallback, so without it a misnamed KDOS wallpaper gives a blank desktop [measured: res-branding verdict]. Measure what cursor rendering needs at build time |
| kwayland (kde, 43) | 6.7.5 | 6.8.2 | Consumers ask for 6.8.0 |
| plasma-integration (kde, 43) | 6.7.5; `DEFAULT_UNION_STYLE=OFF`, `FontNotoSans`/`FontNotoColorEmoji`/`FontHack` and `XDGDesktopPortalKDE` disabled (`build.sh:21-25`) | 6.8.2; drop `DEFAULT_UNION_STYLE` (removed in 6.8.2 [prior]); re-enable `FontNotoSans` and `FontNotoColorEmoji` (noto-fonts and noto-emoji are present; D7), and `FontHack` only if D7 picks Hack. Keep `XDGDesktopPortalKDE` off: it is a runtime-only package check, and the portal is a 50_desktop port | — |
| libksysguard (kde, 43) | 6.7.5; `ENABLE_KAUTH_HELPER=OFF`, libpcap off | 6.8.2; `ENABLE_KAUTH_HELPER=ON`; libpcap stays off | The process-kill and renice helper goes through polkit under A3. CAP_NET_RAW would be lost in the tar anyway [prior] |
| plasma-wayland-protocols (kde, 43) | 1.22.0 | 1.23.0 | KWin `CMakeLists.txt:230` [measured] |
| kwallet (kf6, 43) | `BUILD_KSECRETD=OFF`, `BUILD_KWALLET_QUERY=OFF`, Gpgmepp disabled; `build.sh:27-31` writes `[KSecretD] Enabled=false` | `BUILD_KSECRETD=ON`, `BUILD_KWALLET_QUERY=ON`, Gpgmepp on (qgpgme is 43); delete the `kwalletrc` write | A7. kwallet-pam execs `bin/ksecretd` [measured: `build/fs/usr/lib/cmake/KF6Wallet/KF6WalletConfig.cmake:38`] |
| baloo (kf6, 43) | `BUILD_INDEXER_SERVICE=OFF` (`build.sh:19`) | `ON`; delete its systemd unit; ship `/usr/share/kdos/xdg/baloofilerc` excluding `~/.local/share/containers`, `/var/lib/kdos/packs`, `~/.cache` and build trees, with indexing off on the live medium | Plasma file search |
| purpose (kf6, 43) | default plugins | No flag change. It is rebuilt in the KF 6.31 bump; the bluetooth share plugin works once bluedevil ships | — |
| kio-extras (kde, 43) | `BUILD_KCMS=OFF`, `BUILD_ACTIVITIES=OFF`, Samba and libssh disabled (`build.sh:20-26`) | `BUILD_KCMS=ON`, `BUILD_ACTIVITIES=ON`; drop both disables; add qcoro, kdsoap-ws-discovery-client, samba, libssh, plasma-activities and plasma-activities-stats to depends | sftp:/ runs `find_package(QCoro6 REQUIRED)` when libssh is found (`CMakeLists.txt:207-210`). smb:/ REQUIRES KDSoapWSDiscoveryClient [measured: inv-features verdict]. Samba is built `--without-ads`, so Kerberos for SMB stays unverified |
| appstream (xdg, 42 → **43**) | `-Dqt=false -Dcompose=false` (`build.sh:19-20`) | `-Dqt=true -Dcompose=true`; move to `43_toolkits/packages.d/xdg.txt` | Discover REQUIRES AppStreamQt ≥ 1.0.4 (`CMakeLists.txt:40`). With Qt in its closure, the phase table puts it in 43. Its only reverse dependency is gimp (44_apps), so the move breaks no phase [measured: `grep -w appstream` over every depends]. compose builds the KDOS catalogues at image time (section 6) |
| discover (new, 50) | — | `-DBUILD_SystemdSysupdateBackend=ON -DBUILD_FwupdBackend=ON -DBUILD_FlatpakBackend=OFF -DBUILD_SnapBackend=OFF -DBUILD_PackageKitBackend=OFF -DBUILD_RpmOstreeBackend=OFF -DBUILD_HoloBackend=OFF -DBUILD_DummyBackend=OFF -DBUILD_AlpineApkBackend=OFF -DBUILD_AppStreamPreviewBackend=OFF -DCMAKE_DISABLE_FIND_PACKAGE_KF6NewStuff=ON -DCMAKE_DISABLE_FIND_PACKAGE_KF6UserFeedback=ON -DWITH_KCM=ON -DWITH_NOTIFIER=ON`, the list in 06 §6.2.1 | D10. The KNS backend has **no option**: it builds whenever KF6Attica, KF6NewStuff and AppStreamQt are found (`libdiscover/backends/CMakeLists.txt:38-40`), and all three are in the tree once appstream has `-Dqt=true`. Disabling the NewStuff find is the only flag that turns it off. `BUILD_AppStreamPreviewBackend` defaults to `${AppStreamQt_FOUND}` (`:33`), so it is ON unless passed OFF. `BUILD_AlpineApkBackend` defaults ON (`:101`), and only ApkQt's absence keeps it out, so it is passed OFF rather than left to a find. Gate (06 §6.2.6): the package's `usr/lib/qt6/plugins/discover/` holds only the systemdsysupdate and fwupd backends, plus kdos-discover's once it is installed [measured: discover 6.8.2 `libdiscover/backends/CMakeLists.txt:33-103`] |
| kwin (new, 50) | — | `-DKWIN_BUILD_X11=ON` (Xwayland only); `xwayland` added to depends; libqaccessibilityclient, libei (EIS) and libevdev (game controller) in depends | After the deletion nothing else declares xwayland (`CMakeLists.txt:281` finds Xwayland ≥ 23.1.0) [measured: inv-code verdict]. kpkg's tar loses `CAP_SYS_NICE`, so RT priority comes from pam_limits (3.4); record that in its `kfs.md` |
| plasma-workspace (new, 50) | — | `-DWITH_X11=ON -DWITH_X11_SESSION=OFF -DINSTALL_SDDM_WAYLAND_SESSION=ON -DGLIBC_LOCALE_GEN=OFF`; `rm -rf $PKG/usr/lib/systemd`; carry `locales.patch` (4.7) | Option defaults are measured at `CMakeLists.txt:13-15,292`. The X11 session is off (rule 4), and Xwayland and xembedsniproxy stay. `INSTALL_SDDM_WAYLAND_SESSION` defaults OFF with "BETA, do not deploy" (`:14`, `:391`), and D9 accepts that behind the rig gate |
| gwenview (graphics, 44) | `CMAKE_DISABLE_FIND_PACKAGE_KF6Purpose=ON`, `…PlasmaActivities=ON` (`build.sh:27-28`) | Drop both | Share menu and activities |
| kalgebra (math, 44) | `CMAKE_DISABLE_FIND_PACKAGE_Plasma=ON` | Drop it | Plasmoid |
| marble (gis, 44) | `-DWITH_Plasma=OFF` [prior] | `ON` | World-clock wallpaper and runners |
| kleopatra (security, 44) | 3 KPim6 disables (`build.sh:23-25`) | Drop them after PIM tier 5 (W6) | Send keys by mail |
| kdeconnect (mobile, 44) | `WITH_X11=OFF` | Keep `WITH_X11=OFF`. Restore `NotShowIn=KDE` on the non-Plasma indicator entry; add krdp to depends | Avoids a second tray icon under Plasma |
| kdenlive (video-tools, 44) | `no-qmllint-in-build.patch`, with no `Subject:` | Delete the patch; pass `-DBUILD_RUN_QMLLINT=OFF` (26.08.2) | Hard rule 9 |
| ark, falkon, dolphin, filelight (44) | `app-id.patch` (StartupWMClass to the desktop-file name) | Re-check that each applies at 26.08.2; keep them. Plasma's task manager also matches on the app_id | — |
| konsole (shells, 43) | 26.08.1 | 26.08.2; default terminal (B3) | — |
| All 55 Gear ports | 26.08.1 | 26.08.2, `release = 1`, a new sha256, `ports/publish`, index line | Lockstep: libkleo, kmbox and mimetreeparser before messagelib and merkuro; libkdegames and libkmahjongg before the games; kdevelop before kdev-php and kdev-python [prior] |
| qt6-qtmultimedia (qt6, 43) | FFmpeg backend only | `-DFEATURE_gstreamer=ON` beside FFmpeg; gstreamer and gst-plugins-base in depends (W7) | audiotube's `FATAL_ERROR` without `QGstreamerMediaPlugin`. FFmpeg stays the default backend |
| polkit (auth, 41) | `-Dsession_tracking=ConsoleKit`, `-Dgettext=false` | `-Dsession_tracking=elogind` (polkit-128 `meson_options.txt:1`); `-Dgettext=false` stays (A1) | Section 3.5 |
| networkmanager (network, 41) | `-Dsession_tracking=no -Dsession_tracking_consolekit=false -Dsuspend_resume=consolekit` (`build.sh:22-26`) | `-Dsession_tracking=elogind -Dsuspend_resume=elogind` | Section 3.7. Remove kdos-powerd's NM `Sleep()` calls in the same change |
| pipewire, wireplumber, fwupd, libpulse, gvfs, gnome-disk-utility | `-Dlogind=disabled` / `-Delogind=disabled` / `-Delogind=disabled` / `-Dlogind=false` / `-Dlogind=none` | `-Dlogind=enabled` / `-Delogind=enabled` / `-Dlogind=enabled` / `-Delogind=enabled` / `-Dlogind=true` / `-Dlogind=libelogind` | Session awareness (3.2) |
| shadow (auth, 41) | `--disable-logind`, `--without-libpam` (`build.sh:20,28`) | **Unchanged**; shadow's `/usr/bin/login` is deleted from its package | 03 §3.2: util-linux owns `login`. `pam.d/other` is deny-all (`pam/build.sh:65-76`), so every PAM-enabled shadow tool would need its own stack; shadow's tools stay PAM-free [measured: `ports/core/auth/shadow/build.sh:20,28`] |
| util-linux (base, 41) | `--disable-login` (`build.sh:16`) | `--enable-login` (PAM); installs `/etc/pam.d/login` with `include system-login` | 03 §3.2: a tty2 login registers with elogind and gets `allow_active` [measured: `ports/core/base/util-linux/build.sh:16`] |
| pam (auth, 41) | `-D logind=disabled -D elogind=disabled` (`build.sh:19-20`) | Flags **unchanged**. Adds `/etc/pam.d/system-login`: auth, account and password `include system-*`, `session include system-session`, `session optional pam_elogind.so`, `-session optional pam_turnstile.so` | 03 §3.2. elogind ships pam_elogind itself and depends on pam, so `pam -Delogind=enabled` would be a dependency cycle [inferred: both recipes] |
| tlp (hardware, 41) | `TLP_WITH_ELOGIND=0` (`build.sh:12,16`) | `TLP_WITH_ELOGIND=1`, which installs the elogind system-sleep hook; tlp-pd is split out (4.1.4) | A6; radios are restored around suspend [measured: res-session missed] |
| fcitx5 (input, 50) | Wayland-only, no Qt or GTK module, no config tool | Add `fcitx5-qt` (Qt6 IM module), `fcitx5-gtk` (GTK 3 and GTK 4 IM modules, no GTK 2) and `fcitx5-configtool` (kcm_fcitx5) on `input`, phase 50 | KWin's virtual-keyboard setting starts fcitx5 over input-method-v1. A10 moves fcitx5's Super+Space |
| seatd, wlroots, xdg-desktop-portal-wlr, wvkbd, fuzzel, grim, slurp, wf-recorder, foot, fcft | present | Deleted (section 2) | wlroots is seatd's only consumer, and wlroots' consumers are all cell programs [measured: inv-code verdict] |
| qt6ct | 0.11 | Deleted; qt5ct stays (A11) | — |
| The 109 recipes with `-DKF_SKIP_PO_PROCESSING=ON` | translations skipped | Kept; every new KDE port passes it too (4.5) | A1 |

### 4.3 Phase placement

`testing/phaseclosure.py:195-249` checks only 41 to 44 against the phase table. Each port's closure (its own name and every `depends` below it) decides:
- 41_system: the closure reaches neither the graphics roots (`wayland`, `libX11`, `mesa`, `libdrm`, `cairo`, `pango`, `gstreamer`, `ffmpeg`, `pipewire`) nor a toolkit root (`qt6-qtbase` and the others).
- 42_graphics: graphics but no toolkit.
- 43_toolkits: a toolkit.
- 44_apps: a toolkit, or an application shelf and graphics, and **no port depends on it**.

50_desktop is not checked against the table. Its list must still equal its closure minus earlier phases, and a dependency must not resolve only on a later phase's `PORT_REPO` [measured: `phaseclosure.py:29-49`].

**Placement rules for the KDE set.**
1. **Every library a 43 or 44 port links goes to 43.** This covers all of KF6 and the 13 Plasma libraries: kdecoration, layer-shell-qt, kpipewire, krdp, libkscreen, libplasma, plasma-activities, plasma-activities-stats, plasma5support, union, qqc2-breeze-style, breeze and oxygen. It also covers the four bumped Plasma ports, the Gear libraries and the qt-extra third-party ports.
2. **The session goes to 50.** That is every other Plasma module, SDDM, fcitx5-qt, fcitx5-gtk and fcitx5-configtool, krohnkite, the KDOS ports whose 4.1.5 phase is 50 and the meta ports (4.4).
3. **A Gear port never names a 50_desktop port in `depends`.** If it did, the port would be pulled into 50 (or refused as a later-phase dependency). Examples are kclock's powerdevil wake-up and kweather's plasmoid host. Session daemons that a Gear app only talks to at run time come from the `kde-plasma` meta (4.4). Gate: `python3 testing/phaseclosure.py` passes with no 44 port's closure reaching a 50 port.
4. **Data-only ports go to 41.** ocean-sound-theme, oxygen-sounds, plasma-workspace-wallpapers and the monospace font have only cmake and ECM, or nothing, in their closure, so the table places them in 41. KDOS's `src/art` data follows its closure too: kdos-theme, kdos-icons and kdos-wallpapers in 41, and kdos-sounds in 42 because it renders with kdos-tools. Data meaningful only in the session goes to 50, which is not table-checked, and may `depends` on a 50 port: kdos-sddm-theme names plasma-desktop, whose installed `breeze` SDDM theme it copies at build time (05 §5.2.10); kdos-plasma-look, kdos-addons and krohnkite are 50 as well. The rule that binds is rule 3: no 41-44 port names any of them.
5. **A leaf Gear app goes to 44; a Gear library goes to 43.** When a Gear app becomes a dependency (kaddressbook is a build dependency of kdepim-addons; kgraphviewer of massif-visualizer), the table moves it to 43. Let phaseclosure name it.
6. **A recipe written ahead of its wave goes in `ports/unbuilt`** until its phase list names it. Otherwise `testing/bookcounts.py:36-39` fails on a recipe no phase builds [measured: inv-docs verdict]. Remove the line in the change that lists the port.

**Traps.**
1. **appstream moves from 42 to 43** when it gains `-Dqt=true` (4.2). Its only consumer is gimp in 44, so the move breaks nothing. If any 42 port gains appstream later, a split `appstream-qt` port (`group = appstream`) is the fallback.
2. **tlp-pd cannot live in tlp's recipe.** tlp is in 41. tlp-pd needs python3-gobject, which is in 42_graphics [measured], so the whole port would move to 42. As a separate port with `group = tlp`, tlp stays in 41 and tlp-pd is placed in 42 (writing-ports.md "A second build of the same source").
3. **xwayland loses its only declarer** when wlroots goes. kwin must name it, or the 42 list has a port nothing pulls in. That is legal, but it would be deleted as an orphan at the next cleanup.
4. **udisks2 is in 43** [measured]. plasma-disks and kpmcore's runtime use it, and it needs no change.
5. **Deletions in section 2 and additions here must land in one order run.** Deleting fcft before kdos-shell, or slurp before kdos-record, leaves a listed port pulling an unlisted one [measured: inv-code risk]. Each deletion change runs phaseclosure.
6. **Shelf grouping for `ports/update`.** The grouping key is "forge organisation plus current version" (writing-ports.md:2580-2586). KF6 is grouped without a `group` key, so download.kde.org's directories are probably read. That is not measured for `stable/plasma/<v>/`. Task: run `ports/update --fixture` (or a dry run) on two Plasma recipes on different shelves. If they do not group, the Plasma set needs `group = plasma`, and shelf rule 1 then forces every Plasma port onto one shelf (`kde`). In that case spectacle, plasma-systemmonitor and the other function-shelved rows above move to `kde`.

**What 50_desktop's files become.** Every line here replaces text; none is kept with an annotation (rules 1 and 2).

`script/phases/50_desktop/phase.env`:
```bash
export KDOS_PHASE_TITLE="Desktop"
export KDOS_PHASE_DESC="KDE Plasma, SDDM, KDOS integration"
# src/desktop and src/daemons join the port repos here and nowhere earlier:
# KDOS's KCMs, plasmoids, KWin plugins, Discover backends and installer link
# libplasma, KWin and libdiscover, which this phase installs, and the daemons
# serve that session.
export PORT_REPO="/ports/core /kdos/src/system /kdos/src/art /kdos/src/desktop /kdos/src/daemons"
```
`PORT_REPO` keeps all five entries, so `testing/preflight.sh:55`, `70_image/040_orphans.sh:34`, `bookcounts.py:56`, `anchorcheck.py:123,143` and `kfsgen.py:832` need no edit (A9) [measured: inv-code load-bearing claim].

The `packages.txt` header becomes:
> Everything the desktop session is, and nothing an application needs at build time: KWin, the Plasma shell and its daemons, SDDM, the portals, the input method, KDOS's own Plasma code and daemons, and the meta ports. The Plasma libraries that Gear applications link (libplasma, kpipewire, plasma-activities, union and the rest) are 43_toolkits. A port listed here is never named in the `depends` of a 41-44 port. See docs/kdos/03-architecture/session.md.

Its shelf sections are `auth` (sddm, kwallet-pam), `kde` (the session modules), `graphics`, `sysmon`, `security`, `disk`, `filesystem`, `hardware`, `printing` and `transfer` (the function-shelved Plasma tools), `input` (fcitx5 and its engines, fcitx5-qt, fcitx5-gtk, fcitx5-configtool, plasma-keyboard), `xdg` (xdg-desktop-portal-kde), `kde` also lists krohnkite. `src-art` (kdos-sddm-theme, kdos-addons), `src-desktop` (the 4.1.5 rows with area `src/desktop`), `src-daemons` (minus the deleted kdos-mountd, B2; plus kdos-sysupdated and kdos-settingsd) and `meta`. kdos-wallpapers joins `41_system/packages.d/src-art.txt`, kdos-sounds a `42_graphics/packages.d/src-art.txt` [inferred placements; phaseclosure decides]; kinstall-core is built inside kdos-installer and has no list line. The `wl` and `xdg-desktop-portal-wlr` sections are removed. Every port gets a one-to-three-line comment saying what it is in the session, matching the current density. Gate: `python3 testing/phaseclosure.py` reports 50_desktop with zero refusals.

**Docs (rule 1):**
- `05-developer/writing-ports.md` "Which phase lists a port": add the rule that a 41-44 port never depends on a 50 port, and the data-only case.
- `05-developer/build-system.md`: the phase list descriptions.
- `docs/kfs` phase intro pages for 43 and 50.
- `06-reference/ports-catalogue.md`: every row, through `bookcounts`.

### 4.4 Meta ports and the KP_MAX_DEPS limit

`KP_MAX_DEPS` is 128. `kp_depends` stops reading there (`src/libs/libkpkg/kp_conf.c:883`, the loop condition `n < max`), so a recipe that names more silently loses the rest of its build closure [measured: `kpkg.h:52-55`]. `testing/phaseclosure.py` reads every name with no limit. The two would disagree: phaseclosure passes while kpkg builds an incomplete closure. Today's largest depends lines are vlc 77, chromium 67 and libreoffice 65 [measured]. One "all of Gear" meta would name about 200.

**Decision: split the metas and make overflow an error. Keep the limit at 128.**
- [ ] `kp_depends` returns an error when a recipe names more than `KP_MAX_DEPS` depends, and kpkg refuses the recipe with `<port>: depends names N ports; the limit is 128`. preflight adds the same check over every kpkgbuild. The `kpkg.h:52-54` comment is rewritten to state the refusal. Gate: a scratch recipe with 129 depends fails `kpkg build` and preflight; the tree passes.
- [ ] Metas are source-less ports, the shape `containers-common` already uses (no `source`, a trivial `build.sh`) [measured]. All metas sit on the `kde` shelf and in 50_desktop's `meta` section. 50 is not table-checked, so a meta that depends on other metas is legal there. It would be refused in 43 or 44, because a meta another meta depends on is not a leaf and would pull 44 ports into 43.

| Meta | Depends on (leaf names only; libraries arrive through closure) | Approx. count |
|---|---|---|
| `kde-plasma` | every PORT and BUMP Plasma module from 4.1.2 that is not a library, plus sddm, kwallet-pam, xdg-desktop-portal-kde, fcitx5 with its engines, fcitx5-qt, fcitx5-gtk and fcitx5-configtool, plasma-keyboard, krohnkite and every 4.1.5 port that is not a build-time tool or library (kdos-theme arrives through closure) | ~77 |
| `kde-gear-core` | the D8 defaults and every app the Welcome tour names: dolphin, konsole, kate, okular, gwenview, ark, spectacle (Plasma), kcalc, kcharselect, kwalletmanager, khelpcenter, filelight, elisa, kdeconnect, partitionmanager, isoimagewriter, kfind, ksystemlog, sweeper, yakuake, krdc, krfb, ktorrent, kdialog, markdownpart, svgpart, kio-admin, kio-zeroconf, kio-gdrive, dolphin-plugins, kdenetwork-filesharing, kde-inotify-survey, kdesdk-thumbnailers, ffmpegthumbs, kdegraphics-thumbnailers, kdegraphics-mobipocket | ~40 |
| `kde-gear-games` | 34 new games and the 17 present ones | ~51 |
| `kde-gear-education` | 12 new, kdeedu-data and the 9 present (kalzium, kgeography, ktouch, kturtle, kwordquiz, parley, kalgebra, cantor, marble) | ~22 |
| `kde-gear-pim` | kmail, kontact, korganizer, kaddressbook, akregator, merkuro, kalarm, zanshin, kdepim-runtime, kdepim-addons, akonadi-import-wizard, akonadi-calendar-tools, akonadiconsole, grantlee-editor, kmail-account-wizard, mbox-importer, pim-data-exporter, pim-sieve-editor, kgpg, itinerary, kongress | ~21 |
| `kde-gear-multimedia` | dragon, juk, kmix, kasts, audex, audiocd-kio, k3b, kamoso, kwave, kdenlive, plasmatube, telly-skout, krecorder, plasma-camera | ~14 |
| `kde-gear-graphics` | kolourpaint, skanlite, skanpage, koko, kruler, kcolorchooser, kontrast, kimagemapeditor, kgraphviewer, kamera, qrca, arianna | ~12 |
| `kde-gear-utilities` | the remaining apps: kalk, kalm, kbackup, kclock, kweather, kteatime, ktimer, keysmith, keepsecret, kget, konqueror, neochat, tokodon, alligator, angelfish, calindori, ktrip, plasma-phonebook, qmlkonsole, francis, ghostwriter, kmouth, keditbookmarks, kdf, konversation, falkon, kleopatra, calligra, lokalize | ~30 |
| `kde-gear-devtools` | kdevelop, kdev-php, kdev-python, kompare, umbrello, kcachegrind, massif-visualizer, kapptemplate, kdebugsettings, kde-dev-scripts, kde-dev-utils, kirigami-gallery, accessibility-inspector, poxml, kdesdk-kio, plasma-sdk | ~16 |
| `kde-gear` | the 8 `kde-gear-*` metas | 8 |
| `kde-all` | `kde-plasma`, `kde-gear`. Not `kde`: that is the shelf every meta sits on, and preflight's ports layout check refuses a port named like a shelf | 2 |

- [ ] Write the 11 metas. Gate: a preflight cross-check that every Gear leaf (the 55 bumped plus the 193 new, minus libraries) is named by exactly one `kde-gear-*` meta. Each meta's depends count must be ≤ 128.
- [ ] The metas are the image's KDE install set; the phase lists stay the build set. `kpkg install kde-gear-games` on an installed machine resolves offline from the medium's package store. Docs: `03-architecture/packaging.md` (a new "Meta ports" subsection), `writing-ports.md` (the `depends` row gains the 128 refusal; a meta recipe example), `06-reference/ports-catalogue.md`.

### 4.5 Translations: English only (A1)

**Today, and kept** [measured]:
- kpkg's prune removes every non-English `.mo`, `.gmo` and `.qm`, every translated `usr/share/doc/HTML/<lang>/` and every non-`man<section>` man directory (`src/system/kdos-kpkg/prune.c:205-245`; called at `build.c:728`; the policy is `writing-ports.md:1762`).
- 109 `build.sh` files pass `-DKF_SKIP_PO_PROCESSING=ON`, and 17 `kfs.md` files explain it. About 120 more recipes disable NLS (`--disable-nls`, `-Dnls=false`, `-Dgettext=false`).
- `70_image/020_cleanup.sh:203-273` fails the image on an unowned file under `usr/share/locale`, and `087_gates.sh:48` passes `--fail-under usr/share/locale`.

**Rule for the KDE set.** Every new KF6, Plasma and Gear recipe passes `-DKF_SKIP_PO_PROCESSING=ON`, matching the 109 that do today; prune and both image gates need no change. KDOS's own Qt code still wraps every string in `i18n()` (6.1), so a translated release later is a build-flag change rather than a code change.

**Region & Language.** The KCM offers formats only (dates, numbers, currency, measurement, paper size) through `locale1` (A4); its language list shows American and British English, the two catalogues prune keeps.

**Checks.**
- [ ] Every new KDE recipe passes the flag. Gate: `grep -L KF_SKIP_PO_PROCESSING` over the `build.sh` of every new ECM-built port returns nothing, and `020_cleanup.sh`'s unowned-locale check passes on the first full image.
- [ ] Rig gate: Region & Language sets `LC_TIME=de_DE.UTF-8`; after log out and in, the panel clock shows German date format with the English UI (shot).

**Docs (rule 1):**
- `06-reference/known-gaps.md`: the English-only gap names KDE's catalogues among what is skipped.
- `02-user-guide/configuration.md` (or the Plasma settings page that replaces it): formats, not languages, in Region & Language.

### 4.6 Build cost, image size, vendoring, publishing, notes, order

**New port count.**

| Group | New | Bumped or rebuilt |
|---|---|---|
| KF6 | 4 (krunner, ksvg, syndication, kdav) | 67 (6.30 → 6.31) |
| Plasma 6.8.2 | 56 | 4 |
| Gear 26.08.2 | 193 (185 in W5/W6, 8 in W7) | 55 |
| Third-party (4.1.4) | ~41 (krohnkite included), ~17 of them W7-only; cronie (B25) | ~20 flag rebuilds (4.2) |
| KDOS (4.1.5) | 27 | 2 (kdos-theme, kdos-icons) |
| Metas | 11 | — |
| **Total** | **~332** | **~148** |

After the cell-desktop deletion (section 2) the tree is about 2,083 − ~25 + ~331 ≈ 2,389 recipes [inferred]. `KP_MAX_ORDER` is 4096 [measured: `kpkg.h:56`].

**Build time [inferred; measure].** No per-port timing is recorded in `build/logs` (log mtimes are reused across rebuilds) [measured]. These estimates assume the 16-core, 64 GB build host [measured: `nproc`, `/proc/meminfo`] with ccache warm for unchanged code:

| Wave | Content | Estimate |
|---|---|---|
| W0 | KF6 6.31 (74) | 4-6 h |
| W1 | Plasma libraries + third-party (43) | 1.5-2.5 h |
| W2-W3 | Plasma session (50): kwin ~20-30 min, plasma-workspace ~15-25 min, kdeplasma-addons ~15 min plus Rust | 4-7 h |
| W4 | 55 Gear bumps (kdenlive, kdevelop, marble and okular dominate) | 4-6 h |
| W5 | 140 non-PIM Gear (calligra alone ~1-1.5 h) | 8-12 h |
| W6 | 44 PIM modules (messagelib, kdepim-addons and kmail are the big ones) | 4-6 h |
| **Sum** | | **~25-40 h** of build on top of today's ~20 h fresh build |

- [ ] Measure the real cost. Wrap each wave's `make build` in `time` and record per-port wall time with a one-line patch to kdosbuild's install loop that logs `start` and `end` epochs in `<N>_<port>.install.log`. Write the totals into `how-kdos-is-built.md` and `developing.md`.

**Image size [measured base; inferred deltas].**
- The last measured ISO is 23,853,701,120 bytes, with `system.sfs` at 22,693 MiB and an install of about 53 GB (`how-kdos-is-built.md:744-746`).
- KF6 installs 153 MiB today, English-only [measured over `build/fs/var/lib/kpkg/db/<kf6 port>` file lists]. qtwebengine is 227 MiB and qt6-qtbase 83 MiB [measured].
- Gear sources are 626.5 MB for all 252 modules [measured: `gear/sizes.txt`].
- Inferred additions:
  - Plasma installed: ~0.5-0.9 GiB.
  - 193 new Gear: ~1.0-1.6 GiB (calligra alone ~0.15 GiB).
  - plasma-workspace-wallpapers: 0.29 GB [measured: prior].
  - Packs on the medium for ~60 languages: several GiB, outside `system.sfs`.
- Net +2-3 GiB to `system.sfs`. The cell desktop it replaces is a few hundred MiB. The 32 GB USB requirement and the multi-part release stay, but the BD-R statement has to be re-checked against the new total.
- [ ] Measure the new sizes after W3 and after W6 (`ls -l build/iso-build/kdos.iso`, the `system.sfs` size). Rewrite `how-kdos-is-built.md:744-751`, `02-user-guide/getting-started.md#what-you-need`, `installation.md` and `kinstall.md`'s minimum disk in the same change (rule 1).

**Vendoring.** Four KDE ports add Rust vendor bundles: cxx-rust-cssparser (62 crates), kdeplasma-addons (kameleon, 101 crates), akonadi-search (`agent/rs/htmlparser`) and kdepim-addons (adblock).
- Bundles are generated in the `kdos-fetch` container at the tree's Rust 1.98.1 (`ports/Containerfile.fetch`). Each bundle carries a `sha256 =` line and is published.
- The vendoring counts stated in prose ("161 ports do (75 Rust, …)", `writing-ports.md:2041`, plus `principles.md`, `glossary.md`, `how-kdos-is-built.md` and `packaging.md`) are not recomputed by any check [measured: inv-docs verdict]. Every change that adds or deletes a vendored port updates all five.

**Publishing.**
- Every new or bumped source goes through `make fetch`, then `ports/publish <port>`, and the `ports/sources.idx` line is committed with the recipe. The pre-push hook refuses otherwise.
- No new shelf is added, so no volume is assigned. `kde` (volume 3), `kf6` (3), `qt-extra` (4) and `qt6` (4) absorb most of the set [measured: `ports/shelves`].
- Publish in waves, because one publish sends at most what GitHub's 2 GiB-per-asset limit and the volume layout allow. calligra's 56 MB tarball is the largest single Gear source [measured].
- The stale 26.08.1 and 6.30.0 index lines are listed by `make publish-orphans`. A hash any `v*` tag's recipe names (tag v0.1 exists) stays protected. Pruning is `ports/publish --orphans --prune=yes-delete` and needs the user's go-ahead [measured: inv-code verdict].

**kfs.md.** kfscheck requires a note for a port with a patch, a vendor bundle, a `build.sh` over 30 lines or 20 or more reverse dependencies [measured: CLAUDE.md rule 6; inv-docs item]. The required set:
- elogind, turnstile, sddm, accountsservice and tlp-pd;
- krunner, ksvg, libplasma, union, cxx-rust-cssparser and qcoro;
- kwin, plasma-workspace, plasma-desktop, kscreenlocker, powerdevil, kdeplasma-addons, discover, plasma-systemmonitor (its XDG override), kio-extras, appstream, breeze, kwallet and baloo;
- akonadi, akonadi-search, kdepim-addons, messagelib, ktextaddons, cdparanoia and kpmcore;
- krohnkite (a renamed release asset) and every 4.1.5 port; kdos-addons' note records each item's licence (07 §7.4.4).

That is roughly 40-60 new or rewritten notes [inferred: inv-docs item]. Each one is a one-line link per the book-sync traps, `make book-stamp` runs after each recipe batch, and `python3 testing/kfscheck.py` gates each wave.

**Build order: waves.** Each wave ends in a buildable, phaseclosure-clean tree. W2 needs section 3's elogind, turnstile, PAM and kdos-settingsd. W0 and W1 do not.

**W0: prerequisites (41-43)**
- [ ] kpkg: the `KP_MAX_DEPS` refusal (4.4) and the `KPKG_L10N` split (4.5). Gate: `testing/selftest.sh` and the ASan run pass.
- [ ] KF6 to 6.31.0 (67), plus krunner, ksvg, syndication and kdav, each new one passing `KF_SKIP_PO_PROCESSING` (4.5). Gate: every `ports/core/kf6/*/kpkgbuild` reads 6.31.0; `make build BUILD_ARGS="--phases 43_toolkits"` succeeds; phaseclosure passes.
- [ ] plasma-wayland-protocols 1.23.0. Gate: rebuild succeeds and its index line is committed.
- [ ] Third-party 43 set: qcoro, kquickimageeditor, libqaccessibilityclient, kdsoap, kdsoap-ws-discovery-client, kio-fuse, kirigami-app-components, futuresql, qt6-qtvirtualkeyboard, qt6-qthttpserver, qt6-qtwebview (6.11.2). Gate: each `--rebuild <port>` succeeds; `ports/publish --check <port>` is clean.
- [ ] 41 set: cxx-rust-cssparser (vendored), xdg-user-dirs, libsass, sassc, aha, the monospace font, libphonenumber, cdparanoia, libodfgen, libwps, liblangtag, libetonyek. Gate: as above, plus the vendoring counts updated on five pages.
- [ ] appstream `-Dqt=true -Dcompose=true`, moved to 43. Gate: phaseclosure passes; `pkg-config --modversion AppStreamQt` is 1.2.0 in `build/fs`.

**W1: Plasma libraries (43)**
- [ ] kdecoration, layer-shell-qt, kpipewire, plasma-activities, plasma-activities-stats, libkscreen, union, libplasma, plasma5support, krdp, qqc2-breeze-style and oxygen; bump kwayland, libksysguard, plasma-integration and breeze with the 4.2 flags. Gate: `make build BUILD_ARGS="--phases 43_toolkits"`; `ls build/fs/usr/share/icons/breeze_cursors build/fs/usr/share/wallpapers/Next`.

**W2: Plasma session core (50); needs section 3's W-stack**
- [ ] kglobalacceld, knighttime, kscreenlocker, aurorae, milou, kactivitymanagerd, kwin, plasma-workspace, plasma-desktop, kde-cli-tools, systemsettings, polkit-kde-agent-1, xdg-desktop-portal-kde, kwallet-pam, sddm, sddm-kcm, plasma-keyboard, kio-extras with the 4.2 flags, and the `kde-plasma` meta in its partial form. Gate: `testing/vnc-shot.py` boots the ISO to the SDDM greeter, logs in and shows a Plasma desktop. `loginctl session-status` shows the session active. The shot is cropped and pixel-checked, never judged from the whole frame.
- [ ] Every W2 package has no `usr/lib/systemd` (kpkg lint) and keeps its XDG autostart entries. Gate: `ls build/fs/etc/xdg/autostart` lists polkit-kde-authentication-agent-1, pam_kwallet_init, baloo_file and the others.

**W3: the rest of Plasma (50, 41, 44)**
- [ ] kscreen, powerdevil (with tlp-pd), plasma-nm, plasma-pa, bluedevil, kinfocenter, ksystemstats, plasma-systemmonitor, kdeplasma-addons (vendored), spectacle, discover (4.2 flags), plasma-welcome, plasma-setup, kmenuedit, print-manager, plasma-disks, plasma-thunderbolt, plasma-vault, plasma-browser-integration, ksshaskpass, kwrited, kup, kde-gtk-config, breeze-gtk with xsettingsd, iio-sensor-proxy, ocean-sound-theme, oxygen-sounds, plasma-workspace-wallpapers and plasma-sdk. Gate: rig shots of System Settings (Display, Power, Network, Sound pages), Discover's Updates page (with the stub sysupdate1) and Spectacle's region capture. `python3 testing/phaseclosure.py` passes.

**W4: Gear 26.08.2 bumps (43/44)**
- [ ] 55 bumps in lockstep order, the flag flips (gwenview, kalgebra, marble, kdeconnect, kdenlive). Gate: the release-set check (1.5.3) passes; `kfscheck` passes; the app-id patches apply.

**W5: Gear, non-PIM (43/44)**
- [ ] The 118 tier-1 modules, then the tier-2 modules whose prerequisites W0 gave. Gate per batch of about 20: `--rebuild` succeeds and phaseclosure passes. Final gate: every app's `.desktop` file is listed by `kbuildsycoca6 --menutest` in the rig.

**W6: PIM (43/44), tiers 1-10**
- [ ] ktextaddons and kdiagram (W0) first, then akonadi on SQLite and onward up to korganizer and kdepim-addons; then kleopatra's KPim6 flip. Gate: in the rig, `akonadictl status` reports running; KMail opens a local Maildir; Merkuro shows a local calendar.

**W7: late**
- [ ] The accounts-sso stack with kaccounts-integration, kaccounts-providers and signon-kwallet-extension; colord with libgusb and colord-kde; audiotube with the qtmultimedia GStreamer flip and ytmusicapi; kajongg with the Twisted chain; kigo with gnugo. Gate: each builds, and phaseclosure passes.


**KDOS ports** (4.1.5) land in the milestone their row names, each after the KDE modules it links: kdos-settingsd with section 3's W-stack (M1), the M4 identity ports after W3, kdos-discover after discover (W3), krohnkite with W3. Gate per port: `make build BUILD_ARGS="--phases <phase>,70_image --rebuild <port>"` and the gate its designing section gives.

**Metas** land last (4.4). Gate: the meta cross-check.

### 4.7 musl traps and the test-build rule

**Test-build rule.** A new port is test-built before it enters a phase list. The `kdos-devdeps` image is Alpine 3.23, and Plasma 6.8 exists only on Alpine edge [measured: inv-docs verdict]. So:
- A third-party port whose dependencies exist in Alpine 3.23 (sassc, libsass, xsettingsd, aha, cdparanoia, the font, gnugo, the Python chain) is test-built in kdos-devdeps first.
- A KDE module, whose KF 6.31, Plasma 6.8 and Qt 6.12 dependencies exist only in this tree, is test-built in the tree's own chroot, with `CC`, `CXX`, `CFLAGS` and `LDFLAGS` exported from `script/env/chroot.env`. Outside a chroot it is tested with `make build BUILD_ARGS="--phases <phase> --rebuild <port>"`. A chroot test without those flags passes where the real build fails.
- `testing/Dockerfile.devdeps` drops libseat-dev, wlroots and fcft (section 2) and gains the Qt6 and KF6 headers Alpine 3.23 has, so that KDOS's own Qt ports (4.1.5) compile there (A12).

**Known traps on musl.**

| Port | Trap | Mitigation | Evidence |
|---|---|---|---|
| plasma-workspace | The Region & Language KCM reads locale definitions from `/usr/share/i18n/locales`, while musl-locales installs them under `…/locales/musl` | Carry Chimera's `locales.patch` (one line, `kcms/region_language/glibclocaleconstructor.cpp:158`) with a `Subject:` line, and upstream an option for the path | [measured: cports `plasma-workspace/patches/locales.patch`; `build/fs/usr/share/i18n/locales/musl` exists, from `toolchain/musl-locales`] |
| plasma-workspace | `GLIBC_LOCALE_GEN` defaults ON and expects `locale-gen` and `/etc/locale.gen` | `-DGLIBC_LOCALE_GEN=OFF` (Chimera). This drops a polkit helper that musl cannot use. `GLIBC_LOCALE=ON` stays: it is a code path, not a glibc link | [measured: `CMakeLists.txt:287-312`; cports template] |
| plasma-workspace | A parallel build can compile `shell/` before its generated protocol headers exist (upstream issue #132) | In `build.sh`, after configure: `cmake --build build --target shell/qwayland-plasma-shell.h shell/wayland-plasma-shell-client-protocol.h`, then the full build. These are build commands, not a source edit (rule 9) | [measured: Alpine `plasma-workspace` APKBUILD lines 147-152; https://invent.kde.org/plasma/plasma-workspace/-/issues/132] |
| plasma-workspace | Chimera also carries `32bit-narrowing.patch`, `default-wallpaper.patch` and `fix-broken-testdata-paths.patch` | Carry none of them. They fix 32-bit only, set branding (KDOS sets its wallpaper through the global theme) and fix tests (`BUILD_TESTING=OFF`) | [measured: cports patch headers; prior verdict] |
| plasma-systemmonitor | The Applications page needs per-app cgroups, which elogind does not create | XDG `pageOrder` override (4.1.2); no patch | [measured: cports patch description] |
| kinfocenter | Chimera's `fix-output.patch` | Not carried: it fixes a libc++ `wcout` locale bug, and KDOS uses libstdc++ | [measured: patch header] |
| kwin | `CAP_SYS_NICE` is lost in kpkg's tar | pam_limits RTPRIO (3.4). Record it in kwin's `kfs.md` | [prior] |
| kwin | Chimera skips about 35 tests that need DRM or compositing, or segfault in libc | `BUILD_TESTING=OFF` as everywhere; autotests run in `plasmatest.sh` only where they need no DRM (A12) | [measured: cports `kwin/template.py`] |
| elogind | 10 Alpine patches (385 lines) for 255.24 | Carry them with `Subject:` lines, plus Chimera's `no-runtime-dir.patch` (section 3.2) | [measured: prior verdict 7] |
| sddm | utmpx through utmps; `-DSDDM_INITIAL_VT` | utmps 0.1.3.4 is present (30_foundation). The VT is set in section 3.4 | [measured: Alpine APKBUILD] |
| tlp-pd | `install-pd` installs a systemd unit unconditionally | Stage it and delete it (4.1.4) | [measured: TLP Makefile:253-262] |
| cdparanoia | Unmaintained autoconf C from 2008 | Carry Alpine's patches | [prior] |
| Qt 6.12 + Plasma 6.8 | Unproven on musl: Alpine runs 6.8.1 on Qt 6.11.1 | W1 and W2 are the proof. A Qt-private-header break in KWin or plasma-workspace is fixed with an upstream backport patch, never by downgrading Qt | [measured: prior] |
| KDE modules in general | They install systemd user units into `KDE_INSTALL_SYSTEMDUSERUNITDIR`, and kpkg lint refuses `usr/lib/systemd` | `rm -rf $PKG/usr/lib/systemd` in each such recipe, as 15 recipes do today. lint stays the guard | [measured: `lint.c:143-144`; `grep -rl usr/lib/systemd --include=build.sh` = 15] |
| Rust in KDE builds | Corrosion runs cargo; any network access fails the offline build | `vendoring = rust`, with the bundle unpacked where cargo runs (writing-ports.md "Where the bundle goes") | [measured: writing-ports.md:2035-2075] |

**Gates for the whole section.**
- [ ] `make check` passes after each wave: preflight, selftest, kfscheck, docscheck, anchorcheck, booknav and phaseclosure.
- [ ] `bash testing/docscheck.sh` passes with `bookcounts`. Every added or deleted port is in `06-reference/ports-catalogue.md` under its shelf with its version and phase, and `status.md` and `packaging.md` carry the counts.
- [ ] `docs/kfs/data` regenerated with `make book-data BUILDFS=build/fs` after each built wave, and `docs/kfs/data/tier-a.txt` gains the Plasma Tier-A rows in place of the 13 removed ones (section 8).
- [ ] `05-developer/build-troubleshooting.md` gains entries for the failures this section predicts: an ECM/KF version mismatch, a QML module not found at run time, a Corrosion or cargo offline failure, a missing generated protocol header (#132) and the `GLIBC_LOCALE_GEN` failure on musl. Each states the symptom and the fix, with no history (rule 2).


## 5. Identity: KDOS Modern and Phosphor mode

KDOS Modern is the default look (D2): Breeze-grade polish painted in KDOS's colours and carried by data packages that KDE already knows how to load. Phosphor mode is the opt-in CRT layer on top of it (D2, D7). The kit stays small and derives from upstream so that each Plasma release costs hours of upkeep, not weeks [measured: res-branding distro survey: forked QML and third-party effects break on every release (KDE bug 482637; Garuda rounded-corners report); openSUSE's build-time rsync+patch derivation tracks upstream automatically].

### 5.1 Identity principles

| # | Principle | Consequence in the plan |
|---|---|---|
| P1 | **One look from firmware to desktop.** Limine, kdos-splash, the SDDM greeter, KSplash, the desktop, the lock screen and the logout screen all draw the same `deep` ground, the same mark and the same accent. Every handover passes through full black. | KWin's first modeset (atomic commit with ALLOW_MODESET) lands inside a black frame [measured: kwin 6.8.2 src/backends/drm/drm_commit.cpp:164,182]. The accent reaches pre-session surfaces through `/etc/kdos/accent`, `/etc/vtrgb`, `limine.conf` and the sddm user's config (§5.3 step 8). |
| P2 | **Eight accents, `bone` the default, compiled once.** `libkcolor`'s `KCOL_SCHEMES` stays the single source (phosphor, amber, ice, bone, norton, borland, perfect, paper) [measured: src/libs/libkcolor/kcolor.h:60-76, `KCOL_DEFAULT_ID bone` :80-108]. Every KDE colour file, Konsole scheme, wallpaper and greeter palette is generated from it at build time. | `libkcolor` survives D1 (it is linked by kdos-theme, kdos-splash, kdos-powerd, kdos-tools and kinstall [measured: inv-features item 166]). No hand-written colour literal ships anywhere. |
| P3 | **One accent = one name.** Each accent is a separately named colour scheme (`KDOS<Accent>`) and a separately named global theme (`org.kdos.<accent>.desktop`). | `plasma-apply-colorscheme` returns "already set" for an unchanged name and applies nothing [measured: plasma-workspace kcms/colors/plasma-apply-colorscheme.cpp:77-80]; `plasma-apply-lookandfeel -a` re-applies even an unchanged id [measured: kcms/lookandfeel/tool/lnftool.cpp:75-111]. The Global Theme page becomes the accent picker, with previews. |
| P4 | **Dark and light.** Seven accents are dark; `paper` is the light one [measured: kcolor.h comment "`paper` is the light one"]. Plasma's light/dark switch flips between the user's dark accent and `paper`. | `/usr/share/kdos/xdg/kdeglobals [KDE] DefaultDarkLookAndFeel=org.kdos.bone.desktop`, `DefaultLightLookAndFeel=org.kdos.paper.desktop`, `AutomaticLookAndFeel=false` [measured: plasma-desktop kcms/landingpage/landingpage_kdeglobalssettings.kcfg:8-37]. |
| P5 | **Contrast is asserted, not hoped for.** Every text role in every generated colour set must clear WCAG AA 4.5:1 against both its backgrounds; 7:1 is the target on `text`/`deep` as libkcolor already states [measured: kcolor.h:31-58]. | **Today's KDE mapping fails.** `kdos.c` writes `[Colors:Selection] BackgroundNormal=pdark` with `ForegroundNormal=text` [measured: src/system/kdos-tools/kdos.c:2208-2209]; that pair measures 3.64 (phosphor), 4.22 (amber), 3.67 (ice), **2.42 (bone)**, 3.06 (norton), 3.68 (borland), 2.95 (perfect), **2.43 (paper)** [measured: WCAG 2 ratio computed from KCOL_SCHEMES, this session]. `ForegroundInactive` (a 55% mix of text into variant, kdos.c:2195) measures 2.84–4.30 on View and fails in all eight; `paper`'s ForegroundNeutral (secondary) on View is 4.31 and `DecorationHover`(pdark)-vs-primary is 1.14. See the fix in §5.2.1. |
| P6 | **Derive, never fork.** Lock screen, SDDM theme and panel templates start from the installed Breeze files and carry a `Subject:`-headed `.patch`, so upstream drift is a build failure instead of a black lock screen [measured: openSUSE plasma6-openSUSE.spec:101-127; kscreenlocker greeterapp.cpp:364-377 falls back to built-in QML on a load error]. | No Kvantum, no third-party plasmoids, no third-party KWin effects except the bundled, disabled auto-tiling script (D6) [measured: res-innovation risk list: Better Blur archived, LightlyShaders stale since 2025-06]. |
| P7 | **Keep investment in Breeze-specific SVG small.** KDE's 2026–2028 goal moves Plasma, decorations and the login manager to the Union/Ocean design system [source: invent.kde.org/teams/goals/goal-setting/2026/-/work_items/3]; Union is beta in 6.8 and its style format changed between 6.7 and 6.8 [source: kde.org/announcements/plasma/6/6.8.0; Union summer-2026 update]. | A Union `kdos` style is deferred until Union is KDE's default (§5.2.13). |
| P8 | **A user's choice is never overwritten.** Distribution defaults reach users through the kdedefaults layer, under the user's own keys [measured: plasma-workspace startkde/startplasma.cpp:421-445; libklookandfeel/klookandfeelmanager.cpp:345-362]. Anything `kdos` writes into `~/.config` touches only keys whose current value is KDOS-owned (a `KDOS*` scheme, a `KDOS-*` wallpaper package, a `KDOS *` Konsole profile). | Each step of §5.3 has a "only if KDOS-owned" guard. |
| P9 | **Off on software rendering, honest about cost.** Phosphor mode refuses llvmpipe; the CRT pass costs direct scanout and the hardware cursor plane while it runs [measured: kwin src/effect/effect.cpp:535-538, src/scene/workspacescene.cpp:415-417, src/compositor.cpp:918-936]. | Off by default; offered once, on the Welcome tour (D2). |
| P10 | **Credit KDE, ship no modified KDE logos.** KDOS marks replace the KDE logo in Kickoff and KSplash; KSplash and the Welcome intro carry a small "Plasma by KDE" credit [inferred: ev.kde.org lists no distribution trademark policy; KDE® is a registered trademark of KDE e.V.; upstream splash carries "Plasma 30th Anniversary Edition by KDE", Splash.qml:79]. | §5.2.8, §5.2.17. |

### 5.2 The identity kit

#### 5.2.0 Inventory

Ports and files the kit ships. "Upkeep" is the per-Plasma-release maintenance estimate [measured where cited: res-branding kdos-kit rows 63-84; otherwise inferred].

| Piece | Port / file (repo path) | Installs | Mechanism | Upkeep |
|---|---|---|---|---|
| Generators | `src/art/kdos-theme` (REWRITE) | build-time tool only | C, libkcolor + libkbase, no Qt; new subcommands `kde-colors`, `konsole`, `qt5ct-colors`, `wallpaper`, `greeter-globals`, `contrast` | 0 h |
| Colour schemes | `src/desktop/kdos-plasma-look` (NEW) | `/usr/share/color-schemes/KDOS{Phosphor,Amber,Ice,Bone,Norton,Borland,Perfect,Paper}.colors` | KColorScheme files | 0 h (format stable since KDE 4) |
| Global themes | same port | `/usr/share/plasma/look-and-feel/org.kdos.<accent>.desktop` ×8 | Plasma/LookAndFeel KPackage | 0–1 h + previews |
| Plasma style | same port | `/usr/share/plasma/desktoptheme/kdos/` | desktoptheme, `FallbackTheme=default`, no `colors` file | 1–4 h visual check |
| Desk layouts | same port | `/usr/share/plasma/layout-templates/org.kdos.desk.{kdos,dock,tiling}/` + plasmoidsetupscripts | Plasma scripting | 0–1 h |
| KSplash | same port (copied into each global theme at build time) | `…/org.kdos.<accent>.desktop/contents/splash/Splash.qml` | KSplashQML | 0–1 h |
| Konsole | same port | `/usr/share/konsole/KDOS-<Accent>.colorscheme` ×8, `KDOS-<Accent>.profile` ×8 | Konsole data dirs | 0 h |
| Qt 5 palettes | same port | `/usr/share/qt5ct/colors/KDOS-<Accent>.conf` ×8 | qt5ct colour files | 0 h |
| Distribution defaults | same port | `/usr/share/kdos/xdg/{kdeglobals,kwinrc,plasmarc,ksplashrc,kcminputrc,kscreenlockerrc,plasmashellrc,breezerc,konsolerc,kcm-about-distrorc,plasma_workspace.notifyrc,startkderc,dolphinrc,baloofilerc,klipperrc,krunnerrc,plasmanotifyrc,kde-mimeapps.list,kglobalshortcutsrc,fcitx5/config}` | XDG config cascade (§5.2.19) | 0–1 h (renamed keys) |
| Icon theme | `src/art/kdos-icons` (REWRITE) | `/usr/share/icons/KDOS` (dark) and `/usr/share/icons/KDOS-Light`; marks in `hicolor` | icon theme `Inherits=` + `FollowsColorScheme=true` | 0 h |
| Cursors | `ports/core/themes/breeze` (CHANGE: `BUILD_CURSOR=ON`) | `/usr/share/icons/breeze_cursors` | Breeze cursors | 0 h |
| Wallpapers | `src/art/kdos-wallpapers` (NEW) | `/usr/share/wallpapers/KDOS-<Accent>/` ×8 (`images/`, `images_dark/`) | Plasma wallpaper packages | 0 h |
| Window decoration | `ports/core/themes/breeze` (CHANGE: bump 6.7.5→6.8.x, `WITH_DECORATIONS=ON`) + new `kdecoration` port | `/usr/lib/qt6/plugins/org.kde.kdecoration3/breezedecoration.so` | KDecoration3, configured via breezerc | 0 h |
| Lock / logout | none (stock plasma-desktop) + `kscreenlockerrc` keys | — | shell package `org.kde.plasma.desktop` | 0 h |
| SDDM theme | `src/art/kdos-sddm-theme` (NEW) | `/usr/share/sddm/themes/kdos/` | copy of plasma-desktop's `breeze` SDDM theme + `kdos-sddm.patch` | 1–4 h |
| Sounds | `src/art/kdos-sounds` (NEW) + `ocean-sound-theme` port (NEW, 6.8.x) | `/usr/share/sounds/kdos/` | XDG sound theme, `Inherits=ocean,freedesktop` | 0 h |
| Fonts | new port for the chosen monospace (§5.2.11), `noto-fonts` (exists), `terminus-ttf` (exists) | `/usr/share/fonts/…`, `/etc/fonts/conf.d/56-kdos-*.conf` | fontconfig + kdeglobals | 0 h |
| GTK | new ports `breeze-gtk`, `kde-gtk-config` (6.8.x) | Breeze GTK 2/3/4 themes; kded `gtkconfig` | kde-gtk-config writes `~/.config/gtk-{3,4}.0` | 0 h; carried libadwaita-colour patch 1 h |
| About / os-release | `fs/etc/os-release` (CHANGE), `kcm-about-distrorc`, `/usr/libexec/kdos/about-{slot,tree,accent,boxes}` (in `src/system/kdos-tools`) | — | kinfocenter about-distro | 0 h |
| Welcome pages | `src/desktop/kdos-welcome` (NEW) | `/usr/share/plasma/plasma-welcome/{intro-customization.desktop,extra-pages/NN-*.qml}` | plasma-welcome extra pages (QML only) | 0–1 h |
| Accent watcher | `src/desktop/kded-kdos` (NEW, C++/KF6, D3; also owns the `.ktar` import, §6) | kded6 module | KConfigWatcher on kdeglobals | 0 h (KF6-stable API) |
| CRT effect | `ports/core/<kde shelf>/kwin/kdos-crt.patch` (NEW) | built-in KWin effect `kdos_crt` | C++ `Effect::paintScreen` | 1–8 h (rebase) |
| CRT motion | `src/desktop/kdos-kwin-motion` (NEW) | `/usr/share/kwin/effects/kdos_{unfold,login,quit}/` | KWin JS scripted effects | 0–2 h |
| Auto-tiling | `krohnkite` port (NEW, KWin script) | `/usr/share/kwin/scripts/krohnkite/` | KWin/Script, disabled | 0–2 h |

Deleted or rewritten by the kit (each removal lands with the change that makes it dead):

| Path | Action | Why |
|---|---|---|
| `fs/etc/profile.d/10-wayland.sh:75-76` (`XCURSOR_THEME=KDOS-cursors`, `XCURSOR_SIZE=24`) | DELETE the two exports | KWin prefers `$XCURSOR_THEME`+`$XCURSOR_SIZE` over kcminputrc whenever both are set, so the global theme's cursor would be ignored [measured: kwin src/cursor.cpp:115-126 (verify-res-branding missed)] |
| `fs/etc/profile.d/10-wayland.sh:41,59` (`XDG_CURRENT_DESKTOP=KDOS` fallback, `XDG_MENU_PREFIX=kdos-`) | REWRITE to `KDE` / `plasma-` | startplasma forces `KDE`/`plasma-` in the session only [measured: verify-res-branding missed]; shells outside it must agree so `kde-mimeapps.list` and `plasma-applications.menu` apply |
| `fs/usr/share/glib-2.0/schemas/90_kdos.gschema.override` | REWRITE to `gtk-theme='Breeze'`, `icon-theme='KDOS'`, `cursor-theme='breeze_cursors'`, `cursor-size=24`, `font-name='Noto Sans 10'`, `monospace-font-name='<KDOS mono> 10'` | it names the deleted `KDOS` GTK theme and `KDOS-cursors` [measured: file contents]; kde-gtk-config writes GSettings per user, this stays the pre-login default |
| `fs/etc/skel/.config/gtk-3.0/settings.ini`, `gtk-4.0/settings.ini` | DELETE | kde-gtk-config owns them; the skel copy names `KDOS-phosphor` [measured: verify-res-branding missed] |
| `fs/etc/xdg/kdos-mimeapps.list`, `fs/etc/xdg/menus/kdos-applications.menu` | REPLACE by `/usr/share/kdos/xdg/kde-mimeapps.list` (§5.7); menu deleted (plasma-workspace ships `plasma-applications.menu`) | read only under `XDG_CURRENT_DESKTOP=KDOS` / `XDG_MENU_PREFIX=kdos-` |
| `fs/etc/skel/.config/kdos/{comp.conf,panel.conf,launcher.conf}`, `fs/etc/skel/.config/kdos-comp/`, `fs/etc/skel/.config/xdg-desktop-portal-wlr/` | DELETE | cell-desktop configuration (D1). `fs/etc/skel/.config/kdos/favorites` stays a KDOS file — the box-warmup list and the two-letter codes [measured: src/system/kdos-appbox/main.c:649-673]; its fate is owned by the deletion section (§2.5), and §5.5.1 keeps the Task Manager launchers in step with it |
| `src/art/kdos-gtk-theme` (recoloured adw-gtk3), `src/art/kdos-cursors` (Bibata recolour), the Papirus art in `src/art/kdos-icons/art` (45 MB, 23 254 SVGs) | DELETE (host). The box-environment section decides whether boxes keep an exported GTK theme | Breeze-GTK, Breeze cursors and Breeze icons replace them (§5.2.5–5.2.6, §5.2.15) |
| `ports/core/themes/qt6ct` | DELETE (and from `script/phases/44_apps/packages.d/themes.txt:29`) | never selected; KDE recommends against it [measured: verify-res-branding qt5ct verdict] |
| `kdos.c` `kde_palette()` and the kdeglobals merge (kdos.c:2182-2253, 2477-2528), the qt6ct writer, `themerc-override`, `kdos-term`, the wallpaper retint into `~/.cache/kdos/wallpaper.png`, the SIGHUP list | DELETE (§5.3) | replaced by global-theme application |

Doc pages the inventory changes (hard rule 1; written as present-tense constraints, rule 2): `docs/kdos/02-user-guide/theming.md` (rewritten whole: every section from "The eight accents" to "How the theme is generated"), `docs/kdos/03-architecture/design-language.md` (the colour-slot contract becomes the KColorScheme role contract of §5.2.1; cell-only sections move out with D1), `docs/kdos/06-reference/configuration.md` and `filesystem-and-ipc.md` (every path above), `docs/kdos/06-reference/ports-catalogue.md`, `docs/kdos/06-reference/repository-layout.md` (src/desktop now holds Qt/QML code), and the `kfs.md` of every new or changed port with its `docs/kfs` page.

#### 5.2.1 Colour schemes and the contrast fix

- Generated by `kdos-theme kde-colors <outdir>` from `KCOL_SCHEMES`; one file per accent, `[General] Name=KDOS <Label>`, `ColorScheme=KDOS<Accent>`; groups `[Colors:View|Window|Button|Selection|Tooltip|Complementary|Header]`, `[Colors:Header][Inactive]`, `[WM]`, `[ColorEffects:Disabled|Inactive]`, `[General] AccentColor` left unset so the generated Selection applies [measured: kcms/colors/colorsapplicator.cpp:125-336 recolours Selection, Link and Decoration only when an accent is set].
- **Role mapping (replaces kdos.c:2182-2253):**

| Role | Today | KDOS Modern | Measured ratio range over the 8 accents |
|---|---|---|---|
| Selection BackgroundNormal / ForegroundNormal | `pdark` / `text` | `primary` / `deep` | 6.02 (paper) – 18.31 (perfect) [measured: this session] |
| Selection ForegroundLink/Active/Positive/Visited | `primary`, `pdark` | `deep` (Link/Active/Positive), `kcol_mix(deep, primary, 30)` (Visited) — solver-checked | to measure (gate below) |
| ForegroundInactive (all sets) | `kcol_mix(text, variant, 55)` (2.84–4.30) | `kcol_muted(sc)` — libkcolor's existing solver that walks `text` until 4.5:1 on `deep` (dark schemes at 56%, paper 60% → 4.54) [measured: design-language.md:411-414] — re-solved per background (View and Window differ) | ≥4.5 by construction |
| ForegroundNeutral on View | `secondary` (paper 4.31) | `secondary` darkened by the solver until ≥4.5 on both backgrounds | ≥4.5 |
| DecorationHover | `pdark` (paper 1.14 against primary) | `kcol_mix(primary, deep, 35)` — a decoration, not text; asserted ≥3:1 against the background (WCAG 1.4.11 non-text) | ≥3.0 |
| Window / View / Button / Header backgrounds | `deep` / `variant` / mix 9% / mix 7% | unchanged | text on Button 12.08–16.19 [measured] |

- [ ] Add `kdos-theme kde-colors` emitting the 8 files with the mapping above. Gate: `kdos-theme contrast /usr/share/color-schemes/KDOS*.colors` (new subcommand, C, parses the files it wrote) exits 0 and prints one row per scheme × set × role, every text role ≥4.5 on BackgroundNormal **and** BackgroundAlternate, every Decoration ≥3.0.
- [ ] Add the same assertion to `testing/selftest.sh` (libkcolor self-test already asserts 7:1 text/deep and 4.5:1 accent/deep [measured: kcolor.h:31-35]); prove it fails by feeding today's pdark mapping (memory: guards-and-config-traps). Gate: selftest fails on the old mapping, passes on the new.
- [ ] `kdos-theme greeter-globals <accent>` writes the full `[Colors:*]`, `[WM]`, `[ColorEffects:*]` block for the default accent into `/usr/share/kdos/xdg/kdeglobals`, because processes outside a Plasma session (greeter, pkexec, early autostart) read only XDG config dirs, not kdedefaults [measured: klookandfeelmanager.cpp:216-223; startplasma.cpp:446-466]. Gate: `XDG_CONFIG_DIRS=/usr/share/kdos/xdg kreadconfig6 --file kdeglobals --group Colors:Selection --key BackgroundNormal` prints bone's primary.
- [ ] Docs: theming.md "How the accents are kept readable" rewritten with the role table and the measured ranges; design-language.md colour section. Gate: `bash testing/docscheck.sh && python3 testing/anchorcheck.py` pass; a grep of the old surface name in each named page is empty.

#### 5.2.2 Global themes `org.kdos.<accent>.desktop`

- 8 packages, ids `org.kdos.{phosphor,amber,ice,bone,norton,borland,perfect,paper}.desktop`; `metadata.json` `KPackageStructure=Plasma/LookAndFeel`, `X-Plasma-APIVersion=2` [measured: lookandfeel.cpp:20-61]. Generated from one template by the port's `build.sh` (data, no rationale comments: rule 6; the reasons go in `kfs.md`).
- `contents/defaults` uses only recognised `[file][group]` keys [measured: klookandfeelmanager.cpp:66-101, 436-552]:
  - `[kdeglobals][General] ColorScheme=KDOS<Accent>`, `font`, `fixed`, `smallestReadableFont`, `toolBarFont`, `menuFont`; **plus `[kdeglobals][WM] font=<same as General font>`** — without a WM-group font key the theme's fonts are never applied (detection reads WM, apply reads General; unfixed on master 2026-10-28) [measured: klookandfeelmanager.cpp:95-97 vs :510-520; verify-res-branding verdict]. File an upstream bug + MR in parallel (D3).
  - `[kdeglobals][KDE] widgetStyle=Breeze`; `[kdeglobals][Icons] Theme=KDOS` (`KDOS-Light` for paper).
  - `[plasmarc][Theme] name=kdos`; `[kcminputrc][Mouse] cursorTheme=breeze_cursors`.
  - `[kwinrc][org.kde.kdecoration2] library=org.kde.breeze`, `theme=Breeze`, `BorderSize=Normal` (the group keeps the `kdecoration2` name under KDecoration3 [measured: res-branding item 1]); `[kwinrc][WindowSwitcher] LayoutName=thumbnail_grid`; `[kwinrc][Windows] Placement=Centered`.
  - `[ksplashrc][KSplash] Theme=org.kdos.<accent>.desktop` (own id; the nested form — a top-level `[KSplash]` is inert, as Kubuntu's and Garuda's are [measured: klookandfeelmanager.cpp:536-546]).
  - `[Wallpaper] Image=KDOS-<Accent>` (a default only for desktops with no wallpaper set [measured: wallpapers/defaultwallpaper.cpp:10-45]).
  - `layouts/defaults`: `[kwinrc][org.kde.kdecoration2] ButtonsOnLeft=M`, `ButtonsOnRight=IAX`.
- `contents/layouts/org.kde.plasma.desktop-layout.js` delegates to the desk template (§5.5.4). `contents/plasmoidsetupscripts/org.kde.plasma.kickoff.js` sets `icon=start-here-kdos`, `menuLabel=Start` whenever Kickoff is added [measured: shellcorona.cpp:1706-1735; Fedora ships the same file].
- `contents/previews/{preview.png,fullscreenpreview.jpg}` captured on the rig per accent (`testing/lnf-previews.sh`, new), committed as port art.
- A global theme with a missing file falls back to `org.kde.breeze.desktop` for it [measured: lookandfeel.cpp:53-60]: every package must ship `splash/` and `layouts/`, or the Breeze splash/layout appears. The lint below checks it.
- `/usr/share/kdos/xdg/kdeglobals [KDE] LookAndFeelPackage=org.kdos.bone.desktop` is the distribution default [measured: kubuntu-settings kdeglobals; startplasma.cpp:369-407].
- Trap: a changed `defaults` under an unchanged id never reaches an existing user (`kdedefaults/package` equality) [measured: startplasma.cpp:438-445]. KDOS has no installed base [memory: kdos-first-version-no-users]; colour edits still propagate through `ColorSchemeHash`. No migration hook is built now.
- [ ] Port `src/desktop/kdos-plasma-look` with the 8 themes. Gate: `kpackagetool6 --type Plasma/LookAndFeel --list` lists 8 `org.kdos.*`; `plasma-apply-lookandfeel -l` lists them.
- [ ] `testing/lnfcheck.py` (new): parses every `contents/defaults` against the recognised-key list above, fails on an unrecognised `[file][group]`, a missing `[kdeglobals][WM]` font key, a missing `splash/Splash.qml` or `layouts/`. Prove it fails on a Kubuntu-style top-level `[KSplash]`. Gate: `python3 testing/lnfcheck.py` in `make check`.
- [ ] Rig: first login on bone. Gate: `vnc-shot.py --wait … --shot` per accent after `kdos theme <accent>`; pixel-sample the panel and a Dolphin selection against the generated colours (memory: reading-rig-screenshots).

#### 5.2.3 Plasma style `kdos`

- `/usr/share/plasma/desktoptheme/kdos/{metadata.json,plasmarc,widgets/panel-background.svgz,dialogs/background.svgz,widgets/tooltip.svgz,widgets/button.svgz,widgets/tasks.svgz,icons/start.svgz}`; `plasmarc [Settings] FallbackTheme=default`; **no `colors` file** so it follows all eight schemes [measured: libplasma private/theme_p.cpp:395-425; Breeze desktoptheme has no colors file].
- Square frames (0 px radius), 1 px `Highlight` rule on the panel edge; Task Manager running indicator as an accent underline; `[BlurBehindEffect] enabled=false`; `[AdaptiveTransparency] enabled=false`; opaque panel [measured: theme_p.cpp:315-349 reads these groups].
- [ ] Gate: rig crops of panel, a popup (Kickoff), a tooltip at NEAREST 4x on bone and paper; no Breeze-rounded corner visible in the overridden SVGs.

#### 5.2.4 Window decoration (decision)

| Option | Follows the 8 schemes live | Box `decocolor` rule tint | Cost |
|---|---|---|---|
| **Breeze KDecoration3, configured (chosen, P0)** | yes, from `[WM]` colours | yes [measured: kwin rulesettings.kcfg:338-341 forces a colour scheme on the decoration] | breeze bump 6.7.5→6.8.x, `WITH_DECORATIONS=ON`, new `kdecoration` port; 0 h upkeep |
| Aurorae v2 SVG theme | no — static SVG per accent, 8 theme dirs, renamed on every accent (`__aurorae__svg__KDOS-<Accent>`) | not honoured by a static SVG [inferred; not measured] | ~10 SVG ×8 generated; 0–1 h |
| KDOS C++ KDecoration3 plugin | yes | yes | 1–1.5k LOC; SONAME-bound, not release-bound, but the API broke 2→3 inside Plasma 6 [measured: breezedecoration.cpp:37 plain K_PLUGIN_FACTORY; kdecoration src/CMakeLists.txt:49-50]; 1–8 h/release |

Decision: Breeze, square (`/usr/share/kdos/xdg/breezerc [Common] RoundedCorners=false`, `OutlineEnabled=true`; `[Windeco] DrawBackgroundGradient=false`, `TitleAlignment=AlignLeft`, `ButtonSize=ButtonDefault`) [measured: breeze kdecoration/breezesettingsdata.kcfg:9-122]. Aurorae is refused because it would break the per-box titlebar tint and adds a name per accent. A C++ `kdos` decoration stays P2, written only if the rig shows Breeze cannot carry the look; it waits for the Union/Ocean decoration work (P7).

- [ ] `ports/core/themes/breeze`: version 6.8.x, `BUILD_CURSOR=ON`, `WITH_DECORATIONS=ON`, `WITH_WALLPAPERS=ON` (keeps `Next`, the last-resort wallpaper fallback, so a misnamed KDOS wallpaper never leaves a blank desktop [measured: defaultwallpaper.cpp:43-46; verify-res-branding breeze verdict]); `kfs.md` Flags rewritten; new `kdecoration` 6.8.x port; `ports/publish`; `make book-stamp`. Gate: `kfscheck.py`, a built `breezedecoration.so`, `kwin_wayland` loads `library=org.kde.breeze` (rig `--shot` of a titlebar).

#### 5.2.5 Icon themes `KDOS` and `KDOS-Light`

- `index.theme`: `Inherits=breeze-dark,breeze,hicolor` (KDOS) / `Inherits=breeze,hicolor` (KDOS-Light), **`FollowsColorScheme=true`** in both. KIconLoader recolours only when the *top* theme's own index.theme sets it, and only SVGs containing `<style id="current-color-scheme">` [measured: kiconthemes 6.30.0 kiconloader.cpp:719, :1342-1348; kicontheme.cpp:440; processSvg :668-711]. Today's `KDOS/index.theme` has `Inherits=breeze-dark,Adwaita,hicolor` and no `FollowsColorScheme` [measured: build/fs/usr/share/icons/KDOS/index.theme].
- Marks (generated by the existing `genmarks.py`): `start-here-kdos`, `start-here-kde`, `start-here-kde-symbolic` (alias so Kicker, the window list and any re-added launcher show the penguin [measured: res-branding item 29]), `distributor-logo-kdos`, `kdos` (the os-release LOGO name nothing installs today [measured: fs/etc/os-release:7; src/art/kdos-icons/build.sh:25-27]).
- Breeze icons follow the accent through `ColorScheme-Accent` (1 261 of 19 860 Breeze SVGs carry it [measured: build/fs/usr/share/icons/breeze, this session]). Trap: GTK, libadwaita and non-KDE Qt apps render the baked fallback `#3daee9` (Breeze blue) [measured: verify-res-branding icons verdict]. Mitigation: none built now; listed in known-gaps.md.
- [ ] Rewrite `src/art/kdos-icons` to marks + two index.theme files; delete `art/`, `atlas`, `vendor.py`, the Papirus licence notice. Gate: `du -sh /usr/share/icons/KDOS` < 1 MB; Dolphin folder icon pixel-sampled in amber equals amber's primary.

#### 5.2.6 Cursor

`breeze_cursors` for every accent (SVG `cursors_scalable` + Xcursor), selected by each global theme and `/usr/share/icons/default/index.theme Inherits=breeze_cursors`. Cursors cannot follow the accent: KWin renders SVG cursors with a plain QSvgRenderer [measured: kwin src/utils/svgcursorreader.cpp:126], so per-accent cursors would cost 8 themes (~4.4 MB each as today [measured: res-branding item 16]) and a cursor-theme switch per accent. **Recommendation, confirm with user:** one neutral cursor; `kdos-cursors` deleted.

- [ ] Gate: after removing the `XCURSOR_*` exports, `kreadconfig6 --file kcminputrc --group Mouse --key cursorTheme` → `breeze_cursors` and a rig shot shows the Breeze arrow under both KDE and Xwayland apps.

#### 5.2.7 Wallpapers and the live-switch trap

- `src/art/kdos-wallpapers`: `/usr/share/wallpapers/KDOS-<Accent>/metadata.json`, `contents/images_dark/<W>x<H>.png` (the mark on the accent's `deep`) and `contents/images/<W>x<H>.png` (the same composition on paper's ground with the accent's primary), from `fs/usr/share/backgrounds/kdos/default-wallpaper.png` (2561×1601) moved into the port, retinted by `kdos-theme wallpaper`. A package with both dirs gets Plasma's dark/light switch, so the wallpaper follows light/dark with no KDOS code [measured: wallpapers/image/plugin/finder/packagefinder.cpp:67-110; imagebackend.cpp:487-503].
- **Trap:** the image wallpaper computes its default once per instance and never re-reads `LookAndFeelPackage` [measured: mediaproxy.cpp:67-75]; `KLookAndFeelManager::save` never writes a wallpaper [measured: klookandfeelmanager.cpp:88, :618-620]. A live accent switch through the Global Theme page or `plasma-apply-lookandfeel` therefore leaves the old accent's wallpaper up until plasmashell restarts [inferred from code; verify-res-branding wallpaper verdict]. Mitigation: `kdos theme` and the accent watcher (§5.3 step 3) set it explicitly, and only on desktops whose current Image is a `KDOS-*` package, through `org.kde.PlasmaShell /PlasmaShell evaluateScript` (not `plasma-apply-wallpaperimage`, which sets every desktop unconditionally).
- Lock screen: `/usr/share/kdos/xdg/kscreenlockerrc [Greeter][Wallpaper][org.kde.image][General] Image=file:///usr/share/wallpapers/KDOS-Bone/` [measured: Fedora kscreenlockerrc pattern]; switched by §5.3 step 3 under the same KDOS-owned guard.
- [ ] Gate: on the rig, `kdos theme amber` then a `--shot` with **no sleep** beyond 2 s shows the amber wallpaper on every output without a plasmashell restart (memory: rig-shot-cost); a user-chosen image set beforehand survives the switch.

#### 5.2.8 KSplash

- One template `Splash.qml` (150–300 lines), copied into each global theme at build time (no cross-package reference; whether KPackage follows a symlink out of the package root is unmeasured, so no symlink).
- Contract: root `Rectangle`, integer `stage` 1–6, imports only QtQuick and Kirigami [measured: ksplash/ksplashqml/splashwindow.cpp:85-125; splashapp.cpp:28-35,110-117]. Draws kdos-splash's settled layout: black → phosphor line expanding → penguin mark + KDOS wordmark at the same position the greeter uses → `[■■■□]` bar driven by `stage` → small "Plasma by KDE" credit. Colours from `Kirigami.Theme`, so it follows the accent with no regeneration. Text in the KDOS monospace (§5.2.11). `Kirigami.Units.longDuration === 0` (reduced motion) shows the settled frame only.
- [ ] Gate: rig `--shot` during login on bone and phosphor (KSplash is short; photograph with no sleep, memory: rig-shot-cost); `QML_IMPORT_TRACE` free of import errors in `~/.local/share/sddm/wayland-session.log` (path per the login section).

#### 5.2.9 Lock and logout screens (decision)

The lock and logout QML load from the Plasma/Shell package named by `plasmashellrc [Shell] ShellPackage`, never from a global theme; a global theme's `lockscreen/` is ignored [measured: libplasma shellpackage.cpp:62-83; kscreenlocker greeterapp.cpp:502-516; logout-greeter/main.cpp:28-36,85-86; verify-res-branding confirmed].

Decision for D2/D9 "matching login/splash/lock": **stock Breeze lock and logout screens in KDOS colours, wallpaper and fonts**, with the SDDM theme derived from the same Breeze components (§5.2.10), so lock and login share one layout by construction. KSplash is KDOS-drawn and continues the boot animation. Reasons: the lock QML imports private modules (`org.kde.plasma.private.sessions`, `org.kde.breeze.components`, `org.kde.kscreenlocker`) [measured: LockScreenUi.qml:10-26] and is security-sensitive; a KDOS shell package `org.kdos.desktop` would rename the applets file, stop plasma-desktop's 16 migration scripts and cost 2–8 h per minor [measured: shellcorona.cpp:794,1111; scriptengine.cpp:270-271]. That override is P2, revisited after Ocean/Union lands.

- `/usr/share/kdos/xdg/kscreenlockerrc`: `[Daemon] Autolock=true`, `Timeout=10`, `LockGrace=5`, `LockOnResume=true`; `[Greeter][LnF][General] alwaysShowClock=true`, `showMediaControls=true` [measured: kscreenlockersettings.kcfg; lockscreen config.xml keys].
- [ ] Gate: rig `--keys meta-l` then `--shot`: KDOS wallpaper, KDOS colours, clock; unlock by typed password succeeds.

#### 5.2.10 SDDM theme `kdos`

- Port `src/art/kdos-sddm-theme`: `build.sh` copies `/usr/share/sddm/themes/breeze/` (installed by plasma-desktop) and applies `kdos-sddm.patch` (`Subject:` header) — the openSUSE derivation without its `sed` half (hard rule 9) [measured: plasma6-openSUSE.spec:101-109]. The patch changes: `theme.conf` `background=`, `type=image`, `showlogo=shown`, `logo=` → the KDOS mark; footer credit; the clock in the KDOS monospace. Built against the plasma-desktop version in the tree, so a Breeze QML change breaks the patch at build time, never at the greeter.
- **Greeter-safe default:** SDDM shows one fixed theme; it reads no light/dark or day/night keys (Plasma Login Manager's day/night code is commented out as well [measured: plasma-login-manager startplasma.cpp:245-289]). The greeter always uses the system accent's dark variant (paper → bone for the greeter), background `images_dark/`.
- Colours: SDDM's theme reads the sddm user's `~/.config/kdeglobals`, `plasmarc`, `kcminputrc` — the files sddm-kcm's helper copies into the sddm user's home [measured: sddm-kcm sddmauthhelper.cpp:63,127-193]. `kdos-powerd`'s `accent` verb writes those three files for the system accent (§5.3 step 8), so `kdos theme` and sddm-kcm's "Apply Plasma Settings" converge on the same files.
- `/etc/sddm.conf.d/00-kdos.conf` (the one KDOS SDDM drop-in, owned by the login section §3; it holds `[General]`, `[Wayland]` and `[Theme]`) carries `[Theme] Current=breeze` until M4 and `Current=kdos` from M4, plus `CursorTheme=breeze_cursors`, `CursorSize=24`, `Font=Noto Sans,10`; this section supplies the `[Theme]` values. `[Autologin]` lives only in `kde_settings.conf` (§3.3). `/etc/sddm.conf.d` is SDDM's own config dir, not an XDG dir, so §5.2.19's rule does not touch it.
- The Wayland greeter is upstream beta (plasma-workspace `INSTALL_SDDM_WAYLAND_SESSION` default OFF, "BETA, do not deploy to final users yet"; SDDM "Wayland support is experimental") [measured: verify-res-session missed]: the theme ships only behind the rig gate below. The on-screen keyboard element must avoid the virtual-keyboard QML type that broke Manjaro's Breath theme on 6.8 [source: forum.manjaro.org/t/190649].
- [ ] Gate: rig boot of the ISO to the greeter, `--shot` on bone and after `kdos theme norton` + reboot; login by typed password lands in Plasma; `--keys` drive the power and keyboard-layout buttons.
- [ ] Docs: boot-and-init.md (greeter handover), theming.md "The boot menu, the splash and the text consoles". Gate: `bash testing/docscheck.sh && python3 testing/anchorcheck.py` pass; a grep of the old surface name in each named page is empty.

#### 5.2.11 Fonts (D7)

- UI: Noto Sans 10 (`font`, `menuFont`, `toolBarFont`, `[WM] activeFont`), `smallestReadableFont=Noto Sans,8` [measured: Fedora kdeglobals; plasma-workspace kcms/fonts/fontssettings.kcfg:11-53 defaults Noto Sans 10, Hack 10].
- Monospace: **JetBrains Mono** (B17). Neither Hack nor JetBrains Mono is ported [measured: ls ports/core/fonts]; Hack is plasma-integration's declared default monospace [measured: verify-res-branding missed, CMakeLists.txt:83-87], so without an explicit `fixed` key the fallback is whatever fontconfig picks.

| Candidate | Status | Measure |
|---|---|---|
| JetBrains Mono 2.304 | new port, **chosen** (version to confirm at fetch) | tall x-height; ligatures off in the KDOS Konsole profile |
| Noto Sans Mono 2.015 | ported (`noto-fonts`) | already the gschema monospace |
| DejaVu Sans Mono 2.37 | ported (`ttf-dejavu`) | reference |

  Criteria, in `testing/monofont.py` (new, fontTools is ported): coverage of U+2500–257F box drawing, U+2580–259F blocks, U+2800–28FF braille (btop), Latin Extended-A/B, Greek, Cyrillic; cell width measured on `M`, not the face maximum (memory: cell-width-is-not-max-advance); 0/O, 1/l/I distinction; rig shots of a Konsole test file at scale 1, 1.25, 2. The script gates JetBrains Mono on these criteria and records its metrics; a coverage hole is filled by a fontconfig fallback to Noto Sans Mono, not by changing face.
- Rendering: `XftAntialias=true`, `XftHintStyle=hintslight`, `XftSubPixel=none` (greyscale, identical at fractional scales) [measured: Kubuntu sets hintslight; inferred for subpixel none].
- `/etc/fonts/conf.d/56-kdos-mono.conf` puts the chosen face first for `monospace`; `75-yes-terminus.conf` stays (bitmap Terminus by explicit name only) [measured: prior report 6].
- Terminus (TTF, `terminus-ttf` 4.49.3) appears only as a Phosphor-mode option (§5.4.4).
- **Monospace touches (KDOS Modern):** Konsole, Kate's editor (`fixed`), the panel clock (`org.kde.plasma.digitalclock` `fontFamily` via layout), KSplash text, the SDDM clock, System Monitor numerals. Window titles stay Noto Sans.
- [ ] Gate: `monofont.py` report committed to the plan's evidence; `fc-match monospace` = the chosen face; Fonts KCM shows it as Fixed width.

#### 5.2.12 Konsole

- `KDOS-<Accent>.colorscheme` ×8 generated by `kdos-theme konsole` from the same ANSI table foot uses (`ansi_all()`) [measured: res-branding item 73]; `KDOS-<Accent>.profile` ×8 (`Name=KDOS <Label>`, `ColorScheme=KDOS-<Accent>`, `Font=<KDOS mono>,11`, `HistorySize=10000`, block cursor, visual bell), in `/usr/share/konsole`.
- `/usr/share/kdos/xdg/konsolerc`: `[Desktop Entry] DefaultProfile=KDOS-Bone.profile`; `[LightDarkTheme] SyncProfileWithSystemTheme=true`, `DarkThemeProfile=KDOS Bone`, `LightThemeProfile=KDOS Paper` — values are profile **Names**, not file names, and without the sync key both are inert [measured: konsole 26.08.2 src/settings/konsole.kcfg:236-248; ProfileManager.cpp:295-303,332]. `/etc/xdg/konsolerc` and XDG config dirs are safe: Konsole installs no konsolerc (its `data/konsolerc` is compiled into the binary) [measured: data/data.qrc:4].
- `[General] TerminalApplication=konsole`, `TerminalService=org.kde.konsole.desktop` in kdeglobals (today `foot` [measured: kdos.c:2241-2242]).
- [ ] Gate: Konsole on the rig opens in KDOS Bone; flipping light/dark in Quick Settings switches the open tab to KDOS Paper.

#### 5.2.13 Union (deferred)

`unionStyle=kdos` (`@import breeze`, `--corner-radius:0`, KDOS fonts) only after Union is KDE's default and its format is stable; needs `union` + `cxx-rust-cssparser` ports [measured: union v6.8.2 StyleRegistry.cpp:139-153; src/input/css/CMakeLists.txt:35-37]. No task now; recorded in known-gaps.md.

#### 5.2.14 Sound theme

- `kdos-sfx render <dir>` (new mode in `src/system/kdos-tools/sfx.c`, which survives D1) writes OGG at build time for `desktop-login`, `desktop-logout`, `message-new-instant`, `dialog-error`, `dialog-warning`, `complete`, `device-added`, `device-removed`; `/usr/share/sounds/kdos/index.theme Inherits=ocean,freedesktop`; `kdeglobals [Sounds] Theme=kdos` [measured: ocean-sound-theme 6.8.2 index.theme; plasma-desktop calibrationtool.cpp:313-321 reads `[Sounds] Theme`; sfx.c:96-120 login/notify/error/degauss].
- `/usr/share/kdos/xdg/plasma_workspace.notifyrc [Event/startkde] Action=Sound`, `[Event/exitkde] Action=Sound` [measured: Fedora notifyrc; startkde/startup-sound/main.cpp:50-55].
- New port `ocean-sound-theme` 6.8.x (fallback names).
- [ ] Gate: `canberra-gtk-play -i dialog-error` and a Notifications-KCM test sound play the KDOS sample on the rig with `--audio` (memory: rig-audio-capture).

#### 5.2.15 GTK

- New ports `breeze-gtk` and `kde-gtk-config` 6.8.x. Breeze-GTK apps follow the KDE scheme; kde-gtk-config emits only `*_breeze` colour names and no libadwaita names (`window_bg_color`, `accent_bg_color`) [measured: kde-gtk-config kded/configvalueprovider.cpp:349-530; custom_css.cpp:63,97]; libadwaita apps get the portal accent and colour-scheme only.
- Carried patch `kde-gtk-config/libadwaita-colors.patch` (`Subject:`), adding the libadwaita named colours to the same `colors.css` it writes, upstream MR in parallel (D3). Two writers of `gtk.css` are not allowed: KDOS writes nothing under `~/.config/gtk-*`.
- [ ] Gate: GTK3 (Breeze-GTK) and GTK4/libadwaita test windows on the rig pick up amber's selection colour after `kdos theme amber`.

#### 5.2.16 Qt 5 applications (Default A11)

27 ports depend on qt5-qtbase (vlc, keepassxc, lmms, quassel, mumble, smplayer, sqlitebrowser, librecad, wsjtx, qsstv, gnuradio, candle2, python3-pyqt5, …); plasma-integration is built `BUILD_QT5=OFF` and no KF5 stack exists [measured: verify-res-branding qt5ct verdict; ports/core/kde/plasma-integration/build.sh:19]. **Recommendation, confirm with user:** keep `qt5ct` on its patched `kde` key (`ports/core/themes/qt5ct/kde-key.patch`), so `QT_QPA_PLATFORMTHEME=kde` gives Qt 6 plasma-integration and Qt 5 qt5ct.

- `kdos-theme qt5ct-colors` generates `/usr/share/qt5ct/colors/KDOS-<Accent>.conf`; `kdos theme` writes `~/.config/qt5ct/qt5ct.conf [Appearance] color_scheme_path=…`, `custom_palette=true`, `style=Fusion`, `icon_theme=KDOS`, fonts (merged keys only); skel ships bone. qt5ct's settings launcher is hidden (`NoDisplay=true` via a `.desktop` override in `/usr/share/applications`? — measure whether the port installs one).
- [ ] Gate: KeePassXC on the rig wears the accent after `kdos theme borland`.

#### 5.2.17 About page, os-release, Kickoff

- `fs/etc/os-release`: `LOGO=distributor-logo-kdos`, add `VARIANT="Plasma"`, `VARIANT_ID=plasma`, `DOCUMENTATION_URL="file:///usr/share/doc/kdos/index.html"`, `SUPPORT_URL` [measured: fs/etc/os-release:7; kinfocenter main.cpp:162-166 — a non-empty unknown LOGO suppresses the start-here-kde fallback].
- `/usr/share/kdos/xdg/kcm-about-distrorc [General] LogoPath=distributor-logo-kdos`, `Website=https://github.com/kunaldawn/kdos`, `UseOSReleaseVersion=true`, `ShowBuild=true`, `ExtraSoftwareData=/usr/libexec/kdos/about-slot;/usr/libexec/kdos/about-tree;/usr/libexec/kdos/about-accent;/usr/libexec/kdos/about-boxes` (`;`-separated [measured: main.cpp:172-176]). This section owns `about-accent`; the other three are specified in §6.
- Each script **always prints exactly two lines** (label, value), on every machine including a single-root one with no boot state and on error, and returns in well under 100 ms with no podman, network or disk wait: fewer than two lines `qFatal`s kinfocenter *and* System Settings, and each script runs twice synchronously on the GUI thread with a 30 s default timeout [measured: kinfocenter ThirdPartyEntry.cpp:11-49; verify-res-branding verdict]. Output: `Boot slot` / `A · 0.2 · confirmed` (or `one root`); `Accent` / `Bone`.
- Kickoff: `icon=start-here-kdos`, `menuLabel=Start` through layout.js, the plasmoidsetupscript and the icon alias (three layers) [measured: plasma-desktop applets/kickoff/main.xml:8-26].
- [ ] Gate: `testing/aboutcheck.sh` (new) runs each script under `LANGUAGE=en_US:C`, under a root with no `/boot/efi/EFI/kdos/bootstate`, and with `/var` read-only; asserts 2 lines and < 100 ms. Rig shot of System Settings → About this System shows the mark and both rows.

#### 5.2.18 plasma-welcome identity pages

`src/desktop/kdos-welcome` owns `intro-customization.desktop` (KDOS mark, one sentence, "Plasma by KDE" credit) and every extra page; the page order is owned by §6.5.2: 00 Accessibility, 10 Look, 20 Desk, 30 Your system, 40 Software and boxes, 50 Library, 60 Keys, 70 Privacy, 80 Help [measured: plasma-welcome v6.8.2 src/app.cpp:26-75; Controller.runCommand from page QML]. This section owns the content of two:
- `10-Look.qml`: 8 live accent swatches (runs `kdos theme <accent>`), light/dark/automatic, **the Phosphor mode toggle** (§5.4), the Terminus sub-option, Night Light on/off, reduced motion.
- `20-Desk.qml`: the D5 picker (§5.5.4) and the auto-tile checkbox (§5.5.3).
- [ ] Gate: rig `plasma-welcome --pages` shots of pages 10 and 20 on bone; each action's command runs through `Controller.runCommand` with an argument vector (no shell string).

#### 5.2.19 Where defaults live and how shadowing is avoided

- All KDOS defaults live in `/usr/share/kdos/xdg`, **after** `/etc/xdg`: `XDG_CONFIG_DIRS=/etc/xdg:/usr/share/kdos/xdg` (Fedora and Kubuntu do the same [measured: fedora-kde-settings env.sh; kubuntu kf5-settings]). `/etc/xdg` stays the administrator's, and a package update never collides with an edited `/etc` file (an A/B apply reports edited `/etc` configuration [measured: docs/kdos/04-programs/kdos-command.md:1072]).
- Cascade at run time: `~/.config` > `~/.config/kdedefaults` (session only) > `/etc/xdg` > `/usr/share/kdos/xdg` [measured: startplasma.cpp:421-428].
- `XDG_CONFIG_DIRS` must reach every reader, or it sees Breeze: (1) login shells — `fs/etc/profile.d/10-wayland.sh` (SDDM's Wayland session script runs a login shell [inferred; the login section measures it]); (2) the greeter — `/etc/sddm.conf.d/00-kdos.conf [Wayland] CompositorCommand` / `GreeterEnvironment`; (3) boxes — kdos-appbox's environment; (4) VM machines — `plasma-workspace/env/kdos-vm.sh` (sourced by startplasma [measured: startplasma.cpp:308-345]) prepends `/usr/share/kdos/xdg-vm` (screen locking and power timers off) when `kdos in-vm` (libkbase `kb_in_vm()` [measured: src/libs/libkbase/kbase.h:203]) is true.
- **The one sanctioned `/etc/xdg` writer** is the installer (§6.6): it writes the chosen accent and desk (and the keyboard layout, `kxkbrc`) as `/etc/xdg` overrides, which sit above `/usr/share/kdos/xdg` in the cascade. Every other shipped default of every section (discoverrc, powerdevilrc, startkderc, kwinrc, kcm-about-distrorc, konsolerc, the `ghns` key in kdeglobals, …) lives in `/usr/share/kdos/xdg`.
- pkexec-started GUI processes get a cleared environment and read `/etc/xdg` only [inferred]; KDE root actions run as KAuth helpers without a GUI, so this is accepted and listed in known-gaps.md.
- KDOS writes into `~/.config` only through §5.3 and §5.5, and only keys whose current value is KDOS-owned (P8). `/etc/skel` ships no KDE rc files: an applets file or kdedefaults in skel bypasses layout scripts and is rewritten at first login [measured: CachyOS survey; startplasma.cpp:438-445].
- [ ] Gate: `testing/xdgcheck.sh` (new) asserts that no packaged file and no file under `fs/` lands in `/etc/xdg` on the image (`testing/unowned.py`-style), with one allowlist: the files the installer writes (`kdeglobals`, `kxkbrc`, and the desk key's file), which must be absent from the image and present only on an installed disk; it also asserts that `env -i bash -lc 'echo $XDG_CONFIG_DIRS'` prints the two-element list.
- [ ] Docs: configuration.md and filesystem-and-ipc.md list `/usr/share/kdos/xdg` and the reader list; theming.md "Files you may edit, and files you may not" rewritten. Gate: `bash testing/docscheck.sh && python3 testing/anchorcheck.py` pass; a grep of the old surface name in each named page is empty.

### 5.3 `kdos theme <accent>`

One action switches everything. `kdos theme` stays in `src/system/kdos-tools` (C, no Qt); every external program runs through the argument-vector builder (no `system()`, no shell); D-Bus calls go through the same mechanism the tools use today or a basu call [inferred: which of the two kdos-tools uses is to be read from kdos.c].

**Inside a Plasma session** (`KDE_FULL_SESSION=true`), in this order:

1. Validate with `kcol_find()`; write the accent atomically to `~/.config/kdos/accent` (replaces `~/.cache/kdos/theme`).
2. `plasma-apply-lookandfeel -a org.kdos.<accent>.desktop -k` — colours, Plasma style, icons, cursor, decoration, splash, fonts; `-k` keeps automatic light/dark; no `--resetLayout`, so panels are untouched [measured: lnftool.cpp:98-106]. For a dark accent also `kwriteconfig6 --file kdeglobals --group KDE --key DefaultDarkLookAndFeel org.kdos.<accent>.desktop`, so the Quick Settings light/dark flip pairs this accent with paper. (`plasma-apply-colorscheme` is not used: it no-ops on an unchanged name.)
3. Wallpaper (the live-switch trap, §5.2.7): `org.kde.PlasmaShell /PlasmaShell evaluateScript` with a script that, for each desktop whose `[Wallpaper][org.kde.image][General] Image` starts with `file:///usr/share/wallpapers/KDOS-`, writes `KDOS-<Accent>`; then `kwriteconfig6 --notify --file kscreenlockerrc` for the lock-screen Image under the same guard.
4. Konsole: if `konsolerc [Desktop Entry] DefaultProfile` is a `KDOS-*.profile`, set it to `KDOS-<Accent>.profile` and `[LightDarkTheme] DarkThemeProfile=KDOS <Label>`; then for every `org.kde.konsole-*` service, each `/Sessions/N` whose `profile()` is a `KDOS *` profile gets `setProfile("KDOS <Label>")` [measured: konsole src/session/Session.h:765-772].
5. CLI tool configs kept as today: bat, micro, helix, neovim, delta, newsboat, aerc, fzf, tmux, btop, mc, yazi, starship, `LS_COLORS` (merged files unchanged: mc `ini`, `starship.toml`) [measured: kdos-command.md:296-305]; a running tmux re-reads its config.
6. qt5ct: merge `color_scheme_path` into `~/.config/qt5ct/qt5ct.conf` (§5.2.16).
7. If `kdos_crt` is loaded (`org.kde.KWin /Effects isEffectLoaded`), `reconfigureEffect kdos_crt` — new tint, one degauss pulse [measured: kwin src/org.kde.kwin.Effects.xml methods].
8. `kdos-power accent <name>` (wheel only, as today [measured: kdos-command.md:313-321]) writes `/etc/kdos/accent` (kdos-splash), `/etc/vtrgb` and `limine.conf` through `kdos-bootctl theme`, **and** the sddm user's `~/.config/{kdeglobals,plasmarc,kcminputrc}` generated for that accent's greeter-safe dark variant (§5.2.10), plus `/var/lib/sddm/kdos-background.png` → the accent's `images_dark` file. Failure prints `boot menu, splash and login screen unchanged (kdos-powerd did not answer)` and the rest stands.

**Outside a session** (tty, ssh): steps 1, 5, 6, 8, plus `kwriteconfig6 --file kdeglobals --group KDE --key LookAndFeelPackage org.kdos.<accent>.desktop` so the next login's startplasma applies it in Defaults mode [inferred from startplasma.cpp:430-445; measure].

**From System Settings** (the user picks a KDOS global theme on the Global Theme page, or flips light/dark): `kded-kdos` (kded6 module, KConfigWatcher on `kdeglobals [General] ColorScheme`) sees `KDOS<Accent>` differing from `~/.config/kdos/accent` and runs `kdos theme --from-plasma <accent>`, which skips step 2 (no loop). A non-KDOS scheme leaves KDOS state alone.

**What it stops writing** (code deleted from `kdos.c` and `kdos-theme`): the per-user GTK stylesheet, icon and cursor regeneration (`gtk.c`, `icons.c`, `cursors.c` hex-swap paths); the `[Colors:*]`/`[WM]` merge into `~/.config/kdeglobals` (`kde_palette`, kdos.c:2182-2253) and `TerminalApplication=foot`; qt6ct files; `~/.config/kdos-comp/themerc-override`; the `kdos-term` config; the wallpaper retint into `~/.cache/kdos/wallpaper.png`; the SIGHUP list (`kdos-shell`, `kdos-desk`, `kdos-notifyd`, `kdos-slit`, `kdos-res`, `kdos-comp`, `kdos-term`) [measured: kdos-command.md:296-321]; `--preview` (the Global Theme page has previews).

- [ ] Rewrite `kdos theme` per the steps above; `kdos theme list|next|prev` unchanged. Gate: rig sequence `kdos theme phosphor` → `--shot` → `kdos theme paper` → `--shot`; pixel-sample panel, Konsole background, wallpaper, a Dolphin selection, a GTK window; then reboot and `--shot` Limine, kdos-splash and SDDM in paper's greeter variant (bone).
- [ ] The accent watcher in `kded-kdos` (src/desktop, D3). Gate: pick Amber on the Global Theme page on the rig; within 2 s `~/.config/kdos/accent` reads amber and the wallpaper is amber.
- [ ] Selftest: `kdos theme` under a fake `$HOME` with a user wallpaper, a user Konsole profile and a user colour scheme leaves all three untouched (P8).
- [ ] Docs: kdos-command.md "What setting an accent does" and "Previews" rewritten to these steps; daemons.md `kdos-powerd` `accent` verb (greeter files); theming.md "Switching the accent" and "What repaints at once"; command-index.md. Gate: `bash testing/docscheck.sh && python3 testing/anchorcheck.py` pass; a grep of the old surface name in each named page is empty.

### 5.4 Phosphor mode

#### 5.4.1 What the one toggle does

`kdos phosphor on|off|status` (kdos-tools), surfaced on the Welcome Look page, as a KRunner verb, and as the `kdos_crt` entry under System Settings → Desktop Effects. No default chord (avoids an accidental full-screen flash). **On:**

1. Refuse unless `org.kde.KWin /Effects isEffectSupported kdos_crt` is true; print `phosphor mode needs GPU rendering (this session renders in software)`.
2. `kwinrc [Plugins] kdos_crtEnabled=true`, `kdos_unfoldEnabled=true`, `kdos_loginEnabled=true`, `kdos_quitEnabled=true`, `loginEnabled=false`, `sessionquitEnabled=false`; `loadEffect`/`unloadEffect` for each so it applies without relogin.
3. Retro accent pairing: if the accent is `bone` or `paper`, remember it in `~/.config/kdos/phosphor [State] PreviousAccent` and run `kdos theme phosphor`; amber, ice, norton, borland, perfect and phosphor are kept as chosen. **Confirm with user** that bone/paper → phosphor is the pairing.
4. If the Terminus sub-option is set: `fixed=Terminus (TTF),12`, Konsole profile font Terminus (TTF) 12, KSplash text in Terminus; the UI font stays Noto Sans (Terminus covers Latin, Greek and Cyrillic only [measured: prior report 6]).

**Off** reverses 2–4 and restores `PreviousAccent`. Default: off on every new user (D2).

#### 5.4.2 The CRT effect `kdos_crt` (decision: in-tree KWin plugin via a carried patch)

- A full-screen post-process is only possible as a C++ Effect overriding `paintScreen` with a per-output offscreen texture + framebuffer (RGBA8 for sRGB, RGBA16F otherwise), as `zoom` does; JS/QML effects are per-window only [measured: kwin 6.8.2 src/plugins/zoom/zoom.cpp:320-409; src/effect/effect.h:712; scripting/scriptedeffect.h:167-169; effect/offscreeneffect.h:26-66].
- **Version lock, both ways.** An out-of-tree effect's IID embeds `KWIN_PLUGIN_VERSION_STRING` = major.minor.patch and the loader refuses a mismatch ("was built for a different version of KWin and needs to be rebuilt") at every 6.8.x point release [measured: effect.h:1134; config-kwin.h.cmake:4; effectloader.cpp:298-303]. A built-in effect (`kwin_add_builtin_effect`) is a static plugin and skips that check [measured: effectloader.cpp:291-296; src/plugins/CMakeLists.txt:26]. The effect API (0.237) still moves between minors, so a patch rebase is needed per minor either way.
- **Decision: `ports/core/<kde shelf>/kwin/kdos-crt.patch`** (`Subject: Add the kdos_crt full-screen CRT effect`) adds `src/plugins/kdos_crt/{CMakeLists.txt,kdos_crt.{h,cpp},kdos_crt.frag,kdos_crt.kcfg,kcm/}` and one `add_subdirectory` line. It rebuilds with KWin, so it can never silently fail to load; the cost is a 1–8 h rebase per release. In parallel, propose upstream a generic "custom screen shader" effect (GLSL file + uniforms) after which KDOS's shader becomes data and the patch is dropped (D3).
- **Port the shader before D1 deletes kdos-comp:** the GLSL is `src/desktop/kdos-comp/src/kdos-crt-pass.c` (155 lines; uniforms `u_res`, `u_int`, `u_scan`, `u_curve`, `u_tint`, `u_fx` (degauss), `u_collapse`) with `kdos-crt.c` (1 213 lines) as the reference for timing and failure cooldown [measured: wc, grep this session]. The deletion task in the delete section must not land before this task.
- Behaviour: `supported()` false when `EglContext::isSoftwareRenderer()` [measured: src/opengl/eglcontext.h:62]; `blocksDirectScanout()` false while a fullscreen window is active and `Fullscreen=false` (games and video keep direct scanout); presets in `kwinrc [Effect-kdos_crt]`: `Strength=55`, `Scanlines=0`, `Curve=0`, `Fullscreen=false` = **Subtle** (today's kdos-comp defaults: crt 55, scanlines 0, curve 0 [measured: docs/kdos/04-programs/kdos-comp.md:153-156]); **Retro** = `Scanlines=60`, `Curve=30`. Degauss on a colour-scheme change is one pulse ≤ 400 ms (no more than three flashes per second, WCAG 2.3.1) [source: w3.org/TR/WCAG21 2.3.1]. Whole-screen collapse on `screenAboutToLock` and session quit lives here, because JS effects get no paintScreen and KWin gives effects no blank/DPMS signal [measured: effecthandler.h:955,962; verify-res-innovation missed].
- Costs stated in theming.md: the cursor is composited into the offscreen texture and is curved with the screen; direct scanout and the hardware cursor plane are lost while the pass runs [measured: workspacescene.cpp:389-417, :737-797; compositor.cpp:918-936]. HDR outputs take the RGBA16F path, where the shader maths differ: **refuse per output on a non-sRGB output** until measured. Whether Spectacle/ScreenShot2 captures include the pass is unmeasured — a task.
- [ ] Write the patch; `kfs.md` of the kwin port explains the patch (rule 6). Gate: kwin builds; on `make run-hw` or `vnc-shot.py --gl` (virgl renders the effect; the plain rig is software and never shows it [measured: verify-res-innovation verdict]) `kdos phosphor on` → `--shot` shows scanlines/bleed in Retro; on the plain rig `kdos phosphor on` prints the refusal and `isEffectSupported` is false.
- [ ] Measure: a Spectacle screenshot with the pass on (is the pass in it?); a fullscreen mpv with `Fullscreen=false` keeps direct scanout (`KWIN_DRM` debug log); record both in theming.md "What it costs".

#### 5.4.3 CRT motion (JS, no ABI tie)

`src/desktop/kdos-kwin-motion`: KPackage JS effects using `addFragmentShader`/`setUniform` [measured: scriptedeffect.cpp:786-815]:
- `kdos_unfold` — window open/close as a CRT unfold/fold.
- `kdos_login` — replaces KWin's `login` effect, which fades the `ksplashqml` window [measured: kwin src/plugins/login/package/contents/code/main.js]: the KSplash window closes with a power-on unfold.
- `kdos_quit` — power-down on logout/shutdown, replacing `sessionquit`.
Only what JS can do: per-window. Lock/blank collapse is in `kdos_crt` (§5.4.2). All use `animationTime()`, so `AnimationDurationFactor` scales them and 0 disables them; `animationsSupported()` is false on software rendering, so they switch themselves off there [measured: kwin login/main.js:14; verify-res-innovation verdict].
- [ ] Gate: `--gl` rig: a window open photographed mid-animation (`--sleep 0`); with `AnimationDurationFactor=0` the same shot shows the window settled.

#### 5.4.4 Accessibility and reduced motion

- Reduced motion (`kdeglobals [KDE] AnimationDurationFactor=0`, offered on the Welcome Look page and in the a11y page) stops degauss, collapse and every motion effect; the static pass may stay on.
- High-contrast: Phosphor mode never lowers the contrast floor of §5.2.1 — the pass's bleed and vignette are applied to pixels, so the measured ratio is the scheme's, not the screen's; Retro scanlines reduce effective contrast and are labelled as such on the toggle [inferred].
- Phosphor mode is not offered while a screen reader or zoom is active on the Welcome page's first visit (the a11y page comes first) [inferred].
- [ ] Docs: theming.md "The phosphor pass", "What it costs", "When it switches itself off", "Screenshots" rewritten for KWin; accessibility.md (reduced motion). Gate: `bash testing/docscheck.sh && python3 testing/anchorcheck.py` pass; a grep of the old surface name in each named page is empty.

### 5.5 Layouts (D5) and windows (D6)

#### 5.5.1 KDOS desk (default)

Template `org.kdos.desk.kdos` (`layout.js`, ~120 lines) [measured API: shell/scripting/panel.h:25-47; scriptengine_v1.h:32-61]:

| Property | Value | KDE default |
|---|---|---|
| Panel | bottom, `floating=false`, `opacity` opaque, height `2 * Math.ceil(gridUnit * 2.5 / 2)`, one per screen | floating [measured: panelview.cpp:134-137] |
| Widgets, left → right | `org.kde.plasma.kickoff` (icon `start-here-kdos`, `menuLabel=Start`, favourites below) · `org.kde.plasma.pager` (4 desktops) · `org.kde.plasma.icontasks` · `org.kde.plasma.marginsseparator` · `org.kde.plasma.systemmonitor.cpu`, `.memory`, `.net` (sparkline face, the meters strip) · `org.kde.plasma.systemtray` · `org.kde.plasma.digitalclock` (date shown, KDOS mono) · `org.kde.plasma.showdesktop` | kickoff, pager, icontasks, marginsseparator, (kimpanel by locale), systemtray, digitalclock, showdesktop [measured: plasma-desktop layout-templates/org.kde.plasma.desktop.defaultPanel/contents/layout.js:24-75] |
| icontasks | `launchers=applications:org.kde.konsole.desktop,applications:org.kde.dolphin.desktop,applications:firefox-esr.desktop,applications:org.kde.kate.desktop`; `groupingStrategy=1`, `showOnlyCurrentScreen=true`, `wheelEnablesVolume` per 6.6 | Konsole/Dolphin/Discover-less |
| Kickoff favourites | `preferred://browser`, `org.kde.konsole.desktop`, `org.kde.dolphin.desktop`, `systemsettings.desktop`, `org.kde.discover.desktop` (seed; whether Kickoff seeds KActivities favourites from this key is to measure) | `preferred://browser, kontact, systemsettings, dolphin, discover` [measured: kickoff main.xml] |
| Kimpanel | keep upstream's locale rule | same |
| Desktop | `org.kde.plasma.folder` on `~/Desktop`; wallpaper Image **not** set (follows the global theme default, §5.2.7) | same |

`~/.config/kdos/favorites` stays the box-warmup list and the home of the two-letter codes (§2.5, §6.5.1); no KDOS program parses Plasma's applets file. layout.js seeds the icontasks launchers from the same desktop ids that skel `favorites` lists, so the panel and the pre-started boxes agree on a fresh account [measured: fs/etc/skel/.config/kdos/favorites lists mc, btop, lazygit, foot, firefox-esr, org.kde.kate, gimp; its `foot` line becomes `org.kde.konsole` in the deletion section's rewrite].
- [ ] `testing/preflight.sh` check: every `applications:<id>.desktop` in the KDOS desk layout.js (all three templates) appears as a first word in `fs/etc/skel/.config/kdos/favorites`, and every `favorites` id resolves to a `.desktop` file some port installs. Prove it fails by adding an unlisted launcher. Gate: preflight passes. Docs: kdos-appbox.md "Warmup and collection" and configuration.md (favorites) say the panel launchers are seeded from the same list.

#### 5.5.2 Dock desk

Template `org.kdos.desk.dock`: top bar 1.5 gridUnit (Kickoff icon-only, `org.kde.plasma.appmenu`, spacer, systemtray, digitalclock); bottom floating panel, `alignment=center`, `lengthMode=fit`, `hiding=dodgewindows`, holding `icontasks` with the same launchers and `showdesktop`. KDE's own widgets only.

#### 5.5.3 Tiling desk (D6)

Template `org.kdos.desk.tiling`: top bar 1.25 gridUnit (Kickoff icon-only, pager with desktop names, `org.kde.plasma.windowlist`, spacer, system-monitor CPU/RAM text face, systemtray, digitalclock); applying it also enables the bundled auto-tiler.
- Auto-tiler: **Krohnkite** (anametologin fork on Codeberg, MIT, release 0.9.10.0 2026-10-25; the original esjeon repo is dead since 2023-08) [source: res-innovation DF15], new port, KWin/Script in `/usr/share/kwin/scripts/krohnkite`, shipped `kwinrc [Plugins] krohnkiteEnabled=false`. Polonium is not shipped.
- One toggle: `kdos tiling on|off` (`kwriteconfig6 --file kwinrc --group Plugins --key krohnkiteEnabled`, then `org.kde.KWin /KWin reconfigure`), also a KRunner verb and a Welcome Desk-page checkbox.
- For all desks, D6 defaults: floating windows; KWin edge and quarter snap on (`kwinrc [Windows] ElectricBorderMaximize=true`, `ElectricBorderTiling=true`) [measured: kwin src/kwin.kcfg:182-189]; custom tiles on Meta+T with per-desktop layouts (6.4) [source: kde.org 6.4.0 announcement].
- [ ] Gate: rig with tiling desk: open three Konsoles → `--shot` shows them tiled; `kdos tiling off` → next window floats.

#### 5.5.4 The picker and applying without clobbering

- Each `org.kdos.<accent>.desktop/contents/layouts/org.kde.plasma.desktop-layout.js` is one call: read `ConfigFile("kdosrc", "Desk")` key `Layout` (default `kdos`) and `loadTemplate("org.kdos.desk." + layout)` [measured: scriptengine_v1.h:32-61 (loadTemplate, configFile)]. So "Desktop and window layout" on the Global Theme page applies the desk the user picked, never a different one.
- Templates are not marked as panel templates, so they stay out of Add Panel [inferred from res-branding item 7; verify].
- Picker: Welcome `20-Desk.qml` on first run, three preview cards. `kdos desk <kdos|dock|tiling>` writes `~/.config/kdosrc [Desk] Layout`, backs up `plasma-org.kde.plasma.desktop-appletsrc` to `~/.local/share/kdos/desk-backup/<UTC-stamp>`, and runs `evaluateScript` that removes existing panels and calls `loadTemplate`; `tiling` also runs `kdos tiling on`, the others `off`.
- Clobber trap: applying a layout deletes every group of the applets file [measured: shellcorona.cpp:672-692]; the KDE Global Theme page leaves the layout out unless the user ticks it [measured: kcm.cpp:533-541]; `kdos theme` never passes `--resetLayout`. The picker states "replaces your panels" when panels were edited (appletsrc mtime newer than first login) and offers the backup path.
- Changeable later: Welcome (`plasma-welcome --pages`), KRunner `desk dock`, `kdos desk`.
- [ ] Gate: rig: pick each desk → `--shot`; edit a panel, pick again → backup file exists and the dialog text was shown.
- [ ] Docs: desktop.md (the three desks), window-model.md (floating + snap + auto-tile toggle), kdos-command.md (`kdos desk`, `kdos tiling`, `kdos phosphor`). Gate: `bash testing/docscheck.sh && python3 testing/anchorcheck.py` pass; a grep of the old surface name in each named page is empty.

### 5.6 Polish defaults

All values ship in `/usr/share/kdos/xdg/<rc>` unless noted. "KDE" is the 6.8.2 default.

| Area | KDE default | KDOS choice | Reason [evidence] |
|---|---|---|---|
| Click behaviour | double-click (`SingleClick=false`) | keep | KDE's own 6.0 reasoning holds [measured: workspaceoptions_kdeglobalssettings.kcfg:8-10] |
| Panel | floating, bottom | non-floating, opaque, bottom (§5.5.1) | KDOS plates are opaque; Fitts' edge kept; floating panel is a top review complaint [source: dedoimedo 6.5 review] |
| Global theme | Breeze Light | `org.kdos.bone.desktop` | D2 |
| Light/dark auto | off | off; offered on Welcome | P4 |
| Accent from wallpaper | off | off | accents are KDOS-named schemes [measured: shellcorona.cpp:1812] |
| UI / fixed fonts | Noto Sans 10 / Hack 10 | Noto Sans 10 / JetBrains Mono 10 | D7, B17 (§5.2.11) |
| Rendering | — | greyscale AA, hintslight | identical at fractional scales [inferred] |
| HiDPI | KScreen auto scale from EDID | keep; KDOS assets (splash, SDDM, wallpapers) checked at 1, 1.25, 2 | do not fight KScreen [source: res-innovation DF5] |
| Night Light | off | off; offered on Welcome; `kdos toggle night-light` invokes KWin "Toggle Night Color" | offering beats a surprise colour shift [source: res-innovation DF6] |
| Sounds | Ocean | `kdos` (inherits Ocean), login sound on | §5.2.14 |
| Notifications | near tray, DND when fullscreen | keep | KDE defaults good [source: 6.4/6.7 announcements] |
| Task manager | icons-only, grouped | icons-only, grouped, current screen only, accent underline, 4 pinned | §5.5.1 |
| Launcher | Kickoff, icon only | Kickoff with mark + "Start"; Kicker/Dashboard available | KDOS heritage |
| Virtual desktops | 1 | 4, one row, pager visible (`kwinrc [Desktops] Number=4`, `Rows=1`) | KDOS workspace heritage [measured: virtualdesktops.cpp:693] |
| Activities | service + Meta+Q UI | service kept, no UI, Meta+Q rebound (§5.8) | fewer concepts; Dolphin/Gwenview use the service |
| Screen edges | top-left → Overview; edge-drag tiling | keep | discoverable [measured: overviewconfig.kcfg:23-24] |
| Snapping / tiling | quick tile + Meta+T custom tiles | same + Krohnkite bundled disabled | D6 |
| Window placement / Alt+Tab | Centered / thumbnail_grid | keep | [measured: kwin.kcfg:117-136, :309-311] |
| Decoration | Breeze rounded | Breeze square, outline (§5.2.4) | D2 |
| Animation speed | `AnimationDurationFactor=1` | `0.5` (B18) | KDOS motion is short; speed is the reviewers' praise [source: res-innovation reviews]; CachyOS ships 0.25 |
| Blur | on (GPU) | off in the `kdos` Plasma style | opaque plates |
| Dolphin | Icons, Places, Home | Icons, folders first, previews incl. video (ffmpegthumbs ported), F4 → Konsole, same-disk move without prompt | [source: res-innovation DF9]; `dolphinrc` keys to measure |
| KRunner | Alt+Space, Alt+F2, Search | + Meta+Space; KDOS runners enabled | §5.8 |
| Baloo | content indexing on | file-name indexing only (`baloofilerc [General] only basic indexing=true` — key to verify); content offered on Welcome | first-login CPU storm [inferred; measure on rig] |
| Klipper | history kept across sessions | keep, Meta+V; password-manager-hinted entries are not stored | KeePassXC sets the hint [inferred; measure] |
| Screen locking | 5 min, grace 5 s | 10 min, grace 5 s, lock on resume; zero timers in a VM via `xdg-vm` | carries KDOS idle policy (dim 300 s, lock 600 s, off 900 s) [measured: fs/etc/skel/.config/kdos/comp.conf:222-224] |
| Power (dim/off) | PowerDevil | dim 5 min, screen off 15 min | same carry |
| KWallet | first-use wizard | kwallet-pam unlocks at password login (login section) | most-cited papercut [source: dedoimedo 6.5] |
| Get New… (KNewStuff) | on | off: `kdeglobals [KDE Action Restrictions][$i] ghns=false` | D10 [measured: kconfig src/core/kauthorized.h:57 GHNS] |
| User feedback | opt-in, off | off (see the defaults table of the plan) | D10 spirit |
| Welcome | shown at first login | shown, KDOS pages | §5.2.18 |
| KSplash | Breeze | KDOS (§5.2.8) | P1 |
| Startup | `startkderc [General] systemdBoot` | `false` | no systemd (rule 3 as amended) [measured: res-branding item 74] |

- [ ] Ship each rc file; `testing/defaultscheck.py` (new) reads every key above from the built image with `kreadconfig6` under `XDG_CONFIG_DIRS=/etc/xdg:/usr/share/kdos/xdg` and compares to this table (the table is the fixture). Gate: in `make check`.
- [ ] Docs: desktop.md and getting-started.md describe these defaults as the system's behaviour; configuration.md lists every file. Gate: `bash testing/docscheck.sh && python3 testing/anchorcheck.py` pass; a grep of the old surface name in each named page is empty.

### 5.7 Default applications (D8)

Rule: the KDE app by default, unless an already-ported app is clearly better. Today's KDOS defaults already pick several non-KDE apps [measured: fs/etc/xdg/kdos-mimeapps.list; docs/kdos/02-user-guide/applications.md:124,160,162,226,242].

| Role | Proposed default | KDE candidate | Reason |
|---|---|---|---|
| Browser | Firefox ESR 153.5 (ported) | Falkon (ported) | site compatibility and extensions; Falkon is upstream's default in kde-mimeapps.list [measured: plasma-desktop kde-mimeapps.list]. Plasma Browser Integration needs its extension preinstalled from the medium (D10, no store download) — task |
| Mail, calendar, contacts | Thunderbird 153.5 (ported) | KMail/Kontact/Merkuro (Gear, Akonadi) | no Akonadi database server in the default session; one app for mail+calendar+contacts |
| Video | Haruna 1.8.1 (ported, KDE) | — | KDE native, mpv engine |
| Music | Elisa (ported, KDE) — **or keep Strawberry** (today's default) | Elisa | Elisa: Kirigami, Baloo-fed, matches the look; Strawberry: stronger collection/tag tools. Recommendation Elisa; user decides |
| Images | Gwenview (ported) | — | |
| PDF, PostScript, DjVu | Okular (ported) | — | |
| E-books | Calibre viewer (ported) | Arianna (Gear) | today's default; format coverage |
| Office | LibreOffice 26.8 (ported) | Calligra (Gear) | format fidelity. Its build has `--disable-kf6` [measured: ports/core/office/libreoffice/build.sh:79]; enabling the KF6 VCL plugin gives KDE dialogs — task |
| Text and code | Kate (ported) | KWrite (built `BUILD_kwrite=OFF` [measured: ports/core/editors/kate/build.sh]) | one editor; keeps today's mapping |
| Terminal | Konsole | — | fixed by the brief |
| Files | Dolphin | — | fixed by the brief |
| Archives | Ark (ported) | — | |
| Calculator | KCalc (Gear, port) | Qalculate-Qt (ported) | **KCalc** (user ruling): KDE native; KRunner keeps libqalculate; Qalculate-Qt stays installed |
| Screenshots, recording | Spectacle (port) | — | |
| Remote desktop | KRDC (Gear) | Remmina (today) | **Recommendation Remmina**: RDP/VNC/SPICE/SSH in one; KRDC lacks SPICE [inferred] |
| Torrents | KTorrent (Gear) | qBittorrent (ported) | **qBittorrent** (user ruling); KTorrent stays installed |
| Disk images (.iso write) | ISO Image Writer (Gear) | — | replaces `kdos-burn.desktop` |
| `.zim` | Kiwix Desktop (ported) | — | offline library |
| `geo:` | Organic Maps (ported) | Marble | offline World.mwm [measured: res-innovation I20] |
| Passwords | KeePassXC (ported) | KeepSecret (Gear) | mature, browser integration |
| `tel:` | KDE Connect handler | — | upstream default |

Produces `/usr/share/kdos/xdg/kde-mimeapps.list` — the XDG config-dir copy wins over plasma-desktop's `/usr/share/applications/kde-mimeapps.list` [measured: plasma-desktop CMakeLists.txt:300-304 installs to KDE_INSTALL_APPDIR; XDG mime-apps spec orders config dirs before data dirs]. Content: upstream's list with `x-scheme-handler/http|https=firefox-esr.desktop`, `text/html=firefox-esr.desktop`, `x-scheme-handler/mailto=thunderbird.desktop`, `text/calendar=thunderbird.desktop`, `x-scheme-handler/geo=…organicmaps.desktop` (file id to read from the port), music → the confirmed player, the PDF/DjVu/PS → `okularApplication_*.desktop` ids, ebooks → `calibre-ebook-viewer.desktop`, zim → `org.kiwix.desktop.desktop`, rdp/vnc → Remmina, `application/x-cd-image` → ISO Image Writer, all `text/*` code types → `org.kde.kate.desktop`, the office types → `libreoffice-*.desktop` (carried from today's list).
- [ ] Gate: `testing/mimecheck.py` (new): every desktop id in the list exists in the built image's `/usr/share/applications` (a dangling id is a silent fallback); rig: `xdg-open` of a PDF, a URL, a `.zim` opens the expected app.
- [ ] Docs: applications.md (defaults table and "Default applications"), desktop.md. Gate: `bash testing/docscheck.sh && python3 testing/anchorcheck.py` pass; a grep of the old surface name in each named page is empty.

### 5.8 Shortcuts

KDE's defaults win the window-management chords; KDOS adds only chords KDE leaves free, after a collision check. KDOS defaults ship as `/usr/share/kdos/xdg/kglobalshortcutsrc` entries (`current` field) and as `X-KDE-Shortcuts` in KDOS `.desktop` files [measured: konsole/desktop/org.kde.konsole.desktop:11 pattern]; whether a config-dir `kglobalshortcutsrc` sets `current` for a user with no entry is to be measured on the rig before relying on it [inferred].

| Chord | Action | Owner | Evidence / collision |
|---|---|---|---|
| Meta | Kickoff | KDE | |
| Meta+Space | KRunner (KDOS command palette) | **KDOS add** | free in KDE; **collides with fcitx5's default group switch** (`EnumerateGroupForwardKeys`, Super+space) and KWin's shortcut filter runs before the IM filter, so fcitx5 would silently lose it [measured: fcitx5 5.1.22 globalconfig.cpp:109-115; kwin input.cpp:3196 vs 3219]. Mitigation: `/usr/share/kdos/xdg/fcitx5/config [Hotkey/EnumerateGroupForwardKeys] 0=Super+Shift+space` (whether fcitx5 reads XDG config dirs is to measure; else skel) |
| Alt+Space, Alt+F2, Search | KRunner | KDE | [measured: krunner/org.kde.krunner.desktop.cmake:71] |
| Meta+F1 | KDOS key card (KWin QML effect, integration section) | **KDOS rebind** | KWin default "Switch to Desktop 1" is Meta+F1 **and** Ctrl+F1; KDOS removes Meta+F1 from it, Ctrl+F1 stays [measured: kwin virtualdesktops.cpp:884] |
| Meta+/ | Help Center at the KDOS book | **KDOS add** | KDOS heritage (W-slash) [measured: rc.xml]; free in the grep of KWin/plasma-workspace defaults [measured: this session; rig check] |
| Meta+Return | Konsole | **KDOS add** | KDOS heritage (W-Return → foot) [measured: rc.xml:114]; Ctrl+Alt+T stays [measured: org.kde.konsole.desktop:11] |
| Meta+E | Dolphin | KDE | [measured: services fixture X-KDE-Shortcuts=Meta+E] |
| Meta+L | Lock | KDE | same as KDOS |
| Meta+Q | Close window | **KDOS rebind** | KDE default is activities management; no activity UI ships (§5.6); KDOS heritage (W-q) |
| Alt+F4 | Close window | KDE | |
| Meta+D | Show desktop | KDE | [measured: useractions.cpp:1013]; KDOS W-d launcher dropped |
| Meta+W / Meta+G | Overview / grid | KDE | [measured: overvieweffect.cpp:154,160] |
| Meta+T | Tile editor | KDE | [measured: tileseditoreffect.cpp:33]; KDOS W-t always-on-top dropped |
| Meta+Shift+T | Toggle auto-tiling (Krohnkite) | **KDOS add** | to check against Krohnkite's own defaults on the rig |
| Meta+Arrows | Quick tile | KDE | [measured: useractions.cpp:891-897]; matches KDOS SnapToEdge |
| Meta+PgUp / PgDown | Maximise / minimise | KDE | [measured: useractions.cpp:831,839] |
| Meta+Tab, Alt+Tab | Walk windows | KDE | [measured: kcms/tabbox/shortcutsettings.cpp:131-134] |
| Meta+1…9 | Task-manager entry N | KDE | KDOS W-1…9 (desktops) dropped; desktops on Ctrl+F1…F4 and Meta+Ctrl+Arrows [measured: virtualdesktops.cpp:775-778,884-887] |
| Meta+V | Klipper at pointer | KDE | [measured: klipper.cpp:137]; replaces KDOS W-C-v |
| Meta+. | Emoji selector | KDE | [measured: emojier .desktop:160]; replaces KDOS W-C-e |
| Meta+Alt+S | Screen reader | KDE | [measured: plasma-desktop kded/screenreader .desktop:140]; KDOS W-A-s (sticky keys) dropped |
| Meta+Alt+K / Meta+Alt+L | Keyboard layout next / last | KDE | [measured: keyboard_layout.cpp:47,55]; KDOS W-A-l (slow keys) dropped |
| Meta+= / Meta+- / Meta+0 | Zoom | KDE | [measured: zoom.cpp:56-66]; matches KDOS W-equal |
| Meta+Ctrl+I / U | Invert | KDE | [measured: invert.cpp:45,52]; **KDOS W-C-i (stay awake) dropped** — collision |
| Meta+Ctrl+Esc / Ctrl+Esc | Kill window / System Activity | KDE | [measured: useractions.cpp:1015] |
| Meta+Alt+P | Cycle panel focus | KDE | [measured: shellcorona.cpp:271] |
| Print | Spectacle (full screen) | KDE | Spectacle's own defaults to read from its 26.08 `.desktop` on the rig (Meta+Print, Meta+Shift+Print, recording chords) [inferred] |
| Shift+Print | Spectacle rectangular region | **KDOS add** | KDOS heritage (S-Print) [measured: rc.xml] — collision check |
| Alt+Print | Spectacle record region | **KDOS add** | KDOS heritage (A-Print → kdos-record) — collision check |
| Meta+Ctrl+Shift+N | Toggle Night Light | **KDOS add** | KDOS heritage (W-C-S-n); KWin "Toggle Night Color" has no default [source: res-innovation DF6] |
| (none) | Phosphor mode | — | deliberate: no chord (§5.4.1) |
| Media / volume / brightness keys | PowerDevil, plasma-pa | KDE | replace kdos-osd/kdos-mpctl |

Every other KDOS rc.xml chord (127 keybinds [measured: grep of fs/etc/skel/.config/kdos-comp/rc.xml]) is dropped with its cell surface (D1) or replaced by the KDE default above; the file is deleted with kdos-comp.

- [ ] `testing/chordcheck.py` (new, runs in the rig): reads every component's active shortcuts from `org.kde.kglobalaccel /kglobalaccel` (`allMainComponents`, then each component's `allShortcutInfos`), plus fcitx5's config, and fails on any chord bound twice. Prove it fails by binding Meta+Space in both KRunner and fcitx5. Gate: passes on the image.
- [ ] Gate: rig `--keys meta-space` → KRunner shot; `--keys meta-f1` → key card; `--keys meta-return` → Konsole; Ctrl+F1 still switches to desktop 1.
- [ ] Docs: desktop.md (keys), accessibility.md (a11y chords), the key-card data source, command-index.md. Gate: `bash testing/docscheck.sh && python3 testing/anchorcheck.py` pass; a grep of the old surface name in each named page is empty.


## 6. KDOS-native integration

### 6.1 Feature map

Every KDOS desktop feature, the KDE component that replaces it, the gap left, what KDOS builds,
and its priority. **P0** = required before the first image ships with the cell desktop deleted;
**P1** = before 1.0; **P2** = later or optional. "Stock" means a Plasma 6.8.2 / Gear 26.08 / KF6
component with KDOS configuration only. Source for the rows: `research.json` key `inv-features`
(169 items) as corrected by its verdicts and `missed` list; every row with a bracket cites the
correction, not the original claim.

Precondition for every row: none of the KDE components named here is ported yet. `discover`,
`kwin`, `plasma-workspace`, `krunner` (KF6), `khelpcenter`, `kinfocenter`, `plasma-welcome`,
`plasma-systemmonitor`, `ksystemstats`, `qcoro`, `kdialog`, `kup`, `kpmcore`, `partitionmanager`
and `accountsservice` are absent from `ports/core` [measured: `find ports/core -maxdepth 2 -name …`];
`ports/core/kde/` holds only 9 ports and `ports/core/kf6/` 67. Porting is section 4's work; this
section owns what KDOS writes on top.

#### First boot and install

| KDOS feature | KDE component | Gap | KDOS builds | P |
|---|---|---|---|---|
| Limine accent, `kdos-splash` CRT boot | — (pre-session) | hand-off to the greeter | Accent-sync kded (6.4) drives `kdos-bootctl theme`; black-frame hand-off [measured: boot-and-init.md:530-600] | P0 |
| kinstall TUI in foot (`fs/etc/skel/.local/share/applications/kdos-install.desktop`) | none fits (Calamares 3.4.3 has no Limine) | GUI | Kirigami KDOS Installer over kinstall (6.6) | P0 |
| First-run key tour (`kdos-keys --first-run`), welcome card | plasma-welcome | KDOS pages | Extra pages (6.5) | P0 |
| `apps-pending` offer at first login | — | | Notification whose action opens Discover on the pending set (6.2) | P1 |
| Post-update "what changed" | `plasma-welcome --post-update` | KDOS page | Page reading `/var/lib/kdos/update.state` | P1 |
| OEM first boot | plasma-setup 6.8.2 | providers, launch path | Init script + plasma-setup port (6.6) | P1 |
| Console banner, `kdos-getty`, tty2 recovery | — | none | Kept | — |

#### Login, lock, session

| KDOS feature | KDE component | Gap | KDOS builds | P |
|---|---|---|---|---|
| tty1 autologin → `kdos-desktop` (`kdos-login`, `login.conf`) | SDDM 0.21 + Wayland greeter (D9) | KDOS theme | §3.4 (SDDM), §5.2.10 (theme); autologin writer moves to `/etc/sddm.conf.d/kde_settings.conf` (6.6) | P0 |
| `kdos-users` autologin row, `kdos-powerd autologin` verb | sddm-kcm | none | Delete the verb | P0 |
| `kdos-lock` + setuid `kdos-checkpass` | kscreenlocker (PAM `kde`, `unix_chkpwd` 4755 present) | none | Lock QML in the KDOS theme (§5.2.9) | P0 |
| Fast user switching | Plasma userswitcher over SDDM + elogind | none | — | P1 |
| Wallet at login | ksecretd + kwallet-pam | `BUILD_KSECRETD=OFF` today | Rebuild kwallet ON (prerequisite of kwallet-pam, which execs `ksecretd`) [verdict res-session] | P0 |
| Supervised chrome, crash dialog (`kdos-child.c`) | ksmserver/plasma_session; KCrash | DrKonqi needs systemd 255 | §3.8 default (KCrash only, no crash dialog; A5) | P0 |
| Session restore (`kdos-session-save`) | ksmserver restore + KWin xdg-session-management-v1 | boxed apps | `kdos-appbox` live ledger + Plasma autostart relaunch (shutdown scripts run after `closeSession`) [measured: shutdown.cpp:157,187-205] | P1 |

#### Desktop and panel

| KDOS feature | KDE component | Gap | KDOS builds | P |
|---|---|---|---|---|
| Panel `kdos-shell`, one per output | plasmashell panel | layout | `layout.js` per desk (D5, §5.5) | P0 |
| Start button "Start" + mark | Kickoff `icon`, `menuLabel` [measured: kickoff/main.xml:8-15] | none | config | P0 |
| `kdos-start` (favourites, synonyms, two-letter codes, `[box]`, install-from-medium) | Kickoff | codes, medium rows, box badge | KRunner runner (6.5); `Keywords=` + `X-KDOS-Box` in launchers (6.3) | P0 |
| Quick-launch row | Task Manager pinned launchers | none | `layout.js` | P0 |
| Window list, badges, progress | Task Manager, Unity LauncherEntry | none | — | P0 |
| Hover previews (`kdos-thumb.c`) | Task Manager PipeWire thumbnails | none | — | P0 |
| Meters strip | systemmonitor applets + ksystemstats | none | Face/config (§5.5, §7.6.1) | P1 |
| Status wing: pager, volume, media | Pager, plasma-pa, Media Player (MPRIS) | mpd has no MPRIS | MPRIS bridge in `kdos-mpctl` (C, basu, 400-700 LOC) | P1 |
| Disk-full warning | freespacenotifier (all writable volumes) | thresholds | `/usr/share/kdos/xdg/freespacenotifierrc` `minimumSpacePercentage=10` | P1 |
| `kdos-status` overflow, stutter/restarts/energy panes | System Tray hidden items | restarts, energy | Notices (6.2), Energy page (6.4); stutter dropped | P1 |
| Tray (`tray.c`), `kdos-traymenu` | System Tray + statusnotifierwatcher | none | `hiddenItems` in `layout.js` | P0 |
| `kdos-tip` tooltips, layer focus | Plasma tooltips | none | Delete | — |
| `kdos panel toggle` | panel visibility | verb | `evaluateScript` (6.7) | P2 |
| `kdos-desk` icons + menu | Folder View containment | none | `layout.js` | P0 |
| Wallpaper retinted per accent | wallpaper package | per-accent images | §5.2.7 | P1 |
| `kdos-menu` (Applications/Places/System/window) | Kickoff, Dolphin Places, KWin window menu | none | Delete | — |
| `kdos-slit` dockapp column | — | **no equivalent** | **Dropped**: off by default today [measured: kdos-shell.md:2455] | — |
| `kdos-saver` attract mode | — (Plasma has no screensaver) | **no equivalent** | Optional QML wallpaper plugin "KDOS Attract" | P2 (deferred, no task in v1) |
| `kdos-teams` window list | System Monitor + KWin Kill Window | box column | ksystemstats plugin (6.4) | P1 |

#### Windows (D6)

| KDOS feature | KDE component | Gap | KDOS builds | P |
|---|---|---|---|---|
| 4 named workspaces | `kwinrc [Desktops] Number=4` | none | config | P0 |
| libkwm half/quarter tiles, grow/move to edge | KWin Quick Tile, Custom Tiling, Pack Window actions | none | `kglobalshortcutsrc` defaults | P0 |
| Window memory (`kdos-winpos.c`) | Window Rules "Remember"; 6.9 per-arrangement memory | default-on | KWin JS script `kdos-window-memory` (150-300 LOC) until 6.9 | P1 |
| Window tab groups (`kdos-group.c`) | — | **no equivalent** (KWin has no tabbing) | **Dropped** | — |
| App-first switcher | TabBox `OneWindowPerApplication` | none | config | P1 |
| Peek, fades, transitions, `motion = no` | KWin effects, Animation speed "Instant" | none | — | P0 |
| Box colour chip (`kdos-boxchip.c`) | KWin rule `decocolor` (force) [measured: rulesettings.kcfg:338-341] | none | One rule per box, wmclass exact = box app id (6.3) | P1 |
| Auto-tiling | Krohnkite KWin script | bundled, disabled | `kdos tiling on` (6.7, §5.5.3) | P1 |
| Phosphor pass (`kdos-crt.c`) | — | **no equivalent** | KWin effect `kdos_crt`, §5.4 (D2) | P1 |
| Stutter chip, `kdos-frames.sock` | showfps only | **no equivalent** | **Dropped** in v1; P2 KWin plugin exporting late frames | P2 |

#### Launching and boxes

| KDOS feature | KDE component | Gap | KDOS builds | P |
|---|---|---|---|---|
| Launch path `sh_launch` | KIO ApplicationLauncherJob | none | libkxdg stays for kdos-appbox | P0 |
| `genlaunchers` (`X-KDOS-Alien`) | KService | identity, icons, stale entries | 6.3 | P0 |
| Box Wayland identity | KWin security-context override | app id rewritten | 6.3 | P0 |
| Box confinement + `grant =` | KWin denies 7 globals to sandboxed clients | everything else open | KWin patch + bus proxy + private runtime dir (6.3) | P0 |
| `XDG_CURRENT_DESKTOP=KDOS`, `kdos-mimeapps.list`, `kb_desktop_prefix` | startplasma forces `KDE`/`plasma-` | rename | `KDE` everywhere; `/usr/share/kdos/xdg/kde-mimeapps.list` [measured: profile.d/10-wayland.sh:41, kdos-appbox/main.c:327, kb_fs.c:389] | P0 |
| Default apps per role | componentchooser + keditfiletype | "KDOS default" provenance tag | **Dropped** tag | P0 |
| Portals for boxes (`xdg-desktop-portal-kdos`, `-wlr`) | xdg-desktop-portal-kde | none | Delete both; `kde-portals.conf` | P0 |
| `X-KDOS-Term`/`Float`/`Size` picture entries | Konsole (sixel, OSC 1337, kitty APC) [measured: Vt102Emulation.cpp:476,640-651,984-986] | none | `Terminal=true`; remove the reader in `open.c` | P0 |
| Run-or-raise TUI chords | kglobalaccel desktop files + KWin script | Konsole has one app id per process [measured: qwaylandwindow.cpp:152-175] | `kdos-raise` KWin script matching caption (150 LOC JS) | P1 |
| Scratchpad terminal | Yakuake (`-DWITH_X11=OFF`) | none | port | P1 |
| `kdos-run` | KRunner shell runner | none | — | P0 |
| `kdos-palette`/`kdos-launcher` | KRunner + stock runners | routes, verbs, codes, chords, medium | KDOS runner (6.5) | P0 |
| Routes (`/etc/kdos/menu.conf`, 37) | — | | Served by the runner; repointed at KDE programs/KCMs | P1 |
| Idle box autostop (`kdos-box gc` → `kdos hey boxes`) | org_kde_plasma_window_management | C client | 6.3/6.7 | P1 |
| a11y inside a box | shared at-spi bus | proxy rule | `org.a11y.Bus` allowed in the box proxy (6.3) | P0 |

#### Files

| KDOS feature | KDE component | Gap | KDOS builds | P |
|---|---|---|---|---|
| `kdos-pick` chooser/browser | KDE file dialog, Dolphin | none | — | P0 |
| libkxdg verbs (Share/croc, Git status/lazygit, Verify) | Dolphin context menu | three verbs | KIO service menus (data only): share, lazygit, transcribe, verify | P1 |
| Places + `kdos places` | Dolphin Places (`user-places.xbel`) | zoxide group | **Dropped** zoxide group; verb rewrite (6.7) | P2 |
| `kdos-trash` | Dolphin `trash:/`, Trash applet | none | `kdos trash` CLI kept (same spec) | P0 |
| `kdos-peek` quick look | — | **no equivalent** | **Dropped** (Dolphin preview panel, Okular, Gwenview, Ark) | — |
| `kdos-pix` | Gwenview (ported) | none | — | P0 |
| `kdos-find` | KRunner + Baloo, KFind | Baloo `BUILD_INDEXER_SERVICE=OFF` | Rebuild ON + `/usr/share/kdos/xdg/baloofilerc` excludes (section 4) | P1 |
| Archives | Ark (ported) | none | — | P0 |
| `kdos thumb` | KIO thumbnailers (ffmpegthumbs, kdegraphics-thumbnailers) | none | **Deleted** with `thumb.c` (§2.3) | — |
| `kdos-connect` SMB | Dolphin `smb:/` | kio-extras `CMAKE_DISABLE_FIND_PACKAGE_Samba/libssh=ON` [measured: kio-extras/build.sh:25-26] | Flags off + ports `qcoro`, `kdsoap`, `kdsoap-ws-discovery-client`; scoped nftables rule for WS-Discovery UDP 3702 [inferred] | P0 |
| Sharing a folder over SMB | Dolphin Properties → Share (kdenetwork-filesharing, `net usershare`) | smbd is not running | smbd starts only when a usershare exists: boot starts it when `/var/lib/samba/usershares` holds one, and the first share, made through kdenetwork-filesharing, starts it through a polkit action; with no share left it is not started at the next boot (ruling 2, §8.5) | P1 |
| `kdos-mediad` toast | Device Notifier + Solid over udisks2 | session tracking | elogind (§3.2) | P0 |
| `.ktar` found on a stick | — | **no equivalent** | kded module offers import; Dolphin "Open with Discover" for `.ktar`/`.kpack` (6.2) | P1 |

#### Settings (`kdos-settings`, `kdos-style`, `kdos-time`, `kdos-users`, `kdos-firewall`, `kdos-backup`, `kdos-display`)

| KDOS feature | KDE component | Gap | KDOS builds | P |
|---|---|---|---|---|
| Control centre, 9 categories | System Settings | Boxes, Slots, Firewall | KCMs (6.4) | P0 |
| `kdos-style` 8 accents + font | Colors KCM (8 `KDOS<Accent>` schemes), Global Theme, Fonts | none | generator (§5.2.1, §5.2.2) | P0 |
| `kdos-time` | Date & Time KCM — **timedate1 only, no helper** [measured: dateandtime/main.cpp:43-105] | provider | §3.6 provider | P0 |
| Hostname | hostname1 consumers | provider | §3.6 | P1 |
| `kdos-users` | Users KCM (accountsservice, RUNTIME dep) | port | accountsservice `-Delogind=true -Dadmin_group=wheel` | P0 |
| Region & language | `region_language` | musl | `-DGLIBC_LOCALE_GEN=OFF` | P1 |
| `kdos-firewall` named services | plasma-firewall: firewalld/ufw only, systemctl + journal [measured: firewalldclient.cpp:19,63,579] | **no equivalent** | KDOS Firewall KCM (6.4) | P1 |
| Boxes page | flatpak-kcm (needs Flatpak) | **no equivalent** | Boxes KCM (6.4) | P1 |
| Idle/lock/power/session pages | PowerDevil, kscreenlocker, ksmserver KCMs | none | — | P0 |
| Shortcut editing (`rc.xml`) | Shortcuts KCM | defaults | `/usr/share/kdos/xdg/kglobalshortcutsrc` + `/usr/share/kglobalaccel/*.desktop` | P0 |
| Services ("not here") | KDE's systemd KCM n/a | **no equivalent** | Services KCM over ksvc | P2 (deferred, no task in v1) |
| Autostart | Autostart KCM | Plasma runs XDG autostart KDOS never ran | Audit `/etc/xdg/autostart` of every port (section 4) | P0 |
| `kdos-display`, `displays.conf` | KScreen KCM, Meta+P OSD | none | Delete `displays.conf` | P0 |
| `kdos-backup` (restic) | Kup: bup + rsync only [measured: kup README.md:8,18-19] | **no equivalent** | Kup restic backend (6.4) | P1 |
| `kdos-about` | kinfocenter About + `kcm-about-distrorc` | slot, keys, libc | ExtraSoftwareData scripts (6.4); Konami degauss dropped | P0 |

#### Updates, slots, store (`kdos-update`, `kdos-store`)

| KDOS feature | KDE component | Gap | KDOS builds | P |
|---|---|---|---|---|
| A/B state, trial, rollback | Discover SystemdSysupdateBackend (OFF by default), logout screen boot-entry text | **no equivalent for A/B on non-systemd** | `kdos-sysupdated` (6.2), System Versions KCM (6.4) | P0 |
| Update apply (hours of source builds) | — | policy | D11 delivery (6.2) | M8 (stage 1), P1 for stage 2 |
| mark-good before login [measured: rcS:188] | — | | Two-marker bless (6.2) | P0 |
| `kdos-store` catalogue, sets, `.ktar` import/export | Discover | **no equivalent** | KdosBackend (6.2) | P0 |
| Install-from-medium | — | **no equivalent** | KdosBackend origin `medium` + runner | P1 |
| CVE list, restarts | — | **no equivalent** | Notices + Discover update info (6.2) | P1 |
| Firmware (sudo only today) | Discover FwupdBackend | polkit | fwupd `-Dlogind` on, agent (6.2) | P1 |
| KDE Store add-ons | KNS backend, "Get New…" | D10: off | Disabled (6.2) | P0 |

#### Monitoring (`kdos-res`, `kdos-resctl`)

| KDOS feature | KDE component | Gap | KDOS builds | P |
|---|---|---|---|---|
| 11-page monitor, setuid `kdos-resctl` | plasma-systemmonitor + ksystemstats + libksysguard KAuth helper | none | Delete both (one setuid bit fewer) | P0 |
| Apps page naming apps | Applications page roots at `user@UID.service`, `/app-*` only [measured: libksysguard v6.8.2 application_data_model.cpp:20,165] | empty on KDOS | hidden by `pageOrder` (§4.1.2) until scope placement (§3.2) + aggregation patch (§7.6.1) pass (6.3.3) | P1 |
| Boxes page, per-app energy | — | **no equivalent** | ksystemstats `kdos` plugin + 2 pages (6.4) | P1 |
| GPU/sensors/drives/net/battery | ksystemstats plugins | none | — | P0 |
| Ctrl+Shift+Esc | Plasma default Ctrl+Esc | binding | `kglobalshortcutsrc` | P0 |
| OOM victim named by box | oom_notifier listens to systemd1 only [measured: oom_notifier/module.cpp:47-57,119-121] | **no equivalent** | protected-name rewrite (§3.7) + user notice (6.5) | P0 |
| Logs | KSystemLog (journald off) | port | — | P2 |

#### Terminal (`kdos-term`), capture (`kdos-shot`, `kdos-record`, `kdos-rec`, `kdos-ascii`)

| KDOS feature | KDE component | Gap | KDOS builds | P |
|---|---|---|---|---|
| Default terminal foot (4 places) | Konsole 26.08.1 | rewrite | `kb_terminal`, `xdg-terminal-exec`, `TerminalApplication`, chord [measured: kb_fs.c:411, kdos.c:2243-2247] | P0 |
| `kdos-term` + libkvt | Konsole: sixel, OSC 1337, kitty APC, OSC 777/99, OSC 133, mode 2026 [measured: Vt102Emulation.cpp:2354-2355] | none | Delete | P0 |
| Accent per terminal | Konsole `.colorscheme` + `Session.setProfile` [measured: Session.h:765-772] | generator | §5.2.12, §5.3 step 4 | P0 |
| `kdos-shot` (grim+slurp) | Spectacle | grim cannot run on KWin; QR mode | Spectacle defaults; `kdos-shot` **deleted** (B5, §2.3); QR mode **dropped** | P0 |
| `kdos-record` | Spectacle recording (6.8 audio+mic) | none | Delete; `Alt+Print` → Record Screen | P0 |
| `kdos-ascii` | — | **no equivalent** | **Dropped** | — |
| `kdos-rec` + whisper transcription | KRecorder | transcription | Service menu "Transcribe" (whisper-cli) | P1 |

#### Notifications, toggles, power

| KDOS feature | KDE component | Gap | KDOS builds | P |
|---|---|---|---|---|
| `kdos-notifyd`/`kdos-notify` | Plasma notifications | none | Delete | P0 |
| `kdos notify` dismiss/raise/count/list | no public API | **no equivalent** | **Dropped** verbs (6.7) | P1 |
| Toggles stay-awake/night-light/dnd | PowerDevil inhibit, KWin Night Light, Plasma DND | verb rewrite | 6.7 | P1 |
| Reminders | Notifications | autostart | Plasma autostart entry | P1 |
| Sounds (`kdos-sfx`) | Sounds KCM, canberra | theme | XDG sound theme `kdos` (§5.2.14) | P1 |
| Idle dim/lock/off, VM zero timers | PowerDevil + kscreenlocker | VM defaults | `plasma-workspace/env` script detecting a hypervisor | P1 |
| Lid, suspend, power keys | PowerDevil over elogind | none | kdos-powerd loses suspend/lid verbs (§3.5, §3.7) | P0 |
| Power profiles | PowerDevil → UPower.PowerProfiles | none installed | TLP `install-pd` + ksvc script (§3.7) | P1 |
| Brightness/OSD | PowerDevil (+DDCUtil) | flag | section 4 | P1 |
| Inhibit for X11 (`kdos-screensaver.c`) | `org.freedesktop.ScreenSaver` by Plasma | none | — | P0 |

#### Network, Bluetooth, audio, devices, printing (`kdos-net`, `kdos-netagent`, `kdos-bt`, `kdos-audio`, `kdos-osd`, `kdos-devices`, `kdos-disks`, `kdos-print`)

| KDOS feature | KDE component | Gap | KDOS builds | P |
|---|---|---|---|---|
| Wi-Fi/VPN/hotspot, secret agent | plasma-nm + kded agent | wallet | kwallet-pam (above) | P0 |
| Bluetooth + agent | bluedevil | port | — | P0 |
| Audio, volume OSD | plasma-pa | ALSA-card fallback pane | **Dropped** pane | P0 |
| Cameras/mics/scanners | Kamoso, plasma-pa, Skanlite | none | — | P1 |
| Disks (mount, unlock, SMART, format, write image) | Device Notifier, plasma-disks, Partition Manager, ISO Image Writer | `kdos-mountd` | Delete `kdos-mountd` with its consumers; re-point the `seat` tier (§3.5) | P1 |
| Printers | print-manager | port | — | P0 |
| Privacy lamps naming the box | plasma-pa mic indicator, cameraindicator (PipeWire only, bug 482614), portal SNI | box name, `/dev/video*` holders | Patch cameraindicator (upstream) + box name from pid (6.3) | P2 |
| Phone | KDE Connect (ported) | firewall name off | "Allow on this network?" prompt (6.5) | P1 |
| Remote desktop | krdp | `vnc` name describes wayvnc, which dies on KWin | Rewrite the `vnc` row in `kdos-powerd`'s table and daemons.md:420 to krdp | P2 |

#### Accessibility and input (`kdos-ime`, `kdos-a11y.c`, `kdos-a11ymon.c`)

| KDOS feature | KDE component | Gap | KDOS builds | P |
|---|---|---|---|---|
| Desktop not read by a screen reader [measured: accessibility.md:13] | Qt/Plasma AT-SPI + Orca | none | Accessibility page first in Welcome (6.5) | P0 |
| Sticky/slow/bounce, dwell, magnifier, zoom, a11y monitor | KWin plugins, `org.freedesktop.a11y` | none | — | P0 |
| Large pointer chord | Cursors KCM, shakecursor | chord | 50-LOC helper | P2 (deferred, no task in v1) |
| wl-kbptr | mousekeys | labelled grid | **Dropped** | — |
| wvkbd OSK | plasma-keyboard (+qt6-qtvirtualkeyboard) | ports | section 4 | P1 |
| fcitx5 + `kdos-ime` candidate window | KWin input method (`kwinrc InputMethod`), fcitx5-qt, fcitx5-gtk | Meta+Space collision (6.5) | Delete `kdos-ime` | P0 |

#### Help and desk accessories (`kdos-doc`, `kdos-keys`, `kdos-cal`, `kdos-calc`, `kdos-note`, `kdos-chars`, `kdos-contacts`, `kdos-clip`, `kdos-openwith`, `kdos-prompt`, `kdos-burn`, `kdos-verify`)

| KDOS feature | KDE component | Gap | KDOS builds | P |
|---|---|---|---|---|
| `kdos-doc`, F1 on 8 surfaces | KHelpCenter | book entry | `share/khelpcenter/plugins/kdos.desktop` (6.5); the `doc:/port:/reason:/man:/key:` link kinds **dropped** | P1 |
| `kdos-keys` card | Shortcuts KCM | no overview sheet in Plasma | Welcome Keys page + runner chord search + KWin key-card effect (§5.8, §7.5.2) | P1 |
| `kdos-cal` with khal marks | Digital Clock calendar | khal events | P2 CalendarEvents plugin, else **dropped** marks | P2 |
| `kdos-calc` | KRunner calculator (libqalculate), KCalc | none | — | P1 |
| `kdos-note` | Notes widget | none | — | P1 |
| `kdos-chars` | KCharSelect, characters runner, Emoji Selector | none | — | P1 |
| `kdos-contacts` (khard) | Merkuro Contacts (ported, B6) | none | — ; khard CLI kept | P0 |
| `kdos-clip` | Klipper (Meta+V) | none | — | P0 |
| `kdos-openwith` | KIO Open With dialog | `open.c` fallback | 6.7 | P0 |
| `kdos-prompt` | kdialog | port | 6.7 | P1 |
| `kdos-burn` | K3b (ported) | none | — | P1 |
| `kdos-verify` | Dolphin checksums | par2, list files | "Verify" service menu: the file against its sidecar checksum (a `.sha256` beside it, or the `SHA256SUMS` of its directory) and, where a `.sig` is beside it, its KDOS signature against `/etc/kdos/keys` (ruling 9, §8.5) | P1 |
| `kdos why/explain/oracle` | — | none | CLI kept; optional runner verb | P2 |

#### Features with no KDE equivalent

Built by KDOS: A/B slots, trial and rollback, session-confirmed boot; signed binary and image
updates; the box store (catalogue, sets, `.ktar`, medium, pending offer); box profiles and grants;
box identity in tasks, monitor, OOM notices and privacy lamps; per-app energy; the named-service
firewall; restic backup; the 8-accent chain from firmware to apps; the Phosphor pass; two-letter
codes and routes; run-or-raise TUIs; `kdos` verbs in search.

Dropped, with the reason in the row above: window tab groups, `kdos-slit`, `kdos-ascii`,
`kdos-peek`, the `kdos-doc` link kinds, the zoxide places group, `kdos notify`
dismiss/raise/count/list, the default-app provenance tag, the ALSA fallback pane, wl-kbptr, the
stutter chip (P2 revisit), `kdos-contacts` window, the Konami degauss, `kdos-shot` with its QR
mode, `kdos thumb`.

### 6.2 Software and updates

#### 6.2.1 Discover configuration (D10)

Discover 6.8.2 is ported with these flags [measured: discover v6.8.2 `CMakeLists.txt:98,103`,
`libdiscover/backends/CMakeLists.txt:33-101`]:

| Flag | Value | Why |
|---|---|---|
| `BUILD_SystemdSysupdateBackend` | `ON` | default OFF; OS updates (6.2.2) |
| `BUILD_FwupdBackend` | `ON` | firmware |
| `BUILD_FlatpakBackend`, `BUILD_SnapBackend`, `BUILD_AlpineApkBackend` | `OFF` | default ON; D10 |
| `BUILD_PackageKitBackend`, `BUILD_RpmOstreeBackend`, `BUILD_HoloBackend`, `BUILD_DummyBackend`, `BUILD_AppStreamPreviewBackend` | `OFF` | not KDOS |
| `CMAKE_DISABLE_FIND_PACKAGE_KF6NewStuff` | `ON` | KNSBackend is built whenever Attica+NewStuff+AppStreamQt are found (backends/CMakeLists.txt:38-40) |
| `CMAKE_DISABLE_FIND_PACKAGE_KF6UserFeedback` | `ON` | Default A8: telemetry not ported |
| `WITH_KCM`, `WITH_NOTIFIER` | `ON` | Software Update page; tray notifier |

Prerequisites: `qcoro` port (REQUIRED, discover `CMakeLists.txt:59`); `appstream` rebuilt
`-Dqt=true -Dcompose=true` [measured: ports/core/xdg/appstream/build.sh:19-20 sets both false].

Shipped configuration:

- `/usr/share/kdos/xdg/discoverrc`, `/usr/share/kdos/xdg/PlasmaDiscoverUpdates` (B10: KDOS
  defaults sit after `/etc/xdg`, §5.2.19): `UseUnattendedUpdates=false` (an
  update is never applied unasked) [inferred key names: measure in `notifier/UnattendedUpdates.cpp`
  and the KCM's kcfg].
- KNewStuff off system-wide: `/usr/share/kdos/xdg/kdeglobals [KDE Action Restrictions][$i]
  ghns=false` [measured by §7.4.4: knewstuff 6.30.0 src/widgets/action.cpp:34-42]. Gate: no
  "Get New…" button in Global Theme, Wallpapers, Colors, Plasma Style, Splash, Konsole on a rig shot.
- The curated offline add-on set ships as the pack `kdos-addons` (wallpapers, colour and Konsole
  schemes, KWin scripts, a few plasmoids) shown by the KdosBackend under category "Add-ons";
  §7.4.4 owns its contents.
- Live medium: `PlasmaDiscoverUpdates RequiredNotificationInterval=-1` [source: KDE Linux
  `mkosi.extra/live/usr/lib/live-setup:37-80`].

Network rule: Discover itself downloads nothing at run time. The only network paths are the ones
`kdos update fetch`, `kpkg fetch`/`kpkg binhost` and fwupd already have, each started by a user
action or the existing nightly check timer. `kdos-sysupdated` `CheckNew` reads cached state
(6.2.2). fwupd's LVFS remote: measure whether `FwupdBackend` refreshes metadata on start; if it
does, ship `/etc/fwupd/remotes.d/lvfs.conf` with automatic refresh off so refresh happens only on
an explicit Discover "Check for updates" [inferred: measure FwupdBackend.cpp].

#### 6.2.2 kdos-sysupdated: org.freedesktop.sysupdate1 over the A/B slots

New root daemon `src/daemons/kdos-sysupdated` (C on basu, ported as `ports/core/base-libs/basu`),
supervised by ksvc (`fs/etc/init.d/NN_sysupdated.sh`), owning `org.freedesktop.sysupdate1` on the
system bus. It implements only what Discover calls [measured: SystemdSysupdateBackend.cpp:48-53
(Ping), :133-199 (ListTargets, GetAppStream, GetVersion, CheckNew), SystemdSysupdateTransaction.cpp
:35-123 (Acquire then Install), :53-80 (Progress only on the Acquire job)]. It also serves
`io.kdos.Slots1` for the System Versions KCM (`kcm-kdos-system`). Owning a systemd-shaped D-Bus name is not systemd
under D3 (no systemd init, units or supervisor); the plan states it in principles.md.

| sysupdate1 call | KDOS meaning |
|---|---|
| `Manager.ListTargets` | one target `("host", "kdos", /org/freedesktop/sysupdate1/target/host)` |
| `Target.GetAppStream` | `file:///usr/share/kdos/update/io.kdos.system.metainfo.xml`, regenerated by the daemon from `update.json` and the new tree's release notes. **Must be non-empty** or Discover drops the target [verdict res-session] |
| `Target.GetVersion` | running slot's version (`/etc/kdos-release` + slot letter) |
| `Target.CheckNew` | cached `/var/lib/kdos/update.json` (written by the existing nightly `kdos update check --fetch --out`); refresh at most hourly and only the signed index, never sources; empty when offline or nothing is behind. Returns a version only when the whole set is binary-coverable (stage 1) or an image exists (stage 2). Until M8 (§8.3) it answers empty on a release image, and Discover's Updates page shows only fwupd |
| `Target.Acquire` | **all long work**: fetch tree + packages/deltas (stage 1) or image/delta (stage 2), install into the inactive slot, `kpkg check --all`. `Job.Progress` = items done / total, 0-100 |
| `Target.Install` | short: copy `/ports` into the slot, `kdos-bootctl deploy`, `kdos-bootctl try <slot>`. No progress is shown for it |
| `Job.Cancel` | stops Acquire between packages; the slot stays untried |
| `Describe`, `List`, `Vacuum`, `ListJobs` | minimal correct answers; Discover never calls `ListJobs` |
| `GET http://localhost:3129/v1/updatesize` | **not served**; Discover shows size 0 [measured: SystemdSysupdateResource.cpp:98] |

Traps and mitigations:

- A Discover restarted during Acquire does not reattach (it never calls `ListJobs`). The KDOS
  notifier plugin shows "Update in progress: N/M" from `io.kdos.Slots1.State` so the job stays
  visible.
- Every method must accept interactive polkit authorisation. Action ids copied from systemd's
  `org.freedesktop.sysupdate1.policy` [inferred: measure systemd v258 `src/sysupdate/`], granted
  `auth_admin_keep` to the active session; `io.kdos.slots.try` the same.
- The API is unstable upstream (systemd 260 broke Discover once [source: pointieststick
  2026-03-31]): the daemon is tested against the 6.8.2 XML vendored into its test dir, and every
  Plasma bump re-diffs `org.freedesktop.sysupdate1.xml`.
- Restart: Discover's reboot goes through login1 → elogind (§3.2). The tray notifier sees no
  sysupdate updates: it loads only `discover-notifier/` BackendNotifierModule plugins [measured:
  notifier/BackendNotifierFactory.cpp:24]. KDOS ships `kdos-discover-notifier` (C++,
  `BackendNotifierModule` in `src/desktop/kdos-discover`, ~250 LOC, version-locked to
  libdiscover): it calls `CheckNew` per sysupdate1 target for `hasUpdates`, reads
  `hasSecurityUpdates` from `kdos cve --json`, and takes `needsReboot` from
  `io.kdos.Slots1.State` (a slot armed by `try`). The sysupdate1 half is proposed upstream as a
  `SystemdSysupdateNotifier` in Discover's tree (§7.6.3); once merged, only the `needsReboot`
  from `io.kdos.Slots1` stays KDOS-specific. This plugin plus Discover's notifier plasmoid is the
  D4 update tray; a separate KDOS plasmoid is built only if the plugin cannot show trial state
  (gate below).
- Host test without root: Discover uses the session bus when `DISCOVER_TEST_SYSUPDATE=1`
  [measured: SystemdSysupdateBackend.cpp:238-241]; `kdos-sysupdated --session --root <fixture>`
  serves it there.

#### 6.2.3 Update delivery (D11)

Measured state: kpkg already has Ed25519 signing (Monocypher, `src/libs/libksig`), a signed binhost
index with exact arch/build-config/recipe-hash matching, and `zstd --patch-from` deltas over
uncompressed archives that are verified by hash against the signed index, so need no signature
[measured: packaging.md:1154-1292]. `kdos update` reads no binhost and builds from source
[measured: packaging.md:1296-1299]. No binhost is published [measured: known-gaps.md:1119].

Stage 1, signed binhost (stepping stone):

- Traps that make today's binhost unusable for updates, each a task below:
  1. A client must run `kpkg binhost` with the indexing machine's `CC CFLAGS CXXFLAGS LDFLAGS
     KDOS_TARGET`, or every package reports `different build config` [measured:
     packaging.md:1191-1193]. Fix: `kdos-sysupdated` runs inside the slot chroot with the
     environment from the slot's `/etc/kdos/chroot.env`, which the image ships.
  2. The index phase runs with `PORT_REPO=/ports/core` alone, so every `src/` port gets an empty
     `E:` and never matches [measured: packaging.md:1209-1211]: all KDOS programs would always
     build from source. Fix: index with `src/` on `PORT_REPO`.
  3. A port installed into the running slot is absent from the other slot's package database, so
     the next update into that slot drops it [inferred from packaging.md:1327-1328: "behind" is
     computed from the slot's database]. Fix: a world set `/var/lib/kdos/world` (explicit installs)
     applied to the inactive slot by every Acquire.
  4. `/home`, `/etc` and `/var/lib/kdos/packs` live inside each root slot as far as the docs show
     (slot B is "a copy of the system", installation.md:564-571), so user data written after the
     copy is missing after a switch. Measure; fix by a shared volume (section 6.6 installer task).
     Packs keep one manifest, in the shared store (`/home/.kdos/packs`, bound into each slot at
     `/var/lib/kdos/packs`), never one per slot, so both slots see the same installed packs and a
     switch neither drops nor duplicates one (ruling 3, §8.5).
- Release pipeline: `make build KDOS_MAKE_BINHOST=1` → `kpkg index --sign` with the release key →
  `kpkg delta` against the previous release's packages → publish as sharded releases
  `binhost-<version>-NN` (01, 02, …, each under the release-asset limits) beside the ISO, with one
  signed index naming the shard that holds every package and delta, its public key in
  `/etc/kdos/keys` (host keyring) at image time (ruling 4, §8.5).
- Client: `kdos update apply` default = binhost; any package not matched aborts Acquire before the
  slot is touched with "N packages need a source build: `kdos update apply --source`"; `CheckNew`
  therefore offers only fully binary updates. `KPKG_REQUIRE_SIG=1` for binhost installs.

Stage 2, signed whole-slot images (end state, KDE Linux model):

- Release produces one root image per version (filesystem-agnostic payload: a reproducible
  uncompressed tar or EROFS of the slot root, zstd-compressed, hash in a signed manifest), plus a
  `zstd --patch-from` delta from each of the previous two versions (the inactive slot holds n-1 or
  n-2). Reuse kpkg's delta rule: deltas unsigned, result hash-checked against the signed manifest.
- Acquire: fetch manifest, verify, fetch delta (else full image), rebuild, mkfs the inactive slot
  (keeping its UUID via `set-slot`), unpack, carry `/etc` merge rules, `kpkg check --all`.
- Decision task (measure first): payload format (tar vs EROFS + writable overlay) given that KDOS
  slots are writable roots on ext4/xfs/btrfs/f2fs chosen at install [measured:
  boot-and-init.md:397-407].
- Packs and boxes are untouched by slot images; under stage 2 Discover's ports lane becomes
  read-only (what the image contains) and new software arrives as packs/boxes.

Source fallback: `kdos update apply --source` keeps today's path (chroot builds into the inactive
slot); a GUI never starts it.

#### 6.2.4 Session-confirmed boot (bless) and "boot previous slot"

This subsection is the one design of the two-marker bless; §3.1 (rcS row and task) and §7.3.6
point here. Both markers are root-owned files under `/run/kdos/bless/`; no root daemon reads a
file in a user's runtime dir.

- `rcS` stops running `kdos-bootctl mark-good` [measured: fs/etc/init.d/rcS:187-189] and instead
  writes `/run/kdos/bless/userspace`.
- Marker 2 `/run/kdos/bless/graphical` is written only by `kdos-sysupdated`, on
  `io.kdos.Slots1.Bless()`. The caller is the Plasma autostart helper `kdos-session-good`
  (`/etc/xdg/autostart/kdos-session-good.desktop`, `X-KDE-autostart-phase=2`, `OnlyShowIn=KDE`,
  `Exec=kdos-notice --bless`; 6.5.2), which calls `Bless()` after `org.kde.plasmashell` has been owned and answering for 60 s. The
  daemon accepts the call only from a process in the active elogind session on seat0 (polkit
  action `io.kdos.slots.bless`, `allow_active=yes`, `allow_any=no`). On a machine where nobody
  logs in, the daemon writes the marker itself after `sddm-greeter` and its `kwin_wayland` stayed
  up 60 s. A machine whose inittab starts no SDDM needs marker 1 only.
- `kdos-sysupdated` runs `mark-good` only when **both** markers exist, checked explicitly with
  AND. KDE Linux's `.path` unit probably fires on either marker [verdict res-session]: do not copy it.
- Rollback is the existing attempt counter: `kdos-bootctl try` arms `attempts = 1` when UEFI
  BootNext is available and `TRY_ATTEMPTS` = 3 menu-led boots otherwise [measured:
  src/system/kdos-tools/bootctl.c:107, :2247-2253]; a trial that never reaches mark-good is
  rolled back by `select` once the attempts are spent (bootctl.c:1868-1869).
- Trial notice: "Running 0.2.14 on trial; it is kept after this session" (notifier plugin).
- Logout screen "When restarted, the computer will switch to %1" [measured: shutdowndlg.cpp:146-190,
  LogoutControls.qml:166]: `kdos-bootctl try` writes `/run/systemd/reboot-to-boot-loader-entry`;
  elogind runs with `SYSTEMD_REBOOT_TO_BOOT_LOADER_ENTRY=1` [measured: elogind logind-dbus.c
  :3843-3867]. Plasma runs `bootctl list --json=short` by bare name [measured:
  shutdowndlg.cpp:189-191]: carry `ports/core/kde/plasma-workspace/kdos-bootctl-entry.patch`
  (`Subject: logout-greeter: list boot entries with kdos-bootctl`) and add `kdos-bootctl list
  --json=short` printing `[{"id":"kdos-a","showTitle":"KDOS 0.2.13 (slot A)"},…]`. No binary named
  `bootctl` ships.
- "Start previous version next time": System Versions KCM `kcm_kdos_system` (6.4) → `io.kdos.Slots1.Try(slot)` →
  `kdos-bootctl try <slot> 1`.

#### 6.2.5 KdosBackend (ports, packs, boxes) and AppStream

`src/desktop/kdos-discover/` (C++, libdiscover internal API, rebuilt per Plasma minor), modelled
on the backend API's 11 pure virtuals [measured: AbstractResourcesBackend.h:144-339].

| Origin | Resources | Install / remove | Metadata |
|---|---|---|---|
| `kdos-packs` | catalogue packs (73 `app` rows) and installed packs | `kdos-appbox install`/`uninstall`, progress from its output | generated `kdos-packs.xml.gz` from pack metadata + icon |
| `kdos-store` | Debian store lane | `kdos-appbox` store lane | Debian trixie DEP-11 `Components-amd64.yml.gz` (8,046,370 bytes) shipped as a hashed source, offline [measured: res-session] |
| `kdos-boxes` | installed boxes (state, accent, permissions summary from 6.4) | stop/remove via `kdos-box` | live |
| `kdos-ports` | host ports with a desktop entry or metainfo | stage 1: `kpkg binhost` into the running slot + world set; stage 2: read-only | `appstreamcli compose` over `build/fs` per package in `70_image`, origin `kdos-ports`, `pkgname` = kpkg name |
| `medium` | catalogue sets on an attached KDOS medium | import lane | medium index |
| local file | `.ktar`, `.kpack` opened from Dolphin | import lane | file metadata |

Catalogues install to `/usr/share/swcatalog/{xml,yaml,icons}/`. Each box resource carries a
"Runs in a box" badge and links to the Boxes KCM. `apps-pending` becomes a Discover "Selected
during install" list opened from the first-login notification.

#### 6.2.6 Tasks

- [ ] Port `qcoro` 0.13.0; rebuild `appstream` `-Dqt=true -Dcompose=true`. S. Gate: `kpkg info
  appstream` lists `libAppStreamQt.so`; `python3 testing/kfscheck.py` passes (kfs.md explains both).
- [ ] Port Discover with the 6.2.1 flags; ship the configuration files. M. Gate: `readelf -d` of the
  backend dir shows only `systemdsysupdate` and `fwupd` plugins; rig shot of Discover with no
  Flatpak/Snap/KNS sources.
- [ ] Write `kdos-sysupdated` (≈1,000 LOC C + polkit policy + D-Bus conf). L. Gate: a selftest
  driving it on the session bus with `DISCOVER_TEST_SYSUPDATE=1` against a fixture slot; `busctl
  call` sequence Ping/ListTargets/GetAppStream/CheckNew/Acquire/Install returns the documented
  answers. Docs: new section in `04-programs/daemons.md`; `03-architecture/packaging.md` "Updating
  a machine"; `06-reference/filesystem-and-ipc.md`; `command-index.md`.
- [ ] `kdos-discover-notifier` plugin; file the `SystemdSysupdateNotifier` MR upstream (§7.6.3).
  M. Gate: rig: drop a newer `update.json`, and within one check interval the tray shows the
  update (cropped shot); after Install it says "Restart to update"; the MR URL is recorded in the
  kdos-discover `kfs.md`. If trial state cannot be shown, open a task for a KDOS plasmoid.
- [ ] Stage 1 fixes 1-3 (chroot env, `src/` indexing, world set). M each. Gate: on the rig, `kdos
  update apply` from a published `binhost-<v>-NN` set installs every package by binhost, zero source
  builds, in minutes; `kpkg binhost --dry-run` reports no `different build config`. Docs:
  packaging.md binhost + updating sections, kdos-command.md `kdos update`, known-gaps.md (remove
  "There is no public binary host" once published), how-kdos-differs.md#updates.
- [ ] Measure which paths are per-slot (`/home`, `/etc`, `/var/lib/kdos`); write the shared-volume
  design. S (measure) + M. Gate: an A/B switch on the rig keeps a file written to `~` after the
  slot copy.
- [ ] Release pipeline: binhost publish + deltas in `ports/publish --release`, sharded into
  `binhost-<version>-NN` releases. M. Gate: the shards together hold every package and
  `*.kdelta` the signed `kdos-binhost.idx` (with its `.sig`) names, each entry naming its shard;
  `kpkg apply-delta --expect` succeeds for every delta.
- [ ] Stage 2 design decision (payload, `/etc` merge, delta bases) then implementation. XL. Gate: rig
  update from image N-1 to N in under 5 minutes, rollback by power cut during Acquire leaves slot
  untried.
- [ ] Two-marker bless (6.2.4): remove `mark-good` from rcS; `io.kdos.Slots1.Bless()` + polkit
  action; `kdos-session-good` autostart helper. S. Gate: a trial slot with plasmashell replaced
  by `/bin/false` is rolled back by kdos-bootctl's attempt counter (after 1 boot under UEFI
  BootNext, after 3 menu-led boots otherwise; `kdos-bootctl status` names the old slot active),
  and a healthy trial is marked good only after both files exist under `/run/kdos/bless/`. This
  is the gate §3.1 and §7.3.6 cite. Docs: boot-and-init.md#counting-and-confirming (rewrite
  "runs at the end of rcS"), installation.md#ab-root-slots step 3, kdos-command.md#kdos-bootctl.
- [ ] `kdos-bootctl list --json=short` + plasma-workspace patch with `Subject:`. S. Gate: logout
  screen shot showing "will switch to KDOS … (slot B)".
- [ ] KdosBackend with the six origins. L (2.5-4k LOC). Gate: rig shots: install a catalogue pack,
  import a `.ktar` from Dolphin, remove a box; QtTest of resource mapping on fixtures. Docs:
  packs-and-boxes.md, kdos-appbox.md (store section), applications.md.
- [ ] `appstreamcli compose` step in `70_image` for ports, packs, store lane. M. Gate: `appstreamcli
  status` on the image lists the three origins with zero errors.

### 6.3 Boxes under KWin

#### 6.3.1 Identity

KWin 6.8.2 replaces a sandboxed window's `applicationId` with the security-context app id unless
the window's app id starts with it, then sets **both** `resourceClass` and `desktopFileName`
[measured: kwin src/xdgshellwindow.cpp:847-863; clientconnection.cpp:191-195]. `kdos-boxsock`
passes the box name [measured: src/desktop/kdos-boxsock/main.c:290], a box is named after its pack
(`app.<id>`, one box per application) [verdict inv-features: kdos-appbox.md:115-116,
main.c:870-876], and launchers are written under upstream ids (`org.pwmt.zathura.desktop`)
[measured: src/system/kdos-appbox/launchers.c:679-681]. So nearly every boxed window gets
`desktopFileName = app.<id>`, matching no launcher: generic icon, a pin that never lights,
broken grouping and rules.

Design:

- The security-context app id is the box's **launcher id**: `app.<id>` when the box exports one
  launcher, `app.<id>.<stem>` for each launcher of a multi-launcher pack (`app.maxima` carries
  maxima and wxmaxima [verdict inv-features]). One socket per (box, launcher):
  `kdos-box-<box>.<stem>@<tag>.sock` (today per box: main.c:110-111).
- `genlaunchers` writes `<appid>.desktop` (not the upstream id) with `Icon=<appid>`,
  `StartupWMClass=<appid>`, `X-KDOS-Box=<box>`, `Keywords=` + "box", the upstream `MimeType=`; it
  exports the icons into `~/.local/share/icons/hicolor/<size>/apps/<appid>.{png,svg}` (none are
  copied today), deletes launchers whose pack or box is gone (only `kdos-shell` filtered stale
  rows, `sh_box_missing()`), then runs `kbuildsycoca6`. Host and boxed copies of one program now
  have distinct ids.
- `kdos-box gc` / `kdos hey boxes` match a window to a box by app id `== <box>` or prefix
  `<box>.`; boxes created untagged (fallback below) are matched by pid → `/proc/<pid>/cgroup`.
- Box title tint: one KWin rule per box in `~/.config/kwinrulesrc`, `wmclass` exact `<box>` (and
  prefix for multi-launcher), `decocolor=KDOS<BoxAccent>` force [verdict res-innovation: per-app
  upstream wmclass rules never match].
- `kdos appid` compares KWin's effective `desktopFileName` with launcher file names (6.7).
- Store-lane box naming: measure whether store boxes follow `app.<id>`; apply the same rule.

#### 6.3.2 Confinement

What KWin 6.8.2 gives: 7 globals denied to sandboxed clients (`org_kde_plasma_window_management`,
`org_kde_kwin_fake_input`, `zkde_screencast_unstable_v1`, `org_kde_plasma_activation_feedback`,
`kde_lockscreen_overlay_v1`, `wp_security_context_manager_v1`, `ext_data_control_manager_v1`);
everything to every untagged client [measured: wayland_server.cpp:132-162]. A box shares
`$XDG_RUNTIME_DIR` and can open `wayland-0` untagged, `kdos-appbox` falls back to the untagged
socket when boxsock is slow, and boxes get the raw session bus [measured: kdos-appbox.md:556,
906-910; kdos-appbox/main.c:276-283]. The tag is therefore advisory today, under kdos-comp and
under KWin alike.

Mitigations, all P0 before boxed apps are shown under KWin, which is the M3 login switch (§8.3); 1–3 and the launcher-id task of 6.3.4 land in M3:

1. **Private runtime dir.** Each box gets `/run/user/<uid>/kdos/box/<box>/` bound as its
   `XDG_RUNTIME_DIR`, containing only: the tagged Wayland socket(s), `pipewire-0`, `pulse/native`,
   the proxied bus socket, `doc/` (document portal), `at-spi/` [inferred list: measure each app
   class]. No `wayland-0`, no `bus`. The untagged fallback is removed: a box whose boxsock fails
   does not start, with the reason printed.
2. **Bus proxy.** `xdg-dbus-proxy` (ported, `ports/core/containers/xdg-dbus-proxy`) per box,
   `--filter`, talk allowlist: `org.freedesktop.portal.*`, `org.freedesktop.Notifications`,
   `org.kde.StatusNotifierWatcher`, `org.a11y.Bus`, `org.freedesktop.ScreenSaver`,
   `org.mpris.MediaPlayer2.*` (own), `org.freedesktop.secrets` only with grant `secrets`. Hidden by
   default: `org.kde.KWin` (ScreenShot2 trusts any caller [verdict res-innovation], `/Scripting`),
   `org.kde.kglobalaccel`, `org.kde.plasmashell`, `org.kde.klipper`, `org.kde.krunner`,
   `org.kde.ksmserver`, `org.freedesktop.systemd1`.
3. **KWin denylist patch** `ports/core/kde/kwin/sandbox-policy.patch` (`Subject: wayland: deny
   desktop-shell globals to sandboxed clients unless granted`, ~250 LOC, upstreamed in parallel):
   also deny `zwlr_layer_shell_v1`, `kde_output_management_v2`, `org_kde_kwin_dpms`,
   `org_kde_plasma_shell`, `org_kde_kwin_keystate`, screen-edge, virtual-desktop management,
   external brightness, input-method globals; allow a granted name per security-context app id
   read from `~/.config/kdos/boxes/<box>.conf` `grant =` [measured: profile path
   kdos-appbox.md:856], cached per client.
4. **New grant vocabulary.** The wlroots names die with kdos-comp [measured: kdos-appbox.md:981-1003;
   kdos-grant.c:44-57]. New table: `window-list` (`org_kde_plasma_window_management`),
   `fake-input`, `screencast` (`zkde_screencast_unstable_v1`; portal is the normal route),
   `data-control` (`ext_data_control_manager_v1`), `layer-shell`, `output-management`, `dpms`,
   `plasma-shell`, `input-method`, `secrets` (bus). `input-method` is never implied.
5. **Honest text.** security-model.md states: the boundary is the private runtime dir + bus proxy
   + container; the KWin tag filters only what reaches the tagged socket; a box granted
   `fake-input` or `window-list` can act on the whole desktop; KWin's systemd-cgroup sandbox
   detection is compiled out on KDOS (`HAVE_SYSTEMD`, clientconnection.cpp:25-58).

Boxed apps lose `ext_data_control` by default: clipboard managers inside a box stop; ordinary
copy-paste through their own windows works.

#### 6.3.3 Decorations, monitor, re-homing

- Sandbox badge: patch set I10 (KWin exposes `securityContextAppId`/engine on `Window` and
  KDecoration3 `DecoratedWindow`; Breeze/KDOS decoration draws a box glyph; libtaskmanager
  tooltip "In box: <box>") — ~220 LOC over four modules, `Subject:`-headed, upstreamed (helps
  Flatpak, engine `org.flatpak`). P1.
- System Monitor grouping. §3.2 owns the cgroup layout; this item consumes it. Under elogind
  with `DelegateSessionCgroups=yes` the session moves to `/sys/fs/cgroup/<sid>`, the user owns
  `<sid>/{session,delegated}`, and the `user.slice` tree is deleted [measured: §3.2,
  logind-session.c:144,252-307; today's tree is `user.slice/user-<uid>/session`, 15_userdirs.sh:33,
  getty.c:418]. `kdos-appbox` starts each box in a scope named `app-kdos-<appid>-<n>.scope`
  (`podman --cgroup-parent`), placed by §3.2's measured choice: under
  `/sys/fs/cgroup/<sid>/delegated/`, or in a root-created
  `user.slice/user-<uid>.slice/user@<uid>.service/app.slice/` subtree that elogind never touches
  [inferred: which one podman accepts under `DelegateSessionCgroups` is a §3.2 measurement].
  libksysguard lists only `.scope`/`.service` nodes under `user@<uid>.service` and reads only each
  node's own `cgroup.procs`, while podman puts processes in `libpod-<id>` and `conmon` children
  [measured: application_data_model.cpp:20,165; verdict res-innovation:
  container_internal_linux.go:372]. So the stock Applications page shows boxes only with the
  `user@<uid>.service` placement plus §7.6.1's carried
  `ports/core/kde/libksysguard/aggregate-scope-children.patch` (`Subject: processcore: count
  processes of a scope's descendant cgroups`). Until that gate passes, `applications.page` stays
  hidden by the `pageOrder` override of §4.1.2, and the KDOS page of §7.6.1 shows boxes from the
  ksystemstats plugin. Host apps get scopes only through KIO's
  `_KDE_APPLICATIONS_AS_SCOPE=1` + a session-bus `org.freedesktop.systemd1` answering
  `StartTransientUnit` fast and failing `ListUnitFilesByPatterns` [verdict res-innovation]: not in
  v1 (Default A13), P2.
- Portal app ids for boxes: patch `xdg-desktop-portal` to parse an `app-*.scope` component of
  `/proc/<pid>/cgroup` outside `HAVE_LIBSYSTEMD` [measured: xdp-app-info-host.c:113-201]. P1.
- `kdos-boxsock` moves to `src/system/kdos-boxsock/` (box infrastructure beside `kdos-appbox`; it
  links no toolkit); its comment naming kdos-comp is rewritten to KWin.

#### 6.3.4 Tasks

- [ ] Per-launcher security context in `kdos-boxsock` + `kdos-appbox`; `genlaunchers` ids, icons,
  stale removal, `kbuildsycoca6`. M (200-400 LOC C). Gate: rewritten `kdos appid` reports zero
  MISMATCH on the rig with 5 boxed apps incl. `app.maxima`; Task Manager shot shows pinned
  launchers lighting up with their own icons. Docs: kdos-appbox.md (launchers, environment,
  sockets), packs-and-boxes.md, session.md#the-environment-a-box-receives.
- [ ] Private runtime dir + bus proxy + remove untagged fallback. M. Gate: inside a box `ls
  $XDG_RUNTIME_DIR` shows no `wayland-0`/`bus`; `busctl --user call org.kde.KWin
  /org/kde/KWin/ScreenShot2 …` fails; Firefox in a box plays audio, opens the file portal, reads
  with Orca. Docs: kdos-appbox.md, security-model.md#sandboxed-clients.
- [ ] KWin sandbox-policy patch + grant table. M. Gate: a test client in a box binding each denied
  global is refused, and bound after `kdos-box profile <b> grant=layer-shell`. Docs: kdos-appbox.md
  "memory and grants" table replaced; security-model.md.
- [ ] Box title rules from genlaunchers. S. Gate: rig shot of boxed vs host app titlebars.
- [ ] Scope placement (per §3.2's measured choice) + §7.6.1's libksysguard patch. M. Gate: a box
  started from Plasma has `memory.max` set to its profile's limit and its scope is visible to
  libksysguard (`kstatsviewer` lists it); then the System Monitor Applications page lists the boxed
  app with icon and memory, and §4.1.2's `pageOrder` override drops its omission of
  `applications.page` in the same change.
- [ ] Portal cgroup fallback patch. S. Gate: portal permission store records `app.<id>`.
- [ ] Sandbox badge patch set. M. Gate: decoration shot shows the glyph; upstream MRs opened.
- [ ] Re-home `kdos-boxsock`. S. Gate: `grep -rlx kdos-boxsock script/phases/*/packages*` names the
  `src-system` list; preflight passes.

### 6.4 System Settings

KDOS KCMs live in `src/desktop/kcm-kdos-<name>/` (KCMUtils/Kirigami, KF6-stable API: no rebuild
per Plasma minor), ids `kcm_kdos_<name>`, `X-DocPath` `kdos/<page>.html#<anchor>`, which the
Help button opens as `help:/kdos/…` in KHelpCenter (ruling 8, §8.5; kdos-doc links
`/usr/share/doc/HTML/en/kdos` to the book so KIO's `help:` worker serves it).

| KCM | Category | Talks to | Size | P |
|---|---|---|---|---|
| **System Versions** `kcm-kdos-system` (slots, trial, rollback, update channel) | System | `io.kdos.Slots1` | ~400 C++ + 500 QML | P0 |
| **Boxes** (list; Network, Files, Devices/GPU, Resources, Display grants, Lifecycle incl. recreate banner and `.ktar` export, Colour) — modelled on flatpak-kcm's per-app UI, which itself needs Flatpak [measured: flatpak-kcm CMakeLists.txt:22] | Application Permissions | `kdos-box profile <name> key=value` (no root); each `!` line shown inline | ~600 C++ + 1,200 QML | P1 |
| **Firewall** (named services on/off, policy line, `open` in the warning slot) | Network | `io.kdos.Power1` on the system bus (`SetFirewallService`, `ListFirewallServices`; polkit `io.kdos.firewall.set`, section 3.7) | ~300 C++ + 400 QML | P1 |
| **KDOS Look** `kcm-kdos-look` (Phosphor mode toggle + preset, Terminus option, accent-sync status) | Appearance | `kdos phosphor on\|off` (§5.4.1) | ~250 C++ + 300 QML | P1 |
| **Health** (`kdos doctor` output) | kinfocenter | CommandOutputContext pattern [measured: kinfocenter kcms/interrupts] | ~40 C++ | P1 |
| Services (ksvc) | System | `service` | 800-1,200 | P2 |

Decisions:

- **No Accent KCM.** D4's "Accent" is the stock Global Theme page: each accent is its own
  global theme `org.kdos.<accent>.desktop` with previews, so that page is the accent picker
  (§5.1 P3), and `kded-kdos` carries a pick made there through `kdos theme --from-plasma`
  (§5.3). The KDOS Look KCM holds the Phosphor toggle; the Welcome Look page (§5.2.18) offers
  the 8 swatches on first run. A separate `kcm_kdos_accent` would be a second accent picker.
- **Firewall: a KDOS KCM, not plasma-firewall.** plasma-firewall's backends are firewalld/ufw, it
  drives services through `systemctl` and reads ufw logs from the journal [measured: missed list
  res-session], and its UI is rule-oriented (ports, protocols), which KDOS's daemon refuses by
  design ("names services, never ports"). plasma-firewall is not ported.
- **Backup: Kup + a restic backend** (job, schedule, "back up when this disk is plugged in",
  KIO worker for snapshot browsing; ~1,200-3,000 LOC C++, upstreamed), P1. restic stays the KDOS
  engine; bup is not ported (it would be a new Python port [verdict inv-features]).
- **Users:** stock Users KCM over accountsservice (`-Delogind=true`, D3), avatars in
  `/usr/share/plasma/avatars`; fprintd 1.94.5 present.
- **Date & Time / Region / Keyboard:** stock KCMs over the §3.6 provider; the Date & Time KCM
  runs its own PolkitQt1 check of `org.freedesktop.timedate1.set-time` before any call [verdict
  inv-features], so that action id must exist in the shipped policy.
- **Login Screen:** sddm-kcm; `login.conf` and `kdos-powerd autologin` deleted.

kinfocenter: `/usr/share/kdos/xdg/kcm-about-distrorc` (the §5.2.17 file; B10 location)
`[General] LogoPath=distributor-logo-kdos`, `Website`, `UseOSReleaseVersion=true`, `ShowBuild=true`,
`ExtraSoftwareData=/usr/libexec/kdos/about-slot;/usr/libexec/kdos/about-accent;/usr/libexec/kdos/about-tree;/usr/libexec/kdos/about-libc;/usr/libexec/kdos/about-boxes`.
The list is `;`-separated: kinfocenter reads it with `readXdgListEntry`, so a `,` list is one
nonexistent path [measured: kinfocenter-6.8.2 kcms/about-distro/src/main.cpp:172-173]. Each
script **always prints exactly two lines** (label, value) and returns in well under 100 ms:
fewer than two lines makes `ThirdPartyEntry::processOutput` `qFatal`, killing System Settings,
and each runs twice synchronously on the GUI thread [measured: ThirdPartyEntry.cpp:12-34,45-49].
`about-boxes` reads a cached count, never podman.

ksystemstats plugin `src/desktop/ksystemstats-plugin-kdos/` (§7.6.1) (version-locked libksysguard API, 500-900
LOC): per-box CPU/memory/IO from box cgroups via libkproc, per-app energy from `kdos-energy
--json`; plasma-systemmonitor page files `boxes.page`, `energy.page`. P1.

External modules: list Partition Manager, KSystemLog, System Monitor under System Settings
[inferred mechanism: measure systemsettings 6.8.2 external-module `.desktop` format].

Tasks:

- [ ] System Versions KCM `src/desktop/kcm-kdos-system` (§7.3.6 owns the page's content). M. Gate: QtTest over a fixture `io.kdos.Slots1`; rig shot; "Start
  previous version next time" arms `try` (bootstate fixture shows `try = b attempts = 1`). Docs:
  `04-programs/kdos-settings.md`, installation.md#ab-root-slots, administration.md.
- [ ] Boxes KCM. L. Gate: AT-SPI tree golden; changing network to `none` shows the recreate banner;
  `kdos-box profile` reflects every control. Docs: kdos-settings.md, kdos-appbox.md (profile UI
  pointer replaces the kdos-settings Boxes page).
- [ ] Firewall KCM. M. Gate: toggling `ssh` starts sshd and rewrites
  `/etc/nftables.d/50-kdos-services.nft`; non-wheel user sees read-only. Docs: daemons.md#firewall
  (client list), kdos-settings.md.
- [ ] KDOS Look KCM `src/desktop/kcm-kdos-look`. S. Gate: toggle runs `kdos phosphor on` by
  argument vector and flips effect, global theme and font in one action (rig `--gl` shot; plain
  rig shows the "unavailable on software rendering" note). Docs: theming.md "Phosphor mode",
  kdos-settings.md.
- [ ] kcm-about-distrorc + 5 scripts. S. Gate: §5.2.17's `testing/aboutcheck.sh` (each script
  under `env -i`, `LANGUAGE=en_US:C`, a missing bootstate and a read-only `/var` prints two lines
  in <100 ms); `grep -c ';' /usr/share/kdos/xdg/kcm-about-distrorc` on the image is non-zero and
  no `,` appears in the `ExtraSoftwareData` value; About shot shows the slot line.
- [ ] Kup restic backend. L. Gate: backup + restore of a fixture dir; snapshot browsable in Dolphin.
- [ ] ksystemstats plugin + pages. M. Gate: `kstatsviewer` lists `kdos/boxes/*` sensors.
- [ ] accountsservice port. S. Gate: Users KCM shot lists accounts; avatar change persists.
- [ ] `kwin-script-kdos-window-memory` (6.1 Windows, P1). S (150-300 LOC JS). Gate: a moved Kate
  window reopens at its geometry after a Kate restart (rig shot pair). Docs: window-model.md, kwin.md.
- [ ] `/etc/xdg/freespacenotifierrc` `minimumSpacePercentage=10` (6.1 Desktop, P1). S. Gate: a
  95%-full loop-mounted disk raises the notice (rig shot). Docs: desktop.md, configuration.md.
- [ ] `.ktar` import offer in `kded-kdos` (6.1 Files, P1). S. Gate: inserting a stick holding a
  `.ktar` shows the import offer, and its action opens Discover on it (rig shot). Docs:
  packs-and-boxes.md, discover.md.
- [ ] `apps-pending` notification (6.1 First boot, P1). S. Gate: the first login after an install
  with selected apps shows the notification, and its action opens Discover on the pending set
  (rig shot). Docs: installation.md, discover.md.
- [ ] TabBox `OneWindowPerApplication` in the shipped kwinrc (6.1 Windows, P1). S. Gate: with two
  Konsole windows and one Kate open, Alt+Tab shows one entry per app (rig shot). Docs: desktop.md
  (keys), window-model.md.

### 6.5 Search, help, onboarding, tray, notifications

#### 6.5.1 KDOS KRunner runner

`src/desktop/kdos-runner/` in C on basu (no Qt: passes the toolkit guard unchanged; avoids a
per-Plasma rebuild). D-Bus runner `org.kde.krunner1` [measured: krunner v6.30.0
org.kde.krunner1.xml; dbusrunner.cpp:30-42,96-97], registered by
`/usr/share/krunner/dbusplugins/kdos.desktop` (`X-Plasma-API=DBus2`,
`X-Plasma-DBusRunner-Service=io.kdos.Runner`, `X-Plasma-DBusRunner-Path=/runner`,
`X-Plasma-Runner-Syntaxes`), activated by `/usr/share/dbus-1/services/io.kdos.Runner.service`.

| Source | Example | Action (argv, never a shell) |
|---|---|---|
| `kdos` verbs | `accent amber`, `night light`, `update check`, `phosphor on`, `tiling on`, `desk dock` | `kdos <verb> …` |
| two-letter codes `~/.config/kdos/favorites` | `ff` | launch desktop id |
| routes `/etc/kdos/menu.conf` | `setup.network` | KCM / program |
| packs and boxes | `box stop firefox`, `install gimp` | `kdos-box` / Discover deep link |
| install-from-medium | `gimp (on medium)` | Discover `medium` origin |
| chords | `keys tile` | shows the binding from `kglobalshortcutsrc` |
| offline library (`?` prefix; plain queries also get library matches ranked below the others) | `?photosynthesis` | kiwix-serve 127.0.0.1:8080 suggest, Kolibri :8081, the KDOS and KFS book indexes, `kdos-*` man pages, Zeal docsets, Organic Maps `geo:` (§7.4.1 owns the sources; one runner, no separate library runner port) |
| `why <path>` | | `kdos why` in Konsole |

Chord: KRunner keeps `Alt+Space`, `Alt+F2`, `Search` and gains `Meta+Space`. Trap: fcitx5 5.1.22
binds Super+Space to group switching and KWin's global-shortcut filter runs first, so fcitx5 would
silently stop switching [measured: fcitx5 globalconfig.cpp:109-115; kwin input.cpp:3196 vs 3219].
Ship `/usr/share/kdos/xdg/fcitx5/config` (B10) with `EnumerateGroupForwardKeys` moved to
`Super+Alt+Space` [inferred: fcitx5 reads `XDG_CONFIG_DIRS`; measure, else `/etc/xdg/fcitx5/config`].
Matches run within 50 ms from cached indexes (KRunner aborts slow runners).

#### 6.5.2 Help, welcome, tray, notifications

- **KHelpCenter:** `share/khelpcenter/plugins/kdos.desktop` (`X-DocPath=file:///usr/share/doc/kdos/index.html`,
  `X-DOC-Weight=-100`) and `kfs.desktop` [measured: khelpcenter docentry.cpp:213-237,
  mainwindow.cpp:296-322; book at /usr/share/doc/kdos via `src/system/kdos-doc/mkbook.py:51`]. F1 in
  KDOS KCMs opens the section. P1.
- **plasma-welcome extra pages** `/usr/share/plasma/plasma-welcome/extra-pages/NN-*.qml` (~1,500
  QML, port `src/desktop/kdos-welcome`), in order: 00 Accessibility (Orca chord, larger text,
  high-contrast accent, reduced motion, keyboard aids); 10 Look (8 accents, Phosphor mode off by
  default with one toggle, D2; content §5.2.18); 20 Desk (KDOS / Dock / Tiling, D5, applies the
  global-theme layout; content §5.5.4); 30 Your system (A/B, previous version button); 40 Software
  and boxes; 50 Offline library; 60 Keys; 70 Privacy and network
  (KDE Connect firewall name, Background Apps, wallet choice on autologin); 80 Help. Live mode:
  `plasma-welcomerc LiveEnvironment=true`, `LiveInstaller=kdos-installer` (6.6);
  `--post-update` page reads `update.state` and the release notes. P0 for 00/10/20/30/80, P1 rest.
- **Tray:** Discover's notifier plasmoid + `kdos-discover-notifier` (6.2.2) is the update/slot tray.
- **Notices daemon** `src/desktop/kdos-notice/` (the one notices program; §7.6.3 and §7.3.8 add
  to it): C on basu, `org.freedesktop.Notifications` client, XDG autostart `OnlyShowIn=KDE`. It
  watches `/var/lib/kdos/update.json`, `kdos restarts`, `kdos cve` and `kdos-bootctl status
  --json`, and posts: restarts after an update ("Firefox and Konsole were updated: Restart
  them"), CVE summary, trial state, the rollback notice (§7.3.6), OOM kill naming the box (reads
  `kdos-oomd` events; Plasma's oom_notifier is systemd1-only), and §7.3.8's "Allow on this
  network?" prompt when KDE Connect/krdp/kiwix is blocked. The same binary, run as
  `kdos-notice --bless` from `kdos-session-good.desktop`, is the 6.2.4 bless helper that calls
  `io.kdos.Slots1.Bless()`. Persistent notifications only for decisions. ~500 LOC. P1 (bless part
  P0).

Tasks:

- [ ] Port KF6 `krunner`; write `kdos-runner`. M (~900 LOC). Gate: `qdbus6 org.kde.krunner /App
  query ff` returns Firefox first; selftest of the matcher on fixture favourites/menu.conf under
  ASan (it parses user files). Docs: plasma-shell.md, kdos-command.md#kdos-menu,
  02-user-guide/desktop.md (search).
- [ ] fcitx5 config override. S. Gate: rig: Meta+Space opens KRunner and Super+Alt+Space switches
  fcitx5 group.
- [ ] KHelpCenter plugin files. S. Gate: shot with "KDOS" first in the tree.
- [ ] Welcome pages. L. Gate: AT-SPI tree goldens per page; rig shots of each; Orca reads page 00.
  Docs: getting-started.md, accessibility.md.
- [ ] `kdos-notice` (restart, CVE, trial, rollback, OOM notices; `--bless`; §7.3.8's prompt). M.
  Gate: fixture `update.json`/`restarts`/`cve` JSONs post exactly one notification per state
  change (`dbus-monitor` count in selftest); `kdos-notice --bless` against a fixture
  `io.kdos.Slots1` on the session bus calls `Bless()` once, only after a fake `org.kde.plasmashell`
  has answered for the configured interval.

### 6.6 Installer (D12)

Backend facts: `install.c` (2,525 lines), `probe.c` (1,150), `conf.c` (356), `dump.c` (478) carry
no libktui calls; `pages.c` and `main.c` hold 274 and 94 `kt_` refs; `kinstall.h` includes
`ktui.h` and `kcolor.h` [measured: res-session]. kinstall already has `--dump probe|plan --json`,
`--config/--save/--unattended` and the S/K/P/N/L/W/F/D child line protocol [measured:
kinstall.md:73-113,1039-1072]. The backend keeps libkcolor (accent for Limine/splash) [verdict
res-session]. Calamares is rejected: no Limine (`bootloader.conf:16`).

Design:

- Split `src/system/kdos-installer`'s sources into `src/system/kinstall-core` (install/probe/conf/dump,
  libkbase+libkcolor) and the TUI (`pages.c`, `main.c`, libktui). kinstall-core is not a port: the
  kdos-installer recipe archives it as a static `libkinstall-core.a` and links it with the TUI into
  the one binary `/usr/bin/kinstall`. Add `--events json`: each
  S/K/P/N/L/W/F/D line as one JSON object on stdout (`{"t":"P","step":4,"frac":0.42}`), ~100 LOC.
- `src/desktop/kdos-installer-gui/` (Kirigami, ~2.5-4k QML/C++): pages Welcome/Language →
  Keyboard → Disk (guided, or "Manual (Partition Manager)") → Encryption → Account or OEM → Look
  (accent, desk) → Applications → Summary (`--dump plan --json`) → Install (events) → Done. It
  writes an answer file and runs `pkexec /usr/bin/kinstall --config <f> --unattended --events
  json`; polkit action `io.kdos.installer.run` = `yes` for the live session only (rule shipped on
  the live medium).
- Manual layouts: `partitionmanager`/`kpmcore` ports; the GUI launches Partition Manager, then
  re-probes with `--dump probe --json`.
- Rewrite in the backend: the Theme step [measured: install.c:83, 1996-2034 runs live `kdos
  theme`] writes `/etc/xdg/kdeglobals` `[KDE] LookAndFeelPackage=org.kdos.<accent>.desktop`
  (the one key B10 puts there; startplasma applies it at first login, §5.3 "Outside a session")
  and the chosen desk, runs `kdos-bootctl theme`; the autologin writer [measured: install.c:1765-1833 edits
  `login.conf`] writes `/etc/sddm.conf.d/kde_settings.conf` `[Autologin] User=`,
  `Session=plasma.desktop` (the file sddm-kcm writes, so one source); the keymap writes
  `/etc/xdg/kxkbrc` too.
- **Slot B and shared state:** the installer creates and fills slot B and the shared volume from
  6.2.6, removing the "Nothing creates or first fills the second root slot" gap [measured:
  known-gaps.md:698-708]. Prerequisite of D11.
- **OEM:** answer `oem = 1` skips the account; the backend leaves `/etc/plasma-setup-done` absent.
  `fs/etc/init.d/NN_plasma-setup.sh` (before SDDM's inittab respawn) creates the `plasma-setup`
  user and `/run/plasma-setup` home with tmpfiles' C/Z semantics [verdict res-session], and
  plasma-setup's own bootutil writes `/etc/sddm.conf.d/99-plasma-setup.conf` (its systemd probe
  falls back to SDDM paths, no patch) [measured: plasma-setup bootutil.cpp:22-46]. The recipe drops
  `plasma-setup.service` (kpkg lint refuses `usr/lib/systemd`, lint.c:143). Needs the §3.6
  providers (timedate1/hostname1/locale1 action ids its polkit rule names) and plasma-keyboard.
  KDOS modules: accent/desk, app groups, privacy. P1.
- **Live medium:** a live-only boot step writes `/usr/share/applications/kdos-installer.desktop`
  (plasma-welcome requires that location [verdict inv-features]) and a Desktop icon; an installed
  system never contains it. Live defaults: `kscreenlockerrc Autolock=false`, `baloofilerc
  Indexing-Enabled=false`, `kwalletrc Enabled=false`. `fs/etc/skel/.local/share/applications/kdos-install.desktop`
  (Exec=foot …) is deleted.
- **TUI kept** for serial/unattended; libktui stays.

Tasks:

- [ ] Backend split + `--events json`. M. Gate: `kinstall --dry-run --unattended --events json
  --config t.conf | python3 -c 'json lines parse'`; selftest; TUI unchanged (existing goldens).
  Docs: kinstall.md (synopsis, child-process protocol).
- [ ] Theme and autologin writers for Plasma/SDDM. S. Gate: dry-run plan lists the new files; rig
  install boots to the chosen accent and autologs in.
- [ ] Slot B + shared volume creation. L. Gate: fresh rig install has `slot_b` recorded and a
  successful `kdos update apply` A→B→A keeps `~` contents. Docs: installation.md#ab-root-slots,
  known-gaps.md (remove entry), boot-and-init.md#ab-slot-selection.
- [ ] Kirigami installer. L. Gate: rig install end to end with the GUI (shots per page, AT-SPI
  goldens), plus one LUKS+LVM install. Docs: kinstall.md new "The graphical installer" section,
  installation.md rewritten around it.
- [ ] Ports kpmcore + partitionmanager. M. Gate: Manual path creates a layout the re-probe sees.
- [ ] OEM mode + plasma-setup port + init script. M. Gate: rig OEM install boots into plasma-setup,
  creates an account, never shows it again.
- [ ] Live-only installer entry + live defaults. S. Gate: Welcome on the live medium offers
  "Install KDOS"; `ls /usr/share/applications/kdos-installer.desktop` on an installed rig disk
  fails.

### 6.7 The `kdos` command

| Verb | Today | Under Plasma | P |
|---|---|---|---|
| `theme` | GTK, icons, cursors, kdeglobals, qt5ct/qt6ct, themerc, foot, kdos-term, CLI tools, SIGHUP surfaces, powerd accent [measured: kdos-command.md:276-392] | the §5.3 steps 1-8, owned there: `plasma-apply-lookandfeel -a org.kdos.<accent>.desktop -k` (not `plasma-apply-colorscheme`, which no-ops on an unchanged name [measured: plasma-apply-colorscheme.cpp:77-80]); wallpaper by `org.kde.PlasmaShell evaluateScript` guarded to `KDOS-*` images (not `plasma-apply-wallpaperimage`, §5.2.7); Konsole `setProfile` on `KDOS *` sessions; qt5ct merge; `reconfigureEffect kdos_crt`; `kdos-power accent`. Keeps CLI-tool configs, Limine, splash, `/etc/vtrgb`; drops themerc, kdos-term, qt6ct, the SIGHUP list | P0 |
| `toggle stay-awake` | flag read by kdos-comp | holder process keeping `org.freedesktop.PowerManagement.Inhibit` (released on exit) | P1 |
| `toggle night-light` | flag + retint | kglobalaccel `invokeShortcut("Toggle Night Color")` component `kwin` | P1 |
| `toggle dnd` | flag read by kdos-notifyd | Plasma DND (`plasmanotifyrc [DoNotDisturb] Until`) [inferred: measure] | P1 |
| `phosphor on\|off\|status` (new, D2) | — | §5.4.1 owns what it switches (effect `kdos_crt`, retro accent, optional Terminus) | P1 |
| `tiling on\|off` (new, D6) | — | `kwinrc [Plugins] krohnkiteEnabled` + `org.kde.KWin /KWin reconfigure` (§5.5.3) | P1 |
| `desk <kdos\|dock\|tiling>` (new, D5) | — | §5.5.4: backup of the applets file, `evaluateScript` `loadTemplate` | P1 |
| `notify` | toast via kdos-notify.sock | `org.freedesktop.Notifications.Notify`; `--time`, `--battery` kept; `--dnd` → toggle; dismiss/raise/count/list **dropped** (exit 2, documented) | P1 |
| `remind` | toasts; `--ask` via `kdos-prompt` [measured: remind.c:500-502] | Notify; `--ask` → `kdialog --inputbox`; loop from Plasma autostart | P1 |
| `panel toggle` | SIGUSR1 to `kdos-shell` | `org.kde.PlasmaShell.evaluateScript` flipping `panels()[0].hiding` | P2 |
| `menu summon/toggle` | `kdos-palette --route` | `org.kde.krunner /App querySingleRunner("kdos", "route:<r>")` | P1 |
| `settings [page]` | `kdos-settings --page` | `systemsettings <kcm>` via a fixed page→KCM map; unknown page → `systemsettings` | P1 |
| `places [add]` | `~/.config/kdos/places` | prints/edits `~/.local/share/user-places.xbel` (format measured from KFilePlacesModel output) | P2 |
| `thumb` | cache + `--ppm` | **deleted** with `thumb.c` (§2.3): ffmpegthumbs and kdegraphics-thumbnailers write the same `~/.cache/thumbnails` | — |
| `trash` | unchanged | unchanged | — |
| `share` | no-arg → `kdos-pick`; terminal = foot | no-arg → `kdialog --getopenfilename`; terminal via `xdg-terminal-exec` → Konsole | P1 |
| `hey list/run/outputs/boxes` | `kdos-cmd.sock` | org_kde_plasma_window_management C client (wayland-scanner, `plasma-wayland-protocols` ported); outputs via `kscreen-doctor -j`; actions via KWin scripting D-Bus; boxes by app-id prefix (6.3) | P1 |
| `appid` | ledger by kdos-comp | live windows' effective `desktopFileName` vs launcher file names; ledger appended by a KWin script calling `io.kdos.Runner.RecordAppId` | P1 |
| `stutter` | `kdos-frames.sock` | **dropped** in v1 (P2 KWin plugin) | — |
| `help [--keys]` | key sheet from rc.xml | reads `kglobalshortcutsrc` + `/usr/share/kglobalaccel/*.desktop`; `--print` kept | P1 |
| `status`, `version` | session field = KDOS desktop | session = `Plasma <ver> (Wayland)` | P1 |
| `doctor` | kdos-comp/kdos-shell checks | kwin_wayland, plasmashell, `org.freedesktop.impl.portal.desktop.kde`, elogind/login1, timedate1, polkit agent, ksecretd, udisks2, `kdos-sysupdated`, box proxy | P1 |
| `update` | source only | `apply` = binhost/image; `--source` = today's path; `check --json` feeds `kdos-sysupdated` | P0 |
| `kdos-shot` | grim + slurp | **deleted** with `shot.c`, its TOOLS row and symlink (§2.3, B5): Spectacle is the screenshot tool; `qr` mode is dropped | — |
| `kdos-mpctl` | ranks mpd vs MPRIS | MPRIS server for mpd; Plasma handles media keys | P1 |
| open-with (`kdos-appbox` `open.c` `OPENWITH_PROG "kdos-openwith"` [measured: open.c:68]) | cell chooser | portal `OpenURI` with `ask=true` (xdg-desktop-portal-kde AppChooser) [inferred: measure] | P0 |
| `app` / launch | `X-KDOS-Term` reader | reader removed; `XDG_CURRENT_DESKTOP=KDE` for boxes [measured: main.c:327] | P0 |

Shipped chords (`rc.xml` today, kdos-command.md:105-117) move to `/usr/share/kglobalaccel/kdos-*.desktop`
with `X-KDE-Shortcuts`; collisions with KDE defaults are resolved for KDE (§5.8 owns the
chord table). Bash completion (`/usr/share/bash-completion/completions/kdos`) gains `phosphor`,
`tiling`, `desk` and loses the dropped verbs (`stutter`, `thumb`, `notify` dismiss/raise/count/list).

Tasks:

- [ ] `theme` KDE branch. M (300-600 LOC). Gate: rig `kdos theme amber` repaints Plasma, an open
  Konsole and the boot menu; `kdos theme --audit` passes. Docs: kdos-command.md#kdos-theme,
  theming.md.
- [ ] `toggle` rewrite (stay-awake, night-light, dnd). S. Gate: each toggle's state visible in
  Plasma (DND bell, Night Light applet, battery applet "blocked"). `kdos phosphor`, `kdos tiling`
  and `kdos desk` are §5.4 and §5.5 tasks. Docs: kdos-command.md#kdos-toggle, configuration.md
  toggles entry.
- [ ] `notify`, `remind`, `share`, `menu`, `settings`, `panel`, `places` rewrites. M. Gate:
  selftest per verb with a fake session bus (`dbus-run-session`); exit codes documented.
- [ ] `hey`/`appid`/`box gc` client. M (400-700 LOC C). Gate: `kdos hey list --json` lists Konsole
  and a boxed app with correct app ids; `kdos-box gc --dry-run` names a windowless box only.
- [ ] `doctor`, `status`, `version`, `help --keys`. S. Gate: `kdos doctor` on the rig reports no
  failure; with kwin killed it names kwin.
- [ ] MPRIS bridge in `kdos-mpctl`. M. Gate: `playerctl -l` lists mpd; media keys pause mpd.
- [ ] open-with via portal. S. Gate: opening a file type with two handlers and no default shows
  KDE's chooser.
- [ ] Every row's doc: kdos-command.md rewritten (subcommand table, chord table, each section);
  command-index.md; man pages generated from the book. Gate: `bash testing/docscheck.sh && python3 testing/anchorcheck.py` pass; a grep of the old surface name in each named page is empty; preflight's man-page check passes.

### 6.8 Where KDOS Qt code lives, conventions, tests

Location: `src/desktop/<name>/` (area name reused; listed in `50_desktop`'s `packages.txt`), each
a normal src port: `kpkgbuild` + `build.sh` running `cmake -G Ninja` against ECM/KF6 +
`kfs.md`. C programs that are box or system infrastructure (`kdos-boxsock`, and `kinstall-core`, built
inside the kdos-installer recipe) live under `src/system`, daemons (`kdos-sysupdated`) under `src/daemons`. C session programs
(`kdos-runner`, `kdos-notice`) may live in `src/desktop` and link no Qt.

| Unit | Path | API stability |
|---|---|---|
| Discover backend + notifier plugin | `src/desktop/kdos-discover` | libdiscover internal: rebuild + re-test every Plasma minor |
| ksystemstats plugin | `src/desktop/ksystemstats-plugin-kdos` | libksysguard: per minor |
| KWin effect / scripts | `src/desktop/kwin-kdos` (§5.4) | effect IID per KWin version; JS scripts stable |
| KCMs (`kcm-kdos-system` System Versions, `-boxes`, `-firewall`, `-look`, `-health`) | `src/desktop/kcm-kdos-*` | KF6 KCMUtils: stable |
| kded module (accent sync §5.3, `.ktar` stick import) | `src/desktop/kded-kdos` | KF6 kded: stable |
| Welcome pages | `src/desktop/kdos-welcome` (§5.2.18) | plasma-welcome extra-page QML: per minor re-test |
| Installer GUI | `src/desktop/kdos-installer-gui` | Kirigami: stable |
| KRunner runner, notices (C) | `src/desktop/kdos-runner`, `kdos-notice` | D-Bus API: stable |

Conventions (a new chapter in `05-developer/writing-desktop-software.md`, replacing the cell
chapter):

- Prefixes: C++ namespace `Kdos`, D-Bus names and polkit actions `io.kdos.*`, KCM ids
  `kcm_kdos_*`, plugin ids `org.kdos.*`, translation domain `kdos-<name>`; exported C symbols keep
  the libk* prefix rule.
- No shell: `QProcess` with program + argument list only, never `sh -c` or a command string;
  KDOS daemons are reached over their sockets or D-Bus.
- Colour from Kirigami.Theme / KColorScheme roles, never literals (the slot rule carried over).
- KDE HIG: Kirigami FormLayout for settings, `Kirigami.InlineMessage` for `!` lines,
  `KLocalizedString`/`i18n()` on every string (the image is English-only, A1; the strings stay translatable),
  KF6DocTools for handbooks where a page has one.
- Accessibility: every control has an accessible name (enforced by the AT-SPI golden diff, §8.2.2).
- Rule-1 docs: each unit's behaviour lives in the Part IV chapter for its kind (§8.1.1): KCMs and
  the ksystemstats plugin in `04-programs/kdos-settings.md`; the runner, welcome pages and
  plasmoids in `plasma-shell.md`; the Discover backend, notifier and kdos-sysupdated in
  `discover.md`; KWin scripts, the effect and patches in `kwin.md` (`python3 testing/booknav.py
  --write` after adding them).

Tests (§8.2.2 owns the design; A12):

- `testing/selftest.sh` keeps its bare-host contract: the C units here (`kdos-sysupdated`,
  `kdos-runner`, `kdos-notice`, the about scripts, `kinstall-core`) are tested there, on a fake
  session bus where they need one.
- New `testing/plasmatest.sh` (`make plasmatest`), run only in the devdeps container
  (`testing/devdeps-image.sh` gains Qt6/KF6 dev packages); on a bare host it refuses with a
  message. It runs each Qt unit's QtTest suites under `ctest --output-on-failure`, headless with
  `QT_QPA_PLATFORM=offscreen` inside `dbus-run-session`; `qmllint` over every KDOS QML file; and
  `kpackagetool6 --install` of every KCM, plasmoid, look-and-feel, SDDM theme and KWin script into
  a temporary prefix.
- AT-SPI tree goldens replace cell goldens: KDE's selenium-webdriver-at-spi drives each KDOS KCM,
  installer page, welcome page and the SDDM theme over AT-SPI2 in the rig guest; a text dump of
  each tree, committed under `testing/atspi/`, is the golden. An unnamed control fails.
- Rig: `testing/vnc-shot.py` and `testing/quick.sh` assume tty1 autologin into kdos-desktop
  [measured: missed list res-session: vnc-shot.py:139-145,850-866]; they are rewritten for SDDM
  autologin before any shot gate in this section can run (§8.2.3 owns the harness). The Phosphor
  effect needs `--gl` / `make run-hw` [verdict res-innovation].
- **Preflight guard:** `testing/preflight.sh:1302-1340` fails any `src/*/*/` ELF that NEEDs
  `libQt*`/`libKF*` [measured], so every unit above fails preflight as it stands. Rewrite in the
  same change as hard rule 5 (D3): `src/desktop/*` may link `libQt6*`/`libKF6*` and nothing else
  from the toolkit list (no GTK, no Qt5, no Xlib); every other area keeps the full ban. Prove the
  guard fails on a planted Qt link in `src/system` before trusting it.

Tasks:

- [ ] Preflight guard rewrite + CLAUDE.md rule 5 + principles.md toolkit section. S. Gate:
  preflight fails a fixture `src/system` port linking `libQt6Core`, passes a `src/desktop` KCM.
- [ ] Devdeps image with Qt6/KF6 dev; `testing/plasmatest.sh` + `make plasmatest` (§8.2.2). M.
  Gate: `make plasmatest` runs one QtTest in the container and refuses on a bare host;
  `testing/atspi` runs one golden on the rig.
- [ ] selenium-webdriver-at-spi in the rig guest; `testing/atspi/` goldens. M. Gate: the golden
  for one stock KCM is stable across three runs; a removed accessible name fails.
- [ ] `writing-desktop-software.md` and `testing.md` rewritten; kwin.md, plasma-shell.md,
  kdos-settings.md and discover.md added in slots 21–24. M.
  Gate: `bash testing/docscheck.sh`, `python3 testing/anchorcheck.py`, `python3 testing/booknav.py`.
- [ ] Version-locked unit list wired into the Plasma bump procedure (§1.5.2, §8.3 M9): Discover backend,
  notifier plugin, ksystemstats plugin, KWin effect rebuilt and their tests run on every 6.x bump.
  S. Gate: the 6.9 bump checklist names each.


## 7. Innovations

### 7.1 Scope, selection rules and index

This section covers what KDOS adds on top of a correctly integrated Plasma. It does not cover the baseline that §5 (identity kit, layouts, defaults, shortcuts) and §6 (Discover, kdos-sysupdated, boxes under KWin, KCMs, installer) already deliver. Each item names the baseline it extends. Selection rules:

- **D10 is absolute.** No item fetches anything at run time, and none depends on Flathub, KNewStuff or a web service. The Plasma 6.9 Open-Meteo weather widget and wallpaper-of-the-day plugins are excluded for that reason (see 7.9).
- **D2.** Phosphor mode and the CRT motion are off by default. Their only entry points are one toggle (§7.2.1), offered on the Welcome tour.
- **Mechanism.** Every item uses a KDE extension point that already exists: a KWin effect or script, a KRunner D-Bus runner, a KCM, a plasmoid, a KIO worker, a KHelpCenter plugin, kcm-about-distrorc, a plasma-welcome page, a Discover plugin, or KWin window rules. Where none exists, the item is a carried `Subject:`-headed patch, upstreamed in parallel (D3).
- **Priority.** P1 ships in milestone M7 (§8.3) and P2 in M9. Nothing here gates the deletion (M6), except the shader port of 7.2.1, which lands at M5.
- **Size** is new KDOS lines, not counting ports.
- **Where code goes.** KDOS Qt/QML code goes under `src/desktop/<name>`, per the §1.3 default. C helpers that talk D-Bus through basu also go under `src/desktop/<name>`, because they serve the desktop. Root daemons go under `src/daemons/<name>`.

| ID | Innovation | Group | Prio | Size | Upstream | Depends on |
|---|---|---|---|---|---|---|
| 7.2.1 | Phosphor mode: one toggle, CRT KWin effect, retro accents | identity | P1 | ~900 C++ + 150 GLSL + 200 C | generic screen-shader effect | §4 (kwin), §5.4 |
| 7.2.2 | CRT power-cycle motion (login unfold, lock arrival, logout collapse) | identity | P2 | ~300 JS + 120 GLSL | no | 7.2.1, §5.4 |
| 7.2.3 | Accent everywhere in one action, down to the boot menu | identity | P1 | ~250 C++ + 300 QML | no | §5.3 |
| 7.2.4 | One continuous power-on, with every handover through black | identity | P1 | ~350 QML/C | no | §3.9, §5.2 |
| 7.2.5 | KDOS sound theme rendered from kdos-sfx | identity | P2 | ~150 C | no | §5.2 |
| 7.2.6 | The lock screen as the login (single-user, LUKS root) | identity | P2 | ~100 config | no | §3.4 |
| 7.3.1 | Box colour = security domain (per-box titlebar tint) | trust | P1 | ~200 C | no | §6.3 |
| 7.3.2 | Sandbox badge in title bars and Task Manager tooltips | trust | P1 | ~220 C++ (4 patches) | yes | §4, §6.3 |
| 7.3.3 | Honest confinement indicator (tagged / tagged+private / untagged) | trust | P1 | ~250 C + QML | partly | 7.3.2, §6.3 |
| 7.3.4 | Privacy lamps that name the box | trust | P1 | ~300 QML + 300 C | camera lamp | §3.3, §6.3 |
| 7.3.5 | "Open in a throwaway box" | trust | P2 | ~300 C + 1 .desktop | no | §6.3 |
| 7.3.6 | "Use previous version" everywhere, with session-confirmed slots | trust | P1 | ~600 C++ + 500 QML + 180 C | no | §3.6, §6.2 |
| 7.3.7 | Signed updates surfaced in Discover and About | trust | P1 | ~150 C | no | §6.2 |
| 7.3.8 | "Allow on this network?" for the name-based firewall | trust | P2 | ~300 C | no | §3.7, §6.4 |
| 7.4.1 | KDOS Library in KRunner (Kiwix, books, man, Zeal, Kolibri, maps) | offline | P1 | ~700 C | no | §6.5 |
| 7.4.2 | `library:/` in Dolphin and a Library category in Kickoff | offline | P2 | ~800 C++ | no | 7.4.1 |
| 7.4.3 | The KDOS and KFS books in KHelpCenter, with F1 to the right page | offline | P1 | 2 .desktop + 50 CSS | no | §4 (khelpcenter), §6.5 |
| 7.4.4 | Curated offline add-on pack in place of "Get New…" | offline | P1 | pack + Kiosk keys | no | §4, §5.2 |
| 7.4.5 | Local AI, OCR and speech: opt-in, dark until a model pack exists | offline | P2 | ~400 C + 2 menus | no | 7.4.1 |
| 7.5.1 | KRunner as the KDOS command palette | productivity | P1 | ~900 C | no | §5.8, §6.5, §6.7 |
| 7.5.2 | Key card overlay (Meta+F1) from the live bindings | productivity | P1 | ~500 QML + 300 C | yes (shortcuts overview) | §5.8 |
| 7.5.3 | Desk picker with undo | productivity | P1 | ~300 QML + 100 C | no | §5.5 |
| 7.5.4 | Auto-tiling toggle (Krohnkite bundled, off) | productivity | P1 | port + ~80 C | no | §5.6, §4 |
| 7.5.5 | Run-or-raise terminal programs, one key each | productivity | P2 | ~150 JS + 10 .desktop | no | §5.8 |
| 7.5.6 | Session restore that includes boxes | productivity | P2 | ~200 C | no | §3.8, §6.3 |
| 7.6.1 | KDOS System Monitor face and page (boxes, energy, slots) | system | P1 | ~400 QML + 500 C++ | face to the KDE Store; libksysguard aggregation patch | §6.3 |
| 7.6.2 | About this System and a Health page that report KDOS facts | system | P1 | ~60 sh + 150 C++ | no | §5.2 |
| 7.6.3 | Update tray for A/B updates, plus restart and CVE notices | system | P1 | ~250 C++ plugin + 500 C | yes (sysupdate notifier, proposed upstream) | §6.2.2, §6.5 |
| 7.6.4 | Firmware in Discover, with no network needed to list it | system | P2 | ~100 config + 1 build flag | no | §4 (discover), §6.2 |
| 7.7.1 | Welcome tour with KDOS pages, live-medium and post-update modes | onboarding | P1 | ~1,500 QML | no | §5.5, §6.5 |
| 7.7.2 | Accessibility first, on the greeter and on page 1 | onboarding | P1 | ~300 QML | no | §3.4 |
| 7.7.3 | First-run checklist that ticks itself | onboarding | P2 | ~300 QML + 150 C | no | 7.7.1 |

Every task below that changes behaviour names the pages it rewrites (hard rule 1). §8.1 owns the book's structure; the page names below are the pages as they exist today, or their §8.1 successors.

### 7.2 Identity and delight

#### 7.2.1 Phosphor mode

- **What the user sees.** One switch, "Phosphor mode", on the Welcome tour's Look page, the Accent page (§5.3), and KRunner (`phosphor on|off`); all three call `kdos phosphor on|off|status` (§5.4.1, the one CLI). Turning it on applies, in about a second:
  - the CRT pass, preset Subtle or Retro;
  - a retro accent, by §5.4.1's one pairing rule: `bone` or `paper` switch to `phosphor` and are remembered in `~/.config/kdos/phosphor [State] PreviousAccent`; amber, ice, norton, borland, perfect and phosphor are kept (the pairing is marked for user confirmation in §5.4.1);
  - with the Terminus sub-option, Terminus (TTF) in Konsole, `kdeglobals fixed` and KSplash text only; window titles and the UI stay Noto Sans (D7);
  - the CRT motion of 7.2.2.

  Turning it off restores `PreviousAccent`, the fonts and the effects that were set before. KDOS Modern stays the default (D2).
- **How it is built.**
  - **The effect.** A C++ effect `kdos_crt` overriding `paintScreen` with a per-output offscreen texture, modelled on zoom. zoom allocates a GLTexture (RGBA8 for sRGB, RGBA16F otherwise), renders into a GLFramebuffer and draws it back through `setColorspaceUniforms` [measured: kwin 6.8.2 src/plugins/zoom/zoom.cpp:320-409].
  - **Where it lives.** A KWin `.patch` adds it under `src/plugins/kdos_crt` as a built-in plugin. An out-of-tree `.so` would break at every point release, because the effect IID embeds `KWIN_PLUGIN_VERSION_STRING` = major.minor.patch [measured: effect.h:1134, config-kwin.h.cmake:4]. Static plugins skip the IID check [measured: effectloader.cpp:291-296]. §5.4 owns this choice; this item assumes the patch.
  - **The shader.** Ported from the 155 lines of `src/desktop/kdos-comp/src/kdos-crt-pass.c` [measured: wc -l]. The port lands before §2.7 deletes kdos-comp.
  - **Uniforms.** bleed, scanline (every third row), vignette, curve, phosphor floor, degauss and collapse. Presets Off/Subtle/Retro go in `kwinrc [Effect-kdos_crt] Preset=`. Subtle means bleed, floor and vignette, with no scanlines and no curve.
  - **The toggle.** `kdos phosphor on|off|status` (§5.4.1; KRunner verb `phosphor` in §6.7) does the following, without a shell:
    - saves the Modern state to the one state file, `~/.config/kdos/phosphor`;
    - `kwriteconfig6 --file kwinrc --group Plugins --key kdos_crtEnabled true`, then `org.kde.KWin /Effects loadEffect kdos_crt`;
    - applies the paired retro accent's global theme through §5.3's path;
    - switches the Konsole profile through `org.kde.konsole Session.setProfile` [measured: konsole 26.08.2 src/session/Session.h:767].
- **Rules the effect must keep.**
  - `supported()` returns false when `isSoftwareRenderer()`, which matches llvmpipe, softpipe and Software Rasterizer [measured: eglcontext.cpp:322-325]. KWin has no global "animations off": each effect gates its own `supported()` [measured: workspacescene.cpp:855-858, scriptedeffect.cpp:201, glide.cpp:183].
  - `blocksDirectScanout()` returns false while a fullscreen window is focused, so games and video keep direct scanout. While the pass is active, the cursor is composited into the curved scene and loses the hardware cursor plane [measured: workspacescene.cpp:389-417; effect.cpp:535-538]. That latency cost is accepted inside Phosphor mode only.
  - Degauss is one pulse of at most 400 ms (photosensitivity). `AnimationDurationFactor=0` disables degauss and collapse.
  - HDR outputs: run in encoded space through KWin's colour-pipeline uniforms, or refuse on an HDR output. Measure which.
- **Builds on.** The libkcolor palette (`src/libs/libkcolor/kcolor.h:60-76`: phosphor, amber, ice, bone, norton, borland, perfect, paper). The kdos-comp shader. terminus-ttf (ported; §2.4 decides whether it stays a port; this item needs it).
- **Risk.** High.
  - The patch is rebased at every KWin release, 6.8.x and then 6.9 (§1.5).
  - Colour management and HDR interact with the pass.
  - Scanlines cost up to 45% contrast on one row in three, so Subtle is the preset the tour offers.
  - Unknown whether ScreenShot2, Spectacle and screencasts capture the pass.
- **Upstream.** Propose a generic "Custom screen shader" effect that loads a GLSL file plus a uniforms file, about 500 LOC. Once it is merged, KDOS's effect becomes data and the patch goes.
- [ ] Port `kdos-crt-pass.c` GLSL to `ports/core/<kde-shelf>/kwin/kdos-crt.patch` (Subject: "plugins: add kdos_crt full-screen CRT effect"), before kdos-comp is deleted. Gate: `git log --diff-filter=D -- src/desktop/kdos-comp` is empty while the patch's shader diff names every uniform in kdos-crt-pass.c; `python3 testing/kfscheck.py` passes with the patch in the kwin `kfs.md` Patches section.
- [ ] Implement the presets and the rules above. Gate: plain rig (llvmpipe), `qdbus org.kde.KWin /Effects isEffectSupported kdos_crt` returns false; `vnc-shot.py --gl` (virgl) returns true, and a shot per preset shows scanlines only on Retro [measured: testing/vnc-shot.py:23-31 `--gl` gives virtio-vga-gl].
- [ ] Measure the capture question. Gate: a Spectacle full-screen shot and an OBS/portal screencast taken under Retro on `--gl`, with the result written into design-language.md as a rule. Recommendation: captures exclude the pass, so screenshots stay true to pixels.
- [ ] Measure the fullscreen scanout release. Gate: mpv fullscreen on `make run-hw`; `KWIN_DRM` debug shows the direct-scanout path.
- [ ] Implement `kdos phosphor on|off|status` and its KRunner verb (7.5.1). Gate: on, off, on round-trips the accent, Konsole profile and fonts byte-identically (`diff` of kdeglobals, kwinrc and konsolerc before and after).
- [ ] Docs: design-language.md (Phosphor mode replaces the phosphor pass section), 02-user-guide/theming.md, kdos-command.md `kdos phosphor`, accessibility.md (photosensitivity, reduced motion). Gate: `bash testing/docscheck.sh && python3 testing/anchorcheck.py` pass; a grep of the old surface name in each named page is empty.
- [ ] Upstream MR: the generic screen-shader effect on invent.kde.org/plasma/kwin. Gate: MR URL recorded in the kwin `kfs.md`.

#### 7.2.2 CRT power-cycle motion

- **What the user sees.** Inside Phosphor mode, the desktop unfolds from a horizontal line at login, the lock screen arrives with a collapse, and logout and shutdown power down like a monitor.
- **How it is built.** Three KWin JS scripted effects in KPackage form, using `addFragmentShader` and `setUniform` [measured: scriptedeffect.h:167-169], which carry no C++ ABI tie:
  - `kdos_login` animates the ksplashqml window closing; the stock `login` effect does only a fade on `ksplashqml` [source: kwin src/plugins/login/package/contents/code/main.js];
  - `kdos_lock` animates the lock-screen window on `screenAboutToLock` [measured: effecthandler.h:955,962];
  - `kdos_quit` replaces `sessionquit`.

  The stock `login` and `sessionquit` are disabled in `kwinrc [Plugins]` only while Phosphor mode is on.
- **Limit (verdict).** A JS effect has no `paintScreen`, and shaders reach only individual windows through `animate()`/`set()` [measured: scriptedeffect.cpp:131-132, animationeffect.h:378]. KWin gives effects no DPMS or screen-blank signal. A coherent whole-screen collapse, and anything tied to the screen blanking, therefore uses `kdos_crt`'s `u_collapse` uniform (7.2.1) driven over D-Bus, not a JS effect.
- **Builds on.** kdos-comp's `u_collapse`/`u_fx` uniforms. kdos-splash's power-off collapse (boot-and-init.md:530-600).
- **Size / risk / priority / upstream.** ~300 JS + ~120 GLSL. Low to medium risk: the JS effect API is stable in practice. P2. Not upstreamable.
- [ ] Write the three effects. Gate: `--gl` rig shots taken without sleep at 0/150/300 ms of the lock (memory: a shot costs ~15 s, so drive the timing with `--cmd` and `--shot` in one call); on the plain rig none load (`supported()` false through `animationsSupported()`).
- [ ] Wire the whole-screen collapse for logout and lock to `kdos_crt` over D-Bus (`org.kde.KWin /Effects` plus an effect-exported method in the patch). Gate: `--gl` shot mid-logout shows the collapse across the panel and every window.
- [ ] Docs: design-language.md (the motion language: what moves and when, reduced motion). Gate: `bash testing/docscheck.sh && python3 testing/anchorcheck.py` pass; a grep of the old surface name in each named page is empty.

#### 7.2.3 Accent everywhere in one action

- **What the user sees.** One click on the Accent page, `kdos theme amber`, or KRunner `accent amber` repaints the whole machine in about a second, in both Modern and Phosphor mode:
  - the Plasma colours and global theme, the icons (via the accent class), the cursor and GTK;
  - every open Konsole;
  - the lock screen, the SDDM greeter and KSplash;
  - kdos-splash, Limine's menu and `/etc/vtrgb`;
  - the wallpaper, if it is a KDOS one.

  No other distribution carries the accent below the desktop [inferred: Omarchy's theme switch stops at the session; source: https://learn.omacom.io/2/the-omarchy-manual].
- **How it is built.** §5.3 defines the ordered steps; this item is the native surface and the parts that are new.
  - **Accent page.** A KQuickConfigModule `kcm_kdos_accent` under Colours & Themes, with the 8 named swatches (PHOSPHOR, AMBER…) and a live preview.
  - **Global theme ids.** One global theme per accent (`org.kdos.<accent>.desktop`). `plasma-apply-colorscheme` refuses a name that is already set [measured: plasma-apply-colorscheme.cpp:77-80], while `plasma-apply-lookandfeel -a` re-applies even an unchanged id [measured: lnftool.cpp:75-111].
  - **Live Konsole repaint.** `Session.setProfile(KDOS-<Accent>)` on every `org.kde.konsole-*` service.
  - **Wallpaper.** Set explicitly with `plasma-apply-wallpaperimage`. The image plugin computes its default once per instance [measured: mediaproxy.cpp:67-75], so an accent switch alone leaves the old wallpaper up until plasmashell restarts.
  - **Below the desktop.** Greeter, splash, Limine and vtrgb go through the existing `kdos-power accent` verb (kdos-command.md:292-320), moved behind its §3.5 polkit action.
- **Builds on.** libkcolor and the kdos-theme generators (`src/art/kdos-theme`). The accent step of `kdos-power`.
- **Risk.** Medium:
  - two GTK writers if kde-gtk-config and kdos-theme both write `gtk.css` (§5.2 decides);
  - the selection-contrast fix (WCAG AA) must land first (§5.1);
  - GTK and libadwaita icons stay Breeze blue, because only KIconLoader recolours [measured: kiconthemes 6.30.0 kiconloader.cpp:719, :1342-1348].
- **Priority / upstream.** P1. Not upstreamable.
- [ ] `src/desktop/kcm-kdos-accent` (C++ + QML), calling the same code path as `kdos theme` (one implementation; the KCM execs the verb through the argument-vector builder). Gate: QtTest selects each of the 8 accents and asserts kdeglobals `ColorScheme=KDOS<Accent>`; `--gl` rig shots of Konsole, Dolphin, the lock screen and the SDDM greeter per accent; the contrast script from §5 passes on every accent.
- [ ] Measure the end-to-end switch time. Gate: under 1.5 s from click to the last Konsole repaint on `make run-hw` (timestamped `dbus-monitor`).
- [ ] Docs: theming.md, kdos-command.md `kdos theme`, design-language.md (the accent slots). Gate: `bash testing/docscheck.sh && python3 testing/anchorcheck.py` pass; a grep of the old surface name in each named page is empty.

#### 7.2.4 One continuous power-on

- **What the user sees.** Power button to desktop as one animation, with no blinks:
  1. Limine in accent colours;
  2. kdos-splash on fb0;
  3. kdos-splash's collapse to black;
  4. the SDDM greeter fading in from black in the same accent;
  5. KSplash continuing kdos-splash's layout (mark, stage list, progress bar);
  6. the desktop.
- **How it is built.**
  - **Black at every handover.** Every stage ends on full black and the next starts from black, so KWin's first atomic commit with `ALLOW_MODESET` [measured: kwin drm_commit.cpp:164,182] falls inside a black frame. Users report 2–4 s of black when KWin starts [source: https://bugs.kde.org/show_bug.cgi?id=514351].
  - **Splash continuity.** KSplash QML in each global theme's `org.kdos.<accent>.desktop/contents/splash/Splash.qml` redraws kdos-splash's settled frame. KSplash sees stages 1–6 and imports only QtQuick and Kirigami [measured: res-branding; ksplash splashapp.cpp:28-35].
  - **Avoiding the second modeset.** The greeter KWin and the session KWin get the same KScreen config and bpc, so the second start can be a fastset. Set `KWIN_DRM_PREFER_COLOR_DEPTH=24` [measured: drm_output.cpp:388] if a bpc change forces a full modeset [inferred].
  - **Mode.** kdos-splash's motion follows the mode: a flat mark in Modern, the CRT power-on in Phosphor. That needs a new kdos-splash input [inferred; kdos-splash has no such flag today; measure `src/art/kdos-splash`].
- **Builds on.** kdos-splash (stays, `src/art/kdos-splash`), the Limine theme, `/etc/kdos/accent`.
- **Risk.** Medium: modeset behaviour is driver-specific (i915 fastset, amdgpu), and VMs differ. fbcon drawing over the splash on installed machines is a known gap that must be closed here (boot-and-init.md:530-600).
- **Priority / upstream.** P1. Not upstreamable.
- [ ] KSplash QML matching kdos-splash's last frame. Gate: rig shot of kdos-splash's final frame and KSplash's first frame, cropped and diffed; layout offset ≤ 2 px at 1x and 2x.
- [ ] SDDM theme fade from black (§5.2 designs the theme). Gate: a 10 fps frame capture across greeter start (`vnc-shot.py` burst) contains no non-black frame before the greeter's first.
- [ ] Measure modesets on real hardware (i915 + amdgpu). Gate: `drm.debug=0x10` log counts modesets per boot; target one full modeset after kdos-splash.
- [ ] Docs: boot-and-init.md (the splash and the handover), design-language.md. Gate: `bash testing/docscheck.sh && python3 testing/anchorcheck.py` pass; a grep of the old surface name in each named page is empty.

#### 7.2.5 KDOS sound theme

- **What the user hears.** A KDOS login thump, notify, error and degauss, instead of Ocean.
- **How it is built.** `kdos-sfx render <dir>` writes OGG files under freedesktop names (`desktop-login`, `desktop-logout`, `message-new-instant`, `dialog-error`, `dialog-warning`, `complete`, `device-added`, `device-removed`) into `/usr/share/sounds/kdos/stereo/`, at build time. `index.theme` sets `Inherits=ocean,freedesktop`, and kdeglobals sets `[Sounds] Theme=kdos`. libcanberra plays them through knotifications [measured: res-branding, calibrationtool.cpp:313-321]. Startup and exit sounds are enabled in an xdg `plasma_workspace.notifyrc`, as Fedora ships it.
- **Builds on.** `src/system/kdos-tools/sfx.c` (282 lines, stays [measured: wc -l]).
- **Size / risk / priority / upstream.** ~150 C + data. Low risk. P2. Not upstreamable.
- [ ] Add the render subcommand and a data port. Gate: `canberra-gtk-play -i desktop-login` under the session plays the KDOS file (`pw-dump` shows the stream with media.filename under /usr/share/sounds/kdos).
- [ ] Docs: theming.md, kdos-command.md (`kdos-sfx`). Gate: `bash testing/docscheck.sh && python3 testing/anchorcheck.py` pass; a grep of the old surface name in each named page is empty.

#### 7.2.6 The lock screen as the login

- **What the user sees.** On a single-user machine with an encrypted root: power on, type the LUKS passphrase once, and the desktop is already loaded behind it. Every later unlock uses the same screen.
- **How it is built.** SDDM autologin once per boot (§3.4) plus `kscreenlockerrc [Daemon] LockOnStart=true` [measured: kscreenlockersettings.kcfg:36-39; ksldapp.cpp:282-285]. That removes the greeter's compositor start, and so one modeset. With an encrypted root the user has already authenticated in the initramfs, so the lock can be left off, as macOS does after FileVault [inferred]. kwallet-pam cannot unlock at autologin, so the wallet unlocks at first unlock or is passwordless; §3.8 decides.
- **Risk.** Medium (security model): session processes start before authentication. It is offered only by the installer (D12), only for one user, and only when the root is LUKS; otherwise SDDM greets normally. Unknown whether a desktop frame shows before the lock is up [measured gap: verdict on I8].
- **Priority / upstream.** P2. Not upstreamable. Recorded as a §1.3 default to confirm.
- [ ] Measure pre-lock exposure. Gate: a `--gl` rig burst capture at session start shows no desktop pixels before the lock.
- [ ] kinstall answer key and SDDM config writer. Gate: unattended install with the key → reboot → the rig shows the lock screen and `loginctl show-session` reports `LockedHint=yes`.
- [ ] Docs: security-model.md (who is authenticated when), kinstall.md, boot-and-init.md. Gate: `bash testing/docscheck.sh && python3 testing/anchorcheck.py` pass; a grep of the old surface name in each named page is empty.

### 7.3 Trust and safety

#### 7.3.1 Box colour = security domain

- **What the user sees.** Boxed Firefox has an amber titlebar and host Firefox does not, so it is always clear which one is in front. This is Qubes's domain colour, in a desktop distribution.
- **How it is built.**
  - **One rule per box.** genlaunchers writes one KWin rule per box into a KDOS-owned `kwinrulesrc` group: `wmclass` exact-match on the box name `app.<id>`, plus `decocolor=KDOS<BoxAccent>` as a force rule [measured: rulesettings.kcfg:338-344, rules.cpp:148-150, 306-313].
  - **Why per box, not per app.** KWin rewrites both `resourceClass` and `desktopFileName` to the security-context app id whenever the client's app_id does not start with it. That happens for nearly every boxed window, because boxes are named `app.<id>` [measured: xdgshellwindow.cpp:847-862; kdos-appbox.md:116]. A per-app-id rule therefore never matches.
  - **CSD apps.** Client-side-decorated apps get 6.8's CSD outline rule in the accent instead [source: https://kde.org/announcements/plasma/6/6.8.0/].
  - **No accent.** A box with no `accent` gets a neutral `KDOSBox` scheme.
- **Builds on.** The box profile `accent` key (kdos-appbox.md:854-900). genlaunchers (`src/system/kdos-appbox/launchers.c`). §6.3's `app.<id>.desktop` entries.
- **Size / risk / priority / upstream.** ~200 C. Low risk. P1. Not upstreamable.
- **Trap.** When kdos-appbox falls back to the untagged session socket (kdos-appbox.md:556), the rewrite does not happen and the tint silently vanishes. 7.3.3 makes that visible.
- [ ] Rule writer in genlaunchers. Gate: rig with two boxes: crop the titlebar of each and the host window, then pixel-scan (memory: crop, NEAREST 4x); the colours equal each box's accent Decoration colour.
- [ ] Docs: kdos-appbox.md (`accent`), packs-and-boxes.md, security-model.md. Gate: `bash testing/docscheck.sh && python3 testing/anchorcheck.py` pass; a grep of the old surface name in each named page is empty.

#### 7.3.2 Sandbox badge in title bars and Task Manager tooltips

- **What the user sees.** A small box glyph before the caption of every confined window, and "In box: www" in the task tooltip. Flatpak windows get the same badge with their engine name.
- **How it is built.** KWin already stores the sandboxed flag and `securityContextAppId` [measured: clientconnection.cpp:191-195; xdgshellwindow.cpp:847-861]. Four patches:
  - kwin: `Window` Q_PROPERTY `sandboxEngine`/`sandboxAppId`, ~80 LOC;
  - kdecoration: `DecoratedWindow` accessors, ~40;
  - breeze: draw the glyph, ~60;
  - libtaskmanager: the tooltip line, ~40.

  The KDOS decoration (§5.2) draws the same glyph.
- **Builds on.** kdos-boxsock's security-context tag (engine `io.kdos.appbox`, app_id = box) [measured: src/desktop/kdos-boxsock/main.c:67,289-290]. kdos-boxsock is re-homed (D1, §2.2).
- **Risk.** Medium: the API additions need upstream acceptance. Until then they are carried patches across four modules, rebased each release.
- **Priority / upstream.** P1. Upstreamable: yes. No existing upstream bug for a sandbox indicator was found [source: res-innovation search].
- [ ] Write the four patches, each with a `Subject:` line. Gate: `testing/preflight.sh` patch-header check passes; rig shot of a boxed window shows the glyph, and the host window does not.
- [ ] Open the upstream MRs. Gate: URLs recorded in each port's `kfs.md` Patches section.
- [ ] Docs: security-model.md (what the badge means and does not mean), packs-and-boxes.md. Gate: `bash testing/docscheck.sh && python3 testing/anchorcheck.py` pass; a grep of the old surface name in each named page is empty.

#### 7.3.3 Honest confinement indicator

- **What the user sees.** The badge has three states, and the tooltip says which one applies:
  - **Confined:** tagged, with a private runtime dir that has no `wayland-0`, and the bus proxied.
  - **Tagged:** the KWin denylist applies, but the box could still reach the session socket.
  - **Not confined:** the box fell back to the untagged socket. The badge is replaced by a warning chip, and the box's titlebar tint (7.3.1) is replaced by the warning colour through the CSD outline rule.

  "Confined" in the KDOS UI then means what it says.
- **Why.** The Wayland tag is advisory today:
  - a box shares `$XDG_RUNTIME_DIR` and can open `wayland-0` untagged [measured: security-model.md:712-714];
  - kdos-appbox falls back to it after 1 s [measured: kdos-appbox.md:556];
  - boxes get the raw session bus;
  - an authorisation check on ScreenShot2 is unlikely upstream, because KWin dropped `X-KDE-DBUS-Restricted-Interfaces` on purpose and expects sandboxes to filter the bus [measured: kwin commit 0ea31ae7, 2026-06-18].

  §6.3 builds the real boundary (private runtime dir, xdg-dbus-proxy). This item reports which boundary actually held.
- **How it is built.** kdos-appbox records the state per running box in `/run/user/<uid>/kdos/boxes/<name>.state`. A KWin rule written beside 7.3.1's carries the state to the decoration through the 7.3.2 property (`sandboxEngine` suffix, e.g. `io.kdos.appbox+private`) [inferred: needs the 7.3.2 patch to pass a structured engine string].
- **Size / risk / priority / upstream.** ~250 C + QML. Low risk. P1. The states map to Flatpak too, so the property design is upstreamable; KDOS's labels are not.
- [ ] Record the states in kdos-appbox. Gate: selftest forces each path (boxsock up, boxsock timed out, private runtime dir) and asserts the state file.
- [ ] Badge states in breeze and the KDOS decoration. Gate: three rig shots, one per state.
- [ ] Docs: security-model.md is rewritten to state the three states plainly (hard rule 1: the page today calls the tag confinement). Gate: `bash testing/docscheck.sh && python3 testing/anchorcheck.py` pass; a grep of the old surface name in each named page is empty.

#### 7.3.4 Privacy lamps that name the box

- **What the user sees.** Three tray lamps: microphone, camera and screen. Each lights only while something is capturing, and names it ("Firefox in box www"). Clicking a lamp mutes or stops the capture. Plasma's own mic indicator and screencast icon never name the box, and Plasma has no camera indicator [inferred from plasma-pa and plasma-workspace 6.8.2; measure].
- **How it is built.** A QML plasmoid `src/desktop/plasmoid-kdos-privacy` over a C helper, `kdos privacy --json`, which reads:
  - PipeWire stream state for the microphone and the screen;
  - `/dev/video*` holders plus PipeWire camera nodes for the camera;
  - the portal screencast sessions;
  - the process's box from its cgroup (§6.3 scope naming).

  The plasmoid replaces Plasma's microphone indicator, which is hidden in the tray config, so there are never two microphone icons.
- **Builds on.** The three lamps of today's panel (desktop.md:881-897), re-homed as a plasmoid.
- **Size / risk / priority / upstream.** ~300 QML + ~300 C. Low risk. P1. A camera indicator could be proposed to plasma-workspace.
- [ ] Helper and plasmoid. Gate: rig: `pw-record` inside a box lights the mic lamp with the box name; the lamp goes dark within 1 s of stop.
- [ ] Docs: desktop.md (privacy lamps), security-model.md. Gate: `bash testing/docscheck.sh && python3 testing/anchorcheck.py` pass; a grep of the old surface name in each named page is empty.

#### 7.3.5 "Open in a throwaway box"

- **What the user sees.** In Dolphin and the file dialog, right-click → "Open in a throwaway box". The file opens in its app inside a fresh box with no network, and the box disappears when the window closes. This is the Qubes disposable-VM / Windows Sandbox pattern.
- **How it is built.** A KIO service menu `/usr/share/kio/servicemenus/kdos-disposable.desktop` → `kdos-appbox throwaway <pack> <file>`, which:
  - starts an ephemeral box with `network = none` and a tmpfs writable layer;
  - passes only that file through the document portal;
  - destroys the box when its last window closes.

  `persistence = ephemeral` is recorded but not enforced today [measured: kdos-appbox.md:854-936], so enforcement lands first.
- **Size / risk / priority / upstream.** ~300 C + 1 file. Medium risk (portal document handling in boxes). P2. Not upstreamable.
- [ ] Enforce `persistence = ephemeral`. Gate: selftest: a write inside an ephemeral box is gone after stop (`kdos-box ls` shows no box; the layer directory is absent).
- [ ] Add the service menu. Gate: rig: open a PDF throwaway; `ss -tnp` inside the box shows no sockets; the box is gone 2 s after the window closes.
- [ ] Docs: kdos-appbox.md, packs-and-boxes.md, applications.md. Gate: `bash testing/docscheck.sh && python3 testing/anchorcheck.py` pass; a grep of the old surface name in each named page is empty.

#### 7.3.6 "Use previous version" everywhere

- **What the user sees.**
  - **System Versions page.** It lists slot A and slot B with version, build date, tree commit and state (running, confirmed, trial, failed), plus a "Start this version next time" button.
  - **Logout screen.** It says "When restarted, the computer will switch to KDOS 0.2.13 (slot B)".
  - **Rollback notice.** After an automatic rollback, the first session says "Your last update didn't start, so KDOS went back to 0.2.13", with a Details link.
  - **Limine** lists both slots, as it does today.
- **How it is built.**
  - **The page.** A KCM `src/desktop/kcm-kdos-system`. Its button calls `kdos-bootctl try <slot> 1` through kdos-sysupdated's polkit-guarded D-Bus method, not a shell.
  - **The logout message.** Plasma reads login1 `RebootToBootLoaderEntry`, then runs `bootctl list --json=short` by bare name and shows the entry's `showTitle` [measured: plasma-workspace 6.8.2 shutdowndlg.cpp:143-191; LogoutControls.qml:166-168]. Under elogind, `kdos-bootctl try` writes `/run/systemd/reboot-to-boot-loader-entry`, and elogind runs with `SYSTEMD_REBOOT_TO_BOOT_LOADER_ENTRY=1` [measured: elogind master (cloned 2026-10-05) logind-dbus.c:3843-3867; the pinned 255.24 is re-measured in §3.2's elogind task].
  - **No systemd tool name on PATH (rule 3).** The entry title comes from §6.2.4's carried `ports/core/kde/plasma-workspace/kdos-bootctl-entry.patch`, which makes shutdowndlg call `kdos-bootctl list --json=short`. No binary named `bootctl` ships, under any path. This item adds no code of its own for the message.
  - **Session-confirmed slots.** `mark-good` moves out of rcS [measured: fs/etc/init.d/rcS:188]. A slot is confirmed only after KWin, plasmashell and a responsive `org.kde.plasmashell` have run for 60 s, or after the greeter has been up for 60 s on a machine that boots to the greeter. The two markers are ANDed per §6.2. A Plasma that crashes on a trial slot then rolls back by itself.
- **Builds on.** kdos-bootctl status/try/mark-good (kdos-command.md:1551-1570). The A/B slots (boot-and-init.md:664-700). The pattern matches Haiku's boot-time "select system state" [source: https://www.haiku-os.org/docs/userguide/en/bootloader].
- **Size / risk / priority / upstream.** KCM ~400 C++ + 500 QML; health hook ~100 C. Low to medium risk. P1. Not upstreamable.
- [ ] The logout sentence, through §6.2.4's patch and the elogind environment (no task here beyond its gate). Gate: arm a try on the rig, open the logout screen, and a cropped shot shows the slot sentence; `busctl get-property org.freedesktop.login1 /org/freedesktop/login1 org.freedesktop.login1.Manager RebootToBootLoaderEntry` returns the entry after `kdos-bootctl try`.
- [ ] Move mark-good to the session-health hook. Gate: rig: install a trial slot whose plasmashell is replaced by `/bin/false`; the next boot is on the old slot and the rollback notice shows.
- [ ] Add the System Versions KCM. Gate: QtTest against a fake `kdos-bootctl status --json`; rig shot.
- [ ] Docs: boot-and-init.md (counting and confirming), kdos-command.md (`kdos-bootctl`), administration.md. Gate: `bash testing/docscheck.sh && python3 testing/anchorcheck.py` pass; a grep of the old surface name in each named page is empty.

#### 7.3.7 Signed updates surfaced

- **What the user sees.**
  - **Discover.** The KDOS system update shows "Signed by the KDOS release key (ed25519 ab12…cd34), verified on this machine" above the release notes.
  - **Refused updates.** An update that fails verification is refused with that reason in Discover's error banner; it is never "failed to install".
  - **About this System.** It shows the key and when the last update was verified.
- **How it is built.** kdos-sysupdated (§6.2) verifies before `Install`, which deploys the image into the inactive slot (D11 end state) or the binhost packages (stepping stone). Signatures are Ed25519 through Monocypher [measured: packaging.md:1222, :438].
  - **Where the text goes.** Into the generated AppStream file that `GetAppStream` returns as `file://`. Discover fetches it with `QNetworkAccessManager::get` [measured: SystemdSysupdateBackend.cpp:161]; whether `file://` works is inferred from Qt and must be measured.
  - **Failure path.** The error string travels as the Job's failure reason.
  - **Update size.** Discover asks a hard-coded `http://localhost:3129/v1/updatesize?version=…` [measured: SystemdSysupdateResource.cpp:98]. §6.2.2 owns whether it is served; this item assumes the recommendation made there: an unprivileged helper of kdos-sysupdated (own user, no capabilities) bound to 127.0.0.1:3129 only, parsing nothing but `version=`, answering the delta size from the already verified signed manifest and fetching nothing. Loopback HTTP is not a download (01 §1.4.1, rule-3 text). It is reachable by every local process and by any box sharing the host network namespace, and it serves one integer from a public manifest; security-model.md lists it. If §6.2.2 keeps it unserved, Discover shows size 0 and this bullet and its task go.
  - **Source builds** (`kdos update apply --source`) are not offered in Discover (D11). The page links the CLI.
- **Size / risk / priority / upstream.** ~150 C. Low risk. P1. Not upstreamable (it uses the upstream interface).
- [ ] Put the signature block in the AppStream file. Gate: rig: Discover's update page shows the fingerprint; a tampered image (flipped byte) produces the refusal text.
- [ ] Add the 127.0.0.1:3129 size helper, if §6.2.2 serves it. Gate: Discover shows the size the manifest records (`curl 127.0.0.1:3129/v1/updatesize?version=X` equals the shown value); `ss -ltnp` shows it bound to 127.0.0.1 only, owned by the helper's user; any path other than `/v1/updatesize` or any query key other than `version` gets 404.
- [ ] Docs: packaging.md (signing), security-model.md (update trust; the loopback size helper), kdos-command.md (`kdos update`). Gate: `bash testing/docscheck.sh && python3 testing/anchorcheck.py` pass; a grep of the old surface name in each named page is empty.

#### 7.3.8 "Allow on this network?"

- **What the user sees.** When something starts listening behind the KDOS firewall, the user is asked. Examples: KDE Connect pairing, krdp or krfb starting, kiwix-serve asked to serve the LAN. The prompt reads, for instance, "KDE Connect can't be reached by your phone. Allow on this network?". Allow opens the firewall by name, never by port. This is macOS's incoming-connections prompt.
- **How it is built.** The notices daemon `kdos-notice` (7.6.3, §6.5) watches the per-name drop counters in the KDOS nftables table, and the services' own D-Bus signals where they have them (kdeconnect pairing request). Allow calls kdos-powerd's firewall verb behind its §3.5 polkit action [measured: daemons.md:391-427 names services, with `kdeconnect` = TCP/UDP 1714-1764].
- **Size / risk / priority / upstream.** ~300 C. Medium risk: detection heuristics, and prompt fatigue (one prompt per name per network, remembered per NetworkManager connection UUID). P2. Not upstreamable.
- [ ] Counter-based detection. Gate: rig with a second VM sending to 1714; one notification appears and none on repeat.
- [ ] Docs: daemons.md (firewall), security-model.md. Gate: `bash testing/docscheck.sh && python3 testing/anchorcheck.py` pass; a grep of the old surface name in each named page is empty.

### 7.4 Offline-first

#### 7.4.1 KDOS Library in KRunner

- **What the user sees.** Typing "photosynthesis" shows the offline Wikipedia article, the KDE handbook page, a Kolibri course and a Zeal docset hit, with no network. A leading `?` limits results to the library.
- **How it is built.**
  - **The runner.** A D-Bus runner `src/desktop/kdos-library-runner` (C on basu, interface `org.kde.krunner1`), declared in `/usr/share/krunner/dbusplugins/kdos-library.desktop` with `X-Plasma-API=DBus2` [measured: krunner 6.30.0 dbusrunner.cpp:30-42, :96; runnermanager.cpp:192].
  - **What it queries.**
    - kiwix-serve on 127.0.0.1:8080 for title suggestions;
    - Kolibri on :8081;
    - the KDOS and KFS book indexes;
    - `kdos-*` man pages;
    - Zeal docsets;
    - Organic Maps places through a `geo:` open.

    The service ports come from boot-and-init.md:1216-1232.
  - **Results** open in Kiwix, KHelpCenter, Zeal, the browser or Organic Maps.
- **Builds on.** The ported library stack: kiwix-tools, kiwix-desktop, kolibri, zeal and organicmaps [measured: ls ports/core/*/]. The books at `/usr/share/doc/kdos` and `/usr/share/doc/kfs`.
- **Size / risk / priority / upstream.** ~700 C. Low risk. P1. Not upstreamable. krunner is not yet ported (§4).
- [ ] Write the runner. Gate: `qdbus` Match call returns ≥1 result per source on a rig image with a test ZIM; with networking down (`ip link set eth0 down`) the results are identical.
- [ ] Docs: applications.md (the offline library), kdos-command.md if a verb is added. Gate: `bash testing/docscheck.sh && python3 testing/anchorcheck.py` pass; a grep of the old surface name in each named page is empty.

#### 7.4.2 `library:/` in Dolphin and a Library category in Kickoff

- **What the user sees.** "Library" in Dolphin's Places, holding Books (the ZIMs, the KDOS and KFS books), Maps, Models (gguf, whisper, Piper voices), Docsets and Courses. Each item shows its size and its pack. Kickoff has a Library category with one launcher per item, for example "Wikipedia (offline)".
- **How it is built.** A KIO worker `src/desktop/kio-library` (KF6-stable worker API) listing what data packs install. A menu file `/etc/xdg/menus/applications-merged/kdos-library.menu`. Per-item `.desktop` files written by each data pack.
- **Size / risk / priority / upstream.** ~800 C++. Low risk. P2. Not upstreamable.
- [ ] Write the worker. Gate: `kioclient6 ls library:/Books` lists the test ZIM; rig shot of Places.
- [ ] Docs: applications.md. Gate: `bash testing/docscheck.sh && python3 testing/anchorcheck.py` pass; a grep of the old surface name in each named page is empty.

#### 7.4.3 The KDOS and KFS books in KHelpCenter

- **What the user sees.** Help Center opens on "KDOS" first, fully offline. The Help button of every KDOS KCM opens the right book section, and KDE's own handbooks stay available through `help:/`.
- **How it is built.** Two plugin files in `/usr/share/khelpcenter/plugins/`:
  - `kdos.desktop`: `X-DocPath=file:///usr/share/doc/kdos/index.html`, `X-DOC-Weight=-100`, `Icon=distributor-logo-kdos`;
  - `kfs.desktop`: the same for `/usr/share/doc/kfs`.

  Children sort by ascending weight, so -100 comes first [measured: khelpcenter 26.08.2 docentry.cpp:213-237, :263-270]. Local HTML renders in-app through QtWebEngine [measured: mainwindow.cpp:310-317]. KDOS KCMs set `X-DocPath` to `kdos/<page>.html#<anchor>`, a `help:/kdos/…` address KHelpCenter opens, served from the book through the `/usr/share/doc/HTML/en/kdos` link kdos-doc installs (ruling 8, §8.5). KDE handbooks survive the prune because `usr/share/doc/HTML/en` is kept [measured: prune.c:205-209]. The book CSS gets a KHelpCenter variant in `src/system/kdos-doc`.
- **Size / risk / priority / upstream.** 2 files + ~50 CSS. Low risk. P1. Not upstreamable. khelpcenter is not ported (§4); qt6-qtwebengine is.
- [ ] Plugin files and CSS. Gate: rig shot of KHelpCenter's tree with KDOS on top; `python3 testing/anchorcheck.py` covers every `X-DocPath` anchor written by KDOS KCMs (extend the check).
- [ ] Docs: kdos-command.md (where the book lives), 05-developer/writing-desktop-software.md (a KCM sets `X-DocPath`). Gate: `bash testing/docscheck.sh && python3 testing/anchorcheck.py` pass; a grep of the old surface name in each named page is empty.

#### 7.4.4 Curated offline add-on pack in place of "Get New…"

- **What the user sees.** No "Get New…" button anywhere. In its place, the Global Theme, Wallpaper, Colours, Plasma Style, KWin Scripts and Konsole pages already list a curated KDOS set of extra wallpapers, colour schemes, Konsole schemes, KWin scripts and a few plasmoids, from a pack on the medium. KDOS reviews the set and Discover installs it like any other pack.
- **How it is built.**
  - **Hiding "Get New…".** `/etc/xdg/kdeglobals [KDE Action Restrictions][$i] ghns=false`. KNewStuff hides its action when `KAuthorized::authorize(KAuthorized::GHNS)` fails [measured: knewstuff 6.30.0 src/widgets/action.cpp:34-42; qtquick Action.qml `enabled: NewStuff.Settings.allowedByKiosk`].
  - **Discover's KNS backend** is OFF (§4.2, D10).
  - **The pack** `kdos-addons` installs into the standard XDG dirs (`/usr/share/wallpapers`, `/usr/share/color-schemes`, `/usr/share/konsole`, `/usr/share/kwin/scripts`, `/usr/share/plasma/plasmoids`), so every stock page lists its contents with no code.
  - **Licences.** Only items with a licence that permits redistribution, each recorded in the pack's `kfs.md`.
- **Builds on.** The pack format (packs-and-boxes.md) and Discover's pack backend (§6.2).
- **Risk.** Low. Third-party plasmoids break across Plasma releases [measured: res-branding pitfall, Manjaro Breath bug 482637], so the pack carries no third-party QML plasmoid without a rig test per Plasma bump. Wallpapers, colour schemes and JS KWin scripts are low-churn.
- **Size / priority / upstream.** Pack plus Kiosk keys. P1. Not upstreamable.
- [ ] Kiosk key. Gate: rig: the Wallpaper, Global Theme and KWin Scripts pages show no "Get New" control (cropped shots); `grep -r ghns` in `~/.config` after use is empty.
- [ ] `kdos-addons` pack with an initial curated set. Gate: `kpkg` install, then each page lists the items; preflight licence fields present.
- [ ] Docs: theming.md, applications.md, principles.md ("nothing downloads at run time" names KNewStuff explicitly). Gate: `bash testing/docscheck.sh && python3 testing/anchorcheck.py` pass; a grep of the old surface name in each named page is empty.

#### 7.4.5 Local AI, OCR and speech: opt-in

- **What the user sees.** Nothing, until a model pack is installed. Then:
  - KRunner `ask <question>` streams an answer from the local llama-server;
  - Spectacle's text recognition works;
  - Dolphin offers "Transcribe" on audio files;
  - "Read selection aloud" appears as a Klipper action.

  Nothing leaves the machine.
- **How it is built.**
  - an `ask` verb in 7.4.1's runner, calling the OpenAI-compatible llama-server on the loopback address only, 127.0.0.1:8082 (boot-and-init.md:1227); no remote endpoint is configurable (ruling 10, §8.5);
  - Spectacle built against the ported tesseract [source: https://kde.org/announcements/plasma/6/6.6.0/];
  - a service menu running `whisper-cli` (whisper.cpp and whisper-model-base-en are ported);
  - a Klipper action running piper (ported).
- **Size / risk / priority / upstream.** ~400 C + 2 menus. Risk: scope creep, so every entry is hidden while its model or binary is absent. P2. Not upstreamable.
- [ ] Gate each surface on its model's presence. Gate: on an image without model packs, KRunner `ask` returns no match and the service menu does not appear; with them, one rig run of each.
- [ ] Docs: applications.md (local AI), kdos-command.md (`kdos speech`). Gate: `bash testing/docscheck.sh && python3 testing/anchorcheck.py` pass; a grep of the old surface name in each named page is empty.

### 7.5 Productivity

#### 7.5.1 KRunner as the KDOS command palette

- **What the user sees.** One chord, then type, to do anything KDOS can do:
  - `accent amber`, `phosphor on`, `tiling on`, `night light`;
  - `update check`, `box stop www`, `box enter dev`;
  - `doctor`, `persist`;
  - two-letter codes (`ff` gives Firefox);
  - deep links to KDOS KCMs.

  KRunner's help lists the syntaxes. Search is the reviewers' answer to the depth of the settings tree [source: https://www.xda-developers.com/kde-plasma-became-the-best-linux-desktop-but-its-terrifying-for-new-users/].
- **How it is built.** A D-Bus runner `src/desktop/kdos-runner` (C on basu), with `X-Plasma-Runner-Syntaxes` [measured: dbusrunner.cpp:96]. Actions run through the argument-vector builder, never a shell (Conventions: no `system()`). KRunner keeps Alt+Space, Alt+F2 and Search [measured: org.kde.krunner.desktop.cmake:71] and gains Meta+Space.
- **The collision trap (verdict).**
  - fcitx5 5.1.22 is shipped and binds Super+Space to `EnumerateGroupForwardKeys` by default [measured: fcitx5 globalconfig.cpp:109-115; ports/core/input/fcitx5].
  - KWin's GlobalShortcutFilter runs before the input-method filter [measured: kwin input.cpp:3196 vs :3219], so KRunner would win and fcitx5 group switching would stop silently.
  - Mitigation: ship `/etc/xdg/fcitx5/config` with `[Hotkey/EnumerateGroupForwardKeys]` moved to Super+Shift+Space. Check the key against §5.8's canonical table and fcitx5's own other defaults; measure that fcitx5 reads `/etc/xdg/fcitx5/config` as a system default [inferred].
- **Builds on.** `~/.config/kdos/favorites` (two-letter codes, desktop.md:282-300), menu.conf routes, the `kdos` verbs.
- **Size / risk / priority / upstream.** ~900 C. Low risk. P1. Not upstreamable.
- [ ] Write the runner and the syntaxes. Gate: `qdbus org.kde.krunner /App querySingleRunner kdos "accent amber"` returns a match; the run changes kdeglobals; selftest feeds hostile names (`;`, `$()`) and asserts argv-only execution.
- [ ] fcitx5 config and chord. Gate: rig: Meta+Space opens KRunner and Super+Shift+Space switches the fcitx5 group (`fcitx5-remote -q`, or a D-Bus probe of the current group).
- [ ] Docs: desktop.md (the palette), kdos-command.md, accessibility.md and the input-method page (the fcitx5 key). Gate: `bash testing/docscheck.sh && python3 testing/anchorcheck.py` pass; a grep of the old surface name in each named page is empty.

#### 7.5.2 Key card overlay

- **What the user sees.** Meta+F1 (and Meta+/) shows a full-screen, searchable card of every global chord, grouped into Windows, Desktops, Launch, KDOS and Accessibility. It reflects the user's own changes and closes on Esc. Plasma has no shortcut overview; ChromeOS (Ctrl+Alt+/), macOS and Omarchy (Super+K) do [source: https://learn.omacom.io/2/the-omarchy-manual].
- **How it is built.**
  - **The effect.** A KWin declarativescript effect `src/desktop/kwin-effect-kdos-keys` that registers its shortcut through `ShortcutHandler` [measured: effectloader.cpp:131-135; scripting/shortcuthandler.cpp].
  - **The data.** It reads the live bindings from kglobalacceld with the `DBusCall` QML type, whose replies unwrap generically [measured: dbuscall.cpp:28-44; scriptingutils.cpp:19-60].
  - **Freeing Meta+F1.** KWin binds Meta+F1..F4 and Ctrl+F1..F4 to "Switch to Desktop N" [measured: virtualdesktops.cpp:884-887]. Meta+F1 is taken off desktop 1 in §5.8's table; Ctrl+F1 stays.
  - **Meta+/.** Today it opens the documentation browser (desktop.md:381). Under Plasma, Help is F1 inside apps, so Meta+/ joins the card [recommendation; §5.8 confirms].
- **Builds on.** The key card that is generated from `rc.xml` today (desktop.md:368), re-sourced from kglobalaccel. The chord-table generator of §5.8.
- **Size / risk / priority / upstream.** ~500 QML + ~300 C generator. Low risk (the QML effect API is internal but light). P1. Upstreamable as a generic "Shortcuts overview" effect.
- [ ] Write the effect. Gate: rig shot after Meta+F1; rebinding one action in System Settings → Shortcuts changes the card on the next open (no restart).
- [ ] Upstream MR. Gate: URL in the effect's notes.
- [ ] Docs: desktop.md (shortcut table), window-model.md (Meta+F1 opens the key card; desktop 1 is Ctrl+F1). Gate: `bash testing/docscheck.sh && python3 testing/anchorcheck.py` pass; a grep of the old surface name in each named page is empty.

#### 7.5.3 Desk picker with undo

- **What the user sees.** A Welcome-tour page with three live previews (KDOS, Dock, Tiling) and Apply (D5). It can be reopened later from KRunner (`desk`) or from Global Theme → layout. If the user had customised panels, the picker says so and offers "Undo" for that session.
- **How it is built.**
  - **Applying.** Each desk is a global theme `layouts/org.kde.plasma.desktop-layout.js` calling `loadTemplate` (§5.5). Apply runs `plasma-apply-lookandfeel -a <id> --resetLayout`; it applies the layout only with `--resetLayout` [measured: lnftool.cpp:75-111].
  - **Undo, for the trap.** Applying a layout deletes every group of `plasma-org.kde.plasma.desktop-appletsrc` [measured: shellcorona.cpp:672-692]. The picker copies that file, `plasmashellrc` and `kwinrc`'s tiling keys to `~/.config/kdos/desk-undo/` first. Undo restores them and restarts plasmashell.
  - **Tiling desk.** It also enables 7.5.4.
- **Size / risk / priority / upstream.** ~300 QML + ~100 C. Low risk. P1. Not upstreamable.
- [ ] Picker page and undo. Gate: rig: add a widget, pick Dock, Undo; the widget is back (cropped shot); `diff` of appletsrc equals the saved copy.
- [ ] Docs: desktop.md (desks), getting-started.md. Gate: `bash testing/docscheck.sh && python3 testing/anchorcheck.py` pass; a grep of the old surface name in each named page is empty.

#### 7.5.4 Auto-tiling toggle

- **What the user sees.** Floating windows with smart snap by default (D6): KWin edge and quarter snap, and custom tiles on Meta+T. One toggle ("Auto-tile") turns auto-tiling on; it is offered on the Welcome tour, in KRunner (`tiling on|off`) and in a tray toggle; all call `kdos tiling on|off` (§5.5.3, the one CLI).
- **How it is built.**
  - **The script.** Krohnkite, the maintained fork, ships as a port, disabled [source: codeberg.org/anametologin/krohnkite, MIT, 0.9.10.0 of 2026-10-25; the original esjeon/krohnkite was last pushed 2023-08-09].
  - **The toggle.** `kdos tiling on|off` writes `kwinrc [Plugins] krohnkiteEnabled=true|false` and calls KWin `reconfigure` [inferred; measure that a script toggled this way loads and unloads without a restart].
  - **Per desktop.** Whether Krohnkite can be limited to one virtual desktop is unknown, so measure it. If it cannot, the toggle is global and says so.
- **Builds on.** The KDOS tiling heritage (window-model.md) and KWin's 6.4 per-desktop tiling layouts [source: https://kde.org/announcements/plasma/6/6.4.0/].
- **Size / risk / priority / upstream.** Port + ~80 C. Low to medium risk: a third-party script must be re-tested on each KWin bump. P1. Not upstreamable.
- [ ] Krohnkite port (shelf per §4; `kfs.md` with licence and why the fork). Gate: `kpkg` install, then the KWin Scripts page lists it, disabled.
- [ ] Toggle. Gate: rig: open three windows, toggle on, and the shot shows a tiled layout; toggle off and the windows return to floating.
- [ ] Docs: window-model.md (rewritten around KWin: snap, tiles, the auto-tile toggle), desktop.md. Gate: `bash testing/docscheck.sh && python3 testing/anchorcheck.py` pass; a grep of the old surface name in each named page is empty.

#### 7.5.5 Run-or-raise terminal programs, one key each

- **What the user sees.** The terminal-program keys survive the move to Konsole: Super+Shift+U raises rmpc, Super+Shift+W raises micro, and so on.
- **How it is built.** A KWin JS script `src/desktop/kwin-script-kdos-runraise` with `registerShortcut`. Each key raises the Konsole window whose title or tag matches its program, or else runs `konsole --profile KDOS -e <prog>`. Each program gets a kglobalaccel `.desktop` with `X-KDE-Shortcuts`, so the keys show on the Shortcuts page and on the key card.
- **Trap.** `Super+E` is KDE's Dolphin key; mc moves to another key in §5.8's table. rmpc's album art needs a terminal with image support: measure Konsole's sixel and kitty-graphics support against rmpc [inferred: partial kitty support].
- **Builds on.** desktop.md:403-423.
- **Size / risk / priority / upstream.** ~150 JS + ~10 desktop files. Low risk. P2. Not upstreamable.
- [ ] Write the script. Gate: rig: the key opens, a second press raises the same window (`qdbus org.kde.KWin` window list count unchanged).
- [ ] Docs: desktop.md. Gate: `bash testing/docscheck.sh && python3 testing/anchorcheck.py` pass; a grep of the old surface name in each named page is empty.

#### 7.5.6 Session restore that includes boxes

- **What the user sees.** After a logout or reboot, the apps come back, and so do the boxes that were running, with their windows.
- **How it is built.**
  - **Host apps.** ksmserver's restore, plus KWin's `XdgSessionManagerV1Interface` for the staging xdg-session-management-v1 protocol, which KWin 6.8.2 serves with storage `kwinsession` [measured: kwin src/wayland_server.cpp:348-349; src/wayland/CMakeLists.txt:64]. Which ported apps implement the protocol is unknown; measure it.
  - **Boxes.** A `plasma-workspace/shutdown/kdos-boxes.sh` hook [measured: res-branding, shutdown.cpp:190] records running boxes and their launched apps. An XDG autostart restores them through `kdos-appbox`, not through KDE's restore, which cannot start a box.
- **Size / risk / priority / upstream.** ~200 C. Medium risk: restore on Wayland is partial upstream. P2. Not upstreamable.
- [ ] Measure the restore coverage. Gate: a table, written into session.md, of which default apps (§5.7) restore their windows on the rig.
- [ ] Box record and restore. Gate: rig: start box `www`, log out and in, and `kdos-box ls` shows it running with its window.
- [ ] Docs: session.md, kdos-appbox.md. Gate: `bash testing/docscheck.sh && python3 testing/anchorcheck.py` pass; a grep of the old surface name in each named page is empty.

### 7.6 System

#### 7.6.1 KDOS System Monitor face and page

- **What the user sees.** A "KDOS" page in System Monitor showing:
  - Overview;
  - Boxes, with per-box CPU, memory and IO;
  - Energy, per app, from kdos-energyd;
  - Slots: the running slot, trial state and the other slot's version.

  It uses a KDOS sensor face: accent sparklines with monospace numerals, plus a faint scanline ground in Phosphor mode. The panel's meter strip (§5.5) uses the same face.
- **How it is built.**
  - **Face.** A KPackage at `/usr/share/ksysguard/sensorfaces/org.kdos.meters` [measured: libksysguard faces/facepackages/CMakeLists.txt:3].
  - **Page.** `/usr/share/plasma-systemmonitor/kdos.page`, in the format of `pages/applications.page`.
  - **Sensors.** A ksystemstats plugin `src/desktop/ksystemstats-plugin-kdos` (libksysguard API, version-locked) publishing `kdos/box/<name>/*`, `kdos/energy/*` and `kdos/slot/*`.
- **The Applications page trap (verdict).**
  - libksysguard lists only `.scope`/`.service` nodes under `/user.slice/user-UID.slice/user@UID.service` and reads only each node's own `cgroup.procs` [measured: application_data_model.cpp:20,165; cgroup.cpp:138-143; cgroup_data_model.cpp:428-431].
  - podman puts box processes in a `libpod-<id>` child [measured: podman 6.1.2 container_internal_linux.go:371-373].
  - So the stock Applications page shows boxes only once §6.3 names scopes `app-kdos-app.<id>-<n>.scope` AND either the box processes sit directly in the scope or libksysguard aggregates descendants.
  - Decision (ruling 7, §8.5): libksysguard carries the aggregation patch, so boxes show on System Monitor's stock Applications page. It is upstreamable and also helps nested Flatpak helpers [inferred].
- **Size / risk / priority / upstream.** ~400 QML + ~500 C++. Medium risk: the plugin API is version-locked. P1. The face could go to the KDE Store; the aggregation patch upstream.
- [ ] ksystemstats plugin. Gate: `kstatsviewer kdos/box/www/cpu` prints values while box `www` runs.
- [ ] Face and page. Gate: rig shot of the page per accent.
- [ ] libksysguard descendant-aggregation patch. Gate: the stock Applications page lists "Firefox (box www)" with its icon.
- [ ] Docs: kdos-settings.md (System Monitor face and ksystemstats plugin). Gate: `bash testing/docscheck.sh && python3 testing/anchorcheck.py` pass; a grep of the old surface name in each named page is empty.

#### 7.6.2 About this System and a Health page

- **What the user sees.** About this System answers "which version, which slot, is it healthy?", with lines such as:
  - `System slot: A · 0.2.14 confirmed (previous: B · 0.2.13)`;
  - `Ports tree: <commit>`;
  - `Accent: Bone`;
  - `Update key: ed25519 ab12…`.

  A Health page shows `kdos doctor`.
- **How it is built.** `/etc/xdg/kcm-about-distrorc [General] ExtraSoftwareData=` lists scripts under `/usr/libexec/kdos/about-*` [measured: kinfocenter 6.8.2 main.cpp:171-176]. The Health page is a CommandOutputContext KCM, the 38-line pattern of `kcms/interrupts` [measured].
- **Traps (verdict).**
  - Each script is run twice, synchronously, on the GUI thread (once in the system language and once in English).
  - A script that prints nothing hits `qFatal` and kills kinfocenter, or System Settings when the page is embedded there [measured: ThirdPartyEntry.cpp:11-49].
  - Mitigation: every script prints a label and a value unconditionally (`Unknown` on a single-root machine), reads only cached state files (`/run/kdos/slot.json`), never podman or the disk, and is under 20 lines of POSIX sh.
- **Size / risk / priority / upstream.** ~60 sh + ~150 C++. Low risk. P1. Not upstreamable.
- [ ] Write the scripts. Gate: a selftest runs each with an empty `/run/kdos` and asserts exactly 2 lines and exit 0 within 50 ms.
- [ ] Health KCM. Gate: rig shot; a failing doctor check shows in red.
- [ ] Docs: administration.md, kdos-command.md (`kdos doctor`). Gate: `bash testing/docscheck.sh && python3 testing/anchorcheck.py` pass; a grep of the old surface name in each named page is empty.

#### 7.6.3 Update tray for A/B updates, plus restart and CVE notices

- **What the user sees.**
  - The stock Discover tray icon says "System update available". After Acquire and Install, it says "Restart to update".
  - Calm, persistent notices with actions, only where a decision is needed: "Firefox and Konsole were updated: Restart them" (binhost and source paths only), "3 known vulnerabilities in installed packages: Details", and "Running 0.2.14 on trial".
- **How it is built.**
  - **The tray (verdict).** DiscoverNotifier loads only `discover-notifier/` plugins, and the SystemdSysupdate backend installs none [measured: notifier/BackendNotifierFactory.cpp:24], so stock Discover's tray never learns of a sysupdate1 update.
  - **The notifier.** §6.2.2 owns it: `kdos-discover-notifier`, an out-of-tree `BackendNotifierModule` in `src/desktop/kdos-discover` (~250 C++), which calls `CheckNew` per target and sets `needsReboot`. libdiscover is internal, so it is rebuilt and re-tested on every Plasma minor (§6.8). In parallel, propose the same module upstream as a `SystemdSysupdateNotifier` in Discover's tree, because every sysupdate1 distribution (KDE Linux) has the same gap [inferred]; once merged, the KDOS plugin goes.
  - **The notices.** `src/desktop/kdos-notice` (§6.5; C on basu, `org.freedesktop.Notifications` client; XDG autostart `OnlyShowIn=KDE`) watches `/var/lib/kdos/update.json`, `kdos restarts`, `kdos cve` (offline Alpine secdb; kdos-command.md:919-1040) and `kdos-bootctl status --json`. It also hosts 7.3.8's prompt.
  - **Cadence.** Check nightly with the existing timer, and never auto-apply. With D11 images an apply takes minutes, but it still needs a restart.
- **Size / risk / priority / upstream.** ~250 C++ plugin (§6.2.2) + ~500 C. Low to medium risk. P1. The notifier is upstreamable.
- [ ] Use §6.2.2's `kdos-discover-notifier`, and file the upstream MR. Gate: rig: drop a newer `update.json`, and within one check interval the tray shows the update (cropped shot); after Install it shows Restart; MR URL recorded in the kdos-discover `kfs.md`.
- [ ] `kdos-notice` (§6.5 owns the task; this item adds the CVE and trial notices and 7.3.8's prompt). Gate: selftest with fixture JSONs posts exactly one notification per state change (`dbus-monitor` count).
- [ ] Docs: kdos-command.md (`kdos update`, `kdos restarts`, `kdos cve`), administration.md (updates). Gate: `bash testing/docscheck.sh && python3 testing/anchorcheck.py` pass; a grep of the old surface name in each named page is empty.

#### 7.6.4 Firmware in Discover, offline-listable

- **What the user sees.** Device firmware appears in Discover's Updates next to the KDOS system update, with vendor release notes. The Firmware Security page in Info Center shows the HSI level.
- **How it is built.**
  - **Discover.** `BUILD_FwupdBackend` (default ON [measured: discover libdiscover/backends/CMakeLists.txt:83]).
  - **HSI.** fwupd is already built with `-Dhsi=enabled` and `-Dpolkit=enabled` [measured: ports/core/*/fwupd/build.sh].
  - **The offline trap.** Today `30-fwupd-refresh.timer` downloads LVFS metadata daily [measured: administration.md:256, :884], which contradicts D10's "nothing downloads at run time".
  - **Mitigation.** Each KDOS release carries a signed LVFS metadata snapshot, installed as a fwupd local remote under `/usr/share/fwupd/remotes.d/` [inferred: fwupd supports directory remotes; measure], so Discover lists applicable firmware with no network. Firmware cabinets come from the release medium's firmware pack. Whether the online refresh timer stays as an opt-in is a §6.2/D10 call; recommendation: opt-in, off.
- **Size / risk / priority / upstream.** ~100 config + 1 flag. Medium risk: snapshot freshness is only as good as the release cadence. P2. Not upstreamable.
- [ ] Local remote with snapshot. Gate: `fwupdmgr get-remotes` lists `kdos-snapshot`; with the network down, `fwupdmgr get-updates` uses it (on the rig, with the fwupd test device).
- [ ] Docs: administration.md (firmware), principles.md (the offline rule's firmware clause). Gate: `bash testing/docscheck.sh && python3 testing/anchorcheck.py` pass; a grep of the old surface name in each named page is empty.

### 7.7 Onboarding

#### 7.7.1 Welcome tour with KDOS pages

- **What the user sees.** A two-minute tour that makes KDOS's unusual features discoverable, which answers the reviewers' complaint that nothing says which switches matter [source: XDA 2026-07]. Pages, in order:
  1. Accessibility (7.7.2).
  2. Accent: the 8 live swatches.
  3. Look: KDOS Modern, or Phosphor mode (Subtle preview) with a short warning about motion and photosensitivity (7.2.1); light/dark automatic.
  4. Desk: the picker (7.5.3) and the auto-tile toggle (7.5.4).
  5. Your system: A/B slots, updates and the previous-version button (7.3.6).
  6. Boxes & Store, with the badge states (7.3.3).
  7. Offline Library (7.4.1).
  8. Keys: Meta+F1 and Meta+Space.
  9. Privacy & network: firewall names, KDE Connect, Background Apps.
  10. Help: KHelpCenter and the book.

  On the live medium it adds "Install KDOS" and "Keep my changes on this stick" (`kdos persist create`, kdos-command.md:1263-1316). After a slot switch, `--post-update` shows "What's new in 0.2.14" from the release notes.
- **How it is built.** QML under `/usr/share/plasma/plasma-welcome/extra-pages/NN-*.qml`, plus `intro-customization.desktop`. Pages call verbs through `Controller.runCommand` [measured: plasma-welcome 6.8.2 CMakeLists.txt:42-43, README]. The live skeleton sets `plasma-welcomerc [General] LiveEnvironment=true, LiveInstaller=<the D12 installer's desktop id>`.
- **Builds on.** The first-run key card and the welcome card it replaces (kdos-keys --first-run). os-release `LOGO` must name an installed icon: `LOGO=kdos` names none today [measured: fs/etc/os-release:7; kdos-icons build.sh:25-27], so §5.2 fixes it.
- **Size / risk / priority / upstream.** ~1,500 QML. Low risk. P1. Not upstreamable.
- [ ] Write the pages. Gate: rig shot of every page (dismiss nothing first: the tour is the first-login surface the rig traps note); each action's effect asserted by config `diff`.
- [ ] Live and post-update modes. Gate: ISO rig run shows Install; a slot switch on the rig shows What's new.
- [ ] Docs: getting-started.md (rewritten around the tour), installation.md (live medium). Gate: `bash testing/docscheck.sh && python3 testing/anchorcheck.py` pass; a grep of the old surface name in each named page is empty.

#### 7.7.2 Accessibility first

- **What the user sees.** Orca can be started at the SDDM greeter and on the tour's first page with one documented chord. Page 1 offers:
  - larger text;
  - a high-contrast accent (paper and perfect solved toward 7:1);
  - reduced motion (`AnimationDurationFactor=0`, which also stops CRT degauss and collapse);
  - Sticky, Slow and Bounce keys, Mouse keys, dwell click and Zoom.

  This happens before anything else, as macOS and Windows do in their first-run setup.
- **How it is built.** Every listed feature is a KWin 6.8 plugin: `stickykeys slowkeys bouncekeys mousekeys dwellclicker magnifier zoom colorblindnesscorrection` [measured: kwin 6.8.2 src/plugins]. Contrast targets come from libkcolor's solver (7:1 target, 4.5:1 floor) [measured: kcolor.h:53-58]. Orca is ported. The greeter chord comes from the SDDM theme (§5.2) and needs AT-SPI in the greeter session [inferred; measure].
- **Size / risk / priority / upstream.** ~300 QML. Low risk. P1. Not upstreamable.
- [ ] Page 1 and the greeter chord. Gate: rig with `--audio`: the chord at the greeter produces Orca speech (capture per memory note on rig audio); the reduced-motion toggle sets `kdeglobals [KDE] AnimationDurationFactor=0`.
- [ ] Docs: accessibility.md (rewritten for Plasma: Orca reads the desktop over AT-SPI). Gate: `bash testing/docscheck.sh && python3 testing/anchorcheck.py` pass; a grep of the old surface name in each named page is empty.

#### 7.7.3 First-run checklist that ticks itself

- **What the user sees.** The tour's last page, and afterwards a small tray entry, holding a short checklist that ticks itself as things become true:
  - set up a backup;
  - pair your phone (optional);
  - add a language;
  - create your first box;
  - check firmware;
  - confirm disk encryption.

  It disappears when every item is done or dismissed.
- **How it is built.** A QML page plus a plasmoid that read the state through a C probe, `kdos checklist --json`. The probe reads: a restic repo in `~/.config/kdos/backup` or a Kup profile; KDE Connect paired devices over D-Bus; installed `kdos-lang-*` (§4.5); `kdos-box ls`; `fwupdmgr get-updates` exit status; `lsblk` crypto type. Items whose component is absent are omitted.
- **Size / risk / priority / upstream.** ~300 QML + ~150 C. Low risk. P2. Not upstreamable. [inferred: design; no research item measured it]
- [ ] Probe and plasmoid. Gate: selftest per probe against fixtures; rig: creating a box ticks the item within 5 s.
- [ ] Docs: getting-started.md. Gate: `bash testing/docscheck.sh && python3 testing/anchorcheck.py` pass; a grep of the old surface name in each named page is empty.

### 7.8 Upstream contributions this section produces

| Contribution | Module | Carried as | Benefit beyond KDOS | Item |
|---|---|---|---|---|
| Generic custom screen-shader effect | kwin | `kdos-crt.patch` until merged | any shader (colour-blind sims, retro) as data | 7.2.1 |
| Sandbox engine/app-id on `Window` and `DecoratedWindow`; badge in Breeze; tooltip in libtaskmanager | kwin, kdecoration, breeze, plasma-workspace | 4 patches | Flatpak windows badged | 7.3.2, 7.3.3 |
| Shortcuts overview effect | kwin | KDOS effect package | a shortcut sheet, which Plasma lacks | 7.5.2 |
| SystemdSysupdate notifier plugin | discover | out-of-tree `kdos-discover-notifier` until merged | tray for every sysupdate1 distribution | 7.6.3 |
| Descendant-cgroup aggregation on the Applications page | libksysguard | patch | podman and nested scopes listed | 7.6.1 |
| Camera-in-use indicator | plasma-workspace | none until proposed | parity with GNOME and macOS | 7.3.4 |

§6.3 owns the sandbox denylist and portal cgroup patches.

- [ ] Each row: MR opened, URL in the carrying port's `kfs.md`, and the patch dropped in the first Plasma release that contains it. Gate: `python3 testing/kfscheck.py` passes; at each Plasma bump, a carried patch whose MR is merged fails to apply and is removed.

### 7.9 Considered and rejected

| Idea | Why rejected | Evidence |
|---|---|---|
| Phosphor / CRT on by default | D2 makes it opt-in. Scanlines cost up to 45% contrast; it is invisible on software rendering, so first boot in a VM would differ from metal; it composites the cursor and costs scanout while active. | kdos-crt-pass.c; eglcontext.cpp:322-325; workspacescene.cpp:389-417 |
| Kvantum (or QtCurve) widget engine | A third widget engine besides Breeze and Union. Garuda-style themes break on every release. KDOS's look is reached with colour schemes, a Plasma style and the Breeze decoration settings. | res-branding: Garuda dr460nized anti-pattern; Manjaro Breath bug 482637 |
| Union style now | It is in development ("Union (In Development)" themerc); styles need conversion between releases; it needs Rust CSS-parser vendoring. Revisit at Plasma 6.9 or 7 as a `kdos` Union style (§5). | union 6.8.2 src/output/qtwidgets/union.themerc; res-branding Union row |
| Flathub / Flatpak backend in Discover | D10: KDOS-only, offline. Boxes and packs fill the role, with KDOS's own confinement. | D10; discover CMakeLists.txt:53 (`BUILD_FlatpakBackend` ON by default, so it is turned OFF) |
| KNewStuff online ("Get New…") | D10 and "nothing downloads at run time". Replaced by 7.4.4's curated pack and the `ghns` Kiosk key. | knewstuff action.cpp:34-42 |
| Plasma 6.9 Open-Meteo weather widget, online wallpaper-of-the-day, online geolocation for Night Light | Each fetches at run time. Night Light uses the time zone's coordinates offline (§5.6). | TWiP 2026-10-17 |
| A per-app KWin `decocolor` rule | Never matches a boxed window, because KWin rewrites wmclass to the box name. Replaced by one rule per box (7.3.1). | xdgshellwindow.cpp:847-862 |
| An authorisation check on `org.kde.KWin.ScreenShot2` as KDOS's box defence | Upstream removed the restricted-interface check on purpose and expects sandboxes to filter the bus; it would stay a carried patch forever. Replaced by xdg-dbus-proxy and a private runtime dir (§6.3), and reported honestly by 7.3.3. | kwin commit 0ea31ae7 |
| The CRT whole-screen collapse as a JS effect | JS effects have no `paintScreen` and no blank/DPMS signal. Done in `kdos_crt` instead (7.2.2). | scriptedeffect.cpp:131-132; effecthandler.h:955,962 |
| A fake `org.freedesktop.systemd1` with Version<250 so KIO makes scopes | It flips other KDE paths (Version≥250 means SystemdAsService), and `_KDE_APPLICATIONS_AS_SCOPE=1` already forces scope mode. §3.6 decides the scope shim. | kio systemdprocessrunner.cpp:26-51 |
| Polonium alongside Krohnkite | Two auto-tilers means two behaviours to document and test; one is bundled. | res-innovation DF15 |
| A KDOS fork of the lock-screen or logout QML | The QML imports private modules and breaks across releases. The stock screens are restyled through colours, wallpaper and the Plasma style (§5.2). | LockScreenUi.qml:17-26; res-branding pitfall |
| Plasma Login Manager for the greeter | D9 chose SDDM. PLM ignores day/night themes, compiles its QML into the binary, and needs systemd. | plasma-login-manager startplasma.cpp:245-289; greeter/CMakeLists.txt:30-40 |
| kuserfeedback telemetry on | Contradicts offline-first. §1.3: build with it off, or do not port it. | §1.3 |
| Auto-applying OS updates | An update ends in a slot switch and a restart; KDOS asks first (7.6.3). | kdos-command.md:1043-1131 |
| Auto-tiling on by default | D6. It surprises every user who expects floating windows; it is offered instead (7.5.4, 7.7.1). | D6 |


## 8. Documentation, tests, milestones and risks

Hard rule 1 holds for every milestone below. Each change that alters behaviour rewrites the pages that describe it in the same change, and a milestone is not closed until `make check` passes in the worktree. Hard rule 2 holds for every page this section proposes. No book page, comment or shipped file says what the cell desktop was or what replaced it. The old-surface-to-new-tool mapping lives only in this plan and in commit messages. The book carries a present-tense task index ("Networking: the Networks applet; System Settings > Wi-Fi") [measured: testing/docscheck.sh:48 BAD regex refuses `no longer`, `used to`, `previously,` and `historically,`, but it cannot catch a paragraph that tells the history in other words].

Counts here were measured on branch v0.2 at a81bec1c unless marked otherwise. Inbound links are `git grep -o` over every `*.md` in the tree, excluding the page itself; that includes kfs.md notes and the kfs book [measured: this session].

### 8.1 The book

#### 8.1.1 Part IV layout (recommendation: option A)

Part IV is chapters 20–29: 21 kdos-comp, 22 kdos-shell, 23 kdos-res, 24 kdos-term, 25 kdos-appbox, 26 daemons, 27 kinstall, 28 kdos command and 29 kdos-bb [measured: docs/kdos/README.md:181-201]. Under D1, kdos-bb stays, so chapter 29 keeps its number.

**Option A (recommended):** four new chapters take slots 21–24 in place:

| Slot | New file | Content |
|---|---|---|
| 21 | `04-programs/kwin.md` | The KDOS kwinrc/kwinrulesrc/kglobalshortcutsrc defaults. Each carried KWin `.patch` with its `Subject:` line. The Phosphor-mode effect, the bundled auto-tiling script (off), the smart-snap tiles and Xwayland. Box identity: security-context tagging, the restricted globals and the per-box decocolor rule. Idle, lock and lid through powerdevil and kscreenlocker. Debugging: supportInformation, the debug console, the log |
| 22 | `04-programs/plasma-shell.md` | The three `layout.js` desks (KDOS / Dock / Tiling) and the first-run picker. The panel and the KDOS plasmoids (update tray, boxes). Kickoff, KRunner and the KDOS runner. Notifications, the system tray and the curated offline add-on pack |
| 23 | `04-programs/kdos-settings.md` | KDOS's KCMs: Boxes, Slots/Updates, Accent, Firewall, Backups, and whatever else the integration section ships. What each one writes, and the root daemon or D-Bus name behind it |
| 24 | `04-programs/discover.md` | Discover's KDOS backend(s): ports, packs, boxes and fwupd. `kdos-sysupdated` serves org.freedesktop.sysupdate1. A/B "Restart to update", the binhost stepping stone, and `kdos update apply --source` |

The Kirigami installer (D12) is documented in `04-programs/kinstall.md` (ch 27), which covers both front ends over the one backend. No chapter is added for it.

Option A leaves chapters 25–45, the hand-typed `Ch NN` reading paths and "Forty-five chapters in six parts." valid as they are [measured: README.md:50-97 cite chapters 1, 3, 5–10, 12, 13, 15–18, 20, 26, 28, 30–38 and 40–44, never 21–24 or 29; README.md:101; testing/booknav.py:30-37 validates only `## Contents` and the footers]. Option B (one chapter) renumbers 22–45 by −3. Option C (about 8 chapters) renumbers by +4. Under B or C every `Ch NN` in README.md:50-97 must be renumbered by hand, because no check reads them.

#### 8.1.2 Page fates, docs/kdos

The links column gives `bare links / anchored links` into the page from other files. The anchored figure is the debt: a page that keeps its filename breaks only the anchors of headings it drops. Keep-heading lists name headings whose text must survive unchanged.

| Ch | Page (lines) | Action | What replaces the content | Links in (all / #anchored) | Keep heading text |
|---|---|---|---|---|---|
| — | README.md (280) | EDIT-SECTIONS | Preface (:10-15): KDOS's desktop is KDE Plasma 6.8 and all of KDE Gear, integrated and branded as KDOS. Retitle Contents rows 7, 9, 11, 14, 18, 19, 20–24, 26 and 35–36. Reading-path glosses (:65, :95) | 12 / 0 | `## Contents` |
| 1 | why-kdos.md (538) | EDIT-SECTIONS | Desktop ring row (:52) and login chain (:64-70): greeter, then startplasma-wayland. Absent table (:253-267): systemd row names elogind as the one systemd-derived component; toolkit row deleted; display-manager row deleted. Libraries 17 → 9 (:283). Third-party rows for kdos-comp/labwc and libkvt/libtsm deleted (:474-492) | 45 / 24 | — |
| 2 | how-kdos-differs.md (491) | EDIT-SECTIONS | At a glance, Init (no systemd init, units or supervisor; elogind for logins), Display stack (Wayland-only SDDM greeter), Toolkits, The desktop (compared with how Arch, Fedora KDE, neon and KDE Linux ship Plasma) | 60 / 33 | `The desktop` (6 in), `Toolkits` (3) |
| 3 | principles.md (501) | EDIT-SECTIONS | Bodies of No systemd (:32-78, seatd row :43) and No Xorg (:80-114) rewritten under D3. The toolkits section (:116-172) becomes "Qt and KDE Frameworks are the desktop's toolkit", and its 12 inbound links are retargeted. The offline principle states D10 (KNewStuff disabled, no Flatpak). The vendoring counts (:259-262) are recounted | 123 / 106 | `No systemd` (60 refs in 58 files), `No Xorg server, and one carve-out` (29 in 28) [measured] |
| 4 | decisions.md (965) | EDIT-SECTIONS | Delete: labwc fork (:44-73), No KDE/GNOME (:242-263), libtsm fork (:758-788), one file chooser (:790-814), and the narrowings Drops, Screens, Tabs, One font, ReGIS, Braille-does-not-read-the-desktop, Tray menu read once. Add D1–D12 as present-tense decisions: Plasma is the desktop; elogind; SDDM with a Wayland greeter; KWin patched, not forked; KDOS Modern plus Phosphor mode; floating windows with smart snap; Discover backends rather than a store; slot-image updates; one installer backend. Rewrite the src-areas table (:577-613): `src/desktop` holds Qt code. Recount "26 src ports" and "seventeen" | 67 / 53 | `nothing-downloads-at-run-time` |
| 5 | getting-started.md (407) | EDIT-SECTIONS | Boot: splash, SDDM (autologin on the live medium), KSplash, desktop. The first-run desk picker. Konsole on the chosen chord. `kdos theme amber` retints Plasma. Screenshots replaced (§8.1.6) | 22 / 5 | — |
| 6 | installation.md (827) | EDIT-SECTIONS | The Kirigami installer and the TUI. Autologin is written to `/etc/sddm.conf.d/`. The accent page writes the colour scheme and the look-and-feel. "Set up now" against OEM (plasma-setup). Partition Manager for manual layouts | 46 / 30 | — |
| 7 | desktop.md (936) | REWRITE-WHOLLY | The Plasma desktop as KDOS ships it: desks and the picker, panel, Kickoff, KRunner, floating plus smart snap, the auto-tiling toggle, the shortcut table, notifications, Dolphin, Klipper, lock/idle/power, KScreen, removable media, the privacy indicator, troubleshooting, and the task index | 45 / 25 | `Keyboard shortcuts` (6), `Locking, idle and power` (1) |
| 8 | applications.md (989) | EDIT-SECTIONS | Kickoff and KRunner list boxed apps. File dialogs come through xdg-desktop-portal-kde. Discover replaces the store sections (:589-615). Terminal entries open in Konsole. The D8 default-apps table | 45 / 18 | — |
| 9 | theming.md (774) | REWRITE-WHOLLY | KDOS Modern: 8 accents (bone default) as colour schemes and global themes; the Accent KCM and `kdos theme`, and what each reaches. Phosphor mode: the effect, retro accents, Terminus as an option. Wallpapers; Noto Sans plus KDOS Mono (D7); boot menu, splash and console palette (about 150 lines kept); GTK apps; Qt5 apps through qt5ct's `kde` key; boxes through the portal Settings; the generator | 79 / 43 | `Fonts` (4), `The eight accents` (2), `How the theme is generated` (8), `Wallpaper` (4), `What changes when` (2). `The phosphor pass` (4) becomes `Phosphor mode`; retarget its links |
| 10 | administration.md (1524) | EDIT-SECTIONS | Users, network, Bluetooth and printers through System Settings. Updates through Discover, the update plasmoid and the Slots KCM. Login and autologin through sddm-kcm. Time through the timedate1 provider. Input methods under KWin (:1492-1507). The `seat` group text (:316-326, :439) | 181 / 168 | every existing heading the section edits leave in place (168 anchored links) |
| 11 | accessibility.md (570) | REWRITE-WHOLLY | Orca reads the whole desktop over AT-SPI. KWin Zoom/Magnifier, sticky/slow/bounce/mouse keys, dwell click, colour-blindness correction and invert. On-screen keyboard. Animation speed. Plasma scale and font size. BRLTTY console (:46-170) kept [measured: kwin/src/plugins lists stickykeys, slowkeys, bouncekeys, mousekeys, dwellclicker, magnifier, zoom, colorblindnesscorrection, invert] | 55 / 30 | BRLTTY headings |
| 12 | overview.md (515) | EDIT-SECTIONS | Rings; the login is SDDM, elogind session, startplasma-wayland, plasma_session (classic boot), kwin_wayland_wrapper, ksmserver, kded6 and plasmashell. A Plasma process map. "How the screen is drawn" becomes KWin compositing (GL; llvmpipe in a VM). Rewrite the supervised-chrome table (:262-393). oomd's protected set (:405) | 29 / 3 | — |
| 13 | boot-and-init.md (1610) | EDIT-SECTIONS | rcS starts elogind. SDDM runs from inittab or ksvc after the splash quits (handoff race, §8.4). Login records. "The console": tty2 recovery, VT switching through logind and KWin, getty.c's cgroup and RT moves. mark-good moves out of rcS's last step (D11) | 273 / 244 | `The console` (14 refs in 9 files) |
| 14 | session.md (1122) | REWRITE-WHOLLY | The Plasma session: startplasma with `startkderc [General] systemdBoot=false`; the per-user service manager (turnstile or `ksvc --user`) and the bus at `$XDG_RUNTIME_DIR/bus`; PipeWire (about 150 lines kept); portals (xdg-desktop-portal-kde, kde-portals.conf); capture through KWin and kpipewire; Klipper; fcitx5 under KWin; the box environment with `XDG_CURRENT_DESKTOP=KDE`; logout | 159 / 135 | `Portals` (12), `Input methods` (15), `Starting a session` (10). Retarget `What kdos-desktop-start does` (9), `The KDOS backend` (6), `Screen capture and the clipboard` (4), `Supervised chrome` (3) |
| 15 | packaging.md (1442) | EDIT-SECTIONS | Per-area and per-phase counts (bookcounts). The 50_desktop description. The KDE release-set rule (Plasma 6.8.x, KF6, Gear pinned together). Vendoring counts (:312, :841) | 232 / 198 | all |
| 16 | packs-and-boxes.md (913) | EDIT-SECTIONS | The store lane (:139-191) becomes Discover's backend. "Reaching the desktop" (:850-894): genlaunchers, the `app.<id>.desktop` entries that match KWin's rewritten desktopFileName, then kbuildsycoca6 | 99 / 52 | — |
| 17 | security-model.md (1206) | EDIT-SECTIONS | Delete kdos-checkpass and kdos-resctl (:144-248); the setuid table drops by 2. kscreenlocker uses PAM and unix_chkpwd. polkit: polkit-kde-agent-1 with elogind session tracking, and a rewritten 50-kdos.rules. Sandboxed clients under KWin: the restricted list and the box `grant =` vocabulary. "Not protected" gains KWin scripting, ScreenShot2 (no caller check), kglobalaccel and KWallet on the shared bus. The `seat` group (:287, :315-325) | 168 / 141 | — |
| 18 | design-language.md (1145) | REWRITE-WHOLLY | The KDOS-on-Plasma identity spec: the 8 accents mapped to KColorScheme roles with contrast floors; Noto Sans and KDOS Mono; the Plasma style, decoration, icons (`FollowsColorScheme=true`), cursors, sounds and wallpapers; Limine → splash → SDDM → KSplash → lock as one look; Motion; desks; Kirigami.Theme in KDOS code; libkcolor slots and glyph tiers for the TUI tools; the checklist | 64 / 35 | `Colour` (4), `Motion` (7), `The glyph tiers` (6), `The checklist` |
| 19 | window-model.md (565) | REWRITE-WHOLLY | KWin as KDOS configures it: focus policy, placement, quick tiling and custom tiles, the auto-tiling toggle (D6), the window rules KDOS ships (box tint), virtual desktops, Overview, multi-monitor | 15 / 3 | retarget `Placement is a search, not a cascade` (2) and `Window memory` (1) |
| 20 | 04-programs/README.md (297) | REWRITE-WHOLLY | Index of KDOS's own programs: the TUI tools, the daemons, kdos-boxsock, and the Qt components (KCMs, plasmoids, runner, Discover backend, KWin effect and scripts, theme packages, SDDM theme), each with its chapter | 12 / 3 | — |
| 21 | kdos-comp.md (1398) | DELETE → kwin.md | see 8.1.1 | 131 / 97 (54 bare from surviving pages) | top anchors to retarget: `The phosphor pass` 9, `Xwayland` 8, `Idle, dim, lock and lid` 6, `Motion` 5 |
| 22 | kdos-shell.md (2547) | DELETE → plasma-shell.md, kdos-settings.md | see 8.1.1 | 140 / 119 (58 in command-index.md) | `The system surfaces` 12, `The desk accessories` 8 |
| 23 | kdos-res.md (812) | DELETE → discover.md takes the slot | plasma-systemmonitor and ksystemstats (upstream). An optional box ksystemstats plugin is documented in kdos-settings.md | 27 / 9 | — |
| 24 | kdos-term.md (747) | DELETE | Konsole (upstream). The KDOS profile and colour scheme go in theming.md; OSC 133 prompt marks go in the bash kfs.md | 28 / 11 | — |
| 25 | kdos-appbox.md (1236) | EDIT-SECTIONS | Box env (:548-605), launcher generation (:606-759: `app.<id>.desktop`, kbuildsycoca6), terminal entries in Konsole (:826-853), the `grant =` vocabulary in KWin terms, gc autostop without `kdos hey` (:1150-1160) | 76 / 50 | — |
| 26 | daemons.md (1459) | EDIT-SECTIONS | Delete the portal (:1329-1337) and kdos-lock/checkpass (:1338-1393) sections. Suspend through elogind PrepareForSleep and kscreenlocker, with TLP and NM hooks in elogind's system-sleep dir. Autologin writes sddm.conf.d. Device offers through Solid. Firewall `vnc` row becomes krfb, plus an `rdp` row for krdp. oomd's protected set. kdos-boxsock kept (:1277-1328). `seat` group (:70-78) | 152 / 125 | `How it decides` |
| 27 | kinstall.md (1105) | EDIT-SECTIONS | Two front ends over one backend (D12); the OEM path arms plasma-setup; the Theme step applies the Plasma scheme; autologin through SDDM; the TUI for serial and unattended installs | 46 / 21 | — |
| 28 | kdos-command.md (1700) | EDIT-SECTIONS | "The desktop" (:274-724): `kdos theme` through plasma-apply-colorscheme, lookandfeeltool and an explicit wallpaper step; delete panel, menu, settings; notify through org.freedesktop.Notifications; doctor checks KWin, plasmashell and elogind. Delete or port kdos-shot (grim has no protocol on KWin), stutter (kdos-frames.sock), hey (kdos-cmd.sock) and appid (:849-875). Reasons corpus count "78" (:885) recounted | 195 / 173 | — |
| 29 | kdos-bb.md (776) | EDIT-SECTIONS (kept, D1) | Synchronized-output section (:325-361) names any DECSET 2026 terminal. Konsole's support is unknown and must be measured before the page claims it [source: contour-terminal vt-extensions synchronized-output.md lists Konsole "unknown", measured 2026-10-28] | 15 / 5 | all |
| 30 | how-kdos-is-built.md (840) | EDIT-SECTIONS | Phase diagram; "The desktop: 50_desktop" (:545-560) lists elogind, SDDM, Plasma 6.8.x and the KDOS Qt components; Gear on subject shelves in 44_apps; build-time figures re-measured; vendoring counts (:148-149); quick loop (:803) | 149 / 82 | — |
| 31 | developing.md (922) | EDIT-SECTIONS | `make run` is KWin on llvmpipe with Phosphor mode unavailable; `make run-hw` and `--gl` run it. Rebuild rows for a Plasma module and a KDOS Qt component. Quick loop. Qt code with no build (devdeps container) (:745-782) | 98 / 71 | — |
| 32 | build-system.md (1580) | EDIT-SECTIONS | 50_desktop row (:95). Skel-owned directories (:745): `.config/kdeglobals`, `plasma-org.kde.plasma.desktop-appletsrc` and the rest | 162 / 132 | — |
| 33 | writing-ports.md (3131) | EDIT-SECTIONS | KDE release-set section; ECM build shape; `X-KDOS-Term` names konsole; icon fallback through KIconLoader; the 50_desktop rule (:1405); translations (`KF_SKIP_PO_PROCESSING` on every KDE port, A1); vendoring counts | 773 / 714 | all |
| 34 | build-troubleshooting.md (1944) | EDIT-SECTIONS | New entries: ECM/KF version mismatch, QML module not found, cxx-rust-cssparser vendoring, musl locale, a Tier-B port without kfs.md | 132 / 106 | all |
| 35 | c-libraries.md (2206) | EDIT-SECTIONS | Delete libkwm (:751-800), libkdisp (:801-934), libkvt (:1217-1330), libkimg (:1331-1359), libkchrome (:1360-1523), libkicon (:1524-1585), libkcell (:1586-1869) and libkwl (:1870-2145), about 1,100 lines. Nine survivors: libkbase, libkcolor, libktui, libkxdg, libkproc, libkpkg, libksig, libkpack, libkbuild. libktui is scoped to kinstall, kdos-appbox and kdosbuild. kxdg_verbs follows its selftest.c cases | 97 / 67 | retarget `libkchrome` 8, `The fractional scale` 4, `The cell's size` 4, `libkwl`/`libkvt`/`libkimg`/`libkdisp` 3 each |
| 36 | writing-desktop-software.md (1270) | REWRITE-WHOLLY | KDOS desktop code in Qt 6, KF6 and QML: `src/desktop/<name>` with an ECM build.sh; plasmoid, KCM, runner, Discover backend, KWin effect/script, kded module and notifyrc; Kirigami and colour rules; `org.kdos.*` names; QProcess with an argv, never `sh -c`; reaching the root daemons; QtTest, qmllint, AT-SPI; checklist | 40 / 22 | — |
| 37 | testing.md (1893) | EDIT-SECTIONS | Delete partial paint, inspector, frame clock, selection plate, display text, tile, chart and window-model subsections (:257-486), goldens and the dump harness (:581-937), and compiling the compositor (:971-1043). Add the Qt suite, AT-SPI tests, the session probe, timing and RAM, the rig's SDDM/Plasma gate, and frame rate through KWin's showfps | 135 / 101 | — |
| 38 | ports-catalogue.md (5478) | EDIT (rows) | −13 certain deleted recipes (plus each dead wlroots tool §8.2.6). Plus about 70 Plasma, 193 Gear and about 98 third-party rows, lockstep with each port batch [measured: bookcounts.py:24-29] | 100 / 23 | per-shelf headings |
| 39 | command-index.md (534) | EDIT-SECTIONS | Delete about 70 rows: 55 kdos-shell names, kdos-comp, -term, -res, -lock, -record, -checkpass, -resctl, kdos-shot. Add KDOS Qt binaries and session commands (startplasma-wayland, kscreen-doctor, qdbus6, kquitapp6, loginctl). Each `kdos-*` row generates a man page [measured: src/system/kdos-doc/mkbook.py:22-29; preflight.sh:3441-3468] | 14 / 3 | — |
| 40 | configuration.md (1488) | EDIT-SECTIONS | Delete comp.conf, panel.conf, launcher.conf, slit.conf, menu.conf, res.conf, term.conf, rc.xml, menu.xml and themerc-override (:79-850). Add "Plasma's files KDOS ships": /etc/xdg/kdeglobals, kwinrc, kwinrulesrc, kglobalshortcutsrc, kscreenlockerrc, ksplashrc, startkderc, powerdevilrc, konsolerc, the layout.js presets, sddm.conf.d and logind.conf. favorites stays (kdos-appbox warmup reads it) or is re-sourced | 116 / 93 | `backup.conf`, `timers.d`, `boxes`, `sandbox` |
| 41 | filesystem-and-ipc.md (770) | EDIT-SECTIONS | Delete kdos-cmd.sock, kdos-frames.sock, kdos-notify.sock and kdos-clip.sock (:496-575). Add `/run/user/<uid>` (elogind), the `org.kdos.*` names, the SDDM socket and the plasmoid state. Env vars (:607-685) | 25 / 10 | — |
| 45 | glossary.md (940) | EDIT-SECTIONS | Delete about 20 entries (box chip, cell grid, chrome, coast, display text, dockapp, dump, front end, glide, golden, labwc, libkdisp, libkwl, route, scene, seatd, slit, sprite, tile, toast, wlroots). Rewrite accent, Phosphor mode, retint, session, store→Discover, surface, Xwayland. Add KWin, plasmashell, plasmoid, KCM, KRunner, Kirigami, look-and-feel package, layout script, kded, elogind, display manager, greeter, KIO. Vendoring count (:868) | 129 / 76 | `slot` (5), `accent` (2) |
| 43 | known-gaps.md (1256) | EDIT-SECTIONS | Retarget about 60 links, then delete the 20 Desktop entries (:28-214) [measured: verdict, 20 entries not 22]. Add: Plasma rig-verified only on virgl; Phosphor mode off on software GL; the Discover backend's scope; KNewStuff disabled; the Qt5 app look; kwallet with autologin; the SDDM Wayland greeter is upstream beta; the camera indicator sees only PipeWire streams. Flip the screen-reader entry | 319 / 294 | retarget `There is no graphical login screen (greeter)` 7, `The power button and the sleep key work only inside a graphical session` 7, `A fingerprint unlocks sudo and nothing else` 8 (kept if still true), the screen-reader entry 7 |
| 44 | status.md (327) | EDIT-SECTIONS | Desktop verdicts (:169-222). Counts (bookcounts) | 25 / 12 | — |
| 42 | repository-layout.md (566) | EDIT-SECTIONS | `src/desktop` holds Qt components; 9 libraries; docs/screenshots | 12 / 1 | — |

No page is untouched: build-troubleshooting.md mentions `src/desktop` and `50_desktop` (:540-548) [measured: inv-docs-tests].

#### 8.1.3 Cross-page text no tool recounts

- [ ] **Vendoring counts.** These are stated exactly at principles.md:259-262, glossary.md:868, how-kdos-is-built.md:148-149 and packaging.md:312 and :841 (writing-ports.md as well), and nothing recomputes them [measured: bookcounts.py:31-34; today 161 recipes with `vendoring =` (75 Rust, 39 Go, 42 Python, 4 Haskell, 1 Node), 166 with a bundle]. Each KDE port that adds `vendoring =` (cxx-rust-cssparser at least) changes all five pages in the same change. Gate: `git grep -n '161 \|166 ' docs/kdos` matches the figures `grep -l '^vendoring' -r ports/core --include=kpkgbuild | wc -l` gives.
- [ ] **The `seat` group.** It is defined as "the group seatd hands the display to" in about 35 lines across 10 pages: security-model.md:287 and :315-325, daemons.md:70-78, overview.md:403-409, administration.md:316-326 and :439, installation.md:310, getting-started.md:260, 04-programs/README.md:102, kinstall.md and filesystem-and-ipc.md [measured: inv-docs-tests missed]. Under Default A3 the non-administrator tier of kdos-powerd and kdos-oomd moves to polkit `allow_active` on the active elogind session (kdos-mountd is deleted, B2), and only then is `seat` removed from fs/etc/group:29 and install.c:917-921 (03 §3.5). Rewrite all ten pages, the comments at kdos-powerd main.c:29-36 and :958, kdos-oomd main.c:29, :62 and :237-242, kdos-installer conf.c:22 and install.c:917-921, and selftest.sh:5584, in that change. Gate: `git grep -n 'seatd\|"seat"\|KO_SEAT\|KP_SEAT' -- docs src fs testing` returns nothing.
- [ ] **seatd and no-logind text.** 34 places in 19 files [measured: res-session missed]. They are rewritten in the M1 change that adds elogind. Gate: the same grep, plus `git grep -n 'no systemd component\|seatd, not' docs` returns nothing.
- [ ] **Shipped prose outside the books.** The `kdos why` reasons corpus has 78 files under src/system/kdos-tools/reasons/. 24 of them fail preflight once their ports go and 25 cite something deleted; about 5 more need rewording that preflight will not flag [measured: inv-code verdict]. Also comments in surviving code: kdos-tools thumb.c:35 (libkcell), kdos.c:789 (libkvt), kdos-bb bb.c:143 (libkvt), kdos-boxsock main.c:11 and :96 (kdos-comp), and the recipe text in xcb-util-wm kpkgbuild:16, libnsgif kpkgbuild:16, 43_toolkits disk.txt:28-29, pam kfs.md:4 and :34, and the icu kfs.md. Editing a kpkgbuild description rebuilds that port; kfs.md and list comments do not. Gate: `testing/preflight.sh` reasons check green; `git grep -nE 'libkcell|libkvt|kdos-comp|wayvnc' -- src/system src/daemons ports/core` names no surviving comment or description.

#### 8.1.4 Order that keeps the checks green

Each step is one change that ends with `bash testing/docscheck.sh && python3 testing/anchorcheck.py && python3 testing/booknav.py && python3 testing/kfscheck.py` passing.

1. [ ] **Baseline.** Optional but recommended: extend anchorcheck to CLAUDE.md, and extend docscheck's dead-link pass to HTML `<img src>` in README.md. Neither is checked today [measured: anchorcheck.py:14-16 reads both books plus README.md Markdown links only; docscheck dead-link regex `\]\(`]. Gate: `make check` green, and a seeded broken CLAUDE.md link fails it.
2. [ ] **With the first platform port (M1):** CLAUDE.md rules 3/4/5 (M0 has the text), the principles.md bodies, the decisions.md new entries, why-kdos.md and how-kdos-differs.md. Retarget the 12 `#toolkits-are-for-applications-not-the-desktop` links before renaming that heading.
3. [ ] **With each KDE port batch (M2):** catalogue rows, kfs.md for each Tier-B port, tier-a.txt, `ports/unbuilt` for recipes staged before a phase list names them [measured: bookcounts.py:36-39], then `make book-stamp`.
4. [ ] **At the login switch (M3):** session.md, overview.md, boot-and-init.md "The console", getting-started.md, configuration.md (login and SDDM keys) and filesystem-and-ipc.md. The kfs pages session-and-environment, login-and-autologin and desktop-troubleshooting (`python3 testing/kfscheck.py --repin`, or replace stale pins). The rig's gate.
5. [ ] **At the deletion (M6), one change:** write kwin.md, plasma-shell.md, kdos-settings.md and discover.md, and replace Contents lines 21–24 in place. Retarget every inbound link in surviving files: about 270 page links, 95 `#port-` links to the 13 deleted recipes (plus those to any wlroots tool deleted), about 35 c-libraries anchors and the decisions narrowings. Then `git rm` kdos-comp.md, kdos-shell.md, kdos-res.md and kdos-term.md with their ports and kfs.md. Run `python3 testing/booknav.py --write`, then the four checks.
6. [ ] **Same milestone:** rewrite desktop.md, theming.md, accessibility.md, design-language.md, window-model.md, writing-desktop-software.md and 04-programs/README.md in place, keeping the headings in the Keep column. Update the CLAUDE.md map rows.
7. [ ] **Reference part:** command-index (man pages), configuration, glossary; retarget the known-gaps desktop anchors, then delete those entries; prune c-libraries; status.md and repository-layout.md.
8. [ ] **docs/kfs** (8.1.5); `booknav --write`, `make book-stamp`, kfscheck.
9. [ ] **After the first full KDE build:** `make book-data BUILDFS=build/fs`; screenshots (8.1.6); README.md.

Link retargeting is done with a reviewed script that prints a diff, or with Edit, never a blind `sed -i` (rule 9's spirit). Run anchorcheck and kfscheck after each batch. kfs.md links are validated only by kfscheck's render, not by anchorcheck [measured: kfscheck.py:71-74].

#### 8.1.5 docs/kfs

| Item | Action | Detail |
|---|---|---|
| 13-desktop/intro.md (194) | REWRITE-WHOLLY | 50_desktop builds elogind, SDDM, Plasma 6.8.x, the portals and the KDOS Qt components. Its pins into 50_desktop packages.txt (lines 13-18), phase.env (25-28) and kdos-comp/build.sh are replaced. `KDOS_PHASE_DESC="wlroots, kdos-comp, kdos-shell"` in phase.env is rewritten with it |
| 16-wiki rewrites (10 pages, 3,175 lines) | REWRITE-WHOLLY, keeping the 7-heading skeleton docscheck enforces | session-and-environment, compositor-keybindings (renamed `shortcuts-and-window-rules`), panel-and-launcher, fonts-and-theming, accessibility, portals-and-screen-sharing, desktop-troubleshooting, screen-lock, login-and-autologin, displays-and-scaling |
| 16-wiki edits (about 30 pages) | EDIT-SECTIONS | Hardware and desktop-adjacent: input-methods, xwayland-and-x11-apps, laptops, graphics-drivers, virtual-machines, diagnostics, bluetooth, printing-and-scanning, audio, power-management, storage-and-removable-media, backups. System, security and networking: networkmanager, vpn, mobile-broadband, access-point, file-sharing, locale-and-console, users-and-groups, init-and-runlevels, sudo-pam-polkit, fingerprint-fido-smartcards, boxes, logs, time-and-ntp, avahi-and-mdns, firewall, timers, updating, recovering-an-unbootable-system |
| 16-wiki/intro.md:3 `kfs:order` | EDIT | Rename compositor-keybindings and add new pages (e.g. kwin-effects-and-scripts, discover-and-updates). booknav refuses an unlisted page in a grouped part [measured: booknav.py:51-56] |
| 11-toolkits/intro.md :30-48, :73, :105 | EDIT | Qt and KF6 serve the desktop; the ELF-guard description follows the new preflight guard |
| 01-preparing/fs-overlay.md, 09-system/intro.md (seatd), 10-graphics/intro.md, 12-apps/intro.md, 15-image/030-launchers.md (kbuildsycoca6), 045-identity.md, 105-ports.md, 00-front/preface.md, 17-appendices/glossary.md (:14, :27, :169), flag-glossary.md (ECM and KDE flags) | EDIT | as listed |
| 15-image/050-theme.md | REWRITE | Seeding the Plasma theme (:48-52 name kdos-comp and themerc-override) |
| 15-image/070-user.md:53 | EDIT | Console-desktop residue: the `.config/cosmic`, `.config/kdos-con` row. Delete it together with its code at script/phases/70_image/070_user.sh:79 (`rm -rf "$home/.config/cosmic" "$home/.config/kdos-con"`). That line sits inside the pin `lines=73-97` at 070-user.md:43, so `kfscheck.py --repin` follows. The `.config/kdos-comp` row (:55) is rewritten for whatever stale Plasma state the step removes [measured: 070_user.sh:79; 070-user.md:43, :53] |
| kfs:include pins | REPIN or REPLACE | 83 pins in 21 pages point into deleted or changed desktop and session files: rc.xml 19, comp.conf 15, kdos-desktop 7, 10-wayland.sh 6, session-common.sh 5, kdos-desktop-start 5, getty.c 3, .bash_profile 3 and about 20 singles. There are 593 directives in all [measured: verify-inv-docs-pins.tsv]. kfscheck's includes check fails on a missing file |
| Port notes, deleted | DELETE | 13 kfs.md beside deleted ports, about 530 lines: src/desktop/{kdos-comp,kdos-lock,kdos-record,kdos-res,kdos-shell,kdos-term,xdg-desktop-portal-kdos}, src/art/{kdos-icons,kdos-cursors,kdos-gtk-theme}, ports/core/wl/wlroots, xdg/xdg-desktop-portal-wlr, base/seatd. Plus one beside each dead wlroots tool deleted (§8.2.6) |
| Port notes, surviving | EDIT | 85 port kfs.md and 26 src kfs.md name the old desktop [measured: inv-docs/port-notes-touching-desktop.txt], e.g. gpu/mesa:8 `#the-phosphor-pass` and x11/xwayland:12. Rewrite the Flags bullets of ports whose flags flip to elogind (pipewire, wireplumber, libpulse, fwupd, gvfs, gnome-disk-utility, polkit, networkmanager); kfscheck's sidecar check refuses a bullet whose flag is gone. kdos-theme kfs.md is rewritten for the colour-scheme generator |
| Port notes, new | WRITE | Every new port that meets a Tier-B criterion (a patch, a vendor bundle, build.sh over 30 lines, 20+ reverse dependencies) needs About/Flags/Patches/Notes or kfscheck `notes` fails [measured: kfscheck.py:28-34]. Estimate 40–80 [inferred]. Must-haves: kwin, plasma-workspace (`-DINSTALL_SDDM_WAYLAND_SESSION=ON`), libplasma, ksvg, union, cxx-rust-cssparser, kdeplasma-addons, elogind (musl patch set), sddm, discover (`-DBUILD_SystemdSysupdateBackend=ON`), kscreenlocker, powerdevil, breeze (6.8.x; decorations, cursors and wallpapers ON), appstream (`-Dqt=true`) |
| data/tier-a.txt (262 lines) | EDIT | Remove 13 rows: 9 desktop (wlroots :222, xdg-desktop-portal-wlr :223, kdos-comp, kdos-shell, kdos-term, kdos-lock, kdos-res, xdg-desktop-portal-kdos, kdos-record at :235-242, keeping kdos-boxsock), 3 art (kdos-cursors :253, kdos-gtk-theme :255, kdos-icons :256) and seatd :200 [measured: verdict]. Add kwin, plasma-workspace, libplasma, kscreenlocker, powerdevil, xdg-desktop-portal-kde, elogind, sddm, discover, kdos-sysupdated and the KDOS Qt components |
| data/recipes.stamp | `make book-stamp` | After every recipe, kfs.md, phase-list or include-target edit; kfscheck and pre-push refuse a stale stamp |
| data/{packages,contents,pc,sources,whatis}.tsv | `make book-data BUILDFS=build/fs` | After the first full KDE build (M3/M6). Until then the new ports render "unrecorded", which is tolerated |
| script/book/kfsgen.py:829-835 AREA_DESC | EDIT | `desktop`: "KDOS's own desktop components: settings modules, widgets, the Discover backend, KWin effects". `art` (:831): drop "icons, cursors" |
| ports/shelves | EDIT | :80 `wl` loses "wlroots". The KDE shelves the porting section adds go into the closed list, which renumbers the generated kfs shelf chapters (`booknav --write`) |

#### 8.1.6 Screenshots, README.md, CLAUDE.md

- [ ] **docs/screenshots** (22 PNGs, 2.3 MB): delete 20 and keep boot-splash.png and tty-banner.png. kdos-doc symlinks docs/screenshots into /usr/share/doc/kdos, so the set ships [measured: inv-docs-tests verdict]. Shots are scripted: `make shots` boots the ISO in the rig and runs `spectacle -b -n -f -o <file>` (or `-a` for a window) for a fixed list. That list is the desktop per desk, Kickoff, KRunner, System Settings with the KDOS KCMs, Discover with an A/B update, the desk picker, the SDDM greeter, the lock screen, Accent bone and amber, and Phosphor mode (`--gl` only). One hand-picked hero shot for README.md is allowed. Gate: `make shots` regenerates the set and `git status docs/screenshots` lists only intended files. testing/usability.sh and usability.md reference desktop.png and start-menu.png, so they change in the same commit.
- [ ] **README.md** (about 15 blocks, ~60 of 300 lines): :9, :13 ("a desktop drawn in cells", wlroots), :21-26 (desktop.png and the phosphor caption), :39, :77-82, :92-96 ("17 C libraries"), :100-105 (screenshot table), :119 (screen reader, "no graphical login screen"), :125, :138-139, :204-207, :224-226, :287-288, :298 (kdos-comp's GPL fork; licence notes for carried KDE patches). Gate: `bash testing/docscheck.sh && python3 testing/anchorcheck.py` pass (with 8.1.4.1, the `<img>` paths too); `grep -nE 'cells|wlroots|kdos-comp|17 C libraries' README.md` is empty.
- [ ] **CLAUDE.md:** map rows :24-29, :40-42 become rows for window-model (KWin), design-language (the look), kwin.md, plasma-shell.md, kdos-settings.md, discover.md and writing-desktop-software (KCM, plasmoid, runner, effect); the res and term rows are deleted. Rules 3/4/5 (:88-105) take the D3 text. Conventions (:165-170): colour comes from Kirigami.Theme or KColorScheme roles in Qt, libkcolor slots in TUI tools and uniforms in a shader, never a literal; no shell means QProcess with an argument list. Build table row for a Plasma module and a KDOS component (:209). Rig section (:261-308): spectacle example; the "welcome card" trap becomes "the desk picker is open on first login"; the "phosphor never in a rig photo" trap becomes "Phosphor mode needs `--gl`"; the 15-character kill names now include xdg-desktop-portal-kde and polkit-kde-authentication-agent-1; a login shell needs `DBUS_SESSION_BUS_ADDRESS`; goldens become the Qt suite in devdeps. "add X" (:343-346): the KDE release set and Gear on subject shelves. CLAUDE.md is checked by no tool unless step 8.1.4.1 lands. Gate: `git grep -nE 'kdos-comp|kdos-shell|kdos-res|kdos-term|welcome card' CLAUDE.md` is empty; with 8.1.4.1, anchorcheck passes over CLAUDE.md.

### 8.2 Tests

#### 8.2.1 Deleted (at M6, with the code they test)

| What | Where | Size | Boundary corrections |
|---|---|---|---|
| Goldens | testing/goldens/ | 222 goldens + README (223 files, 1.2 MB) [measured: `ls testing/goldens \| wc -l` = 223] | All are kdos-shell, kdos-res, kdos-term or libkvt surfaces |
| Desktop fixtures | testing/fixtures/{about,ascii,backup,burn,cellclip,crt,deco,devices,display,disptext,firewall,fontpolicy,iconink,iconpng,img,kwl,motion,net,netagent,oblique,openwith,plot,print,privacy,rowanim,sched,shell,term,tile,tone,tray,traymenu,update,verify,vt,wm} | 36 dirs | **Keep `view`**: it serves the libktui view test (selftest.sh:9074-9087). `devices` has no consumer today. `update`, `firewall` and `backup` move to the Qt suite if the KCM tests want them [measured: verdict] |
| selftest.sh desktop blocks | :460-1191 (libkcell/libkwl suite through the comp grafts, crt scopecheck, motion, render-late), :1216-1272 and :1543-1665 (libkvt replay, term goldens), :5376-5583 (netagent, scrollbar, launcher box, app index), :6756-7539 (tray SNI, FileChooser portal, offscreen front ends), :7540-8967 (goldens, kdos-rec, recording indicator), :9061-9072 (key card `--print`) | about 3,400–3,700 of 9,208 lines | **End the first span at :1191.** :1192-1212 compiles kdos-boxsock, which stays; its path at :1207 changes if the port moves. **Keep :9074-9087** (libktui views). :9036-9059 (`kdos appid`) follows the appid decision (§8.5). **Also delete :247-268's kdos-checkpass compile** (the line `$CC ... src/desktop/kdos-lock/checkpass.c -lcrypt` at :267-268 and the "one setuid binary" comment at :247-248), which sits inside the otherwise kept :122-453. Edit INC (:24): drop `-Isrc/libs/libkwm`, `-Isrc/libs/libkvt` and `-Isrc/libs/libkdisp` [measured: selftest.sh:247-268, :460, :1192-1212; verdicts] |
| selftest.c | src/libs/selftest.c test_wm :4876, vt :5724-8258 (test_vt_da, vt graphics/rules/modes, styles/links/marks/notify/img, test_kvt_term, test_kimg :7505/:7697, test_kvt_drive, test_term_render, test_osc52, test_bracketed_paste), includes :37-40, the kdos-shell.md#settings link :5869-5873, the `XDG_CURRENT_DESKTOP=KDOS` cases, and the kxdg_verb_* cases if kxdg_verbs.c goes | about 2,190 of 8,648 lines | — |
| Harness programs | testing/barcheck.c (125), boxcheck.c (119), appindex.c (88) | 332 lines | each compiles kdos-shell sources |
| preflight checks | testing/preflight.sh: rc.xml/menu.xml/menu.conf (:1516-1847), scanout writer (:1919-1942), kdos-shell flags (:1987-2060), libkwl protocols (:2220-2240), help .doc pages (:2445-2455), icon names and icon layer (:2456-2694), control-centre table (:3055-3134) | about 1,000 of 3,477 lines touched | **Rewrite, don't delete:** the shadow-mode check (:1352-1366) stays. Its rationale changes from kdos-checkpass to pam_unix's setuid unix_chkpwd [measured: ports/core/auth/pam/build.sh:32; build/fs/usr/sbin/unix_chkpwd mode 4755]. Also rewrite store wiring (:2107-2128), mimeapps handlers (:2365-2386), toggles/notifyd (:2752-2779) and the Start-list wording (:3401-3440). Keep the glyph, KT_DIM, literal-colour and sync-bracket checks, scoped to libktui tools, and the man-page check (:3441) |
| Build-script text | script/kdosbuild.sh:50-51 (the setuid bits chown strips name kdos-checkpass and kdos-resctl); script/phases/10_bootstrap/000_file_system.sh:146-148 ("THIS IS WHAT kdos-checkpass EXISTS FOR"); script/phases/50_desktop/packages.txt:96, :112 | — | Rewritten to name unix_chkpwd, newuidmap/newgidmap and whatever setuid bits the Plasma set adds (kcheckpass does not exist in 6.x; measure `find build/fs -perm -4000` at M3). The expected setuid count in memory note buildfs-setuid-contamination ("18") drops by 2 plus whatever Plasma adds |
| Rig scripts tied to the cell desktop | quickpatch.sh:88-172, packlane.sh:229-232 (kdos-res dumps), usability.sh:46-54, appsweep.sh (compositor window report), install-to-disk.sh (autologin answer), bootcheck/README.md:51-52, qemu-hw/run.sh:72-89 | — | rewritten (8.2.3). hostcheck.sh has no desktop reference [measured: verdict] |
| Containers | testing/Dockerfile.devdeps:18-75 (alpine:3.23, fcft, wlroots, libseat-dev :67, pango, basu, vim/htop/mc/tmux for kdos-term) | — | Rewritten to alpine:edge (Plasma 6.8.1 is there only) with qt6-qtbase-dev, qt6-qtdeclarative-dev, the kf6 -dev set (kirigami, kcmutils, krunner, kpackage, ki18n), libplasma-dev, kwin-dev, discover headers, qmllint, clang-format, plus the C deps of the libktui tools [measured: inv-docs-tests; Alpine edge 6.8.1 per prior verdict] |

The `kdos stutter`, `hey`, `notify`, `doctor` and `appid` fixtures and tests (fixtures/stutter; selftest :6726) lose their data source: kdos-frames.sock, kdos-cmd.sock, kdos-notify.sock and observed-app-ids are all written by deleted programs [measured: stutter.c:14, :461; hey.c:41; kdos.c:4643-4663, :4949; appid.c header]. Each verb is ported (KWin script ledger, showfps, org.freedesktop.Notifications) or deleted, and its tests and docs follow in the same change.

#### 8.2.2 What replaces them

| Net | Mechanism | Where | Gate |
|---|---|---|---|
| **Split entry points** | `selftest.sh` keeps its bare-host contract (C libraries, daemons, kpkg, A/B, kinstall backend; selftest.sh:11-14). New `testing/plasmatest.sh` runs only in the devdeps container. `make check` runs selftest; `make plasmatest` is separate and part of every milestone gate from M4 | Makefile:282 | `make plasmatest` exits 0 in the container; on a bare host it refuses with a clear message instead of silently skipping |
| **QtTest + ctest** | Unit tests beside each `src/desktop/<qt-port>/tests/`: the Discover backend parsing kpkg/pack/box output, KCM models, the KRunner runner's matches, kdos-sysupdated's sysupdate1 replies (against the XML of the Discover version shipped) | src/desktop/*/tests | `ctest --output-on-failure` green in plasmatest |
| **QML lint and package validity** | `qmllint` over every KDOS QML file; `kpackagetool6 --install` of every plasmoid, KCM, look-and-feel, SDDM theme and KWin script into a temporary prefix; `metadata.json` parse | plasmatest.sh | zero qmllint warnings above the agreed level; every package installs |
| **AT-SPI tree goldens** | KDE's selenium-webdriver-at-spi drives KDOS KCMs, plasmoids, the desk picker, the installer and the SDDM theme over AT-SPI2. A text dump of each tree is the golden. That doubles as the accessibility gate: Orca can read every KDOS surface [source: https://community.kde.org/Selenium] | testing/atspi/ (new), run in the guest | dumps match the committed goldens; a surface with an unnamed control fails |
| **Pixel goldens** | Only for the Phosphor shader: offscreen render of a fixed frame through the effect's shader, compared with a tolerance | plasmatest.sh | within tolerance on the container's Mesa |
| **Session probe** | `testing/plasma-probe.sh`, run through `vnc-shot --root-script` as `su - kdos -c` with the session bus. It asserts the processes kwin_wayland, plasmashell, ksmserver, kded6, xdg-desktop-portal-kde and polkit-kde-authentication-agent-1. It asserts the owned names org.kde.KWin, org.kde.plasmashell, org.freedesktop.Notifications, org.kde.StatusNotifierWatcher and org.freedesktop.portal.Desktop on the session bus, and org.freedesktop.login1 on the system bus. **The session backend is asserted through elogind**: `loginctl show-session $XDG_SESSION_ID -p Active -p Type -p Seat` gives `Active=yes`, `Type=wayland`, `Seat=seat0`, and KWin's log has no `Failed to take control of` line (session_logind.cpp:106, :111). KWin's supportInformation prints no session backend [measured: workspace.cpp:1827-2085]. It is used only for Loaded Effects (Phosphor absent by default) and Compositing. Layout through plasmashell `evaluateScript` (shellcorona.cpp:1991) matches the chosen desk. Window geometry, rules and box tagging through a KWin script (`org.kde.KWin /Scripting loadScript`, scripting.h:323-326). The portal Settings accent reaches a box. org.kdos.* names answer | testing/plasma-probe.sh | every assertion prints `ok`; exit 0 |
| **Login flow** | Rig flows, one boot each, asserted by `testing/flows/probe.sh` through logind (session class, Active, PAM service, VTNr, LockedHint), the account's session bus (org.kde.plasmashell, org.freedesktop.ScreenSaver) and the kernel (foreground VT, `KDGKBMODE`, `FIONREAD`, `suspend_stats`), never a pixel: (1) `01-live-autologin.sh`, live ISO autologin (PAM service `sddm-autologin`) → KSplash → Plasma; (2) `02-greeter-password.sh`, installed disk: SDDM greeter → typed password (PAM service `sddm`) → Plasma; (3) `03-logout-greeter.sh`, installed disk, because the live autologin's `Relogin=true` logs straight back in: `org.kde.Shutdown logout` → greeter; (4) `04-lock-unlock.sh`, lock (Meta+L) → kscreenlocker → password → unlocked; (5) `05-suspend-resume.sh`, RTC-armed `loginctl suspend` (QEMU S3) resumes onto the lock screen; (6) `06-vt-switch.sh`, Ctrl+Alt+F2 reaches tty2 with the session inactive and Ctrl+Alt+F1 (`KDOS_FLOW_SESSION_VT` for another VT) returns; (7) `07-no-tty-leak.sh`, typing `touch /root/kdos-flow-leak` + Enter in Plasma leaves no file, the session VT is `K_OFF` and holds no unread input (a path under `/root`, because KRunner opens on desktop typing and its command runner would create a file the account may write). Written and syntax-checked, with the probe exercised against stubbed loginctl and dbus-send; never run on a rig (ruling 11) | testing/flows/ | one shot per step, every probe step `ok`, and plasma-probe passes after (1), (2) and (5) |
| **Per-accent shots** | `kdos theme <accent>` for each of the 8 accents, then a spectacle shot of panel + Konsole + a KCM, plus the selftest contrast assertion (≥4.5:1 text on every scheme) | flows + selftest theme audit (:9108+, rewritten to read the generated `.colors` and Plasma files instead of GTK/icon/cursor recolours) | 8 shots; contrast check green |
| **Phosphor mode shots** | Only with `vnc-shot.py --gl` (virtio-vga-gl/virgl) or `make run-hw`. KWin treats llvmpipe as software (eglcontext.cpp:322-325), and the plain rig shows no effect that gates itself on that. Whether spectacle's ScreenShot2 capture includes a full-screen post-process effect is unmeasured, so the first Phosphor shot is taken both ways (spectacle and the rig's VNC frame) and the result decides which tool the docs name [measured: verdict; inferred for the capture] | flows | shot pair recorded at M7 |
| **Boot timing** | `testing/session-cost.sh`: time from rcS start to the SDDM greeter, from greeter auth (or autologin) to KSplash's last stage (org.kde.KSplash setStage, ksmserver/server.cpp:623-627, startplasma.cpp:698), and to org.kde.plasmashell owned. The baseline is today's ISO, tty1 autologin to the kdos-shell panel, measured at M0 | testing/session-cost.sh | numbers recorded per release in testing.md. The budget is set at M3 from the first measurement: proposed ≤ +5 s versus the M0 baseline on the rig with `--gl` [inferred: target, not measured] |
| **RAM budget** | Idle PSS of the session processes from `/proc/<pid>/smaps_rollup`, 60 s after the desktop settles, summed. Recorded with the ISO size delta | session-cost.sh | proposed ceiling recorded at M3, then a regression of more than 10% fails the M6+ gates [inferred: threshold] |
| **Polkit model** | fixtures/polkit's stub-and-assert is rewritten for the new 50-kdos.rules: assert every action id KDOS relies on (timedate1, hostname1, locale1, org.kde.powerdevil.*, kpmcore, sddm-kcm, fwupd, Accounts, tlp-pd, the KDOS KCM helpers) against an active local session, an inactive session and a wheel member | selftest.sh :5352-5375 (rewrite) | every action answers as the table in the security section says |
| **oomd protected set** | selftest: the oomd fixtures (80 files) exercise kwin_wayland, `kwin_wayland_wr` (comm cut at 15), plasmashell, ksmserver, kded6, sddm, elogind-daemon, ksecretd and xdg-desktop-portal-kde as protected | selftest.sh oomd block + fixtures/oomd | the fixture's largest non-session process is the one chosen |

#### 8.2.3 Rig changes

- [ ] **vnc-shot.py readiness gate.** Today it is not a probe. Without `--console-cmd` it is a fixed `time.sleep(args.wait)` after "letting the desktop settle" (:873-875), and the docstring at :852-875 reads "TTY1 IS THE DESKTOP". The seat explanation at :139-145 and :862 is seatd's [measured: verdict]. The new gate polls in the guest until `org.kde.plasmashell` is owned on `/run/user/1000/bus` and KSplash has reached its final stage, with a timeout that fails the run. `--cmd` exports `DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/1000/bus` (the default per-user bus path; it changes if the per-user service decision changes it). A `--dismiss-picker` step closes the first-run desk picker. `--console-cmd` moves to tty2, reached by Ctrl+Alt+F2 through logind VT switching. `--gl` keeps governing Phosphor mode. Gate: a run with a deliberately broken plasmashell fails at the gate instead of photographing a black screen.
- [ ] **SDDM VT.** Ship `fs/etc/sddm.conf.d/00-kdos.conf`, the one KDOS SDDM drop-in (05 §5.2; `[Autologin]` lives only in `kde_settings.conf`), as Wayland greeter only with no `[X11]` section, with the greeter's VT chosen (tty1 or vt7) and tty2 kept as the recovery getty. Measure which VT SDDM takes under elogind before vnc-shot hard-codes one. Gate: the vnc-shot gate and flow (6) pass.
- [ ] **quick.sh / quickpatch.sh.** Replace `pkill -x kdos-comp`, `pkill -f kdos-desktop-start` and `su - kdos -c 'exec kdos-desktop'` (quickpatch.sh:100, :118-119, :151, :155, :172) with a restart per component kind:
  - plasmashell: `kquitapp6 plasmashell; kstart plasmashell`;
  - KWin: `kwin_wayland --replace`, which calls org.kde.KWin /KWin replace and returns; the wrapper restarts on exit 133 without counting a crash [measured: main_wayland.cpp:410-489; kwin_wrapper.cpp:140-160];
  - a KWin effect: `qdbus6 org.kde.KWin /Effects unloadEffect/loadEffect <id>`;
  - a KWin script: `/Scripting unloadScript`, then `loadScript`;
  - KCMs, Discover and the installer: spawned fresh.
  Names past 15 characters (xdg-desktop-portal-kde, polkit-kde-authentication-agent-1) need `pkill -f`, guarded against matching the loop's own command line. Logs are read from the session's own log path (SDDM's `~/.local/share/sddm/wayland-session.log`; measure). quick.sh cannot carry an SDDM theme or anything owning a D-Bus name, the same limit it has today. Gate: `testing/quick.sh kdos-plasmoid-updates -- --sleep 3 --shot build/shots/x.png` shows the changed plasmoid.
- [ ] **usability.sh/.md, packlane.sh, appsweep.sh, install-to-disk.sh, bootcheck/README.md, qemu-hw/run.sh.** Window presence comes from a KWin script (`workspace.windowList()`) or org.kde.KWin, boxes from `kdos-box list`, and the install answer names SDDM autologin. The sweep walks the desk picker, Kickoff, KRunner, Overview, snap, notifications, the KDOS KCMs, Discover, lock and greeter, and the Phosphor toggle; a person reads the shots. Gate: each script runs green on an M3 ISO, and `git grep -nE 'kdos-comp|kdos-desktop|kdos-res' testing/*.sh testing/*/README.md testing/qemu-hw` is empty.
- [ ] **Makefile.** Comments :24-33 (KDOS_RES as Plasma scale), :204-216, :226-241 (run-hw renders KWin on the real GPU), and the QUICK example :355-369. New targets: `plasmatest`, `shots`, `session-probe` (boot, probe, timing, RAM). Gate: `make -n plasmatest shots session-probe` resolves each target; `grep -n 'kdos-comp\|kdos-shell' Makefile` is empty.

#### 8.2.4 Preflight toolkit guard

testing/preflight.sh:1302-1342 reads every ELF of every port under `src/*/*/` and fails on any NEEDED entry matching `libgtk-|libgdk-|libadwaita-|libwebkit|libjavascriptcoregtk|libQt[0-9]|libKF[0-9]|libwx_|libfltk|libtk[0-9]|libX…` [measured: :1322]. As it stands it fails every KDOS Qt component, including the installer, the Discover backend, the KCMs, a C++ runner, the ksystemstats plugin and the KWin effect.

- [ ] Rewrite it under D3/rule 5, at M0 in the same change as the rule (02 §2.7 step 0). Ports under `src/desktop/` may link `libQt6*` and `libKF6*` (plus libplasma, libkwin, libdiscover and kdecoration). Every `src/` port is still refused GTK, libadwaita, webkitgtk, wxWidgets, FLTK, Tk, `libQt5*` and Xlib. Ports under `src/system`, `src/daemons`, `src/art` and `src/devtools` (the TUI tools and daemons) are still refused every toolkit. The check title and comment state the new rule. Rewrite docs/kfs/11-toolkits/intro.md:30-48 in the same change. Gate: a fixture ELF linking libQt6Core under src/desktop passes, the same ELF under src/system fails, and one linking libgtk-3 under src/desktop fails.
- [ ] New preflight checks:
  - the release set is one version: every Plasma port at the same 6.8.x, every Gear port at one 26.08.x, every KF6 port at one 6.x;
  - `startkderc` has `systemdBoot=false`; no file under fs/ or any package path matches `usr/lib/systemd/`, `etc/systemd/`, or a `*.service`/`*.socket` outside `/usr/share/dbus-1`; `*.timer` is allowed only under `etc/kdos/timers.d` and `etc/skel/.config/kdos/timers.d` (KDOS's own timer files: fs/etc/kdos/timers.d/10-update-check.timer and 20-fstrim.timer, fs/etc/skel/.config/kdos/timers.d/10-backup.timer and 20-updatedb.timer, and fwupd's `etc/kdos/timers.d/30-fwupd-refresh.timer`) [measured: `git ls-files fs`; ports/core/boot/fwupd/build.sh:56]. Gate: a fixture `fs/etc/systemd/system/x.timer` fails the check, and the five KDOS timer files pass;
  - every KDOS plasmoid, KCM and look-and-feel has a valid metadata.json;
  - the shipped kglobalshortcutsrc and kwinrulesrc parse, and name existing commands;
  - the layout.js presets parse;
  - the KNewStuff kiosk restriction `[KDE Action Restrictions] ghns=false` is in /etc/xdg/kdeglobals (D10);
  - no X11 greeter config, and `fs/etc/X11` does not exist;
  - every kcm-about-distrorc `ExtraSoftwareData` script prints two lines on a single-root machine, because one line makes kinfocenter qFatal [measured: ThirdPartyEntry.cpp processOutput];
  - `XCURSOR_THEME`/`XCURSOR_SIZE` are not exported by profile.d, because KWin prefers them over kcminputrc [measured: fs/etc/profile.d/10-wayland.sh:75-76; kwin cursor.cpp:115-126].

#### 8.2.5 Memory notes that go stale

They are handled in the implementation round, not deferred to M6 (ruling 12, §8.5): the obsolete notes are deleted and the partly wrong ones corrected to the tree as it stands, and the index `MEMORY.md` loses or rewrites their lines in the same change. The lists below are what the round works from.

- **Obsolete (describe deleted code), 22:** bb-capture-replay, be-desktop-state, cage-compile-gate, cell-width-is-not-max-advance, console-cage-for-everything, console-desktop-state, console-desktop-traps, console-sprite-registration, console-window-model-resize, con-view-retint, dump-harness-candidate-link, embedded-guest-one-frame, embed-path-ceiling, golden-regeneration, haiku-desktop-state, imgui-gains-implementation, kdos-con-host-lab, libs-compile-gate, render-path-benchmarks, tui-engine-cannot-be-replaced, view-backpressure, wlroots-drag-source-lifetime.
- **Partly wrong after M6, 14:**
  - kdos-fast-loop (quick.sh);
  - rig-sweep-recipe (compositor window report, goldens);
  - host-vs-container-selftest (libkwl);
  - ctrl-chord-vocabulary (the libktui half survives);
  - kdos-audio-stack (the RT grant moves from kdos-getty to pam_limits under SDDM);
  - nm-no-session-tracking (wrong once NetworkManager is built with `-Dsession_tracking=elogind`);
  - buildfs-setuid-contamination ("18 setuid bits");
  - book-structure (chapter list);
  - plan-files-state (desktop.rev.3);
  - native-apps-plan (toolkits now also for the desktop);
  - terminal-animation-load and kdos-bb-timing (the kdos-term render path; kdos-bb itself stays);
  - workflow-agents-have-bash (the libkwl/libkcell gate);
  - kde-plasma-session-plan (the second-session framing is superseded by D1–D12).
- **Still valid:** the other 29 or so, e.g. adding-a-port-real-traps, musl-port-traps, port-test-needs-tree-flags, fresh-build-cost (its hours grow), pgrep-self-match, reading-rig-screenshots, rig-shot-cost.

#### 8.2.6 Deletion-adjacent ports the test and doc counts depend on

These go with the M6 deletion (02 §2.4, §2.7 steps 7a–7d), each with its catalogue row, kfs.md and `#port-` links:

- grim, slurp, wtype, wl-kbptr, wvkbd, wl-mirror, wf-recorder and wayvnc: their wlr protocols, screencopy, layer-shell or virtual input, are absent from KWin [measured: inv-features verdict and missed]; neatvnc with wayvnc, its only consumer;
- fuzzel (KRunner), foot (Konsole is the one terminal, B3), fcft and tllist (no consumer once foot, fuzzel and src/desktop go), qt6ct, ffmpegthumbnailer, whisper-model-base-en, and terminus-ttf unless Phosphor mode keeps it [measured: 02 §2.4];
- xdg-desktop-portal-wlr, wlroots and seatd.

foot, fcft and tllist go (01 B3); gnuradio moves to konsole first (`-DGRC_XTERM_EXE=/usr/bin/konsole`, depends foot → konsole) [measured: ports/core/sdr/gnuradio/build.sh:37,89]. qt6ct goes; qt5ct stays on its `kde` key for the 28 Qt5 apps. wl-clipboard stays: it works on the host over ext-data-control, though not inside a box. The new kwin recipe must list `xwayland` in `depends`, because its only declared reverse dependency today is wlroots and KWin's find_package is optional [measured: kwin CMakeLists.txt:281]. phaseclosure refuses a partial order, so each consumer goes in the same change as its dependency [measured: inv-code risk].

### 8.3 Milestones

Milestones run in order. The work happens in a git worktree and never in the main checkout while a build runs from it [measured: inv-code risk 15]. Each gate is measurable. **The tree always has a working desktop.** Until M6 the cell desktop stays built and in the phase lists, and until M3 the image keeps its tty1 autologin into it. M3 is the **login switch**: the shipped login path becomes SDDM → Plasma once Plasma passes its gate. **The box confinement and identity work lands in M3, not later.** From M3 the shipped image runs boxes under KWin, and without 06 §6.3.2 mitigations 1–3 (private runtime dir, xdg-dbus-proxy, no untagged fallback, `kwin/sandbox-policy.patch`) and the §6.3.4 launcher-id task every box would reach the globals KWin leaves open to untagged clients, ScreenShot2 and the raw session bus, and show generic icons [measured: 06 §6.3.2, wayland_server.cpp:132-162]. 06 §6.3.2 already requires them "before boxed apps are shown under KWin"; M3 is that point. From M3 until M6 the cell desktop is still built but has no login path; a developer can reach it only by `exec kdos-desktop` from tty2, and that path is never documented as a feature. M6 is the **deletion**; it lands only after M5's gate has passed on an image without a cell login path. "Login switch" (M3) and "deletion" (M6) are the names every section uses. The user commits; no milestone commits on its own.

02 §2.7's ordered steps map onto the milestones as follows:

| 02 §2.7 step | What it lands | Milestone |
|---|---|---|
| 0 | Rule rewrite and the narrowed preflight ELF guard | M0 |
| 1 | elogind and the flag flips; Plasma, frameworks, Gear, SDDM built and listed | M1 + M2 |
| 2 | Rig harness for Plasma (vnc-shot gate, quickpatch, probe) | M2/M3 (on a step-1 ISO) |
| 3 | Login switch | M3 |
| 4 | Kept consumers rewritten (kinstall TUI autologin and Theme step at M3; the kdos-tools verbs, kb_terminal, kdos-doc, fallbacks, gnuradio and X-KDOS-Term recipes, fcitx5 by M5) | M3–M5 |
| 5 | kdos-boxsock re-homed to `src/system` | M5 |
| 6, 7a–7d, 8 | The deletion change, the third-party removals, image and tooling text | M6 |

| M | Scope | Covers | GATE |
|---|---|---|---|
| **M0 Worktree and policy** | Open a worktree and record baselines: ISO size, installed size, the boot-to-panel time and idle PSS of today's image (session-cost.sh run against the cell desktop). Rewrite CLAUDE.md rules 3/4/5 and the conventions to the D3 text, plus the decisions.md new entries (policy only; principles.md bodies wait for M1, because they describe running behaviour). Optional book-check extension (8.1.4.1). The preflight toolkit guard rewritten in the same change as rule 5 (8.2.4, first checkbox; 02 step 0), because the first `src/desktop` Qt port fails the guard as it stands. Test-build krunner, ksvg, plasma-wayland-protocols, kdecoration, libplasma, kwin and plasma-workspace 6.8.x in the devdeps container with the tree's CC/CXX/CFLAGS/LDFLAGS exported | Policy text; 8.1.4.1; 8.2.4 guard; porting-section prototypes | `make check` green in the worktree. The guard's three fixtures behave as 8.2.4 states (a libQt6Core ELF under src/system fails, under src/desktop passes; libgtk-3 under src/desktop fails). kwin and plasma-workspace compile in the container with no unexplained patch. Baseline numbers written to the plan. `git status` in the main checkout is unchanged |
| **M1 Platform** | elogind (musl patch set, `SYSTEMD_REBOOT_TO_BOOT_LOADER_ENTRY=1`), the per-user service manager (turnstile with a ksvc backend, or `ksvc --user`) and the bus at `$XDG_RUNTIME_DIR/bus`, polkit with `-Dsession_tracking=elogind` plus polkit-kde-agent-1 and a rewritten 50-kdos.rules, the timedate1/hostname1/locale1 provider, tlp-pd under ksvc, and the elogind flag flips (pipewire, wireplumber, libpulse, fwupd, gvfs, gnome-disk-utility, networkmanager; pam and shadow unchanged), the single-owner change for the session bus, PipeWire and `/run/user` (3.3: `kdos_session_bus`/`kdos_session_audio` and `15_userdirs.sh`'s `/run/user` go in the same change) with kdos-powerd's sleep hooks moved to elogind's system-sleep directory. Docs: principles.md bodies, the seatd/no-logind text (34 places), security-model polkit, boot-and-init rcS, daemons suspend, the affected kfs.md Flags bullets, and administration time/users | Platform section | In a full build of the worktree, under the **cell desktop**, still the login: `loginctl list-sessions` shows the tty1 session Active and seat0; `pkcheck --action-id org.freedesktop.timedate1.set-time --process $$` succeeds for the active wheel user and fails for an inactive session; `busctl --user list` answers at `/run/user/1000/bus`; `pgrep -c -u kdos -x pipewire` and `pgrep -c -u kdos -x dbus-daemon` are each 1 and `ss -xl \| grep -c /run/user/1000/bus` is 1 (3.3); NetworkManager sleeps exactly once per `loginctl suspend` (syslog); PipeWire keeps RLIMIT_RTPRIO 95; `make check` green |
| **M2 KDE stack builds (waves)** | Wave 1: KF6 gaps (krunner, ksvg, kuserfeedback off or absent, syndication, kdav) and the Plasma libraries (kdecoration, layer-shell-qt, kpipewire, plasma-activities(-stats), libplasma, plasma5support, union, cxx-rust-cssparser, qcoro, appstream `-Dqt=true`, plasma-wayland-protocols bump, breeze/kwayland/plasma-integration/libksysguard bumps). Wave 2: Plasma core (kwin, kscreenlocker, kglobalacceld, plasma-workspace with `-DINSTALL_SDDM_WAYLAND_SESSION=ON`, plasma-desktop, powerdevil, systemsettings, xdg-desktop-portal-kde, sddm, sddm-kcm, and the rest of the core tier). Wave 3: the 55 Gear bumps, then 118 tier-1 modules, 39 tier-2 and PIM tiers 3–10 with their about 98 third-party prerequisites. Every new KDE recipe passes `KF_SKIP_PO_PROCESSING` (04 §4.5; A1). Every port's sources published with `ports/publish` and indexed; catalogue rows, kfs.md, tier-a and `make book-stamp` per wave. No meta port names more than 128 depends [measured: KP_MAX_DEPS] | Porting section; 8.1.4.3; 8.1.5 | Per wave: `make build BUILD_ARGS="--phases <phase>,70_image"` completes; `python3 testing/phaseclosure.py`, `bookcounts` (via docscheck) and kfscheck green; the pre-push hook accepts the sources.idx lines. Full build time and ISO size recorded against M0. |
| **M3 Login switch: Plasma boots via SDDM** | The shipped login path switches: SDDM on kwin_wayland (upstream beta; gate below), the KDOS SDDM theme placeholder (Breeze until M4), autologin on the live medium, `startkderc systemdBoot=false`, the splash→SDDM handoff from inittab/ksvc after the splash quits, kscreenlocker with PAM, powerdevil, the logout greeter's boot-entry text (`bootctl` shim on PATH), KWallet unlock through kwallet-pam, and oomd's protected set. The 02 §2.2 kdos-installer TUI edits: autologin is written to `/etc/sddm.conf.d/kde_settings.conf` `[Autologin]` instead of `login.conf` [measured: src/system/kdos-installer/install.c:1765-1833], and the Theme step writes the Plasma look-and-feel (Breeze until M4's global themes exist) instead of running the cell `kdos theme` [measured: install.c:2000-2034]; without them an install from an M3 ISO ignores `autologin=yes`. Boxes under KWin, from 06 §6.3.2 mitigations 1–3 and §6.3.4: each box's private `XDG_RUNTIME_DIR` without `wayland-0` or `bus`, xdg-dbus-proxy per box, the untagged fallback removed, `ports/core/kde/kwin/sandbox-policy.patch` with the grant table, and genlaunchers' `app.<id>.desktop` ids and icons with kbuildsycoca6. Docs: 8.1.4.4 (session.md, overview, boot-and-init, getting-started, configuration, filesystem-and-ipc, three kfs wiki pages), kinstall.md and installation.md (autologin writer, Theme step), kdos-appbox.md (environment, sockets, launchers) and security-model.md (sandboxed clients). Rig: the new vnc-shot gate, the probe, and the flows | Session/login section; 8.2.2 probe and flows; 8.2.3; 06 §6.3.2, §6.3.4 | On the rig, from the ISO (flows (2) and (3) from an installed disk): flows (1)–(7) pass with shots, which is also their first run (ruling 11). `testing/install-to-disk.sh` with autologin chosen boots the installed disk into SDDM autologin. Inside a box, `ls $XDG_RUNTIME_DIR` shows no `wayland-0` or `bus`; a boxed test client binding `zwlr_layer_shell_v1` is refused; Task Manager shows a boxed app's own icon (shot). plasma-probe exits 0. `loginctl` shows `Type=wayland Active=yes`. `find build/fs -perm -4000` recorded. session-cost.sh numbers recorded, and the timing and RAM budgets set from them. A non-US keymap typed at the greeter unlocks (XKB_DEFAULT_* or locale1). No black frame between splash and greeter in a 1-fps VNC capture |
| **M4 Identity kit** | KDOS Modern: 8 accent colour schemes and global themes (bone default), Plasma style, window decoration (breeze rebuilt with decorations, cursors and wallpapers ON, or a KDOS KDecoration3 plugin), icons with `FollowsColorScheme=true`, cursors, wallpapers, sound theme, the SDDM theme, KSplash and the lock screen designed together, Noto Sans + the measured KDOS Mono, the Konsole profile (`/etc/xdg/konsolerc` with `SyncProfileWithSystemTheme=true`), qt5ct written from the KDE scheme, `[Colors:*]` for out-of-session processes, the `kdos theme` wrapper with an explicit wallpaper step, the 10-wayland.sh XCURSOR lines removed, and the GTK identity files rewritten (90_kdos.gschema.override, gtk-3.0/gtk-4.0 settings.ini). Docs: theming.md and design-language.md rewritten (they describe the shipped look from here on) | Identity section | 8 per-accent shot sets. A live `kdos theme amber` repaints panel, decorations, Konsole and a GTK app without relogin (before/after shots). The SDDM, KSplash and lock shots are visually one family (person review). The contrast assertion passes for all 8 accents. A Qt5 app (VLC) shows KDOS colours |
| **M5 Integration P0** | Discover: the KDOS backend(s) for ports, packs, boxes and fwupd; `-DBUILD_SystemdSysupdateBackend=ON`; kdos-sysupdated serving org.freedesktop.sysupdate1, with the long work in Acquire (the only job whose Progress Discover shows), Install kept short, and `127.0.0.1:3129/v1/updatesize` answered or 0 accepted; the KDOS notifier plugin or the update plasmoid. Until M8, `CheckNew` answers empty on a release image and Discover's Updates page shows only fwupd (06 §6.2.2; risk 15); the A/B path is proven here on fixtures. KNewStuff kiosk-disabled. Boxes under KWin, on top of M3's confinement: one decocolor rule per box and box gc without `kdos hey`. kdos-boxsock re-homed to `src/system` (02 step 5). Phosphor mode's built-in effect: `ports/core/kde/kwin/kdos-crt.patch` adds `kdos_crt` with the GLSL ported from `src/desktop/kdos-comp/src/kdos-crt-pass.c` (05 §5.4.2; 07 §7.2.1 task), so the shader exists in the tree before M6 deletes kdos-comp; the toggle and polish wait for M7. The Kirigami installer over kinstall's backend with Partition Manager and the OEM path arming plasma-setup. The desk picker and the three layout.js desks. The Boxes, Slots/Updates and Accent KCMs, the KRunner runner. Docs written for each as it lands: draft kwin.md/plasma-shell.md/kdos-settings.md/discover.md (they are not linked from Contents until M6), applications.md, packs-and-boxes.md, kinstall.md, installation.md | Integration, updates and installer sections; 8.2.2 QtTest/AT-SPI | `make plasmatest` green. Rig: Discover lists a port, a pack and a box and installs one. With a fixture `update.json` and a locally built fixture binhost (`kpkg index --sign` over two packages, served from the rig), Discover shows the system update, Acquire/Install arm `kdos-bootctl try`, the notifier shows Restart, and the reboot lands in the trial slot; a crashing trial session rolls back (mark-good requires both the userspace and the session marker). A boxed app shows the box tint. A boxed test client cannot bind org_kde_kwin_fake_input. On `--gl`, `qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kdos_crt` returns true and a shot shows the pass; the 07 §7.2.1 gate (the patch names every uniform of kdos-crt-pass.c) passes. The Kirigami installer installs to a qcow2 disk that boots to SDDM. The OEM path boots into plasma-setup. The desk picker switches between all three desks (shots). AT-SPI dumps for every KDOS surface committed |
| **M6 Deletion** | Precondition: M5's `kdos-crt.patch` carries the ported shader. Exactly 02 §2.7 steps 6–8. **Step 6, one change:** src/desktop/{kdos-comp,kdos-shell,kdos-term,kdos-res,kdos-lock,kdos-record,xdg-desktop-portal-kdos} and their 50_desktop lines; kdos-mountd and `fs/etc/init.d/58_mountd.sh` (B2); src/art/{kdos-icons,kdos-cursors,kdos-gtk-theme} and kdos-theme with their `41_system/src-art.txt` lines; the kdos-tools files hey, stutter, shot, thumb and speech and the panel/menu dispatch; the 8 libraries and `kxdg_verbs.c`; the test deletions of 8.2.1; the fs/ deletions of 02 §2.5 that the login switch left (kdos-desktop, kdos-desktop-start, skel kdos-comp, foot and xdg-desktop-portal-wlr configs, kdos-portals.conf, menu.conf, kdos-applications.menu, the 23 kdos-*.desktop surfaces, the F1 pages); `90_kdos.gschema.override` is rewritten (M4, 02 §2.5, 05 §5.2.0), not deleted; the reasons of 02 §2.6; `Dockerfile.devdeps` drops wlroots, fcft and libseat. **Steps 7a–7d, in order:** grim, wtype, wl-mirror, wvkbd, wl-kbptr, wf-recorder, fuzzel, qt6ct, terminus-ttf (unless Phosphor mode keeps it), ffmpegthumbnailer, whisper-model-base-en and foot (after gnuradio moves to konsole); slurp and xdg-desktop-portal-wlr, wayvnc then neatvnc; fcft then tllist; wlroots, then seatd and `45_seatd.sh`. **Step 8:** image and tooling text (050_theme.sh, 070_user.sh:73 and :79-90 including the console/VT residue, phase.env, ports/shelves:80, Makefile, kdosbuild.sh:50-51, 000_file_system.sh:146, kfsgen.py AREA_DESC). Every src port rebuilds once, because the src/libs hash changes. Then the whole book work in 8.1.4.5–8.1.4.8, the new preflight checks (8.2.4), CLAUDE.md map and rig rows, README.md and the screenshots | 8.1, 8.2; 02 §2.7 steps 6–8 | `make check` green, with a seeded stale link failing. `make plasmatest` green. `git grep -nE 'kdos-comp|kdos-shell|kdos-term|kdos-res|libkwl|libkcell|seatd|wlroots' -- docs src fs testing script CLAUDE.md README.md` prints only intended survivors (none expected), and `git grep -nE 'foot|fcft|tllist|fuzzel|neatvnc|qt6ct|ffmpegthumbnailer|kdos-mountd' -- ports/core script/phases` names no recipe or list line. A fresh full build with build/fs, build/mark and build/cross emptied [measured: fresh-build-cost note] produces an ISO on which flows (1)–(7), the probe and the M5 gate rerun green. `make book BUILDFS=build/fs` renders. `make book-data` refreshed |
| **M7 Polish and innovations P1** | Phosphor mode on `kdos_crt`, the built-in KWin effect added by `ports/core/kde/kwin/kdos-crt.patch` at M5 (05 §5.4.2; an out-of-tree effect's IID embeds KWIN_PLUGIN_VERSION_STRING and breaks at every point release [measured: effect.h:1134]), with fullscreen windows exempt for direct scanout, the HDR path checked and the degauss pulse within photosensitivity limits; retro accents; Terminus as an option; one toggle offered on the Welcome tour. The auto-tiling KWin script bundled but disabled, with its toggle. The Firewall and Backups KCMs (plasma-firewall has no nftables backend). The update tray polish, the KRunner `kdos` verbs, khelpcenter integration of the book (X-DOC-Weight -100), the kcm-about-distrorc scripts, mpd media keys and box session restore on logout. P1 innovations per the innovation section | Identity and innovation sections | Rig on `--gl`: Phosphor on/off shot pair; the effect is absent from Loaded Effects by default (probe). Software-GL rig: the toggle is disabled with an explanation. Krohnkite toggled tiles three windows (shot). About This System shows the slot and box lines with no qFatal. AT-SPI dumps updated |
| **M8 Updates delivery** | Step 1: a signed kpkg binhost, with Discover's progress at package granularity, kept as the default update path until slot images exist. Step 2: signed whole-slot images with deltas written into the inactive A/B slot (D11), so "Restart to update" takes minutes. `kdos update apply --source` stays. Docs: discover.md, packaging.md (binhost, deltas), administration updates, ab-slots-and-rollback and updating kfs wiki | Updates section | Step 1: a rig machine updates from the binhost in under 30 minutes, with a signature failure refused (seeded bad signature). Step 2: a delta update on the rig writes the inactive slot, reboots into it, rolls back on a seeded session failure, and the elapsed time is recorded (target minutes [inferred]) |
| **M9 P2** | P2 innovations and app extras: AT-SPI coverage of every Gear default app in D8, krdp/krfb with firewall rows, the ksystemstats box plugin, Plasma 6.9 tracking (6.9.0 due 2027-02-23, needs KF 6.34) with a rebuild-and-verify of every version-locked plugin | Innovation section | Per item, its own gate. The 6.9 bump: every KDOS plugin loads (probe Loaded Effects, plasmoid install, Discover backend listed), and the full flow set passes |

### 8.4 Risks

Merged from all six reports and their verifications, deduplicated. L = likelihood, I = impact (H/M/L).

| # | Risk | L | I | Mitigation |
|---|---|---|---|---|
| 1 | The SDDM Wayland greeter is upstream beta. plasma-workspace defaults `INSTALL_SDDM_WAYLAND_SESSION=OFF` ("BETA, do not deploy to final users yet", CMakeLists.txt:391), and SDDM calls Wayland "experimental" (Configuration.h:41-42). Rule 4 forces this path | M | H | `-DINSTALL_SDDM_WAYLAND_SESSION=ON`. M3 flows are the gate; a greeter regression blocks every Plasma bump. tty2 recovery always works |
| 2 | SDDM has had no tagged release since Feb 2024, and KDE is moving to plasma-login-manager, which needs systemd. sddm-kcm or the SDDM Breeze theme may leave Plasma | M | M | Pin SDDM from git with a sha256; carry sddm-kcm as its own port; the KDOS SDDM theme is self-contained QML. Re-evaluate at each Plasma minor |
| 3 | elogind on musl means carrying Alpine's or Chimera's 385–862-line patch set, re-checked on every bump. Its /run/user tmpfs and its cgroups change what boxes and rootless podman see | H | H | Pin elogind; port the patches with `Subject:` lines; re-prove warm boxes and podman limits (`DelegateSessionCgroups=yes`) at M1; the box and podman selftests run under elogind |
| 4 | Splash→greeter handoff races for DRM master, giving a black screen. Bug 514351 (2–4 s black at KWin start) is open | M | M | Start SDDM from inittab/ksvc after the splash quits; M3 1-fps capture gate |
| 5 | The greeter keymap is US unless locale1 or XKB_DEFAULT_* reaches the greeter compositor | M | H | Ship XKB_DEFAULT_* in the greeter environment at M3, and locale1 with the settings provider at M1 |
| 6 | kscreenlocker depends on PAM. A wrong `/etc/pam.d/kde` locks the user out of their own session | M | H | Flow (4) at every gate; an empty-hash account must be refused; tty2 recovery documented |
| 7 | KWin's CAP_SYS_NICE file capability does not survive kpkg's tar; without pam_limits, KWin and PipeWire lose real-time | H | M | pam_limits for the session in SDDM's PAM stack; M1/M3 gates check RLIMIT_RTPRIO |
| 8 | Box confinement regresses. KWin restricts only 7 globals for sandboxed clients; ScreenShot2 has no caller check (upstream removed it on purpose, kwin 0ea31ae7); a box can open wayland-0 untagged, and kdos-appbox falls back to it when boxsock is slow; boxes get the raw session bus and lose ext-data-control | H | H | The KWin restricted-global patch at M3, before the shipped login runs boxes under KWin; a private runtime dir without wayland-0 plus xdg-dbus-proxy as the real boundary; security-model.md states "same posture as before, not secure" until then; no fallback to the untagged socket |
| 9 | KWin rewrites a box window's desktopFileName and resourceClass to `app.<id>`, which matches no `<upstream>.desktop` launcher: generic icons and broken pins for nearly every box | H | M | genlaunchers writes `app.<id>.desktop`; one decocolor rule per box; gc keys on pid-to-box, not on app_id |
| 10 | Turning on session tracking wakes polkit's allow_active defaults; KAuth actions (powerdevil, kpmcore, timedate1, sddm-kcm, ksysguard helper) were never granted | H | M | Rewrite 50-kdos.rules and security-model.md in the M1 change; polkit fixture asserts every action |
| 11 | Settings pages break without systemd-shaped services: Date & Time (timedate1), Users (AccountsService), Leave and Discover reboot (login1), plasma-setup (hostname1/timedate1 plus a systemd unit), System Monitor's Applications view (systemd cgroups), the OOM notifier (systemd1) | H | M | elogind provides login1; the settings provider provides the rest; accountsservice is ported; the Applications page is hidden or the cgroup layout changes to user-<uid>.slice/user@<uid>.service; each gap not closed goes in known-gaps |
| 12 | Serving systemd-named APIs steers KDE onto systemd paths: startplasma's useSystemdBoot probe, KIO's SystemdAsService at Version ≥ 250, and shutdowndlg running `bootctl` | M | M | `systemdBoot=false`; `_KDE_APPLICATIONS_AS_SCOPE=1` or no systemd1 name; a `bootctl` shim that answers only `list --json=short`; shims answer only what is measured |
| 13 | Version-locked KDOS plugins (CRT effect, decoration, Discover backend and notifier, ksystemstats plugin, carried KWin patches) break on every Plasma release, every 4 months. Effect IIDs embed KWIN_PLUGIN_VERSION_STRING | H | M | The CRT effect is built into KWin by `kdos-crt.patch`, so it cannot fail to load against a mismatched KWin, at a 1–8 h rebase per release (05 §5.4.2); the other plugins each live in a port that depends on the exact KWin/Plasma version; the M9 bump gate; the probe checks Loaded Effects |
| 14 | org.freedesktop.sysupdate1 is not API-stable (systemd 260 broke Discover's sysupdate on KDE Linux, March 2026). Discover shows progress only for Acquire, never calls ListJobs, and fetches size from localhost:3129 | H | M | Pin kdos-sysupdated to the XML of the shipped Discover; QtTest against that XML; long work in Acquire |
| 15 | Source-built updates take hours to a day for the Qt/KF6/Plasma/QtWebEngine stack; Discover would sit at "3%" | H | H | Do not ship the Discover update path before the M8 binhost; `--source` stays a CLI path |
| 16 | mark-good runs at the end of rcS before login, so a slot whose Plasma fails is confirmed | H | H | mark-good requires both a userspace and a session marker (an explicit AND); greeter-only and headless boots get their own confirmation point, or every trial slot rolls back |
| 17 | The rig loses its regression net: 222 goldens and about 3,500 selftest lines. Plasma regressions are invisible to `make check` until the replacements exist | H | M | Probe, flows, QtTest and AT-SPI goldens land by M5; M6 deletes only after they pass |
| 18 | selftest's bare-host contract means desktop coverage moves to the container and the rig; a bare-host pass proves little about the desktop | H | L | `make plasmatest` is part of every gate from M4; testing.md says what each proves |
| 19 | Spectacle via ScreenShot2 may not capture post-process effects; the plain rig is software-rendered | M | L | Phosphor shots only with `--gl`/run-hw; measure spectacle against the VNC frame at M7 before docs claim either |
| 20 | The CRT pass blocks direct scanout (power, latency), interacts with HDR (RGBA16F), may appear in screencasts, costs contrast and has a degauss pulse | M | M | Off by default (D2); exempt fullscreen windows; photosensitivity check; documented costs in theming.md |
| 21 | Link debt: about 400–470 links into deleted or rewritten anchors (95 `#port-`, about 60 known-gaps, about 35 c-libraries, 135 into session.md); kfs.md links are checked only by kfscheck's render | H | M | 8.1.4 order; Keep-heading lists; a reviewed retarget script; anchorcheck plus kfscheck after each batch |
| 22 | The hand-typed `Ch NN` reading paths are checked by no tool | M | L | Option A (4-for-4); if B or C, renumber README.md:50-97 by hand in the same change |
| 23 | Rewrites drift into history ("KDOS no longer…", mapping tables); the BAD regex catches only stock phrases | H | M | The mapping lives in this plan and commits; review every rewritten page against rule 2 |
| 24 | bookcounts forces catalogue rows with every one of about 360 port additions; vendoring counts are hand-maintained; staged recipes need ports/unbuilt | H | L | Per-wave catalogue edits; a row generator script (optional); the 8.1.3 vendoring task |
| 25 | Deleting command-index rows deletes their man pages; new KDOS Qt binaries need rows, or the preflight man-page check fails | M | L | Rows added with each binary |
| 26 | CLAUDE.md is unchecked; stale rows mislead every later session | M | M | Rewrite in M0/M6; 8.1.4.1 extends anchorcheck |
| 27 | Removing src/libs directories changes the hash of every source-less src port; editing a library rebuilds every port of ours | H | L | Pay it once in the M6 build |
| 28 | 040_orphans.sh removes deleted packages only at 70_image; a phase-50 rig run can still show stale kdos-* binaries | M | L | Gates run on a 70_image ISO |
| 29 | Deleting src/desktop loses assets still needed: the CRT GLSL (kdos-crt-pass.c, 155 lines), kdos-boxsock, the kdos-res page semantics and the privacy-lamp/meter behaviour. libktui, libkcolor, libkxdg and libkbase are linked by surviving programs | H | M | Port the GLSL into `kdos-crt.patch` at M5, a precondition of M6; re-home kdos-boxsock (M5); keep the 9 libraries |
| 30 | Kept verbs degrade silently: `kdos-appbox open` uses kdos-openwith, `kdos remind ask` uses kdos-prompt, `kdos share` uses kdos-pick, `kdos help`/`doctor` use kdos-keys, the backup timer runs `kdos-backup --once` (a kdos-shell surface), and the appbox warmup reads `favorites` | H | M | Each re-pointed at a KDE tool (kdialog, KOpenWithDialog, the portal) or a CLI in the M6 change; the backup CLI before kdos-shell goes |
| 31 | kdos-oomd's protected list names kdos-comp, kdos-shell, kdos-desk, kdos-notifyd and seatd (main.c:237-242); under Plasma, kwin_wayland and plasmashell become OOM victims; comm is cut at 15 (`kwin_wayland_wr`) | H | H | Rewrite the list at M3 with the session's comms; fixture test |
| 32 | KWallet with autologin prompts for a wallet password on every boot (plasma-nm stores Wi-Fi secrets there) | H | M | kwallet-pam with a password login; for autologin, system-wide NM secrets or an empty-password wallet, documented |
| 33 | Plasma runs XDG autostart and D-Bus activation that KDOS never ran; every port's autostart entry becomes live. Ports that strip autostart (build/fs/etc/xdg/autostart holds only at-spi and orca) break the polkit agent, kwallet-pam, KDE Connect and baloo | H | M | Audit every recipe's autostart at M2; preflight lists autostart entries against an allowlist |
| 34 | qt5ct is the only theming path for 28 Qt5 ports; plasma-integration is Qt6-only and there is no KF5 | H | M | Keep qt5ct on its `kde` key; the KDOS kit writes its colours; qt5ct and its writer are kept or deleted together |
| 35 | XCURSOR_THEME/XCURSOR_SIZE in profile.d override KDE's cursor choice | H | L | Remove the export at M4; preflight check |
| 36 | A live accent switch does not change the wallpaper until plasmashell restarts | H | L | `kdos theme` sets the wallpaper explicitly |
| 37 | KLookAndFeelManager's font detection silently drops a theme's fonts without the workaround key | M | L | Ship the key; per-accent shot shows the fonts |
| 38 | A broken ExtraSoftwareData script crashes kinfocenter and System Settings (qFatal); each runs twice synchronously | M | M | Scripts always print 2 lines and return fast; preflight check |
| 39 | Out-of-session processes (greeter, root apps, boxes) read only XDG_CONFIG_DIRS and render Breeze | H | L | `[Colors:*]` in /etc/xdg |
| 40 | Global-theme defaults do not update for existing users when the theme id is unchanged | M | L | No users yet (first version); documented for later releases |
| 41 | kdos-splash draws to /dev/fb0; dropping FB drivers per KDE advice keeps /dev/fb0 only while CONFIG_FB stays and DRM_FBDEV_EMULATION is on | L | M | Measure fb0's owner (03 §3.9) and set DRM_FBDEV_EMULATION/FB_DEVICE explicitly if simpledrm must provide fb0; kdos.config sets neither today, only SYSFB_SIMPLEFB, DRM_SIMPLEDRM, FB, FB_VESA and FB_EFI [measured: ports/core/boot/linux/kdos.config:826,3478,3502-3504]; boot gate photographs the splash |
| 42 | Per-accent cursors (about 35 MB as Xcursor) and nine theme previews grow the ISO | M | L | SVG cursors or one neutral cursor; ISO delta recorded per milestone |
| 43 | Forked QML (lock, SDDM, welcome) and third-party add-ons break on Plasma minors; Better Blur is archived, LightlyShaders stale, the cube unmaintained | H | M | Derive with `.patch` at build time; ship no third-party KWin add-on except a disabled Krohnkite; lock-screen flow on every bump |
| 44 | KDE's Ocean/Union goal (2026–2028) may obsolete Breeze-specific SVG work | M | L | Keep Plasma-style and decoration SVG investment small |
| 45 | plasma-setup, SDDM theme QML and `--inputmethod plasma-keyboard` need qt6-qtvirtualkeyboard, which the tree lacks | H | M | Port qt6-qtvirtualkeyboard in wave 1 |
| 46 | Discover is unbuildable until qcoro and appstream `-Dqt=true` land; without AppStream metadata for ports and boxes it looks empty | H | M | Wave 1 ports; the backend generates AppStream for ports, packs and boxes |
| 47 | Discover's KNS backend and "Get New" download unsigned QML | — | H | Decided (D10): kiosk `ghns=false`, Discover without KNS, curated offline add-on pack; preflight check |
| 48 | Konsole cannot give each window its own Wayland app_id, so the run-or-raise TUI chords need foot or title matching | H | L | KWin rules or the kdos-raise script match on caption (06 §6.1 Launching row; 07 §7.5.5); foot is not kept (B3) |
| 49 | Meta+Space collides with fcitx5's default group switch (KWin's shortcut filter runs first) | H | M | Ship fcitx5 config moving EnumerateGroupForwardKeys, or pick another chord |
| 50 | mpd has no MPRIS; media keys stop reaching mpd | H | L | mpDris2-style bridge or a kglobalaccel binding to kdos-mpctl |
| 51 | The camera indicator sees only PipeWire streams (bug 482614) | H | L | known-gaps entry; optional KDOS /proc watcher plasmoid (P2) |
| 52 | plasma-firewall has only firewalld/ufw backends and drives systemctl and the journal | H | M | KDOS Firewall KCM over kdos-powerd (M7); `vnc`→krfb and new `rdp` rows |
| 53 | KP_MAX_DEPS=128 silently truncates a meta port naming more depends | M | M | No meta port over 128 depends; split, or raise the limit with a test |
| 54 | A running build mounts ports/, script/, src/ and fs/; list edits in the checkout corrupt it | M | H | All work in the worktree |
| 55 | Scope: 34 innovations exceed one release; P0 alone is a large milestone | H | M | M5 holds only P0; P1 at M7, P2 at M9 |
| 56 | Date ambiguity for Plasma 6.8.0 (tarballs about 2026-10-10, announced 2026-10-14); 6.9 features are merged but unreleased | L | L | The book states versions, not dates; 6.9 tracked at M9 |
| 57 | If systemd were chosen instead, musl support is experimental and SysV support is gone since v260 | — | — | Not chosen (D3); recorded only so the reason is in this plan |

### 8.5 Rulings on the former open questions

Every ruling here is binding: plan text that disagrees with one is wrong and is corrected to it. Elsewhere in this plan "ruling N, §8.5" names the numbered list.

The user ruled on 2026-10-28:

- A. **Part IV layout:** option A, the four chapters in slots 21–24 (8.1.1).
- B. **Book checks over CLAUDE.md and README `<img>`:** anchorcheck and docscheck are extended before the deletion.
- C. **`kdos` verbs that lose their data source:** as B27 — port `notify`, `doctor`, `hey` and `appid`; `kdos-shot` calls Spectacle; delete `stutter`.

The user ruled on 2026-11-02, on the decisions the implementation left open:

1. **Every new account joins `audio`.** Whatever makes the account (kinstall, plasma-setup's OEM account, the Users page through accountsservice, `useradd`) puts it in `audio`, so its PipeWire and KWin get the RT limits `25-pw-rlimits.conf` grants to `@audio` (3.2).
2. **smbd starts only when a usershare exists.** Boot starts smbd only when `/var/lib/samba/usershares` holds a share; the first share a person makes (Dolphin → Properties → Share, kdenetwork-filesharing) starts it through a polkit action, so a machine that shares nothing runs no SMB server (6.1, Files).
3. **One pack manifest, in the shared store.** The record of installed packs lives once, in the shared pack store (`/home/.kdos/packs`, bound into each slot at `/var/lib/kdos/packs`), never per slot, so an A/B switch neither drops nor duplicates a pack (6.2.3).
4. **Updates are published as sharded `binhost-<version>-NN` releases with a signed index.** One signed `kdos-binhost.idx` names, for every package and delta, the shard release that holds it; `kdos update` verifies the index against `/etc/kdos/keys` before fetching from any shard (6.2.3).
5. **kinstall-core stays inside kdos-installer.** It is the directory `src/system/kinstall-core`, compiled and archived as a static `libkinstall-core.a` by the kdos-installer recipe and linked into the one binary `/usr/bin/kinstall`; it is no port and is in no phase list. The Kirigami installer drives `/usr/bin/kinstall` through pkexec (4.1.5, 6.6).
6. **fcitx5-gtk is ported.** The latest release (5.1.7), GTK 3 and GTK 4 modules and no GTK 2 module, on the `input` shelf in 50_desktop and in the `kde-plasma` meta; it writes GTK 3's `immodules.cache` (3.8).
7. **libksysguard is patched so boxes show in System Monitor.** `aggregate-scope-children.patch` counts a scope's descendant cgroups, so a box's processes appear under its app on the stock Applications page (6.3.3, 7.6.1).
8. **KCM help goes through `help:/kdos` into KHelpCenter.** Every KDOS KCM's `X-DocPath` is `kdos/<page>.html#<anchor>`; kdos-doc links the book into `/usr/share/doc/HTML/en/kdos` so KIO's `help:` worker serves it, and the Help button opens it in KHelpCenter (6.4, 7.4.3).
9. **The Verify menu checks a sidecar checksum and the KDOS signature.** Dolphin's "Verify" service menu checks a file against the checksum beside it (`<file>.sha256`, or the directory's `SHA256SUMS`) and, where a `.sig` is beside it, its KDOS signature against `/etc/kdos/keys` (6.1, Help and desk accessories).
10. **KRunner's `ask` talks to llama-server on the loopback address only.** The runner's `ask` verb calls the OpenAI-compatible llama-server on 127.0.0.1:8082 and offers no remote endpoint (7.4.5).
11. **The login flows are written now and proven at the first rig run.** The seven `testing/flows/` scripts of 8.2.2 exist, syntax-checked, before any rig run; the first rig run that boots the image runs them, and until it passes `testing.md` marks them unproven (8.2.2, 8.3 M3).
12. **Memory notes are handled now.** The stale notes of 8.2.5 are deleted or corrected in the implementation round, not deferred to M6.

### 8.6 Work estimate

| Item | Estimate | Basis |
|---|---|---|
| KDOS code deleted | about 172,000 lines of C/H: kdos-comp 56,226, kdos-shell 66,567, libkvt 15,084, libkwl 9,378, kdos-res 6,896, libkcell 5,317, libkchrome 2,996, kdos-term 2,651, libkicon 1,683, xdg-desktop-portal-kdos 1,491, libkimg 1,121, libkdisp 875, libkwm 751, kdos-lock 724, kdos-record 667; plus about 5,900 test lines (selftest.sh about 3,500, selftest.c about 2,190, harnesses 332), 223 goldens and 216 fixture files | [measured: `find … -name '*.[ch]' \| xargs cat \| wc -l`, this session] |
| KDOS code new | about 20,000–28,000 lines. Discover backend(s) and notifier 2.5–3.5k C++. kdos-sysupdated 1.5–2.5k C. KCMs (Boxes, Slots/Updates, Accent, Firewall, Backups) 4–5k QML/C++. Update plasmoid 0.5k. KRunner runner 0.6k. Kirigami installer front end 3–4k. Settings provider kdos-settingsd 1–1.5k C (Default A4). Per-user service backend 0.3k. CRT effect 0.8–1k C++/GLSL. KWin scripts and the desk layout.js files 1k. Global themes, SDDM/KSplash/lock QML 2k. Tests (QtTest, probe, flows, AT-SPI, session-cost) 3–4k. Carried patches: count unknown, measured as written | [inferred] |
| Ports new | about 360: about 70 Plasma, 193 Gear (197 missing minus 4 skips), about 98 third-party, about 10 platform (elogind, sddm, sddm-kcm, turnstile, accountsservice, qt6-qtvirtualkeyboard, qcoro and others). Plus 55 Gear bumps and about 5 Plasma bumps; minus the 13 deleted desktop, art and wlroots-stack recipes, kdos-mountd and kdos-theme, and the 17 other third-party removals of 02 §2.4 (16 if Phosphor mode keeps terminus-ttf) | [measured: gear-inventory.json summary; prior synthesis.ports; inferred for Plasma and platform] |
| Sources to publish | about 360 new tarballs, plus bumps, through `ports/publish` with sources.idx lines before any push | [inferred from the port count] |
| Docs | about 150 files and about 25,000 changed lines. docs/kdos: about 6,300 lines deleted, about 9,000 new or rewritten across 8 rewritten and 4 new chapters, about 6,500 lines of section edits. docs/kfs: about 3,200 rewritten wiki lines, about 1,000 edited, 40–80 new kfs.md (1,500–2,500 lines) and about 85 port-note edits. 83 pins re-pinned | [measured: inv-docs-tests volume; inferred for kfs.md] |
| Build hours | Today a from-scratch build is about 20 h (phase 4 about 11.5 h for 746 packages). The added about 360 KDE ports are mostly ECM/CMake and smaller than QtWebEngine, which is already built. Estimate +6–10 h for a from-scratch build [inferred], measured at M2. Plus one full rebuild of every src port at M6. A Plasma minor bump is about 70 ports plus every version-locked KDOS plugin, estimated 1.5–3 h [inferred] | [measured: memory fresh-build-cost; inferred additions] |
| Disk | Snapshots already cost 84 GB, and KDE adds an unknown share. Measure at M2 and clear build/snapshots before the M6 fresh build | [measured: fresh-build-cost note] |
| Rig time | about 15 s per shot. M3–M6 flows plus 8 accent sets plus the screenshot set is about 120 shots, about 30 min per full pass excluding boot | [measured: memory rig-shot-cost; inferred count] |

