// Loads the real QML UI against the simulated camera and fails on any QML warning.
#include "app/AppSetup.h"
#include "app/LiveViewItem.h"
#include "core/CameraController.h"
#include "core/CredentialStore.h"
#include "core/FakeBackend.h"
#include "core/Language.h"

#include "CrDeviceProperty.h"

#include <QQmlApplicationEngine>
#include <QQmlExtensionPlugin>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QTest>

Q_IMPORT_QML_PLUGIN(KamLynkPlugin)

namespace {
// ListView delegates are only in the visual item tree, not QObject children of the window.
QQuickItem *findItem(QQuickItem *root, const QString &name)
{
    if (!root)
        return nullptr;
    if (root->objectName() == name)
        return root;
    for (QQuickItem *child : root->childItems())
        if (QQuickItem *found = findItem(child, name))
            return found;
    return nullptr;
}

QStringList g_warnings;
QtMessageHandler g_previous = nullptr;

void collect(QtMsgType type, const QMessageLogContext &ctx, const QString &msg)
{
    if (type == QtWarningMsg || type == QtCriticalMsg)
        g_warnings << msg;
    if (g_previous)
        g_previous(type, ctx, msg);
}
} // namespace

class TestQml : public QObject
{
    Q_OBJECT

    QTemporaryDir m_dir;
    kl::FakeBackend *m_fake = nullptr;
    kl::CameraController *m_ctl = nullptr;
    kl::Language *m_language = nullptr;
    QQmlApplicationEngine *m_engine = nullptr;
    QQuickWindow *m_window = nullptr;

    // Set KAMLYNK_SHOT_DIR to keep screenshots of dialogs for visual review.
    void shot(const QString &name) const
    {
        const QString dir = qEnvironmentVariable("KAMLYNK_SHOT_DIR");
        if (!dir.isEmpty()) {
            QTest::qWait(300); // let open animations finish
            m_window->grabWindow().save(dir + QLatin1Char('/') + name + QStringLiteral(".png"));
        }
    }

    QQuickItem *currentPage() const
    {
        auto *loader = m_window->findChild<QQuickItem *>(QStringLiteral("pageLoader"));
        return loader ? loader->property("item").value<QQuickItem *>() : nullptr;
    }

private slots:
    void initTestCase()
    {
        g_previous = qInstallMessageHandler(collect);
        QQuickStyle::setStyle(QStringLiteral("Material"));
        m_fake = new kl::FakeBackend;
        m_ctl = new kl::CameraController(m_fake, nullptr, new kl::MemoryCredentialStore, this);
        m_ctl->setDownloadDirectory(m_dir.path());
        // The assertions below use the Portuguese texts, whatever the test machine's language.
        m_language = new kl::Language(nullptr, QLocale(QLocale::Portuguese, QLocale::Brazil), this);
        kl::registerQmlTypes(m_ctl, m_language);
        m_engine = new QQmlApplicationEngine(this);
        kl::retranslateOnLanguageChange(m_language, m_engine, m_ctl);
        m_engine->loadFromModule("KamLynk", "Main");
        QCOMPARE(m_engine->rootObjects().size(), 1);
        m_window = qobject_cast<QQuickWindow *>(m_engine->rootObjects().first());
        QVERIFY(m_window);
        QVERIFY(QTest::qWaitForWindowExposed(m_window));
    }

    void termsAreShownUntilAccepted()
    {
        auto *dialog = m_window->findChild<QObject *>(QStringLiteral("termsDialog"));
        QVERIFY(dialog);
        QTRY_VERIFY(dialog->property("opened").toBool());
        shot(QStringLiteral("terms"));
        auto *accept = findItem(m_window->contentItem(), QStringLiteral("acceptTerms"));
        if (!accept) // the footer lives in the popup's own item tree
            accept = dialog->findChild<QQuickItem *>(QStringLiteral("acceptTerms"));
        QVERIFY(accept);
        QMetaObject::invokeMethod(accept, "clicked");
        QTRY_VERIFY(!dialog->property("visible").toBool());
        QVERIFY(m_ctl->termsAccepted());
    }

    void startsOnConnectPage()
    {
        QTRY_VERIFY(currentPage());
        QCOMPARE(currentPage()->objectName(), QStringLiteral("connectPage"));
    }

    void connectSwitchesToShootPage()
    {
        m_ctl->discover();
        QTRY_COMPARE(m_ctl->cameras().size(), 1);
        m_ctl->connectTo(0);
        QTRY_COMPARE(currentPage()->objectName(), QStringLiteral("shootPage"));
        auto *battery = m_window->findChild<QQuickItem *>(QStringLiteral("batteryLabel"));
        QVERIFY(battery);
        QTRY_COMPARE(battery->property("text").toString(), QStringLiteral("78%"));
    }

