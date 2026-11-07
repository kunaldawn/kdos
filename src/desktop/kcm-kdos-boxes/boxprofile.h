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
 * One box's profile, as the page edits it.
 *
 * READ FROM THE FILE, REPORTED BY kdos-box, WRITTEN BY kdos-box. The values
 * come from ~/.config/kdos/boxes/<box>.conf, read from $HOME/.config and not
 * $XDG_CONFIG_HOME because that is where kdos-appbox and KWin's grant check
 * read it, and two readers resolving it differently is the failure this
 * layout exists to prevent. What each key is enforced by, and every `!`
 * line about a setting that cannot be delivered, come from
 * `kdos-box profile <box>`, which prints them; the page shows each `!` line
 * beside the control it is about. A change is written by
 * `kdos-box profile <box> key=value …` with only the keys that changed, so a
 * key this page does not show keeps whatever it held.
 *
 * Values are normalised exactly as kdos-box's profile_set() reads them, so a
 * hand-written `network = yes` shows as private, which is what it does.
 *
 * SOME KEYS APPLY ONLY WHEN THE BOX IS CREATED. A namespace or a volume
 * cannot be changed on a live container; such a change is written, and the
 * box keeps its old setting until it is removed and created again. The keys
 * are createTimeKeys(); a change to one on a box that has a container is
 * remembered in this page's state file until the box is made again, which is
 * what the "Recreate" banner shows.
 */

#pragma once

#include <QDateTime>
#include <QObject>
#include <QQmlPropertyMap>
#include <QString>
#include <QStringList>
#include <QVariantMap>

namespace Kdos
{

/* ~/.config/kdos/boxes, from $HOME. */
QString boxProfileDir();

class BoxProfile : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString name READ name NOTIFY loaded)
    Q_PROPERTY(bool described READ isDescribed NOTIFY loaded)
    Q_PROPERTY(QQmlPropertyMap *values READ values CONSTANT)
    Q_PROPERTY(QQmlPropertyMap *mechanisms READ mechanisms CONSTANT)
    Q_PROPERTY(QQmlPropertyMap *notes READ notes CONSTANT)
    Q_PROPERTY(QStringList generalNotes READ generalNotes NOTIFY notesChanged)
    Q_PROPERTY(bool notesLoaded READ notesLoaded NOTIFY notesChanged)
    Q_PROPERTY(QStringList otherGrants READ otherGrants NOTIFY loaded)
    Q_PROPERTY(bool dirty READ isDirty NOTIFY dirtyChanged)
    Q_PROPERTY(bool atDefaults READ isAtDefaults NOTIFY dirtyChanged)
    Q_PROPERTY(QStringList pendingRecreate READ pendingRecreate NOTIFY pendingChanged)
    Q_PROPERTY(int cpuCount READ cpuCount CONSTANT)

public:
    explicit BoxProfile(QObject *parent = nullptr);

    /* The keys the page edits, in the order kdos-box writes them. */
    static QStringList editableKeys();
    /* The keys a running container cannot take. */
    static QStringList createTimeKeys();
    /* kdos-box's profile_defaults(), typed. */
    static QVariantMap defaultValues();
    /* The grant names KWin's sandbox policy knows, in the order written. */
    static QStringList grantVocabulary();

    /* A file's text for one key, read the way profile_set() reads it. */
    static QVariant normalize(const QString &key, const QString &raw);
    /* A typed value spelled for `kdos-box profile <box> key=<this>`. */
    static QString spell(const QString &key, const QVariant &value);
    static int parseMemoryMiB(const QString &raw);
    static QString memorySpelling(int mib);
    static int parseCpuTenths(const QString &raw);
    static QString cpuSpelling(int tenths);
    static int parseDurationSeconds(const QString &raw);

    /* Parses `kdos-box profile <box>` output into mechanisms and notes. */
    void applyReport(const QString &report);
    /* Reads the profile file; the report is applied separately. */
    void loadFile(const QString &name);

    QString name() const;
    bool isDescribed() const;
    QQmlPropertyMap *values() const;
    QQmlPropertyMap *mechanisms() const;
    QQmlPropertyMap *notes() const;
    QStringList generalNotes() const;
    bool notesLoaded() const;
    QStringList otherGrants() const;
    bool isDirty() const;
    bool isAtDefaults() const;

    QStringList changedKeys() const;
    /* `key=value` words for the keys that changed. */
    QStringList changedArguments() const;

    void resetToDefaults();
    void discardEdits();

    /* The page's own memory of create-time changes not yet applied. */
    QStringList pendingRecreate() const;
    void addPendingRecreate(const QStringList &keys);
    /* The same for a box this object is not showing: a save that finishes
     * after the page has moved to another box still records its keys. */
    static void recordPendingRecreate(const QString &box, const QStringList &keys);
    void clearPendingRecreate();
    /* Forget the pending keys once the box has been made since they were
     * recorded, or has no container any more. */
    void reconcilePending(bool hasContainer, const QDateTime &created);

    int cpuCount() const;

    /* For the spin boxes: what they show, and what a typed text means. */
    Q_INVOKABLE QString formatDuration(int seconds) const;
    Q_INVOKABLE QString formatMemory(int mib) const;
    Q_INVOKABLE QString formatCpus(int tenths) const;
    Q_INVOKABLE int memoryFromText(const QString &text) const;
    Q_INVOKABLE int durationFromText(const QString &text) const;
    Q_INVOKABLE int cpusFromText(const QString &text) const;

Q_SIGNALS:
    void loaded();
    void notesChanged();
    void dirtyChanged();
    void pendingChanged();

private:
    void setValues(const QVariantMap &v);
    void recomputeDirty();
    void loadPending();
    void savePending(const QStringList &keys, const QDateTime &since);

    QString m_name;
    bool m_described = false;
    QVariantMap m_saved;
    QStringList m_otherGrants;
    QQmlPropertyMap *m_values;
    QQmlPropertyMap *m_mechanisms;
    QQmlPropertyMap *m_notes;
    QStringList m_generalNotes;
    bool m_notesLoaded = false;
    bool m_dirty = false;
    bool m_atDefaults = true;
    QStringList m_pending;
    QDateTime m_pendingSince;
};

}
