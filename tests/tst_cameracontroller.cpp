#include "core/CameraController.h"
#include "core/CredentialStore.h"
#include "core/FakeBackend.h"

#include "CrDefines.h"
#include "CrDeviceProperty.h"

#include <QFileInfo>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace kl;
namespace SDK = SCRSDK;
using Command = CameraBackend::Command;

namespace {
// Controller with an in-memory keychain and a camera that wants SSH user/password.
struct SshRig
{
    FakeBackend *fake = new FakeBackend;
    MemoryCredentialStore *store = new MemoryCredentialStore;
    CameraController *ctl;
    explicit SshRig(const QString &settingsPath)
        : ctl(new CameraController(fake, new QSettings(settingsPath, QSettings::IniFormat), store))
    {
        fake->setRequiresCredentials(true);
        fake->setRequiredPassword(QStringLiteral("secret"));
    }
    ~SshRig() { delete ctl; }
};
static constexpr auto kFp = "SHA256:demo-fingerprint";
} // namespace

class TestCameraController : public QObject
{
    Q_OBJECT

    QTemporaryDir m_dir;
    FakeBackend *m_fake = nullptr;
    CameraController *m_ctl = nullptr;

    void connectDemo()
    {
        m_ctl->discover();
        QTRY_COMPARE(m_ctl->cameras().size(), 1);
        m_ctl->connectTo(0);
        QTRY_COMPARE(m_ctl->state(), CameraController::Connected);
        QTRY_VERIFY(m_ctl->iso()->available());
    }

private slots:
    void init()
    {
        QVERIFY(m_dir.isValid());
        QFile::remove(m_dir.filePath("prefs.ini"));
        m_fake = new FakeBackend;
        m_ctl = new CameraController(m_fake);
        m_ctl->setDownloadDirectory(m_dir.path());
    }

    void cleanup()
    {
        delete m_ctl; // owns the backend
        m_ctl = nullptr;
        m_fake = nullptr;
    }

    void discoverListsCameras()
    {
        QSignalSpy state(m_ctl, &CameraController::stateChanged);
        m_ctl->discover();
        QCOMPARE(m_ctl->state(), CameraController::Discovering);
        QTRY_COMPARE(m_ctl->cameras().size(), 1);
        QCOMPARE(m_ctl->state(), CameraController::Idle);
        QCOMPARE(m_ctl->cameras().first().toMap().value("model").toString(), QStringLiteral("ZV-E10M2"));
    }

    void connectLoadsPropertiesAndStartsLiveView()
    {
        connectDemo();
        QCOMPARE(m_ctl->modelName(), QStringLiteral("ZV-E10M2"));
        QVERIFY(m_fake->liveViewEnabled());
        QCOMPARE(m_fake->downloadDirectory(), m_dir.path());
        QCOMPARE(m_ctl->iso()->text(), QStringLiteral("ISO 800"));
        QCOMPARE(m_ctl->fNumber()->text(), QStringLiteral("F4"));
        QCOMPARE(m_ctl->shutterSpeed()->text(), QStringLiteral("1/50"));
        QCOMPARE(m_ctl->batteryPercent(), 78);
        QCOMPARE(m_ctl->mediaRemaining(), QStringLiteral("1:30:00"));
        QVERIFY(m_ctl->touchFocusAvailable());
        QVERIFY(m_ctl->manualFocusAvailable());
        QVERIFY(m_ctl->zoomAvailable());
        QVERIFY(!m_ctl->overheating());
    }

    void saveDestinationSwitchedToPcAndCard()
    {
        connectDemo();
        QTRY_COMPARE(m_ctl->saveDestination()->rawValue(), qint64(SDK::CrStillImageStoreDestination_HostPCAndMemoryCard));
    }

    void changingIsoRoundTripsThroughCamera()
    {
        connectDemo();
        m_ctl->iso()->stepBy(1);
        QCOMPARE(m_fake->setCalls().last(), qMakePair(quint32(SDK::CrDeviceProperty_IsoSensitivity), qint64(1000)));
        QTRY_COMPARE(m_ctl->iso()->text(), QStringLiteral("ISO 1000"));
    }

    void cameraSideChangeIsReflected()
    {
        connectDemo();
        m_fake->setPropertyValue(SDK::CrDeviceProperty_FNumber, 560);
        QTRY_COMPARE(m_ctl->fNumber()->text(), QStringLiteral("F5.6"));
    }

    void recordStartStop()
    {
        connectDemo();
        QSignalSpy rec(m_ctl, &CameraController::recordingChanged);
        m_ctl->startRecording();
        QCOMPARE(m_fake->commands().last(), qMakePair(Command::MovieRecord, true));
        QTRY_VERIFY(m_ctl->recording());
        QCOMPARE(rec.count(), 1);
        m_ctl->startRecording(); // already recording: ignored
        QCOMPARE(m_fake->commands().size(), 1);

        m_ctl->stopRecording();
        QCOMPARE(m_fake->commands().last(), qMakePair(Command::MovieRecord, false));
        QTRY_VERIFY(!m_ctl->recording());
        QCOMPARE(rec.count(), 2);
    }

