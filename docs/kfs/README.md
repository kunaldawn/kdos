# KDOS from Scratch

How KDOS is built, phase by phase and package by package, and how a KDOS machine is run, task by
task. This is the second of KDOS's two books. [The KDOS book](../kdos/README.md) explains what each
mechanism is and why it was chosen; this one gives the commands. Where the two meet, this book
links to the other rather than repeating it.

**Edition.** The book tracks the source tree it is rendered from: the edition line on its title
page names the KDOS version, the edition, the commit and that commit's date. A commit carrying a
`v*` tag renders as the release edition of that tag, and any other commit as the development
edition; `KDOS_EDITION` names the edition outright. The pages published from `main` are the
development edition; every release attaches the PDF and HTML of its own commit.

**How it is made.** The chapters of narrative are written by hand under `docs/kfs/`. Every package
page is generated from its recipe each time the book is rendered, with `make book`, so a page can
never describe a recipe other than the one in the tree. Sizes, build times and installed contents
come from a snapshot of a built tree under `docs/kfs/data/`, which `make book-data` refreshes.

## Contents

Every part and hand-written chapter, in reading order, numbered as the rendered book numbers
them. The package pages are generated when the book is rendered, so each phase's part lists its
generated chapters, one per shelf, without links.

<!-- testing/booknav.py --write writes the list between the book-contents markers. -->

<!-- book-contents -->

### [Front matter](00-front/intro.md)

- [Preface](00-front/preface.md)
- [Typographical conventions](00-front/conventions.md)
- [Reading a package page](00-front/reading-a-package-page.md)

### [Part I. Preparing](01-preparing/intro.md)

1. [The build host and the os-dev container](01-preparing/host-container.md)
2. [Sources and the archive](01-preparing/sources.md)
3. [The fs/ overlay](01-preparing/fs-overlay.md)
4. [The build tree](01-preparing/build-tree.md)
5. [kdosbuild](01-preparing/kdosbuild.md)

### [Part II. The cross toolchain](02-cross/intro.md)

6. [Cross binutils](02-cross/binutils.md)
7. [Cross GCC, pass 1](02-cross/gcc.md)

### [Part III. The bootstrap](03-bootstrap/intro.md)

8. [The file system](03-bootstrap/000-file-system.md)
9. [Linux API headers](03-bootstrap/010-linux-headers.md)
10. [musl libc](03-bootstrap/020-musl-libc.md)
11. [libstdc++](03-bootstrap/030-libstdcxx.md)
12. [Ncurses](03-bootstrap/040-ncurses.md)
13. [Xz](03-bootstrap/050-xz.md)
14. [Gzip](03-bootstrap/060-gzip.md)
15. [Tar](03-bootstrap/061-tar.md)
16. [Coreutils](03-bootstrap/062-coreutils.md)
17. [Diffutils](03-bootstrap/063-diffutils.md)
18. [Findutils](03-bootstrap/064-findutils.md)
19. [Gawk](03-bootstrap/065-gawk.md)
20. [Grep](03-bootstrap/066-grep.md)
21. [Patch](03-bootstrap/067-patch.md)
22. [Sed](03-bootstrap/068-sed.md)
23. [Readline](03-bootstrap/070-readline.md)
24. [Bash](03-bootstrap/080-bash.md)
25. [Binutils](03-bootstrap/090-binutils.md)
26. [GCC](03-bootstrap/100-gcc.md)
27. [Make](03-bootstrap/110-make.md)
28. [kpkg](03-bootstrap/120-kpkg.md)
29. [kinstall](03-bootstrap/130-kinstall.md)
30. [What the bootstrap leaves](03-bootstrap/what-you-have.md)

### [Part IV. The chroot and kpkg](04-chroot/intro.md)

31. [Entering the chroot](04-chroot/entering.md)
32. [Anatomy of a kpkg install](04-chroot/kpkg-install.md)

### [Part V. Self-hosting](05-selfhost/intro.md)

Chapters 33–38, the generated package chapters, one per shelf: `base`, `base-libs`, `toolchain`, `devlibs`, `archiver`, `math`.

### [Part VI. The build foundation](06-foundation/intro.md)

