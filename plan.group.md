# Plan: shelving the ports tree, and reorganising src/ and script/

Move the 1,999 upstream ports from one flat directory, `ports/core/<name>/`, into 102 subject
directories, `ports/core/<shelf>/<name>/`, in the manner of T2 SDE's `package/<repository>/<name>/`.
Then regroup the package lists and the ports catalogue by the same shelves. Part B reorganises
`src/`, KDOS's own code, by what each program is. Part C reorganises `script/`, the build phases,
into 13 granular phases derived from the dependency graph, with names, a numbering with gaps,
one shared environment, and every userland list split by shelf. This file is a plan: nothing below has been
implemented.

Every port has been assigned a shelf (Appendix A). The assignment was derived from the ports
themselves: name families, homepage hosts, descriptions and dependency clusters. The comment
groups in `script/*/packages.txt` were not used as a seed. It was then reviewed for split families
and misfits, and the review's 23 moves are applied here.

**Order across the three parts:**

1. A step 1: the resolver accepts both depths.
2. Part B: `src/`.
3. Part C: `script/`.
4. A steps 2-8: the `ports/core` move, the lists regrouped into `packages.d/`, tightening, and
   proof.

Each part has its own gates, and nothing moves while a build runs.

---

# Part A: shelving `ports/core`

## 1. Decisions

| # | Decision | Why |
|---|---|---|
| D1 | The concept is called a **shelf**. | "group" is taken twice (the `group =` key read by `ports/update`, and the package-list groups). "category" is taken by the appbox catalogue, the pack index `G:` field, `Categories=` and the Start menu. `packs-and-boxes.md` already says "A group is not a category". "section", "family", "tier" and "ring" are all in use in the book. "shelf" has no hits in `docs/kdos`. T2's own word, "repository", collides with `PORT_REPO`. |
| D2 | The layout is `ports/core/<shelf>/<name>/`, not `ports/<shelf>/<name>/`. | `ports/` also holds `fetch`, `publish`, `update`, `srclib.sh`, `sources.idx`, `hackage-vendor` and the dot-caches. With `ports/core` kept as the repository root, the following stay valid unchanged: every `PORT_REPO` value, the `kpkg.conf` default, the chroot `/ports` bind, the `Makefile` mount, the `rebuild.c` tree test, the `02_iso.sh` copy, the lf `gp` bind, and every `ports/core/*/…` git pathspec (a pathspec `*` crosses `/`). |
| D3 | **A port's identity stays its bare name.** The shelf is only where it is filed. | This is T2's model: its tools glob `package/*/$pkg/` and abort on a name found in two trees. `name =` must equal the directory name. A name is unique across every shelf and every `src/` port directory. `sources.idx` labels stay `<port>/<file>`: the index is append-only and its 1,678 lines already use that form. |
| D4 | **The shelf comes from the path alone. There is no `shelf =` recipe key.** | `kpkgbuild` bytes are hashed (`kp_hash.c:151-169`). A new key in 1,999 recipes changes every recipe hash, and under `KPKG_STRICT_RECIPE=1` that rebuilds the whole tree. |
| D5 | **`src/` is reorganised separately, and every port there stays exactly two levels deep: `src/<area>/<name>/`** (Part B). `src/` ports are not shelved under `ports/core`. | 18 `build.sh` files use `$PORT_SRC/../../libs`, `kdos-installer` uses `$PORT_SRC/../kdos-appbox`, and `kp_hash.c:188-198` hashes `<portdir>/../../libs` for source-less ports. A third level would break all three. |
| D6 | The resolver descends inside libkpkg. Shelves are never listed on `PORT_REPO`. | `KP_MAX_REPOS` is 8 (`kpkg.h:34`), and `split_repos` (`kp_conf.c:62-69`) truncates **silently**. With 3 repositories plus 102 shelves, most shelves would not exist to the build. |
| D7 | The shelf list is closed. It is a data file, `ports/shelves`, with one line per shelf: `<id> <one-line description>`. | T2 validates its `[C]` tags against `PKG-CATEGORIES`. Preflight validates the tree against this file, and a newcomer reads it to place a port. It sits outside `ports/core` so that nothing walking the repository sees it. |
| D8 | Shelf names are lowercase `[a-z0-9-]`. A shelf is never named `libs` or `core`, and never shares a name with any port. | A shelf named `libs` makes `ports/core/<shelf>/<name>/../../libs` resolve to a real directory, and the two source-less core ports (`containers-common`, `musl-ldd`) would hash that whole shelf. A shelf named after a port (for example `perl`, `lua`, `llvm` or `wayland`) cannot be told from a loose port. It also makes `git mv ports/core/perl ports/core/perl/perl` a move into itself, and makes `kpkg meta perl` inside `ports/core` take the argument as a path (`front.c:481`). All 102 ids below were checked: none is a port name. |
| D9 | The package lists and the ports catalogue are regrouped by shelf. The existing list groups are retired. | The current list groups are not a sound classification. Keeping two classifications would let them drift apart (`principles.md:150-158` and `packaging.md:150-162` would describe both). |

### What the move does *not* change

These were measured, not assumed:

- **Recipe hashes:** unchanged. `kp_recipe_hash` hashes each file's basename, length and bytes (`kp_hash.c:164`). `hash_tree` uses paths relative to the port. `kp_buildconfig_hash` reads flags and the compiler id only. Nothing rebuilds.
- **The installed database and the binhost:** unchanged. `db/.recipe/<name>` is keyed by name. The binhost stanza (`binhost.c:295-298`) and delta names (`delta.c:200-205`) hold no path.
- **`sources.idx` and `.srccache`:** unchanged. Both are keyed by hash.
- **Tarball hard links:** they survive. `git mv ports/core/foo ports/core/base/foo` renames the whole directory, untracked tarballs included, and keeps the inode, so the hard link into `.srccache` survives. This was measured in a scratch repo. No refetch is needed.
- **Recipes:** none refers to another port by path at run time. Every `ports/core` path inside a recipe is in a comment.

---

## 2. Placement rules

These rules go into `writing-ports.md` as the procedure for choosing a shelf. Apply them in order;
the first that matches decides.

1. **A `group =` family stays together.** Every port sharing a `group =` key lives on the shelf of
   the group's lead port, so `ports/update` bumps one directory:
   - qt6, including python3-pyside6, goes to `qt6`; qt5 goes to `qt5`.
   - qscintilla (with python3-qscintilla) and qwt go to `qt-extra`.
   - qca goes to `crypto`; glib goes to `gtk`; webkitgtk goes to `web-engines`; texlive goes to `doctools`.
   - python3 (with python3-tkinter) goes to `python`; gcc-arm-none-eabi goes to `embedded`.
   - mgba (with libretro-mgba) goes to `emulators`; supertuxkart (with stk-assets) goes to `games-action`.
2. **A name-prefix family decides.** The prefix wins over the port's domain.
   - `python3-*` goes to one of the `python*` shelves (see the Python rule below).
   - `perl` and `perl-*` go to `perl-cpan`.
   - `qt6-*` goes to `qt6`, and `qt5-*` to `qt5`.
   - `libretro-*` and `retroarch*` go to `libretro`.
   - `sdl*-*` goes to `game-libs`.
   - `font-*`, `noto-*`, `ttf-*` and `terminus-*` go to `fonts`.
   - `libX*`, `xcb-*` and `xorgproto` go to `x11`.
   - `gst-*` and `gstreamer` go to `media-frameworks`.
   - `soapy*` goes to `sdr-hw`, and `gr-*` to `sdr`.
   - `kicad*` goes to `eda`.
   - `fcitx5*` goes to `input`.
   - `lua54-*` goes to `lang`.
   - `texlive*` goes to `doctools`.
3. **KDE Frameworks.** A homepage under `invent.kde.org/frameworks` or `api.kde.org/frameworks`
   goes to `kf6`, whatever the port does. KDE-hosted libraries outside frameworks
   (`kirigami-addons`, `kqtquickcharts`) go to `qt-extra`.
4. **The kind of data, and plug-ins.**
   - Font files go to `fonts`.
   - Icon, cursor and sound themes go to `themes`.
   - Firmware for the host goes to `boot`. Firmware for an external device goes to that device's
     shelf: `meshtastic-firmware` goes to `hamradio`, and `sigrok-firmware-fx2lafw` goes to `eda`.
   - A data, asset, help or model package, or a plug-in that exists for one program, follows that
     program: `gimp-help`, `xonotic-data`, `retroarch-assets`, `mpv-mpris`, `audacious-plugins`.
     The same holds for an engine that exists for one game: `flare-engine` goes to `games-rpg`.
5. **An application is filed by what it does, never by its toolkit or desktop project.** There is
   no `kde-apps` or `gnome-apps` shelf. For example:
   - `dolphin` goes to `files`, and `konsole` to `shells`.
   - `okular` goes to `documents`, and `kdenlive` to `video-tools`.
   - `kpat` and `gnome-mines` go to `games-board`.
6. **A library goes to its domain shelf when it has one.** For example:
   - `libsoup3` goes to `net-libs`, and `librsvg` to `graphics-libs`.
   - `libkexiv2` goes to `image-libs`, and `libksane` to `printing`.
   - `libkdegames` goes to `game-libs`, and `goffice` to `office`.

   Only a library whose job is the toolkit or desktop platform itself goes to that stack's shelf
   (`gtk`, `qt-extra`, `kde`, `wl`, `x11`). A Qt or GTK widget library goes to `qt-extra` or `gtk`
   even when it is small and generic.

   A general-purpose C or C++ library with no domain goes to `devlibs`, and this rule beats the
   main-consumer tie-breaker (`tllist`, `libdaemon`, `talloc`). That includes portable SIMD and
   parallel runtimes (`highway`, `simde`, `onetbb`), except a helper that belongs to one framework
   (`orc` goes to `media-frameworks`, `libvolk` to `sdr`, `xsimd` to `sci-libs`). A low-level
   library the base system links goes to `base-libs`.
7. **Python.** Only `python3-*` ports go to the `python*` shelves, split by domain:
   - `python`: the interpreter, build and packaging.
   - `python-libs`: general runtime libraries and file formats.
   - `python-net`: web, HTTP and networking.
   - `python-sci`: science and numerics.
   - `python-gui`: GUI and desktop bindings.
   - `python-dev`: Jupyter, LSP and debugging.
   - `python-hw`: hardware and device I/O.

   An unprefixed Python module goes by its domain: `numpy` goes to `sci-libs`, `sympy` to `math`,
   and `astropy` to `astronomy`. A Python application goes by its function: `calibre`, `meson`,
   `ocrmypdf`.
8. **Other languages.** A language with its own module shelf keeps its interpreter there (`python3`
   in `python`, `perl` in `perl-cpan`). Every other language goes to `lang` with its modules: Lua
   and `lua54-*`, Ruby, Haskell, Go, Rust, Node, OCaml, Java, Tcl/Tk (with `bwidget`), Guile, R,
   Zig and Vala.

   A language that reaches about 10 module ports gets its own shelf, named `<lang>-<something>`
   and never the interpreter's port name. Vendored crates, Go modules and hackage packages stay
   inside their port; they are not ports.
9. **Small tie-breakers.**
   - A client-and-server program goes to `servers` when it ships a daemon (`openssh`, `mosh`,
     `mosquitto`). A database server goes to `database`.
   - Terminal emulators go to `shells`.
   - File managers and disk-usage tools go to `files`. Partitioning, RAID, LVM and block encryption
     go to `disk`. Filesystem tools and FUSE filesystems go to `filesystem`. Burning and CD reading
     go to `optical`.
   - Sandbox primitives and container networking go to `containers`.
   - Remote-desktop protocols and VNC go to `virt`.
   - Code and binding generators go to `toolchain` (`bison`, `flex`, `gperf`, `bindgen`,
     `cbindgen`, `swig`).
10. **Still ambiguous.** Choose the shelf of the port's only main consumer. If there is none, choose
    the shelf a user would open first. Record the choice in the shelf's "does not hold" text in the
    book, never in the recipe (hard rule 6).

**Moving a port later** is `git mv ports/core/<old>/<name> ports/core/<new>/`, and nothing else
changes. So every comment, doc and reasons file names ports by bare name or by `kpkg meta <name>`,
never by a `ports/core/<shelf>/<name>` path. A path written into prose goes stale at the first
re-shelving.

---

## 3. The shelves

102 shelves, holding 4 to 67 ports each (median about 17). T2 has 84 repositories for 6,813
packages. Appendix A lists every port by shelf.

