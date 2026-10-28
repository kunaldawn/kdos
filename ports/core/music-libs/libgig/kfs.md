## About

libgig reads and writes the sample-library formats of hardware and software samplers: Gigasampler
(`.gig`), DLS, SoundFont 2 and KORG, with `libakai` for Akai sampler disks. It installs the
`gigextract`, `gigdump`, `dlsdump`, `sf2dump`, `korgdump`, `akaidump` and related tools. LMMS
loads Gigasampler instruments through it.

## Flags

- `pkglibdir=/usr/lib` — Install `libgig` and `libakai` in `/usr/lib` rather than the automake
  default `/usr/lib/libgig`, which is not on musl's library path: every consumer would link and then
  fail to load them. It is passed to both `make` calls, so the build and the install agree.

## Patches

- `pc-libdir.patch` — sets `libdir=@libdir@` instead of `@libdir@/libgig` in `gig.pc.in` and `akai.pc.in`, so `gig.pc` and `akai.pc` point consumers at `/usr/lib`, where `pkglibdir=/usr/lib` installs the libraries, rather than at `/usr/lib/libgig`, which this build does not create.

## Notes

The configure script stops unless it finds libsndfile or libaudiofile, which `gigextract`,
`sf2extract`, `akaiextract` and `wav2gig` read and write audio files with. It searches for
libaudiofile only when libsndfile is missing, so the [audiofile](#port-audiofile) port is not a
dependency. `uuid_generate()`, which gives a new DLS or gig file its DLSID, comes from
util-linux's libuuid; without it configure stops. `build.sh` checks that both libraries were
installed in `/usr/lib` and that `gig.pc` says `libdir=/usr/lib`.
