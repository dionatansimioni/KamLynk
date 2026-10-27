#include "core/CredentialStore.h"

namespace kl {

void MemoryCredentialStore::load(const QString &key)
{
    QMetaObject::invokeMethod(this, [this, key] {
        const auto it = m_items.constFind(key);
        if (it == m_items.constEnd())
            emit loaded(key, false, {}, {});
        else
            emit loaded(key, true, it->first, it->second);
    }, Qt::QueuedConnection);
}

void MemoryCredentialStore::save(const QString &key, const QString &, const QString &user, const QString &password)
{
    m_items.insert(key, {user, password});
}

} // namespace kl
