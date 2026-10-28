## About
[Rhythmbox]{.idx} is GNOME's music player and library organiser, a GTK 3 application that plays
through [gstreamer](#port-gstreamer). Beside the library it handles audio CDs, portable players and
phones, playlists, internet radio and podcasts, with much of its behaviour in plugins.
[gst-libav](#port-gst-libav) is its AAC decoder, so `.m4a` and `.aac` files in the library play.

## Flags
Every optional feature is named, so a dependency that goes missing stops the configure instead of
being dropped by meson's `auto`.

- `-Dbrasero=enabled` — burn a playlist to an audio CD through [brasero](#port-brasero)'s library.
- `-Ddaap=disabled` — no DAAP music sharing: it needs libdmapsharing, which is not a port. It is
  upstream's default.
- `-Dfm_radio=enabled` — the FM radio plugin, which needs only the kernel's headers.
- `-Dgrilo=disabled` — no Grilo media sources: Grilo is not a port.
- `-Dgudev=enabled` — find MTP players and Android phones through [libgudev](#port-libgudev).
- `-Dipod=enabled` — sync iPods through [libgpod](#port-libgpod).
- `-Dlibnotify=enabled` — post the now-playing notification through
  [libnotify](#port-libnotify).
- `-Dlibsecret=enabled` — keep online services' passwords in the secret store through
  [libsecret](#port-libsecret).
- `-Dlirc=enabled` — take infrared remote controls through [lirc](#port-lirc)'s client library.
- `-Dmtp=enabled` — MTP devices through [libmtp](#port-libmtp).
- `-Dplugins_python=enabled` — the Python plugins (replay gain, the Python console, lyrics, cover
  search and the online services), loaded through [python3-gobject](#port-python3-gobject).
- `-Dplugins_vala=enabled` — install the Vala bindings, `rb.vapi` and `rhythmdb.vapi`, that a
  plugin written in Vala builds against; it needs [vala](#port-vala). No plugin Rhythmbox ships
  is written in Vala.
- `-Dsample-plugins=false` — with `-Dapidoc=false`: no sample plugins and no API documentation,
  both upstream's defaults.
- `-Dhelp=true` — install the user manual, upstream's default; `build.sh` keeps only its English
  pages, under `C`.

## Notes
The online plugins (Last.fm and ListenBrainz scrobbling, Magnatune, internet radio, podcasts,
lyrics, cover search) have no build switch. Offline each finds nothing, and the scrobblers send
nothing until an account is signed in.

Upstream ships the application icon as SVG only, and the panel draws only PNG icons, so `build.sh`
renders `org.gnome.Rhythmbox3.svg` at 48, 64 and 128 pixels with `rsvg-convert` from
[librsvg](#port-librsvg).

The desktop entry is KDOS's own, written over upstream's. Its `StartupWMClass` is the
GtkApplication id `org.gnome.Rhythmbox3`, which is the Wayland app id. It claims no MIME type:
[strawberry](#port-strawberry) is the program audio files open in, in
`/etc/xdg/kdos-mimeapps.list`. Upstream's second entry, `org.gnome.Rhythmbox3.device.desktop`, is
installed unchanged: hidden from menus, it runs `rhythmbox-client --select-source` and claims only
the `x-content/audio-player` and `x-content/audio-cdda` content types, a connected player and an
audio CD.
