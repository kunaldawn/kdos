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
#include <KConfigGroup>
#include <KFormat>
#include <KLocalizedString>
#include <KSharedConfig>

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDateTime>
#include <QDesktopServices>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
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
 * ONE DUMP AT A TIME PER KIND. A page that asks again before the last answer
 * came — the plan after every change on the Summary page — supersedes it, and
 * the superseded process is killed, so a slow answer never lands after a
 * newer one. The answers go on standard input (`--config -`), as for the
 * install itself.
 */
void Backend::runDump(const QString &what, bool withAnswers, Done done)
{
    if (QProcess *old = m_dumps.take(what)) {
        old->disconnect(this);
        old->kill();
        old->deleteLater();
    }
    auto *p = new QProcess(this);
    p->setProcessChannelMode(QProcess::SeparateChannels);
    QStringList args;
    if (withAnswers) {
        args << QStringLiteral("--config") << QStringLiteral("-");
    }
    args << QStringLiteral("--dump") << what << QStringLiteral("--json");
    m_dumps.insert(what, p);
    Q_EMIT busyChanged();

    connect(p, &QProcess::finished, this, [this, p, what, done](int code, QProcess::ExitStatus status) {
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
            setError(err.isEmpty() ? i18n("kinstall could not describe this machine (%1).", what) : err);
            return;
        }
        setError(QString());
        done(doc.object().toVariantMap());
    });
    connect(p, &QProcess::errorOccurred, this, [this, p, what](QProcess::ProcessError e) {
        if (e != QProcess::FailedToStart || m_dumps.value(what) != p) {
            return;
        }
        m_dumps.remove(what);
        Q_EMIT busyChanged();
        p->deleteLater();
        setError(i18n("The installer's backend, %1, is not installed.", KinstallPath));
    });

    p->start(KinstallPath, args);
    if (withAnswers) {
        p->write(m_answers->toText().toUtf8());
    }
    p->closeWriteChannel();
}

/* With the answers: whether a disk fits, and holds two slots, depends on
 * the swap and on whether the box store is copied. */
void Backend::refreshProbe()
{
    m_probeTimer.stop();
    m_probeKey = m_answers->probeKey();
    runDump(QStringLiteral("probe"), true, [this](const QVariantMap &m) {
        m_probe = m;
        Q_EMIT probeChanged();
    });
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
    runDump(QStringLiteral("plan"), true, [this](const QVariantMap &m) {
        m_plan = m;
        Q_EMIT planChanged();
    });
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
    QDesktopServices::openUrl(QUrl::fromLocalFile(KinstallLog));
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

QString Backend::formatSize(double bytes) const
{
    return KFormat().formatByteSize(bytes);
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
