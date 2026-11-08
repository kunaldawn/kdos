## About

TLP is laptop power management written in shell: at boot, on every change between mains and
battery, and around suspend, it writes the settings in `/etc/tlp.conf` and `/etc/tlp.d/` into
`/sys` — CPU and platform profiles, disk and PCIe power management, USB autosuspend, radio
switching and battery charge thresholds. It is not a daemon; `55_tlp.sh` runs it once at boot
and its udev rules run it on every power-source change. `tlp-rdw` switches radios when a cable or
dock is connected, through a NetworkManager dispatcher script.

The power profiles Plasma shows are served by [tlp-pd](#port-tlp-pd), a second build of this
source that maps each profile to `tlp power-saver`, `tlp balanced` or `tlp performance`.

## Flags

- `TLP_WITH_SYSTEMD=0` — Install no systemd unit and no systemd sleep hook.
- `TLP_WITH_ELOGIND=1` — Install `49-tlp-sleep` into `TLP_ELOD`, the directory of hooks
  [elogind](#port-elogind) runs before and after a suspend. It runs `tlp suspend` and
  `tlp resume`, which save and restore the radios' state and reapply the settings for the power
  source the machine wakes on. Without it, a resume on the other power source keeps the
  profile it went to sleep with.
- `TLP_ELOD=/usr/lib/elogind/system-sleep` — elogind's hook directory, `system-sleep` under the
  `libexecdir` its recipe sets. Named although it is TLP's default, so the two recipes are seen to
  agree.
- `TLP_NO_INIT=1` — Install no SysV `/etc/init.d/tlp`; KDOS's own `55_tlp.sh` runs TLP at boot.
- `TLP_SBIN=/usr/sbin` — With `TLP_BIN`, `TLP_TLIB`, `TLP_ULIB`, `TLP_CONFDIR` and
  `TLP_CONFDEF`: TLP's install paths, given on both `make` lines because the first substitutes
  them into the scripts and the second installs to them.

## Notes

The targets `install-tlp` and `install-rdw` with their manual pages are the whole package.
`install-pd` is left to [tlp-pd](#port-tlp-pd), which needs Python's GObject bindings from a later
phase; both recipes carry `group = tlp`, so the version checker offers them as one bump.

The sleep hook runs after NetworkManager has taken its devices down and before it brings them
back: elogind emits `PrepareForSleep(true)`, waits for the delay inhibitor NetworkManager holds
(it is built `-Dsuspend_resume=elogind`), runs the `pre` hooks and suspends, then runs the
`post` hooks before `PrepareForSleep(false)` wakes NetworkManager, so the radios TLP restores are
back before NetworkManager rescans. A hook cannot stop a suspend; one that fails is a hook that
did not run.

Charge thresholds have two possible owners on a Plasma desktop, TLP and PowerDevil's battery page;
on KDOS TLP owns them, and PowerDevil's thresholds are left unset.
