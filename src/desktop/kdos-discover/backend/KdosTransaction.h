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
 * CANCEL ENDS THE RUNNING STEP'S WHOLE PROCESS GROUP. kdos-appbox runs podman
 * and podman runs apt; a signal to kdos-appbox alone would leave the build
 * going with nobody reading its output.
 * ---------------------------------
 */

#pragma once

#include "KdosMeter.h"
#include "KdosPlan.h"

#include <Transaction/Transaction.h>

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

private:
    void runStep();
    void stepSucceeded();
    void fail(const QString &why);
    void finish(Status status);
    void readOutput();
    void readErrors();
    void updateProgress();

    KdosBackend *m_backend;
    Plan m_plan;
    int m_step = -1;
    int m_unitsDone = 0;
    std::unique_ptr<StepMeter> m_meter;
    QProcess *m_process = nullptr;
    QByteArray m_stdout;
    QStringList m_stderrTail;
    bool m_cancelled = false;
    bool m_finished = false;
};

}
