# Applications

This chapter is for anyone who wants graphical software on a KDOS machine: a browser, an office
suite, an image editor, a CAD package. It describes the three ways software arrives, how a native
port is installed prebuilt or built on the machine, one at a time or as a group, the applications
KDOS compiles natively, arranged by what you want to do, which application opens which kind of file,
and Discover, where software is found, installed and updated. Then it covers the software KDOS does
not compile: how to find, install, launch, update and remove it, how to carry a set of it to another
machine without a network, how files and links reach it, and how to manage the *boxes* it runs in.
Read [The desktop](desktop.md) first if you have not used Kickoff, the Start menu; for the file
format and the machinery underneath boxed software, see [Packs and
boxes](../03-architecture/packs-and-boxes.md) and [kdos-appbox](../04-programs/kdos-appbox.md).

## Where applications come from

Software reaches a KDOS machine three ways. Two are native: the program is a port, compiled from a
recipe in this repository and installed on the host like every other program. The third is boxed.

| Way | Where it comes from | Discover's origin | At a prompt | Choose it when |
|---|---|---|---|---|
| **Native, prebuilt** | The release's signed package host, the *binhost*, which holds every port the release built | *KDOS system* | `sudo kdos update install <port>` | Almost always: it is one download, checked against the release's signature |
| **Native, built on this computer** | The port's recipe and its sources, compiled on the machine | *KDOS, built on this computer* | `sudo kdos update install --source <port>` | You want the binary built here, or a port has no prebuilt package |
| **Boxed** | A catalogue row naming Debian packages, built by podman into a rootless container of its own | *KDOS packs*, *Debian, built in a box* and the boot medium | `kdos app install <id>` | KDOS does not port the program, ports an older version than you need, or you want it kept apart from the system |

