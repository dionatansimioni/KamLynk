#include "sdk/SonyBackend.h"

#include "core/PropertyCodec.h"

#include "CameraRemote_SDK.h"
#include "IDeviceCallback.h"

#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QImage>
#include <QCoreApplication>

#include <arpa/inet.h>
#include <dlfcn.h>
#include <cerrno>
#include <chrono>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

namespace SDK = SCRSDK;

namespace kl {

// Receives SDK events on SDK-owned threads and forwards them to the backend.
class SdkCallback : public SDK::IDeviceCallback
{
public:
    explicit SdkCallback(SonyBackend *backend) : m_backend(backend) {}

    void OnConnected(SDK::DeviceConnectionVersioin version) override
    {
        qInfo() << "SDK OnConnected, version" << Qt::hex << quint32(version);
        m_backend->onSdkConnected();
    }
    void OnDisconnected(CrInt32u error) override
    {
        qInfo() << "SDK OnDisconnected" << Qt::hex << error;
        m_backend->onSdkDisconnected(error);
    }
    void OnError(CrInt32u error) override
    {
        qWarning() << "SDK OnError" << Qt::hex << error;
        m_backend->onSdkError(error);
    }
    void OnWarningExt(CrInt32u warning, CrInt32 p1, CrInt32 p2, CrInt32 p3) override
    {
        qInfo() << "SDK OnWarningExt" << Qt::hex << warning << p1 << p2 << p3;
    }

    void OnPropertyChanged() override {}
    void OnPropertyChangedCodes(CrInt32u num, CrInt32u *codes) override
    {
        QVector<quint32> list;
        for (CrInt32u i = 0; i < num; ++i)
            list.append(codes[i]);
        m_backend->post([b = m_backend, list] { b->loadProperties(list); });
    }

    void OnCompleteDownload(CrChar *filename, CrInt32u) override
    {
        if (filename)
            emit m_backend->downloadCompleted(QString::fromLocal8Bit(filename));
    }

    void OnWarning(CrInt32u warning) override
    {
        qInfo() << "SDK OnWarning" << Qt::hex << warning;
        switch (warning) {
        case SDK::CrWarning_Connect_Reconnecting:
            emit m_backend->reconnecting();
            break;
        case SDK::CrWarning_Connect_Reconnected:
            emit m_backend->reconnected();
            m_backend->post([b = m_backend] { b->loadProperties({}); });
            break;
        case SDK::CrWarning_File_StorageFull:
            emit m_backend->warning(SonyBackend::tr("Cartão de memória cheio"));
            break;
        default:
            break;
        }
    }

private:
    SonyBackend *m_backend;
};

namespace {

// Non-blocking connect with a timeout; true when something accepts on ip:port.
bool tcpPortOpen(const QString &ip, quint16 port, int timeoutMs)
{
    const int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (fd < 0)
        return true; // cannot check: let the SDK try
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    bool open = false;
    if (::inet_pton(AF_INET, ip.toLatin1().constData(), &addr.sin_addr) == 1) {
        if (::connect(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == 0) {
            open = true;
        } else if (errno == EINPROGRESS) {
            pollfd pfd{fd, POLLOUT, 0};
            if (::poll(&pfd, 1, timeoutMs) == 1) {
                int error = 0;
                socklen_t len = sizeof(error);
                open = ::getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) == 0 && error == 0;
            }
        }
    }
    ::close(fd);
    return open;
}

// The SDK ships its own libssh2 (1.11.0, OpenSSL inside) in CrAdapter/ and dlopens it.
// Qt Network -> libproxy -> libcurl already loaded the system libssh2.so.1 into the global
// scope, so the SDK copy's internal calls (e.g. libssh2_keepalive_send) bound to the system
// one: GetFingerprint failed with 0x8218 or crashed. Loading the SDK copy first with
// RTLD_DEEPBIND makes it resolve its own symbols; the SDK's later dlopen of the same file
// reuses this instance.
void preloadSdkSsh()
{
    static bool done = false;
    if (done)
        return;
    done = true;
    const QByteArray path =
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("CrAdapter/libssh2.so")).toLocal8Bit();
    if (::dlopen(path.constData(), RTLD_NOW | RTLD_LOCAL | RTLD_DEEPBIND))
        qInfo() << "SDK libssh2 preloaded (deepbind):" << path;
    else
        qWarning() << "Could not preload SDK libssh2:" << ::dlerror();
}

QString sdkString(const CrInt16u *str)
{
    // SDK strings: first element is the length (including the terminating null), then UTF-16.
    if (!str || str[0] == 0)
        return {};
    QString s = QString::fromUtf16(reinterpret_cast<const char16_t *>(str + 1), str[0]);
    while (s.endsWith(QChar(0)))
        s.chop(1);
    return s;
}

PropertyInfo toInfo(const SDK::CrDeviceProperty &prop)
{
    PropertyInfo info;
    info.code = prop.GetCode();
    info.dataType = prop.GetValueType();
    info.readable = prop.IsGetEnableCurrentValue();
    info.writable = prop.IsSetEnableCurrentValue();
    if (codec::isString(info.dataType)) {
        info.currentText = sdkString(prop.GetCurrentStr());
    } else {
        info.current = codec::normalize(info.dataType, prop.GetCurrentValue());
        codec::applyValueTable(info, codec::decodeValues(info.dataType, prop.GetValues(), prop.GetValueSize()));
    }
    return info;
}

} // namespace

