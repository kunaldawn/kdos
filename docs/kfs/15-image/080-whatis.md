# The manual index

`man` finds a page by walking the manual tree, but [`apropos`]{.idx} and [`whatis`]{.idx} search an index, and
without one they print nothing and exit 0, which looks exactly like "nothing matches". The build
writes no index while it installs packages: `chroot.env` sets `KPKG_SKIP_INDEX=man`, so kpkg's
manual trigger is skipped on every install. `script/phases/70_image/080_whatis.sh` writes the
index once, over the finished tree, with mandoc's [`makewhatis`]{.idx}, and checks that it was written.

## The manual roots

<!-- kfs:include script/phases/70_image/080_whatis.sh lines=41-48 pin=76c97b921572 -->

mandoc keeps one database, `mandoc.db`, in each manual root, and `apropos` opens the one in each
directory of its manual path. The two roots here are the two the [mandoc](#port-mandoc) port
compiles into `MANPATH_DEFAULT`, `/usr/share/man` and `/usr/local/share/man`. `makewhatis` is the
name the port installs in `/usr/sbin` for mandoc's indexer. Without it the step says so and exits
0.

<!-- kfs:include script/phases/70_image/080_whatis.sh lines=50-63 pin=02c388c146b4 -->

The old database of each root is removed first. The tree is incremental, and a root left with an
old database would answer `apropos` with programs the image does not carry. Only the roots that
exist are passed on, because `makewhatis` treats a missing directory as an error; none existing at
all means no package installed a manual page, and stops the phase.

## Writing and checking the index

<!-- kfs:include script/phases/70_image/080_whatis.sh lines=65-71 pin=ae3097c84281 -->

`makewhatis` given directories creates a database in each, from the pages under it. Its exit
status is not trusted: it is non-zero for a single unreadable page or dangling link in the tree,
and the database it writes is still complete for everything else. A non-zero status is printed as a
warning, and the phase goes on.

<!-- kfs:include script/phases/70_image/080_whatis.sh lines=73-86 pin=04784a93946a -->

The database is the check. A root with no pages in it gets no database, which is not an error, so
the phase stops only when no root has one.

## Why it runs here

After [040_orphans.sh](040-orphans.md), so the pages of a removed package are not indexed, and
before `090_initramfs.sh` and `110_iso.sh`. On an installed system `KPKG_SKIP_INDEX` is not set,
and kpkg keeps the index current on every install and removal.

## What it leaves

`/usr/share/man/mandoc.db`, and `/usr/local/share/man/mandoc.db` when that root holds pages, on the
image. On a running system:

```bash
apropos editor
whatis ls
```

## See also

- [Databases stamped into the image](../../kdos/05-developer/build-system.md#databases-stamped-into-the-image): the three indexes of this phase
- [Manual pages](../../kdos/05-developer/writing-ports.md#manual-pages): what a port installs
- [Shared indexes](../../kdos/05-developer/writing-ports.md#shared-indexes): kpkg's manual trigger and `KPKG_SKIP_INDEX`

<!-- book-nav -->
---

*Part XV. The image, chapter 350.* Previous: [349. Creating the homes](070-user.md) · [Contents](../README.md) · Next: [351. The font cache](085-fontcache.md)
