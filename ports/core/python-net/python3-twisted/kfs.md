## About

Twisted is an event-driven networking engine for Python: a reactor over the system's event
interfaces, with TCP, UDP, TLS and Unix-socket transports, and protocol implementations for
HTTP, DNS, SSH, IRC, SMTP, IMAP and others. It installs the `twistd`, `twist` and `trial`
commands. [kajongg](#port-kajongg) runs its game server and client over Twisted's Perspective
Broker.

Its run-time imports are ports of their own in `depends`: [python3-attrs](#port-python3-attrs),
[python3-automat](#port-python3-automat), [python3-constantly](#port-python3-constantly),
[python3-hyperlink](#port-python3-hyperlink), [python3-incremental](#port-python3-incremental),
[python3-typing-extensions](#port-python3-typing-extensions) and
[python3-zope-interface](#port-python3-zope-interface).

## Notes

Twisted builds with [python3-hatchling](#port-python3-hatchling), takes its version from
[python3-incremental](#port-python3-incremental)'s hatchling plugin, and assembles its long
description with hatch-fancy-pypi-readme. That last plugin is not a port, and it is the one entry
of the recipe's `pypackages`. `build.sh` installs it from the bundle into `$SRC_ROOT/backends` and
puts that directory on `PYTHONPATH` for the build, so the plugin is used and nothing of it lands
in the build root or the package; see
[The three Python keys](../../../../docs/kdos/05-developer/writing-ports.md#the-three-python-keys).
The other sdists in the bundle are the plugin's own build requirements, which this build does not
use, because both installs run with `--no-build-isolation`.

The optional extras are not in `depends`: TLS (`pyopenssl`, `service_identity`), Conch's SSH
(`cryptography`, `bcrypt`), HTTP/2 (`h2`, `priority`) and serial ports (`pyserial`). Twisted
imports each only when a program uses the feature, and raises `ImportError` then.

The test packages (`twisted/test` and each subpackage's `test`) stay in the package: other
Twisted modules import them, so removing them breaks those modules.
