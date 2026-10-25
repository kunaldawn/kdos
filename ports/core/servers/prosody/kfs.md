## About

Prosody is an XMPP (Jabber) chat server written in Lua. On a LAN with no route beyond it, it lets
people on the same network talk to each other; [dino](#port-dino) is the graphical client that
speaks to it. The package adds a service script, `74_prosody`, which runs the server under the supervisor as the
`prosody` account; other machines reach it once the firewall's `xmpp` name is on (see
[Services](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/02-user-guide/administration.md#services)).

Prosody's `configure` accepts none of the Lua versions as new as the tree's main [lua](#port-lua),
so it is built against and runs on the parallel [lua54](#port-lua54) port, with the Lua libraries
it needs built for that version (`lua54-luasocket`, `lua54-luasec`, `lua54-luaexpat`, `lua54-luafilesystem`).

## Flags

- `--lua-version=$_lv`, `--with-lua-bin=/usr/bin`, `--with-lua-include=/usr/include/lua$_lv`,
  `--with-lua-lib=/usr/lib`, `--lua-suffix=$_lv` — Build against Lua 5.4, the `_lv` helper in the
  recipe: the interpreter `lua5.4`, its headers in `/usr/include/lua5.4`, its library `liblua5.4`.
  All of them are named, because `--lua-version` alone would find the newer Lua's headers on the
  default include path, `/usr/include`, and compile Prosody's C modules against them.
- `--sysconfdir=/etc/prosody` — Where `prosody.cfg.lua` is installed and read from; for a `/usr`
  prefix, upstream's default as well.
- `--datadir=/var/lib/prosody` — Where the server keeps accounts, rosters and other data; for a
  `/usr` prefix this is also what `configure` picks on its own.
- `--idn-library=idn` — Do internationalised domain names with GNU [libidn](#port-libidn) instead of
  upstream's default, ICU.
- `--with-random=getrandom` — Take random numbers from the kernel's `getrandom()` system call rather
  than upstream's default, reading `/dev/urandom`.
- `--no-example-certs` — Generate no example certificates at install time.
- `--add-cflags="$CFLAGS"`, `--add-ldflags="$LDFLAGS"` — Hand the tree's compiler and linker flags
  to Prosody's own makefiles, which set theirs from `configure` and would otherwise build the C
  modules without them. The `add-` forms keep Prosody's own defaults and append these.

## Notes

`prosody` refuses to start as root and never drops privileges itself (only `prosodyctl` does), so
the service script runs it through `setpriv --reuid=prosody --regid=prosody --init-groups`, with
`-F` to keep it in the foreground where the supervisor can watch it.

The service skips itself, with a `[SKIP]` line, until the `prosody` account exists and until at
least one XMPP account does, as a file under `/var/lib/prosody/<host>/accounts/`. The first is made
with `prosodyctl adduser <user>@<host>`. The shipped configuration serves `localhost` with
registration off, so a server with no account has nobody to let in, and every machine carrying the
port would otherwise hold ports 5222 and 5269 open behind the firewall for nothing.

`postinstall.sh` creates the `prosody` group and system account unless they exist and gives it
`/var/lib/prosody`. It writes under `PKG_ROOT`, reads that root's `/etc/passwd` and `/etc/group`
itself rather than asking `getent`, which would answer for the running system, and takes the
numeric ids from `PKG_ROOT`'s own `passwd`, because `chown` would resolve a name against the
running system's.
