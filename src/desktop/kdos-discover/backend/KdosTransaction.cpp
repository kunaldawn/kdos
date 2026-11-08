/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-discover — a plan run as one Discover transaction
 * ---------------------------------
 */

#include "KdosTransaction.h"
#include "KdosBackend.h"
#include "KdosPackd.h"
#include "KdosResource.h"
#include "libdiscover_kdos_debug.h"

#include <KLocalizedString>

#include <QNetworkInformation>

#include <signal.h>
#include <unistd.h>

using namespace Qt::StringLiterals;

namespace Kdos
{

// How many of a failed step's last standard-error lines reach the message.
static constexpr int StderrTail = 3;

KdosTransaction::KdosTransaction(KdosResource *resource, Role role, const Plan &plan, KdosBackend *backend)
    : Transaction(backend, resource, role, {})
    , m_backend(backend)
    , m_plan(plan)
{
    setCancellable(true);
    setStatus(QueuedStatus);
}

// The step's process group, never pid 0: kill(-0) would signal Discover's own
// group, and a process still starting has no pid yet.
static void endGroup(QProcess *p)
{
    if (!p || p->state() == QProcess::NotRunning)
        return;
    const qint64 pid = p->processId();
    if (pid > 0)
        ::kill(-pid_t(pid), SIGTERM);
    else
        p->kill();
}

KdosTransaction::~KdosTransaction()
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->disconnect(this);
        endGroup(m_process);
        m_process->waitForFinished(3000);
    }
}

bool KdosTransaction::start()
{
    if (m_cancelled || m_finished)
        return false;
    runStep();
    return true;
}

void KdosTransaction::runStep()
{
    m_step++;
    if (m_step >= m_plan.steps.size()) {
        finish(DoneStatus);
        return;
    }
    const Step &step = m_plan.steps.at(m_step);
    m_meter = std::make_unique<StepMeter>(step.meter, step.units);
    m_stdout.clear();
    m_stderrTail.clear();

    if (step.downloads) {
        // QNetworkInformation answers only when a backend for it runs; with
        // none, the step is tried and its own error is the message.
        const QNetworkInformation *net = QNetworkInformation::instance();
        if (net && net->reachability() == QNetworkInformation::Reachability::Disconnected) {
            fail(i18n("Installing %1 needs a network connection, and this computer is offline.", resource()->name()));
            return;
        }
        setStatus(DownloadingStatus);
    } else {
        setStatus(CommittingStatus);
    }

    if (step.kind == Step::Kind::Packd) {
        qCDebug(LIBDISCOVER_BACKEND_KDOS_LOG) << "packd:" << step.request;
        auto req = PackdRequest::send(step.request, this);
        connect(req, &PackdRequest::finished, this, [this](const QByteArray &reply, const QString &error) {
            if (m_cancelled)
                return;
            QString why = error;
            if (why.isEmpty() && !replyOk(reply, &why)) {
                fail(why);
                return;
            }
            if (!why.isEmpty()) {
                fail(why);
                return;
            }
            stepSucceeded();
        });
        return;
    }

    qCDebug(LIBDISCOVER_BACKEND_KDOS_LOG) << "run:" << step.program << step.arguments;
    m_process = new QProcess(this);
    m_process->setProgram(step.program);
    m_process->setArguments(step.arguments);
    // Its own process group, so cancel reaches podman and apt below it.
    m_process->setChildProcessModifier([] {
        ::setpgid(0, 0);
    });
    connect(m_process, &QProcess::readyReadStandardOutput, this, &KdosTransaction::readOutput);
    connect(m_process, &QProcess::readyReadStandardError, this, &KdosTransaction::readErrors);
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart && !m_cancelled)
            fail(i18n("%1 could not be started: %2", m_process->program(), m_process->errorString()));
    });
    connect(m_process, &QProcess::finished, this, [this](int code, QProcess::ExitStatus how) {
        // What the step wrote last is in the pipes still: the closing line
        // its meter waits for, and the error line a failure is told by.
        readOutput();
        readErrors();
        QProcess *p = m_process;
        m_process = nullptr;
        p->deleteLater();
        if (m_cancelled)
            return;
        if (how != QProcess::NormalExit) {
            fail(i18n("%1 stopped before it finished.", p->program()));
            return;
        }
        if (code != 0) {
            const QString detail = m_stderrTail.isEmpty() ? i18n("exit status %1", code) : m_stderrTail.join(u' ');
            fail(detail);
            return;
        }
        stepSucceeded();
    });
    m_process->start();
}

// Progress is set at every line that moves the meter, so lines that arrive
// together still show each step they report.
void KdosTransaction::readOutput()
{
    if (!m_process)
        return;
    m_stdout += m_process->readAllStandardOutput();
    qsizetype nl;
    while ((nl = m_stdout.indexOf('\n')) >= 0) {
        const QString line = QString::fromUtf8(m_stdout.left(nl));
        m_stdout.remove(0, nl + 1);
        if (m_meter && m_meter->feed(line))
            updateProgress();
    }
}

void KdosTransaction::readErrors()
{
    if (!m_process)
        return;
    const QStringList got = QString::fromUtf8(m_process->readAllStandardError()).split(u'\n', Qt::SkipEmptyParts);
    for (const QString &l : got) {
        m_stderrTail << l.trimmed();
        if (m_stderrTail.size() > StderrTail)
            m_stderrTail.removeFirst();
    }
}

void KdosTransaction::updateProgress()
{
    const int total = qMax(1, m_plan.units());
    const int current = m_meter ? m_meter->done() : 0;
    setProgress(qBound(0, (m_unitsDone + current) * 100 / total, 100));
}

void KdosTransaction::stepSucceeded()
{
    m_unitsDone += qMax(1, m_plan.steps.at(m_step).units);
    m_meter.reset();
    updateProgress();
    runStep();
}

void KdosTransaction::fail(const QString &why)
{
    if (m_finished)
        return;
    qCWarning(LIBDISCOVER_BACKEND_KDOS_LOG) << resource()->name() << "failed:" << why;
    Q_EMIT passiveMessage(role() == RemoveRole ? i18n("%1 was not removed: %2", resource()->name(), why)
                                                : i18n("%1 was not installed: %2", resource()->name(), why));
    finish(DoneWithErrorStatus);
}

void KdosTransaction::cancel()
{
    if (m_finished)
        return;
    m_cancelled = true;
    endGroup(m_process);
    finish(CancelledStatus);
}

void KdosTransaction::finish(Status status)
{
    if (m_finished)
        return;
    m_finished = true;
    setCancellable(false);
    if (status == DoneStatus)
        setProgress(100);
    // The backend refreshes before the status is set, so the page shows the
    // machine as it now is when Discover drops the transaction.
    m_backend->transactionFinished(this, status);
}

}
