/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   backend — the graphical installer's one door into kinstall
 */

#include "backend.h"

#include <KColorScheme>
#include <KConfig>
#include <KConfigGroup>
#include <KFormat>
#include <KIO/JobUiDelegateFactory>
#include <KIO/OpenUrlJob>
#include <KLocalizedString>
#include <KSharedConfig>

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusPendingCall>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimeZone>
#include <QUrl>

#include <algorithm>

#ifndef KDOS_KINSTALL
#define KDOS_KINSTALL "/usr/bin/kinstall"
#endif

namespace Kdos
{

namespace
{
/* The binary the pkexec policy names. pkexec matches the program's path
 * exactly, so this is never looked up on $PATH. */
const QString KinstallPath = QStringLiteral(KDOS_KINSTALL);
const QString KinstallLog = QStringLiteral("/var/log/kinstall.log");
const QString Zone1970 = QStringLiteral("/usr/share/zoneinfo/zone1970.tab");
const QString Mounts = QStringLiteral("/proc/mounts");
/* Set on a dump started through pkexec. */
const char *const RootDumpProperty = "kdosRootDump";

QString capitalised(const QString &id)
{
    return id.isEmpty() ? id : id.left(1).toUpper() + id.mid(1);
}
}

Backend::Backend(QObject *parent)
    : QObject(parent)
    , m_answers(new Answers(this))
    , m_events(new EventStream(this))
    , m_keyboards(new KeyboardModel(this))
    , m_live(liveRoot(Mounts))
{
    m_keyboards->load();
    m_answers->setProperty("timezone", systemTimeZone());

    /* The probe's sizes follow the swap and the box-store answers, so a
     * change to either asks again. The timer gathers a spin box's steps
     * into one dump. */
    m_probeTimer.setSingleShot(true);
    m_probeTimer.setInterval(400);
    connect(&m_probeTimer, &QTimer::timeout, this, &Backend::refreshProbe);
    connect(m_answers, &Answers::changed, this, [this]() {
        if (m_answers->probeKey() != m_probeKey) {
            m_probeTimer.start();
        }
    });
}

Backend::~Backend()
{
    for (QProcess *p : std::as_const(m_dumps)) {
        if (p->property(RootDumpProperty).toBool()) {
            orphanRootDump(p);
            continue;
        }
        p->kill();
        p->waitForFinished(1000);
    }
    if (m_partitionManager) {
        m_partitionManager->disconnect(this);
    }
}

void Backend::setError(const QString &e)
{
    if (m_error != e) {
        m_error = e;
        Q_EMIT errorChanged();
    }
}

/*
 * A DUMP RUN THROUGH pkexec IS ROOT'S, and this user's signal cannot reach
 * it: killing it fails, and destroying a QProcess that is still running
 * waits up to thirty seconds on this thread for it to end. One that is no
 * longer wanted is cut loose instead — detached from this object, its answer
 * never read, deleted when it ends of its own accord.
 */
void Backend::orphanRootDump(QProcess *p)
{
    p->disconnect(this);
    p->setParent(nullptr);
    connect(p, &QProcess::finished, p, &QObject::deleteLater);
    connect(p, &QProcess::errorOccurred, p, &QObject::deleteLater);
}

/*
 * ONE DUMP AT A TIME PER KIND. A page that asks again before the last answer
 * came — the plan after every change on the Summary page — supersedes it, and
 * the superseded process is killed, or cut loose when it is root's, so a slow
 * answer never lands after a newer one. The answers go on standard input
 * (`--config -`), as for the install itself. `asRoot` runs it through pkexec,
 * and a failure there — pkexec missing, the request refused or dismissed —
 * runs it again as this user rather than reporting it.
 */
void Backend::runDump(const QString &what, bool withAnswers, Done done, bool asRoot)
{
    const QString pkexec = asRoot ? QStandardPaths::findExecutable(QStringLiteral("pkexec")) : QString();
    if (asRoot && pkexec.isEmpty()) {
        asRoot = false;
    }
    if (QProcess *old = m_dumps.take(what)) {
        if (old->property(RootDumpProperty).toBool()) {
            orphanRootDump(old);
        } else {
            old->disconnect(this);
            old->kill();
            old->deleteLater();
        }
    }
    auto *p = new QProcess(this);
    p->setProperty(RootDumpProperty, asRoot);
    p->setProcessChannelMode(QProcess::SeparateChannels);
    const QStringList args = dumpArgs(what, withAnswers, m_rootPayload, asRoot);
    m_dumps.insert(what, p);
    Q_EMIT busyChanged();

    connect(p, &QProcess::finished, this, [this, p, what, withAnswers, done, asRoot](int code, QProcess::ExitStatus status) {
        if (m_dumps.value(what) != p) {
            p->deleteLater();
            return;
        }
        m_dumps.remove(what);
        Q_EMIT busyChanged();
        const QByteArray out = p->readAllStandardOutput();
        const QString err = QString::fromUtf8(p->readAllStandardError()).trimmed();
        p->deleteLater();
        QJsonParseError perr;
        const QJsonDocument doc = QJsonDocument::fromJson(out, &perr);
        if (status != QProcess::NormalExit || code != 0 || perr.error != QJsonParseError::NoError || !doc.isObject()) {
            if (asRoot) {
                runDump(what, withAnswers, done);
                return;
            }
            setError(err.isEmpty() ? i18n("kinstall could not describe this machine (%1).", what) : err);
            return;
        }
        setError(QString());
        QVariantMap m = doc.object().toVariantMap();
        if (asRoot && what == QLatin1String("probe") && m.value(QStringLiteral("payload_kb")).toULongLong() > 0) {
            m_rootPayload = {{QStringLiteral("payload_kb"), m.value(QStringLiteral("payload_kb"))},
                             {QStringLiteral("appbox_kb"), m.value(QStringLiteral("appbox_kb"))},
                             {QStringLiteral("copy_system_bytes"), m.value(QStringLiteral("copy_system_bytes"))},
                             {QStringLiteral("copy_store_bytes"), m.value(QStringLiteral("copy_store_bytes"))}};
        }
        if (what == QLatin1String("probe")) {
            m = withCopyBytes(m, m_rootPayload);
        }
        done(m);
    });
    connect(p, &QProcess::errorOccurred, this, [this, p, what, withAnswers, done, asRoot](QProcess::ProcessError e) {
        if (e != QProcess::FailedToStart || m_dumps.value(what) != p) {
            return;
        }
        m_dumps.remove(what);
        Q_EMIT busyChanged();
        p->deleteLater();
        if (asRoot) {
            runDump(what, withAnswers, done);
            return;
        }
        setError(i18n("The installer's backend, %1, is not installed.", KinstallPath));
    });

    if (asRoot) {
        p->start(pkexec, QStringList{KinstallPath} + args);
    } else {
        p->start(KinstallPath, args);
    }
    if (withAnswers) {
        p->write(m_answers->toText().toUtf8());
    }
    p->closeWriteChannel();
}

/*
 * ROOT'S PAYLOAD FOR EVERY PLAN. The payload walk as this user cannot enter
 * what only root may, so it measures less than root's, and the Disk page's
 * plan, run as this user, would offer a swap a mebibyte larger than the
 * Summary's root plan accepts. The last root probe's figures go to every
 * plan, root's or not, and to every probe as this user, so both pages
 * decide on one number. A root probe is given none: it measures again,
 * which is how Look Again takes in a tree that has grown, and its figures
 * replace the kept ones. The install measures its own.
 */
QStringList Backend::dumpArgs(const QString &what, bool withAnswers, const QVariantMap &rootPayload, bool asRoot)
{
    QStringList args;
    if (withAnswers) {
        args << QStringLiteral("--config") << QStringLiteral("-");
    }
    const qulonglong payload = rootPayload.value(QStringLiteral("payload_kb")).toULongLong();
    if (payload > 0 && (what == QLatin1String("plan") || (what == QLatin1String("probe") && !asRoot))) {
        args << QStringLiteral("--payload-kb") << QString::number(payload) << QStringLiteral("--appbox-kb")
             << QString::number(rootPayload.value(QStringLiteral("appbox_kb")).toULongLong());
    }
    args << QStringLiteral("--dump") << what << QStringLiteral("--json");
    return args;
}

/* With the answers: whether a disk fits, and holds two slots, depends on
 * the swap and on whether the box store is copied. */
void Backend::runProbe(bool asRoot)
{
    m_probeTimer.stop();
    m_probeKey = m_answers->probeKey();
    runDump(
        QStringLiteral("probe"),
        true,
        [this](const QVariantMap &m) {
            m_probe = m;
            Q_EMIT probeChanged();
            if (m_probeAgain) {
                m_probeAgain = false;
                refreshProbe();
            }
        },
        asRoot);
}

/*
 * A ROOT PROBE STILL RUNNING IS LET FINISH. The first probe on the live
 * medium runs as root, and the Disk page asks again on every visit; a probe
 * as this user started over it would cut it loose, its payload never kept,
 * and the two pages would again decide on different figures. The probe
 * asked for meanwhile runs once the root one has answered.
 */
void Backend::refreshProbe()
{
    const QProcess *running = m_dumps.value(QStringLiteral("probe"));
    if (running && running->property(RootDumpProperty).toBool()) {
        m_probeTimer.stop();
        m_probeAgain = true;
        return;
    }
    const bool asRoot = m_live && !m_probedAsRoot;
    m_probedAsRoot = true;
    runProbe(asRoot);
}

void Backend::lookAgain()
{
    runProbe(m_probe.value(QStringLiteral("live")).toBool());
}

void Backend::refreshLook()
{
    runDump(QStringLiteral("look"), false, [this](const QVariantMap &m) {
        m_look = m;
        m_answers->setReservedNames(m.value(QStringLiteral("reserved_names")).toStringList());
        Q_EMIT lookChanged();
    });
}

/* The first answer, read without the answers, seeds the boxed ticks from the
 * backend's own preselection, so the page starts with what an unattended
 * install would carry. Every later one is read with the answers, whose
 * keyboard and time zone name the language groups to tick (`why`); the
 * first is followed by one of those at once. */
void Backend::refreshApps()
{
    runDump(QStringLiteral("apps"), !m_apps.isEmpty(), [this](const QVariantMap &m) {
        const bool first = m_apps.isEmpty();
        m_apps = m;
        if (first) {
            const QVariantList groups = m.value(QStringLiteral("groups")).toList();
            for (const QVariant &g : groups) {
                const QVariantMap gm = g.toMap();
                m_answers->setAppChosen(gm.value(QStringLiteral("id")).toString(), gm.value(QStringLiteral("chosen")).toBool());
            }
        } else {
            QStringList suggested;
            const QVariantList native = m.value(QStringLiteral("native")).toList();
            for (const QVariant &g : native) {
                const QVariantMap gm = g.toMap();
                if (!gm.value(QStringLiteral("why")).toString().isEmpty()) {
                    suggested << gm.value(QStringLiteral("id")).toString();
                }
            }
            m_answers->suggestGroups(suggested);
        }
        Q_EMIT appsChanged();
        if (first) {
            refreshApps();
        }
    });
}

void Backend::refreshPlan()
{
    runDump(
        QStringLiteral("plan"),
        true,
        [this](const QVariantMap &m) {
            m_plan = m;
            Q_EMIT planChanged();
        },
        m_live);
}

/* As this user once a root probe's payload is kept; on the live medium
 * before that (the root probe refused or dismissed), as root, the
 * Summary's route, so the two pages still measure the same way. */
void Backend::checkPlan()
{
    runDump(
        QStringLiteral("plan"),
        true,
        [this](const QVariantMap &m) {
            m_plan = m;
            Q_EMIT planChanged();
        },
        m_live && m_rootPayload.isEmpty());
}

/*
 * THE ZONES ARE tzdata's, NOT ICU's. QTimeZone's list keeps every backward
 * alias (Africa/Asmera beside Africa/Asmara, the whole US block), which reads as
 * the same place twice; zone1970.tab names each region once, and every name
 * in it is a file under /usr/share/zoneinfo, which is what the install links
 * /etc/localtime to.
 */
QStringList Backend::readZones(const QString &tabPath)
{
    QStringList out;
    QFile f(tabPath);
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        while (!f.atEnd()) {
            const QString l = QString::fromUtf8(f.readLine());
            if (l.startsWith(QLatin1Char('#'))) {
                continue;
            }
            const QStringList cols = l.split(QLatin1Char('\t'));
            if (cols.size() >= 3 && !cols.at(2).trimmed().isEmpty()) {
                out << cols.at(2).trimmed();
            }
        }
    }
    std::sort(out.begin(), out.end());
    out.removeDuplicates();
    out.prepend(QStringLiteral("UTC"));
    return out;
}

