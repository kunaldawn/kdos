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
 * The shipped plugin, loaded the way System Settings loads it, with its page
 * built against a fixture server.
 *
 * Two things only a running page can show: the QML loads (a missing import or
 * a type error is a blank page in System Settings and no build error), and
 * EVERY CONTROL ON IT HAS AN ACCESSIBLE NAME, which is what Orca reads and
 * what the rig's AT-SPI golden diffs. A control is anything a person can
 * press, type into or choose from; its name is read through QAccessible, the
 * same interface the AT-SPI bridge reads.
 */

#include <KPluginMetaData>
#include <KQuickConfigModule>
#include <KQuickConfigModuleLoader>

#include <QAccessible>
#include <QColor>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QProcess>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTest>
#include <QtMath>

#include <memory>

using namespace Qt::StringLiterals;

namespace
{
bool copyTree(const QString &from, const QString &to)
{
    QDir src(from);
    if (!QDir().mkpath(to)) {
        return false;
    }
    const auto entries = src.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden);
    for (const QFileInfo &fi : entries) {
        const QString dst = to + u'/' + fi.fileName();
        if (fi.isDir() ? !copyTree(fi.filePath(), dst) : !QFile::copy(fi.filePath(), dst)) {
            return false;
        }
    }
    return true;
}

bool nameOwned()
{
    return QDBusConnection::sessionBus().interface()->isServiceRegistered(u"io.kdos.Slots1"_s);
}

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
        return true;
    default:
        return false;
    }
}

/* Visible controls with no name, as "role class (parent's name)" lines.
 *
 * A READ-ONLY TEXT EDIT IS NOT A CONTROL: Kirigami.SelectableLabel and a
 * non-editable ComboBox each carry one inside a parent that holds the name
 * (a StaticText, the ComboBox), and nothing can be typed into it. */
