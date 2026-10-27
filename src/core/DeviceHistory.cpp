#include "core/DeviceHistory.h"

#include <QSettings>

namespace kl {

QString DeviceHistory::Entry::key() const
{
    return model + QLatin1Char('|') + (uid.isEmpty() ? address : uid);
}

void DeviceHistory::load(QSettings &settings)
{
    m_entries.clear();
    const int count = settings.beginReadArray(QStringLiteral("devices"));
    for (int i = 0; i < count; ++i) {
        settings.setArrayIndex(i);
        Entry e;
        e.model = settings.value(QStringLiteral("model")).toString();
        e.connection = settings.value(QStringLiteral("connection")).toString();
        e.address = settings.value(QStringLiteral("address")).toString();
        e.uid = settings.value(QStringLiteral("uid")).toString();
        e.fingerprint = settings.value(QStringLiteral("fingerprint")).toString();
        e.savedLogin = settings.value(QStringLiteral("savedLogin"), false).toBool();
        e.lastUsed = settings.value(QStringLiteral("lastUsed")).toDateTime();
        if (!e.model.isEmpty())
            m_entries.append(e);
    }
    settings.endArray();

    // Earlier versions only remembered the last camera.
    if (m_entries.isEmpty() && settings.contains(QStringLiteral("lastCamera/model"))) {
        Entry e;
        e.model = settings.value(QStringLiteral("lastCamera/model")).toString();
        e.connection = settings.value(QStringLiteral("lastCamera/connection")).toString();
        e.address = settings.value(QStringLiteral("lastCamera/address")).toString();
        if (!e.model.isEmpty())
            m_entries.append(e);
    }
}

void DeviceHistory::save(QSettings &settings) const
{
    settings.remove(QStringLiteral("lastCamera"));
    settings.remove(QStringLiteral("devices"));
    settings.beginWriteArray(QStringLiteral("devices"), int(m_entries.size()));
    for (int i = 0; i < m_entries.size(); ++i) {
        const Entry &e = m_entries[i];
        settings.setArrayIndex(i);
        settings.setValue(QStringLiteral("model"), e.model);
        settings.setValue(QStringLiteral("connection"), e.connection);
        settings.setValue(QStringLiteral("address"), e.address);
        settings.setValue(QStringLiteral("uid"), e.uid);
        settings.setValue(QStringLiteral("fingerprint"), e.fingerprint);
        settings.setValue(QStringLiteral("savedLogin"), e.savedLogin);
        settings.setValue(QStringLiteral("lastUsed"), e.lastUsed);
    }
    settings.endArray();
    settings.sync();
}

int DeviceHistory::find(const CameraDescriptor &camera) const
{
    for (int i = 0; i < m_entries.size(); ++i) {
        const Entry &e = m_entries[i];
        if (e.model != camera.model)
            continue;
        if (!camera.uid.isEmpty() && e.uid == camera.uid)
            return i;
        if (e.address == camera.address && !camera.address.isEmpty() && (e.uid.isEmpty() || camera.uid.isEmpty()))
            return i;
    }
    return -1;
}

int DeviceHistory::findKey(const QString &key) const
{
    for (int i = 0; i < m_entries.size(); ++i)
        if (m_entries[i].key() == key)
            return i;
    return -1;
}

DeviceHistory::Entry &DeviceHistory::touch(const CameraDescriptor &camera, const QDateTime &when)
{
    Entry e;
    const int index = find(camera);
    if (index >= 0)
        e = m_entries.takeAt(index);
    e.model = camera.model;
    e.connection = camera.connection;
    e.address = camera.address;
    if (!camera.uid.isEmpty())
        e.uid = camera.uid;
    e.lastUsed = when;
    m_entries.prepend(e);
    while (m_entries.size() > kMaxEntries)
        m_entries.removeLast();
    return m_entries.first();
}

bool DeviceHistory::refreshAddress(const CameraDescriptor &camera)
{
    const int index = find(camera);
    if (index < 0)
        return false;
    Entry &e = m_entries[index];
    bool changed = false;
    if (!camera.address.isEmpty() && e.address != camera.address) {
        e.address = camera.address;
        changed = true;
    }
    if (e.uid.isEmpty() && !camera.uid.isEmpty()) {
        e.uid = camera.uid;
        changed = true;
    }
    return changed;
}

} // namespace kl