/* The probe's rule, probe_system()'s: any line mounting an overlay at `/`.
 * Read whole: a /proc file reports no size, so a line loop that asks
 * atEnd() first reads nothing. */
bool Backend::liveRoot(const QString &mountsPath)
{
    QFile f(mountsPath);
    if (!f.open(QIODevice::ReadOnly)) {
        return false;
    }
    const QList<QByteArray> lines = f.readAll().split('\n');
    for (const QByteArray &l : lines) {
        const QList<QByteArray> cols = l.simplified().split(' ');
        if (cols.size() >= 3 && cols.at(1) == "/" && cols.at(2) == "overlay") {
            return true;
        }
    }
    return false;
}

QString Backend::diskPlan(const QString &plan, int partitions)
{
    return partitions > 0 ? plan : QStringLiteral("wipe");
}

QString Backend::diskTitle(const QVariantMap &disk)
{
    const QString model = disk.value(QStringLiteral("model")).toString();
    const double bytes = disk.contains(QStringLiteral("bytes")) ? disk.value(QStringLiteral("bytes")).toDouble()
                                                               : disk.value(QStringLiteral("sectors")).toDouble() * 512;
    return i18nc("@info %1 model %2 size %3 device", "%1, %2 (%3)",
                 model.isEmpty() ? i18nc("@item a disk with no model name", "Disk") : model,
                 KFormat().formatByteSize(bytes, 1), disk.value(QStringLiteral("path")).toString());
}

