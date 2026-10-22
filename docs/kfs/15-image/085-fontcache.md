# The font cache

A fontconfig client that finds no valid cache for a font directory scans every face in it before
it can draw text. `script/phases/70_image/085_fontcache.sh` writes the system cache over the
finished tree into [`/usr/lib/fontconfig/cache`]{.idx}, the directory the [fontconfig](#port-fontconfig)
port is configured with, and checks that fontconfig accepts what it wrote. Without a valid cache
the first fontconfig program of every user scans every font on the system, and on the live medium,
which starts from the same root at every boot, that is every boot.

## Clamping the directory times

<!-- kfs:include script/phases/70_image/085_fontcache.sh lines=32-46 pin=17cbe9a4f430 -->

Without [`fc-cache`]{.idx} the step says so and exits 0. A tree with no [`/usr/share/fonts`]{.idx} has had no font
installed, and stops the phase.

fontconfig judges a cache by the modification time of the directory it describes. Under
`SOURCE_DATE_EPOCH`, which every chroot phase sets, `fc-cache` records the epoch as each
directory's time instead of the real one, so a directory whose real time differs makes its own
cache stale: `fc-cache` deletes that cache as an invalid file, and a client scans the directory
again. The `touch -h` gives every directory under `/usr/share/fonts` the epoch as its time, so the
recorded and real times agree. `mksquashfs` reads the same variable and clamps every time in
`system.sfs` to the epoch, so the medium keeps the directories at that time.

The old cache files are removed first, so the files counted afterwards are this run's.

## Writing the cache

<!-- kfs:include script/phases/70_image/085_fontcache.sh lines=48-54 pin=80780c33cafc -->

`-s` caches the system directories only, never a user's, and `-f` rescans every directory even
when its cache looks valid. A run that writes no cache file stops the phase.

## Checking it

<!-- kfs:include script/phases/70_image/085_fontcache.sh lines=59-63 pin=26ab7ca93a96 -->

The second run, `fc-cache -sv`, does what a client does at start: for each directory it either
skips it, reporting the existing cache as valid, or writes it again (`caching, new cache
contents`), reporting the old file as an `invalid cache file`. Either of those two lines means a
client would rescan, and stops the phase. The log ends with the number of cache files and faces.

## Why it runs here

After [040_orphans.sh](040-orphans.md), so the faces of a removed package are not cached, and
before [087_gates.sh](087-gates.md), `090_initramfs.sh` and `110_iso.sh`.

## What it leaves

The cache files in `/usr/lib/fontconfig/cache` on the image, and every directory under
`/usr/share/fonts` with the epoch as its time. On a running system, `fc-cache -sv` should report
every directory as `skipping, existing cache is valid`.

## See also

- [Databases stamped into the image](../../kdos/05-developer/build-system.md#databases-stamped-into-the-image): the font cache among the three indexes
- [Fonts](../../kdos/02-user-guide/theming.md#fonts): the fonts KDOS ships and how to choose one
- [Reproducible packages](../../kdos/03-architecture/packaging.md#reproducible-packages): `SOURCE_DATE_EPOCH`

<!-- book-nav -->
---

*Part XV. The image, chapter 351.* Previous: [350. The manual index](080-whatis.md) · [Contents](../README.md) · Next: [352. The image gates](087-gates.md)