    void toggleRecording()
    {
        connectDemo();
        m_ctl->toggleRecording();
        QTRY_VERIFY(m_ctl->recording());
        m_ctl->toggleRecording();
        QTRY_VERIFY(!m_ctl->recording());
    }

    void recordingClockTicks()
    {
        connectDemo();
        m_ctl->startRecording();
        QTRY_VERIFY(m_ctl->recording());
        QCOMPARE(m_ctl->recordingTime(), QStringLiteral("00:00"));
        QTRY_VERIFY_WITH_TIMEOUT(m_ctl->recordingSeconds() >= 1, 3000);
        QCOMPARE(m_ctl->recordingTime(), QStringLiteral("00:01"));
    }

    void commandsIgnoredWhenDisconnected()
    {
        m_ctl->startRecording();
        m_ctl->capturePhoto();
        m_ctl->touchFocus(0.5, 0.5);
        m_ctl->zoom(1);
        QVERIFY(m_fake->commands().isEmpty());
        QVERIFY(m_fake->setCalls().isEmpty());
    }

    void photoIsDownloaded()
    {
        connectDemo();
        QTRY_COMPARE(m_ctl->saveDestination()->rawValue(), qint64(SDK::CrStillImageStoreDestination_HostPCAndMemoryCard));
        QSignalSpy saved(m_ctl, &CameraController::photoSaved);
        m_ctl->capturePhoto();
        QCOMPARE(m_fake->commands().last(), qMakePair(Command::Release, true));
        QTRY_COMPARE(saved.count(), 1);
        QCOMPARE(m_fake->commands().last(), qMakePair(Command::Release, false));
        QVERIFY(QFileInfo::exists(m_ctl->lastDownload()));
        QVERIFY(m_ctl->lastDownload().startsWith(m_dir.path()));
    }

    void touchFocusPacksCoordinates()
    {
        connectDemo();
        m_ctl->touchFocus(1.0, 0.0);
        QCOMPARE(m_fake->setCalls().last(),
                 qMakePair(quint32(SDK::CrDeviceProperty_RemoteTouchOperation), qint64(639u << 16)));
        m_ctl->cancelTouchFocus();
        QCOMPARE(m_fake->commands().last(), qMakePair(Command::CancelRemoteTouch, false));
    }

    void touchFocusRespectsEnableStatus()
    {
        connectDemo();
        m_fake->setPropertyValue(SDK::CrDeviceProperty_RemoteTouchOperationEnableStatus,
                                 SDK::CrRemoteTouchOperation_Disable);
        QTRY_VERIFY(!m_ctl->touchFocusAvailable());
        const int before = m_fake->setCalls().size();
        m_ctl->touchFocus(0.5, 0.5);
        QCOMPARE(m_fake->setCalls().size(), before);
    }

    void focusStepIsClamped()
    {
        connectDemo();
        m_ctl->focusStep(3);
        QCOMPARE(m_fake->setCalls().last(), qMakePair(quint32(SDK::CrDeviceProperty_NearFar), qint64(3)));
        m_ctl->focusStep(-20);
        QCOMPARE(m_fake->setCalls().last().second, qint64(-7));
    }

    void zoomTeleWideStop()
    {
        connectDemo();
        m_ctl->zoom(1);
        QCOMPARE(m_fake->setCalls().last(), qMakePair(quint32(SDK::CrDeviceProperty_Zoom_Operation), qint64(1)));
        m_ctl->zoom(-5);
        QCOMPARE(m_fake->setCalls().last().second, qint64(-1));
        m_ctl->zoom(0);
        QCOMPARE(m_fake->setCalls().last().second, qint64(0));
    }

    void liveViewFramesReachController()
    {
        connectDemo();
        QSignalSpy frames(m_ctl, &CameraController::frameReady);
        m_fake->emitFrame();
        QCOMPARE(frames.count(), 1);
        QCOMPARE(m_ctl->frameSize(), QSize(1024, 576));
        QCOMPARE(m_ctl->frameCount(), 1);
    }

    void unexpectedDisconnectResetsState()
    {
        connectDemo();
        m_ctl->startRecording();
        QTRY_VERIFY(m_ctl->recording());
        m_fake->dropConnection(QStringLiteral("Wi-Fi perdido"));
        QCOMPARE(m_ctl->state(), CameraController::Idle);
        QVERIFY(!m_ctl->recording());
        QVERIFY(!m_ctl->iso()->available());
        QVERIFY(m_ctl->frame().isNull());
        QVERIFY(m_ctl->statusMessage().contains(QStringLiteral("Wi-Fi perdido")));
    }

    void userDisconnect()
    {
        connectDemo();
        m_ctl->disconnectCamera();
        QTRY_COMPARE(m_ctl->state(), CameraController::Idle);
        QVERIFY(!m_fake->isConnected());
    }

