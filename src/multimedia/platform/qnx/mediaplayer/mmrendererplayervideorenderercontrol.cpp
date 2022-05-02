/****************************************************************************
**
** Copyright (C) 2016 Research In Motion
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:COMM$
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** $QT_END_LICENSE$
**
**
**
**
**
**
**
**
**
**
**
**
**
**
**
**
**
**
**
******************************************************************************/

#include "mmrendererplayervideorenderercontrol_p.h"

#include "windowgrabber.h"

#include <QCoreApplication>
#include <QDebug>
#include <QVideoFrameFormat>
#include <QOpenGLContext>

#include <mm/renderer.h>

QT_BEGIN_NAMESPACE

static int winIdCounter = 0;

MmRendererPlayerVideoRendererControl::MmRendererPlayerVideoRendererControl(QObject *parent)
    : QVideoRendererControl(parent)
    , m_windowGrabber(new WindowGrabber(this))
    , m_context(0)
    , m_videoId(-1)
{
    connect(m_windowGrabber, SIGNAL(updateScene(const QSize &)), SLOT(updateScene(const QSize &)));
}

MmRendererPlayerVideoRendererControl::~MmRendererPlayerVideoRendererControl()
{
    detachDisplay();
}

QVideoSink *MmRendererPlayerVideoRendererControl::sink() const
{
    return m_sink;
}

void MmRendererPlayerVideoRendererControl::setSink(QVideoSink *surface)
{
    m_sink = QPointer<QAbstractVideoSurface>(surface);
    if (QOpenGLContext::currentContext())
        m_windowGrabber->checkForEglImageExtension();
    else if (m_sink)
        m_sink->setProperty("_q_GLThreadCallback", QVariant::fromValue<QObject*>(this));
}

void MmRendererPlayerVideoRendererControl::attachDisplay(mmr_context_t *context)
{
    if (m_videoId != -1) {
        qWarning() << "MmRendererPlayerVideoRendererControl: Video output already attached!";
        return;
    }

    if (!context) {
        qWarning() << "MmRendererPlayerVideoRendererControl: No media player context!";
        return;
    }

    const QByteArray windowGroupId = m_windowGrabber->windowGroupId();
    if (windowGroupId.isEmpty()) {
        qWarning() << "MmRendererPlayerVideoRendererControl: Unable to find window group";
        return;
    }

    const QString windowName = QStringLiteral("MmRendererPlayerVideoRendererControl_%1_%2")
                                             .arg(winIdCounter++)
                                             .arg(QCoreApplication::applicationPid());

    m_windowGrabber->setWindowId(windowName.toLatin1());

    // Start with an invisible window, because we just want to grab the frames from it.
    const QString videoDeviceUrl = QStringLiteral("screen:?winid=%1&wingrp=%2&initflags=invisible&nodstviewport=1")
                                                 .arg(windowName)
                                                 .arg(QString::fromLatin1(windowGroupId));

    m_videoId = mmr_output_attach(context, videoDeviceUrl.toLatin1(), "video");
    if (m_videoId == -1) {
        qWarning() << "mmr_output_attach() for video failed";
        return;
    }

    m_context = context;
}

void MmRendererPlayerVideoRendererControl::detachDisplay()
{
    m_windowGrabber->stop();

    if (m_sink)
        m_sink->stop();

    if (m_context && m_videoId != -1)
        mmr_output_detach(m_context, m_videoId);

    m_context = 0;
    m_videoId = -1;
}

void MmRendererPlayerVideoRendererControl::pause()
{
    m_windowGrabber->pause();
}

void MmRendererPlayerVideoRendererControl::resume()
{
    m_windowGrabber->resume();
}

class QnxTextureBuffer : public QAbstractVideoBuffer
{
public:
    QnxTextureBuffer(WindowGrabber *windowGrabber) :
        QAbstractVideoBuffer(QVideoFrame::GLTextureHandle)
    {
        m_windowGrabber = windowGrabber;
        m_handle = 0;
    }
    MapMode mapMode() const {
        return QVideoFrame::ReadWrite;
    }
    void unmap() {}
    MapData map(MapMode mode) override { return {}; }
    QVariant handle() const {
        if (!m_handle) {
            const_cast<QnxTextureBuffer*>(this)->m_handle = m_windowGrabber->getNextTextureId();
        }
        return m_handle;
    }
private:
    WindowGrabber *m_windowGrabber;
    int m_handle;
};

void MmRendererPlayerVideoRendererControl::updateScene(const QSize &size)
{
    if (m_sink) {
        // Depending on the support of EGL images on the current platform we either pass a texture
        // handle or a copy of the image data
        if (m_windowGrabber->eglImageSupported()) {
            QnxTextureBuffer *textBuffer = new QnxTextureBuffer(m_windowGrabber);
            QVideoFrame actualFrame(textBuffer, QVideoFrameFormat(size, QVideoFrameFormat::Format_BGR32));
            m_sink->setVideoFrame(actualFrame);
        } else {
            m_sink->setVideoFrame(m_windowGrabber->getNextImage().copy());
        }
    }
}

void MmRendererPlayerVideoRendererControl::customEvent(QEvent *e)
{
    // This is running in the render thread (OpenGL enabled)
    if (e->type() == QEvent::User)
        m_windowGrabber->checkForEglImageExtension();
}

QT_END_NAMESPACE
