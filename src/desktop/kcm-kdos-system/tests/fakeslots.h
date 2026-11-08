/*
 * ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   KD's Homebrew Linux Distro
 * ---------------------------------
 *
 * A fixture io.kdos.Slots1 on the session bus, for the KCM's tests.
 *
 * IT IMPLEMENTS THE ADAPTOR GENERATED FROM ../io.kdos.Slots1.xml, so a
 * member the contract drops or renames stops compiling here, and a member
 * the client calls that the contract lacks is refused as UnknownMethod.
 *
 * IT READS A FIXTURE DIRECTORY SHAPED LIKE WHAT THE REAL SERVER READS, and
 * writes the boot state in kdos-bootctl's own layout, so a test asserts on
 * the same file a person would `cat`:
 *
 *   bootstate             kdos-bootctl's state file (absent: Configured=false)
 *   cmdline               the kernel command line, for kdos_slot=
 *   slot-<x>/etc/os-release, slot-<x>/ports/TREE
 *                         what is inside each slot's root
 *   kernel-<x>            the version of the kernel the slot boots
 *   update.conf           /etc/kdos/update.conf
 *   rolledback            the slot whose last trial was abandoned
 *   job                   `<acquire|install> <done> <total>`
 *   offer                 the version CheckNew would offer
 *   acquired              `<slot> <version>`: an update prepared in a slot
 *                         and not yet installed
 *   error                 why the last update job stopped
 *   bless-userspace, bless-graphical
 *                         the two markers under /run/kdos/bless
 *   deny                  method names polkit refuses, one per line
 *   calls.log             every changing call, appended
 *
 * Nothing is cached: every call reads the directory again, so a test
 * changes the machine by writing a file.
 */

#pragma once

#include <QDBusContext>
#include <QList>
#include <QObject>
#include <QString>
#include <QVariantMap>

class FakeSlots : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_PROPERTY(bool Configured READ configured)
    Q_PROPERTY(QString RunningSlot READ runningSlot)
    Q_PROPERTY(QString ActiveSlot READ activeSlot)
    Q_PROPERTY(QString TrySlot READ trySlot)
    Q_PROPERTY(uint TryAttempts READ tryAttempts)
    Q_PROPERTY(bool TryBootNext READ tryBootNext)
    Q_PROPERTY(QString Channel READ channel)
    Q_PROPERTY(QVariantMap State READ state)

public:
    explicit FakeSlots(const QString &dir, QObject *parent = nullptr);

    bool configured() const;
    QString runningSlot() const;
    QString activeSlot() const;
    QString trySlot() const;
    uint tryAttempts() const;
    bool tryBootNext() const;
    QString channel() const;
    QVariantMap state() const;

public Q_SLOTS:
    QList<QVariantMap> ListSlots();
    void Try(const QString &slot);
    void CancelTry();
    void SetChannel(const QString &channel);
    void Bless();

Q_SIGNALS:
    void SlotsChanged();

private:
    struct Boot {
        bool have = false;
        QString slot[2];
        QString crypt[2];
        int active = 0;
        int trying = -1;
        int attempts = 0;
        bool bootnext = false;
    };

    Boot readBoot() const;
    bool writeBoot(const Boot &b) const;
    QString readFile(const QString &name) const;
    QString slotVersion(int i) const;
    bool denied(const QString &method);
    void log(const QString &line) const;
    void announce();
    QVariantMap slotMap(const Boot &b, int i) const;

    QString m_dir;
};