SonyBackend::SonyBackend(QObject *parent)
    : CameraBackend(parent)
    , m_callback(std::make_unique<SdkCallback>(this))
{
    m_worker = new QObject;
    m_worker->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    m_thread.setObjectName(QStringLiteral("kamlynk-sdk"));
    preloadSdkSsh();
    m_thread.start();
    post([this] {
        m_sdkReady = SDK::Init();
        if (!m_sdkReady)
            emit discoveryFailed(tr("Não foi possível iniciar o SDK da Sony"));
        else
            qInfo() << "Sony Camera Remote SDK" << Qt::hex << SDK::GetSDKVersion();
    });
}

SonyBackend::~SonyBackend()
{
    QMetaObject::invokeMethod(m_worker, [this] {
        stopLiveViewThread();
        if (m_handle && m_connected) {
            m_userDisconnect = true;
            SDK::Disconnect(m_handle);
        }
        releaseDevice();
        releaseCameraInfos();
        if (m_sdkReady)
            SDK::Release();
        m_sdkReady = false;
    }, Qt::BlockingQueuedConnection);
    m_thread.quit();
    m_thread.wait();
}

bool SonyBackend::selfCheck(QString *report)
{
    preloadSdkSsh();
    QStringList lines;
    const bool ready = SDK::Init();
    lines << QStringLiteral("init: %1").arg(ready ? QStringLiteral("ok") : QStringLiteral("FAILED"));
    bool ok = ready;
    if (ready) {
        lines << QStringLiteral("version: %1").arg(SDK::GetSDKVersion(), 0, 16);
        SDK::ICrEnumCameraObjectInfo *list = nullptr;
        const SDK::CrError err = SDK::EnumCameraObjects(&list, 1);
        const CrInt32u count = list ? list->GetCount() : 0;
        lines << QStringLiteral("enumerate: 0x%1, cameras: %2").arg(err, 0, 16).arg(count);
        for (CrInt32u i = 0; i < count; ++i)
            lines << QStringLiteral("  - %1").arg(QString::fromLocal8Bit(list->GetCameraObjectInfo(i)->GetModel()));
        if (list)
            list->Release();
        // Adaptor_Create means the CrAdapter/ plugins were not found next to the executable.
        if (err == SDK::CrError_Adaptor_Create || err == SDK::CrError_Init) {
            ok = false;
            lines << QStringLiteral("FAILED: %1").arg(codec::describeError(err));
        }
        SDK::Release();
    }
    if (report)
        *report = lines.join(QLatin1Char('\n'));
    return ok;
}

