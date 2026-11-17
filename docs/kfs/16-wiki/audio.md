# Audio

Sound on KDOS is [PipeWire]{.idx}, started once per user at the user's first login, with [WirePlumber]{.idx}
deciding where each stream goes and `pipewire-pulse` serving PulseAudio programs. Under it is
[ALSA]{.idx}, which is all there is outside a login. This page covers choosing an output and its
volume, playing from an ssh login or a root shell, the mixer levels kept across boots, and what
to check when nothing plays or the sound breaks up. A Bluetooth headset is paired on
[Bluetooth](bluetooth.md) and then behaves as any other output here.

## Installation

| Part | Ports |
|:--|:--|
| The sound server | [pipewire](#port-pipewire), with `pipewire-pulse`, its ALSA plugin and its own `libjack.so.0` |
| The session manager | [wireplumber](#port-wireplumber), which also provides `wpctl` |
| ALSA | [alsa-lib](#port-alsa-lib), [alsa-utils](#port-alsa-utils) (`alsactl`, [`alsamixer`]{.idx}, `aplay`, `speaker-test`), [alsa-ucm-conf](#port-alsa-ucm-conf) |
| Firmware for Intel's [audio]{.idx} DSP | [sof-firmware](#port-sof-firmware) |
| The desktop's volume control | [plasma-pa](#port-plasma-pa): the volume applet in the [panel]{.idx}'s tray, the **Sound** page of System Settings and the volume keys |

Every image carries them. No system service runs PipeWire. The three are user services in
`/etc/kdos/user-services/`, which turnstile starts under the user's own `ksvc` at the user's first
login of any kind, the desktop, a console on `tty2` or the serial line, and stops at the last
logout. `wireplumber` and `pipewire-pulse` are supervised to restart with `pipewire`. From
`fs/etc/kdos/user-services/20_pipewire.sh`:

<!-- kfs:include fs/etc/kdos/user-services/20_pipewire.sh lines=10-18 pin=a75f53d663ad -->

See [The user's services](../../kdos/03-architecture/session.md#the-users-services) and
[Session and environment](session-and-environment.md#the-per-user-services).

One service script concerns sound, `50_alsa`, which restores the cards' mixer levels at boot and
saves them at shutdown. It is part of the base filesystem; the installer offers it as a choice,
on by default:

<!-- kfs:include src/system/kinstall-core/conf.c lines=25-31 pin=9a2600b3e3b2 -->

There is no [JACK]{.idx} server and no PulseAudio daemon. JACK programs load PipeWire's `libjack.so.0`,
and PulseAudio programs talk to `pipewire-pulse`; see
[JACK programs on the host](../../kdos/03-architecture/session.md#jack-programs-on-the-host).

## Configuration

### /etc/alsa/conf.d/99-kdos-pipewire.conf

The one file that decides where a plain ALSA program's sound goes. It defines two devices and
points ALSA's `default` at one of them, chosen by the environment variable `KDOS_ALSA_DEFAULT`,
PipeWire when it is unset. The mixer control is left on the card. The comments at the top of the
file give the rule behind each part; these are its definitions:

<!-- kfs:include fs/etc/alsa/conf.d/99-kdos-pipewire.conf lines=27-82 pin=32ff7b3a68d3 -->

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

PipeWire runs its threads at real-time priority only when its PAM session allows it. turnstile
starts the user's services inside its own PAM session, whose `pam_limits` applies pipewire's
`/etc/security/limits.d/25-pw-rlimits.conf` to members of the `audio` group: real-time priority 95,
nice -19 and 4 MiB of locked memory. KDOS adds a floor of real-time priority 1 for every login,
which KWin uses, from `fs/etc/security/limits.d/30-kdos.conf`:

<!-- kfs:include fs/etc/security/limits.d/30-kdos.conf -->

A member of `audio` keeps 95: a group entry outranks `*`. Whether it took is visible per thread:

```bash
ps -L -o pid,cls,rtprio,comm -p "$(pgrep -x pipewire)"
```

lists PipeWire's `data-loop.0` thread as `FF` (`SCHED_FIFO`) with a real-time priority of 88; a
`TS` there is a thread that was not promoted.

## Usage

### Choosing the output and its volume

1. Click the volume applet in the system tray: it lists every output and input, with a slider
   each, and the stream of each application playing. Pick the default output there.
2. System Settings → **Sound** sets each device's profile (stereo, surround, HDMI), its balance
   and the default output and input.
3. The volume keys change the default output's volume, with an on-screen display.

From a terminal, `wpctl` does the same:

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

plays a voice naming each channel once, through ALSA's `default`, which is PipeWire for any
login. `pw-play <file>` plays a file into PipeWire directly.

### Playing from an ssh login or a root shell

A console login on `tty2` or the serial line starts the user's PipeWire like the desktop does. An
ssh login of a user with no other login has none, because OpenSSH opens no PAM session, and
neither does root or anything `init` starts. Play straight to the card instead:

```bash
KDOS_ALSA_DEFAULT=kdos_card aplay sound.wav
```

`kdos_card` is exclusive, so this fails while a user's PipeWire holds the card, with
`audio open error: Resource busy`, and the session has no sound while the program plays.

### Keeping mixer levels

1. Set the levels with `alsamixer` (`F6` chooses the card).
2. Save them at once, rather than waiting for the next shutdown:

   ```bash
   sudo alsactl store
   ```

### Restarting the sound server

```bash
service --user restart pipewire
```

`wireplumber` and `pipewire-pulse` restart with it. `service --user status` shows all three.
Programs that were playing reconnect or need restarting, according to the program.

## Tips

- `aplay -L` lists the PCM names ALSA knows, with the two from `99-kdos-pipewire.conf` among them:
  "Default Audio Device (PipeWire)" and "Sound Card (no sound server)". Run it after any edit of an
  ALSA configuration file before believing anything else.
- `aplay -l` lists the cards and their devices; `cat /proc/asound/cards` does the same without
  alsa-utils.
- `pw-top` shows every node with its quantum and its `ERR` column, the count of underruns.
  `pw-metadata -n settings` shows the quantum in force.
- `defaults.pcm.card` and `defaults.ctl.card` in `~/.asoundrc` choose the card `kdos_card` and
  `sysdefault` open, and the card `alsamixer` shows first. They do not move the session's sound,
  which goes through PipeWire.
- The microphone privacy lamp in the system tray lights only while a recording stream is running;
  a click mutes or unmutes every microphone capture.
- After an upgrade of pipewire or alsa-lib, `kdos restarts` lists the processes still running the
  old code.

## Troubleshooting

**Nothing plays in the desktop session.**

1. Is PipeWire running, and does it have a sink?

   ```bash
   service --user status pipewire
   wpctl status
   ```

   The services' logs are `$XDG_RUNTIME_DIR/ksvc/kdos-svc.pipewire.log` and the same for
   `wireplumber`.

2. If `wpctl status` lists no sinks, another program may hold the card. The owner of the first
   playback stream is in:

   ```bash
   cat /proc/asound/card0/pcm0p/sub0/status
   ```

   Under PipeWire its `owner_pid` is the PipeWire daemon. Another process there is a program that
   opened the card directly, through `kdos_card`, `hw:` or `sysdefault`; close it.

3. If the sink exists and is silent, check its volume and mute in the volume applet or with
   `wpctl status`, then the card's own levels in `alsamixer`.

**A program fails with `Host is down`.** There is no PipeWire for that login. Run it
with `KDOS_ALSA_DEFAULT=kdos_card`, as in
[Playing from an ssh login or a root shell](#playing-from-an-ssh-login-or-a-root-shell).

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
- [The system tray](../../kdos/04-programs/plasma-shell.md#the-system-tray): the tray's audio applet,
  and the privacy lamps that show what is recording
- [Session, screen and media](../../kdos/02-user-guide/desktop.md#session-screen-and-media): the volume and microphone keys
- [Services](../../kdos/02-user-guide/administration.md#services): `50_alsa` in the boot order
- [Bluetooth](bluetooth.md), [udev and device groups](udev-and-device-groups.md),
  [Users and groups](users-and-groups.md), [Microcode and firmware](microcode-and-firmware.md)

<!-- book-nav -->
---

*Part XVI. System administration, chapter 424.* Previous: [423. Printing and scanning](printing-and-scanning.md) · [Contents](../README.md) · Next: [425. Displays and scaling](displays-and-scaling.md)
