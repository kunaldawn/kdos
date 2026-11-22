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

#include <algorithm>

#include <signal.h>
#include <unistd.h>

using namespace Qt::StringLiterals;

namespace Kdos
{

// How many of a failed step's last standard-error lines reach the message.
static constexpr int StderrTail = 3;

// A failed step's last error lines as one reason. Each is one tool's
// sentence, often its own full stop at the end; they are joined with "; "
// and that stop dropped, so the lines of two tools read as two clauses.
static QString joinedErrors(const QStringList &lines)
{
    QStringList said;
    for (QString l : lines) {
        while (l.endsWith(u'.'))
            l.chop(1);
        if (!l.isEmpty())
            said << l;
    }
    return said.join(u"; "_s);
}

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

// A step run through pkexec is root once the password is in, and only its
// standard input closing stops it (the helper's CANCEL IS STDIN CLOSING);
// before that pkexec is still the user's, and the signal ends it and its
// password dialog.
static bool rootStep(const Step &step)
{
    return step.program == "pkexec"_L1;
}

KdosTransaction::~KdosTransaction()
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->disconnect(this);
        m_process->closeWriteChannel();
        endGroup(m_process);
        m_process->waitForFinished(3000);
    }
}

bool KdosTransaction::start()
{
    if (m_cancelled || m_finished)
        return false;
    static_cast<KdosResource *>(resource())->setFailure({});
    runStep();
    return true;
}

// Discover shows its offline sheet for every reachability but Online, and a
// downloading step follows it: a network with no route out (Local) or only to
// the site (Site) fetches no archive, and NetworkManager with networking
// switched off is asleep, which Qt reports as Unknown. A step let through
// would sit in Downloading until its tool gave up.
bool KdosTransaction::offline(QNetworkInformation::Reachability reachability)
{
    return reachability != QNetworkInformation::Reachability::Online;
}

