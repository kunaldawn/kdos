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

function addMeter(panel, plugin) {
    var meter = panel.addWidget(plugin);
    meter.currentConfigGroup = ["Appearance"];
    meter.writeConfig("chartFace", "org.kde.ksysguard.textonly");
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

    addMeter(bar, "org.kde.plasma.systemmonitor.cpu");
    addMeter(bar, "org.kde.plasma.systemmonitor.memory");

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
