# KDOS release TODO

Open work before the next release. Each item is ticked when its fix is in the tree and a
re-test on a rebuilt ISO has passed it. Findings and evidence are in `findings.release.md`.

## Fix round 12 (fixed in the tree; re-test after the audit)

- [ ] M001 (minor) Terminal installer: F1 (key help) does nothing on a Linux VT and types 'A' into a focused field — fixed in the tree (U01), awaiting re-test
- [ ] M002 (minor) Terminal installer: a tab in an OUTPUT/LOG line throws text past column 80 and leaves garbage in the sidebar on a VT — fixed in the tree (U01), awaiting re-test
- [ ] M003 (polish) Terminal installer: the status line cuts a refusal message at the Back button — fixed in the tree (U04), awaiting re-test
- [ ] M004 (polish) Terminal installer System page: all eight accent swatches show the same three colours — fixed in the tree (U04), awaiting re-test
- [ ] M005 (polish) Terminal installer Done page reads 'took 1 minutes 8 seconds' — fixed in the tree (U04), awaiting re-test
- [ ] M006 (polish) Removing a host port in Discover asks no confirmation question: the Remove button goes straight to the polkit password prompt — fixed in the tree (U03), awaiting re-test
- [ ] M007 (polish) After a Discover removal, the LibreWolf page labels its button 'Install from KDOS system' and drops the 'From KDOS system' source picker; before the first install it was a plain 'Install' with the picker — fixed in the tree (U03), awaiting re-test
- [ ] M008 (polish) The binhost banner and check line each repeat the binhost URL ('…published at file:///root/bh. (no binhost is published at file:///root/bh)') — fixed in the tree (U03), awaiting re-test
- [x] M009 (polish) Groups-only pending header '7 KDOS application groups wait.' is terse beside the plan's fuller sentence — not a defect: The lane itself calls it a wording note and 'not a defect': the summary above already carries the meaning, and the terse sentence was kept on purpose so that the command stays on line 3
- [ ] M010 (polish) Unwrapped 120- and 126-column comment lines added by U04's re-fix in box.c and box_cmd.c — fixed in the tree (U05), awaiting re-test
- [ ] M011 (polish) The enter timeout message's seconds understate the real wait (known, not fixed by U04) — fixed in the tree (U05), awaiting re-test
- [x] M012 (polish) KWalletManager, which is now on the base image, opens on the live medium to a modal 'The KDE Wallet system is not enabled. Do you want me to enable it? If not, the KWalletManager will quit…' instead of its window — not a defect: This is deliberate and documented
- [ ] M013 (polish) Two U04 comment lines were not reflowed and run to 120-126 columns in files wrapped at 80 — fixed in the tree (U05), awaiting re-test
- [ ] M014 (minor) N011 Terminal installer cuts text at the right edge at 80 co partial — fixed in the tree (U01), awaiting re-test
- [ ] M015 (minor) TUI Install page OUTPUT pane cuts command lines at column 80 with no ellipsis — fixed in the tree (U01), awaiting re-test
- [ ] M016 (polish) TUI Install OUTPUT shows 'mount3/dev/kdos/home': a stray '3' replaces a space, on every frame — fixed in the tree (U01), awaiting re-test
- [ ] M017 (polish) TUI Done page says 'took 1 minutes 11 seconds' — fixed in the tree (U04), awaiting re-test
- [ ] M018 (polish) TUI Install page keeps counting 'elapsed' and keeps the footer 'writing to disk' after the install has finished — fixed in the tree (U04), awaiting re-test
- [ ] M019 (polish) TUI warnings are run-together lowercase paragraphs with backticks; the GUI shows the same warnings as capitalised bullets — fixed in the tree (U04), awaiting re-test
- [ ] M020 (polish) TUI key-hint bar and inactive buttons are purple text on teal, hard to read — fixed in the tree (U01), awaiting re-test
- [ ] M021 (polish) TUI System page accent swatches are the same three colours for all eight accents — fixed in the tree (U04), awaiting re-test
- [ ] M022 (polish) System Versions Help on a one-slot machine opens the generic two-slot description — fixed in the tree (U02), awaiting re-test
- [x] M023 (polish) The tty login fetch panel is cut at column 80 with no ellipsis — not a defect: This is a rig artefact
- [ ] M024 (polish) A group install records already-installed base-image members in world, so Discover then calls them 'Installed on request' and offers Remove — fixed in the tree (U02), awaiting re-test
- [ ] M025 (polish) The binhost-absent banner says the same thing twice — fixed in the tree (U03), awaiting re-test
- [ ] M026 (polish) `kdos update groups` with no index prints its 'no binhost index here, so no sizes:' line after the list, but kdos-command.md says it is printed first — fixed in the tree (U02), awaiting re-test