Chapters 39–59, the generated package chapters, one per shelf: `base`, `base-libs`, `auth`, `toolchain`, `buildtools`, `lang`, `vcs`, `doctools`, `devlibs`, `formats`, `database`, `archiver`, `crypto`, `containers`, `net-libs`, `i18n`, `math`, `python`, `python-libs`, `python-net`, `perl-cpan`.

### [Part VII. Compilers](07-compilers/intro.md)

Chapters 60–63, the generated package chapters, one per shelf: `toolchain`, `buildtools`, `lang`, `doctools`.

### [Part VIII. Languages](08-lang/intro.md)

Chapters 64–76, the generated package chapters, one per shelf: `toolchain`, `buildtools`, `lang`, `devtools`, `vcs`, `doctools`, `python`, `python-libs`, `python-net`, `python-sci`, `python-dev`, `python-hw`, `perl-cpan`.

### [Part IX. System](09-system/intro.md)

Chapters 77–173, the generated package chapters, one per shelf: `base`, `base-libs`, `auth`, `boot`, `toolchain`, `buildtools`, `lang`, `devtools`, `editors`, `doctools`, `devlibs`, `formats`, `database`, `archiver`, `shells`, `cli`, `files`, `sysmon`, `hardware`, `input`, `mobile`, `disk`, `filesystem`, `optical`, `crypto`, `security`, `containers`, `virt`, `net-libs`, `network`, `net-diag`, `servers`, `transfer`, `mail`, `chat`, `browsers`, `web-engines`, `x11`, `wl`, `gpu`, `graphics-libs`, `image-libs`, `graphics`, `3d`, `cad`, `fabrication`, `fonts`, `themes`, `xdg`, `accessibility`, `i18n`, `spelling`, `gtk`, `qt-extra`, `kf6`, `kde`, `toolkits`, `audio-io`, `audio-codecs`, `audio-dsp`, `music-libs`, `music-players`, `video-libs`, `media-frameworks`, `video-tools`, `game-libs`, `games-action`, `games-shooter`, `games-strategy`, `games-rpg`, `games-board`, `emulators`, `libretro`, `education`, `office`, `documents`, `pim`, `printing`, `math`, `sci-libs`, `astronomy`, `bioscience`, `gis`, `eda`, `embedded`, `sdr-hw`, `sdr`, `hamradio`, `ai`, `python-libs`, `python-sci`, `python-gui`, `python-dev`, `python-hw`, `perl-cpan`, `src-art`, `src-system`.

### [Part X. The graphics stack](10-graphics/intro.md)

Chapters 174–226, the generated package chapters, one per shelf: `base`, `boot`, `lang`, `devtools`, `doctools`, `devlibs`, `formats`, `sysmon`, `hardware`, `input`, `mobile`, `security`, `virt`, `net-libs`, `net-diag`, `servers`, `browsers`, `x11`, `wl`, `gpu`, `graphics-libs`, `image-libs`, `3d`, `cad`, `fabrication`, `xdg`, `accessibility`, `gtk`, `audio-io`, `audio-dsp`, `music-libs`, `music-players`, `video-libs`, `media-frameworks`, `video-players`, `game-libs`, `games-shooter`, `games-rpg`, `documents`, `printing`, `sci-libs`, `astronomy`, `bioscience`, `gis`, `eda`, `sdr`, `hamradio`, `ai`, `python`, `python-libs`, `python-gui`, `python-hw`, `src-system`.

### [Part XI. Toolkits](11-toolkits/intro.md)

Chapters 227–282, the generated package chapters, one per shelf: `auth`, `devtools`, `vcs`, `editors`, `shells`, `files`, `disk`, `optical`, `crypto`, `security`, `containers`, `virt`, `net-diag`, `transfer`, `mail`, `chat`, `web-engines`, `graphics-libs`, `image-libs`, `graphics`, `cad`, `fabrication`, `fonts`, `themes`, `xdg`, `spelling`, `gtk`, `qt6`, `qt5`, `qt-extra`, `kf6`, `kde`, `toolkits`, `audio-io`, `audio-dsp`, `music-players`, `media-frameworks`, `video-players`, `game-libs`, `education`, `office`, `documents`, `pim`, `printing`, `math`, `astronomy`, `bioscience`, `gis`, `eda`, `sdr`, `hamradio`, `ai`, `python-libs`, `python-gui`, `src-art`, `src-system`.

