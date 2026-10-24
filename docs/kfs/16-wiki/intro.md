# System administration

<!-- kfs:order [System] init-and-runlevels services writing-a-service timers logs modules-and-sysctl zram-and-swap out-of-memory time-and-ntp locale-and-console users-and-groups [Boot] boot-menu-and-kernel-parameters initramfs-and-rescue microcode-and-firmware ab-slots-and-rollback live-persistence [Storage] filesystems-and-fstab storage-and-removable-media encrypted-root lvm-and-raid disk-health backups [Security] sudo-pam-polkit login-and-autologin screen-lock fingerprint-fido-smartcards firewall ssh-server signing-keys-and-trust [Networking] networkmanager dns vpn mobile-broadband access-point wireless-regulatory-domain file-sharing avahi-and-mdns [Hardware] udev-and-device-groups bluetooth printing-and-scanning audio displays-and-scaling graphics-drivers power-management laptops firmware-updates [Packages and applications] kpkg updating local-rebuilds-and-march packs boxes containers virtual-machines [Desktop] session-and-environment compositor-keybindings panel-and-launcher xwayland-and-x11-apps input-methods fonts-and-theming accessibility portals-and-screen-sharing [Troubleshooting] diagnostics desktop-troubleshooting recovering-an-unbootable-system -->

This part is about running a KDOS machine rather than building one. Each chapter takes one
subject, such as the firewall, a [printer]{.idx} or a system that will not boot, and gives the
commands for it in the manner of a wiki: what is installed, which file configures it, how to use
it, and what to do when it fails. The chapters can be read in any order.

Every chapter has the same sections, in this order: Installation, Configuration, Usage, Tips,
Troubleshooting, Known gaps and See also; `testing/docscheck.sh` refuses a chapter in this part
that departs from them. A configuration file shown on a page is read from the tree each time the
book is rendered, so it is the file KDOS ships. The chapters give numbered procedures; why a
mechanism works the way it does is in [the KDOS book](../../kdos/README.md), which every chapter
links to. A step that has not been carried out on a running machine says so where it is given.

The chapters fall into nine groups.

## System

Starting and stopping what runs, its logs, memory, time, language and accounts.

- [Init and runlevels](init-and-runlevels.md)
- [Services](services.md)
- [Writing a service](writing-a-service.md)
- [Timers](timers.md)
- [Logs](logs.md)
- [Modules and sysctl](modules-and-sysctl.md)
- [zram and swap](zram-and-swap.md)
- [Out of memory](out-of-memory.md)
- [Time and NTP](time-and-ntp.md)
- [Locale and console](locale-and-console.md)
- [Users and groups](users-and-groups.md)

## Boot

The boot menu, the initramfs, microcode, the two slots and the live medium's own store.

- [Boot menu and kernel parameters](boot-menu-and-kernel-parameters.md)
- [The initramfs and the rescue shell](initramfs-and-rescue.md)
- [Microcode and firmware](microcode-and-firmware.md)
- [A/B slots and rollback](ab-slots-and-rollback.md)
- [Live persistence](live-persistence.md)

## Storage

Filesystems, removable disks, encryption, volumes and arrays, disk health and backups.

- [Filesystems and fstab](filesystems-and-fstab.md)
- [Storage and removable media](storage-and-removable-media.md)
- [Encrypted root](encrypted-root.md)
- [LVM and RAID](lvm-and-raid.md)
- [Disk health](disk-health.md)
- [Backups](backups.md)

## Security

Who may do what, logging in and locking, hardware keys, the firewall, SSH and signing keys.

- [sudo, PAM and polkit](sudo-pam-polkit.md)
- [Login and autologin](login-and-autologin.md)
- [Screen lock](screen-lock.md)
- [Fingerprints, FIDO keys and smart cards](fingerprint-fido-smartcards.md)
- [Firewall](firewall.md)
- [SSH server](ssh-server.md)
- [Signing keys and trust](signing-keys-and-trust.md)

## Networking

Connections, name resolution, tunnels, modems, a hotspot, the radio's country, sharing files and finding services.

- [NetworkManager](networkmanager.md)
- [DNS](dns.md)
- [VPN](vpn.md)
- [Mobile broadband](mobile-broadband.md)
- [Access point](access-point.md)
- [Wireless regulatory domain](wireless-regulatory-domain.md)
- [File sharing](file-sharing.md)
- [Avahi and mDNS](avahi-and-mdns.md)

## Hardware

Devices and the groups that open them, sound, printers, displays, graphics, power and firmware.

- [udev and device groups](udev-and-device-groups.md)
- [Bluetooth](bluetooth.md)
- [Printing and scanning](printing-and-scanning.md)
- [Audio](audio.md)
- [Displays and scaling](displays-and-scaling.md)
- [Graphics drivers](graphics-drivers.md)
- [Power management](power-management.md)
- [Laptops](laptops.md)
- [Firmware updates](firmware-updates.md)

## Packages and applications

Installing and updating packages, rebuilding them, and running applications in packs, boxes, containers and virtual machines.

- [kpkg](kpkg.md)
- [Updating](updating.md)
- [Local rebuilds and kdos march](local-rebuilds-and-march.md)
- [Packs](packs.md)
- [Boxes](boxes.md)
- [Containers](containers.md)
- [Virtual machines](virtual-machines.md)

## Desktop

The session, the compositor's keys, the panel, X11 programs, input methods, fonts, accessibility and portals.

- [Session and environment](session-and-environment.md)
- [Compositor keybindings](compositor-keybindings.md)
- [The panel and the launcher](panel-and-launcher.md)
- [Xwayland and X11 applications](xwayland-and-x11-apps.md)
- [Input methods](input-methods.md)
- [Fonts and theming](fonts-and-theming.md)
- [Accessibility](accessibility.md)
- [Portals and screen sharing](portals-and-screen-sharing.md)

## Troubleshooting

Finding out what is wrong, and getting a desktop or a whole system back.

- [Diagnostics](diagnostics.md)
- [Desktop troubleshooting](desktop-troubleshooting.md)
- [Recovering an unbootable system](recovering-an-unbootable-system.md)

## See also

- [Administration](../../kdos/02-user-guide/administration.md): the KDOS book's own chapter on running the system
- [Configuration reference](../../kdos/06-reference/configuration.md): every configuration key
- [Known gaps](../../kdos/06-reference/known-gaps.md): what does not exist

<!-- book-nav -->
---

*Part XVI. System administration.* Previous: [357. The ISO](../15-image/110-iso.md) · [Contents](../README.md) · Next: [358. Init and runlevels](init-and-runlevels.md)