    void sshCredentialsFlow()
    {
        m_fake->setRequiresCredentials(true);
        QSignalSpy asked(m_ctl, &CameraController::credentialsRequested);
        m_ctl->discover();
        QTRY_COMPARE(m_ctl->cameras().size(), 1);
        m_ctl->connectTo(0);
        QTRY_COMPARE(asked.count(), 1);
        QCOMPARE(m_ctl->state(), CameraController::Connecting);
        QVERIFY(!m_ctl->pendingFingerprint().isEmpty());
        m_ctl->submitCredentials(QStringLiteral("admin"), QStringLiteral("secret"));
        QTRY_COMPARE(m_ctl->state(), CameraController::Connected);
        QCOMPARE(m_fake->lastCredentials().password, QStringLiteral("secret"));
        QCOMPARE(m_fake->lastCredentials().fingerprint, m_ctl->pendingFingerprint());
    }

    void cancelCredentials()
    {
        m_fake->setRequiresCredentials(true);
        QSignalSpy asked(m_ctl, &CameraController::credentialsRequested);
        m_ctl->discover();
        QTRY_COMPARE(m_ctl->cameras().size(), 1);
        m_ctl->connectTo(0);
        QTRY_COMPARE(asked.count(), 1);
        m_ctl->cancelCredentials();
        QCOMPARE(m_ctl->state(), CameraController::Idle);
    }

    void connectByAddress()
    {
        m_ctl->connectToAddress(QStringLiteral("not-an-ip"), CameraController::defaultModelIndex());
        QCOMPARE(m_ctl->state(), CameraController::Idle);
        m_ctl->connectToAddress(QStringLiteral("192.0.2.20"), CameraController::defaultModelIndex());
        QTRY_COMPARE(m_ctl->state(), CameraController::Connected);
    }

    // --- last camera / auto-reconnect ---

    void connectRemembersCamera()
    {
        auto *settings = new QSettings(m_dir.filePath("prefs.ini"), QSettings::IniFormat);
        CameraController ctl(new FakeBackend, settings);
        QSignalSpy changed(&ctl, &CameraController::lastCameraChanged);
        QVERIFY(ctl.lastCamera().isEmpty());
        ctl.discover();
        QTRY_COMPARE(ctl.cameras().size(), 1);
        ctl.connectTo(0);
        QTRY_COMPARE(ctl.state(), CameraController::Connected);
        QCOMPARE(changed.count(), 1);
        QCOMPARE(ctl.lastCamera(), QStringLiteral("ZV-E10M2 (192.168.122.1)"));

        QSettings check(m_dir.filePath("prefs.ini"), QSettings::IniFormat);
        QCOMPARE(check.beginReadArray("devices"), 1);
        check.setArrayIndex(0);
        QCOMPARE(check.value("model").toString(), QStringLiteral("ZV-E10M2"));
        QCOMPARE(check.value("address").toString(), QStringLiteral("192.168.122.1"));
        QCOMPARE(check.value("uid").toString(), QStringLiteral("AA:BB:CC:DD:EE:FF"));
        check.endArray();
    }

    void termsAcceptanceIsRemembered()
    {
        const QString path = m_dir.filePath("terms.ini");
        {
            CameraController ctl(new FakeBackend, new QSettings(path, QSettings::IniFormat));
            QVERIFY(!ctl.termsAccepted()); // first start
            QSignalSpy changed(&ctl, &CameraController::termsAcceptedChanged);
            ctl.acceptTerms();
            QVERIFY(ctl.termsAccepted());
            QCOMPARE(changed.count(), 1);
        }
        {
            CameraController ctl(new FakeBackend, new QSettings(path, QSettings::IniFormat));
            QVERIFY(ctl.termsAccepted());
        }
        {
            // Newer terms than the ones accepted: ask again.
            QSettings s(path, QSettings::IniFormat);
            s.setValue("legal/acceptedTermsVersion", CameraController::kTermsVersion - 1);
        }
        CameraController ctl(new FakeBackend, new QSettings(path, QSettings::IniFormat));
        QVERIFY(!ctl.termsAccepted());
        ctl.setTermsAccepted(true, false); // this session only
        QVERIFY(ctl.termsAccepted());
        QSettings check(path, QSettings::IniFormat);
        QCOMPARE(check.value("legal/acceptedTermsVersion").toInt(), CameraController::kTermsVersion - 1);
    }

    void reconnectLastConnectsToSavedCamera()
    {
        {
            QSettings s(m_dir.filePath("prefs.ini"), QSettings::IniFormat);
            s.setValue("lastCamera/model", "ZV-E10M2");
            s.setValue("lastCamera/connection", "Demo");
            s.setValue("lastCamera/address", "192.168.122.1");
        }
        auto *fake = new FakeBackend;
        CameraController ctl(fake, new QSettings(m_dir.filePath("prefs.ini"), QSettings::IniFormat));
        ctl.reconnectLast();
        QTRY_COMPARE(ctl.state(), CameraController::Connected);
        QCOMPARE(ctl.modelName(), QStringLiteral("ZV-E10M2"));
    }

