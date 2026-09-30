/*
 * What the shipped rules must and must not grant.
 *
 * THE ABSENCES ARE ASSERTED AS WELL AS THE GRANTS. A file that granted
 * everything would pass a test that only checked the grants, and naming
 * actions rather than an interface is worth exactly what is NOT in the list.
 */
function sub(g) { return { isInGroup: function (n) { return n === g; } }; }

/* Every id a shipped surface or program calls: kdos-net's list, join, forget
 * and wifi toggle, the hotspot, mmcli, pcscd, udisks2 for the toolkit
 * applications, and the administrative applications' own actions. */
var GRANT = [
    "org.freedesktop.NetworkManager.network-control",
    "org.freedesktop.NetworkManager.wifi.scan",
    "org.freedesktop.NetworkManager.enable-disable-wifi",
    "org.freedesktop.NetworkManager.enable-disable-network",
    "org.freedesktop.NetworkManager.settings.modify.own",
    /*
     * The one forgetting a network actually lands on. With
     * -Dsession_tracking=no a profile naming an owner is invisible and never
     * autoconnects, so kdos-net writes no owner and everything it creates is
     * a system connection.
     */
    "org.freedesktop.NetworkManager.settings.modify.system",
    "org.freedesktop.NetworkManager.wifi.share.open",
    "org.freedesktop.NetworkManager.wifi.share.protected",
    /* mmcli: a SIM's PIN, enabling a modem, and its text messages. */
    "org.freedesktop.ModemManager1.Device.Control",
    "org.freedesktop.ModemManager1.Messaging",
    /* pcscd: gpg's scdaemon, opensc and ykman reaching a smart card. */
    "org.debian.pcsc-lite.access_pcsc",
    "org.debian.pcsc-lite.access_card",
    /* udisks2: Dolphin's devices, GNOME Disks, K3b, Impression. */
    "org.freedesktop.udisks2.filesystem-mount",
    "org.freedesktop.udisks2.filesystem-mount-system",
    "org.freedesktop.udisks2.filesystem-unmount-others",
    "org.freedesktop.udisks2.filesystem-take-ownership",
    "org.freedesktop.udisks2.encrypted-unlock",
    "org.freedesktop.udisks2.encrypted-unlock-system",
    "org.freedesktop.udisks2.encrypted-lock-others",
    "org.freedesktop.udisks2.encrypted-change-passphrase",
    "org.freedesktop.udisks2.eject-media",
    "org.freedesktop.udisks2.eject-media-system",
    "org.freedesktop.udisks2.power-off-drive",
    "org.freedesktop.udisks2.loop-setup",
    "org.freedesktop.udisks2.loop-delete-others",
    "org.freedesktop.udisks2.open-device",
    "org.freedesktop.udisks2.open-device-system",
    "org.freedesktop.udisks2.modify-device",
    "org.freedesktop.udisks2.modify-device-system",
    "org.freedesktop.udisks2.rescan",
    "org.freedesktop.udisks2.cancel-job",
    "org.freedesktop.udisks2.ata-check-power",
    "org.freedesktop.udisks2.ata-smart-update",
    "org.freedesktop.udisks2.nvme-smart-update",
    /* GParted, K3b's and KTextEditor's KAuth helpers, Resources, libvirt. */
    "org.gnome.gparted",
    "org.kde.k3b.updatepermissions",
    "org.kde.ktexteditor6.katetextbuffer.savefile",
    "net.nokyan.Resources.kill",
    "org.libvirt.unix.manage"
];

/* Named in the rules file's own comment as deliberately absent. If one starts
 * being granted, that comment has stopped being true. */
var WITHHOLD = [
    "org.freedesktop.NetworkManager.settings.modify.hostname",
    "org.freedesktop.NetworkManager.settings.modify.global-dns",
    "org.freedesktop.NetworkManager.checkpoint-rollback",
    "org.freedesktop.NetworkManager.sleep-wake",
    "org.freedesktop.NetworkManager.reload",
    "org.freedesktop.udisks2.filesystem-fstab",
    "org.freedesktop.udisks2.encrypted-unlock-crypttab",
    "org.freedesktop.udisks2.modify-system-configuration",
    "org.freedesktop.udisks2.read-system-configuration-secrets",
    "org.freedesktop.udisks2.manage-md-raid",
    "org.freedesktop.udisks2.manage-swapspace",
    "org.freedesktop.udisks2.lvm2.manage-lvm",
    "org.freedesktop.udisks2.btrfs.manage-btrfs",
    "org.freedesktop.udisks2.ata-secure-erase",
    "org.freedesktop.udisks2.nvme-sanitize",
    "org.freedesktop.udisks2.nvme-format-namespace",
    "org.kde.k3b.addtogroup",
    "org.gtk.vfs.file-operations",
    "org.kde.ksysguard.processlisthelper.sendsignal",
    "org.freedesktop.policykit.exec"
];

var fails = 0, i, r;

if (polkit._rules.length === 0) {
    print("  FAIL  the rules file registered no rule at all");
    fails++;
}
for (i = 0; i < GRANT.length; i++) {
    r = polkit._ask(GRANT[i], sub("wheel"));
    if (r !== polkit.Result.YES) {
        print("  FAIL  wheel is not granted " + GRANT[i]);
        fails++;
    }
}
for (i = 0; i < WITHHOLD.length; i++) {
    r = polkit._ask(WITHHOLD[i], sub("wheel"));
    if (r !== undefined) {
        print("  FAIL  wheel is granted " + WITHHOLD[i] + ", which the file says it is not");
        fails++;
    }
}
/* Outside wheel the rule returns nothing, so polkit falls through to its own
 * default rather than this file deciding a denial on its behalf. */
for (i = 0; i < GRANT.length; i++) {
    r = polkit._ask(GRANT[i], sub("users"));
    if (r !== undefined) {
        print("  FAIL  a subject outside wheel was answered for " + GRANT[i]);
        fails++;
    }
}

if (fails)
    print("  " + fails + " failed");
else
    print("  ok    wheel gets the " + GRANT.length + " actions the surfaces and applications ask for,\n" +
          "        is refused the " + WITHHOLD.length + " the file says it withholds, and a subject\n" +
          "        outside wheel is not answered at all");