int SonyBackend::sshCheck(const QString &ip, QString *report)
{
    preloadSdkSsh();
    QStringList lines;
    int result = 1;
    quint32 packed = 0;
    if (!codec::parseIPv4(ip, &packed)) {
        lines << QStringLiteral("invalid IP: %1").arg(ip);
    } else if (!tcpPortOpen(ip, 22, 3000)) {
        lines << QStringLiteral("port 22 closed on %1").arg(ip);
        result = 2;
    } else if (!SDK::Init()) {
        lines << QStringLiteral("init: FAILED");
    } else {
        SDK::ICrCameraObjectInfo *info = nullptr;
        CrInt8u mac[6] = {0};
        SDK::CrError err = SDK::CreateCameraObjectInfoEthernetConnection(
            &info, SDK::CrCameraDeviceModel_ZV_E10M2, packed, mac, 1);
        if (CR_SUCCEEDED(err) && info) {
            char fp[128] = {0};
            CrInt32u len = 0;
            err = SDK::GetFingerprint(info, fp, &len);
            lines << QStringLiteral("fingerprint: 0x%1 %2").arg(err, 0, 16)
                         .arg(QString::fromLatin1(fp, int(std::min<CrInt32u>(len, sizeof(fp)))));
            if (CR_SUCCEEDED(err) && len > 0)
                result = 0;
            info->Release();
        } else {
            lines << QStringLiteral("create: FAILED 0x%1").arg(err, 0, 16);
        }
        SDK::Release();
    }
    if (report)
        *report = lines.join(QLatin1Char('\n'));
    return result;
}

void SonyBackend::post(std::function<void()> fn)
{
    QMetaObject::invokeMethod(m_worker, std::move(fn), Qt::QueuedConnection);
}

void SonyBackend::releaseCameraInfos()
{
    if (m_enum) {
        m_enum->Release();
        m_enum = nullptr;
    }
    if (m_manualInfo) {
        m_manualInfo->Release();
        m_manualInfo = nullptr;
    }
}

void SonyBackend::discover()
{
    post([this] {
        if (!m_sdkReady) {
            emit discoveryFailed(tr("SDK da Sony indisponível"));
            return;
        }
        if (m_connected) {
            emit discoveryFailed(tr("Desconecte antes de procurar"));
            return;
        }
        releaseCameraInfos();
        SDK::ICrEnumCameraObjectInfo *list = nullptr;
        const SDK::CrError err = SDK::EnumCameraObjects(&list, 3);
        QList<CameraDescriptor> cameras;
        if (CR_SUCCEEDED(err) && list) {
            m_enum = list;
            for (CrInt32u i = 0; i < list->GetCount(); ++i) {
                const SDK::ICrCameraObjectInfo *info = list->GetCameraObjectInfo(i);
                CameraDescriptor d;
                d.id = QString::number(i);
                d.model = QString::fromLocal8Bit(info->GetModel());
                const QString type = QString::fromLocal8Bit(info->GetConnectionTypeName());
                if (type == QStringLiteral("IP")) {
                    d.connection = QStringLiteral("Wi-Fi/LAN");
                    d.address = QString::fromLocal8Bit(info->GetIPAddressChar());
                    d.uid = QString::fromLocal8Bit(info->GetMACAddressChar());
                } else {
                    d.connection = QStringLiteral("USB");
                    d.address = QString::fromLocal8Bit(reinterpret_cast<const char *>(info->GetId()), int(info->GetIdSize()));
                    d.uid = d.address;
                }
                d.needsSsh = info->GetSSHsupport() == SDK::CrSSHsupport_ON;
                qInfo().noquote() << "found:" << d.model << d.connection << d.address << "ssh=" << d.needsSsh
                                  << "pairing=" << QString::fromLocal8Bit(info->GetPairingNecessity())
                                  << "status=" << info->GetConnectionStatus();
                cameras.append(d);
            }
        } else if (err != SDK::CrError_None && err != SDK::CrError_Adaptor_EnumDevice) {
            qWarning() << "EnumCameraObjects failed" << Qt::hex << err;
        }
        qInfo() << "EnumCameraObjects:" << Qt::hex << err << Qt::dec << cameras.size() << "camera(s)";
        emit discovered(cameras);
    });
}

