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
 * A panel does not wrap or scroll. Short of room it narrows the pinned apps
 * and the meters' lines; what still does not fit is cut off at the screen's
 * edge, the clock and Show Desktop first. Everything but the pinned apps is
 * about 69 grid units wide at its preferred width; the pinned apps bring the
 * full desk to about 78. A screen narrower in logical pixels than that
 * drops parts of it when it is laid out: under 78 grid units the Start
 * label and the network meter, under 64 the pager as well. Under 50 grid
 * units even that does not fit, and the Dock or Tiling desk is the one to
 * use. The widths are the screen's when the desk is laid out; `kdos desk
 * kdos` lays it out again after a change of scale.
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

// A meter's total sensor is what its button speaks: the applet's tooltip
// subtext is "<sensor name>: <value>" of the first total sensor and empty
// without one, and a screen reader reads the panel button as "Open " and
// that subtext. The stock network preset names none. The entry is a JSON
// array, as the face controller reads it.
function addMeter(panel, plugin, face, total) {
    var meter = panel.addWidget(plugin);
    meter.currentConfigGroup = ["Appearance"];
    meter.writeConfig("chartFace", face);
    meter.currentConfigGroup = ["Sensors"];
    meter.writeConfig("totalSensors", JSON.stringify([total]));
    return meter;
}

// The tray's own settings, in its applet's [General] group (plasma-workspace
// systemtraysettings.cpp). plasma-pa's microphone item is hidden, and two
// default-on tray applets are kept off. Plasma's camera indicator: KDOS's
// privacy lamps (org.kdos.privacy) show the microphone, the camera and the
// screen together, naming the box that holds each. Merkuro's contacts widget:
// loading it starts Akonadi and its agents, so it would run the whole PIM
// stack at every login of an account that uses no KDE PIM program; enabled
// from the tray's settings it works as upstream ships it. The tray matches a
// status item by its StatusNotifierItem id, and plasma-pa publishes the
// microphone as plasmashell_microphone; a bare "microphone" hides nothing. The
// tray enables a default-on applet it has not seen yet, so each kept-off
// applet is taken out of extraItems if the new tray already enabled it and
// written into knownItems so it is never enabled later. readConfig needs the
// [] default to return a list: without one it returns the entry as one
// comma-joined string, no id is ever found in it, and writing it back escapes
// every comma into one item.
function addTray(panel) {
    var off = ["org.kde.plasma.cameraindicator", "org.kde.merkuro.contact.applet"];
    var tray = panel.addWidget("org.kde.plasma.systemtray");
    tray.currentConfigGroup = ["General"];
    tray.writeConfig("hiddenItems", ["plasmashell_microphone"]);
    var extra = [].concat(tray.readConfig("extraItems", []));
    tray.writeConfig("extraItems", extra.filter(function (id) { return off.indexOf(id) == -1; }));
    var known = [].concat(tray.readConfig("knownItems", []));
    tray.writeConfig("knownItems", known.concat(off.filter(function (id) { return known.indexOf(id) == -1; })));
    return tray;
}

for (var s = 0; s < screenCount; s++) {
    var width = screenGeometry(s).width / gridUnit;
    var full = width >= 78;
    var roomy = width >= 64;

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
    kickoff.writeConfig("menuLabel", full ? "Start" : "");
    kickoff.writeConfig("favorites", favorites);

    if (roomy) {
        panel.addWidget("org.kde.plasma.pager");
    }

    var tasks = panel.addWidget("org.kde.plasma.icontasks");
    tasks.currentConfigGroup = ["General"];
    tasks.writeConfig("launchers", launchers);
    tasks.writeConfig("groupingStrategy", 1);
    tasks.writeConfig("showOnlyCurrentScreen", true);

    panel.addWidget("org.kde.plasma.marginsseparator");

    // The meter strip's face is ksystemstats-kdos's org.kdos.meters.
    addMeter(panel, "org.kde.plasma.systemmonitor.cpu", "org.kdos.meters", "cpu/all/usage");
    addMeter(panel, "org.kde.plasma.systemmonitor.memory", "org.kdos.meters", "memory/physical/used");
    if (full) {
        addMeter(panel, "org.kde.plasma.systemmonitor.net", "org.kdos.meters", "network/all/download");
    }

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
