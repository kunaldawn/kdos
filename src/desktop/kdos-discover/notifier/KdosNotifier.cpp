/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-discover — KdosNotifier: the A/B update in Discover's tray
 * ---------------------------------
 */

#include "KdosNotifier.h"
#include "kdos_notifier_debug.h"

#include <KJob>
#include <KLocalizedString>
#include <KUiServerV2JobTracker>

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTimer>

#include <memory>

using namespace Qt::StringLiterals;

namespace Kdos
{

// The service kdos-sysupdated runs beside org.freedesktop.sysupdate1; its
// contract is src/desktop/kcm-kdos-system/io.kdos.Slots1.xml.
static const auto SlotsService = u"io.kdos.Slots1"_s;
static const auto SlotsPath = u"/io/kdos/Slots1"_s;
static const auto SlotsInterface = u"io.kdos.Slots1"_s;

// `kdos cve` and `kdos-bootctl status` read local files; one that has not
// answered in this long is taken as having said nothing.
static constexpr int ToolTimeoutMs = 30000;

// DISCOVER_TEST_SYSUPDATE is Discover's own switch for putting the sysupdate1
// backend on the session bus, and KDOS_SLOTS1_BUS=session the System Versions
// page's; the plugin follows either, so one kdos-sysupdated started with
// --session serves the whole tray on a test bus.
static QDBusConnection slotsBus()
{
    if (qEnvironmentVariableIntValue("DISCOVER_TEST_SYSUPDATE") || qEnvironmentVariable("KDOS_SLOTS1_BUS") == "session"_L1)
        return QDBusConnection::sessionBus();
    return QDBusConnection::systemBus();
}

static QString updateDocumentPath()
{
    const QString env = qEnvironmentVariable("KDOS_UPDATE_DOC");
    return env.isEmpty() ? u"/var/lib/kdos/update.json"_s : env;
}

// The update being acquired, mirrored into the notification area so it stays
// visible with Discover closed. It does no work of its own.
class UpdateJob : public KJob
{
    Q_OBJECT
public:
    explicit UpdateJob(QObject *parent)
        : KJob(parent)
    {
        setProgressUnit(KJob::Items);
        // Discover's icon and name in the notification, and shown at once:
        // an update is minutes long and its first item can take most of one.
        setProperty("desktopFileName", u"org.kde.discover"_s);
        setProperty("immediateProgressReporting", true);
    }

    void start() override
    {
    }

    void show(const QString &phase, qulonglong done, qulonglong total)
    {
        Q_EMIT description(this, i18nc("@title job name", "Preparing the system update"), {i18nc("@label what the update is doing", "Step"), phase});
        setTotalAmount(KJob::Items, total);
        setProcessedAmount(KJob::Items, done);
    }

    void end()
    {
        emitResult();
    }
};

KdosNotifier::KdosNotifier(QObject *parent)
    : BackendNotifierModule(parent)
    , m_settle(new QTimer(this))
{
    // update.json is renamed into place whole, which changes its directory.
    const QFileInfo doc(updateDocumentPath());
    if (doc.absoluteDir().exists())
        m_watcher.addPath(doc.absolutePath());
    m_settle->setSingleShot(true);
    m_settle->setInterval(500);
    connect(m_settle, &QTimer::timeout, this, [this] {
        readUpdateDocument();
        readSecurity();
    });
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, m_settle, qOverload<>(&QTimer::start));

    qDBusRegisterMetaType<QList<QVariantMap>>();
    slotsBus().connect(SlotsService,
                       SlotsPath,
                       u"org.freedesktop.DBus.Properties"_s,
                       u"PropertiesChanged"_s,
                       this,
                       SLOT(slotsPropertiesChanged(QString, QVariantMap, QStringList)));

    // What is on disk now, so the tray is right before Discover's first
    // recheck, which waits for the session to settle.
    readUpdateDocument();
    readSlots();
}

// A tool still running is ended before any member goes: QObject deletes
// children only after this destructor, and a process ending then would
// deliver its answer to a notifier half gone.
KdosNotifier::~KdosNotifier()
{
    const auto processes = findChildren<QProcess *>(Qt::FindDirectChildrenOnly);
    for (QProcess *p : processes) {
        p->disconnect();
        p->kill();
        p->waitForFinished(1000);
        delete p;
    }
    if (m_job)
        m_job->end();
}

void KdosNotifier::runTool(const QString &program, const QStringList &arguments, std::function<void(const QByteArray &, bool)> done)
{
    auto *p = new QProcess(this);
    auto fired = std::make_shared<bool>(false);
    auto finish = [p, done, fired](bool ok) {
        if (*fired)
            return;
        *fired = true;
        done(ok ? p->readAllStandardOutput() : QByteArray(), ok);
        p->deleteLater();
    };
    QTimer::singleShot(ToolTimeoutMs, p, [p] {
        p->kill();
    });
    connect(p, &QProcess::finished, p, [finish](int, QProcess::ExitStatus how) {
        finish(how == QProcess::NormalExit);
    });
    connect(p, &QProcess::errorOccurred, p, [finish](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart)
            finish(false);
    });
    p->start(program, arguments);
}

void KdosNotifier::recheckSystemUpdateNeeded()
{
    readUpdateDocument();
    readSecurity();
    readSlots();
}

void KdosNotifier::readUpdateDocument()
{
    QFile f(updateDocumentPath());
    m_updateDocument = f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
    recompute();
}

void KdosNotifier::readSecurity()
{
    // `kdos cve` exits 1 when it has findings; the document is what counts.
    runTool(u"kdos"_s, {u"cve"_s, u"--json"_s}, [this](const QByteArray &out, bool) {
        m_cveDocument = out;
        recompute();
    });
}