    void reconnectLastFallsBackToSavedIp()
    {
        // Camera not announced by discovery, but still answering on its router IP.
        {
            QSettings s(m_dir.filePath("prefs.ini"), QSettings::IniFormat);
            s.setValue("lastCamera/model", "ZV-E10M2");
            s.setValue("lastCamera/connection", "Wi-Fi/LAN");
            s.setValue("lastCamera/address", "198.51.100.50");
        }
        CameraController ctl(new FakeBackend, new QSettings(m_dir.filePath("prefs.ini"), QSettings::IniFormat));
        ctl.reconnectLast();
        QTRY_COMPARE(ctl.state(), CameraController::Connected);
        QCOMPARE(ctl.lastCamera(), QStringLiteral("ZV-E10M2 (198.51.100.50)"));
    }

    void reconnectLastWithoutSavedCameraOnlyDiscovers()
    {
        m_ctl->reconnectLast();
        QTRY_COMPARE(m_ctl->cameras().size(), 1);
        QTest::qWait(50);
        QCOMPARE(m_ctl->state(), CameraController::Idle);
        QVERIFY(!m_fake->isConnected());
    }

    void reconnectLastGivesUpWhenCameraIsOff()
    {
        {
            QSettings s(m_dir.filePath("prefs.ini"), QSettings::IniFormat);
            s.setValue("lastCamera/model", "ZV-E10M2");
            s.setValue("lastCamera/connection", "Demo");
            s.setValue("lastCamera/address", "192.168.122.1");
        }
        auto *fake = new FakeBackend;
        fake->setUnresponsive(true);
        CameraController ctl(fake, new QSettings(m_dir.filePath("prefs.ini"), QSettings::IniFormat));
        ctl.setAutoConnectTimeout(200);
        ctl.reconnectLast();
        QTRY_COMPARE(ctl.state(), CameraController::Connecting);
        QTRY_COMPARE(ctl.state(), CameraController::Idle);
        QVERIFY(ctl.statusMessage().contains(QStringLiteral("não respondeu")));
        // The user can still connect by hand afterwards.
        fake->setUnresponsive(false);
        ctl.connectTo(0);
        QTRY_COMPARE(ctl.state(), CameraController::Connected);
    }

    void reconnectLastWaitsForCredentials()
    {
        {
            QSettings s(m_dir.filePath("prefs.ini"), QSettings::IniFormat);
            s.setValue("lastCamera/model", "ZV-E10M2");
            s.setValue("lastCamera/connection", "Demo");
            s.setValue("lastCamera/address", "192.168.122.1");
        }
        auto *fake = new FakeBackend;
        fake->setRequiresCredentials(true);
        CameraController ctl(fake, new QSettings(m_dir.filePath("prefs.ini"), QSettings::IniFormat));
        QSignalSpy asked(&ctl, &CameraController::credentialsRequested);
        ctl.setAutoConnectTimeout(150);
        ctl.reconnectLast();
        QTRY_COMPARE(asked.count(), 1);
        QTest::qWait(300); // user typing past the auto-connect timeout
        QCOMPARE(ctl.state(), CameraController::Connecting);
        ctl.submitCredentials(QStringLiteral("admin"), QStringLiteral("secret"));
        QTRY_COMPARE(ctl.state(), CameraController::Connected);
    }

    void mirrorPreviewIsPersisted()
    {
        {
            CameraController ctl(new FakeBackend, new QSettings(m_dir.filePath("prefs.ini"), QSettings::IniFormat));
            QVERIFY(!ctl.mirrorPreview());
            QSignalSpy spy(&ctl, &CameraController::mirrorPreviewChanged);
            ctl.setMirrorPreview(true);
            ctl.setMirrorPreview(true);
            QCOMPARE(spy.count(), 1);
        }
        CameraController again(new FakeBackend, new QSettings(m_dir.filePath("prefs.ini"), QSettings::IniFormat));
        QVERIFY(again.mirrorPreview());
    }

    // --- recording failures ---

    void recordingFailedIsReported()
    {
        connectDemo();
        m_fake->setRecordResponse(FakeBackend::RecordResponse::Fail);
        m_ctl->startRecording();
        QTRY_VERIFY(!m_ctl->recordingError().isEmpty());
        QVERIFY(!m_ctl->recording());
        QVERIFY(m_ctl->recordingError().startsWith(QStringLiteral("Falha na gravação")));
        m_ctl->dismissRecordingError();
        QVERIFY(m_ctl->recordingError().isEmpty());
    }

    void recordingNotConfirmedIsReported()
    {
        connectDemo();
        m_ctl->setRecordConfirmTimeout(150);
        m_fake->setRecordResponse(FakeBackend::RecordResponse::Ignore);
        m_ctl->startRecording();
        QTRY_VERIFY(m_ctl->recordingError().contains(QStringLiteral("não iniciou")));
        QVERIFY(!m_ctl->recording());
    }

