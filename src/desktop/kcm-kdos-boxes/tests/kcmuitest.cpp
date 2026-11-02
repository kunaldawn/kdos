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
 * The shipped plugin, opened on one box the way Discover's "Runs in a box"
 * link opens it (`systemsettings kcm_kdos_boxes --args <box>`), over the
 * real kdos-box and the fake engine.
 *
 * The list page and the box page must load, and EVERY CONTROL ON BOTH MUST
 * HAVE AN ACCESSIBLE NAME: what Orca reads and the rig's AT-SPI golden diffs.
 * A control is anything a person can press, type into or choose from.
 */

#include <KPluginMetaData>
#include <KQuickConfigModule>
#include <KQuickConfigModuleLoader>

#include <QAccessible>
#include <QDir>
#include <QFile>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

using namespace Qt::StringLiterals;

namespace
{
bool isControl(QAccessible::Role role)
{
    switch (role) {
    case QAccessible::Button:
    case QAccessible::CheckBox:
    case QAccessible::RadioButton:
    case QAccessible::ComboBox:
    case QAccessible::EditableText:
    case QAccessible::SpinBox:
    case QAccessible::Slider:
    case QAccessible::PageTab:
    case QAccessible::ButtonMenu:
    case QAccessible::ButtonDropDown:
    case QAccessible::Link:
    case QAccessible::ListItem:
        return true;
    default:
        return false;
    }
}

/* A read-only text edit is not a control: Kirigami.SelectableLabel and a
 * non-editable ComboBox each carry one inside a parent that holds the name.
 * A SpinBox's own text field is named by the SpinBox around it. */
void unnamed(QQuickItem *item, QStringList *out, int *controls)
{
    if (!item->isVisible()) {
        return;
    }
    if (QAccessibleInterface *iface = QAccessible::queryAccessibleInterface(item)) {
        QAccessibleInterface *up = iface->parent();
        const bool readOnlyText = iface->role() == QAccessible::EditableText && iface->state().readOnly;
        const bool spinField = iface->role() == QAccessible::EditableText && up && up->role() == QAccessible::SpinBox;
        if (isControl(iface->role()) && !readOnlyText && !spinField && !iface->state().invisible) {
            ++*controls;
            if (iface->text(QAccessible::Name).trimmed().isEmpty()) {
                *out << u"%1 %2 (in \"%3\")"_s.arg(QString::number(int(iface->role())),
                                                   QString::fromLatin1(item->metaObject()->className()),
                                                   up ? up->text(QAccessible::Name) : QString());
            }
        }
    }
    const auto children = item->childItems();
    for (QQuickItem *child : children) {
        unnamed(child, out, controls);
    }
}

/* Whether a visible item shows text containing `needle`. */
bool shows(QQuickItem *item, const QString &needle)
{
    if (!item->isVisible()) {
        return false;
    }
    if (item->property("text").toString().contains(needle)) {
        return true;
    }
    const auto children = item->childItems();
    for (QQuickItem *child : children) {
        if (shows(child, needle)) {
            return true;
        }
    }
    return false;
}

/* The first visible accessible object with this name. */
QAccessibleInterface *named(QQuickItem *item, const QString &name)
{
    if (!item->isVisible()) {
        return nullptr;
    }
    if (QAccessibleInterface *iface = QAccessible::queryAccessibleInterface(item)) {
        if (iface->text(QAccessible::Name) == name) {
            return iface;
        }
    }
    const auto children = item->childItems();
    for (QQuickItem *child : children) {
        if (QAccessibleInterface *found = named(child, name)) {
            return found;
        }
    }
    return nullptr;
}

void put(const QString &path, const QString &text)
{
    QDir().mkpath(QFileInfo(path).path());
    QFile f(path);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(text.toUtf8());
    }
}
}

class KcmUiTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_root;

