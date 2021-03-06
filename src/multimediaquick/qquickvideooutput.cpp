/****************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
** Copyright (C) 2016 Research In Motion
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/
#include "qquickvideooutput_p.h"

#include "qquickvideooutput_render_p.h"
#include <private/qvideooutputorientationhandler_p.h>
#include <QtMultimedia/qmediaplayer.h>
#include <QtMultimedia/qmediacapturesession.h>
#include <private/qfactoryloader_p.h>
#include <QtCore/qloggingcategory.h>
#include <qvideosink.h>

QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(qLcVideo, "qt.multimedia.video")

/*!
    \qmltype VideoOutput
    //! \instantiates QQuickVideoOutput
    \brief Render video or camera viewfinder.

    \ingroup multimedia_qml
    \ingroup multimedia_video_qml
    \inqmlmodule QtMultimedia

    \qml

    Rectangle {
        width: 800
        height: 600
        color: "black"

        MediaPlayer {
            id: player
            source: "file://video.webm"
            autoPlay: true
        }

        VideoOutput {
            id: videoOutput
            source: player
            anchors.fill: parent
        }
    }

    \endqml

    The VideoOutput item supports untransformed, stretched, and uniformly scaled video presentation.
    For a description of stretched uniformly scaled presentation, see the \l fillMode property
    description.

    The VideoOutput item works with backends that support either QObject or
    QPlatformVideoSink. If the backend only supports QPlatformVideoSink, the video is rendered
    onto an overlay window that is layered on top of the QtQuick window. Due to the nature of the
    video overlays, certain features are not available for these kind of backends:
    \list
    \li Some transformations like rotations
    \li Having other QtQuick items on top of the VideoOutput item
    \endlist
    Most backends however do support QObject and therefore don't have the limitations
    listed above.

    \sa MediaPlayer, Camera

\omit
    \section1 Screen Saver

    If it is likely that an application will be playing video for an extended
    period of time without user interaction it may be necessary to disable
    the platform's screen saver. The \l ScreenSaver (from \l QtSystemInfo)
    may be used to disable the screensaver in this fashion:

    \qml
    import QtSystemInfo 5.0

    ScreenSaver { screenSaverEnabled: false }
    \endqml
\endomit
*/

// TODO: Restore Qt System Info docs when the module is released

/*!
    \internal
    \class QQuickVideoOutput
    \brief The QQuickVideoOutput class provides a video output item.
*/

QQuickVideoOutput::QQuickVideoOutput(QQuickItem *parent) :
    QQuickItem(parent),
    m_geometryDirty(true),
    m_orientation(0),
    m_autoOrientation(false),
    m_screenOrientationHandler(nullptr)
{
    setFlag(ItemHasContents, true);
    createBackend();
}

QQuickVideoOutput::~QQuickVideoOutput()
{
    m_backend.reset();
}

/*!
    \qmlproperty object QtMultimedia::VideoOutput::videoSurface
    \since 5.15

    This property holds the underlaying video surface that can be used
    to render the video frames to this VideoOutput element.
    It is similar to setting a QObject with \c videoSurface property as a source,
    where this video surface will be set.

    \sa source
*/

QVideoSink *QQuickVideoOutput::videoSink() const
{
    return m_backend ? m_backend->videoSink() : nullptr;
}

bool QQuickVideoOutput::createBackend()
{
    m_backend.reset(new QQuickVideoBackend(this));

    // Since new backend has been created needs to update its geometry.
    m_geometryDirty = true;

    return true;
}

/*!
    \qmlproperty enumeration QtMultimedia::VideoOutput::fillMode

    Set this property to define how the video is scaled to fit the target area.

    \list
    \li Stretch - the video is scaled to fit.
    \li PreserveAspectFit - the video is scaled uniformly to fit without cropping
    \li PreserveAspectCrop - the video is scaled uniformly to fill, cropping if necessary
    \endlist

    The default fill mode is PreserveAspectFit.
*/

QQuickVideoOutput::FillMode QQuickVideoOutput::fillMode() const
{
    return FillMode(videoSink()->aspectRatioMode());
}

void QQuickVideoOutput::setFillMode(FillMode mode)
{
    if (mode == fillMode())
        return;

    videoSink()->setAspectRatioMode(Qt::AspectRatioMode(mode));

    m_geometryDirty = true;
    update();

    emit fillModeChanged(mode);
}