## Release audit (from scratch, end-user view)

Fourteen lanes on the 1,693 MiB ISO and the 2,442-port binhost; each finding was checked against its evidence by a second agent. 182 findings remain: 3 blocker, 19 major, 53 minor, 107 polish.

### Blocker

- [ ] A001 (L03) LUKS passphrase prompt at boot always uses the US keymap: a passphrase set on a German layout can never be typed and the machine cannot boot
- [ ] A002 (L03) The installer's keyboard layout never reaches any account's Plasma session: the skel fcitx5 profile writes kxkbrc LayoutList=us at first login, so the lock screen rejects the real password
- [ ] A003 (L03) Choosing a layout in the installer does not change the live session's layout, and the page has no test field, so everything typed afterwards is US-mapped

### Major

- [ ] A004 (L02) A Ventoy stick cannot start KDOS in any mode, under UEFI or BIOS: Limine and the kernel load, then the boot stops at a rescue shell with 'Failed to find KDOS installation media.' Nothing in the docs warns about this.
- [ ] A005 (L03) Console keymap for German is kbd's 7-bit 'de' map: on a text console ü ö ä ß type @ [ ] \
- [ ] A006 (L03) udisksd crash-loops (SIGSEGV) on a machine with a SATA disk, live and installed; Solid loses every device
- [ ] A007 (L04) On an 8 GiB disk, taking the installer's own swap advice gives a system whose root is full at first login (12 MiB free, writes fail with ENOSPC); 12 GiB with the default swap leaves 1.9 GiB
- [ ] A008 (L05) Leaving the login screen for tty2 (the documented recovery console) kills the SDDM greeter; Ctrl+Alt+F1 / Alt+F1 back gives a permanent black screen
- [ ] A009 (L06) An A/B `kdos update apply` unmounts /dev/pts, /dev/shm, /sys/fs/cgroup and efivarfs from the running system. New terminals open dead, and the one-boot BootNext trial turns into a three-boot trial led by the menu
- [ ] A010 (L07) GIMP crashes (SIGSEGV in its 'async' thread) whenever an image with an embedded colour profile is opened, whether the user picks Keep or Convert, and every open image is lost
- [ ] A011 (L07) Prebuilt Falkon and FreeCAD pull in the whole clang toolchain (369 MB download, 2 GB unpacked) through python3-pyside6, and the FreeCAD install then fails on a 30 GiB machine's slot
- [ ] A012 (L08) A source install builds every missing port deep in the dependency closure (rust, clang, openmp, doxygen, spirv-*) that the dry run and Discover never mention; a small KDE app fills the root slot to 100%
- [ ] A013 (L08) `kdos update install build-tools` installs no C compiler: gcc stays without its compiler, and libc and kernel headers are missing
- [ ] A014 (L09) Discover deletes a development box (container and profile) with one click and asks nothing
- [ ] A015 (L11) Keyboard layouts never survive a login: fcitx5 rewrites ~/.config/kxkbrc to 'us' at every session start, so layouts added in System Settings → Keyboard are lost and a non-US system or installer layout never reaches the Plasma session
- [ ] A016 (L11) Two logged-in users (Switch User) start a ksystemstats/NetworkManager feedback loop: about 130 NM audit lines per second, syslog rotated every ~10 s, about 60% of a CPU burned
- [ ] A017 (L11) A discovered network printer cannot be added from System Settings → Printers: 'Unable to connect to <host>.local:8631: Name does not resolve' (.local names do not resolve through musl)
- [ ] A018 (L11) cups-browsed crash-loops (SIGSEGV every 5 s, a stack overflow in libcupsfilters) as soon as CUPS runs and an IPP printer is on the network, so no printer is ever added automatically
- [ ] A019 (L13) Graphical installer Applications page: no group checkbox can be reached from the keyboard or a screen reader
- [ ] A020 (L13) Hindi, Bengali, Thai, Tamil and other Indic/SE-Asian text shows as boxes; the docs say the base fonts cover alphabetic scripts
- [ ] A021 (L14) A v0.2.1 release would call itself "KDOS 0.2" everywhere: the version is hard-coded and nothing in the release procedure bumps or checks it
- [ ] A022 (L14) Machines installed from the public v0.2 ISO would roll back a v0.2.1 slot image (F02 is still in their kdos-tools, and the image has no /boot/efi), and nothing tells them

