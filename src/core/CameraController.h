#pragma once

#include "core/CameraBackend.h"
#include "core/CameraProperty.h"
#include "core/CredentialStore.h"
#include "core/DeviceHistory.h"
#include "core/UsbSetup.h"

#include <QElapsedTimer>
#include <QHash>
#include <QImage>
#include <QObject>
#include <QSettings>
#include <QSize>
#include <QTimer>
#include <QVariantList>
#include <functional>

namespace kl {

// Everything the QML UI needs: connection flow, live view frames, recording,
// focus/zoom actions and one CameraProperty per exposed setting.
class CameraController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(QVariantList cameras READ cameras NOTIFY camerasChanged)
    Q_PROPERTY(QString modelName READ modelName NOTIFY stateChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(QString pendingFingerprint READ pendingFingerprint NOTIFY credentialsRequested)
    Q_PROPERTY(QString credentialsError READ credentialsError NOTIFY credentialsRequested)
    Q_PROPERTY(bool credentialStoreAvailable READ credentialStoreAvailable CONSTANT)
    Q_PROPERTY(QVariantList devices READ devices NOTIFY devicesChanged)
    Q_PROPERTY(bool recording READ recording NOTIFY recordingChanged)
    Q_PROPERTY(int recordingSeconds READ recordingSeconds NOTIFY recordingSecondsChanged)
    Q_PROPERTY(QString recordingTime READ recordingTime NOTIFY recordingSecondsChanged)
    Q_PROPERTY(int batteryPercent READ batteryPercent NOTIFY statusChanged)
    Q_PROPERTY(QString mediaRemaining READ mediaRemaining NOTIFY statusChanged)
    Q_PROPERTY(bool overheating READ overheating NOTIFY statusChanged)
    Q_PROPERTY(QString mediaStatus READ mediaStatus NOTIFY statusChanged)
    Q_PROPERTY(bool mediaOk READ mediaOk NOTIFY statusChanged)
    Q_PROPERTY(bool colorTempMode READ colorTempMode NOTIFY statusChanged)
    Q_PROPERTY(bool audioEnabled READ audioEnabled NOTIFY statusChanged)
    Q_PROPERTY(int mediaPhotosLeft READ mediaPhotosLeft NOTIFY statusChanged)
    Q_PROPERTY(double mediaFreeGb READ mediaFreeGb NOTIFY statusChanged)
    Q_PROPERTY(double mediaUsedFraction READ mediaUsedFraction NOTIFY statusChanged)
    Q_PROPERTY(int cardCapacityGb READ cardCapacityGb WRITE setCardCapacityGb NOTIFY statusChanged)
    Q_PROPERTY(bool lowBattery READ lowBattery NOTIFY lowBatteryChanged)
    Q_PROPERTY(bool reconnecting READ reconnecting NOTIFY reconnectingChanged)
    Q_PROPERTY(bool touchFocusAvailable READ touchFocusAvailable NOTIFY statusChanged)
    Q_PROPERTY(bool manualFocusAvailable READ manualFocusAvailable NOTIFY statusChanged)
    Q_PROPERTY(bool zoomAvailable READ zoomAvailable NOTIFY statusChanged)
    Q_PROPERTY(QString lastDownload READ lastDownload NOTIFY lastDownloadChanged)
    Q_PROPERTY(QString downloadDirectory READ downloadDirectory WRITE setDownloadDirectory NOTIFY downloadDirectoryChanged)
    Q_PROPERTY(QSize frameSize READ frameSize NOTIFY frameSizeChanged)
    Q_PROPERTY(int frameCount READ frameCount NOTIFY frameReady)
    Q_PROPERTY(int liveViewFps READ liveViewFps NOTIFY liveViewFpsChanged)
    Q_PROPERTY(int sidePanelWidth READ sidePanelWidth WRITE setSidePanelWidth NOTIFY sidePanelWidthChanged)
    Q_PROPERTY(QStringList supportedModels READ supportedModels CONSTANT)
    Q_PROPERTY(int defaultModelIndex READ defaultModelIndex CONSTANT)
    Q_PROPERTY(kl::UsbSetup *usb READ usb CONSTANT)
    Q_PROPERTY(QString lastCamera READ lastCamera NOTIFY lastCameraChanged)
    Q_PROPERTY(bool mirrorPreview READ mirrorPreview WRITE setMirrorPreview NOTIFY mirrorPreviewChanged)
    Q_PROPERTY(QString recordingError READ recordingError NOTIFY recordingErrorChanged)
    // The user accepted the terms shown on first start (Sony SDK conditions).
    Q_PROPERTY(bool termsAccepted READ termsAccepted NOTIFY termsAcceptedChanged)

    Q_PROPERTY(kl::CameraProperty *exposureMode READ exposureMode CONSTANT)
    Q_PROPERTY(kl::CameraProperty *fNumber READ fNumber CONSTANT)
    Q_PROPERTY(kl::CameraProperty *shutterSpeed READ shutterSpeed CONSTANT)
    Q_PROPERTY(kl::CameraProperty *iso READ iso CONSTANT)
    Q_PROPERTY(kl::CameraProperty *exposureBias READ exposureBias CONSTANT)
    Q_PROPERTY(kl::CameraProperty *whiteBalance READ whiteBalance CONSTANT)
    Q_PROPERTY(kl::CameraProperty *colorTemp READ colorTemp CONSTANT)
    Q_PROPERTY(kl::CameraProperty *focusMode READ focusMode CONSTANT)
    Q_PROPERTY(kl::CameraProperty *focusArea READ focusArea CONSTANT)
    Q_PROPERTY(kl::CameraProperty *subjectRecognition READ subjectRecognition CONSTANT)
    Q_PROPERTY(kl::CameraProperty *pictureProfile READ pictureProfile CONSTANT)
    Q_PROPERTY(kl::CameraProperty *creativeLook READ creativeLook CONSTANT)
    Q_PROPERTY(kl::CameraProperty *cinematicVlog READ cinematicVlog CONSTANT)
    Q_PROPERTY(kl::CameraProperty *frameRate READ frameRate CONSTANT)
    Q_PROPERTY(kl::CameraProperty *liveViewQuality READ liveViewQuality CONSTANT)
    Q_PROPERTY(kl::CameraProperty *saveDestination READ saveDestination CONSTANT)
    Q_PROPERTY(kl::CameraProperty *audioRecording READ audioRecording CONSTANT)
    Q_PROPERTY(kl::CameraProperty *audioLevel READ audioLevel CONSTANT)
    Q_PROPERTY(kl::CameraProperty *movieFileFormat READ movieFileFormat CONSTANT)
    Q_PROPERTY(kl::CameraProperty *recordingSetting READ recordingSetting CONSTANT)

public:
    enum State { Idle, Discovering, Connecting, Connected };
    Q_ENUM(State)

    // Takes ownership of the backend, `settings` and `credentials`. Without settings,
    // preferences (camera history, mirroring) only live for this session; without a
    // credential store, SSH logins are never saved.
    explicit CameraController(CameraBackend *backend, QSettings *settings = nullptr,
                              CredentialStore *credentials = nullptr, QObject *parent = nullptr);
    ~CameraController() override;

    State state() const { return m_state; }
    QVariantList cameras() const;
    QString modelName() const { return m_modelName; }
    QString statusMessage() const { return m_statusMessage; }
    QString pendingFingerprint() const { return m_pendingFingerprint; }
    QString credentialsError() const { return m_credentialsError; }
    bool credentialStoreAvailable() const { return m_credentialStore && m_credentialStore->available(); }
    // Known cameras (history, most recent first) followed by newly discovered ones.
    // Each: key, model, connection, address, known, online, savedLogin, lastUsed, canConnect.
    QVariantList devices() const;
    const DeviceHistory &history() const { return m_history; }
    bool recording() const;
    int recordingSeconds() const { return m_recordingSeconds; }
    QString recordingTime() const;
    int batteryPercent() const;
    QString mediaRemaining() const;
    bool overheating() const;
    QString mediaStatus() const;
    // Card status is fine or not reported yet.
    bool mediaOk() const;
    // White balance set to a color temperature (the Kelvin value applies).
    bool colorTempMode() const;
    bool audioEnabled() const;
    int mediaPhotosLeft() const;
    // Free space estimated from the remaining recording time and the current bitrate (the SDK
    // does not report card capacity); -1 when unknown.
    double mediaFreeGb() const;
    // Used part of the card, from the capacity the user entered; -1 when unknown.
    double mediaUsedFraction() const;
    int cardCapacityGb() const { return m_cardCapacityGb; }
    void setCardCapacityGb(int gb);
    bool lowBattery() const { return m_lowBattery; }
    bool reconnecting() const { return m_reconnecting; }
    void setConnectionLossTimeout(int ms) { m_lostTimer.setInterval(ms); }
    static constexpr int kLowBatteryPercent = 5;
    bool touchFocusAvailable() const;
    bool manualFocusAvailable() const;
    bool zoomAvailable() const;
    QString lastDownload() const { return m_lastDownload; }
    QString downloadDirectory() const { return m_downloadDirectory; }
    void setDownloadDirectory(const QString &path);
    QSize frameSize() const { return m_frame.size(); }
    int frameCount() const { return m_frameCount; }
    int liveViewFps() const { return m_liveViewFps; }
    int sidePanelWidth() const { return m_sidePanelWidth; }
    void setSidePanelWidth(int width);
    QImage frame() const { return m_frame; }
    QString lastCamera() const;
    bool mirrorPreview() const { return m_mirrorPreview; }
    void setMirrorPreview(bool mirrored);
    QString recordingError() const { return m_recordingError; }
    bool termsAccepted() const { return m_termsAccepted; }
    // `remember` saves the acceptance in the settings; otherwise it lasts for this session.
    void setTermsAccepted(bool accepted, bool remember = true);
    // Bump when the terms change, so everyone is asked again.
    static constexpr int kTermsVersion = 1;
    // How long reconnectLast() waits for the saved camera, and how long the camera has to
    // confirm a record start/stop before it is reported as a failure.
    void setAutoConnectTimeout(int ms) { m_autoConnectTimer.setInterval(ms); }
    void setConnectHintDelay(int ms) { m_connectHintTimer.setInterval(ms); }
    // While idle on the connection screen, discovery repeats to keep online status fresh.
    void setRefreshInterval(int ms) { m_refreshTimer.setInterval(ms); }
    void setRecordConfirmTimeout(int ms) { m_recordConfirmTimer.setInterval(ms); }
    static QStringList supportedModels();
    static int defaultModelIndex();

    CameraProperty *exposureMode() const;
    CameraProperty *fNumber() const;
    CameraProperty *shutterSpeed() const;
    CameraProperty *iso() const;
    CameraProperty *exposureBias() const;
    CameraProperty *whiteBalance() const;
    CameraProperty *colorTemp() const;
    CameraProperty *focusMode() const;
    CameraProperty *focusArea() const;
    CameraProperty *subjectRecognition() const;
    CameraProperty *pictureProfile() const;
    CameraProperty *creativeLook() const;
    CameraProperty *cinematicVlog() const;
    CameraProperty *frameRate() const;
    CameraProperty *liveViewQuality() const;
    CameraProperty *saveDestination() const;
    CameraProperty *audioRecording() const;
    CameraProperty *audioLevel() const;
    CameraProperty *movieFileFormat() const;
    CameraProperty *recordingSetting() const;
    CameraProperty *propertyByCode(quint32 code) const { return m_properties.value(code); }
    CameraBackend *backend() const { return m_backend; }
    UsbSetup *usb() const { return m_usb; }

    Q_INVOKABLE void discover(bool quiet = false);
    // Searches and connects to the camera used last time; plain discovery when there is none.
    Q_INVOKABLE void reconnectLast();
    Q_INVOKABLE void connectTo(int index);
    Q_INVOKABLE void connectToAddress(const QString &ip, int modelIndex);
    Q_INVOKABLE void submitCredentials(const QString &user, const QString &password, bool remember = false);
    Q_INVOKABLE void connectDevice(const QString &key);
    Q_INVOKABLE void forgetDevice(const QString &key);
    Q_INVOKABLE void cancelCredentials();
    Q_INVOKABLE void disconnectCamera();

    Q_INVOKABLE void startRecording();
    Q_INVOKABLE void stopRecording();
    Q_INVOKABLE void toggleRecording();
    Q_INVOKABLE void capturePhoto();
    Q_INVOKABLE void touchFocus(double nx, double ny);
    Q_INVOKABLE void cancelTouchFocus();
    Q_INVOKABLE void focusStep(int step);
    Q_INVOKABLE void zoom(int direction);
    Q_INVOKABLE void dismissRecordingError();
    Q_INVOKABLE void dismissLowBattery();
    Q_INVOKABLE void acceptTerms() { setTermsAccepted(true); }
    // The UI language changed: re-emits every text this object exposes.
    void retranslate();

signals:
    void stateChanged();
    void camerasChanged();
    void statusMessageChanged();
    void credentialsRequested();
    void recordingChanged();
    void recordingSecondsChanged();
    void statusChanged();
    void lastDownloadChanged();
    void downloadDirectoryChanged();
    void frameSizeChanged();
    void frameReady();
    void photoSaved(const QString &path);
    void lastCameraChanged();
    void devicesChanged();
    void mirrorPreviewChanged();
    void recordingErrorChanged();
    void termsAcceptedChanged();
    void liveViewFpsChanged();
    void lowBatteryChanged();
    void reconnectingChanged();
    void sidePanelWidthChanged();

private:
    void setState(State state);
    void setStatusMessage(const QString &message);
    void onPropertiesUpdated(const PropertyList &properties);
    void onConnected(const QString &modelName);
    void onDisconnected(const QString &reason);
    void onFrame(const QImage &frame);
    void updateRecordingClock(bool wasRecording);
    void checkRecordingOutcome(qint64 previousState);
    void setRecordingError(const QString &message);
    QString recordingFailureHint() const;
    void beginConnect(const CameraDescriptor &camera, std::function<void(const Credentials &)> connect);
    void finishAutoConnect(const QString &message);
    void recordConnection();
    void updateLowBattery();
    void setReconnecting(bool reconnecting);
    void saveHistory();
    void onCredentialsRequired(const QString &fingerprint);
    void onSavedLoginLoaded(const QString &key, bool found, const QString &user, const QString &password);
    void onAuthenticationFailed(const QString &reason);
    CameraProperty *addProperty(quint32 code);
    qint64 statusValue(quint32 code, qint64 fallback) const;
    bool isWritable(quint32 code) const;
    quint32 dataTypeOf(quint32 code, quint32 fallback) const;

    CameraBackend *m_backend;
    UsbSetup *m_usb;
    QHash<quint32, CameraProperty *> m_properties;
    QHash<quint32, PropertyInfo> m_status; // read-only/status properties not shown as settings
    QList<CameraDescriptor> m_cameras;
    State m_state = Idle;
    QString m_modelName;
    QString m_statusMessage;
    QString m_pendingFingerprint;
    std::function<void(const Credentials &)> m_pendingConnect;
    QString m_lastDownload;
    QString m_downloadDirectory;
    QImage m_frame;
    int m_frameCount = 0;
    int m_recordingSeconds = 0;
    QElapsedTimer m_recordingClock;
    QTimer m_recordingTicker;
    bool m_destinationAdjusted = false;

    enum class RecordRequest { None, Start, Stop };
    RecordRequest m_recordRequest = RecordRequest::None;
    QTimer m_recordConfirmTimer;
    QString m_recordingError;

    QSettings *m_settings;
    CameraDescriptor m_connecting;   // camera of the connection in progress
    DeviceHistory m_history;         // cameras connected before
    bool m_autoConnecting = false;
    CredentialStore *m_credentialStore;
    QString m_credentialsError;
    bool m_savedLoginTried = false;  // saved login used at most once per connection attempt
    bool m_usingSavedLogin = false;
    QString m_rememberUser;          // typed login to save once the camera accepts it
    QString m_rememberPassword;
    QString m_deferredDevice;        // connectDevice() while a discovery is running
    QTimer m_refreshTimer;
    int m_cardCapacityGb = 0;
    bool m_lowBattery = false;
    bool m_lowBatteryNotified = false; // once per drop below the threshold
    bool m_reconnecting = false;
    QTimer m_lostTimer;                 // gives up a connection that does not come back
    int m_staleDisconnects = 0;         // backend confirmations of sessions already closed here
    QTimer m_autoConnectTimer;
    QTimer m_connectHintTimer; // tells the user to look at the camera while it waits
    bool m_mirrorPreview = false;
    bool m_termsAccepted = false;
    int m_sidePanelWidth = 340;
    int m_liveViewFps = 0;
    int m_framesThisSecond = 0;
    QTimer m_fpsTimer;
};

} // namespace kl