void QQuickVideoOutput::_q_newFrame(const QVideoFrame &frame)
{
    if (!m_backend)
        return;

    m_backend->present(frame);
    QSize size = frame.size();
    if (!qIsDefaultAspect(m_orientation)) {
        size.transpose();
    }

    if (m_nativeSize != size) {
        m_nativeSize = size;

        m_geometryDirty = true;

        setImplicitWidth(size.width());
        setImplicitHeight(size.height());

        emit sourceRectChanged();
    }
}

/* Based on fill mode and our size, figure out the source/dest rects */
void QQuickVideoOutput::_q_updateGeometry()
{
    const QRectF rect(0, 0, width(), height());
    const QRectF absoluteRect(x(), y(), width(), height());

    if (!m_geometryDirty && m_lastRect == absoluteRect)
        return;

    QRectF oldContentRect(m_contentRect);

    m_geometryDirty = false;
    m_lastRect = absoluteRect;

    const auto fill = fillMode();
    if (m_nativeSize.isEmpty()) {
        //this is necessary for item to receive the
        //first paint event and configure video surface.
        m_contentRect = rect;
    } else if (fill == Stretch) {
        m_contentRect = rect;
    } else if (fill == PreserveAspectFit || fill == PreserveAspectCrop) {
        QSizeF scaled = m_nativeSize;
        scaled.scale(rect.size(), fill == PreserveAspectFit ?
                         Qt::KeepAspectRatio : Qt::KeepAspectRatioByExpanding);

        m_contentRect = QRectF(QPointF(), scaled);
        m_contentRect.moveCenter(rect.center());
    }

    if (m_backend)
        m_backend->updateGeometry();


    if (m_contentRect != oldContentRect)
        emit contentRectChanged();
}

void QQuickVideoOutput::_q_screenOrientationChanged(int orientation)
{
    setOrientation(orientation % 360);
}

/*!
    \qmlproperty int QtMultimedia::VideoOutput::orientation

    In some cases the source video stream requires a certain
    orientation to be correct.  This includes
    sources like a camera viewfinder, where the displayed
    viewfinder should match reality, no matter what rotation
    the rest of the user interface has.

    This property allows you to apply a rotation (in steps
    of 90 degrees) to compensate for any user interface
    rotation, with positive values in the anti-clockwise direction.

    The orientation change will also affect the mapping
    of coordinates from source to viewport.

    \sa autoOrientation
*/
int QQuickVideoOutput::orientation() const
{
    return m_orientation;
}

void QQuickVideoOutput::setOrientation(int orientation)
{
    // Make sure it's a multiple of 90.
    if (orientation % 90)
        return;

    // If there's no actual change, return
    if (m_orientation == orientation)
        return;

    // If the new orientation is the same effect
    // as the old one, don't update the video node stuff
    if ((m_orientation % 360) == (orientation % 360)) {
        m_orientation = orientation;
        emit orientationChanged();
        return;
    }

    m_geometryDirty = true;

    // Otherwise, a new orientation
    // See if we need to change aspect ratio orientation too
    bool oldAspect = qIsDefaultAspect(m_orientation);
    bool newAspect = qIsDefaultAspect(orientation);

    m_orientation = orientation;

    if (oldAspect != newAspect) {
        m_nativeSize.transpose();

        setImplicitWidth(m_nativeSize.width());
        setImplicitHeight(m_nativeSize.height());

        // Source rectangle does not change for orientation
    }

    update();
    emit orientationChanged();
}

/*!
    \qmlproperty bool QtMultimedia::VideoOutput::autoOrientation

    This property allows you to enable and disable auto orientation
    of the video stream, so that its orientation always matches
    the orientation of the screen. If \c autoOrientation is enabled,
    the \c orientation property is overwritten.

    By default \c autoOrientation is disabled.

    \sa orientation
    \since 5.2
*/
bool QQuickVideoOutput::autoOrientation() const
{
    return m_autoOrientation;
}

