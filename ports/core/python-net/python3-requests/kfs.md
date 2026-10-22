## About
[Requests]{.idx} is an HTTP client library for Python: sessions, cookies,
redirects, authentication, and verification of TLS certificates. It is pure Python; its runtime
imports, [python3-charset-normalizer](#port-python3-charset-normalizer),
[python3-idna](#port-python3-idna), [python3-urllib3](#port-python3-urllib3) and
[python3-certifi](#port-python3-certifi), are ports of their own in `depends`. Through certifi it
trusts the system's CA bundle.

## Notes
A `socks5://` proxy needs PySocks, which requests imports only when such a proxy is configured.
[python3-pysocks](#port-python3-pysocks) is a port, but it is not in requests' `depends`, so it
is present only where something else has installed it.