| Shelf | Ports | Holds | Does not hold |
|---|---:|---|---|
| `base` | 27 | The minimal userland and system daemons that every KDOS image boots with. | Libraries go to base-libs. Login and privilege tools (pam, shadow, sudo, polkit) go to auth. zsh and interactive add-ons go to shells. musl goes to toolchain. |
| `base-libs` | 16 | Low-level C libraries that the base system links. | General C/C++ utility libraries with no base role (fmt, pcre2, jemalloc, bdwgc) go to devlibs. musl compatibility libraries go to toolchain. |
| `auth` | 13 | Authentication, authorisation, accounts and directory services. | polkit-qt6 goes to qt-extra. Password managers and GnuPG go to security. Smartcard and TPM stacks go to crypto. |
| `boot` | 14 | Everything before userspace: the kernel, microcode, firmware for the host, bootloader and EFI tooling. | Firmware for external devices goes to that device's shelf. flashrom goes to embedded. |
| `toolchain` | 36 | The native compilers, the linker, the C library, and the LLVM family. | Cross toolchains for microcontrollers go to embedded. Language runtimes (rust, go, ghc) go to lang. Build systems go to buildtools. |
| `buildtools` | 19 | Build systems and build helpers. | extra-cmake-modules goes to kf6 (step 3). Python build backends go to python. Compilers go to toolchain. |
| `lang` | 26 | Language implementations, plus the modules of any language that has no module shelf of its own. | python3 goes to python and perl goes to perl-cpan (languages with module shelves). llvm and clang go to toolchain. sdcc goes to embedded. |
| `devtools` | 33 | Debuggers, tracers, profilers, analysers, linters, language servers and developer utilities. | Git tools go to vcs. Editors and IDEs go to editors. python3-lsp-* goes to python-dev. |
| `vcs` | 10 | Version control and diff/merge tools. | diffutils goes to base. delta and difftastic go to devtools. |
| `editors` | 12 | Text editors, IDEs, notebooks and REPLs. | ktexteditor goes to kf6. TeX editors (lyx, texstudio) go to doctools. mc goes to files. |
| `doctools` | 26 | Documentation toolchains, man pages, markup converters and TeX typesetting. | kdoctools goes to kf6. Document viewers go to documents. python3-sphinx and python3-docutils go to python-libs. |
| `devlibs` | 34 | General-purpose C and C++ utility libraries with no domain. | Serialisation libraries (JSON, YAML, XML, protobuf) go to formats. Numerics go to sci-libs. Toolkit add-ons go to qt-extra or gtk. |
| `formats` | 34 | Data-format and serialisation libraries (XML, JSON, YAML, TOML, INI, RDF, protobuf, thrift), plus data query and convert tools. | serd, sord, sratom and zix go to audio-dsp (the LV2 stack). Office document import libraries go to office. Archive formats go to archiver. |
| `database` | 15 | Database engines, embedded key-value stores, database servers, clients and full-text indexes. | python3-psycopg2 goes to python-libs. baloo goes to kf6. |
| `archiver` | 28 | Compression libraries and archive tools. | karchive goes to kf6. Disk images and imaging tools go to disk. |
| `shells` | 10 | Interactive shells and their add-ons, terminal multiplexers, and terminal emulators. | bash goes to base (the system shell). mosh goes to servers. vte3 and kpty are libraries, in gtk and kf6. |
| `cli` | 19 | Command-line productivity utilities: search, view, navigation and text wrangling in the terminal. | File managers go to files. Process and system monitors go to sysmon. jq and yq go to formats. |
| `files` | 11 | File managers, disk-usage analysers, duplicate finders and file locators. | Partitioning goes to disk, filesystem tools to filesystem, archive tools to archiver. |
| `sysmon` | 8 | Process, resource and system-information monitors. | powertop goes to hardware. Network monitors (iftop, bandwhich) go to net-diag. lnav goes to cli. |
| `hardware` | 27 | Device access, bus and sensor tools, power management, and Bluetooth. | Input devices go to input. Phones and cameras go to mobile. Bluetooth audio codecs (sbc, ldacbt, libfreeaptx, liblc3) go to audio-codecs. bluez-qt goes to kf6. Microcontroller flashing goes to embedded. |
| `input` | 15 | Input-device libraries, keyboard data, input methods and on-screen keyboards. | xkbcomp and libxkbfile go to x11. Spell checking goes to spelling. Unicode and text-shaping libraries go to i18n. |
| `mobile` | 14 | Phones, media players and cameras over USB or the network. | SDR and radio devices go to sdr-hw. Microcontroller boards go to embedded. |
| `disk` | 29 | Block devices: partitioning, RAID, LVM, block encryption, SMART, NVMe, recovery, image writing and async I/O. | Filesystem creation and repair go to filesystem. Disk-usage tools go to files. Burning goes to optical. |
| `filesystem` | 20 | Filesystem utilities, FUSE, and network or encrypted filesystems. | fuse-overlayfs goes to containers. samba goes to servers. |
| `optical` | 13 | CD, DVD and Blu-ray burning, mastering and CD reading or ripping. | DVD and Blu-ray playback libraries (libdvdread, libdvdnav, libdvdcss, libbluray, libudfread) go to video-libs. |
| `crypto` | 31 | Cryptographic libraries, PKCS#11, smartcards and TPM. | kwallet goes to kf6. The gnupg program and password managers go to security. Chat end-to-end encryption libraries (olm, libomemo-c) go to chat. python3-cryptography goes to python-libs. |
| `security` | 20 | Security applications: GnuPG and front-ends, password and secret managers, certificates, scanners, forensics and audit. | Crypto libraries go to crypto. Login and PAM go to auth. Sandboxing goes to containers. Packet capture goes to net-diag. |
| `containers` | 18 | Container runtimes, image tools, container networking and sandbox primitives. | Virtual machines go to virt. libslirp goes to virt, because qemu consumes it. |
| `virt` | 25 | Virtual machines, their tooling, and remote-desktop and VNC protocols. | swtpm goes to crypto. Emulators of old consoles and computers go to emulators or libretro. Containers go to containers. |
| `net-libs` | 33 | Networking and protocol libraries. | Mail libraries (gmime, libetpan) go to mail. XMPP and Matrix libraries go to chat. kdnssd goes to kf6. protobuf and thrift go to formats. libnftnl goes to network. |
| `network` | 28 | Host network configuration and connection management: links, Wi-Fi, DHCP, DNS forwarding, time, modems, firewall and VPN. | networkmanager-qt and modemmanager-qt go to kf6. Diagnostics and capture go to net-diag. Servers go to servers. |
| `net-diag` | 15 | Network diagnostics, capture, measurement and low-level socket tools. | nmap and aircrack-ng go to security. HTTP API clients (xh, hurl) go to devtools. |
| `servers` | 10 | Daemons that serve other machines: remote shell, web, mail, XMPP, IRC, CalDAV/CardDAV, SMB and DLNA. | Database servers go to database. syncthing goes to transfer. openldap goes to auth. Client-only MQTT libraries go to net-libs. |
| `transfer` | 14 | File transfer, synchronisation, backup and BitTorrent. | Serial-line transfer (lrzsz) goes to embedded. Web browsers go to browsers. |
| `mail` | 13 | Email clients, fetch and send tools, mail libraries, and feed readers. | kmime, kcontacts and kcalendarcore go to kf6. Calendars and contacts go to pim. |
| `chat` | 16 | Instant messaging, IRC, XMPP, Matrix, fediverse and voice clients, with their protocol and E2E libraries. | ngircd and prosody go to servers. Meshtastic and Reticulum clients go to hamradio. |
| `browsers` | 10 | Web, Gemini and text-mode browsers. | Engines and their component libraries go to web-engines. qt6-qtwebengine goes to qt6. |
| `web-engines` | 14 | Browser engines and the libraries that make them up. | qt6-qtwebengine goes to qt6 (prefix rule). pdfium goes to documents. gumbo-parser goes to formats. |
| `x11` | 42 | X11 protocol, client libraries and data, plus Xwayland, the one X server. | libxkbcommon and xkeyboard-config go to input. motif goes to toolkits. There is no xorg-server: hard rule 4. |
| `wl` | 17 | Wayland libraries and protocols, wlroots, and Wayland-only desktop utilities. | foot goes to shells. VNC servers (wayvnc, neatvnc) go to virt. wf-recorder goes to video-tools. kwayland and plasma-wayland-protocols go to kde. qt6-qtwayland goes to qt6. The cell-drawn desktop is in src/desktop and is never shelved. |
| `gpu` | 25 | The GPU stack: Mesa, DRM, GL and Vulkan loaders and headers, shader compilers, VA-API, OpenCL. | libclc and spirv-llvm-translator go to toolchain. Video codec libraries go to video-libs. |
| `graphics-libs` | 25 | 2D rendering, vector and SVG, text layout, colour management and graph drawing libraries. | Raster image codecs go to image-libs. freetype2 and fontconfig go to fonts. 3D goes to 3d. |
| `image-libs` | 43 | Raster image formats, codecs, metadata, processing, barcodes, and terminal image output. | kimageformats goes to kf6 and qt6-qtimageformats to qt6. Image applications go to graphics. OCR goes to printing. |
| `graphics` | 23 | Graphics applications: painting, vector, photo development and management, image viewers, screenshot and image optimisers. | 3D modelling goes to 3d. CAD goes to cad. grim and slurp go to wl. |
| `3d` | 14 | 3D modelling, animation, voxel and mesh tools and libraries, and scientific visualisation. | Engineering CAD and geometry kernels go to cad. Slicers go to fabrication. Game engines go to game-libs. |
| `cad` | 17 | Mechanical, architectural and 2D CAD, geometry kernels, meshing and FEM. | Electronics CAD goes to eda. 3D printing and CNC go to fabrication. geos goes to gis. |
| `fabrication` | 13 | 3D-printing slicers and hosts, and CNC control. | python3-pyarcus, python3-pynest2d and python3-pysavitar go to python-hw (prefix rule). Paper printing goes to printing. |
| `fonts` | 24 | Font packages, and font libraries and tooling. | harfbuzz and pango go to graphics-libs. fcft goes to wl. perl-font-ttf goes to perl-cpan. |
| `themes` | 7 | Icon, cursor, sound and widget themes, and theme-configuration tools. | breeze-icons and kcolorscheme go to kf6 (step 3). The KDOS cursors, icons and GTK theme are in src/art. |
| `xdg` | 15 | freedesktop desktop integration: portals, MIME, desktop entries, AppStream, tray and indicators, notifications, event sounds, script dialogs. | xdg-dbus-proxy and bubblewrap go to containers. xdg-desktop-portal-kdos is in src/desktop. geoclue goes to gis. |
| `accessibility` | 9 | Screen readers, speech, braille and alternative input. | qt6-qtspeech goes to qt6. On-screen keyboards (wvkbd) go to input. |
| `i18n` | 14 | Unicode, locale, text shaping and segmentation, conversion, and translation. | musl-locales and libintl go to toolchain. Spell checkers go to spelling. translatelocally goes to ai. Input methods go to input. |
| `spelling` | 10 | Spell checking, hyphenation, thesaurus and their dictionaries. | sonnet goes to kf6. Reference dictionaries such as goldendict-ng and sdcv go to education. |
| `gtk` | 28 | The GLib and GTK platform and GNOME/Xfce desktop-platform libraries: widgets, settings, bindings, GIO plug-ins. | GNOME-hosted libraries that have a domain go to that domain: libsoup3 to net-libs, librsvg to graphics-libs, goffice to office, libsecret to security, json-glib to formats. GNOME applications are filed by function. The desktop links no toolkit: hard rule 5. |
| `qt6` | 29 | The Qt 6 modules, and everything in `group = qt6`. | Third-party Qt add-ons go to qt-extra. PyQt6 goes to python-gui. |
| `qt5` | 11 | The Qt 5 modules, `group = qt5`. | qca-qt5 goes to crypto and qwt-qt5 to qt-extra, following their own groups. |
| `qt-extra` | 18 | Third-party and KDE-hosted Qt add-on libraries that are not in KF6. | KDE Frameworks go to kf6. qtkeychain goes to security. coin and soqt go to cad. |
| `kf6` | 67 | KDE Frameworks 6: every port whose homepage is under invent.kde.org/frameworks, whatever it does. | Non-framework KDE libraries go to their domain or to kde. KDE applications are filed by function. |
| `kde` | 8 | KDE Plasma and Gear platform pieces outside Frameworks that have no other domain: KIO workers, thumbnailers, Plasma integration, KDE Wayland glue. | Domain libraries go to their domain: analitza to math, libkdegames to game-libs, libksane to printing, libkexiv2 to image-libs, libkleo to security, pulseaudio-qt to audio-io. There is no Plasma shell: hard rule 5. |
| `toolkits` | 4 | Other GUI and TUI toolkits for applications. | GTK goes to gtk, Qt to qt6 or qt5, SDL to game-libs. wxPython goes to python-gui. Tk goes to lang. |
| `audio-io` | 15 | The sound stack: ALSA, PipeWire and WirePlumber, PulseAudio client, and portable audio I/O APIs. | DSP goes to audio-dsp. Codecs go to audio-codecs. MIDI goes to music-libs. |
| `audio-codecs` | 28 | Audio codecs, containers and tagging, including the Bluetooth audio codecs. | Tracker, chip and MIDI formats go to music-libs. ffmpeg and gstreamer go to media-frameworks. |
| `audio-dsp` | 27 | Audio DSP, resampling, analysis, spatial audio, and the LV2/LADSPA/Vamp plug-in APIs with their RDF libraries. | Audio applications go to studio. raptor2 goes to formats. |
| `music-libs` | 17 | Music-format libraries: tracker modules, chip emulation, MIDI, OSC, synthesis and soundfonts. | Tracker applications go to studio. Players go to music-players. |
| `studio` | 12 | Music production: DAWs, audio editors, trackers, drum machines and notation. | Plug-in libraries go to audio-dsp. Video editing goes to video-tools. |
| `music-players` | 10 | Music players, library managers and taggers. | Video players go to video-players. Ripping goes to optical. |
| `video-libs` | 22 | Video codecs, containers, subtitles, disc playback and effects libraries. | ffmpeg, gstreamer and mlt go to media-frameworks. GPU video acceleration (libva) goes to gpu. |
| `media-frameworks` | 17 | Multimedia frameworks, capture, and media inspection. | qt6-qtmultimedia goes to qt6. phonon goes to qt-extra. |
| `video-players` | 8 | Video players and media centres, and their front-end add-ons. | mpvqt goes to qt-extra. |
| `video-tools` | 9 | Video editing, transcoding, muxing, recording, streaming and webcam apps. | Libraries go to video-libs or media-frameworks. Screenshots go to graphics or wl. |
| `game-libs` | 18 | Game engines and game-development libraries, SDL in all versions, and shared game libraries. | Game data goes with its game. Emulator cores go to libretro. |
| `games-action` | 9 | Action, arcade, racing, platform and sandbox games. | First-person shooters go to games-shooter. |
| `games-shooter` | 11 | First-person shooters, Doom and Quake engines, and their data. | Arcade shooters go to games-action. |
| `games-strategy` | 12 | Strategy, simulation and space-trading games, and their data. | Board games and chess go to games-board. |
| `games-rpg` | 8 | RPGs, roguelikes and interactive fiction. | The flare engine goes to game-libs. |
| `games-board` | 18 | Board, card, puzzle, chess and classic small games, whatever toolkit or desktop project they come from. | Educational games go to education. |
| `emulators` | 18 | Standalone emulators of computers and consoles, and compatibility layers. | libretro cores and RetroArch go to libretro. Virtual machines go to virt. xa goes to embedded. |
| `libretro` | 12 | RetroArch, its assets, databases and libretro cores. | libretro-mgba goes to emulators (group = mgba). |
| `education` | 16 | Learning software, offline encyclopedias and reference dictionaries. | Mathematics apps (kalgebra, cantor) go to math. marble goes to gis. |
| `office` | 22 | Office suites, word processors, spreadsheets, presentations, accounting and finance, and office-format import libraries. | PDF and ebook go to documents. Calendars, tasks and notes go to pim. Label printing goes to printing. |
| `documents` | 20 | PDF, PostScript, DjVu, EPUB, CHM and DOCX viewers, libraries and tools, and ebook managers. | ghostscript goes to printing. OCR goes to printing. Document authoring goes to doctools. |
| `pim` | 12 | Personal information: calendars, contacts, tasks, time tracking, notes and genealogy. | kcalendarcore and kcontacts go to kf6. Mail goes to mail. radicale goes to servers. |
| `printing` | 26 | Printing, printer drivers, labels, scanning and OCR. | python3-cups goes to python-libs. 3D printing goes to fabrication. |
| `math` | 24 | Mathematics applications and libraries: computer algebra, arbitrary precision, solvers, calculators, statistics and plotting apps. | BLAS, LAPACK and FFT libraries go to sci-libs. The R language goes to lang. python3-mpmath and python3-gmpy2 go to python-sci. |
| `sci-libs` | 21 | Numerical and scientific-data libraries, and the unprefixed core Python science stack. | python3-* science modules go to python-sci. Domain science goes to astronomy, bioscience or gis. |
| `astronomy` | 14 | Astronomy, planetaria, ephemerides, FITS and satellite prediction. | python3-pyerfa, python3-sgp4 and python3-jplephem go to python-sci. Weather-satellite decoding (aptdec, satdump) goes to sdr. |
| `bioscience` | 12 | Bioinformatics, chemistry and medical informatics or imaging. | General numerics go to sci-libs. |
| `gis` | 20 | GIS, maps, projections, GPS, navigation and location. | libspatialite goes to database. qt6-qtlocation and qt6-qtpositioning go to qt6. python3-shapely and python3-owslib go to python-sci. |
| `eda` | 29 | Electronics: schematic and PCB, circuit simulation, antenna modelling, FPGA synthesis and simulation, logic analysers and bench instruments. | Microcontroller toolchains and flashers go to embedded. python3-pyvisa goes to python-hw. |
| `embedded` | 27 | Microcontroller and bare-metal development: cross toolchains, C libraries, flashers, debug probes and serial consoles. | python3-esptool goes to python-hw. FPGA goes to eda. Radio firmware goes to hamradio. |
| `sdr-hw` | 18 | SDR hardware drivers and the SoapySDR device layer. | DSP libraries and receiver applications go to sdr. |
| `sdr` | 24 | SDR DSP libraries, GNU Radio and its blocks, receivers, decoders and signal analysis. | Operator applications for amateur radio go to hamradio. |
| `hamradio` | 25 | Amateur-radio operation: rig control, digital modes, logging, packet and APRS, propagation, and mesh radio. | python3-rns, python3-lxmf and python3-meshtastic go to python-net (prefix rule). SDR receivers go to sdr. Map servers and GIS libraries go to gis. |
| `ai` | 7 | Machine learning inference, speech recognition, computer vision and neural translation. | piper (TTS) goes to accessibility. tesseract goes to printing. |
| `python` | 36 | The python3 interpreter, tkinter, and Python build, packaging and extension tooling. | Runtime libraries go to python-libs. Applications written in Python go by function. |
| `python-libs` | 63 | General-purpose python3-* runtime libraries: text, templating, markup, parsing, config, CLI, system, files, crypto, imaging (split out python-text when this passes about 75). | HTTP and web go to python-net. Numerics go to python-sci. GUI bindings go to python-gui. |
| `python-net` | 22 | python3-* HTTP, web-framework, TLS-trust and network-stack modules. | python3-tornado and python3-pyzmq go to python-dev (the Jupyter stack). |
| `python-sci` | 13 | python3-* science, numerics, astronomy and geo modules. | The unprefixed numpy, scipy, pandas and matplotlib go to sci-libs, and astropy and skyfield go to astronomy. |
| `python-gui` | 15 | python3-* GUI toolkit, desktop and D-Bus bindings. | python3-pyside6 goes to qt6 (group = qt6). python3-qscintilla goes to qt-extra (group = qscintilla). python3-tkinter goes to python (group = python3). |
| `python-dev` | 13 | python3-* Jupyter, kernel, language-server and debugger modules. | jupyterlab and ipython (unprefixed) go to editors. |
| `python-hw` | 12 | python3-* hardware, serial, USB, instrument, CAN and device-protocol modules. | python3-meshtastic goes to python-net. |
| `perl-cpan` | 18 | The perl interpreter and every perl-* CPAN module (the shelf cannot be named `perl`, which is a port). | exiftool (unprefixed) goes to image-libs, and applications written in Perl go by function. |

