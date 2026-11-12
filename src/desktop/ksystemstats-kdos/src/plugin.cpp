/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   ksystemstats-kdos — the `kdos` sensors of System Monitor
 */

#include "plugin.h"

#include <KConfigGroup>
#include <KLocalizedString>
#include <KPluginFactory>

#include <QDeadlineTimer>
#include <QFileInfo>
#include <QLocalSocket>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QTimer>

#include <unistd.h>

namespace Kdos
{

namespace
{
/* kdos-energyd samples every ten seconds; asking more often only repeats the
 * last ledger. With nothing subscribed the ledger is still read, more rarely,
 * because an application's sensors exist only once a report has named it, and
 * a face that asks for `kdos/energy/.*` can match only sensors that exist. */
constexpr qint64 EnergyIntervalMs = 10 * 1000;
constexpr qint64 EnergyIdleIntervalMs = 60 * 1000;
/* A daemon that has not answered in this long is not going to. */
constexpr int EnergyTimeoutMs = 3 * 1000;
/* The reply is a few hundred bytes per application. */
constexpr qint64 EnergyMaxReply = 1024 * 1024;
/* The one report read while the daemon starts. */
constexpr qint64 EnergyFirstReadMs = 1000;
/* Launchers change when a box is installed or removed, not between samples. */
constexpr qint64 LauncherIntervalMs = 30 * 1000;

QString envOr(const char *name, const QString &fallback)
{
    const QString v = qEnvironmentVariable(name);
    return v.isEmpty() ? fallback : v;
}

KSysGuard::SensorProperty *makeProperty(KSysGuard::SensorObject *o, const QString &id, const QString &name, KSysGuard::Unit unit, QMetaType::Type type)
{
    auto *p = new KSysGuard::SensorProperty(id, name, o);
    p->setUnit(unit);
    p->setVariantType(QVariant::Type(type));
    return p;
}

/* A string sensor's unit is UnitInvalid: libksysguard's formatter shows only
 * that unit's value as text, and formats UnitNone as a number, so a string
 * under it reads "0". */
KSysGuard::SensorProperty *makeText(KSysGuard::SensorObject *o, const QString &id, const QString &name)
{
    return makeProperty(o, id, name, KSysGuard::UnitInvalid, QMetaType::QString);
}

KSysGuard::AggregateSensor *makeTotal(KSysGuard::SensorObject *all, const QString &id, const QString &name, KSysGuard::Unit unit)
{
    auto *a = new KSysGuard::AggregateSensor(all, id, name);
    a->setUnit(unit);
    a->setVariantType(QVariant::Double);
    a->setMatchSensors(QRegularExpression(QStringLiteral("^box/(?!all$).+$")), id);
    return a;
}

/* The object a removed box or application leaves behind: off the container
 * first, so the daemon tells its clients the sensors are gone, then freed. */
void retire(KSysGuard::SensorContainer *c, KSysGuard::SensorObject *o)
{
    c->removeObject(o);
    o->deleteLater();
}

QString slotLabel(const QString &state)
{
    if (state == QLatin1String("trial")) {
        return i18nc("@info state of a system slot", "on trial");
    }
    if (state == QLatin1String("armed")) {
        return i18nc("@info state of a system slot", "starts at the next restart");
    }
    if (state == QLatin1String("running") || state == QLatin1String("confirmed")) {
        return i18nc("@info state of a system slot", "confirmed");
    }
    if (state == QLatin1String("failed")) {
        return i18nc("@info state of a system slot", "failed to start");
    }
    if (state == QLatin1String("empty")) {
        return i18nc("@info state of a system slot", "empty");
    }
    if (state == QLatin1String("inactive")) {
        return i18nc("@info state of a system slot", "previous version");
    }
    return state;
}
} // namespace

/* ── one box ───────────────────────────────────────────────────────────── */

BoxObject::BoxObject(const QString &boxName, KSysGuard::SensorContainer *parent)
    : KSysGuard::SensorObject(QStringLiteral("box/") + boxName, i18nc("@title a box's sensors", "Box %1", boxName), parent)
    , box(boxName)
{
    m_name = makeText(this, QStringLiteral("name"), i18nc("@title", "Name"));
    m_name->setValue(i18nc("@info a box with no launcher", "Box %1", boxName));
    m_icon = makeText(this, QStringLiteral("icon"), i18nc("@title", "Icon"));
    m_icon->setValue(QStringLiteral("kdos-box"));

    m_cpu = makeProperty(this, QStringLiteral("cpu"), i18nc("@title", "CPU Usage"), KSysGuard::UnitPercent, QMetaType::Double);
    m_cpu->setShortName(i18nc("@title short for CPU usage", "CPU"));
    m_cpu->setDescription(i18nc("@info", "Share of all processors the box's processes used since the last sample"));
    m_cpu->setMax(100);

    m_memory = makeProperty(this, QStringLiteral("memory"), i18nc("@title", "Memory"), KSysGuard::UnitByte, QMetaType::LongLong);
    m_memory->setDescription(i18nc("@info", "Memory charged to the box's cgroup, page cache included, or its processes' proportional share when it has no cgroup"));
    m_memoryLimit = makeProperty(this, QStringLiteral("memoryLimit"), i18nc("@title", "Memory Limit"), KSysGuard::UnitByte, QMetaType::LongLong);
    m_memoryLimit->setDescription(i18nc("@info", "The box's memory limit from its profile; 0 when it has none"));
    m_memory->setMax(m_memoryLimit);

    m_ioRead = makeProperty(this, QStringLiteral("ioRead"), i18nc("@title", "Disk Read Rate"), KSysGuard::UnitByteRate, QMetaType::Double);
    m_ioRead->setShortName(i18nc("@title short for disk read rate", "Read"));
    m_ioWrite = makeProperty(this, QStringLiteral("ioWrite"), i18nc("@title", "Disk Write Rate"), KSysGuard::UnitByteRate, QMetaType::Double);
    m_ioWrite->setShortName(i18nc("@title short for disk write rate", "Write"));

    m_processes = makeProperty(this, QStringLiteral("processes"), i18nc("@title", "Processes"), KSysGuard::UnitNone, QMetaType::Int);
    m_energy = makeProperty(this, QStringLiteral("energy"), i18nc("@title", "Energy Impact"), KSysGuard::UnitPercent, QMetaType::Double);
    m_energy->setDescription(i18nc("@info", "The box's share of the energy kdos-energyd could attribute"));
    m_energy->setMax(100);
}

void BoxObject::setLauncher(const BoxLauncher &launcher)
{
    if (!launcher.name.isEmpty()) {
        const QString label = i18nc("@info %1 application, %2 box", "%1 (box %2)", launcher.name, box);
        m_name->setValue(label);
        setName(label);
    }
    if (!launcher.icon.isEmpty()) {
        m_icon->setValue(launcher.icon);
    }
}

void BoxObject::setEnergy(double fraction)
{
    m_energy->setValue(fraction * 100.0);
}

void BoxObject::sample(const CgroupCounters &now, qint64 nowMs, int cpus)
{
    if (now.memoryBytes >= 0) {
        m_memory->setValue(now.memoryBytes);
    }
    if (now.memoryMaxBytes >= 0) {
        m_memoryLimit->setValue(now.memoryMaxBytes);
    }
    if (now.processes >= 0) {
        m_processes->setValue(int(now.processes));
    }
    if (m_lastMs >= 0 && nowMs > m_lastMs) {
        const double secs = double(nowMs - m_lastMs) / 1000.0;
        /* A counter that went backwards is a box restarted under the same
         * name between two samples, or a box counted by its processes that
         * lost one: a fresh start, not negative use, and no rate this time. */
        if (now.cpuUsec >= 0 && m_last.cpuUsec >= 0 && now.cpuUsec >= m_last.cpuUsec) {
            const double busy = double(now.cpuUsec - m_last.cpuUsec) / 1e6;
            m_cpu->setValue(qBound(0.0, busy / (secs * qMax(1, cpus)) * 100.0, 100.0));
        }
        if (now.readBytes >= 0 && m_last.readBytes >= 0 && now.readBytes >= m_last.readBytes) {
            m_ioRead->setValue(double(now.readBytes - m_last.readBytes) / secs);
        }
        if (now.writeBytes >= 0 && m_last.writeBytes >= 0 && now.writeBytes >= m_last.writeBytes) {
            m_ioWrite->setValue(double(now.writeBytes - m_last.writeBytes) / secs);
        }
    }
    m_last = now;
    m_lastMs = nowMs;
}

/* ── one application's energy ──────────────────────────────────────────── */

EnergyObject::EnergyObject(const EnergyApp &app, KSysGuard::SensorContainer *parent)
    : KSysGuard::SensorObject(QStringLiteral("energy/") + app.id, app.name, parent)
{
    m_name = makeText(this, QStringLiteral("name"), i18nc("@title", "Name"));
    m_app = makeText(this, QStringLiteral("app"), i18nc("@title", "Application"));
    m_box = makeText(this, QStringLiteral("box"), i18nc("@title", "Box"));
    m_impact = makeProperty(this, QStringLiteral("impact"), i18nc("@title", "Energy Impact"), KSysGuard::UnitPercent, QMetaType::Double);
    m_impact->setDescription(i18nc("@info", "Share of the energy used above the idle floor over the daemon's window"));
    m_impact->setMax(100);
    m_cpuTime = makeProperty(this, QStringLiteral("cpuTime"), i18nc("@title", "Processor Time"), KSysGuard::UnitTime, QMetaType::Double);
    m_gpu = makeProperty(this, QStringLiteral("gpu"), i18nc("@title", "GPU Share"), KSysGuard::UnitPercent, QMetaType::Double);
    m_gpu->setMax(100);
}

void EnergyObject::set(const EnergyApp &app, const QString &label)
{
    setName(label);
    m_name->setValue(label);
    m_app->setValue(app.app);
    m_box->setValue(app.box);
    m_impact->setValue(app.impact * 100.0);
    m_cpuTime->setValue(app.cpuSeconds);
    if (app.gpuShare >= 0) {
        m_gpu->setValue(app.gpuShare * 100.0);
    }
}

/* ── the plugin ────────────────────────────────────────────────────────── */

KdosPlugin::KdosPlugin(QObject *parent, const QVariantList &args)
    : KSysGuard::SensorPlugin(parent, args)
    , m_container(new KSysGuard::SensorContainer(QStringLiteral("kdos"), i18nc("@title", "KDOS"), this))
    , m_cgroupRoot(envOr("KDOS_CGROUP_ROOT", QStringLiteral("/sys/fs/cgroup")))
    , m_procRoot(envOr("KDOS_PROC_ROOT", QStringLiteral("/proc")))
    , m_energySocket(envOr("KDOS_ENERGYD_SOCKET", QStringLiteral("/run/kdos-energyd.sock")))
    , m_slotFile(envOr("KDOS_BOOTSTATE_PUB", QStringLiteral("/run/kdos/bootstate.json")))
{
    /* A node of the sensor tree takes its title only from an object at its
     * exact path, and `box/<box>` and `energy/<app>` make `kdos/box` and
     * `kdos/energy` nodes: without these two, sensorless objects, the picker
     * shows those paths as the groups' names. */
    new KSysGuard::SensorObject(QStringLiteral("box"), i18nc("@title", "Boxes"), m_container);
    new KSysGuard::SensorObject(QStringLiteral("energy"), i18nc("@title", "Energy Impact"), m_container);

    m_allBoxes = new KSysGuard::SensorObject(QStringLiteral("box/all"), i18nc("@title", "All Boxes"), m_container);
    m_boxCount = makeProperty(m_allBoxes, QStringLiteral("boxes"), i18nc("@title", "Running Boxes"), KSysGuard::UnitNone, QMetaType::Int);
    m_boxCount->setValue(0);
    auto *cpu = makeTotal(m_allBoxes, QStringLiteral("cpu"), i18nc("@title", "CPU Usage"), KSysGuard::UnitPercent);
    cpu->setMax(100);
    makeTotal(m_allBoxes, QStringLiteral("memory"), i18nc("@title", "Memory"), KSysGuard::UnitByte);
    makeTotal(m_allBoxes, QStringLiteral("ioRead"), i18nc("@title", "Disk Read Rate"), KSysGuard::UnitByteRate);
    makeTotal(m_allBoxes, QStringLiteral("ioWrite"), i18nc("@title", "Disk Write Rate"), KSysGuard::UnitByteRate);
    makeTotal(m_allBoxes, QStringLiteral("processes"), i18nc("@title", "Processes"), KSysGuard::UnitNone);

    m_energyAll = new KSysGuard::SensorObject(QStringLiteral("energy/all"), i18nc("@title", "Energy"), m_container);
    m_energyStatus = makeText(m_energyAll, QStringLiteral("status"), i18nc("@title", "Status"));
    m_energyStatus->setValue(i18nc("@info", "Waiting for kdos-energyd"));
    m_energyWindow = makeProperty(m_energyAll, QStringLiteral("window"), i18nc("@title", "Sampled Time"), KSysGuard::UnitTime, QMetaType::Double);
    m_energyFloor = makeProperty(m_energyAll, QStringLiteral("idleFloor"), i18nc("@title", "Idle Floor"), KSysGuard::UnitWatt, QMetaType::Double);
    m_energyAttributable =
        makeProperty(m_energyAll, QStringLiteral("attributable"), i18nc("@title", "Attributable Energy"), KSysGuard::UnitPercent, QMetaType::Double);
    m_energyAttributable->setMax(100);
    m_energyShortLived =
        makeProperty(m_energyAll, QStringLiteral("shortLived"), i18nc("@title", "Short-lived Processes"), KSysGuard::UnitPercent, QMetaType::Double);
    m_energyShortLived->setMax(100);
    m_energyDomains = makeText(m_energyAll, QStringLiteral("domains"), i18nc("@title", "Measured Domains"));

    m_slot = new KSysGuard::SensorObject(QStringLiteral("slot"), i18nc("@title", "System Slots"), m_container);
    m_slotStatus = makeText(m_slot, QStringLiteral("status"), i18nc("@title", "Status"));
    m_slotRunning = makeText(m_slot, QStringLiteral("running"), i18nc("@title", "Running Slot"));
    m_slotRunningVersion = makeText(m_slot, QStringLiteral("runningVersion"), i18nc("@title", "Running Version"));
    m_slotRunningState = makeText(m_slot, QStringLiteral("runningState"), i18nc("@title", "Running Slot State"));
    m_slotOther = makeText(m_slot, QStringLiteral("other"), i18nc("@title", "Other Slot"));
    m_slotOtherVersion = makeText(m_slot, QStringLiteral("otherVersion"), i18nc("@title", "Other Slot Version"));
    m_slotOtherState = makeText(m_slot, QStringLiteral("otherState"), i18nc("@title", "Other Slot State"));
    m_slotNext = makeText(m_slot, QStringLiteral("next"), i18nc("@title", "Next Boot"));
    m_slotTrial = makeProperty(m_slot, QStringLiteral("trial"), i18nc("@title", "On Trial"), KSysGuard::UnitNone, QMetaType::Int);
    m_slotAttempts = makeProperty(m_slot, QStringLiteral("attempts"), i18nc("@title", "Trial Boots Left"), KSysGuard::UnitNone, QMetaType::Int);

    m_look = new KSysGuard::SensorObject(QStringLiteral("look"), i18nc("@title", "Look"), m_container);
    m_phosphor = makeProperty(m_look, QStringLiteral("phosphor"), i18nc("@title", "Phosphor Mode"), KSysGuard::UnitNone, QMetaType::Int);
    m_phosphor->setDescription(i18nc("@info", "1 while Phosphor mode's CRT pass is switched on, else 0"));
    m_phosphor->setValue(0);
    m_kwinrc = KSharedConfig::openConfig(QStringLiteral("kwinrc"), KConfig::NoGlobals);
    m_kwinrcPath = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QLatin1String("/kwinrc");

    /* A client that lists the sensors before the first sample still sees the
     * boxes that are running. */
    updateBoxes(QDateTime::currentMSecsSinceEpoch());
    updateSlots();
    updatePhosphor();
    readEnergyNow();
}

KdosPlugin::~KdosPlugin() = default;

/* An application's energy sensors exist only once a report has named them,
 * and ksystemstats starts when its first client asks for sensors by name, so
 * the first report is read before the daemon answers anyone. The daemon
 * answers from memory at once; the wait is bounded for one that does not. */
void KdosPlugin::readEnergyNow()
{
    QDeadlineTimer deadline(EnergyFirstReadMs);
    QLocalSocket sock;
    QByteArray reply;
    sock.connectToServer(m_energySocket);
    if (!sock.waitForConnected(int(deadline.remainingTime()))) {
        EnergyReport r;
        r.error = i18nc("@info", "kdos-energyd is not running");
        applyEnergy(r);
        m_energyAge.start();
        return;
    }
    sock.write("report-json\n");
    sock.waitForBytesWritten(int(deadline.remainingTime()));
    while (!deadline.hasExpired() && reply.size() <= EnergyMaxReply) {
        if (!sock.waitForReadyRead(int(deadline.remainingTime()))) {
            break;
        }
        reply += sock.readAll();
    }
    reply += sock.readAll();
    applyEnergy(kdosParseEnergyReport(reply));
    m_energyAge.start();
}

void KdosPlugin::update()
{
    updateBoxes(QDateTime::currentMSecsSinceEpoch());
    updateSlots();
    updatePhosphor();
    const qint64 every = energyWanted() ? EnergyIntervalMs : EnergyIdleIntervalMs;
    if (!m_energyAge.isValid() || m_energyAge.elapsed() >= every) {
        requestEnergy();
    }
}

void KdosPlugin::updateBoxes(qint64 nowMs)
{
    const QList<BoxScope> scopes = kdosFindBoxScopes(m_cgroupRoot, ::getuid());
    QHash<QString, CgroupCounters> byBox;
    for (const BoxScope &s : scopes) {
        const CgroupCounters c = kdosReadCgroup(s.path);
        if (!c.present) {
            continue;
        }
        byBox.insert(s.box, byBox.contains(s.box) ? kdosAddCounters(byBox.value(s.box), c) : c);
    }
    /* A box with no scope — every box where elogind keeps the session's
     * cgroup.procs root's — is found by its processes instead. */
    const QHash<QString, CgroupCounters> byProcess = kdosBoxesByProcess(m_procRoot, QStringLiteral("self"), ::getuid(), byBox.keys());
    for (auto it = byProcess.cbegin(); it != byProcess.cend(); ++it) {
        byBox.insert(it.key(), it.value());
    }

    bool added = false;
    for (auto it = m_boxes.begin(); it != m_boxes.end();) {
        if (!byBox.contains(it.key())) {
            retire(m_container, it.value());
            it = m_boxes.erase(it);
        } else {
            ++it;
        }
    }
    for (auto it = byBox.cbegin(); it != byBox.cend(); ++it) {
        if (!m_boxes.contains(it.key())) {
            m_boxes.insert(it.key(), new BoxObject(it.key(), m_container));
            added = true;
        }
    }

    if (added || !m_launcherAge.isValid() || m_launcherAge.elapsed() >= LauncherIntervalMs) {
        QStringList dirs = QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, QStringLiteral("applications"), QStandardPaths::LocateDirectory);
        m_launchers = kdosBoxLaunchers(dirs);
        m_launcherAge.start();
        for (BoxObject *b : std::as_const(m_boxes)) {
            b->setLauncher(m_launchers.value(b->box));
        }
    }