namespace
{
QString pvText(const QString &vg)
{
    return vg.isEmpty() ? i18nc("@info", "LVM physical volume")
                        : i18nc("@info %1 volume group", "LVM physical volume (volume group %1)", vg);
}
}

QString Backend::partitionText(const QVariantMap &p)
{
    const QString fstype = p.value(QStringLiteral("fstype")).toString();
    const QString label = p.value(QStringLiteral("label")).toString();
    const QString kind = p.value(QStringLiteral("esp")).toBool()       ? i18nc("@item a partition type", "EFI system")
                       : p.value(QStringLiteral("bios_boot")).toBool() ? i18nc("@item a partition type", "BIOS boot")
                       : fstype == QLatin1String("LVM2_member")        ? pvText(p.value(QStringLiteral("vg")).toString())
                       : !fstype.isEmpty()                             ? fstype
                                                                       : i18nc("@item no filesystem", "empty");
    const QString size = KFormat().formatByteSize(p.value(QStringLiteral("sectors")).toDouble() * 512, 1);
    return label.isEmpty() ? i18nc("@item %1 type %2 size", "%1 %2", kind, size)
                           : i18nc("@item %1 type %2 label %3 size", "%1 “%2” %3", kind, label, size);
}

QString Backend::diskContents(const QVariantMap &disk)
{
    const QVariantList parts = disk.value(QStringLiteral("partitions")).toList();
    if (disk.value(QStringLiteral("iso_image")).toBool()) {
        const QString label = disk.value(QStringLiteral("label")).toString();
        const double bytes = disk.value(QStringLiteral("fs_bytes")).toDouble();
        const QString size = KFormat().formatByteSize(bytes, 1);
        if (bytes <= 0) {
            return label.isEmpty() ? i18nc("@info", "ISO 9660 image on the whole disk")
                                   : i18nc("@info %1 volume label", "ISO 9660 image “%1” on the whole disk", label);
        }
        return label.isEmpty() ? i18nc("@info %1 image size", "ISO 9660 image on the whole disk, %1", size)
                               : i18nc("@info %1 volume label %2 image size", "ISO 9660 image “%1” on the whole disk, %2", label, size);
    }
    if (parts.isEmpty()) {
        const QString fstype = disk.value(QStringLiteral("fstype")).toString();
        if (fstype == QLatin1String("LVM2_member")) {
            return pvText(disk.value(QStringLiteral("vg")).toString());
        }
        return fstype.isEmpty() ? QString() : i18nc("@info %1 filesystem type", "%1 on the whole disk", fstype);
    }
    QStringList out;
    for (const QVariant &p : parts) {
        out << partitionText(p.toMap());
    }
    return out.join(QStringLiteral(", "));
}

