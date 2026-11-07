## About

KDE Partition Manager edits the partition tables of the machine's disks: it creates, deletes,
resizes, moves and copies partitions, creates and checks file systems, sets labels and flags, and
applies a queue of operations in one go. The work is done by [kpmcore](#port-kpmcore), the
library this window drives, which also serves the graphical installer's manual layouts; the
operations run as root in kpmcore's helper, after polkit has asked through
[polkit-qt6](#port-polkit-qt6).

## Flags

- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON` — The handbook, through
  [kdoctools](#port-kdoctools), which upstream asks for as an optional component.
- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

The configure asks for kpmcore at the version of its own release branch (26.07 for a 26.08
release), so the two ports move together. Nothing can be changed until polkit grants kpmcore's
`org.kde.kpmcore.externalcommand.init` action, which needs a polkit agent in the session and a
rule or an `allow_active` default that lets the person through. Each file system needs its own
tools at run time (e2fsprogs, dosfstools, btrfs-progs and the rest), which kpmcore looks for when
the operation is chosen. `open_in_partitionmanager.desktop` is a Solid action: the device
notifier offers "Reformat or Edit with Partition Manager" for a plugged-in disk.
