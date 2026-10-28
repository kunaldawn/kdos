## About

iana-etc installs three [network databases]{.idx}: `/etc/services` (service names and ports,
which musl's `getservbyname()` reads), `/etc/protocols` (IP protocol numbers) and `/etc/rpc`
(ONC RPC program numbers, which libtirpc's `getrpcbyname()` reads). musl's `getprotobyname()`
answers from a table compiled into the C library and does not open `/etc/protocols`.
The port carries no generated text: its three sources are IANA's own registry files in XML,
and `build.sh` turns them into the tables, which is why it is long.

## Notes

- **The version is a registry date.** The three `source =` lines name IANA's XML files, each
  saved under a name with the version in it, and `version` is the newest `<updated>` date across
  the three. Refreshing the tables is a new version, a fetch and three new hashes; the tables on
  the system are a function of three hashed registry snapshots and nothing else.
- **`gawk`, by name.** The `rpc` transform uses `delete` on a whole array and `length()` of an
  array, which not every `awk` has, so `build.sh` runs `gawk` rather than `awk`, and
  `gawk` is the recipe's one dependency. Each table's header carries the registry date rather
  than the build time, so two builds of one version are byte-identical.
- **What the transforms leave out.** In `services`, a record marked Unassigned, Reserved or
  historic is not a service a resolver should answer for, and a name containing a parenthesis is
  a description rather than a service name; both are skipped.
- **The `rpc` aliases.** IANA names program 100000 `pmapprog portmap rpcbind`; `rpcbind`,
  `rpcinfo` and older tools look up `portmapper` and `sunrpc`, and programs 100001, 100002, 100005
  and 100008 are looked up as `rstatd`, `rusersd`, `mountd` and `rwalld`. The transform adds those
  names where the registry lacks them, so the lookups do not fail on a name every other system
  resolves.
- **An empty table stops the build.** A registry whose layout changes would make the transforms
  print nothing but the header, and a system whose `getservbyname()` fails for everything. After
  writing the files, `build.sh` fails when any of them has no entry, when `services` has no
  `ssh 22/tcp` and when `protocols` has no `tcp 6`.
