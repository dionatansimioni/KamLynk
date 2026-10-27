#pragma once

#include <QPointF>
#include <QRectF>
#include <QSizeF>

namespace kl::geometry {

// Largest rect with `source` aspect ratio centered inside `bounds` (letterboxing).
inline QRectF fitRect(const QSizeF &source, const QRectF &bounds)
{
    if (source.isEmpty() || bounds.isEmpty())
        return {};
    const QSizeF scaled = source.scaled(bounds.size(), Qt::KeepAspectRatio);
    return QRectF(bounds.x() + (bounds.width() - scaled.width()) / 2.0,
                  bounds.y() + (bounds.height() - scaled.height()) / 2.0, scaled.width(), scaled.height());
}

// Point in item coordinates -> normalized [0,1] position inside `image`, or (-1,-1) if outside.
// With `mirrored`, the picture is drawn flipped horizontally, so x is flipped back to the
// camera's own coordinates.
inline QPointF normalizedIn(const QRectF &image, const QPointF &point, bool mirrored = false)
{
    if (image.isEmpty() || !image.contains(point))
        return {-1.0, -1.0};
    const qreal x = (point.x() - image.x()) / image.width();
    return {mirrored ? 1.0 - x : x, (point.y() - image.y()) / image.height()};
}

} // namespace kl::geometry