    void cameraSideStopIsReported()
    {
        connectDemo();
        m_fake->setPropertyValue(SDK::CrDeviceProperty_MediaSLOT1_RemainingTime, 0);
        m_ctl->startRecording();
        QTRY_VERIFY(m_ctl->recording());
        m_fake->setPropertyValue(SDK::CrDeviceProperty_RecordingState, SDK::CrMovie_Recording_State_Not_Recording);
        QTRY_VERIFY(!m_ctl->recording());
        QVERIFY(m_ctl->recordingError().contains(QStringLiteral("cartão está cheio")));
    }

    void userStopIsNotAnError()
    {
        connectDemo();
        m_ctl->setRecordConfirmTimeout(150);
        m_ctl->startRecording();
        QTRY_VERIFY(m_ctl->recording());
        m_ctl->stopRecording();
        QTRY_VERIFY(!m_ctl->recording());
        QTest::qWait(250); // past the confirm timeout
        QVERIFY(m_ctl->recordingError().isEmpty());
    }

    void audioSettingsExposed()
    {
        connectDemo();
        QVERIFY(m_ctl->audioRecording()->available());
        QCOMPARE(m_ctl->audioRecording()->text(), QStringLiteral("Ligado"));
        QCOMPARE(m_ctl->audioLevel()->text(), QStringLiteral("26"));
        m_ctl->audioLevel()->stepBy(1);
        QTRY_COMPARE(m_ctl->audioLevel()->text(), QStringLiteral("27"));
        m_ctl->audioRecording()->setValue(SDK::CrAudioRecording_Off);
        QTRY_COMPARE(m_ctl->audioRecording()->text(), QStringLiteral("Desligado"));
    }

    void connectHintShownWhileCameraWaits()
    {
        m_fake->setUnresponsive(true);
        m_ctl->setConnectHintDelay(100);
        m_ctl->discover();
        QTRY_COMPARE(m_ctl->cameras().size(), 1);
        m_ctl->connectTo(0);
        QCOMPARE(m_ctl->state(), CameraController::Connecting);
        QTRY_VERIFY(m_ctl->statusMessage().contains(QStringLiteral("Permitir emparelhamento")));
    }

    void connectHintNotShownWhileAskingCredentials()
    {
        m_fake->setRequiresCredentials(true);
        m_ctl->setConnectHintDelay(100);
        QSignalSpy asked(m_ctl, &CameraController::credentialsRequested);
        m_ctl->discover();
        QTRY_COMPARE(m_ctl->cameras().size(), 1);
        m_ctl->connectTo(0);
        QTRY_COMPARE(asked.count(), 1);
        QTest::qWait(200);
        QVERIFY(m_ctl->statusMessage().contains(QStringLiteral("usuário e senha")));
    }

    void liveViewQualityChangesFrameSize()
    {
        connectDemo();
        m_fake->emitFrame();
        QTRY_COMPARE(m_ctl->frameSize(), QSize(1024, 576));
        m_ctl->liveViewQuality()->setValue(SDK::CrPropertyLiveViewImageQuality_Low);
        QTRY_COMPARE(m_ctl->liveViewQuality()->text(), QStringLiteral("Baixa"));
        m_fake->emitFrame();
        QTRY_COMPARE(m_ctl->frameSize(), QSize(640, 360));
    }

    void liveViewFpsIsMeasured()
    {
        connectDemo();
        QCOMPARE(m_ctl->liveViewFps(), 0);
        QSignalSpy spy(m_ctl, &CameraController::liveViewFpsChanged);
        for (int i = 0; i < 12; ++i) {
            m_fake->emitFrame();
            QTest::qWait(20);
        }
        QTRY_VERIFY_WITH_TIMEOUT(m_ctl->liveViewFps() > 0, 2500);
    }

    void sidePanelWidthIsClampedAndPersisted()
    {
        {
            CameraController ctl(new FakeBackend, new QSettings(m_dir.filePath("prefs.ini"), QSettings::IniFormat));
            QCOMPARE(ctl.sidePanelWidth(), 340);
            ctl.setSidePanelWidth(9999);
            QCOMPARE(ctl.sidePanelWidth(), 560);
            ctl.setSidePanelWidth(400);
        }
        CameraController again(new FakeBackend, new QSettings(m_dir.filePath("prefs.ini"), QSettings::IniFormat));
        QCOMPARE(again.sidePanelWidth(), 400);
    }

    // --- saved SSH logins ---


    void rememberedLoginIsUsedNextTime()
    {
        SshRig rig(m_dir.filePath("prefs.ini"));
        QSignalSpy asked(rig.ctl, &CameraController::credentialsRequested);
        rig.ctl->discover();
        QTRY_COMPARE(rig.ctl->cameras().size(), 1);
        rig.ctl->connectTo(0);
        QTRY_COMPARE(asked.count(), 1); // nothing saved yet
        rig.ctl->submitCredentials(QStringLiteral("admin"), QStringLiteral("secret"), true);
        QTRY_COMPARE(rig.ctl->state(), CameraController::Connected);
        QVERIFY(rig.store->contains(kFp));
        QCOMPARE(rig.store->item(kFp).first, QStringLiteral("admin"));
        QVERIFY(rig.ctl->devices().first().toMap().value("savedLogin").toBool());

        rig.ctl->disconnectCamera();
        QTRY_COMPARE(rig.ctl->state(), CameraController::Idle);
        rig.ctl->connectDevice(rig.ctl->devices().first().toMap().value("key").toString());
        QTRY_COMPARE(rig.ctl->state(), CameraController::Connected);
        QCOMPARE(asked.count(), 1); // no dialog the second time
        QCOMPARE(rig.fake->lastCredentials().password, QStringLiteral("secret"));
    }

