/*
 * ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   The KDOS desk — one opaque bottom panel per screen
 *
 * Start (the mark and its label), the desktops, the pinned apps, the meters,
 * the tray and the clock, left to right. The panel is not floating: KDOS
 * plates are opaque and sit on the screen edge, so the edge stays a target
 * the pointer cannot overshoot.
 *
 * THE PINNED APPS ARE THE FIRST WORDS OF ~/.config/kdos/favorites. That file
 * is also the list of boxes warmed at login, and a launcher here that it does
 * not name is an app whose box is cold on a fresh account. The lnfcheck test
 * compares the two lists.
 * ---------------------------------
 */

var launchers = [
    "applications:org.kde.konsole.desktop",
    "applications:org.kde.dolphin.desktop",
    "applications:firefox-esr.desktop",
    "applications:org.kde.kate.desktop",
];

var favorites = [
    "preferred://browser",
    "org.kde.konsole.desktop",
    "org.kde.dolphin.desktop",
    "systemsettings.desktop",
    "org.kde.discover.desktop",
];

// The clock wears the fixed-width face in force when the desk is laid out,
// read from kdeglobals rather than named here, so the desk never pins a face
// the global theme did not set. A manual font is a manual weight too: the
// applet's own default, 50, is Qt 5's scale and draws the thinnest cut.
function fixedFamily() {
    var fixed = String(ConfigFile("kdeglobals", "General").readEntry("fixed") || "");
    return fixed.split(",")[0];
}

function addMeter(panel, plugin, face) {
    var meter = panel.addWidget(plugin);
    meter.currentConfigGroup = ["Appearance"];
    meter.writeConfig("chartFace", face);
    return meter;
}

// The tray's own settings, in its applet's [General] group (plasma-workspace
// systemtraysettings.cpp). plasma-pa's microphone item is hidden, and
// Plasma's camera indicator is kept off: KDOS's privacy lamps
// (org.kdos.privacy) show the microphone, the camera and the screen together,
// naming the box that holds each. The tray enables a default-on item it has
// not seen yet, so the indicator is taken out of extraItems if the new tray
// already enabled it and written into knownItems so it is never enabled
// later.
function addTray(panel) {
    var camera = "org.kde.plasma.cameraindicator";
    var tray = panel.addWidget("org.kde.plasma.systemtray");
    tray.currentConfigGroup = ["General"];
    tray.writeConfig("hiddenItems", ["microphone"]);
    var extra = [].concat(tray.readConfig("extraItems") || []);
    if (extra.indexOf(camera) != -1) {
        tray.writeConfig("extraItems", extra.filter(function (id) { return id != camera; }));
    }
    var known = [].concat(tray.readConfig("knownItems") || []);
    if (known.indexOf(camera) == -1) {
        tray.writeConfig("knownItems", known.concat([camera]));
    }
    return tray;
}

for (var s = 0; s < screenCount; s++) {
    var panel = new Panel;
    panel.screen = s;
    panel.location = "bottom";
    // Two grid units and a half, rounded up to an even number: the height an
    // icons-only Task Manager is drawn for, and what the panel's own size
    // control can show.
    panel.height = 2 * Math.ceil(gridUnit * 2.5 / 2);
    panel.floating = false;
    panel.opacity = "opaque";
    panel.hiding = "none";

    var kickoff = panel.addWidget("org.kde.plasma.kickoff");
    kickoff.currentConfigGroup = ["General"];
    kickoff.writeConfig("icon", "start-here-kdos");
    kickoff.writeConfig("menuLabel", "Start");
    kickoff.writeConfig("favorites", favorites);

    panel.addWidget("org.kde.plasma.pager");

    var tasks = panel.addWidget("org.kde.plasma.icontasks");
    tasks.currentConfigGroup = ["General"];
    tasks.writeConfig("launchers", launchers);
    tasks.writeConfig("groupingStrategy", 1);
    tasks.writeConfig("showOnlyCurrentScreen", true);

    panel.addWidget("org.kde.plasma.marginsseparator");

    // The meter strip's face is ksystemstats-kdos's org.kdos.meters.
    addMeter(panel, "org.kde.plasma.systemmonitor.cpu", "org.kdos.meters");
    addMeter(panel, "org.kde.plasma.systemmonitor.memory", "org.kdos.meters");
    addMeter(panel, "org.kde.plasma.systemmonitor.net", "org.kdos.meters");

    // KDE's own rule for the input-method panel, by the locale's language.
    var imeLanguages = ["as", "bn", "bo", "brx", "doi", "gu", "hi", "ja", "kn", "ko", "kok",
                        "ks", "lep", "mai", "ml", "mni", "mr", "ne", "or", "pa", "sa", "sat",
                        "sd", "si", "ta", "te", "th", "ur", "vi", "zh_CN", "zh_TW"];
    if (imeLanguages.indexOf(languageId) != -1) {
        panel.addWidget("org.kde.plasma.kimpanel");
    }

    addTray(panel);

    var clock = panel.addWidget("org.kde.plasma.digitalclock");
    clock.currentConfigGroup = ["Appearance"];
    clock.writeConfig("showDate", true);
    var family = fixedFamily();
    if (family.length > 0) {
        clock.writeConfig("autoFontAndSize", false);
        clock.writeConfig("fontFamily", family);
        clock.writeConfig("fontSize", 10);
        clock.writeConfig("fontWeight", 400);
    }

    panel.addWidget("org.kde.plasma.showdesktop");
}

// When the desk was laid out, in seconds since the epoch: `kdos desk` takes an
// applets file written well after it for panels a person has changed since.
ConfigFile("kdosrc", "Desk").writeEntry("LaidOut", String(Math.floor(Date.now() / 1000)));