void SonyBackend::connectCamera(const QString &id, const Credentials &credentials)
{
    post([this, id, credentials] {
        bool ok = false;
        const int index = id.toInt(&ok);
        if (!m_enum || !ok || index < 0 || CrInt32u(index) >= m_enum->GetCount()) {
            emit connectionFailed(tr("Câmera não encontrada. Procure novamente."));
            return;
        }
        auto *info = const_cast<SDK::ICrCameraObjectInfo *>(m_enum->GetCameraObjectInfo(CrInt32u(index)));
        doConnect(info, credentials, id);
    });
}

void SonyBackend::connectByAddress(const QString &ip, int modelIndex, const Credentials &credentials)
{
    post([this, ip, modelIndex, credentials] {
        quint32 packed = 0;
        if (!m_sdkReady || !codec::parseIPv4(ip, &packed)) {
            emit connectionFailed(tr("Endereço IP inválido"));
            return;
        }
        const QString id = QStringLiteral("ip:") + ip;
        if (!credentials.password.isEmpty()) {
            // Second step of an SSH connection: the user typed the credentials.
            if (createManualInfo(ip, modelIndex, true))
                doConnect(m_manualInfo, credentials, id);
            return;
        }
        // Unknown whether "Access Authen." is on: ask for the SSH fingerprint first (the camera
        // default on a router) and fall back to a pairing connection when SSH does not answer.
        if (!createManualInfo(ip, modelIndex, true))
            return;
        QString fingerprint;
        if (CR_SUCCEEDED(fetchFingerprint(m_manualInfo, &fingerprint))) {
            emit credentialsRequired(id, fingerprint);
            return;
        }
        qInfo() << "manual IP: no SSH answer, trying a pairing connection";
        if (createManualInfo(ip, modelIndex, false))
            doConnect(m_manualInfo, credentials, id);
    });
}

bool SonyBackend::createManualInfo(const QString &ip, int modelIndex, bool ssh)
{
    if (m_manualInfo) {
        m_manualInfo->Release();
        m_manualInfo = nullptr;
    }
    quint32 packed = 0;
    codec::parseIPv4(ip, &packed);
    CrInt8u mac[6] = {0};
    const SDK::CrError err = SDK::CreateCameraObjectInfoEthernetConnection(
        &m_manualInfo, SDK::CrCameraDeviceModelList(modelIndex), packed, mac, ssh ? 1 : 0);
    qInfo() << "CreateCameraObjectInfoEthernetConnection ssh=" << ssh << Qt::hex << err;
    if (CR_FAILED(err) || !m_manualInfo) {
        emit connectionFailed(codec::describeError(err));
        return false;
    }
    return true;
}