QString Backend::diskHolds(const QVariantMap &disk)
{
    const QString what = diskContents(disk);
    if (what.isEmpty()) {
        return i18nc("@info a disk with no partitions", "Empty");
    }
    if (disk.value(QStringLiteral("partitions")).toList().isEmpty()
        && disk.value(QStringLiteral("fstype")).toString() == QLatin1String("LVM2_member")) {
        return what;
    }
    return i18nc("@info %1 what a disk holds", "Holds: %1", what);
}

QVariantMap Backend::probedDisk(const QString &path) const
{
    const QVariantList disks = m_probe.value(QStringLiteral("disks")).toList();
    for (const QVariant &d : disks) {
        const QVariantMap m = d.toMap();
        if (m.value(QStringLiteral("path")).toString() == path) {
            return m;
        }
    }
    return {};
}

QString Backend::diskName(const QString &path) const
{
    const QVariantMap d = probedDisk(path);
    return d.isEmpty() ? path : diskTitle(d);
}

QString Backend::diskErased(const QString &path) const
{
    const QVariantMap d = probedDisk(path);
    return d.isEmpty() ? QString() : diskContents(d);
}

QString Backend::partitionNamed(const QVariantMap &probe, const QString &path)
{
    const QVariantList disks = probe.value(QStringLiteral("disks")).toList();
    for (const QVariant &d : disks) {
        const QVariantList parts = d.toMap().value(QStringLiteral("partitions")).toList();
        for (const QVariant &p : parts) {
            const QVariantMap m = p.toMap();
            if (m.value(QStringLiteral("path")).toString() == path) {
                return i18nc("@info %1 device %2 what it holds", "%1 (%2)", path, partitionText(m));
            }
        }
    }
    return path;
}

