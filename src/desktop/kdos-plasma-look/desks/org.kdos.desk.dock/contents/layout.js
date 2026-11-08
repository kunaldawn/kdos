/*
 * ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   The Dock desk — a top bar and a floating dock, per screen
 *
 * The bar holds the Start button (the mark alone), the focused window's
 * application menu, the tray and the clock. The dock is centred, as long as
 * its contents, and moves out of a window's way; it holds the same pinned
 * apps as the KDOS desk, which are the first words of
 * ~/.config/kdos/favorites for the reason the KDOS desk gives.
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

function fixedFamily() {
    var fixed = String(ConfigFile("kdeglobals", "General").readEntry("fixed") || "");
    return fixed.split(",")[0];
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
    bar.height = 2 * Math.ceil(gridUnit * 1.5 / 2);
    bar.floating = false;
    bar.opacity = "opaque";
    bar.hiding = "none";

    var kickoff = bar.addWidget("org.kde.plasma.kickoff");
    kickoff.currentConfigGroup = ["General"];
    kickoff.writeConfig("icon", "start-here-kdos");
    kickoff.writeConfig("menuLabel", "");
    kickoff.writeConfig("favorites", favorites);

    bar.addWidget("org.kde.plasma.appmenu");
    bar.addWidget("org.kde.plasma.panelspacer");
    addTray(bar);

    var clock = bar.addWidget("org.kde.plasma.digitalclock");
    clock.currentConfigGroup = ["Appearance"];
    clock.writeConfig("showDate", true);
    var family = fixedFamily();
    if (family.length > 0) {
        clock.writeConfig("autoFontAndSize", false);
        clock.writeConfig("fontFamily", family);
        clock.writeConfig("fontSize", 10);
        clock.writeConfig("fontWeight", 400);
    }

    var dock = new Panel;
    dock.screen = s;
    dock.location = "bottom";
    dock.height = 2 * Math.ceil(gridUnit * 2.5 / 2);
    dock.floating = true;
    dock.opacity = "opaque";
    dock.alignment = "center";
    dock.lengthMode = "fit";
    dock.hiding = "dodgewindows";

    var tasks = dock.addWidget("org.kde.plasma.icontasks");
    tasks.currentConfigGroup = ["General"];
    tasks.writeConfig("launchers", launchers);
    tasks.writeConfig("groupingStrategy", 1);
    tasks.writeConfig("showOnlyCurrentScreen", true);

    dock.addWidget("org.kde.plasma.showdesktop");
}

// When the desk was laid out, in seconds since the epoch: `kdos desk` takes an
// applets file written well after it for panels a person has changed since.
ConfigFile("kdosrc", "Desk").writeEntry("LaidOut", String(Math.floor(Date.now() / 1000)));
