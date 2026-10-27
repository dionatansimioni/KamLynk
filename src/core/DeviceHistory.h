#pragma once

#include "core/CameraBackend.h"

#include <QDateTime>
#include <QList>
#include <QString>

class QSettings;

namespace kl {

// Cameras connected before, most recent first, persisted in QSettings ("devices" array).
// A camera is identified by model + uid (Wi-Fi MAC or USB serial); cameras added by manual
// IP have no uid and match by model + address. The address is refreshed when a known camera
// shows up elsewhere (DHCP).
class DeviceHistory
{
public:
    struct Entry
    {
        QString model;
        QString connection;
        QString address;
        QString uid;
        QString fingerprint; // SSH host key, also the key of a saved login
        bool savedLogin = false;
        QDateTime lastUsed;

        QString key() const;
    };

    void load(QSettings &settings);
    void save(QSettings &settings) const;

    const QList<Entry> &entries() const { return m_entries; }
    bool isEmpty() const { return m_entries.isEmpty(); }
    // Index of the entry describing `camera`, or -1.
    int find(const CameraDescriptor &camera) const;
    int findKey(const QString &key) const;
    // Records a successful connection: moves the camera to the top.
    Entry &touch(const CameraDescriptor &camera, const QDateTime &when);
    void setSavedLogin(int index, bool saved) { m_entries[index].savedLogin = saved; }
    void setFingerprint(int index, const QString &fingerprint) { m_entries[index].fingerprint = fingerprint; }
    // Updates the address of a known camera seen by discovery; true if it changed.
    bool refreshAddress(const CameraDescriptor &camera);
    Entry take(int index) { return m_entries.takeAt(index); }

    static constexpr int kMaxEntries = 12;

private:
    QList<Entry> m_entries;
};

} // namespace kl
