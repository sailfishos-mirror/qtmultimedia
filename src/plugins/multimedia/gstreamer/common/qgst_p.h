// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QGST_P_H
#define QGST_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API. It exists purely as an
// implementation detail. This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <common/qgst_handle_types_p.h>

#include <private/qtmultimediaglobal_p.h>
#include <private/qmultimediautils_p.h>

#include <QtCore/qdebug.h>
#include <QtCore/qlist.h>
#include <QtCore/qsemaphore.h>

#include <QtMultimedia/qaudioformat.h>
#include <QtMultimedia/qvideoframe.h>

#include <gst/gst.h>
#include <gst/video/video-info.h>

#include <type_traits>

#if QT_CONFIG(gstreamer_photography)
#define GST_USE_UNSTABLE_API
#include <gst/interfaces/photography.h>
#undef GST_USE_UNSTABLE_API
#endif

QT_BEGIN_NAMESPACE

namespace QGstImpl {

template <typename T>
struct GstObjectTraits
{
    // using Type = T;
    // template <typename U>
    // static bool isObjectOfType(U *);
    // template <typename U>
    // static T *cast(U *);
};

#define QGST_DEFINE_CAST_TRAITS(ClassName, MACRO_LABEL) \
    template <>                                         \
    struct GstObjectTraits<ClassName>                   \
    {                                                   \
        using Type = ClassName;                         \
        template <typename U>                           \
        static bool isObjectOfType(U *arg)              \
        {                                               \
            return GST_IS_##MACRO_LABEL(arg);           \
        }                                               \
        template <typename U>                           \
        static Type *cast(U *arg)                       \
        {                                               \
            return GST_##MACRO_LABEL##_CAST(arg);       \
        }                                               \
        template <typename U>                           \
        static Type *checked_cast(U *arg)               \
        {                                               \
            return GST_##MACRO_LABEL(arg);              \
        }                                               \
    };                                                  \
    static_assert(true, "ensure semicolon")

QGST_DEFINE_CAST_TRAITS(GstPipeline, PIPELINE);
QGST_DEFINE_CAST_TRAITS(GstElement, ELEMENT);
QGST_DEFINE_CAST_TRAITS(GstBin, BIN);
QGST_DEFINE_CAST_TRAITS(GstPad, PAD);

#undef QGST_DEFINE_CAST_TRAITS

} // namespace QGstImpl

template <typename DestinationType, typename SourceType>
DestinationType *qGstSafeCast(SourceType *arg)
{
    using Traits = QGstImpl::GstObjectTraits<DestinationType>;
    if (arg && Traits::isObjectOfType(arg))
        return Traits::cast(arg);
    return nullptr;
}

template <typename DestinationType, typename SourceType>
DestinationType *qGstCheckedCast(SourceType *arg)
{
    using Traits = QGstImpl::GstObjectTraits<DestinationType>;
    if (arg)
        Q_ASSERT(Traits::isObjectOfType(arg));
    return Traits::cast(arg);
}

class QSize;
class QGstStructure;
class QGstCaps;
class QGstPipelinePrivate;
class QCameraFormat;

template <typename T> struct QGRange
{
    T min;
    T max;
};

struct QGString : QUniqueGStringHandle
{
    using QUniqueGStringHandle::QUniqueGStringHandle;

    QLatin1StringView asStringView() const { return QLatin1StringView{ get() }; }
    QString toQString() const { return QString::fromUtf8(get()); }
};

class QGValue
{
public:
    explicit QGValue(const GValue *v);
    const GValue *value;

    bool isNull() const;

    std::optional<bool> toBool() const;
    std::optional<int> toInt() const;
    std::optional<int> toInt64() const;
    template<typename T>
    T *getPointer() const
    {
        return value ? static_cast<T *>(g_value_get_pointer(value)) : nullptr;
    }

    const char *toString() const;
    std::optional<float> getFraction() const;
    std::optional<QGRange<float>> getFractionRange() const;
    std::optional<QGRange<int>> toIntRange() const;

    QGstStructure toStructure() const;
    QGstCaps toCaps() const;

    bool isList() const;
    int listSize() const;
    QGValue at(int index) const;

    QList<QAudioFormat::SampleFormat> getSampleFormats() const;
};

namespace QGstPointerImpl {

template <typename RefcountedObject>
struct QGstRefcountingAdaptor;

template <typename GstType>
class QGstObjectWrapper
{
    using Adaptor = QGstRefcountingAdaptor<GstType>;

    GstType *m_object = nullptr;

public:
    enum RefMode { HasRef, NeedsRef };

    constexpr QGstObjectWrapper() = default;

