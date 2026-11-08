/*
 * What polkitd answers, with the shipped rules and the actions' defaults
 * together, for six subjects: a wheel member and a user outside it, each at
 * the machine (local, active), in a local session switched away from (local,
 * inactive) and away from it (an SSH login: neither).
 *
 * THE TABLE IS WRITTEN BY HAND, not derived from the defaults, so it says
 * what the security model promises and the rules file plus defaults.js have
 * to produce it. The refusals and prompts are asserted as well as the grants:
 * a file that granted everything would pass a test that only checked the
 * grants.
 *
 *   y yes   n no   A auth_admin   K auth_admin_keep   S auth_self_keep
 */
var WA = polkit._subject("wheel at the machine", ["wheel", "users"], true, true);
var WI = polkit._subject("wheel, switched away", ["wheel", "users"], true, false);
var WR = polkit._subject("wheel over ssh", ["wheel", "users"], false, false);
var UA = polkit._subject("a user at the machine", ["users"], true, true);
var UI = polkit._subject("a user, switched away", ["users"], true, false);
var UR = polkit._subject("a user over ssh", ["users"], false, false);
var WHO = [WA, WI, WR, UA, UI, UR];
var WORD = { y: "yes", n: "no", A: "auth_admin", K: "auth_admin_keep",
             S: "auth_self_keep" };

