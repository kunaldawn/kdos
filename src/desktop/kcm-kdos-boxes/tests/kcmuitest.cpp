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

#include <KCModule>
#include <KCModuleLoader>
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

/* The box page of `box`, laid out beside the list in `window`. */
QQuickItem *openPage(KQuickConfigModule *kcm, QQuickWindow *window, const QString &box)
{
    QQuickItem *ui = kcm->mainUi();
    if (!ui) {
        return nullptr;
    }
    if (!QTest::qWaitFor([&] { return kcm->property("currentBox").toString() == box && kcm->depth() == 2; })) {
        return nullptr;
    }
    window->resize(1600, 2000);
    ui->setParentItem(window->contentItem());
    ui->setSize(QSizeF(500, window->height()));
    window->show();
    if (!QTest::qWaitForWindowExposed(window)) {
        return nullptr;
    }
    QQuickItem *page = kcm->subPage(0);
    if (page) {
        page->setParentItem(window->contentItem());
        page->setPosition(QPointF(500, 0));
        page->setSize(QSizeF(1100, window->height()));
    }
    return page;
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

        put(r + u"/fake/podman.state"_s, u"container app.gimp running\ncontainer dev exited\ncontainer devrun running\n"_s);
        put(r + u"/home/.config/kdos/boxes/app.gimp.conf"_s, u"base=pack:app.gimp\naccent=amber\ngrant=window-list\nwayland=no\n"_s);
        put(r + u"/home/.config/kdos/boxes/dev.conf"_s, u"base=image:docker.io/library/debian:trixie\n"_s);
        /* A development box `kdos-box remove` took the container of and
         * kept the profile of, with the line it marks such a profile by. */
        put(r + u"/home/.config/kdos/boxes/ubu.conf"_s, u"# kept by kdos-box remove; remove --purge deletes it\nbase=image:ubuntu:24.04\n"_s);
        /* A store box whose application's uninstall released its image. */
        put(r + u"/home/.config/kdos/boxes/app.zathura.conf"_s, u"# kept by kdos-box remove; remove --purge deletes it\nbase=image:kdos/app.zathura\n"_s);
        /* A running development box nothing was launched in: no record. */
        put(r + u"/home/.config/kdos/boxes/devrun.conf"_s, u"base=image:ubuntu:24.04\n"_s);
        put(r + u"/run/kdos/boxes/app.gimp.state"_s, u"confinement = tagged\nreason      = its bus is not filtered\n"_s);
        /* Pending recreates, so the banner and its action are on the page:
         * two settings on app.gimp, one on dev. */
        put(r + u"/state/kcm_kdos_boxesstaterc"_s, u"[app.gimp]\nPendingKeys=network,ipc\nSince=1\n\n[dev]\nPendingKeys=network\nSince=1\n"_s);
    }

    /* Opened without a box: the column beside the list has a title of its
     * own, and Defaults is not offered, since there is nothing to reset. */
    void opensWithNoBoxChosen()
    {
        QTest::failOnWarning(QRegularExpression(u"\\.qml:\\d+"_s));

        const KPluginMetaData md(QStringLiteral(KCM_PLUGIN_PATH));
        QVERIFY2(md.isValid(), KCM_PLUGIN_PATH);
        auto engine = std::make_shared<QQmlEngine>();
        const auto result = KQuickConfigModuleLoader::loadModule(md, nullptr, {}, engine);
        QVERIFY2(result.plugin, qPrintable(result.errorText));
        std::unique_ptr<KQuickConfigModule> kcm(result.plugin);
        kcm->load();

        QQuickItem *ui = kcm->mainUi();
        QVERIFY2(ui, qPrintable(kcm->errorString()));
        QTRY_COMPARE(kcm->depth(), 2);
        QCOMPARE(kcm->property("currentBox").toString(), QString());
        QVERIFY(kcm->representsDefaults());
        QVERIFY(!kcm->needsSave());

        QQuickItem *placeholder = kcm->subPage(0);
        QVERIFY(placeholder);
        QCOMPARE(placeholder->objectName(), u"boxPlaceholder"_s);
        const QString mainTitle = ui->property("title").toString();
        const QString sideTitle = placeholder->property("title").toString();
        QCOMPARE(sideTitle, u"Box Settings"_s);
        QVERIFY2(sideTitle != mainTitle, qPrintable(mainTitle));
    }

    /* The same, as System Settings opens it: KCModuleLoader wraps the page
     * in a KCModuleQml, whose load() sets the wrapper's own
     * representsDefaults to false before it loads the page, and copies the
     * page's value only when the page reports a change. The wrapper's value
     * is what enables the Defaults button. */
    void wrapperOpensWithNoBoxChosen()
    {
        const KPluginMetaData md(QStringLiteral(KCM_PLUGIN_PATH));
        QVERIFY2(md.isValid(), KCM_PLUGIN_PATH);
        auto engine = std::make_shared<QQmlEngine>();
        std::unique_ptr<KCModule> module(KCModuleLoader::loadModule(md, nullptr, {}, engine));
        QVERIFY(module);
        QVERIFY2(module->inherits("KCModuleQml"), module->metaObject()->className());
        module->load();
        QVERIFY(module->representsDefaults());
        QVERIFY(!module->needsSave());
        QVERIFY(module->buttons() & KAbstractConfigModule::Default);

        /* A second load, as Reset makes, leaves it disabled too. */
        module->load();
        QVERIFY(module->representsDefaults());
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

        /* The banner lists the pending settings with "and" before the last. */
        QVERIFY(shows(window.contentItem(),
                      u"Changed since the box was created: network and shared memory. These settings apply when the box is created, so create it again to apply them now."_s));

        /* A box with a container offers a terminal inside it. */
        QVERIFY(named(window.contentItem(), u"Open Terminal"_s));
        QVERIFY(!named(window.contentItem(), u"Create"_s));

        /* The honest indicator: the verdict and the launch's own reason,
         * set as a sentence of its own after the verdict's. */
        QVERIFY(shows(window.contentItem(), u"Tagged. KWin limits"_s));
        QVERIFY(shows(window.contentItem(), u"reach past that. Its bus is not filtered."_s));

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

    /* A kept profile with no container says it was removed, and offers
     * Create, never a Start that `kdos-box start` would refuse. */
    void keptBoxOffersCreate()
    {
        QTest::failOnWarning(QRegularExpression(u"\\.qml:\\d+"_s));

        const KPluginMetaData md(QStringLiteral(KCM_PLUGIN_PATH));
        auto engine = std::make_shared<QQmlEngine>();
        const auto result = KQuickConfigModuleLoader::loadModule(md, nullptr, {QVariant(u"ubu"_s)}, engine);
        QVERIFY2(result.plugin, qPrintable(result.errorText));
        std::unique_ptr<KQuickConfigModule> kcm(result.plugin);
        kcm->load();

        QQuickItem *ui = kcm->mainUi();
        QVERIFY2(ui, qPrintable(kcm->errorString()));
        QTRY_COMPARE(kcm->property("currentBox").toString(), u"ubu"_s);

        QQuickWindow window;
        window.resize(1600, 2000);
        ui->setParentItem(window.contentItem());
        ui->setSize(QSizeF(500, window.height()));
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QTRY_COMPARE(kcm->depth(), 2);
        QQuickItem *page = kcm->subPage(0);
        QVERIFY(page);
        page->setParentItem(window.contentItem());
        page->setPosition(QPointF(500, 0));
        page->setSize(QSizeF(1100, window.height()));

        QTRY_VERIFY(kcm->property("current").toMap().value(u"listed"_s).toBool());
        QVERIFY(kcm->property("current").toMap().value(u"kept"_s).toBool());
        QTRY_VERIFY(shows(page, u"Removed — settings kept"_s));
        /* The list row too, under the box's own name. */
        QVERIFY(shows(ui, u"Removed — settings kept"_s));
        QTRY_VERIFY(named(page, u"Create"_s));
        QVERIFY(!named(page, u"Start"_s));
        QVERIFY(!named(page, u"Open Terminal"_s));
        QVERIFY(named(page, u"Delete Settings…"_s));
        QVERIFY(!shows(page, u"Its application is not installed"_s));
    }

    /* A store box whose application is uninstalled offers no Create, which
     * could only fail asking a registry for an image built here; it says
     * how the box comes back instead. */
    void uninstalledAppBoxOffersNoCreate()
    {
        QTest::failOnWarning(QRegularExpression(u"\\.qml:\\d+"_s));

        const KPluginMetaData md(QStringLiteral(KCM_PLUGIN_PATH));
        auto engine = std::make_shared<QQmlEngine>();
        const auto result = KQuickConfigModuleLoader::loadModule(md, nullptr, {QVariant(u"app.zathura"_s)}, engine);
        QVERIFY2(result.plugin, qPrintable(result.errorText));
        std::unique_ptr<KQuickConfigModule> kcm(result.plugin);
        kcm->load();

        QQuickWindow window;
        QQuickItem *page = openPage(kcm.get(), &window, u"app.zathura"_s);
        QVERIFY(page);
        QTRY_VERIFY(kcm->property("current").toMap().value(u"listed"_s).toBool());
        QVERIFY(kcm->property("current").toMap().value(u"appMissing"_s).toBool());
        QTRY_VERIFY(shows(page, u"Removed — settings kept"_s));
        QTRY_VERIFY(shows(page, u"Its application is not installed. Installing it again from Discover makes this box with these settings"_s));
        QVERIFY(named(page, u"Delete Settings…"_s));
        QVERIFY(!named(page, u"Create"_s));
        QVERIFY(!named(page, u"Start"_s));
        QVERIFY(!named(page, u"Export as Archive…"_s));
    }

    /* One pending setting is named in the singular. */
    void recreateBannerForOneSetting()
    {
        QTest::failOnWarning(QRegularExpression(u"\\.qml:\\d+"_s));

        const KPluginMetaData md(QStringLiteral(KCM_PLUGIN_PATH));
        auto engine = std::make_shared<QQmlEngine>();
        const auto result = KQuickConfigModuleLoader::loadModule(md, nullptr, {QVariant(u"dev"_s)}, engine);
        QVERIFY2(result.plugin, qPrintable(result.errorText));
        std::unique_ptr<KQuickConfigModule> kcm(result.plugin);
        kcm->load();

        QQuickWindow window;
        QQuickItem *page = openPage(kcm.get(), &window, u"dev"_s);
        QVERIFY(page);
        QTRY_VERIFY(kcm->property("needsRecreate").toBool());
        QCOMPARE(kcm->property("recreateKeys").toStringList(), QStringList{u"network"_s});
        QTRY_VERIFY(shows(page,
                          u"Changed since the box was created: network. This setting applies when the box is created, so create it again to apply it now."_s));
        QVERIFY(!shows(page, u"These settings apply"_s));
    }

    /* A running box no launch has recorded says what would record it,
     * not that it cannot say. */
    void runningBoxWithNoRecordSaysWhy()
    {
        QTest::failOnWarning(QRegularExpression(u"\\.qml:\\d+"_s));

        const KPluginMetaData md(QStringLiteral(KCM_PLUGIN_PATH));
        auto engine = std::make_shared<QQmlEngine>();
        const auto result = KQuickConfigModuleLoader::loadModule(md, nullptr, {QVariant(u"devrun"_s)}, engine);
        QVERIFY2(result.plugin, qPrintable(result.errorText));
        std::unique_ptr<KQuickConfigModule> kcm(result.plugin);
        kcm->load();

        QQuickWindow window;
        QQuickItem *page = openPage(kcm.get(), &window, u"devrun"_s);
        QVERIFY(page);
        QTRY_VERIFY(kcm->property("current").toMap().value(u"running"_s).toBool());
        QCOMPARE(kcm->property("current").toMap().value(u"confinement"_s).toString(), QString());
        QTRY_VERIFY(shows(page, u"Nothing has been launched in this box through KDOS yet (an exported launcher or kdos-box run), so how its windows are confined is not recorded."_s));
        QVERIFY(!shows(page, u"cannot say"_s));
    }

    /* A limit with a unit can be typed: the text stays as typed until
     * Return, which takes it and shows its value's spelling. */
    void limitTypedWithAUnitIsTaken()
    {
        QTest::failOnWarning(QRegularExpression(u"\\.qml:\\d+"_s));
        const KPluginMetaData md(QStringLiteral(KCM_PLUGIN_PATH));
        auto engine = std::make_shared<QQmlEngine>();
        const auto result = KQuickConfigModuleLoader::loadModule(md, nullptr, {QVariant(u"dev"_s)}, engine);
        QVERIFY2(result.plugin, qPrintable(result.errorText));
        std::unique_ptr<KQuickConfigModule> kcm(result.plugin);
        kcm->load();

        QQuickWindow window;
        QQuickItem *page = openPage(kcm.get(), &window, u"dev"_s);
        QVERIFY(page);
        QObject *profile = kcm->property("profile").value<QObject *>();
        QVERIFY(profile);
        QTRY_VERIFY(profile->property("notesLoaded").toBool());
        QObject *values = profile->property("values").value<QObject *>();

        QAccessibleInterface *field = named(window.contentItem(), u"Memory limit"_s);
        QVERIFY(field);
        auto *item = qobject_cast<QQuickItem *>(field->object());
        QVERIFY(item);
        QQuickItem *input = item->property("contentItem").value<QQuickItem *>();
        if (!input) {
            input = item;
        }
        const QString unset = input->property("text").toString();
        const auto type512 = [&] {
            input->forceActiveFocus();
            QTRY_VERIFY(input->hasActiveFocus());
            QMetaObject::invokeMethod(input, "selectAll");
            for (const char c : {'5', '1', '2', 'm'}) {
                QTest::keyClick(&window, c);
            }
            QCOMPARE(input->property("text").toString(), u"512m"_s);
            QTest::keyClick(&window, Qt::Key_Return);
            QTRY_COMPARE(values->property("memory").toInt(), 512);
            QTRY_VERIFY(input->property("text").toString().contains(u"512"_s));
            QTRY_VERIFY(kcm->needsSave());
        };
        type512();

        /* A typed field follows Reset and Defaults like a stepped one. */
        kcm->load();
        QTRY_COMPARE(values->property("memory").toInt(), 0);
        QTRY_COMPARE(item->property("value").toInt(), 0);
        QTRY_COMPARE(input->property("text").toString(), unset);
        type512();
        kcm->defaults();
        QTRY_COMPARE(item->property("value").toInt(), 0);
        QTRY_COMPARE(input->property("text").toString(), unset);
    }

    /* Each limit, typed and applied, is written, and the page lives on
     * through the reload of the profile kdos-box wrote, which redraws the
     * notes of the key whose limits note changed. */
    void limitAppliedKeepsThePage_data()
    {
        QTest::addColumn<QString>("field");
        QTest::addColumn<QString>("typed");
        QTest::addColumn<QString>("written");
        QTest::newRow("memory") << u"Memory limit"_s << u"1g"_s << u"memory=1G"_s;
        QTest::newRow("cpus") << u"Processor limit"_s << u"2"_s << u"cpus=2"_s;
        QTest::newRow("pids") << u"Process limit"_s << u"1024"_s << u"pids=1024"_s;
        QTest::newRow("autostop") << u"Stop the box when it has had no window for"_s << u"90m"_s << u"autostop=5400s"_s;
    }

    void limitAppliedKeepsThePage()
    {
        QFETCH(QString, field);
        QFETCH(QString, typed);
        QFETCH(QString, written);
        QTest::failOnWarning(QRegularExpression(u"\\.qml:\\d+"_s));
        const QString conf = m_root.path() + u"/home/.config/kdos/boxes/dev.conf"_s;
        put(conf, u"base=image:docker.io/library/debian:trixie\nmemory=512M\n"_s);

        const KPluginMetaData md(QStringLiteral(KCM_PLUGIN_PATH));
        auto engine = std::make_shared<QQmlEngine>();
        const auto result = KQuickConfigModuleLoader::loadModule(md, nullptr, {QVariant(u"dev"_s)}, engine);
        QVERIFY2(result.plugin, qPrintable(result.errorText));
        std::unique_ptr<KQuickConfigModule> kcm(result.plugin);
        kcm->load();

        QQuickWindow window;
        QQuickItem *page = openPage(kcm.get(), &window, u"dev"_s);
        QVERIFY(page);
        QObject *profile = kcm->property("profile").value<QObject *>();
        QVERIFY(profile);
        QTRY_VERIFY(profile->property("notesLoaded").toBool());

        QAccessibleInterface *box = named(window.contentItem(), field);
        QVERIFY(box);
        auto *item = qobject_cast<QQuickItem *>(box->object());
        QVERIFY(item);
        QQuickItem *input = item->property("contentItem").value<QQuickItem *>();
        if (!input) {
            input = item;
        }
        input->forceActiveFocus();
        QTRY_VERIFY(input->hasActiveFocus());
        QMetaObject::invokeMethod(input, "selectAll");
        for (const QChar c : typed) {
            QTest::keyClick(&window, c.toLatin1());
        }
        QTest::keyClick(&window, Qt::Key_Return);
        QTRY_VERIFY(kcm->needsSave());
        kcm->save();
        QTRY_VERIFY_WITH_TIMEOUT(kcm->property("message").toString().contains(u"were written"_s), 10000);
        QTest::qWait(500);
        QVERIFY(named(window.contentItem(), field));
        QFile f(conf);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QVERIFY2(QString::fromUtf8(f.readAll()).contains(written), qPrintable(written));
        put(conf, u"base=image:docker.io/library/debian:trixie\n"_s);
    }

    /* An accent no swatch names shows Neutral, the colour KWin paints, as
     * the choice without making it an edit: the page opens unmodified, and
     * only a click on Neutral writes `session`. */
    void unknownAccentShowsNeutralUnmodified()
    {
        QTest::failOnWarning(QRegularExpression(u"\\.qml:\\d+"_s));
        const QString conf = m_root.path() + u"/home/.config/kdos/boxes/devrun.conf"_s;
        put(conf, u"base=image:ubuntu:24.04\naccent=nonsense\n"_s);

        const KPluginMetaData md(QStringLiteral(KCM_PLUGIN_PATH));
        auto engine = std::make_shared<QQmlEngine>();
        const auto result = KQuickConfigModuleLoader::loadModule(md, nullptr, {QVariant(u"devrun"_s)}, engine);
        QVERIFY2(result.plugin, qPrintable(result.errorText));
        std::unique_ptr<KQuickConfigModule> kcm(result.plugin);
        kcm->load();

        QQuickWindow window;
        QQuickItem *page = openPage(kcm.get(), &window, u"devrun"_s);
        QVERIFY(page);
        QObject *profile = kcm->property("profile").value<QObject *>();
        QVERIFY(profile);
        QTRY_VERIFY(profile->property("notesLoaded").toBool());
        QObject *values = profile->property("values").value<QObject *>();
        QTRY_COMPARE(values->property("accent").toString(), u"nonsense"_s);

        QAccessibleInterface *neutral = named(window.contentItem(), u"Neutral"_s);
        QVERIFY(neutral);
        QTRY_VERIFY(neutral->state().checked);
        QTest::qWait(300);
        QVERIFY(!kcm->needsSave());
        QCOMPARE(values->property("accent").toString(), u"nonsense"_s);

        auto *item = qobject_cast<QQuickItem *>(neutral->object());
        QVERIFY(item);
        const QPointF centre = item->mapToScene(QPointF(item->width() / 2, item->height() / 2));
        QTest::mouseClick(&window, Qt::LeftButton, {}, centre.toPoint());
        QTRY_COMPARE(values->property("accent").toString(), u"session"_s);
        QTRY_VERIFY(kcm->needsSave());

        put(conf, u"base=image:ubuntu:24.04\n"_s);
    }
};

QTEST_MAIN(KcmUiTest)

#include "kcmuitest.moc"