    void loginNotSavedWhenNotAsked()
    {
        SshRig rig(m_dir.filePath("prefs.ini"));
        QSignalSpy asked(rig.ctl, &CameraController::credentialsRequested);
        rig.ctl->discover();
        QTRY_COMPARE(rig.ctl->cameras().size(), 1);
        rig.ctl->connectTo(0);
        QTRY_COMPARE(asked.count(), 1);
        rig.ctl->submitCredentials(QStringLiteral("admin"), QStringLiteral("secret"), false);
        QTRY_COMPARE(rig.ctl->state(), CameraController::Connected);
        QVERIFY(!rig.store->contains(kFp));
        QVERIFY(!rig.ctl->devices().first().toMap().value("savedLogin").toBool());
    }

    void rejectedSavedLoginIsDeletedAndNotRetried()
    {
        SshRig rig(m_dir.filePath("prefs.ini"));
        rig.store->save(kFp, QString(), QStringLiteral("admin"), QStringLiteral("old-password"));
        QSignalSpy asked(rig.ctl, &CameraController::credentialsRequested);
        rig.ctl->discover();
        QTRY_COMPARE(rig.ctl->cameras().size(), 1);
        rig.ctl->connectTo(0);
        QTRY_COMPARE(asked.count(), 1);
        QCOMPARE(rig.ctl->state(), CameraController::Connecting);
        QVERIFY(rig.ctl->credentialsError().contains(QStringLiteral("login salvo")));
        QVERIFY(!rig.store->contains(kFp));
        // First attempt asks for the fingerprint, second used the saved login. No more.
        QTest::qWait(100);
        QCOMPARE(rig.fake->connectAttempts(), 2);

        rig.ctl->submitCredentials(QStringLiteral("admin"), QStringLiteral("secret"), true);
        QTRY_COMPARE(rig.ctl->state(), CameraController::Connected);
        QCOMPARE(rig.store->item(kFp).second, QStringLiteral("secret"));
    }

    void wrongTypedPasswordAsksAgain()
    {
        SshRig rig(m_dir.filePath("prefs.ini"));
        QSignalSpy asked(rig.ctl, &CameraController::credentialsRequested);
        rig.ctl->discover();
        QTRY_COMPARE(rig.ctl->cameras().size(), 1);
        rig.ctl->connectTo(0);
        QTRY_COMPARE(asked.count(), 1);
        QVERIFY(rig.ctl->credentialsError().isEmpty());
        rig.ctl->submitCredentials(QStringLiteral("admin"), QStringLiteral("typo"), true);
        QTRY_COMPARE(asked.count(), 2);
        QVERIFY(rig.ctl->credentialsError().contains(QStringLiteral("incorretos")));
        QVERIFY(!rig.store->contains(kFp)); // a rejected password is never saved
        rig.ctl->submitCredentials(QStringLiteral("admin"), QStringLiteral("secret"), true);
        QTRY_COMPARE(rig.ctl->state(), CameraController::Connected);
    }

    // --- camera list (history + online status) ---

    void devicesShowOnlineAndOffline()
    {
        auto *settings = new QSettings(m_dir.filePath("prefs.ini"), QSettings::IniFormat);
        auto *fake = new FakeBackend;
        CameraController ctl(fake, settings);
        ctl.discover();
        QTRY_COMPARE(ctl.devices().size(), 1);
        QVERIFY(!ctl.devices().first().toMap().value("known").toBool()); // found, never used
        ctl.connectDevice(ctl.devices().first().toMap().value("key").toString());
        QTRY_COMPARE(ctl.state(), CameraController::Connected);
        ctl.disconnectCamera();
        QTRY_COMPARE(ctl.state(), CameraController::Idle);

        auto device = ctl.devices().first().toMap();
        QCOMPARE(ctl.devices().size(), 1);
        QVERIFY(device.value("known").toBool());
        QVERIFY(device.value("online").toBool());
        QCOMPARE(device.value("lastUsed").toString(), QStringLiteral("agora"));

        fake->setDiscoverable(false); // camera switched off
        ctl.discover();
        QTRY_VERIFY(!ctl.devices().first().toMap().value("online").toBool());
        device = ctl.devices().first().toMap();
        QVERIFY(device.value("known").toBool());
        QVERIFY(device.value("canConnect").toBool()); // Wi-Fi: can still try its last IP
    }

