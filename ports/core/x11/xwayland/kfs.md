## About

Xwayland is an X server that runs as a client of a Wayland compositor, drawing each X11 window as
a Wayland surface of its own. In KDOS it is the one X server on the system: there is no Xorg
server, no display manager and nothing X on the login path, and every program that has no Wayland
path (Motif, Tk and Swing programs, [VLC](#port-vlc) 3, an X11-only application inside a box)
runs on it. The compositor starts it rootless, through [wlroots](#port-wlroots), when the first
X11 client connects; wlroots is built with Xwayland support and is the only port that depends on this one.
What the carve-out allows and what it keeps out is in
[Principles](../../../../docs/kdos/01-philosophy/principles.md#no-xorg-server-and-one-carve-out),
and how the compositor runs it is in
[kdos-comp](../../../../docs/kdos/04-programs/kdos-comp.md#xwayland).

The source is the X.Org server tree cut down to its Xwayland part, so the switches below are the
X server's own. The build turns off every other server and every feature that needs something KDOS
does not carry, forces on the GPU paths an X11 client needs to get OpenGL, and points the keymap
and font paths at the directories the KDOS ports install.

## Flags

- `-Dxvfb=false` — Does not build `Xvfb`, the virtual-framebuffer X server the tree would
  otherwise install beside `Xwayland`. Xwayland is the only X server KDOS ships.
- `-Dsecure-rpc=false` — Leaves out the `SUN-DES-1` authorisation scheme, which needs Sun RPC from
  libtirpc or the C library. musl has no RPC, and the port does not depend on
  [libtirpc](#port-libtirpc); with the option on, configure would stop or link libtirpc depending
  on whether it happened to be installed.
- `-Dxwayland_ei=false` — Builds no emulated-input support. With it, Xwayland passes the XTEST
  requests of X clients (`xdotool` and the like) to the compositor through libei, directly or
  through the remote-desktop portal; without it they reach only other X clients. The default,
  `auto`, turns it on whenever [libei](#port-libei) is installed, and libei is built in the same
  phase; `false` takes the answer out of build order.
- `-Dlibdecor=false` — Draws no window decorations with libdecor when Xwayland runs rootful, as a
  single window holding a whole X desktop. The compositor runs it rootless only, where each X11
  window is decorated by the compositor; as with libei, `auto` would follow whether
  [libdecor](#port-libdecor) happened to be built first.
- `-Dsystemd_notify=false` — Builds no `sd_notify` readiness call. KDOS has no systemd (see
  [No systemd](../../../../docs/kdos/01-philosophy/principles.md#no-systemd)).
- `-Dxselinux=false` — Builds no SELinux extension. No libselinux port exists.
- `-Dglamor=true` — Renders the X server's drawing with OpenGL through EGL and GBM from
  [mesa](#port-mesa). This is upstream's default. It makes libxshmfence, libdrm and libepoxy
  required at configure time, but GBM is only looked for: Xwayland's GBM backend, which is also
  where its DRI3 buffer sharing lives, is compiled only when glamor is on *and* Mesa's `gbm` is
  found, and without `gbm` configure carries on and leaves that backend out.
- `-Ddri3=true` — Builds the DRI3 extension, through which an X client's OpenGL or Vulkan driver
  shares buffers with the server instead of copying pixels. `true` makes a missing
  [libxshmfence](#port-libxshmfence), [libdrm](#port-libdrm) or `eventfd` stop configure, where
  `auto` would quietly build without it.
- `-Dglx=true` — Builds GLX, the server side of OpenGL for X11, which is upstream's default.
  Without it the server offers no GLX extension and an X11 client that asks for OpenGL through
  GLX gets none. It is half of a pair; Mesa, built with `-D glx=dri` behind libglvnd, is the
  other, and with GLX on configure requires the `dri` and `gl` pkg-config files those two
  install.
- `-Dsha1=libcrypto` — Takes the SHA-1 the server uses to identify glyphs in the RENDER
  extension's glyph cache from OpenSSL's libcrypto ([openssl](#port-openssl) is a dependency).
  Left on `auto`, the probe tries the C library, libmd, libsha1, nettle and libgcrypt before
  libcrypto and takes the first it finds, and [libmd](#port-libmd) is a dependency of
  [libbsd](#port-libbsd), so the result would depend on what else was in the root.
- `-Dxkb_dir=/usr/share/X11/xkb` — Where the server reads the XKB keymap sources, the directory
  [xkeyboard-config](#port-xkeyboard-config) installs.
- `-Dxkb_output_dir=/var/lib/xkb` — Where the server writes the keymaps [xkbcomp](#port-xkbcomp)
  compiles for it at run time. Upstream's default is `compiled/` inside `xkb_dir`, under `/usr`;
  this keeps the server's own writes in `/var`, and the recipe's `install -dm755` creates the
  directory in the package.
- `-Dxkb_bin_dir=/usr/bin` — Where the server finds `xkbcomp`, which it runs for every keymap.
- `-Ddefault_font_path` — The core-font path compiled into the server:
  `/usr/share/fonts/misc/` and `/usr/share/fonts/75dpi/`, the two directories
  [font-misc-misc](#port-font-misc-misc), [font-cursor-misc](#port-font-cursor-misc) and
  [font-adobe-75dpi](#port-font-adobe-75dpi) install into. Upstream's default lists `misc`, `TTF`,
  `OTF`, `Type1`, `100dpi` and `75dpi` under the font-util root, most of which nothing in KDOS
  fills. A Motif or Xt program asks this server for `-misc-fixed` or `-adobe-helvetica`, and a
  font directory inside a box is invisible to a server running outside it, so these host fonts
  are the only core fonts any X client has.
- `-Ddocs=false`, `-Ddevel-docs=false` — Builds none of the DocBook protocol and developer
  manuals. They need xorg-sgml-doctools, which is not a port. The `Xwayland(1)` and `Xserver(1)`
  manual pages are built either way.

## Notes

GLX for X clients takes both sides: turn GLX off here (`-Dglx=false`), or drop the GLX build of
[mesa](#port-mesa), and every X11 program that asks for OpenGL through GLX stops at start-up,
while every Wayland program carries on working. An X11 client inside a box draws with its box's
own Mesa over this server's DRI3, so for boxed programs `-Ddri3=true` is the half that matters.

X11 programs under Xwayland have no input method. Xwayland carries no text input for its X clients,
and [fcitx5](#port-fcitx5) is built without its XIM frontend; see
[Input methods](../../../../docs/kdos/03-architecture/session.md#input-methods).