// What a running step is doing: downloading when it fetches anything, else
// changing the machine.
static Transaction::Status runningStatus(const Step &step)
{
    return step.downloads ? Transaction::DownloadingStatus : Transaction::CommittingStatus;
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
    m_previewOut.clear();

    if (step.kind == Step::Kind::Ask) {
        setStatus(SetupStatus);
        m_asking = true;
        Q_EMIT proceedRequest(step.question, step.questionText);
        return;
    }
    if (step.kind == Step::Kind::Preview) {
        setStatus(SetupStatus);
    } else {
        // QNetworkInformation answers only once a backend for it is loaded,
        // which Discover's window does for its offline sheet; with none, the
        // step is tried and its own error is the message.
        const QNetworkInformation *net = QNetworkInformation::instance();
        if (step.downloads && net && offline(net->reachability())) {
            fail(i18n("it needs a network connection, and this computer is offline"));
            return;
        }
        // A step run through pkexec waits for the administrator's password
        // and writes nothing until it has it: it stays in SetupStatus, which
        // Discover says as "Waiting", until its first line of output
        // (readOutput), so Discover never says Downloading beside an open
        // password dialog.
        setStatus(rootStep(step) ? SetupStatus : runningStatus(step));
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
        if (m_cancelled) {
            finish(CancelledStatus);
            return;
        }
        if (how != QProcess::NormalExit) {
            fail(i18n("%1 stopped before it finished.", p->program()));
            return;
        }
        if (code != 0) {
            fail(m_stderrTail.isEmpty() ? i18n("exit status %1", code) : joinedErrors(m_stderrTail));
            return;
        }
        if (m_plan.steps.at(m_step).kind == Step::Kind::Preview) {
            ask();
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
    const Step &step = m_plan.steps.at(m_step);
    if (step.kind != Step::Kind::Preview && status() == SetupStatus && m_stdout.contains('\n'))
        setStatus(runningStatus(step));
    qsizetype nl;
    while ((nl = m_stdout.indexOf('\n')) >= 0) {
        const QString line = QString::fromUtf8(m_stdout.left(nl));
        if (m_plan.steps.at(m_step).kind == Step::Kind::Preview)
            m_previewOut += m_stdout.left(nl + 1);
        m_stdout.remove(0, nl + 1);
        // Everything a step fetches is fetched before its first package
        // goes in, so from either line on it is building or installing.
        if (step.kind != Step::Kind::Preview && status() == DownloadingStatus
            && (line.startsWith("==> installing "_L1) || line.startsWith("==> restoring the development files of "_L1)))
            setStatus(CommittingStatus);
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

// The question a Preview step's dry run becomes. A removal's names each
// package it takes out, the port first (`remove <pkg>`, update.c's dry run),
// and the question counts the others, so whoever approves it knows what goes
// with the port; output without the port's line is no removal anybody can be
// asked about, and fails before the password.
void KdosTransaction::ask()
{
    if (role() == RemoveRole) {
        const auto *res = static_cast<KdosResource *>(resource());
        const QByteArray want = "remove " + res->entry().id.toUtf8();
        const QList<QByteArray> lines = QByteArray(m_previewOut + m_stdout).split('\n');
        bool named = false;
        int others = 0;
        for (const QByteArray &l : lines) {
            const QByteArray t = l.trimmed();
            if (t == want)
                named = true;
            else if (t.startsWith("remove ") && t.size() > 7)
                ++others;
        }
        if (!named) {
            fail(i18n("what the removal would do could not be read"));
            return;
        }
        const QString name = resource()->name().toHtmlEscaped();
        m_asking = true;
        Q_EMIT proceedRequest(i18nc("@title:window", "Remove?"),
                              others ? i18np("<b>%2</b> is removed from this computer, with 1 package it brought that nothing else needs. "
                                             "Discover can install it again.",
                                             "<b>%2</b> is removed from this computer, with %1 packages it brought that nothing else needs. "
                                             "Discover can install it again.",
                                             others,
                                             name)
                                     : i18n("<b>%1</b> is removed from this computer. Discover can install it again.", name));
        return;
    }
    const InstallPreview p = parseInstallPreview(m_previewOut + m_stdout);
    if (!p.valid) {
        fail(i18n("what the install would do could not be read"));
        return;
    }
    m_asking = true;
    // The dialog is narrow and elides its title, so the program's name opens
    // the body, which wraps, and the title names no program.
    Q_EMIT proceedRequest(i18nc("@title:window", "Install?"),
                          i18nc("@info %1 is a program's name, %2 what installing it does",
                                "<b>%1</b>: %2",
                                resource()->name().toHtmlEscaped(),
                                installPreviewText(p)));
}

void KdosTransaction::proceed()
{
    if (!m_asking || m_cancelled || m_finished)
        return;
    m_asking = false;
    stepSucceeded();
}

void KdosTransaction::fail(const QString &why)
{
    if (m_finished)
        return;
    qCWarning(LIBDISCOVER_BACKEND_KDOS_LOG) << resource()->name() << "failed:" << why;
    // Discover shows a transaction's message only behind "See Technical
    // Details" of a generic error, so the reason is also the notice on the
    // resource's own page.
    const QString said = role() == RemoveRole ? i18n("%1 was not removed: %2", resource()->name(), why)
                                              : i18n("%1 was not installed: %2", resource()->name(), why);
    static_cast<KdosResource *>(resource())->setFailure(said);
    Q_EMIT passiveMessage(said);
    finish(DoneWithErrorStatus);
}

void KdosTransaction::cancel()
{
    if (m_finished)
        return;
    m_cancelled = true;
    // A running step through pkexec ends Cancelled only once the process
    // has: the backend's refresh after it must not meet a root run still
    // holding the package lock. Until then the status stays and the
    // button cannot be pressed again.
    if (m_process && m_process->state() != QProcess::NotRunning && rootStep(m_plan.steps.at(m_step))) {
        setCancellable(false);
        m_process->closeWriteChannel();
        endGroup(m_process);
        return;
    }
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