    void liveViewShowsFrames()
    {
        auto *view = m_window->findChild<kl::LiveViewItem *>(QStringLiteral("liveView"));
        QVERIFY(view);
        QVERIFY(!view->hasFrame());
        m_fake->emitFrame();
        QTRY_VERIFY(view->hasFrame());
        const QRectF r = view->imageRect();
        QVERIFY(r.width() > 0 && r.height() > 0);
        // Center of the picture maps to the center of the frame.
        const QPointF n = view->normalizedPoint(r.center().x(), r.center().y());
        QVERIFY(qAbs(n.x() - 0.5) < 0.01 && qAbs(n.y() - 0.5) < 0.01);
    }

    void exposureTilesAreSymmetric()
    {
        auto *grid = m_window->findChild<QQuickItem *>(QStringLiteral("exposureGrid"));
        QVERIFY(grid);
        QList<QQuickItem *> tiles;
        for (QQuickItem *child : grid->childItems())
            if (child->isVisible())
                tiles << child;
        QCOMPARE(tiles.size(), 4); // F, shutter, ISO, EV
        for (QQuickItem *t : tiles) {
            QCOMPARE(t->width(), tiles[0]->width());
            QCOMPARE(t->height(), tiles[0]->height());
        }
        QCOMPARE(tiles[0]->x(), tiles[2]->x()); // columns line up
        QCOMPARE(tiles[1]->x(), tiles[3]->x());
        QCOMPARE(tiles[0]->y(), tiles[1]->y()); // rows line up
        QVERIFY(tiles[1]->x() > tiles[0]->x() + tiles[0]->width());
    }

    void sidePanelIsResizableAndScrollBarDoesNotCoverControls()
    {
        auto *panel = m_window->findChild<QQuickItem *>(QStringLiteral("sidePanel"));
        auto *split = m_window->findChild<QQuickItem *>(QStringLiteral("shootSplit"));
        QVERIFY(panel && split);
        QTRY_COMPARE(int(panel->width()), m_ctl->sidePanelWidth());
        m_ctl->setSidePanelWidth(420);
        QTRY_COMPARE(int(panel->width()), 420);
        // The settings column ends before the scroll bar area.
        auto *grid = m_window->findChild<QQuickItem *>(QStringLiteral("exposureGrid"));
        const QPointF gridRight = grid->mapToItem(panel, QPointF(grid->width(), 0));
        QVERIFY2(gridRight.x() <= panel->width() - 14, qPrintable(QString::number(gridRight.x())));
        m_ctl->setSidePanelWidth(340);
    }

    void previewQualityInBottomBar()
    {
        // Bottom bar tools never overlap the capture buttons, even in the minimum window size.
        m_window->resize(m_window->minimumWidth(), m_window->minimumHeight());
        auto *record = m_window->findChild<QQuickItem *>(QStringLiteral("recordButton"));
        auto *quality = m_window->findChild<QQuickItem *>(QStringLiteral("previewQuality"));
        QVERIFY(record && quality);
        QTRY_VERIFY(quality->mapToScene(QPointF(quality->width(), 0)).x() < record->mapToScene(QPointF(0, 0)).x());
        m_window->resize(1360, 820);
        // At normal sizes the capture buttons are centered under the preview.
        auto *view = m_window->findChild<QQuickItem *>(QStringLiteral("liveView"));
        auto *photo = m_window->findChild<QQuickItem *>(QStringLiteral("photoButton"));
        QTRY_VERIFY(qAbs((record->mapToScene(QPointF(0, 0)).x() + photo->mapToScene(QPointF(photo->width(), 0)).x()) / 2
                         - view->mapToScene(QPointF(view->width() / 2, 0)).x()) < 4);

        auto *box = m_window->findChild<QQuickItem *>(QStringLiteral("previewQuality"));
        QVERIFY(box);
        QVERIFY(box->isVisible());
        QCOMPARE(box->property("displayText").toString(), QStringLiteral("Alta"));
        const int low = m_ctl->liveViewQuality()->options().indexOf(QStringLiteral("Baixa"));
        QVERIFY(low >= 0);
        QMetaObject::invokeMethod(box, "activated", Q_ARG(int, low));
        QTRY_COMPARE(box->property("displayText").toString(), QStringLiteral("Baixa"));
        m_fake->emitFrame();
        auto *stats = m_window->findChild<QQuickItem *>(QStringLiteral("previewStats"));
        QVERIFY(stats);
        QTRY_VERIFY(stats->property("text").toString().startsWith(QStringLiteral("Prévia · 640×360")));
        m_ctl->liveViewQuality()->setValue(SCRSDK::CrPropertyLiveViewImageQuality_High);
        QTRY_COMPARE(box->property("displayText").toString(), QStringLiteral("Alta"));
    }

