/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-discover — an Entry as Discover's AbstractResource
 *
 * LAUNCHING NEVER GOES THROUGH A SHELL. A boxed application is started from
 * the launcher kdos-appbox genlaunchers wrote for its box, through KIO's
 * launcher job; a box with no application opens Konsole on `kdos-box enter`;
 * a host program is started from its own desktop entry. Each is a program and
 * an argument list.
 * ---------------------------------
 */

#include "KdosResource.h"
#include "KdosBackend.h"

#include <appstream/AppStreamUtils.h>

#include <KConfigGroup>
#include <KDesktopFile>
#include <KFormat>
#include <KIO/ApplicationLauncherJob>
#include <KLocalizedString>
#include <KService>
#include <KShell>

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QIcon>
#include <QProcess>
#include <QStandardPaths>

using namespace Qt::StringLiterals;

namespace Kdos
{

static QString laneName(Lane lane)
{
    switch (lane) {
    case Lane::Catalogue:
        return u"catalogue"_s;
    case Lane::Set:
        return u"set"_s;
    case Lane::Debian:
        return u"debian"_s;
    case Lane::Pack:
        return u"pack"_s;
    case Lane::Box:
        return u"box"_s;
    case Lane::Port:
        return u"port"_s;
    case Lane::Archive:
        return u"archive"_s;
    case Lane::PackFile:
        return u"packfile"_s;
    }
    return {};
}

QString KdosResource::keyOf(const Entry &e)
{
    const bool isFile = e.lane == Lane::Archive || e.lane == Lane::PackFile;
    return laneName(e.lane) + u'/' + (isFile ? e.path : e.id);
}

KdosResource::KdosResource(const Entry &entry, KdosBackend *backend)
    : AbstractResource(backend)
    , m_entry(entry)
{
}

KdosBackend *KdosResource::kdosBackend() const
{
    return static_cast<KdosBackend *>(backend());
}

void KdosResource::setEntry(const Entry &e)
{
    const Entry old = std::exchange(m_entry, e);
    QVector<QByteArray> moved;
    if (old.state != e.state) {
        Q_EMIT stateChanged();
        moved << "state" << "status" << "canUpgrade" << "isInstalled";
    }
    if (old.installedVersion != e.installedVersion || old.availableVersion != e.availableVersion) {
        Q_EMIT versionsChanged();
        moved << "installedVersion" << "availableVersion";
    }
    if (old.size != e.size || old.sizeIsEstimate != e.sizeIsEstimate) {
        Q_EMIT sizeChanged();
        moved << "size" << "sizeDescription";
    }
    if (old.icon != e.icon)
        Q_EMIT iconChanged();
    if (old.description != e.description)
        Q_EMIT longDescriptionChanged();
    if (old.license != e.license)
        Q_EMIT licensesChanged();
    Q_EMIT entryChanged();
    if (!moved.isEmpty())
        Q_EMIT backend()->resourcesChanged(this, moved);
}

QString KdosResource::packageName() const
{
    return m_entry.lane == Lane::Archive || m_entry.lane == Lane::PackFile ? m_entry.path : m_entry.id;
}

QString KdosResource::name() const
{
    return m_entry.name.isEmpty() ? m_entry.id : m_entry.name;
}

QString KdosResource::comment()
{
    return m_entry.summary;
}

QVariant KdosResource::icon() const
{
    if (QDir::isAbsolutePath(m_entry.icon) && QFileInfo::exists(m_entry.icon))
        return QIcon(m_entry.icon);
    if (!m_entry.icon.isEmpty() && QIcon::hasThemeIcon(m_entry.icon))
        return m_entry.icon;
    return u"application-x-executable"_s;
}

AbstractResource::State KdosResource::state()
{
    switch (m_entry.state) {
    case EntryState::None:
        return None;
    case EntryState::Installed:
        return Installed;
    case EntryState::Upgradeable:
        return Upgradeable;
    }
    return Broken;
}

bool KdosResource::hasCategory(const QString &category) const
{
    return m_entry.categories.contains(category);
}

QUrl KdosResource::homepage()
{
    return QUrl(m_entry.homepage);
}

AbstractResource::Type KdosResource::type() const
{
    switch (m_entry.lane) {
    case Lane::Catalogue:
    case Lane::Pack:
        return m_entry.kind.isEmpty() || m_entry.kind == "app"_L1 ? Application : ApplicationSupport;
    case Lane::Set:
    case Lane::Debian:
    case Lane::Box:
    case Lane::Port:
    case Lane::Archive:
    case Lane::PackFile:
        return Application;
    }
    return Application;
}

quint64 KdosResource::size()
{
    return m_entry.size;
}

QString KdosResource::sizeDescription()
{
    if (m_entry.size == 0)
        return i18nc("@info the size of an item", "Unknown");
    const QString bytes = KFormat().formatByteSize(double(m_entry.size));
    return m_entry.sizeIsEstimate ? i18nc("@info an estimated size", "About %1 (an estimate)", bytes) : bytes;
}

QJsonArray KdosResource::licenses()
{
    return m_entry.license.isEmpty() ? QJsonArray() : AppStreamUtils::licenses(m_entry.license);
}

QString KdosResource::installedVersion() const
{
    return m_entry.installedVersion;
}

QString KdosResource::availableVersion() const
{
    return m_entry.availableVersion;
}

QString KdosResource::longDescription()
{
    if (m_entry.descriptionIsMarkup)
        return m_entry.description;
    QString text = m_entry.description.toHtmlEscaped();
    text.replace(u"\n\n"_s, u"</p><p>"_s);
    text.replace(u'\n', u"<br/>"_s);
    return text.isEmpty() ? QString() : u"<p>"_s + text + u"</p>"_s;
}

QString KdosResource::origin() const
{
    return originOf(m_entry.lane, m_entry.packOrigin);
}

QString KdosResource::displayOrigin() const
{
    return KdosBackend::originName(origin());
}

QString KdosResource::section()
{
    for (const QString &c : m_entry.categories) {
        if (!c.startsWith("kdos-"_L1))
            return c;
    }
    return {};
}

QString KdosResource::author() const
{
    return m_entry.author;
}

QStringList KdosResource::mimetypes() const
{
    return m_entry.mimetypes;
}

QList<PackageState> KdosResource::addonsInformation()
{
    return {};
}

QString KdosResource::appstreamId() const
{
    return m_entry.appstreamId;
}

QUrl KdosResource::url() const
{
    if (m_entry.lane == Lane::Archive || m_entry.lane == Lane::PackFile)
        return QUrl::fromLocalFile(m_entry.path);
    if (!m_entry.appstreamId.isEmpty())
        return QUrl(u"appstream://"_s + m_entry.appstreamId);
    QUrl u;
    u.setScheme(backend()->name());
    u.setHost(laneName(m_entry.lane));
    u.setPath(u'/' + m_entry.id);
    return u;
}

QString KdosResource::sourceIcon() const
{
    const QString o = origin();
    if (o == "kdos-store"_L1)
        return u"application-vnd.debian.binary-package"_s;
    if (o == "kdos-boxes"_L1)
        return u"utilities-terminal"_s;
    if (o == "kdos-ports"_L1)
        return u"start-here-kdos"_s;
    if (o == "medium"_L1)
        return u"media-optical"_s;
    if (o == "local"_L1)
        return u"document-open"_s;
    return u"package-x-generic"_s;
}

QDate KdosResource::releaseDate() const
{
    return {};
}

bool KdosResource::isRemovable() const
{
    switch (m_entry.lane) {
    case Lane::Port:
    case Lane::Archive:
    case Lane::PackFile:
        return false;
    default:
        return true;
    }
}

// The launcher genlaunchers wrote for this box: `Exec=kdos-appbox -b <box> run
// ...` in the user's applications directory, marked X-KDOS-Alien.
static KService::Ptr boxedLauncher(const QString &box)
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
    QDirIterator it(dir, {u"*.desktop"_s}, QDir::Files);
    while (it.hasNext()) {
        const QString path = it.next();
        KDesktopFile df(path);
        const KConfigGroup g = df.desktopGroup();
        if (!g.readEntry("X-KDOS-Alien", false) || df.noDisplay())
            continue;
        const QStringList args = KShell::splitArgs(g.readEntry("Exec"));
        if (args.size() >= 3 && args[0] == "kdos-appbox"_L1 && args[1] == "-b"_L1 && args[2] == box)
            return KService::Ptr(new KService(path));
    }
    return {};
}

