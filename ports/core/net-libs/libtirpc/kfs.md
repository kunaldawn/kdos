## About
libtirpc is the transport-independent Sun RPC library: the client and server calls, XDR encoding,
`rpcbind` registration and the `/etc/netconfig` transport table. musl has no RPC code of its own,
so every program here that speaks ONC RPC links this library instead, among them
[nfs-utils](#port-nfs-utils), [samba](#port-samba), [libvirt](#port-libvirt) and
[lsof](#port-lsof).

## Flags
- `--enable-gssapi` — Build RPCSEC_GSS, the Kerberos-authenticated RPC security flavour that
  NFS's `sec=krb5` mounts use. It is configure's default, and configure stops if `gssapi.h` or
  `krb5-config` is missing, so [krb5](#port-krb5) is in `depends`.
- `--enable-rpcdb` — Compile libtirpc's own `getrpcent()`, `getrpcbyname()` and
  `getrpcbynumber()`, which look RPC program names up in `/etc/rpc`, and export them from the
  library. Upstream leaves them out by default, for a C library that has its own; musl's `<netdb.h>`
  declares none of them. `/etc/rpc` itself comes from [iana-etc](#port-iana-etc).