    void aboutDialogOpens()
    {
        auto *button = m_window->findChild<QQuickItem *>(QStringLiteral("aboutButton"));
        auto *dialog = m_window->findChild<QObject *>(QStringLiteral("aboutDialog"));
        QVERIFY(button && dialog);
        QMetaObject::invokeMethod(button, "clicked");
        QTRY_VERIFY(dialog->property("opened").toBool());
        shot(QStringLiteral("about"));
        QMetaObject::invokeMethod(dialog, "close");
        QTRY_VERIFY(!dialog->property("visible").toBool());
    }

    void fullscreenShowsOnlyThePreview()
    {
        auto *page = currentPage();
        auto *button = m_window->findChild<QQuickItem *>(QStringLiteral("fullscreenButton"));
        auto *panel = m_window->findChild<QQuickItem *>(QStringLiteral("sidePanel"));
        auto *exit = m_window->findChild<QQuickItem *>(QStringLiteral("exitFullscreen"));
        auto *view = m_window->findChild<kl::LiveViewItem *>(QStringLiteral("liveView"));
        QVERIFY(button && panel && exit && view);
        QVERIFY(!exit->isVisible());

        QMetaObject::invokeMethod(button, "clicked");
        QTRY_VERIFY(page->property("fullscreen").toBool());
        QVERIFY(!panel->isVisible());
        QVERIFY(!button->isVisible()); // bottom bar hidden
        QVERIFY(exit->isVisible());
        QTRY_COMPARE(view->width(), qreal(m_window->width())); // the preview fills the window
        shot(QStringLiteral("fullscreen"));

        QTest::keyClick(m_window, Qt::Key_Escape);
        QTRY_VERIFY(!page->property("fullscreen").toBool());
        QVERIFY(panel->isVisible());

        QTest::keyClick(m_window, Qt::Key_F11, Qt::ControlModifier);
        QTRY_VERIFY(page->property("fullscreen").toBool());
        QTest::keyClick(m_window, Qt::Key_F11, Qt::ControlModifier);
        QTRY_VERIFY(!page->property("fullscreen").toBool());
    }

    void memoryCardIndicator()
    {
        auto *label = m_window->findChild<QQuickItem *>(QStringLiteral("mediaLabel"));
        auto *indicator = m_window->findChild<QQuickItem *>(QStringLiteral("mediaIndicator"));
        auto *details = m_window->findChild<QObject *>(QStringLiteral("mediaDetails"));
        QVERIFY(label && indicator && details);
        QCOMPARE(label->property("text").toString(), QStringLiteral("≈67,5 GB livres · 1:30:00"));
        QMetaObject::invokeMethod(indicator, "clicked");
        QTRY_VERIFY(details->property("opened").toBool());
        m_ctl->setCardCapacityGb(128);
        shot(QStringLiteral("card"));
        QMetaObject::invokeMethod(details, "close");
        QTRY_VERIFY(!details->property("visible").toBool());
        m_ctl->setCardCapacityGb(0);
    }

    void languageSwitchesLive()
    {
        auto *button = m_window->findChild<QQuickItem *>(QStringLiteral("settingsButton"));
        auto *dialog = m_window->findChild<QObject *>(QStringLiteral("settingsDialog"));
        auto *media = m_window->findChild<QQuickItem *>(QStringLiteral("mediaLabel"));
        QVERIFY(button && dialog && media);
        QMetaObject::invokeMethod(button, "clicked");
        QTRY_VERIFY(dialog->property("opened").toBool());

        auto *english = findItem(m_window->contentItem(), QStringLiteral("language_en"));
        QVERIFY(english);
        QMetaObject::invokeMethod(english, "clicked");
        QCOMPARE(m_language->current(), QStringLiteral("en"));
        // QML texts, C++ texts and number formatting all follow, without restarting.
        QTRY_COMPARE(media->property("text").toString(), QStringLiteral("≈67.5 GB free · 1:30:00"));
        QCOMPARE(dialog->property("title").toString(), QStringLiteral("Settings"));
        QCOMPARE(m_ctl->whiteBalance()->name(), QStringLiteral("White balance"));
        QVERIFY(m_ctl->statusMessage().startsWith(QStringLiteral("Connected: ")));
        shot(QStringLiteral("settings-en"));

        auto *portuguese = findItem(m_window->contentItem(), QStringLiteral("language_pt_BR"));
        QVERIFY(portuguese);
        QMetaObject::invokeMethod(portuguese, "clicked");
        QTRY_COMPARE(media->property("text").toString(), QStringLiteral("≈67,5 GB livres · 1:30:00"));
        QCOMPARE(dialog->property("title").toString(), QStringLiteral("Configurações"));
        QMetaObject::invokeMethod(dialog, "close");
        QTRY_VERIFY(!dialog->property("visible").toBool());
        m_language->setChoice({});
    }

