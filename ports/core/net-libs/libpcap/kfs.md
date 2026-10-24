## About
libpcap is the packet capture library: it opens a network interface, applies a BPF filter in the
kernel and hands captured packets to the program, and reads and writes the pcap file format.
[tcpdump](#port-tcpdump), [wireshark](#port-wireshark), [nmap](#port-nmap), [iftop](#port-iftop)
and [aircrack-ng](#port-aircrack-ng) are among the ports built on it. Besides network interfaces
this build captures from USB buses, Bluetooth adapters and the D-Bus message buses.

## Flags
- `--with-libnl` — Link [libnl](#port-libnl)'s generic netlink library, which libpcap uses to
  create a monitor-mode interface on a mac80211 wireless device (`tcpdump -I`). The default
  takes libnl only if present; naming it makes configure stop when libnl is missing.
- `--enable-dbus` — Add capture from a D-Bus bus (`dbus-system`, `dbus-session`). As
  an explicit `yes` it stops configure when `dbus-1` is missing, instead of building without it.
- `--enable-usb` — Add capture from the kernel's `usbmon` interfaces (`usbmon0`, `usbmon1`…).
- `--enable-bluetooth` — Add capture from Bluetooth HCI adapters and the HCI monitor channel,
  through [bluez](#port-bluez)'s headers.
- `--disable-netmap` — Build no netmap capture module. netmap is a kernel framework kept outside
  Linux's own tree, and no port here builds it or its headers.
- `--disable-rdma` — Build no RDMA (InfiniBand) capture module, which needs `libibverbs`; the
  tree has no rdma-core port.
- `--without-dag` — Build without Endace DAG capture-card support, and `--without-snf` without
  Myricom SNF; both need the vendors' own libraries, which the tree does not carry.