/*            action                                         WA WI WR UA UI UR */
var EXPECT = [
    /* NetworkManager: the everyday actions are allow_active upstream; a
     * system connection is the one grant. */
    ["org.freedesktop.NetworkManager.network-control",          "yyAyyA"],
    ["org.freedesktop.NetworkManager.wifi.scan",                "yyAyyA"],
    ["org.freedesktop.NetworkManager.enable-disable-wifi",      "ynnynn"],
    ["org.freedesktop.NetworkManager.enable-disable-network",   "ynnynn"],
    ["org.freedesktop.NetworkManager.wifi.share.open",          "ynnynn"],
    ["org.freedesktop.NetworkManager.wifi.share.protected",     "ynnynn"],
    ["org.freedesktop.NetworkManager.settings.modify.own",      "yySyyS"],
    ["org.freedesktop.NetworkManager.settings.modify.system",   "yKKKKK"],
    ["org.freedesktop.NetworkManager.settings.modify.hostname", "KKKKKK"],
    ["org.freedesktop.NetworkManager.settings.modify.global-dns", "KKKKKK"],
    ["org.freedesktop.NetworkManager.checkpoint-rollback",      "KKKKKK"],
    ["org.freedesktop.NetworkManager.sleep-wake",               "nnnnnn"],
    ["org.freedesktop.NetworkManager.reload",                   "KKKKKK"],
    /* Modems and smart cards: allow_active upstream, no rule. */
    ["org.freedesktop.ModemManager1.Device.Control",            "ynnynn"],
    ["org.freedesktop.ModemManager1.Messaging",                 "ynnynn"],
    ["org.debian.pcsc-lite.access_pcsc",                        "ynnynn"],
    ["org.debian.pcsc-lite.access_card",                        "ynnynn"],
    /* udisks2: removable drives are anybody's at the machine; internal
     * drives and other users' mounts are wheel's without a prompt there. */
    ["org.freedesktop.udisks2.filesystem-mount",                "yAAyAA"],
    ["org.freedesktop.udisks2.filesystem-mount-system",         "yAAKAA"],
    ["org.freedesktop.udisks2.filesystem-unmount-others",       "yAAKAA"],
    ["org.freedesktop.udisks2.filesystem-take-ownership",       "yAAKAA"],
    ["org.freedesktop.udisks2.encrypted-unlock",                "yAAyAA"],
    ["org.freedesktop.udisks2.encrypted-unlock-system",         "yAAKAA"],
    ["org.freedesktop.udisks2.encrypted-lock-others",           "yAAKAA"],
    ["org.freedesktop.udisks2.encrypted-change-passphrase",     "yAAyAA"],
    ["org.freedesktop.udisks2.eject-media",                     "yAAyAA"],
    ["org.freedesktop.udisks2.eject-media-system",              "yAAKAA"],
    ["org.freedesktop.udisks2.power-off-drive",                 "yAAyAA"],
    ["org.freedesktop.udisks2.power-off-drive-system",          "KAAKAA"],
    ["org.freedesktop.udisks2.loop-setup",                      "yAAyAA"],
    ["org.freedesktop.udisks2.loop-delete-others",              "yAAKAA"],
    ["org.freedesktop.udisks2.open-device",                     "yAAKAA"],
    ["org.freedesktop.udisks2.open-device-system",              "yAAKAA"],
    ["org.freedesktop.udisks2.modify-device",                   "yAAyAA"],
    ["org.freedesktop.udisks2.modify-device-system",            "yAAKAA"],
    ["org.freedesktop.udisks2.filesystem-mount-other-seat",     "KAAKAA"],
    ["org.freedesktop.udisks2.rescan",                          "yAAyAA"],
    ["org.freedesktop.udisks2.cancel-job",                      "yAAyAA"],
    ["org.freedesktop.udisks2.ata-smart-update",                "yAAyAA"],
    ["org.freedesktop.udisks2.nvme-smart-update",               "yAAyAA"],
    /* …and the storage layout and the firmware erases keep their prompt. */
    ["org.freedesktop.udisks2.filesystem-fstab",                "KAAKAA"],
    ["org.freedesktop.udisks2.encrypted-unlock-crypttab",       "KAAKAA"],
    ["org.freedesktop.udisks2.modify-system-configuration",     "AAAAAA"],
    ["org.freedesktop.udisks2.read-system-configuration-secrets", "AAAAAA"],
    ["org.freedesktop.udisks2.manage-md-raid",                  "KAAKAA"],
    ["org.freedesktop.udisks2.manage-swapspace",                "KAAKAA"],
    ["org.freedesktop.udisks2.lvm2.manage-lvm",                 "KAAKAA"],
    ["org.freedesktop.udisks2.btrfs.manage-btrfs",              "KAAKAA"],
    ["org.freedesktop.udisks2.ata-secure-erase",                "KAAKAA"],
    ["org.freedesktop.udisks2.nvme-sanitize",                   "KAAKAA"],
    ["org.freedesktop.udisks2.nvme-format-namespace",           "KAAKAA"],
    /* Administrative applications: one password per agent session for
     * wheel at the machine, the defaults everywhere else. */
    ["org.gnome.gparted",                                       "KAAAAA"],
    ["org.kde.k3b.updatepermissions",                           "KnnAnn"],
    ["org.kde.ktexteditor6.katetextbuffer.savefile",            "KnnAnn"],
    ["org.libvirt.unix.manage",                                 "KKKKKK"],
    ["net.nokyan.Resources.kill",                               "KnnKnn"],
    ["org.kde.k3b.addtogroup",                                  "AnnAnn"],
    ["org.kde.ksysguard.processlisthelper.sendsignal",          "AnnAnn"],
    ["org.freedesktop.policykit.exec",                          "AAAAAA"],
    /* Held at a password for everyone at the machine. */
    ["org.freedesktop.fwupd.update-internal",                   "KnAKnA"],
    ["org.freedesktop.bolt.enroll",                             "KAAKAA"],
    ["org.freedesktop.bolt.authorize",                          "KAAKAA"],
    ["org.freedesktop.bolt.manage",                             "KAAKAA"],
    ["org.gtk.vfs.file-operations",                             "KnnKnn"],
    ["org.gtk.vfs.file-operations-helper",                      "KnnKnn"],
    /* Signed firmware and the metadata Discover lists it from. */
    ["org.freedesktop.fwupd.update-internal-trusted",           "ynAynA"],
    ["org.freedesktop.fwupd.get-remotes",                       "ynAynA"],
    ["org.freedesktop.fwupd.refresh-remote",                    "yyAyyA"],
    /* The system crontab is not offered. */
    ["local.kcron.crontab.save",                                "nnnnnn"],
    /* elogind: the person at the machine suspends, powers off and
     * reboots; nobody away from it does without an administrator. */
    ["org.freedesktop.login1.suspend",                          "yKKyKK"],
    ["org.freedesktop.login1.power-off",                        "yKKyKK"],
    ["org.freedesktop.login1.reboot",                           "yKKyKK"],
    ["org.freedesktop.login1.power-off-multiple-sessions",      "yKKyKK"],
    ["org.freedesktop.login1.suspend-ignore-inhibit",           "KKKKKK"],
    ["org.freedesktop.login1.inhibit-handle-lid-switch",        "yynyyn"],
    ["org.freedesktop.login1.inhibit-block-sleep",              "yynyyn"],
    ["org.freedesktop.login1.inhibit-delay-sleep",              "yyyyyy"],
    ["org.freedesktop.login1.lock-sessions",                    "KKKKKK"],
    ["org.freedesktop.login1.set-self-linger",                  "yyyyyy"],
    ["org.freedesktop.login1.set-user-linger",                  "KKKKKK"],
    ["org.freedesktop.login1.chvt",                             "yyKyyK"],
    ["org.freedesktop.login1.set-reboot-to-firmware-setup",     "yKKyKK"],
    /* Accounts: one's own details freely, anyone else's with an admin. */
    ["org.freedesktop.accounts.change-own-user-data",           "yyyyyy"],
    ["org.freedesktop.accounts.change-own-password",            "AAAAAA"],
    ["org.freedesktop.accounts.user-administration",            "KAAKAA"],
    ["org.freedesktop.accounts.set-login-option",               "KAAKAA"],
    /* kdos-settingsd: the zone and network time at the machine, the rest
     * an administrator's. */
    ["org.freedesktop.timedate1.set-timezone",                  "yKKyKK"],
    ["org.freedesktop.timedate1.set-ntp",                       "yKKyKK"],
    ["org.freedesktop.timedate1.set-time",                      "KKKKKK"],
    ["org.freedesktop.timedate1.set-local-rtc",                 "KKKKKK"],
    ["org.freedesktop.hostname1.set-hostname",                  "KKKKKK"],
    ["org.freedesktop.hostname1.set-static-hostname",           "KKKKKK"],
    ["org.freedesktop.hostname1.set-machine-info",              "KKKKKK"],
    /* …and the two hardware identifiers, which name this one machine. */
    ["org.freedesktop.hostname1.get-product-uuid",              "KKKKKK"],
    ["org.freedesktop.hostname1.get-hardware-serial",           "KKKKKK"],
    ["org.freedesktop.locale1.set-locale",                      "KKKKKK"],
    ["org.freedesktop.locale1.set-keyboard",                    "KKKKKK"],
    /* kdos-sysupdated: anyone at the machine checks; installing an update
     * or a chosen version, trying the other slot and changing channel are
     * an administrator's; the session that came up blesses its own boot. */
    ["org.freedesktop.sysupdate1.check",                        "yAAyAA"],
    ["org.freedesktop.sysupdate1.update",                       "KAAKAA"],
    ["org.freedesktop.sysupdate1.update-to-version",            "AAAAAA"],
    ["io.kdos.slots.try",                                       "KAAKAA"],
    ["io.kdos.slots.channel",                                   "KAAKAA"],
    ["io.kdos.slots.bless",                                     "ynnynn"],
    /* Discover's KDOS backend: a port built and installed from the tree. */
    ["io.kdos.ports.install",                                   "KAAKAA"],
    /* The graphical installer erases a disk: an administrator's password
     * every time, at the machine only. The live medium's own rule
     * (49-kdos-installer-live.rules) grants it there and is not shipped on
     * an installed root, which is what this table models. */
    ["io.kdos.installer.run",                                   "AnnAnn"],
    /* Power profiles, sensors, brightness: the person at the machine. */
    ["org.freedesktop.UPower.PowerProfiles.switch-profile",     "ynnynn"],
    ["org.freedesktop.UPower.PowerProfiles.hold-profile",       "ynnynn"],
    ["org.freedesktop.UPower.PowerProfiles.release-profile",    "ynnynn"],
    ["net.hadess.SensorProxy.claim-sensor",                     "ynnynn"],
    ["org.kde.powerdevil.backlighthelper.setbrightness",        "ynnynn"],
    ["org.kde.powerdevil.backlighthelper.brightness",           "yynyyn"],
    /* Charge thresholds are TLP's: PowerDevil may read them, never set them. */
    ["org.kde.powerdevil.chargethresholdhelper.getthreshold",   "yynyyn"],
    ["org.kde.powerdevil.chargethresholdhelper.setthreshold",   "nnnnnn"],
    ["org.kde.powerdevil.chargethresholdhelper.setconservationmode", "nnnnnn"],
    ["org.kde.powerdevil.discretegpuhelper.hasdualgpu",         "ynnynn"],
    ["org.kde.powerdevil.wakeupsourcehelper.setwakeupcount",    "ynnynn"],
    /* System Settings' administrator pages. */
    ["org.kde.kcontrol.kcmsddm.save",                           "AnnAnn"],
    ["org.kde.kcontrol.kcmsddm.sync",                           "AnnAnn"],
    ["org.kde.kcontrol.kcmsddm.reset",                          "AnnAnn"],
    ["org.kde.kpmcore.externalcommand.init",                    "KnnKnn"],
    ["org.kde.kinfocenter.dmidecode.memoryinformation",         "ynnynn"],
    ["org.kde.kinfocenter.dmidecode.systeminformation",         "yynyyn"],
    /* kdos-powerd's changes. Samba's start is the person at the machine's
     * whoever they are: it serves only shares that already exist. */
    ["io.kdos.firewall.set",                                    "KnnKnn"],
    ["io.kdos.accent.set",                                      "ynnynn"],
    ["io.kdos.filesharing.start",                               "ynnynn"]
];

