/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   healthcontroller — run `kdos doctor --json`, and hold what it found
 */

#include "healthcontroller.h"

#include <KLocalizedString>

#include <QClipboard>
#include <QFileInfo>
#include <QGuiApplication>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>

namespace Kdos
{

namespace
{
/* doctor asks podman for its store, which on a cold machine takes seconds;
 * two minutes is a doctor that hangs. */
constexpr int DoctorTimeoutMs = 2 * 60 * 1000;

QString findKdos()
{
    const QString onPath = QStandardPaths::findExecutable(QStringLiteral("kdos"));
    if (!onPath.isEmpty()) {
        return onPath;
    }
    /* kdos-tools installs its links in /usr/local/bin, which a session
     * started without a login shell may not have on PATH. */
    const QString local = QStringLiteral("/usr/local/bin/kdos");
    return QFileInfo(local).isExecutable() ? local : QString();
}

QString lastLine(const QByteArray &bytes)
{
    const QStringList lines = QString::fromUtf8(bytes).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    return lines.isEmpty() ? QString() : lines.constLast().trimmed();
}
}

HealthController::HealthController(const QString &program, QObject *parent)
    : QObject(parent)
    , m_program(program.isEmpty() ? findKdos() : program)
    , m_model(new DoctorModel(this))
    , m_filtered(new DoctorFilterModel(this))
{
    m_filtered->setSourceModel(m_model);
}

/* A page closed while doctor runs leaves it to finish: it only reads, and
 * killing it mid-way would leave nothing worse than letting it end. */
HealthController::~HealthController()
{
    if (m_process) {
        m_process->disconnect(this);
        m_process->setParent(nullptr);
        connect(m_process, &QProcess::finished, m_process, &QObject::deleteLater);
    }
}

QAbstractItemModel *HealthController::checks() const
{
    return m_filtered;
}

DoctorModel *HealthController::report() const
{
    return m_model;
}

bool HealthController::ready() const
{
    return m_ready;
}

QString HealthController::error() const
{
    return m_error;
}

QString HealthController::explanation() const
{
    return m_explanation;
}

int HealthController::warnings() const
{
    return m_model->count(DoctorModel::Warn);
}

int HealthController::passed() const
{
    return m_model->count(DoctorModel::Ok);
}

int HealthController::skipped() const
{
    return m_model->count(DoctorModel::Skip);
}

int HealthController::total() const
{
    return m_model->rowCount();
}

QString HealthController::filter() const
{
    return m_filter;
}

void HealthController::setFilter(const QString &filter)
{
    if (m_filter == filter) {
        return;
    }
    m_filter = filter;
    m_filtered->setFilterText(filter);
    Q_EMIT filterChanged();
}

QString HealthController::text() const
{
    return m_model->rowCount() ? m_model->text() : QString();
}

QString HealthController::program() const
{
    return m_program;
}

QStringList HealthController::arguments() const
{
    return {QStringLiteral("doctor"), QStringLiteral("--json")};
}

void HealthController::refresh()
{
    if (m_process) {
        /* A refresh asked for while one runs is one more run after it, not
         * a second doctor beside the first. */
        m_again = true;
        return;
    }
    if (m_ready) {
        m_ready = false;
        Q_EMIT readyChanged();
    }
    if (m_program.isEmpty()) {
        m_model->clear();
        setError(i18nc("@info", "The kdos command is not installed"),
                 i18nc("@info", "The health checks are kdos doctor, which the kdos-tools package installs."));
        m_ready = true;
        Q_EMIT readyChanged();
        return;
    }

    m_process = new QProcess(this);
    m_process->setProgram(m_program);
    m_process->setArguments(arguments());
    auto *process = m_process;
    auto *timer = new QTimer(process);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, process, &QProcess::kill);
    connect(process, &QProcess::finished, this, [this, process](int exitCode, QProcess::ExitStatus status) {
        const QByteArray out = process->readAllStandardOutput();
        const QByteArray err = process->readAllStandardError();
        process->deleteLater();
        m_process = nullptr;
        finish(exitCode, status != QProcess::NormalExit, out, err);
    });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart) {
            return;
        }
        const QString why = process->errorString();
        process->deleteLater();
        m_process = nullptr;
        /* A refresh asked for meanwhile would fail to start the same way;
         * left set, it would add a run after the next good one. */
        m_again = false;
        m_model->clear();
        setError(i18nc("@info", "kdos doctor could not be run"), why);
        m_ready = true;
        Q_EMIT readyChanged();
    });
    process->start();
    timer->start(DoctorTimeoutMs);
}

void HealthController::finish(int exitCode, bool crashed, const QByteArray &out, const QByteArray &err)
{
    if (crashed) {
        m_model->clear();
        setError(i18nc("@info", "kdos doctor did not finish"), lastLine(err));
    } else if (exitCode != 0 && exitCode != 1) {
        m_model->clear();
        setError(i18nc("@info %1 is a number", "kdos doctor failed with exit status %1", exitCode), lastLine(err));
    } else if (!m_model->parse(out)) {
        setError(i18nc("@info", "kdos doctor gave no report this page can read"), lastLine(out));
    } else {
        setError({}, {});
    }
    m_ready = true;
    Q_EMIT resultChanged();
    Q_EMIT readyChanged();
    if (m_again) {
        m_again = false;
        refresh();
    }
}

void HealthController::setError(const QString &error, const QString &explanation)
{
    m_error = error;
    m_explanation = explanation;
    Q_EMIT resultChanged();
}

void HealthController::copyToClipboard() const
{
    if (QClipboard *clipboard = QGuiApplication::clipboard()) {
        clipboard->setText(text());
    }
}

} // namespace Kdos
