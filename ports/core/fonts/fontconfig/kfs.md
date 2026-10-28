## About
fontconfig is how programs find fonts: it indexes every face under the font directories into a
cache, and answers a request such as "monospace, bold, able to draw this character" with an ordered
list of matching faces, following the rules in `/etc/fonts/conf.d`. Almost every program that draws
text asks it, cairo, Pango, Qt and fcft among them. The font ports install their own rules beside
upstream's, and the numbers in their file names fix the order in which faces are preferred; see
[noto-fonts](#port-noto-fonts) and [nerd-fonts-symbols](#port-nerd-fonts-symbols).

## Flags
- `--with-cache-dir=/usr/lib/fontconfig/cache` — Keep the system font cache under `/usr/lib`
  instead of upstream's default, `/var/cache/fontconfig`. The image and every pack exclude
  `var/cache`, so a cache there would never reach a booted machine, and the first program to start
  would rescan the whole font tree. `kpkg` rebuilds this cache with `fc-cache -s` whenever a package
  touches `/usr/share/fonts` or `/etc/fonts`; see
  [Shared indexes](../../../../docs/kdos/05-developer/writing-ports.md#shared-indexes).
- `--with-default-fonts=/usr/share/fonts` — The font directory compiled in for use when the
  configuration cannot be read. The same as upstream's default on Linux; every font port installs
  under it.
- `--disable-docbook` — Do not rebuild the documentation with `docbook2html`. The manual pages
  shipped pre-generated in the release tarball are installed as they are.

## Notes
`make install` with `DESTDIR` set does not run `fc-cache`, so the package carries an empty
`/usr/lib/fontconfig/cache/`, and the cache is written on the target by `kpkg`'s font index. The
cache the image ships is written once more by `70_image`, after every font directory's time has been
set to `SOURCE_DATE_EPOCH`: fontconfig records each directory's time in the cache and discards an
entry whose directory time differs. See
[Databases stamped into the image](../../../../docs/kdos/05-developer/build-system.md#databases-stamped-into-the-image).

fontconfig installs no font itself, which is why its font index also watches `/etc/fonts`: a
system whose fonts were all installed before fontconfig would otherwise have no cache.