    void connectingToOfflineDeviceTriesLastAddress()
    {
        auto *fake = new FakeBackend;
        CameraController ctl(fake, new QSettings(m_dir.filePath("prefs.ini"), QSettings::IniFormat));
        ctl.discover();
        QTRY_COMPARE(ctl.cameras().size(), 1);
        ctl.connectTo(0);
        QTRY_COMPARE(ctl.state(), CameraController::Connected);
        ctl.disconnectCamera();
        QTRY_COMPARE(ctl.state(), CameraController::Idle);
        const QString key = ctl.devices().first().toMap().value("key").toString();

        fake->setDiscoverable(false);
        ctl.discover();
        QTRY_VERIFY(!ctl.devices().first().toMap().value("online").toBool());
        ctl.connectDevice(key);
        QCOMPARE(ctl.state(), CameraController::Connecting);
        QTRY_COMPARE(ctl.state(), CameraController::Idle);
        QVERIFY(ctl.statusMessage().contains(QStringLiteral("não respondeu")));
        QCOMPARE(ctl.devices().size(), 1); // no duplicate entry for the IP attempt
    }

    void forgetDeviceRemovesEntryAndSavedLogin()
    {
        SshRig rig(m_dir.filePath("prefs.ini"));
        QSignalSpy asked(rig.ctl, &CameraController::credentialsRequested);
        rig.ctl->discover();
        QTRY_COMPARE(rig.ctl->cameras().size(), 1);
        rig.ctl->connectTo(0);
        QTRY_COMPARE(asked.count(), 1);
        rig.ctl->submitCredentials(QStringLiteral("admin"), QStringLiteral("secret"), true);
        QTRY_COMPARE(rig.ctl->state(), CameraController::Connected);
        rig.ctl->disconnectCamera();
        QTRY_COMPARE(rig.ctl->state(), CameraController::Idle);

        rig.ctl->forgetDevice(rig.ctl->devices().first().toMap().value("key").toString());
        QVERIFY(!rig.store->contains(kFp));
        QVERIFY(rig.ctl->history().isEmpty());
        QVERIFY(rig.ctl->lastCamera().isEmpty());
        // Still on the network: listed again as a newly found camera.
        QCOMPARE(rig.ctl->devices().size(), 1);
        QVERIFY(!rig.ctl->devices().first().toMap().value("known").toBool());
    }

    void idleScreenRefreshesOnlineStatus()
    {
        m_ctl->setRefreshInterval(100);
        QSignalSpy found(m_ctl, &CameraController::camerasChanged);
        m_ctl->discover();
        QTRY_VERIFY_WITH_TIMEOUT(found.count() >= 3, 3000);
        connectDemo();
        const int before = found.count();
        QTest::qWait(350);
        QCOMPARE(found.count(), before); // no searching while connected
    }

    void connectDuringDiscoveryWaitsForIt()
    {
        m_ctl->discover();
        QTRY_COMPARE(m_ctl->cameras().size(), 1);
        const QString key = m_ctl->devices().first().toMap().value("key").toString();
        m_ctl->discover();
        QCOMPARE(m_ctl->state(), CameraController::Discovering);
        m_ctl->connectDevice(key);
        QTRY_COMPARE(m_ctl->state(), CameraController::Connected);
    }

    // --- disconnecting / connection loss ---

    void userDisconnectReturnsImmediately()
    {
        connectDemo();
        m_ctl->disconnectCamera();
        QCOMPARE(m_ctl->state(), CameraController::Idle); // no wait for the camera's confirmation
        QVERIFY(!m_fake->liveViewEnabled());
        QVERIFY(!m_ctl->iso()->available());
    }

    void lateDisconnectDoesNotCloseNextSession()
    {
        connectDemo();
        m_ctl->disconnectCamera();
        m_ctl->connectTo(0); // reconnect before the old session's confirmation arrives
        QTRY_COMPARE(m_ctl->state(), CameraController::Connected);
        QTest::qWait(100);
        QCOMPARE(m_ctl->state(), CameraController::Connected);
    }

    void lostConnectionReturnsToCameraList()
    {
        connectDemo();
        m_ctl->setConnectionLossTimeout(150);
        m_fake->simulateReconnecting();
        QVERIFY(m_ctl->reconnecting());
        QCOMPARE(m_ctl->state(), CameraController::Connected); // short Wi-Fi drops are tolerated
        QTRY_COMPARE(m_ctl->state(), CameraController::Idle);
        QVERIFY(!m_ctl->reconnecting());
        QVERIFY(m_ctl->statusMessage().contains(QStringLiteral("perdida")));
    }

    void connectionThatComesBackKeepsTheSession()
    {
        connectDemo();
        m_ctl->setConnectionLossTimeout(150);
        m_fake->simulateReconnecting();
        m_fake->simulateReconnected();
        QVERIFY(!m_ctl->reconnecting());
        QTest::qWait(250);
        QCOMPARE(m_ctl->state(), CameraController::Connected);

        m_fake->simulateReconnecting();
        m_fake->emitFrame(); // live view resumed
        QTRY_VERIFY(!m_ctl->reconnecting());
        QTest::qWait(250);
        QCOMPARE(m_ctl->state(), CameraController::Connected);
    }

    // --- battery / memory card / video settings ---

