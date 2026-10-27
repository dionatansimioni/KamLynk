#pragma once

#include "core/CameraBackend.h"

#include <QHash>
#include <QPair>
#include <QTimer>

namespace kl {

// Simulated ZV-E10 II. Used by the tests and by `kamlynk --demo`, so the whole UI can be
// exercised without a camera. Records every request for test assertions.
class FakeBackend : public CameraBackend
{
    Q_OBJECT
public:
    explicit FakeBackend(QObject *parent = nullptr);

    void discover() override;
    void connectCamera(const QString &id, const Credentials &credentials) override;
    void connectByAddress(const QString &ip, int modelIndex, const Credentials &credentials) override;
    void disconnectCamera() override;
    void setProperty(quint32 code, qint64 value, quint32 dataType) override;
    void sendCommand(Command command, bool down) override;
    void setLiveViewEnabled(bool enabled) override;
    void setDownloadDirectory(const QString &path) override;

    // --- test hooks ---
    void setAnimated(bool animated) { m_animated = animated; }
    void setRequiresCredentials(bool required) { m_requiresCredentials = required; }
    // How the simulated camera answers MovieRecord: start/stop, refuse, or ignore it.
    enum class RecordResponse { Normal, Fail, Ignore };
    void setRecordResponse(RecordResponse response) { m_recordResponse = response; }
    // Connections never complete (camera switched off).
    void setUnresponsive(bool unresponsive) { m_unresponsive = unresponsive; }
    // Password the simulated SSH camera accepts (any non-empty one when unset).
    void setRequiredPassword(const QString &password) { m_requiredPassword = password; }
    // Camera switched off / out of range: discovery does not list it.
    void setDiscoverable(bool discoverable) { m_discoverable = discoverable; }
    int connectAttempts() const { return m_connectAttempts; }
    void setPropertyValue(quint32 code, qint64 value); // simulate a change made on the camera body
    void emitFrame();
    void dropConnection(const QString &reason);
    void simulateReconnecting() { emit reconnecting(); }
    void simulateReconnected() { emit reconnected(); }

    bool isConnected() const { return m_connected; }
    bool liveViewEnabled() const { return m_liveView; }
    QString downloadDirectory() const { return m_downloadDir; }
    PropertyInfo property(quint32 code) const { return m_props.value(code); }
    const QList<QPair<quint32, qint64>> &setCalls() const { return m_setCalls; }
    const QList<QPair<Command, bool>> &commands() const { return m_commands; }
    Credentials lastCredentials() const { return m_lastCredentials; }

private:
    void seedProperties();
    void addProp(quint32 code, quint32 type, qint64 current, QVector<qint64> table, bool writable);
    void update(quint32 code, qint64 value);
    QImage renderFrame();

    QHash<quint32, PropertyInfo> m_props;
    QList<QPair<quint32, qint64>> m_setCalls;
    QList<QPair<Command, bool>> m_commands;
    Credentials m_lastCredentials;
    QString m_downloadDir;
    QTimer m_frameTimer;
    int m_frameNo = 0;
    int m_photoNo = 0;
    bool m_connected = false;
    bool m_liveView = false;
    bool m_animated = false;
    bool m_requiresCredentials = false;
    bool m_unresponsive = false;
    bool m_discoverable = true;
    QString m_requiredPassword;
    int m_connectAttempts = 0;
    RecordResponse m_recordResponse = RecordResponse::Normal;
};

} // namespace kl
