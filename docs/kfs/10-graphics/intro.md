# The graphics stack

<!-- kfs:phase 42_graphics -->

`42_graphics` builds what draws and what plays: the Wayland libraries and protocols, the X11
client libraries and Xwayland, the GPU stack around Mesa, the 2D rendering and text-layout
libraries, PipeWire, [FFmpeg]{.idx} and [GStreamer]{.idx}, SDL, and every library or tool whose closure reaches one
of them without reaching a toolkit. This part describes the rule that fills the phase, the shelves
that carry it, why one program of KDOS's own is built here, and which of its ports have a full
page. The package pages follow, in chapters by shelf, in the shelves' own order, and within a shelf
in the order [kpkgdepends]{.idx} builds them.

## What belongs here

Every file of the phase's list opens with the rule:

<!-- kfs:include script/phases/42_graphics/packages.d/3d.txt lines=16-18 pin=1169fe2a2986 -->

"Leaf applications" is meant narrowly. A port nothing depends on goes to
[Part XII](../12-apps/intro.md) when it sits on one of the [application shelves]{.idx term="application shelf"}, such as
`graphics`, `browsers` or `video-players`; on any other shelf it stays here, as KOReader on
`documents` and `nvtop` on `sysmon` do. The application shelves are listed under
[Which phase lists a port](../../kdos/05-developer/writing-ports.md#which-phase-lists-a-port). A
port whose closure reaches GTK, Qt, wxWidgets, FLTK or Motif is never here; it is
[Part XI](../11-toolkits/intro.md)'s or Part XII's.

The [Wayland]{.idx} base is in this phase and not in Part XIII because it is useful without a compositor:
`foot`, `grim`, `slurp`, `wl-clipboard` and `waypipe` work under any Wayland compositor, and
[Xwayland]{.idx} is a client of whichever one runs it. The `50_desktop` list says so in its header.

## The list, by shelf

The list is `script/phases/42_graphics/packages.d/`, one file per shelf, read in byte order as one
list, with no order file. The chapters that carry the phase are these:

`x11`
: Xlib and the X extension libraries (`libXext`, `libXrandr`, `libXi` and the rest), `xkbcomp`,
  `xauth`, and [Xwayland](#port-xwayland), the one X server KDOS has (see
  [No Xorg server](../../kdos/01-philosophy/principles.md#no-xorg-server-and-one-carve-out)). The
  protocol headers and the XCB libraries under them are Part IX's.

`wl`
: [wayland](#port-wayland) and [wayland-protocols](#port-wayland-protocols), `libdecor`, the font
  library `fcft`, and Wayland utilities that need no particular compositor: `wl-clipboard`, `grim`,
  `slurp`, `waypipe`, `wtype`, `fuzzel`, `wev`. wlroots is on this shelf too, and is Part XIII's.

`gpu`
: `libdrm`, `libglvnd`, [Mesa](#port-mesa), the [Vulkan]{.idx} loader and tools, [VA-API]{.idx} with the Intel
  media drivers, and the GL helper libraries GLU, GLEW, GLFW and freeglut.

`graphics-libs`, `image-libs`
: cairo, HarfBuzz, Pango and librsvg with their C++ bindings, and the image libraries whose
  closure reaches the graphics stack, such as `chafa` and OpenImageIO.

`audio-io`, `media-frameworks`
: [PipeWire](#port-pipewire) and [WirePlumber](#port-wireplumber), OpenAL and PortAudio; FFmpeg,
  GStreamer and its plugin sets, and libcamera. PipeWire is here because its closure reaches
  FFmpeg and GStreamer.

`game-libs`
: SDL 3, `sdl2-compat`, which answers SDL 2 calls through SDL 3, and `sdl12-compat`, which answers
  SDL 1.2 calls through SDL 2; their image, font, sound and network libraries; and the LÖVE game
  framework.

Smaller chapters
: `input` holds [libxkbcommon](#port-libxkbcommon), whose closure reaches Wayland, with `libei`
  and the on-screen keyboard `wvkbd`; `shells` holds `foot`; `xdg` holds `xdg-desktop-portal` and
  AppStream; `lang` holds OpenJDK, Tk and R; and the subject shelves from `3d` to `ai` hold the
  libraries and tools of their fields that draw without a toolkit, such as VTK, MuPDF and
  KOReader.

`src-system`
: [kdos-tools](#port-kdos-tools), one binary installed under many names: the service supervisor
  [`ksvc`]{.idx term="supervisor (ksvc)"}, the [`kdos` command]{.idx term="kdos (command)"} and the console getty [`kdos-getty`]{.idx} among them. It is built here
  because it depends on `kbd`, for the [`loadkeys`]{.idx} the getty runs, and `kbd` depends on
  libxkbcommon.

## Which ports have full pages

The [Tier A]{.idx} ports of this phase are the display and audio spine: wayland and wayland-protocols,
libxkbcommon, Mesa, Xwayland, PipeWire and WirePlumber, and `kdos-tools`. Each has a page with a
hand-written note beside its recipe (see
[The three tiers](../00-front/reading-a-package-page.md#the-three-tiers)).

The phase's [Tier B]{.idx} pages are dense with libraries much of the tree links: `libdrm`, `libX11`,
`libglvnd`, the Vulkan loader, cairo, HarfBuzz, Pango, librsvg, FFmpeg and GStreamer among them.
Many others are Tier B for a `build.sh` longer than the threshold, a patch or a vendor bundle. The
rest of the phase is on cards.

## See also

- [The userland: `40_lang` to `44_apps`](../../kdos/05-developer/how-kdos-is-built.md#the-userland-40_lang-to-44_apps): the phase among the five userland phases
- [Audio](../../kdos/03-architecture/session.md#audio): what PipeWire and WirePlumber do in the session
- [Xwayland](../../kdos/04-programs/kdos-comp.md#xwayland): how the compositor runs the X server built here
- [Reading a package page](../00-front/reading-a-package-page.md): how the pages below are laid out
- [Part XI](../11-toolkits/intro.md): the next phase

<!-- book-nav -->
---

*Part X. The graphics stack.* Previous: [Part IX. System](../09-system/intro.md) · [Contents](../README.md) · Next: [Part XI. Toolkits](../11-toolkits/intro.md)