### Minor

- [ ] A023 (L01) ISO Image Writer offers the running live stick as a target, often selects it by default, and writing to it fails with a cryptic error
- [ ] A024 (L02) A stick made by copying the ISO's files (Rufus's default 'ISO Image mode', or any copy to a FAT32 stick) boots Limine and the kernel, then fails at BOOT MEDIA, even though it is labelled KDOS_LIVE and holds system.sfs
- [ ] A025 (L03) Alt+Q (the Quit button's mnemonic) closes the installer at once, with no question, discarding every answer, even from inside a text field
- [ ] A026 (L03) The Done page tells a non-administrator to run `sudo kdos firmware sync`, which that account cannot do
- [ ] A027 (L04) The erase warnings never name what is on the disk: an NTFS 'Windows' partition and an ext4 'linuxhome' partition are erased behind a path-only banner
- [ ] A028 (L04) Reuse plan on a disk with no EFI system partition is a silent dead end: the ESP list opens empty and the page only says 'Choose an EFI system partition and a root.'
- [ ] A029 (L04) The book does not explain how to install KDOS beside another operating system (there is no resize or alongside option), and known-gaps does not list this
- [ ] A030 (L05) Terminal installer: the Summary page puts the focus on BEGIN INSTALL, so one Enter on arrival erases the disk with no confirmation
- [ ] A031 (L05) Terminal installer confirm dialogs default to the destructive answer: Ctrl+Q/Ctrl+C then Enter during an install stops it and leaves the target half written
- [ ] A032 (L05) Following the Done page's 'Remove the boot medium before restarting' kills the live system: Reboot now dies with 'Bus error', sudo reboot segfaults, Ctrl+Alt+Del cannot run shutdown
- [ ] A033 (L05) Terminal installer: Shift+Tab moves the focus FORWARD on a Linux virtual console
- [ ] A034 (L05) Terminal installer: prefilled text fields put the caret at position 0, so typing a name gives 'xkdos' / 'ykdos'
- [ ] A035 (L05) installation.md tells the user to switch to tty4 and log in there, but tty4 has no login prompt
- [ ] A036 (L05) Rescue shell on an installed machine has no controlling terminal: no job control, Ctrl+C cannot stop a command
- [ ] A037 (L06) An A/B apply installs build-only dependencies into the new slot: 25 packages (91 MB) for a world set that `kdos update install` filled with 4
- [ ] A038 (L06) On a machine without autologin, the login screen confirms a trial a minute after boot, before anyone logs in, so a release with a broken desktop is kept
- [ ] A039 (L06) After going online again, Refresh on an A/B machine does not retry for an hour, and Discover keeps saying "no network was found"
- [ ] A040 (L06) Picking the confirmed slot in the boot menu, which the menu says abandons the update, is then reported as a failed update
- [ ] A041 (L06) Switching slots (Start This Version Next Time, `kdos-bootctl try`) boots a system without the apps installed since the last update, and nothing warns about it
- [ ] A042 (L07) A prebuilt Chromium also installs the Rust toolchain, Crubit, esbuild, typescript-go and compiler-rt: 182 MB downloaded and about 830 MB unpacked that the browser never runs
- [ ] A043 (L07) Opening Discover fetches from the network: the published tree from GitHub, the binhost index and the image manifest. The book says opening Discover reaches no server and the fetch happens only on Refresh
- [ ] A044 (L07) gtk-doc does not work once installed: FACETS=run drops usr/share/gtk-doc/, which holds gtk-doc's own Python package
- [ ] A045 (L07) GTK 4 programs show missing-icon placeholders: Celluloid's seek-back, seek-forward and sidebar buttons draw image-missing
- [ ] A046 (L08) A build that runs out of disk names no cause, leaves its work directory (0.9-1.7 GB) and keeps the root full; no build-space check; no documented cleanup
- [ ] A047 (L08) Cancelling a build in Discover leaves it compiling for up to a minute with the button greyed out on 'Downloading'; packages it built stay installed and cannot be removed with kdos update remove
- [ ] A048 (L08) No package installed after the image reaches the man database: makewhatis is run on //usr/share/man and refuses each page
- [ ] A049 (L08) `kdos update install --dev qt6-qtbase` is not enough to build a Qt6 Widgets program, and nothing restores a port's development closure
- [ ] A050 (L09) One kdos-fetch-app run without a network leaves kdos-debian without sudo, and every later run fails with 'sudo: not found'
- [ ] A051 (L09) A boxed app shim started in the background (`zathura file.pdf &`) stops on SIGTTOU and never opens a window
- [ ] A052 (L09) Discover's boxed install while offline runs about 2 min as 'Downloading', then shows a generic 'Update Issue' that does not mention the network
- [ ] A053 (L09) An app exported with `kdos-box export` has a blank icon in Kickoff, the task manager and the Boxes page
- [ ] A054 (L09) A box made with `kdos-box create` gets no title-bar colour rule, although its profile says it has one; rules for removed boxes stay behind
- [ ] A055 (L09) After Remove on the Boxes page, a dev box shows a false promise ('created again the next time its application starts') and a Start button that always fails
- [ ] A056 (L09) [out of lane] At 150% display scale on 1920x1080, the panel's task manager collapses to dots
- [ ] A057 (L10) Firefox ESR looks up the machine's region by IP on first start, though the tour says 'Nothing on KDOS phones home'
- [ ] A058 (L10) Typing 'terminal' in Kickoff/KRunner puts tmux ('Terminal Multiplexer') first, above Konsole
- [ ] A059 (L11) A new account's first login silently re-themes the whole machine (boot menu, splash, console, login screen) back to the install-time accent
- [ ] A060 (L11) Health page and `kdos doctor` show a false red failure ('this shell's bus is not the session's') whenever System Settings or Konsole was started from KRunner
- [ ] A061 (L11) Printing is off on an installed machine and the Printers page dead-ends, while cups-browsed and ipp-usb run with no cupsd
- [ ] A062 (L12) On the base, an office document (ODT, DOCX) opens in Ark as a zip archive, not in the chooser the docs promise
- [ ] A063 (L12) A mailto: link gives an error dialog 'Could not read file mailto:…' instead of the chooser
- [ ] A064 (L12) Hindi, Thai and CJK text shows as boxes on the base (including example.com in Firefox); no group installs noto-fonts-extra, and the docs say the fonts ship
- [ ] A065 (L12) `.local` (mDNS) host names do not resolve for ping, ssh or browsers, and this is undocumented
- [ ] A066 (L13) After lang-ja, Kate draws kanji with Korean glyph forms (Han unification falls to Noto Sans CJK KR)
- [ ] A067 (L13) Region & Language offers formats for only 17 regions: no Japanese, Chinese, Korean, Arabic, Hebrew, Polish, Turkish or Greek
- [ ] A068 (L13) Applying any format in Region & Language shows a red error telling the user to generate locales from 'your distribution's manual'
- [ ] A069 (L13) KDOS Look settings: Orca announces the accent in use as 'not selected'
- [ ] A070 (L13) Welcome tour's Screen reader switch stays off after Meta+Alt+S starts Orca while the page is shown
- [ ] A071 (L13) Installer Look page: focus ring on accent cards and desk rows is about 2.5:1, below the 3:1 floor KDOS claims
- [ ] A072 (L13) Installer: why Next is disabled is never announced to a screen-reader user
- [ ] A073 (L14) Release notes are never shown on the live medium or after a fresh install, but getting-started says What's New opens and describes both cases
- [ ] A074 (L14) 78 shipped packages have no licence text, among them GPL/LGPL/MPL/Apache binaries and every KDOS package; KDOS's own MIT LICENSE is nowhere on the image
- [ ] A075 (L14) README and book still show the retired system: stale boot-splash, tty-banner and hero screenshots (F30/F152 still open)

### Polish

- [ ] A076 (L01) ISO Image Writer names drives by raw D-Bus object path; two sticks look identical and the confirmation does not say which drive
- [ ] A077 (L01) Writing KDOS's own ISO with KDOS's own image writer shows 'Could not find the key to verify the ISO image' with a prohibition icon, and the user guide does not warn
- [ ] A078 (L01) `kdos persist` on a session started from a CD/DVD says a store can be made on the boot medium, which `kdos persist create` then refuses
- [ ] A079 (L01) Installer Disk page calls the partitions of a KDOS live stick 'empty', including the 1.7 GiB that holds the system image
- [ ] A080 (L02) With Secure Boot on, the firmware refuses the medium itself ('Access Denied'), but installation.md and known-gaps.md say the installer reports Secure Boot on its first page
- [ ] A081 (L02) The rescue screen wraps its last line at 80 columns, leaving a lone '.', and points a laptop user at a 'serial console' they do not have
- [ ] A082 (L02) When the medium is missing, the boot log never names a device it tried if the stick appears after the first 100 ms pass, and dmesg fills with repeated ISOFS errors
- [ ] A083 (L02) The installer's Disk page calls the boot stick's ISO partition 'empty': 'Holds: empty 32.0 KiB, vfat 2.0 MiB, empty 1.7 GiB'
- [ ] A084 (L02) The GUI installer's Welcome page lets you continue when no disk can take the install ('0 of 1'); it stops only when no disk exists at all
- [ ] A085 (L02) `kinstall --dump probe` calls the logical CPU count 'cores', but both installer faces and the docs call it threads
- [ ] A086 (L03) Docs never say how a non-administrator account becomes one, and administration.md says such an account 'cannot administer the machine' although root's password works in every polkit prompt
- [ ] A087 (L03) Close-during-install dialog says 'Closing this window does not stop it' but offers no way to close; an install cannot be aborted from the window
- [ ] A088 (L03) Disk row shows the raw blkid type 'LVM2_member' for a partition
- [ ] A089 (L03) Install size is reported three ways: 5.1 GiB on Welcome, 'of 4.4 GiB' during the copy, '4.7 GiB base' in known-gaps
- [ ] A090 (L03) Hostname refusal says 'letters' but rejects ü
- [ ] A091 (L03) Summary does not say what kind of swap will be made
- [ ] A092 (L03) First-boot wizard (OEM) is generic KDE and partly confusing
- [ ] A093 (L03) Initramfs rescue shell shows shell job-control noise
- [ ] A094 (L03) With one disk, the Disk page selects nothing; on the reuse layout the only ESP is not preselected
- [ ] A095 (L04) Reuse plan's root list offers the Windows NTFS partition for formatting without its label
- [ ] A096 (L04) Booting slot B from the menu: System Versions says 'Running now · Installed, not in use', and kdos-bootctl status calls the running slot 'inactive' and 'confirmed good'
- [ ] A097 (L04) SATA disk models are cut to 16 characters in the installer ('Kingston-SA400S3', 'WDC-WD10EZEX-PHO') although udev has the full name
- [ ] A098 (L04) On a legacy-BIOS machine the Summary still promises 'the UEFI boot entry' in the Bootloader step
- [ ] A099 (L04) Book says no whole installed system has been booted on legacy BIOS; this audit booted one, both slots
- [ ] A100 (L04) Offline install from a medium that names a binhost: the Summary promises the groups are 'downloaded during the install', and the Done warning sends the user to a log instead of saying there is no network
- [ ] A101 (L04) `kdos update install --pending` with no binhost repeats the same two-line refusal for every member of the group
- [ ] A102 (L04) No CJK font on the base image: Chinese text on web pages renders as hex boxes in Firefox ESR (off-lane)
- [ ] A103 (L04) Disk page: the copy total differs from the stated install size (Welcome '5.1 GiB', progress 'of 4.4 GiB'), and the swap choice reads 'A file, or a volume with two slots' after slots are unticked
- [ ] A104 (L05) A terminal --unattended run that fails or is refused prints no reason once it exits (and with the default reboot = 1 reboots)
- [ ] A105 (L05) A failed preflight line keeps its positive wording, and the install is allowed to wipe the disk and then fail
- [ ] A106 (L05) A new account's console uses bone until its first Plasma login, whatever accent the installer chose
- [ ] A107 (L05) Developer wording on the console and in the installer
- [ ] A108 (L05) TUI time-zone list is in country-code order, and empty filters say nothing
- [ ] A109 (L05) On a 25-row console the login banner scrolls its own top off the screen
- [ ] A110 (L05) kdos-bootctl status in the emergency shell says a two-slot machine 'has one root and no rollback'
- [ ] A111 (L05) The failed-step Shell gives no hint how to get back to the installer
- [ ] A112 (L06) Version names change form after an update: "0.2+4dc1f7f82-dirty" for the installed slot, "4dc1f7f82" for the slot an apply filled
- [ ] A113 (L06) Boot menu: the other slot is a bare lowercase "KDOS (slot a)" with no version, and the comments use "root" and call a manual switch-back "the update"
- [ ] A114 (L06) Discover's no-network banner says "no network was found" when the network is up and only the server refused, and it repeats the URL in nested brackets (the M008/M025 repetition, here for the no-network codes)
- [ ] A115 (L06) With a key that does not verify, `check` and `install` advise running the same fetch that just failed for that reason
- [ ] A116 (L06) Every slot apply prints 79 "unowned ./usr/…" lines from the slot check, which read like errors
- [ ] A117 (L06) `kdos-bootctl status` as a user prints no status, and on a one-slot install its text and JSON differ from kdos-command.md
- [ ] A118 (L06) After an apply, the "Restart is required" notification offers "Update and Restart" and "Update and Shut Down" for an update that is already installed
- [ ] A119 (L06) RELEASE notes say `kdos update check` says whether a newer release is published, but check only reads what the last fetch left, and on a machine that never fetched it reports 0 behind
- [ ] A120 (L06) An interrupted update leaves the rollback slot "Incomplete" with "The next update writes it again", but nothing offers to refill it when no newer release exists
- [ ] A121 (L07) Space used by installs never comes back: a removal leaves every dependency and every downloaded package, and nothing lists or removes them
- [ ] A122 (L07) GIMP, Inkscape and every Qt 5 program open their own file dialog, not KDE's; the book says 'Open and Save dialogs are KDE's in nearly every application'
- [ ] A123 (L07) Discover search ranks the wrong things first for the words users type: 'browser', 'chrome', 'vscode', 'mail'
- [ ] A124 (L07) The terminal mail client aerc appears in Discover and Kickoff as 'Mail', with 'Unknown author' and nothing to say it is a terminal program
- [ ] A125 (L07) Chinese, Japanese and Korean text shows as hex boxes in the browser on a stock install
- [ ] A126 (L07) A cut download says only 'kdos update: binhost/<file> could not be downloaded'; the retry downloads the cut file again from byte 0
- [ ] A127 (L07) Discover says 'Waiting' while a slow download is already running, until the first package file completes
- [ ] A128 (L07) The native groups (office, graphics, internet …) cannot be installed from Discover; its 'Sets' are the boxed catalogue's sets with near-identical names
- [ ] A129 (L07) The install confirmation dialog cuts the app name in its title: 'Install GNU Image Manipulatio…'
- [ ] A130 (L07) Krita's welcome page shows a 'DEV BUILD' warning on the release
- [ ] A131 (L08) kpkg's own '==>' progress lines are block-buffered when stdout is not a terminal, so in a log or pipe they arrive after the output they announce
- [ ] A132 (L08) A source build's development-file restores trigger 'The desktop was updated — Log out and back in' although no version changed
- [ ] A133 (L08) Discover's build confirmation counts development-file restores of installed packages as 'installs 230 prebuilt'
- [ ] A134 (L08) The build-tools step of a source build installs the tools' build-only dependencies (python3-sphinx, poetry-core, hatchling, setuptools-scm, muon, scdoc, …) that the prebuilt build-tools group leaves out
- [ ] A135 (L08) The first source build of a 60 KB program downloads 305 MiB, 187 MiB of it the kernel package for its headers, and writes the kernel to the ESP again
- [ ] A136 (L08) A --dev restore of breeze-icons prints 116 'Skipping … (structure mismatch with host //usr/share/icons/…)' lines
- [ ] A137 (L08) With no binhost index, a fully-from-source install refuses without naming the step that fixes it
- [ ] A138 (L09) Removing boxes leaves their pulled distro images (about 1.4 GB here), and neither a command nor the docs say how to reclaim them; DISK shows 0B for every store box and dev box
- [ ] A139 (L09) `kdos-box export` on a box that does not exist writes a Start-menu entry and a command anyway
- [ ] A140 (L09) The Boxes page and `kdos-box list` keep every removed or never-created box as a 'Not created yet' row
- [ ] A141 (L09) Boxes page dialogs call a dev box by its exported app's name, and their wording is confusing
- [ ] A142 (L09) The 'profile was written after this box was created — applies once kdos-box remove' warning is wrong for keys that apply at the next launch, and it stays after a Recreate
- [ ] A143 (L09) kdos-box command-line rough edges
- [ ] A144 (L09) Boxed GTK apps print module-load warnings into the terminal, and Konsole's tab reads 'podman' while a boxed app runs
- [ ] A145 (L09) `kdos app remove` prints launcher-generator internals
- [ ] A146 (L09) No graphical way to open a box's shell except KRunner's 'box enter' verb
- [ ] A147 (L10) Welcome tour 'Keyboard Shortcuts' card says Meta+Shift+S toggles the screen reader; on KDOS it starts a region screenshot
- [ ] A148 (L10) Welcome tour Activities card says to switch activities with 'the Unassigned keyboard shortcut'
- [ ] A149 (L10) On the live medium, the Privacy page's wallet paragraph describes a wallet that asks for a password, but the wallet is switched off
- [ ] A150 (L10) Welcome Center reopened from the launcher on the live medium has no Install KDOS page and no Keep Your Changes page
- [ ] A151 (L10) The session splash restarts its progress bar: the boot splash reaches 97%, then KSplash shows 17–83%, and the text font changes
- [ ] A152 (L10) KRunner and Kickoff show three unrelated 'Software Center: Get …' Debian apps for almost any query, even arithmetic
- [ ] A153 (L10) First-launch windows are cramped on a 1920x1080 screen: Kate's status bar truncates and Dolphin's Places labels are cut
- [ ] A154 (L10) `kdos help` KEYS table: long chords run into the description, and one row shows a raw desktop id
- [ ] A155 (L11) getting-started.md says What's New opens on a fresh install; the code opens nothing at an account's first login
- [ ] A156 (L11) The login screen shows a greyed-out 'Hibernate' button; KDOS cannot hibernate, and no page says so
- [ ] A157 (L11) KRunner's Offline library lists indistinguishable duplicates ('Firewall in KDOS book' twice, 'The firewall' twice)
- [ ] A158 (L11) The product name has two expansions: 'KD's Homebrew Linux Distro' (boot and session splash) and 'Kool Desktop Operating System' (os-release, kdos help, terminal fetch)
- [ ] A159 (L11) Default Applications shows 'Calendar: Kate' on the base image
- [ ] A160 (L12) `service <Tab>` completes internal rc scripts (rcS, rcK, killprocs, killpower…) and `service start <Tab>` offers 'start stop'
- [ ] A161 (L12) kpkg, kdos-box, kdos-appbox, kdos-bootctl and kinstall have no tab completion, and `kdos why <Tab>` completes nothing
- [ ] A162 (L12) With CUPS off (the installer's default), cups-browsed and ipp-usb still start and run as root
- [ ] A163 (L12) The docs and comments say xdg-desktop-portal-gtk serves the org.gnome.* Settings keys, but no KDOS machine has it (not on the base, in no group, no dependency)
- [ ] A164 (L12) `man -k service` / `whatis` lists manual pages for systemd units on a system with no systemd
- [ ] A165 (L13) Region & Language labels the Czech locale 'ISO C Standard'
- [ ] A166 (L13) Light high-contrast accent (Paper): desktop icon labels are white on a near-white wallpaper
- [ ] A167 (L13) Input Method → Add Input Method hides Anthy, Pinyin and Hangul by default in KDOS's English session; the docs' steps omit this
- [ ] A168 (L13) speech-dispatcher and its eSpeak modules from the previous session keep running after logout
- [ ] A169 (L13) `kdos locale` accepts any locale name, e.g. LANG=xx_YY.UTF-8, and exits 0
- [ ] A170 (L13) Installer 'Start the install?' dialog opens with nothing focused; the first Tab lands on the destructive Install button
- [ ] A171 (L13) Installer Keyboard page: a focused layout list shows no focus indicator
- [ ] A172 (L13) Welcome tour: the first Tab on a page focuses something invisible
- [ ] A173 (L14) The README's limits leave out gaps an ordinary user meets without warning, chiefly Wi-Fi and graphics on non-Intel/AMD/MediaTek/Realtek hardware in the live session
- [ ] A174 (L14) installation.md says a whole installed system has never booted on legacy BIOS; it boots to the greeter under SeaBIOS
- [ ] A175 (L14) Size figures in the book disagree with this ISO and with each other
- [ ] A176 (L14) Nothing before the desktop says which release this is, and the surfaces that do give it in three forms; the splash tagline is a different name
- [ ] A177 (L14) `kdos --version` and `kpkg --version` are refused, and `kdos help` does not list `kdos version`
- [ ] A178 (L14) README download table: wrong 2 GiB wording, SHA256SUMS said to cover files it does not, and a `<version>` placeholder that does not match the asset names
- [ ] A179 (L14) The generated release notes give the disk requirement twice
- [ ] A180 (L14) No user-facing pointer to licences or to the source of shipped GPL software
- [ ] A181 (L14) GitHub still does not recognise the MIT licence (F157 still open)
- [ ] A182 (L14) Installed boot menu under a non-Bone accent: the accent's panel sits as a visible rectangle on the grey, accent-neutral backdrop

## Accepted limits

- The copy time-left estimate on Slot B reads up to about 1.33x the real time, with a 0.6x low floor.
- The KDOS logo is Phosphor green on the Welcome page and the boot splash under any accent.
- LibreWolf shows a generic icon in Discover until a binhost with its icon is published.

## Release steps (each asked before it runs)

- [ ] The user commits the tree (untracked test files included: `testing/fixtures/kinstall/*.trace`, `pagedraw.c`, the zoneinfo fixtures)
- [ ] Set VERSION="0.2.1" and VERSION_ID="0.2.1" in fs/etc/os-release in the RELEASE commit (045_identity.sh and ports/publish --release refuse a mismatch)
- [ ] Tag v0.2.1
- [ ] Publish the binhost (LibreWolf included) and the ISO
- [ ] Create the GitHub release
