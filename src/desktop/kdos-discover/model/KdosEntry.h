/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-discover — one resource as the KDOS tools describe it
 *
 * THE MODEL LIBRARY HOLDS NO LIBDISCOVER TYPE. The parsers and the action plans
 * live here so they are tested on fixtures without a Discover build, and the
 * backend only maps an Entry onto Discover's AbstractResource.
 * ---------------------------------
 */

#pragma once

#include <QString>
#include <QStringList>

namespace Kdos
{

// Where a resource comes from. The lane decides how it is installed and
// removed, and which origin Discover shows for it.
enum class Lane {
    Catalogue, // a catalogue row: built by the store lane, or composed over its pack
    Set, // a catalogue group, installed and removed as one action
    Debian, // a Debian desktop application no catalogue row names, built by the store lane
    Pack, // a pack in the pack store or on the medium that no catalogue row names
    Box, // a box no other resource accounts for
    Port, // a host port: it arrives and leaves with the slot
    PortSource, // a host port the binhost offers, built on this computer instead
    Archive, // a .ktar opened from a file manager or found on the medium
    PackFile, // a .kpack opened from a file manager
};

enum class EntryState {
    None,
    Installed,
    Upgradeable,
};

struct Entry {
    Lane lane = Lane::Catalogue;
    QString id;
    QString name;
    QString summary;
    QString description; // plain text, or AppStream markup when descriptionIsMarkup
    bool descriptionIsMarkup = false;
    QString icon; // a theme icon name or an absolute path
    QStringList categories; // desktop-entry categories and the kdos-* markers
    QString license;
    QString homepage;
    QString installedVersion;
    QString availableVersion;
    quint64 size = 0;
    bool sizeIsEstimate = false;
    EntryState state = EntryState::None;
    QString appstreamId;
    QString desktopId; // the host desktop entry a Port launches
    QStringList screenshots; // local image files only: nothing is fetched
    QString author;
    QStringList mimetypes;
    QStringList packages; // the Debian packages behind a Catalogue or Debian entry
    QStringList members; // what a Set or an Archive installs
    QString kind; // a pack's kind: app, runtime, base or data
    QString packOrigin; // store or medium, for a Pack or a Catalogue row; empty for none
    QString signature; // what kdos-pack verify answered, for a PackFile
    QString path; // the file behind an Archive or a PackFile
    QString box; // the box the resource runs in; empty for none
    QString boxBase; // pack:<id> or image:<ref>
    QString boxState; // podman's state, or "not created"
    QString boxAccent;
    QString boxPersistence;
};

// The origin Discover filters and groups by, and names on the Install
// button. The names are the plan's: a set, a pack in the store and a
// catalogue row whose pack is there are kdos-packs; a pack, or a catalogue
// row whose pack, is seen only on the boot medium is medium; a catalogue
// row with no pack anywhere, which the store lane builds, and the Debian
// archive outside the catalogue are kdos-store. A host port is kdos-ports,
// and the same port built on this computer is kdos-ports-source.
QString originOf(Lane lane, const QString &packOrigin = {});

}
