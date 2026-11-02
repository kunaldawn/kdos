/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   checklist — the first-run checklist that ticks itself
 */

#include "checklist.h"

#include <KLocalizedString>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

#include <algorithm>

namespace Kdos
{

namespace
{
constexpr int KnownVersion = 1;
/* The probe asks fwupd and lsblk, which it caches; a run that takes this
 * long has hung on something else and is given up. */
constexpr int ProbeTimeoutMs = 20 * 1000;
constexpr qint64 MaxReply = 256 * 1024;

const char *Domain = "kdos-plasmoid-checklist";
} // namespace

QStringList kdosChecklistIds()
{
    return {QStringLiteral("backup"),
            QStringLiteral("phone"),
            QStringLiteral("language"),
            QStringLiteral("box"),
            QStringLiteral("firmware"),
            QStringLiteral("encryption")};
}

ChecklistText kdosChecklistText(const QString &id)
{
    ChecklistText t;
    if (id == QLatin1String("backup")) {
        t.title = i18ndc(Domain, "@title checklist item", "Set up a backup");
        t.explanation = i18ndc(Domain, "@info", "Copies of your files on another disk, made on a schedule, so a lost or broken disk loses nothing.");
        t.actionText = i18ndc(Domain, "@action:button", "Set Up Backups…");
        t.icon = QStringLiteral("drive-harddisk-symbolic");
        t.command = {QStringLiteral("systemsettings"), QStringLiteral("kcm_kup")};
    } else if (id == QLatin1String("phone")) {
        t.title = i18ndc(Domain, "@title checklist item", "Pair your phone");
        t.explanation = i18ndc(Domain, "@info", "Share files, the clipboard and notifications with your phone over this network, with nothing going through the internet.");
        t.actionText = i18ndc(Domain, "@action:button", "Pair a Phone…");
        t.icon = QStringLiteral("smartphone-symbolic");
        t.optional = true;
        t.command = {QStringLiteral("kdeconnect-app")};
    } else if (id == QLatin1String("language")) {
        t.title = i18ndc(Domain, "@title checklist item", "Add a language");
        t.explanation = i18ndc(Domain, "@info", "Choose how dates, numbers and money are written, and add a keyboard layout for each language you type.");
        t.actionText = i18ndc(Domain, "@action:button", "Region and Language…");
        t.icon = QStringLiteral("preferences-desktop-locale-symbolic");
        t.command = {QStringLiteral("systemsettings"), QStringLiteral("kcm_regionandlang")};
    } else if (id == QLatin1String("box")) {
        t.title = i18ndc(Domain, "@title checklist item", "Create your first box");
        t.explanation = i18ndc(Domain, "@info", "A box runs an application apart from your files and the rest of the system, with only what you give it.");
        t.actionText = i18ndc(Domain, "@action:button", "Open Boxes…");
        t.icon = QStringLiteral("package-x-generic-symbolic");
        t.command = {QStringLiteral("systemsettings"), QStringLiteral("kcm_kdos_boxes")};
    } else if (id == QLatin1String("firmware")) {
        t.title = i18ndc(Domain, "@title checklist item", "Check firmware");
        t.explanation = i18ndc(Domain, "@info", "See whether this machine's devices have firmware updates waiting.");
        t.actionText = i18ndc(Domain, "@action:button", "Open Updates…");
        t.icon = QStringLiteral("cpu-symbolic");
        t.command = {QStringLiteral("plasma-discover"), QStringLiteral("--mode"), QStringLiteral("update")};
    } else if (id == QLatin1String("encryption")) {
        t.title = i18ndc(Domain, "@title checklist item", "Confirm disk encryption");
        t.explanation = i18ndc(Domain, "@info", "With the disk encrypted, nobody who takes the machine can read your files without the passphrase.");
        t.actionText = i18ndc(Domain, "@action:button", "Read About Encryption…");
        t.icon = QStringLiteral("security-high-symbolic");
        t.command = {QStringLiteral("khelpcenter"), QStringLiteral("file:///usr/share/doc/kdos/03-architecture/security-model.html")};
    }
    return t;
}

bool kdosParseChecklist(const QByteArray &json, QList<ChecklistEntry> *out)
{
    out->clear();
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(json.trimmed(), &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }
    const QJsonObject root = doc.object();
    if (root.value(QLatin1String("version")).toInt() != KnownVersion) {
        return false;
    }
    const QStringList order = kdosChecklistIds();
    const QJsonArray items = root.value(QLatin1String("items")).toArray();
    for (const QJsonValue &v : items) {
        const QJsonObject o = v.toObject();
        ChecklistEntry e;
        e.id = o.value(QLatin1String("id")).toString();
        if (!order.contains(e.id)) {
            continue;
        }
        if (std::any_of(out->cbegin(), out->cend(), [&e](const ChecklistEntry &x) {
                return x.id == e.id;
            })) {
            continue;
        }
        e.done = o.value(QLatin1String("done")).toBool();
        e.dismissed = o.value(QLatin1String("dismissed")).toBool();
        out->append(e);
    }
    std::sort(out->begin(), out->end(), [&order](const ChecklistEntry &a, const ChecklistEntry &b) {
        return order.indexOf(a.id) < order.indexOf(b.id);
    });
    return true;
}

ChecklistModel::ChecklistModel(QObject *parent)
    : QAbstractListModel(parent)
{
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &ChecklistModel::refresh);
    QTimer::singleShot(0, this, &ChecklistModel::refresh);
}

