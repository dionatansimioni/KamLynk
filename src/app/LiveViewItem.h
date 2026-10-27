#pragma once

#include "core/CameraController.h"

#include <QImage>
#include <QPointer>
#include <QQuickItem>
#include <QRectF>

namespace kl {

// Shows the latest live view frame, letterboxed. The frame is uploaded as a texture and
// scaled by the GPU (scene graph), so large windows cost no CPU. `imageRect` is exposed so
// QML overlays (focus reticle, grid) can align with the picture.
class LiveViewItem : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(kl::CameraController *controller READ controller WRITE setController NOTIFY controllerChanged)
    Q_PROPERTY(QRectF imageRect READ imageRect NOTIFY imageRectChanged)
    Q_PROPERTY(bool hasFrame READ hasFrame NOTIFY imageRectChanged)
    // Shows the picture like a mirror (display only; recordings are unaffected).
    Q_PROPERTY(bool mirrored READ mirrored WRITE setMirrored NOTIFY mirroredChanged)

public:
    explicit LiveViewItem(QQuickItem *parent = nullptr);

    CameraController *controller() const { return m_controller; }
    void setController(CameraController *controller);
    QRectF imageRect() const { return m_imageRect; }
    bool hasFrame() const { return !m_imageRect.isEmpty(); }
    bool mirrored() const { return m_mirrored; }
    void setMirrored(bool mirrored);

    // Item coordinates -> normalized [0,1] camera position, or (-1,-1) outside the picture.
    Q_INVOKABLE QPointF normalizedPoint(qreal x, qreal y) const;

signals:
    void controllerChanged();
    void imageRectChanged();
    void mirroredChanged();

protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

private:
    void updateImageRect();

    QPointer<CameraController> m_controller;
    QRectF m_imageRect;
    bool m_mirrored = false;
    bool m_frameDirty = false;
};

} // namespace kl
