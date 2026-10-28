## About
[keyring]{.idx} gives Python programs one API, and a `keyring` command, for storing and
reading secrets in the system's password store. On Linux its backend is the freedesktop Secret
Service, reached through SecretStorage over jeepney's pure-Python D-Bus client: it stores into
whichever program owns `org.freedesktop.secrets` on the session bus. On KDOS that is
[keepassxc](#port-keepassxc), when it is running with its Secret Service integration turned on;
see [A saved login needs KeePassXC running](../../../../docs/kdos/06-reference/known-gaps.md#a-saved-login-needs-keepassxc-running).
[cura](#port-cura) and [spyder](#port-spyder) depend on it.

## Notes
keyring's runtime dependencies that are not ports, the three `jaraco` packages,
`more-itertools`, `jeepney` and `secretstorage`, are the bundle's `pypackages` and are
installed into this package by name with `--no-deps`; [python3-cryptography](#port-python3-cryptography),
which SecretStorage needs, is a port in `depends`.

keyring, `jaraco.context` and `jaraco.functools` build with setuptools and the
`coherent.licensed` plugin, which is not a port. `build.sh` installs the plugin from the bundle into `$SRC_ROOT/backends` and
puts that directory on `PYTHONPATH` for the build alone, so nothing of it lands in the build
root or the package.

The bundle is unpacked into `$SRC_ROOT/vendor`, outside the source tree. keyring keeps its
package at the top of the tree and lists no packages, so setuptools would take a `vendor`
directory there for a second top-level package and stop; see
[Where the bundle goes](../../../../docs/kdos/05-developer/writing-ports.md#where-the-bundle-goes). `test -x "$PKG/usr/bin/keyring"` checks that
the command was installed.