    const int cpus = int(qMax(1L, ::sysconf(_SC_NPROCESSORS_ONLN)));
    for (BoxObject *b : std::as_const(m_boxes)) {
        b->sample(byBox.value(b->box), nowMs, cpus);
        b->setEnergy(m_boxEnergy.value(b->box));
    }
    m_boxCount->setValue(int(m_boxes.size()));
}

void KdosPlugin::updateSlots()
{
    const QFileInfo fi(m_slotFile);
    const QDateTime stamp = fi.exists() ? fi.lastModified() : QDateTime();
    if (m_slotRead && stamp == m_slotStamp) {
        return;
    }
    m_slotRead = true;
    m_slotStamp = stamp;

    SlotState s;
    QFile f(m_slotFile);
    if (f.open(QIODevice::ReadOnly)) {
        s = kdosParseSlotState(f.read(64 * 1024));
    }
    if (!s.configured) {
        m_slotStatus->setValue(i18nc("@info", "This machine boots one system; it has no A/B slots"));
        for (auto *p : {m_slotRunning, m_slotRunningVersion, m_slotRunningState, m_slotOther, m_slotOtherVersion, m_slotOtherState, m_slotNext}) {
            p->setValue(QString());
        }
        m_slotTrial->setValue(0);
        m_slotAttempts->setValue(0);
        return;
    }
    m_slotRunning->setValue(s.running);
    m_slotRunningVersion->setValue(s.runningVersion);
    m_slotRunningState->setValue(slotLabel(s.runningState));
    m_slotOther->setValue(s.other);
    m_slotOtherVersion->setValue(s.otherVersion);
    m_slotOtherState->setValue(slotLabel(s.otherState));
    m_slotNext->setValue(s.next);
    m_slotTrial->setValue(s.trial ? 1 : 0);
    m_slotAttempts->setValue(s.trial ? s.attempts : 0);
    if (s.trial) {
        m_slotStatus->setValue(i18ncp("@info", "Slot %2 is on trial: %1 boot left before it is given up",
                                      "Slot %2 is on trial: %1 boots left before it is given up", s.attempts, s.running));
    } else if (s.running.isEmpty()) {
        m_slotStatus->setValue(i18nc("@info", "The running slot is not known"));
    } else {
        m_slotStatus->setValue(i18nc("@info", "Running slot %1, confirmed", s.running));
    }
}

