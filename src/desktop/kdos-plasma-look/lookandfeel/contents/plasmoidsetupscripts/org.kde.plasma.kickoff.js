/*
 * ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   Every Kickoff, however it is added
 *
 * Plasma runs this each time a Kickoff is created: by a desk, by Add
 * Widgets, by a panel template. Without it a Kickoff a user adds by hand
 * shows KDE's logo with no label while the shipped one shows the KDOS mark.
 * The desks that want an icon-only button clear the label after adding.
 * ---------------------------------
 */

applet.currentConfigGroup = ["General"];
applet.writeConfig("icon", "start-here-kdos");
applet.writeConfig("menuLabel", "Start");
applet.reloadConfig();