private Q_SLOTS:
    void initTestCase()
    {
        QAccessible::setActive(true);
        QVERIFY(m_root.isValid());
        const QString r = m_root.path();
        for (const char *d : {"home", "run", "state", "bin", "fake"}) {
            QVERIFY(QDir().mkpath(r + u'/' + QString::fromLatin1(d)));
        }
        QFile::setPermissions(r + u"/run"_s, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
        QVERIFY(QFile::link(QStringLiteral(KDOS_APPBOX_PATH), r + u"/bin/kdos-box"_s));
        for (const char *n : {"podman", "distrobox", "kdos-appbox"}) {
            QVERIFY(QFile::link(QStringLiteral(BOXFAKE_PATH), r + u"/bin/"_s + QString::fromLatin1(n)));
        }
        qputenv("HOME", (r + u"/home"_s).toLocal8Bit());
        qputenv("XDG_RUNTIME_DIR", (r + u"/run"_s).toLocal8Bit());
        qputenv("XDG_STATE_HOME", (r + u"/state"_s).toLocal8Bit());
        qputenv("KDOS_FAKE_DIR", (r + u"/fake"_s).toLocal8Bit());
        qputenv("PATH", QByteArray((r + u"/bin:"_s).toLocal8Bit() + qgetenv("PATH")));

        put(r + u"/fake/podman.state"_s, u"container app.gimp running\ncontainer dev exited\n"_s);
        put(r + u"/home/.config/kdos/boxes/app.gimp.conf"_s, u"base=pack:app.gimp\naccent=amber\ngrant=window-list\nwayland=no\n"_s);
        put(r + u"/home/.config/kdos/boxes/dev.conf"_s, u"base=image:docker.io/library/debian:trixie\n"_s);
        put(r + u"/run/kdos/boxes/app.gimp.state"_s, u"confinement = tagged\nreason      = its bus is not filtered\n"_s);
        /* A pending recreate, so the banner and its action are on the page. */
        put(r + u"/state/kcm_kdos_boxesstaterc"_s, u"[app.gimp]\nPendingKeys=network\nSince=1\n"_s);
    }

    void pagesLoadAndNameEveryControl()
    {
        /* A QML error at run time (a binding on an undefined property, a
         * type error in a handler) is a warning, not a failed load; any
         * warning that names a QML file fails the test. */
        QTest::failOnWarning(QRegularExpression(u"\\.qml:\\d+"_s));

        const KPluginMetaData md(QStringLiteral(KCM_PLUGIN_PATH));
        QVERIFY2(md.isValid(), KCM_PLUGIN_PATH);
        QCOMPARE(md.pluginId(), u"kcm_kdos_boxes"_s);
        QCOMPARE(md.value(u"X-KDE-System-Settings-Parent-Category"_s), u"applications-permissions"_s);
        // X-DocPath is the book's section under help:/, the path KIO's help
        // worker resolves through /usr/share/doc/HTML/en/kdos and
        // testing/anchorcheck.py holds to a heading of that page.
        QVERIFY(md.value(u"X-DocPath"_s).startsWith(u"kdos/"_s));

        auto engine = std::make_shared<QQmlEngine>();
        const auto result = KQuickConfigModuleLoader::loadModule(md, nullptr, {QVariant(u"app.gimp"_s)}, engine);
        QVERIFY2(result.plugin, qPrintable(result.errorText));
        std::unique_ptr<KQuickConfigModule> kcm(result.plugin);
        kcm->load();

        QQuickItem *ui = kcm->mainUi();
        QVERIFY2(ui, qPrintable(kcm->errorString()));
        QTRY_COMPARE(kcm->property("currentBox").toString(), u"app.gimp"_s);

        QQuickWindow window;
        window.resize(1600, 4000);
        ui->setParentItem(window.contentItem());
        ui->setSize(QSizeF(500, window.height()));
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));

        /* The list and the opened box's page, side by side as System
         * Settings shows them. */
        QTRY_COMPARE(kcm->depth(), 2);
        QQuickItem *page = kcm->subPage(0);
        QVERIFY(page);
        page->setParentItem(window.contentItem());
        page->setPosition(QPointF(500, 0));
        page->setSize(QSizeF(1100, window.height()));

        QTRY_VERIFY(kcm->property("current").toMap().value(u"listed"_s).toBool());
        QTRY_VERIFY(kcm->property("needsRecreate").toBool());
        QObject *profile = kcm->property("profile").value<QObject *>();
        QVERIFY(profile);
        QTRY_VERIFY(profile->property("notesLoaded").toBool());
        QTest::qWait(300);

        /* The honest indicator: the verdict and the launch's own reason. */
        QVERIFY(shows(window.contentItem(), u"Tagged. KWin limits"_s));
        QVERIFY(shows(window.contentItem(), u"its bus is not filtered"_s));

        QStringList missing;
        int controls = 0;
        unnamed(window.contentItem(), &missing, &controls);
        /* Two list rows, the search field, lifecycle buttons, nine swatches,
         * radios, checks, boxes, spin boxes and ten grants. */
        QVERIFY2(controls >= 40, qPrintable(u"only %1 controls found"_s.arg(controls)));
        QVERIFY2(missing.isEmpty(), qPrintable(u"controls without an accessible name: "_s + missing.join(u", "_s)));

        /* The controls show the profile: app.gimp shares the network,
         * hides no windows of its own accord but has wayland=no, and is
         * granted the window list. */
        QAccessibleInterface *shared = named(window.contentItem(), u"Shared with this computer"_s);
        QVERIFY(shared);
        QVERIFY(shared->state().checked);
        QAccessibleInterface *windows = named(window.contentItem(), u"Show its windows"_s);
        QVERIFY(windows);
        QVERIFY(!windows->state().checked);
        QAccessibleInterface *windowList = named(window.contentItem(), u"See and manage other windows"_s);
        QVERIFY(windowList);
        QVERIFY(windowList->state().checked);
        QAccessibleInterface *writes = named(window.contentItem(), u"What the box writes"_s);
        QVERIFY(writes);
        QCOMPARE(writes->object()->property("currentText").toString(), u"Is kept"_s);

        /* Choosing a control is an edit the module sees, whether a mouse
         * or a screen reader's Toggle action chose it. */
        QVERIFY(!kcm->needsSave());
        QObject *values = profile->property("values").value<QObject *>();
        QAccessibleInterface *none = named(window.contentItem(), u"No network"_s);
        QVERIFY(none && none->actionInterface());
        none->actionInterface()->doAction(QAccessibleActionInterface::toggleAction());
        QTRY_VERIFY(kcm->needsSave());
        QCOMPARE(values->property("network").toString(), u"none"_s);

        QAccessibleInterface *gpu = named(window.contentItem(), u"Graphics card"_s);
        QVERIFY(gpu && gpu->actionInterface());
        gpu->actionInterface()->doAction(QAccessibleActionInterface::toggleAction());
        QTRY_COMPARE(values->property("gpu").toString(), u"no"_s);

        QAccessibleInterface *neutral = named(window.contentItem(), u"Neutral"_s);
        QVERIFY(neutral && neutral->actionInterface());
        neutral->actionInterface()->doAction(QAccessibleActionInterface::toggleAction());
        QTRY_COMPARE(values->property("accent").toString(), u"session"_s);

        QAccessibleInterface *shell = named(window.contentItem(), u"Draw panels and overlays on the desktop"_s);
        QVERIFY(shell && shell->actionInterface());
        shell->actionInterface()->doAction(QAccessibleActionInterface::toggleAction());
        QTRY_COMPARE(values->property("grant").toStringList(), (QStringList{u"window-list"_s, u"layer-shell"_s}));

        /* And a mouse click on another network choice. */
        QAccessibleInterface *host = named(window.contentItem(), u"Its own network, with internet access"_s);
        QVERIFY(host);
        auto *hostItem = qobject_cast<QQuickItem *>(host->object());
        QVERIFY(hostItem);
        const QPointF centre = hostItem->mapToScene(QPointF(hostItem->width() / 2, hostItem->height() / 2));
        QTest::mouseClick(&window, Qt::LeftButton, {}, centre.toPoint());
        QTRY_COMPARE(values->property("network").toString(), u"private"_s);
    }
};

QTEST_MAIN(KcmUiTest)

#include "kcmuitest.moc"
