## About
[Audacious]{.idx} is a playlist-oriented music player. This port builds the player itself: the
`audacious` program, the `audtool` remote control, and the libraries `libaudcore`, `libaudqt` and
`libaudtag` that its plugins are written against. Every decoder, every audio output and the Qt
window itself are plugins, which [audacious-plugins](#port-audacious-plugins) builds; without that
port the player opens no window and plays nothing.

## Flags
- `-Dqt=true` — with `-Dqt5=false`: build `libaudqt`, the Qt support library the Qt interface
  plugins use, against Qt 6. Both are upstream's defaults, named.
- `-Dgtk=false` — with `-Dgtk2=false`: leave out `libaudgui`, the GTK support library. Upstream's
  default builds it against GTK 3. The interface built here is the Qt one only.
- `-Ddbus=true` — the `org.atheme.audacious` interface on the session bus, which `audtool` and a
  second `audacious` command use to reach the running player. `audtool` is built only with it.
  Upstream's default, named.
- `-Dlibarchive=true` — build `ArchiveReader` into `libaudcore`, which reads files inside an
  archive through Audacious's VFS layer, with [libarchive](#port-libarchive). Off by upstream's
  default.
- `-Dvalgrind=false` — no Valgrind-friendly build, which replaces the shared string pool with
  plain `g_strdup` copies and keeps plugin modules loaded at exit, so Valgrind can attribute
  memory. Upstream's default, named.
- `-Dbuildstamp=KDOS` — the text `audacious --version` and the About page of the preferences
  print in parentheses after the version; upstream's default is `unknown build`.

## Notes
`build.sh` writes its own `audacious.desktop` over upstream's. It has no `MimeType` line, so
Audacious is not offered as the program that opens an audio file; of the music players in the
tree, only strawberry's entry claims audio file types, and an audio file opens in that one
program. It adds `StartupWMClass=audacious`, the `Qt` category and search keywords, replaces
upstream's comment, and leaves out upstream's translated generic names and comments.