void KdosNotifier::readSlots()
{
    // With no bus at all there is no reply to wait for: bootctl answers.
    if (!slotsBus().isConnected()) {
        readBootctl();
        return;
    }
    QDBusMessage get = QDBusMessage::createMethodCall(SlotsService, SlotsPath, u"org.freedesktop.DBus.Properties"_s, u"Get"_s);
    get << SlotsInterface << u"State"_s;
    auto *w = new QDBusPendingCallWatcher(slotsBus().asyncCall(get), this);
    connect(w, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        const QDBusPendingReply<QDBusVariant> reply = *w;
        if (!reply.isError()) {
            applySlots(readSlotsState(qdbus_cast<QVariantMap>(reply.value().variant())), true);
            return;
        }
        qCDebug(KDOS_NOTIFIER_LOG) << "io.kdos.Slots1:" << reply.error().message() << "- asking kdos-bootctl";
        readBootctl();
    });
}

void KdosNotifier::readBootctl()
{
    runTool(u"kdos-bootctl"_s, {u"status"_s, u"--json"_s}, [this](const QByteArray &out, bool) {
        applySlots(readSlotsState(QJsonDocument::fromJson(out).object().toVariantMap()), false);
    });
}

void KdosNotifier::slotsPropertiesChanged(const QString &interface, const QVariantMap &changed, const QStringList &invalidated)
{
    if (interface != SlotsInterface)
        return;
    if (changed.contains(u"State"_s))
        applySlots(readSlotsState(qdbus_cast<QVariantMap>(changed.value(u"State"_s))), true);
    else if (invalidated.contains(u"State"_s))
        readSlots();
}

// State names no slot's version. The last ListSlots answer stands in until
// the next one arrives, so an armed update stays pending across a State
// change instead of flickering out and posting its notification again.
void KdosNotifier::applySlots(const SlotsState &state, bool fromDaemon)
{
    const QString runningVersion = m_slots.runningVersion;
    const QString otherVersion = m_slots.otherVersion;
    m_slots = state;
    if (fromDaemon) {
        m_slots.runningVersion = runningVersion;
        m_slots.otherVersion = otherVersion;
    }
    trackJob(state);
    recompute();
    if (fromDaemon && state.rebootRequired)
        readSlotList();
}

void KdosNotifier::readSlotList()
{
    QDBusMessage list = QDBusMessage::createMethodCall(SlotsService, SlotsPath, SlotsInterface, u"ListSlots"_s);
    auto *w = new QDBusPendingCallWatcher(slotsBus().asyncCall(list), this);
    connect(w, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        const QDBusPendingReply<QList<QVariantMap>> reply = *w;
        if (reply.isError()) {
            qCDebug(KDOS_NOTIFIER_LOG) << "io.kdos.Slots1.ListSlots:" << reply.error().message();
            return;
        }
        Kdos::readSlotList(reply.value(), &m_slots);
        recompute();
    });
}

void KdosNotifier::recompute()
{
    const bool updates = m_slots.offerKnown ? !m_slots.offer.isEmpty() : updateOffer(m_updateDocument, m_slots);
    const bool security = updates && securityFixes(m_updateDocument, m_cveDocument) > 0;
    const bool reboot = stagedUpdate(m_slots);
    qCDebug(KDOS_NOTIFIER_LOG) << "updates" << updates << "security" << security << "reboot" << reboot;

    if (updates != m_hasUpdates || security != m_hasSecurity) {
        m_hasUpdates = updates;
        m_hasSecurity = security;
        Q_EMIT foundUpdates();
    }

    // Emitted on both transitions: Discover's tray reads needsReboot() at
    // this signal, and a trial cancelled from System Versions disarms the
    // slot with the session still running, so "Restart to update" has to go
    // as well as come.
    if (reboot != m_needsReboot) {
        m_needsReboot = reboot;
        Q_EMIT needsRebootChanged();
    }
}

// Set on the notifier while Discover was running when the job in State
// began: Discover's own transaction reports that job, and a second one here
// would leave two entries for one update in the notification history. State
// does not name the client that started a job, so one another sysupdate1
// client starts while a Discover window is open is taken for Discover's and
// has no entry.
static const char DiscoverOwnsJob[] = "kdosDiscoverOwnsJob";

static bool discoverRunning()
{
    const QDBusConnectionInterface *bus = QDBusConnection::sessionBus().interface();
    return bus && bus->isServiceRegistered(u"org.kde.discover"_s).value();
}

void KdosNotifier::trackJob(const SlotsState &state)
{
    if (state.job.isEmpty()) {
        // A finished job deletes itself later; the next update is a new one.
        if (m_job)
            m_job->end();
        m_job = nullptr;
        setProperty(DiscoverOwnsJob, QVariant());
        return;
    }
    // Decided when the job is first seen and kept for its length, so one
    // job's entry is never posted twice.
    if (!m_job && !property(DiscoverOwnsJob).isValid())
        setProperty(DiscoverOwnsJob, discoverRunning());
    if (property(DiscoverOwnsJob).toBool())
        return;
    if (!m_job) {
        m_job = new UpdateJob(this);
        auto *tracker = new KUiServerV2JobTracker(m_job);
        tracker->registerJob(m_job);
        m_job->start();
    }
    const QString phase = state.job == "install"_L1 ? i18nc("@info the update's phase", "Arming the other system slot for the next start")
                                                     : i18nc("@info the update's phase", "Downloading and installing into the other system slot");
    m_job->show(phase, state.jobDone, state.jobTotal);
}

bool KdosNotifier::hasUpdates()
{
    return m_hasUpdates;
}

bool KdosNotifier::hasSecurityUpdates()
{
    return m_hasSecurity;
}

bool KdosNotifier::needsReboot() const
{
    return m_needsReboot;
}

}

#include "KdosNotifier.moc"