bool KdosResource::canExecute() const
{
    if (m_entry.state == EntryState::None)
        return false;
    if (m_entry.lane == Lane::Box)
        return !QStandardPaths::findExecutable(u"konsole"_s).isEmpty();
    if (m_entry.lane == Lane::Port)
        return !m_entry.desktopId.isEmpty();
    return !m_entry.box.isEmpty() && m_entry.kind != "data"_L1;
}

void KdosResource::invokeApplication() const
{
    if (m_entry.lane == Lane::Box) {
        // `enter` would open a terminal window of its own when it finds one;
        // KDOS_BOX_NOTERM makes it run in the Konsole started here.
        QProcess p;
        p.setProgram(u"konsole"_s);
        p.setArguments({u"-e"_s, u"kdos-box"_s, u"enter"_s, m_entry.id});
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(u"KDOS_BOX_NOTERM"_s, u"1"_s);
        p.setProcessEnvironment(env);
        p.startDetached();
        return;
    }

    KService::Ptr service;
    if (m_entry.lane == Lane::Port) {
        QString id = m_entry.desktopId;
        if (id.endsWith(".desktop"_L1))
            id.chop(8);
        service = KService::serviceByDesktopName(id);
    } else {
        service = boxedLauncher(m_entry.box);
    }
    if (!service) {
        Q_EMIT backend()->passiveMessage(i18n("%1 has no launcher yet. Run “kdos-appbox genlaunchers --packs --user” and try again.", name()));
        return;
    }
    auto *job = new KIO::ApplicationLauncherJob(service);
    job->start();
}

