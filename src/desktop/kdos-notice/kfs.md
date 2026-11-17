## About

`kdos-notice` tells the person at a Plasma session what the system knows and they do not. It is
KDOS's own C, a client of `org.freedesktop.Notifications` linked to basu and to nothing from Qt,
started by the session's autostart (`/usr/share/kdos/xdg/autostart/kdos-notice.desktop`,
`OnlyShowIn=KDE`). Plasma draws each notice, keeps its history and honours Do Not Disturb.

| Notice | From | Stays until dismissed |
|---|---|---|
| "Firefox ESR and Konsole were updated", or "The desktop was updated" with Log Out | `kdos restarts --json` | yes |
| "3 known vulnerabilities in installed packages" | `kdos cve --json` | yes |
| "Running KDOS 0.2.14 on trial" | `kdos-bootctl status --json` | no |
| "Your last update didn't start" | `kdos-bootctl status --json` | no |
| "firefox-esr in the box www was closed to free memory" | `status` on `/run/kdos-oomd.sock` | no |
| "This computer's hardware needs firmware-nvidia", with Install | `kdos firmware sync --dry-run --json` | yes |

The same binary run as `kdos-notice --bless`, from `/etc/xdg/autostart/kdos-session-good.desktop`,
is the second mark of a confirmed boot: once `org.kde.plasmashell` has answered on the session bus
for a minute without a gap or a restart, it calls `io.kdos.Slots1.Bless()` on
[kdos-sysupdated](#port-kdos-sysupdated) once and exits.

## Notes

A notice is posted once per change of what it is about. Each kind reduces its facts to a key (the
set of processes running replaced code, the set of findings, the boot and its slot, the failed
slot) and posts only when the key differs from the one it last posted, which
`$XDG_STATE_HOME/kdos/notice` keeps across logins: the same CVE list is not announced at every
login, and a changed one replaces the old notification instead of stacking beside it. A notice
whose state ends is withdrawn. A command that did not answer changes nothing, so a missing secdb
never withdraws a CVE notice. The state file is read when kdos-notice starts, so deleting it announces everything once more
at the next login.

Nothing polls a command. An inotify watch on `/var/lib/kpkg/db` and on
`/var/lib/kdos/update.json` asks `kdos restarts` and `kdos cve` ten seconds after the last change,
so an update of hundreds of packages is one look, and asks `kdos firmware sync --dry-run` then too,
which withdraws the firmware notice once the ports it named are installed; one on `/run/kdos/bootstate.json`, the copy of
the boot state `kdos-bootctl` publishes for users, asks `kdos-bootctl`. The one thing read on a
timer is kdos-oomd's kill count, a single line over its socket every ten seconds; the count when
the session first asks is the baseline, so a kill before the login is not this session's news.
Everything is looked at twenty seconds after login, every six hours, and two seconds after a new
owner takes `org.freedesktop.Notifications`.

A notification's id is good only while the server that issued it runs. Plasma takes a
`replaces_id` it never issued as the new notification's own number, so replacing or closing an
id from an earlier login, or from a plasmashell that has since restarted, would replace or close
another program's notification. kdos-notice forgets every id when it starts and when the server
changes owner; a notice that was waiting for a person (restarts, CVEs, the applications still to install) is
then posted again to the
new server, since a restarted plasmashell lost it.

The actions are fixed argument vectors: Details opens Konsole on `kdos cve`, or System Settings at
System Versions for the trial and rollback notices; Show Programs opens Konsole on `kdos restarts`; Install opens Konsole on `sudo kdos firmware sync`;
Open Discover opens Discover at "Selected during install"; and Log Out asks Plasma's logout prompt
over D-Bus. A signal
that claims to be the notification server's is acted on only when its sender owns
`org.freedesktop.Notifications` at that moment. The activation token the server sends before
an action goes to the program it starts, so its window takes focus.

`--bless` follows the owner of `org.kde.plasmashell`, not only its answers: a shell that crashed
and was restarted between two looks is a new owner, and its minute starts again. polkit grants
`io.kdos.slots.bless` to the active local session alone and never with a prompt, so the call
carries no interactive flag. A shell that does not manage a minute within fifteen minutes is given
up on, and the slot's attempt counter then rolls a trial back. A machine without kdos-sysupdated is
not an error. The firewall's "Allow on this network?" prompt is not this program's: it is
`kdos-fwask`, beside the Firewall settings page in [kcm-kdos-firewall](#port-kcm-kdos-firewall).
