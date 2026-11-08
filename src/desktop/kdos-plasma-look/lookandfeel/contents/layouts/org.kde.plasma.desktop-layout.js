/*
 * ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   The desktop layout every KDOS global theme applies
 *
 * Plasma runs this at first login and whenever the Global Theme page is told
 * to apply "Desktop and window layout". It lays out the desk the user picked,
 * never a fixed one: the choice is `[Desk] Layout` in kdosrc, which `kdos
 * desk` writes for the user and the installer writes in /etc/xdg, so applying
 * any accent's layout gives back the same panels. An unknown or missing value
 * is the KDOS desk.
 *
 * The desks are layout templates of their own (org.kdos.desk.*), so `kdos
 * desk` can lay one out over a running shell without touching the desktop
 * containments, and this file stays one call wherever it is copied.
 * ---------------------------------
 */

var desks = ["kdos", "dock", "tiling"];
var desk = String(ConfigFile("kdosrc", "Desk").readEntry("Layout") || "");
if (desks.indexOf(desk) < 0) {
    desk = "kdos";
}

// A missing KDOS template must still leave a desktop someone can use.
if (!loadTemplate("org.kdos.desk." + desk)) {
    loadTemplate("org.kde.plasma.desktop.defaultPanel");
}

// The wallpaper plugin only; the image itself is the global theme's default,
// so it follows the accent (and light and dark) with nothing written here.
var desktopsArray = desktopsForActivity(currentActivity());
for (var j = 0; j < desktopsArray.length; j++) {
    desktopsArray[j].wallpaperPlugin = "org.kde.image";
}
