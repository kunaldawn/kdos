# Port fix plan

The fixes found by scanning every build log (2,039 logs in 53 batches), an ELF, ownership and mode scan of `build/fs`, and a fresh audit of the finished tree and ISO (`build/iso-build/kdos.iso`): ports, logs, `fs/`, defaults, initramfs, kernel, versions and metadata. Items from the fresh audit carry their finding id in parentheses; the measurements behind them are in the audit scratch (`audit2/`).

**Rules for every item:**
- Each fix updates every page under `docs/kdos/`, every code comment and every shipped config that describes the behaviour, in the same change (CLAUDE.md hard rule 1).
- A recipe carries no rationale comment (hard rule 6, Q10). A stale "X is not a port" comment is deleted, never annotated.
- No source edit uses a stream editor (hard rule 9). Where no flag exists, ship a real `.patch` beside the recipe.
- A recipe that names a new source hash is not done until `ports/publish <port>` has run and its `ports/sources.idx` line is committed.
- After each `depends` or phase move, run `python3 testing/phaseclosure.py`.
- An item marked **(Qn)** follows that closed decision.

**How to read it:**
- Recipe paths are relative to `ports/core/`.
- Log names are the filtered logs, `<phase>__<NNNN>_<port>.install.log`.
- Severity:
  - **P0**: the package is wrong or empty, or a core tool is unusable for users.
  - **P1**: user-visible breakage, a security fix, or a large image cost.
  - **P2**: a lost feature, a policy breach, or a dependency hazard.
  - **P3**: hygiene.
- Classes: `dwarf` (debug info shipped), `layout` (packaging layout), `feat` (feature disabled), `dep` (dependency ordering), `newport` (missing dependency, new port), `hidden` (hidden error), `tool` (toolchain), `musl`, `net` (network access), `hyg` (warning hygiene), `security`, `defaults` (shipped configuration), `hardening`, `linkage` (runtime resolution), `version` (upstream security release), `kernel` (kernel and boot path), `size`, `ux`, `metadata`, `other`.

---

## Summary

### Progress

560 items are `[x]` and 8 are `[ ]`, all of them S18 gates. The Stage 0 gate carries **status:** and waits on `ports/publish` and the commit of the init.d modes; the other seven run on the `--fresh` rebuilt tree and the rig.

These ports need `ports/publish`, with the `sources.idx` lines committed alongside the recipes: fortify-headers, linux, grep, which, musl-utils, sysvinit, cpio, iputils, tinyxxd, skalibs, utmps, execline, s6, openssl, openssl3, qt5-qtbase, curl, polkit, openssh, firefox-esr, thunderbird, librewolf, chromium, xwayland, python3, python3-tkinter, expat, libarchive, libpng, pcre2, cups, pam, openvpn, gstreamer, gst-plugins-base, gst-plugins-good, gst-plugins-bad, gst-plugins-ugly, gst-libav, libsoup3 (3.8.0), imagemagick, libheif (1.23.6), harfbuzz, qt6-qt3d, qt6-qt5compat, qt6-qtbase, qt6-qtcharts, qt6-qtconnectivity, qt6-qtdeclarative, qt6-qtgraphs, qt6-qtimageformats, qt6-qtlanguageserver, qt6-qtlocation, qt6-qtmqtt, qt6-qtmultimedia, qt6-qtnetworkauth, qt6-qtpositioning, qt6-qtquick3d, qt6-qtremoteobjects, qt6-qtscxml, qt6-qtsensors, qt6-qtserialport, qt6-qtshadertools, qt6-qtspeech, qt6-qtsvg, qt6-qttools, qt6-qttranslations, qt6-qtwayland, qt6-qtwebchannel, qt6-qtwebsockets, graphite2, jasper, libspng, webp-pixbuf-loader (webp-pixbuf-loader-0.2.7.tar.gz, 61ce5e8e…411a), netpbm, fig2dev, gts, libspiro, opencl-clhpp, meshoptimizer, oniguruma, tinyxml2, yt-dlp, perl-db-file, perl-mime-base32, python3-ruamel-yaml, python3-hatch-tryton, python3-networkx, sfsexp, snappy, gputils, spacenavd, xauth, xmlrpc-c, libusb-compat, libpoly, cudd, ubdsrv, switcheroo-control, libbfio, libvhdi, libvmdk, libvslvm, afflib, convertlit, antlr2, opensp, libofx, libmaxminddb, libsmi, catdoc, libmusicbrainz5, chemical-mime-data, cppcheck, castxml, python3-pygccxml, python3-pyqtgraph, python3-colorama, libdbusmenu-lxqt, kdegraphics-mobipocket, gpsman, ispc, oidn, embree, libsrt, librist, libdovi (libdovi-3.4.0.tar.gz and libdovi-vendor-3.4.0.tar.xz), go, muon, mpdecimal, pkgconf, khal, khard, rizin (second source sigdb-4addbed50cd3b50eeef5a41d72533d079ebbfbf8.tar.gz, 95e7e0b3…22b1), avogadrolibs, qca-qt5, deluge, hydrogen, tevent, libdbusmenu-qt5, sonnet-qt5, mumble (1.5.939), tryton (vendor bundle 417d80c0…a941).

Every `[x]` item is a recipe, code or documentation change. Its verification waits for the Q9 `--fresh` world rebuild and the rig, and each "Verify" line still applies.

| Phase | P0 | P1 | P2 | P3 | Total | By class |
|---|---|---|---|---|---|---|
| systemic | 1 | 11 | 72 | 3 | 87 | linkage 32, version 24, hardening 10, layout 7, other 2, security 2, size 2, defaults 1, dep 1, dwarf 1, hidden 1, hyg 1, metadata 1, tool 1, ux 1 |
| 00_cross | 0 | 0 | 0 | 0 | 0 |  |
| 10_bootstrap | 0 | 2 | 5 | 6 | 13 | defaults 5, hardening 2, security 2, ux 2, hyg 1, layout 1 |
| 20_selfhost | 0 | 1 | 3 | 5 | 9 | hidden 2, hyg 2, layout 2, dwarf 1, feat 1, hardening 1 |
| 30_foundation | 0 | 2 | 9 | 19 | 30 | layout 7, hyg 6, dwarf 5, dep 2, hardening 2, linkage 2, other 2, feat 1, net 1, newport 1, tool 1 |
| 31_compilers | 0 | 1 | 3 | 6 | 10 | dwarf 2, hardening 2, layout 2, defaults 1, dep 1, metadata 1, size 1 |
| 40_lang | 0 | 0 | 5 | 8 | 13 | dwarf 5, layout 3, hyg 2, dep 1, newport 1, other 1 |
| 41_system | 1 | 2 | 45 | 102 | 150 | feat 29, layout 27, hyg 21, dwarf 11, newport 11, hidden 9, tool 9, dep 8, hardening 5, musl 5, security 4, defaults 2, metadata 2, other 2, size 2, linkage 1, net 1, ux 1 |
| 42_graphics | 0 | 1 | 12 | 20 | 33 | hyg 8, layout 8, feat 5, dep 3, dwarf 3, hardening 1, linkage 1, newport 1, other 1, size 1, ux 1 |
| 43_toolkits | 0 | 3 | 11 | 32 | 46 | feat 11, newport 8, hyg 6, layout 5, dep 3, linkage 2, other 2, security 2, defaults 1, dwarf 1, metadata 1, musl 1, net 1, size 1, ux 1 |
| 44_apps | 0 | 5 | 27 | 40 | 72 | layout 15, feat 9, newport 9, dep 8, tool 8, other 5, linkage 3, security 3, hidden 2, hyg 2, musl 2, ux 2, dwarf 1, hardening 1, metadata 1, size 1 |
| 50_desktop | 0 | 0 | 0 | 6 | 6 | ux 5, hyg 1 |
| 60_kernel | 0 | 1 | 2 | 1 | 4 | kernel 3, feat 1 |
| 70_image | 0 | 2 | 4 | 3 | 9 | kernel 2, security 2, defaults 1, other 1, size 1, ux 1, version 1 |
| **Total** | **2** | **31** | **198** | **251** | **482** | layout 77, feat 57, hyg 50, linkage 41, newport 31, dwarf 30, dep 27, version 25, hardening 24, tool 19, other 16, security 15, hidden 14, ux 14, defaults 11, size 9, musl 8, metadata 6, kernel 5, net 3 |

Every `- [ ]` line is one item; an item that carries a table counts its rows instead. Method: `audit2/count.final.py` reads each `- [ ] **` line under Systemic fixes and the phase headings for its severity and class, and counts its `| [ ] |` rows in its place; the decisions, the rebuild procedure and the steps inside the S18 section are not counted.

### Top 10

1. **P0 S1** (DEF-01, PLAN-HEAD-01): `/etc/gitconfig` ships 0600, so every `git` command run by a non-root user dies with `fatal: unable to access '/etc/gitconfig'`. `/usr/bin/croc` is 0600 and cannot run. 460 files are unreadable to users, including thunderbird and kodi defaults, two `profile.d` scripts and `bzip2.pc` (129 pkg-config modules fail): 407 are neverball's `mapc` output, the rest come from 24 recipes whose toybox `install` wrote 0600; S18 removes that cause, neverball gets a `chmod`, and a kpkg lint keeps both out.
2. **P0 `gpsd`**: scons ignores the `DESTDIR` argument. The package holds 2 files; the daemon, library and Python module sit unowned in the root.
3. **P1 S23** (VER-01…05, VER-24): shipped security releases are behind upstream: OpenSSL 4.0.2/3.5.8 (High CVE-2026-84782), curl 8.22.0 (High HTTP/2 use-after-free), chromium 154.0.8037.57, firefox-esr/thunderbird 153.3, librewolf 156, polkit 127 with its setuid helper, and Qt 6.11.2 with six unapplied CVE diffs.
4. **P1 S18**: toybox's `awk` gives the kernel the ORC hash of empty input, its `install` writes 0600, and PID 1, getty, awk, expr, tr and stty come from its unfinished `toys/pending`. GNU at every name, sysvinit as PID 1, agetty on every console.
5. **P1 `090_initramfs.sh`** (IMG-02, KRN-02): no `vmd`, MMC/SDHCI, Hyper-V, Xen or VMware storage module in the initramfs. kinstall installs to those disks; the installed root then fails its first boot.
6. **P1 `090_initramfs.sh`** (IMG-01, LOG-01): AMD CPUs get no early microcode; the glob wants `.bin.zst` and linux-firmware ships `.bin`.
7. **P1 `fs/etc/init.d`** (DEF-02): `86_kiwix-serve`, `87_kolibri`, `88_llama-server` and `killpower` are mode 0644, so the three servers never start and a NUT low-battery shutdown never cuts UPS power.
8. **P1 `70_sshd.sh`** (DEF-07): sshd starts at every boot with password authentication while the live accounts have known passwords; only the firewall's drop on port 22 stands in front of it.
9. **P1 `linux`**: `--module` on the bool `CONFIG_MEDIA_CAMERA_SUPPORT` drops uvcvideo, gspca and every camera sensor driver.
10. **P1 `go`** (DEF-03): `$GOROOT/go.env` is not installed; `GOPROXY` is empty and every module download fails.

Next in line: `rtl-433` (RF stack overflow), S2 (3.1 GB of DWARF), S16 (OpenSSL 3 and 4 in one process), `openttd` (weak network keys), S7 (`libsqlite3` without SONAME), Qt 5 with no 2026 CVE fixes (Q26), CRT-01 (firmware licence texts missing), S26 (no ports tree on the image, so no update path).

### Decisions

All closed. Items follow them.

| Q | Topic | Decision |
|---|---|---|
| Q1 | DWARF (S2b) | Per-recipe fixes plus a failing `testing/debuginfo.sh` gate. kpkg strips nothing. |
| Q2 | `.symtab` (S14) | Strip `.symtab`/`.strtab` from objects over 32 MB only; everything else keeps its symbols. |
| Q3 | lib64 (S3) | gcc `t-linux64` patch, `/usr/lib64` → `lib` symlink, cmake GNUInstallDirs patch. |
| Q4 | userland (S18) | Toybox is removed. GNU at every name from `10_bootstrap` on: coreutils in full (separate programs), grep, sed, gawk, findutils, diffutils, patch, tar. PID 1 is sysvinit 3.18 (`init`, `telinit`, `halt`, `poweroff`, `reboot`, `shutdown`, `runlevel`, `killall5`); getty is util-linux `agetty`. New ports: grep, which, musl-utils, sysvinit, cpio, iputils, tinyxxd. |
| Q5 | writes outside `$PKG` (S4) | `70_image` unowned-file report. No read-only root. |
| Q6 | optional-dep tracking (S9) | Preflight heuristic with an alias table. |
| Q7 | OpenSSL split (S16) | A port that stays on Qt 5 builds against openssl3, as qt5-qtbase does: qca-qt5, and any Qt 5 consumer that links OpenSSL. mumble and quassel move to Qt 6 (Q26) and link OpenSSL 4 throughout. |
| Q8 | size policy (S14) | Static `.a`: ship all. Docs: prune HTML and PDF, keep man and info. NLS: English only — no message catalogs ship. |
| Q9 | rebuild batching | S2a, S3, S8, S18, perl threads (Q12), fortify (Q34), Go PIE (Q33), Q42 and Q43 land in one `--fresh` world rebuild from an empty tree (procedure below). S9's check and the S17 sweep land before it (Q20). |
| Q10 | comment rule (S17) | CLAUDE.md rule 6 covers `build.sh` as well as `kpkgbuild`; `writing-ports.md:1101` is rewritten to match and the `build.sh` files are swept. |
| Q11 | "new port or known-gaps" alternatives | Always a new port. A known-gaps entry only where no port can fix it (gtk4 F16C/ifunc). Its reach is Q40. |
| Q12 | perl | `-Dusethreads`, and full reproducibility through `config.over` (Configure sources it after setting `cf_time` and `myuname`, so no patch is needed). |
| Q13 | pkgconf | Add the `muon` port now, ordered before pkgconf, and move pkgconf to 3.1 built with muon. |
| Q14 | curl ↔ brotli cycle | cmake builds with its bundled curl (`--no-system-curl`) and drops curl from its depends. |
| Q15 | libstdc++ for picolibc | Ship the `HAVE_*L` patch; the C++ module files stay. Every picolibc libstdc++ port carries it. |
| Q16 | image scope (S19) | Every package stays in `system.sfs`. Only dead weight goes: DWARF, texlive-doc, firmware no x86 kernel loads, test suites (Q23). The 32 GB stick, the ~56 GB disk, BD-only optical boot and the multi-part release are documented. |
| Q17 | update path (S26) | Every image ships the recipe-only ports tree; updates build from source, fetching sources over the network. No binhost. |
| Q18 | licences, source offer (S26) | kpkg copies `LICENSE*`/`COPYING*`/`NOTICE*` into `/usr/share/licenses/<name>/`; an SPDX `license =` key is required in every `kpkgbuild`; 70_image writes `/usr/share/kdos/SOURCES` (commit and `sources-NNN`). |
| Q19 | Q4 scope | Superseded by S18 (GNU in full). |
| Q20 | S17 timing | S9's check and the S17 sweep land before the world rebuild. |
| Q21 | Q8 scope for `.a` | Every `.a` ships, the unlinkable ones included; the plan removes none. |
| Q22 | `libpython3.14.a` | `libpython3.14.a` ships. |
| Q23 | Python test suites | Prune `tests` in pandas, sympy, scipy, astropy (nested only), numpy and matplotlib. |
| Q24 | unversioned `python` | python3 ships `/usr/bin/python` → `python3`; git gets `PYTHON_PATH`; the S21 gate warns on a bare `python` shebang. |
| Q25 | Qt 6 security | The whole qt6 set moves to 6.12.0, plus the qt5compat CVE diffs. |
| Q26 | Qt 5 policy | Keep Qt 5. keepassxc, mumble, quassel, vlc, smplayer and every other consumer with a Qt 6 build move to it; the rest are documented. |
| Q27 | engine cadence | A standing security pass before `70_image`: advisories for the web engines, OpenSSL, curl, Qt, WebKitGTK and the kernel. |
| Q28 | OpenSSL 4 via gnutls (S16) | gnutls `--with-tpm2=dlopen`, with the SONAME probe cache variable and a check. |
| Q29 | sshd default | sshd is off until enabled, by the firewall ssh toggle or the installer. |
| Q30 | ttyS0 | The installer rewrites ttyS0 to an agetty login on installed systems; the live ISO keeps its passwordless shell (S18). |
| Q31 | resolv.conf | `nameserver 127.0.0.1`. |
| Q32 | `go.env` | Upstream `go.env` with `GOTOOLCHAIN=local`. |
| Q33 | Go PIE | `-buildmode=pie` for every Go port. |
| Q34 | fortification | New fortify-headers port; `-D_FORTIFY_SOURCE=2` and `-D_GLIBCXX_ASSERTIONS` in the world rebuild. |
| Q35 | CET | No CET; recorded as ruled out in `decisions.md`. |
| Q36 | module signing | A persistent module-signing key in `build/keys/`, never in git; `MODULE_SIG_FORCE` stays off. |
| Q37 | kernel hardening | `X86_X32_ABI=n`, `UEVENT_HELPER=n`, `PM_AUTOSLEEP=n`, `PROC_MEM_FORCE_PTRACE=y`, `LDISC_AUTOLOAD=n`, `BUG_ON_DATA_CORRUPTION=y`. |
| Q38 | JACK | pipewire-jack is built; JACK is on in ardour, hydrogen, lmms and every consumer that auto-detects it. |
| Q39 | utmp | skalibs, execline, s6 and utmps ports; `utmpd` and `wtmpd` supervised; kdos-getty/login, procps-ng and util-linux built against utmps. |
| Q40 | Q11 reach | S9's preflight report runs over all 67 recipes. New ports only for user-visible gains: OIDN with ispc, Embree, libsrt, librist, meshoptimizer. The rest go to known-gaps with reasons. |
| Q41 | libsoup3 NTLM | NTLM is enabled: `-Dntlm=enabled`, the `ntlm_auth` path, samba in depends. |
| Q42 | stale `config.guess` | kpkg refreshes stale `config.guess`/`config.sub` before `build.sh`. |
| Q43 | Rust `-sys` crates | Global system-library exports in `chroot.env` from 40_lang on. |
| Q44 | AWS-LC | native-tls where the crate offers it; otherwise an inventory row `kdos cve` reads. |
| Q45 | Start menu noise | NoDisplay on non-launchables (servers, session pickers, launch pads, diagnostics, tray helpers); a `writing-ports.md` rule and a preflight allowlist. |
| Q46 | role names | A plain name belongs to what the desktop launches: Terminal is foot (kdos-term is renamed); Files is the Start row (mc becomes "Files (mc)"); Mail is aerc, with `mailto:` on Thunderbird documented. |
| Q47 | kdos man pages | Both: a generated man page for every `kdos-*` command, and the book on the image (`/usr/share/doc/kdos`, `kdos(7)`). The book is an exception to the Q8/Q53 HTML prune. |
| Q48 | port descriptions | The recipe is the source of truth, carrying the catalogue's wording; docscheck or preflight fails on a mismatch. |
| Q49 | book counts | Exact counts only on the authority pages (`status.md`, `ports-catalogue.md`, `packaging.md`), recomputed by docscheck; elsewhere "about 2,000" or a link. |
| Q50 | unlisted recipes | List all five (helix, musl-locales, setconf, icon-naming-utils, perl-xml-simple) in their phases. |
| Q51 | consumerless libraries | All 15 stay as the application-developer platform, documented in the catalogue; qca-qt5 joins them (16) once quassel moves to Qt 6 (Q26). |
| Q52 | squashfs block size | `-b 1M`. |
| Q53 | HTML-only manuals | A Q8 exception for a program whose only manual is HTML (mednafen), and for the KDOS book. |

### Rebuild procedure (Q9)

`make build BUILD_ARGS=--fresh` on a finished tree rebuilds only ports whose recipe hash changed (`KPKG_STRICT_RECIPE=1`, `developing.md:602-611`), so consumers whose recipes are unchanged — the S3 lib64-RPATH set, the S7 sqlite consumers, perl XS ports, libgcc DWARF consumers — would be skipped, and `fs/usr/lib64` cannot replace the real `build/fs/usr/lib64` (112 entries) in place (PLAN-HEAD-10).

1. Land S9's check and the S17 sweep first (Q20). Then every recipe and kpkg edit of the batch: S1's lint, S2a, S3, S7, S8, S14 packaging, S18, perl (Q12), Go PIE (Q33), fortify (Q34), the `config.guess` refresh (Q42), the Rust `-sys` exports (Q43), the S23 bumps. Every other recipe item landed by then rides the same build, because an empty tree builds every port.
2. Run S18's 10/20 canary first. Then, from a container, empty `build/{fs,mark,cross}`, `build/iso_root`, the initramfs trees, `build/logs` and the snapshots. Remove the stray `build/fs/tmp/claude-1000` tree.
3. `make build BUILD_ARGS=--fresh`. Budget about 20 h and the snapshot disk (196 GB free today).
4. The separate "rebuild N consumers" and "reinstall" steps in S3, S5, S7, Q12 and S16 are covered by this build and are not repeated.

---

## Systemic fixes

Each of these cuts across phases. The per-phase items point here and do not repeat them.

