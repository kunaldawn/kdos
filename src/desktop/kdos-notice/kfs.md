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
| "Software chosen during install is not installed yet", with Open Discover when a boxed selection waits | `/var/lib/kdos/apps-pending` and `/var/lib/kdos/groups-pending`, with `/ports/groups` for the groups' titles | yes |

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
changes owner; a notice that was waiting for a person (restarts, CVEs, the software still to install) is
then posted again to the
new server, since a restarted plasmashell lost it.

The software notice covers both of the installer's records. `apps-pending`'s boxed selections
are counted and sent to Discover; `groups-pending`'s native groups are named by the titles their
`group <id> <title>` lines give them in `/ports/groups`, the names the installer offered them by,
and by id when the tree has no title for one, one name a line, since a title may hold a comma or an
"and" that a separator inside a line would read as another group; a list too long for its buffer
falls back to the count alone. Several groups take a one-line header, "3 KDOS application groups:" beside
boxed selections and "3 KDOS application groups wait." alone, since the summary already says the
software was chosen during the install and is not installed yet. One group's sentence and the
count alone say the groups chosen during the install "are not installed yet", true both when a medium with no binary host only recorded them and when the binary
host did not answer, with `sudo kdos update install --pending`, "once a binary host answers", as
the way to install them, or `su -c 'kdos update install --pending'` for an account outside `wheel`,
which sudo refuses: the installer leaves the account out of `wheel` when Administrator is unticked.
Membership is read from the group database with `getgrouplist` at every decision, as sudo reads it,
and `KDOS_NOTICE_ADMIN_GROUP` names another group in place of `wheel` for the tests. With no boxed selection waiting there is no Open Discover, since Discover does not
list a group, and the icon is `system-software-install` rather than Discover's. The body is plain
text: Plasma's notification body is a small subset of HTML, so Markdown backticks would be drawn as
backticks, and a title's `&`, `<` and `>` go as entities. Qt wraps a line after a hyphen, so the
command stands on a line of its own, plain ASCII, after a newline that Plasma's notification server
turns into a line break, and the line before it, "Once a binary host answers, run:", is short enough
not to wrap; with boxed selections waiting too, the groups' text and the command come first and
"1 boxed app also waits in Discover." follows as the last line. Plasma's popup shows eight lines
before it scrolls, each 38 to 41 characters wide at 1920x1080 as its proportional font's glyphs
fall; `KN_POPUP_LINES` and `KN_POPUP_COLS` take 8 and the narrow end, 38, and that body is laid
out to them, each line counted by its wrapped height: when the titles would pass the eighth line,
as many as fit are listed and then "and N more", or the count alone when not even one fits, so the
command and the boxed-app line stay above the scroll. The popup shows the top of a body and scrolls the
rest, so several groups alone go as a one-line count, "7 KDOS application groups wait.", then the
command, then "They are:" and every title with no line budget: the command stands on the popup's
third line and a scroll hides only titles, and Discover does not list a group and that notice has
no Details action, so it is the one place on the desktop that names them. An id named for want of a title goes in typographic quotes with
each hyphen a non-breaking one (U+2011), which is display text and not the command.
The notice is keyed on both files' ids, not on the titles, so a tree whose title changed posts
nothing new, and it is withdrawn when neither names anything; both are read again once the package database has been quiet for ten
seconds, so the notice changes or goes within seconds of a `--pending` run. `groups-pending` is
read the way `--pending` reads it, each line cut at its first space, tab or `#`, so the notice names
exactly the groups that command would install. `KDOS_APPS_PENDING`, `KDOS_GROUPS_PENDING` and
`KDOS_NOTICE_GROUPS` move the three files for the tests.

The actions are fixed argument vectors: Details opens Konsole on `kdos cve`, or System Settings at
System Versions for the trial and rollback notices; Show Programs opens Konsole on `kdos restarts`; Install opens Konsole on `sudo kdos firmware sync`, or on `su -c 'kdos firmware sync'`, which asks for root's password there, for an account outside `wheel`, and the firmware notice's body names the same command;
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