void KdosPlugin::updatePhosphor()
{
    const QFileInfo fi(m_kwinrcPath);
    const QDateTime stamp = fi.exists() ? fi.lastModified() : QDateTime();
    if (m_kwinrcRead && stamp == m_kwinrcStamp) {
        return;
    }
    if (m_kwinrcRead) {
        m_kwinrc->reparseConfiguration();
    }
    m_kwinrcRead = true;
    m_kwinrcStamp = stamp;
    m_phosphor->setValue(m_kwinrc->group(QStringLiteral("Plugins")).readEntry("kdos_crtEnabled", false) ? 1 : 0);
}

bool KdosPlugin::energyWanted() const
{
    if (m_energyAll->isSubscribed()) {
        return true;
    }
    for (const EnergyObject *e : std::as_const(m_energy)) {
        if (e->isSubscribed()) {
            return true;
        }
    }
    for (const BoxObject *b : std::as_const(m_boxes)) {
        if (b->sensor(QStringLiteral("energy"))->isSubscribed()) {
            return true;
        }
    }
    return false;
}

void KdosPlugin::requestEnergy()
{
    if (m_energyRequest) {
        return;
    }
    m_energyAge.start();
    m_energyReply.clear();

    auto *sock = new QLocalSocket(this);
    m_energyRequest = sock;

    auto fail = [this, sock](const QString &why) {
        if (m_energyRequest != sock) {
            return;
        }
        m_energyRequest.clear();
        sock->abort();
        sock->deleteLater();
        EnergyReport r;
        r.error = why;
        applyEnergy(r);
    };

    connect(sock, &QLocalSocket::connected, this, [sock]() {
        sock->write("report-json\n");
    });
    connect(sock, &QLocalSocket::readyRead, this, [this, sock, fail]() {
        m_energyReply += sock->readAll();
        if (m_energyReply.size() > EnergyMaxReply) {
            fail(i18nc("@info", "kdos-energyd sent more than a report"));
        }
    });
    connect(sock, &QLocalSocket::disconnected, this, [this, sock]() {
        if (m_energyRequest != sock) {
            return;
        }
        m_energyReply += sock->readAll();
        m_energyRequest.clear();
        sock->deleteLater();
        applyEnergy(kdosParseEnergyReport(m_energyReply));
    });
    connect(sock, &QLocalSocket::errorOccurred, this, [sock, fail](QLocalSocket::LocalSocketError e) {
        /* The daemon closes the connection after its reply; that is the end of
         * a report, not a failure. */
        if (e == QLocalSocket::PeerClosedError) {
            return;
        }
        if (e == QLocalSocket::ServerNotFoundError || e == QLocalSocket::ConnectionRefusedError) {
            fail(i18nc("@info", "kdos-energyd is not running"));
        } else {
            fail(sock->errorString());
        }
    });
    QTimer::singleShot(EnergyTimeoutMs, sock, [fail]() {
        fail(i18nc("@info", "kdos-energyd did not answer"));
    });
    sock->connectToServer(m_energySocket);
}