QString KdosResource::executeLabel() const
{
    if (m_entry.lane == Lane::Box)
        return i18nc("@action:button", "Open Terminal");
    return AbstractResource::executeLabel();
}

QStringList KdosResource::topObjects() const
{
    return {u"qrc:/kdos/KdosNotice.qml"_s};
}

QStringList KdosResource::bottomObjects() const
{
    return {u"qrc:/kdos/KdosBox.qml"_s, u"qrc:/kdos/KdosMembers.qml"_s};
}

// The badge beside the author on the application page: a pack whose
// signature verified, and every resource that runs in a box, named.
static bool signatureVerified(const Entry &e)
{
    return e.lane == Lane::PackFile && !e.signature.isEmpty() && e.signature != "unsigned"_L1 && e.signature != "refused"_L1;
}

QString KdosResource::verifiedMessage() const
{
    QStringList said;
    if (signatureVerified(m_entry))
        said << i18n("Signed by %1, verified on this computer.", m_entry.signature);
    if (!m_entry.box.isEmpty())
        said << i18n("Runs in the box “%1”, apart from the rest of the system.", m_entry.box);
    return said.join(u' ');
}

QString KdosResource::verifiedIconName() const
{
    return signatureVerified(m_entry) ? u"security-high"_s : u"security-medium"_s;
}

QString KdosResource::kdosNotice() const
{
    const Entry &e = m_entry;
    switch (e.lane) {
    case Lane::Catalogue:
        if (e.state != EntryState::None)
            return {};
        if (e.packOrigin == "store"_L1)
            return i18n("Installs from the pack already in the pack store; no network connection is needed.");
        if (e.packOrigin == "medium"_L1)
            return i18n("Installs from the pack on the KDOS medium; no network connection is needed.");
        return i18n("Built on this computer from Debian packages, in a box of its own. This needs a network connection, and the size is an estimate.");
    case Lane::Debian:
        if (e.state != EntryState::None)
            return {};
        return i18n("Built on this computer from the Debian package %1, in a box of its own. This needs a network connection.", e.packages.value(0));
    case Lane::Set:
        return i18np("Installs one application from the KDOS catalogue.", "Installs %1 applications from the KDOS catalogue, one after another.", int(e.members.size()));
    case Lane::Pack:
        if (e.packOrigin == "medium"_L1)
            return i18n("On the KDOS medium. Installing copies it into the pack store, where kdos-packd checks it.");
        if (e.state == EntryState::Upgradeable)
            return i18n("The KDOS medium carries a newer version, %1.", e.availableVersion);
        return {};
    case Lane::Box:
        return i18n("Removing a box keeps its profile and the files it wrote under ~/.local/share/kdos/boxes/%1.", e.id);
    case Lane::Port:
        if (e.state == EntryState::None)
            return i18n("Downloads the prebuilt package from the signed KDOS binary host into this version of the system, and the next system update keeps it. This needs a network connection; nothing is built.");
        return i18n("Part of the KDOS system: it is updated with the system and is not removed on its own.");
    case Lane::Archive:
        if (e.signature == "signed-index"_L1)
            return i18n("The set's index is signed; kdos-packd checks every pack against it before installing.");
        return i18n("The set's index is not signed. Its packs install as unsigned, and a computer set to require signatures refuses them.");
    case Lane::PackFile:
        if (e.signature == "unsigned"_L1)
            return i18n("This pack is not signed. It installs as unsigned, and a computer set to require signatures refuses it.");
        if (e.signature == "refused"_L1)
            return i18n("kdos-pack refused this pack: its payload or its signature does not verify.");
        return {};
    }
    return {};
}