    void lowBatteryWarnsOnceWithoutStoppingRecording()
    {
        connectDemo();
        QSignalSpy spy(m_ctl, &CameraController::lowBatteryChanged);
        m_ctl->startRecording();
        QTRY_VERIFY(m_ctl->recording());
        m_fake->setPropertyValue(SDK::CrDeviceProperty_BatteryRemain, 6);
        QTRY_COMPARE(m_ctl->batteryPercent(), 6);
        QVERIFY(!m_ctl->lowBattery());
        m_fake->setPropertyValue(SDK::CrDeviceProperty_BatteryRemain, 5);
        QTRY_VERIFY(m_ctl->lowBattery());
        QVERIFY(m_ctl->recording()); // silent: recording goes on
        QCOMPARE(m_fake->commands().count({Command::MovieRecord, false}), 0);
        m_ctl->dismissLowBattery();
        QVERIFY(!m_ctl->lowBattery());
        m_fake->setPropertyValue(SDK::CrDeviceProperty_BatteryRemain, 4);
        QTRY_COMPARE(m_ctl->batteryPercent(), 4);
        QVERIFY(!m_ctl->lowBattery()); // already warned for this discharge
        m_fake->setPropertyValue(SDK::CrDeviceProperty_BatteryRemain, 40); // charged
        QTRY_COMPARE(m_ctl->batteryPercent(), 40);
        m_fake->setPropertyValue(SDK::CrDeviceProperty_BatteryRemain, 5);
        QTRY_VERIFY(m_ctl->lowBattery());
    }

    void memoryCardSpaceIsEstimated()
    {
        connectDemo();
        QCOMPARE(m_ctl->mediaStatus(), QStringLiteral("OK"));
        QCOMPARE(m_ctl->mediaPhotosLeft(), 4321);
        // 1:30:00 left at 100 Mbps.
        QVERIFY(qAbs(m_ctl->mediaFreeGb() - 67.5) < 0.01);
        QCOMPARE(m_ctl->mediaUsedFraction(), -1.0); // capacity not entered
        m_ctl->setCardCapacityGb(128);
        QVERIFY(qAbs(m_ctl->mediaUsedFraction() - (1.0 - 67.5 / 128)) < 0.001);
        m_ctl->recordingSetting()->setValue(SDK::CrRecordingSettingMovie_30p_60M_XAVC_S_4K);
        QTRY_VERIFY(qAbs(m_ctl->mediaFreeGb() - 40.5) < 0.01);
    }

    void cardCapacityIsPersisted()
    {
        {
            CameraController ctl(new FakeBackend, new QSettings(m_dir.filePath("prefs.ini"), QSettings::IniFormat));
            ctl.setCardCapacityGb(256);
        }
        CameraController again(new FakeBackend, new QSettings(m_dir.filePath("prefs.ini"), QSettings::IniFormat));
        QCOMPARE(again.cardCapacityGb(), 256);
    }

    void videoResolutionAndQualityExposed()
    {
        connectDemo();
        QCOMPARE(m_ctl->movieFileFormat()->text(), QStringLiteral("XAVC S 4K · 3840×2160"));
        QCOMPARE(m_ctl->recordingSetting()->text(), QStringLiteral("30p · 100 Mbps · XAVC S 4K"));
        m_ctl->movieFileFormat()->setValue(SDK::CrFileFormatMovie_XAVC_S_HD);
        QTRY_COMPARE(m_ctl->movieFileFormat()->text(), QStringLiteral("XAVC S HD · 1920×1080"));
    }

    void everySettingHasADescription()
    {
        for (CameraProperty *p : {m_ctl->exposureMode(), m_ctl->fNumber(), m_ctl->shutterSpeed(), m_ctl->iso(),
                                  m_ctl->exposureBias(), m_ctl->whiteBalance(), m_ctl->colorTemp(),
                                  m_ctl->focusMode(), m_ctl->focusArea(), m_ctl->subjectRecognition(),
                                  m_ctl->pictureProfile(), m_ctl->creativeLook(), m_ctl->cinematicVlog(),
                                  m_ctl->frameRate(), m_ctl->liveViewQuality(), m_ctl->saveDestination(),
                                  m_ctl->audioRecording(), m_ctl->audioLevel(), m_ctl->movieFileFormat(),
                                  m_ctl->recordingSetting()})
            QVERIFY2(!p->description().isEmpty(), qPrintable(p->name()));
    }

    void supportedModelsMatchSdkEnum()
    {
        const QStringList models = CameraController::supportedModels();
        QCOMPARE(models.size(), int(SDK::CrCameraDeviceModel_ILCE_7RM6) + 1);
        QCOMPARE(models.at(SDK::CrCameraDeviceModel_ZV_E10M2), QStringLiteral("ZV-E10M2"));
        QCOMPARE(models.at(SDK::CrCameraDeviceModel_ILCE_6700), QStringLiteral("ILCE-6700"));
        QCOMPARE(models.at(CameraController::defaultModelIndex()), QStringLiteral("ZV-E10M2"));
    }
};

QTEST_MAIN(TestCameraController)
#include "tst_cameracontroller.moc"