`toolkits` keeps 4 ports (wxwidgets, fltk, motif, stfl). It is a distinct domain, and its scope is
"widget toolkits for applications, GUI and TUI".

---

## 4. Tooling changes

Every site below assumes that a port sits directly under `ports/core`. "Silent" marks a site that
succeeds on an empty set instead of failing. Silent sites are the danger: with no change, the build
sees zero ports and reports success.

### 4.1 One resolver, shared

| Site | Change |
|---|---|
| `src/libs/libkpkg/kp_conf.c:150-162` `kp_port_dir` | Per repository, try `<repo>/<name>/kpkgbuild` first; `src/` and the test fixtures stay flat. Then try `<repo>/<shelf>/<name>/kpkgbuild` over a sorted shelf list, cached once per `KpConf`. **A second hit is a hard error naming both paths**, like T2's `detect_confdir`; it is not a silent "first wins". |
| `src/libs/libkpkg/kp_conf.c:165-218` `kp_all_ports` | Descend exactly one level; deeper nesting is an error. **Silent today:** it returns 0 ports, which breaks `kdos update`, `kdos cve` and `kdos-portup`. |
| `src/libs/libkbuild/kb_plan.c:106-150` `kbuild_ports` | Call the libkpkg walker instead of its own `kb_listdir`, so there is one walker in one library. **Silent today:** the `kdosbuild` picker lists no core ports. |
| `src/libs/libkpkg/kpkg.h:22-23`, `kp_conf.c:62-69` | Rewrite the repository contract comment to say a repository holds ports directly or one shelf down. Warn when `split_repos` truncates. |
| `src/libs/libkpkg/kp_hash.c:188-198` | No code change. Add a comment stating the D8 constraint: no shelf may be named `libs`. |
| `src/libs/selftest.c:1142-1185` | Add fixtures for a two-level repository, a duplicate name in two shelves (must fail), a shelf with no ports, and a flat repository (must still resolve). |
| `script/util/port.sh:72,144` | Phase 0 and phase 1 (15 scripts) run before kpkg exists, so this needs its own shell lookup: `for d in "$WORKSPACE"/ports/core/"$port" "$WORKSPACE"/ports/core/*/"$port"; do [ -f "$d/kpkgbuild" ] …`. Exactly one match is allowed. |

### 4.2 Consumers

| Site | Change |
|---|---|
| `script/06_packaging/00_orphans.sh:30-40` | **Highest severity.** Unchanged, every installed core package counts as an orphan and is `kpkgdel`'d, and an empty ISO ships. Ask `kpkg meta <pkg>`, or use the shell lookup from `port.sh`. |
| `script/06_packaging/02_iso.sh:137` | `ls …/sources/ports/core \| wc -l` would count shelves. Use `find … -name kpkgbuild \| wc -l`. |
| `ports/fetch:905-907`, `647-651` | Iterate `*/kpkgbuild` and `*/*/kpkgbuild`. **Silent today:** `make fetch` fetches nothing and exits 0. `--tree` must still accept an old flat checkout. |
| `ports/fetch:811-812` `ver()` | Resolve by name. Unchanged, it passes empty rust/go/node/ghc/cabal versions into the fetch container build. |
| `ports/publish:179-199` `port_lines` | Descend one level. `CARRIED[…]` at line 196 is keyed by the real path, shelf included. |
| `ports/publish:393` `index_add` | Keep labels as `<port>/<file>`: strip the shelf on the `--history` path (lines 640-643). |
| `ports/publish:554-556` `--freeze` | Change the pattern to `^ports/core/([^/]+/)?[^/]+/kpkgbuild$`, so a tag from before or after the move both work. |
| `script/hooks/pre-push:88-90,102` | Take the port name as the path component before `/kpkgbuild`, not the first one after `ports/core/`. Key `carried[…]` by the full path. Unchanged, a later push carrying a new hash for a git-tracked file is refused, and 41 such files exist. The move commit itself pushes cleanly: it adds no hash. |
| `testing/preflight.sh` | This is about 44 lines. Most use `for d in ports/core/*`, which is vacuous after the move: `[ -f "$d/kpkgbuild" ] \|\| continue` skips every shelf. Glob one level deeper at 139, 190, 235, 278, 312, 444, 591, 673, 704, 716, 742-743, 792, 1164, 1458, 1479 and 1735. Resolve by name at 59, 86-90, 142, 622, 891 and 916. Count with `find` at 735 and 1875. Fix the label at 1169. Make the `a[n-1]`/`sed` sets match at 1284-1302. Put the probe two levels deep at 1302. Descend in the Python block at 1799-1803. |
| `testing/selftest.sh:420,1022,1658,1702-1704,2874,3173,5028` | Resolve each hard-coded `ports/core/<x>/…` by name. Unchanged, the wlroots dumps are **silently** skipped. |
| `testing/test_runner.py:16-22` | Descend one level. Unchanged, it runs `kpkg install -f <shelf>`. |
| `ports/core/linux/genlogo-mono.py:18,41` | The default output path is in code. Derive it from `__file__`. |
| Path-bearing comments | `src/packages/kdos-kpkg/decl.c:25`, `02_iso.sh:168`, `script/04_phase4/packages.txt:401,1116,1889,3169`, `script/05_desktop/packages.txt:25`, `ports/core/{fastfetch,wireplumber,rust-analyzer}/build.sh`, `lua54/build.sh:11,18,46`. Rewrite each to name the port, not the path (rule 2 above). The recipe comments are covered by hard rule 1. Changing them changes those ports' hashes, so do it in its own change, after the move. |

### 4.3 Unchanged, verified

- `PORT_REPO` in `script/phase4.env.sh:22`, `phase5.env.sh:22` and `desktop.env.sh:24`
- `kpkg.conf:12`
- `chroot_exec.sh:82,96`, `Makefile:89` and `prepare_base.py:37`
- `rebuild.c:69`
- `kdos-portup` (`main.c:2016` and its flat fixture)
- `front.c`, `build.c`, `binhost.c`, `update.c`, `cve.c`, `march.c`, `why.c` and `kdos-shell/doc.c`, which all go through `kp_port_dir`
- `srclib.sh` and `ports/update`
- `.gitattributes`, whose patterns are not tied to paths
- `fs/etc/skel/.config/lf/lfrc:93`

### 4.4 `.gitignore`: this must be fixed before anything is staged

`.gitignore:42-60` ignores `/ports/core/*/*.tar` and its siblings. After the move,
`git check-ignore --no-index ports/core/base/foo/foo-1.tar.gz` returns 1 (not ignored), so
`git add -A` would stage every fetched tarball. This was measured.

- Rewrite the patterns to `/ports/core/*/*/*.tar` and so on.
- Give the four per-port lines their shelf: `digikam`, `fluidr3-gm-sf3`, `meshtastic-firmware`,
  `rnode-firmware`.
- Verify with `git check-ignore` on both depths.

### 4.5 The in-chroot `kpkg` and installed systems

`kpkg` inside the chroot is compiled only by `script/01_phase1/12_kpkg.sh`; it has no recipe. A
moved tree with the old `/usr/bin/kpkg` resolves no port. Refresh it with
`--phases 01_phase1,… --steps 01_phase1:12_kpkg.sh`.

The same holds for an installed system: `kdos update`, `kdos cve` and `kdos rebuild` use the
install's own libkpkg. An older install pointed at a shelved tree sees zero ports. **The resolver
change ships in a release before the tree moves**, or at the latest in the same release.

A libkpkg edit rebuilds all 24 `src/` ports (`kp_hash.c:196-198`), but none of the 1,999 core
ports.

---

## 5. Package lists and the catalogue

- **`script/*/packages.txt`.** Each list's comment groups become one group per shelf, titled
  `<shelf> — <shelf description>`, in the order shelves appear in `ports/shelves`. The port order
  inside a list is **not free**, because some comments pin an order. Example: toybox must come
  before the ports that take back names it compiles out. So:
  1. Before editing, record `kpkgdepends` output for each list, with
     `source script/<phase>.env.sh && PKGDB_DIR=/dev/null kpkgdepends $(grep -v '^#' packages.txt)`.
  2. Regroup the list.
  3. Record the output again. Regrouping reorders independent ports, so the two orders cannot be
     byte-identical. The gate is narrower and exact:
     - the two outputs hold the **same set** of ports;
     - every **ordering-comment run** (for example toybox, then the owners it hands names back
       to) keeps its relative order;
     - every **contested path** keeps its winner. Take each path listed by more than one
       package's manifest in the current `build/fs/var/lib/kpkg/db`; whoever comes last owns it
       (`build-system.md`, "whoever comes last in the dependency order owns the path"). The
       relative order of those claimants must be the same in both outputs.

     A pinned run that cannot move stays as one block, with its ordering comment kept. With the
     `packages.d/` split (Part C), such runs go into `00-order.txt`.
