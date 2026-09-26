/*
 * What the shipped rules must and must not grant.
 *
 * THE ABSENCES ARE ASSERTED AS WELL AS THE GRANTS. A file that granted
 * everything would pass a test that only checked the grants, and naming
 * actions rather than an interface is worth exactly what is NOT in the list.
 */
function sub(g) { return { isInGroup: function (n) { return n === g; } }; }

/* Every id a shipped surface or program calls: kdos-net's list, join, forget
 * and wifi toggle, the hotspot, mmcli, and pcscd. */
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
    "org.debian.pcsc-lite.access_card"
];

/* Named in the rules file's own comment as deliberately absent. If one starts
 * being granted, that comment has stopped being true. */
var WITHHOLD = [
    "org.freedesktop.NetworkManager.settings.modify.hostname",
    "org.freedesktop.NetworkManager.settings.modify.global-dns",
    "org.freedesktop.NetworkManager.checkpoint-rollback",
    "org.freedesktop.NetworkManager.sleep-wake",
    "org.freedesktop.NetworkManager.reload"
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
    print("  ok    wheel gets the twelve the surfaces, mmcli and pcscd ask for, is refused the five\n" +
          "        the file says it withholds, and a subject outside wheel is\n" +
          "        not answered at all");
