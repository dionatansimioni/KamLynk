#pragma once

#include <QList>
#include <QMetaType>
#include <QString>
#include <QVector>
#include <cstdint>

namespace kl {

// Camera-independent snapshot of one SDK device property.
struct PropertyInfo
{
    quint32 code = 0;
    quint32 dataType = 0;       // SCRSDK::CrDataType
    qint64 current = 0;         // sign-extended according to dataType
    QString currentText;        // only for string properties
    QVector<qint64> values;     // discrete candidates the camera accepts
    bool isRange = false;       // values hold nothing; use min/max/step
    qint64 rangeMin = 0;
    qint64 rangeMax = 0;
    qint64 rangeStep = 1;
    bool readable = false;
    bool writable = false;
};

using PropertyList = QList<PropertyInfo>;

} // namespace kl

Q_DECLARE_METATYPE(kl::PropertyInfo)
Q_DECLARE_METATYPE(kl::PropertyList)
