/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-discover — the factory Discover's loader asks for KdosBackend
 *
 * Alone in its file and only in the plugin, so the backend's own code builds
 * into a library the tests link without a second plugin instance in them.
 * The interface id carries discover's full version: Discover skips a plugin
 * built against any other release.
 * ---------------------------------
 */

#include "KdosBackend.h"

namespace Kdos
{
DISCOVER_BACKEND_PLUGIN(KdosBackend)
}

#include "KdosPlugin.moc"