    explicit QGstObjectWrapper(GstType *object, RefMode mode) : m_object(object)
    {
        if (m_object && mode == NeedsRef)
            Adaptor::ref(m_object);
    }

    QGstObjectWrapper(const QGstObjectWrapper &other) : m_object(other.m_object)
    {
        if (m_object)
            Adaptor::ref(m_object);
    }

    ~QGstObjectWrapper()
    {
        if (m_object)
            Adaptor::unref(m_object);
    }

    QGstObjectWrapper(QGstObjectWrapper &&other) noexcept
        : m_object(std::exchange(other.m_object, nullptr))
    {
    }

    QGstObjectWrapper &
    operator=(const QGstObjectWrapper &other) // NOLINT: bugprone-unhandled-self-assign
    {
        if (m_object != other.m_object) {
            GstType *originalObject = m_object;

            m_object = other.m_object;
            if (m_object)
                Adaptor::ref(m_object);
            if (originalObject)
                Adaptor::unref(originalObject);
        }
        return *this;
    }

    QGstObjectWrapper &operator=(QGstObjectWrapper &&other) noexcept
    {
        if (this != &other) {
            GstType *originalObject = m_object;
            m_object = std::exchange(other.m_object, nullptr);

            if (originalObject)
                Adaptor::unref(originalObject);
        }
        return *this;
    }

    friend bool operator==(const QGstObjectWrapper &a, const QGstObjectWrapper &b)
    {
        return a.m_object == b.m_object;
    }
    friend bool operator!=(const QGstObjectWrapper &a, const QGstObjectWrapper &b)
    {
        return a.m_object != b.m_object;
    }

    explicit operator bool() const { return bool(m_object); }
    bool isNull() const { return !m_object; }
    GstType *release() { return std::exchange(m_object, nullptr); }

protected:
    GstType *get() const { return m_object; }
};

} // namespace QGstPointerImpl

class QGstreamerMessage;

class QGstStructure
{
public:
    const GstStructure *structure = nullptr;
    QGstStructure() = default;
    QGstStructure(const GstStructure *s);
    void free();

    bool isNull() const;

    QByteArrayView name() const;
    QGValue operator[](const char *name) const;

    QSize resolution() const;
    QVideoFrameFormat::PixelFormat pixelFormat() const;
    QGRange<float> frameRateRange() const;
    QGstreamerMessage getMessage();
    std::optional<Fraction> pixelAspectRatio() const;
    QSize nativeSize() const;

    QGstStructure copy() const;
};

template <>
struct QGstPointerImpl::QGstRefcountingAdaptor<GstCaps>
{
    static void ref(GstCaps *arg) noexcept { gst_caps_ref(arg); }
    static void unref(GstCaps *arg) noexcept { gst_caps_unref(arg); }
};

class QGstCaps : public QGstPointerImpl::QGstObjectWrapper<GstCaps>
{
    using BaseClass = QGstPointerImpl::QGstObjectWrapper<GstCaps>;

public:
    using BaseClass::BaseClass;
    QGstCaps(const QGstCaps &) = default;
    QGstCaps(QGstCaps &&) noexcept = default;
    QGstCaps &operator=(const QGstCaps &) = default;
    QGstCaps &operator=(QGstCaps &&) noexcept = default;

    enum MemoryFormat { CpuMemory, GLTexture, DMABuf };

    int size() const;
    QGstStructure at(int index) const;
    GstCaps *caps() const;

    MemoryFormat memoryFormat() const;
    std::optional<std::pair<QVideoFrameFormat, GstVideoInfo>> formatAndVideoInfo() const;

    void addPixelFormats(const QList<QVideoFrameFormat::PixelFormat> &formats, const char *modifier = nullptr);

    static QGstCaps create();

    static QGstCaps fromCameraFormat(const QCameraFormat &format);
};

template <>
struct QGstPointerImpl::QGstRefcountingAdaptor<GstObject>
{
    static void ref(GstObject *arg) noexcept { gst_object_ref_sink(arg); }
    static void unref(GstObject *arg) noexcept { gst_object_unref(arg); }
};

class QGstObject : public QGstPointerImpl::QGstObjectWrapper<GstObject>
{
    using BaseClass = QGstPointerImpl::QGstObjectWrapper<GstObject>;

public:
    using BaseClass::BaseClass;
    QGstObject(const QGstObject &) = default;
    QGstObject(QGstObject &&) noexcept = default;

    virtual ~QGstObject() = default;

    QGstObject &operator=(const QGstObject &) = default;
    QGstObject &operator=(QGstObject &&) noexcept = default;

