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


#ifndef QAUDIODEVICEINFO_P_H
#define QAUDIODEVICEINFO_P_H

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

#include <QtMultimedia/qaudiodevice.h>

QT_BEGIN_NAMESPACE

class QAudioDevicePrivate : public QSharedData
{
public:
    QAudioDevicePrivate(const QByteArray &i, QAudioDevice::Mode m)
        : id(i),
          mode(m)
    {}
    virtual ~QAudioDevicePrivate();
    QByteArray  id;
    QAudioDevice::Mode mode = QAudioDevice::Output;
    bool isDefault = false;

    QAudioFormat preferredFormat;
    QString description;
    int minimumSampleRate = 0;
    int maximumSampleRate = 0;
    int minimumChannelCount = 0;
    int maximumChannelCount = 0;
    QList<QAudioFormat::SampleFormat> supportedSampleFormats;

    QAudioDevice create() { return QAudioDevice(this); }
};

QT_END_NAMESPACE

#endif // QAUDIODEVICEINFO_H
