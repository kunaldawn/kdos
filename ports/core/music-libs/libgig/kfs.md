## About

libgig reads and writes the sample-library formats of hardware and software samplers: Gigasampler
(`.gig`), DLS, SoundFont 2 and KORG, with `libakai` for Akai sampler disks. It installs the
`gigextract`, `gigdump`, `dlsdump`, `sf2dump`, `korgdump` and related tools. LMMS loads Gigasampler
instruments through it.

## Flags

- `pkglibdir=/usr/lib` — Install `libgig` and `libakai` in `/usr/lib` rather than the automake
  default `/usr/lib/libgig`, which is not on musl's library path: every consumer would link and then
  fail to load them. It is passed to both `make` calls, so the build and the install agree.

## Patches

- `pc-libdir.patch` — sets `libdir=@libdir@` instead of `@libdir@/libgig` in `gig.pc.in` and `akai.pc.in`, so `pkg-config --libs gig` names `/usr/lib`, where the libraries are installed.

## Notes

The configure script stops unless it finds libsndfile or libaudiofile, which `gigextract` writes
its audio files with, and it takes libsndfile when it finds it; [audiofile](#port-audiofile) is a port but is not searched for once libsndfile is found.
`uuid_generate()` comes from util-linux's libuuid. `build.sh` checks that both libraries were
installed in `/usr/lib` and that `gig.pc` says `libdir=/usr/lib`.
