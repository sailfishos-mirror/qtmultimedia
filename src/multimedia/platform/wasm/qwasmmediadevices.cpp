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

#include "qwasmmediadevices_p.h"
#include "qcameradevice_p.h"

#include "audio/qwasmaudiosource_p.h"
#include "audio/qwasmaudiosink_p.h"
#include "audio/qwasmaudiodevice_p.h"
#include <AL/al.h>
#include <AL/alc.h>

QT_BEGIN_NAMESPACE

QWasmMediaDevices::QWasmMediaDevices()
    : QPlatformMediaDevices()
{
    auto capture = alcGetString(nullptr, ALC_CAPTURE_DEFAULT_DEVICE_SPECIFIER);
    // present even if there is no capture device
    if (capture)
        m_ins.append((new QWasmAudioDevice(capture, "WebAssembly audio capture device", true,
                                               QAudioDevice::Input))->create());

    auto playback = alcGetString(nullptr, ALC_DEFAULT_DEVICE_SPECIFIER);
    // present even if there is no playback device
    if (playback)
        m_outs.append((new QWasmAudioDevice(playback, "WebAssembly audio playback device", true,
                                                QAudioDevice::Output))->create());
}

QList<QAudioDevice> QWasmMediaDevices::audioInputs() const
{
    return m_ins;
}

QList<QAudioDevice> QWasmMediaDevices::audioOutputs() const
{
    return m_outs;
}

QList<QCameraDevice> QWasmMediaDevices::videoInputs() const
{
    return {};
}

QPlatformAudioSource *QWasmMediaDevices::createAudioSource(const QAudioDevice &deviceInfo)
{
    return new QWasmAudioSource(deviceInfo.id());
}

QPlatformAudioSink *QWasmMediaDevices::createAudioSink(const QAudioDevice &deviceInfo)
{
    return new QWasmAudioSink(deviceInfo.id());
}

QT_END_NAMESPACE