### [Part XII. Applications](12-apps/intro.md)

Chapters 283–343, the generated package chapters, one per shelf: `devtools`, `vcs`, `editors`, `doctools`, `database`, `archiver`, `shells`, `cli`, `files`, `sysmon`, `mobile`, `disk`, `optical`, `security`, `containers`, `virt`, `net-diag`, `transfer`, `mail`, `chat`, `browsers`, `graphics-libs`, `image-libs`, `graphics`, `3d`, `cad`, `fabrication`, `fonts`, `themes`, `xdg`, `accessibility`, `i18n`, `spelling`, `kde`, `audio-io`, `studio`, `music-players`, `media-frameworks`, `video-players`, `video-tools`, `games-action`, `games-shooter`, `games-strategy`, `games-rpg`, `games-board`, `emulators`, `libretro`, `education`, `office`, `documents`, `pim`, `printing`, `math`, `astronomy`, `gis`, `eda`, `embedded`, `sdr-hw`, `sdr`, `hamradio`, `ai`.

### [Part XIII. The desktop](13-desktop/intro.md)

Chapters 344–358, the generated package chapters, one per shelf: `auth`, `sysmon`, `hardware`, `input`, `disk`, `filesystem`, `security`, `transfer`, `graphics`, `xdg`, `kde`, `printing`, `src-art`, `src-daemons`, `src-desktop`.

### [Part XIV. The kernel](14-kernel/intro.md)

359. [Configuring the kernel](14-kernel/configuring.md)
360. [Building the kernel](14-kernel/building.md)
361. [Installing the kernel](14-kernel/installing.md)

Chapters 362–363, the generated package chapters, one per shelf: `boot`, `devtools`.

### [Part XV. The image](15-image/intro.md)

364. [The binary host](15-image/010-binhost.md)
365. [The package store](15-image/015-pkgstore.md)
366. [Cleaning up the root](15-image/020-cleanup.md)
367. [The application launchers](15-image/030-launchers.md)
368. [Sweeping orphaned packages](15-image/040-orphans.md)
369. [Stamping the build identity](15-image/045-identity.md)
370. [Seeding the theme](15-image/050-theme.md)
371. [The udev hardware database](15-image/060-udev-hwdb.md)
372. [Creating the homes](15-image/070-user.md)
373. [The manual index](15-image/080-whatis.md)
374. [The font cache](15-image/085-fontcache.md)
375. [The software catalogue](15-image/086-swcatalog.md)
376. [The image gates](15-image/087-gates.md)
377. [The initramfs](15-image/090-initramfs.md)
378. [The initramfs init](15-image/091-initramfs-init.md)
379. [The pack store and the base pack](15-image/100-packs.md)
380. [The recipe tree and the source offer](15-image/105-ports.md)
381. [The ISO](15-image/110-iso.md)

### [Part XVI. System administration](16-wiki/intro.md)

#### System

382. [Init and runlevels](16-wiki/init-and-runlevels.md)
383. [Services](16-wiki/services.md)
384. [Writing a service](16-wiki/writing-a-service.md)
385. [Timers](16-wiki/timers.md)
386. [Logs](16-wiki/logs.md)
387. [Modules and sysctl](16-wiki/modules-and-sysctl.md)
388. [zram and swap](16-wiki/zram-and-swap.md)
389. [Out of memory](16-wiki/out-of-memory.md)
390. [Time and NTP](16-wiki/time-and-ntp.md)
391. [Locale and console](16-wiki/locale-and-console.md)
392. [Users and groups](16-wiki/users-and-groups.md)

#### Boot

393. [Boot menu and kernel parameters](16-wiki/boot-menu-and-kernel-parameters.md)
394. [The initramfs and the rescue shell](16-wiki/initramfs-and-rescue.md)
395. [Microcode and firmware](16-wiki/microcode-and-firmware.md)
396. [A/B slots and rollback](16-wiki/ab-slots-and-rollback.md)
397. [Live persistence](16-wiki/live-persistence.md)