int KdosResource::kdosNoticeType() const
{
    const Entry &e = m_entry;
    if (e.lane == Lane::PackFile && e.signature == "refused"_L1)
        return InlineMessage::Error;
    if ((e.lane == Lane::PackFile && e.signature == "unsigned"_L1) || (e.lane == Lane::Archive && e.signature != "signed-index"_L1))
        return InlineMessage::Warning;
    if (e.lane == Lane::Archive || e.lane == Lane::PackFile)
        return InlineMessage::Positive;
    return InlineMessage::Information;
}

QStringList KdosResource::memberNames() const
{
    QStringList names;
    const PlanContext ctx = kdosBackend()->planContext();
    for (const QString &id : m_entry.members) {
        const QString shown = ctx.catalogue ? ctx.catalogue->rows.value(id).name : QString();
        names << (shown.isEmpty() ? id : shown);
    }
    return names;
}

void KdosResource::fetchBoxGrants()
{
    if (!m_entry.box.isEmpty())
        kdosBackend()->readBoxGrants(this);
}

// The permissions the page shows, in the order the Boxes settings page
// lists them, each said as what it means for the person rather than as
// the podman flag; a key kdos-box did not print is left out.
void KdosResource::setBoxGrants(const BoxGrants &g)
{
    auto said = [](const QString &key, const QString &v) -> QString {
        if (key == "network"_L1) {
            if (v == "host"_L1)
                return i18nc("@info a box's network", "The computer's own network");
            if (v == "private"_L1)
                return i18nc("@info a box's network", "A network of its own, out to the internet");
            if (v == "none"_L1)
                return i18nc("@info a box's network", "None");
        }
        if (key == "home"_L1) {
            if (v == "shared"_L1)
                return i18nc("@info a box's files", "Your whole home folder");
            if (v == "private"_L1)
                return i18nc("@info a box's files", "A home folder of its own");
        }
        if (key == "devices"_L1) {
            if (v == "shared"_L1)
                return i18nc("@info a box's devices", "Every device of the computer");
            if (v == "private"_L1)
                return i18nc("@info a box's devices", "None beyond its own");
        }
        if (key == "grant"_L1 && v == "none"_L1)
            return i18nc("@info a box's desktop grants", "None");
        if (v == "yes"_L1)
            return i18nc("@info a permission is given", "Allowed");
        if (v == "no"_L1)
            return i18nc("@info a permission is not given", "Not allowed");
        if (v == "auto"_L1)
            return i18nc("@info a permission follows the application", "When the application asks for it");
        if (v == "unlimited"_L1 || v == "all"_L1)
            return i18nc("@info a resource has no limit", "No limit");
        return v;
    };
    const QList<std::pair<QString, QString>> shown{
        {u"network"_s, i18nc("@label a box's permission", "Network:")},
        {u"home"_s, i18nc("@label a box's permission", "Files:")},
        {u"devices"_s, i18nc("@label a box's permission", "Devices:")},
        {u"gpu"_s, i18nc("@label a box's permission", "Graphics acceleration:")},
        {u"wayland"_s, i18nc("@label a box's permission", "Windows on the desktop:")},
        {u"x11"_s, i18nc("@label a box's permission", "X11 windows:")},
        {u"audio"_s, i18nc("@label a box's permission", "Sound:")},
        {u"grant"_s, i18nc("@label a box's permission", "Desktop grants:")},
        {u"memory"_s, i18nc("@label a box's limit", "Memory:")},
        {u"cpus"_s, i18nc("@label a box's limit", "Processors:")},
    };
    QVariantList rows;
    for (const auto &[key, label] : shown) {
        const auto v = g.values.constFind(key);
        if (v == g.values.constEnd())
            continue;
        rows << QVariantMap{{u"label"_s, label}, {u"value"_s, said(key, *v)}};
    }
    m_grants = rows;
    m_grantWarnings = g.warnings;
    Q_EMIT boxGrantsChanged();
}

void KdosResource::fetchChangelog()
{
    Q_EMIT changelogFetched({});
}

void KdosResource::fetchScreenshots()
{
    Screenshots shots;
    for (const QString &path : m_entry.screenshots)
        shots << Screenshot(QUrl::fromLocalFile(path));
    Q_EMIT screenshotsFetched(shots);
}

}