QString Backend::soleEsp(const QVariantList &candidates)
{
    if (candidates.size() != 1) {
        return QString();
    }
    const QVariantMap p = candidates.first().toMap();
    return p.value(QStringLiteral("esp")).toBool() ? p.value(QStringLiteral("path")).toString() : QString();
}

/*
 * THE LIVE SESSION TYPES THE CHOSEN LAYOUT. The Account and Encryption pages
 * come after the Keyboard page, and a password typed there on the live
 * medium's US keys holds other characters than the same keys type at the
 * installed machine's greeter, or no umlaut at all. kxkbrc is KWin's layout
 * list; `us` stays second so the live medium's own keys remain one switch
 * away. KWin reads it again only on KConfig's change notice for its
 * [Layout] group (layoutNotice()), never on a write alone. fcitx5 needs no
 * change: with its XKB override off (/etc/xdg/fcitx5/conf/wayland.conf) it
 * decodes keys with the keymap KWin hands it, so it types the new layout too.
 */
bool Backend::writeLiveLayout(const QString &configHome, const QString &layout, const QString &variant)
{
    static const QRegularExpression xkbName(QStringLiteral("^[A-Za-z0-9_+:.-]+$"));
    if (!xkbName.match(layout).hasMatch() || (!variant.isEmpty() && !xkbName.match(variant).hasMatch()) || configHome.isEmpty()) {
        return false;
    }
    if (!QDir().mkpath(configHome)) {
        return false;
    }
    const bool us = layout == QLatin1String("us");
    const QString layouts = us ? layout : QString(layout + QStringLiteral(",us"));
    const QString variants = us ? variant : QString(variant + QLatin1Char(','));
    KConfig kxkb(configHome + QStringLiteral("/kxkbrc"), KConfig::SimpleConfig);
    KConfigGroup g(&kxkb, QStringLiteral("Layout"));
    g.writeEntry("LayoutList", layouts);
    g.writeEntry("VariantList", variants);
    g.writeEntry("Use", true);
    return kxkb.sync();
}

