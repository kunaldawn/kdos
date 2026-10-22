## About
[wimlib]{.idx} reads, writes and mounts Windows Imaging (WIM) archives, the format of the images in
a Windows installation medium, as a library and the `wimlib-imagex` command, with `wimapply`,
`wimcapture`, `wimmount`, `wimsplit` and the other commands that run it. [impression](#port-impression),
which writes a disk image to a USB drive or memory card, splits a Windows installer with it.

## Flags
- `--with-ntfs-3g` — capture an NTFS volume into a WIM image and apply an image to one directly,
  with NTFS security descriptors and named data streams, through [ntfs-3g](#port-ntfs-3g). On
  Linux it is configure's default, named.
- `--with-fuse` — `wimmount` and `wimmountrw`, which mount an image through FUSE 3, from the
  [fuse](#port-fuse) port. On Linux it too is configure's default, named. With either switch on, a
  library that is missing stops configure with an error rather than building without the feature.

## Patches
- `pc-fuse3.patch` — corrects `configure` so that `wimlib.pc` names `fuse3` in `Requires.private`, the module configure itself checks for, instead of `fuse`. The [fuse](#port-fuse) port installs `fuse3.pc` and no `fuse.pc`, so without the patch the installed `wimlib.pc` names a module that does not exist (see the pkg-config rule in [What the package lint refuses](../../../../docs/kdos/05-developer/writing-ports.md#what-the-package-lint-refuses)).

## Notes
The last line of `build.sh` runs `pkg-config --static wimlib` against the staged `wimlib.pc`, which
reads `Requires.private`. A `configure` that writes a module name the tree does not have fails the
build there, naming the module, rather than at a consumer's build.