- **Explanatory comments.** Comments inside `packages.txt` that explain a port move with the port,
  unchanged.
- **The ports catalogue.** `docs/kdos/06-reference/ports-catalogue.md` gets a primary view by
  shelf: one table per shelf in `ports/shelves` order, with the columns name, version,
  description and phase. The per-phase view shrinks to one line per port naming the phase and the
  shelf, or is dropped if the shelf tables carry the phase column. The page's generator, or its
  measurement commands, change with it.
- **`kpkg info` and `kpkg meta`.** Printing the shelf is optional. It would be derived from the
  path only (D4).

---

## 6. Documentation (hard rule 1, in the same change)

29 pages under `docs/kdos` name `ports/core`. The ones that describe layout, lookup or counts:

- **`03-architecture/packaging.md`**
  - Rewrite "The three repositories" (69-84): a repository holds ports one shelf down; the lookup
    order; a name found twice is an error.
  - Rewrite "Phases, package lists and groups" (116-187), now that list groups follow the shelves.
  - Also update 112, 137-138, 181, 224, 238 and 512.
- **`05-developer/writing-ports.md`**
  - Update the anatomy tree (26).
  - Rewrite the search rule (67-68).
  - Add a new step, "choose the shelf", to the add-a-port procedure (577-621), with the placement
    rules of §2.
  - Update the `git commit ports/core/<shelf>/<port>` example (621).
  - Also update 48, 115, 118, 351, 526, 583, 654, 1762, 1777 and 1806.
- **`06-reference/repository-layout.md`**: 192-205, 333 and 393.
- **`06-reference/ports-catalogue.md`**: restructure it (§5).
- **`06-reference/glossary.md`**
  - Add a **shelf** entry.
  - Add a line to the `group` entry (270): "a shelf is a third, unrelated thing".
  - Update 491 and 588.
- **`05-developer/build-system.md`**: 103-116, 371 and 403.
- **`03-architecture/overview.md`**: 51-65. Shelves subdivide the core ring; they are not a ring.
- **`01-philosophy/decisions.md`**: add a decision, "Ports are shelved by subject; identity stays
  the bare name", covering D1-D8. Check 575-587 (the source-less ports hash their own directory
  alone), which stays true under D8.
- **`01-philosophy/principles.md:150-158`**: the lists are grouped by shelf.
- **Path examples in other pages**
  - `02-user-guide/administration.md`: 943-950
  - `04-programs/kdos-command.md`: 1111-1155
  - `build-troubleshooting.md`: 209, 240
  - `testing.md`: 916, 928, 1460
  - `how-kdos-is-built.md`: 37, 278
  - `developing.md`: 142, 379
  - `status.md`: 240-241. `find ports/core -name kpkgbuild` still counts correctly.
- **`README.md`**: 67, 205, 223 and 232.
- **`CLAUDE.md`**
  - Line 278: `ls ports/core | wc -l` would count shelves; change it to
    `find ports/core -name kpkgbuild | wc -l`.
  - Add a map row for "choosing a shelf".
- **Checks**: `testing/docscheck.sh` and `bash testing/docscheck.sh` must pass.

---

## 7. Guards

Each guard must be shown to fail on a planted fixture before it is trusted.

**Preflight**

1. Every `kpkgbuild` under `ports/core` is exactly at `ports/core/<shelf>/<name>/kpkgbuild`.
   Nothing is loose at depth 1, and nothing sits at depth 3.
2. Every `<shelf>` is listed in `ports/shelves`. Every listed shelf exists and is non-empty.
3. `name =` equals the directory name. This is not enforced anywhere today.
4. Every bare name is unique across `ports/core/*/*` and every `src/<area>/*` holding a `kpkgbuild`.
   Today a duplicate is silently shadowed (`kp_conf.c:198-203`, `kb_plan.c:134-138`).
5. No shelf is named `libs` or `core`, and no shelf shares a name with a port.
6. Every `group =` family sits on one shelf (placement rule 1).
7. The tarball ignore probe works at two levels.

**Pre-push hook**

Checks 1, 4 and 5 again as a cheap `git ls-tree` pass, so they hold even when preflight is
skipped.

---

## 8. Order of work

The user commits. Each step below is a natural commit boundary. **The build must never see a
half-moved tree.**

1. **Tools accept both depths, on the flat tree.**
   - Make the §4.1 and §4.2 changes, except the tightening in step 5.
   - Rebuild kpkg with
     `make build BUILD_ARGS="--phases 01_phase1,05_desktop --steps 01_phase1:12_kpkg.sh"`.
     All 24 `src/` ports rebuild.
   - Run `testing/preflight.sh`, `testing/selftest.sh`, and
     `CC="cc -fsanitize=address,undefined -g" testing/selftest.sh`. The resolver is a parser of
     the tree.
   - **Gate: on the flat tree nothing changes.** `kpkgdepends` for every list is byte-identical to
     HEAD's, and `make fetch-check` reports nothing missing.
2. **`ports/shelves` and `.gitignore`.**
   - Add the shelf list.
   - Rewrite the ignore patterns (§4.4).
   - Verify with `git check-ignore` at both depths.
3. **The move.**
   - Make sure no build or fetch is running.
   - Make one scripted pass of `git mv ports/core/<name> ports/core/<shelf>/<name>`, driven by
     the Appendix A table saved as a TSV.
   - **Edit no file in this commit**, so that rename detection shows 100% renames and
     `git log --follow` works.
   - **Gates:**
     - `git diff --cached -M --stat` shows only renames.
     - Hard-link counts on a sample of tarballs are unchanged.
     - `git status` shows no untracked tarball.
4. **Regroup the package lists and the catalogue (§5).**
   - **Gate:** the §5 check passes: same set of ports, pinned runs kept, every contested path
     keeps its winner.
5. **Tighten.**
   - Turn on the preflight rules "no loose port" and "closed shelf list", and the pre-push check.
   - Shell walkers stop accepting depth 1 under `ports/core`. libkpkg keeps accepting flat
     repositories, for `src/` and the fixtures.
6. **Documentation (§6)**, in the same change as each step it describes. It is listed separately
   only for completeness.
7. **Proof.**
   - Run `testing/preflight.sh`, `testing/selftest.sh`, `bash testing/docscheck.sh` and
     `make fetch-check`.
   - Build one port with `make build BUILD_ARGS="--phases 04_phase4,06_packaging --rebuild zlib"`.
   - Confirm that a no-op `04_phase4` pass rebuilds nothing: no hash moved.
   - Boot the ISO and run `kpkg meta <name>`, `kdos update check` and `kdos cve` against
     `/sources/ports`.
8. **Path comments inside recipes** (§4.2, last row). This is a separate change after the move,
   because it changes those ports' recipe hashes.

---

## 9. Open points for the user

- **Scope.** This plan does both things: it shelves the directories and regroups the package lists.
  Step 4 on its own (regrouping `packages.txt` and the catalogue with no directory move) is also a
  valid first delivery, and it carries no rebuild or path risk.
- **Borderline placements**, decided as shown, and easy to flip with one `git mv` each:
  - `onetbb` is in `devlibs`; the alternative is `sci-libs`.
  - `swig` is in `toolchain`; the alternative is `lang`.
  - `libspatialite` is in `database`; the alternative is `gis`.
  - `libnftnl` is in `network`, while `libmnl` is in `net-libs`.
  - `aml` is in `virt`; the alternative is `wl`.
  - `stk` and `sox` could go either way.
  - `libmodbus` and `mbpoll` are in their current shelves; the alternative is `eda`.
- **KDE and GNOME apps are scattered by function** (rule 5). T2 does the opposite: it has `kde/`
  and `gnome/` repositories of 437 and 368 packages. The choice here keeps shelves by subject and
  avoids two dumping grounds. It costs the ability to address "all of KDE" by directory; if that
  is needed, it belongs in a list, not in the tree.
- **Granular phases** (Part C2a): 13 phases instead of 8. The six new snapshots are not yet
  measured; any phase can opt out of its snapshot.
- **Phase names** (Part C2): `cross`, `bootstrap`, `selfhost`, `foundation`, `compilers`, `lang`,
  `system`, `graphics`, `toolkits`, `apps`, `desktop`, `kernel`, `image`. Any of them can be renamed before step C5.2 at no cost; after it,
  renaming needs the migration again.
- **Growth.** A shelf over about 80 ports splits. A language reaching about 10 module ports gets its
  own shelf. `kf6` (67) and `python-libs` (63) are the nearest to the limit.

---

# Part B: reorganising `src/`

## B1. What is wrong with the current layout

`src/` has five directories, and two of them are grab-bags:

| Today | Holds | Problem |
|---|---|---|
| `src/libs/` | 17 `libk*` libraries and `selftest.c` | Nothing. It is one kind of thing. |
| `src/desktop/` (13) | `kdos-comp`, `kdos-shell`, `kdos-term`, `kdos-lock`, `kdos-res`, `kdos-boxsock`, `kdos-record`, `xdg-desktop-portal-kdos`, **and** `kdos-powerd`, `kdos-energyd`, `kdos-oomd`, `kdos-mountd`, `kdos-packd` | Five of the 13 are root daemons with no Wayland dependency. They sit here only because this directory is the one on the 05_desktop search path. |
| `src/packages/` (11 ports and `kdos-kpkg`) | tools, package manager, packer, box runtime, box init, installer, theme generators, icon/cursor/GTK themes, boot splash, the `kdos-bb` demo | "Not the desktop" is the only rule. `packages` says nothing, since everything under `src/` is a package. |
| `src/build/` | `kdosbuild` | This directory and `src/tools/` each hold one host-side build tool, split over two directories. |
| `src/tools/` | `kdos-portup` | See `src/build/`. |

## B2. The layout

Every port stays **exactly two levels below `src/`** (D5). `../../libs` still resolves to
`src/libs`, the recipe hashes of the 22 source-less `src/` ports are unchanged by the move, and
`kdos-installer` keeps `kdos-appbox` as its sibling.

| New directory | Members | What belongs here | On `PORT_REPO` in |
|---|---|---|---|
| `src/libs/` | the 17 `libk*` libraries, `selftest.c` | unchanged | (not a port repository) |
| `src/desktop/` | `kdos-comp`, `kdos-shell`, `kdos-term`, `kdos-lock`, `kdos-res`, `kdos-boxsock`, `kdos-record`, `xdg-desktop-portal-kdos` (8) | Programs that draw the session or serve it over Wayland or D-Bus | `05_desktop` only |
| `src/daemons/` | `kdos-powerd`, `kdos-energyd`, `kdos-oomd`, `kdos-mountd`, `kdos-packd` (5) | Root daemons the desktop account talks to. This matches the book's own chapter, `04-programs/daemons.md`. | `05_desktop` only |
| `src/system/` | `kdos-kpkg` (no recipe), `kdos-tools`, `kdos-pack`, `kdos-appbox`, `kdos-boxinit`, `kdos-installer` (6) | The system layer: the package manager, the `kdos` command and its services, packs and boxes, and the installer. `kdos-installer` compiles `kdos-appbox/catalogue.c`, so the two stay siblings. | phases 4, 5 and desktop |
| `src/art/` | `kdos-theme`, `kdos-icons`, `kdos-cursors`, `kdos-gtk-theme`, `kdos-splash`, `kdos-bb` (6) | Generated and hand-made visuals: theme generators, the themes built from them, the boot splash, and the demo | phases 4, 5 and desktop |
| `src/devtools/` | `kdosbuild`, `kdos-portup` (2) | Host-side build tools that are not ports. They replace `src/build/` and `src/tools/`. | (not a port repository) |

**Rules for a new program** (for `writing-desktop-software.md` and `repository-layout.md`):

1. A library goes to `src/libs`.
2. A program that is not installed on the target goes to `src/devtools`.
3. A program that draws, or that speaks the session's Wayland or D-Bus, goes to `src/desktop`.
4. A root daemon whose client is the desktop account goes to `src/daemons`.
5. Pictures, themes and their generators go to `src/art`.
6. Everything else goes to `src/system`.

**Why each boundary is where it is:**

- **The phase split is kept exactly.** Today phases 4 and 5 see `src/packages`, and only
  05_desktop also sees `src/desktop`. After the move, phases 4 and 5 see `src/system` and
  `src/art`, and 05_desktop adds `src/desktop` and `src/daemons`. Every port is visible in the
  same phases as before. `kdos-pack` stays visible to phase 4, which `kdos-tools` needs (`depends`
  names it).
- **The repository count stays under the cap.** 05_desktop has 5 repositories (`ports/core` plus
  4 in `src/`), under `KP_MAX_REPOS` = 8. Part A adds the truncation warning.
- **`src/libs` stays flat.** Its 17 libraries are one kind of thing, already namespaced by their
  `libk*` prefix. Grouping them (for example drawing libraries apart from system libraries) would
  change every `-I$LIBS/libk…` flag in 18 `build.sh` files and every library path in
  `kp_hash`'s tree. It would buy nothing that the prefix does not already give.
- **Not a single `src/<name>` level.** Flattening all 24 ports into `src/` would break
  `../../libs` and put the libraries beside the programs. It would also lose the phase split,
  which is the one structural job these directories do.

## B3. Sites to change

`git grep` finds about 580 references to `src/packages`, `src/desktop`, `src/build` and
`src/tools`: roughly 70 in code, scripts and tests, and the rest in the book. Some matches are
unrelated (`ports/core/opensc/openssl4.patch` and `ports/core/uosc/build.sh` name upstream `src/`
trees). The sites that change behaviour:

| Site | Change |
|---|---|
| `script/phase4.env.sh:22`, `phase5.env.sh:22` | `PORT_REPO="/ports/core /kdos/src/system /kdos/src/art"` |
| `script/desktop.env.sh:24` | `… /kdos/src/system /kdos/src/art /kdos/src/desktop /kdos/src/daemons` |
| `script/06_packaging/00_orphans.sh:30` | The `REPOS` list: the same four `src/` areas plus `/kdos/src/libs`. A miss here deletes the missing area's packages from the ISO. |
| `src/libs/libkbuild/kb_plan.c:108` `REPOS[]`, `kbuild.h:126-127` | `ports/core`, `src/system`, `src/art`. The desktop areas keep reaching the index through `packages.txt`, as `src/desktop` does today. Rewrite the comment. |
| `src/tools/kdos-portup/main.c:168,2016` | `src/system/kdos-kpkg`, and a `PORT_REPO` with `src/system src/art` |
| `src/packages/kdos-tools/rebuild.c:77,289,331` | `src/devtools/kdosbuild` |
| `src/packages/kdos-tools/cve.c:393` | The printed path of `secdb/vendor.py` |
| `script/kdosbuild.sh:24-26` | `src/devtools/kdosbuild` |
| `ports/update:26` | `src/devtools/kdos-portup` |
| `ports/srclib.sh:181,199-200` | `src/system/kdos-kpkg`. This is the host-side reader's rebuild-by-mtime check; a stale path would disable the check. |
| `script/01_phase1/12_kpkg.sh:31`, `13_kinstall.sh:34,54,57` | `src/system/kdos-kpkg`, `src/system/kdos-installer`, `src/system/kdos-appbox` |
| `testing/selftest.sh` (94 lines), `testing/preflight.sh` (65 lines) | Mostly per-program paths. Rewrite each, or add a helper that resolves a `src/` port by name. |
| Comments | `src/packages/kdos-pack/main.c:17`, which explains why `kdos-pack` is not beside `kdos-portup`; rewrite it against the new areas. `kb_json.c:18`, `kbuild.h:182`, `script/05_desktop/packages.txt:25`, `script/04_phase4/packages.txt` (8 lines). `LICENSE.notice` files in `kdos-cursors`, `kdos-icons` and `kdos-gtk-theme` name their own path. `src/packages/kdos-appbox/catalogue:1`. |

Unchanged:

- `script/chroot_exec.sh:101-102`, which binds all of `src/` at `/kdos/src`
- `.gitignore:11`, which is about `build/`
- `testing/goldens/vt-vim.txt` and `testing/fixtures/vt/vim.esc`, which record a terminal stream
  and contain the text by accident. Verify this before leaving them.

