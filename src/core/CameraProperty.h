#pragma once

#include "core/PropertyInfo.h"

#include <QObject>
#include <QStringList>
#include <functional>

namespace kl {

// QML-facing view of one camera setting (ISO, aperture...). Only shows what the
// connected camera reports, so the UI adapts to each model automatically.
class CameraProperty : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int code READ code CONSTANT)
    Q_PROPERTY(QString name READ name NOTIFY changed)
    Q_PROPERTY(QString description READ description NOTIFY changed)
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(bool writable READ writable NOTIFY changed)
    Q_PROPERTY(double value READ value NOTIFY changed)
    Q_PROPERTY(QString text READ text NOTIFY changed)
    Q_PROPERTY(QStringList options READ options NOTIFY changed)
    Q_PROPERTY(int index READ index NOTIFY changed)
    Q_PROPERTY(bool canDecrease READ canDecrease NOTIFY changed)
    Q_PROPERTY(bool canIncrease READ canIncrease NOTIFY changed)

public:
    using Setter = std::function<void(quint32 code, qint64 value, quint32 dataType)>;

    CameraProperty(quint32 code, Setter setter, QObject *parent = nullptr);

    int code() const { return int(m_info.code); }
    QString name() const;
    QString description() const;
    bool available() const { return m_available; }
    bool writable() const { return m_available && m_info.writable; }
    double value() const { return double(m_info.current); }
    qint64 rawValue() const { return m_info.current; }
    QString text() const;
    QStringList options() const;
    int index() const;
    bool canDecrease() const;
    bool canIncrease() const;
    const PropertyInfo &info() const { return m_info; }

    // Candidate values in camera order (ranges are expanded when small enough).
    QVector<qint64> candidates() const;

    Q_INVOKABLE void setIndex(int index);
    Q_INVOKABLE void stepBy(int delta);
    Q_INVOKABLE void setValue(double value);

    void update(const PropertyInfo &info);
    void reset();
    // The UI language changed: texts must be read again.
    void retranslate() { emit changed(); }

signals:
    void changed();

private:
    PropertyInfo m_info;
    Setter m_setter;
    bool m_available = false;
};

} // namespace kl
