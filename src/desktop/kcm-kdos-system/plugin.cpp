/*
 * ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   KD's Homebrew Linux Distro
 * ---------------------------------
 *
 *
 * The plugin's entry point and nothing else, so the module itself links
 * into the tests without a second plugin factory.
 */

#include "systemversions.h"

#include <KPluginFactory>

K_PLUGIN_FACTORY_WITH_JSON(KdosSystemVersionsFactory, "kcm_kdos_system.json", registerPlugin<Kdos::SystemVersionsModule>();)

#include "plugin.moc"
