/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   ksystemstats-kdos — the `kdos` sensors of System Monitor
 *
 *     kdos/box/<box>/{name,cpu,memory,memoryLimit,ioRead,ioWrite,processes,energy}
 *     kdos/box/all/{boxes,cpu,memory,ioRead,ioWrite,processes}
 *     kdos/energy/<app>[@<box>]/{name,app,box,impact,cpuTime,gpu}
 *     kdos/energy/all/{status,window,idleFloor,attributable,shortLived,domains}
 *     kdos/slot/{status,running,runningVersion,runningState,other,otherVersion,otherState,next,trial,attempts}
 *     kdos/look/phosphor
 *
 * One container, `kdos`, whose object ids carry a `/` of their own:
 * ksystemstats splits a sensor path at its first and last `/`, so the object
 * of `kdos/box/www/cpu` is `box/www`, and System Monitor's sensor tree shows
 * KDOS ▸ Boxes ▸ Box www ▸ CPU. The middle node takes its title from the
 * sensorless object `box`; `energy` titles the energy rows' node the same way.
 *
 * It is built against libksysguard's SensorPlugin, which is not a stable API:
 * the plugin is rebuilt and its tests rerun on every Plasma minor release.
 */

#pragma once

#include <systemstats/AggregateSensor.h>
#include <systemstats/SensorContainer.h>
#include <systemstats/SensorObject.h>
#include <systemstats/SensorPlugin.h>
#include <systemstats/SensorProperty.h>

#include <KSharedConfig>

#include <QDateTime>
#include <QElapsedTimer>
#include <QHash>
#include <QPointer>

#include "kdosstats.h"

class QLocalSocket;

namespace Kdos
{

class BoxObject : public KSysGuard::SensorObject
{
    Q_OBJECT
public:
    BoxObject(const QString &box, KSysGuard::SensorContainer *parent);

    /* One sample of the box's scopes; rates are taken against the last one. */
    void sample(const CgroupCounters &now, qint64 nowMs, int cpus);
    void setLauncher(const BoxLauncher &launcher);
    void setEnergy(double fraction);

    const QString box;

private:
    KSysGuard::SensorProperty *m_name;
    KSysGuard::SensorProperty *m_icon;
    KSysGuard::SensorProperty *m_cpu;
    KSysGuard::SensorProperty *m_memory;
    KSysGuard::SensorProperty *m_memoryLimit;
    KSysGuard::SensorProperty *m_ioRead;
    KSysGuard::SensorProperty *m_ioWrite;
    KSysGuard::SensorProperty *m_processes;
    KSysGuard::SensorProperty *m_energy;

    CgroupCounters m_last;
    qint64 m_lastMs = -1;
};

class EnergyObject : public KSysGuard::SensorObject
{
    Q_OBJECT
public:
    EnergyObject(const EnergyApp &app, KSysGuard::SensorContainer *parent);
    void set(const EnergyApp &app, const QString &label);

private:
    KSysGuard::SensorProperty *m_name;
    KSysGuard::SensorProperty *m_app;
    KSysGuard::SensorProperty *m_box;
    KSysGuard::SensorProperty *m_impact;
    KSysGuard::SensorProperty *m_cpuTime;
    KSysGuard::SensorProperty *m_gpu;
};

class KdosPlugin : public KSysGuard::SensorPlugin
{
    Q_OBJECT
public:
    KdosPlugin(QObject *parent, const QVariantList &args);
    ~KdosPlugin() override;

    QString providerName() const override
    {
        return QStringLiteral("kdos");
    }

    void update() override;

private:
    void updateBoxes(qint64 nowMs);
    void updateSlots();
    void updatePhosphor();
    void requestEnergy();
    void readEnergyNow();
    void applyEnergy(const EnergyReport &report);
    bool energyWanted() const;

    KSysGuard::SensorContainer *m_container;

    /* boxes */
    QString m_cgroupRoot;
    QString m_procRoot;
    QHash<QString, BoxObject *> m_boxes;
    QHash<QString, BoxLauncher> m_launchers;
    QElapsedTimer m_launcherAge;
    KSysGuard::SensorObject *m_allBoxes;
    KSysGuard::SensorProperty *m_boxCount;

    /* energy */
    QString m_energySocket;
    QPointer<QLocalSocket> m_energyRequest;
    QByteArray m_energyReply;
    QElapsedTimer m_energyAge;
    QHash<QString, EnergyObject *> m_energy;
    QHash<QString, double> m_boxEnergy;
    KSysGuard::SensorObject *m_energyAll;
    KSysGuard::SensorProperty *m_energyStatus;
    KSysGuard::SensorProperty *m_energyWindow;
    KSysGuard::SensorProperty *m_energyFloor;
    KSysGuard::SensorProperty *m_energyAttributable;
    KSysGuard::SensorProperty *m_energyShortLived;
    KSysGuard::SensorProperty *m_energyDomains;

    /* slots */
    QString m_slotFile;
    QDateTime m_slotStamp;
    bool m_slotRead = false;
    KSysGuard::SensorObject *m_slot;
    KSysGuard::SensorProperty *m_slotStatus;
    KSysGuard::SensorProperty *m_slotRunning;
    KSysGuard::SensorProperty *m_slotRunningVersion;
    KSysGuard::SensorProperty *m_slotRunningState;
    KSysGuard::SensorProperty *m_slotOther;
    KSysGuard::SensorProperty *m_slotOtherVersion;
    KSysGuard::SensorProperty *m_slotOtherState;
    KSysGuard::SensorProperty *m_slotNext;
    KSysGuard::SensorProperty *m_slotTrial;
    KSysGuard::SensorProperty *m_slotAttempts;

    /* look */
    KSharedConfig::Ptr m_kwinrc;
    QString m_kwinrcPath;
    QDateTime m_kwinrcStamp;
    bool m_kwinrcRead = false;
    KSysGuard::SensorObject *m_look;
    KSysGuard::SensorProperty *m_phosphor;
};

} // namespace Kdos
