"""An API a client was built against, against the one served.

    apidiff.py CONTRACT.xml LIVE.xml...

CONTRACT.xml is the file a client's proxies are generated from: Discover's
own copy of org.freedesktop.sysupdate1.xml (vendored in
src/daemons/kdos-sysupdated/tests/), or the System Versions page's
io.kdos.Slots1.xml. Each LIVE.xml is an Introspect reply from
kdos-sysupdated. Every method, signal and property of every interface the
contract declares, the bus daemon's own org.freedesktop.DBus.* aside, must be
served with the same argument types in the same directions, and each
property with its type and access. Argument names are not compared: they are
not on the wire, and basu's vtables carry none.

Prints one line per difference and exits 1 when there is any.
"""

import sys
import xml.etree.ElementTree as ET


def members(root):
    out = {}
    for iface in root.iter("interface"):
        name = iface.get("name")
        if name.startswith("org.freedesktop.DBus."):
            continue
        m = out.setdefault(name, {})
        for meth in iface.findall("method"):
            m["method " + meth.get("name")] = [
                (a.get("direction", "in"), a.get("type"))
                for a in meth.findall("arg")
            ]
        for sig in iface.findall("signal"):
            m["signal " + sig.get("name")] = [
                a.get("type") for a in sig.findall("arg")
            ]
        for prop in iface.findall("property"):
            m["property " + prop.get("name")] = (
                prop.get("type"), prop.get("access"))
    return out


def parse(path):
    with open(path, encoding="utf-8") as f:
        text = f.read()
    # A DOCTYPE names an external DTD, which ElementTree never fetches; it
    # is dropped so a live reply and the vendored file parse alike.
    start = text.find("<node")
    return ET.fromstring(text[start:])


def main():
    if len(sys.argv) < 3:
        print("usage: apidiff.py CONTRACT.xml LIVE.xml...", file=sys.stderr)
        return 2
    want = members(parse(sys.argv[1]))
    have = {}
    for p in sys.argv[2:]:
        for iface, m in members(parse(p)).items():
            have.setdefault(iface, {}).update(m)
    bad = 0
    if not want:
        print(f"{sys.argv[1]} declares no interface")
        return 1
    for iface in sorted(want):
        if iface not in have:
            print(f"{iface} is not served")
            bad = 1
            continue
        for key, sig in sorted(want[iface].items()):
            got = have[iface].get(key)
            if got is None:
                print(f"{iface}: {key} is not served")
                bad = 1
            elif got != sig:
                print(f"{iface}: {key} is {got}, the contract says {sig}")
                bad = 1
    return bad


if __name__ == "__main__":
    sys.exit(main())