    void lowBatteryToastIsSilent()
    {
        auto *toast = m_window->findChild<QQuickItem *>(QStringLiteral("lowBatteryToast"));
        QVERIFY(toast);
        QVERIFY(!toast->isVisible());
        m_fake->setPropertyValue(SCRSDK::CrDeviceProperty_BatteryRemain, 5);
        QTRY_VERIFY(toast->isVisible());
        QCOMPARE(currentPage()->objectName(), QStringLiteral("shootPage")); // nothing else changes
        shot(QStringLiteral("battery"));
        m_ctl->dismissLowBattery();
        QVERIFY(!toast->isVisible());
        m_fake->setPropertyValue(SCRSDK::CrDeviceProperty_BatteryRemain, 78);
        QTRY_COMPARE(m_ctl->batteryPercent(), 78);
    }

    void mirrorToggleFlipsTouchCoordinates()
    {
        auto *view = m_window->findChild<kl::LiveViewItem *>(QStringLiteral("liveView"));
        auto *toggle = m_window->findChild<QQuickItem *>(QStringLiteral("mirrorToggle"));
        QVERIFY(view && toggle);
        const QRectF r = view->imageRect();
        const QPointF left(r.x() + r.width() * 0.25, r.center().y());
        QVERIFY(qAbs(view->normalizedPoint(left.x(), left.y()).x() - 0.25) < 0.01);

        QMetaObject::invokeMethod(toggle, "toggle");
        QMetaObject::invokeMethod(toggle, "toggled");
        QTRY_VERIFY(m_ctl->mirrorPreview());
        QVERIFY(view->mirrored());
        // A click on the left of the mirrored picture is the right side for the camera.
        QVERIFY(qAbs(view->normalizedPoint(left.x(), left.y()).x() - 0.75) < 0.01);

        m_ctl->setMirrorPreview(false);
        QVERIFY(!view->mirrored());
        QVERIFY(!toggle->property("checked").toBool());
    }

    void recordingFailureShowsAlert()
    {
        auto *alert = m_window->findChild<QQuickItem *>(QStringLiteral("recordingAlert"));
        QVERIFY(alert);
        QVERIFY(!alert->isVisible());
        m_fake->setRecordResponse(kl::FakeBackend::RecordResponse::Fail);
        m_ctl->startRecording();
        QTRY_VERIFY(alert->isVisible());
        QVERIFY(!m_ctl->recording());
        m_ctl->dismissRecordingError();
        QVERIFY(!alert->isVisible());
        m_fake->setRecordResponse(kl::FakeBackend::RecordResponse::Normal);
        // Clear the Recording_Failed state as the camera would.
        m_fake->setPropertyValue(SCRSDK::CrDeviceProperty_RecordingState, SCRSDK::CrMovie_Recording_State_Not_Recording);
        QTest::qWait(50);
        QVERIFY(m_ctl->recordingError().isEmpty());
    }

    void recordButtonStartsAndStops()
    {
        auto *button = m_window->findChild<QQuickItem *>(QStringLiteral("recordButton"));
        QVERIFY(button);
        QMetaObject::invokeMethod(button, "clicked");
        QTRY_VERIFY(m_ctl->recording());
        QMetaObject::invokeMethod(button, "clicked");
        QTRY_VERIFY(!m_ctl->recording());
    }

    void disconnectReturnsToConnectPage()
    {
        m_ctl->disconnectCamera();
        QTRY_COMPARE_WITH_TIMEOUT(currentPage()->objectName(), QStringLiteral("connectPage"), 200);
    }