**Book pages** (hard rule 1): `repository-layout.md` (17 lines, including the table at 192-205 and
the phase-1 special cases at 221-227), `status.md` (16), `writing-desktop-software.md` (11),
`overview.md` (11), `build-system.md` (10, including 78-89, the reason the desktop is its own
phase, and 115-116), `command-index.md` (9), `writing-ports.md` (9), `04-programs/README.md` (9),
`packaging.md` (9, including the "three repositories" table at 69-84, which becomes five),
`why-kdos.md` (8), `glossary.md`, `how-kdos-is-built.md`, `developing.md`, `decisions.md` (6 each,
including 584), `README.md` (6), `ports-catalogue.md`, `c-libraries.md` (5 each), and single lines
in about 20 more pages. `CLAUDE.md`'s map needs no change; its build table names `--rebuild
<port>`, not paths.

**Guards:** extend the preflight uniqueness check (§7, check 4) to every `src/<area>/*` holding a
`kpkgbuild`. Add a check that every `src/<area>` is one of the six, that no port sits at any other
depth under `src/`, and that `00_orphans.sh`'s `REPOS` names every area that holds a recipe.

**Observed while measuring (not part of this plan):** `kdos-installer` is source-less, so its
recipe hash covers its own directory and `src/libs`, but not `../kdos-appbox/catalogue.c`, which
it compiles. An edit to the catalogue does not change the installer's hash. The move keeps the
two siblings, so this gap neither grows nor shrinks.

## B4. Order

`src/` moves **as its own step, before Part A's step 3**. Part A's step 1 already edits libkpkg,
which rebuilds all 24 `src/` ports, so the `src/` move rides the same rebuild.

1. One commit that renames only: `git mv` into the six areas, with no edits, so that rename
   detection shows 100% renames.
2. In the same change, update every §B3 site and the book.
3. Gates:
   - `kpkgdepends` for each phase list is byte-identical before and after.
   - Each `src/` port's recipe hash, the `E:` field the binhost index writes (`binhost.c:283`),
     is unchanged before and after the rename. There is no command that prints the hash on its
     own, so either index both trees or add a small selftest case.
   - `testing/preflight.sh` and `testing/selftest.sh` pass.
   - `make build BUILD_ARGS="--phases 05_desktop --rebuild kdos-shell"` passes, and so does the
     phase-1 kpkg step.


---

# Part C: reorganising `script/`

## C1. What is wrong with the current layout

`script/` holds 48 files. The orchestrator (`kb_phase.c:190-240`) runs every `script/<N>_<name>/`
directory in sorted order, and sources `script/<name>.env.sh` for it.

| Problem | Where |
|---|---|
| Four of eight phase names are only numbers: `phase1` to `phase5`. The names say nothing about what the phase builds. `03_phase3` is the "Toolchain" phase while `00_toolchain` is the *cross* toolchain. | `01_phase1` … `05_phase5` |
| Two phases share the number 05. The desktop runs before the kernel only because `d` sorts before `p` (`build-system.md` states this). | `05_desktop`, `05_phase5` |
| No gaps in the numbering. A new phase between two others renames every later one, and so every snapshot, log directory and doc reference. | all |
| The phase environment files sit loose at the root, apart from the phases they configure, and they are named by the part of the name after the number. | `script/*.env.sh` (8 files) |
| **About 20 lines are copied into every environment file**: reproducibility (`SOURCE_DATE_EPOCH`, `TZ`, `LC_ALL`, `-ffile-prefix-map`, `--build-id`), `MAKEFLAGS`, `KPKG_STRICT_RECIPE`, and for chroot phases `PKG_CONFIG_PATH`, `CC`, `CXX` and base `CFLAGS`. A change to one has to be made six to eight times. | `script/*.env.sh` |
| Step numbers collide. `06_gzip.sh`, `06_tar.sh` and `06_toybox.sh` run in alphabetical order. Eight of the ten packaging steps are `00_*`, so their real order is alphabetical too. | `01_phase1/`, `06_packaging/` |
| Phase 4 is one 3,203-line list that names 1,685 ports and installs 1,842. Its 183 comment groups are not a sound grouping (Part A §5). Phase 3 spends 7.6 of its 8.7 hours on 22 compiler ports, with no restore point in between. | `04_phase4/packages.txt`, `03_phase3` |
| `util/` mixes a library that is sourced (`port.sh`) with a program that one packaging step runs (`psf2limine.py`). The chroot and orchestrator wrappers sit loose at the root. | `util/`, `chroot_*.sh`, `kdosbuild.sh` |

## C2. The layout

```
script/
  phases/
    00_cross/        phase.env  00_binutils.sh 01_gcc.sh
    10_bootstrap/    phase.env  000_file_system.sh … 130_kinstall.sh
    20_selfhost/     phase.env  packages.txt
    30_foundation/   phase.env  packages.txt
    31_compilers/    phase.env  packages.txt
    40_lang/         phase.env  packages.d/<shelf>.txt …
    41_system/       phase.env  packages.d/00-order.txt packages.d/<shelf>.txt …
    42_graphics/     phase.env  packages.d/<shelf>.txt …
    43_toolkits/     phase.env  packages.d/<shelf>.txt …
    44_apps/         phase.env  packages.d/<shelf>.txt …
    50_desktop/      phase.env  packages.txt
    60_kernel/       phase.env  packages.txt
    70_image/        phase.env  10_binhost.sh … 110_iso.sh  psf2limine.py
  env/
    common.env       every phase: reproducibility, MAKEFLAGS, KPKG_STRICT_RECIPE
    chroot.env       chroot phases: PKG_CONFIG_PATH, CC/CXX, base CFLAGS, the work-dir reset
  lib/
    port.sh          sourced by 00_cross and 10_bootstrap steps
  chroot/
    enter.sh         (was chroot_enter.sh)
    exec.sh          (was chroot_exec.sh)
  kdosbuild.sh       the host wrapper the Makefile runs
  hooks/pre-push     unchanged: every clone has set core.hooksPath=script/hooks
```

| Old | New | Short name (`--phases`) | Title |
|---|---|---|---|
| `00_toolchain` | `00_cross` | `cross` | Cross Toolchain |
| `01_phase1` | `10_bootstrap` | `bootstrap` | Base Userland |
| `02_phase2` | `20_selfhost` | `selfhost` | Self-Hosting Bootstrap |
| `03_phase3` | `30_foundation` + `31_compilers` | `foundation`, `compilers` | Build Foundation; Compilers |
| `04_phase4` | `40_lang` … `44_apps` (five phases) | `lang`, `system`, `graphics`, `toolkits`, `apps` | see C2a |
| `05_desktop` | `50_desktop` | `desktop` | Desktop |
| `05_phase5` | `60_kernel` | `kernel` | Kernel |
| `06_packaging` | `70_image` | `image` | Image |

**Rules the layout keeps:**

- **Phase order is unchanged at the coarse level.** Every new phase sorts where its old phase did.
  The desktop still runs before the kernel, now because 50 < 60 rather than because `d` < `p`.
  Inside the old phases 3 and 4, the port order changes, so the Part A §5 ownership gate applies
  across the new phases.
- **Gaps.** Bands of ten, with the split phases numbered inside their band. A phase can be added
  between two others without renaming either.
- **Step order inside a phase is unchanged.** Steps are renumbered in their current sorted order,
  with gaps. In `10_bootstrap`, the 06 group becomes `060_gzip`, `061_tar`, `062_toybox`. In
  `70_image`, the eight `00_*` steps become `10_binhost` … `80_whatis` in today's alphabetical
  order. Any order that is really required is then visible in the number; the
  `anything-a-chroot-prints-is-parsed` reason and the `00_orphans.sh` comments say which ones are.
- **Each phase is self-contained.** Its environment is `phase.env` inside the phase directory.
  It is not named `*.sh`, so script-phase step discovery never takes it for a step.
- **The shared lines are written once.** `phase.env` sources `env/common.env` (and
  `env/chroot.env` for a chroot phase), then sets only what differs: title, snapshot paths,
  `PORT_REPO`, `MARK`, and phase-specific flags such as `LD_LIBRARY_PATH` in 30 and
  `KDOS_ACCENT` in 70.

  The `KDOS_*` keys **stay in `phase.env` itself**, because `parse_env` (`kb_phase.c:105-180`)
  reads that file's text and does not follow `source`. `KDOS_SNAPSHOT_EXCLUDE` is repeated per
  phase for the same reason.
- **The userland phases use `packages.d/`.** Each phase's list is split one file per Part A shelf,
  read in sorted file order. In `41_system`, `00-order.txt` holds the runs whose order a comment
  pins (toybox and the owners it hands names back to, and the like), before any shelf file. The
  other phases keep a single `packages.txt`; the largest is 172 lines. A phase uses `packages.txt`
  or `packages.d/`, never both, and preflight enforces that.
- **A granular phase names every port it installs.** Today a list names what it wants and the
  dependency closure pulls in the rest: phase 4 names 1,685 and installs 1,842. From 30 on, each
  list names exactly the ports that phase installs, and none that it does not. The new preflight
  check is **phase closure**: for each package phase N, closure(list N) minus everything installed
  by phases before N must equal the set list N names. This is what makes a granular phase mean
  something. A port cannot quietly move to an earlier phase because something there started
  depending on it, and a list cannot reach forward. A new dependency that breaks this fails
  preflight and names the port and both phases.
- **`psf2limine.py` moves next to the step that runs it**, `70_image/110_iso.sh`. It is not
  shared.
- **`hooks/` does not move.** Every clone has `core.hooksPath` set to `script/hooks`, and moving it
  would silently disable the pre-push source check on each one.

## C2a. The granular phases, measured

Each split below comes from the dependency graph: every `depends =` line of all 2,023 recipes, and
the closure of each phase's list. It does not come from the list comments. Every port's
dependencies sit in the same phase or an earlier one: **0 violations**, measured. The per-port
assignment is at `~/.cache/kdos-portwork/userland-phases.tsv` (`name<TAB>phase<TAB>shelf`).

**Old phase 3 (137 ports, 8.7 h measured in `timings.json`) splits in two.** The split is by
whether a port's closure reaches one of the big compilers.

| Phase | Ports | Measured time | Holds |
|---|---|---|---|
| `30_foundation` | 115 | 1.0 h | build systems, interpreters (python3, perl), base libraries |
| `31_compilers` | 22 | 7.6 h | llvm, clang, lld, compiler-rt, libunwind, openmp, their `21` pins, rust, cargo-c, bindgen, cbindgen, go, ghc, cabal-install, pandoc, zig, nodejs, ruby, asciidoctor, ccache |

A restore point after the first hour, before the next seven.

**Old phase 4 (1,842 installed) splits in five.** The split is by what a port's dependency closure
reaches:

| Phase | Ports | Rule | Largest shelves |
|---|---|---|---|
| `40_lang` | 169 | Language-module and developer-tool shelves (`python*`, `perl-cpan`, `lang`, `devtools`, `buildtools`, `doctools`, `vcs`) whose closure needs nothing below | python-libs 47, devtools 23, python-net 21, python 20, perl-cpan 12 |
| `41_system` | 965 | Everything with no graphics and no toolkit in its closure: services, network, storage, CLI, codecs, science and hardware libraries | devlibs, image-libs, formats, audio-codecs, network, embedded, hardware, crypto |
| `42_graphics` | 186 | The closure reaches wayland, libX11, mesa, libdrm, cairo, pango, gstreamer, ffmpeg or pipewire, but no toolkit, and the port is not a leaf application | x11 23, gpu 17, wl 14, game-libs 13, graphics-libs 11 |
| `43_toolkits` | 242 | The closure reaches GTK, Qt, wxWidgets, FLTK or Motif, and the port is a library (a toolkit shelf, or something depends on it) | kf6 66, qt6 29, qt-extra 17, gtk 14, qt5 11 |
| `44_apps` | 280 | A leaf application whose closure reaches a toolkit, or graphics on an application shelf | graphics 18, emulators 14, studio 12, games-board 12, hamradio 11 |

**`41_system` stays one phase of 965.** It was measured, and the graph does not support a finer
cut:

- Ordering the non-graphical shelf groups (languages, libraries, system, science) in any of the 24
  possible orders, a system/library/domain split bumps 136 to 183 ports past their shelf's phase.
  The results are nonsense: `openssh`, `gnutls`, `networkmanager` and `cups` all land in the
  "science" phase, because a system library on a hardware shelf (`libusb`, `tpm2-tss`) sits under
  them.
- A cut by dependency depth is valid (451 ports at depth 0, 514 deeper) but means nothing: `zsh`
  and `ripgrep` land beside `libpng`.
- `packages.d/` gives `41_system` its grouping instead.

**What the layering exposes.** These are real edges in today's graph, reported here rather than
fixed:

- `podman`, `distrobox`, `qemu`, `libvirt` and `sane-backends` build after Qt, in `43_toolkits`.
  The chain is gpgme → gnupg → pinentry → qt6-qtbase.
- `kdos-tools` builds in `42_graphics`: kbd → libxkbcommon → wayland.

Changing either chain, for example with a terminal-only `pinentry`, is its own change.

**Packaging stays one phase.** It is about 5.5 minutes (CLAUDE.md), and a restore point inside it
would cost an `fs` snapshot to save minutes.

**Snapshot cost.** Six new phases (31, 40-43) each take an `fs` snapshot, compressed and
cumulative. A full set measures about 84 GB today, 59 GB of it packaging, so the extra snapshots
are bounded by six times the old phase-4 snapshot. This has **not been measured**; measure it on
the first full build. A phase declares no snapshot by leaving `KDOS_SNAPSHOT_PATHS` empty
(`kbuild_snapshottable`, `kb_phase.c`), so any of them can opt out without a code change.

## C3. Sites to change

About 100 files name a phase directory or an environment file. The ones that change behaviour:

| Site | Change |
|---|---|
| `src/libs/libkbuild/kb_phase.c:190-240` `kbuild_discover` | List `<script_dir>/phases/`. Read `<phase dir>/phase.env` instead of `<script_dir>/<name>.env.sh`. |
| `kb_phase.c:30` `is_pkg_phase` | A package phase has `packages.txt` **or** `packages.d/`. Having both is an error. |
| `src/libs/libkbuild/kb_plan.c` | Wherever a list is read, read `packages.d/*.txt` in sorted order, the same way as a single file. |
| `src/build/kdosbuild/manager.c:200-215` | `repo_root` is derived by stripping the last component of `script_dir`, so `script_dir` must stay `script`, not `script/phases`. `chroot_exec` becomes `script/chroot/exec.sh`. The comment at 207 names `script/phase2.env.sh`; rewrite it. |
| `testing/preflight.sh` (new check) | **Phase closure** (C2). For each package phase in order, resolve the list's closure against the recipes, subtract what earlier phases installed, and require equality with the list. Name each port that differs and its two phases. Prove the check fails on a planted forward dependency before trusting it. |
| `src/libs/libkbuild/kbuild.h:28` | `KBUILD_MAX_PHASES` is 32, which is enough for 13. No change. |
| `src/build/kdosbuild/tui.c:2530-2536,2583-2617`, `view.c:310-496` | These are the dump and demo fixtures, with hard-coded phase names. Rename them, then regenerate the kdosbuild goldens in the build container (memory: goldens regenerate only where the Wayland dependencies exist). |
| `src/libs/libkbuild/kbuild.h:36-39,64,77-78` | Rewrite the examples in the comments. |
| `src/packages/kdos-tools/rebuild.c:381` | `"70_image"` |
| Every step script | `source script/phaseN.env.sh` becomes `source script/phases/<dir>/phase.env`, and `source script/util/port.sh` becomes `source script/lib/port.sh`. That is 28 step scripts. |
| `script/chroot/exec.sh` | Self-references and comments (137-144). It is also where a new opt-in variable must be named (CLAUDE.md); that rule is unchanged. |
| `70_image/10_binhost.sh`, `src/packages/kdos-kpkg/depends.c`, `kb_snap.c` | Their references to `chroot_exec.sh`. |
| `Makefile:88-93` | Only `script/kdosbuild.sh`, which does not move. Verify. |
| `testing/mini_build.py:45-48`, `testing/quick.sh`, `testing/preflight.sh` (9), `testing/selftest.sh` (3) | Phase names and list paths. The preflight checks that read `packages.txt` must read `packages.d/` too. |
| `src/desktop/kdos-shell/progkeys.c`, `src/packages/kdos-tools/kdos.c`, `themeaudit.c:237`, `src/packages/kdos-appbox/catalogue`, `src/packages/kdos-installer/build.sh` | Each names `packages.txt` or a phase path. Check each and rewrite it. |
| `ports/core/util-linux/build.sh:12-26` | Comments naming `03_phase3` and `04_phase4`. **Changing them changes the port's recipe hash**, so do it in the Part A step 8 change, not in the rename. |
| `CLAUDE.md` | The narrow-it table (`--phases 04_phase4,06_packaging`, `--steps 01_phase1:00_file_system.sh`), `--continue-from 04_phase4`, `ls build/logs/04_phase4`, and the two-edit flag rule's path. |

**Book:** `build-system.md` (43 lines, including the phase table at 59-68 and the `PORT_REPO`
table), `how-kdos-is-built.md` (28), `developing.md` (28), `ports-catalogue.md` (24, whose
structure is by phase), `glossary.md` (14), `writing-ports.md`, `build-troubleshooting.md` (9
each), `repository-layout.md`, `packaging.md` (8 each), and about 25 more pages with 1-5 lines
each. Rewrite `build-system.md` "Phases" (56-93) with the new table and the gap rule. Delete the
sentence explaining the `d`-before-`p` ordering, since that tie no longer exists.

## C4. Build state on disk

Four places in `build/` key on the phase **directory** name:

- `build/snapshots/<dir>/`, whose `manifest.json` also carries `"phase"` and `"phase_dir"`
  (`kb_snap.c:99-100`)
- `build/logs/<dir>/`
- `build/snapshots/timings.json`
- `build/mark/<short name>/`, set by `MARK=` in the phase-1 and cross environment files. The
  phase-1 marks make its steps skip.

After the rename, the orchestrator would see **no snapshot at all**. The next build would offer
only a fresh start, which is about 20 h (memory: fresh build cost), and phase 1 would re-run
every step.

**The fix is a one-time host script**, `script/migrate-phase-names.sh`. It is deleted after it has
been run on every build tree, so no shipped file records the old names. It:

1. renames the four kinds of state by the C2 table. A split phase maps as follows:
   - the old snapshot goes to its **last** successor: `03_phase3` to `31_compilers`, and
     `04_phase4` to `44_apps`. The phases before it (30, 40-43) have no snapshot until the next
     build passes them.
   - `timings.json` step keys are remapped **per port** from `userland-phases.tsv` and the phase-3
     split, so ETA history survives.
   - old log directories go to the last successor;
2. rewrites `phase` and `phase_dir` in each `manifest.json`;
3. rewrites the keys in `timings.json`;
4. refuses to run while a build is running, and when both an old and a new name exist.

A root-owned tree from a container run is migrated **from a container** (CLAUDE.md). Snapshots are
only renamed, never copied: 84 GB would not fit twice.

**Gate:** `make build BUILD_ARGS=--list` (or `kdosbuild --list`) shows the same snapshots under
their new names, and `--continue-from 50_desktop` restores the `44_apps` snapshot (the old
phase-4 one) without rebuilding.

## C5. Order

Part C goes **after Part A's step 1** (the libkbuild edits go in that same library rebuild) and
**before Part A's step 4**, so the phase-4 list regroup is written once, straight into the five
userland phases' `packages.d/`. Those lists are generated from `userland-phases.tsv` and the
shelf table, and the phase-closure check (C2) is their gate.

1. **Code accepts the new layout.** Make the `kb_phase.c`/`kb_plan.c` discovery changes, the
   `packages.d/` reader, `manager.c`, `rebuild.c`, the fixtures and the testing scripts.
2. **The rename.** In one change: `git mv` the eight phases into `script/phases/` under their new
   names, the environment files to `phase.env`, `util/` and `chroot_*.sh`, and renumber the steps.
   Update the `source` lines in the same change: they are the one content edit the move cannot
   avoid.
3. **Factor `env/common.env` and `env/chroot.env`.**
   - **Gate:** for each phase, `env -i bash -c 'source <old env>; export -p'` and the same for the
     new `phase.env` produce identical output, apart from the `KDOS_PHASE_TITLE` of 40 and 70.
     Also check `kbuild_discover`'s parsed title, snapshot paths and chroot flag per phase.
4. **Migrate `build/`** with the one-time script, then delete the script.
5. **Proof.**
   - `testing/preflight.sh` and `testing/selftest.sh` pass.
   - `make build BUILD_ARGS="--phases 10_bootstrap --steps 10_bootstrap:000_file_system.sh"`
     passes.
   - `make build BUILD_ARGS="--phases 50_desktop --rebuild kdos-shell"` passes.
   - A no-op pass over `30_foundation` to `44_apps` rebuilds nothing, because the recipe hashes
     did not move. Each phase installs only what its list names, which is the phase-closure
     check.


---

## Appendix A: every port, by shelf

The assignment was measured and then reviewed. Step 3 turns this table into one `git mv` per port.

**`base`** (27): bash, bc, ca-certificates, coreutils, dbus, diffutils, eudev, file, findutils, gawk, gzip, iana-etc, kbd, kmod, less, lsof, patch, procps-ng, psmisc, seatd, sed, snooze, sysklogd, tar, toybox, tzdata, util-linux

**`base-libs`** (16): acl, attr, basu, keyutils, libbsd, libcap, libcap-ng, libedit, libffi, libmd, libxdg-basedir, ncurses, newt, popt, readline, slang

**`auth`** (13): cracklib, cyrus-sasl, cyrus-sasl-xoauth2, fprintd, krb5, libfprint, libpwquality, oath-toolkit, openldap, pam, polkit, shadow, sudo

**`boot`** (14): acpica, dtc, efibootmgr, efivar, fwupd, fwupd-efi, gnu-efi, intel-ucode, limine, linux, linux-firmware, memtest86plus, sof-firmware, wireless-regdb

**`toolchain`** (36): argp-standalone, bindgen, binutils, bison, bmake, cbindgen, clang, clang21, compiler-rt, elfutils, flex, gcc, gperf, libclc, libintl, libunwind, libunwind-nongnu, lld, lld21, lldb, llvm, llvm21, m4, make, musl, musl-fts, musl-ldd, musl-locales, musl-obstack, musl-rpmatch, nasm, openmp, patchelf, spirv-llvm-translator, swig, yasm

**`buildtools`** (19): autoconf, autoconf-archive, automake, buildsystem, cargo-c, ccache, cmake, corrosion, gn, intltool, itstool, libtool, meson, ninja, pkgconf, scons, setconf, unifdef, util-macros

**`lang`** (26): R, bwidget, cabal-install, duktape, esbuild, ghc, go, guile, lua, lua54, lua54-luaexpat, lua54-luafilesystem, lua54-luasec, lua54-luasocket, luajit, nodejs, ocaml, ocaml-facile, openjdk, ruby, rust, tcl, tk, vala, yarn, zig

**`devtools`** (33): bcc, bpftool, bpftrace, capstone, delta, difftastic, dwarves, gdb, gef, gopls, hurl, hyperfine, imhex, just, kdevelop-pg-qt, libbpf, libtraceevent, libtracefs, ltrace, perf, rizin, ruff, rust-analyzer, shellcheck, shfmt, strace, tokei, tree-sitter, universal-ctags, valgrind, xh, zeal, zls

**`vcs`** (10): gh, git, git-cola, git-lfs, jujutsu, kdiff3, lazygit, libgit2, libkomparediff2, tig

**`editors`** (12): geany, helix, ipython, jupyterlab, kate, kdevelop, lite-xl, micro, nano, neovim, qt-creator, spyder

**`doctools`** (26): asciidoc, asciidoctor, cmark, discount, docbook-xml, docbook-xsl, doxygen, go-md2man, groff, gtk-doc, help2man, lowdown, lyx, man-pages, mandoc, pandoc, scdoc, sgml-common, smu, texinfo, texlive, texlive-doc, texstudio, typst, xmlto, xmltoman

**`devlibs`** (34): abseil-cpp, bdwgc, boost, coeurl, docopt.cpp, double-conversion, fast-double-parser, fmt, gflags, glog, gtest, highway, immer, jemalloc, lager, libdaemon, libtommath, liburcu, mustache, onetbb, pcre2, pystring, range-v3, re2, rinutils, rttr, simde, sparsehash, spdlog, talloc, tllist, uthash, xxhash, zug

**`formats`** (34): cereal, cjson, dotconf, expat, fx, gumbo-parser, inih, iniparser, jansson, jq, json-c, json-glib, jsoncpp, libconfig, libfyaml, libxml2, libxmlb, libxslt, miller, nlohmann-json, protobuf, protobuf-c, pugixml, rapidjson, raptor2, thrift, tinyxml, tomlplusplus, visidata, xerces-c, yajl, yaml, yaml-cpp, yq

**`database`** (15): db, duckdb, gdbm, libspatialite, lmdb, lmdbxx, postgresql, recoll, sqlite, sqlitebrowser, tdb, unixodbc, usql, valkey, xapian-core

**`archiver`** (28): 7zip, ark, brotli, bzip2, c-blosc, cabextract, lhasa, libaec, libarchive, libdeflate, libmspack, libzip, lz4, lzip, lzo, minizip, minizip-ng, ouch, par2cmdline-turbo, unshield, unzip, xz, zip, zlib, zlib-ng, zopfli, zstd, zziplib

**`shells`** (10): atuin, bash-completion, byobu, direnv, foot, konsole, starship, tmux, zoxide, zsh

**`cli`** (19): bat, bitwise, chezmoi, entr, eza, fd, fzf, glow, hexyl, lesspipe, lnav, parallel, pv, ripgrep, ripgrep-all, sd, tealdeer, tree, ttyper

**`files`** (11): dolphin, duf, dust, filelight, lf, mc, ncdu, plocate, qdirstat, rdfind, yazi

**`sysmon`** (8): bottom, btop, fastfetch, htop, iotop, nvtop, procs, resources

**`hardware`** (27): bluez, bolt, ddcutil, dmidecode, freeipmi, gusb, hidapi, hwdata, hwloc, i2c-tools, libcec, libftdi, libgpiod, libpciaccess, libserialport, libusb, lirc, lm-sensors, numactl, nut, pciutils, powerman, powertop, thermald, tlp, upower, usbutils

**`input`** (15): anthy-unicode, fcitx5, fcitx5-anthy, fcitx5-chinese-addons, fcitx5-hangul, libei, libevdev, libhangul, libime, libinput, libwacom, libxkbcommon, mtdev, wvkbd, xkeyboard-config

**`mobile`** (14): android-file-transfer, android-tools, gphoto2, ifuse, kdeconnect, libgphoto2, libgpod, libimobiledevice, libimobiledevice-glue, libmtp, libplist, libtatsu, libusbmuxd, usbmuxd

**`disk`** (29): caligula, cryptsetup, ddrescue, f3, gnome-disk-utility, gparted, gptfdisk, hdparm, impression, libaio, libatasmart, libblockdev, libbytesize, libiscsi, libnvme, libstoragemgmt, liburing, lvm2, mdadm, ndctl, nvme-cli, partclone, parted, smartmontools, testdisk, thin-provisioning-tools, udisks2, veracrypt, volume_key

**`filesystem`** (20): btrfs-progs, cifs-utils, dosfstools, e2fsprogs, erofs-utils, exfatprogs, f2fs-tools, fuse, gocryptfs, hfsprogs, jfsutils, libnfs, mtools, nfs-utils, nilfs-utils, ntfs-3g, squashfs-tools, sshfs, udftools, xfsprogs

**`optical`** (13): brasero, cdrdao, dvd+rw-tools, k3b, libburn, libcdio, libcdio-paranoia, libcue, libdiscid, libisoburn, libisofs, libkcddb, xfburn

**`crypto`** (31): argon2, botan, ccid, gnutls, gpgme, gpgmepp, libassuan, libcbor, libfido2, libgcrypt, libgpg-error, libksba, libsodium, libtasn1, libtpms, mbedtls, nettle, npth, nspr, nss, opensc, openssl, openssl3, p11-kit, pcsc-lite, qca, qca-qt5, qgpgme, swtpm, tpm2-tools, tpm2-tss

**`security`** (20): age, aircrack-ng, clamav, gnupg, john, keepassxc, kleopatra, libewf, libkleo, libsecret, lynis, nmap, pass, pass-otp, pinentry, qtkeychain, sleuthkit, step-ca, step-cli, yara

**`containers`** (18): aardvark-dns, bubblewrap, buildah, catatonit, conmon, containers-common, crun, distrobox, fuse-overlayfs, lazydocker, libseccomp, netavark, passt, podman, podman-tui, skopeo, slirp4netns, xdg-dbus-proxy

**`virt`** (25): aml, freerdp, gtk-vnc, libcacard, libnbd, libosinfo, libslirp, libvirt, libvirt-glib, libvncserver, nbdkit, neatvnc, osinfo-db, osinfo-db-tools, phodav, qemu, remmina, spice, spice-gtk, spice-protocol, usbredir, virt-manager, virtiofsd, wayvnc, wlvncc

**`net-libs`** (33): asio, avahi, c-ares, cppzmq, curl, glib-networking, libevent, libidn, libidn2, libmicrohttpd, libmnl, libndp, libnice, libnl, libpcap, libpsl, libsoup3, libsrtp, libssh, libssh2, libtirpc, libuv, libwebsockets, miniupnpc, neon, nghttp2, nghttp3, ngtcp2, nng, poco, rpcsvc-proto, uriparser, zeromq

**`network`** (28): babeld, batctl, can-utils, chrony, dhcpcd, dnsmasq, ethtool, hostapd, iproute2, iptables, iw, ldns, libmbim, libnftnl, libqmi, libqrtr-glib, mobile-broadband-provider-info, modemmanager, networkmanager, networkmanager-openvpn, nftables, openconnect, openresolv, openvpn, ppp, vpnc-script, wireguard-tools, wpa_supplicant

**`net-diag`** (15): bandwhich, doggo, iftop, iperf3, mqttui, mtr, net-snmp, netcat, sniffnet, socat, tcpdump, termshark, trippy, whois, wireshark

**`servers`** (10): caddy, maddy, minidlna, mosh, mosquitto, ngircd, openssh, prosody, radicale, samba

**`transfer`** (14): croc, deja-dup, deluge, filezilla, fzssh, libfilezilla, libtorrent-rasterbar, qbittorrent, rclone, restic, rsync, syncthing, syncthingtray, transmission

**`mail`** (13): aerc, gmime, isync, kmbox, libetpan, libpst, mimetreeparser, msmtp, newsboat, notmuch, pizauth, sfeed, thunderbird

**`chat`** (16): baresip, dino, iamb, ii, konversation, libomemo-c, libre, libstrophe, mtxclient, mumble, nheko, olm, profanity, quassel, toot, weechat

**`browsers`** (10): chromium, falkon, firefox-esr, inlyne, lagrange, librewolf, lynx, netsurf, qutebrowser, w3m

**`web-engines`** (14): libcss, libdom, libhubbub, libnsbmp, libnsgif, libnslog, libnspsl, libnsutils, libparserutils, libsvgtiny, libwapcaplet, nsgenbind, webkitgtk, webkitgtk6

**`x11`** (42): libICE, libSM, libX11, libXScrnSaver, libXau, libXcomposite, libXcursor, libXdamage, libXdmcp, libXext, libXfixes, libXfont2, libXft, libXi, libXinerama, libXmu, libXpm, libXpresent, libXrandr, libXrender, libXres, libXt, libXtst, libXv, libXxf86vm, libfontenc, libxcb, libxcvt, libxkbfile, libxshmfence, xbitmaps, xcb-proto, xcb-util, xcb-util-cursor, xcb-util-image, xcb-util-keysyms, xcb-util-renderutil, xcb-util-wm, xkbcomp, xorgproto, xtrans, xwayland

**`wl`** (17): fcft, fuzzel, grim, libdecor, libdisplay-info, slurp, wayland, wayland-protocols, wayland-utils, waylandpp, waypipe, wev, wl-clipboard, wl-kbptr, wl-mirror, wlroots, wtype

**`gpu`** (25): freeglut, glew, glfw, glmark2, glslang, glu, intel-gmmlib, intel-media-driver, libdrm, libepoxy, libglvnd, libplacebo, libva, libva-intel-driver, libva-utils, mesa, mesa-demos, ocl-icd, opencl-headers, shaderc, spirv-headers, spirv-tools, vulkan-headers, vulkan-loader, vulkan-tools

**`graphics-libs`** (25): babl, blend2d, cairo, cairomm, cairomm1.14, gdk-pixbuf, gegl, goocanvas, graphene, graphviz, harfbuzz, kseexpr, lasem, lcms2, lib2geom, libraqm, librsvg, pango, pangomm, pangomm2.46, pixman, plutosvg, plutovg, potrace, resvg

**`image-libs`** (43): aalib, chafa, exempi, exiftool, exiv2, farbfeld, gexiv2, giflib, graphicsmagick, imagemagick, imath, jbigkit, lensfun, leptonica, libavif, libdmtx, libexif, libgd, libheif, libimagequant, libiptcdata, libjpeg-turbo, libjxl, libkdcraw, libkexiv2, libmypaint, libpng, libraw, libsixel, libtiff, libvips, libwebp, libyuv, mypaint-brushes, opencolorio, openexr, openimageio, openjpeg, qrencode, zbar, zimg, zint, zxing-cpp

**`graphics`** (23): aa3d, aview, converseen, darktable, digikam, flameshot, gimp, gimp-help, glaxnimate, grafx2, gthumb, gwenview, imv, inkscape, kolourpaint, krita, optipng, oxipng, pencil2d, pngquant, rawtherapee, swayimg, timg

**`3d`** (14): admesh, alembic, assimp, blender, draco, gl2ps, goxel, lib3mf, libspnav, manifold, opencsg, opensubdiv, openvdb, vtk

**`cad`** (17): calculix, cgal, clipper2, coin, freecad, gmsh, ifcopenshell, librecad, netgen, opencascade, openscad, pivy, polyclipping, qcad, qhull, solvespace, soqt

**`fabrication`** (13): bcnc, candle2, cncjs, cura, curaengine, libarcus, libnest2d, libsavitar, linuxcnc, orcaslicer, printrun, prusaslicer, uranium

**`fonts`** (24): bdftopcf, encodings, font-adobe-75dpi, font-caladea, font-carlito, font-cursor-misc, font-liberation, font-manager, font-misc-misc, font-util, fontconfig, fontforge, fonttools, freetype2, mkfontscale, nerd-fonts-symbols, noto-cjk, noto-emoji, noto-fonts, noto-fonts-extra, terminus-font, terminus-ttf, ttf-dejavu, woff2

**`themes`** (7): adwaita-icon-theme, breeze, hicolor-icon-theme, icon-naming-utils, qt5ct, qt6ct, sound-theme-freedesktop

**`xdg`** (15): appstream, ayatana-ido, desktop-file-utils, libayatana-appindicator, libayatana-indicator, libcanberra, libdbusmenu, libnotify, libportal, shared-mime-info, xdg-desktop-portal, xdg-desktop-portal-gtk, xdg-desktop-portal-wlr, xdg-utils, zenity

**`accessibility`** (9): at-spi2-core, brltty, dasher, espeak-ng, liblouis, orca, pcaudiolib, piper, speech-dispatcher

**`i18n`** (14): fribidi, gettext, icu, iso-codes, libdatrie, libgrapheme, libthai, libunibreak, libunistring, opencc, snowball, uchardet, utf8proc, utfcpp

**`spelling`** (10): aspell, aspell-en, enchant, gspell, hunspell, hunspell-en, hyphen, libspelling, mythes, qtspell

**`gtk`** (28): atkmm2.28, blueprint-compiler, dconf, exo, glib, glib-introspection, glibmm, glibmm2.66, gobject-introspection, gsettings-desktop-schemas, gtk3, gtk4, gtkmm3, gtkmm4, gtksourceview4, gtksourceview5, gvfs, libadwaita, libgee, libgudev, libhandy, libpeas, libsigc++2, libsigc++3, libxfce4ui, libxfce4util, vte3, xfconf

**`qt6`** (29): python3-pyside6, qt6-qt3d, qt6-qt5compat, qt6-qtbase, qt6-qtcharts, qt6-qtconnectivity, qt6-qtdeclarative, qt6-qtgraphs, qt6-qtimageformats, qt6-qtlanguageserver, qt6-qtlocation, qt6-qtmqtt, qt6-qtmultimedia, qt6-qtnetworkauth, qt6-qtpositioning, qt6-qtquick3d, qt6-qtremoteobjects, qt6-qtscxml, qt6-qtsensors, qt6-qtserialport, qt6-qtshadertools, qt6-qtspeech, qt6-qtsvg, qt6-qttools, qt6-qttranslations, qt6-qtwayland, qt6-qtwebchannel, qt6-qtwebengine, qt6-qtwebsockets

**`qt5`** (11): qt5-qtbase, qt5-qtdeclarative, qt5-qtgraphicaleffects, qt5-qtmultimedia, qt5-qtquickcontrols2, qt5-qtserialport, qt5-qtsvg, qt5-qttools, qt5-qtwayland, qt5-qtwebsockets, qt5-qtx11extras

**`qt-extra`** (18): cpp-utilities, kcolorpicker, kddockwidgets, kdsingleapplication, kimageannotator, kirigami-addons, kqtquickcharts, mpvqt, phonon, phonon-backend-vlc, polkit-qt6, python3-qscintilla, qscintilla, qtforkawesome, qtutilities, quazip, qwt, qwt-qt5

**`kf6`** (67): attica, baloo, bluez-qt, breeze-icons, extra-cmake-modules, frameworkintegration, karchive, kauth, kbookmarks, kcalendarcore, kcmutils, kcodecs, kcolorscheme, kcompletion, kconfig, kconfigwidgets, kcontacts, kcoreaddons, kcrash, kdbusaddons, kdeclarative, kded, kdesu, kdnssd, kdoctools, kfilemetadata, kglobalaccel, kguiaddons, kholidays, ki18n, kiconthemes, kidletime, kimageformats, kio, kirigami, kitemmodels, kitemviews, kjobwidgets, kmime, knewstuff, knotifications, knotifyconfig, kpackage, kparts, kpeople, kplotting, kpty, kquickcharts, kservice, kstatusnotifieritem, ktexteditor, ktexttemplate, ktextwidgets, kunitconversion, kwallet, kwidgetsaddons, kwindowsystem, kxmlgui, modemmanager-qt, networkmanager-qt, prison, purpose, qqc2-desktop-style, solid, sonnet, syntax-highlighting, threadweaver

**`kde`** (8): baloo-widgets, ffmpegthumbs, kdegraphics-thumbnailers, kio-extras, kwayland, libksysguard, plasma-integration, plasma-wayland-protocols

**`toolkits`** (4): fltk, motif, stfl, wxwidgets

**`audio-io`** (15): alsa-lib, alsa-topology-conf, alsa-ucm-conf, alsa-utils, libao, libpulse, libsoundio, openal-soft, pipewire, portaudio, pulseaudio-qt, rtaudio, shairplay, wiremix, wireplumber

**`audio-codecs`** (28): a52dec, audiofile, codec2, faad2, fdk-aac, flac, gsm, id3lib, lame, ldacbt, libfreeaptx, libid3tag, liblc3, libmad, libogg, libopusenc, libsndfile, libvorbis, mpg123, opus, opus-tools, opusfile, sbc, speex, taglib, twolame, vorbis-tools, wavpack

**`audio-dsp`** (27): aubio, chromaprint, iir1, ladspa, libbs2b, libebur128, libmysofa, libsamplerate, libsbsms, libspatialaudio, lilv, lrdf, lv2, rnnoise, rubberband, serd, sord, soundtouch, sox, soxr, speexdsp, sratom, stk, suil, vamp-sdk, webrtc-audio-processing, zix

**`music-libs`** (17): adplug, fluidr3-gm-sf3, fluidsynth, libbinio, libgig, libgme, liblo, libmikmod, libmodplug, libmt32emu, libopenmpt, libresidfp, libsidplayfp, libxmp, portmidi, portsmf, rtmidi

**`studio`** (12): ardour, audacity, ft2-clone, furnace, hydrogen, kwave, lmms, milkytracker, musescore, pt2-clone, schismtracker, tenacity

**`music-players`** (10): audacious, audacious-plugins, cmus, elisa, mpd, picard, rhythmbox, rmpc, strawberry, termusic

**`video-libs`** (22): dav1d, frei0r-plugins, gavl, libass, libbluray, libde265, libdvbpsi, libdvdcss, libdvdnav, libdvdread, libebml, libmatroska, libmpeg2, libtheora, libudfread, libvpx, movit, openh264, svt-av1, vidstab, x264, x265

**`media-frameworks`** (17): ffmpeg, ffmpegthumbnailer, gst-libav, gst-plugins-bad, gst-plugins-base, gst-plugins-good, gst-plugins-ugly, gstreamer, libcamera, libmediainfo, libzen, mediainfo, mlt, opentimelineio, orc, totem-pl-parser, v4l-utils

**`video-players`** (8): celluloid, haruna, kodi, mpv, mpv-mpris, smplayer, uosc, vlc

**`video-tools`** (9): avidemux, blind, handbrake, kamoso, kdenlive, mkvtoolnix, obs-studio, shotcut, wf-recorder

**`game-libs`** (18): enet, glm, libkdegames, libkmahjongg, love, physfs, sdl12-compat, sdl2-compat, sdl2-gfx, sdl2-image, sdl2-mixer, sdl2-net, sdl2-pango, sdl2-ttf, sdl3, sdl3-image, sdl3-mixer, sdl3-ttf

**`games-action`** (9): ddnet, luanti, moon-buggy, neverball, powder-toy, srb2, stk-assets, supertux, supertuxkart

**`games-shooter`** (11): chocolate-doom, crispy-doom, deutex, dsda-doom, freedoom, ioquake3, ironwail, sauerbraten, xonotic, xonotic-data, yamagi-quake2

**`games-strategy`** (12): endless-sky, fheroes2, freeciv, openrct2, openttd, openttd-opengfx, openttd-openmsx, openttd-opensfx, pioneer, warzone2100, wesnoth, widelands

**`games-rpg`** (8): brogue-ce, cataclysm-dda, crawl-tiles, devilutionx, flare-engine, flare-game, frotz, nethack

**`games-board`** (18): aisleriot, black-hole-solver, bsd-games, chess-tui, freecell-solver, gnome-mahjongg, gnome-mines, gnome-sudoku, katomic, kblocks, kbounce, kmahjongg, kmines, kpat, kreversi, ksudoku, qqwing, stockfish

**`emulators`** (18): amiberry, dosbox-staging, dosbox-x, fuse-emulator, hatari, libretro-mgba, libspectrum, mednafen, mgba, mupen64plus, openmsx, ppsspp, scummvm, stella, vice, wine, wine-gecko, wine-mono

**`libretro`** (12): libretro-beetle-psx, libretro-core-info, libretro-database, libretro-gambatte, libretro-genesis-plus-gx, libretro-melonds, libretro-mupen64plus-next, libretro-nestopia, libretro-snes9x, retroarch, retroarch-assets, retroarch-joypad-autoconfig

**`education`** (16): gcompris, goldendict-ng, kalzium, kgeography, kiwix-desktop, kiwix-tools, kolibri, ktouch, kturtle, kwordquiz, libkeduvocdocument, libkiwix, libzim, parley, sdcv, tuxpaint

**`office`** (22): abiword, freexl, gnucash, gnumeric, goffice, homebank, ledger, libcdr, libgsf, libixion, liborcus, libreoffice, librevenge, libvisio, libwpd, libwpg, libxlsxwriter, mdds, presenterm, sc-im, tryton, wv

**`documents`** (20): calibre, chmlib, djvulibre, docx2txt, doxx, ebook-tools, epy, jbig2dec, koreader, libharu, libspectre, mupdf, okular, pdf4qt, pdfio, pdfium, podofo, poppler, poppler-data, qpdf

**`pim`** (12): calcurse, gramps, khal, khard, libical, nb, qownnotes, taskwarrior, taskwarrior-tui, timewarrior, vdirsyncer, zim

**`printing`** (26): brlaser, cups, cups-browsed, cups-filters, foomatic-db, foomatic-db-engine, ghostscript, gimagereader, glabels, gutenprint, hplip, ipp-usb, ksanecore, libcupsfilters, libksane, libpaper, libppd, ocrmypdf, ptouch-print, sane-airscan, sane-backends, skanlite, splix, system-config-printer, tessdata-eng, tesseract

**`math`** (24): analitza, cantor, ceres-solver, coolprop, glpk, gmp, gnuplot, highs, kalgebra, labplot, libqalculate, mpc, mpfr, nlopt, numbat, octave, pari, qalculate-qt, rkward, sympy, units, veusz, yices2, z3

**`sci-libs`** (21): arpack-ng, eigen, fftw, gsl, hdf5, libcerf, libmed, matio, matplotlib, nco, netcdf-c, numpy, openblas, pandas, qrupdate, scipy, spooles, suitesparse, sundials, udunits, xsimd

**`astronomy`** (14): astropy, celestia, celestia-content, cfitsio, cspice, erfa, gnuastro, gpredict, libnova, predict, sgp4, skyfield, stellarium, wcslib

**`bioscience`** (12): avogadrolibs, bcftools, dcmtk, diamond, gnuhealth, hmmer, htslib, mafft, minimap2, openbabel, samtools, seqkit

**`gis`** (20): gdal, geoclue, geos, go-pmtiles, gpsd, gpxsee, josm, libgeotiff, librttopo, libspatialindex, marble, opencpn, organicmaps, pdal, proj, qgis, qmapshack, routino, shapelib, viking

**`eda`** (29): digital, horizon-eda, icestorm, iverilog, kicad, kicad-footprints, kicad-symbols, kicad-templates, liblxi, libmodbus, librepcb, libsigrok, libsigrokdecode, logisim-evolution, lxi-tools, mbpoll, nec2c, nextpnr, ngspice, openfpgaloader, prjtrellis, qucs-s, sigrok-cli, sigrok-firmware-fx2lafw, surfer, symbiyosys, verilator, xnec2c, yosys

**`embedded`** (27): avr-libc, avrdude, binutils-arm-none-eabi, binutils-avr, binutils-riscv64-unknown-elf, dfu-util, espflash, flashrom, gcc-arm-none-eabi, gcc-avr, gcc-riscv64-unknown-elf, libjaylink, libstdcxx-arm-none-eabi, lrzsz, minicom, openocd, picocom, picolibc-arm-none-eabi, picolibc-riscv64-unknown-elf, picotool, probe-rs, sdcc, srecord, stlink, stm32flash, tio, xa

**`sdr-hw`** (18): airspy, airspyhf, bladerf, hackrf, libad9361, libfobos, libiio, libmirisdr, libperseus-sdr, librfnm, limesuite, rtl-sdr, soapyairspy, soapybladerf, soapyhackrf, soapyrtlsdr, soapysdr, uhd

**`sdr`** (24): aptdec, cm256cc, cubicsdr, dsdcc, gnuradio, gqrx, gr-funcube, gr-iqbal, gr-osmosdr, inspectrum, libdab, libinmarsatc, libosmo-dsp, libsigmf, libvolk, liquid-dsp, mbelib, multimon-ng, rtl-433, satdump, sdrangel, sdrpp, serialdv, urh

**`hamradio`** (25): ardopcf, chirp, contact, direwolf, flamp, fldigi, flmsg, flrig, flxmlrpc, freedv-gui, ggmorse, hamlib, js8call, klog, meshtastic-firmware, nomadnet, pat, qsstv, rnode-firmware, sideband, splat, tlf, voacapl, wsjtx, xastir

**`ai`** (7): libggml, llama.cpp, onnxruntime, opencv, translatelocally, whisper-model-base-en, whisper.cpp

**`python`** (36): python3, python3-calver, python3-cffi, python3-cppy, python3-cython, python3-extension-helpers, python3-flit-core, python3-hatch-jupyter-builder, python3-hatch-nodejs-version, python3-hatch-vcs, python3-hatchling, python3-maturin, python3-meson-python, python3-nanobind, python3-packaging, python3-pathspec, python3-pip, python3-pkgconfig, python3-pluggy, python3-poetry-core, python3-pybind11, python3-pycparser, python3-pyproject-metadata, python3-pyqt-builder, python3-scikit-build-core, python3-semantic-version, python3-setuptools, python3-setuptools-rust, python3-setuptools-scm, python3-sip, python3-tkinter, python3-tomlkit, python3-trove-classifiers, python3-vcs-versioning, python3-versioneer, python3-wheel

**`python-libs`** (63): python3-attrs, python3-bcrypt, python3-click, python3-click-log, python3-configobj, python3-cryptography, python3-cssselect, python3-cups, python3-dateutil, python3-decorator, python3-defusedxml, python3-distro, python3-docutils, python3-ecdsa, python3-glad2, python3-isodate, python3-jellyfish, python3-jinja2, python3-jsonschema, python3-jsonschema-specifications, python3-keyring, python3-lark, python3-libvirt, python3-lxml, python3-mako, python3-markdown, python3-markdown-it-py, python3-markupsafe, python3-mdurl, python3-openpyxl, python3-orjson, python3-pathvalidate, python3-pefile, python3-pikepdf, python3-pillow, python3-pillow-heif, python3-platformdirs, python3-ply, python3-protobuf, python3-psutil, python3-psycopg2, python3-pydantic-core, python3-pyemf3, python3-pygments, python3-pyparsing, python3-pypdfium2, python3-pytz, python3-pyxdg, python3-qrcode, python3-referencing, python3-rich, python3-rpds-py, python3-scour, python3-send2trash, python3-six, python3-sphinx, python3-tinycss2, python3-typing-extensions, python3-uharfbuzz, python3-wcwidth, python3-yaml, python3-yapps, python3-zstandard

**`python-net`** (22): python3-asgiref, python3-beautifulsoup4, python3-blinker, python3-certifi, python3-charset-normalizer, python3-flask, python3-flask-cors, python3-html5lib, python3-httplib2, python3-idna, python3-itsdangerous, python3-lxmf, python3-meshtastic, python3-pysocks, python3-requests, python3-rns, python3-soupsieve, python3-truststore, python3-urllib3, python3-waitress, python3-webencodings, python3-werkzeug

**`python-sci`** (13): python3-astropy-iers-data, python3-contourpy, python3-cycler, python3-gmpy2, python3-h5py, python3-iminuit, python3-jplephem, python3-kiwisolver, python3-mpmath, python3-owslib, python3-pyerfa, python3-sgp4, python3-shapely

**`python-gui`** (15): python3-cairo, python3-dasbus, python3-dbus, python3-gobject, python3-kivy, python3-opengl, python3-pyqt5, python3-pyqt5-sip, python3-pyqt6, python3-pyqt6-sip, python3-pyqt6-webengine, python3-qtpy, python3-urwid, python3-wxpython, python3-xlib

**`python-dev`** (13): python3-cloudpickle, python3-comm, python3-debugpy, python3-ipykernel, python3-jupyter-client, python3-jupyter-core, python3-jupyterlab-pygments, python3-lsp-ruff, python3-lsp-server, python3-nbconvert, python3-nest-asyncio, python3-pyzmq, python3-tornado

**`python-hw`** (12): python3-adafruit-nrfutil, python3-cantools, python3-esptool, python3-obd, python3-pyarcus, python3-pynest2d, python3-pysavitar, python3-pyserial, python3-pyusb, python3-pyuvula, python3-pyvisa, python3-pyvisa-py

**`perl-cpan`** (18): perl, perl-authen-sasl, perl-class-inspector, perl-crypt-urandom, perl-digest-hmac, perl-file-homedir, perl-file-sharedir, perl-file-sharedir-install, perl-file-which, perl-font-ttf, perl-io-socket-ssl, perl-io-string, perl-net-ssleay, perl-parse-yapp, perl-uri, perl-xml-parser, perl-xml-simple, perl-yaml-tiny
