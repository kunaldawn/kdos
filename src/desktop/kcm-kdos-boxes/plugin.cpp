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

#include "boxesmodule.h"

#include <KPluginFactory>

K_PLUGIN_FACTORY_WITH_JSON(KdosBoxesFactory, "kcm_kdos_boxes.json", registerPlugin<Kdos::BoxesModule>();)

#include "plugin.moc"
