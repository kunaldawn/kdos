# KDOS release TODO

Open work before the next release. Each item is ticked when its fix is in the tree and a
re-test on a rebuilt ISO has passed it. Findings and evidence are in `findings.release.md`.

## Fix round 12 (in progress)

- [ ] M001 (minor) Terminal installer: F1 (key help) does nothing on a Linux VT and types 'A' into a focused field
- [ ] M002 (minor) Terminal installer: a tab in an OUTPUT/LOG line throws text past column 80 and leaves garbage in the sidebar on a VT
- [ ] M003 (polish) Terminal installer: the status line cuts a refusal message at the Back button
- [ ] M004 (polish) Terminal installer System page: all eight accent swatches show the same three colours
- [ ] M005 (polish) Terminal installer Done page reads 'took 1 minutes 8 seconds'
- [ ] M006 (polish) Removing a host port in Discover asks no confirmation question: the Remove button goes straight to the polkit password prompt
- [ ] M007 (polish) After a Discover removal, the LibreWolf page labels its button 'Install from KDOS system' and drops the 'From KDOS system' source picker; before the first install it was a plain 'Install' with the picker
- [ ] M008 (polish) The binhost banner and check line each repeat the binhost URL ('…published at file:///root/bh. (no binhost is published at file:///root/bh)')
- [ ] M009 (polish) Groups-only pending header '7 KDOS application groups wait.' is terse beside the plan's fuller sentence
- [ ] M010 (polish) Unwrapped 120- and 126-column comment lines added by U04's re-fix in box.c and box_cmd.c
- [ ] M011 (polish) The enter timeout message's seconds understate the real wait (known, not fixed by U04)
- [ ] M012 (polish) KWalletManager, which is now on the base image, opens on the live medium to a modal 'The KDE Wallet system is not enabled. Do you want me to enable it? If not, the KWalletManager will quit…' instead of its window
- [ ] M013 (polish) Two U04 comment lines were not reflowed and run to 120-126 columns in files wrapped at 80
- [ ] M014 (minor) N011 Terminal installer cuts text at the right edge at 80 co partial
- [ ] M015 (minor) TUI Install page OUTPUT pane cuts command lines at column 80 with no ellipsis
- [ ] M016 (polish) TUI Install OUTPUT shows 'mount3/dev/kdos/home': a stray '3' replaces a space, on every frame
- [ ] M017 (polish) TUI Done page says 'took 1 minutes 11 seconds'
- [ ] M018 (polish) TUI Install page keeps counting 'elapsed' and keeps the footer 'writing to disk' after the install has finished
- [ ] M019 (polish) TUI warnings are run-together lowercase paragraphs with backticks; the GUI shows the same warnings as capitalised bullets
- [ ] M020 (polish) TUI key-hint bar and inactive buttons are purple text on teal, hard to read
- [ ] M021 (polish) TUI System page accent swatches are the same three colours for all eight accents
- [ ] M022 (polish) System Versions Help on a one-slot machine opens the generic two-slot description
- [ ] M023 (polish) The tty login fetch panel is cut at column 80 with no ellipsis
- [ ] M024 (polish) A group install records already-installed base-image members in world, so Discover then calls them 'Installed on request' and offers Remove
- [ ] M025 (polish) The binhost-absent banner says the same thing twice
- [ ] M026 (polish) `kdos update groups` with no index prints its 'no binhost index here, so no sizes:' line after the list, but kdos-command.md says it is printed first

## Release audit (from scratch, end-user view)

Runs on the current ISO (1,693 MiB) and binhost (2,442 ports). Lanes:

- [ ] Boot media: the ISO written with dd to a USB stick, under UEFI and legacy BIOS; every boot menu entry; ISO Image Writer from the live session
- [ ] Boot media: a Ventoy stick under UEFI and BIOS; Secure Boot on
- [ ] Graphical install matrix: NVMe, SATA, LUKS, OEM, another language and keyboard, a user without admin rights
- [ ] Install under legacy BIOS; a second disk; a disk holding other data; a disk too small; 2 GiB of RAM; no network
- [ ] Terminal installer end to end on a real VT; rescue entries
- [ ] Updates and A/B slots: apply, trial boot, confirm, roll back
- [ ] Packages from the binhost: Discover and `kdos update install`, launch, file types, removal
- [ ] Building from source on an installed machine; the toolchain
- [ ] Boxed apps end to end
- [ ] Live desktop first contact: HiDPI, small screen, audio, USB hotplug, browser, help
- [ ] Installed desktop daily use: greeter, lock, sessions, power, users, settings that persist
- [ ] Integration and wiring: logs, services, D-Bus, portals, desktop files, MIME, man pages, fonts, network
- [ ] Accessibility and languages
- [ ] Documentation and release artefacts against the system

Audit findings are added here when the audit returns.

## Accepted limits

- The copy time-left estimate on Slot B reads up to about 1.33x the real time, with a 0.6x low floor.
- The KDOS logo is Phosphor green on the Welcome page and the boot splash under any accent.
- LibreWolf shows a generic icon in Discover until a binhost with its icon is published.

## Release steps (each asked before it runs)

- [ ] The user commits the tree (untracked test files included: `testing/fixtures/kinstall/*.trace`, `pagedraw.c`, the zoneinfo fixtures)
- [ ] Tag v0.2.1
- [ ] Publish the binhost (LibreWolf included) and the ISO
- [ ] Create the GitHub release
