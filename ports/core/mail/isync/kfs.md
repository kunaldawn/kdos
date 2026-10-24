## About

isync is `mbsync`, which makes an IMAP mailbox and a local Maildir, `~/Mail`, equal in both
directions. It moves messages and does nothing else: notmuch indexes what it fetches, and msmtp
sends. `notmuch new` runs it from the pre-new hook in `/etc/skel/.config/notmuch/default/hooks` and
then indexes what it left, so one command fetches and files. The templates in `/etc/skel`,
`.mbsyncrc` and notmuch's `config`, name the same Maildir root, `~/Mail`.

## Flags

- `ac_cv_berkdb4=no` — Answer the Berkeley DB check in advance with "no". The configure script links
  `-ldb` whenever a test program calling `db_create` links, with no switch to refuse it, and the
  [db](#port-db) port exists, so without the answer the result would depend on whether db happened
  to be built first. With it, `mdconvert`, the converter between Maildir UID storage schemes, is not
  built, and the Maildir driver keeps only its native scheme, the UID in each message's file name.
- `--with-ssl` — Build TLS support against OpenSSL.
- `--with-zlib` — Build IMAP `COMPRESS=DEFLATE` support.
- `--with-sasl` — Authenticate through Cyrus SASL, which takes whatever mechanism a plugin under
  `/usr/lib/sasl2` provides. Cyrus SASL itself has no XOAUTH2;
  [cyrus-sasl-xoauth2](#port-cyrus-sasl-xoauth2) is the plugin that carries it, and both are in
  `depends`. The plugin sends the account's password as the bearer token, so `AuthMechs XOAUTH2`
  beside `PassCmd "pizauth show <account>"` is the whole of an OAuth2 setup with
  [pizauth](#port-pizauth). OAUTHBEARER has no plugin here.

## Patches

- `openssl4.patch` — reads a certificate's `dNSName` through `ASN1_STRING_get0_data()` and `ASN1_STRING_length()` in `verify_hostname()` instead of the structure's `data` and `length` fields, which OpenSSL 4 makes opaque; the compatibility block maps `ASN1_STRING_get0_data()` onto `ASN1_STRING_data()` for the older libraries it already covers.

## Notes

perl is a build dependency, not a convenience: configure stops with "perl not found" without it, and
`src/drv_proxy_gen.pl` generates `drv_proxy.inc` at build time, which the release does not carry.