- [x] **S1 · P0 · defaults: shipped files are unreadable or unexecutable by users.** (DEF-01, PLAN-HEAD-01, PLAN-HEAD-02, DEF-12, LNK-16)
  - Effect: `/etc/gitconfig` (git-lfs) is 0600, and git stops on an unreadable system config, so every git command a non-root user runs is fatal. `/usr/bin/croc` is 0600. thunderbird `policies.json` and `kdos-prefs.js`, kodi `appliance.xml`, jupyter `kdos-offline.json` and `syncthing-offline.xml` are ignored. `fs/etc/profile:53` skips the unreadable `50-ssh-agent.sh` and `50-sfeed.sh`. `bzip2.pc` makes 129 pkg-config modules fail for a non-root user; `glm.pc`, `pdfium.pc`, `libpdfium.pc` are unreadable. `/etc/zsh/zshrc`, `/etc/nanorc` and neverball's levels are unreadable.
  - Count: `find build/fs/{etc,usr} -type f ! -perm -o+r ! -perm -4000 ! -perm -2000` gives 460.
  - Cause: toybox `install` ignores `-m` when the source is `/dev/stdin` (reproduced with the tree's toybox: `-Dm644` and `-Dm755` both give 0600). 24 recipes: git-lfs geoclue zsh radicale openssh caddy kodi thunderbird sfeed hplip syncthing croc glm bzip2 krb5 gnuhealth celestia pdfium jupyterlab nano tealdeer frotz clamav kwordquiz. neverball's `mapc` writes 407 `.sol` files at 0600.
  - Fix:
    - The cause goes with S18: GNU `install` writes the requested mode (an S18 gate), so the world rebuild repackages the 24 recipes correctly with no recipe rewrite.
    - neverball: `chmod -R a+rX` after the copy.
    - Add the constraint to `build-troubleshooting.md`.
    - kpkg `build.c` packaging lint: fail a regular file without `o+r` that is not setuid or setgid, and a file under `usr/bin` or `usr/sbin` without an `x` bit. Allowlist: `etc/shadow*`, `etc/gshadow*`, `etc/sudoers`, `etc/sudoers.dist`, `etc/sudoers.d/*`, `etc/ppp/*-secrets`, `etc/cups/*.conf*`, `usr/lib/cups/backend/*`, `etc/freeipmi/*.conf`, `etc/fwupd/fwupd.conf`, `etc/mumble/mumble-server.ini`, `etc/skel/.msmtprc`, `etc/skel/.mbsyncrc`.
  - Verify: the find above lists only the allowlist; `su - kdos -c 'git -C /tmp init -q x && git -C /tmp/x status'` succeeds; `test -x /usr/bin/croc`; `pkgconf --cflags --libs` succeeds for every installed module as uid 1000.
- [x] **S2 · P1 · dwarf: 1,493 ELF files carry 3,142 MB of DWARF.** (PLAN-HEAD-12, IMG-11, PLAN-EARLY-24)
  - (a) libgcc builds with `LIBGCC2_DEBUG_CFLAGS=-g`. See the 20_selfhost `gcc` item.
  - (b) Per-port overrides are missing. Ports with no per-phase item need the same `-g0` or `-Wl,--strip-debug` treatment:
    - `eda/nextpnr` (944 MiB: ice40 338, machxo2 253, ecp5 233, generic 120)
    - `transfer/filezilla` (86 MB)
    - `graphics/darktable` (69 MB)
    - `mobile/android-tools` (64 MB)
    - `qt5/qt5-qtbase` (qmake, 18 MB)
    - `games-shooter/crispy-doom` and `chocolate-doom` (10 and 9 MB)
    - `gis/opencpn` (5 MB)
    - `browsers/netsurf` (4.6 MB)
    - `gis/viking` (3.9 MB, `-g -O2` in its log)
    - `eda/xnec2c` (1.1 MB)
    - `education/kalzium` (1.6 MB from OCaml objects): rebuilds after the 40_lang `ocaml` fix.
  - Gate (Q1): `testing/debuginfo.sh` exists (`make debuginfo`) but counts only `.debug_info`, runs `head|od|tr` per file and did not finish in 590 s on `build/fs`, and its owner join does not canonicalise `./lib/`. Rework it: select ELF files in one pass, sum every `.debug_*`/`.zdebug*` section, fail above a byte threshold, canonicalise ownership as `kp_canon_path` does, and allowlist ghc's `libHSrts*_debug-ghc*.so` (the `ghc -debug` runtimes, DWARF by design) with the reason in its header. Rewrite `testing.md:1536-1542` and the `developing.md:194` row to describe the reworked gate and its allowlist.
  - Verify: `make debuginfo` passes; the DWARF total outside the allowlist is under 1 MB.
- [x] **S3 · P1 · layout: the gcc runtime lives in a real `/usr/lib64`.** (PLAN-HEAD-10, PLAN-EARLY-18, PLAN-44-TAIL-03)
  - Effects: 318 ELF files carry `RPATH /usr/lib/../lib64`. meson and CMake default to lib64. avidemux, gcc, opensubdiv and xfsprogs record `./usr/lib64` paths.
  - Fix (Q3):
    - Ship `toolchain/gcc/t-linux64-lib.patch` setting `m64=../lib`.
    - Drop the compat symlinks and the `auto-load/usr/lib64` move.
    - Make `fs/usr/lib64` a symlink to `lib`. This only takes effect on an empty tree (rebuild procedure).
    - Remove the `.la` files in `10_bootstrap/{050_xz,090_binutils,100_gcc}.sh` (see 10_bootstrap).
    - Make CMake default to `lib`.
    - kpkg packaging rejects a `./usr/lib64` or `./lib64` payload path: `kp_canon_load` folds only top-level `usr/<same name>` aliases, so `usr/lib64 -> lib` would make `./usr/lib64/x` and `./usr/lib/x` two ownership keys. Document it in `packaging.md`.
  - Covers: qqwing, libfilezilla, love, vlc, libbs2b, portaudio, exempi, libetpan, graphicsmagick, libqalculate, avidemux (its `admInstallDir.cmake` picks `lib` once gcc's implicit link dirs drop `/lib64`), opensubdiv, xfsprogs, goldendict.
  - Verify: `gcc -print-multi-os-directory` prints `../lib`; `find build/fs -name '*.la'` finds nothing; no object has a lib64 RPATH; `grep -l '^\./usr/lib64/' build/fs/var/lib/kpkg/db/*` is empty.
- [x] **S4 · P2 · layout: unowned files ship.** (PLAN-HEAD-07, PLAN-HEAD-08, PLAN-44-TAIL-09)
  - Bootstrap cross toolchain: see 20_selfhost.
  - gpsd: see 41_system, P0.
  - Build-only pip installs into `/`: hatch-fancy-pypi-readme, expandvars and rjsmin, from attrs, vdirsyncer, gnuhealth and deluge. Install them with `pip --target $SRC_ROOT/buildtools` plus `PYTHONPATH`, the `python3-jsonschema` shape. Rewrite `writing-ports.md` ~l.1607. hatch-tryton becomes a port (44_apps `tryton`).
  - Live-root writes: lmms bash completion, hplip `ipp-usb/quirks/HPLIP.conf`, opencpn `98-sglock-ocpn.rules` (44_apps items).
  - Gate (Q5): `70_image` reports unowned files. It canonicalises through the root's aliases as `kp_canon_path` does; subtracts the `fs/` manifest, `home/kdos`, `etc/skel`, `var/lib/kpkg/db` and the generated files (mime, `hwdb.bin`, info `dir`, `mandoc.db`, `gschemas.compiled`, the initramfs, checked-hash `__pycache__`); and runs before `020_cleanup` or exempts `__pycache__`. Build-time bytecode never ships (`020_cleanup.sh` deletes and recompiles it), so no `PYTHONDONTWRITEBYTECODE` change is needed.
  - Verify: the gate reports only the UAPI headers.
- [x] **S5 · P2 · layout: kpkg misses takeovers it should refuse.** (PLAN-HEAD-04, PLAN-HEAD-05, PLAN-HEAD-09, PLAN-EARLY-14, PLAN-44-TAIL-21)
  - kpkg already canonicalises `/bin`, `/sbin`, `/lib` aliases (`kp_canon_path`). The gaps:
    - `check_conflict` (`add.c`) asks ownership only for staged paths already present on disk (`collect_present`), so a file deleted from the live root first is taken silently: python3-pyxdg's and python3-pysocks's pip steps ran `Uninstalling pyxdg`/`PySocks` against `/`, and khal+pyxdg (34 paths) and toot+pysocks (11) are now owned twice.
    - The scan is skipped on upgrade (`add.c:506`, `if (!upgrade && !force)`), so an upgrade that overwrites another package's path leaves no record, and the Verify grep for `Taking` cannot see it.
    - The manifest is `tar -xpvf` stdout verbatim, in GNU tar's escape quoting, so aisleriot's and openocd's non-ASCII paths are never owned or removed.
  - Fix:
    - Add a `replaces =` key; kpkg refuses any undeclared takeover. `check_conflict` asks db ownership for every staged non-directory path, on install and upgrade. Document `replaces =` in `writing-ports.md` (keys) and the refusal in `packaging.md`.
    - pip build steps never uninstall from the live root (`--ignore-installed`).
    - Pass `--quoting-style=literal` to the extraction tar and any `tar -t` listing; reinstall aisleriot and openocd; describe the manifest encoding in `packaging.md`.
    - Fix the takers (per-phase items): ppsspp→freetype2, qmapshack→blend2d, sdcc→binutils, deluge→python3-distro, utfcpp↔taglib, nfs-utils→rpcsvc-proto (`rpcgen`), tcl→perl (`Thread.3`).
    - Reinstall freetype2 and blend2d.
  - Verify: a normalised scan of every db finds no path owned twice; `grep -l Taking build/logs/*/*` shows only declared takeovers.
- **S6** is closed by S18. Its two Verify lines (`readlink build/fs/usr/bin/awk` resolves to gawk; no `awk: bad regex` line in the 60_kernel log) are S18 gates.
- [x] **S7 · P1 · layout: `libsqlite3` has no SONAME.** 91 objects in 56 packages link it: 45 NEED `libsqlite3.so`, 46 NEED `/usr/lib/libsqlite3.so` (`audit2/`). See the 30_foundation `sqlite` item; the consumers rebuild in the world rebuild. (PLAN-HEAD-13, PLAN-EARLY-17)
  - Verify: no ELF has an unversioned or path sqlite NEEDED.
- [x] **S8 · P2 · hidden: binutils and gcc man pages are built before perl exists.** See the 20_selfhost items.
- [x] **S9 · P2 · dep: features are off because of false "X is not a port" claims.** (PLAN-HEAD-15, PLAN-41B-08, PLAN-42-43-15, MET-10)
  - Fix (Q6): a preflight check reads `--without-X`, `--disable-X`, `-D…=OFF` and `-D…=disabled`, maps X to a port through an alias table (libnuma→numactl, PyGObject→python3-gobject, PySide6→python3-pyside6, libstemmer→snowball, nanobind→python3-nanobind, XScreenSaver→libXScrnSaver, tdb→tdb) and reports matches. Its report also drives Q40.
  - Per-port items are marked **[S9]**. A row that only carries a stale comment deletes it under S17; it is never corrected in place.
  - Extra stale claims, not otherwise listed:
    - `sci-libs/netcdf-c` (c-blosc)
    - `3d/opensubdiv` (glew)
    - `education/libkiwix` (gtest; `devlibs/gtest` exists)
    - `3d/openvdb` (jemalloc; nanobind for the Python module, `-DOPENVDB_BUILD_PYTHON_MODULE=OFF`)
    - `lang/tk` (`--disable-xss`; libXScrnSaver is in 42_graphics)
    - `xdg/libcanberra` (`--disable-tdb`; tdb is in 41_system)
    - `browsers/chromium` (openh264: add it to the `_system` unbundle list and depends; crc32c is genuinely not a port)
    - `servers/samba` (talloc and tdb are ports; see S11)
  - Q40: the report runs over all 67 recipes that still claim a missing port. User-visible gains become new ports, marked Q40 in the New ports table: OIDN with ispc, Embree and meshoptimizer (blender, qgis), libsrt and librist (obs-studio, vlc, gst-plugins-bad). Every other claim (USD, MaterialX, OSL, PETSc, …) gets a `known-gaps.md` entry naming what is missing and why it is not ported; its recipe comment goes under S17.
- [x] **S10 · P2 · layout: Python packaging leaks build state, and most application bytecode is lost.** (PLAN-HEAD-18, PLAN-42-43-08, CRT-04)
  - 262 `direct_url.json` files name the kpkg work directory (204 in site-packages, 58 in application trees: gnuhealth 50, calibre 3, sideband 2, radicale 1, spyder 1, python3-sphinx 1).
  - `usr/bin/__pycache__` never reaches the image (`020_cleanup.sh` deletes every `__pycache__`), but db/ocrmypdf, printrun and python3-esptool still list it.
  - `020_cleanup.sh:107-114` deletes every `.pyc` in the tree and recompiles only `/usr/lib/python3*`. Outside it, 12,077 `.py` files ship with 543 `.pyc` (all python3-sphinx): calibre 2,589/0, freecad 1,469/0, gnuhealth 1,392/0, spyder 1,164/0, blender 810/0, qgis, sideband, inkscape, cura, libsigrokdecode, krita, radicale, virt-manager, hplip, lyx. Every start of those programs compiles everything it imports. 8,433 db paths across 25 packages no longer exist.
  - Fix:
    - kpkg packaging deletes `*.dist-info/direct_url.json` and `usr/bin/__pycache__`.
    - kpkg packaging compiles checked-hash bytecode into `$PKG` for every Python tree the package ships, so the db owns what ships; `020_cleanup` becomes a check that every shipped `.py` outside an allowlist (`*.tmpl.py`) has a `.pyc`, and prints the failing paths.
    - Update `how-kdos-is-built.md:550` and `build-system.md:713` to describe the scope.
  - Verify: `find build/fs/usr/lib/calibre -name '*.pyc' | wc -l` is non-zero; the missing-db-path list holds no `.pyc`.
- [x] **S11 · P2 · layout: `.pc` and CMake files carry bad data.** (PLAN-HEAD-14, PLAN-EARLY-23, LNK-11)
  - Tree LDFLAGS in `Libs:`: ncurses (30_foundation); check `net-diag/net-snmp`, `network/ldns`, `lang/R`, `image-libs/graphicsmagick`, `bioscience/htslib`, `lang/ruby` (`ruby-4.0.pc` `DLDFLAGS`).
  - `talloc.pc` `Libs: -Wl,-rpath,/usr/lib`: `--disable-rpath`.
  - Empty `Version:` in `wl/waylandpp`, `sdr-hw/hackrf`, `virt/remmina`, libfobos and libmirisdr (41_system).
  - `gnu-efi.pc` names `/usr/local`: see 41_system.
  - CMake configs embed the build tree: boost (`/var/cache/kpkg/work/boost/pkg`), netgen (`NetgenConfig.cmake`), qt6-qtbase (`QtBuildInternalsExtra.cmake`). dcmtk, netCDF and opencascade carry `-ffile-prefix-map=/var/cache/kpkg/work=/build`, which the lint allows.
  - Modules that do not resolve: samba `ndr*`/`samba-util` (`Requires: tevent`, bundled; delete the false "none is a port" comment for talloc and tdb (S17)), `qrupdate` (`Requires: blas, lapack`; openblas ships only `openblas.pc`), `wimlib` (`fuse`, while it links fuse3), `libgpod-sharp` (removed by the libgpod item).
  - Bad directories: `pyside6.pc` `includedir=/usr/PySide6/include`, `gig.pc` `libdir=/usr/lib/libgig`, `KF6PulseAudioQt.pc` `-I/KF6PulseAudioQt`.
  - libpsx, gmodule and luajit carry upstream `-Wl,` flags on purpose.
  - Lint in kpkg packaging: reject an empty `Version`, `/usr/local`, `/var/cache/kpkg` and the tree LDFLAGS markers (`--sort-common`, `-z,now`, `pack-relative-relocs`) in `Libs`; run `pkgconf --cflags --libs` on every module as a non-root user and fail on any error. Document the lint in `packaging.md` and the `.pc` rules in `writing-ports.md`.
- [x] **S12 · P2 · hardening: ELF hardening has no gate.** (PLAN-41A-04, HRD-14, LNK-13, HRD-13)
  - Exec-stack: `studio/ardour` (9 objects), `libretro/libretro-melonds`, onnxruntime (41_system), `wsjtx` `jt9`. TEXTREL: none.
  - RPATH into the build dir: tenacity (17), sleuthkit (28), yara, `accessibility/espeak-ng`.
  - Relative RPATH: `libqhull_r.so.8.0.2` has `RPATH [lib]`, searched from the working directory (41_system `qhull`).
  - 380 objects have a redundant `/usr/lib` RPATH; 318 carry `/usr/lib/../lib64` (S3).
  - 256 native objects lack BIND_NOW. Heaviest: ghc 58, koreader 45, blind 36, gcc 13, nss 11, go 11.
  - Lint: consider only `e_machine == EM_X86_64` (543 Guile `.go` files are ELF with `EM_NONE`) and skip `*/testdata/*`. Fail on exec-stack, TEXTREL, an RPATH under `/var/cache` or `/workspace`, or an RPATH entry that is neither absolute nor `$ORIGIN`-based. Warn when `-z now` is missing or a `/usr/lib` RPATH is present. Empty RPATH components (gcompris, 20 objects) are harmless on musl and only warned. Document the lint beside the hardening policy in `writing-ports.md:422-457` and in `packaging.md`.
  - Tree option: build `qt6-qtbase` with `-DQT_DISABLE_RPATH=ON` (gpxsee, python3-pyqt6, about 40 qmake binaries).
- [x] **S13 · P2 · layout: ports install outside the layout.** (LOG-06, PLAN-41B-18, PLAN-41A-14)
  - `/usr/etc` (compiled-in `sysconfdir`, so `/etc` config is never read): `editors/jupyterlab`, kbd, gnuastro, ltrace, texinfo, htop, popt, libgpg-error, vorbis-tools, lnav, bladerf. Fix with `--sysconfdir=/etc` (bladerf: its CMake sysconfdir switch).
  - `/usr/var`: libdaemon (`/usr/var/run`), xfsprogs (`usr/var/lib/xfsprogs`). Fix with `--localstatedir=/var`.
  - `/usr/doc`: `buildtools/cmake`, lasem.
  - `/usr/com`: units.
  - `/usr/lib/systemd/user`: xfconf, `files/dolphin`.
  - `/etc/grub.d`: fwupd.
  - `/etc/bash_completion.d`: perf, `security/pass-otp`, rust, `education/tuxpaint`.
  - `/usr/share/themes`: sc-im.
  - `/run` and `/var/run`: `auth/krb5`, `net-libs/avahi` (`rm -rf "$PKG/var/run"`).
  - `/usr/lib/locale` `.mo`: done; kpkg's S14 prune removes them.
  - Lint: a packaging prefix allow/deny table, with `/etc/X11`, `/usr/local`, `/run` and `/var/run` denied except kdos-*; fail on a `/usr/etc` or `/usr/var` string in a packaged ELF outside an allowlist. Export `DATADIRNAME=share` for glib-gettext ports. Optional: a tree `config.site` exported as `CONFIG_SITE`, documented in `writing-ports.md`. Document the prefix table and `DATADIRNAME` in `writing-ports.md` and the lint in `packaging.md`.
- [x] **S14 · P2 · size: size and NLS policy (Q2, Q8), written into `writing-ports.md` and enforced in kpkg packaging.** (IMG-06, IMG-11, PLAN-HEAD-18, PLAN-42-43-04, PLAN-42-43-05, PLAN-42-43-16, PLAN-44-TAIL-01)
  - Static archives (3,713 `.a`, 3,096 MB raw, about 584 MB on the medium): every one ships, the unlinkable ones and the 104 MB `libpython3.14.a` included (Q8, Q21, Q22).
  - `.symtab`/`.strtab` (2,975 MB in all, 1,564 MB in the 144 objects over 32 MB): kpkg packaging strips symbol tables from ELF objects over 32 MB. It must catch `usr/lib/chromium/chromium` (428 MB) and the `libxul.so` of librewolf, firefox-esr and thunderbird; `chrome_crashpad_handler` and the bundled `.so` files stay. Those recipes keep `--disable-strip`/`--disable-install-strip`.
  - Docs: kpkg packaging drops HTML and PDF manuals (`*.html`, `*.pdf`, `html/` trees) under `/usr/share/doc` and `/usr/share/texmf-dist/doc`; man and info ship. texlive-doc (2,877 MB, 30,813 files, all PDF/TeX manuals) leaves `42_graphics/packages.d/doctools.txt`; known-gaps says texdoc has no local manuals. Exceptions (Q47, Q53): `usr/share/doc/mednafen` (its only manual is HTML) and `usr/share/doc/kdos` (the book).
  - Locale data: English only. kpkg packaging drops `*/locale/*/LC_MESSAGES/*.mo` (covers `/usr/lib/locale`: homebank, libgpod), Qt `.qm` translations, KDE translated handbooks, gettext `usr/share/*/translations` and translated man pages (`usr/share/man/*` other than `man[0-9n]*`, about 10 MB: xz, procps-ng, psmisc, weechat, ark, nmap, kded, mc). Input-method dictionaries, fonts and `locale/locale.alias` stay. Ports gain `--disable-nls` / `-DKF_SKIP_PO_PROCESSING=ON` where the flag exists. The per-port prune loops in kpackage and kservice and qwt-qt5's `rm` go when this lands. krb5's empty `cat*` directories go.
  - Verification list: hwloc's two PDFs, qwt's 37 MB `html/`, libvirt's `html/` and 49 `.mo` (27 MB), kio/kio-extras/konversation handbooks, libtraceevent/libtracefs HTML, imagemagick's `www/` (partly; see 43_toolkits).
  - Document the policy, and that it removes every non-English UI, in `writing-ports.md`, `docs/kdos/02-user-guide` (language settings) and `known-gaps.md`.
- [x] **S15 · P3 · hyg: build-environment noise, one global fix each.** (PLAN-HEAD-18, PLAN-42-43-06, PLAN-42-43-07, PLAN-42-43-11)
  - `-Wno-dev` → `-Wno-author` in the 267 `build.sh` files.
  - KDE recipes: `-DCMAKE_INSTALL_LIBDIR=lib` → `-DKDE_INSTALL_LIBDIR=lib` (139 logs). Moot once S3 lands.
  - Unused CMake variables: a grep over every log's "Manually-specified variables were not used" block drives the sweep; the per-phase tables are its current output.
  - Locale: `export LANG=C.UTF-8` in chroot.env (230 Qt "Detected locale C" lines).
  - `export PIP_ROOT_USER_ACTION=ignore` in chroot.env (210 logs).
  - `-std=gnu11` reaches C++ in 21 logs: lib3mf, ghostscript, fldigi, handbrake, gn, flrig, ninja, mupen64plus. Move the standard into a C-only variable.
  - Export `FFLAGS` and `FCFLAGS` in chroot.env beside CFLAGS (R `Makeconf` and cfitsio record `-g`). Verify R's `FFLAGS`, `FCFLAGS`, `SAFE_FFLAGS` have no `-g`.
  - A `script/bin/meson` wrapper adds `--wrap-mode=nodownload` (204 of 240 meson recipes lack it). Covers libpeas (gi-docgen clone) and gst-plugins-base (gl-headers clone); both clones fail harmlessly today.
  - `/usr/bin/file` is missing in 19 logs: order `file` earlier.
  - `ldconfig: command not found` in 11 logs: ship a no-op `ldconfig` or ignore it.
  - kpkg extraction: pass `--warning=no-unknown-keyword` to tar (`build.c` ~l.283).
  - Update the chroot environment table in `build-system.md` (~l.471-482) for the new exports (`LANG`, `PIP_ROOT_USER_ACTION`, `FFLAGS`/`FCFLAGS`), and `writing-ports.md` plus `build-troubleshooting.md:1110` for the meson wrapper.
  - The aalib `AM_PATH_AALIB` underquoting and intltool's `Unescaped left brace`: patch in those ports.
  - Stale `config.guess` (Q42): 54 autotools ports report `x86_64-*-linux-gnu`. kpkg copies automake's `config.guess` and `config.sub` over every older copy (by its `timestamp=` line) in the unpacked source before `build.sh` runs. Document it in `packaging.md` and `writing-ports.md`. Lands before the world rebuild.
- [x] **S16 · P1 · security: OpenSSL 3 and OpenSSL 4 load into one process** (Q7, Q28). (PLAN-HEAD-16, PLAN-42-43-14, PLAN-44-TAIL-08)
  - Per-port items: `qca-qt5` (43), `mumble` (44), `quassel` (44), `gnutls` (41).
  - A port that moves to Qt 6 (Q26, 43_toolkits) links OpenSSL 4 throughout, as `qt6-qtbase` does (`depends = … openssl`), which closes its item here. mumble and quassel move to Qt 6 (Q26); their items below are that move.
  - The gnutls path: `libgnutls.so.30` → `libtss2-esys.so.0` → `libcrypto.so.4`; `libcups.so.2` NEEDs gnutls, so Qt 5's CUPS print plugin maps OpenSSL 4 into processes whose `libQt5Network.so.5` maps OpenSSL 3 (sqlitebrowser, wsjtx, qt5-qttools). Python's `_hashlib` NEEDs `libcrypto.so.4`, so a PyQt5 network program that imports hashlib does the same. musl ignores symbol versions. Static analysis; not yet seen at run time.
  - Verify: the NEEDED closure scan, extended to `qt5/plugins` and `lib-dynload`, finds no object set that maps both `libcrypto.so.3` and `libcrypto.so.4`.
- [x] **S17 · P3 · other: rule 6 covers `build.sh` (Q10, Q20).** (MET-13, PLAN-HEAD-11)
  - Rewrite `writing-ports.md:1101` to say a recipe — `kpkgbuild` and `build.sh` — carries a banner and no rationale; reasoning goes in the book or the commit.
  - Sweep the 1,376 `ports/core` `build.sh` files and the 22 `src/` ones carrying 3 or more comment lines (16,688 lines): a constraint worth keeping moves to the port's book entry; a stale "X is not a port" claim is deleted (S9).
  - The recipe hash covers `build.sh`, so each swept file rebuilds its port: land the sweep after S9's check and before the world rebuild (Q20).
  - Comment deletions folded in: `network/networkmanager` iptables (`build.sh:16`; `-Diptables` already set), and the `pim/khal` and `chat/toot` vendoring paragraphs.
- [x] **S18 · P1 · tool: toybox leaves KDOS — GNU at every name from `10_bootstrap` on, sysvinit as PID 1, `agetty` on every console.** Closes S6 and the cause of S1. The work is the S18 section below.
- [x] **S19 · P2 · size: the medium carries everything; strip dead weight and state what it needs.** (IMG-04, IMG-05, IMG-07, IMG-09, IMG-10, LNK-14, PLAN-42-43-D2)
  - Measured: `kdos.iso` 23,853,701,120 B; `system.sfs` 22,693 MiB (zstd, 128 KiB blocks); install size about 53 GB (kinstall walks `/`, `probe.c:797`, and refuses a disk smaller than that plus 768 MiB). It needs a 32 GB stick, a BD-R for optical boot and 12 release parts. Largest contributors on the medium: games shelves about 6.7 GB, texlive-doc about 2.4 GB (S14), toolchain 1.1 GB, lang 1.1 GB, eda 0.9 GB.
  - Scope (Q16): every package stays in `system.sfs`; there is no image manifest and no binhost. State the requirements — a 32 GB USB stick, about 56 GB of disk for an install, optical boot from BD-R only, a release in several parts — in `getting-started.md` (requirements), `installation.md`, `kinstall.md`, `how-kdos-is-built.md` and the release notes. Re-measure after the dead-weight items and write the measured figures.
  - Dead weight: DWARF (S2), texlive-doc (S14), firmware no x86 kernel loads and superseded firmware images (41_system `linux-firmware`), Go `src/**/testdata` (31_compilers `go`), Python test suites (Q23), initramfs trims (70_image).
  - `make run` and `make cleandisk` (`Makefile:194`, `:229`) create a 20 GB qcow2 that kinstall refuses: create it sparse above the measured install size (for example 96G) or derive it from `build/fs`; update `getting-started.md:145`, `developing.md` and the Makefile help.
  - Block size (Q52): `110_iso.sh` passes `-b 1M` to `mksquashfs` (l.81-82), ahead of `-e`, which takes every later argument as an exclude. It measures 3–8 % smaller (about 0.75–1.7 GB) and makes cold random reads from a stick slower; `how-kdos-is-built.md` names both.
  - Verify: `unsquashfs -s system.sfs` reports a 1048576-byte block size; `make run` installs to its disk.
- [x] **S20 · P2 · security: archive modes survive extraction, so world-writable files ship.** (PLAN-41B-02, PLAN-HEAD-03, DEF-15)
  - `find build/fs/{usr,etc,var} ! -type l -perm -o+w ! -perm -1000` gives 1,982: 1,981 under `usr/share/gimp/3.0/help` (gimp-help's tarball holds 6,492 `-rw-rw-rw-` and 440 `drwxrwxrwx` entries) and `var/games/tetris-bsd.scores`. 53,207 group-writable entries also ship (wesnoth, kdos icons, libretro assets, tldr, `usr/include/lmdb.h`).
  - Cause: kpkg runs `tar -xf` as root (`build.c:283-289`), which keeps archive modes; `umask(022)` (l.658) does not touch them.
  - Fix: add `--no-same-permissions` to the extraction argv, with `umask(022)` in effect when tar is spawned. Extend the S1 lint: fail a non-sticky `o+w` path; report `g+w` paths outside the deliberate setgid and score-file cases; fail `*.orig` and `*.rej` (rapidjson ships `reader.h.orig`). Allowlist `var/games/tetris-bsd.scores` at 0666 (tetris-bsd is not setgid, so a 0644 or 0664 file stops non-root players saving). Describe the extraction mode rule in `packaging.md`.
  - Verify: `find build/fs/{usr,etc} ! -type l -perm -o+w ! -perm -1000` is empty.
- [x] **S21 · P2 · linkage: nothing checks that a NEEDED library's owner is in the consumer's depends, or that runtime references resolve.** (LNK-06, PLAN-44-TAIL-07, PLAN-41A-12, MET-11, LNK-07, PLAN-41B-04, LNK-12, LOG-07)
  - Gate: a `testing/` or 70_image check fails on any DT_NEEDED whose owning package is outside the consumer's transitive `depends` (musl, gcc and linux-headers excepted), and on a shebang interpreter under `/usr` that does not resolve. The S9 heuristic only reads disable flags, so it cannot catch these. Document the gate in `testing.md`.
  - Undeclared edges (add the provider to depends; for the libintl edges prefer `--disable-nls` per Q8):

    | ☐ | Consumer | Provider | Phase note |
    |---|---|---|---|
    | [x] | shadow | libintl | same phase (30) |
    | [x] | openldap | util-linux (libuuid) | same phase (30) |
    | [x] | dhcpcd, lowdown | libmd | same phase (41) |
    | [x] | nbdkit, tdb, talloc, zeromq | libbsd | same phase (41) |
    | [x] | net-snmp | pciutils, libpcap | same phase (41) |
    | [x] | cryptsetup, gdk-pixbuf, neovim, fcitx5 | libintl | earlier phase |
    | [x] | avahi, libwebsockets | libcap | earlier phase |
    | [x] | ardour | libcap, libwebsockets | earlier phase |
    | [x] | zeromq | libunwind-nongnu | earlier phase |
    | [x] | xapian-core | icu | earlier phase |
    | [x] | libzip | musl-fts | earlier phase |
    | [x] | ocaml | zstd | earlier phase |
    | [x] | R | libtirpc | earlier phase |
    | [x] | avidemux, openscad | glu | earlier phase |
    | [x] | ppsspp | glew | earlier phase |
    | [x] | dosbox-x | duktape | earlier phase |
    | [x] | zbar | libSM, libICE | earlier phase |
    | [x] | kidletime | libXScrnSaver | earlier phase |
    | [x] | yosys | tomlplusplus | earlier phase |
    | [x] | retroarch | v4l-utils | earlier phase |
    | [x] | obs-studio | rnnoise (delete the "in-tree copy" comment, `build.sh:25-26`, S17) | earlier phase |
    | [x] | sdrangel | hamlib | earlier phase |
    | [x] | qt-creator | qt6-qtcharts | earlier phase |
    | [x] | orcaslicer | gspell | earlier phase |
    | [x] | stellarium | onetbb | earlier phase |
    | [x] | opencpn | openssl | earlier phase |
    | [x] | libksysguard | kquickcharts (QML import, 20 files) | same phase (43) |
  - Unversioned `python` (Q24): 298 executables use `#!/usr/bin/env python` or `#!/usr/bin/python` and `/usr/bin/python` does not exist (bcc 134, john 79, hplip 39, texlive, konversation 5, llvm, clang `scan-view`, git `git-p4`, libpsl, bcftools, libevent, dbus, bmake); 7 need `python2` (john, tuxpaint). Fix: the python3 port ships `python -> python3`; git also gets `PYTHON_PATH=/usr/bin/python3`; delete the `python2` scripts; update `build-troubleshooting.md` ("A build calling python"); the gate warns on a bare `python` shebang.
  - Rust `-sys` crates compile vendored C copies of ports. The switches are exported once in `chroot.env` from 40_lang on (Q43); each row is a port that must then link the system copy:

    | ☐ | Ports | Vendored | Switch |
    |---|---|---|---|
    | [x] | bat, delta, eza | libgit2 | `LIBGIT2_NO_VENDOR=1`, drop `--features vendored-libgit2`, libgit2 in depends, guard NEEDED `libgit2.so.1.9`; delete the "no libgit2 port" comments (S17) |
    | [x] | bat, delta, xh, presenterm, yazi, inlyne | oniguruma | `RUSTONIG_SYSTEM_LIBONIG=1` after the oniguruma port |
    | [x] | atuin, iamb, termusic | SQLite | `LIBSQLITE3_SYS_USE_PKG_CONFIG=1` after S7 |
    | [x] | atuin, ouch, doxx, surfer | zstd | `ZSTD_SYS_USE_PKG_CONFIG=1` |
    | [x] | caligula | liblzma | check lzma-sys's static feature |

    Each guarded by a `readelf -d` NEEDED check. The 31_compilers `cargo-c`, `rust` item moves libgit2 to 30_foundation, ahead of every row.
- [x] **S22 · P2 · hardening: the book claims hardening "stays on every port"; whole language classes and bootstrap tools are exempt.** (HRD-15, HRD-01, HRD-02, HRD-03, LOG-10, HRD-09, HRD-12)
  - `decisions.md:382-383` says PIE, SSP, stack-clash and a read-only GOT stay on every port. Measured exceptions: Go (58 of 59 executables non-PIE, Q33); GHC (non-PIE, lazy binding); Rust has no SSP by design; upstream `-fno-stack-protector` in yices2, wine, valgrind, LuaJIT/texlive, mednafen; kpkg and kinstall lack BIND_NOW. No `_FORTIFY_SOURCE` takes effect on musl without fortify-headers, though 40 logs pass it (Q34). No CET (Q35).
  - Fix:
    - Go PIE (Q33): `export GOFLAGS="-trimpath -buildvcs=false -buildmode=pie"` (`chroot.env:78`); the comment at l.71-73 states each flag's constraint.
    - Fortification (Q34): the fortify-headers port first in 20_selfhost; `-D_FORTIFY_SOURCE=2` in `CFLAGS` and `CXXFLAGS` and `-D_GLIBCXX_ASSERTIONS` in `CXXFLAGS`, in `chroot.env`.
    - CET (Q35): `decisions.md` records `-fcf-protection` as ruled out while musl enables no user shadow stacks.
    - Rewrite `decisions.md:382-383` and `writing-ports.md:422-457` to state the measured policy with the remaining exceptions by name.
  - Verify: every Go executable is ET_DYN; a fortified `memcpy` overflow test aborts under musl.
  - Per-port link-flag gaps:

    | ☐ | Port | Phase | Gap | Fix |
    |---|---|---|---|---|
    | [x] | speech-dispatcher | 41 | `sd_*` modules and `libspeechd_module.so` lack NOW (`src/modules/Makefile.am:30` `LDFLAGS =`) | pass the tree LDFLAGS to the modules' make |
    | [x] | stockfish | 41 | final link passes `ENV_LDFLAGS` only (`Makefile:1189`) | `ENV_LDFLAGS="$LDFLAGS"` on the make line |
    | [x] | ngspice | 41 | 7 `.cm` code models lack NOW | pass LDFLAGS to the code-model link |
    | [x] | john | 41 | `cprepair` lacks NOW | LDFLAGS on its link |
    | [x] | cataclysm-dda | 44 | `zzip` lacks NOW | LDFLAGS on its link |
    | [x] | linuxcnc | 44 | `panelui`, `linuxcnc_module_helper` lack NOW | LDFLAGS on their links |
    | [x] | libreoffice | 44 | 6 bundled `-lo` libraries lack NOW | pass LDFLAGS to the externals |
    | [x] | qt6-qtwebengine | 43 | `libexec/gn` lacks NOW | gn bootstrap with LDFLAGS |
    | [x] | kdos-splash, kdos-boxinit | 41 | ET_EXEC: `-static`, and kdos-splash drops `$LDFLAGS` (`src/art/kdos-splash/build.sh:19`) | `-static-pie` and `$LDFLAGS` |
- [x] **S23 · P1 · version: shipped security releases are behind upstream.** (VER-01…VER-24)
  - Bump, publish the source, and rebuild with `--continue-from` the port's phase (or fold into the world rebuild). The standing pre-image pass is the 70_image item (Q27); Qt 5 is the 43_toolkits item (Q26); AWS-LC is the 44_apps item (Q44).
  - P1:

    | ☐ | Port | From → to | Why |
    |---|---|---|---|
    | [x] | openssl, openssl3, qt5-qtbase `_openssl` | 4.0.2 → 4.0.3, 3.5.8 → 3.5.9 | 14 CVEs of 29 Sep 2026, High CVE-2026-84782 (VER-01) |
    | [x] | curl | 8.22.0 → 8.23.0 | 21 CVEs, High CVE-2026-92392 HTTP/2 upload UAF; can share the Q14 brotli rebuild (VER-02) |
    | [x] | chromium | 154.0.8037.57 → 155.0.8059.x | Stable is 155.0.8059.84; refresh the 14 patches and `_copium`; fallback 154.0.8037.97 (VER-03) |
    | [x] | firefox-esr, thunderbird, librewolf | 153.3 → 153.5; 153.3.1 → 153.5.x; 156 → the Firefox 158 release | MFSA 2026-100/103/108/110, 2026-97/104/106 (VER-04) |
    | [x] | polkit | 127 → 128 | CVE-2026-4897 and CVE-2026-85498 in the setuid agent helper; check the build stays systemd-free (VER-05) |
  - P2:

    | ☐ | Port | From → to | Why |
    |---|---|---|---|
    | [x] | sudo | 1.9.17p2 + upstream commit `3e474c2` as a `.patch` | CVE-2026-35535; CVE-2026-82474 needs ptrace intercept, which KDOS does not enable (VER-06) |
    | [x] | openssh | 10.5p1 → 10.6p1 | 2026-10-06 security fixes (VER-07) |
    | [x] | xwayland | 24.1.13 → 24.1.14 | X.Org advisory of October 2026 (VER-08) |
    | [x] | linux | 7.2.7 → 7.2.11 | stable point releases; rides the ORC-hash kernel rebuild (VER-09) |
    | [x] | python3 | 3.14.7 → 3.14.8 | expedited security release, 7 CVEs (VER-10) |
    | [x] | go | 1.27.1 → 1.27.2 | crypto/tls, net/http, html/template, go command; rebuild every Go port (VER-11) |
    | [x] | expat | 2.8.5 → 2.9.0 | CVE-2026-77214, CVE-2026-102633 (VER-12) |
    | [x] | libarchive | 3.8.9 → 3.8.10 | security release (VER-13) |
    | [x] | libpng | 1.6.58 → 1.6.59 | CVE-2026-46675 UAF in `png_read_end` (VER-14) |
    | [x] | pcre2 | 10.48 → 10.49 | JIT stack OOB write, GHSA-r9hj-j2rw-4q3m (VER-15) |
    | [x] | cups | 2.4.19 → 2.4.20 | 7 CVEs (VER-16) |
    | [x] | pam | 1.7.2 → 1.7.3 | pam_group/pam_time OOB reads, pam_faillock bypass; check `fs/etc/pam.d` against the new SHA512 default (VER-17) |
    | [x] | openvpn | 2.7.7 → 2.7.8 | CVE-2026-84790, CVE-2026-88964, CVE-2026-84256 (VER-18) |
    | [x] | gstreamer and 5 plugin sets | 1.28.7 → 1.28.8 | SA-2026-0078…0092, RTP depayloader OOB write (VER-19) |
    | [x] | libsoup3 | 3.6.6 → 3.8.0 | 9 CVEs from the 3.7/3.8 line; check new meson options (VER-20) |
    | [x] | imagemagick | 7.1.2-31 → 7.1.2-33 | CVE-2026-109259, CVE-2026-109276 (VER-21) |
    | [x] | libheif | 1.23.5 → 1.23.6 | GHSA-h4h8-qgvc-m7r2 (VER-22) |
    | [x] | harfbuzz | 14.5.0 → 14.6.0 | overflows incl. the shipped `libharfbuzz-gpu` (VER-23) |
    | [x] | the qt6 set (28 `qt6-*` ports) | 6.11.2 → 6.12.0, plus the qt5compat CVE diffs as `.patch` files | six post-release CVE diffs unapplied on 6.11.2, no 6.11.3 (VER-24, Q25) |
- [x] **S24 · P2 · ux: desktop indexes and entries are not built or checked.** (UX-07, LNK-12, UX-D1)
  - No `icon-theme.cache` exists for any theme (KDOS 23,279 files, breeze and breeze-dark 19,919 each, hicolor, Adwaita): kpkg's trigger table (`triggers.c` `t_tool[]`) has no `gtk-update-icon-cache`, so every GTK process walks the inherited themes at start. Add a `T_ICONS` trigger on `usr/share/icons/` running `gtk-update-icon-cache -qtf` per theme with an `index.theme`, with the theme's directory mtimes clamped to `SOURCE_DATE_EPOCH` first (the fontconfig constraint, 70_image). Delete the virt-manager `build.sh:15-16` comment (S17), and correct the `writing-ports.md` "Shared indexes" list.
  - A 70_image check fails on a dangling in-root symlink, a desktop `Exec=` or D-Bus `Exec=` that does not resolve, a visible desktop `Icon=` with no icon, and a udev `RUN` with no program. Its current findings are items in 43_toolkits (Qt 6 links), 41_system (byobu, ccid, hwloc) and 44_apps.
  - Start's NoDisplay rule (Q45) goes into `writing-ports.md` ("Desktop entries belong to the port"), with a preflight allowlist.
- [x] **S25 · P3 · metadata: recipe metadata and the book's figures drift from the tree.** (MET-02, MET-03, MET-04, MET-05, MET-06, MET-07, MET-12, CRT-05)
  - Descriptions: 297 recipe descriptions differ from `ports-catalogue.md`, 211 open with `<name> —`, 3 end with `.`, 4 are bare stubs. The recipe is the source of truth and takes the catalogue's wording (Q48); a docscheck or preflight check fails on any mismatch.
  - Literal versions in `source =`: rtl-sdr, cjson, lrzsz, aerc, sane-airscan, sane-backends, wcslib, admesh, mbpoll, rtl-433, rubberband, visidata (the pythonhosted `/packages/source/v/visidata/` path) use `$version`. docbook-xml stays (several DTD versions on purpose). A preflight check flags a first source that names the literal version.
  - 188 recipes have no `homepage =`; musl-rpmatch keeps its URL in a comment; kdos-record and xdg-desktop-portal-kdos carry an empty `source =`. Add the keys, drop the empty lines, and make preflight refuse a recipe without `description` or `homepage`.
  - `writing-ports.md:190-191`: the solver reads at most 128 names (`KP_MAX_DEPS`); the longest line is vlc's, 73.
  - Tree counts: 2,027 recipes (2,003 ports + 24 src), lists of 2,053 lines naming 2,017 ports, 2,021 installed, 6 absent. Fix `README.md:82`, `glossary.md:518`, `repository-layout.md:201`, `writing-ports.md:5,926`, `ports-catalogue.md:3-4`, `packaging.md:76,181,673,834`, `build-system.md:182,1083`, `how-kdos-differs.md:43,231`, `principles.md:265`, `status.md:103-106,251` (every listed port has been built; name the 6 absent). Exact counts stay only on `status.md`, `ports-catalogue.md` and `packaging.md`, recomputed by docscheck; every other page says "about 2,000" or links there (Q49).
  - `known-gaps.md:597-598` and `status.md:74-75`: libdvdcss is built and libdvdread links it; keep only "no disc has been read".
  - Unlisted recipes (Q50, MET-08): list helix (`41_system/packages.d/editors.txt`; rust, git), musl-locales (`40_lang/packages.d/toolchain.txt`; cmake, ninja, gettext), setconf (`40_lang/packages.d/buildtools.txt`; python3), perl-xml-simple (`40_lang/packages.d/perl-cpan.txt`; perl, perl-xml-parser) and icon-naming-utils (`41_system/packages.d/themes.txt`; perl-xml-simple). musl-locales' catalogs go under S14; its `locale` command ships. Run phaseclosure.
  - Consumerless libraries (Q51, MET-09): the 15 library ports with no consumer, and qca-qt5 once quassel moves to Qt 6 (16 in all), (about 225 MB: webkitgtk6 193 MB, gtkmm4 14 MB, rttr, goocanvas, libgrapheme, the small KF6, Qt and GTK libraries) stay; `ports-catalogue.md` marks them as the application-developer platform. openh264 gains consumers through the libheif row (41_system) and the chromium unbundle (S9).
  - 21 sources fetch over `http`/`ftp` and 28 homepages are `http`: switch to https where upstream serves it (x.org, cpan, landley.net, fftw, launchpad, webmproject, infradead); bytes are unchanged, so no republish. Optional preflight warning.
- [x] **S26 · P1 · other: licences, source offer and updates on a shipped machine.** (CRT-02, CRT-03, CRT-06)
  - Licences (Q18): 1,525 of 2,021 packages ship no licence text (musl, gcc, bash, curl, mesa, ffmpeg, linux, kdos-comp among them); no recipe has a licence key; the default ISO (`KDOS_ISO_SOURCES=0`) carries no source and no written offer. Fix: kpkg packaging copies the source root's top-level `LICENSE*`, `LICENCE*`, `COPYING*`, `COPYRIGHT*`, `NOTICE*` into `/usr/share/licenses/<name>/`; recipes gain an SPDX `license =` key that preflight requires; 70_image writes `/usr/share/kdos/SOURCES` naming the commit and the `sources-NNN` releases; S14's prune never touches `/usr/share/licenses`. Document in `writing-ports.md` and `packaging.md`.
  - Updates (Q17): `110_iso.sh` excludes `ports` from `system.sfs` (`-e … ports`, l.91) and copies the tree only onto a `KDOS_ISO_SOURCES=1` developer stick, so `/ports` is empty, `kdos update check` exits 2 every night (`update.c:200`), and the panel reads only `"behind":` (`panel.c:2424-2427`) and shows no badge.
    - Every image carries the recipe-only ports tree at `/ports`: each `kpkgbuild`, `build.sh` and patch, `ports/shelves` and `ports/sources.idx`, and no source tarball. Our own programs' `src/` trees travel with it, since no archive holds them.
    - A newer tree reaches the machine over the network: a `kdos update` verb replaces `/ports` with the tree of a newer KDOS commit, verified before use. `update.c` has no fetch path today (`update.c:49`: "Nothing in this tree fetches over the network").
    - `kdos update apply` builds each port whose recipe changed from source; kpkg fetches its sources (the `sources-NNN` archive, then upstream). No binhost: `--binhost-only`, the `binhost` key and the binhost wording in `update.c:10-34` go.
    - The panel and `kdos update check` show the error state: no tree, no network, a failed fetch or build.
    - `administration.md`, `kdos-command.md#kdos-update` and `how-kdos-differs.md#updates` state the policy: updates need the network for sources and compile on the machine, for hours on a large set.
  - Integrity: the db records paths only, so nothing can check an installed root. kpkg records `sha256 mode path` per regular file (a db column or `db/.files/`); add `kpkg check [<pkg>|--all] [--root DIR]` for missing, changed or unowned files; `kdos update apply` runs it on the slot before `kdos-bootctl try`. Document in `packaging.md` and `kdos-command.md`. Lands after S4 and S10 so the db matches the image.

---

## S18 · userland: replace toybox

The S18 item in Systemic fixes. It lands in the Q9 `--fresh` rebuild (it rewrites `10_bootstrap`), closes S6 and removes the cause of S1.

**Decision.** Toybox is deleted: `ports/core/base/toybox/`, `10_bootstrap/062_toybox.sh`, the image and the initramfs. GNU coreutils 9.12 in full (separate programs, no single binary) and a new GNU grep join the GNU sed, gawk, findutils, diffutils, patch and tar already built, at every name, cross-built in `10_bootstrap` as LFS 12.4 chapter 6 does. PID 1 is **sysvinit 3.18**: `init`, `telinit`, `halt`, `poweroff`, `reboot`, `shutdown`, `runlevel` and `killall5`, nothing else. getty is util-linux `agetty` on tty1, tty2 and ttyS0. BusyBox is not used anywhere. uutils, chimerautils and sbase are not candidates.

**Why.** Toybox fails silently:
- 0.8.14 `install -Dm644 /dev/stdin` writes 0600 (re-run 2026-10-16; GNU 9.12 on musl writes 644), leaving 24 recipes' files unreadable (S1: 460 in all, 407 of them neverball's).
- Its `awk` hands the kernel an ORC hash of empty input.
- 300 error lines hit 7 ports.
- PID 1, getty, awk, expr, tr and stty come from `toys/pending`, which upstream calls "unfinished commands", kept out of defconfig.

BusyBox does not remove the problem:
- On musl it differs from GNU on 36 of 103 probes (toybox: 45), with at least 9 silent wrong answers. Example: 1.38.0 awk `printf "%d"` of any value ≥2³¹ prints −2147483648.
- Its last stable release is 1.36.1 (2023-05-19); 1.37.0 and 1.38.0 are labelled unstable.
- Debian tracks 25 open CVEs, 20 of them from 2026.
- Alpine, which ships BusyBox, pulls GNU coreutils into 141 of its build recipes.

uutils reaches 90–95 % of GNU's tests, had 113 audit findings (44 CVEs) in 2026, and needs Rust, which KDOS builds only in `31_compilers`.

GNU coreutils 9.12 passes its own suite on musl (761 tests, 0 FAIL, 1 ERROR caused by the environment) and has had 4 CVEs since 2021-10, all fixed in 9.12.

sysvinit (measured 2026-10-16 on 3.18 source and in an `alpine:3.22` container):
- Upstream is https://codeberg.org/thejessesmith/sysvinit; the GitHub repo `slicer69/sysvinit` says it moved, and its last release there is 3.14. 3.18 is 2026-03-26; `main` holds an unreleased 3.19 (leak fixes in `init`, `shutdown`, `killall5`; a `bootlogd` bounds fix).
- Builds on musl with no patch: `make -C src init halt shutdown runlevel killall5` gives warnings only. It also builds against utmps with `-I/usr/include/utmps -lutmps -lskarnet` and no patch, because `utmps/utmp.h` maps the `utmp` API onto `utmpx` with macros. Adélie (musl) ships 3.14 against utmps; Alpine and Void ship no sysvinit (Repology).
- Booted as PID 1 in the container: `runlevel` answers `N 3`. SIGTERM is ignored. SIGINT runs the `ctrlaltdel` entry. `telinit 0` and plain `reboot` first TERM the inittab entries the new runlevel does not list, then run that runlevel's `wait` entries in file order, each to completion.
- **SIGUSR2 closes `/run/initctl` for the rest of the boot** (`init.c:2846-2852`). After it, `telinit` gives up after 3 s (`init.c:3089`) with "timeout opening/writing control channel". `kdos-powerd` sends exactly that signal today, so it must change in the same commit.
- `init` signals only its own inittab children (`init.c:1761-1830`): TERM, then KILL after 3 s (`init.h:43`). Every other process is left to the runlevel 0 and 6 entries.
- A respawn that fails 10 times in 120 s is disabled for 5 minutes (`init.h:46-48`, `init.c:1077`).
- Single-user mode is runlevel `S`, served by util-linux `sulogin` (`/sbin/sulogin`, `paths.h:37`), which asks for root's password. `init single` and `init emergency` are honoured (`init.c:3206-3210`), and leaving the `sulogin` shell continues to the default runlevel (`init.c:2679-2690`).

**Applet → provider (all 144; each name once).**

| Applets | Provider | Note |
|---|---|---|
| `[` arch base32 base64 basename cat chgrp chmod chown chroot cksum comm cp cut dd df dirname du echo env expand factor false fmt fold groups head hostname id install link logname ls md5sum mkdir mkfifo mknod mktemp mv nice nl nohup nproc od paste printenv pwd readlink realpath rm rmdir seq sha1sum sha224sum sha256sum sha384sum sha512sum shred shuf sleep sort split stat stty sync tac tail tee test timeout touch tr true truncate tsort tty uname uniq unlink wc who whoami yes (83) | coreutils (full) | `arch` and `hostname` need `--enable-install-program`. `chroot` moves `/usr/sbin` → `/usr/bin`; nothing names the path |
| sha3sum, crc32, fsync, usleep | coreutils, names dropped | `cksum -a sha3`, `cksum -a crc32b`, `sync FILE`, `sleep 0.1` |
| grep egrep fgrep | **grep** (new) | |
| awk | gawk | `/usr/bin/awk` → gawk |
| which | **which** (new) | R's configure records `/usr/bin/which` |
| getconf, iconv | **musl-utils** (new) | adds `getent`. waf (tdb, talloc, samba), zeromq and liburcu run `getconf`; gnuplot runs `iconv` |
| cpio | **cpio** (new) | `boot/linux/postinstall.sh:98,146` and `090_initramfs.sh:1172,1196` |
| ping, ping6 | **iputils** (new) | `ping -6`; a `ping6` link is optional |
| xxd | **tinyxxd** (new) | configure probes it in dosfstools, libperseus-sdr and viking |
| init halt poweroff reboot killall5 | **sysvinit** (new) | adds `telinit`, `shutdown` and `runlevel` |
| getty | util-linux `agetty` | |
| hd, iorenice, fstype, freeramdisk | util-linux | `hexdump -C`, `ionice`, `blkid -s TYPE -o value`, `blockdev --flushbufs` |
| ifconfig, netstat, vconfig, tunctl | iproute2 | `ip addr`, `ss`, `ip link add … type vlan`, `ip tuntap` |
| host | ldns | `drill` |
| wget, ftpget, ftpput | curl | |
| sntp | chrony | |
| microcom | picocom, minicom | |
| gpiofind | libgpiod 2.3.1 | `gpioinfo NAME` (v2 has no gpiofind) |
| mix | alsa-utils | `amixer` |
| mkpasswd | whois | the recipe builds `mkpasswd` too |
| time | bash keyword | GNU time is optional |
| oneit httpd dnsdomainname hexedit dos2unix unix2dos uuencode uudecode ts inotifyd pwgen nbd-client nbd-server acpi ascii unicode devmem watchdog readahead makedevs memeater help (22) | dropped | nothing in `fs/`, `src/`, `script/` or any `build.sh` runs them. lynx loses its uudecode probe and libnbd its nbd-server test probe |

**New ports.**

| Port | Upstream | Shelf | Phase | Notes |
|---|---|---|---|---|
| grep 3.12 | https://www.gnu.org/software/grep/ | base | 30_foundation first block; cross-built in 10_bootstrap | `--disable-nls`; `depends = pcre2` (for `-P`) |
| which 2.25 | https://www.gnu.org/software/which/ | base | 30_foundation first block | |
| musl-utils | `getconf.c` `getent.c` `iconv.c` from https://gitlab.alpinelinux.org/alpine/aports/-/tree/master/main/musl, pinned to one commit | base | 30_foundation first block | three `file::url` sources, one `cc` each |
| sysvinit 3.18 | https://codeberg.org/thejessesmith/sysvinit | base | 30_foundation, base section | `releases/download/3.18/sysvinit-3.18.tar.xz`, sha256 `793d7427e219f1ccf122e7c55655cef5f3419adc620ec2ee0ed557e8c94b438f`; `depends = utmps` |
| cpio 2.15 | https://www.gnu.org/software/cpio/ | archiver | 41_system `archiver.txt` | the tree's `-std=gnu11` avoids its GCC 15 C23 breakage |
| iputils 20250605 | https://github.com/iputils/iputils | net-diag | 41_system `net-diag.txt` | `-DNO_SETCAP_OR_SUID=true -DUSE_GETTEXT=false` |
| tinyxxd | https://github.com/xyproto/tinyxxd | devtools | 40_lang `devtools.txt` | installs `/usr/bin/xxd`; lands before its configure-time users |

**coreutils (`base/coreutils`)**
- [x] `build.sh`:
  - Drop `INSTALL_PROGRAMS` (L36), the install loop (L51–54) and every comment (L12–35, L38–41, L44–47); keep `export FORCE_UNSAFE_CONFIGURE=1` (L42), which kpkg needs because it builds as root.
  - Run `make DESTDIR=$PKG install`.
- [x] Configure: `fu_cv_sys_stat_statvfs=n ./configure --prefix=/usr --disable-nls --without-openssl --without-selinux --with-libgmp --enable-acl --enable-xattr --enable-libcap --enable-install-program=arch,hostname`.
  - Separate programs, no `--enable-single-binary`.
  - `kill` and `uptime` are already off by default in 9.12.
  - Against utmps (Q39): `CPPFLAGS=-I/usr/include/utmps LIBS="-lutmps -lskarnet"`, so `who`, `users` and `pinky` read utmpd.
- [x] The constraints behind those flags go in the book (`packaging.md`, "The base userland"), not in `build.sh` (rule 6, Q10):
  - musl `statvfs` keeps only 32 bits of `f_fsid`, so `stat -f -c %i` is wrong without the cache variable;
  - configure links libcrypto into every program when it is present;
  - `--enable-xattr` is what lets `cp -a` carry xattrs.
- [x] `kpkgbuild`: `depends = gmp acl attr libcap utmps`; new `description`; release bump.

**`10_bootstrap`**
- [x] Delete `062_toybox.sh`.
- [x] Add seven scripts in the `050_xz.sh` shape: `extract_port_source`, `--host=$KDOS_TARGET --build=$(build-aux/config.guess) --prefix=/usr`, `make DESTDIR=$SYSROOT install`, and their own `$MARK`.
  - `062_coreutils.sh`: `FORCE_UNSAFE_CONFIGURE=1 fu_cv_sys_stat_statvfs=n`, `--disable-nls --without-openssl --without-selinux --without-libgmp --disable-acl --disable-xattr --disable-libcap --enable-install-program=arch,hostname`.
  - `063_diffutils.sh`: `--disable-nls gl_cv_func_strcasecmp_works=y`.
  - `064_findutils.sh`: `--disable-nls --localstatedir=/var/lib/locate`.
  - `065_gawk.sh`: `--disable-nls`.
  - `066_grep.sh`: `--disable-nls --disable-perl-regexp`.
  - `067_patch.sh`: needed by S3's gcc patch (20_selfhost) and Q12's perl patch (30_foundation).
  - `068_sed.sh`: `--disable-nls`.
- [x] `060_gzip.sh`, `061_tar.sh`: add `--host=$KDOS_TARGET`. Without it they are built by the builder's own gcc and run only because alpine:3.23 is also musl.
- [x] `phase.env`: set `KDOS_PHASE_DESC="musl, GNU userland, bash, native gcc, kpkg, kinstall"`.
- [x] `Dockerfile`: add `grep sed gawk findutils diffutils patch` to the apk list, so `00_cross` and `10_bootstrap` configure under GNU tools rather than BusyBox.

**Phase lists, pins, store key**
- [x] `30_foundation/packages.txt`:
  - Replace `toybox` (L49) with `coreutils grep which musl-utils`.
  - Rewrite L26-27 to name bash and GNU coreutils.
  - Rewrite the L51–57 comment (its toybox reason goes; the run stays) and delete the L75–80 sed comment.
  - Add `skalibs utmps sysvinit` to the base section, ahead of `util-linux`.
  - skalibs and utmps are listed here, not in 41_system: util-linux (30_foundation, L70), coreutils and sysvinit build against utmps (Q39). execline and s6 are needed only at run time and go in 41_system. utmps' configure takes `--enable-pkgconfig` (off by default) so consumers can use `pkg-config libutmps`.
  - kpkgdepends pulls coreutils' depends (gmp, acl, attr, libcap) and grep's (pcre2) ahead.
- [x] `40_lang/packages.d/00-order.txt`:
  - Delete the `toybox` run (L39) and its L41–54 comment.
  - The L55–81 re-pins exist only to take names back after toybox; remove them. A port named nowhere else moves to its shelf file.
- [x] `41_system/packages.d/00-order.txt`:
  - Delete the `coreutils` pin (L39–42).
  - The games run (L44–51 comment) loses its reason; move nethack, frotz, bsd-games and moon-buggy to their shelf files unless another pin holds them.
- [x] `boot/linux` `kpkgbuild`: add `cpio` to `depends`.
- [x] `src/daemons/kdos-powerd/kpkgbuild` and `src/system/kdos-installer/kpkgbuild`: `depends = sysvinit`. They run `telinit` and `reboot`.
- [x] `script/env/chroot.env:152` `KPKG_STORE_BASE`: replace `toybox` with `grep which musl-utils`. sysvinit is not a build tool and stays out. Every store key changes; the fresh rebuild absorbs it.
- [x] Run `python3 testing/phaseclosure.py`.

**PID 1: the sysvinit port (`base/sysvinit`)**
- [x] `build.sh`, commands only (rule 6):
  - `make -C src init halt shutdown runlevel killall5 CFLAGS="$CFLAGS -I/usr/include/utmps" LDLIBS="-lutmps -lskarnet"`.
  - Name explicit targets and never run `make install`. It would install `last`, `lastb`, `mesg` and `utmpdump` (util-linux's; `src/Makefile:27,36`), make a `/bin/pidof` link (procps-ng's `/usr/bin/pidof` through the `/bin` link; `src/Makefile:219`) and a `lastb → last` link over util-linux's `lastb` (`:220-222`).
  - Nothing in the tree uses `sulogin`, `wall`, `bootlogd`, `readbootlog`, `logsave` (e2fsprogs') or `fstab-decode` from sysvinit, so they are not built. Build flags only, no patch (rule 9).
  - Pass `CFLAGS` on the command line, never `CPPFLAGS`. A command-line `CPPFLAGS` overrides the target-specific `init_utmp.o: CPPFLAGS += -DINIT_MAIN` (`src/Makefile:171`). The Makefile appends its own flags to `CFLAGS` with `override +=` (`:13`).
  - Install `init halt shutdown runlevel killall5` into `$PKG/sbin`, and add the links `telinit → init`, `reboot → halt` and `poweroff → halt`.
  - Install `man/{init,telinit,halt,reboot,poweroff,shutdown,runlevel,killall5}.8` and `man/{inittab,initctl}.5`.
  - No `/etc/inittab.d` and no `/etc/initscript`. The `initscript` hook would wrap every entry in `/bin/sh` (`init.c` spawn).
- [x] Against utmps, `init` writes `INIT_PROCESS` and `DEAD_PROCESS` for every console. It also writes the `BOOT_TIME` record when it leaves `sysinit` (`init.c:2654-2660`), which is after `rcS` has started `utmpd`, and `RUN_LVL` on every runlevel change. `wtmp` is appended directly to `/var/log/wtmp` in utmps' record layout, because `HAVE_UPDWTMP` is glibc-only (`utmp.c:46-54,139-144`).

**inittab, getty, boot scripts**
- [x] `fs/etc/inittab`, rewritten whole (banner kept):

  ```
  # Runlevel 3 is the running system. S is single-user: sulogin on the
  # console, no consoles from this file. 0 powers off and 6 reboots.
  id:3:initdefault:

  si::sysinit:/etc/init.d/rcS

  # sulogin asks for root's password on the console. Leaving its shell
  # continues to runlevel 3. The initramfs passes `single` from the kernel
  # command line, and `telinit S` reaches it from a running system.
  ~~:S:wait:/sbin/sulogin

  # Local Console (QEMU Window / Physical) — the desktop's login.
  #
  # kdos-login reads /etc/kdos/login.conf and hands the tty to agetty, with
  # `--autologin` where that file names an account. The desktop itself is
  # started by the login shell's profile, not from here.
  #
  # kdos-getty stays in the chain and stays FIRST: it loads the console font
  # and palette after fbcon's deferred take-over, and doing that in rcS is
  # futile because the take-over resets every VT's font. It also falls back
  # to the plain autologin getty if the program named here is missing, so a
  # broken login path still gives a console.
  1:2345:respawn:/usr/local/sbin/kdos-getty tty1 /usr/local/sbin/kdos-login tty1

  # Second console, root login prompt. THE RECOVERY CONSOLE: it is a plain
  # agetty with no --autologin, and nothing above may be allowed to make it
  # one too, because it is what is left when the desktop on tty1 does not
  # come up.
  2:2345:respawn:/usr/local/sbin/kdos-getty tty2 /sbin/agetty --noclear tty2 38400 linux

  # Serial console. The live image logs root in with no password; the
  # installer removes `--autologin root` from the installed copy, which then
  # asks for a login. --keep-baud stays at the rate the kernel console set.
  S0:2345:respawn:/sbin/agetty --autologin root --keep-baud --noclear -L ttyS0 115200,38400,9600 vt102

  ca::ctrlaltdel:/sbin/shutdown -r now

  # Runlevels 0 and 6. init first sends TERM, and KILL 3 s later, to every
  # entry above that the runlevel does not list (the consoles, and with tty1
  # the session), then runs these in order, each to completion. rcK stops
  # the services while the filesystems they save to are writable; killprocs
  # signals every process still alive; killpower may tell a UPS to cut the
  # power, so every filesystem is read-only before it runs.
  k0:06:wait:/etc/init.d/rcK
  k1:06:wait:/etc/init.d/killprocs
  k2:06:wait:/sbin/swapoff -a
  k3:06:wait:/bin/umount -a -r
  k4:06:wait:/etc/init.d/killpower
  h0:0:wait:/sbin/halt -d -f -p
  h6:6:wait:/sbin/reboot -d -f
  ```

  - No process field contains a shell metacharacter, so init executes each one directly and never through `/bin/sh -c` (`init.c:1110`). Each field is under the 253-character limit.
  - `::restart:/sbin/init` has no sysvinit form and is dropped. `telinit u` re-executes init.
  - `-d` writes no wtmp record, because `/var/log` is read-only by then. `-f` calls `reboot(2)` without asking init again.
  - Nothing sends SIGPWR (NUT shuts down through `SHUTDOWNCMD`), so there are no `power*` entries.
- [x] `fs/etc/init.d/killprocs`, a new script with no `NN_` prefix so `rcS` and `rcK` never run it. It runs `killall5 -15`, then polls `killall5 -18` until it answers 2 (no process left, `killall5.c:1096,1198`) for at most 3 s, then runs `killall5 -9` and `sync`. `killall5` spares its own session and kernel threads, so the script survives its own signals.
- [x] Single-user mode exists, and nothing else is needed for it:
  - `090_initramfs.sh`: in the `/proc/cmdline` loop (L573), set `INIT_ARG` for `single` or `emergency`. Both execs (L873, L1102) become `exec switch_root /newroot /sbin/init $INIT_ARG`.
  - `single`: `rcS` runs as on every boot (every enabled service starts), then `sulogin`, with no gettys and no desktop.
  - `emergency`: `sulogin` before `rcS` (`init.c:2949-2954`); only the initramfs's mounts exist.
  - The installed menu's `KDOS (single user)` entry (`bootctl.c:875`, `install.c:2120-2125`) now does what its comment says.
- [x] Q30, installed ttyS0: after copying the image, `kdos-installer/install.c` edits `TARGET/etc/inittab` in place, the way it edits `login.conf` (`install.c:1603-1670`). It removes `--autologin root` from the `S0` line, leaving `agetty … ttyS0` asking for a login. Document it in `kinstall.md`.
- [x] ttyS0 with no UART (`SERIAL_8250_RUNTIME_UARTS=32` makes `/dev/ttyS0` always exist): agetty fails on the dead port and init respawns it at most 10 times in 120 s, then pauses for 5 minutes and logs "respawning too fast". If the `-serial none` gate shows that, land a guard. `S0` would run `/etc/init.d/serial-getty`, which runs `exec sleep infinity` when `/sys/class/tty/ttyS0/type` is `0` and `exec /sbin/agetty "$@"` otherwise.
- [x] `fs/etc/init.d/05_hostname.sh:10`: `read -r _h < /etc/hostname && hostname "$_h"` (coreutils `hostname` has no `-F`).
- [x] `rcS:100`: drop `|| ifconfig lo up`.
- [x] `rcS:17`, `rcK:35`: keep the PATH export. sysvinit hands its children `PATH=/sbin:/usr/sbin:/bin:/usr/bin` (`init.h:51`, measured), and `TERM` stays the kernel's `linux` because init sets none on Linux (`init.c:819-827`).
  - Reword `rcS:13-16` to name sysvinit.
  - Rewrite the `rcK:13-33` comment: rcK is the first runlevel 0 and 6 entry; init has already stopped the consoles, and `killprocs` signals everything left after rcK.
- [x] `kdos-powerd/main.c:215-235` `do_reboot`:
  - Replace `kill(1, cmd == RB_POWER_OFF ? SIGUSR2 : SIGTERM)` (L231) with `/sbin/telinit 0` or `/sbin/telinit 6` through the argv builder (`kb_argv_add` / `kb_run`, as `tlp_hook`, L180-186, does).
  - Keep `sync()`, `sleep(60)` and the `reboot(cmd)` fallback.
  - Rewrite the L219-230 comment as constraints only:
    - telinit writes a runlevel request to `/run/initctl` and gives up after 3 s;
    - init then stops the consoles and runs `rcK` first;
    - SIGTERM is ignored by sysvinit and SIGUSR2 closes `/run/initctl` for the rest of the boot, so no signal may stand in for telinit;
    - the 60 s wait must outlast the console TERM/KILL (≤3 s) plus rcK's walk down to `55_powerd`.
- [x] Installer reboot (`main.c:529`, `pages.c:1953`, `execlp("reboot")`): no change. sysvinit `reboot` without `-f`, outside runlevel 0 or 6, execs `/sbin/shutdown -r now`, which execs `init 6` (`halt.c:185`, `shutdown.c:462-463`). Measured in the container.
- [x] `/run/initctl`: no `fs/` entry. init makes the FIFO itself and makes it again on the tmpfs that `rcS`'s `mount -a` lays over `/run`, within one 5 s loop (`init.c:2505` and the same-inode check after it).
- [x] `ports/core/hardware/nut/build.sh:52-53`: the comment says the system has no `/sbin/shutdown`. Now upstream's `SHUTDOWNCMD "/sbin/shutdown -h +0"` works as shipped; delete the comment (S17).
- [x] `testing/vnc-shot.py`: no change. agetty's autologin banner contains `login:`, so the rig types `root` into the shell once (harmless), and the probe answers. A disk installed by this ISO stops at `Password:` (Q30); the rig's installed-disk runs need the root password passed in.
- [x] `testing/qemu-hw/verify.py:5,87` and `testing/qemu-hw/probe.py:55`: they wait for `Please press Enter`. Wait for the shell as `vnc-shot.py` does.

**Initramfs (`script/phases/70_image/090_initramfs.sh`)**
- [x] L31–36: replace the toybox copy and applet loop with `copy_closure` (L250), which already fails the build on a missing library. Copy:
  - what `/init` runs: coreutils `cat chmod cut ls mkdir mknod seq sleep`;
  - a rescue set: `cp mv rm ln rmdir head tail wc stat df dd sync uname readlink touch sort tr`;
  - GNU `grep` and `sed`.
- [x] L38–69, L105–131: replace each `grep -qa 'Toybox .* multicall'` with a positive check that `--version` names util-linux. Applies to switch_root, mount, umount, losetup, dmesg and blkid.
- [x] L573, L873, L1102: the `single` / `emergency` forwarding above.
- [x] L1172, L1196: `find . | LC_ALL=C sort | cpio -o -H newc --reproducible` (GNU cpio ≥2.12).

**kpkg**
- [x] Delete the toybox port outright. Nothing declares `replaces = toybox`, because a `--fresh` build never installs toybox.

**Recipes (same change)**
- [x] Delete `fabrication/linuxcnc/toybox-options.patch`, `disk/libblockdev/toybox-mktemp.patch` and `office/libreoffice/toybox-cp.patch`, with their `patch` lines.
- [x] Remove the `install -C` workaround from the 12 NetSurf recipes.
- [x] `net-diag/whois`: build `make … whois mkpasswd` with `-DHAVE_SHA_CRYPT` added to CPPFLAGS, install with `install-whois install-mkpasswd install-bashcomp`, delete the `rm` of the mkpasswd completion (L56) and the comments L33–43 and L46–52.
- [x] `office/libreoffice/build.sh:229`: keep the `rm` (GNU `cp` also writes through a destination link and refuses a dangling one); delete the comment, and state the constraint in libreoffice's book entry.
- [x] Delete the toybox comments in the rest of the 29 `build.sh` files (53 lines; S17), including xapian-core, passt, aerc, qemu, brltty, openjdk, wv, man-pages, cataclysm-dda, tzdata, libdeflate and sed. The 8 ports that `depends = coreutils` keep the dependency.

**`src/`, `fs/`, `testing/` (rule 1)**
- [x] Strings:
  - `kdos.c:4677` "userland toybox" → "userland GNU";
  - `about.c:467` and the `:774` badge;
  - regenerate `testing/goldens/about-{machine,build,credits}-78x19.txt` in kdos-devdeps.
- [x] Comments:
  - `kdos.c:162`;
  - `kdos.c:1404` (GNU `ls` reads `LS_COLORS`, which the comment denies);
  - `kdos.c:4321`;
  - `kdosbuild/manager.c:1316`;
  - `kdosbuild/tui.c:2648` (preview step name);
  - `kdos-kpkg/front.c:670`;
  - `libkpkg/kpkg.h:242`;
  - the strings in `kdos-splash` and `kdos-bb`;
  - `kdos-tools/reasons/*.txt` (3 files).
- [x] The "no getent on musl" comments: `87_kolibri.sh:21`, `41_polkitd.sh:35`, `45_seatd.sh:16`, `profile.d/10-wayland.sh:10`.
- [x] `script/env/common.env:46`; `70_image/070_user.sh:45`: see the 70_image `070_user.sh` (DEF-17) item; `fs/etc/skel/.config/fastfetch/config.jsonc`.
- [x] `testing/selftest.sh:3650-3653` reads `$(port_dir toybox)/build.sh` and greps for the initramfs's `rm -f bin/blkid`; both checks break once toybox is gone. Replace them with "no port installs `usr/bin/blkid`" and keep the `cp /usr/sbin/blkid bin/blkid` check (L3648). Fix the comments at `:3543` and `:3642-3647` and the `:3654` message.
- [x] `testing/hostcheck.sh:20-38`; `testing/preflight.sh:1191`.
- [x] `testing/preflight.sh:1852-1853`: the inittab check already reads four colon fields and works on sysvinit lines. Its comment cites `::shutdown:`; use `ca::ctrlaltdel:` (empty runlevels, still names a program).
- [x] `testing/bootcheck/README.md:46-48`: the serial console is agetty's autologin root shell (live) and a login on an installed disk.

**Book (rule 1: replace, never annotate)**
- [x] `03-architecture/boot-and-init.md`:
  - L31 glossary row (toybox → sysvinit and runlevels); L48–49 (`init (sysvinit)`);
  - L53 and L1309 (ttyS0: agetty autologin on live, login when installed), L1085 (sysvinit actions: `sysinit`, `respawn`, `wait`, `ctrlaltdel`, `initdefault`, runlevels);
  - L178–179 and L219 (`single` → the initramfs passes it, rcS runs, then `sulogin`; `emergency` row);
  - L368, L415;
  - L976–1081 "Tools that must not be toybox's" → the util-linux identity rule;
  - L1089 (PATH from sysvinit);
  - L1192–1221 "Shutdown" rewritten in this order:
    1. how a shutdown starts: `telinit`, `shutdown`, `reboot`, Ctrl-Alt-Del, powerd;
    2. init TERMs and then KILLs the consoles;
    3. rcK, killprocs, swapoff, `umount -a -r`, killpower;
    4. `halt -d -f -p` or `reboot -d -f`;
    5. powerd's `telinit` and its 60 s fallback.
  - L1308 (tty2 is agetty);
  - L1351;
  - L1365–1366: init does not respawn forever. It disables an entry for 5 minutes after 10 failures in 120 s, and the console stays down that long.
- [x] `05-developer/build-troubleshooting.md`: delete "A tool missing after toybox reinstalls" (L622–636) and its L100 row; rewrite L1025–1081; fix L1347 and L1698.
- [x] `01-philosophy/how-kdos-differs.md`: L38–39 (Init row: sysvinit), "The core userland" (L76–106), L114 (PID 1), L134 (single-user mode exists).
- [x] `03-architecture/packaging.md`: L191–194, L234–237, L544; L557–571 becomes "The base userland" and carries the coreutils constraints.
- [x] `01-philosophy/decisions.md`: L550 and L841–846, plus the userland and PID 1 decisions themselves.
- [x] `01-philosophy/why-kdos.md`: L50, L55, L60, L250; `01-philosophy/principles.md:41` (name sysvinit as PID 1).
- [x] `README.md:78`.
- [x] `06-reference/ports-catalogue.md` (add the 7 new ports) and `06-reference/glossary.md` (L12, L320, L566, L571; replace the `toybox` entry L831–837; add `runlevel` and `sysvinit`).
- [x] `06-reference/known-gaps.md`:
  - delete "There is no single-user mode" (L716–722);
  - delete "Upgrading toybox on its own can delete tools other ports own" (L842–849);
  - L730 (the serial root shell is live-only, Q30).
- [x] `06-reference/configuration.md:989-1004` `/etc/inittab`: rewrite the table for the sysvinit lines (`initdefault`, `sysinit`, `S`/sulogin, tty1, tty2, `S0`, `ctrlaltdel`, the 0/6 entries, with `killprocs`). Drop the `restart` row.
- [x] Command index: `telinit`, `runlevel`, `shutdown`, `halt`, `poweroff`, `reboot`, `killall5`, `sulogin`, plus getent, cpio, which, xxd, ping, arping, tracepath.
- [x] `03-architecture/overview.md`: L23, L30, L93, L110, L257 (PID 1 is sysvinit); L124, L164, L307 (ttyS0).
- [x] `03-architecture/security-model.md`: L45; L88 (ping comes from iputils, without setuid); L983–984 (the serial root shell is live-only; installed ttyS0 asks for a login; single-user asks for root's password).
- [x] `03-architecture/session.md:46` (tty2 is agetty).
- [x] `04-programs/daemons.md:276-280` (powerd runs `telinit 0`/`6`, then the 60 s `reboot(2)` fallback).
- [x] `04-programs/kinstall.md` (the installer rewrites the `S0` line).
- [x] `02-user-guide/getting-started.md:266` (ttyS0: logged in as root on the live image with no keypress).
- [x] `05-developer/how-kdos-is-built.md`; `build-system.md:76`, `:99` and `:186-195` (the 10_bootstrap row, the `062_toybox.sh` example, and the 30_foundation and 40_lang order runs); `writing-ports.md` L1034, L1081; `developing.md`.
- [x] `04-programs/kdos-shell.md`; `04-programs/kdos-command.md` (`kdos version`).
- [x] Run `bash testing/docscheck.sh`.

**Gates**
- [ ] Stage 0, needs no `build/`: — **status:** open: `ports/publish` and the `sources.idx` lines for the 11 new sources are the user's; `preflight.sh` still reports 15 problems, which clear with the commit of the init.d 0755 modes and the rebuild; every other sub-bullet passes
  - each new port and coreutils test-built in kdos-devdeps with chroot.env's CC, CFLAGS and LDFLAGS exported (sysvinit against utmps);
  - coreutils `make check` gives 0 FAIL;
  - the 7 bootstrap scripts built against a scratch sysroot in the builder image;
  - sysvinit as PID 1 in a musl container, using a test inittab of the same shape:
    - `telinit 0` and plain `reboot` TERM the respawn entries, then run the `06` entries in file order;
    - SIGINT runs `ctrlaltdel`;
    - `runlevel` answers;
  - `grep -n 'SIGUSR2\|kill(1' src/daemons/kdos-powerd/main.c` finds nothing;
  - `ports/publish` and a `sources.idx` line for each of the 7 new sources;
  - `preflight.sh`, `selftest.sh`, `phaseclosure.py`, `docscheck.sh`, `make fetch-check`.
- [ ] After 20_selfhost (canary run: `--fresh --phases 10_bootstrap,20_selfhost` after emptying `build/fs`, `build/mark` and `build/cross`):
  - no `toybox` file under `build/fs`;
  - `ls`, `grep`, `sed`, `awk`, `patch` and `cmp` all report GNU from `--version`.
- [ ] After 30_foundation, inside the chroot:
  - `echo hi | install -Dm644 /dev/stdin /tmp/f; stat -c %a /tmp/f` gives 644;
  - `readlink -f /usr/bin/awk` points at gawk;
  - `stat -f -c %i /` does not end in `00000000`;
  - `getconf _NPROCESSORS_ONLN` answers;
  - `/sbin/init --version` names sysvinit 3.18;
  - no file in the sysvinit manifest is also in util-linux's, procps-ng's or e2fsprogs';
  - no `Taking … from toybox` line in any log.
- [ ] After 41_system: none of these in any log: `Unknown option '…' (see "… --help")`, `bad regex`, `version is toybox`, or a `Toybox` uname string.
- [ ] After 60_kernel:
  - `orc_hash.h` is not `0xda,0x39,0xa3,0xee…`;
  - the linux postinstall wrote `/boot/initramfs-kdos.cpio.gz`.
- [ ] After 70_image:
  - `find build/fs/{etc,usr} -type f ! -perm -o+r ! -perm -4000 ! -perm -2000` lists only the allowlist (it was 460 files);
  - still 19 setuid files (the list measured before the rebuild);
  - `find build/fs -lname '*toybox*'` and the initramfs listing show no toybox.
- [ ] Rig, ISO:
  - tty1 autologin; tty2 agetty root login; ttyS0 is a root shell with no keypress;
  - `runlevel` prints `N 3`; `who -b` and `who -r` answer;
  - poweroff and reboot through powerd (`kdos-power poweroff|reboot` → `telinit`), the installer's `reboot`, and Ctrl-Alt-Del on tty2 each come back with a clean boot;
  - the serial log of each shows the consoles TERMed, then `rcK`, `killprocs`, `swapoff`, `umount -a -r` and `killpower`, in that order;
  - `telinit S` from the serial shell stops tty1 and tty2 and starts `sulogin` on the console; root's password gives a shell, and `telinit 3` brings the consoles back;
  - `ping -c1 127.0.0.1` works as `kdos`; `getent passwd kdos` answers;
  - a `kdos-box snapshot` / `rollback` round trip keeps a directory that was deleted and recreated empty.
- [x] Rig, ISO with `-serial none`: over 3 minutes, count the `S0` agetty PIDs and look for `respawning too fast` in the log. Any churn lands the guard. — **closed:** the ttyS0 guard landed: `S0` runs `/etc/init.d/serial-getty`, which sleeps instead of starting agetty when `/sys/class/tty/ttyS0/type` is `0`
- [ ] Rig, a disk installed by this ISO:
  - ttyS0 asks for a login (Q30);
  - `KDOS (single user)` boots through rcS to a `sulogin` password prompt with no desktop, and leaving the shell continues to the desktop;
  - a kernel install writes its initramfs.

**Risks**
1. Changing the bootstrap means one full `--fresh` run (about 20 h and 84 GB). Mitigation: the 10/20 canary run above.
2. The cross configures in `00_cross` and `10_bootstrap` run under the builder image's tools; the Dockerfile item gives them GNU grep, sed, gawk, findutils, diffutils and patch (coreutils is already in its apk list, `Dockerfile:20`), as LFS cross-builds this set.
3. Ports that took toybox's fallback path now take the GNU path (libnbd, OpenJDK's date check, podman `grep -P`, libvpx `cat -n`, uname strings). Their outputs change; diff the logs.
4. GNU grep 3.8 and later warn on `egrep`/`fgrep` and stray backslashes. This is stderr only, and no KDOS script uses either.
5. `cp -a` now copies xattrs. In appbox snapshots this is probably a fix; a gate checks it.
6. The shutdown order changes:
   - the session on tty1 is TERMed before `rcK` runs, not after `umount -a -r`;
   - the services stop with no user processes left;
   - `killprocs` reaches only what rcK did not stop.
7. sysvinit has one maintainer. Its repo moved to Codeberg; GitHub stops at 3.14.
   - 3.19 is unreleased and carries leak fixes in `init`, `shutdown` and `killall5`; take it when it is tagged.
   - It is dynamically linked against musl, libutmps and libskarnet. musl is in `KPKG_STORE_BASE`, so a musl change rebuilds it.
8. coreutils' multibyte code keeps producing CVEs (uniq, unexpand and factor across 9.5–9.11). Track point releases.
9. utmp rests on two supervised daemons (Q39). With `utmpd` down, `who`, `users` and `pinky` print nothing and init's `BOOT_TIME` and `RUN_LVL` records are lost; `runlevel` still answers from `/run/runlevel` (measured in the container).
10. Size: the rootfs grows by about 10 MB (separate programs), the initramfs by about 0.5 MB gzipped. Compare the 3.1 GB of DWARF in S2.
11. The ttyS0 churn on a machine with no UART is inferred from kernel and agetty behaviour, not measured. sysvinit's respawn limit bounds it; the `-serial none` gate decides on the guard.
12. init prints `INIT: version 3.18 booting` on `/dev/console`. Check on the rig that the splash is undisturbed.

Effort: about 5–6 working days, plus the fresh build and one rig session.

---

## 00_cross

No findings.

## 10_bootstrap

Bootstrap steps, the programs only bootstrap builds (kpkg, kinstall), and the `fs/` overlay laid down by `000_file_system.sh`.

- [x] **P2** `script/phases/10_bootstrap/100_gcc.sh` · layout: stale bootstrap `.la` files give C++ links `RPATH /usr/lib/../lib64` and a build-dir `-L` (S3).
  - Evidence: `41_system__0292_libbs2b`, `-Wl,--rpath -Wl,/usr/lib/../lib64 … -L/workspace/build/tmp/gcc-16.2.0/build/x86_64-kdos-linux-musl/libgcc`. Also `0864_libqalculate`, `0863_libetpan`, `0841_graphicsmagick`. kpkg already deletes every `.la` in a package (`build.c` `strip_la`); these are unowned bootstrap leftovers.
  - Fix:
    - After install: `rm -f $SYSROOT/usr/lib64/lib{stdc++{,exp,fs},supc++,itm}.la $SYSROOT/usr/lib64/libstdc++.so.*-gdb.py`.
    - Remove the same way the `.la` files that `090_binutils.sh` (libbfd, libctf, libctf-nobfd, libopcodes, libsframe) and `050_xz.sh` (liblzma) leave, and `liblto_plugin.la`.
    - The libtool C++ consumers rebuild in the world rebuild.
- [x] **P3** `script/phases/10_bootstrap/080_bash.sh` · hyg: `--disable-static` is not a bash option.
  - Evidence: `10_bootstrap__0010_bash.sh.log`, `unrecognized options: --disable-static`.
  - Fix: delete l.35.
- [x] **P3** `120_kpkg.sh`, `130_kinstall.sh` · hardening: kpkg and kinstall ship as the bootstrap cross build and are never rebuilt; they compile with `-O2 -pipe` and no link flags, so no BIND_NOW, no stack-clash probes, no build-id. kpkg runs as root and parses package archives. (PLAN-EARLY-21, HRD-04)
  - Evidence: `readelf -d build/fs/usr/bin/kpkg` shows only `FLAGS_1 PIE`; both unowned, dated with the bootstrap.
  - Fix: build kdos-kpkg and kdos-installer as owned ports in 41_system with the tree flags (update `build-system.md:102-105`), or add `-fstack-protector-strong -fstack-clash-protection -Wl,-O1,--as-needed,-z,now,-z,pack-relative-relocs -Wl,--build-id=sha1` to both compile lines and name the exception at `decisions.md:384`. Update the pages that quote the compile line.
- [x] **P2** `src/system/kdos-installer` · ux: the live-only "Install KDOS" entry (`fs/etc/skel/.local/share/applications/kdos-install.desktop`) survives onto every installed system, in `/etc/skel` and the user's home, and reruns the installer. (UX-04)
  - Fix: after the copy, unlink both copies on the target and refresh the `mimeinfo.cache` beside each. Update `kinstall.md` ("Starting the installer") to say the entry exists only on the medium.
- [x] **P1** `fs/etc/init.d` · defaults: `86_kiwix-serve.sh`, `87_kolibri.sh`, `88_llama-server.sh` and `killpower` are mode 100644 in git. rcS skips non-executable scripts (`rcS:137`, `[ -x "$_script" ] || continue`), and init executes `/etc/init.d/killpower` directly from its inittab entry (S18) and gets EACCES, so a NUT low-battery shutdown never cuts UPS power. (DEF-02)
  - Fix: `git update-index --chmod=+x` on the four files and `chmod +x` in the work tree. Preflight: every `fs/etc/init.d/*` except `service_helper` is 100755 in `git ls-files -s`.
  - Verify: `administration.md:131-133`, `boot-and-init.md:1183-1185,1203` and `configuration.md:1006` match.
- [x] **P2** `fs/etc/profile` · defaults: l.14 exports `LD_LIBRARY_PATH=/usr/lib:/lib:/usr/local/lib64:/usr/local/lib` to the whole session. musl searches it before an object's RPATH, so koreader's `$ORIGIN` bundled libraries are shadowed by `/usr/lib` copies; a program behaves differently from a shell than from an init script or D-Bus activation; and `configuration.md:1083` ("`/usr/local/lib` is not searched") is false in every login session. (DEF-08, LNK-05)
  - Fix: delete the line. If `/usr/local/lib` should load, add it to `fs/etc/ld-musl-x86_64.path` (that file is in the store salt, so it rebuilds everything) and update `configuration.md:1083`. Check no `profile.d` script or launcher depends on the export first.
  - Verify: `grep -n LD_LIBRARY_PATH fs/etc/profile fs/etc/profile.d/*` prints nothing.
- [x] **P2** `fs/etc/init.d/25_nftables.sh` · security: when the ruleset fails `nft -c`, the script keeps "the current ruleset", which at boot is empty, so one bad drop-in under `/etc/nftables.d/` boots with no input filter while sshd starts. (DEF-09)
  - Fix: on a failed check with no `inet filter` table loaded, load a shipped minimal drop-policy ruleset (or the shipped conf without its include line, written to `/run`) and report `[FAIL]`. Document it in `configuration.md:942` and `security-model.md`.
- [x] **P1** `fs/etc/init.d/70_sshd.sh` · security: sshd starts at every boot with password authentication (upstream default; `sshd_config:58` leaves it commented) while the live accounts have known passwords; only the firewall's `input` drop keeps it off the network (`security-model.md:987-988`), and one toggle exposes it. (DEF-07)
  - Fix (Q29): ship `fs/etc/service.disabled/sshd`; rcS skips a service with a marker (`rcS:134-140`). The firewall `ssh` toggle (`kdos-powerd/main.c:257`, `kdos-shell/firewall.c:12`) removes the marker and starts sshd when it opens port 22, and restores both when it closes it. The installer offers SSH and, when chosen, removes the marker on the target. Rewrite `security-model.md:987-988`; update `administration.md`, `kdos-shell.md` (the firewall surface) and `kinstall.md`.
  - Verify: a live boot has no `sshd` process; the ssh toggle starts it and stops it.
- [x] **P3** `fs/etc/resolv.conf` · defaults: the file names Google's resolvers (`nameserver 8.8.8.8`, `8.8.4.4`), so every lookup before NetworkManager hands resolvconf its servers goes to a third party. (DEF-18)
  - Fix (Q31): `nameserver 127.0.0.1`. Describe the pre-network file beside the `/etc/resolvconf.conf` row (`configuration.md:1229`).
- [x] **P3** `fs/etc/sysctl.conf` · hardening: no network hardening; ICMP redirects are accepted, no reverse-path filter, `kptr_restrict=0`. (DEF-13)
  - Fix: add `net.ipv4.conf.{all,default}.accept_redirects=0`, `secure_redirects=0`, `send_redirects=0`, `net.ipv6.conf.{all,default}.accept_redirects=0`, `rp_filter=2`, `kernel.kptr_restrict=1`, each with a constraint comment (`10_sysctl.sh` reads only this file). Check hotspot and podman forwarding; update `security-model.md` and the `10_sysctl` row at `administration.md:78`.
- [x] **P2** `fs/etc/xdg/kdos-mimeapps.list` · defaults: `.docx` opens a chooser every time, because doxx (a terminal entry) and Writer both claim it and no default row exists; `desktop.md:707,720-722` says Writer is the only claimant. (UX-05)
  - Fix: add `application/vnd.openxmlformats-officedocument.wordprocessingml.document=libreoffice-writer.desktop`; rewrite the `desktop.md` paragraph so `.docx` has a row and says why.
- [x] **P3** `fs/etc/xdg/kdos-mimeapps.list` · defaults: 57 common types with two or more handlers have no default and open a chooser: about 20 camera RAW types, `image/x-xbitmap`, `x-pcx`, `x-icns`, openraster, `audio/webm`, `audio/ac3`, `x-scheme-handler/ssh`. (UX-11)
  - Fix: RAW and minor image types → `org.kde.gwenview.desktop`; `audio/webm` and `ac3` → a handler that claims them (Haruna or mpv); set `x-scheme-handler/ssh` deliberately; extend the `desktop.md` table.
- [x] **P3** `kdos theme` kdeglobals writer, `/usr/local/bin/xdg-terminal-exec` · ux: KDE apps open Konsole for a terminal (no `TerminalApplication` in kdeglobals) while the desktop uses foot; the xdg-terminal-exec header says no other listed terminal is installed, but konsole is. (UX-12)
  - Fix: emit `[General] TerminalApplication=foot` and `TerminalService=foot.desktop`; rewrite the xdg-terminal-exec comment as the constraint that it must be first in GLib's list. Update `kdos-command.md` (`kdos theme`), `04-programs/README.md:162` and `kdos-appbox.md:796`.

## 20_selfhost

- [x] **P1** `gcc` (`toolchain/gcc`) · layout: `MULTILIB_OSDIRNAMES m64=../lib64` puts the runtime in `/usr/lib64` (S3, Q3).
  - Evidence: 318 ELF files with RPATH `/usr/lib/../lib64`, e.g. `readelf -d libqqwing.so.2.1.0`. Also `41_system__1125_libfilezilla`, `42_graphics__0462_love`, and vlc plugins.
  - Fix: ship `t-linux64-lib.patch` (`m64=../lib`) and apply it; drop the lib64 symlinks and the gdb auto-load move.
- [x] **P2** `gcc` (`toolchain/gcc`) · dwarf: libgcc builds with `LIBGCC2_DEBUG_CFLAGS=-g`, so every static libgcc pull ships DWARF.
  - Evidence: `readelf -S libgcc.a | grep -c debug_info` gives 596. libgcc_eh gives 12. `libgcc_s.so.1` carries about 1 MB of DWARF. In `20_selfhost__0013_gcc`, libgcc compiles show `-g -DIN_LIBGCC2`.
  - Fix:
    - `make LIBGCC2_DEBUG_CFLAGS=-g0` and `make DESTDIR=$PKG install LIBGCC2_DEBUG_CFLAGS=-g0`.
    - The same in `10_bootstrap/100_gcc.sh` and in `embedded/gcc-{avr,arm-none-eabi,riscv64-unknown-elf}` (their multilib `.a` carry the same DWARF).
    - If `readelf -S libgcc.a | grep -c debug_info` is not 0, fall back to `strip --strip-debug` on `libgcc{,_eh,cov}.a`.
    - Name LIBGCC2_DEBUG_CFLAGS in the `writing-ports.md` DWARF paragraph.
- [x] **P3** `gcc` (`toolchain/gcc`), `gcc-avr`, `gcc-arm-none-eabi`, `gcc-riscv64-unknown-elf` · hardening: the target libraries (libstdc++, libgcc_s, libgomp, libgfortran, the sanitizers; 13 objects) link without `-z now` or DT_RELR, and the host programs (`cc1`, `lto1`, `collect2`, `gcc`; 27 in gcc, 18 in each cross gcc) are non-PIE. (PLAN-EARLY-20, HRD-06)
  - Fix: `LDFLAGS_FOR_TARGET="$LDFLAGS"` on gcc, and `--enable-host-pie --enable-host-bind-now` on all four. Land in the gcc rebuild with S2a and S3.
  - Verify: `readelf -d libstdc++.so.6` and `libgcc_s.so.1` show BIND_NOW and RELR; `cc1` is DYN; a C++ hello, an OpenMP program and a sanitizer build still link.
- [x] **P2** `gcc` · layout: about 195 MB of unowned bootstrap toolchain ships (S4).
  - Evidence: `usr/lib/gcc/x86_64-kdos-linux-musl` 175 MB, `usr/x86_64-kdos-linux-musl` 19 MB, seven `usr/bin/x86_64-kdos-linux-musl-*` drivers; no db entry owns them. `usr/share/info/{gcc,gccint,cpp,cppinternals,gccinstall}.info` are unowned bootstrap copies too.
  - Fix:
    - A late 20_selfhost step removes all three trees, the five bootstrap info files and `include/c++/16.2.0/x86_64-kdos-linux-musl`, after native gcc and binutils are installed. `grep -r kdos-linux-musl ports/core src` is empty today.
    - Update `build-system.md`.
- [x] **P2** `binutils` (`toolchain/binutils`) · hidden: 16 man pages are zero-length (S8).
  - Evidence: `20_selfhost__0004_binutils`, `pod2man: command not found` (x16) and `doc/objdump.1] Error 127 (ignored)`.
  - Fix: after install, copy the tarball's prebuilt pages (`../binutils/doc/{addr2line,ar,c++filt,elfedit,nm,objcopy,objdump,ranlib,readelf,size,strings,strip}.1` and `../gas/doc/as.1`) into `$PKG/usr/share/man/man1/`; remove `dlltool.1`, `windres.1` and `windmc.1`; bump release.
- [x] **P3** `gcc` · hidden: `gcc.1`, `g++.1` and `gfortran.1` contain only the word "timestamp"; `cpp.1`, `gcov*.1` and `lto-dump.1` are fine (S8). (PLAN-EARLY-07)
  - Fix: `install -m644 ../gcc/doc/{gcc,g++,gfortran}.1 -t "$PKG/usr/share/man/man1"` after `make install`. Not `gcc/doc/*.1`, which holds pages for languages that are not built. Land in the gcc rebuild.
- [x] **P3** `gcc` · feat: native gcc installs no `gcc.info`, `cpp.info` or `gfortran.info`; the copies in the image are unowned bootstrap files. (PLAN-EARLY-08)
  - Evidence: `20_selfhost__0013_gcc:2866`, `Makeinfo is missing or too old`.
  - Fix: after install, `install -m644 ../gcc/doc/{gcc,cpp}.info* ../gcc/fortran/gfortran.info* -t $PKG/usr/share/info/` (add gccint and cppinternals if wanted). Leave `BUILD_INFO` alone: setting it to `info` makes the build fail.
- [x] **P3** `gawk` (`base/gawk`) · hyg: `gawknotes.info` has no dir entry, so install-info warns in 37+ logs.
  - Evidence: `41_system__0021_moon-buggy`, ``install-info: warning: no info dir entry in `//usr/share/info/gawknotes.info'``.
  - Fix: `rm -f "$PKG/usr/share/info/gawknotes.info"`.
- [x] **P3** `mpfr`, `mpc`, `ncurses`, `readline`, `make`, `bash` · hyg: `--host=$TARGET` with `TARGET` unset.
  - Evidence: `20_selfhost__0008_mpfr`, `checking host system type... x86_64-pc-linux-musl` (guessed).
  - Fix: remove `--host=$TARGET` from all six.

Withdrawn from 20_selfhost: the interim `rm -f "$PKG"/usr/lib64/*.la`; kpkg already deletes every `.la` in a package, and the stale ones are bootstrap leftovers (10_bootstrap item) (PLAN-EARLY-09).

## 30_foundation

- [x] **P1** `sqlite` (`database/sqlite`) · layout: `libsqlite3.so.3.53.4` has no SONAME; 91 objects link it by the dev name or by path (S7).
  - Fix: `--soname=legacy` and `--disable-rpath` (the latter removes the redundant DT_RPATH `/usr/lib`). The consumers rebuild in the world rebuild.
  - Verify: no ELF has an unversioned sqlite NEEDED.
- [x] **P1** `perl` (`perl-cpan/perl`) · feat: perl is built without ithreads, yet `threads` and `threads::shared` ship. GNU parallel's `parcat` dies.
  - Evidence: `30_foundation__0015_perl:39`, `Build a threading Perl? [n]`. Config shows `useithreads='undef'`.
  - Fix: `-Dusethreads`. archname becomes `-thread-multi`; every perl XS port rebuilds in the world rebuild.
- [x] **P2** `perl` · other: not reproducible. Config records the container hostname, the host kernel and the wall-clock time. (PLAN-EARLY-02)
  - Evidence: `Config_heavy.pl:236` `cf_time='Tue Oct 13 …'`, `:1148` `myhostname='4f71458b3bdf'`, `:1149` `myuname='linux 4f71458b3bdf …'`.
  - Fix: `build.sh` writes one `config.over` setting `myhostname`, `mydomain`, `cf_email`, `perladmin`, `cf_by`, `osvers`, `myuname` and `cf_time` (from `SOURCE_DATE_EPOCH`). Configure sources `config.over` (l.24880-24887) after it sets `myuname` (l.3349) and `cf_time` (l.3924) and before it writes `config.sh`, so no patch is needed.
  - Verify: build twice and compare `Config_heavy.pl` and `CORE/config.h`.
- [x] **P2** `perl` · linkage: `libperl.so` is only in `/usr/lib/perl5/core_perl/CORE`, so PostgreSQL's PL/Perl (`plperl.so`, `jsonb_plperl.so`, `hstore_plperl.so`, `bool_plperl.so`, built `--disable-rpath`) cannot load, and `CREATE EXTENSION plperl` fails. (LNK-03)
  - Fix: perl installs `usr/lib/libperl.so -> perl5/core_perl/CORE/libperl.so`; land with Q12's rebuild. Verify every NEEDED `libperl.so` resolves through the path file.
- [x] **P3** `perl` · layout: the 707 man3 pages use the bare `.3` suffix, so tcl's `Thread.3` takes perl's (S5). (PLAN-HEAD-05)
  - Fix: `-Dman3ext=3pm` in Q12's rebuild.
- [x] **P2** `ncurses` (`base-libs/ncurses`) · layout: `ncursesw.pc`, `formw`, `menuw`, `panelw`, `ncurses++w` `.pc` and `ncursesw6-config` carry the tree LDFLAGS (S11). (PLAN-EARLY-01)
  - Evidence: `41_system__0633_ncdu`, `skipping pkg-config for package ncursesw due to unknown flags`.
  - Fix: `--disable-pkg-ldflags` on configure, with LDFLAGS exported as now. Verify the `.pc` has `Libs: -lncursesw` and `ncursesw6-config --libs` prints no `-Wl` flag.
- [x] **P2** `python3` (`python/python3`) · dwarf: the LTO path adds `-g` (about 37 MB).
  - Evidence: `30_foundation__0039_python3:2007`, `-flto … -ffat-lto-objects -g`.
  - Fix: `ac_cv_prog_cc_g=no` on configure. Name it in `writing-ports.md` ~l.400. Lands with the 3.14.8 bump (S23).
- [x] **P2** `libuv` (`net-libs/libuv`) · dwarf: configure appends `-g`.
  - Evidence: `30_foundation__0050_libuv:67`.
  - Fix: `export CFLAGS="$CFLAGS -g0"` before configure.
- [x] **P2** `ninja` (`buildtools/ninja`) · dwarf: 8.9 MB binary with DWARF, and C `-std=gnu11` reaches C++.
  - Evidence: `30_foundation__0074_ninja`, `'-std=gnu11' is valid for C/ObjC but not for C++` (x33).
  - Fix: `env -u CFLAGS CXXFLAGS="$CXXFLAGS -g0" python3 ./configure.py --bootstrap`.
- [x] **P2** `nasm` (`toolchain/nasm`) · dwarf: `-g3 -ggdb`.
  - Evidence: `30_foundation__0095_nasm:289`.
  - Fix: `--disable-gdb pa_cv_CFLAGS__g3=no pa_cv_CFLAGS__ggdb=no`.
- [x] **P2** `argp-standalone` (`toolchain/argp-standalone`) · dwarf: `libargp.a` is built with `-ggdb3`, so 115 shipped binaries carry its DWARF.
  - Evidence: eu-* tools, libdw, crun, f3, v4l-utils `ir-ctl` and the bpf-* tools all show producer `-ggdb3 … argp-help.c`. `configure.ac:86` appends the flag after `$CFLAGS`.
  - Fix: `make CFLAGS="$CFLAGS -g0"` (an override before configure loses). Bump release; its consumers rebuild in the world rebuild.
- [x] **P2** `curl` (`net-libs/curl`) · dep: no brotli, because of the cycle brotli→cmake→curl.
  - Evidence: `30_foundation__0047_curl:625`, `brotli: no`.
  - Fix: break the cycle (Q14): cmake `--no-system-curl`, curl dropped from its depends. Then `--with-brotli`, brotli in depends, guard `grep -q '^#define HAVE_BROTLI 1' lib/curl_config.h`. Land with the 8.23.0 bump (S23).
- [x] **P3** `bzip2` (`archiver/bzip2`) · hardening: `libbz2.so.1.0.8`, `bzip2` and `bzip2recover` link without the tree LDFLAGS (no BIND_NOW, no RELR); `Makefile-libbz2_so` has no LDFLAGS variable. (PLAN-EARLY-19, HRD-09)
  - Fix: `make -f Makefile-libbz2_so CC="gcc $LDFLAGS" CFLAGS="$CFLAGS -fPIC"` and `make CC="gcc $LDFLAGS" CFLAGS="$CFLAGS"`. Bump release.
- [x] **P3** `kmod` (`base/kmod`) · layout: the six tools are recorded twice, `./sbin/*` and `./usr/sbin/*`; `build.sh:31-34` adds a `$PKG/sbin` loop on top of kmod's own install, and `--bindir=/bin` routes the links through the `/bin` alias. (PLAN-HEAD-20, PLAN-EARLY-22)
  - Fix: drop the `$PKG/sbin` loop and `ln … $PKG/bin/lsmod`; drop `--bindir=/bin` after changing `relative-tool-links.patch` to `../bin/kmod`. `090_initramfs.sh` and `01_udev.sh` use `/usr/bin/kmod` and keep working. Bump release.
- [x] **P3** `doxygen` (`doctools/doxygen`) · linkage: compiles its bundled SQLite although sqlite builds earlier in 30_foundation. (LOG-12)
  - Fix: `-Duse_sys_sqlite3=ON` and sqlite in depends, after S7. fmt and spdlog stay bundled.
- [x] **P3** `texinfo` (`doctools/texinfo`) · hardening: `tta/configure.ac:300` replaces CFLAGS with Perl's ccflags, so the C parser and 10 XS modules build with no `-O` and none of the tree flags; `makeinfo` runs in hundreds of builds. Its sysconfdir is `/usr/etc` (S13). (LOG-14, LOG-06)
  - Fix: `PERL_EXT_CFLAGS="$CFLAGS" PERL_EXT_LDFLAGS="$LDFLAGS"` and `--sysconfdir=/etc` on configure. Verify the `tta/C` compile lines carry `-O2`.
- [x] **P3** `python3-markupsafe` (`python-libs/python3-markupsafe`) · other: a dead vendor bundle (only setuptools 82, while pip runs `--no-build-isolation --no-deps`). (PLAN-EARLY-16)
  - Fix: drop `vendoring`, the vendor sha256, the extraction and `--find-links`; retire the `sources.idx` line. Bump release.
- [x] **P3** `perl` · hyg: `-L/usr/local/lib` is recorded in `ldflags` and `lddlflags`, so it reaches every XS link.
  - Evidence: `40_lang__0172_perl-crypt-urandom`. `Config_heavy.pl:1088`.
  - Fix: `-Dloclibpth=' ' -Dlocincpth=' ' -Dglibpth='/usr/lib'`.
- [x] **P3** `perl` · dep: DB_File is not built.
  - Evidence: `:828`, no DB_File. `i_db='undef'`.
  - Fix: the new port `perl-db-file` (perl-cpan, 41_system, depends db perl).
- [x] **P3** `python3` · newport: uses the bundled libmpdec, which upstream removes in 3.16.
  - Evidence: `:282`, `bundled copy of libmpdec is scheduled for removal`.
  - Fix: the new port `mpdecimal`, then `--with-system-libmpdec`.
- [x] **P3** `pkgconf` (`buildtools/pkgconf`) · hyg: the autotools build is deprecated and removed in 3.1.
  - Evidence: `30_foundation__0009_pkgconf:2`.
  - Fix (Q13): the new port `muon` ordered before pkgconf, then pkgconf 3.1 built with muon. muon must build without pkg-config (its bootstrap script).
- [x] **P3** `libssh2` (`net-libs/libssh2`) · layout: DT_RPATH `/usr/lib`.
  - Evidence: `30_foundation__0041_libssh2:116`.
  - Fix: `--disable-rpath`.
- [x] **P3** `libffi` (`base-libs/libffi`) · hyg: `--enable-pax_emutramp` does nothing.
  - Fix: remove the flag.
- [x] **P3** `python3-setuptools` (`python/python3-setuptools`) · hyg: installed with `setup.py install`, which gives egg-info.
  - Evidence: `30_foundation__0051_python3-setuptools:827`.
  - Fix: build a wheel with the in-tree `build_meta`, then `python3 -m zipfile -e` it into site-packages.
- [x] **P3** `meson` (`buildtools/meson`) · hyg: installed with `setup.py install`.
  - Evidence: `30_foundation__0075_meson:308`.
  - Fix: `pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .`, and add python3-pip to depends.
- [x] **P3** `flex` (`toolchain/flex`) · layout: no POSIX `lex`.
  - Fix: `ln -s flex $PKG/usr/bin/lex` and `ln -s flex.1 $PKG/usr/share/man/man1/lex.1`.
- [x] **P3** `python3-sphinx` (`python-libs/python3-sphinx`) · net: the man-page build tries three intersphinx fetches.
  - Evidence: `30_foundation__0072_python3-sphinx`, `intersphinx inventory 'https://docs.python.org/3/objects.inv' not fetchable`.
  - Fix: pass `-D extensions=` with conf.py's list minus `sphinx.ext.intersphinx`; delete the build.sh comment (S17).
- [x] **P3** `shadow` (`auth/shadow`) · layout: installs through `/bin` and `/lib`, and setuid `su`, and `login`, carry RPATH `/lib`.
  - Evidence: `30_foundation__0093_shadow`, ``remember to run 'libtool --finish /lib'``.
  - Fix: `--exec-prefix=/usr`. Check the newuidmap and newgidmap setuid lines still name the right paths.
- [x] **P3** `lua54-luafilesystem` (`lang/lua54-luafilesystem`) · hyg: `lfs.so` is linked without LDFLAGS (no BIND_NOW).
  - Fix: `LIB_OPTION="-shared $LDFLAGS"`.
- [x] **P3** `itstool` (`buildtools/itstool`) · tool: 7 invalid-escape `SyntaxWarning`s under Python 3.14.
  - Evidence: `44_apps__1025_gnome-mines:239`, `/usr/bin/itstool:223: SyntaxWarning: "\s" is an invalid escape sequence`.
  - Fix: ship `raw-strings.patch` making the literals at l.223, 321, 459, 461, 892, 927 and 1471 raw strings.

Static archives in this phase ship (Q8); no change (PLAN-EARLY-10).

Moved to S18: the `toybox` items (the awk takeover, the missing RELRO and build-id) and sed's `replaces = toybox`; toybox is deleted (PLAN-EARLY-13, PLAN-HEAD-06, PLAN-EARLY-14, HRD-05). Withdrawn: `--without-static-libpython`; `libpython3.14.a` ships (Q22, PLAN-EARLY-11).

## 31_compilers

- [x] **P2** `ruby` (`lang/ruby`) · dwarf: libruby carries about 1.9 MB of DWARF from YJIT (`rustc -g`, hard-coded in `YJIT_RUSTC_ARGS`). (PLAN-EARLY-03)
  - Fix: after `make install`, `strip --strip-debug "$PKG/usr/lib/libruby.so.$version"`. Do not touch LDFLAGS: `rbconfig.rb` records them, so every gem extension would inherit the change. Bump release.
- [x] **P2** `rust` (`lang/rust`) · dwarf: rustc, rustdoc and clippy-driver each carry about 1.35 MB of jemalloc `-g3` DWARF.
  - Evidence: `31_compilers__0075_rust:287`, `Compiling tikv-jemalloc-sys`.
  - Fix: `rustflags = ["-Cstrip=debuginfo"]` (or `strip = true`) under `[rust]` in the generated config.toml.
- [x] **P1** `go` (`lang/go`) · defaults: `$GOROOT/go.env` is not installed (`build.sh` copies `bin pkg src lib api misc` only), so `GOPROXY`, `GOSUMDB` and `GOTOOLCHAIN` are empty and `go get`, `go install x@latest` and every un-vendored `go build` fail with "GOPROXY list is not the empty string, but contains no entries". (DEF-03)
  - Fix: `cp go.env VERSION $PKG/usr/lib/go/`, with the content per Q32 (`GOTOOLCHAIN=local`). Verify `go env GOPROXY` is non-empty against `build/fs`. Land with the 1.27.2 bump (S23).
- [x] **P3** `go` · size: 33 MB of `src/**/testdata` ships, including glibc, FreeBSD and riscv64 ELF fixtures that trip any ELF gate. (LNK-14)
  - Fix: `rm -rf` every `$PKG/usr/lib/go/src/**/testdata` (only `go test std` reads it).
- [x] **P2** `bindgen` (`toolchain/bindgen`) · metadata: its description is cbindgen's and says the opposite of what it does. (MET-01)
  - Fix: `description = Generates Rust FFI bindings from C and C++ headers`.
- [x] **P3** `llvm` (`toolchain/llvm`) · layout: `LLVM_INSTALL_UTILS=ON` (`build.sh:49`) installs the generic test utilities `count`, `not`, `FileCheck`, `split-file`, `yaml-bench` into `/usr/bin` under names another port may own (S5). (PLAN-HEAD-05, PLAN-EARLY-14)
  - Fix: remove those five from `$PKG/usr/bin` (or install the lit utilities outside `/usr/bin`).
- [x] **P3** `zig` (`lang/zig`, `build.sh:24`) · hardening: `-D ZIG_PIE=OFF` makes `/usr/bin/zig` ET_EXEC. (HRD-10)
  - Fix: `-D ZIG_PIE=ON`; verify `readelf -h` reports DYN.
- [x] **P3** `ghc`, `cabal-install`, `pandoc`, `shellcheck` · hardening: GHC drives the linker itself and links non-PIE with lazy binding: `pandoc` (234 MB), `shellcheck`, `cabal`, 11 ghc tools. (HRD-11)
  - Fix: measure `-pie`/`-optl-pie` and `-optl-Wl,-z,now` in the GHC settings file; adopt if the test programs pass, otherwise record the exception under S22.
- [x] **P3** `rust` · layout: rust-installer metadata ships (a 2.5 MB `install.log`, `uninstall.sh`, manifests naming `$PKG`).
  - Fix: `rm -f "$PKG"/usr/lib/rustlib/{components,install.log,rust-installer-version,uninstall.sh} "$PKG"/usr/lib/rustlib/manifest-*`.
- [x] **P3** `cargo-c`, `rust` (`buildtools/cargo-c`, `lang/rust`) · dep **[S9]**: both compile a vendored libgit2. The libgit2 port is in 40_lang.
  - Evidence: `31_compilers__0084_cargo-c:146`, `Compiling libgit2-sys v0.18.8+1.9.7`.
  - Fix: move `libgit2` from `40_lang/packages.d/vcs.txt` to 30_foundation, add it to both ports' depends, `export LIBGIT2_NO_VENDOR=1`, delete the stale comment.

## 40_lang

- [x] **P2** `python3-attrs` (`python-libs/python3-attrs`) · layout: the build-only `hatch-fancy-pypi-readme` is pip-installed into `/` and left unowned (S4).
  - Evidence: `40_lang__0217_python3-attrs:12`.
  - Fix: `pip3 install … --target "$SRC_ROOT/backends"` plus `PYTHONPATH`, the python3-jsonschema shape. The same in `pim/vdirsyncer`, `bioscience/gnuhealth` (expandvars) and `transfer/deluge`. Delete the stranded files from build/fs.
- [x] **P2** `bcc` (`devtools/bcc`) · dwarf: 109 MB in the 74 bpf-* tools, from libbpf-tools `CFLAGS := -g`.
  - Fix: pass `CFLAGS="$CFLAGS -Wno-error=missing-field-initializers"` on both make lines.
- [x] **P2** `valgrind` (`devtools/valgrind`) · dwarf: 105 MB from `AM_CFLAGS_BASE = -O2 -g`.
  - Fix: `./configure … CFLAGS="$CFLAGS -g0"`.
- [x] **P2** `ocaml` (`lang/ocaml`) · dwarf: 42.4 MB across 47 files. (PLAN-EARLY-04)
  - Evidence: `configure.ac:2975` adds `-g` to C; `ocamlopt.opt` has 302 `GNU AS` (OCaml) and 58 `GNU C11` compile units.
  - Fix: `CFLAGS="$CFLAGS -g0"` for the C runtime (`libasmrun*.a` is linked into every user executable), and after install `strip --strip-debug` the ELF executables in `$PKG/usr/bin`. Leave the OCaml `-g` (frametable backtraces) and Makefile.config alone.
- [x] **P2** `zls` (`devtools/zls`) · dwarf and hardening: 15 MB of DWARF, and a non-PIE static ET_EXEC. (PLAN-EARLY-05, HRD-10)
  - Fix: `strip --strip-debug "$PKG/usr/bin/zls"` after install, not `-Dstrip=true` (which drops `.symtab`, against Q2). zls 0.16.0's `-Dpie` sets only `.pic`, so a PIE needs a `build.zig` `.patch` setting `exe.pie`; otherwise record it under S22.
- [x] **P3** `luajit` (`lang/luajit`) · dwarf: 4.7 MB, from `CCDEBUG= -g`. (PLAN-EARLY-06)
  - Fix: `make amalg PREFIX=/usr XCFLAGS="$xcflags" CCDEBUG= TARGET_STRIP=:` (an empty CCDEBUG switches on a strip-all). Verify `.symtab` stays and `.debug_info` is gone.
- [x] **P3** `ltrace` (`devtools/ltrace`) · layout: `SYSCONFDIR=/usr/etc`, so `/etc/ltrace.conf` is ignored (S13).
  - Fix: `--sysconfdir=/etc`.
- [x] **P3** `gdb`, `gef` (`devtools/gdb`, `devtools/gef`) · dep **[S9]**: the comment says guile is not a port, but it is (41_system). (PLAN-EARLY-15)
  - Evidence: `40_lang__0126_gdb:3694`, `guile support disabled`.
  - Fix: move gdb and gef together to `41_system/packages.d/devtools.txt` (gef depends on gdb), `--with-guile`, guile in depends; run phaseclosure.
- [x] **P3** `rust-analyzer` (`devtools/rust-analyzer`) · hyg: reports version 0.0.0.
  - Fix: `export CFG_RELEASE=$version`.
- [x] **P3** `perl-uri` (`perl-cpan/perl-uri`) · newport: `URI::otpauth` cannot load (no MIME::Base32).
  - Fix: the new port `perl-mime-base32`, added to depends.
- [x] **P3** `gn` (`buildtools/gn`) · hyg: C `-std=gnu11` reaches every C++ compile (212 warnings).
  - Fix: `CFLAGS="${CFLAGS/-std=gnu11/}" python3 build/gen.py …`.
- [x] **P3** `python3-mako` (`python-libs/python3-mako`) · other: a dead vendor bundle.
  - Fix: drop `vendoring = python`, the vendor sha256, the extraction and `--find-links`. Add `homepage`. Retire the `sources.idx` line.
- [x] **P3** `python3-meshtastic` (`python-net/python3-meshtastic`) · layout and linkage: installs a top-level `site-packages/contrib/` of Python 2 skeleton code (the only cause of 70_image's compileall failure, and a generic import name), and `mesh-analysis` cannot import (no `dash`, `plotly`, `parse`). (IMG-13, LOG-04, LNK-12)
  - Fix: `rm -rf "$PKG"/usr/lib/python3.*/site-packages/contrib`; drop the `mesh-analysis` entry point or add its modules as ports (Q11). Bump release.

Withdrawn from 40_lang: the bcc tools prune; with `/usr/bin/python` (Q24) the 134 tools run, and S21's gate covers the shebangs (LNK-08).

## 41_system

- [x] **P0** `gpsd` (`gis/gpsd`) · layout: `scons install` writes into the live root. The package holds 2 files. (PLAN-41A-10)
  - Evidence: `build/logs/41_system/0705_gpsd.install.log:245`, `Install file: … as "/usr/bin/gpsdebuginfo"` (86 such lines). `SConscript:676` reads `DESTDIR` from the environment only.
  - Fix:
    - `DESTDIR="$PKG" scons prefix=/usr … install`, with DESTDIR in the environment.
    - Guard with `test -x "$PKG/usr/sbin/gpsd" && test -e "$PKG/usr/lib/libgps.so"`. Bump release.
    - Purge the unowned files from build/fs: `usr/sbin/gpsd*`, `usr/bin/{cgps,gpsmon,…}`, `libgps*`, `gps.h`, `libgpsmm.h`, python `gps/`, `usr/share/gpsd`.
  - Verify: stellarium, gpredict, viking, direwolf and marble's `GpsdPositionProviderPlugin.so` resolve `libgps.so.32`, now in db/gpsd. No consumer rebuild is needed (same path and soname).
- [x] **P1** `rtl-433` (`sdr/rtl-433`) · tool: a stack overflow in `proflame2_mc`, writing up to about 39 bytes into `b[7]` from RF input.
  - Evidence: `41_system__1058_rtl-433`, `proflame2.c:85:16: warning: stack-based buffer overflow [CWE-121]`.
  - Fix: ship `proflame2-bound.patch` (`if (f >= 7) return f;`) and apply it before cmake. Use `$version` in `source =` (S25).
- [x] **P1** `wv` (`office/wv`) · tool: `wvSummary` truncates GObject pointers to 32 bits, so it crashes on every document.
  - Evidence: `41_system__0954_wv:500`, `initialization of 'const GValue *' … from 'int' makes pointer from integer`.
  - Fix: ship `wvsummary-gsf-include.patch` (`#include <gsf/gsf-doc-meta-data.h>`) and add `-Werror=int-conversion`.
- [x] **P3** `libdvbpsi` (`video-libs/libdvbpsi`) · security: `dvbpsi_DecodeAACDr` (descriptor 0x7c) writes through the old pointer after `realloc`, and over-reads on short descriptors (`i_length` 0 reaches `p_data[0]`; with `i_length` 2 and the type bit set it reads `p_data[2]` and `i_info_length = i_length - 3` wraps to 255 for the memcpy). The TOT length shifts left where it should shift right. VLC calls none of these (its TS demux uses 16 other descriptor decoders, and the TOT generator is unused), so no shipped program reaches them. (PLAN-HEAD-17, PLAN-41A-01)
  - Evidence: `41_system__1139_libdvbpsi`, `dr_7c.c:262:38: warning: pointer 'p_decoded' may be used after 'realloc'` and `tot.c:529:40 … [-Woverflow]`.
  - Fix: `dr_7c-realloc.patch`: return NULL (freeing) before any `p_data` access when `i_length < 1`, or `i_length < 3` with the type bit set; `p_decoded = p_tmp` after the NULL check; `>> 8` in `tot.c`. The soname is unchanged, so vlc needs no rebuild.
- [x] **P2** `gnutls` (`crypto/gnutls`) · security: links tpm2-tss, which links OpenSSL 4, into every gnutls consumer (S16, Q28). (PLAN-HEAD-16, PLAN-42-43-14, PLAN-44-TAIL-08)
  - Evidence: `0156_gnutls:929` `dlopen works with SONAME... no` (musl folds libm into libc, so the probe finds no soname), `:1018` `TPM2 support: link`. `build.sh:19` `--with-tpm2`.
  - Fix: `gnutls_cv_dlopen_soname_works=yes --with-tpm2=dlopen --with-zlib=link --with-zstd=link --with-brotli=link` (the cache variable also flips those three to dlopen). Verify `readelf -d libgnutls.so.30` has no `libtss2-*` NEEDED and the configure SONAME checks for tss2 passed. Fallback: tpm2-tss `--with-crypto=mbed`.
- [x] **P2** `hamlib` (`hamradio/hamlib`) · musl: the forced `-include sys/types.h` breaks configure's select probe, so the microHAM backend (`src/microham.c`, the only `HAVE_SELECT` user) is a stub. (PLAN-41A-02)
  - Evidence: `41_system__0753_hamlib:135`, `checking for select... no`.
  - Fix: `ac_cv_func_select=yes ./configure …`, keeping the `-include` export. Guard `grep -q '^#define HAVE_SELECT 1' config.h`.
- [x] **P2** `libstdcxx-arm-none-eabi` (`embedded/libstdcxx-arm-none-eabi`) · hidden: C++23 `import std` compiles an empty module, and `libstdc++.modules.json` still advertises it.
  - Evidence: `41_system__0597_libstdcxx-arm-none-eabi`, `'acosl' has not been declared in 'std'` (638 times) and `stamp-modules-bits] Error 1 (ignored)`.
  - Fix: ship a patch defining `HAVE_*L` in the `--with-picolibc` branch of `configure.ac` and `configure` (Q15).
- [x] **P2** `gcc-riscv64-unknown-elf` (`embedded/gcc-riscv64-unknown-elf`) · newport: RISC-V bare-metal C++ has no libstdc++. (PLAN-41A-15)
  - Fix: the new port `libstdcxx-riscv64-unknown-elf`, cloned from the ARM one with the riscv target options, the same Q15 `HAVE_*L` patch and `group = gcc-riscv64-unknown-elf`; listed after picolibc-riscv64 in `embedded.txt`. Guard its log against `stamp-modules-bits] Error 1 (ignored)`.
- [x] **P2** `avr-libc` (`embedded/avr-libc`) · layout: `/usr/bin/avr-man` is broken (unsubstituted `$(VERSION)`, and docs are off).
  - Fix: `rm -f "$PKG/usr/bin/avr-man"`. Optional: the new port `fig2dev`.
- [x] **P2** `avr-libc` · tool: `ccp_write_spm` is assembled with a truncated CCP address on ATtiny102 and 104.
  - Evidence: `ccp_write_spm.S:18: Warning: operand out of range: 0x3c` (x2).
  - Fix: ship `ccp-write-spm-avrtiny.patch` (`out _SFR_IO_ADDR(CCP), r24` under `__AVR_TINY__`).
- [x] **P2** `bsd-games` (`games-board/bsd-games`) · tool: adventure's `linkdata()` reads and writes past `[LOCSIZ]`.
  - Evidence: `adventure/init.c:158:29: warning: iteration 140 invokes undefined behavior`.
  - Fix: ship `adventure-linkdata-bounds.patch` (`i < LOCSIZ`). The 0666 score file stays and is allowlisted (S20).
- [x] **P2** `aalib` (`image-libs/aalib`) · tool: the Linux-console keyboard driver passes a truncated ioctl pointer and runs a 256-byte memset into `key_down[128]`.
  - Evidence: `aalinuxkbd.c:135:37` and `aalinuxkbd.c:171:21 … writing 256 bytes into a region of size 128`.
  - Fix: a third patch: `key_down[NR_KEYS]`, and `&keyb_ent` with no cast at l.135 and l.144. Also patch the `AM_PATH_AALIB` underquoting (S15).
- [x] **P2** `id3lib` (`audio-codecs/id3lib`) · hidden: `aclocal` runs without `-I m4`, so libid3 is built without iconv and mangles non-ASCII text.
  - Evidence: `41_system__0263_id3lib:205`, `ID3_UNICODE: command not found`.
  - Fix: `aclocal -I m4`. Verify `nm -D libid3-3.8.so.3` shows `iconv_open`.
- [x] **P2** `libwebp` (`image-libs/libwebp`) · feat: a broken upstream configure test drops every AVX2 path, and no `WebPConfig.cmake` is installed, so openimageio loses WebP. (PLAN-42-43-09)
  - Evidence: `41_system__0814_libwebp:109`, `./configure: line 14870: -a: command not found`. 0 `ymm` instructions in `libwebp.so`.
  - Fix: switch to libwebp's CMake build (`-DWEBP_ENABLE_SIMD=ON -DBUILD_SHARED_LIBS=ON -DWEBP_BUILD_LIBWEBPMUX=ON -DWEBP_BUILD_EXTRAS=ON -DWEBP_BUILD_VWEBP=OFF`), keeping the sonames and `.pc` files. The autotools route installs no CMake config and is not an option. Use `https` for the source (S25).
- [x] **P2** `taglib` (`audio-codecs/taglib`) · dep: installs its bundled utfcpp, and utfcpp then takes the 10 paths (S5).
  - Evidence: `41_system__0812_utfcpp`, `Taking 10 paths from taglib`.
  - Fix: add `utfcpp` to depends and delete the comment (S17). Rebuild utfcpp, then taglib.
- [x] **P2** `toot` (`chat/toot`) · layout: bundles PySocks although `python3-pysocks` is a port; `socks.py` is owned twice. (PLAN-41A-03, PLAN-EARLY-16)
  - Fix: add `python3-pysocks` to depends; drop the `toot-vendor` source and the `vendoring`, `pyrequirements`, `pyruntime` and `pypackages` keys; install with `pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .`.
- [x] **P2** `khal` (`pim/khal`) · layout: vendors pyxdg, so 34 `xdg/` files are owned twice. (PLAN-41A-03)
  - Fix: `pypackages = icalendar tzdata tzlocal`, add `python3-pyxdg` to depends, regenerate the bundle, `ports/publish khal`, commit the `sources.idx` line.
- [x] **P3** `script/phases/41_system/packages.d/00-order.txt` · layout: l.78-97 force python3-pyxdg and python3-pysocks after khal and toot and justify the vendoring that the two items above remove. (PLAN-41A-03)
  - Fix: delete the two paragraphs and the forced placements in the same change; list both ports by shelf and let depends order them; check spd-conf still gets pyxdg through depends; run phaseclosure.
- [x] **P2** `sudo` (`auth/sudo`) · defaults: `visudo`'s default editor is the absent `/usr/bin/vi`, and env_reset drops `EDITOR`. (PLAN-41A-06)
  - Evidence: `41_system__0303_sudo:121`, `checking for vi... no`; `:457`, `default visudo editor :` (empty).
  - Fix: `--with-editor=/usr/bin/nvim:/usr/bin/nano` (the `bash.bashrc` order), `Defaults env_keep += "EDITOR VISUAL SUDO_EDITOR"` in `00-sudo`, and `chmod 440` on `00-sudo`. Update `configuration.md:1230` and `administration.md:294`. Lands with the CVE patch (S23).
- [x] **P2** `sox` (`audio-dsp/sox`) · dep: links libgsm, which is not in depends.
  - Fix: add `gsm` to depends and `--with-gsm=yes`.
- [x] **P2** `iproute2` (`network/iproute2`) · dep: `arpd` links libdb, which is not in depends.
  - Fix: add `db` to depends.
- [x] **P3** `gnuastro` (`astronomy/gnuastro`) · size: links the reference `libgslcblas`, which is complete; only BLAS-heavy speed differs. The `/usr/etc` install is S13. (PLAN-41A-13) — **closed:** premise false: gnuastro 0.24 hard-codes `-lgslcblas` (configure.ac:356) and has no BLAS option, so openblas needs a patch the item marks optional
  - Fix: optional `gnuastro-openblas.patch` (`configure.ac:356`) with openblas in depends.
- [x] **P2** `ghostscript` (`printing/ghostscript`) · layout: `/usr/bin/gs` duplicates the 36 MB interpreter already in `libgs.so`.
  - Fix: `make so; make DESTDIR=$PKG soinstall; ln -sf gsc $PKG/usr/bin/gs`.
- [x] **P2** `nettle` (`crypto/nettle`) · dwarf: `-ggdb3`.
  - Fix: `make CFLAGS="$CFLAGS"` on both make lines.
- [x] **P2** `jemalloc` (`devlibs/jemalloc`) · dwarf: about 5 MB of `-g3` in a 6.4 MB lib.
  - Fix: `export CFLAGS="${CFLAGS/-O2/-O3} -g0" CXXFLAGS="${CXXFLAGS/-O2/-O3} -g0"`.
- [x] **P2** `argon2` (`crypto/argon2`) · dwarf: the Makefile hard-codes `-g`.
  - Fix: `export LDFLAGS="$LDFLAGS -Wl,--strip-debug"`.
- [x] **P2** `verilator` (`eda/verilator`) · dwarf: `verilator_bin_dbg` is 226 MB, 176 MB of it DWARF.
  - Fix: `export LDFLAGS="$LDFLAGS -Wl,--strip-debug"` before configure.
- [x] **P2** `intel-gmmlib` (`gpu/intel-gmmlib`) · dwarf: 1.5 MB from `add_compile_options(-g)`.
  - Fix: `export LDFLAGS="$LDFLAGS -Wl,--strip-debug"`.
- [x] **P2** `libiptcdata` (`image-libs/libiptcdata`) · dwarf: configure appends `-g`.
  - Fix: `make CFLAGS="$CFLAGS"`.
- [x] **P2** `libfilezilla` (`transfer/libfilezilla`) · dwarf: configure appends `-g`.
  - Fix: `export LDFLAGS="$LDFLAGS -Wl,--strip-debug"`. The lib64 RPATH is S3.
- [x] **P2** `duckdb` (`database/duckdb`) · other: stamped `v0.0.1`, so extension paths are wrong.
  - Fix: `-DOVERRIDE_GIT_DESCRIBE=v$version-0-g0000000000`.
- [x] **P2** `duckdb` · musl: links the bundled jemalloc, which upstream excludes on musl.
  - Fix: `-DENABLE_JEMALLOC=OFF`. Its 21 linkable archives ship (Q21).
- [x] **P2** `bitwise` (`cli/bitwise`) · feat: readline declarations are misdetected, so the idle loop polls at 1 kHz.
  - Fix: `ac_cv_have_decl_rl_change_environment=yes ac_cv_have_decl_rl_input_available_hook=yes`.
- [x] **P2** `gnu-efi` (`boot/gnu-efi`) · layout: `gnu-efi.pc` names `prefix=/usr/local`.
  - Fix: `make PREFIX=/usr` and the same on install.
- [x] **P2** `diamond` (`bioscience/diamond`) · layout: statically links a private `libsqlite3.a`. Lands after S7. (PLAN-41A-11)
  - Fix: `-DSQLite3_LIBRARY=/usr/lib/libsqlite3.so` once sqlite has its SONAME; before that, linking by path records `/usr/lib/libsqlite3.so` as NEEDED. Guard that diamond NEEDs `libsqlite3.so.0`.
- [x] **P2** `nfs-utils` (`filesystem/nfs-utils`) · layout: `mount.nfs` is recorded as `./sbin/…`, and `--with-rpcgen=internal` installs `usr/bin/rpcgen`, taking it from rpcsvc-proto (S5). (PLAN-HEAD-05)
  - Fix: `--disable-sbin-override`; remove `usr/bin/rpcgen` and its man page from `$PKG`. Every archive, the plugin ones included, ships (Q21).
- [x] **P2** `xfconf` (`gtk/xfconf`) · layout: ships a systemd user unit (hard rule 3; S13).
  - Fix: `rm -rf "$PKG/usr/lib/systemd"`.
- [x] **P2** `tlf` (`hamradio/tlf`) · newport: no fldigi XML-RPC.
  - Fix: the new port `xmlrpc-c`, then `--enable-fldigi-xmlrpc`.
- [x] **P2** `groff` (`doctools/groff`) · newport: grohtml has no image or equation path (no netpbm).
  - Fix: the new port `netpbm` in depends; delete the comment (S17).
- [x] **P2** `picotool` (`embedded/picotool`) · feat **[S9]**: no signing or encryption, although the mbedtls 3.6.7 port exists.
  - Fix: add the mbedtls 3.6.7 tarball as a second source and pass `-DPICO_MBEDTLS_PATH="$SRC_ROOT/mbedtls-3.6.7"`.
- [x] **P2** `nco` (`sci-libs/nco`) · newport: no `ncap2` (needs ANTLR 2).
  - Fix: the new port `antlr2` (42_graphics, after openjdk), move nco to 42_graphics or later, then `--enable-ncap2`.
- [x] **P2** `limesuite` (`sdr-hw/limesuite`) · dep: LimeSuiteGUI is off because wxwidgets is in 43_toolkits. (PLAN-41A-05)
  - Fix: the split port `limesuite-gui` in 44_apps with `-DENABLE_LIBRARY=ON -DENABLE_GUI=ON -DENABLE_HEADERS=OFF -DENABLE_LIME_UTIL=OFF -DENABLE_QUICKTEST=OFF -DENABLE_SOAPY_LMS7=OFF -DENABLE_OCTAVE=OFF` (`ENABLE_GUI` depends on `ENABLE_LIBRARY`), depends wxwidgets limesuite; build the `LimeSuiteGUI` target and install only it with the desktop and icon files. Guard: no `libLimeSuite.so*` in `$PKG`, and LimeSuiteGUI NEEDs `libLimeSuite.so.23.11-1`.
- [x] **P2** `nmap` (`security/nmap`) · dep: no Zenmap, because python3-gobject is in 42_graphics.
  - Fix: the split port `zenmap` in 44_apps (pip from `./zenmap`, depends nmap python3-gobject gtk3).
- [x] **P2** `yara` (`security/yara`) · layout: `/usr/bin/yara` has an RPATH into the build dir (S12).
  - Fix: `make yara_LDFLAGS=`, and guard with `! readelf -d … | grep -q /var/cache`.
- [x] **P2** `sleuthkit` (`security/sleuthkit`) · layout: 28 tools have an RPATH into the build dir (S12). (PLAN-41A-04)
  - Fix: `make LDFLAGS="$LDFLAGS" sigfind_LDFLAGS=` on both the build and install lines (`Makefile.am:469` sets `sigfind_LDFLAGS = -static` per target). Guard every ELF in `$PKG` with `readelf -d`, failing on `/var/cache`.
- [x] **P2** `atuin` (`shells/atuin`) · dep: NEEDs libssl and libcrypto, but `depends = rust`; `build.sh:46-47` claims a static-pie with no NEEDED while l.21 sets `-crt-static`. (PLAN-41A-12)
  - Fix: add `openssl` to depends and delete the false comment.
- [x] **P2** `nbdkit` (`virt/nbdkit`) · feat **[S9]**: the iso plugin is off, though xorriso exists.
  - Fix: `--with-iso` and add `libisoburn` to depends.
- [x] **P2** `nss` (`crypto/nss`) · hardening: the 11 shared libraries link without the tree LDFLAGS (`coreconf/Linux.mk:154` `DSO_LDOPTS` never includes them). (HRD-07)
  - Fix: `DSO_LDOPTS="-shared -m64 -Wl,--gc-sections -Wl,-z,defs $LDFLAGS"` on the make line.
- [x] **P3** `ncdu` (`files/ncdu`) · hardening: `-Dstrip=true` ships it without `.symtab`, against Q2. (PLAN-EARLY-05)
  - Fix: drop `-Dstrip=true`; `strip --strip-debug` after install. Bump release.
- [x] **P3** `libnova` (`astronomy/libnova`) · hardening: `configure.in:66` sets `CFLAGS=-Wall`, so it compiles at `-O0` with none of the tree flags. (LOG-13)
  - Fix: `make CFLAGS="$CFLAGS -Wall"`; verify the compile lines carry `-O2` and `-fstack-clash-protection`.
- [x] **P3** `qhull` (`cad/qhull`) · hardening: `libqhull_r.so.8.0.2` has a relative `RPATH [lib]` (S12). (HRD-13)
  - Fix: `-DCMAKE_SKIP_RPATH=ON` (or clear `INSTALL_RPATH`).
- [x] **P2** `linux-firmware` (`boot/linux-firmware`) · metadata: ships WHENCE but none of the 131 `LICENSES/*` texts it points to; `install -m644 LICENSE* … 2>/dev/null || true` matches only the top-level files. Many firmware licences allow redistribution only with the text. (CRT-01)
  - Fix: `cp -r LICENSES "$PKG/usr/share/licenses/$name/"`, keeping `LICENSE*` and WHENCE; a build check fails when a `See LICEN[CS]E.*` name in WHENCE is missing from the installed directory.
- [x] **P3** `linux-firmware` · size: about 435 MB of firmware for hardware this x86 kernel cannot drive: arm64 Qualcomm SoC DSPs and GPUs (qcom 259 MB), superseded Mellanox Spectrum `.mfa2` images (98 MB) and Marvell prestera v2.0/v3.0 (72 MB). (IMG-07)
  - Fix: prune by an explicit list, never by a modinfo diff: `qaic` loads `qcom/aic100/fw*.bin` by image table, prestera falls back by format string, mlxsw line cards load `lc_ini_bundle_%u_%u.bin`. Keep `qcom/aic100`, `aic200` and what `mhi_pci_generic` names, mlxsw's referenced `.mfa2`, prestera v4.0 and v4.1. Document in `ports-catalogue.md` and `known-gaps.md`.
- [x] **P3** `avahi` (`net-libs/avahi`) · security: `ssh.service` and `sftp-ssh.service` advertise SSH on every network while the firewall drops port 22. (DEF-05)
  - Fix: `rm -f $PKG/etc/avahi/services/{ssh,sftp-ssh}.service`; optionally the firewall ssh toggle writes it when it opens the port. `/var/run` is S13. State in `security-model.md` (the ssh paragraph, l.983-988) that nothing advertises SSH while port 22 is closed.
- [x] **P3** `rapidjson` (`formats/rapidjson`) · metadata: ships `usr/include/rapidjson/reader.h.orig`; a patch applies with fuzz. (DEF-15)
  - Fix: refresh `gcc14.patch` or `CVE-2024-38517.patch` to apply cleanly (or `--no-backup-if-mismatch`). The S20 lint fails `*.orig`/`*.rej`.
- [x] **P3** `spacenavd` (new port) · newport: libspnav (42_graphics, `3d.txt:30`) has no daemon, so the openscad, freecad, kicad, horizon-eda and blender 3D-mouse paths never connect.
  - Fix: the new port `spacenavd` (input, 41_system) with an `fs/etc/init.d` service; document it in `administration.md`.
- [x] **P3** · feat/dep **[S9]**: features off on stale claims or missing `depends` (one row each). (PLAN-41B-08, PLAN-41A-07, PLAN-41B-10, PLAN-41B-16)

  | ☐ | Port | Path | Evidence | Fix | Dep |
  |---|---|---|---|---|---|
  | [x] | rubberband | `audio-dsp/rubberband` | `0267`: `LV2 plugin : NO` | `-Dlv2=enabled -Dladspa=enabled`, add lv2 ladspa to depends; `$version` in `source =` | exist |
  | [x] | ceres-solver | `math/ceres-solver` | `0877`: `Building without SuiteSparse.` | `-DSUITESPARSE=ON -DLAPACK=ON`, add suitesparse openblas to depends | exist |
  | [x] | upower | `hardware/upower` | `0789`: `idevice disabled` | `-Didevice=enabled`, add libimobiledevice to depends | exists |
  | [x] | libheif | `image-libs/libheif` | `0824`: `OpenH264 decoder : - disabled` | `-DWITH_OpenH264_DECODER=ON`, add openh264 to depends; lands with 1.23.6 (S23) | exists |
  | [x] | fuse | `filesystem/fuse` | `0211:39`: `liburing found: NO` | `-Denable-io-uring=true`, add liburing numactl to depends, guard with readelf | exist |
  | [x] | libtiff | `image-libs/libtiff` | `0225:66`: `Could NOT find JBIG` | `-Djbig=ON -DCMAKE_REQUIRE_FIND_PACKAGE_JBIG=ON`, add jbigkit to depends | exists |
  | [x] | vorbis-tools | `audio-codecs/vorbis-tools` | `0258`: `Speex libraries … missing` | drop `--without-speex`, add speex to depends; `--sysconfdir=/etc` (S13) | exists |
  | [x] | rizin | `devtools/rizin` | `0493`: `System libmspack library : NO` | `-Duse_sys_libmspack=enabled`, add libmspack to depends | exists |
  | [x] | nss | `crypto/nss` | `libnssckbi.so` is NSS's own builtin roots | `ln -sf pkcs11/p11-kit-trust.so $PKG/usr/lib/libnssckbi.so`, add p11-kit to depends; update `security-model.md`; drop `libnssckbi-testlib.so` | exists |
  | [x] | libnbd | `virt/libnbd` | `1151:301`: `Bash tab completion … no`; `--disable-ublk` | `--with-bash-completions`, add bash-completion to depends; `--enable-ublk` with liburing and ubdsrv in depends | exist / new port |
  | [x] | nbdkit | `virt/nbdkit` | `1152:470`: `bash-completion … no` | the same completions fix | exists |
  | [x] | libjpeg-turbo | `image-libs/libjpeg-turbo` | `0224:68`: `System-supplied zlib disabled` | `-DWITH_SYSTEM_ZLIB=ON -DWITH_TESTS=OFF`, drop `CMAKE_CXX_FLAGS_RELEASE`, depends: yasm→nasm, add zlib; optional new port libspng | exists |
  | [x] | libxlsxwriter | `office/libxlsxwriter` | `0948`: bundled `third_party/zip.h` | `-DUSE_SYSTEM_MINIZIP=ON`, add minizip to depends | exists |
  | [x] | pciutils | `hardware/pciutils` | `pci.ids.gz` 2026.04.05 shadows hwdata 2026.09.03 | `rm -f $PKG/usr/share/hwdata/pci.ids.gz`, add hwdata to depends | exists |
  | [x] | x265 | `video-libs/x265` | comment says libnuma is not a port | delete the comment (S17); drop the unused `CMAKE_INSTALL_LIBDIR` | numactl exists |
  | [x] | tlp | `hardware/tlp` | comment says PyGObject is not a port | delete the comment (S17) | exists (42) |
  | [x] | enchant | `spelling/enchant` | comment says groff is not a port | delete the comment (S17) | exists |
  | [x] | dcmtk | `bioscience/dcmtk` | comment says libsndfile is not a port; `BUILD_TESTING` unused | delete the comment (S17); drop the var; add a shebang | exists |
  | [x] | glib-networking | `net-libs/glib-networking` | comment says no proxy schema exists; `gsettings-desktop-schemas` ships it | delete the comment (S17) | — |
  | [x] | libsoup3 | `net-libs/libsoup3` | `build.sh:21`: NTLM off because "`ntlm_auth` … is not a port"; "nothing here is written in Vala" | `-Dntlm=enabled -Dntlm_auth=/usr/bin/ntlm_auth` (db/samba owns it), samba in depends; delete both comments (S17); lands with 3.8.0 (S23). Both are in 41_system and samba's depends closure does not reach libsoup3 (measured), so there is no cycle; every libsoup3 consumer (webkitgtk, gvfs, geoclue, …) then pulls samba (Q41) | samba exists (41) |
  | [x] | argon2 | `crypto/argon2` | `0448`: `Building without optimizations` | `OPTTARGET=x86-64`; delete the comment (S17) | — |
  | [x] | stlink | `embedded/stlink` | `0593`: `GTK3 not found!` | the split port `stlink-gui` in 44_apps (Q11) | gtk3 is in 43 |
  | [x] | v4l-utils | `media-frameworks/v4l-utils` | `0879`: `qv4l2 : NO` | the split port `qv4l2` in 44_apps with `-Dv4l-utils=true -Dqv4l2=enabled -Dqvidcap=enabled`, installing only the GUI files (Q11) | qt6 is in 43 |
- [x] **P3** `xfsprogs` (`filesystem/xfsprogs`) · layout: libhandle installs to `/usr/lib64` (S3), and state to `/usr/var` (S13).
  - Fix: `--enable-lib64=no --localstatedir=/var`.
- [x] **P3** `gimp-help` (`graphics/gimp-help`) · security: 1,981 help files ship 0666 (tarball modes, S20); the quick-reference PDF install fails because rsvg-convert is absent, which Q8 makes irrelevant. (PLAN-HEAD-03, PLAN-41B-03)
  - Fix: `chmod -R u=rwX,go=rX "$PKG/usr/share/gimp"` until S20's kpkg fix lands. gimp-help stays in 41_system with no librsvg; the PDF stays off.
- [x] **P3** `jq` (`formats/jq`) · layout: ships its vendored oniguruma as a public library.
  - Fix: the new port `oniguruma` and `--with-oniguruma=yes`.
- [x] **P3** `openssh` (`servers/openssh`) · newport: no xauth, so X11 forwarding is broken; `sshd` records `/usr/X11R6/bin/xauth`. (PLAN-44-TAIL-12)
  - Fix: `--with-xauth=/usr/bin/xauth` with no depends entry; xauth is a new 42_graphics port (New ports table). Verify with `strings` on sshd. Lands with 10.6p1 (S23).
- [x] **P3** `sdcc` (`embedded/sdcc`) · newport: no PIC back end, yet the description says PIC; takes 12 paths from binutils (S5).
  - Fix: the new port `gputils`, then enable pic14/pic16. Remove the binutils-owned info files sdcc installs.
- [x] **P3** `ebook-tools` (`documents/ebook-tools`) · layout: `lit2epub` needs `clit`, which does not exist.
  - Fix: the new port `convertlit` in depends.
- [x] **P3** `lirc` (`hardware/lirc`) · newport: no libusb-0.1 plugins.
  - Fix: the new port `libusb-compat`, then enable the USB plugins.
- [x] **P2** `graphicsmagick` (`image-libs/graphicsmagick`) · newport: no JPEG-2000, and `openjpeg` in depends is useless.
  - Fix: the new port `jasper`; replace openjpeg with jasper in depends; guard on the libjasper NEEDED. The `.pc` Libs leak is S11.
- [x] **P3** `yices2` (`math/yices2`) · newport: MCSAT is off.
  - Fix: the new ports `libpoly` and `cudd`, then `--enable-mcsat`.
- [x] **P3** `sleuthkit` · newport: no VMDK, VHD, LVM or AFF support.
  - Fix: the new ports `libbfio`, `libvhdi`, `libvmdk`, `libvslvm` and `afflib`.
- [x] **P3** `uhd` (`sdr-hw/uhd`) · dep: `rfnoc_utils` imports ruamel.yaml, present only through khard.
  - Fix: the new port `python3-ruamel-yaml` in uhd's depends, removed from khard's `pypackages`.
- [x] **P3** `procps-ng` (`base/procps-ng`) · musl: `w`, `who`, `uptime` and `last` show no sessions: nothing writes utmp, and musl's utmp calls are stubs. (PLAN-41B-13, PLAN-44-TAIL-14)
  - Fix (Q39): skalibs and utmps in 30_foundation (S18), execline and s6 in 41_system. `utmps-utmpd` and `utmps-wtmpd` run under `s6-svscan`, started by an `fs/etc/init.d` script ahead of the first getty. Writers (sysvinit, kdos-getty/login, shadow, util-linux) and readers (procps-ng, util-linux, coreutils) build against utmps (`-I/usr/include/utmps -lutmps -lskarnet`). Rewrite `boot-and-init.md:1359` and document the two services in `boot-and-init.md` and `daemons.md`.
  - Verify: on the rig, `who` lists the tty1 session and `last` lists the boot.
- [x] **P3** `ndctl` (`disk/ndctl`) · musl: the XSI `strerror_r` int is passed to `%s`.
  - Fix: ship a patch using `strerror(errno)` at l.56 and l.68.
- [x] **P3** `samba` (`servers/samba`) · musl: the netlink probe includes `<bits/sockaddr.h>`.
  - Fix: ship `musl-netlink.patch` (`<sys/socket.h>`).
- [x] **P3** `xa` (`embedded/xa`) · tool: off-by-one in `binfnam[255]`, and misc tools lack LDFLAGS.
  - Fix: ship `binfnam-bound.patch`, and pass `CFLAGS="$CFLAGS $LDFLAGS"` for misc/.
- [x] **P3** `onnxruntime` (`ai/onnxruntime`) · tool: the pybind module has an RWE GNU_STACK (S12).
  - Fix: `export ASMFLAGS="-Wa,--noexecstack" LDFLAGS="$LDFLAGS -Wl,-z,noexecstack"`.
- [x] **P3** `perf` (`devtools/perf`) · layout and dwarf: man-page asciidoc errors, tools/lib DWARF (`-ggdb3`), `etc/bash_completion.d/perf`.
  - Fix: one `perf-docs.patch` (perf-check underline 13 `=`, perf-kwork l.186 underline 28 `-`); `export LDFLAGS="$LDFLAGS -Wl,--strip-debug"` in the environment; move the completion to `/usr/share/bash-completion/completions/perf`.
- [x] **P3** `ngspice` (`eda/ngspice`) · dwarf: hicum2 adds `-g`, about 1 MB each.
  - Fix: `export CFLAGS="$CFLAGS -g0" CXXFLAGS="$CXXFLAGS -g0"`.
- [x] **P3** `neovim` (`editors/neovim`) · dwarf: about 2.6 MB from the bundled LuaJIT `CCDEBUG+=-g`.
  - Fix: `export LDFLAGS="$LDFLAGS -Wl,--strip-debug"`.
- [x] **P3** `valkey` (`database/valkey`) · dwarf: libvalkey `-g -ggdb`.
  - Fix: `export LDFLAGS="$LDFLAGS -Wl,--strip-debug"`.
- [x] **P3** `taskwarrior` (`pim/taskwarrior`) · dwarf: ring's C objects are built with `-g3`.
  - Fix: `export LDFLAGS="$LDFLAGS -Wl,--strip-debug"`.
- [x] **P3** `tzdata` (`base/tzdata`) · hidden: no `tzdata.zi`, so C++ chrono falls back to GCC's embedded copy.
  - Fix: `make … VERSION=$version … tzdata.zi`, then install it to `/usr/share/zoneinfo/`.
- [x] **P3** `micro` (`editors/micro`) · hidden: version `0.0.0-unknown`.
  - Fix: `make build VERSION=$version HASH=`.
- [x] **P3** `mbpoll` (`eda/mbpoll`) · other: version `1.0-0`.
  - Fix: ship a GitVersion.cmake patch plus `-DMBPOLL_VERSION_OVERRIDE=$version`.
- [x] **P3** `admesh` (`3d/admesh`) · hidden: empty `AUTHORS` and `ChangeLog`.
  - Fix: `rm` both from `$PKG/usr/share/doc/admesh`.
- [x] **P3** `matio` (`sci-libs/matio`) · hidden: the info manual is never built.
  - Fix: `makeinfo -I documentation -o documentation/matio.info documentation/matio_user_guide.texi`, and add texinfo to depends.
- [x] **P3** `pari` (`math/pari`) · hidden and hyg: a masked TeX failure, no BIND_NOW, and RPATH `/usr/lib` on gp.
  - Fix: `make gp`, then `make DESTDIR=$PKG RUNPTH_FINAL= install-doctex install-nodata install-data`, and `DLLDFLAGS="$LDFLAGS" ./Configure`.
- [x] **P3** `stfl` (`toolkits/stfl`) · hardening: no BIND_NOW.
  - Fix: `LDLIBS="-Wl,-z,now,-z,pack-relative-relocs -lncursesw"` on both make lines. `libstfl.a` ships (Q8).
- [x] **P3** `libretro-nestopia` (`libretro/libretro-nestopia`) · hidden: the version suffix is empty.
  - Fix: `make … GIT_VERSION=" $version"`.
- [x] **P3** `dhcpcd` (`network/dhcpcd`) · dep: the NTP hook depends on build order (chronyd detection). (PLAN-41B-05)
  - Fix: `--with-hooks=ntp.conf` (an empty value installs no NTP hook). Verify the log shows `HOOKSCRIPTS = 50-ntp.conf`.
- [x] **P3** `libgpod` (`mobile/libgpod`) · layout: `.mo` files land in `/usr/lib/locale`, and a dangling `libgpod-sharp.pc` ships.
  - Fix: `make DATADIRNAME=share` and the same on install. `rm $PKG/usr/lib/pkgconfig/libgpod-sharp.pc`.
- [x] **P3** `units` (`math/units`) · layout: installs into `/usr/com`.
  - Fix: `--sharedstatedir=/usr/share`.
- [x] **P3** `lm-sensors` (`hardware/lm-sensors`) · hidden: `/sbin/ldconfig` errors.
  - Fix: `LIBICONV=` on both make lines.
- [x] **P3** `cfitsio` (`astronomy/cfitsio`) · layout: `cookbook`, `smem` and `speed` land in `/usr/bin`.
  - Fix: `rm -f` them.
- [x] **P3** `conmon` (`containers/conmon`) · layout: a CRI-O copy ships.
  - Fix: drop `install.crio`.
- [x] **P3** `miniupnpc` (`net-libs/miniupnpc`) · layout: `external-ip.sh` calls the unbuilt `upnpc`.
  - Fix: `rm -f` it.
- [x] **P3** `timewarrior` (`pim/timewarrior`) · layout: its private library's 24 headers land in `/usr/include` under generic names (`format.h`, `utf8.h`, `Log.h`, `JSON.h`, …); no other port owns those names today.
  - Fix: `rm -rf "$PKG/usr/include"`. `libshared.a` ships (Q21).
- [x] **P3** `xapian-core` (`database/xapian-core`) · layout: 7.6 MB of apidoc.
  - Fix: `rm -rf .../apidoc`.
- [x] **P3** `wcslib` (`astronomy/wcslib`) · layout: 9.2 MB of docs and `-L/usr/local/lib`.
  - Fix: `rm` the docs; `make LDFLAGS="$LDFLAGS"`; `$version` in `source =`.
- [x] **P3** `libtheora` (`video-libs/libtheora`) · layout: prebuilt doxygen HTML and LaTeX ship.
  - Fix: `rm -r` html, latex and the stamp.
- [x] **P3** `ouch` (`archiver/ouch`) · feat: the generated completions are not installed.
  - Fix: install the bash, zsh and fish completions from `artifacts/`.
- [x] **P3** `minizip-ng` (`archiver/minizip-ng`) · feat: no zstd (method 93). (PLAN-41B-06)
  - Fix: ship a patch falling back to `pkg_check_modules(libzstd)` when `find_package(zstd CONFIG)` fails, `-DMZ_ZSTD=ON`, zstd in depends, guard NEEDED libzstd. zstd stays on make (a CMake zstd closes zstd→cmake→libarchive→zstd); pdal unchanged.
- [x] **P3** `gavl` (`video-libs/gavl`) · tool: global `-Wno-*` flags, plus upstream's `-mfpmath=387 -ffast-math`.
  - Fix: drop the export and use `make SUBDIRS="gavl include"`. Optional `configure.ac` patch for the 387 flag.
- [x] **P3** `aa3d` (`graphics/aa3d`) · tool: replace `-Wno-implicit-function-declaration` with `-include string.h`.
- [x] **P3** `rizin` · net: no sigdb. Add a pinned second source and `-Dinstall_sigdb=true`.
- [x] **P3** `libmirisdr`, `libfobos` (`sdr-hw/…`) · layout: empty `Version:` (S11).
  - Fix: libmirisdr gets a version `.patch`; libfobos gets `-DLIBVER=2.4.1`. Guard both with `grep -q '^Version: .'`.
- [x] **P3** `libvolk` (`sdr/libvolk`) · layout: legacy `.pyc`/`.pyo` ship. Fix: `rm -f …/volk_modtool/*.py[co]`.
- [x] **P3** `byobu`, `ccid`, `opensc` · linkage: dangling runtime references (S24). (LNK-12, UX-10)
  - byobu: `us.kirkland.terminals.byobu.service` execs the absent `/usr/lib/gnome-terminal/gnome-terminal-server`: remove it from `$PKG`.
  - ccid: `92_pcscd_ccid.rules:63` RUNs the absent `/usr/sbin/Kobil_mIDentity_switch`: drop the rule line through a `.patch` or remove the rule from `$PKG`.
  - opensc: `org.opensc.notify.desktop` execs the absent `opensc-notify`: remove it from `$PKG`.
- [x] **P2** `noto-cjk` (`fonts/noto-cjk`) · defaults: Japanese and Chinese text resolve to the Korean face (`fc-match sans-serif:lang=ja` → `Noto Sans CJK KR`), so Han characters draw with Korean glyph forms; the recipe installs only the `.ttc`. (UX-02)
  - Fix: ship `70-noto-cjk.conf` in `usr/share/fontconfig/conf.avail` with a `conf.d` link: per-lang rules prepending the JP, SC, TC, HK or KR Sans, Serif and Mono faces. Delete the "nothing to configure" comment. Verify with an `fc-match` loop per language.
- [x] **P3** `webp-pixbuf-loader` (new port) · ux: gdk-pixbuf has no WebP loader (15 loaders in `loaders.cache`, no webp), so GTK apps cannot show `image/webp`. (UX-14)
  - Fix: the new port (meson, image-libs, depends libwebp gdk-pixbuf) in 41_system after gdk-pixbuf; the pixbuf trigger regenerates `loaders.cache`.
- [x] **P3** · hyg: unused or unrecognised flags. Remove each. (PLAN-41B-11)

  | ☐ | Port | Path | Flag |
  |---|---|---|---|
  | [x] | zopfli | `archiver/zopfli` | `-DBUILD_TESTING=OFF` |
  | [x] | soundtouch, libsbsms | `audio-dsp/…` | `-DBUILD_TESTING=OFF` |
  | [x] | ldacbt | `audio-codecs/ldacbt` | `-DCMAKE_INSTALL_LIBDIR=lib` |
  | [x] | direwolf | `hamradio/direwolf` | `-DCMAKE_INSTALL_LIBDIR=lib` |
  | [x] | libharu | `documents/libharu` | `-DBUILD_TESTING=OFF` |
  | [x] | srecord | `embedded/srecord` | `-DBUILD_TESTING=OFF` |
  | [x] | portmidi, rtmidi | `music-libs/…` | `-DBUILD_TESTING=OFF` |
  | [x] | tcl | `lang/tcl` | `--disable-static` (unrecognised); delete the comment (S17) |
  | [x] | htslib | `bioscience/htslib` | `--disable-static` (no-op); `libhts.a` ships |
  | [x] | ethtool | `network/ethtool` | `--disable-static` |
  | [x] | bdftopcf | `fonts/bdftopcf` | `--disable-devel-docs` |
  | [x] | xcb-proto, xtrans | `x11/…` | `--disable-static` |
  | [x] | zsh | `shells/zsh` | `--with-tcsetpgrp` |
  | [x] | cpp-utilities | `qt-extra/cpp-utilities` | `-D NO_SPHINX=ON` |
  | [x] | libiio | `sdr-hw/libiio` | `WITH_SYSTEMD`, `WITH_SYSVINIT`, `WITH_UPSTART` |
  | [x] | highs | `math/highs` | `-DPYTHON_BUILD_SETUP=OFF` |
  | [x] | nlopt | `math/nlopt` | add `-DCMAKE_SKIP_INSTALL_RPATH=ON` |
  | [x] | airspy, airspyhf, libmed, libmediainfo, mediainfo, opentimelineio, qhull, thrift | `sdr-hw/airspy`, `sdr-hw/airspyhf`, `sci-libs/libmed`, `media-frameworks/{libmediainfo,mediainfo,opentimelineio}`, `cad/qhull`, `formats/thrift` | `-DCMAKE_INSTALL_LIBDIR=lib` |
  | [x] | json-c | `formats/json-c` | `CMAKE_CXX_FLAGS_RELEASE` |
  | [x] | libcec, xerces-c | `hardware/libcec`, `formats/xerces-c` | `-DBUILD_TESTING=OFF` |
  | [x] | prjtrellis | `eda/prjtrellis` | `CMAKE_POLICY_DEFAULT_CMP0167` |

  libvolk keeps `FETCHCONTENT_FULLY_DISCONNECTED=ON`: it is a network guard.

Every static archive in this phase ships (Q8, Q21); the per-port `--disable-static` table is withdrawn (PLAN-41B-01).

Withdrawn from 41_system: the exiv2, powertop and postgresql NLS rows (they contradict Q8; PLAN-41A-08, LNK-15); `libvpx` (toybox evidence, S18), the qqwing row (S3) and `speex` (output correct) (PLAN-41B-15); the nmap config.guess row (Q42, S15); the archive removals in clamav, crun and voacapl (Q21, PLAN-41A-09); the libevent shebang (Q24, S21).

## 42_graphics

- [x] **P1** `intel-media-driver` (`gpu/intel-media-driver`) · dwarf: a 594 MB `iHD_drv_video.so`, 520-546 MB of it DWARF.
  - Evidence: `42_graphics__0472_intel-media-driver`, `Packaged intel-media-driver: 568 MB`.
  - Fix: `export LDFLAGS="$LDFLAGS -Wl,--strip-debug"` before cmake.
- [x] **P2** `pipewire` (`audio-io/pipewire`) · feat: no JACK library exists (`-Djack=disabled -Dpipewire-jack=disabled`, `build.sh:109-110`), so ardour, hydrogen and lmms have no JACK backend. The `build.sh:35-38` comment names the trap: once `jack.pc` is installed, every consumer that leaves JACK on `auto` (ffmpeg, mpv, sdl3, mpd, portaudio) gains it or not by build order. (LNK-10)
  - Fix (Q38): `-Dpipewire-jack=enabled -Djack-devel=true -Dlibjack-path=/usr/lib`, so `libjack.so.0` and `jack.pc` are on the default paths. ffmpeg, mpv, sdl3 and mpd (all 42_graphics) take pipewire in depends and turn JACK on by name. portaudio is in 41_system (`audio-io.txt:34`), ahead of pipewire: move it to `42_graphics/packages.d/audio-io.txt` after pipewire with JACK on by name and pipewire in depends; any 41_system consumer of portaudio moves with it as phaseclosure requires. ardour, hydrogen and lmms turn JACK on (44_apps). Delete the `build.sh:35-38` comment (S17); state the constraint and JACK in `03-architecture/session.md` (audio). Re-check `kdos-appbox.md:562` (`rt-media` points `LD_LIBRARY_PATH` at PipeWire's JACK library) and its profile once `libjack.so.0` is in `/usr/lib`.
  - Verify: `readelf -d` on ffmpeg's `libavdevice.so` NEEDs `libjack.so.0`; `pw-jack` runs ardour.
- [x] **P2** `openimageio` (`image-libs/openimageio`) · dep: the WebP plugin is not built, because libwebp has no CMake config; Blender links libwebp directly, so oiiotool and OIIO readers lose WebP. (PLAN-42-43-09)
  - Evidence: `0521:112`, `WebP plugin will not be built`.
  - Fix: after libwebp moves to CMake (41_system), `-DCMAKE_REQUIRE_FIND_PACKAGE_WebP=ON`; drop the unused `-DUSE_OPENGL`.
- [x] **P2** `gst-plugins-base` (`media-frameworks/gst-plugins-base`) · feat: Theora and gl-graphene are off. The gl-headers clone is S15's meson wrapper. (PLAN-42-43-07)
  - Evidence: `feature theora disabled`; `-Dgl-graphene=disabled`.
  - Fix: `-Dtheora=enabled` with libtheora, `-Dgl-graphene=enabled` with graphene, in depends. Lands with 1.28.8 (S23).
- [x] **P2** `harfbuzz` (`graphics-libs/harfbuzz`) · newport: no Graphite, so libreoffice and texlive bundle their own harfbuzz; their comments also claim the port lacks ICU, which it has (`-D icu=enabled`, `libharfbuzz-icu.so.0` ships). (PLAN-42-43-10)
  - Fix: the new port `graphite2`, then `-D graphite2=enabled`. Unbundle libreoffice (`--with-system-harfbuzz --with-system-graphite`) and texlive; delete the harfbuzz:13, libreoffice:60-62 and texlive:22-23 comments (S17). Lands with 14.6.0 (S23).
- [x] **P2** `opensubdiv` (`3d/opensubdiv`) · layout: the CMake config lands in `/usr/lib64` (S3).
  - Fix: `-DCMAKE_INSTALL_LIBDIR=lib`. The glew comment is S9.
- [x] **P2** `openbabel` (`bioscience/openbabel`) · feat: no XTC format (no `rpc/xdr.h` on musl). (PLAN-42-43-13)
  - Evidence: `0344:40`, `Looking for xdrstdio_create - not found`.
  - Fix: libtirpc in depends; `-DCMAKE_REQUIRED_INCLUDES=/usr/include/tirpc -DCMAKE_REQUIRED_LIBRARIES=tirpc -DXDR_LIBRARY=tirpc` and `-I/usr/include/tirpc` in CXXFLAGS (`src/formats/CMakeLists.txt:26` links `${XDR_LIBRARY}` into each format plugin only). Drop the unused `CMAKE_DISABLE_FIND_PACKAGE_wxWidgets`.
- [x] **P2** `libgd` (`image-libs/libgd`) · feat **[S9]**: `--without-avif`.
  - Fix: `--with-avif` and add libavif to depends.
- [x] **P3** `R` (`lang/R`) · dwarf: Fortran gets the default `-g -O2` (0.5 MB), and Makeconf records it. Fixed by S15's `FFLAGS`/`FCFLAGS` export. (PLAN-42-43-06)
- [x] **P2** `R` · linkage: `libR.so` is only in `/usr/lib/R/lib`, so cantor's R backend (`cantor_rserver`, NEEDED `libR.so`, no RPATH) cannot start. (LNK-04)
  - Fix: R installs `usr/lib/libR.so -> R/lib/libR.so`; bump R. Rig-check that `cantor_rserver` finds `R_HOME`.
- [x] **P2** `fluidsynth` (`music-libs/fluidsynth`) · feat **[S9]**: LADSPA is off ("no port").
  - Fix: `-Denable-ladspa=ON` and add ladspa to depends.
- [x] **P2** `appstream` (`xdg/appstream`) · dep **[S9]**: stemming is off ("libstemmer not a port").
  - Fix: `-Dstemming=true` and add snowball to depends.
- [x] **P2** `pandas`, `sympy`, `scipy`, `astropy`, `numpy`, `matplotlib` · size: about 150 MB of test suites ship (Q23). (PLAN-42-43-D2)
  - Fix: `rm -rf` the `tests` directories in each recipe. astropy keeps `astropy/tests` (`__init__.py:182` imports `tests.runner`) and loses only the nested ones. twisted and samba keep theirs (imported by other code).
  - Verify: `python3 -c 'import astropy, pandas, scipy, sympy, numpy, matplotlib'`.
- [x] **P2** `koreader` (`documents/koreader`) · hardening: the bundled stack (mupdf, crengine, djvu, libarchive, freetype, its own libcrypto/libssl; 45 objects) gets no tree flags; `base/Makefile.defs:632-637` builds CFLAGS and LDFLAGS from its own variables. It parses untrusted EPUB, PDF and DjVu. (HRD-08)
  - Fix: `QFLAGS="-fstack-clash-protection" COMPAT_LDFLAGS="-Wl,-z,now"` on the make line (`build.sh:8`). Check `readelf` on `libs/libwrap-mupdf.so`.
- [x] **P3** `koreader` · layout: an 11 MB `spec/` test suite ships. Fix: `rm -rf "$PKG/usr/lib/koreader/spec"`.
- [x] **P3** `wvkbd` (`input/wvkbd`) · dwarf: `config.mk` adds `-g`. Fix: `-Wl,--strip-debug` on the CC line or in LDFLAGS.
- [x] **P3** `at-spi2-core` (`accessibility/at-spi2-core`) · dep: XRes depends on build order. Fix: add libXres to depends.
- [x] **P3** `manifold` (`3d/manifold`) · layout: redundant RPATH `/usr/lib`. Fix: `-DCMAKE_SKIP_INSTALL_RPATH=ON`.
- [x] **P3** `freerdp` (`virt/freerdp`) · layout: RPATH `$ORIGIN/../lib`. Fix: `-DCMAKE_SKIP_INSTALL_RPATH=ON`.
- [x] **P3** `hwloc` (`hardware/hwloc`) · ux: `lstopo.desktop` has `Icon=hwloc` and no such icon is installed; its two PDFs are S14's. (LNK-12, UX-10, PLAN-42-43-04)
  - Fix: install the upstream icon or set an existing name in `$PKG`.
- [x] **P3** `mesa` (`gpu/mesa`) · hyg: `-D glvnd=true` is deprecated. Fix: `-D glvnd=enabled`.
- [x] **P3** `kbd` (`base/kbd`) · layout: a stray `/usr/etc/pam.d/vlock` (S13). Fix: `--sysconfdir=/etc`.
- [x] **P3** `fwupd` (`boot/fwupd`) · layout: `/etc/grub.d/35_fwupd` ships. Fix: `rm -rf "$PKG/etc/grub.d"`.
- [x] **P3** `openal-soft` (`audio-io/openal-soft`) · other: the comment claims the UHJ tools ship. Fix: install `uhjdecoder`, `uhjencoder` and `sofa-info`.
- [x] **P3** `lasem` (`graphics-libs/lasem`) · layout: `/usr/doc` (S13). Fix: `make DESTDIR=$PKG lasemdocdir=/usr/share/doc/lasem install`.
- [x] **P3** `sdl2-gfx` (`game-libs/sdl2-gfx`) · hyg: make reruns autoconf through `missing`. Fix: `autoreconf -fi` before configure.
- [x] **P3** `ocrmypdf` (`printing/ocrmypdf`) · layout: the db lists `usr/bin/__pycache__`, which never reaches the image. Covered by S10. (PLAN-42-43-08)
- [x] **P3** `coin` (`cad/coin`) · hyg: unused `BUILD_TESTING`. Fix: drop it.
- [x] **P3** `multimon-ng` (`sdr/multimon-ng`) · hyg: unused `CMAKE_INSTALL_LIBDIR`. Fix: drop it.
- [x] **P3** · hyg: unused CMake variables (S15). Drop each. (PLAN-42-43-11)

  | ☐ | Port | Variable |
  |---|---|---|
  | [x] | opencsg | `BUILD_TESTING` |
  | [x] | vtk | `VTK_MODULE_USE_EXTERNAL_VTK_cli11`, `VTK_MODULE_USE_EXTERNAL_VTK_scn` |
  | [x] | gl2ps | `CMAKE_INSTALL_LIBDIR` |
  | [x] | piper | `Python_EXECUTABLE` |

## 43_toolkits

- [x] **P2** Qt 5 consumers · security: Qt 5.15.19 has no public 2026 CVE patches, and 16 ports link it: candle2, gnuradio, keepassxc, librecad, lmms, mumble, python3-pyqt5, qca-qt5, qsstv, qt5ct, quassel, qwt-qt5, smplayer, sqlitebrowser, vlc, wsjtx (measured from `depends`). (DEC-01)
  - Fix (Q26): keep Qt 5. Move keepassxc, mumble, quassel, vlc, smplayer and every other consumer whose pinned or next stable release builds with Qt 6 to that build (smplayer: its Qt 6 path does not compile today (`build.sh:17-18`: `QTextStream::setCodec`, `QRegExp`); move it with a `.patch` if the fix is small, otherwise list it as a Q26 exception for the user. vlc: 3.0.24 has a Qt 5 interface only and no stable release has a Qt 6 one; it stays on Qt 5 and is listed with that reason, flagged to the user because Q26 named it). A moved port drops its S16 item. The ports that stay are listed in `ports-catalogue.md` and `known-gaps.md` with the reason.
  - Verify: `readelf -d` of each moved binary NEEDs no `libQt5*`.
- [x] **P1** `qca-qt5` (`crypto/qca-qt5`) · security: the ossl plugin links OpenSSL 4 into Qt 5 processes that link OpenSSL 3 (S16).
  - Evidence: `0359`, `Found OpenSSL … "4.0.2"`. `libqca-ossl.so` NEEDs `libssl.so.4`; `libQt5Network.so.5` NEEDs `libssl.so.3`.
  - Fix: build against the openssl3 slot as qt5-qtbase does (generated headers, `-DOPENSSL_*` pointing at `.so.3`), openssl3 in depends. Delete the comment (S17). quassel is its only consumer (`ports/core/chat/quassel/kpkgbuild:18`; mumble does not depend on it) and moves to Qt 6 (Q26), so qca-qt5 is kept as a Qt 5 platform library (Q26 keeps Qt 5) and joins the consumerless list in `ports-catalogue.md` (Q51 becomes 16).
- [x] **P1** `python3-wxpython` (`python-gui/python3-wxpython`) · dwarf: 383 MB package, `_core` alone 193 MB.
  - Fix: `export LDFLAGS="$LDFLAGS -Wl,--strip-debug"`.
- [x] **P2** `avogadrolibs` (`bioscience/avogadrolibs`) · feat: no input generators. (PLAN-42-43-09)
  - Evidence: `0320:36`, `Avogadro Generators not found at …/../avogenerators!`.
  - Fix: add the avogenerators archive at the commit tag 2.0.0 pins as a source, moved to `$SRC_ROOT/avogenerators`. Publish.
- [x] **P1** `gnuradio` (`sdr/gnuradio`) · newport: `gr_filter_design` ships but cannot start (no pyqtgraph).
  - Evidence: `0866:243`, `pyqtgraph - not found`.
  - Fix: the new port `python3-pyqtgraph` in depends.
- [x] **P2** `gnuradio` · defaults: `grc.conf` sets `xterm_executable = /var/cache/kpkg/work/gnuradio/gnuradio-3.10.12.0/foot`, so GRC cannot run no-GUI flow graphs. (IMG-12)
  - Fix: `-DGRC_XTERM_EXE=/usr/bin/foot`; bump release; verify `grc.conf`.
- [x] **P3** `gnuradio` · linkage: `gr-perf-monitorx` imports `networkx`, which is not a port. (LNK-12)
  - Fix: the new port `python3-networkx` in depends (Q11), or drop the script from `$PKG`.
- [x] **P3** `gnuradio` · newport: no pygccxml. Fix: the new ports `castxml` and `python3-pygccxml`.
- [x] **P3** `qt6-qtwebengine`, `qt6-qtconnectivity` · layout: `usr/bin/QtWebEngineProcess6` and `usr/bin/sdpscanner6` point at `../lib/qt6/bin/`, but the binaries are in `usr/lib/qt6/libexec/` (S24). (PLAN-HEAD-19, LNK-12)
  - Fix: remove both links from `$PKG/usr/bin` (or point them at `libexec`).
- [x] **P3** `qt6-qtwebengine` (`qt6/qt6-qtwebengine`) · linkage: compiles Chromium's bundled zlib despite `FEATURE_webengine_system_zlib=ON`, because Qt passes `use_system_zlib` only when system minizip is also on (`src/core/CMakeLists.txt:498-500`). (LOG-05)
  - Fix: `-DFEATURE_webengine_system_minizip=ON`, minizip in depends (port chromium's `cr152-unbundle-minizip-undo-unicode` patch if needed); guard against `bundled_zlib` in the ninja log. Otherwise delete the `build.sh:46-49` comment (S17).
- [x] **P2** `volume_key` (`disk/volume_key`) · dep: links NSS without depends (S21). Fix: add `nss nspr`.
- [x] **P3** `imagemagick` (`image-libs/imagemagick`) · size: 11 MB `/usr/share/doc/ImageMagick-7` `www/` tree, only partly caught by S14's prune (gifs, sources, XML). (PLAN-42-43-02)
  - Fix: `--disable-docs`. The static archives ship (Q8). Lands with 7.1.2-33 (S23).
- [x] **P2** `graphviz` (`graphics-libs/graphviz`) · newport: no triangulation library.
  - Evidence: `0404:437`, `gts: No (disabled)`.
  - Fix: the new port `gts`, then `--with-gts=yes` with a strings guard.
- [x] **P3** `libpeas` (`gtk/libpeas`) · net: tries to clone gi-docgen; the clone fails and the build disables it. Covered by S15's meson wrapper. (PLAN-42-43-07)
- [x] **P2** `gdal` (`gis/gdal`) · feat **[S9]**: Xerces-C, SpatiaLite and Blosc are detected but switched off.
  - Fix: `-DGDAL_USE_XERCESC=ON -DGDAL_USE_SPATIALITE=ON -DGDAL_USE_BLOSC=ON`, and add xerces-c libspatialite c-blosc to depends.
- [x] **P2** `gegl` (`graphics-libs/gegl`) · feat **[S9]**: umfpack is off (GIMP's default Levin matting is lost), and so is spiro. (PLAN-42-43-17)
  - Fix: `-Dumfpack=enabled` with suitesparse; `-Dlibspiro=enabled` with the new port libspiro; delete the comment (S17).
- [x] **P2** `python3-pyqt6` (`python-gui/python3-pyqt6`) · dep **[S9]**: QtBluetooth and QtNfc are disabled ("not ported").
  - Fix: add qt6-qtconnectivity to depends, drop the two `--disable` flags, add both modules to the check loop. Optional: `--qmake-setting 'QMAKE_RPATHDIR ='`.
- [x] **P2** `gst-plugins-good` (`media-frameworks/gst-plugins-good`) · feat **[S9]**: taglib, speex and gdk-pixbuf (and gtk3) are off.
  - Fix: enable them and add taglib speex gdk-pixbuf gtk3 to depends. Lands with 1.28.8 (S23).
- [x] **P2** `notmuch` (`mail/notmuch`) · feat: the notmuch2 Python bindings and `notmuch-git` are never installed, though `notmuch-git`'s man page and info file ship. (PLAN-42-43-12)
  - Evidence: `0679:187`, `No module named 'notmuch2'`; `Makefile.local:276` installs only `notmuch-shared`.
  - Fix: pip-install `bindings/python-cffi` into `$PKG` (python3-cffi, python3-setuptools in depends); `install -m755 notmuch-git "$PKG/usr/bin/notmuch-git"` with git in depends; `--without-rpath`; delete the `build.sh:23-25` comment (S17).
- [x] **P2** `libvirt` (`virt/libvirt`) · layout: `-Dnls=disabled` still installs 49 `.mo` (27 MB) and 8.4 MB of HTML; S14's prune removes both. (PLAN-42-43-04)
  - Fix: find why `-Dnls=disabled` still installs catalogs; delete the false comment (S17).
- [x] **P3** `qt6-qtbase` (`qt6/qt6-qtbase`) · feat: no PSQL driver.
  - Fix: `-DFEATURE_sql_psql=ON` and add postgresql to depends. Not in Qt 5: libpq uses OpenSSL 4.
- [x] **P3** `qt5-qtmultimedia` (`qt5/qt5-qtmultimedia`) · feat **[S9]**: OpenAL is off, so QtAudioEngine is missing.
  - Fix: `-feature-openal` and add openal-soft to depends.
- [x] **P3** `fontforge` (`fonts/fontforge`) · newport: no Spiro. Fix: the new port `libspiro`; delete the comment (S17).
- [x] **P3** `kio`, `kio-extras` (`kf6/kio`, `kde/kio-extras`) · other: the `KF_SKIP_PO_PROCESSING leaves every translation out` comments are false (handbooks in 19 and 20 languages ship; S14 prunes them). (PLAN-42-43-05)
  - Fix: delete the comments (S17).
- [x] **P3** `kio` · newport: no switcheroo-control. Fix: the new port `switcheroo-control` (a D-Bus service started by the KDOS supervisor, no systemd unit).
- [x] **P3** `libcanberra` (`xdg/libcanberra`) · layout: GDM and GNOME session hooks plus `canberra-boot` ship.
  - Fix: `rm -rf` `usr/share/gdm`, `usr/share/gnome`, `usr/lib/gnome-settings-daemon-3.0` and `usr/bin/canberra-boot`; delete the comment (S17).
- [x] **P3** `uranium` (`fabrication/uranium`) · layout: `/usr/bin/stl*` from vendored numpy-stl. Fix: `rm` them.
- [x] **P3** `gtk4` (`gtk/gtk4`) · musl: the F16C probe needs ifunc. Fix: add a known-gaps entry.
- [x] **P3** `notmuch` · newport: no sfsexp. Fix: the new port `sfsexp`.
- [x] **P3** `wireshark` (`net-diag/wireshark`) · feat **[S9]**: the ssh extcaps are off.
  - Fix: `-DBUILD_sshdump=ON -DBUILD_ciscodump=ON -DBUILD_wifidump=ON` and add libssh to depends. Optional new ports: `libmaxminddb`, `snappy`, `libsmi`.
- [x] **P3** `kfilemetadata` (`kf6/kfilemetadata`) · newport: no catdoc. Fix: the new port `catdoc`.
- [x] **P3** `libkcddb` (`optical/libkcddb`) · newport: no MusicBrainz; `kcm_cddb.desktop` execs the absent `systemsettings`. (UX-10)
  - Fix: the new port `libmusicbrainz5`; remove `kcm_cddb.desktop` from `$PKG`.
- [x] **P3** `foomatic-db-engine` (`printing/foomatic-db-engine`) · layout: uses `/lib/cups`. Fix: `LIB_CUPS=/usr/lib/cups` to configure.
- [x] **P3** `sane-backends` (`printing/sane-backends`) · other: the comment says saned stays out, but it ships. Fix: delete the comment (S17); `$version` in `source =` (S25).
- [x] **P3** `vlc` (`video-players/vlc`) · dep: links libSM and libICE without depends. Fix: add `libSM libICE`. The lib64 RPATH is S3.
- [x] **P3** `vlc` · feat: `--disable-srt --disable-librist` (`build.sh:191-192`). Fix (Q40): `--enable-srt --enable-librist`, libsrt and librist in depends; delete the l.43 claim (S17).
- [x] **P3** `gst-plugins-bad` (`media-frameworks/gst-plugins-bad`) · feat: `-Dsrt=disabled` ("srt stays off until srt is a port", `build.sh:40,111`). Fix (Q40): `-Dsrt=enabled`, libsrt in depends; delete the comment (S17).
- [x] **P3** `python3-pyside6` (`qt6/python3-pyside6`) · feat: no NumPy. (PLAN-42-43-03)
  - Evidence: `0838:80`, `-- NUMPY not found`.
  - Fix: `-DNUMPY_INCLUDE_DIR=…` and numpy in depends. `libpyside6remoteobjects.a` has headers and ships (Q8).
- [x] **P3** `zbar` (`image-libs/zbar`) · metadata: `zbarcam.desktop` has `Video` without `AudioVideo`. (UX-10)
  - Fix: `Categories=AudioVideo;Video;` in `$PKG`.
- [x] **P3** `kservice`, `fs/etc/xdg/menus` · ux: no `applications.menu` exists and `XDG_MENU_PREFIX` is unset, so KService likely has no menu tree for Dolphin's "Open With → Other Application". Unconfirmed on screen. (UX-08)
  - Fix: rig check first (`kbuildsycoca6` in a login session, then the Dolphin dialog). If the tree is empty, ship `fs/etc/xdg/menus/kdos-applications.menu` with `XDG_MENU_PREFIX=kdos-` in `10-wayland.sh`; document in `session.md`.
- [x] **P3** · hyg: unused CMake variables (S15). Drop each. (PLAN-42-43-11)

  | ☐ | Port(s) | Variable |
  |---|---|---|
  | [x] | kfilemetadata, baloo, kauth, kpty, kunitconversion, kded, kdesu, kpeople, networkmanager-qt, modemmanager-qt, bluez-qt, ktextwidgets, kdeclarative, kparts, knotifyconfig, ktexteditor, purpose, kmime, kmbox, kconfigwidgets, kpackage, knewstuff, kglobalaccel, kxmlgui, kstatusnotifieritem, kcmutils, frameworkintegration, qqc2-desktop-style, kirigami-addons, kbookmarks, kcompletion, kcrash, kdoctools, kiconthemes, knotifications, kjobwidgets, kservice, kwallet, kio | `BUILD_QCH` |
  | [x] | kidletime, kitemmodels, threadweaver, kplotting, prison, kquickcharts, ktexttemplate, kded, networkmanager-qt, modemmanager-qt, bluez-qt, kmbox, attica, kdegraphics-thumbnailers, kguiaddons, kcrash, breeze-icons, knotifications | `KF_SKIP_PO_PROCESSING` |
  | [x] | libksysguard | `CMAKE_DISABLE_FIND_PACKAGE_Libcap` |
  | [x] | qtutilities, qtforkawesome | `NO_SPHINX` |
  | [x] | avogadrolibs | `USE_SYSTEM_GENXRDPATTERN` |
  | [x] | soqt | `BUILD_TESTING` |

  qt6-qtwayland's `FEATURE_wayland_server` is reported unused but the compositor module builds; leave it.

Withdrawn from 43_toolkits: the webkitgtk WOFF2 item. `OptionsGTK.cmake:405-412` turns `USE_WOFF2` off when FreeType decodes WOFF2 (`FREETYPE_WOFF2_SUPPORT_IS_AVAILABLE - Success`, log :310), so WOFF2 fonts work and the recipe comment is accurate (PLAN-42-43-01).

## 44_apps

- [x] **P1** `mumble` (`chat/mumble`) · security: loads OpenSSL 3 and OpenSSL 4 together (S16).
  - Evidence: `0680`, `libssl.so.3, needed by …libQt5Network.so.5.15.19, may conflict with libssl.so.4`.
  - Fix (Q26): build mumble with Qt 6 (qt6-qtbase, qt6-qtsvg, qt6-qttools, qt6-qtwayland in depends in place of the qt5-* ports) against openssl; drop `openssl4-x509-name.patch` only if the Qt 6 build no longer needs it. Measure first that 1.5.915 or the next stable release has a Qt 6 build; if neither has, stop and put the port to the user as a Q26 exception rather than taking the openssl3 route silently.
  - Verify: no `libQt5*` and no `libssl.so.3` in NEEDED.
- [x] **P1** `quassel` (`chat/quassel`) · security: OpenSSL 3 and 4 together through the qca ossl plugin and libldap (S16). (PLAN-44-TAIL-20)
  - Fix (Q26): build quassel with Qt 6 against `crypto/qca` (the Qt 6 port) in place of qca-qt5; LDAP stays on (`-D WITH_LDAP=ON`), because the whole process then maps OpenSSL 4 only. Measure first that 0.14.0 or the next stable release has a Qt 6 build; if neither has, stop and put the port to the user as a Q26 exception.
  - Verify: no `libQt5*`, no `libssl.so.3`, no `libqca-qt5` in NEEDED.
- [x] **P1** `warzone2100` (`games-strategy/warzone2100`) · dwarf: a 340 MB `warzone2100.debug` lands in `/usr/bin`.
  - Fix: `-DWZ_SKIP_ELF_SEPARATE_DEBUG=ON` and `export LDFLAGS="$LDFLAGS -Wl,--strip-debug"`. Also `rm -rf` the `hosting/linux_scripts` doc samples.
- [x] **P1** `openttd` (`games-strategy/openttd`) · musl: network keys come from the fallback RNG.
  - Evidence: `1053`, `random_func.cpp:120:9: warning: #warning "No cryptographically-strong random generator available"`.
  - Fix: ship `musl-getrandom.patch` (`__GLIBC__` → `__linux__` in both places).
- [x] **P1** `qmapshack` (`gis/qmapshack`) · layout: its vendored Blend2D overwrites the blend2d port's headers and CMake package (now static) (S5). (PLAN-44-TAIL-05)
  - Evidence: `1069:97`, `Enabling install support ('BLEND2D_NO_INSTALL=OFF')`; db/qmapshack owns `usr/include/blend2d/*`, `libblend2d.a`, `usr/lib/cmake/blend2d/*`.
  - Fix: `-DBLEND2D_NO_INSTALL=ON`, then reinstall blend2d. pdf4qt builds earlier and needs no rebuild. Also `-DCMAKE_SKIP_INSTALL_RPATH=ON` and drop `BUILD_TESTING`.
- [x] **P2** `linuxcnc` (`fabrication/linuxcnc`) · linkage: `import gladevcp` dies on `import glib` (`gremlin_view.py:87`, the old PyGTK module), so `gladevcp`, `gscreen` and `gmoccapy` cannot start. (LNK-01)
  - Fix: ship `pygi-glib.patch` (`from gi.repository import GLib as glib`); bump release. Verify `python3 -c 'import gladevcp.makepins'` gets past the import. Its 9 dangling `nc_files` doc links go (S24).
- [x] **P2** `chocolate-doom`, `crispy-doom` (`games-shooter/…`) · ux: the Setup menu rows exec `chocolate-setup` and `crispy-setup`, which do not exist; the binaries are `chocolate-doom-setup` and `crispy-doom-setup`. (LNK-02, UX-03)
  - Fix: set `Exec=` and `StartupWMClass=` to the real names (`build.sh:51,54` and `:53`); keep `Icon=`. Bump both.
- [x] **P2** `blender` (`3d/blender`) · dep **[S9]**: Draco, NDOF, OpenImageDenoise, Embree and meshoptimizer are off ("not ports", `build.sh:30-31,129-133`).
  - Fix: `-DWITH_DRACO=ON -DWITH_INPUT_NDOF=ON`, and add draco and libspnav to depends. Check that the glTF add-on finds draco. Q40: `-DWITH_OPENIMAGEDENOISE=ON -DWITH_CYCLES_EMBREE=ON -DWITH_MESHOPTIMIZER=ON` with oidn, embree and meshoptimizer in depends; USD, MaterialX, Hydra, OSL and OpenPGL go to known-gaps (S9).
- [x] **P2** `falkon` (`browsers/falkon`) · dep **[S9]**: no Python plugins.
  - Fix: add the WebChannel, WebEngineCore and WebEngineWidgets modules to python3-pyside6 first, then `-DBUILD_PYTHON_SUPPORT=ON` and python3-pyside6 in depends.
- [x] **P2** `gmsh` (`cad/gmsh`) · dep **[S9]**: MED is off.
  - Fix: `-DENABLE_MED=ON` and add libmed hdf5 to depends.
- [x] **P2** `recoll` (`database/recoll`) · tool: the qmake GUI misses the tree flags and gets RPATH `/usr/lib`.
  - Fix: `QMAKE='qmake6 QMAKE_CFLAGS+=$$(CFLAGS) QMAKE_CXXFLAGS+=$$(CXXFLAGS) QMAKE_LFLAGS+=$$(LDFLAGS) QMAKE_LFLAGS_RPATH='`.
- [x] **P2** `kdevelop` (`editors/kdevelop`) · newport: the cppcheck plugin ships but there is no cppcheck.
  - Fix: the new port `cppcheck`; delete the comment (S17).
- [x] **P2** `vice` (`emulators/vice`) · tool and dwarf: configure discards the tree flags, so 106 MB of DWARF ships with no hardening flags; `--enable-html-docs` runs texi2any for nothing. (PLAN-44-TAIL-18)
  - Fix: `make CFLAGS="${CFLAGS/-O2/-O3} -g0"` and the same on install; `--disable-html-docs`; add libusb to depends.
- [x] **P3** `mednafen` (`emulators/mednafen`) · tool: the tree `-fPIC` overrides `-fno-pic` in a non-PIE binary; codegen only. (PLAN-44-TAIL-19)
  - Fix: `export CFLAGS="${CFLAGS//-fPIC/}" CXXFLAGS="${CXXFLAGS//-fPIC/}"`.
- [x] **P3** `mednafen` · size: installs its only manual as HTML, which S14 prunes. (PLAN-44-TAIL-18)
  - Fix (Q53): S14's prune exempts `usr/share/doc/mednafen`; `writing-ports.md` names the exception and its rule (a program whose only manual is HTML).
- [x] **P2** `supertuxkart` (`games-action/supertuxkart`) · feat **[S9]**: no Wiimote ("BlueZ not here").
  - Fix: `-DUSE_WIIUSE=ON` and add bluez to depends. Drop `CMAKE_INSTALL_LIBDIR`.
- [x] **P2** `organicmaps` (`gis/organicmaps`) · tool: upstream overwrites the linker flags (no BIND_NOW).
  - Fix: `-DCMAKE_EXE_LINKER_FLAGS_RELEASE="$LDFLAGS"`.
- [x] **P2** `sauerbraten` (`games-shooter/sauerbraten`) · tool: no LDFLAGS, and ENet has no tree CFLAGS.
  - Fix: prepend `$LDFLAGS` to `CLIENT_LIBS` and `SERVER_LIBS`, and pass `CFLAGS="$CFLAGS -O3 -fomit-frame-pointer"`.
- [x] **P2** `pioneer` (`games-strategy/pioneer`) · layout: generic `/usr/bin/editor`, `modelcompiler` and `savegamedump`.
  - Fix: rename them to `pioneer-*`.
- [x] **P2** `timg` (`graphics/timg`) · dep **[S9]**: GraphicsMagick is off.
  - Fix: `-DWITH_GRAPHICSMAGICK=On` and add graphicsmagick to depends.
- [x] **P2** `freedv-gui` (`hamradio/freedv-gui`) · tool: the bundled opus AVX2 probe fails under gcc 15 (`incompatible-pointer-types`), and DWARF ships. (PLAN-44-TAIL-02)
  - Fix: `export X86_AVX2_CFLAGS="-mavx -mfma -mavx2 -Wno-error=incompatible-pointer-types"` (opus `configure.ac:443` `AC_ARG_VAR`; RADE's ExternalProject inherits the environment); no patch. Verify the log says `AVX2 intrinsics with ...yes`. Strip debug.
- [x] **P2** `rawtherapee` (`graphics/rawtherapee`) · feat: no OpenMP FFTW.
  - Fix: in `sci-libs/fftw`, add `--enable-openmp` to COMMON and the long-double configure. Rebuild fftw, then rawtherapee.
- [x] **P3** `sdrangel` (`sdr/sdrangel`) · dep: cm256cc is rejected (its `.pc` says 1.1.0); the remote plugins it would enable need SSE3 or NEON, so x86-64 v1 gains nothing. (PLAN-44-TAIL-04)
  - Fix: `-DCM256CC_DIR=/usr`; the gain is on aarch64 or `kdos march` builds only.
- [x] **P2** `satdump` (`sdr/satdump`) · dep **[S9]**: the Airspy HF+, LimeSDR, PlutoSDR and USRP plugins are off.
  - Fix: turn them on and add airspyhf limesuite libiio libad9361 uhd to depends. Extend the plugin test loop.
- [x] **P2** `homebank` (`office/homebank`) · hidden: `index.theme` and an SVG are missing from the tarball, so the Default icon theme cannot load.
  - Fix: ship a patch adding the missing files, plus a `test -f` guard. Drop `--disable-static`. Its `usr/lib/locale` catalogs are pruned (S14).
- [x] **P2** `homebank`, `gnucash` (`office/…`) · newport: no OFX import.
  - Fix: the new ports `opensp` and `libofx`, then `--with-ofx` and `-DWITH_OFX=ON`. Drop gnucash's dead `ENABLE_BINRELOC`.
- [x] **P2** `hplip` (`printing/hplip`) · hidden: configure clobbers CFLAGS, so 67 C objects build unoptimised and unhardened; its patch copies `HPLIP.conf` into the live `/usr/share/ipp-usb/quirks/` (S4). (PLAN-HEAD-08)
  - Fix: `make CFLAGS="$CFLAGS"`; the cp in `more-imageprocessor-removes.patch` uses `$(DESTDIR)`; remove the file from build/fs.
- [x] **P3** `lmms` (`studio/lmms`) · layout: installs its bash completion into the live root (S4). (PLAN-HEAD-08)
  - Fix: set the completion directory to `$PKG/usr/share/bash-completion/completions` (or install it by hand); remove the file from build/fs. Turn JACK on with pipewire in depends (42_graphics `pipewire`, Q38).
- [x] **P3** `opencpn` (`gis/opencpn`) · layout: writes `98-sglock-ocpn.rules` into the live `/usr/lib/udev/rules.d` (S4). (PLAN-HEAD-08)
  - Fix: write the rule into `$PKG` or disable the writer; remove the file from build/fs. openssl in depends (S21).
- [x] **P2** `deluge` (`transfer/deluge`) · layout: vendors `distro`, taking 16 paths from python3-distro (S5). rjsmin is left unowned (S4).
  - Fix: remove distro from `pypackages`, add python3-distro to depends, re-vendor. Install the build backends with `--target`.
- [x] **P2** `haruna` (`video-players/haruna`) · newport: no yt-dlp. (PLAN-44-TAIL-11)
  - Fix: the new port `yt-dlp` in 41_system, then in the depends of mpv (42_graphics), haruna and celluloid.
- [x] **P2** `audacity`, `musescore` (`studio/…`) · other: `QT_QPA_PLATFORM=xcb` is forced, against hard rule 5.
  - Fix: ship a patch dropping the forced xcb and gtk3 platform theme in `src/app/main.cpp`. Rig-check the result.
- [x] **P2** `blind` (`video-tools/blind`) · layout: the `blind-rotate-*` scripts are missing but their man pages ship. The tree flags are also ignored.
  - Fix: install the three scripts. `make CFLAGS="$CFLAGS -std=c99" LDFLAGS="$LDFLAGS -lm"`.
- [x] **P2** `obs-studio` (`video-tools/obs-studio`) · feat **[S9]**: no VLC source. (PLAN-44-TAIL-06)
  - Fix: `-D ENABLE_VLC=ON` and add vlc to depends; rnnoise in depends (S21). Q40: `-D ENABLE_NEW_MPEGTS_OUTPUT=ON` (`build.sh:64`) with libsrt and librist in depends, for SRT and RIST output. Delete the asio, RNNoise, SRT and RIST comments (S17); drop `ENABLE_OSS`.
- [x] **P2** `avidemux` (`video-tools/avidemux`) · tool: the fluxSmooth table overflows by one element.
  - Fix: ship `fluxsmooth-table-size.patch` (65536).
- [x] **P3** `avidemux` · feat: TwoLAME is off; the lib64 install is S3. (PLAN-44-TAIL-03)
  - Fix: `-DTWOLAME=ON` and twolame in depends; delete the `build.sh:55` comment (S17). Keep FDK off. Verify after S3 that `grep -c lib64 build/fs/var/lib/kpkg/db/avidemux` is 0.
- [x] **P3** `qt6ct` (`themes/qt6ct`) · ux: the visible "Qt 6 Settings" entry changes nothing for native Qt 6 apps (`QT_QPA_PLATFORMTHEME=kde`). (UX-09)
  - Fix: `NoDisplay=true` in the installed desktop file (`desktop-file-edit` on `$PKG`).
- [x] **P3** `openscad`, `kiwix-desktop` · metadata: `desktop-file-validate` errors: `Programming` and `Reference` are not registered categories. (UX-10)
  - Fix: openscad `Development`; kiwix drops `Reference`; both in `$PKG`.
- [x] **P3** `orcaslicer` (`fabrication/orcaslicer`) · hardening: the superbuild compiles private GMP and MPFR (1,055 compile lines) without the tree flags, though both are ports. (LOG-15)
  - Fix: a deps `CMakeLists` patch dropping `dep_GMP` and `dep_MPFR` so CGAL uses the system ones, gmp and mpfr in depends; failing that, pass the tree CFLAGS to those ExternalProjects. Delete the dependency comment (S17).
- [x] **P3** `kodi`, `gmsh`, `amiberry` + `libmediainfo` (41) · linkage: four ports compile their own TinyXML-2 (S9 claim "tinyxml2 is not a port"). (LOG-11)
  - Fix: the new port `tinyxml2` (formats, 41_system); kodi drops `-DENABLE_INTERNAL_TINYXML2=ON`, libmediainfo drops `-DCMAKE_DISABLE_FIND_PACKAGE_TinyXML=ON`, gmsh and amiberry use the system copy; delete the comments (S17).
- [x] **P3** `solvespace` (`cad/solvespace`) · dep and feat: the in-tree Eigen is used, and OpenMP is off.
  - Fix: `-DEIGEN3_INCLUDE_DIRS=/usr/include/eigen3 -DENABLE_OPENMP=ON`.
- [x] **P3** `quassel` · newport: no spell-check and no tray menu. Fix: kf6 `sonnet` (already a port, `43_toolkits/packages.d/kf6.txt:39`) and the new port `libdbusmenu-lxqt` in depends, with the Qt 6 build (Q26).
- [x] **P3** `tuxpaint` (`education/tuxpaint`) · tool: the `strcasestr` replacement leaks memory.
  - Fix: `CPPFLAGS="$CFLAGS -D_POSIX_PRIORITY_SCHEDULING -DHAVE_STRCASESTR"`. Move the bash completion (S13).
- [x] **P3** `goldendict-ng` (`education/goldendict-ng`) · layout: RPATH `$ORIGIN:$ORIGIN/../lib64`. Fix: `-DCMAKE_SKIP_INSTALL_RPATH=ON`.
- [x] **P3** `kalzium` (`education/kalzium`) · newport: no `chemical/x-cml`. Fix: the new port `chemical-mime-data`.
- [x] **P3** `okular` (`documents/okular`) · newport: no Mobipocket. Fix: the new port `kdegraphics-mobipocket`.
- [x] **P3** `nextpnr` (`eda/nextpnr`) · hyg: empty version string, and 944 MiB of DWARF (S2b).
  - Fix: `-DCURRENT_GIT_VERSION="nextpnr-$version"` plus strip debug.
- [x] **P3** `qucs-s`, `qt-creator`, `endless-sky`, `devilutionx`, `inspectrum`, `opencpn` · hyg: unused CMake variables.
  - Fix: drop `CMAKE_DISABLE_FIND_PACKAGE_Git` (qucs-s), `CMAKE_DISABLE_FIND_PACKAGE_sentry`, `CMAKE_INSTALL_LIBDIR`, `DEVILUTIONX_SYSTEM_{LIBPNG,GOOGLETEST}`, `CMAKE_DISABLE_FIND_PACKAGE_Qt5` and `FETCHCONTENT_FULLY_DISCONNECTED`; delete the related comments (S17).
- [x] **P3** `texstudio` (`doctools/texstudio`) · feat: no PDF video player. Fix: turn it on with qt6-qtmultimedia in depends.
- [x] **P3** `dosbox-staging` (`emulators/dosbox-staging`) · musl and hidden: no thread names, and the version has an empty hash.
  - Fix: `-DHAVE_PTHREAD_SETNAME_NP=1 -DCMAKE_DISABLE_FIND_PACKAGE_Git=ON`.
- [x] **P3** `freeciv` (`games-strategy/freeciv`) · layout: man pages for clients that are not built. Fix: `rm` them.
- [x] **P3** `gpxsee` (`gis/gpxsee`) · layout: RPATH `/usr/lib`. Fix: S12's tree option (`QT_DISABLE_RPATH`).
- [x] **P3** `wsjtx` (`hamradio/wsjtx`) · layout: sounds land in `/usr/bin`, 8 orphan man pages, generic tool names. `jt9` has an exec stack (S12).
  - Fix: `rm` the orphan pages; delete the comment about `udp_daemon`, `fcal`, `fmtave` and `fmeasure` (S17).
- [x] **P3** `digikam` (`graphics/digikam`) · newport: no JPEG2000 loader. Fix: the new port `jasper`.
- [x] **P3** `goocanvas` (`graphics-libs/goocanvas`) · layout: a stub gtk-doc ships. Fix: `rm -rf "$PKG/usr/share/gtk-doc"`.
- [x] **P3** `xastir` (`hamradio/xastir`) · newport: `--with-gpsman` finds no `gpsman` (`acinclude.m4:178` `AC_PATH_PROG`). (PLAN-44-TAIL-10)
  - Fix: keep `--with-gpsman`; the new port `gpsman` (providing `gpsmanshp` too) in depends, ordered before xastir. Verify the log says `GPSMan/gpsmanshp ...: yes`.
- [x] **P3** `gimp`, `darktable`, `aview` (`graphics/…`) · other: comments contradict the build (gimp: execinfo backtraces are off on musl; darktable: GraphicsMagick is a port; aview: a dangling CLAUDE.md reference).
  - Fix: delete all three (S17).
- [x] **P3** `qgis` (`gis/qgis`) · newport: bundled OpenCL C++ headers. Fix: the new port `opencl-clhpp`.
- [x] **P3** `qgis` · linkage: `-DWITH_INTERNAL_MESHOPTIMIZER=TRUE` (`build.sh:82`) compiles a bundled meshoptimizer. Fix (Q40): `-DWITH_INTERNAL_MESHOPTIMIZER=FALSE`, meshoptimizer in depends; delete the l.38 comment (S17).
- [x] **P2** `ardour` (`studio/ardour`) · feat: no JACK backend. Fix (Q38): turn the JACK backend on with pipewire in depends; verify `libjack.so.0` in its backend's NEEDED.
- [x] **P3** `chirp` (`hamradio/chirp`) · other: version `py3dev`. Fix: ship `version.patch` (`CHIRP_VERSION` and setup.py version).
- [x] **P3** `octave` (`math/octave`) · feat: no jsonencode PrettyPrint.
  - Fix: backport the post-1.1.0 PrettyWriter fix as a patch in `formats/rapidjson`, and check `HAVE_RAPIDJSON_PRETTYWRITER`.
- [x] **P3** `tryton`, `gnuhealth` (`office/tryton`, `bioscience/gnuhealth`) · layout: hatch-tryton is pip-installed into `/` and left unowned. (PLAN-44-TAIL-09)
  - Fix: the new port `python3-hatch-tryton` (40_lang) in both ports' depends; delete both pip installs (`tryton/build.sh:21`, `gnuhealth/build.sh:47`).
- [x] **P3** `ledger` (`office/ledger`) · layout: `/usr/share/contrib`.
  - Fix: install the bash completion properly, then move `contrib` to `/usr/share/ledger/contrib` and drop its tests.
- [x] **P3** `picard` (`music-players/picard`) · dep: the vendored markdown is silently skipped. Fix: add python3-markdown to depends and drop markdown from `pypackages`.
- [x] **P3** `sc-im` (`office/sc-im`) · layout: `.sc` files land in `/usr/share/themes`. Fix: `THEMEDIR=/usr/share/sc-im/themes`.
- [x] **P3** `schismtracker` (`studio/schismtracker`) · other: no version. Fix: ship `version.patch` plus `SCHISM_VERSION=$version autoreconf -fi`.
- [x] **P3** `hydrogen` (`studio/hydrogen`) · feat: no manual. Fix: add the documentation archive as a second source. Turn JACK on with pipewire in depends (Q38).
- [x] **P3** `handbrake` (`video-tools/handbrake`) · feat: no libdovi. Fix: the new port `libdovi` (`vendoring = rust`) and enable it.
- [x] **P3** `kodi` (`video-players/kodi`) · layout: a duplicate `/usr/bin/TexturePacker`. Fix: `rm` it.
- [x] **P2** `xh`, `iamb`, `termusic`, `chess-tui`, `mqttui`, `surfer`, `sniffnet` · security: each statically embeds AWS-LC (six versions among them; `chess-tui` and `sniffnet` name it in `build.sh`), which `kdos cve` cannot see. (LOG-09)
  - Fix (Q44): switch the TLS backend to native-tls on the system OpenSSL where the crate offers it, guarded by a NEEDED `libssl.so` check; for the rest, an inventory row (port, crate, AWS-LC version) that `kdos cve` reads. Document the inventory in `packaging.md`.
- [x] **P3** `rawtherapee`, `darktable`, `filezilla`, `crispy-doom`, `chocolate-doom`, `netsurf`, `tenacity`, `neverball`, `ppsspp` · other: see S1, S2b, S5 and S12. — **closed:** pointer item whose targets S1, S2b, S5 and S12 are all `[x]`

Withdrawn from 44_apps: the archive removals in openrct2, ardour, kdenlive and freecad (Q21, PLAN-44-TAIL-15).

## 50_desktop

- [x] **P3** `fcitx5`, `libime`, `fcitx5-chinese-addons`, `fcitx5-anthy`, `fcitx5-hangul` (`input/…`) · hyg: `-Wno-dev`. Covered by S15. — **closed:** premise false: CMake has no `-Wno-author` (cmake 4.2.3 still prints the AUTHOR_WARNING with it), so `-Wno-dev` is the right flag
- [x] **P3** `kdos-shell` (`src/desktop/kdos-shell`) · ux: Settings → Apps reads only `~/.config/mimeapps.list`, so a fresh system shows "no default handlers are recorded yet" while about 160 system defaults apply (`settings.c:983-1016,1991`). (UX-06)
  - Fix: show the effective handler for a fixed set of roles through kdos-appbox open's layered lookup (a shared libkxdg function, not a second resolver), marking the layer that answered. Update `kdos-shell.md`.
- [x] **P3** `kdos-shell` · ux: the Start menu stops silently at `SH_MAX_APPS` 512 entries (`shell.h:612`, `apps.c:395,492`), about 120 above today's 390. (UX-13)
  - Fix: allocate dynamically, or raise the cap and log once when hit. A selftest loads `SH_MAX_APPS+1` entries and checks none is lost.
- [x] **P3** `kdos-term`, `mc`, `aerc`, `foot` · ux: "Terminal", "Files" and "Mail" each resolve to a different program by route: `kdos-term.desktop:3` and `mc/build.sh:37` and `aerc/build.sh:148` take the plain names, while the desktop launches foot (`foot.desktop` is "Foot"), the Start "Files" row, and Thunderbird for `mailto:` (`kdos-mimeapps.list:165`). (UX-D2)
  - Fix (Q46): foot's entry takes `Name=Terminal`; kdos-term's entry takes its own name; mc's entry becomes `Name=Files (mc)`; aerc keeps "Mail". `desktop.md` documents that `mailto:` opens Thunderbird. Update `kdos-term.md`.
- [x] **P3** `src/system/kdos-doc` (new) · ux: 92 of 93 `kdos-*` commands have no man page (`kdos-bb.1` is the one), and the book is not on the image. (UX-D3)
  - Fix (Q47): a new src port renders `docs/kdos` to HTML under `/usr/share/doc/kdos/`, writes `kdos(7)` pointing there, and generates one man page per `kdos-*` command from its book section, with lowdown (41_system). The chroot mounts no `docs/` today (`script/chroot/exec.sh`); add a read-only bind. S14's HTML prune exempts `usr/share/doc/kdos`. Preflight fails a `kdos-*` command with no man page. Document in `developing.md` and `kdos-command.md`.
  - Verify: `man kdos-appbox` and `apropos kdos` answer on the image.
- [x] **P3** `kdos-cursors` (`src/art/kdos-cursors`) · ux: no `usr/share/icons/default`, so programs that never see `XCURSOR_THEME` (sudo's env_reset, pkexec, X clients) get no cursor theme. (DEF-16)
  - Fix: ship `usr/share/icons/default/index.theme` with `Inherits=KDOS-cursors`; document it in `03-architecture/session.md` and `06-reference/configuration.md` (the `XCURSOR_THEME` entries).

## 60_kernel

- [x] **P1** `linux` (`boot/linux`) · feat: `--module` on the bool `CONFIG_MEDIA_CAMERA_SUPPORT` drops about 130 camera symbols (uvcvideo, gspca, sensors).
  - Evidence: `60_kernel__0089_linux`, `.config:11716:warning: symbol value 'm' invalid for MEDIA_CAMERA_SUPPORT`.
  - Fix: build.sh:138 becomes `./scripts/config --enable CONFIG_MEDIA_CAMERA_SUPPORT`; an olddefconfig guard requires `CONFIG_USB_VIDEO_CLASS=m` and `CONFIG_USB_GSPCA=m`; remove the invalid `NETFILTER_NETLINK=m`, `MULTIPLEXER=m` and `BOOTPARAM_*_PANIC=n` from `kdos.config`. Lands with 7.2.11 (S23).
- [x] **P2** `linux` · kernel: the config has two sources of truth: `make defconfig`, then the full 12,498-line `kdos.config` appended, then about 250 `scripts/config` lines. 4,267 override warnings bury the 5 invalid-symbol ones; several `build.sh` lines are no-ops (`AHCI`, `INPUT_VMMOUSE`, `GENERIC_CPU`, `PCI_MSI_IRQ_DOMAIN`, `MICROCODE_INTEL/AMD`, `ACPI_ASUS`, `ACPI_LENOVO`, `FB_SIMPLE`, two fonts), two silently reverse `kdos.config` (`UEVENT_HELPER`, `PM_AUTOSLEEP`), and several requested `=y` land `=m`. (KRN-03, LOG-02)
  - Fix: fold the intended `build.sh` values into `kdos.config`, then `cp $PORT_SRC/kdos.config .config && make olddefconfig`; keep the logo copy and the toolchain guard. Fail the build on any `symbol value .* invalid` and on any explicitly set symbol that does not land as requested. Diff the resulting `.config` against today's first. Update `boot-and-init.md:391`, `kinstall.md:519`, `packaging.md:691`.
- [x] **P2** `linux` · kernel: not reproducible; the version string records the container hostname and the build time (`7.2.7 (root@4f71458b3bdf) … Thu Oct 15 19:52:04 UTC 2026`), and a fresh module-signing key is generated per build. (KRN-04)
  - Fix: `export KBUILD_BUILD_TIMESTAMP="@$SOURCE_DATE_EPOCH" KBUILD_BUILD_USER=kdos KBUILD_BUILD_HOST=kdos KBUILD_BUILD_VERSION=1` before any make. The signing key follows Q36; if the kernel stays non-identical, name it as the exception in `packaging.md#reproducible-packages`.
- [x] **P3** `linux` · kernel: module signing and hardening policy (Q36, Q37). (KRN-05, KRN-06)
  - Fix: a persistent signing key in `build/keys/` passed as `CONFIG_MODULE_SIG_KEY` (kept by both cleans, never in git); `X86_X32_ABI=n`, `UEVENT_HELPER=n`, `PM_AUTOSLEEP=n`, `LDISC_AUTOLOAD=n`, `PROC_MEM_FORCE_PTRACE=y`, `BUG_ON_DATA_CORRUPTION=y` in `kdos.config`. Document both in `security-model.md` and `known-gaps.md`.

Moved to S18: the toybox `awk`/`grep` ORC-hash item; its check (`orc_hash.h` is not `0xda,0x39,0xa3,0xee…`) is an S18 gate.

## 70_image

- [x] **P1** `090_initramfs.sh` · kernel: the initramfs carries no driver for several disk transports kinstall offers, so the installed root is not found on first boot: Intel VMD/RST NVMe (the factory default on many current Intel laptops), eMMC and SD (`probe.c:326,334` classifies `mmc`; `installation.md:484` recommends f2fs there), Hyper-V, Xen and VMware PVSCSI guests, MegaRAID and mpt3sas. (IMG-02, KRN-02)
  - Evidence: none of `vmd`, `mmc_block`, `sdhci-pci`, `sdhci-acpi`, `cqhci`, `rtsx_pci_sdmmc`, `hv_storvsc`, `xen-blkfront`, `vmw_pvscsi`, `megaraid_sas`, `mpt3sas` is in `MODULES` (l.431-457) or `build/initramfs` (34 `.ko`); all are modules, not built in.
  - Fix: append them to `MODULES` (`copy_module` pulls `mmc_core`, `hv_vmbus` and the rest); the kernel postinstall reads `/boot/initramfs.modules`, so later kernels carry the same set. Add `/dev/mmcblk*` to the live-medium scan (init:413/911). Write the rule "every transport kinstall classifies is reachable before the root mounts" beside the `ki_filesystems[]` rule and in `boot-and-init.md` (~l.373-395).
  - Verify: `find build/initramfs -name 'vmd.ko*' -o -name 'mmc_block.ko*' -o -name 'sdhci-pci.ko*'` is non-empty.
- [x] **P1** `090_initramfs.sh` · kernel: AMD CPUs get no early microcode. The glob at l.1158 wants `microcode_amd*.bin.zst`; linux-firmware installs plain `.bin`, and the build only logs `Microcode: no amd-ucode blobs`. (IMG-01, LOG-01)
  - Fix: take both `microcode_amd*.bin` (cat) and `*.bin.zst` (`zstd -dc`) into `AuthenticAMD.bin`; fail the build when `amd-ucode/` exists and the result is empty, and the same for Intel. Rewrite the claim that linux-firmware ships `.zst` in the l.1139 comment, `boot-and-init.md:343-359`, `administration.md:658` and `reasons/the-microcode-bundle-must-consume-its-last-byte.txt:10` as the constraint: the early cpio holds uncompressed blobs whatever linux-firmware ships.
  - Verify: `cpio -it < build/ucode.cpio` lists `kernel/x86/microcode/AuthenticAMD.bin`.
- [x] **P2** `090_initramfs.sh`, `110_iso.sh` · ux: every initramfs failure (`init:316,387,548,609,614,619,624,628`) is `exec /bin/sh` on `/dev/console`, which the Live entries put on ttyS0 (`console=ttyS0` last, the verbose entry too); on a laptop the user sees the splash's fail state and no message or shell. (IMG-03)
  - Fix: a `rescue()` helper writes the reason to `/dev/tty1` and through `kdos-splash detail`, then opens the shell on tty1 with setsid, keeping console output for serial; use it at every site. Optionally reorder the verbose entry to `console=ttyS0 console=tty0`. Update the `boot-and-init.md` console paragraph and `getting-started.md:214`.
- [x] **P3** `090_initramfs.sh` · size: initramfs trims. (IMG-09, LOG-03)
  - A 13 MB `boot/System.map-7.2.7` is copied only for `depmod -F`: point `-F` at the chroot's copy.
  - `lib/udev/hwdb.d/*.hwdb` (592 KB) ships with no `hwdb.bin`; udev reads only the binary: skip it.
  - l.444 names `aes_generic aes_x86_64 sha256_generic sha512_generic`, which 7.2 has as built-in `aes`, `sha256`, `sha512`: drop them; optionally make a missing dm-crypt module fatal.
  - l.1205 `ls /kdos/build` prints into the log: delete it.
  - Optional: `zstd -19` in both packers (l.1196 and `linux/postinstall.sh:146`); the postinstall offset walker (l.84-98) learns the zstd magic in the same change.
- [x] **P2** `070_user.sh`, `install.c` · security: `/etc/{passwd,group,shadow}-` ship in `system.sfs` with the live root and kdos hashes, and kinstall's rsync copies them, so the live root password reaches installed systems, contrary to `kinstall.md:323` and `installation.md:292`. (DEF-10)
  - Fix: 070_user.sh removes `etc/{passwd,group,shadow,gshadow}-`; install.c's rsync excludes them. Verify `unsquashfs -ll system.sfs | grep -E 'etc/(passwd|group|shadow|gshadow)-$'` is empty.
- [x] **P3** `020_cleanup.sh` · security: build state ships in `/root` (mode 0755): a CMake package registry pointing into kpkg work dirs (consulted by root's `find_package`), `.gitconfig` `safe.directory`, dated go telemetry counts and kivy logs that make `system.sfs` non-reproducible; and a rootful podman store in `/var/lib/containers/storage`. kinstall copies all of it. (DEF-11, IMG-08)
  - Fix: reset `/root` to exactly what `fs/root` ships and `chmod 700`; remove `var/lib/containers/storage`. Better: point `HOME` at a scratch directory in `chroot.env` (`exec.sh:151` sets `/root`). Update the cleanup comment and `build-system.md`'s 020_cleanup row. Verify with `unsquashfs -ll system.sfs /root`.
- [x] **P2** new step before `110_iso.sh` · defaults: the image ships no fontconfig cache. fc-cache under `SOURCE_DATE_EPOCH` clamps the stored mtime and then deletes its own caches as stale ("invalid cache file"), because the font directories keep real mtimes; every user's first fontconfig client pays a cold scan of 465 faces, on the live medium every boot. (UX-01)
  - Fix: clamp the font directory mtimes (`find /usr/share/fonts -type d -exec touch -h -d @$SOURCE_DATE_EPOCH {} +`), run `fc-cache -sf`, assert a `*.cache-*` file exists. Optionally the same clamp in kpkg's `T_FONTS` trigger. Document under `writing-ports.md` "Shared indexes".
- [x] **P2** release procedure · version: within one build cycle chromium, firefox-esr, thunderbird and librewolf fell two security releases behind, and nothing checks before an image is cut. (DEC-02)
  - Fix (Q27): a standing pass before `70_image` of every release build reads the advisories of the web engines (chromium, firefox-esr, thunderbird, librewolf, WebKitGTK), OpenSSL, curl, Qt and the kernel, and bumps what is behind (the S23 tables are its first output). Write it into `how-kdos-is-built.md` as the step before 70_image, with its source list.
- [x] **P3** `070_user.sh` · other: l.45 says `find` is toybox's and lacks `-empty`/`-delete`; `find` is findutils'. (DEF-17)
  - Fix: `find /etc/skel/.config -mindepth 1 -type d -empty -delete`, and delete the comment.

---

## New ports to add

The user chose "propose adding all". Every row needs `ports/publish` and its `sources.idx` line committed.

| New port | Upstream | Shelf | Phase | Gains |
|---|---|---|---|---|
| grep | https://www.gnu.org/software/grep/ | base | 30_foundation first block; cross-built in 10_bootstrap | S18: the grep family at GNU |
| which | https://www.gnu.org/software/which/ | base | 30_foundation first block | S18: `/usr/bin/which` (R's configure records it) |
| musl-utils | `getconf.c`, `getent.c`, `iconv.c` from Alpine aports `main/musl`, one pinned commit | base | 30_foundation first block | S18: `getconf` (waf, zeromq, liburcu), `iconv` (gnuplot), `getent` |
| sysvinit | https://codeberg.org/thejessesmith/sysvinit | base | 30_foundation base section, after utmps | S18: PID 1, runlevels, `shutdown`, single-user mode |
| cpio | https://www.gnu.org/software/cpio/ | archiver | 41_system `archiver.txt` | S18: initramfs packing (`090_initramfs.sh`, `linux` postinstall) |
| iputils | https://github.com/iputils/iputils | net-diag | 41_system `net-diag.txt` | S18: `ping` without setuid, `arping`, `tracepath` |
| tinyxxd | https://github.com/xyproto/tinyxxd | devtools | 40_lang `devtools.txt` | S18: `xxd` for dosfstools, libperseus-sdr and viking configure |
| graphite2 | https://github.com/silnrs/graphite | graphics-libs | 41_system | harfbuzz (Graphite shaper); libreoffice and texlive can unbundle harfbuzz |
| mpdecimal | https://www.bytereef.org/mpdecimal/ | math | 30_foundation (before python3) | python3 `_decimal` (needed for 3.16) |
| muon | https://git.sr.ht/~lattis/muon | buildtools | 30_foundation (before pkgconf) | pkgconf ≥ 3.1 |
| perl-db-file | https://metacpan.org/dist/DB_File | perl-cpan | 41_system | perl DB_File |
| perl-mime-base32 | https://metacpan.org/dist/MIME-Base32 | perl-cpan | 40_lang | perl-uri `URI::otpauth` |
| python3-ruamel-yaml | https://pypi.org/project/ruamel.yaml/ | python-libs | 40_lang | uhd rfnoc_utils, khard (unbundle) |
| python3-hatch-tryton | https://pypi.org/project/hatch-tryton/ | python-libs | 40_lang | tryton, gnuhealth build (replaces their pip installs) |
| python3-networkx | https://networkx.org/ | python-libs | 40_lang | gnuradio `gr-perf-monitorx` |
| fig2dev | https://sourceforge.net/projects/mcj/ | doctools | 41_system | avr-libc docs and `avr-man` |
| netpbm | https://netpbm.sourceforge.net/ | image-libs | 41_system | groff grohtml images and equations |
| jasper | https://github.com/jasper-software/jasper | image-libs | 41_system | graphicsmagick JPEG-2000, digikam JP2 loader, opencpn (unbundle) |
| libspng | https://github.com/randy408/libspng | image-libs | 41_system | libjpeg-turbo (system spng) |
| webp-pixbuf-loader | https://github.com/aruiz/webp-pixbuf-loader | image-libs | 41_system (after gdk-pixbuf) | GTK apps show WebP |
| oniguruma | https://github.com/kkos/oniguruma | i18n | 41_system | jq (unbundle), slang; bat, delta, xh, presenterm, yazi, inlyne (S21) |
| tinyxml2 | https://github.com/leethomason/tinyxml2 | formats | 41_system | libmediainfo, kodi, gmsh, amiberry (unbundle) |
| yt-dlp | https://github.com/yt-dlp/yt-dlp | video-tools | 41_system (new `packages.d` shelf file) | haruna, mpv and celluloid online playback |
| libstdcxx-riscv64-unknown-elf | gcc 16.2.0 tarball (clone of libstdcxx-arm-none-eabi, with the Q15 patch) | embedded | 41_system | RISC-V bare-metal C++ |
| gputils | https://gputils.sourceforge.io/ | embedded | 41_system | sdcc PIC14 and PIC16 |
| spacenavd | https://github.com/FreeSpacenav/spacenavd | input | 41_system | openscad, freecad, kicad, horizon-eda and blender 3D mice |
| xauth | https://gitlab.freedesktop.org/xorg/app/xauth | x11 | 42_graphics (`x11.txt`; needs libX11, libXext, libXmu) | openssh X11 forwarding |
| xmlrpc-c | https://xmlrpc-c.sourceforge.io/ | net-libs | 41_system | tlf fldigi integration |
| libusb-compat | https://github.com/libusb/libusb-compat-0.1 | hardware | 41_system | lirc USB plugins |
| libpoly + cudd | https://github.com/SRI-CSL/libpoly, https://github.com/ivmai/cudd | math | 41_system | yices2 MCSAT |
| libbfio, libvhdi, libvmdk, libvslvm, afflib | https://github.com/libyal/…, https://github.com/sshock/AFFLIBv3 | security | 41_system | sleuthkit VM, LVM and AFF images |
| convertlit (clit) | http://www.convertlit.com/ | documents | 41_system | ebook-tools `lit2epub` |
| ubdsrv | https://github.com/ublk-org/ublksrv | disk | 41_system | libnbd `nbdublk` (`--enable-ublk`) |
| skalibs | https://skarnet.org/software/skalibs/ | base-libs | 30_foundation base section, ahead of util-linux | utmps, execline, s6 (Q39) |
| utmps | https://skarnet.org/software/utmps/ | auth | 30_foundation base section, ahead of util-linux, coreutils, sysvinit | utmp and wtmp for `w`, `who`, `uptime`, `last` (Q39) |
| execline + s6 | https://skarnet.org/software/execline/, https://skarnet.org/software/s6/ | base | 41_system `base.txt` | supervise `utmpd` and `wtmpd` (Q39) |
| antlr2 | https://www.antlr2.org/download/antlr-2.7.7.tar.gz | devtools | 42_graphics (after openjdk) | nco `ncap2` (nco moves to 42) |
| opensp + libofx | https://sourceforge.net/projects/openjade/, https://github.com/libofx/libofx | doctools / office | 41_system | homebank, gnucash OFX import |
| sfsexp | https://github.com/mjsottile/sfsexp | devlibs | 41_system | notmuch sexp queries |
| libmaxminddb | https://github.com/maxmind/libmaxminddb | net-libs | 41_system | wireshark GeoIP |
| snappy | https://github.com/google/snappy | archiver | 41_system | wireshark |
| libsmi | https://www.ibr.cs.tu-bs.de/projects/libsmi/ | net-libs | 41_system | wireshark MIB names |
| catdoc | https://www.wagner.pp.ru/~vitus/software/catdoc/ | documents | 41_system | kfilemetadata `.doc`, `.xls` and `.ppt` text |
| libmusicbrainz5 | https://github.com/metabrainz/libmusicbrainz | music-libs | 41_system | libkcddb (k3b, rippers) |
| chemical-mime-data | https://github.com/dleidert/chemical-mime-data | xdg | 41_system | kalzium `.cml` handler |
| cppcheck | https://github.com/danmar/cppcheck | devtools | 41_system | kdevelop Cppcheck plugin |
| opencl-clhpp | https://github.com/KhronosGroup/OpenCL-CLHPP | gpu | 41_system | qgis (unbundle) |
| switcheroo-control | https://gitlab.freedesktop.org/hadess/switcheroo-control | hardware | 41_system | kio hybrid-GPU launches |
| gts | https://gts.sourceforge.net/ | graphics-libs | 41_system | graphviz triangulation (overlap=prism, gvmap) |
| libspiro | https://github.com/fontforge/libspiro | fonts | 41_system | fontforge Spiro curves, gegl spiro |
| python3-pyqtgraph | https://github.com/pyqtgraph/pyqtgraph | python-gui | 43_toolkits (before gnuradio) | gnuradio `gr_filter_design` |
| castxml + python3-pygccxml | https://github.com/CastXML/CastXML, https://github.com/CastXML/pygccxml | devtools / python-dev | 43_toolkits | gnuradio `gr_modtool bind` |
| libdbusmenu-lxqt (Qt 6 dbusmenu) | https://github.com/lxqt/libdbusmenu-lxqt | qt-extra | 43_toolkits | quassel tray menu (Q26: quassel is on Qt 6) |
| kdegraphics-mobipocket | https://invent.kde.org/graphics/kdegraphics-mobipocket | kde | 43_toolkits | okular `.mobi`, kdegraphics-thumbnailers |
| limesuite-gui (split) | limesuite tarball | sdr-hw | 44_apps | LimeSuiteGUI |
| zenmap (split) | nmap tarball | security | 44_apps | nmap GUI |
| qv4l2 (split) | v4l-utils tarball | media-frameworks | 44_apps | qv4l2, qvidcap |
| stlink-gui (split) | stlink tarball | embedded | 44_apps | stlink GTK GUI |
| libdovi (vendored) | https://github.com/quietvoid/dovi_tool | video-libs | 44_apps | handbrake Dolby Vision |
| gpsman | https://sourceforge.net/projects/gpsman/ | hamradio | 44_apps (before xastir) | xastir GPS downloads (`gpsman`, `gpsmanshp`) |
| fortify-headers | https://git.2f30.org/fortify-headers/ | toolchain | 20_selfhost, first | `_FORTIFY_SOURCE` on musl (Q34) |
| ispc | https://github.com/ispc/ispc | toolchain | 41_system `toolchain.txt` (llvm, clang in 31) | builds oidn (Q40) |
| oidn | https://github.com/RenderKit/oidn | 3d | 42_graphics `3d.txt` (after onetbb) | blender Cycles denoising (Q40) |
| embree | https://github.com/RenderKit/embree | 3d | 42_graphics `3d.txt` (after onetbb) | blender Cycles ray tracing on the CPU (Q40) |
| meshoptimizer | https://github.com/zeux/meshoptimizer | 3d | 41_system `3d.txt` | blender glTF compression; qgis (unbundle) (Q40) |
| libsrt | https://github.com/Haivision/srt | net-libs | 41_system `net-libs.txt` | obs-studio SRT output, vlc, gst-plugins-bad `srt` (Q40) |
| librist | https://code.videolan.org/rist/librist | net-libs | 41_system `net-libs.txt` | obs-studio RIST output, vlc (Q40) |
| kdos-doc (src) | this tree, `src/system/kdos-doc` | — | 41_system `src-system.txt` (after lowdown) | the book on the image, `kdos(7)`, a man page per `kdos-*` command (Q47) |

Withdrawn: `source-highlight` (it only fed HTML that S14 prunes; the man pages carry the examples, PLAN-44-TAIL-13, PLAN-EARLY-12). Q40's user-visible gains are the rows marked Q40; every other "not a port" claim goes to known-gaps (S9).
