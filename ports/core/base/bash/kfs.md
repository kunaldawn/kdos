## About
Bash is the GNU shell, and on KDOS it is also `/bin/sh`. kpkg sources every recipe's `build.sh`
with it, every configure script runs under it, and the boot and service scripts in `/etc/init.d`
are run by it. It is the login shell of `root` and of the desktop account, `kdos`, and its
interactive setup lives in `/etc/bash.bashrc`.

`10_bootstrap` cross-compiles bash before kpkg exists ([Bash](#03-bootstrap-080-bash)); this port
rebuilds it natively in the first run of `30_foundation` and takes over the files the bootstrap
left. The release tarball is followed by the upstream patch series, `bash<ver>-NNN`, each a
separate source of the recipe, and `build.sh` applies them in order before configuring.

## Flags
- `-DSYS_BASHRC` — Compile in `/etc/bash.bashrc` as the system-wide startup file of an interactive
  shell. Bash reads it before `~/.bashrc` only when this is defined; upstream leaves the definition
  commented out in `config-top.h`. KDOS ships that file, with the history settings, the aliases and
  the prompt marks described in
  [Prompt marks](../../../../docs/kdos/04-programs/kdos-term.md#prompt-marks).
- `--with-curses` — Take the terminal-capability functions from ncurses rather than a separate
  termcap library.
- `--enable-readline` — Build the interactive line editor in. It is upstream's default, stated.
- `--with-installed-readline` — Link the system [readline](#port-readline) instead of compiling the
  copy bash carries in `lib/readline`, so the shell and every other readline program share one
  library.
- `--without-bash-malloc` — Use musl's `malloc` instead of the allocator bash ships in
  `lib/malloc`, which configure builds in on x86-64 unless told not to.

## Notes
`make install` puts `bash` in `/usr/bin`. `build.sh` moves it to `/bin` in the package and makes
`/bin/sh` a link to it. On the installed system `/bin` is a link to `usr/bin`, so both land in
`/usr/bin`, over the two files the bootstrap left there, and kpkg folds `./bin/bash` to
`./usr/bin/bash` when it compares ownership, so no other package can claim either name under the
other spelling ([Who owns a file](../../../../docs/kdos/03-architecture/packaging.md#who-owns-a-file)).

Every `#!/bin/sh` script on the image runs in bash, so a script that works here may still use
something a strict POSIX shell refuses.
