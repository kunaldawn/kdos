## About

librsvg renders SVG images. It is written in Rust and keeps its C API, `librsvg-2`, drawing through
[cairo](#port-cairo) and laying out text with [pango](#port-pango); `rsvg-convert` turns SVG into
PNG, PDF, PS or SVG from the command line. Its gdk-pixbuf loader is what lets every gdk-pixbuf
consumer on the host open an SVG: the portal's icon validation, img2sixel, fcitx5's theme images
and every GTK program's icons. ffmpeg's SVG decoder and imv's SVG support are built on it too.

## Flags

- `-C target-feature=-crt-static` — Links the Rust code dynamically against musl. The musl target
  links statically unless told otherwise, and a crate that binds a system library then fails to
  link or carries a private copy of it (see
  [rust](../../../../docs/kdos/05-developer/writing-ports.md#rust)).
- `CARGO_NET_OFFLINE` — Forbids cargo the network; every crate comes from the vendor bundle.
- `-Dtriplet=x86_64-unknown-linux-musl` — Passes `--target x86_64-unknown-linux-musl` to cargo,
  naming the musl target rather than leaving cargo to its host default.
- `-Dintrospection=enabled` — Installs `Rsvg-2.0.gir` and its typelib, which PyGObject programs
  load; the GIR includes `GdkPixbuf-2.0`.
- `-Dvala=enabled` — Installs the Vala binding, `librsvg-2.0.vapi`, which Vala programs such as
  gnome-mines compile against; without it their build stops at `valac`. It needs `vapigen` from
  [vala](#port-vala).
- `-Ddocs=enabled` — Builds the `rsvg-convert(1)` manual page with `rst2man`, from
  [python3-docutils](#port-python3-docutils). The HTML API reference needs gi-docgen, which is not
  a port, and meson skips it without failing.
- `-Dtests=false` — Builds no test suite; upstream's default is on.
- `-Dpixbuf=enabled` — Builds the library's `GdkPixbuf` functions, through
  [gdk-pixbuf](#port-gdk-pixbuf).
- `-Dpixbuf-loader=enabled` — Installs the SVG loader into gdk-pixbuf's module directory.
  Upstream's default is off.
- `-Davif=enabled` — Decodes AVIF images embedded in an SVG through [dav1d](#port-dav1d).

## Notes

The Rust crates come from `librsvg-vendor-<version>.tar.xz`, which `ports/fetch` makes with
`cargo vendor` and the recipe pins by its `sha256` like any other source (see
[Vendoring](../../../../docs/kdos/05-developer/writing-ports.md#vendoring)).
`build.sh` unpacks it at the top of the source tree, where it puts `vendor/`, `Cargo.lock` and a
`.cargo/config.toml` that replaces crates.io with the vendored directory. Cargo finds that
configuration by walking up from the directory meson runs it in, so the bundle must stay at the top
of the tree. `CARGO_HOME` points at the work directory, so cargo keeps its own files there and not
in root's home.

Under `DESTDIR` the install only prints a note instead of writing gdk-pixbuf's `loaders.cache`;
kpkg's pixbuf trigger rebuilds the cache when the package lands (see
[Shared indexes](../../../../docs/kdos/05-developer/writing-ports.md#shared-indexes)).
