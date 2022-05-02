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
#ifndef BBCAMERAFOCUSCONTROL_H
#define BBCAMERAFOCUSCONTROL_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <private/qplatformcamerafocus_p.h>

QT_BEGIN_NAMESPACE

class BbCameraSession;

class BbCameraFocusControl : public QPlatformCameraFocus
{
    Q_OBJECT
public:
    explicit BbCameraFocusControl(BbCameraSession *session, QObject *parent = 0);

    QCamera::FocusMode focusMode() const override;
    void setFocusMode(QCamera::FocusMode mode) override;
    bool isFocusModeSupported(QCamera::FocusMode mode) const override;
    QPointF focusPoint() const override;
    void setCustomFocusPoint(const QPointF &point) override;

    qreal maximumOpticalZoom() const override;
    qreal maximumDigitalZoom() const override;
    qreal requestedOpticalZoom() const override;
    qreal requestedDigitalZoom() const override;
    qreal currentOpticalZoom() const override;
    qreal currentDigitalZoom() const override;
    void zoomTo(qreal optical, qreal digital) override;

private Q_SLOTS:
    void statusChanged(QCamera::Status status);

private:
    void updateCustomFocusRegion();
    bool retrieveViewfinderSize(int *width, int *height);

    BbCameraSession *m_session;

    QCamera::FocusMode m_focusMode;
    QPointF m_customFocusPoint;

    qreal m_minimumZoomFactor;
    qreal m_maximumZoomFactor;
    bool m_supportsSmoothZoom;
    qreal m_requestedZoomFactor;
};

QT_END_NAMESPACE

#endif