/*
 * KConfig's change notice for kxkbrc, the signal KConfigWatcher follows and
 * KWin's keyboard layout code reconfigures on: path /kxkbrc, interface
 * org.kde.kconfig.notify, member ConfigChanged, a{saay} of group to keys.
 * KConfig sends none itself for a file opened by its absolute path, which
 * is how writeLiveLayout() opens it so a test can name the directory.
 */
QDBusMessage Backend::layoutNotice()
{
    qDBusRegisterMetaType<QByteArrayList>();
    qDBusRegisterMetaType<QHash<QString, QByteArrayList>>();
    const QHash<QString, QByteArrayList> changes{{QStringLiteral("Layout"), {QByteArrayLiteral("LayoutList"), QByteArrayLiteral("VariantList"), QByteArrayLiteral("Use")}}};
    QDBusMessage m = QDBusMessage::createSignal(QStringLiteral("/kxkbrc"), QStringLiteral("org.kde.kconfig.notify"), QStringLiteral("ConfigChanged"));
    m.setArguments({QVariant::fromValue(changes)});
    return m;
}

void Backend::applyLiveLayout(const QString &layout, const QString &variant)
{
    if (!m_live && !m_probe.value(QStringLiteral("live")).toBool()) {
        return;
    }
    if (!writeLiveLayout(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation), layout, variant)) {
        return;
    }
    QDBusConnection::sessionBus().send(layoutNotice());
}

QStringList Backend::timeZones() const
{
    static const QStringList zones = readZones(Zone1970);
    return zones;
}

/* The running system's zone where the list offers it, else UTC: an alias
 * the list does not hold would be an answer the page cannot show. */
QString Backend::systemTimeZone() const
{
    const QString id = QString::fromLatin1(QTimeZone::systemTimeZoneId());
    return timeZones().contains(id) ? id : QStringLiteral("UTC");
}

QString Backend::localTime(const QString &zone) const
{
    const QTimeZone z(zone.toUtf8());
    if (!z.isValid()) {
        return QString();
    }
    return QDateTime::currentDateTimeUtc().toTimeZone(z).toString(QStringLiteral("HH:mm"));
}

QString Backend::logPath() const
{
    return KinstallLog;
}

bool Backend::logReadable() const
{
    return QFileInfo(KinstallLog).isReadable();
}

void Backend::openLog() const
{
    auto *job = new KIO::OpenUrlJob(QUrl::fromLocalFile(KinstallLog), QStringLiteral("text/plain"));
    job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, nullptr));
    job->start();
}

bool Backend::partitionManagerAvailable() const
{
    return !QStandardPaths::findExecutable(QStringLiteral("partitionmanager")).isEmpty();
}

/*
 * PARTITION MANAGER RUNS AS THE SESSION'S USER, with kpmcore's helper asking
 * polkit for each change; the live medium's rule grants that to the live
 * session. Closing it is the signal to look at the disks again, so the reuse
 * plan's lists show the layout just made.
 */
