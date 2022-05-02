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

#ifndef QOPENSLESAUDIOINPUT_H
#define QOPENSLESAUDIOINPUT_H

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

#include <qaudiosystem_p.h>
#include <QElapsedTimer>
#include <SLES/OpenSLES.h>

#ifdef ANDROID
#include <SLES/OpenSLES_Android.h>

#define QT_ANDROID_PRESET_MIC "mic"
#define QT_ANDROID_PRESET_CAMCORDER "camcorder"
#define QT_ANDROID_PRESET_VOICE_RECOGNITION "voicerecognition"
#define QT_ANDROID_PRESET_VOICE_COMMUNICATION "voicecommunication"

#endif

QT_BEGIN_NAMESPACE

class QOpenSLESEngine;
class QIODevice;
class QBuffer;

class QAndroidAudioSource : public QPlatformAudioSource
{
    Q_OBJECT

public:
    QAndroidAudioSource(const QByteArray &device);
    ~QAndroidAudioSource();

    void start(QIODevice *device);
    QIODevice *start();
    void stop();
    void reset();
    void suspend();
    void resume();
    qsizetype bytesReady() const;
    void setBufferSize(qsizetype value);
    qsizetype bufferSize() const;
    qint64 processedUSecs() const;
    QAudio::Error error() const;
    QAudio::State state() const;
    void setFormat(const QAudioFormat &format);
    QAudioFormat format() const;

    void setVolume(qreal volume);
    qreal volume() const;

public Q_SLOTS:
    void processBuffer();

private:
    bool startRecording();
    void stopRecording();
    void writeDataToDevice(const char *data, int size);
    void flushBuffers();

    QByteArray m_device;
    QOpenSLESEngine *m_engine;
    SLObjectItf m_recorderObject;
    SLRecordItf m_recorder;
#ifdef ANDROID
    SLuint32 m_recorderPreset;
    SLAndroidSimpleBufferQueueItf m_bufferQueue;
#else
    SLBufferQueueItf m_bufferQueue;
#endif

    bool m_pullMode;
    qint64 m_processedBytes;
    QIODevice *m_audioSource;
    QBuffer *m_bufferIODevice;
    QByteArray m_pushBuffer;
    QAudioFormat m_format;
    QAudio::Error m_errorState;
    QAudio::State m_deviceState;
    qint64 m_lastNotifyTime;
    qreal m_volume;
    int m_bufferSize;
    QByteArray *m_buffers;
    int m_currentBuffer;
};

QT_END_NAMESPACE

#endif // QOPENSLESAUDIOINPUT_H