    void set(const char *property, const char *str);
    void set(const char *property, bool b);
    void set(const char *property, uint i);
    void set(const char *property, int i);
    void set(const char *property, qint64 i);
    void set(const char *property, quint64 i);
    void set(const char *property, double d);
    void set(const char *property, const QGstObject &o);
    void set(const char *property, const QGstCaps &c);

    QGString getString(const char *property) const;
    QGstStructure getStructure(const char *property) const;
    bool getBool(const char *property) const;
    uint getUInt(const char *property) const;
    int getInt(const char *property) const;
    quint64 getUInt64(const char *property) const;
    qint64 getInt64(const char *property) const;
    float getFloat(const char *property) const;
    double getDouble(const char *property) const;
    QGstObject getObject(const char *property) const;

    void connect(const char *name, GCallback callback, gpointer userData);

    GType type() const;
    GstObject *object() const;
    const char *name() const;
};

class QGstElement;

class QGstPad : public QGstObject
{
public:
    using QGstObject::QGstObject;
    QGstPad(const QGstPad &) = default;
    QGstPad(QGstPad &&) noexcept = default;

    explicit QGstPad(const QGstObject &o);
    explicit QGstPad(GstPad *pad, RefMode mode = NeedsRef);

    QGstPad &operator=(const QGstPad &) = default;
    QGstPad &operator=(QGstPad &&) noexcept = default;

    QGstCaps currentCaps() const;
    QGstCaps queryCaps() const;

    bool isLinked() const;
    bool link(const QGstPad &sink) const;
    bool unlink(const QGstPad &sink) const;
    bool unlinkPeer() const;
    QGstPad peer() const;
    QGstElement parent() const;

    GstPad *pad() const;

    GstEvent *stickyEvent(GstEventType type);
    bool sendEvent(GstEvent *event);

    template<auto Member, typename T>
    void addProbe(T *instance, GstPadProbeType type) {
        auto callback = [](GstPad *pad, GstPadProbeInfo *info, gpointer userData) {
            return (static_cast<T *>(userData)->*Member)(QGstPad(pad, NeedsRef), info);
        };

        gst_pad_add_probe(pad(), type, callback, instance, nullptr);
    }

    template <typename Functor>
    void doInIdleProbe(Functor &&work)
    {
        struct CallbackData {
            QSemaphore waitDone;
            Functor work;
        };

        CallbackData cd{
            .waitDone = QSemaphore{},
            .work = std::forward<Functor>(work),
        };

        auto callback= [](GstPad *, GstPadProbeInfo *, gpointer p) {
            auto cd = reinterpret_cast<CallbackData*>(p);
            cd->work();
            cd->waitDone.release();
            return GST_PAD_PROBE_REMOVE;
        };

        gst_pad_add_probe(pad(), GST_PAD_PROBE_TYPE_IDLE, callback, &cd, nullptr);
        cd.waitDone.acquire();
    }

    template<auto Member, typename T>
    void addEosProbe(T *instance) {
        auto callback = [](GstPad *, GstPadProbeInfo *info, gpointer userData) {
            if (GST_EVENT_TYPE(GST_PAD_PROBE_INFO_DATA(info)) != GST_EVENT_EOS)
                return GST_PAD_PROBE_PASS;
            (static_cast<T *>(userData)->*Member)();
            return GST_PAD_PROBE_REMOVE;
        };

        gst_pad_add_probe(pad(), GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM, callback, instance, nullptr);
    }
};

class QGstClock : public QGstObject
{
public:
    QGstClock() = default;
    explicit QGstClock(const QGstObject &o);
    explicit QGstClock(GstClock *clock, RefMode mode = NeedsRef);

    GstClock *clock() const;
    GstClockTime time() const;
};

class QGstElement : public QGstObject
{
public:
    using QGstObject::QGstObject;

    QGstElement(const QGstElement &) = default;
    QGstElement(QGstElement &&) noexcept = default;
    QGstElement &operator=(const QGstElement &) = default;
    QGstElement &operator=(QGstElement &&) noexcept = default;

    explicit QGstElement(GstElement *element, RefMode mode = NeedsRef);
    static QGstElement createFromFactory(const char *factory, const char *name = nullptr);
    static QGstElement createFromDevice(const QGstDeviceHandle &, const char *name = nullptr);
    static QGstElement createFromDevice(GstDevice *, const char *name = nullptr);

    QGstPad staticPad(const char *name) const;
    QGstPad src() const;
    QGstPad sink() const;
    QGstPad getRequestPad(const char *name) const;
    void releaseRequestPad(const QGstPad &pad) const;

