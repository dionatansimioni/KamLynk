#pragma once

#include "core/CameraBackend.h"

#include <QImage>
#include <QThread>
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

namespace SCRSDK {
class ICrEnumCameraObjectInfo;
class ICrCameraObjectInfo;
}

namespace kl {

class SdkCallback;

// Real cameras through the Sony Camera Remote SDK.
// Every SDK call runs on one worker thread (m_thread); live view polls on its own thread.
class SonyBackend : public CameraBackend
{
    Q_OBJECT
public:
    explicit SonyBackend(QObject *parent = nullptr);
    ~SonyBackend() override;

    // Initializes the SDK, enumerates cameras once and releases it. Verifies that the
    // runtime layout (libxml2, CrAdapter/) is correct. Returns false on setup errors.
    static bool selfCheck(QString *report);
    // Reads the SSH fingerprint of `ip` the way a camera connection does (diagnostics).
    // Returns 0 on success, 1 on SDK failure, 2 when nothing answers on port 22.
    static int sshCheck(const QString &ip, QString *report);

    void discover() override;
    void connectCamera(const QString &id, const Credentials &credentials) override;
    void connectByAddress(const QString &ip, int modelIndex, const Credentials &credentials) override;
    void disconnectCamera() override;
    void setProperty(quint32 code, qint64 value, quint32 dataType) override;
    void sendCommand(Command command, bool down) override;
    void setLiveViewEnabled(bool enabled) override;
    void setDownloadDirectory(const QString &path) override;

private:
    friend class SdkCallback;

    void post(std::function<void()> fn);
    void doConnect(SCRSDK::ICrCameraObjectInfo *info, const Credentials &credentials, const QString &id);
    // GetFingerprint with a few retries (the camera's SSH server can be slow to answer).
    quint32 fetchFingerprint(SCRSDK::ICrCameraObjectInfo *info, QString *fingerprint);
    bool createManualInfo(const QString &ip, int modelIndex, bool ssh);
    void loadProperties(const QVector<quint32> &codes); // empty = all
    void applySaveInfo();
    void startLiveViewThread();
    void stopLiveViewThread();
    void liveViewLoop();
    void publishFrame(QImage frame); // live view thread -> UI, latest frame only
    void releaseDevice();
    void releaseCameraInfos();

    // Called from SDK threads.
    void onSdkConnected();
    void onSdkDisconnected(quint32 error);
    void onSdkError(quint32 error);
    void reportConnectError(quint32 error); // auth errors -> authenticationFailed

    QThread m_thread;
    QObject *m_worker = nullptr;
    std::unique_ptr<SdkCallback> m_callback;
    bool m_sdkReady = false;

    SCRSDK::ICrEnumCameraObjectInfo *m_enum = nullptr;
    SCRSDK::ICrCameraObjectInfo *m_manualInfo = nullptr;
    QString m_connectingModel;
    QString m_downloadDir;

    std::atomic<qint64> m_handle{0};
    std::atomic<bool> m_connected{false};
    std::atomic<bool> m_userDisconnect{false};
    std::atomic<bool> m_liveViewWanted{false};
    std::atomic<bool> m_liveViewRunning{false};
    std::thread m_liveViewThread;
    std::mutex m_frameMutex;
    QImage m_pendingFrame;
    std::atomic<bool> m_frameQueued{false};
};

} // namespace kl
