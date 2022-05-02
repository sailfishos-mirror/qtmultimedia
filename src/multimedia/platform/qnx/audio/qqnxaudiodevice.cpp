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

#include "qqnxaudiodevice_p.h"

#include "qnxaudioutils_p.h"

#include <sys/asoundlib.h>

QT_BEGIN_NAMESPACE

QnxAudioDeviceInfo::QnxAudioDeviceInfo(const QByteArray &deviceName, QAudioDevice::Mode mode)
    : QAudioDevicePrivate(deviceName, mode)
{
}

QnxAudioDeviceInfo::~QnxAudioDeviceInfo()
{
}

QAudioFormat QnxAudioDeviceInfo::preferredFormat() const
{
    QAudioFormat format;
    format.setSampleRate(44100);
    format.setByteOrder(QAudioFormat::LittleEndian);
    format.setSampleType(QAudioFormat::SignedInt);
    format.setSampleSize(16);
    format.setChannelCount(2);
    if(mode == QAudioDevice::Input && !isFormatSupported(format))
        format.setChannelCount(1);
    return format;
}

bool QnxAudioDeviceInfo::isFormatSupported(const QAudioFormat &format) const
{
    const int pcmMode = (mode == QAudioDevice::Output) ? SND_PCM_OPEN_PLAYBACK : SND_PCM_OPEN_CAPTURE;
    snd_pcm_t *handle;

    int card = 0;
    int device = 0;
    if (snd_pcm_open_preferred(&handle, &card, &device, pcmMode) < 0)
        return false;

    snd_pcm_channel_info_t info;
    memset (&info, 0, sizeof(info));
    info.channel = (mode == QAudioDevice::Output) ? SND_PCM_CHANNEL_PLAYBACK : SND_PCM_CHANNEL_CAPTURE;

    if (snd_pcm_plugin_info(handle, &info) < 0) {
        qWarning("QAudioDevice: couldn't get channel info");
        snd_pcm_close(handle);
        return false;
    }

    snd_pcm_channel_params_t params = QnxAudioUtils::formatToChannelParams(format, mode, info.max_fragment_size);
    const int errorCode = snd_pcm_plugin_params(handle, &params);
    snd_pcm_close(handle);

    return errorCode == 0;
}

QList<int> QnxAudioDeviceInfo::supportedSampleRates() const
{
    return QList<int>() << 8000 << 11025 << 22050 << 44100 << 48000;
}

QList<int> QnxAudioDeviceInfo::supportedChannelCounts() const
{
    return QList<int>() << 1 << 2;
}

QList<int> QnxAudioDeviceInfo::supportedSampleSizes() const
{
    return QList<int>() << 8 << 16 << 32;
}

QList<QAudioFormat::Endian> QnxAudioDeviceInfo::supportedByteOrders() const
{
    return QList<QAudioFormat::Endian>() << QAudioFormat::LittleEndian << QAudioFormat::BigEndian;
}

QList<QAudioFormat::SampleType> QnxAudioDeviceInfo::supportedSampleTypes() const
{
    return QList<QAudioFormat::SampleType>() << QAudioFormat::SignedInt << QAudioFormat::UnSignedInt << QAudioFormat::Float;
}

QT_END_NAMESPACE
