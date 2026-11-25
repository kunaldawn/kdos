/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-discover — a plan run as one Discover transaction
 *
 * STEPS RUN ONE AFTER ANOTHER, AND TRANSACTIONS ONE AT A TIME. Two store
 * installs would build the same runtime image twice, and a box create racing
 * the install of its own pack would compose the old copy; the backend queues
 * every transaction behind the one running.
 *
 * NOTHING ROLLS BACK, the rule of every tool underneath. A step that fails
 * stops the plan, and what the earlier steps made stays made: an image built
 * is not harmed by a later box create failing, and the next refresh shows the
 * machine as it is.
 *
 * A PREVIEW STEP WAITS FOR AN ANSWER. Its dry run's output becomes the
 * question Discover's proceed dialog asks; Proceed runs the next step and
 * Cancel, or the dialog closed, ends the transaction with nothing run. An Ask
 * step is the same question with no dry run: the plan wrote its title and
 * text, and it starts no process.
 *
 * A STEP THAT DOWNLOADS IS REFUSED OFFLINE BEFORE IT STARTS, with the network
 * named as the reason, whenever Qt's network information says anything but
 * Online (offline()), as Discover's own offline sheet does.
 *
 * CANCEL ENDS THE RUNNING STEP'S WHOLE PROCESS GROUP. kdos-appbox runs podman
 * and podman runs apt; a signal to kdos-appbox alone would leave the build
 * going with nobody reading its output. A step through pkexec is root once
 * the password is in, and the user's signal is refused: every step starts
 * with its standard input an open pipe, and cancel closes it, which the
 * ports helper takes as its stop (kdos-ports-helper.c, CANCEL IS STDIN
 * CLOSING). That transaction ends Cancelled only when the process has
 * exited, with the button disabled until then, so the backend's refresh
 * after it never meets a root run still holding the package lock.
 *
 * A STEP SAYS DOWNLOADING UNTIL ITS FIRST `==> installing` OR `==> restoring
 * the development files of` LINE. `kdos update install` fetches everything
 * before it installs anything, and restoring a package built here rebuilds
 * it, so from either line on the step is building or installing, which
 * Discover says as Installing.
 * ---------------------------------
 */

#pragma once

#include "KdosMeter.h"
#include "KdosPlan.h"

#include <Transaction/Transaction.h>

#include <QNetworkInformation>
#include <QProcess>
#include <QStringList>

#include <memory>

namespace Kdos
{

class KdosBackend;
class KdosResource;

class KdosTransaction : public Transaction
{
    Q_OBJECT
public:
    KdosTransaction(KdosResource *resource, Role role, const Plan &plan, KdosBackend *backend);
    ~KdosTransaction() override;

    // Called by the backend's queue when this transaction's turn comes; false
    // when it was cancelled while it waited, and the queue moves on.
    bool start();
    void cancel() override;
    void proceed() override;

    // Whether a reachability refuses a step that downloads.
    static bool offline(QNetworkInformation::Reachability reachability);

private:
    void runStep();
    void stepSucceeded();
    void fail(const QString &why);
    void finish(Status status);
    void readOutput();
    void readErrors();
    void updateProgress();
    void ask();

    KdosBackend *m_backend;
    Plan m_plan;
    int m_step = -1;
    int m_unitsDone = 0;
    std::unique_ptr<StepMeter> m_meter;
    QProcess *m_process = nullptr;
    QByteArray m_stdout;
    QStringList m_stderrTail;
    QByteArray m_previewOut; // a Preview step's whole output
    bool m_asking = false; // a Preview or Ask step's question is open
    bool m_cancelled = false;
    bool m_finished = false;
};

}