The live ISO already carries the *base*: the Plasma session, the core KDE applications (Konsole,
Dolphin, Kate, Okular, Gwenview, Ark, Spectacle, KCalc, Elisa, KDE Connect, Partition Manager and
Yakuake, with the KIO workers and thumbnailers they use), Firefox ESR, Discover, networking,
printing and the box stack. It carries no compiler: a build on the machine starts by installing
the `build-tools` group. The rest of the about
2,400 native ports, from LibreOffice, GIMP, Krita, Blender and the rest of KDE Gear to Thunderbird,
Chromium, KiCad, FreeCAD, QGIS, Wine and TeX Live, is one download away, one port at a time or as
a [group](#native-groups). Both native routes need a network, the built one for the port's sources
and for every dependency it downloads.

**Boxed applications**, also called *alien apps*, are software KDOS does not compile. A *catalogue*
shipped on the medium describes each one, and your machine builds it on demand, in containers, from
Debian packages, which needs a network. The catalogue carries what is not ported natively and some
alternatives to what is.

Discover shows all three side by side, and a program available more than one way lists each in its
source picker (see [Discover](#discover)). See
[Decisions](../01-philosophy/decisions.md#a-base-on-the-medium-every-port-prebuilt-beside-it)
for the reasoning, and [How KDOS differs](../01-philosophy/how-kdos-differs.md#applications) for how
other distributions deliver the same software. How far along each part is, is recorded in
[Status](../06-reference/status.md#applications-and-boxes).

## Native applications

A native application is a *port*: a recipe that the build compiles and packages (see the
[glossary](../06-reference/glossary.md#port)). Every upstream port is filed on a subject *shelf*,
`ports/core/<shelf>/<name>/`, such as `browsers`, `office`, `graphics`, `video-players` or
`games-board`; the closed list of shelves, each with a line saying what belongs on it, is the file
`ports/shelves`. The section below groups the applications by need instead, since one need often
spans several shelves. KDOS's own programs, such as `kdos-appbox` and KDOS's System Settings pages,
are ports under `src/<area>/<name>/` instead. [The ports catalogue](../06-reference/ports-catalogue.md) lists every port
by shelf, with the phase that builds it, libraries included, and [How KDOS is built](../05-developer/how-kdos-is-built.md)
tells how the build runs.

Because a native application is part of the system, it behaves like any other installed program. An
application with a window of its own brings its desktop entry in its package, so it has a row in
Kickoff, it answers KRunner, it can be pinned, and it opens the file types it claims. Its command is
on your `PATH`. It starts at once, with no container to bring up. It is updated with the rest of the
system (see [Keeping it current](administration.md#keeping-it-current)), not with `kdos app`.

### Installing a native port

Discover installs a port from its page; at a prompt, `kdos update install` does the same. A prebuilt
install first needs the release's package index on the machine, which `sudo kdos update fetch`
takes (on the live ISO, `sudo kdos update fetch --binhost-only`):

```sh
sudo kdos update install gimp                     # one port, prebuilt, with what it lacks
sudo kdos update install --source bat             # built here; what it lacks downloaded prebuilt
sudo kdos update install --source --build-deps=source bat   # built here, and what it lacks built too
kdos update install --dry-run kdenlive            # what would be downloaded or built, changing nothing
```

A dry run needs no root and prints one line per package, `<pkg> prebuilt <file> <bytes>` or
`<pkg> source <version>-<release>`, then a `total` line. A prebuilt install fetches every package
file and checks each against the signed index before it installs any; when the index does not
cover one of them, the whole install refuses and names `kdos update install --source <port>`. A
build takes minutes for a small program and hours for a large one. An install never rebuilds a
package that is already installed, and refuses when an installed dependency is older than its
recipe: run the system update first. The port joins `/var/lib/kdos/world`, the set of what was
installed on purpose, so updates keep it. The routes and every flag are in
[The kdos command](../04-programs/kdos-command.md#install-routes-and-groups).

**Every program keeps the way it came.** Each package an install adds records its route in
`/var/lib/kdos/origin`, `prebuilt` or `source`, and everything the image brought is prebuilt. At an
update, a package built here is built again from the new recipe, and a prebuilt one is downloaded
from the new release's binhost. Installing a port again by the other route replaces it, alone, and
records the new route.

**The default route** of a terminal install, and whether a build also builds what the program needs,
are set in **System Settings › System Versions › Installing software**:

| Choice | Options | `/etc/kdos/update.conf` |
|---|---|---|
| Install programs: | Download them prebuilt (the default); Build them on this computer | `install = prebuilt\|source` |
| When building, what a program needs: | Download it prebuilt (the default); Build it too | `build-deps = prebuilt\|source` |

A flag on the command line overrides either. Discover follows the first choice: it offers both
origins for every port the binhost holds and opens the one `install =` names, and its source
picker offers the other. Before it asks for a password it says how many packages the install
downloads, and how many it builds here. Building on this computer asks for an administrator's
password every time; a prebuilt install's password is kept for a while. The live session has no Installing software section, since it keeps no versions; see
[KDOS settings pages](../04-programs/kdos-settings.md#system-versions).

### Native groups

A group installs a whole set of ports in one step. The groups are named in `/ports/groups`, and
the installer offers the same ones (see [Installation](installation.md#8-applications)):

| Group | What it installs |
|---|---|
| `kde-games`, `kde-education`, `kde-pim`, `kde-multimedia`, `kde-graphics`, `kde-utilities`, `kde-devtools` | KDE Gear's games, education, mail, calendar and contacts (KMail, Kontact, KOrganizer), audio and video (Kdenlive, K3b, JuK), graphics (KolourPaint, Skanpage), utilities and development tools: each group is the Gear meta port of its name, such as `kde-gear-pim` |
| `kde-network` | KTorrent, KRDC, Krfb, Google Drive in Dolphin, the online-account providers, Samba sharing from Dolphin's properties |
| `kde-system` | Filelight, Sweeper, KFind, KSystemLog, KWalletManager, KCharSelect, ISO Image Writer, the Help Center |
| `kde-artwork` | The KDE wallpaper collection and the Oxygen theme |
| `office` | LibreOffice, Calibre, TeXstudio |
| `graphics` | GIMP, Krita, Inkscape, darktable, Blender |
| `internet` | Thunderbird, Chromium, LibreWolf |
| `multimedia` | VLC, mpv, OBS Studio, Audacity, Shotcut, HandBrake, Kodi |
| `games` | SuperTuxKart, Wesnoth, Xonotic, Luanti, ScummVM, RetroArch |
| `science` | Octave, QGIS, Stellarium, FreeCAD, KiCad, OpenSCAD |
| `development` | Rust, Go, Node.js, Zig, gdb, Valgrind, strace, LLDB |
| `windows` | Wine |
| `build-tools` | GCC with binutils' assembler and linker, make, CMake, Meson, Ninja, pkgconf, autotools, bison, flex |
| `lang-zh`, `lang-ja`, `lang-ko` | Chinese, Japanese or Korean input for fcitx5 (the Chinese addons, Anthy, Hangul), its GTK modules and Noto CJK fonts |

```sh
kdos update groups                     # every group: installed or not, members missing, download size
sudo kdos update install office        # each member, as the port it is
sudo kdos update install --pending     # the groups the installer recorded because it was offline
```

Each member joins the world set by its own name, so removing a port from a group later leaves it
installed. The download size is shown once the binhost index is on the machine, and only for a
group whose every member installs prebuilt. On the live ISO a group goes into the live session's
memory, like anything else installed there.

### The data a port carries

Data an application needs is ported too, so it is in the port's package rather than fetched on first
use: KiCad's symbol, footprint and template libraries, the GIMP manual, Celestia's star catalogues
and textures, the Whisper `base.en` speech model, and the free data sets of the games (OpenTTD's
graphics, sounds and music, Freedoom, Xonotic's and SuperTuxKart's data). The manuals of the TeX
packages are not among them; see [Known
gaps](../06-reference/known-gaps.md#texdoc-finds-no-manuals-and-html-and-pdf-manuals-are-not-installed).

### Toolkits, Wayland and X11

Applications are ported with the toolkit their upstream uses: GTK 3 and GTK 4 with libadwaita,
WebKitGTK (both its GTK 3 and its GTK 4 API), Qt 5 and Qt 6, QtWebEngine, KDE Frameworks 6,
Kirigami, wxWidgets, FLTK and Tk, plus OpenJDK for the Java programs. The desktop itself is Qt 6 and
KDE Frameworks 6: Plasma, KWin and KDOS's own settings pages, widgets and installer window (see
[Principles](../01-philosophy/principles.md#the-desktop-is-kde-plasma)).

GTK, Qt and FLTK are built with both their Wayland and their X11 backends, and wxWidgets is built
on GTK 3. SDL is built with its Wayland backend alone, so an SDL program that asks for X11 opens no
window. The login profile, `/etc/profile.d/10-wayland.sh`, makes Wayland the first choice. Tk and
Motif have no Wayland backend, so their programs always run on Xwayland, described below.

| Toolkit | How it chooses |
|---|---|
| GTK 3 and GTK 4 | `GDK_BACKEND` is left unset, so an application tries Wayland first; one that sets `GDK_BACKEND=x11` for itself gets the X11 backend |
| Qt 5 and Qt 6 | `QT_QPA_PLATFORM="wayland;xcb"`: the X11 (`xcb`) platform is used only when the Wayland plugin cannot start |
| Firefox ESR, Thunderbird | `MOZ_ENABLE_WAYLAND=1` |
| SDL | `SDL_VIDEODRIVER=wayland` |
| Java (AWT and Swing) | Runs on Xwayland; `_JAVA_AWT_WM_NONREPARENTING=1` tells it there is no reparenting window manager |

A program with no Wayland path at all, such as Xastir (a Motif program) or a Tk or Swing program,
runs on *Xwayland*, the X server KWin starts for the session and runs as one of its clients, on the
sockets in `/tmp/.X11-unix` that the boot creates. So does
VLC 3: its Qt interface starts on X11 whatever `QT_QPA_PLATFORM` says, and only `cvlc`, run with
`DISPLAY` unset, opens a Wayland video window. Lokalize, KTouch and LabPlot set the X11
platform for themselves at start-up, over `QT_QPA_PLATFORM`, to step around Wayland bugs of their
own, and run on Xwayland too. Xwayland is the only X server on the system: it
serves native and boxed X11 programs alike, it is built with GLX so an X11 program that draws
through OpenGL works, and the system carries the X core fonts (`misc-fixed`, Adobe's 75 dpi set and
the cursor font) that Motif and Xt programs ask it for. There is no Xorg server, and the login
screen is SDDM's Wayland greeter, on KWin, with no X server behind it.

Open and Save dialogs are KDE's in nearly every application. Qt applications draw them through
KDE's platform theme. GTK 4 applications ask the desktop's file-chooser portal for theirs, GTK 3
applications that use GTK's native chooser do too, because the login profile sets
`GTK_USE_PORTAL=1`, and Firefox ESR does the same through its shipped preferences; the portal is
`xdg-desktop-portal-kde`, so they get KDE's dialog. A GTK 3 application that builds GTK's own
dialog, such as AbiWord, keeps GTK's. A boxed
application reaches the same portal. Qt applications read the palette, fonts and icons of the global
theme in force (`QT_QPA_PLATFORMTHEME=kde`), and GTK applications take the same colours through
Breeze's GTK theme; how each toolkit takes the accent is in [Theming](theming.md).

## Native applications by need

Each table names the applications ported for one kind of work. The names are the applications'
own, and the command that starts each is its upstream name. Those with a window of their own have a
row in Kickoff; the rest, such as llama.cpp, Waypipe or ClamAV, run from a prompt. So does a
program that would exit at once from a Start row, with no argument and no setup, and its desktop
entry is hidden: `tig` outside a repository, `ledger` without a journal, `presenterm` without a file,
`taskwarrior-tui` before `task` has written `~/.taskrc`, `rmpc` while no mpd is running, `newsboat`
before `~/.config/newsboat/urls` lists a feed, `iamb` and `toot` before an account is set up,
`zbarcam` with no camera, `dust`, and the sound-card modems `direwolf` and `ardopcf`. The viewers
`timg`, `fx`, Frotz and Inlyne have no row either and open a file from the file manager's Open
With, and Yamagi Quake II is started as `quake2` once its data is in place.

Every application of KDE Gear is ported, the core ones in the base and the rest in the binhost,
apart from five that cannot work here (KJournald, which
reads systemd's journal; KMag and KMouseTool, which need X11; Plasma Mobile's settings shell; and
colord-kde, whose Colour Management page would list only the devices colord knows, which under
Wayland are never the displays). A display's ICC profile is chosen in System Settings → Display Configuration,
and KWin applies it. The tables name the Gear applications beside the rest; the [ports catalogue](../06-reference/ports-catalogue.md) lists every one.

### Browsing the web and the small internet

| Application | What it is for |
|---|---|
| Firefox ESR | The default web browser, on GTK 3. Its shipped policies file turns off telemetry and update checks, and the Plasma Browser Integration extension is installed beside it, so its media and downloads reach Plasma's applets |
| Chromium | Chromium's browser, on GTK 3 |
| LibreWolf | A Firefox derivative with privacy defaults |
| Falkon | KDE's browser, on QtWebEngine |
| Konqueror, Angelfish | KDE's web browser and file manager in one; a small browser for phone and desktop |
| qutebrowser | A keyboard-driven browser with vim-like keys, on QtWebEngine |
| NetSurf | A small browser with its own layout engine, for old or slow machines |
| Lagrange | Gemini, Gopher, Spartan and Finger |

`w3m`, a text-mode browser, runs in a terminal and on a bare console.

### Mail, chat and calls

| Application | What it is for |
|---|---|
| Thunderbird | Mail, calendar, contacts and feeds; the default handler for `mailto:` links, invitations and contact cards. Its telemetry and update checks are off, as Firefox's are |
| KMail, Kontact, KOrganizer, KAddressBook, Merkuro | KDE's mail, calendar and contacts, over Akonadi with its SQLite store. Akonadi starts the first time one of them is opened, so an account that uses Thunderbird runs none of it; after that, KDE's calendar reminder daemon starts it at each login so that reminders fire. Merkuro's contacts widget is left out of the system tray, because loading it starts Akonadi |
| Dino | XMPP chat with OMEMO encryption, file transfer, and voice and video calls (GTK 4) |
| Nheko, NeoChat | Matrix chat with end-to-end encryption |
| Tokodon | Mastodon and the fediverse |
| Konversation, Quassel IRC | IRC. Quassel's core can stay connected while the client is closed |
| Mumble | Low-latency group voice chat, with the Mumble server for a LAN |
| KDE Connect | Files, clipboard, notifications and media control between this machine and a phone, and the handler for `tel:` links |

In a terminal, `aerc` reads mail over a local `notmuch` index, `profanity` is an XMPP client and
`newsboat` reads feeds once `~/.config/newsboat/urls` lists them; [Administration](administration.md#mail) sets mail up.

### Sharing files and reaching other machines

| Application | What it is for |
|---|---|
| qBittorrent | The default BitTorrent client, for `magnet:` links and `.torrent` files |
| KTorrent, Transmission, Deluge | Other BitTorrent clients |
| KGet | Queued, resumable downloads |
| FileZilla | FTP, FTPS and SFTP with a two-pane site manager |
| Syncthing Tray | Status and control of a local Syncthing, in the system tray or as a Plasma panel widget, with Syncthing actions in Dolphin's context menu |
| Remmina | The default remote-desktop client, over RDP, VNC, SPICE and SSH |
| KRDC | KDE's VNC and RDP client |
| krfb, krdp | Share this desktop with another machine, over VNC or RDP. krdp is System Settings → Remote Desktop: its switch starts and stops the server, and its start-at-login option adds an autostart entry. Open the `vnc` or `rdp` name in [the firewall](administration.md#opening-a-service) first |
| FreeRDP | The RDP engine under Remmina, with its own SDL client, `sdl-freerdp` |
| wlvncc | A Wayland-native VNC client |
| Waypipe | Runs a Wayland program on another machine and shows its window here |

### Phones

`ifuse`, over `usbmuxd`, mounts an iPhone's or iPad's documents as a directory; Android File
Transfer shows an Android phone's storage in a window ([Administration](administration.md#thunderbolt-phones-smart-cards-and-firmware-updates)
covers connecting an Android phone over USB). KDE Connect, above, links a phone over the
network once the `kdeconnect` firewall name is on ([Administration](administration.md#opening-a-service)).

### Office, documents and reading

| Application | What it is for |
|---|---|
| LibreOffice | The default for documents, spreadsheets, presentations, drawings and databases |
| Calligra | KDE's office suite: Words, Sheets, Stage and Karbon |
| Gnumeric | A spreadsheet with careful statistics, reading Excel and OpenDocument |
| AbiWord | A word processor that reads and writes Word, OpenDocument and RTF |
| Okular | The default PDF viewer; also PostScript, DjVu, XPS and comics |
| PDF4QT | View and edit PDFs: annotate, redact, sign, compare, split and merge |
| Calibre | E-book library, converter and editor; its viewer is the default for EPUB, MOBI and FB2 |
| Arianna | KDE's EPUB reader and library |
| KOReader | An e-book and document reader with no toolkit, suited to small or old machines |
| GnuCash | Double-entry accounting for a household or a small business |
| HomeBank | Personal finance: accounts, budgets and reports |
| KCalc | The default calculator, on the Calculator key |
| Qalculate!, Kalk | Calculators with units, currencies and symbolic algebra |
| GoldenDict-ng | Dictionary lookup over StarDict, DSL, MDict and other formats |
| Skanpage | Scan a document's pages, recognise the text and save a searchable PDF |

`typst` and `ledger` run from a prompt; `ocrmypdf` adds a searchable text layer to a scanned PDF.

GoldenDict-ng ships with no dictionary and with its online sources turned off. Dictionary files
copied into `/usr/share/stardict/dic` are found at its next rescan; a folder elsewhere is added
under *Edit > Dictionaries > Sources > Files*, and dict.org, Wikipedia and Wiktionary are ticked in
the same dialog (see
[GoldenDict-ng ships with no dictionary](../06-reference/known-gaps.md#goldendict-ng-ships-with-no-dictionary)).

### Writing text, notes and typesetting

| Application | What it is for |
|---|---|
| Kate | The default text editor: tabs, split views, projects, a terminal panel and language servers |
| Ghostwriter | Distraction-free Markdown with a live preview |
| Geany | A lightweight programmer's editor (GTK) |
| Lite XL | A small editor with no toolkit, drawn through SDL |
| Zim | A desktop wiki of linked notebook pages, with a journal and tasks |
| QOwnNotes | Plain-text Markdown notes with a live preview |
| TeX Live | TeX, LaTeX, pdfTeX, XeTeX, LuaTeX, MetaPost and BibTeX |
| TeXstudio | A LaTeX editor with a live PDF preview |
| LyX | A structure-first document processor that writes LaTeX |

Konsole is the terminal, on `Meta+Return`, with Yakuake, a Konsole that drops down from the top of
the screen. Konsole shows pictures in the terminal through the sixel, iTerm2 and kitty protocols,
which the image viewers and file managers of the terminal use. Its KDOS profile follows the
accent (see [Theming](theming.md#konsole-and-the-terminal-programs)).

### Scanning, OCR and printing

| Application | What it is for |
|---|---|
| Skanlite | Scan through SANE: preview, select and save |
| Skanpage | Multi-page document scanning with text recognition |
| gImageReader | OCR through Tesseract, from a scan, an image or a PDF |
| Printers, in System Settings | Add and configure CUPS printers, and watch the queue; system-config-printer does the same |

The printer drivers are ported with them: HPLIP for HP printers and scanners, `brlaser` for
Brother monochrome lasers, SpliX for Samsung, Xerox, Dell and Lexmark SPL printers, Gutenprint, and
the Foomatic database. [Administration](administration.md#printing) covers the print service.

### Pictures, drawing and photos

| Application | What it is for |
|---|---|
| GIMP | Raster image editing and photo retouching, with its manual installed |
| Krita | Digital painting, illustration and frame-by-frame animation |
| Inkscape | Vector drawing in SVG, with PDF import |
| darktable, RawTherapee | RAW photo development |
| digiKam | Photo library: tagging, face recognition and RAW import, with showFoto |
| Gwenview | The default picture viewer, RAW files included |
| Koko | A photo and video gallery by folder, place and date |
| gThumb | Browse, tag and edit photos and videos (GTK 4) |
| KolourPaint | A simple paint program |
| Spectacle | Screenshots and screen recordings, with an annotation editor; on the `Print` keys |
| Flameshot | Screenshots with an annotation editor |
| KColorChooser, KRuler, Kontrast | A colour picker, a screen ruler, and a contrast checker for text and background colours |
| Glaxnimate | Vector animation and motion design |
| Pencil2D | Hand-drawn 2D animation |
| Converseen | Batch image conversion and resizing |
| Font Manager | Browse, compare and enable fonts |
| swayimg | An image viewer with no toolkit |
| GrafX2, Goxel | Pixel art in the Deluxe Paint tradition, and a 3D voxel editor |

ImageMagick, `vips` and `exiftool` work from a prompt.

### Music and audio

| Application | What it is for |
|---|---|
| Elisa | The default music player, over the music collection the file indexer finds |
| Strawberry | A music player with stronger tag and collection tools |
| JuK, Rhythmbox, Audacious | Other music players |
| Kasts | Podcasts |
| Audacity, Tenacity, Kwave | Recording and audio editing |
| KRecorder | Voice clips |
| Audex | Rip an audio CD to FLAC, Ogg, Opus or MP3 |
| Ardour | A digital audio workstation: multitrack recording, editing and mixing |
| LMMS | Pattern-based music production with synthesizers and samples |
| Hydrogen | A drum machine and sequencer |
| MuseScore Studio | Music notation: write, play back, print and export scores |
| MusicBrainz Picard | Tag and rename music files from the MusicBrainz database |
| Schism Tracker, MilkyTracker, Furnace, ft2-clone, pt2-clone | Trackers, from Impulse Tracker and Fasttracker II to chiptune |

In a terminal, `mpd` with `rmpc`, `termusic` and `cmus` play music; `sox` converts and edits.

### Video

| Application | What it is for |
|---|---|
| Haruna | The default video player, over mpv |
| mpv | A player with no toolkit, with the `uosc` on-screen controls |
| VLC | VLC 3 with its Qt 5 interface; it runs on Xwayland |
| SMPlayer, Celluloid, Dragon Player | Other players |
| PlasmaTube | YouTube through Invidious or Piped, and PeerTube |
| Kodi | A full-screen media centre for local and network media |
| Kdenlive, Shotcut | Multitrack video editing |
| OBS Studio | Screen recording and live streaming |
| HandBrake | Video transcoding, with `HandBrakeCLI` |
| Avidemux | Simple cuts, filters and re-encodes |
| MKVToolNix | Create, split and edit Matroska files |
| Kamoso, Camera | Pictures and video from a webcam |

Spectacle records the screen (`Alt+Print`); `ffmpeg` and `mediainfo` work from a prompt.

### Burning discs

K3b writes data, audio and video CDs, DVDs and Blu-ray discs and copies them. Brasero and Xfburn
are simpler burners, on GTK. From a prompt, `cdrdao` writes audio CDs disc-at-once and
`growisofs` (from `dvd+rw-tools`) writes DVDs and Blu-ray.

### Files, disks and safety

| Application | What it is for |
|---|---|
| Dolphin | The file manager a double-clicked folder opens in |
| Ark | The default archive manager for zip, tar, 7z and rar, which it opens read-only; comic books packed as 7z or rar open in `kdos-openarchive`, which extracts them |
| Filelight, QDirStat, KDiskFree | What is using the disk |
| KFind | Search for files by name, type, date, size and contents |
| Partition Manager, GParted | Create, resize, move and check partitions; the filesystem tools they drive are ported with them |
| GNOME Disks | Partition, format, encrypt, image and benchmark disks |
| ISO Image Writer | The default for disk images: writes one to a USB stick, checking a known distribution's signature |
| Impression | Write a disk image to a USB stick or memory card, or a Windows installer ISO to a bootable stick |
| Kup, Déjà Dup, KBackup | Scheduled backups; Kup is the one on System Settings' Backups page (see [Backups](administration.md#backups)). Déjà Dup's scheduler is not started at login, so its "Back up automatically" runs only once its monitor is added to the Autostart page |
| KeePassXC | The default password manager for KeePass databases, with one-time codes. Its own Secret Service is off, because KWallet serves that name (see [Passwords and one-time codes](administration.md#passwords-and-one-time-codes)) |
| KWallet Manager, KeepSecret | Browse the passwords the session's secret store keeps. The live medium has no secret store, so on it both report the service missing (see [Known gaps](../06-reference/known-gaps.md#a-login-that-typed-no-password-leaves-the-wallet-locked)) |
| Keysmith | One-time codes for two-factor sign-in |
| VeraCrypt, Plasma Vaults | Encrypted volumes and folders |
| Kleopatra, KGpg | OpenPGP and S/MIME certificates, over GnuPG; Kleopatra is the default for OpenPGP keys, encrypted files and signatures |
| Sweeper | Clear recent documents, histories, cookies, caches and thumbnails |
| ClamAV | Virus scanning. It ships no signature database: run `sudo freshclam` once, with a network, before the first scan |
| ImHex | A hex editor for reverse engineering |
| System Monitor, Resources | System monitors for processors, memory, GPUs, disks and network; System Monitor has the KDOS page of boxes, energy and slots |
| KSystemLog | The system, kernel, authentication and cron logs in tabs, with filters. It runs as you, and the system, kernel and authentication logs are root's, so it shows only the cron log; read the others with `sudo less /var/log/messages` (see [Known gaps](../06-reference/known-gaps.md#ksystemlog-shows-only-the-cron-log)) |

In a terminal, `mc` and `btop` do the same jobs; see [The desktop](desktop.md#files).

### Virtual machines and remote consoles

Virtual Machine Manager (`virt-manager`) creates and runs virtual machines through libvirt and QEMU,
and shows their consoles over SPICE or VNC. QEMU runs from a prompt as well. The libvirt
services these need, `62_virtlogd` and `63_libvirtd`, are listed in
[Administration](administration.md#services).

### Maps and navigation

| Application | What it is for |
|---|---|
| Organic Maps | Offline maps with search and turn-by-turn routing; the default for `geo:` links |
| Marble | A virtual globe and world atlas |
| KTrip, Itinerary | Public transport journeys and departures; travel bookings and tickets |
| KWeather | Weather forecasts for saved places |
| GPXSee | View and analyse GPS tracks over offline maps |
| QMapShack | Topographic maps, elevation models and GPS tracks |
| Viking | Manage GPS tracks, waypoints and routes on maps |
| JOSM | Edit OpenStreetMap data (Java) |
| OpenCPN | Chart plotting and navigation for boats |
| QGIS | A full geographic information system; see below |

### Reference, learning and offline knowledge

| Application | What it is for |
|---|---|
| Kiwix | Read ZIM libraries, such as Wikipedia, offline; the default for `.zim` files. `kiwix-serve` serves them to other machines |
| Zeal | Offline API documentation sets |
| Kolibri | An offline learning platform of courses and quizzes |
| Stellarium | A planetarium |
| Celestia | A 3D space simulator |
| Kalzium | The periodic table of the elements |
| KAlgebra, KmPlot, Kig, KBruch | Graphing, function plots, interactive geometry and fractions |
| Step, Rocs | A physics simulator; graph theory |
| KTurtle, KGeography, KTouch | Programming, geography and touch typing |
| KWordQuiz, Parley, Kanagram, KHangMan, KLettres, Artikulate, Kiten | Vocabulary, letters, pronunciation and a Japanese reference |
| Minuet, Blinken | Ear training; a memory game |
| Kalm | Breathing exercises |
| GCompris, Tux Paint | Activities and drawing for young children |

The KDOS and *KDOS from Scratch* books open in the browser from the menu's Library, and in the
Help Center, KHelpCenter, beside every KDE application's handbook once it is installed
(`kdos update install khelpcenter`); `kdos-doc` opens the KDOS book from a prompt. *KDOS from
Scratch* is `kdos update install kdos-kfs`.

### Local AI, speech and translation

These run on this machine's processor and send nothing to a remote service:

| Program | What it is for |
|---|---|
| llama.cpp | Local language models, from a prompt, as a local server, and from KRunner as [`ask <question>`](../04-programs/plasma-shell.md#ask) |
| whisper.cpp | Speech to text; the `base.en` model is a port of its own and is installed too, and Dolphin's **Transcribe** action uses it |
| Piper | Text to speech |
| translateLocally | Machine translation |

### Software radio

| Application | What it is for |
|---|---|
| GNU Radio | Signal-processing blocks and GNU Radio Companion, for building receivers and transmitters |
| Gqrx, SDR++, CubicSDR | Receivers with a spectrum and waterfall |
| SDRangel | Receive, transmit and decode many modes |
| SatDump | Receive and decode weather and science satellites |
| Universal Radio Hacker | Record, demodulate and decode wireless protocols |
| Inspectrum | Inspect a recorded signal |

The drivers for RTL-SDR, HackRF, Airspy, LimeSDR, USRP and the FUNcube Dongle, and SoapySDR over
them, are ported with them.

### Amateur radio and mesh networks

| Application | What it is for |
|---|---|
| WSJT-X, JS8Call | FT8, FT4, WSPR and other weak-signal modes; keyboard messaging |
| fldigi, flmsg, flamp, flrig | Sound-card digital modes, message forms, multicast file transfer and rig control |
| FreeDV | HF digital voice |
| QSSTV | Slow-scan television |
| Gpredict | Satellite tracking and pass prediction |
| CHIRP | Program a radio's memory channels |
| KLog | A logbook |
| Pat, ARDOP | Winlink email over radio |
| Xastir | APRS maps and messaging (Motif, on Xwayland) |
| xnec2c | Antenna modelling |
| Nomad Network, Sideband | Messages and pages over a Reticulum mesh |
| Contact | A terminal client for a Meshtastic radio |

The Meshtastic and RNode firmware images are ported as well, to flash a LoRa radio with no network.

### CAD, electronics, 3D printing and 3D

| Application | What it is for |
|---|---|
| FreeCAD | Parametric 3D CAD, with FEM through CalculiX and meshing through Netgen and Gmsh |
| SolveSpace | Parametric 2D and 3D CAD with a constraint solver |
| OpenSCAD | Solid models written as scripts |
| LibreCAD, QCAD | 2D CAD with DXF |
| KiCad | Schematics, PCB layout, Gerber viewing and SPICE simulation, with its official libraries |
| Horizon EDA, LibrePCB | Other schematic and PCB tools |
| Qucs-S | Circuit simulation over ngspice |
| Digital, Logisim-evolution | Digital logic design and simulation for teaching |
| PrusaSlicer, OrcaSlicer, Cura | Slice 3D models into G-code for a printer |
| Blender | 3D modelling, sculpting, animation and rendering |
| LinuxCNC, bCNC, Candle, CNCjs | Drive a CNC machine or a laser |

### Science, data and development

| Application | What it is for |
|---|---|
| GNU Octave | MATLAB-compatible numerical computing |
| R, RKWard | Statistics, and a KDE front end to R |
| LabPlot, Veusz | Plots and data analysis |
| Cantor | Worksheets over Python, R, Octave, Lua, Qalculate and KAlgebra |
| QGIS | Desktop GIS |
| JupyterLab | Notebooks served on this machine |
| Spyder | A scientific Python IDE |
| Qt Creator, KDevelop | IDEs for C, C++ and Qt; KDevelop with PHP and Python support. No Qt API documentation is installed for their Help ([Known gaps](../06-reference/known-gaps.md#qt-creator-and-kdevelop-show-no-qt-documentation)) |
| KDiff3, Kompare, Git Cola | Compare and merge files; view a patch; a Git GUI |
| Umbrello | UML diagrams, with code import and generation |
| KCachegrind, Massif-Visualizer | Valgrind's profiles and heap snapshots |
| Accessibility Inspector, Kirigami Gallery | The accessibility tree a program publishes; every Kirigami component, for developers |
| DB Browser for SQLite | Create, browse and query SQLite databases |

### Clinic, shop and family

DCMTK handles medical DICOM images; Zint and gLabels make barcodes, labels and business cards, and
`ptouch-print` prints on Brother P-touch label printers; Gramps keeps a family tree; the Tryton
client and GNU Health run a clinic's records.

### Accessibility

Orca is the screen reader, and reads the whole desktop; Dasher enters text by steering a pointer or
a switch; KMouth speaks typed sentences and phrase books; Kontrast checks a pair of colours against
the contrast guidelines. What each does on this desktop is in [Accessibility](accessibility.md).

### Windows programs

Wine runs Windows programs, 64-bit and 32-bit. Wine Mono and Wine Gecko are ported with it, so a
program that asks for .NET or for Wine's HTML engine does not wait for a download.

### Games and emulators

The games are native ports, each with its free data set: OpenTTD, The Battle for Wesnoth, SuperTux,
SuperTuxKart, Luanti, Warzone 2100, Endless Sky, Freeciv, Widelands, Xonotic, Cube 2: Sauerbraten,
Pioneer, Cataclysm: Dark Days Ahead, Dungeon Crawl Stone Soup, Brogue, Neverball, The Powder Toy,
Flare and DDraceNetwork; Freedoom with the Chocolate Doom, Crispy Doom and dsda-doom engines; every
KDE game (KPatience, KMines, Knights against Stockfish, KSudoku, Kolf, Bomber, KSirK and the rest)
and GNOME's card and puzzle games; and `chess-tui`, chess in a terminal against Stockfish. Engines
for games you own are ported too: Yamagi Quake II, Ironwail, ioquake3, DevilutionX, OpenRCT2, fheroes2 and ScummVM.

RetroArch comes with its cores for Game Boy Advance, Game Boy, SNES, NES, Mega Drive, PlayStation,
Nintendo 64 and Nintendo DS. The standalone emulators are mGBA, Mednafen, DOSBox Staging, DOSBox-X,
Fuse (ZX Spectrum), VICE (Commodore), Hatari (Atari ST), Stella (Atari 2600), Mupen64Plus,
Amiberry (Amiga), PPSSPP and openMSX.

### Services for a LAN

A few ports are servers for a small network rather than applications: Radicale (calendars and
contacts), maddy (mail), ngIRCd (IRC), MiniDLNA (media for televisions) and Network UPS Tools.
[Administration](administration.md#services) lists the service that starts each one and what it
needs before it will start, and [Opening a service](administration.md#opening-a-service) names the
firewall rule other machines need to reach it.

### Programs for old hardware

These programs use no GTK or Qt, which suits the oldest hardware and software rendering: Lite XL,
KOReader, the `uosc` controls for mpv, GrafX2, Goxel, the trackers, Sniffnet
(network traffic by host and program), Inlyne (a Markdown viewer) and Surfer (a waveform viewer for
simulation traces). mpv and `swayimg`, listed above, need no toolkit either.

## Default applications

Which application opens a kind of file or link is decided by one list,
`/usr/share/kdos/xdg/kde-mimeapps.list`: the KDE application by default, unless an application
KDOS ports is clearly better at the job. Your own choice, made in Dolphin with **Open
With**, *Other Application…* and *Set as default app to open … files*, or on System Settings'
Default Applications page (a role) and File Associations page (one type), lands in
`~/.config/mimeapps.list` and outranks it.

| Role | Default | Also ported |
|---|---|---|
| Web, `http:` and `https:` links | Firefox ESR | Falkon, Konqueror, Chromium, LibreWolf |
| Mail (`mailto:`), invitations and contact cards | Thunderbird | KMail, Kontact, KOrganizer, KAddressBook, Merkuro |
| Office documents, spreadsheets, presentations, drawings | LibreOffice | Calligra |
| Folders | Dolphin | |
| Archives | Ark, which reads rar through libarchive; comic books packed as 7z or rar in `kdos-openarchive` | |
| Text and source code | Kate | |
| Pictures, camera RAW | Gwenview; Canon CR3 and Nikon NRW in darktable; JPEG 2000, DDS, OpenEXR and HDR in Okular; Windows metafiles in Inkscape; GIMP's own `.xcf` in GIMP | KolourPaint, GIMP, Krita, Inkscape, darktable |
| PDF, PostScript, DjVu, XPS, comics | Okular | PDF4QT |
| E-books | Calibre's viewer | Arianna, Okular |
| Video | Haruna | mpv, VLC, SMPlayer, Dragon Player |
| Music | Elisa | Strawberry, JuK |
| Terminal | Konsole | Yakuake |
| Screenshots and screen recording | Spectacle, on the `Print` keys | Flameshot |
| Calculator | KCalc, on the Calculator key | Qalculate!, Kalk |
| Remote desktop, `rdp:`, `vnc:` and `spice:` links | Remmina | KRDC |
| `ssh:` links | A shell in Konsole | |
| Torrents, `magnet:` links | qBittorrent | KTorrent |
| Disk images | ISO Image Writer | Impression |
| Offline library, `.zim` | Kiwix | |
| Map locations, `geo:` links, Organic Maps maps (`.mwm`) | Organic Maps | Marble |
| Password databases, `.kdbx` | KeePassXC | KeepSecret |
| OpenPGP keys, encrypted files and signatures | Kleopatra | KGpg |
| Phone numbers, `tel:` links | KDE Connect | |

When Ark is chosen on the Archive manager row of System Settings' Default Applications page, the
page notes that Ark may not open every file type; its details name one, ISO images, which Ark
reads and which stay with ISO Image Writer. *Open All in Ark* there would make Ark the default for
disk images as well.

A default outside the base, such as Thunderbird or LibreOffice, takes effect once its port is
installed. Until then, and after its application is removed, a row falls through to the next
installed application that claims the type. `testing/mimecheck.py` holds the list against the
build tree, so that every row names an application a port installs.

## Discover

Discover is where software is found, installed, updated and removed. On KDOS it shows what KDOS
has, and only that: no Flatpak, no Flathub, no ratings or reviews from a server, and no "Get New…"
downloads anywhere in the desktop. Opening it downloads nothing; each source below reaches the
network only when you press a button that says it will.

| Origin | Lists | Installing |
|---|---|---|
| `kdos-packs` | The catalogue's groups, every pack in the pack store, and the catalogue's applications whose pack is there | A box over that pack, offline |
| `medium` | The packs and application sets on the boot medium, and the catalogue's applications whose pack is only there | Offline, verified as each pack mounts |
| `kdos-store` | The catalogue's applications with no pack anywhere, and Debian's desktop applications that the catalogue does not name, from the Debian catalogue shipped on the image, less those that cannot work in a box | Built from Debian packages in a box of its own, over the network, and each says so before it starts; a Debian application's box is `deb.<package>` |
| `kdos-boxes` | Boxes that no application above accounts for, such as one you made with `kdos-box create` | Removal only |
| `kdos-ports`, shown as *KDOS system* | The host's own software: every installed port, and every port the release's binhost holds | Prebuilt from the binhost, with what it lacks, as an administrator, once `kdos update fetch` has fetched the release's signed index. Installed ports leave with the system version; Discover offers no removal |
| `kdos-ports-source`, shown as *KDOS, built on this computer* | Each port the binhost holds that is not installed, as a second origin of the same application | Built on the machine from its recipe, by `kdos update install --source`, with an administrator's password every time; what it lacks comes prebuilt or is built too, as System Settings › System Versions › Installing software says |
| The system update | The next KDOS release, as an A/B update (see [Keeping it current](administration.md#keeping-it-current)) | Written into the other slot; **Restart** starts it |
| Firmware | Firmware updates fwupd knows for this machine's devices, from the metadata snapshot each release carries | Applied by fwupd; system firmware at the next restart |

An application in a box carries a *Runs in a box* badge on its page, which opens the box's page in
System Settings. Double-clicking a `.ktar` set or a `.kpack` pack in Dolphin opens it in Discover,
which installs it offline, and a stick holding a set is offered in a notification when it is
plugged in. **Check for updates** fetches the newest signed release information, at most once an
hour; nothing else in Discover touches the network until an install says so.

The update tray in the panel is Discover's: it says when a system update is ready, when one is
installed and waiting for a restart, and how many known vulnerabilities the update closes, and it
never installs anything by itself. On the live medium the tray says nothing about an available
update, which could not be kept there; it still shows a running update job or a pending restart.

`kdos app` is the same catalogue from a prompt; see [Finding and installing](#finding-and-installing).

## What an alien app is

An *alien app* is a graphical application that this repository does not compile. It runs in a
*box*: a rootless Podman container of its own, named after the application's catalogue id
(`app.scribus`, `app.hugin`). A box shares your home directory by default, so the files you work on
are the same files inside and outside it. It is a packaging boundary, not a security boundary
against you; see [The security model](../03-architecture/security-model.md).

An alien app reaches your machine by one of two routes, and the route decides what kind of box it
gets:

- **Installed from the catalogue**, it is a stack of container images this machine builds: a base
  system, a shared runtime (for example the GTK or Qt libraries), and the application on top. Its
  box, a *store box*, is created over that top image, and the container engine keeps the box's
  writable layer in its own storage.
- **Imported from an exported set**, it is a *pack*: a read-only filesystem image whose hash, and
  signature where there is one, the pack daemon `kdos-packd` checks before it mounts anything. An
  exported pack holds the application's whole flattened root, base and runtime included. Its box, a
  *pack box*, is that pack with a writable layer composed over it, kept under
  `~/.local/share/kdos/boxes/<name>/`.

Either way the application behaves like a native one. It has a row in Kickoff, it opens
the file types it claims, it can be your default handler for a type, and it has a command you can
type at a prompt. The container shows only on the first launch of a session, which is slower.

## What the catalogue carries

The catalogue is the file `/usr/share/kdos/appstore/catalogue` (in the source tree,
`src/system/kdos-appbox/catalogue`). It describes software rather than containing it: nothing is
baked into the ISO, and an application is built by Podman on the machine that asks for it. The
Discover, the installer and `kdos app` all read this one file, so a row added to it is offered by
all three.

Counted in the shipped file:

| Row kind | Count |
|---|---|
| Applications (`app`) | 70 |
| Shared runtimes (`runtime`) | 7 |
| Bases (`base`) | 2 |
| Data sets (`data`) | 2 |
| Groups (distinct names on `group` rows) | 7 |
| Commands meant to be typed (`cmd`) | 25, in 16 applications |

The runtimes are `rt-gtk`, `rt-qt`, `rt-kde` (built on `rt-qt`), `rt-media`, `rt-sci`,
`rt-electron` and `rt-wine`. Each application is built on one runtime, or directly on a base, and
each runtime on a base, so a runtime is built and stored once however many applications use it. The
two bases are:

- `base` — Debian 13 (trixie), built from the `debian:trixie-slim` image, with what every graphical
  application needs: fonts, icon themes, the Mesa GL and VA-API libraries, D-Bus and the XDG
  utilities;
- `alpine` — the `alpine:3.24.1` image, about 3 MB of busybox and musl, a clean scratch system to
  try something in. It has no packages of its own, and no application is built on it.

Most rows are packages from the Debian archive. One application Debian does not carry, VSCodium,
is fetched as a `.deb` from its own release page and handed to Debian's package manager, which
resolves its dependencies normally. How the Debian packages are pinned is described under
[The Debian snapshot](#the-debian-snapshot).

### What the rows cover

The rows are applications KDOS does not build natively, and a few alternatives to ones it does
(never a KDE application, since KDE Gear is built natively and a boxed copy would be a second
Start-menu entry of the same name):
document readers and note-taking, page layout and panorama stitching, mail and chat, 3D viewing and
reconstruction, electronics viewers and logic analysis, audio routing and effects, development
tools, and a wide range of science (chemistry and materials codes, astronomy, GIS, statistics,
biology and numerical computing), with a few utilities and games.

Groups are how Discover and the installer offer the catalogue:

| Group | What it holds |
|---|---|
| `essential` | Zathura |
| `office` | Documents, spreadsheets and reading |
| `creative` | Images, audio and video |
| `dev` | Programming and electronics |
| `science` | Computation, modelling and data |
| `make` | 3D modelling, slicing and CAD |
| `games` | Games and emulators |

The installer ticks `essential` by default. The base and runtime an application needs come with it;
they are never chosen on their own.

## Finding and installing

`kdos app` is the catalogue at a prompt:

```sh
kdos app list                        # what is installed here, Debian boxes from Discover too
kdos app list --all                  # every application and data set, with its state
kdos app groups                      # the seven groups and what is in each
kdos app search image                # match id, name, category and summary
kdos app info app.scribus            # name, category, state, what it is built on, a size estimate
kdos app install app.scribus         # one application
kdos app install creative            # a whole group
kdos app install app.scribus --dry-run # print what would be built, without building it
kdos app install --pending           # the applications chosen during installation
```

`kdos app show` is another name for `info`. Every verb that builds, removes, exports or imports runs
`kdos-appbox` to do the work.

Installing builds the application on this machine, one image per layer: installing Scribus builds
`kdos/base`, then `kdos/rt-qt`, then `kdos/app.scribus`, in that order, and then creates the box.
The next Qt application finds the base and the runtime already built and needs only one more pass of
Debian's package manager. `--dry-run` prints the generated container build files instead, and counts
a shared runtime once, as a real install would. Sizes are shown as estimates everywhere: what the
package manager resolves depends on the snapshot, and shared layers are stored once however many
applications use them.

When you install several applications and one fails, the others stay installed and the failure is
named, as `N of M did not install`. Nothing is rolled back.

An application that is useless without a data set can name it with a `needs` row, and installing the
application then builds the data set too; the shipped catalogue has none. Both data sets are
installed by name: `data.kicad-packages3d`, the KiCad 3D model library, whose `graft` row names
`/usr/share/kicad/3dmodels`, where the natively built KiCad looks, and `data.tesseract-langs`,
language files for Tesseract OCR. A data set installed this way is built but not grafted into its
application's box; an imported data pack is grafted. See [What does not work](#what-does-not-work).

`kdos app install --pending` reads `/var/lib/kdos/apps-pending`, where the installer records the
groups you ticked when it did not install them: it had neither a set to import nor a network, or
the import or the build failed. After a failure the file is kept, so running the command again does
the rest. The file belongs to root and the command runs as you, so it stays in place after a run in
which everything installed too; a further `--pending` then tries the same groups again and reports
each application as not installed because its box exists. Delete the file once everything has
installed:

```sh
sudo rm /var/lib/kdos/apps-pending
```

The first login after installation says in a notification that the chosen applications are ready to
install, once; delete `~/.config/kdos/apps-offered` to see it again.

Two limits apply before you start:

- **Installing needs a network and takes minutes**: several for one application, the better part
  of an hour for a large group. An exported set installs offline instead; see
  [Carrying a set to another machine](#carrying-a-set-to-another-machine).
- **On a live session, what you install does not outlast the session.** Installing works there:
  the container engine's storage is pinned to `fuse-overlayfs`, which can stack its layers on the
  live session's overlay. But the images and the box are written under `$HOME`, which on a live
  session is that overlay, held in memory and lost at power-off unless the session has a
  persistence store (`kdos persist`; see
  [Boot and init](../03-architecture/boot-and-init.md#the-live-medium-and-persistence)). Install
  KDOS to disk to keep them. `kdos doctor` reports a `$HOME` on an overlay, and `kdos-box profile`
  prints a `!` line under such a box's `persistence`. The overlay is memory, which the session
  shares with everything else it writes, so an install first compares its estimate with the free
  space there and refuses, naming both, when the build would not fit.

### In Discover

Discover shows the same catalogue with a window, each row under `kdos-packs`, `medium` or
`kdos-store` by where its pack is: each application with its
summary, the runtime it is built on and an estimate of its size, its groups as categories, and a
page per application with **Install** and **Remove** (see [Discover](#discover)). The estimate
counts the base and runtime the application is built on when this computer has not built them yet,
which on a first install is several hundred megabytes. Discover builds
nothing itself: an install, an import or a removal runs `kdos-appbox` and `kdos-packd` with the
same arguments `kdos app` uses, one at a time, so the command line and Discover cannot disagree
about what installing means. An install that has to build from Debian packages says so, and that
it needs the network, before it starts.

## Carrying a set to another machine

```sh
kdos app export ~/apps.ktar creative app.vscodium   # on a machine that has them installed
kdos app import ~/apps.ktar                          # on the other machine: the SELECTION ids
kdos app import ~/apps.ktar app.scribus              # only part of the set
```

Export flattens each installed application's image, with its base and runtime, into one pack and
puts the set in one `.ktar` file, together with an index of every pack's hash. An application named
in the selection but not installed here is skipped with a message. The index is signed when the
variable `KDOS_PACK_KEY` names a readable key file; without one the set still exports and imports,
and the export states that the index is unsigned and that an import treats the packs as unsigned.

Import hands each pack to `kdos-packd`, which checks the payload against the hash recorded in the
pack's own footer, so a pack damaged in transit is refused rather than run. Export does not sign the
packs themselves. A signed index travels with them instead: import puts it beside the packs for the
daemon, which installs a pack as signed when its hash matches an index signed by a key the
receiving machine trusts (a `.pub` in `/etc/kdos/keys/packs`), and refuses a pack the signed index
lists under a different hash. A set exported without a key, or signed by a key the machine does not
trust, arrives unsigned: whoever alters a pack can rewrite its footer hash too, so the check catches
corruption, not tampering, and a daemon started with `KDOS_REQUIRE_SIG` set refuses every pack of
such a set. What each lane does and does not prove is in
[How an application is verified](../03-architecture/security-model.md#how-an-application-is-verified).
No network is needed at any point. Importing needs `kdos-packd` running and an account in `wheel`, because the daemon's socket
accepts only root and `wheel`. A pack the daemon refuses is named with its reason and skipped; the
rest of the set still imports, and the command ends with a count of what imported, what was already
installed and what failed.

The archive holds a `SELECTION` file, plain text with one id per line, so a set can be read,
compared and edited by hand. A group named on the export command line is written as a `group` line
with the ids the group held at export listed under it, and each id appears once even when it was
also named on its own or is held by two picked groups. An import with no ids reads the id lines and
skips the `group` lines, so in the example above `kdos app import ~/apps.ktar` imports the creative
applications and VSCodium. An id in `SELECTION` whose application was not installed on the exporting
machine has no pack in the archive; import names it as not in this archive and counts it as a
failure.

The installer looks for a set on a stick too; see
[Installation](installation.md#8-applications) for what it does with one. On a live session an
imported application runs, but its box's writable layer is held in memory and lost at power-off.

## Launching

A native application starts like any program: from its row in Kickoff, from KRunner, or by typing
its command. The rest of this section is about boxed applications.

Start an installed boxed application from its row in Kickoff, from KRunner, by typing its command,
or by id with `kdos app launch`:

```sh
scribus                      # the command on your PATH
kdos app launch app.scribus  # by id; installs it first if it is not installed
```

The command is a *shim*: a symbolic link named after the application that points at `kdos-appbox`,
which reads its own name, looks the application up and starts it in its box. Shims for applications
you install go in `~/.local/bin`, which the login profile puts on your `PATH`; the table that maps
each shim to its box is `~/.local/share/kdos/alien-apps`, or `$XDG_DATA_HOME/kdos/alien-apps`
when that variable is set. `kdos app launch` on an application that
has no launcher, because it is a command-line tool, says so and names `kdos app info`.

A first launch is slow, because a container has to be created or started and its first process
has to finish setting the box up before anything runs in it. A cold launch, with no container at
all, has been measured at about 18 seconds; a launch into a box that is already running skips all
of that. That is why a "Starting app" notification appears while a box comes up.

Each application a box carries gets a launcher of its own in `~/.local/share/applications`, with
its own icon, and KWin knows the box's windows by that launcher's id. So a boxed window shows the
application's icon in the task manager, groups with its launcher, can be pinned, and wears its box's
title-bar colour. Installing or removing an application rewrites the launchers and asks Plasma to
read them again at once.

It is also why, when you log in, the session runs `kdos-appbox warmup` in the background at
`nice` 10. The warmup reads your pinned applications from `~/.config/kdos/favorites`, where each
line is a desktop id optionally followed by a launch code such as `code=WW`, and starts the boxes
of up to eight of them, one at a time. It takes the first word of a line as the desktop id and
ignores the code. A second warmup while one is running does nothing.

To see where a launch spent its time, read `$XDG_RUNTIME_DIR/kdos-appbox.trace`, which gets one
timestamped line per stage.

### Adding a terminal program to the menu

`kdos app tui` gives a terminal program a Kickoff entry of its own, without editing a desktop file by
hand. The entry opens the program in Konsole.

```sh
kdos app tui add "Disk usage" ncdu                     # prints the entry's name, kdos-tui-disk-usage
kdos app tui add btop btop --icon utilities-system-monitor --category System
kdos app tui ls                                        # the entries this command made
kdos app tui rm disk-usage                             # remove one
```

| Option | Effect |
|---|---|
| `--icon NAME` | The menu icon (default `system-run`) |
| `--category X` | The menu category (default `Utility`) |

The display name becomes the file name: lower case, with every run of characters other than
letters and digits turned into one dash. Entries are written to
`~/.local/share/applications/kdos-tui-<name>.desktop`; the prefix keeps an entry from shadowing a
shipped one of the same name. `rm` takes the name with or without the prefix, and removes only an
entry this command wrote, which it recognises by the `X-KDOS-TUI=true` key. `kdos app tui` needs
neither `kdos-packd` nor membership of `wheel`.

## Opening files

Double-clicking a file, or running `kdos-appbox open <path>`, finds the file's type from its name
and then a program to open it. The lookup is the freedesktop one: the file name against the shared
MIME database's glob table, where the longest matching suffix wins, then `mimeapps.list` (default
applications first, then added associations), then each `mimeinfo.cache`. Boxed applications are
found exactly like host ones, because installing one writes its file types into the same places.

```sh
kdos-appbox open report.pdf             # open with the default handler
kdos-appbox open --print report.pdf     # print the type, the candidates and the handler, and open nothing
kdos-appbox open --choose report.pdf    # pick a handler
```

When more than one program claims the type and none is set as the default, `kdos-appbox open` asks,
with the desktop portal's application chooser, the same "Open With" dialog Dolphin shows, which
lists host and boxed applications alike; `--choose` asks whatever the defaults say. Its *Remember*
choice makes the answer the default for that type. A boxed application can be the default handler
for a type. When nothing claims a
type, the request goes to the system's `xdg-open` script. One call opens up to four files, all with
the handler chosen for the first.

The shipped defaults send each everyday type to a native application; the table is under
[Default applications](#default-applications).

`xdg-open` on KDOS is another name for the same program, so links from any program go to your chosen
handler. Links clicked *inside* a box open on the host: a boxed application asks the desktop portal
(a session service that carries requests such as "open this link" out of a box; see
[Portals](../03-architecture/session.md#portals)) to open them, and the portal launches the host's
handler, so a help link in a boxed application opens in your browser.

Printing works from inside a box. The host's print service socket, `/run/cups`, is shared into a box
when the box is created, and the application's own print dialog talks to it directly. Because a
box's shared directories are fixed at creation, a box created while the print service was not
running cannot print until it is created again; see
[Profile changes and re-creating a box](#profile-changes-and-re-creating-a-box).

## Commands that live in boxes

Not all boxed software is an application you click. Sixteen catalogue applications carry a
program meant to be typed, twenty-five in all: `gmic`, `solve-field`, `cp2k`, `grib_ls`, `gmx`,
`ghdl`, `rr`, `glxgears` and the rest, listed in the catalogue as `cmd` rows. They are solvers,
converters, toolchains and tools driven from a prompt, and they get no menu entry: a launcher for
`gmx` with no arguments would do nothing. To see them all:

```sh
grep '^cmd' /usr/share/kdos/appstore/catalogue
```

Run one in its application's box with `-b`:

```sh
kdos-appbox -b app.gmic run gmic input.png -blur 3 -o output.png
kdos-appbox -b app.gromacs run gmx mdrun -h
```

`-b` (or `--box`) names the box. Without it, `kdos-appbox run` looks the command up in the launcher
tables, `~/.local/share/kdos/alien-apps` (or `$XDG_DATA_HOME/kdos/alien-apps`) and then
`/usr/share/kdos/alien-apps`, and takes the box from the row whose command matches. A command with
no row there fails the lookup, and `kdos-appbox` refuses with `no installed pack carries
'<command>'`.

Where a shim exists, typing the name is shorter. Shims are made for the programs an application's
own desktop entries name, for `wine`, `winecfg` and `winetricks` wherever the box carries them, and,
for an application built from the catalogue with `kdos app install`, for each of its `cmd` rows,
which also get a row in the table. An application imported from a set gets shims only for what its
desktop entries name, so its command-only tools are reached with `kdos-appbox -b`. A shim
is never made under the name of a host tool (`git`, `gnuplot`, `mpv`, `python3`, the shell and
file tools, and KDOS's own commands among them), because `~/.local/bin` comes ahead of the system
directories on `PATH` and the shim would take the name over from the host program.

## Updating

Native applications are updated with the system, each by the way it came: one built here is built
again, a prebuilt one is downloaded again; see
[Keeping it current](administration.md#keeping-it-current). This section is about boxed
applications.

A boxed application is rebuilt only when you ask for it. `kdos app install` reuses every image that
already exists, so installing an application that is already here builds nothing: each image
reports that it is already here, the step that creates the box fails because the box exists, and
the command ends by reporting `1 of 1 did not install`. To rebuild an application, remove it and
install it again:

```sh
kdos app remove app.scribus
kdos app install app.scribus
```

This rebuilds the application's own image, and any runtime that no other installed application
was using. Neither the base image nor the pulled `debian:trixie-slim` is removed, so under
`snapshot = auto` the rebuild reads the same snapshot date as before. Packages move to a newer snapshot only when the catalogue names a later
date, or when this machine pulls a newer `debian:trixie-slim`.

An imported set behaves differently. Each exported pack is stamped with the time of the export as
its version, and `kdos-packd` keeps one superseded version of each pack on disk when a newer one is
imported, so an update can be undone. That is `retain` in
[`/etc/kdos/packd.conf`](../06-reference/configuration.md#etckdospackdconf), which ships set to `1`;
`retain = 0` keeps none.

Importing a set skips an application whose box already exists, installing nothing for it: the
import prints `<name>: already installed` and counts it apart from what imported and what failed.
To move such a box onto the new pack, remove the box first with `kdos-box remove <name>` (stopping
it first if it is running) and then import; the box's writable layer and settings stay, and the new
box is composed on the newest version of each pack. A box that cannot be created takes its pack back
out: the import prints `<name>: the pack installed and the box did not: <reason>`, has `kdos-packd`
remove the pack it has just installed, and counts the application as failed.

### The Debian snapshot

The Debian packages come from a pinned snapshot of the Debian archive, set by the catalogue's
`snapshot` key:

| Value | Effect |
|---|---|
| `auto` (shipped) | The date is read from the sources file inside the `debian:trixie-slim` image this machine pulled, so the packages installed on top always match the system under them |
| a date, such as `20260824T000000Z` | Every machine builds against the same snapshot |
| `off` | The live Debian archive; builds are not reproducible |

The `debian:trixie-slim` tag moves as Debian publishes new images, so under `auto` two machines
agree on the snapshot only when they pulled the same image. If the date cannot be read out of the
image, the build warns that it is using the live archive and carries on rather than refusing.

## Removing

```sh
kdos app remove app.scribus   # one application
kdos app remove creative      # a whole group
```

Removing an application:

- stops its box if it is still running (a box stays up for a while after its application's last
  window closes), then removes it; the box's settings in `~/.config/kdos/boxes/<name>.conf` stay,
  so installing it again picks them up. A box that was installed and never started has only those
  settings, and they are removed with it. A box that will not stop is not removed, and the removal
  then stops before any image is deleted;
- deletes the application's image, and any runtime image no other installed application uses;
- never deletes the base image, which every application sits on;
- regenerates the launchers, shims and file-type associations from what remains.

What happens to the work inside the box depends on the kind of box. Your own files in your home
directory are never touched, because the box shares your home directory. A store box's writable
layer, meaning anything the application wrote outside your home directory, lives in the container
and is removed with it. A store box that stays `stopping` after a stop, and is still `stopping`
after it has been killed, is not removed for you: the next launch or `kdos-box start` refuses with
a warning, the box keeps what it wrote, and `kdos-box remove <name> --force` is how you give that up.
A stuck pack box is removed and created again over the same packs, which loses nothing. A pack
box's writable layer stays in `~/.local/share/kdos/boxes/<name>/`,
and its packs stay in `kdos-packd`'s store.

## Boxes

Every application gets its own box, named after it. You do not need to manage boxes to use
applications, but `kdos-box` manages them directly:

```sh
kdos-box list                                      # every box: base, state, persistence, disk use, accent
kdos-box enter app.scribus                         # a shell inside one, in a new terminal window
kdos-box create scratch base=image:alpine:3.24.1   # a new box; base= also takes pack:<id>
kdos-box start | stop | restart | remove <name>
kdos-box gc                                        # stop boxes past their autostop now (--dry-run to preview)
```

`kdos-box list` also shows a box that has a profile and no container yet, with the state
`not created`. `kdos-box enter` sets `KDOS_BOX` to the box's name inside it, which a shell prompt
can show. The full command set, including `run`, `apps`, `export` and `import`, is in
[kdos-appbox](../04-programs/kdos-appbox.md#kdos-box).

### A pack box's writable layer

Five commands work on the writable layer under `~/.local/share/kdos/boxes/<name>/`, and so apply
to pack boxes (imported applications and boxes created with a `pack:` base). A store box keeps its
writable layer in the container engine's storage, where these commands do not reach, and where it
is lost when a stuck box is created again over its image.

```sh
kdos-box snapshot app.scribus before-plugin        # a full copy of the writable layer, tagged
kdos-box snapshots app.scribus                     # list them
kdos-box rollback app.scribus before-plugin        # put one back
kdos-box clone app.scribus scribus-test            # a new box on the same base, with the work copied
kdos-box freeze app.scribus                        # what the box changed, captured as one pack
```

`kdos-box freeze` packs only what the box has *written* over its base, which on a real working box
is a small fraction of its whole root. A pack box is created over mounted packs rather than a
container image, so there is no image to commit, and freezing is the way to capture its state.

To start a second box from the same software with an empty workspace, read the first box's `base`
with `kdos-box profile <name>` and pass it to `kdos-box create`.

### Profiles

A box's settings live in `~/.config/kdos/boxes/<name>.conf`, its *profile*. `kdos-box profile
<name>` shows it, and `kdos-box profile <name> key=value` changes it. The Boxes page of System
Settings, under Security & Privacy → Application Permissions, edits the same file through the same
command, and shows beside each control what enforces it and why a value cannot be delivered. Most
keys map onto a container flag or a namespace, and the printed profile shows each of them beside
the flag that enforces it. An application's box, a store box or a pack box, sees a private runtime
directory of its own as `/run/user/<uid>`, holding only its tagged Wayland socket, its filtered bus
and its audio sockets, so `wayland = no` and `audio = no` are enforced there: nothing of the kind is
put in it. Only a development box made from somebody else's image with `kdos-box create
base=image:<ref>` shares the session's runtime directory, and there the printed profile adds a `!`
line under each of the two. `persistence` is enforced on a pack box; on a store box `frozen` is
only recorded, on a development box every value but `persistent` is, and each of those gets a `!`
line. `export` is only recorded, `display` is kept in the file unread, and a `!` line appears when
`devices` is shared and `audio` or `gpu` is `no`, since the device nodes then follow `devices`. See
[kdos-appbox](../04-programs/kdos-appbox.md#box-profiles) for every key and what enforces it.

### Profile changes and re-creating a box

Namespaces (the kernel namespaces that separate the box's processes, network and users from the
host's) and shared directories are set when a box is created and cannot be changed on a running
container, so a profile change to them takes effect only when the box is created again. Remove the
box with `kdos-box remove <name>`; its profile is kept. A running box is not removed: stop it first
with `kdos-box stop <name>`, or pass `--force`. Then:

- for a pack box, the next launch creates the box again from the kept profile;
- for a store box, the next launch creates the box again over the image it was built on, with an
  empty writable layer; `kdos app install <id>` does the same, finding every image already built and
  creating only the box. `kdos-box start` does not create a removed box.

### Idle boxes

Your user services run `kdos-box gc` every ten minutes, for as long as you are logged in. It stops
a running box whose profile sets `autostop` and which has been running for longer than that
`autostop` time. It asks KWin first which boxes have a window open: a box with a window on the
screen is never stopped, and when there is no session to ask, nothing is stopped. `autostop` is off
(`0`) unless you set it, for example with `kdos-box profile app.scribus autostop=30m` or on the
box's page in System Settings.

Boxes that were running when you logged out are started again at your next login, and a box is
stopped when your last login ends, unless you linger (`loginctl enable-linger`), in which case your
boxes and user services keep running with no login at all.

## Accessibility inside a box

A boxed application offers whatever accessibility support its own toolkit has, and the screen
reader on the host reads it, as it reads the rest of the desktop. The box reaches the accessibility
bus through its bus filter whenever assistive technology is in use: the screen reader is switched
on in System Settings, a reader or other assistive program is attached to the accessibility bus, or
`~/.config/kdos/a11y` exists. Otherwise the box is told not to look for the bus (`NO_AT_BRIDGE=1`
and `GTK_A11Y=none`), so that a GTK application does not spend its start-up probing for it.
`KDOS_A11Y=1` or `0` decides one launch. A box takes the choice when it starts, so a screen reader
switched on later reaches a running box from its next start; see
[Accessibility](accessibility.md#boxed-applications).

## What does not work

- **Applications that need raw block devices**: partitioners, SMART tools, disk utilities. A
  rootless container cannot open a disk, so their launchers are left out even when a package
  carries them. Those jobs belong to the native tools on the host (Partition Manager, `parted`,
  `testdisk`, `ddrescue`, `partclone`), which run as root.
- **Microsoft core fonts for Wine**, and `winetricks`. Both download Windows components at run
  time, which an offline system cannot promise, so a Windows program asking for Arial gets a
  substitute.
- **Data sets reaching their applications from the store lane.** The catalogue's `graft` and
  `boxgraft` rows say where a data set should appear, and `kdos-appbox import` applies them to an
  imported data pack, but `kdos app install` does not: a data set installed from the catalogue is
  built as an image of its own, so KiCad does not see the 3D model library and Tesseract does not
  see the extra languages until the set has been exported and imported as packs.

## See also

- [Packs and boxes](../03-architecture/packs-and-boxes.md) — the pack format, composition and the
  catalogue's machinery
- [kdos-appbox](../04-programs/kdos-appbox.md) — the launcher, the box manager, and every profile key
- [The desktop](desktop.md) — Kickoff, KRunner, Dolphin and the default applications in use
- [Discover](../04-programs/discover.md) — the KDOS backend, the system update and the firmware
- [Theming](theming.md) — how native and boxed applications get the palette
- [The kdos command](../04-programs/kdos-command.md) — `kdos app` and `kdos doctor` in full
- [The ports catalogue](../06-reference/ports-catalogue.md) — every native port, by shelf and phase
- [Known gaps](../06-reference/known-gaps.md) — what does not exist, across the system

<!-- book-nav -->
---

*Part II — Using KDOS, chapter 8.* Previous: [7. The desktop](desktop.md) · [Contents](../README.md) · Next: [9. Theming](theming.md)
