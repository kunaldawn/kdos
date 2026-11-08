## About

kdsoap-ws-discovery-client is a WS-Discovery client library: it multicasts SOAP `Probe` messages on
UDP port 3702 and collects the `ProbeMatch` replies of the devices that answer, which is how
Windows machines, Samba servers with WSD and network printers and cameras announce themselves.
The smb worker of kio-extras requires it to list the hosts of the local network under `smb:/`.
The SOAP client classes are generated at build time from the WS-Discovery WSDL files the release
carries in `wsdl/`, by `kdwsdl2cpp` from [kdsoap](#port-kdsoap).

## Flags

- `-D BUILD_WITH_QT6=ON` — Build against Qt 6 and `KDSoap-qt6`. The project asks ECM for a
  version older than KDE Frameworks 6, so ECM's `QtVersionOption` would otherwise pick Qt 5 and
  the Qt 5 KD SOAP package.
- `-D BUILD_QCH=OFF` — Build no API documentation in Qt Help format. Off is upstream's default
  too; the recipe names it.
- `-D CMAKE_DISABLE_FIND_PACKAGE_Doxygen=ON` — Build no HTML API reference. `docs/` builds and
  installs one under `/usr/share/doc/KDSoapWSDiscoveryClient` whenever Doxygen is found, with no
  switch of its own, so whether the package carried it would depend on whether
  [doxygen](#port-doxygen) happened to be installed when the port was built.

## Notes

The `onvif-discover` example is built whatever the switches say, because `examples/` is added
unconditionally; it is not installed. A probe goes to the multicast group 239.255.255.250 on port
3702, and each host replies with a unicast datagram from its own address, not from the group the
probe went to, so the firewall must admit those replies for discovery to list anything.
