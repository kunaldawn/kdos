## About

[pizauth]{.idx} obtains and refreshes [OAuth2]{.idx} access tokens for mail programs, for providers
that refuse a plain password over IMAP and SMTP. It runs as a per-user daemon; `pizauth show
<account>` prints a current token, which a mail program reads as its password. aerc and msmtp
present it over OAUTHBEARER or XOAUTH2, and mbsync over XOAUTH2 only, through cyrus-sasl and
cyrus-sasl-xoauth2 (see [isync](#port-isync)). It is written in Rust, and its HTTP client uses
rustls, so nothing links openssl for it.

## Notes

The crates come from the vendor bundle, `pizauth-vendor-<version>.tar.xz`, which `ports/fetch`
writes with `cargo vendor` and pins with its own `sha256 =` line; `build.sh` unpacks it at the top
of the tree and cargo builds `--frozen --offline` from it (see
[Vendoring](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#vendoring)).

The crate's one optional feature, `systemd`, adds sd-notify readiness reports; the recipe builds the
default features, so it is not compiled in. The manual pages, `pizauth(1)` and `pizauth.conf(5)`,
and the bash, fish and zsh completions are upstream's own files, installed as they are.