    void lostConnectionReturnsToConnectPage()
    {
        m_ctl->discover();
        QTRY_COMPARE(m_ctl->cameras().size(), 1);
        m_ctl->connectTo(0);
        QTRY_COMPARE(currentPage()->objectName(), QStringLiteral("shootPage"));
        auto *overlay = m_window->findChild<QQuickItem *>(QStringLiteral("reconnectingOverlay"));
        QVERIFY(overlay);
        m_ctl->setConnectionLossTimeout(300);
        m_fake->simulateReconnecting();
        QTRY_VERIFY(overlay->isVisible());
        QTRY_COMPARE(currentPage()->objectName(), QStringLiteral("connectPage"));
        m_ctl->setConnectionLossTimeout(10000);
    }

    void sshCameraOpensCredentialsDialog()
    {
        // Cameras on a router with "Access Authen." on (the default) ask for user/password.
        m_fake->setRequiresCredentials(true);
        m_ctl->discover();
        QTRY_COMPARE(m_ctl->cameras().size(), 1);
        m_ctl->connectTo(0);
        auto *dialog = m_window->findChild<QObject *>(QStringLiteral("credentialsDialog"));
        QVERIFY(dialog);
        QTRY_VERIFY(dialog->property("opened").toBool());
        auto *password = m_window->findChild<QQuickItem *>(QStringLiteral("passwordField"));
        auto *reveal = m_window->findChild<QQuickItem *>(QStringLiteral("revealPassword"));
        auto *remember = m_window->findChild<QQuickItem *>(QStringLiteral("rememberLogin"));
        auto *error = m_window->findChild<QQuickItem *>(QStringLiteral("credentialsError"));
        QVERIFY(password && reveal && remember && error);
        QVERIFY(remember->isVisible());
        QVERIFY(remember->property("checked").toBool()); // remembering is the default
        QVERIFY(!error->isVisible());

        // Wrong password: the dialog comes back with the error.
        m_fake->setRequiredPassword(QStringLiteral("secret"));
        password->setProperty("text", QStringLiteral("typo"));
        QMetaObject::invokeMethod(dialog, "accept");
        QTRY_VERIFY(error->isVisible());
        QTRY_VERIFY(dialog->property("opened").toBool());
        shot(QStringLiteral("credentials-error"));

        password->setProperty("text", QStringLiteral("secret"));
        // The eye button shows and hides the typed password.
        const int hidden = password->property("echoMode").toInt();
        QMetaObject::invokeMethod(reveal, "toggle");
        QCOMPARE(password->property("echoMode").toInt(), 0); // TextInput.Normal
        shot(QStringLiteral("credentials"));
        QMetaObject::invokeMethod(reveal, "toggle");
        QCOMPARE(password->property("echoMode").toInt(), hidden);
        QMetaObject::invokeMethod(dialog, "accept");
        QTRY_COMPARE(currentPage()->objectName(), QStringLiteral("shootPage"));
        QCOMPARE(m_fake->lastCredentials().password, QStringLiteral("secret"));
        QCOMPARE(m_fake->lastCredentials().user, QStringLiteral("admin"));
        m_fake->setRequiresCredentials(false);
        m_ctl->disconnectCamera();
        QTRY_COMPARE(currentPage()->objectName(), QStringLiteral("connectPage"));
    }

    void cameraListShowsHistoryAndStatus()
    {
        // The camera used above is now in "Minhas câmeras", online, with its login saved.
        auto *list = m_window->findChild<QQuickItem *>(QStringLiteral("cameraList"));
        QVERIFY(list);
        QTRY_COMPARE(list->property("count").toInt(), 1);
        const QVariantMap device = m_ctl->devices().first().toMap();
        QVERIFY(device.value("known").toBool());
        QVERIFY(device.value("savedLogin").toBool());
        auto statusOf = [this] {
            auto *row = findItem(m_window->contentItem(), QStringLiteral("device_0"));
            auto *status = findItem(row, QStringLiteral("deviceStatus"));
            return status ? status->property("text").toString() : QString();
        };
        QTRY_COMPARE(statusOf(), QStringLiteral("Online"));
        shot(QStringLiteral("devices"));

        m_fake->setDiscoverable(false);
        m_ctl->discover();
        QTRY_COMPARE(statusOf(), QStringLiteral("Offline"));
        m_fake->setDiscoverable(true);
        m_ctl->discover();
        QTRY_COMPARE(statusOf(), QStringLiteral("Online"));
    }

    void noQmlWarnings()
    {
        if (!g_warnings.isEmpty())
            qInstallMessageHandler(g_previous);
        QVERIFY2(g_warnings.isEmpty(), qPrintable(g_warnings.join(QLatin1Char('\n'))));
    }
};

QTEST_MAIN(TestQml)
#include "tst_qml.moc"