ChecklistModel::~ChecklistModel()
{
    if (m_probe) {
        m_probe->disconnect(this);
        m_probe->kill();
        m_probe->waitForFinished(1000);
    }
}

QString ChecklistModel::kdosProgram()
{
    QString path = QStandardPaths::findExecutable(QStringLiteral("kdos"));
    if (path.isEmpty()) {
        path = QStandardPaths::findExecutable(QStringLiteral("kdos"), {QStringLiteral("/usr/local/bin")});
    }
    return path;
}

int ChecklistModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_entries.size());
}

QVariant ChecklistModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid)) {
        return {};
    }
    const ChecklistEntry &e = m_entries.at(index.row());
    const ChecklistText t = kdosChecklistText(e.id);
    switch (role) {
    case IdRole:
        return e.id;
    case Qt::DisplayRole:
    case TitleRole:
        return t.title;
    case ExplanationRole:
        return t.explanation;
    case ActionTextRole:
        return t.actionText;
    case IconRole:
        return t.icon;
    case DoneRole:
        return e.done;
    case DismissedRole:
        return e.dismissed;
    case OptionalRole:
        return t.optional;
    }
    return {};
}

QHash<int, QByteArray> ChecklistModel::roleNames() const
{
    return {
        {IdRole, "itemId"},
        {TitleRole, "title"},
        {ExplanationRole, "explanation"},
        {ActionTextRole, "actionText"},
        {IconRole, "iconName"},
        {DoneRole, "done"},
        {DismissedRole, "dismissed"},
        {OptionalRole, "optional"},
    };
}

int ChecklistModel::remaining() const
{
    return int(std::count_if(m_entries.cbegin(), m_entries.cend(), [](const ChecklistEntry &e) {
        return !e.done && !e.dismissed;
    }));
}

int ChecklistModel::total() const
{
    return int(m_entries.size());
}

bool ChecklistModel::finished() const
{
    return m_ready && remaining() == 0;
}

bool ChecklistModel::ready() const
{
    return m_ready;
}

QString ChecklistModel::problem() const
{
    return m_problem;
}

int ChecklistModel::interval() const
{
    return m_interval;
}

void ChecklistModel::setInterval(int seconds)
{
    seconds = qMax(0, seconds);
    if (seconds == m_interval) {
        return;
    }
    m_interval = seconds;
    Q_EMIT intervalChanged();
    schedule();
}

void ChecklistModel::refresh()
{
    if (m_probe) {
        m_again = true;
        return;
    }
    m_timer.stop();
    const QString kdos = kdosProgram();
    if (kdos.isEmpty()) {
        m_problem = i18ndc(Domain, "@info", "The kdos command is not installed.");
        m_ready = true;
        apply({});
        return;
    }
    m_probe = new QProcess(this);
    m_probe->setProgram(kdos);
    m_probe->setArguments({QStringLiteral("checklist"), QStringLiteral("--json")});
    m_probe->setStandardInputFile(QProcess::nullDevice());
    connect(m_probe, &QProcess::finished, this, &ChecklistModel::probeFinished);
    connect(m_probe, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart) {
            probeFinished(-1, QProcess::CrashExit);
        }
    });
    QTimer::singleShot(ProbeTimeoutMs, m_probe, [probe = m_probe]() {
        probe->kill();
    });
    m_probe->start();
}