void Backend::openPartitionManager()
{
    if (m_partitionManager || !partitionManagerAvailable()) {
        return;
    }
    m_partitionManager = new QProcess(this);
    connect(m_partitionManager, &QProcess::finished, this, [this]() {
        m_partitionManager->deleteLater();
        m_partitionManager = nullptr;
        Q_EMIT partitionManagerRunningChanged();
        Q_EMIT partitionManagerClosed();
        refreshProbe();
    });
    connect(m_partitionManager, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e != QProcess::FailedToStart || !m_partitionManager) {
            return;
        }
        m_partitionManager->deleteLater();
        m_partitionManager = nullptr;
        Q_EMIT partitionManagerRunningChanged();
        setError(i18n("Partition Manager could not be started."));
    });
    m_partitionManager->start(QStandardPaths::findExecutable(QStringLiteral("partitionmanager")), {});
    Q_EMIT partitionManagerRunningChanged();
}

/*
 * THE INSTALL, AS ROOT, THROUGH pkexec. The polkit action io.kdos.installer.run
 * names /usr/bin/kinstall; on the live medium a rule grants it to the local
 * active session without a password, and anywhere else polkit asks for an
 * administrator's. The answers, passwords included, go down the pipe and are
 * never written to a file.
 */
void Backend::startInstall()
{
    if (m_install) {
        return;
    }
    const QString pkexec = QStandardPaths::findExecutable(QStringLiteral("pkexec"));
    m_events->reset();
    /* Both asked here, before pkexec runs: pkexec's own 127 also means
     * "not authorised", so a missing program could not be told from a
     * refusal afterwards. */
    if (!QFileInfo(KinstallPath).isExecutable()) {
        m_events->fail(i18n("The installer's backend, %1, is not installed.", KinstallPath));
        Q_EMIT installFinished(false);
        return;
    }
    if (pkexec.isEmpty()) {
        m_events->fail(i18n("pkexec is not installed, so the install cannot ask for permission."));
        Q_EMIT installFinished(false);
        return;
    }
    m_install = new QProcess(this);
    m_install->setProcessChannelMode(QProcess::SeparateChannels);
    connect(m_install, &QProcess::readyReadStandardOutput, this, [this]() {
        m_events->feed(m_install->readAllStandardOutput());
    });
    connect(m_install, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        m_events->feed(m_install->readAllStandardOutput());
        m_events->close(status == QProcess::NormalExit ? code : 1);
        m_install->deleteLater();
        m_install = nullptr;
        Q_EMIT installingChanged();
        Q_EMIT installFinished(m_events->status() == 0);
    });
    connect(m_install, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e != QProcess::FailedToStart || !m_install) {
            return;
        }
        m_events->fail(i18n("pkexec could not be started."));
        m_install->deleteLater();
        m_install = nullptr;
        Q_EMIT installingChanged();
        Q_EMIT installFinished(false);
    });
    m_install->start(pkexec,
                     {KinstallPath, QStringLiteral("--config"), QStringLiteral("-"), QStringLiteral("--unattended"), QStringLiteral("--events"),
                      QStringLiteral("json")});
    m_install->write(m_answers->toText().toUtf8());
    m_install->closeWriteChannel();
    Q_EMIT installingChanged();
}

void Backend::reboot()
{
    QDBusMessage m = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.login1"),
                                                    QStringLiteral("/org/freedesktop/login1"),
                                                    QStringLiteral("org.freedesktop.login1.Manager"),
                                                    QStringLiteral("Reboot"));
    m << true;
    QDBusConnection::systemBus().asyncCall(m);
}

