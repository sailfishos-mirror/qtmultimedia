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

#ifndef SOURCERESOLVER_H
#define SOURCERESOLVER_H

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

#include "mfstream_p.h"
#include <QUrl>

class SourceResolver: public QObject, public IMFAsyncCallback
{
    Q_OBJECT
public:
    SourceResolver();

    ~SourceResolver();

    STDMETHODIMP QueryInterface(REFIID riid, LPVOID *ppvObject) override;
    STDMETHODIMP_(ULONG) AddRef(void) override;
    STDMETHODIMP_(ULONG) Release(void) override;

    HRESULT STDMETHODCALLTYPE Invoke(IMFAsyncResult *pAsyncResult) override;

    HRESULT STDMETHODCALLTYPE GetParameters(DWORD*, DWORD*) override;

    void load(const QUrl &url, QIODevice* stream);

    void cancel();

    void shutdown();

    IMFMediaSource* mediaSource() const;

Q_SIGNALS:
    void error(long hr);
    void mediaSourceReady();

private:
    class State : public IUnknown
    {
    public:
        State(IMFSourceResolver *sourceResolver, bool fromStream);
        virtual ~State();

        STDMETHODIMP QueryInterface(REFIID riid, LPVOID *ppvObject) override;

        STDMETHODIMP_(ULONG) AddRef(void) override;

        STDMETHODIMP_(ULONG) Release(void) override;

        IMFSourceResolver* sourceResolver() const;
        bool fromStream() const;

    private:
        long m_cRef;
        IMFSourceResolver *m_sourceResolver;
        bool m_fromStream;
    };

    long              m_cRef;
    IUnknown          *m_cancelCookie;
    IMFSourceResolver *m_sourceResolver;
    IMFMediaSource    *m_mediaSource;
    MFStream          *m_stream;
    QMutex            m_mutex;
};

#endif