    GstState state() const;
    GstStateChangeReturn setState(GstState state);
    bool setStateSync(GstState state, std::chrono::nanoseconds timeout = std::chrono::seconds(1));
    bool syncStateWithParent();
    bool finishStateChange(std::chrono::nanoseconds timeout = std::chrono::seconds(5));

    void lockState(bool locked);
    bool isStateLocked() const;

    void sendEvent(GstEvent *event) const;
    void sendEos() const;

    template<auto Member, typename T>
    void onPadAdded(T *instance) {
        struct Impl {
            static void callback(GstElement *e, GstPad *pad, gpointer userData) {
                (static_cast<T *>(userData)->*Member)(QGstElement(e), QGstPad(pad, NeedsRef));
            };
        };

        connect("pad-added", G_CALLBACK(Impl::callback), instance);
    }
    template<auto Member, typename T>
    void onPadRemoved(T *instance) {
        struct Impl {
            static void callback(GstElement *e, GstPad *pad, gpointer userData) {
                (static_cast<T *>(userData)->*Member)(QGstElement(e), QGstPad(pad, NeedsRef));
            };
        };

        connect("pad-removed", G_CALLBACK(Impl::callback), instance);
    }
    template<auto Member, typename T>
    void onNoMorePads(T *instance) {
        struct Impl {
            static void callback(GstElement *e, gpointer userData) {
                (static_cast<T *>(userData)->*Member)(QGstElement(e));
            };
        };

        connect("no-more-pads", G_CALLBACK(Impl::callback), instance);
    }

    GstClockTime baseTime() const;
    void setBaseTime(GstClockTime time) const;

    GstElement *element() const;
};

template <typename... Ts>
std::enable_if_t<(std::is_base_of_v<QGstElement, Ts> && ...), void>
qLinkGstElements(const Ts &...ts)
{
    bool link_success = [&] {
        if constexpr (sizeof...(Ts) == 2)
            return gst_element_link(ts.element()...);
        else
            return gst_element_link_many(ts.element()..., nullptr);
    }();

    if (Q_UNLIKELY(!link_success)) {
        qWarning() << "qLinkGstElements: could not link elements: "
                   << std::initializer_list<const char *>{
                          (GST_ELEMENT_NAME(ts.element()))...,
                      };
    }
}

template <typename... Ts>
std::enable_if_t<(std::is_base_of_v<QGstElement, Ts> && ...), void>
qUnlinkGstElements(const Ts &...ts)
{
    if constexpr (sizeof...(Ts) == 2)
        gst_element_unlink(ts.element()...);
    else
        gst_element_unlink_many(ts.element()..., nullptr);
}

class QGstBin : public QGstElement
{
public:
    using QGstElement::QGstElement;
    QGstBin(const QGstBin &) = default;
    QGstBin(QGstBin &&) noexcept = default;
    QGstBin &operator=(const QGstBin &) = default;
    QGstBin &operator=(QGstBin &&) noexcept = default;

    explicit QGstBin(GstBin *bin, RefMode mode = NeedsRef);
    static QGstBin create(const char *name);
    static QGstBin createFromFactory(const char *factory, const char *name);

    template <typename... Ts>
    std::enable_if_t<(std::is_base_of_v<QGstElement, Ts> && ...), void> add(const Ts &...ts)
    {
        if constexpr (sizeof...(Ts) == 1)
            gst_bin_add(bin(), ts.element()...);
        else
            gst_bin_add_many(bin(), ts.element()..., nullptr);
    }

    template <typename... Ts>
    std::enable_if_t<(std::is_base_of_v<QGstElement, Ts> && ...), void> remove(const Ts &...ts)
    {
        if constexpr (sizeof...(Ts) == 1)
            gst_bin_remove(bin(), ts.element()...);
        else
            gst_bin_remove_many(bin(), ts.element()..., nullptr);
    }

    template <typename... Ts>
    std::enable_if_t<(std::is_base_of_v<QGstElement, Ts> && ...), void>
    stopAndRemoveElements(Ts... ts)
    {
        bool stateChangeSuccessful = (ts.setStateSync(GST_STATE_NULL) && ...);
        Q_ASSERT(stateChangeSuccessful);
        remove(ts...);
    }

    GstBin *bin() const;

    void addGhostPad(const QGstElement &child, const char *name);
    void addGhostPad(const char *name, const QGstPad &pad);

    bool syncChildrenState();

    void dumpGraph(const char *fileNamePrefix);
};


inline QString errorMessageCannotFindElement(std::string_view element)
{
    return QStringLiteral("Could not find the %1 GStreamer element")
            .arg(QLatin1StringView(element));
}

QT_END_NAMESPACE

#endif
