#include "app/LiveViewItem.h"

#include "core/CameraController.h"
#include "core/Geometry.h"

#include <QQuickWindow>
#include <QSGImageNode>

namespace kl {

LiveViewItem::LiveViewItem(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents, true);
}

void LiveViewItem::setController(CameraController *controller)
{
    if (m_controller == controller)
        return;
    if (m_controller)
        disconnect(m_controller, nullptr, this, nullptr);
    m_controller = controller;
    if (m_controller) {
        connect(m_controller, &CameraController::frameReady, this, [this] {
            m_frameDirty = true;
            update();
        });
        connect(m_controller, &CameraController::frameSizeChanged, this, [this] {
            updateImageRect();
            m_frameDirty = true;
            update();
        });
    }
    updateImageRect();
    m_frameDirty = true;
    update();
    emit controllerChanged();
}

void LiveViewItem::setMirrored(bool mirrored)
{
    if (m_mirrored == mirrored)
        return;
    m_mirrored = mirrored;
    update();
    emit mirroredChanged();
}

QPointF LiveViewItem::normalizedPoint(qreal x, qreal y) const
{
    return geometry::normalizedIn(m_imageRect, QPointF(x, y), m_mirrored);
}

QSGNode *LiveViewItem::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    auto *node = static_cast<QSGImageNode *>(oldNode);
    const QImage frame = m_controller ? m_controller->frame() : QImage();
    if (frame.isNull() || m_imageRect.isEmpty()) {
        delete node;
        return nullptr;
    }
    if (!node) {
        node = window()->createImageNode();
        node->setOwnsTexture(true);
        node->setFiltering(QSGTexture::Linear);
        m_frameDirty = true;
    }
    if (m_frameDirty) {
        // Upload only when a new frame arrived; a resize just re-scales the existing texture.
        node->setTexture(window()->createTextureFromImage(frame));
        m_frameDirty = false;
    }
    node->setRect(m_imageRect);
    node->setTextureCoordinatesTransform(m_mirrored ? QSGImageNode::MirrorHorizontally
                                                    : QSGImageNode::NoTransform);
    return node;
}

void LiveViewItem::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    updateImageRect();
    update();
}

void LiveViewItem::updateImageRect()
{
    const QSizeF source = m_controller ? QSizeF(m_controller->frameSize()) : QSizeF();
    const QRectF r = geometry::fitRect(source, QRectF(0, 0, width(), height()));
    if (r == m_imageRect)
        return;
    m_imageRect = r;
    emit imageRectChanged();
}

} // namespace kl
