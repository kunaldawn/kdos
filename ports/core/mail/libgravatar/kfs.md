## About

libgravatar resolves a mail address to a picture: it hashes the address, asks
`secure.gravatar.com` or `seccdn.libravatar.org` for an avatar, caches what comes back, and offers
the configuration widget KMail shows for it. [messagelib](#port-messagelib) requires it, so it is
on the path to KMail.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the translation catalogues under `po/`, which
  the package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

Every lookup is a request to a third-party server that names the hashed address. The settings
the library installs, `GravatarSupportEnabled` and `LibravatarSupportEnabled`, both default to
off, so nothing is sent until the user turns the feature on in KMail.
