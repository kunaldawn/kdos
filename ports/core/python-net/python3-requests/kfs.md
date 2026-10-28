## About
[Requests]{.idx} is an HTTP client library for Python: sessions, cookies,
redirects, authentication, and verification of TLS certificates. It is pure Python; its runtime
imports, [python3-charset-normalizer](#port-python3-charset-normalizer),
[python3-idna](#port-python3-idna), [python3-urllib3](#port-python3-urllib3) and
[python3-certifi](#port-python3-certifi), are ports of their own in `depends`. Through certifi it
trusts the system's CA bundle.

## Notes
A SOCKS proxy (`socks4://`, `socks4a://`, `socks5://` or `socks5h://`) goes through urllib3's
`contrib.socks`, which needs PySocks. requests tries that import when it is itself imported and,
when it fails, raises `InvalidSchema: Missing dependencies for SOCKS support` only once a request
is sent through such a proxy. [python3-pysocks](#port-python3-pysocks) is a port, but it is not in
requests' `depends`, so it is present only where something else has installed it.
