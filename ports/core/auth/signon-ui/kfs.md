## About

signon-ui is the window [signond](#port-signond) opens when a sign-in needs a person: it shows an
online-account provider's own login page in a Qt WebEngine view, lets the person sign in there,
and hands the page signond's OAuth 2 plugin is waiting for back to it. Adding a Google or
Nextcloud account in System Settings → Online Accounts, from
[kaccounts-providers](#port-kaccounts-providers), goes through it; without it the provider's page
never opens and the account cannot be added. It is started on the session bus by D-Bus
activation, as `com.nokia.singlesignonui`, the name signond calls, and it exits when idle.

## Flags

- `-after "SUBDIRS -= tests"` — An assignment qmake applies after reading each project file. It
  drops the unit tests, which nothing in the build runs.

## Notes

The source is a commit snapshot of upstream's `master`. Its newest tags are Ubuntu packaging
snapshots of 0.17 from 2015, which predate the Qt 6 and Qt WebEngine port that `master` builds
with no patch; the version is 0.17 and the commit's date.

The program links [libnotify](#port-libnotify), for the notification it raises when an account's
sign-in fails, and [libproxy](#port-libproxy), so the login page follows the desktop's
proxy settings.

Both D-Bus service files run `/bin/sh -c` with a fallback to Ubuntu's `online-accounts-service`
for a program that is not executable or a Mir session. On KDOS the program is always there, so
the fallback never runs, and the `.desktop` file it installs is `NoDisplay` and only names the
window.