quint32 SonyBackend::fetchFingerprint(SDK::ICrCameraObjectInfo *info, QString *fingerprint)
{
    // GetFingerprint blocks for the whole TCP timeout (~2 min) on an unreachable address.
    const QString ip = QString::fromLocal8Bit(info->GetIPAddressChar());
    if (!ip.isEmpty() && !tcpPortOpen(ip, 22, 3000)) {
        qInfo().noquote() << "SSH port 22 not reachable on" << ip;
        return SDK::CrError_Connect_SSH_GetFingerprintFailed;
    }
    SDK::CrError err = SDK::CrError_Connect_SSH_GetFingerprintFailed;
    for (int attempt = 1; attempt <= 3; ++attempt) {
        char fp[128] = {0};
        CrInt32u fpLen = 0;
        QElapsedTimer timer;
        timer.start();
        err = SDK::GetFingerprint(info, fp, &fpLen);
        qInfo() << "GetFingerprint attempt" << attempt << ":" << Qt::hex << err << Qt::dec << "length" << fpLen
                << timer.elapsed() << "ms";
        if (CR_SUCCEEDED(err) && fpLen > 0) {
            *fingerprint = QString::fromLatin1(fp, int(std::min<CrInt32u>(fpLen, sizeof(fp))));
            return SDK::CrError_None;
        }
        // Retry only quick refusals (the camera drops SSH sessions right after waking up).
        if (timer.elapsed() > 5000)
            break;
        if (attempt < 3)
            QThread::msleep(800);
    }
    return CR_FAILED(err) ? err : SDK::CrError_Connect_SSH_GetFingerprintFailed;
}

void SonyBackend::doConnect(SDK::ICrCameraObjectInfo *info, const Credentials &credentials, const QString &id)
{
    if (m_connected || m_handle) {
        emit connectionFailed(tr("Já existe uma conexão ativa"));
        return;
    }
    QByteArray fingerprint = credentials.fingerprint.toLatin1();
    const bool ssh = info->GetSSHsupport() == SDK::CrSSHsupport_ON;
    qInfo().noquote() << "connect:" << id << QString::fromLocal8Bit(info->GetModel()) << "ssh=" << ssh
                      << "credentials=" << !credentials.password.isEmpty();
    if (ssh && credentials.password.isEmpty()) {
        QString fp;
        const quint32 err = fetchFingerprint(info, &fp);
        if (CR_FAILED(err)) {
            emit connectionFailed(codec::describeError(err));
            return;
        }
        emit credentialsRequired(id, fp);
        return;
    }

    m_connectingModel = QString::fromLocal8Bit(info->GetModel());
    m_userDisconnect = false;
    const QByteArray user = (credentials.user.isEmpty() ? QStringLiteral("admin") : credentials.user).toUtf8();
    const QByteArray password = credentials.password.toUtf8();
    static const QString displayName = QStringLiteral("KamLynk");
    SDK::CrDeviceHandle handle = 0;
    const SDK::CrError err = SDK::Connect(info, m_callback.get(), &handle, SDK::CrSdkControlMode_Remote,
                                          SDK::CrReconnecting_ON, ssh ? user.constData() : nullptr,
                                          ssh ? password.constData() : nullptr,
                                          ssh ? fingerprint.constData() : nullptr,
                                          ssh ? CrInt32u(fingerprint.size()) : 0,
                                          reinterpret_cast<const CrInt16u *>(displayName.utf16()));
    qInfo() << "Connect:" << Qt::hex << err << "handle" << handle;
    if (CR_FAILED(err)) {
        reportConnectError(err);
        return;
    }
    m_handle = handle;
    // Success is reported asynchronously through OnConnected / OnError.
}

void SonyBackend::onSdkConnected()
{
    m_connected = true;
    post([this] {
        applySaveInfo();
        loadProperties({});
        // The model reported by the camera can be more precise than the enum entry.
        emit connected(m_connectingModel);
        if (m_liveViewWanted)
            startLiveViewThread();
    });
}

void SonyBackend::onSdkError(quint32 error)
{
    post([this, error] {
        m_connected = false;
        releaseDevice();
        reportConnectError(error);
    });
}

void SonyBackend::reportConnectError(quint32 error)
{
    if (error == SDK::CrError_Connect_SSH_UserAuthenticationFailed
        || error == SDK::CrError_Connect_SSH_ServerAuthenticationFailed)
        emit authenticationFailed(codec::describeError(error));
    else
        emit connectionFailed(codec::describeError(error));
}

