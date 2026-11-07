/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   backend — the graphical installer's one door into kinstall
 *
 * Everything this program knows about the machine and everything it does to
 * it goes through `kinstall`, the same binary the terminal wizard is: the
 * pages are built from its dumps (`probe`, `look`, `apps`, `plan`, each as
 * JSON) and the install is `pkexec kinstall --config - --unattended --events
 * json`, the answers on its standard input. Nothing here partitions, formats
 * or writes a file of the target, so the two front ends cannot disagree about
 * what an install is.
 *
 * NO SHELL. Every program runs from an argument list through QProcess; a
 * disk path, a user name or a password is an argument or a line on a pipe,
 * never part of a command string.
 */

#pragma once

#include <QObject>
#include <QProcess>
#include <QStringList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <functional>

#include "answers.h"
#include "eventstream.h"
#include "keyboardmodel.h"

namespace Kdos
{

class Backend : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(Kdos::Answers *answers READ answers CONSTANT)
    Q_PROPERTY(Kdos::EventStream *events READ events CONSTANT)
    Q_PROPERTY(Kdos::KeyboardModel *keyboards READ keyboards CONSTANT)

    Q_PROPERTY(QVariantMap probe READ probe NOTIFY probeChanged)
    Q_PROPERTY(QVariantMap look READ look NOTIFY lookChanged)
    Q_PROPERTY(QVariantMap apps READ apps NOTIFY appsChanged)
    Q_PROPERTY(QVariantMap plan READ plan NOTIFY planChanged)
    /* A dump is being read; the page that asked waits for it. */
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    /* The last dump's failure, or "". */
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

    Q_PROPERTY(QStringList timeZones READ timeZones CONSTANT)
    Q_PROPERTY(QString systemTimeZone READ systemTimeZone CONSTANT)

    Q_PROPERTY(bool partitionManagerAvailable READ partitionManagerAvailable CONSTANT)
    Q_PROPERTY(bool partitionManagerRunning READ partitionManagerRunning NOTIFY partitionManagerRunningChanged)

    Q_PROPERTY(bool installing READ installing NOTIFY installingChanged)

public:
    explicit Backend(QObject *parent = nullptr);
    ~Backend() override;

    Answers *answers() const { return m_answers; }
    EventStream *events() const { return m_events; }
    KeyboardModel *keyboards() const { return m_keyboards; }

    QVariantMap probe() const { return m_probe; }
    QVariantMap look() const { return m_look; }
    QVariantMap apps() const { return m_apps; }
    QVariantMap plan() const { return m_plan; }
    bool busy() const { return !m_dumps.isEmpty(); }
    QString error() const { return m_error; }
    QStringList timeZones() const;
    QString systemTimeZone() const;
    bool partitionManagerAvailable() const;
    bool partitionManagerRunning() const { return m_partitionManager != nullptr; }
    bool installing() const { return m_install != nullptr; }

    /* What the machine is: its disks and partitions, its firmware, whether
     * this is the live medium. */
    Q_INVOKABLE void refreshProbe();
    /* The accents and desks the backend accepts. */
    Q_INVOKABLE void refreshLook();
    /* The application groups, ticked as the answers tick them. */
    Q_INVOKABLE void refreshApps();
    /* What these answers would do, and whether they are refused. */
    Q_INVOKABLE void refreshPlan();

    /* Partition Manager, for a layout the guided plans do not make; the
     * disks are probed again when it closes. */
    Q_INVOKABLE void openPartitionManager();

    Q_INVOKABLE void startInstall();
    /* Restart through login1, which asks polkit like any other session's
     * request would. */
    Q_INVOKABLE void reboot();

    /* An accent's colours out of its KDOS<Label> colour scheme, for a
     * swatch: window, view, text, highlight, highlightedText. Empty when
     * the scheme is not installed. */
    Q_INVOKABLE QVariantMap accentColors(const QString &accent) const;
    /* The global theme's preview picture, as a file URL, or "". */
    Q_INVOKABLE QString accentPreview(const QString &lookAndFeel) const;
    /* A size in bytes, as a person reads it. */
    Q_INVOKABLE QString formatSize(double bytes) const;

Q_SIGNALS:
    void probeChanged();
    void lookChanged();
    void appsChanged();
    void planChanged();
    void busyChanged();
    void errorChanged();
    void partitionManagerRunningChanged();
    void installingChanged();
    void installFinished(bool ok);

private:
    using Done = std::function<void(const QVariantMap &)>;
    void runDump(const QString &what, bool withAnswers, Done done);
    void setError(const QString &e);

    Answers *m_answers;
    EventStream *m_events;
    KeyboardModel *m_keyboards;

    QVariantMap m_probe, m_look, m_apps, m_plan;
    QString m_error;
    QHash<QString, QProcess *> m_dumps;
    QProcess *m_partitionManager = nullptr;
    QProcess *m_install = nullptr;
};

}
