/*
 * ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   The Tiling desk — one thin top bar per screen, and the auto-tiler on
 *
 * The Start button (the mark alone), the desktops by name, the window list,
 * then the processor and memory as text, the tray and the clock. Every
 * window is tiled, so the bar gives the screen back and carries no
 * launchers.
 *
 * Laying this desk out records that the auto-tiler is wanted: krohnkite is
 * enabled in kwinrc here, which KWin reads at its next start or
 * reconfigure. A desk picked in a running session is laid out by `kdos desk
 * tiling`, which also runs `kdos tiling on` and so reconfigures KWin at once.
 * ---------------------------------
 */

var favorites = [
    "preferred://browser",
    "org.kde.konsole.desktop",
    "org.kde.dolphin.desktop",
    "systemsettings.desktop",
    "org.kde.discover.desktop",
];

function fixedFamily() {
    var fixed = String(ConfigFile("kdeglobals", "General").readEntry("fixed") || "");
    return fixed.split(",")[0];
}

// A meter's total sensor is what its button speaks: the applet's tooltip
// subtext is "<sensor name>: <value>" of the first total sensor and empty
// without one, and a screen reader reads the panel button as "Open " and
// that subtext. The entry is a JSON array, as the face controller reads it.
function addMeter(panel, plugin, total) {
    var meter = panel.addWidget(plugin);
    meter.currentConfigGroup = ["Appearance"];
    meter.writeConfig("chartFace", "org.kde.ksysguard.textonly");
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
    var bar = new Panel;
    bar.screen = s;
    bar.location = "top";
    bar.height = 2 * Math.ceil(gridUnit * 1.25 / 2);
    bar.floating = false;
    bar.opacity = "opaque";
    bar.hiding = "none";

    var kickoff = bar.addWidget("org.kde.plasma.kickoff");
    kickoff.currentConfigGroup = ["General"];
    kickoff.writeConfig("icon", "start-here-kdos");
    kickoff.writeConfig("menuLabel", "");
    kickoff.writeConfig("favorites", favorites);

    var pager = bar.addWidget("org.kde.plasma.pager");
    pager.currentConfigGroup = ["General"];
    // displayedText: 0 number, 1 name, 2 none.
    pager.writeConfig("displayedText", 1);

    bar.addWidget("org.kde.plasma.windowlist");
    bar.addWidget("org.kde.plasma.panelspacer");

    addMeter(bar, "org.kde.plasma.systemmonitor.cpu", "cpu/all/usage");
    addMeter(bar, "org.kde.plasma.systemmonitor.memory", "memory/physical/used");

    addTray(bar);

    var clock = bar.addWidget("org.kde.plasma.digitalclock");
    clock.currentConfigGroup = ["Appearance"];
    clock.writeConfig("showDate", false);
    var family = fixedFamily();
    if (family.length > 0) {
        clock.writeConfig("autoFontAndSize", false);
        clock.writeConfig("fontFamily", family);
        clock.writeConfig("fontSize", 10);
        clock.writeConfig("fontWeight", 400);
    }
}

var plugins = ConfigFile("kwinrc", "Plugins");
plugins.writeEntry("krohnkiteEnabled", true);

// When the desk was laid out, in seconds since the epoch: `kdos desk` takes an
// applets file written well after it for panels a person has changed since.
ConfigFile("kdosrc", "Desk").writeEntry("LaidOut", String(Math.floor(Date.now() / 1000)));