void SonyBackend::onSdkDisconnected(quint32 error)
{
    m_connected = false;
    post([this, error] {
        stopLiveViewThread();
        releaseDevice();
        emit disconnected(m_userDisconnect || error == SDK::CrError_None ? QString() : codec::describeError(error));
        m_userDisconnect = false;
    });
}

void SonyBackend::releaseDevice()
{
    if (m_handle) {
        SDK::ReleaseDevice(m_handle);
        m_handle = 0;
    }
}

void SonyBackend::disconnectCamera()
{
    post([this] {
        stopLiveViewThread();
        if (!m_handle)
            return;
        m_userDisconnect = true;
        if (CR_FAILED(SDK::Disconnect(m_handle))) {
            m_connected = false;
            releaseDevice();
            emit disconnected(QString());
        }
    });
}

void SonyBackend::loadProperties(const QVector<quint32> &codes)
{
    if (!m_handle || !m_connected)
        return;
    SDK::CrDeviceProperty *props = nullptr;
    CrInt32 count = 0;
    SDK::CrError err;
    if (codes.isEmpty()) {
        err = SDK::GetDeviceProperties(m_handle, &props, &count);
    } else {
        std::vector<CrInt32u> list(codes.begin(), codes.end());
        err = SDK::GetSelectDeviceProperties(m_handle, CrInt32u(list.size()), list.data(), &props, &count);
    }
    if (CR_FAILED(err) || !props)
        return;
    PropertyList out;
    out.reserve(count);
    for (CrInt32 i = 0; i < count; ++i)
        out.append(toInfo(props[i]));
    SDK::ReleaseDeviceProperties(m_handle, props);
    emit propertiesUpdated(out);
}

void SonyBackend::setProperty(quint32 code, qint64 value, quint32 dataType)
{
    post([this, code, value, dataType] {
        if (!m_handle || !m_connected)
            return;
        SDK::CrDeviceProperty prop;
        prop.SetCode(code);
        prop.SetCurrentValue(static_cast<CrInt64u>(value));
        prop.SetValueType(SDK::CrDataType(dataType));
        const SDK::CrError err = SDK::SetDeviceProperty(m_handle, &prop);
        qInfo().noquote() << "set" << codec::propertyName(code) << Qt::hex << code << "=" << value << "->" << err;
        if (CR_FAILED(err))
            emit warning(QStringLiteral("%1: %2").arg(codec::propertyName(code), codec::describeError(err)));
    });
}

void SonyBackend::sendCommand(Command command, bool down)
{
    post([this, command, down] {
        if (!m_handle || !m_connected)
            return;
        CrInt32u id = SDK::CrCommandId_Release;
        switch (command) {
        case Command::Release: id = SDK::CrCommandId_Release; break;
        case Command::MovieRecord: id = SDK::CrCommandId_MovieRecord; break;
        case Command::CancelRemoteTouch: id = SDK::CrCommandId_CancelRemoteTouchOperation; break;
        }
        const SDK::CrError err =
            SDK::SendCommand(m_handle, id, down ? SDK::CrCommandParam_Down : SDK::CrCommandParam_Up);
        if (CR_FAILED(err))
            emit warning(codec::describeError(err));
    });
}

void SonyBackend::setDownloadDirectory(const QString &path)
{
    post([this, path] {
        m_downloadDir = path;
        applySaveInfo();
    });
}

void SonyBackend::applySaveInfo()
{
    if (!m_handle || m_downloadDir.isEmpty())
        return;
    QDir().mkpath(m_downloadDir);
    QByteArray path = QDir::toNativeSeparators(m_downloadDir).toLocal8Bit();
    QByteArray prefix("KL");
    SDK::SetSaveInfo(m_handle, path.data(), prefix.data(), SDK::CrSETSAVEINFO_AUTO_NUMBER);
}

