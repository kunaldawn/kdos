## About
Cyrus SASL is the library a client program uses to authenticate to a server with SASL, the
negotiation IMAP, SMTP, LDAP and VNC share. The library itself, `libsasl2`, knows no mechanism: it
loads each one as a plugin from `/usr/lib/sasl2`, and the program asks it for whichever the server
offers.

KDOS builds it as a mechanism loader for the client side. What it is here for is mail:
`mbsync` from [isync](#port-isync) authenticates through it, and the XOAUTH2 mechanism a provider
with no application passwords requires comes from the separate plugin
[cyrus-sasl-xoauth2](#port-cyrus-sasl-xoauth2), which installs into the same plugin directory.
What that pair can and cannot sign in to is in
[`mbsync` has XOAUTH2 but not OAUTHBEARER](../../../../docs/kdos/06-reference/known-gaps.md#mbsync-has-xoauth2-but-not-oauthbearer-and-no-synchroniser-has-met-a-server).
[openldap](#port-openldap), [qemu](#port-qemu)'s VNC server, [libvirt](#port-libvirt) and
[spice](#port-spice) link it as well. The build has no `saslauthd` client, no `sasldb` password
store, no PAM check and no sample programs.

## Flags
- `-std=gnu17` — Compile as C17 with GNU extensions. The compiler's default is C23, which does not
  accept old-style (K&R) function definitions such as those in `lib/md5.c`, one of the files the
  library is built from.
- `-include time.h` — Include `<time.h>` in every file. `lib/saslutil.c` calls `time()`, but
  includes `<time.h>` only under `HAVE_TIME_H`, which configure never defines: it checks for
  `<sys/time.h>` only, and the generated `config.h` then includes `<sys/time.h>` in place of
  `<time.h>`. musl's `<sys/time.h>` does not declare `time()`, and the compiler refuses a call to
  an undeclared function.
- `--with-plugindir=/usr/lib/sasl2` — Where `libsasl2` looks for mechanism plugins, and where this
  build and [cyrus-sasl-xoauth2](#port-cyrus-sasl-xoauth2) install them. It is upstream's default
  for this prefix.
- `--with-openssl` — Link OpenSSL's `libcrypto`. SCRAM is built only with it: configure disables
  SCRAM, with a warning, when OpenSSL is not found.
- `--with-dblib=none` — Build no `sasldb` user database (Berkeley DB, GDBM or LMDB), and so no
  `saslpasswd2` or `sasldblistusers2`. Only a server reads such a store.
- `--without-pam` — Do not check passwords through PAM. configure would otherwise look for it;
  [pam](#port-pam) is a `41_system` port, not installed when this builds in `30_foundation`.
- `--without-saslauthd`, `--without-authdaemond` — Build no client of the `saslauthd` daemon or of
  Courier's `authdaemond`; both are server-side password checks.
- `--disable-sample` — Build none of the sample client and server programs.
- `--disable-gssapi`, `--disable-krb4` — No Kerberos mechanisms: GSSAPI (and GS2, which needs
  it) and KERBEROS_V4. [krb5](#port-krb5) is a `41_system` port.
- `--disable-otp`, `--disable-srp`, `--disable-ntlm`, `--disable-digest`, `--disable-cram`,
  `--disable-sql`, `--disable-ldapdb` — Build none of these plugins: the OTP, SRP, NTLM,
  DIGEST-MD5 and CRAM-MD5 mechanisms, and the SQL and LDAP auxiliary property stores. OTP,
  DIGEST-MD5 and CRAM-MD5 are on by upstream's default; the others are off.
- `--enable-plain`, `--enable-login` — The PLAIN and LOGIN mechanisms, which send the user name
  and password as they are and are meant for use over TLS. LOGIN is off by upstream's default.
- `--enable-scram` — The SCRAM-SHA-1 and SCRAM-SHA-256 mechanisms.

## Notes
With `--with-dblib=none`, upstream's `utils/Makefile.am` installs no manual pages at all, not even
the one for `pluginviewer`, which is built regardless. `build.sh` installs `pluginviewer.8` itself;
`pluginviewer -c` lists the client mechanisms the library can load, which is the quickest way to
see whether the XOAUTH2 plugin is in place.

The ANONYMOUS mechanism, `libanonymous.so`, is installed too: upstream builds it by default, and
`build.sh` does not pass `--disable-anon`.