QVariantMap Backend::accentColors(const QString &accent) const
{
    const QString scheme = QStringLiteral("KDOS") + capitalised(accent);
    const QString path = QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("color-schemes/%1.colors").arg(scheme));
    if (path.isEmpty()) {
        return {};
    }
    const KSharedConfigPtr config = KSharedConfig::openConfig(path, KConfig::SimpleConfig);
    const KColorScheme window(QPalette::Active, KColorScheme::Window, config);
    const KColorScheme view(QPalette::Active, KColorScheme::View, config);
    const KColorScheme selection(QPalette::Active, KColorScheme::Selection, config);
    QVariantMap m;
    m.insert(QStringLiteral("name"), KConfigGroup(config, QStringLiteral("General")).readEntry("Name", scheme));
    m.insert(QStringLiteral("window"), window.background(KColorScheme::NormalBackground).color());
    m.insert(QStringLiteral("view"), view.background(KColorScheme::NormalBackground).color());
    m.insert(QStringLiteral("text"), window.foreground(KColorScheme::NormalText).color());
    m.insert(QStringLiteral("highlight"), selection.background(KColorScheme::NormalBackground).color());
    m.insert(QStringLiteral("highlightedText"), selection.foreground(KColorScheme::NormalText).color());
    m.insert(QStringLiteral("accent"), view.decoration(KColorScheme::FocusColor).color());
    return m;
}

QString Backend::accentPreview(const QString &lookAndFeel) const
{
    const QString path = QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                                QStringLiteral("plasma/look-and-feel/%1/contents/previews/preview.png").arg(lookAndFeel));
    return path.isEmpty() ? QString() : QUrl::fromLocalFile(path).toString();
}

QString Backend::formatSize(double bytes, int precision) const
{
    return KFormat().formatByteSize(bytes, precision);
}

/* A probe handed root's payload walks nothing and knows no copy figures
 * (null); root's own go in their place, so every probe names one size. */
QVariantMap Backend::withCopyBytes(const QVariantMap &probe, const QVariantMap &rootPayload)
{
    QVariantMap m = probe;
    for (const QString &k : {QStringLiteral("copy_system_bytes"), QStringLiteral("copy_store_bytes")}) {
        if (m.value(k).isNull() && !rootPayload.value(k).isNull()) {
            m.insert(k, rootPayload.value(k));
        }
    }
    return m;
}

QString Backend::installSize(const QVariantMap &probe)
{
    const QVariant system = probe.value(QStringLiteral("copy_system_bytes"));
    if (system.isNull()) {
        return QStringLiteral("—");
    }
    const double store = probe.value(QStringLiteral("copy_store_bytes")).toDouble();
    const QString sys = KFormat().formatByteSize(system.toDouble(), 1);
    if (store <= 0) {
        return sys;
    }
    return i18nc("@info %1 the whole copy %2 the system's part %3 the box store's", "%1 — %2 system and %3 boxed-app store",
                 KFormat().formatByteSize(system.toDouble() + store, 1), sys, KFormat().formatByteSize(store, 1));
}

QString Backend::swapText(const QVariantMap &plan)
{
    const QString kind = plan.value(QStringLiteral("swap")).toString();
    const QVariantMap lvm = plan.value(QStringLiteral("lvm")).toMap();
    const QString mb = QString::number(plan.value(QStringLiteral("swap_mb")).toLongLong());
    if (kind.isEmpty() || kind == QLatin1String("none")) {
        return i18nc("@info no swap", "None");
    }
    if (lvm.value(QStringLiteral("swap_mb")).toLongLong() > 0) {
        return i18nc("@info %1 swap size", "%1 MiB, a volume in the group", mb);
    }
    if (kind == QLatin1String("partition")) {
        return i18nc("@info %1 swap size", "%1 MiB, a partition", mb);
    }
    return i18nc("@info %1 swap size", "%1 MiB, a file in the root", mb);
}

QString Backend::accentLabel(const QString &accent) const
{
    QString name = accentColors(accent).value(QStringLiteral("name")).toString();
    if (name.startsWith(QLatin1String("KDOS "))) {
        name = name.mid(5);
    }
    return name.isEmpty() ? capitalised(accent) : name;
}

QString Backend::deskTitle(const QString &desk) const
{
    if (desk == QLatin1String("dock")) {
        return i18nc("@option:radio desk", "Dock");
    }
    if (desk == QLatin1String("tiling")) {
        return i18nc("@option:radio desk", "Tiling");
    }
    if (desk == QLatin1String("kdos")) {
        return i18nc("@option:radio desk", "KDOS");
    }
    return desk;
}

}