void KdosPlugin::applyEnergy(const EnergyReport &report)
{
    m_boxEnergy.clear();
    if (!report.ok) {
        QString why = report.error;
        if (why == QLatin1String("not permitted")) {
            why = i18nc("@info", "Only root and members of wheel may read the energy ledger");
        }
        m_energyStatus->setValue(why);
        for (EnergyObject *e : std::as_const(m_energy)) {
            retire(m_container, e);
        }
        m_energy.clear();
        return;
    }

    m_energyStatus->setValue(report.floorSettled ? i18nc("@info", "Measuring")
                                                 : i18nc("@info", "Measuring; the idle floor is still settling"));
    m_energyWindow->setValue(report.windowSeconds);
    m_energyFloor->setValue(report.idleFloorWatts);
    m_energyAttributable->setValue(report.attributable * 100.0);
    m_energyShortLived->setValue(report.shortLived * 100.0);
    m_energyDomains->setValue(report.domains);

    QSet<QString> live;
    for (const EnergyApp &a : report.apps) {
        live.insert(a.id);
        EnergyObject *o = m_energy.value(a.id);
        if (!o) {
            o = new EnergyObject(a, m_container);
            m_energy.insert(a.id, o);
        }
        QString label = a.app;
        if (!a.box.isEmpty()) {
            const BoxLauncher l = m_launchers.value(a.box);
            label = i18nc("@info %1 application, %2 box", "%1 (box %2)", l.name.isEmpty() ? a.app : l.name, a.box);
            m_boxEnergy[a.box] += a.impact;
        }
        o->set(a, label);
    }
    for (auto it = m_energy.begin(); it != m_energy.end();) {
        if (!live.contains(it.key())) {
            retire(m_container, it.value());
            it = m_energy.erase(it);
        } else {
            ++it;
        }
    }
    for (BoxObject *b : std::as_const(m_boxes)) {
        b->setEnergy(m_boxEnergy.value(b->box));
    }
}

} // namespace Kdos

using Kdos::KdosPlugin;
K_PLUGIN_CLASS_WITH_JSON(KdosPlugin, "metadata.json")

#include "plugin.moc"
