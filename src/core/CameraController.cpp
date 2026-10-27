#include "core/CameraController.h"

#include "core/PropertyCodec.h"

#include "CrDefines.h"
#include "CrDeviceProperty.h"

#include <QDebug>
#include <QDir>
#include <QStandardPaths>
#include <QVariantMap>
#include <algorithm>
#include <utility>

namespace SDK = SCRSDK;

namespace kl {

namespace {
const QString kMirror = QStringLiteral("preview/mirrored");
const QString kSidePanelWidth = QStringLiteral("window/sidePanelWidth");
const QString kCardCapacity = QStringLiteral("media/cardCapacityGb");
const QString kTermsAccepted = QStringLiteral("legal/acceptedTermsVersion");
constexpr int kMinSidePanel = 280;
constexpr int kMaxSidePanel = 560;
} // namespace

CameraController::CameraController(CameraBackend *backend, QSettings *settings, CredentialStore *credentials,
                                   QObject *parent)
    : QObject(parent)
    , m_backend(backend)
    , m_usb(new UsbSetup(this))
    , m_settings(settings)
    , m_credentialStore(credentials)
{
    if (m_credentialStore) {
        m_credentialStore->setParent(this);
        connect(m_credentialStore, &CredentialStore::loaded, this, &CameraController::onSavedLoginLoaded);
    }
    registerMetaTypes();
    m_backend->setParent(this);
    connect(m_usb, &UsbSetup::finished, this, [this](bool, const QString &message) { setStatusMessage(message); });

    for (quint32 code : {SDK::CrDeviceProperty_ExposureProgramMode, SDK::CrDeviceProperty_FNumber,
                         SDK::CrDeviceProperty_ShutterSpeed, SDK::CrDeviceProperty_IsoSensitivity,
                         SDK::CrDeviceProperty_ExposureBiasCompensation, SDK::CrDeviceProperty_WhiteBalance,
                         SDK::CrDeviceProperty_Colortemp, SDK::CrDeviceProperty_FocusMode,
                         SDK::CrDeviceProperty_FocusArea, SDK::CrDeviceProperty_SubjectRecognitionAF,
                         SDK::CrDeviceProperty_PictureProfile, SDK::CrDeviceProperty_CreativeLook,
                         SDK::CrDeviceProperty_CinematicVlogSetting,
                         SDK::CrDeviceProperty_Movie_Recording_FrameRateSetting,
                         SDK::CrDeviceProperty_LiveView_Image_Quality,
                         SDK::CrDeviceProperty_StillImageStoreDestination, SDK::CrDeviceProperty_AudioRecording,
                         SDK::CrDeviceProperty_AudioInputMasterLevel, SDK::CrDeviceProperty_Movie_File_Format,
                         SDK::CrDeviceProperty_Movie_Recording_Setting})
        addProperty(code);

    if (m_settings) {
        m_settings->setParent(this);
        m_history.load(*m_settings);
        m_cardCapacityGb = m_settings->value(kCardCapacity, 0).toInt();
        m_mirrorPreview = m_settings->value(kMirror, false).toBool();
        m_termsAccepted = m_settings->value(kTermsAccepted, 0).toInt() >= kTermsVersion;
        m_sidePanelWidth = std::clamp(m_settings->value(kSidePanelWidth, m_sidePanelWidth).toInt(), kMinSidePanel,
                                      kMaxSidePanel);
    }

    connect(m_backend, &CameraBackend::discovered, this, [this](const QList<CameraDescriptor> &cameras) {
        m_cameras = cameras;
        bool historyChanged = false;
        for (const CameraDescriptor &c : cameras)
            historyChanged |= m_history.refreshAddress(c); // known camera got a new DHCP address
        if (historyChanged)
            saveHistory();
        emit camerasChanged();
        emit devicesChanged();
        if (m_state != Discovering)
            return; // a connection started meanwhile
        setState(Idle);
        setStatusMessage(cameras.isEmpty() ? tr("Nenhuma câmera encontrada")
                                           : tr("%n câmera(s) encontrada(s)", nullptr, int(cameras.size())));
        if (!m_deferredDevice.isEmpty()) {
            connectDevice(std::exchange(m_deferredDevice, QString()));
            return;
        }
        if (!m_autoConnecting)
            return;
        const DeviceHistory::Entry last = m_history.entries().first();
        for (int i = 0; i < m_cameras.size(); ++i) {
            if (m_history.find(m_cameras[i]) == 0) {
                connectTo(i);
                return;
            }
        }
        // Not announced on the network: a camera on the router can still answer on its old IP.
        const int modelIndex = supportedModels().indexOf(last.model);
        if (last.connection != QStringLiteral("USB") && modelIndex >= 0 && codec::parseIPv4(last.address, nullptr))
            connectToAddress(last.address, modelIndex);
        else
            finishAutoConnect(tr("Última câmera (%1) não encontrada").arg(last.model));
    });
    connect(m_backend, &CameraBackend::discoveryFailed, this, [this](const QString &reason) {
        setState(Idle);
        setStatusMessage(reason);
        if (m_autoConnecting)
            finishAutoConnect(reason);
    });
    connect(m_backend, &CameraBackend::credentialsRequired, this,
            [this](const QString &, const QString &fp) { onCredentialsRequired(fp); });
    connect(m_backend, &CameraBackend::authenticationFailed, this, &CameraController::onAuthenticationFailed);
    connect(m_backend, &CameraBackend::connected, this, &CameraController::onConnected);
    connect(m_backend, &CameraBackend::connectionFailed, this, [this](const QString &reason) {
        m_rememberPassword.clear();
        setState(Idle);
        setStatusMessage(tr("Falha ao conectar: %1").arg(reason));
        if (m_autoConnecting)
            finishAutoConnect(tr("Não foi possível reconectar a %1: %2").arg(lastCamera(), reason));
    });
    connect(m_backend, &CameraBackend::disconnected, this, [this](const QString &reason) {
        if (m_staleDisconnects > 0) {
            --m_staleDisconnects; // that session was already closed by disconnectCamera()
            return;
        }
        onDisconnected(reason);
    });
    connect(m_backend, &CameraBackend::reconnecting, this, [this] {
        if (m_state != Connected)
            return;
        setStatusMessage(tr("Conexão instável, reconectando…"));
        setReconnecting(true);
    });
    connect(m_backend, &CameraBackend::reconnected, this, [this] { setReconnecting(false); });

    // A connection that does not come back closes the session and returns to the camera list.
    m_lostTimer.setSingleShot(true);
    m_lostTimer.setInterval(10000);
    connect(&m_lostTimer, &QTimer::timeout, this, [this] {
        if (m_state != Connected || !m_reconnecting)
            return;
        qInfo() << "connection lost, giving up";
        m_backend->setLiveViewEnabled(false);
        m_backend->disconnectCamera();
        ++m_staleDisconnects;
        onDisconnected(tr("a conexão com a câmera foi perdida"));
    });
    connect(m_backend, &CameraBackend::propertiesUpdated, this, &CameraController::onPropertiesUpdated);
    connect(m_backend, &CameraBackend::liveViewFrame, this, &CameraController::onFrame);
    connect(m_backend, &CameraBackend::downloadCompleted, this, [this](const QString &path) {
        m_lastDownload = path;
        emit lastDownloadChanged();
        emit photoSaved(path);
        setStatusMessage(tr("Foto salva: %1").arg(path));
    });
    connect(m_backend, &CameraBackend::warning, this, &CameraController::setStatusMessage);

    m_autoConnectTimer.setSingleShot(true);
    m_autoConnectTimer.setInterval(15000);
    connect(&m_autoConnectTimer, &QTimer::timeout, this, [this] {
        if (!m_autoConnecting)
            return;
        if (m_state == Connecting)
            m_backend->disconnectCamera();
        m_pendingConnect = nullptr;
        setState(Idle);
        finishAutoConnect(tr("%1 não respondeu. Verifique se a câmera está ligada e no modo remoto.")
                              .arg(lastCamera()));
    });

    m_refreshTimer.setInterval(20000);
    connect(&m_refreshTimer, &QTimer::timeout, this, [this] {
        if (m_state == Idle)
            discover(/*quiet=*/true);
    });

    m_fpsTimer.setInterval(1000);
    connect(&m_fpsTimer, &QTimer::timeout, this, [this] {
        const int fps = std::exchange(m_framesThisSecond, 0);
        if (fps != m_liveViewFps) {
            m_liveViewFps = fps;
            emit liveViewFpsChanged();
        }
    });
    m_fpsTimer.start();

    m_connectHintTimer.setSingleShot(true);
    m_connectHintTimer.setInterval(8000);
    connect(&m_connectHintTimer, &QTimer::timeout, this, [this] {
        if (m_state != Connecting)
            return;
        setStatusMessage(QStringLiteral(
            "Aguardando a câmera… Se ela perguntar “Permitir emparelhamento?”, confirme com OK. "
            "Se nada aparecer, ative o Emparelhamento na câmera (Rede › Conex./Disp. remoto › "
            "Função de disparo remoto › Emparelhamento) e tente de novo."));
    });

    m_recordConfirmTimer.setSingleShot(true);
    m_recordConfirmTimer.setInterval(5000);
    connect(&m_recordConfirmTimer, &QTimer::timeout, this, [this] {
        const RecordRequest request = std::exchange(m_recordRequest, RecordRequest::None);
        if (m_state != Connected)
            return;
        if (request == RecordRequest::Start && !recording())
            setRecordingError(tr("A câmera não iniciou a gravação. %1").arg(recordingFailureHint()));
        else if (request == RecordRequest::Stop && recording())
            setRecordingError(tr("A câmera não parou a gravação. Pare pelo botão da câmera."));
    });

    m_recordingTicker.setInterval(250);
    connect(&m_recordingTicker, &QTimer::timeout, this, [this] {
        const int secs = int(m_recordingClock.elapsed() / 1000);
        if (secs != m_recordingSeconds) {
            m_recordingSeconds = secs;
            emit recordingSecondsChanged();
        }
    });

    setDownloadDirectory(QDir(QStandardPaths::writableLocation(QStandardPaths::PicturesLocation))
                             .filePath(QStringLiteral("KamLynk")));
}

CameraController::~CameraController()
{
    m_backend->disconnectCamera();
}

CameraProperty *CameraController::addProperty(quint32 code)
{
    auto *p = new CameraProperty(code, [this](quint32 c, qint64 v, quint32 t) { m_backend->setProperty(c, v, t); }, this);
    m_properties.insert(code, p);
    return p;
}

CameraProperty *CameraController::exposureMode() const { return m_properties.value(SDK::CrDeviceProperty_ExposureProgramMode); }
CameraProperty *CameraController::fNumber() const { return m_properties.value(SDK::CrDeviceProperty_FNumber); }
CameraProperty *CameraController::shutterSpeed() const { return m_properties.value(SDK::CrDeviceProperty_ShutterSpeed); }
CameraProperty *CameraController::iso() const { return m_properties.value(SDK::CrDeviceProperty_IsoSensitivity); }
CameraProperty *CameraController::exposureBias() const { return m_properties.value(SDK::CrDeviceProperty_ExposureBiasCompensation); }
CameraProperty *CameraController::whiteBalance() const { return m_properties.value(SDK::CrDeviceProperty_WhiteBalance); }
CameraProperty *CameraController::colorTemp() const { return m_properties.value(SDK::CrDeviceProperty_Colortemp); }
CameraProperty *CameraController::focusMode() const { return m_properties.value(SDK::CrDeviceProperty_FocusMode); }
CameraProperty *CameraController::focusArea() const { return m_properties.value(SDK::CrDeviceProperty_FocusArea); }
CameraProperty *CameraController::subjectRecognition() const { return m_properties.value(SDK::CrDeviceProperty_SubjectRecognitionAF); }
CameraProperty *CameraController::pictureProfile() const { return m_properties.value(SDK::CrDeviceProperty_PictureProfile); }
CameraProperty *CameraController::creativeLook() const { return m_properties.value(SDK::CrDeviceProperty_CreativeLook); }
CameraProperty *CameraController::cinematicVlog() const { return m_properties.value(SDK::CrDeviceProperty_CinematicVlogSetting); }
CameraProperty *CameraController::frameRate() const { return m_properties.value(SDK::CrDeviceProperty_Movie_Recording_FrameRateSetting); }
CameraProperty *CameraController::liveViewQuality() const { return m_properties.value(SDK::CrDeviceProperty_LiveView_Image_Quality); }
CameraProperty *CameraController::saveDestination() const { return m_properties.value(SDK::CrDeviceProperty_StillImageStoreDestination); }
CameraProperty *CameraController::audioRecording() const { return m_properties.value(SDK::CrDeviceProperty_AudioRecording); }
CameraProperty *CameraController::audioLevel() const { return m_properties.value(SDK::CrDeviceProperty_AudioInputMasterLevel); }
CameraProperty *CameraController::movieFileFormat() const { return m_properties.value(SDK::CrDeviceProperty_Movie_File_Format); }
CameraProperty *CameraController::recordingSetting() const { return m_properties.value(SDK::CrDeviceProperty_Movie_Recording_Setting); }

QVariantList CameraController::cameras() const
{
    QVariantList out;
    for (const CameraDescriptor &c : m_cameras) {
        out.append(QVariantMap{{QStringLiteral("model"), c.model},
                               {QStringLiteral("connection"), c.connection},
                               {QStringLiteral("address"), c.address},
                               {QStringLiteral("needsSsh"), c.needsSsh}});
    }
    return out;
}

QStringList CameraController::supportedModels()
{
    // Same order as SCRSDK::CrCameraDeviceModelList.
    return {QStringLiteral("ILCE-7RM4"), QStringLiteral("ILCE-9M2"),  QStringLiteral("ILCE-7C"),
            QStringLiteral("ILCE-7SM3"), QStringLiteral("ILCE-1"),    QStringLiteral("ILCE-7RM4A"),
            QStringLiteral("DSC-RX0M2"), QStringLiteral("ILCE-7M4"),  QStringLiteral("ILME-FX3"),
            QStringLiteral("ILME-FX30"), QStringLiteral("ILME-FX6"),  QStringLiteral("ILCE-7RM5"),
            QStringLiteral("ZV-E1"),     QStringLiteral("ILCE-6700"), QStringLiteral("ILCE-7CM2"),
            QStringLiteral("ILCE-7CR"),  QStringLiteral("ILX-LR1"),   QStringLiteral("MPC-2610"),
            QStringLiteral("ILCE-9M3"),  QStringLiteral("ZV-E10M2"),  QStringLiteral("PXW-Z200"),
            QStringLiteral("HXR-NX800"), QStringLiteral("ILCE-1M2"),  QStringLiteral("ILME-FX3A"),
            QStringLiteral("BRC-AM7"),   QStringLiteral("ILME-FR7"),  QStringLiteral("ILME-FX2"),
            QStringLiteral("DSC-RX1RM3"), QStringLiteral("ILCE-7M5"), QStringLiteral("PXW-Z300"),
            QStringLiteral("PXW-Z380"),  QStringLiteral("ILCE-7RM6")};
}

QString CameraController::lastCamera() const
{
    if (m_history.isEmpty())
        return {};
    const DeviceHistory::Entry &last = m_history.entries().first();
    return last.address.isEmpty() ? last.model : QStringLiteral("%1 (%2)").arg(last.model, last.address);
}

QVariantList CameraController::devices() const
{
    QVariantList out;
    QList<bool> discoveredKnown(m_cameras.size(), false);
    const QDateTime now = QDateTime::currentDateTime();
    const auto &entries = m_history.entries();
    for (int i = 0; i < entries.size(); ++i) {
        const DeviceHistory::Entry &e = entries[i];
        bool online = false;
        for (int c = 0; c < m_cameras.size(); ++c) {
            if (m_history.find(m_cameras[c]) == i) {
                online = true;
                discoveredKnown[c] = true;
            }
        }
        const bool byIp = e.connection != QStringLiteral("USB") && codec::parseIPv4(e.address, nullptr);
        out.append(QVariantMap{{QStringLiteral("key"), e.key()},
                               {QStringLiteral("model"), e.model},
                               {QStringLiteral("connection"), e.connection},
                               {QStringLiteral("address"), e.address},
                               {QStringLiteral("known"), true},
                               {QStringLiteral("online"), online},
                               {QStringLiteral("savedLogin"), e.savedLogin},
                               {QStringLiteral("lastUsed"), codec::formatRelativeTime(e.lastUsed, now)},
                               {QStringLiteral("canConnect"), online || byIp}});
    }
    for (int c = 0; c < m_cameras.size(); ++c) {
        if (discoveredKnown[c])
            continue;
        const CameraDescriptor &d = m_cameras[c];
        out.append(QVariantMap{{QStringLiteral("key"), QStringLiteral("found:%1").arg(c)},
                               {QStringLiteral("model"), d.model},
                               {QStringLiteral("connection"), d.connection},
                               {QStringLiteral("address"), d.address},
                               {QStringLiteral("known"), false},
                               {QStringLiteral("online"), true},
                               {QStringLiteral("savedLogin"), false},
                               {QStringLiteral("lastUsed"), QString()},
                               {QStringLiteral("canConnect"), true}});
    }
    return out;
}

void CameraController::connectDevice(const QString &key)
{
    if (m_state == Connecting || m_state == Connected)
        return;
    if (m_state == Discovering) {
        // Discovery is replacing the camera list; connect when it is done.
        m_deferredDevice = key;
        setStatusMessage(tr("Conectando assim que a busca terminar…"));
        return;
    }
    if (key.startsWith(QStringLiteral("found:"))) {
        connectTo(key.mid(6).toInt());
        return;
    }
    const int index = m_history.findKey(key);
    if (index < 0)
        return;
    for (int c = 0; c < m_cameras.size(); ++c) {
        if (m_history.find(m_cameras[c]) == index) {
            connectTo(c);
            return;
        }
    }
    // Offline in the last search: try its last address directly (Wi-Fi on a router).
    const DeviceHistory::Entry e = m_history.entries()[index];
    const int modelIndex = supportedModels().indexOf(e.model);
    if (e.connection != QStringLiteral("USB") && modelIndex >= 0 && codec::parseIPv4(e.address, nullptr)) {
        connectToAddress(e.address, modelIndex);
        if (m_state == Connecting) // keep the identity so the history entry is updated, not duplicated
            m_connecting = CameraDescriptor{QString(), e.model, e.connection, e.address, e.uid};
    } else {
        setStatusMessage(tr("%1 está offline. Ligue a câmera e procure novamente.").arg(e.model));
    }
}

void CameraController::forgetDevice(const QString &key)
{
    const int index = m_history.findKey(key);
    if (index < 0)
        return;
    const DeviceHistory::Entry e = m_history.take(index);
    if (e.savedLogin && !e.fingerprint.isEmpty() && m_credentialStore)
        m_credentialStore->remove(e.fingerprint);
    saveHistory();
    emit lastCameraChanged();
    emit devicesChanged();
    setStatusMessage(tr("%1 removida da lista").arg(e.model));
}

void CameraController::setMirrorPreview(bool mirrored)
{
    if (m_mirrorPreview == mirrored)
        return;
    m_mirrorPreview = mirrored;
    if (m_settings)
        m_settings->setValue(kMirror, mirrored);
    emit mirrorPreviewChanged();
}

void CameraController::setSidePanelWidth(int width)
{
    width = std::clamp(width, kMinSidePanel, kMaxSidePanel);
    if (m_sidePanelWidth == width)
        return;
    m_sidePanelWidth = width;
    if (m_settings)
        m_settings->setValue(kSidePanelWidth, width);
    emit sidePanelWidthChanged();
}

void CameraController::saveHistory()
{
    if (m_settings)
        m_history.save(*m_settings);
}

void CameraController::recordConnection()
{
    if (m_connecting.model.isEmpty())
        return;
    DeviceHistory::Entry &entry = m_history.touch(m_connecting, QDateTime::currentDateTime());
    if (!m_pendingFingerprint.isEmpty())
        entry.fingerprint = m_pendingFingerprint;
    if (m_usingSavedLogin)
        entry.savedLogin = true;
    if (!m_rememberPassword.isEmpty() && m_credentialStore && !entry.fingerprint.isEmpty()) {
        m_credentialStore->save(entry.fingerprint, QStringLiteral("KamLynk: %1 (%2)").arg(entry.model, entry.address),
                                m_rememberUser, m_rememberPassword);
        entry.savedLogin = true;
    }
    m_rememberUser.clear();
    m_rememberPassword.clear();
    saveHistory();
    emit lastCameraChanged();
    emit devicesChanged();
}

void CameraController::onCredentialsRequired(const QString &fingerprint)
{
    m_pendingFingerprint = fingerprint;
    m_connectHintTimer.stop();
    finishAutoConnect({}); // the camera answered; no timeout while logging in
    if (!m_savedLoginTried && credentialStoreAvailable() && !fingerprint.isEmpty()) {
        m_savedLoginTried = true;
        setStatusMessage(tr("Procurando login salvo…"));
        m_credentialStore->load(fingerprint);
        return;
    }
    setStatusMessage(tr("A câmera pede usuário e senha (SSH)"));
    emit credentialsRequested();
}

void CameraController::onSavedLoginLoaded(const QString &key, bool found, const QString &user,
                                          const QString &password)
{
    if (m_state != Connecting || key != m_pendingFingerprint || !m_pendingConnect)
        return;
    if (!found || password.isEmpty()) {
        setStatusMessage(tr("A câmera pede usuário e senha (SSH)"));
        emit credentialsRequested();
        return;
    }
    m_usingSavedLogin = true;
    setStatusMessage(tr("Entrando com o login salvo…"));
    m_connectHintTimer.start();
    m_pendingConnect(Credentials{user, password, m_pendingFingerprint});
}

void CameraController::onAuthenticationFailed(const QString &reason)
{
    if (m_state != Connecting || !m_pendingConnect) {
        setState(Idle);
        setStatusMessage(tr("Falha ao conectar: %1").arg(reason));
        return;
    }
    m_connectHintTimer.stop();
    m_rememberPassword.clear();
    if (m_usingSavedLogin) {
        // Never retry a rejected saved password: repeated failures lock the camera.
        m_usingSavedLogin = false;
        if (m_credentialStore)
            m_credentialStore->remove(m_pendingFingerprint);
        for (int i = 0; i < m_history.entries().size(); ++i)
            if (m_history.entries()[i].fingerprint == m_pendingFingerprint)
                m_history.setSavedLogin(i, false);
        saveHistory();
        emit devicesChanged();
        m_credentialsError = tr("A câmera recusou o login salvo (a senha mudou?). Digite novamente.");
    } else {
        m_credentialsError = tr("Usuário ou senha incorretos. Atenção: várias tentativas erradas "
                                "bloqueiam a câmera até ela ser desligada e ligada.");
    }
    setStatusMessage(m_credentialsError);
    emit credentialsRequested();
}

int CameraController::defaultModelIndex()
{
    return SDK::CrCameraDeviceModel_ZV_E10M2;
}

void CameraController::setState(State state)
{
    if (m_state == state)
        return;
    if (state != Connecting)
        m_connectHintTimer.stop();
    if (state == Idle)
        m_refreshTimer.start();
    else
        m_refreshTimer.stop();
    qInfo() << "state:" << state;
    m_state = state;
    emit stateChanged();
}

void CameraController::setStatusMessage(const QString &message)
{
    if (m_statusMessage == message)
        return;
    qInfo().noquote() << "status:" << message;
    m_statusMessage = message;
    emit statusMessageChanged();
}

void CameraController::setDownloadDirectory(const QString &path)
{
    if (path.isEmpty() || path == m_downloadDirectory)
        return;
    QDir().mkpath(path);
    m_downloadDirectory = path;
    m_backend->setDownloadDirectory(path);
    emit downloadDirectoryChanged();
}

void CameraController::discover(bool quiet)
{
    if (m_state == Connecting || m_state == Connected || m_state == Discovering)
        return;
    setState(Discovering);
    if (!quiet)
        setStatusMessage(tr("Procurando câmeras…"));
    m_backend->discover();
}

void CameraController::reconnectLast()
{
    if (m_history.isEmpty() || m_state == Connecting || m_state == Connected) {
        discover();
        return;
    }
    m_autoConnecting = true;
    m_autoConnectTimer.start();
    discover();
    setStatusMessage(tr("Reconectando a %1…").arg(lastCamera()));
}

void CameraController::finishAutoConnect(const QString &message)
{
    m_autoConnecting = false;
    m_autoConnectTimer.stop();
    if (!message.isEmpty())
        setStatusMessage(message);
}

void CameraController::beginConnect(const CameraDescriptor &camera, std::function<void(const Credentials &)> connect)
{
    m_connecting = camera;
    m_pendingConnect = std::move(connect);
    m_pendingFingerprint.clear();
    m_credentialsError.clear();
    m_savedLoginTried = false;
    m_usingSavedLogin = false;
    m_rememberPassword.clear();
    setState(Connecting);
    setStatusMessage(tr("Conectando a %1…").arg(camera.address.isEmpty() ? camera.model : camera.address));
    m_connectHintTimer.start();
    m_pendingConnect(Credentials{});
}

void CameraController::connectTo(int index)
{
    if (index < 0 || index >= m_cameras.size() || m_state == Connected)
        return;
    const QString id = m_cameras[index].id;
    beginConnect(m_cameras[index], [this, id](const Credentials &c) { m_backend->connectCamera(id, c); });
}

void CameraController::connectToAddress(const QString &ip, int modelIndex)
{
    if (m_state == Connected)
        return;
    if (!codec::parseIPv4(ip, nullptr)) {
        setStatusMessage(tr("Endereço IP inválido"));
        return;
    }
    CameraDescriptor camera;
    camera.model = supportedModels().value(modelIndex);
    camera.connection = QStringLiteral("Wi-Fi/LAN");
    camera.address = ip;
    beginConnect(camera, [this, ip, modelIndex](const Credentials &c) { m_backend->connectByAddress(ip, modelIndex, c); });
}

void CameraController::submitCredentials(const QString &user, const QString &password, bool remember)
{
    if (!m_pendingConnect)
        return;
    m_usingSavedLogin = false;
    m_rememberUser = remember ? user : QString();
    m_rememberPassword = remember ? password : QString();
    setStatusMessage(tr("Autenticando…"));
    m_connectHintTimer.start();
    m_pendingConnect(Credentials{user, password, m_pendingFingerprint});
}

void CameraController::cancelCredentials()
{
    finishAutoConnect({});
    m_pendingConnect = nullptr;
    m_pendingFingerprint.clear();
    m_rememberPassword.clear();
    setState(Idle);
    setStatusMessage(tr("Conexão cancelada"));
}

void CameraController::disconnectCamera()
{
    if (m_state != Connected && m_state != Connecting)
        return;
    m_backend->setLiveViewEnabled(false);
    m_backend->disconnectCamera();
    // Back to the camera list right away; the camera's own confirmation may never come
    // (camera switched off, Wi-Fi gone). If it does come, it must not close the next session.
    if (m_state == Connected)
        ++m_staleDisconnects;
    m_pendingConnect = nullptr;
    onDisconnected(QString());
}

void CameraController::onConnected(const QString &modelName)
{
    m_modelName = modelName;
    m_pendingConnect = nullptr;
    m_staleDisconnects = 0; // confirmations that never came no longer matter
    m_destinationAdjusted = false;
    finishAutoConnect({});
    recordConnection();
    setState(Connected);
    setStatusMessage(tr("Conectado: %1").arg(modelName));
    m_backend->setDownloadDirectory(m_downloadDirectory);
    m_backend->setLiveViewEnabled(true);
}

void CameraController::onDisconnected(const QString &reason)
{
    const bool wasRecording = recording();
    m_lostTimer.stop();
    setReconnecting(false);
    m_lowBattery = false;
    m_lowBatteryNotified = false;
    emit lowBatteryChanged();
    m_recordRequest = RecordRequest::None;
    m_recordConfirmTimer.stop();
    m_status.clear();
    for (CameraProperty *p : std::as_const(m_properties))
        p->reset();
    m_frame = QImage();
    emit frameSizeChanged();
    updateRecordingClock(wasRecording);
    emit statusChanged();
    setState(Idle);
    setStatusMessage(reason.isEmpty() ? tr("Desconectado")
                                      : tr("Desconectado: %1").arg(reason));
}

void CameraController::onPropertiesUpdated(const PropertyList &properties)
{
    if (m_state == Idle || m_state == Discovering)
        return; // late update from a session that was already closed
    const bool wasRecording = recording();
    const qint64 previousRecordState = statusValue(SDK::CrDeviceProperty_RecordingState, -1);
    for (const PropertyInfo &info : properties) {
        m_status.insert(info.code, info);
        if (CameraProperty *p = m_properties.value(info.code))
            p->update(info);
    }
    updateRecordingClock(wasRecording);
    checkRecordingOutcome(previousRecordState);
    updateLowBattery();
    emit statusChanged();

    // Photos are only transferred when the camera saves to the host as well.
    CameraProperty *dest = saveDestination();
    if (!m_destinationAdjusted && dest->available()) {
        m_destinationAdjusted = true;
        if (dest->writable() && dest->rawValue() == SDK::CrStillImageStoreDestination_MemoryCard
            && dest->candidates().contains(SDK::CrStillImageStoreDestination_HostPCAndMemoryCard))
            dest->setValue(SDK::CrStillImageStoreDestination_HostPCAndMemoryCard);
    }
}

void CameraController::onFrame(const QImage &frame)
{
    if (m_state != Connected || frame.isNull())
        return;
    if (m_reconnecting) // pictures are flowing again
        setReconnecting(false);
    const bool sizeChanged = frame.size() != m_frame.size();
    m_frame = frame;
    ++m_frameCount;
    ++m_framesThisSecond;
    if (sizeChanged)
        emit frameSizeChanged();
    emit frameReady();
}

void CameraController::updateRecordingClock(bool wasRecording)
{
    const bool now = recording();
    if (now == wasRecording)
        return;
    if (now) {
        m_recordingClock.start();
        m_recordingTicker.start();
    } else {
        m_recordingTicker.stop();
    }
    m_recordingSeconds = 0;
    emit recordingSecondsChanged();
    emit recordingChanged();
}

// The camera is the source of truth: the record button follows RecordingState, and any
// refusal or stop the app did not ask for is reported.
void CameraController::checkRecordingOutcome(qint64 previousState)
{
    const qint64 now = statusValue(SDK::CrDeviceProperty_RecordingState, -1);
    if (now == previousState)
        return;
    const RecordRequest request = m_recordRequest;
    if (now == SDK::CrMovie_Recording_State_Recording) {
        if (request == RecordRequest::Start) {
            m_recordRequest = RecordRequest::None;
            m_recordConfirmTimer.stop();
        }
        setRecordingError({});
    } else if (now == SDK::CrMovie_Recording_State_Recording_Failed) {
        m_recordRequest = RecordRequest::None;
        m_recordConfirmTimer.stop();
        setRecordingError(tr("Falha na gravação. %1").arg(recordingFailureHint()));
    } else if (now == SDK::CrMovie_Recording_State_Not_Recording
               && previousState == SDK::CrMovie_Recording_State_Recording) {
        m_recordConfirmTimer.stop();
        m_recordRequest = RecordRequest::None;
        if (request != RecordRequest::Stop)
            setRecordingError(tr("A gravação parou na câmera. %1").arg(recordingFailureHint()));
    }
}

QString CameraController::recordingFailureHint() const
{
    if (overheating())
        return tr("A câmera está superaquecida.");
    if (statusValue(SDK::CrDeviceProperty_MediaSLOT1_RemainingTime, -1) == 0)
        return tr("O cartão está cheio ou ausente.");
    return tr("Verifique o cartão e se a câmera está no modo de vídeo.");
}

void CameraController::setRecordingError(const QString &message)
{
    if (m_recordingError == message)
        return;
    m_recordingError = message;
    if (!message.isEmpty())
        qInfo().noquote() << "Recording:" << message;
    emit recordingErrorChanged();
}

void CameraController::dismissRecordingError()
{
    setRecordingError({});
}

qint64 CameraController::statusValue(quint32 code, qint64 fallback) const
{
    auto it = m_status.constFind(code);
    return it == m_status.constEnd() ? fallback : it->current;
}

bool CameraController::isWritable(quint32 code) const
{
    auto it = m_status.constFind(code);
    return it != m_status.constEnd() && it->writable;
}

quint32 CameraController::dataTypeOf(quint32 code, quint32 fallback) const
{
    auto it = m_status.constFind(code);
    return (it == m_status.constEnd() || it->dataType == 0) ? fallback : it->dataType;
}

bool CameraController::recording() const
{
    return statusValue(SDK::CrDeviceProperty_RecordingState, 0) == SDK::CrMovie_Recording_State_Recording;
}

QString CameraController::recordingTime() const
{
    return codec::formatDuration(m_recordingSeconds);
}

int CameraController::batteryPercent() const
{
    const qint64 v = statusValue(SDK::CrDeviceProperty_BatteryRemain, -1);
    return (v < 0 || v > 100) ? -1 : int(v);
}

QString CameraController::mediaRemaining() const
{
    const qint64 v = statusValue(SDK::CrDeviceProperty_MediaSLOT1_RemainingTime, -1);
    return v < 0 ? QStringLiteral("--") : codec::formatDuration(v);
}

bool CameraController::overheating() const
{
    return statusValue(SDK::CrDeviceProperty_DeviceOverheatingState, 0) != SDK::CrDeviceOverheatingState_NotOverheating;
}

QString CameraController::mediaStatus() const
{
    const qint64 status = statusValue(SDK::CrDeviceProperty_MediaSLOT1_Status, -1);
    return status < 0 ? QString() : codec::format(SDK::CrDeviceProperty_MediaSLOT1_Status, status);
}

bool CameraController::mediaOk() const
{
    const qint64 status = statusValue(SDK::CrDeviceProperty_MediaSLOT1_Status, -1);
    return status < 0 || status == SDK::CrSlotStatus_OK;
}

bool CameraController::colorTempMode() const
{
    return statusValue(SDK::CrDeviceProperty_WhiteBalance, -1) == SDK::CrWhiteBalance_ColorTemp;
}

bool CameraController::audioEnabled() const
{
    return statusValue(SDK::CrDeviceProperty_AudioRecording, -1) != SDK::CrAudioRecording_Off;
}

int CameraController::mediaPhotosLeft() const
{
    const qint64 v = statusValue(SDK::CrDeviceProperty_MediaSLOT1_RemainingNumber, -1);
    return v < 0 || v > 0x7FFFFFFF ? -1 : int(v);
}

double CameraController::mediaFreeGb() const
{
    const qint64 seconds = statusValue(SDK::CrDeviceProperty_MediaSLOT1_RemainingTime, -1);
    const int mbps = codec::recordingSettingMbps(statusValue(SDK::CrDeviceProperty_Movie_Recording_Setting, 0));
    if (seconds < 0 || mbps <= 0)
        return -1;
    return double(seconds) * mbps / 8.0 / 1000.0; // Mbit/s -> GB
}

double CameraController::mediaUsedFraction() const
{
    const double free = mediaFreeGb();
    if (free < 0 || m_cardCapacityGb <= 0)
        return -1;
    return std::clamp(1.0 - free / m_cardCapacityGb, 0.0, 1.0);
}

void CameraController::setCardCapacityGb(int gb)
{
    gb = std::max(gb, 0);
    if (gb == m_cardCapacityGb)
        return;
    m_cardCapacityGb = gb;
    if (m_settings)
        m_settings->setValue(kCardCapacity, gb);
    emit statusChanged();
}

void CameraController::updateLowBattery()
{
    const int level = batteryPercent();
    if (level < 0)
        return;
    if (level > kLowBatteryPercent) {
        m_lowBatteryNotified = false; // charged again: warn next time it drops
        if (m_lowBattery) {
            m_lowBattery = false;
            emit lowBatteryChanged();
        }
    } else if (!m_lowBatteryNotified) {
        m_lowBatteryNotified = true;
        m_lowBattery = true;
        qInfo() << "battery low:" << level << "%";
        emit lowBatteryChanged();
    }
}

void CameraController::dismissLowBattery()
{
    if (!m_lowBattery)
        return;
    m_lowBattery = false;
    emit lowBatteryChanged();
}

void CameraController::setTermsAccepted(bool accepted, bool remember)
{
    if (m_settings && remember) {
        if (accepted)
            m_settings->setValue(kTermsAccepted, kTermsVersion);
        else
            m_settings->remove(kTermsAccepted);
    }
    if (m_termsAccepted == accepted)
        return;
    m_termsAccepted = accepted;
    emit termsAcceptedChanged();
}

void CameraController::retranslate()
{
    for (CameraProperty *p : std::as_const(m_properties))
        p->retranslate();
    emit statusChanged();
    emit devicesChanged();
    if (m_state == Connected && !m_reconnecting)
        setStatusMessage(tr("Conectado: %1").arg(m_modelName));
}

void CameraController::setReconnecting(bool reconnecting)
{
    if (reconnecting)
        m_lostTimer.start();
    else
        m_lostTimer.stop();
    if (m_reconnecting == reconnecting)
        return;
    m_reconnecting = reconnecting;
    if (!reconnecting && m_state == Connected)
        setStatusMessage(tr("Conectado: %1").arg(m_modelName));
    emit reconnectingChanged();
}

bool CameraController::touchFocusAvailable() const
{
    return isWritable(SDK::CrDeviceProperty_RemoteTouchOperation)
        && statusValue(SDK::CrDeviceProperty_RemoteTouchOperationEnableStatus, 0) == SDK::CrRemoteTouchOperation_Enable;
}

bool CameraController::manualFocusAvailable() const
{
    return isWritable(SDK::CrDeviceProperty_NearFar);
}

bool CameraController::zoomAvailable() const
{
    return isWritable(SDK::CrDeviceProperty_Zoom_Operation)
        && statusValue(SDK::CrDeviceProperty_Zoom_Operation_Status, 0) == SDK::CrZoomOperationEnableStatus_Enable;
}

void CameraController::startRecording()
{
    if (m_state != Connected || recording())
        return;
    setRecordingError({});
    m_recordRequest = RecordRequest::Start;
    m_recordConfirmTimer.start();
    m_backend->sendCommand(CameraBackend::Command::MovieRecord, true);
}

void CameraController::stopRecording()
{
    if (m_state != Connected || !recording())
        return;
    m_recordRequest = RecordRequest::Stop;
    m_recordConfirmTimer.start();
    m_backend->sendCommand(CameraBackend::Command::MovieRecord, false);
}

void CameraController::toggleRecording()
{
    recording() ? stopRecording() : startRecording();
}

void CameraController::capturePhoto()
{
    if (m_state != Connected)
        return;
    m_backend->sendCommand(CameraBackend::Command::Release, true);
    QTimer::singleShot(100, this, [this] { m_backend->sendCommand(CameraBackend::Command::Release, false); });
}

void CameraController::touchFocus(double nx, double ny)
{
    if (m_state != Connected || !touchFocusAvailable())
        return;
    m_backend->setProperty(SDK::CrDeviceProperty_RemoteTouchOperation, codec::packTouchPosition(nx, ny),
                           dataTypeOf(SDK::CrDeviceProperty_RemoteTouchOperation, SDK::CrDataType_UInt32));
}

void CameraController::cancelTouchFocus()
{
    if (m_state != Connected)
        return;
    m_backend->sendCommand(CameraBackend::Command::CancelRemoteTouch, true);
    m_backend->sendCommand(CameraBackend::Command::CancelRemoteTouch, false);
}

void CameraController::focusStep(int step)
{
    if (m_state != Connected || step == 0 || !manualFocusAvailable())
        return;
    const PropertyInfo info = m_status.value(SDK::CrDeviceProperty_NearFar);
    const qint64 lo = info.isRange ? info.rangeMin : -7;
    const qint64 hi = info.isRange ? info.rangeMax : 7;
    m_backend->setProperty(SDK::CrDeviceProperty_NearFar, std::clamp<qint64>(step, lo, hi),
                           dataTypeOf(SDK::CrDeviceProperty_NearFar, SDK::CrDataType_Int16));
}

void CameraController::zoom(int direction)
{
    if (m_state != Connected || !zoomAvailable())
        return;
    const PropertyInfo info = m_status.value(SDK::CrDeviceProperty_Zoom_Operation);
    const qint64 lo = info.isRange ? info.rangeMin : -1;
    const qint64 hi = info.isRange ? info.rangeMax : 1;
    m_backend->setProperty(SDK::CrDeviceProperty_Zoom_Operation, std::clamp<qint64>(direction, lo, hi),
                           dataTypeOf(SDK::CrDeviceProperty_Zoom_Operation, SDK::CrDataType_Int8));
}

} // namespace kl