void SonyBackend::setLiveViewEnabled(bool enabled)
{
    m_liveViewWanted = enabled;
    post([this, enabled] {
        if (!enabled)
            stopLiveViewThread();
        else if (m_connected)
            startLiveViewThread();
    });
}

void SonyBackend::startLiveViewThread()
{
    if (m_liveViewRunning)
        return;
    if (m_liveViewThread.joinable())
        m_liveViewThread.join();
    m_liveViewRunning = true;
    m_liveViewThread = std::thread([this] { liveViewLoop(); });
}

void SonyBackend::stopLiveViewThread()
{
    m_liveViewRunning = false;
    if (m_liveViewThread.joinable() && m_liveViewThread.get_id() != std::this_thread::get_id())
        m_liveViewThread.join();
}

// Hands the newest frame to the UI thread. If the UI has not drawn the previous one yet,
// it is replaced instead of queued, so a slow paint never builds up latency.
void SonyBackend::publishFrame(QImage frame)
{
    {
        std::lock_guard lock(m_frameMutex);
        m_pendingFrame = std::move(frame);
    }
    if (m_frameQueued.exchange(true))
        return;
    QMetaObject::invokeMethod(this, [this] {
        QImage latest;
        {
            std::lock_guard lock(m_frameMutex);
            latest = std::move(m_pendingFrame);
            m_pendingFrame = QImage();
            m_frameQueued = false;
        }
        if (!latest.isNull())
            emit liveViewFrame(latest);
    }, Qt::QueuedConnection);
}

void SonyBackend::liveViewLoop()
{
    using namespace std::chrono_literals;
    std::vector<CrInt8u> buffer;
    QSize lastSize;
    int frames = 0;
    QElapsedTimer fpsClock;
    fpsClock.start();
    QElapsedTimer failing; // how long the camera has stopped answering live view requests
    bool reportedLost = false;
    while (m_liveViewRunning && m_connected) {
        const SDK::CrDeviceHandle handle = m_handle;
        SDK::CrImageInfo info;
        if (CR_FAILED(SDK::GetLiveViewImageInfo(handle, &info)) || info.GetBufferSize() == 0) {
            // The SDK may not notice a camera that vanished (switched off, out of Wi-Fi range).
            if (!failing.isValid())
                failing.start();
            if (!reportedLost && failing.elapsed() > 5000) {
                qInfo() << "live view unavailable for 5 s";
                reportedLost = true;
                emit reconnecting();
            }
            std::this_thread::sleep_for(100ms);
            continue;
        }
        failing.invalidate();
        reportedLost = false;
        if (buffer.size() < info.GetBufferSize())
            buffer.resize(info.GetBufferSize());
        SDK::CrImageDataBlock block;
        block.SetData(buffer.data());
        block.SetSize(CrInt32u(buffer.size()));
        const SDK::CrError err = SDK::GetLiveViewImage(handle, &block);
        if (CR_SUCCEEDED(err) && block.GetImageSize() > 0) {
            QImage frame = QImage::fromData(block.GetImageData(), int(block.GetImageSize()), "JPG");
            if (!frame.isNull()) {
                if (frame.size() != lastSize) {
                    qInfo() << "live view frame size" << frame.size();
                    lastSize = frame.size();
                }
                ++frames;
                publishFrame(std::move(frame));
            }
            // A new frame was just taken: ask again right away for the next one.
        } else if (err == SDK::CrWarning_Frame_NotUpdated) {
            std::this_thread::sleep_for(4ms); // poll often so a new frame is picked up quickly
        } else {
            std::this_thread::sleep_for(50ms);
        }
        if (fpsClock.elapsed() >= 10000) {
            qInfo() << "live view" << frames * 1000.0 / fpsClock.elapsed() << "fps";
            frames = 0;
            fpsClock.restart();
        }
    }
    m_liveViewRunning = false;
}

} // namespace kl