void unnamed(QQuickItem *item, QStringList *out, int *controls)
{
    if (!item->isVisible()) {
        return;
    }
    if (QAccessibleInterface *iface = QAccessible::queryAccessibleInterface(item)) {
        const bool readOnlyText = iface->role() == QAccessible::EditableText && iface->state().readOnly;
        if (isControl(iface->role()) && !readOnlyText && !iface->state().invisible) {
            ++*controls;
            if (iface->text(QAccessible::Name).trimmed().isEmpty()) {
                QAccessibleInterface *up = iface->parent();
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

/* The accessible names of every visible control under `item`. */
void controlNames(QQuickItem *item, QStringList *out)
{
    if (!item->isVisible()) {
        return;
    }
    if (QAccessibleInterface *iface = QAccessible::queryAccessibleInterface(item)) {
        if (isControl(iface->role()) && !iface->state().invisible) {
            *out << iface->text(QAccessible::Name);
        }
    }
    const auto children = item->childItems();
    for (QQuickItem *child : children) {
        controlNames(child, out);
    }
}

/* Whether a visible item under `item` has `text` as its text property. */
bool showsText(QQuickItem *item, const QString &text)
{
    if (!item->isVisible()) {
        return false;
    }
    if (item->property("text").toString() == text) {
        return true;
    }
    const auto children = item->childItems();
    for (QQuickItem *child : children) {
        if (showsText(child, text)) {
            return true;
        }
    }
    return false;
}
/* How many visible items under `item` have a text property containing
 * `text`. */
int countShown(QQuickItem *item, const QString &text)
{
    if (!item->isVisible()) {
        return 0;
    }
    int n = item->property("text").toString().contains(text) ? 1 : 0;
    const auto children = item->childItems();
    for (QQuickItem *child : children) {
        n += countShown(child, text);
    }
    return n;
}

/* Whether a visible item under `item` has exactly `name` as its accessible
 * name, of any role. */
bool namesExactly(QQuickItem *item, const QString &name)
{
    if (!item->isVisible()) {
        return false;
    }
    if (QAccessibleInterface *iface = QAccessible::queryAccessibleInterface(item)) {
        if (!iface->state().invisible && iface->text(QAccessible::Name) == name) {
            return true;
        }
    }
    const auto children = item->childItems();
    for (QQuickItem *child : children) {
        if (namesExactly(child, name)) {
            return true;
        }
    }
    return false;
}

/* The item named `name` in the visual tree under `item`, or null. A
 * popup's dim is a child of the window's overlay, not of the popup. */
QQuickItem *itemNamed(QQuickItem *item, const QString &name)
{
    if (item->objectName() == name) {
        return item;
    }
    const auto children = item->childItems();
    for (QQuickItem *child : children) {
        if (QQuickItem *found = itemNamed(child, name)) {
            return found;
        }
    }
    return nullptr;
}

/* Whether a visible item under `item` has an accessible name starting with
 * `prefix`, of any role. */
bool namesStartingWith(QQuickItem *item, const QString &prefix)
{
    if (!item->isVisible()) {
        return false;
    }
    if (QAccessibleInterface *iface = QAccessible::queryAccessibleInterface(item)) {
        if (!iface->state().invisible && iface->text(QAccessible::Name).startsWith(prefix)) {
            return true;
        }
    }
    const auto children = item->childItems();
    for (QQuickItem *child : children) {
        if (namesStartingWith(child, prefix)) {
            return true;
        }
    }
    return false;
}

/* The slot form under `item` that shows `info`. */
QQuickItem *formFor(QQuickItem *item, QObject *info)
{
    if (item->metaObject()->indexOfSignal("startRequested(QVariant)") >= 0 && item->property("slotInfo").value<QObject *>() == info) {
        return item;
    }
    const auto children = item->childItems();
    for (QQuickItem *child : children) {
        if (QQuickItem *found = formFor(child, info)) {
            return found;
        }
    }
    return nullptr;
}

/* Records the size and bounds a window transient for `parent` has as it is
 * shown: QWindow sends QShowEvent before it makes the platform window
 * visible, which is when Qt's Wayland plugin sends the bounds with the
 * window's first state. */
class TransientShowWatch : public QObject
{
public:
    explicit TransientShowWatch(QWindow *parent)
        : m_parent(parent)
    {
    }
    int shows = 0;
    QSize size;
    QSize minimum;
    QSize maximum;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::Show) {
            auto *w = qobject_cast<QWindow *>(watched);
            if (w && w->transientParent() == m_parent) {
                ++shows;
                size = w->size();
                minimum = w->minimumSize();
                maximum = w->maximumSize();
            }
        }
        return false;
    }

private:
    QWindow *m_parent;
};

const QString RollbackHelp = u"Whatever the source, an update is installed only if it carries the KDOS release signature. It goes into the slot not in use and starts after a restart; if it does not start properly, the computer goes back by itself."_s;
const QString OneSlotHelp = u"Whatever the source, an update is installed only if it carries the KDOS release signature. On this computer it is installed from a terminal: “sudo kdos update fetch” downloads it and “sudo kdos update apply” installs it. It is written over the running system and takes effect after a restart, and there is no previous version to go back to if it does not start properly."_s;
}

class KcmUiTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    QProcess m_server;

private Q_SLOTS:
    void initTestCase()
    {
        qputenv("KDOS_SLOTS1_BUS", "session");
        QAccessible::setActive(true);
        QVERIFY2(QDBusConnection::sessionBus().isConnected(), "this suite needs a session bus: run it under dbus-run-session");
        QVERIFY(m_dir.isValid());
        QVERIFY(copyTree(QStringLiteral(FIXTURES_DIR) + u"/ab"_s, m_dir.path()));
        /* An update prepared in slot B, an offer and a stopped job, so every
         * banner and label the page can show before a trial is on it. */
        for (const auto &[name, text] : {std::pair{u"offer"_s, u"0.2.15\n"_s},
                                         std::pair{u"acquired"_s, u"b 0.2.15\n"_s},
                                         std::pair{u"error"_s, u"kpkg: signature check failed\n"_s}}) {
            QFile f(m_dir.path() + u'/' + name);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(text.toUtf8());
        }
        m_server.setProcessChannelMode(QProcess::ForwardedChannels);
        m_server.start(QStringLiteral(FAKESLOTS1_PATH), {m_dir.path()});
        QVERIFY(m_server.waitForStarted());
        QTRY_VERIFY_WITH_TIMEOUT(nameOwned(), 10000);
    }

    void cleanupTestCase()
    {
        m_server.terminate();
        m_server.waitForFinished(5000);
    }

    void pageLoadsAndNamesEveryControl()
    {
        /* A QML error at run time (a binding on an undefined property, a
         * type error in a handler) is a warning, not a failed load; any
         * warning that names a QML file fails the test. */
        QTest::failOnWarning(QRegularExpression(u"\\.qml:\\d+"_s));

        const KPluginMetaData md(QStringLiteral(KCM_PLUGIN_PATH));
        QVERIFY2(md.isValid(), KCM_PLUGIN_PATH);
        QCOMPARE(md.pluginId(), u"kcm_kdos_system"_s);
        QCOMPARE(md.value(u"X-KDE-System-Settings-Parent-Category"_s), u"system-administration"_s);
        // X-DocPath is the book's section under help:/, the path KIO's help
        // worker resolves through /usr/share/doc/HTML/en/kdos and
        // testing/anchorcheck.py holds to a heading of that page.
        QVERIFY(md.value(u"X-DocPath"_s).startsWith(u"kdos/"_s));

        auto engine = std::make_shared<QQmlEngine>();
        const auto result = KQuickConfigModuleLoader::loadModule(md, nullptr, {}, engine);
        QVERIFY2(result.plugin, qPrintable(result.errorText));
        std::unique_ptr<KQuickConfigModule> kcm(result.plugin);

        kcm->load();
        QTRY_VERIFY(kcm->property("configured").toBool());

        QQuickItem *ui = kcm->mainUi();
        QVERIFY2(ui, qPrintable(kcm->errorString()));

        QQuickWindow window;
        window.resize(1100, 1400);
        ui->setParentItem(window.contentItem());
        ui->setSize(window.size());
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));

        /* Both slot forms are built from the server's answer. */
        const auto slotList = kcm->property("slotList").value<QList<QObject *>>();
        QCOMPARE(slotList.size(), 2);
        QTRY_COMPARE(slotList.at(1)->property("version").toString(), u"0.2.13"_s);
        QTRY_VERIFY(kcm->property("updatePrepared").toBool());
        QCOMPARE(kcm->property("offerVersion").toString(), u"0.2.15"_s);
        QTest::qWait(200);

        QStringList missing;
        int controls = 0;
        unnamed(ui, &missing, &controls);
        /* Slot B's start button, the source box, the Discover button and the
         * prepared update's Open Discover. */
        QVERIFY2(controls >= 4, qPrintable(u"only %1 controls found"_s.arg(controls)));
        QVERIFY2(missing.isEmpty(), qPrintable(u"controls without an accessible name: "_s + missing.join(u", "_s)));

        /* With a trial armed the banner's actions are controls too. */
        kcm->metaObject()->invokeMethod(kcm.get(), "startNextTime", Q_ARG(QString, u"b"_s));
        QTRY_VERIFY(kcm->property("trialArmed").toBool());
        QTest::qWait(200);
        missing.clear();
        controls = 0;
        unnamed(window.contentItem(), &missing, &controls);
        /* The banner's Restart Now and Keep replace the start button, and
         * the armed trial replaces the prepared update's banner. */
        QVERIFY2(controls >= 4, qPrintable(u"only %1 controls found"_s.arg(controls)));
        QVERIFY2(missing.isEmpty(), qPrintable(u"controls without an accessible name: "_s + missing.join(u", "_s)));
    }

    /* A slot an update stopped writing shows its state and no start button.
     * Runs after the test above, on the same fixture directory, from the
     * fixture's boot state with no trial armed. */
    void incompleteSlotHasNoStartButton()
    {
        QTest::failOnWarning(QRegularExpression(u"\\.qml:\\d+"_s));
        QFile::remove(m_dir.path() + u"/bootstate"_s);
        QVERIFY(QFile::copy(QStringLiteral(FIXTURES_DIR) + u"/ab/bootstate"_s, m_dir.path() + u"/bootstate"_s));
        QFile unfilled(m_dir.path() + u"/unfilled"_s);
        QVERIFY(unfilled.open(QIODevice::WriteOnly));
        unfilled.write("b\n");
        unfilled.close();

        const KPluginMetaData md(QStringLiteral(KCM_PLUGIN_PATH));
        auto engine = std::make_shared<QQmlEngine>();
        const auto result = KQuickConfigModuleLoader::loadModule(md, nullptr, {}, engine);
        QVERIFY2(result.plugin, qPrintable(result.errorText));
        std::unique_ptr<KQuickConfigModule> kcm(result.plugin);
        kcm->load();
        QTRY_VERIFY(kcm->property("configured").toBool());
        const auto slotList = kcm->property("slotList").value<QList<QObject *>>();
        QTRY_COMPARE(slotList.at(1)->property("state").toString(), u"incomplete"_s);

        QQuickItem *ui = kcm->mainUi();
        QVERIFY2(ui, qPrintable(kcm->errorString()));
        QQuickWindow window;
        window.resize(1100, 1400);
        ui->setParentItem(window.contentItem());
        ui->setSize(window.size());
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QTest::qWait(200);

        QTRY_VERIFY(showsText(ui, u"Incomplete"_s));
        /* The state says what the slot is, and the one sentence below it
         * says why: the reason is not repeated in the state. */
        QVERIFY(namesExactly(ui, u"Slot B state: Incomplete"_s));
        QCOMPARE(countShown(ui, u"Incomplete"_s), 1);
        QCOMPARE(countShown(ui, u"An update stopped while writing"_s), 1);
        QVERIFY(showsText(ui, u"An update stopped while writing this slot. The next update writes it again."_s));
        /* No Version row: the record's version is the system the update
         * began overwriting. Slot A's row stays. */
        QVERIFY(!namesStartingWith(ui, u"Slot B version:"_s));
        QVERIFY(namesStartingWith(ui, slotList.at(0)->property("title").toString() + u" version:"_s));
        QVERIFY(showsText(ui, RollbackHelp));
        QVERIFY(!showsText(ui, OneSlotHelp));
        QStringList names;
        controlNames(window.contentItem(), &names);
        for (const QString &n : std::as_const(names)) {
            QVERIFY2(!n.startsWith(u"Start "_s), qPrintable(names.join(u", "_s)));
        }
        QFile::remove(m_dir.path() + u"/unfilled"_s);
    }

    /* The Start prompt holds its width with the subtitle wrapped, however
     * long the sentence: Kirigami's Dialog takes preferredWidth as a floor
     * under the unwrapped subtitle, so only maximumWidth bounds it. Runs
     * after the test above, from the fixture's boot state. */
    void startPromptIsNarrow()
    {
        QTest::failOnWarning(QRegularExpression(u"\\.qml:\\d+"_s));
        QFile::remove(m_dir.path() + u"/unfilled"_s);
        QFile::remove(m_dir.path() + u"/bootstate"_s);
        QVERIFY(QFile::copy(QStringLiteral(FIXTURES_DIR) + u"/ab/bootstate"_s, m_dir.path() + u"/bootstate"_s));

        const KPluginMetaData md(QStringLiteral(KCM_PLUGIN_PATH));
        auto engine = std::make_shared<QQmlEngine>();
        const auto result = KQuickConfigModuleLoader::loadModule(md, nullptr, {}, engine);
        QVERIFY2(result.plugin, qPrintable(result.errorText));
        std::unique_ptr<KQuickConfigModule> kcm(result.plugin);
        kcm->load();
        QTRY_VERIFY(kcm->property("configured").toBool());
        const auto slotList = kcm->property("slotList").value<QList<QObject *>>();
        QTRY_VERIFY(slotList.at(1)->property("bootable").toBool());

        QQmlComponent units(engine.get());
        units.setData("import QtQml\nimport org.kde.kirigami as Kirigami\nQtObject { readonly property real g: Kirigami.Units.gridUnit }\n", QUrl());
        std::unique_ptr<QObject> u(units.create());
        QVERIFY2(u, qPrintable(units.errorString()));
        const qreal bound = u->property("g").toReal() * 24;
        QVERIFY(bound > 0);

        QQuickItem *ui = kcm->mainUi();
        QVERIFY2(ui, qPrintable(kcm->errorString()));
        QQuickWindow window;
        window.resize(1900, 1000);
        ui->setParentItem(window.contentItem());
        ui->setSize(window.size());
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QTest::qWait(200);

        auto *dialog = ui->findChild<QObject *>(u"startDialog"_s);
        QVERIFY(dialog);
        dialog->setProperty("slotInfo", QVariant::fromValue(slotList.at(1)));
        TransientShowWatch watch(&window);
        qApp->installEventFilter(&watch);
        QVERIFY(QMetaObject::invokeMethod(dialog, "open"));
        QTRY_VERIFY(dialog->property("opened").toBool());

        /* The prompt's window is held from before it is shown, on every
         * open, which leaves the window system nothing to maximise in the
         * window's first state: a hold that arrives after the map leaves a
         * dead close glyph where the maximise button was. */
        QCOMPARE(watch.shows, 1);
        QVERIFY2(!watch.minimum.isEmpty() && watch.minimum == watch.maximum,
                 qPrintable(u"shown at %1x%2 with bounds %3x%4 to %5x%6"_s.arg(watch.size.width()).arg(watch.size.height())
                                .arg(watch.minimum.width()).arg(watch.minimum.height())
                                .arg(watch.maximum.width()).arg(watch.maximum.height())));
        QMetaObject::invokeMethod(dialog, "close");
        QTRY_VERIFY(!dialog->property("visible").toBool());
        QVERIFY(QMetaObject::invokeMethod(dialog, "open"));
        QTRY_VERIFY(dialog->property("opened").toBool());
        qApp->removeEventFilter(&watch);
        QCOMPARE(watch.shows, 2);
        QVERIFY(!watch.minimum.isEmpty());
        QCOMPARE(watch.minimum, watch.maximum);
        QVERIFY(dialog->property("subtitle").toString().size() > 150);
        const qreal width = dialog->property("width").toReal();
        QVERIFY2(width > 0 && width <= bound, qPrintable(u"prompt %1 wide, bound %2"_s.arg(width).arg(bound)));

        /* The prompt is a modal window of its own over the KCM's window,
         * so input to the rest of System Settings, which is outside the
         * KCM's scene, waits until it is answered. */
        QCOMPARE(dialog->property("popupType").toInt(), 1 /* QQuickPopup::Window */);
        QWindow *prompt = QGuiApplication::modalWindow();
        QVERIFY2(prompt, "the prompt does not block the program's other windows");
        QVERIFY(prompt != &window);
        QCOMPARE(prompt->transientParent(), &window);
        QCOMPARE(prompt->modality(), Qt::ApplicationModal);
        auto *card = dialog->property("contentItem").value<QQuickItem *>();
        QVERIFY(card);
        QCOMPARE(card->window(), prompt);
        /* Held again at the size it settled at, which the window keeps:
         * moving the bounds from the first hold does not fit the window
         * back into them. */
        QTRY_COMPARE(prompt->maximumSize(), prompt->size());
        QCOMPARE(prompt->minimumSize(), prompt->size());
        QVERIFY2(prompt->height() >= qFloor(dialog->property("implicitHeight").toReal()),
                 qPrintable(u"prompt %1 tall for %2"_s.arg(prompt->height()).arg(dialog->property("implicitHeight").toReal())));

        /* As a window of its own the prompt draws no dim over the page:
         * the page's scene is all it could dim, and a dimmed page beside a
         * bright sidebar reads as a sidebar still in use. The prompt has an
         * edge of its own. Whether that edge can be seen on a given scheme
         * is a matter for a screenshot; this holds only that it is drawn
         * and is not the fill's colour. */
        QVERIFY(dialog->property("modal").toBool());
        QQuickItem *dim = itemNamed(window.contentItem(), u"startDialogDim"_s);
        QVERIFY2(dim, "a modal prompt has no dim in the page's scene");
        QTRY_VERIFY(dim->opacity() > 0.99);
        QVERIFY2(!dim->isVisible() || dim->property("color").value<QColor>().alpha() == 0,
                 qPrintable(u"the page is dimmed with %1"_s.arg(dim->property("color").value<QColor>().name(QColor::HexArgb))));
        auto *frame = dialog->property("background").value<QQuickItem *>();
        QVERIFY(frame);
        QCOMPARE(frame->objectName(), u"startDialogFrame"_s);
        const auto *pen = frame->property("border").value<QObject *>();
        QVERIFY(pen);
        QVERIFY(pen->property("width").toReal() >= 1);
        const QColor ground = frame->property("color").value<QColor>();
        const QColor edge = pen->property("color").value<QColor>();
        QVERIFY2(edge.rgb() != ground.rgb(), qPrintable(u"edge %1 on ground %2"_s.arg(edge.name(), ground.name())));
        /* In a window of its own the frame is square: a rounded one would
         * show the window's fill at its corners. */
        QCOMPARE(frame->window(), prompt);
        QCOMPARE(frame->property("radius").toReal(), 0.0);
        QMetaObject::invokeMethod(dialog, "close");
        QTRY_VERIFY(!dialog->property("visible").toBool());

        /* Drawn in the page's own scene, where the platform has no second
         * window, the prompt blocks only the page, and the page is faded
         * behind it. */
        QVERIFY(dialog->setProperty("popupType", 0 /* QQuickPopup::Item */));
        QVERIFY(QMetaObject::invokeMethod(dialog, "open"));
        QTRY_VERIFY(dialog->property("opened").toBool());
        QCOMPARE(frame->window(), &window);
        dim = itemNamed(window.contentItem(), u"startDialogDim"_s);
        QVERIFY2(dim, "a prompt in the page's scene has no dim");
        QTRY_VERIFY(dim->opacity() > 0.99);
        QVERIFY(dim->isVisible());
        QVERIFY2(dim->property("color").value<QColor>().alphaF() >= 0.5,
                 qPrintable(u"the page is dimmed with %1"_s.arg(dim->property("color").value<QColor>().name(QColor::HexArgb))));
        QVERIFY(frame->property("radius").toReal() > 0);
        QMetaObject::invokeMethod(dialog, "close");
        QTRY_VERIFY(!dialog->property("visible").toBool());
    }

    /* A hold sets the prompt's bounds to its size and a release lifts
     * them, and neither resizes the window, whichever way its size moved
     * since the last hold: a window fitted back into an old hold shows a
     * stale size. A window held by holdSize() but drawn in the page's own
     * scene is not held. */
    void holdKeepsTheWindowsSize()
    {
        const KPluginMetaData md(QStringLiteral(KCM_PLUGIN_PATH));
        auto engine = std::make_shared<QQmlEngine>();
        const auto result = KQuickConfigModuleLoader::loadModule(md, nullptr, {}, engine);
        QVERIFY2(result.plugin, qPrintable(result.errorText));
        std::unique_ptr<KQuickConfigModule> kcm(result.plugin);
        const QSize unbounded(16777215, 16777215);
        const auto hold = [&kcm](QWindow *prompt, QWindow *page) {
            return QMetaObject::invokeMethod(kcm.get(), "holdSize", Q_ARG(QWindow *, prompt), Q_ARG(QWindow *, page));
        };
        const auto release = [&kcm] {
            return QMetaObject::invokeMethod(kcm.get(), "releaseSize");
        };

        QWindow page;
        QWindow prompt;
        prompt.resize(432, 117);
        QVERIFY(hold(&prompt, &page));
        QCOMPARE(prompt.minimumSize(), QSize(432, 117));
        QCOMPARE(prompt.maximumSize(), QSize(432, 117));

        const QList<QSize> moves = {QSize(432, 60), QSize(500, 200), QSize(300, 250), QSize(600, 90)};
        for (const QSize &to : moves) {
            prompt.resize(to);
            QVERIFY(hold(&prompt, &page));
            QCOMPARE(prompt.size(), to);
            QCOMPARE(prompt.minimumSize(), to);
            QCOMPARE(prompt.maximumSize(), to);
        }
        for (const QSize &to : {QSize(400, 120), QSize(700, 50)}) {
            prompt.resize(to);
            QVERIFY(release());
            QCOMPARE(prompt.size(), to);
            QCOMPARE(prompt.minimumSize(), QSize(0, 0));
            QCOMPARE(prompt.maximumSize(), unbounded);
            QVERIFY(hold(&prompt, &page));
        }

        QVERIFY(hold(&page, &page));
        QCOMPARE(prompt.minimumSize(), QSize(0, 0));
        QCOMPARE(prompt.maximumSize(), unbounded);
        QCOMPARE(page.minimumSize(), QSize(0, 0));
        QCOMPARE(page.maximumSize(), unbounded);
    }

    /* In System Settings the page is drawn in an offscreen window that is
     * never shown, and the prompt's window would take that for its parent
     * and reach the window system with none. Started from a slot form, the
     * prompt's parent chain ends at the program's own window, which is
     * what the window system resolves its parent through; its caption
     * carries no version, which is in the subtitle. Runs from the
     * fixture's boot state. */
    void promptBelongsToTheProgramWindow()
    {
        QTest::failOnWarning(QRegularExpression(u"\\.qml:\\d+"_s));
        const KPluginMetaData md(QStringLiteral(KCM_PLUGIN_PATH));
        auto engine = std::make_shared<QQmlEngine>();
        const auto result = KQuickConfigModuleLoader::loadModule(md, nullptr, {}, engine);
        QVERIFY2(result.plugin, qPrintable(result.errorText));
        std::unique_ptr<KQuickConfigModule> kcm(result.plugin);
        kcm->load();
        QTRY_VERIFY(kcm->property("configured").toBool());
        const auto slotList = kcm->property("slotList").value<QList<QObject *>>();
        QTRY_VERIFY(slotList.at(1)->property("bootable").toBool());

        QWindow top;
        top.resize(1100, 900);
        top.show();
        QVERIFY(QTest::qWaitForWindowExposed(&top));
        top.requestActivate();
        QTRY_COMPARE(QGuiApplication::focusWindow(), &top);

        QQuickItem *ui = kcm->mainUi();
        QVERIFY2(ui, qPrintable(kcm->errorString()));
        QQuickWindow scene;
        scene.resize(1100, 1400);
        ui->setParentItem(scene.contentItem());
        ui->setSize(scene.size());
        QTest::qWait(200);

        QQuickItem *form = nullptr;
        QTRY_VERIFY((form = formFor(ui, slotList.at(1))));
        auto *dialog = ui->findChild<QObject *>(u"startDialog"_s);
        QVERIFY(dialog);
        QVERIFY(QMetaObject::invokeMethod(form, "startRequested", Q_ARG(QVariant, QVariant::fromValue(slotList.at(1)))));
        QTRY_VERIFY(dialog->property("visible").toBool());

        QCOMPARE(scene.transientParent(), &top);
        auto *card = dialog->property("contentItem").value<QQuickItem *>();
        QVERIFY(card);
        QWindow *prompt = card->window();
        QVERIFY(prompt && prompt != &scene);
        QCOMPARE(prompt->modality(), Qt::ApplicationModal);
        QCOMPARE(prompt->transientParent(), &scene);
        QCOMPARE(dialog->property("title").toString(), u"Start Next Time?"_s);
        QVERIFY(dialog->property("subtitle").toString().contains(slotList.at(1)->property("title").toString()));

        /* Open, the prompt's window is held at the size it settled at,
         * which leaves the window system nothing to maximise; closed, it is
         * released. */
        QTRY_VERIFY(dialog->property("opened").toBool());
        QTRY_COMPARE(prompt->maximumSize(), prompt->size());
        QCOMPARE(prompt->minimumSize(), prompt->size());
        QVERIFY(!prompt->size().isEmpty());

        QMetaObject::invokeMethod(dialog, "close");
        QTRY_VERIFY(!dialog->property("visible").toBool());
        QTRY_COMPARE(prompt->minimumSize(), QSize(0, 0));
        QCOMPARE(prompt->maximumSize(), QWindow().maximumSize());

        /* A shown window is the top level already and is left as it is. */
        QQuickWindow shown;
        shown.resize(200, 200);
        shown.show();
        QVERIFY(QTest::qWaitForWindowExposed(&shown));
        QVERIFY(QMetaObject::invokeMethod(kcm.get(), "holdOver", Q_ARG(QWindow *, &shown)));
        QCOMPARE(shown.transientParent(), nullptr);
    }

    /* Installed with one system slot: the one-version message, the running
     * slot's form beside it, and no form for the slot that does not exist.
     * Runs on the same fixture directory, before the live medium removes
     * the boot state. */
    void oneSlotInstallShowsOneForm()
    {
        QTest::failOnWarning(QRegularExpression(u"\\.qml:\\d+"_s));
        QFile::remove(m_dir.path() + u"/bootstate"_s);
        QFile boot(m_dir.path() + u"/bootstate"_s);
        QVERIFY(boot.open(QIODevice::WriteOnly));
        boot.write("slot_a   = 5f0c8a1e-2b44-4c1d-9a51-0d7e3b9c1a01\n"
                   "slot_b   = \n"
                   "active   = a\n");
        boot.close();

        const KPluginMetaData md(QStringLiteral(KCM_PLUGIN_PATH));
        auto engine = std::make_shared<QQmlEngine>();
        const auto result = KQuickConfigModuleLoader::loadModule(md, nullptr, {}, engine);
        QVERIFY2(result.plugin, qPrintable(result.errorText));
        std::unique_ptr<KQuickConfigModule> kcm(result.plugin);
        kcm->load();
        QTRY_VERIFY(kcm->property("configured").toBool());
        const auto slotList = kcm->property("slotList").value<QList<QObject *>>();
        QTRY_VERIFY(slotList.at(0)->property("present").toBool());
        QVERIFY(!slotList.at(1)->property("present").toBool());

        QQuickItem *ui = kcm->mainUi();
        QVERIFY2(ui, qPrintable(kcm->errorString()));
        QQuickWindow window;
        window.resize(1100, 1400);
        ui->setParentItem(window.contentItem());
        ui->setSize(window.size());
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QTest::qWait(200);

        QTRY_VERIFY(showsText(ui, u"This computer has one version of KDOS"_s));
        QVERIFY(showsText(ui, slotList.at(0)->property("title").toString()));
        QVERIFY(!showsText(ui, u"Nothing installed"_s));
        QVERIFY(!showsText(ui, u"Slot B"_s));
        /* Nothing on the page promises a rollback the machine cannot do. */
        QVERIFY(showsText(ui, OneSlotHelp));
        QVERIFY(!showsText(ui, RollbackHelp));
        /* Discover cannot install a system update with no second slot, so
         * the page offers no button to it. */
        QQuickItem *discover = itemNamed(ui, u"openUpdatesButton"_s);
        QVERIFY(discover);
        QVERIFY(!discover->isVisible());
    }

    /* The live medium: the page names the live session, offers the
     * installer, and shows no update source, since nothing there is
     * updated. Runs after the test above, on the same fixture directory. */
    void liveMediumOffersTheInstaller()
    {
        QTest::failOnWarning(QRegularExpression(u"\\.qml:\\d+"_s));
        QFile::remove(m_dir.path() + u"/bootstate"_s);
        QFile live(m_dir.path() + u"/live"_s);
        QVERIFY(live.open(QIODevice::WriteOnly));
        live.close();

        const KPluginMetaData md(QStringLiteral(KCM_PLUGIN_PATH));
        auto engine = std::make_shared<QQmlEngine>();
        const auto result = KQuickConfigModuleLoader::loadModule(md, nullptr, {}, engine);
        QVERIFY2(result.plugin, qPrintable(result.errorText));
        std::unique_ptr<KQuickConfigModule> kcm(result.plugin);
        kcm->load();
        QTRY_VERIFY(kcm->property("live").toBool());
        QVERIFY(!kcm->property("configured").toBool());

        QQuickItem *ui = kcm->mainUi();
        QVERIFY2(ui, qPrintable(kcm->errorString()));
        QQuickWindow window;
        window.resize(1100, 1400);
        ui->setParentItem(window.contentItem());
        ui->setSize(window.size());
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QTest::qWait(200);

        QTRY_VERIFY(showsText(ui, u"This is the live session"_s));
        QVERIFY(!showsText(ui, u"This computer has one version of KDOS"_s));
        QStringList names;
        controlNames(window.contentItem(), &names);
        QVERIFY2(names.contains(u"Install KDOS…"_s), qPrintable(names.join(u", "_s)));
        QVERIFY2(!names.contains(u"Where updates come from"_s), qPrintable(names.join(u", "_s)));
        QVERIFY2(!names.contains(u"Check for updates in Discover"_s), qPrintable(names.join(u", "_s)));
    }
};

QTEST_MAIN(KcmUiTest)

#include "kcmuitest.moc"
