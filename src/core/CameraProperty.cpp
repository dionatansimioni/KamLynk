#include "core/CameraProperty.h"

#include "core/PropertyCodec.h"

#include <algorithm>

namespace kl {

namespace {
constexpr qint64 kMaxExpandedRange = 400;
}

CameraProperty::CameraProperty(quint32 code, Setter setter, QObject *parent)
    : QObject(parent)
    , m_setter(std::move(setter))
{
    m_info.code = code;
}

QString CameraProperty::name() const
{
    return codec::propertyName(m_info.code);
}

QString CameraProperty::description() const
{
    return codec::propertyDescription(m_info.code);
}

QString CameraProperty::text() const
{
    if (!m_available)
        return QStringLiteral("--");
    if (codec::isString(m_info.dataType))
        return m_info.currentText;
    return codec::format(m_info.code, m_info.current);
}

QVector<qint64> CameraProperty::candidates() const
{
    if (!m_info.isRange)
        return m_info.values;
    QVector<qint64> out;
    const qint64 step = m_info.rangeStep > 0 ? m_info.rangeStep : 1;
    if (m_info.rangeMax < m_info.rangeMin || (m_info.rangeMax - m_info.rangeMin) / step >= kMaxExpandedRange)
        return out;
    for (qint64 v = m_info.rangeMin; v <= m_info.rangeMax; v += step)
        out.append(v);
    return out;
}

QStringList CameraProperty::options() const
{
    QStringList out;
    for (qint64 v : candidates())
        out << codec::format(m_info.code, v);
    return out;
}

int CameraProperty::index() const
{
    return int(candidates().indexOf(m_info.current));
}

bool CameraProperty::canDecrease() const
{
    if (!writable())
        return false;
    if (m_info.isRange)
        return m_info.current > m_info.rangeMin;
    return index() > 0;
}

bool CameraProperty::canIncrease() const
{
    if (!writable())
        return false;
    if (m_info.isRange)
        return m_info.current < m_info.rangeMax;
    const int i = index();
    return i >= 0 && i < m_info.values.size() - 1;
}

void CameraProperty::setIndex(int index)
{
    const QVector<qint64> c = candidates();
    if (index < 0 || index >= c.size())
        return;
    setValue(double(c[index]));
}

void CameraProperty::stepBy(int delta)
{
    if (!writable() || delta == 0)
        return;
    if (m_info.isRange) {
        const qint64 next = std::clamp(m_info.current + delta * m_info.rangeStep, m_info.rangeMin, m_info.rangeMax);
        setValue(double(next));
        return;
    }
    const int i = index();
    if (i < 0)
        return;
    setIndex(std::clamp(i + delta, 0, int(m_info.values.size()) - 1));
}

void CameraProperty::setValue(double value)
{
    const auto v = static_cast<qint64>(value);
    if (!writable() || v == m_info.current || !m_setter)
        return;
    m_setter(m_info.code, v, m_info.dataType);
}

void CameraProperty::update(const PropertyInfo &info)
{
    m_info = info;
    m_available = info.readable;
    emit changed();
}

void CameraProperty::reset()
{
    const quint32 code = m_info.code;
    m_info = PropertyInfo{};
    m_info.code = code;
    m_available = false;
    emit changed();
}

} // namespace kl
