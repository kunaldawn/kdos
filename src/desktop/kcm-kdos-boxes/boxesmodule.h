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
 * Boxes (kcm_kdos_boxes), under Security & Privacy → Application
 * Permissions: every box, and for one of them its permissions (network,
 * files, devices and graphics, resources, display grants), its colour and
 * its lifecycle.
 *
 * THE PROFILE IS A SETTING, THE LIFECYCLE IS AN ACT. Permissions and colour
 * are edited and Apply writes them; start, stop, recreate, remove and export
 * run when their button is pressed, each through kdos-box or kdos-appbox and
 * each reported in the banner. A lifecycle act with unapplied edits on the
 * page is refused with a sentence, so "Recreate" never makes a box from
 * settings the person has not applied.
 *
 * A BOX WITH NO CONTAINER IS MADE BY CREATE, NEVER BY START. `kdos-box
 * start` refuses a box that has no container, and only an application's box
 * (BoxRow::isAppBox) is made again by its application's next launch; any
 * other box removed with its settings kept comes back only through
 * `kdos-box create <box>`, which reads the kept profile. What Remove says it
 * leaves follows the same split.
 *
 * NO ROOT. A box is the user's own: its profile is in their home and podman
 * is rootless, so nothing here asks polkit.
 */

#pragma once

#include <KQuickConfigModule>

#include <QString>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

#include <functional>

namespace Kdos
{

class AccentModel;
class BoxListModel;
class BoxProfile;
class BoxTool;

class BoxesModule : public KQuickConfigModule
{
    Q_OBJECT
    Q_MOC_INCLUDE("accentmodel.h")
    Q_MOC_INCLUDE("boxlistmodel.h")
    Q_MOC_INCLUDE("boxprofile.h")
    Q_PROPERTY(Kdos::BoxListModel *boxes READ boxes CONSTANT)
    Q_PROPERTY(Kdos::AccentModel *accents READ accents CONSTANT)
    Q_PROPERTY(Kdos::BoxProfile *profile READ profile CONSTANT)
    Q_PROPERTY(QVariantList grants READ grants CONSTANT)
    Q_PROPERTY(QString currentBox READ currentBox NOTIFY currentBoxChanged)
    Q_PROPERTY(QVariantMap current READ current NOTIFY currentChanged)
    Q_PROPERTY(bool needsRecreate READ needsRecreate NOTIFY currentChanged)
    /* The create-time keys the box was not made with: those this page
     * changed (BoxProfile::pendingRecreate) and those kdos-box's report
     * names (BoxProfile::staleKeys), for a change made with `kdos-box
     * profile`. What the Recreate banner lists. */
    Q_PROPERTY(QStringList recreateKeys READ recreateKeys NOTIFY currentChanged)
    Q_PROPERTY(QString initialBox READ initialBox CONSTANT)
    Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged)
    Q_PROPERTY(QString progressText READ progressText NOTIFY progressTextChanged)
    Q_PROPERTY(QString message READ message NOTIFY messageChanged)
    Q_PROPERTY(int messageType READ messageType NOTIFY messageChanged)
    /* The profile's `!` lines about no one control, less the one that says
     * what the Recreate banner already says while that banner shows. */
    Q_PROPERTY(QStringList generalNotes READ generalNotes NOTIFY generalNotesChanged)

public:
    /* Kirigami.MessageType's values. */
    enum MessageType {
        NoMessage = -1,
        Information = 0,
        Positive = 1,
        Warning = 2,
        Error = 3,
    };
    Q_ENUM(MessageType)

    BoxesModule(QObject *parent, const KPluginMetaData &metaData, const QVariantList &args);

    BoxListModel *boxes() const;
    AccentModel *accents() const;
    BoxProfile *profile() const;
    QVariantList grants() const;
    QString currentBox() const;
    QVariantMap current() const;
    bool needsRecreate() const;
    QStringList recreateKeys() const;
    QString initialBox() const;
    bool isBusy() const;
    QString progressText() const;
    QString message() const;
    int messageType() const;
    QStringList generalNotes() const;

    /* The `!` line kdos-box prints for a profile written after its box was
     * created. */
    static bool isRecreateNote(const QString &note);

    /* Loads one box's profile and report; false for a name kdos-box would
     * refuse. Edits on the page are discarded, so QML asks first. */
    Q_INVOKABLE bool openBox(const QString &name);
    Q_INVOKABLE void closeBox();

    /* `kdos-box create <box>` over the profile, for a box with none. */
    Q_INVOKABLE void createBox();
    Q_INVOKABLE void startBox();
    Q_INVOKABLE void stopBox();
    Q_INVOKABLE void restartBox();
    Q_INVOKABLE void recreateBox();
    Q_INVOKABLE void removeBox();
    Q_INVOKABLE void exportBox(const QUrl &target);
    /* A terminal inside the box, `kdos-box enter <box> --window`, left
     * running on its own. */
    Q_INVOKABLE void openTerminal();
    Q_INVOKABLE void dismissMessage();

public Q_SLOTS:
    void load() override;
    void save() override;
    void defaults() override;

Q_SIGNALS:
    void currentBoxChanged();
    void currentChanged();
    void busyChanged();
    void progressTextChanged();
    void messageChanged();
    void generalNotesChanged();
    /* A System Settings already showing this page was asked for `box`; the
     * page opens it the way a click on its row would. */
    void boxRequested(const QString &box);

public:
    /* The box a module argument list names, or "" when its first argument
     * is not a box name. Public for the tests. */
    static QString boxFromArgs(const QVariantList &args);

private:
    void refreshCurrent();
    void loadReport();
    void setMessage(const QString &text, MessageType type);
    bool refuseWhileEditing();
    void runLifecycle(const QList<std::pair<QString, QStringList>> &steps, const QString &working, std::function<void(int exitCode, int step, const QString &lastLine)> done);

    BoxTool *m_tool;
    BoxListModel *m_boxes;
    AccentModel *m_accents;
    BoxProfile *m_profile;
    QString m_current;
    QVariantMap m_currentRow;
    QString m_initialBox;
    QString m_progress;
    QString m_message;
    MessageType m_messageType = NoMessage;
    int m_reportSerial = 0;
};

}
