## About

OpenSSH is the SSH client and server: `ssh`, `scp`, `sftp`, `ssh-keygen`, `ssh-agent`,
`ssh-add`, `ssh-copy-id` and the daemon `sshd`. `sshd` ships turned off; how it is turned on, and
the firewall rule that goes with it, are in [SSH server](../../../../docs/kfs/16-wiki/ssh-server.md).

## Flags

- `--sysconfdir=/etc/ssh` — Keep the client's and the server's configuration and the host keys
  under `/etc/ssh`, where `70_sshd.sh` makes the missing host keys.
- `--libexecdir=/usr/lib/$name` — Install `sftp-server` and the other helpers under
  `/usr/lib/openssh`, the path `make install` writes into `sshd_config`'s `sftp` subsystem.
- `--with-privsep-path=/var/lib/sshd` — The empty directory the pre-login process is confined to;
  `build.sh` creates it mode 0700.
- `--with-security-key-builtin` — Take keys held on a FIDO security key through
  [libfido2](#port-libfido2), with no separate middleware library.

## Notes

`build.sh` writes `/etc/profile.d/50-ssh-agent.sh`, which only exports `SSH_AUTH_SOCK` to
`$XDG_RUNTIME_DIR/ssh-agent.socket` once that socket exists, and starts nothing. The agent itself
is the `ssh-agent` user service, `/etc/kdos/user-services/15_ssh-agent.sh` from the base file
system, which runs it from the account's first login to its last logout under the user-service
supervisor ([the user's services](../../../../docs/kdos/03-architecture/session.md#the-users-services)). An agent forked
from a login shell belongs to no supervisor: it outlives the runtime directory turnstile removes
at the last logout, with its socket gone, and the next login starts another beside it. A login
that gets no user services, such as one with no runtime directory, gets no agent.
