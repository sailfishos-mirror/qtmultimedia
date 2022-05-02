/****************************************************************************
**
** Copyright (C) 2022 The Qt Company Ltd.
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
#ifndef QPLATFORMAUDIOINPUT_H
#define QPLATFORMAUDIOINPUT_H

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

#include <private/qtmultimediaglobal_p.h>
#include <qaudiodevice.h>

#include <functional>

QT_BEGIN_NAMESPACE

class QAudioInput;

class Q_MULTIMEDIA_EXPORT QPlatformAudioInput
{
public:
    QPlatformAudioInput(QAudioInput *qq) : q(qq) {}
    virtual ~QPlatformAudioInput() {}

    virtual void setAudioDevice(const QAudioDevice &/*device*/) {}
    virtual void setMuted(bool /*muted*/) {}
    virtual void setVolume(float /*volume*/) {}

    QAudioInput *q = nullptr;
    QAudioDevice device;
    float volume = 1.;
    bool muted = false;
    std::function<void()> disconnectFunction;
};

QT_END_NAMESPACE


#endif // QPLATFORMAUDIOINPUT_H
