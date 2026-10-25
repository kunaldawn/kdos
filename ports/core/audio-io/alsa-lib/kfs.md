## About

alsa-lib is the user-space half of the [Advanced Linux Sound Architecture]{.idx}: `libasound`,
through which a program opens a sound card's PCM, mixer, MIDI and timer devices, and the
configuration language, read from `/usr/share/alsa/alsa.conf`, that defines virtual devices on top
of them. Every program that plays or records sound on KDOS without a sound-server library of its
own goes through it, which is why so many ports depend on it, and PipeWire both drives the cards
through it and offers itself back to plain ALSA programs as a device.

## Notes

- **The defaults are the build.** `configure` is given only the prefix: alsa-lib builds shared
  only by default (`AC_DISABLE_STATIC`), and the configuration directory, `/usr/share/alsa`, and
  the plug-in directory, `/usr/lib/alsa-lib`, follow from `/usr`. A different prefix moves both,
  and every ALSA plug-in another port installs must land in that plug-in directory to be loaded.
- **Its two dependencies are data.** `alsa-ucm-conf` installs the use-case files under
  `/usr/share/alsa/ucm2` that `libasound` reads at run time to set up a card's mixer paths, and
  `alsa-topology-conf` the DSP topology sources under `/usr/share/alsa/topology`; naming them as
  dependencies makes installing the library pull them in.
- **Where the default device goes is not decided here.** `alsa.conf`'s `@hooks` read
  `/etc/alsa/conf.d`, and the file KDOS ships there makes `pcm.!default` PipeWire; see
  [Reaching the graph from a plain ALSA program on the host](../../../../docs/kdos/03-architecture/session.md#reaching-the-graph-from-a-plain-alsa-program-on-the-host).
