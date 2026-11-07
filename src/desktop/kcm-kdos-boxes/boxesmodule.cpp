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
 */

#include "boxesmodule.h"

#include "accentmodel.h"
#include "boxlistmodel.h"
#include "boxprofile.h"
#include "boxtool.h"

#include <KLocalizedString>

using namespace Qt::StringLiterals;

namespace Kdos
{

BoxesModule::BoxesModule(QObject *parent, const KPluginMetaData &metaData, const QVariantList &args)
    : KQuickConfigModule(parent, metaData)
    , m_tool(new BoxTool(this))
    , m_boxes(new BoxListModel(m_tool, this))
    , m_accents(new AccentModel(this))
    , m_profile(new BoxProfile(this))
{
    setButtons(Help | Apply | Default);

    /* `systemsettings kcm_kdos_boxes --args <box>` opens that box: the link
     * Discover's "Runs in a box" badge follows. Anything that is not a box
     * name is ignored, never passed on. A System Settings already showing
     * this page passes the next request's arguments as activationRequested
     * instead of building the page again. */
    m_initialBox = boxFromArgs(args);
    connect(this, &KQuickConfigModule::activationRequested, this, [this](const QVariantList &again) {
        const QString box = boxFromArgs(again);
        if (!box.isEmpty()) {
            Q_EMIT boxRequested(box);
        }
    });

    connect(m_profile, &BoxProfile::dirtyChanged, this, [this] {
        setNeedsSave(!m_current.isEmpty() && m_profile->isDirty());
        setRepresentsDefaults(m_current.isEmpty() || m_profile->isAtDefaults());
    });
    connect(m_profile, &BoxProfile::pendingChanged, this, &BoxesModule::currentChanged);
    connect(m_boxes, &BoxListModel::refreshed, this, &BoxesModule::refreshCurrent);
    connect(m_tool, &BoxTool::progress, this, [this](const QString &line) {
        if (!m_progress.isEmpty() && !line.isEmpty()) {
            m_progress = line;
            Q_EMIT progressTextChanged();
        }
    });
}

QString BoxesModule::boxFromArgs(const QVariantList &args)
{
    if (args.isEmpty()) {
        return QString();
    }
    const QString first = args.first().toString();
    return BoxTool::isValidBoxName(first) ? first : QString();
}

BoxListModel *BoxesModule::boxes() const
{
    return m_boxes;
}

AccentModel *BoxesModule::accents() const
{
    return m_accents;
}

BoxProfile *BoxesModule::profile() const
{
    return m_profile;
}

QVariantList BoxesModule::grants() const
{
    auto grant = [](const QString &id, const QString &label, const QString &description, bool desktopWide) {
        return QVariantMap{{u"id"_s, id}, {u"label"_s, label}, {u"description"_s, description}, {u"desktopWide"_s, desktopWide}};
    };
    return {
        grant(u"window-list"_s,
              i18nc("@option:check box grant", "See and manage other windows"),
              i18nc("@info", "Lists every window on the desktop and can activate, move or close them."),
              true),
        grant(u"fake-input"_s,
              i18nc("@option:check box grant", "Type and click in other applications"),
              i18nc("@info", "Sends keys and pointer movement to whatever window is in front."),
              true),
        grant(u"screencast"_s,
              i18nc("@option:check box grant", "Record the screen without asking"),
              i18nc("@info", "Captures the screen directly. Without it the box asks through the screen-sharing dialog."),
              true),
        grant(u"data-control"_s,
              i18nc("@option:check box grant", "Read and change the clipboard at any time"),
              i18nc("@info", "What a clipboard manager needs. Copy and paste in its own windows works without it."),
              true),
        grant(u"layer-shell"_s,
              i18nc("@option:check box grant", "Draw panels and overlays on the desktop"),
              i18nc("@info", "Places surfaces above or below every window, as a panel or a dock does."),
              false),
        grant(u"output-management"_s,
              i18nc("@option:check box grant", "Change display settings"),
              i18nc("@info", "Changes the resolution, position and scale of the screens."),
              false),
        grant(u"dpms"_s,
              i18nc("@option:check box grant", "Turn screens on and off"),
              i18nc("@info", "Switches the screens' power."),
              false),
        grant(u"plasma-shell"_s,
              i18nc("@option:check box grant", "Place windows like desktop components"),
              i18nc("@info", "Positions its windows itself, as the panel and the desktop do."),
              false),
        grant(u"input-method"_s,
              i18nc("@option:check box grant", "Act as an input method"),
              i18nc("@info", "An input method sees every key typed in every application. No other grant includes it."),
              true),
        grant(u"secrets"_s,
              i18nc("@option:check box grant", "Use the password store"),
              i18nc("@info", "Reads and writes passwords kept in the wallet through the Secret Service."),
              true),
    };
}

QString BoxesModule::currentBox() const
{
    return m_current;
}

QVariantMap BoxesModule::current() const
{
    return m_currentRow;
}

bool BoxesModule::needsRecreate() const
{
    return !m_current.isEmpty() && !m_profile->pendingRecreate().isEmpty() && m_currentRow.value(u"created"_s).toBool();
}

QString BoxesModule::initialBox() const
{
    return m_initialBox;
}

bool BoxesModule::isBusy() const
{
    /* A lifecycle command, not the listing that refreshes on a timer: the
     * buttons must not blink every time podman is asked for the state. */
    return !m_progress.isEmpty();
}

QString BoxesModule::progressText() const
{
    return m_progress;
}

QString BoxesModule::message() const
{
    return m_message;
}

int BoxesModule::messageType() const
{
    return m_messageType;
}

void BoxesModule::setMessage(const QString &text, MessageType type)
{
    m_message = text;
    m_messageType = text.isEmpty() ? NoMessage : type;
    Q_EMIT messageChanged();
}

void BoxesModule::dismissMessage()
{
    setMessage(QString(), NoMessage);
}

bool BoxesModule::openBox(const QString &name)
{
    if (!BoxTool::isValidBoxName(name)) {
        setMessage(i18nc("@info %1 name", "“%1” is not a box name: a box is named with letters, digits, dots, dashes and underscores.", name), Error);
        return false;
    }
    dismissMessage();
    m_current = name;
    m_profile->loadFile(name);
    loadReport();
    refreshCurrent();
    setNeedsSave(false);
    setRepresentsDefaults(m_profile->isAtDefaults());
    Q_EMIT currentBoxChanged();
    return true;
}

void BoxesModule::closeBox()
{
    m_current.clear();
    m_currentRow.clear();
    setNeedsSave(false);
    setRepresentsDefaults(true);
    Q_EMIT currentBoxChanged();
    Q_EMIT currentChanged();
}

void BoxesModule::loadReport()
{
    const int serial = ++m_reportSerial;
    const QString name = m_current;
    m_tool->run(u"kdos-box"_s, {u"profile"_s, name}, [this, serial, name](const BoxTool::Result &r) {
        if (serial != m_reportSerial || name != m_current) {
            return;
        }
        if (r.exitCode == 0) {
            m_profile->applyReport(r.output);
        } else {
            setMessage(i18nc("@info %1 box, %2 error", "kdos-box could not report on %1: %2", name, r.lastLine), Warning);
        }
    });
}

void BoxesModule::refreshCurrent()
{
    if (m_current.isEmpty()) {
        return;
    }
    const BoxRow *row = m_boxes->row(m_current);
    QVariantMap m;
    m.insert(u"listed"_s, row != nullptr);
    if (row) {
        m.insert(u"displayName"_s, row->displayName);
        m.insert(u"iconName"_s, row->iconName);
        m.insert(u"base"_s, row->base);
        m.insert(u"state"_s, row->state);
        m.insert(u"stateText"_s, BoxListModel::stateText(row->state));
        m.insert(u"running"_s, row->isRunning());
        m.insert(u"created"_s, row->hasContainer());
        /* Only a pack box writes outside its container (kdos-packd's
         * upper); a store box or a distrobox one keeps every write inside,
         * so removing its container deletes them. */
        m.insert(u"writesInside"_s, !row->base.startsWith(u"pack:"_s));
        m.insert(u"confinement"_s, row->confinement);
        m.insert(u"confinementText"_s, BoxListModel::confinementText(row->confinement));
        m.insert(u"confinementReason"_s, row->confinementReason);
        m.insert(u"exportId"_s, row->exportId());
        m.insert(u"disk"_s, row->diskText());
        m_profile->reconcilePending(row->hasContainer(), row->created);
    } else {
        m.insert(u"displayName"_s, m_current);
        m.insert(u"iconName"_s, u"preferences-security"_s);
        m.insert(u"stateText"_s, m_boxes->isLoaded() ? BoxListModel::stateText(QString()) : QString());
        m.insert(u"running"_s, false);
        m.insert(u"created"_s, false);
    }
    m_currentRow = m;
    Q_EMIT currentChanged();
}

void BoxesModule::load()
{
    dismissMessage();
    m_boxes->refresh();
    if (!m_current.isEmpty()) {
        m_profile->loadFile(m_current);
        loadReport();
    }
}

void BoxesModule::save()
{
    if (m_current.isEmpty()) {
        setNeedsSave(false);
        return;
    }
    const QStringList changed = m_profile->changedKeys();
    const QStringList args = m_profile->changedArguments();
    if (args.isEmpty()) {
        setNeedsSave(false);
        return;
    }
    QStringList createTime;
    for (const QString &k : changed) {
        if (BoxProfile::createTimeKeys().contains(k)) {
            createTime << k;
        }
    }
    const bool hadContainer = m_currentRow.value(u"created"_s).toBool();
    const QString name = m_current;

    m_tool->run(
        u"kdos-box"_s,
        QStringList{u"profile"_s, name} + args,
        [this, name, createTime, hadContainer](const BoxTool::Result &r) {
            /* The page may have moved to another box while this ran (Apply in
             * the switch prompt); the result still belongs to `name`. */
            const bool shown = name == m_current;
            if (r.exitCode != 0) {
                setMessage(i18nc("@info %1 box, %2 error", "The settings of %1 were not written: %2", name, r.lastLine), Error);
                if (shown) {
                    setNeedsSave(m_profile->isDirty());
                }
                return;
            }
            if (hadContainer && !createTime.isEmpty()) {
                if (shown) {
                    m_profile->addPendingRecreate(createTime);
                } else {
                    BoxProfile::recordPendingRecreate(name, createTime);
                }
            }
            /* Nothing more to tell anybody. A new accent is the box's KWin
             * window rule, which `kdos-box profile` writes again and tells
             * KWin about itself; a grant is read by KWin when a client of the
             * box first binds a filtered global, so it reaches the box's next
             * window. */
            if (shown) {
                m_profile->loadFile(name);
                /* `kdos-box profile <box> key=value` prints the profile it
                 * wrote, which is the report for the new values. */
                m_profile->applyReport(r.output);
            }
            m_boxes->refresh();
            setMessage(i18nc("@info %1 box", "The settings of %1 were written.", name), Positive);
        },
        BoxTool::Kind::Act);
}

void BoxesModule::defaults()
{
    if (!m_current.isEmpty()) {
        m_profile->resetToDefaults();
    }
}

bool BoxesModule::refuseWhileEditing()
{
    if (m_profile->isDirty()) {
        setMessage(i18nc("@info", "Apply or discard the changes to this box first."), Warning);
        return true;
    }
    if (isBusy()) {
        setMessage(i18nc("@info", "Another command for this box is still running."), Warning);
        return true;
    }
    return false;
}

void BoxesModule::runLifecycle(const QList<std::pair<QString, QStringList>> &steps,
                               const QString &working,
                               std::function<void(int exitCode, int step, const QString &lastLine)> done)
{
    dismissMessage();
    m_progress = working;
    Q_EMIT progressTextChanged();
    Q_EMIT busyChanged();
    m_tool->runSequence(steps, [this, done](const BoxTool::Result &r) {
        m_progress.clear();
        Q_EMIT progressTextChanged();
        Q_EMIT busyChanged();
        m_boxes->refresh();
        done(r.exitCode, r.step, r.lastLine);
    });
}

void BoxesModule::startBox()
{
    if (m_current.isEmpty() || refuseWhileEditing()) {
        return;
    }
    const QString name = m_current;
    runLifecycle({{u"kdos-box"_s, {u"start"_s, name}}}, i18nc("@info:progress", "Starting %1…", name), [this, name](int code, int, const QString &last) {
        if (code == 0) {
            setMessage(i18nc("@info %1 box", "%1 is running.", name), Positive);
        } else {
            setMessage(i18nc("@info %1 box, %2 error", "%1 did not start: %2", name, last), Error);
        }
    });
}

void BoxesModule::stopBox()
{
    if (m_current.isEmpty() || refuseWhileEditing()) {
        return;
    }
    const QString name = m_current;
    runLifecycle({{u"kdos-box"_s, {u"stop"_s, name}}}, i18nc("@info:progress", "Stopping %1…", name), [this, name](int code, int, const QString &last) {
        if (code == 0) {
            setMessage(i18nc("@info %1 box", "%1 is stopped.", name), Positive);
        } else {
            setMessage(i18nc("@info %1 box, %2 error", "%1 did not stop: %2", name, last), Error);
        }
    });
}

void BoxesModule::restartBox()
{
    if (m_current.isEmpty() || refuseWhileEditing()) {
        return;
    }
    const QString name = m_current;
    runLifecycle({{u"kdos-box"_s, {u"restart"_s, name}}}, i18nc("@info:progress", "Restarting %1…", name), [this, name](int code, int, const QString &last) {
        if (code == 0) {
            setMessage(i18nc("@info %1 box", "%1 was restarted.", name), Positive);
        } else {
            setMessage(i18nc("@info %1 box, %2 error", "%1 did not restart: %2", name, last), Error);
        }
    });
}

void BoxesModule::recreateBox()
{
    if (m_current.isEmpty() || refuseWhileEditing()) {
        return;
    }
    const QString name = m_current;
    if (!m_currentRow.value(u"created"_s).toBool()) {
        setMessage(i18nc("@info %1 box", "%1 has no container yet; its settings apply when it is first created.", name), Information);
        return;
    }
    /* `remove` refuses a running box and `create` refuses one that still
     * exists, so the order is fixed; each step's own refusal ends it. */
    QList<std::pair<QString, QStringList>> steps;
    if (m_currentRow.value(u"running"_s).toBool()) {
        steps.append({u"kdos-box"_s, {u"stop"_s, name}});
    }
    steps.append({u"kdos-box"_s, {u"remove"_s, name}});
    const int createStep = int(steps.size());
    steps.append({u"kdos-box"_s, {u"create"_s, name}});

    runLifecycle(steps, i18nc("@info:progress", "Creating %1 again…", name), [this, name, createStep](int code, int step, const QString &last) {
        if (code == 0) {
            m_profile->clearPendingRecreate();
            setMessage(i18nc("@info %1 box", "%1 was created again with its current settings.", name), Positive);
        } else if (step == createStep) {
            /* The container is gone and the profile is kept, so the box is
             * still described: its next launch creates it. */
            m_profile->clearPendingRecreate();
            setMessage(i18nc("@info %1 box, %2 error",
                             "%1 was removed, but creating it again failed: %2. Its settings are kept, and it is created the next time its application starts.",
                             name,
                             last),
                       Warning);
        } else {
            setMessage(i18nc("@info %1 box, %2 error", "%1 was not created again: %2", name, last), Error);
        }
    });
}

void BoxesModule::removeBox()
{
    if (m_current.isEmpty() || refuseWhileEditing()) {
        return;
    }
    const QString name = m_current;
    const bool created = m_currentRow.value(u"created"_s).toBool();
    QList<std::pair<QString, QStringList>> steps;
    if (m_currentRow.value(u"running"_s).toBool()) {
        steps.append({u"kdos-box"_s, {u"stop"_s, name}});
    }
    steps.append({u"kdos-box"_s, {u"remove"_s, name}});

    runLifecycle(steps, i18nc("@info:progress", "Removing %1…", name), [this, name, created](int code, int, const QString &last) {
        if (code != 0) {
            setMessage(i18nc("@info %1 box, %2 error", "%1 was not removed: %2", name, last), Error);
            return;
        }
        m_profile->clearPendingRecreate();
        if (created) {
            setMessage(i18nc("@info %1 box", "%1 was removed. Its settings are kept, and it is created again the next time its application starts.", name),
                       Positive);
        } else {
            /* A box with no container is only its profile, and `remove`
             * deleted that: there is nothing left to show. */
            setMessage(i18nc("@info %1 box", "The settings of %1 were deleted.", name), Positive);
            closeBox();
        }
    });
}

void BoxesModule::exportBox(const QUrl &target)
{
    if (m_current.isEmpty() || refuseWhileEditing()) {
        return;
    }
    const QString name = m_current;
    const QString id = m_currentRow.value(u"exportId"_s).toString();
    if (id.isEmpty()) {
        setMessage(i18nc("@info %1 box", "%1 is not an application box, so it has no archive to export.", name), Error);
        return;
    }
    QString path = target.isLocalFile() ? target.toLocalFile() : target.toString();
    if (!path.startsWith(u'/')) {
        setMessage(i18nc("@info", "An archive is written to a folder on this computer."), Error);
        return;
    }
    if (!path.endsWith(u".ktar"_s)) {
        path += u".ktar"_s;
    }
    runLifecycle({{u"kdos-appbox"_s, {u"export"_s, path, id}}}, i18nc("@info:progress", "Exporting %1…", name), [this, name, path](int code, int, const QString &last) {
        if (code == 0) {
            setMessage(i18nc("@info %1 box, %2 file", "%1 was exported to %2.", name, path), Positive);
        } else {
            setMessage(i18nc("@info %1 box, %2 error", "%1 was not exported: %2", name, last), Error);
        }
    });
}

}