/* The actions the rules file answers for, and no others: every other id in
 * the model must reach polkitd's defaults untouched, whoever asks. */
var RULED = [
    "local.kcron.crontab.save",
    "org.kde.powerdevil.chargethresholdhelper.setthreshold",
    "org.kde.powerdevil.chargethresholdhelper.setconservationmode",
    "org.freedesktop.fwupd.update-internal",
    "org.freedesktop.bolt.enroll",
    "org.freedesktop.bolt.authorize",
    "org.freedesktop.bolt.manage",
    "org.gtk.vfs.file-operations",
    "org.gtk.vfs.file-operations-helper",
    "org.freedesktop.NetworkManager.settings.modify.system",
    "org.freedesktop.udisks2.filesystem-mount-system",
    "org.freedesktop.udisks2.filesystem-unmount-others",
    "org.freedesktop.udisks2.filesystem-take-ownership",
    "org.freedesktop.udisks2.encrypted-unlock-system",
    "org.freedesktop.udisks2.encrypted-lock-others",
    "org.freedesktop.udisks2.eject-media-system",
    "org.freedesktop.udisks2.loop-delete-others",
    "org.freedesktop.udisks2.open-device",
    "org.freedesktop.udisks2.open-device-system",
    "org.freedesktop.udisks2.modify-device-system",
    "org.gnome.gparted",
    "org.kde.k3b.updatepermissions",
    "org.kde.ktexteditor6.katetextbuffer.savefile",
    "org.libvirt.unix.manage",
    "net.nokyan.Resources.kill"
];

