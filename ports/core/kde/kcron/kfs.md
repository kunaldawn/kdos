## About

KCron is the Task Scheduler page of System Settings: it lists, adds, edits and removes the jobs
and environment variables of the user's crontab, and, with an administrator's password, those of
the system `/etc/crontab`. It is a KCM, a plugin that System Settings or `kcmshell6 kcm_cron`
opens, with no window of its own. On KDOS it edits what [cronie](#port-cronie) runs: cron serves
the jobs people write, and snooze keeps running the timers the system ships in
`/etc/kdos/timers.d`.

## Flags

- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON` — The handbook, through
  [kdoctools](#port-kdoctools), which upstream looks for as optional; required, a missing one
  stops the configure instead of leaving the handbook out.
- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

The user's table is read with `crontab -l` and written by piping the new table to `crontab`, so
the `crontab` program from cronie is what the page needs at run time; without it the page opens
empty and every save fails. The system table is read from `/etc/crontab` directly, and is shown
only when that file exists.

Saving the system table goes through a KAuth helper, `kcron_helper`, which runs as root and is
reached through polkit as the action `local.kcron.crontab.save`, `auth_admin` with the answer kept
for the session. The port installs the helper with its D-Bus system service, its bus policy and
its polkit action; the authentication dialog comes from the session's polkit agent.

"Run now" starts the job's command in [konsole](#port-konsole), by name, so Konsole is in
`depends` for run time.

Upstream's `CMakeLists.txt` adds its `autotests` directory whatever `BUILD_TESTING` says, so the
build compiles one test program, `taskvalidatortest`, and installs nothing from it.