void QQuickVideoOutput::setAutoOrientation(bool autoOrientation)
{
    if (autoOrientation == m_autoOrientation)
        return;

    m_autoOrientation = autoOrientation;
    if (m_autoOrientation) {
        m_screenOrientationHandler = new QVideoOutputOrientationHandler(this);
        connect(m_screenOrientationHandler, SIGNAL(orientationChanged(int)),
                this, SLOT(_q_screenOrientationChanged(int)));

        _q_screenOrientationChanged(m_screenOrientationHandler->currentOrientation());
    } else {
        disconnect(m_screenOrientationHandler, SIGNAL(orientationChanged(int)),
                   this, SLOT(_q_screenOrientationChanged(int)));
        m_screenOrientationHandler->deleteLater();
        m_screenOrientationHandler = nullptr;
    }

    emit autoOrientationChanged();
}

/*!
    \qmlproperty rectangle QtMultimedia::VideoOutput::contentRect

    This property holds the item coordinates of the area that
    would contain video to render.  With certain fill modes,
    this rectangle will be larger than the visible area of the
    \c VideoOutput.

    This property is useful when other coordinates are specified
    in terms of the source dimensions - this applied for relative
    (normalized) frame coordinates in the range of 0 to 1.0.

    \sa mapRectToItem(), mapPointToItem()

    Areas outside this will be transparent.
*/
QRectF QQuickVideoOutput::contentRect() const
{
    return m_contentRect;
}

/*!
    \qmlproperty rectangle QtMultimedia::VideoOutput::sourceRect

    This property holds the area of the source video
    content that is considered for rendering.  The
    values are in source pixel coordinates, adjusted for
    the source's pixel aspect ratio.

    Note that typically the top left corner of this rectangle
    will be \c {0,0} while the width and height will be the
    width and height of the input content. Only when the video
    source has a viewport set, these values will differ.

    The orientation setting does not affect this rectangle.

    \sa QVideoFrameFormat::viewport()
*/
QRectF QQuickVideoOutput::sourceRect() const
{
    // We might have to transpose back
    QSizeF size = m_nativeSize;
    if (!qIsDefaultAspect(m_orientation)) {
        size.transpose();
    }

    // No backend? Just assume no viewport.
    if (!m_nativeSize.isValid() || !m_backend) {
        return QRectF(QPointF(), size);
    }

    // Take the viewport into account for the top left position.
    // m_nativeSize is already adjusted to the viewport, as it originates
    // from QVideoFrameFormat::viewport(), which includes pixel aspect ratio
    const QRectF viewport = m_backend->adjustedViewport();
    Q_ASSERT(viewport.size() == size);
    return QRectF(viewport.topLeft(), size);
}

QSGNode *QQuickVideoOutput::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data)
{
    _q_updateGeometry();

    if (!m_backend)
        return nullptr;

    return m_backend->updatePaintNode(oldNode, data);
}

void QQuickVideoOutput::itemChange(QQuickItem::ItemChange change,
                                         const QQuickItem::ItemChangeData &changeData)
{
    if (m_backend)
        m_backend->itemChange(change, changeData);
}

void QQuickVideoOutput::releaseResources()
{
    if (m_backend)
        m_backend->releaseResources();
}

void QQuickVideoOutput::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    Q_UNUSED(newGeometry);
    Q_UNUSED(oldGeometry);

    QQuickItem::geometryChange(newGeometry, oldGeometry);

    // Explicitly listen to geometry changes here. This is needed since changing the position does
    // not trigger a call to updatePaintNode().
    // We need to react to position changes though, as the window backened's display rect gets
    // changed in that situation.
    _q_updateGeometry();
}

void QQuickVideoOutput::_q_invalidateSceneGraph()
{
    if (m_backend)
        m_backend->invalidateSceneGraph();
}

/*!
    \qmlproperty enumeration QtMultimedia::VideoOutput::flushMode
    \since 5.13

    Set this property to define what \c VideoOutput should show
    when playback is finished or stopped.

    \list
    \li EmptyFrame - clears video output.
    \li FirstFrame - shows the first valid frame.
    \li LastFrame - shows the last valid frame.
    \endlist

    The default flush mode is EmptyFrame.
*/

void QQuickVideoOutput::setFlushMode(FlushMode mode)
{
    if (m_flushMode == mode)
        return;

    m_flushMode = mode;
    emit flushModeChanged();
}

QT_END_NAMESPACE