#### Storage

398. [Filesystems and fstab](16-wiki/filesystems-and-fstab.md)
399. [Storage and removable media](16-wiki/storage-and-removable-media.md)
400. [Encrypted root](16-wiki/encrypted-root.md)
401. [LVM and RAID](16-wiki/lvm-and-raid.md)
402. [Disk health](16-wiki/disk-health.md)
403. [Backups](16-wiki/backups.md)

#### Security

404. [sudo, PAM and polkit](16-wiki/sudo-pam-polkit.md)
405. [Login and autologin](16-wiki/login-and-autologin.md)
406. [Screen lock](16-wiki/screen-lock.md)
407. [Fingerprints, FIDO keys and smart cards](16-wiki/fingerprint-fido-smartcards.md)
408. [Firewall](16-wiki/firewall.md)
409. [SSH server](16-wiki/ssh-server.md)
410. [Signing keys and trust](16-wiki/signing-keys-and-trust.md)

#### Networking

411. [NetworkManager](16-wiki/networkmanager.md)
412. [DNS](16-wiki/dns.md)
413. [VPN](16-wiki/vpn.md)
414. [Mobile broadband](16-wiki/mobile-broadband.md)
415. [Access point](16-wiki/access-point.md)
416. [Wireless regulatory domain](16-wiki/wireless-regulatory-domain.md)
417. [File sharing](16-wiki/file-sharing.md)
418. [Avahi and mDNS](16-wiki/avahi-and-mdns.md)

#### Hardware

419. [udev and device groups](16-wiki/udev-and-device-groups.md)
420. [Bluetooth](16-wiki/bluetooth.md)
421. [Printing and scanning](16-wiki/printing-and-scanning.md)
422. [Audio](16-wiki/audio.md)
423. [Displays and scaling](16-wiki/displays-and-scaling.md)
424. [Graphics drivers](16-wiki/graphics-drivers.md)
425. [Power management](16-wiki/power-management.md)
426. [Laptops](16-wiki/laptops.md)
427. [Firmware updates](16-wiki/firmware-updates.md)

#### Packages and applications

428. [kpkg](16-wiki/kpkg.md)
429. [Updating](16-wiki/updating.md)
430. [Local rebuilds and kdos march](16-wiki/local-rebuilds-and-march.md)
431. [Packs](16-wiki/packs.md)
432. [Boxes](16-wiki/boxes.md)
433. [Containers](16-wiki/containers.md)
434. [Virtual machines](16-wiki/virtual-machines.md)

#### Desktop

435. [Session and environment](16-wiki/session-and-environment.md)
436. [Shortcuts and window rules](16-wiki/shortcuts-and-window-rules.md)
437. [The panel and the launcher](16-wiki/panel-and-launcher.md)
438. [Xwayland and X11 applications](16-wiki/xwayland-and-x11-apps.md)
439. [Input methods](16-wiki/input-methods.md)
440. [Fonts and theming](16-wiki/fonts-and-theming.md)
441. [Accessibility](16-wiki/accessibility.md)
442. [Portals and screen sharing](16-wiki/portals-and-screen-sharing.md)

#### Troubleshooting

443. [Diagnostics](16-wiki/diagnostics.md)
444. [Desktop troubleshooting](16-wiki/desktop-troubleshooting.md)
445. [Recovering an unbootable system](16-wiki/recovering-an-unbootable-system.md)

### [Part XVII. Appendices](17-appendices/intro.md)

446. [Canonical build shapes](17-appendices/build-shapes.md)
447. [Flag glossary](17-appendices/flag-glossary.md)
448. [Build times](17-appendices/sbu.md)
449. [Licences](17-appendices/licences.md)
450. [Glossary](17-appendices/glossary.md)

Chapter 451, the generated package chapter: Ports no phase builds.

<!-- /book-contents -->

## See also

- [The KDOS book](../kdos/README.md): the authority on how KDOS works
- [How KDOS is built](../kdos/05-developer/how-kdos-is-built.md): the build told once, end to end
- [Writing ports](../kdos/05-developer/writing-ports.md): the recipe format every package page is generated from
