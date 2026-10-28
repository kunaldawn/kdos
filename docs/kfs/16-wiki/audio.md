# Audio

Sound on KDOS is [PipeWire]{.idx}, started once per user by the desktop session, with [WirePlumber]{.idx}
deciding where each stream goes and `pipewire-pulse` serving PulseAudio programs. Under it is
[ALSA]{.idx}, which is all there is outside a desktop session. This page covers choosing an output and its
volume, playing from a text console or an ssh login, the mixer levels kept across boots, and what
to check when nothing plays or the sound breaks up. A Bluetooth headset is paired on
[Bluetooth](bluetooth.md) and then behaves as any other output here.

## Installation

| Part | Ports |
|:--|:--|
| The sound server | [pipewire](#port-pipewire), with `pipewire-pulse`, its ALSA plugin and its own `libjack.so.0` |
| The session manager | [wireplumber](#port-wireplumber), which also provides `wpctl` |
| ALSA | [alsa-lib](#port-alsa-lib), [alsa-utils](#port-alsa-utils) (`alsactl`, [`alsamixer`]{.idx}, `aplay`, `speaker-test`), [alsa-ucm-conf](#port-alsa-ucm-conf) |
| Firmware for Intel's [audio]{.idx} DSP | [sof-firmware](#port-sof-firmware) |
| The windows and the [panel]{.idx} | [kdos-shell](#port-kdos-shell): [`kdos-audio`]{.idx} and the panel's volume control |

Every image carries them. No service script runs PipeWire: the desktop session starts it, once
per user, in this order, and starts nothing when that user's PipeWire is already running:

<!-- kfs:include fs/usr/local/lib/kdos/session-common.sh lines=172-181 pin=158808e2df1c -->

The three keep running after Log Out, at the `tty1` prompt it leaves, and end with the login;
the next login starts them again. See
[What `kdos-desktop-start` does](../../kdos/03-architecture/session.md#what-kdos-desktop-start-does).

One service script concerns sound, `50_alsa`, which restores the cards' mixer levels at boot and
saves them at shutdown. It is part of the base filesystem; the installer offers it as a choice,
on by default:

<!-- kfs:include src/system/kdos-installer/conf.c lines=23-29 pin=9a2600b3e3b2 -->

There is no [JACK]{.idx} server and no PulseAudio daemon. JACK programs load PipeWire's `libjack.so.0`,
and PulseAudio programs talk to `pipewire-pulse`; see
[JACK programs on the host](../../kdos/03-architecture/session.md#jack-programs-on-the-host).

## Configuration

### /etc/alsa/conf.d/99-kdos-pipewire.conf

The one file that decides where a plain ALSA program's sound goes. It defines two devices and
points ALSA's `default` at one of them, chosen by the environment variable `KDOS_ALSA_DEFAULT`,
PipeWire when it is unset. The mixer control is left on the card. The comments at the top of the
file give the rule behind each part; these are its definitions:

<!-- kfs:include fs/etc/alsa/conf.d/99-kdos-pipewire.conf lines=27-80 pin=785af6f62550 -->

`kdos_card` is the sound card with no server in between, and it is exclusive: while a program
holds it PipeWire cannot open the card, and the other way round. It is a pass-through, `type
empty`, because `default` is already a `plug` and `sysdefault` starts with one: a third `plug`
between them makes alsa-lib abort any program that opens `default`. Why the file lives in
[`/etc/alsa/conf.d`]{.idx} and why every redefinition carries its `!` are in
[Reaching the graph from a plain ALSA program on the host](../../kdos/03-architecture/session.md#reaching-the-graph-from-a-plain-alsa-program-on-the-host).

### /etc/pipewire/pipewire.conf.d/99-kdos-vm.conf

Under a hypervisor, PipeWire's quantum, the number of frames it processes per cycle, is raised
to keep more audio in the card. On real hardware the rule does not match and PipeWire keeps its
own defaults:

<!-- kfs:include fs/etc/pipewire/pipewire.conf.d/99-kdos-vm.conf lines=38-48 pin=3d3c189a4c9e -->

The measurements behind the numbers are in
[The quantum, and why a virtual machine gets a bigger one](../../kdos/03-architecture/session.md#the-quantum-and-why-a-virtual-machine-gets-a-bigger-one).

PipeWire's and WirePlumber's own configuration is upstream's, under `/usr/share/pipewire` and
`/usr/share/wireplumber`. Add an override as a file in a `.conf.d` directory, [`/etc/pipewire/`]{.idx term="/etc/pipewire"} for
the machine or `~/.config/pipewire/` and `~/.config/wireplumber/` for one user, rather than editing
the files under `/usr/share`, which the next upgrade replaces.

### The mixer levels: 50_alsa and /var/lib/alsa

<!-- kfs:include fs/etc/init.d/50_alsa.sh -->

The saved state is `/var/lib/alsa/asound.state`, in the directory alsa-utils creates:

<!-- kfs:include ports/core/audio-io/alsa-utils/build.sh lines=28-28 pin=e5b05593ea5f -->

### Real-time priority

PipeWire runs its threads at real-time priority only when the session is allowed to. `kdos-getty`
raises three limits before the login starts, and every process of the session inherits them:

<!-- kfs:include src/system/kdos-tools/getty.c lines=244-254 pin=e28f6d5c51ac -->

See [The console](../../kdos/03-architecture/boot-and-init.md#the-console). Whether it took is
visible per thread: in a desktop session,

```bash
ps -L -o pid,cls,rtprio,comm -p "$(pgrep -x pipewire)"
```

lists PipeWire's `data-loop.0` thread as `FF` (`SCHED_FIFO`) with a real-time priority of 88; a
`TS` there is a thread that was not promoted.

## Usage

### Choosing the output and its volume

1. Open `kdos-audio` with `Super+F3`, or *Sound…* on the Hardware page of Settings. Its Outputs pane
   lists every ALSA card that can play and, while PipeWire is running, every PipeWire sink.
2. Select a PipeWire sink row and press `Enter` to make it the default sink, through
   `wpctl set-default`.
3. On a card row, `Left` and `Right` change the card's mixer level by 5%, and `m` mutes or unmutes
   it.

The panel's volume control and the volume keys change the card's level through the ALSA mixer.

From a terminal in the desktop session, `wpctl` does the same:

```bash
wpctl status
wpctl set-default <id>
wpctl set-volume @DEFAULT_AUDIO_SINK@ 5%+
wpctl set-mute @DEFAULT_AUDIO_SINK@ toggle
```

The `<id>` is the number `wpctl status` prints beside each sink.

### Testing the output

```bash
speaker-test -c 2 -t wav -l 1
```

plays a voice naming each channel once, through ALSA's `default`, which is PipeWire in a desktop
session. `pw-play <file>` plays a file into PipeWire directly.

### Playing from a text console or an ssh login

A login that runs no desktop session, on `tty2`, a serial console or over ssh, has no PipeWire, and
nothing starts one for it. Play straight to the card instead:

```bash
KDOS_ALSA_DEFAULT=kdos_card aplay sound.wav
```

`kdos_card` is exclusive, so this fails while a desktop session's PipeWire holds the card, with
`audio open error: Resource busy`, and the session has no sound while the program plays.

### Keeping mixer levels

1. Set the levels with `alsamixer` (`F6` chooses the card).
2. Save them at once, rather than waiting for the next shutdown:

   ```bash
   sudo alsactl store
   ```

### Restarting the sound server

From a terminal inside the desktop session, end the three programs and start them again in the
order the session uses:

```bash
pkill -x pipewire-pulse; pkill -x wireplumber; pkill -x pipewire
setsid -f pipewire >/dev/null 2>&1; sleep 0.3
setsid -f wireplumber >/dev/null 2>&1
setsid -f pipewire-pulse >/dev/null 2>&1
```

Programs that were playing reconnect or need restarting, according to the program.

## Tips

- `aplay -L` lists the PCM names ALSA knows, with the two from `99-kdos-pipewire.conf` among them:
  "Default Audio Device (PipeWire)" and "Sound Card (no sound server)". Run it after any edit of an
  ALSA configuration file before believing anything else.
- `aplay -l` lists the cards and their devices; `cat /proc/asound/cards` does the same without
  alsa-utils.
- `pw-top` shows every node with its quantum and its `ERR` column, the count of underruns.
  `pw-metadata -n settings` shows the quantum in force.
- `Enter` on a card row in `kdos-audio` writes `defaults.pcm.card` and `defaults.ctl.card` into
  `~/.asoundrc`. That chooses the card `kdos_card` and `sysdefault` open, and the card the panel's
  volume control and the volume keys adjust. It does not move the session's sound, which goes
  through PipeWire; a sink row does that.
- The panel's microphone lamp lights only while a recording stream is running; clicking it mutes
  or unmutes the microphone. See
  [Who is using the camera and microphone](../../kdos/02-user-guide/desktop.md#who-is-using-the-camera-and-microphone).
- After an upgrade of pipewire or alsa-lib, `kdos restarts` lists the processes still running the
  old code.

## Troubleshooting

**Nothing plays in the desktop session.**

1. Is PipeWire running, and does it have a sink?

   ```bash
   pgrep -x pipewire
   wpctl status
   ```

2. If `wpctl status` lists no sinks, another program may hold the card. The owner of the first
   playback stream is in:

   ```bash
   cat /proc/asound/card0/pcm0p/sub0/status
   ```

   Under PipeWire its `owner_pid` is the PipeWire daemon. Another process there is a program that
   opened the card directly, through `kdos_card`, `hw:` or `sysdefault`; close it.

3. If the sink exists and is silent, check its volume and mute in `kdos-audio` or with
   `wpctl status`, then the card's own levels in `alsamixer`.

**A program on the console fails with `Host is down`.** There is no PipeWire on that login. Run it
with `KDOS_ALSA_DEFAULT=kdos_card`, as in
[Playing from a text console or an ssh login](#playing-from-a-text-console-or-an-ssh-login).

**A program fails with `Unknown PCM`.** `KDOS_ALSA_DEFAULT` names a device that is not defined:
alsa-lib prints `Unknown PCM <name>`, and `aplay` then `audio open error: No such file or
directory`. `aplay -L` lists the names that are.

**Every ALSA program fails after an edit, with `default is not a compound`.** A redefinition of a
name ALSA already has lacks its `!`; write `pcm.!default`, not `pcm.default`. Check with
`aplay -L`.

**A console program plays and nothing is heard.** The card may be muted. `50_alsa` applies the
card's defaults when there is no saved state, and its log, `/run/kdos-init.alsa.log`, then reads
`No saved mixer state — applying card defaults...`. Unmute in `alsamixer`, and `sudo alsactl store`
writes `/var/lib/alsa/asound.state`, which the next boot restores. `service status alsa` only
reports `[ OK ] alsa: alsactl available`: there is no daemon behind it.

**The sound stutters in a virtual machine.** An emulated card has no clock of its own. Check that
the larger quantum is in force with `pw-metadata -n settings`, and watch `pw-top`'s `ERR` column
while it plays. Nothing appears in `dmesg`: the underrun is the player's, not the card's.

**An Intel laptop has no sound card.** A machine whose audio runs through Intel's DSP needs the
SOF firmware and topology files. `kdos doctor` checks both in its Hardware section, and
`dmesg | grep -i sof` shows what the driver asked for.

**A boxed application has no sound, or takes the card from everything else.** See
[Reaching the graph from inside a box](../../kdos/03-architecture/session.md#reaching-the-graph-from-inside-a-box).

## Known gaps

- [`kdos doctor` cannot check hardware a machine does not have](../../kdos/06-reference/known-gaps.md#kdos-doctor-cannot-check-hardware-a-machine-does-not-have):
  with no SOF device the SOF check reports only that the firmware is installed,
  `SOF firmware installed (no SOF audio device here)`, and says nothing about the topology files
- [Features whose library has no port](../../kdos/06-reference/known-gaps.md#features-whose-library-has-no-port):
  FireWire audio interfaces, and PipeWire's ROC streaming, LC3plus and LDAC decoding
- [Broad hardware support is not a goal](../../kdos/06-reference/known-gaps.md#broad-hardware-support-is-not-a-goal)

## See also

- [Audio](../../kdos/03-architecture/session.md#audio): how the session starts PipeWire, and why it is built as it is
- [kdos-audio](../../kdos/04-programs/kdos-shell.md#kdos-audio): the audio window
- [Session, screen and media](../../kdos/02-user-guide/desktop.md#session-screen-and-media): the volume and microphone keys
- [Services](../../kdos/02-user-guide/administration.md#services): `50_alsa` in the boot order
- [Bluetooth](bluetooth.md), [udev and device groups](udev-and-device-groups.md),
  [Users and groups](users-and-groups.md), [Microcode and firmware](microcode-and-firmware.md)

<!-- book-nav -->
---

*Part XVI. System administration, chapter 398.* Previous: [397. Printing and scanning](printing-and-scanning.md) · [Contents](../README.md) · Next: [399. Displays and scaling](displays-and-scaling.md)