void ChecklistModel::probeFinished(int code, QProcess::ExitStatus status)
{
    if (!m_probe) {
        return;
    }
    const QByteArray out = m_probe->readAllStandardOutput().left(MaxReply);
    const QString err = QString::fromLocal8Bit(m_probe->readAllStandardError()).trimmed().section(QLatin1Char('\n'), -1);
    m_probe->deleteLater();
    m_probe = nullptr;

    QList<ChecklistEntry> entries;
    if (status == QProcess::NormalExit && code == 0 && kdosParseChecklist(out, &entries)) {
        m_problem.clear();
        m_ready = true;
        apply(entries);
    } else {
        /* A probe that failed says nothing about the items: what was shown
         * stays, with the reason beside it. */
        m_problem = err.isEmpty() ? i18ndc(Domain, "@info", "Could not check the list.") : err;
        m_ready = true;
        Q_EMIT changed();
    }
    if (m_again) {
        m_again = false;
        refresh();
        return;
    }
    schedule();
}

void ChecklistModel::apply(const QList<ChecklistEntry> &entries)
{
    bool same = entries.size() == m_entries.size();
    for (qsizetype i = 0; same && i < entries.size(); ++i) {
        same = entries[i].id == m_entries[i].id;
    }
    if (!same) {
        beginResetModel();
        m_entries = entries;
        endResetModel();
    } else {
        for (qsizetype i = 0; i < entries.size(); ++i) {
            if (entries[i].done != m_entries[i].done || entries[i].dismissed != m_entries[i].dismissed) {
                m_entries[i] = entries[i];
                const QModelIndex at = index(int(i));
                Q_EMIT dataChanged(at, at, {DoneRole, DismissedRole});
            }
        }
    }
    Q_EMIT changed();
}

void ChecklistModel::schedule()
{
    m_timer.stop();
    if (m_interval > 0 && !finished() && !m_probe) {
        m_timer.start(m_interval * 1000);
    }
}

bool ChecklistModel::open(const QString &id)
{
    const bool known = std::any_of(m_entries.cbegin(), m_entries.cend(), [&id](const ChecklistEntry &e) {
        return e.id == id;
    });
    const ChecklistText t = kdosChecklistText(id);
    if (!known || t.command.isEmpty()) {
        return false;
    }
    const QString program = QStandardPaths::findExecutable(t.command.first());
    return !program.isEmpty() && QProcess::startDetached(program, t.command.mid(1));
}

void ChecklistModel::dismiss(const QString &id)
{
    if (!kdosChecklistIds().contains(id)) {
        return;
    }
    runKdos({QStringLiteral("checklist"), QStringLiteral("dismiss"), id});
}

void ChecklistModel::restore()
{
    runKdos({QStringLiteral("checklist"), QStringLiteral("restore")});
}

void ChecklistModel::runKdos(const QStringList &arguments)
{
    const QString kdos = kdosProgram();
    if (kdos.isEmpty()) {
        return;
    }
    auto *p = new QProcess(this);
    p->setProgram(kdos);
    p->setArguments(arguments);
    p->setStandardInputFile(QProcess::nullDevice());
    connect(p, &QProcess::finished, this, [this, p](int code, QProcess::ExitStatus status) {
        if (status != QProcess::NormalExit || code != 0) {
            const QString err = QString::fromLocal8Bit(p->readAllStandardError()).trimmed().section(QLatin1Char('\n'), -1);
            m_problem = err.isEmpty() ? i18ndc(Domain, "@info", "Could not change the list.") : err;
            Q_EMIT changed();
        }
        p->deleteLater();
        refresh();
    });
    connect(p, &QProcess::errorOccurred, this, [p](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart) {
            p->deleteLater();
        }
    });
    p->start();
}

} // namespace Kdos