var fails = 0, i, j, k, nshipped = 0, nbuilt = 0;

function fail(msg) {
    print("  FAIL  " + msg);
    fails++;
}

function same3(a, b) {
    return a && b && a[0] === b[0] && a[1] === b[1] && a[2] === b[2];
}

/*
 * KDOS's OWN ACTIONS COME FROM THE .policy FILES IT SHIPS, so what is asserted
 * is what ships. A row in defaults.js for the same action must say the same,
 * and every shipped action must have a row in EXPECT below: a polkit action
 * KDOS adds is a decision about who may do what, and this table is where
 * that decision is written down.
 */
var expected = {};
for (i = 0; i < EXPECT.length; i++)
    expected[EXPECT[i][0]] = true;
for (k in polkit._shipped) {
    nshipped++;
    if (polkit._defaults[k] && !same3(polkit._defaults[k], polkit._shipped[k]))
        fail(k + ": defaults.js says " + polkit._defaults[k].join("/") +
             ", the shipped .policy says " + polkit._shipped[k].join("/"));
    polkit._defaults[k] = polkit._shipped[k];
    if (!expected[k])
        fail(k + " is shipped by a KDOS port and has no row in assert.js's table");
}

if (polkit._rules.length === 0)
    fail("the rules file registered no rule at all");

for (i = 0; i < EXPECT.length; i++) {
    var id = EXPECT[i][0], want = EXPECT[i][1];

    if (!polkit._defaults[id]) {
        fail(id + " has no row in defaults.js, so its default is unknown");
        continue;
    }
    for (j = 0; j < WHO.length; j++) {
        var got = polkit._decide(id, WHO[j]);

        if (got !== WORD[want.charAt(j)])
            fail(id + " for " + WHO[j].who + ": " + got + ", the model says " +
                 WORD[want.charAt(j)]);
    }
}

var ruled = {};
for (i = 0; i < RULED.length; i++)
    ruled[RULED[i]] = true;
for (k in polkit._defaults) {
    if (ruled[k])
        continue;
    for (j = 0; j < WHO.length; j++)
        if (polkit._ask(k, WHO[j]) !== undefined)
            fail("the rules answer " + k + " for " + WHO[j].who +
                 ", which is not one of the actions they name");
}
for (i = 0; i < RULED.length; i++) {
    var any = false;

    for (j = 0; j < WHO.length; j++)
        if (polkit._ask(RULED[i], WHO[j]) !== undefined)
            any = true;
    if (!any)
        fail(RULED[i] + " is named as ruled and no subject gets a rule's answer");
}

/* The model against the .policy files the tree's own once one is built: an
 * action the built tree has must agree where the model has it. */

for (k in polkit._built) {
    if (!polkit._defaults[k])
        continue;
    nbuilt++;
    if (!same3(polkit._defaults[k], polkit._built[k]))
        fail(k + ": defaults.js says " + polkit._defaults[k].join("/") +
             ", the built tree's .policy says " + polkit._built[k].join("/"));
}

if (fails)
    print("  " + fails + " failed");
else
    print("  ok    " + EXPECT.length + " actions answer as the model says for wheel and\n" +
          "        a user, at the machine, switched away and over ssh; the rules\n" +
          "        touch only the " + RULED.length + " actions they name; " + nshipped +
          " shipped KDOS actions\n" +
          "        and " + nbuilt + " actions of the built tree agree with the defaults");
