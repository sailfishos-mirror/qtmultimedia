/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#include "qgstcodecsinfo_p.h"
#include "qgstutils_p.h"
#include <QtCore/qset.h>

#include <gst/pbutils/pbutils.h>

static QSet<QString> streamTypes(GstElementFactory *factory, GstPadDirection direction)
{
    QSet<QString> types;
    const GList *pads = gst_element_factory_get_static_pad_templates(factory);
    for (const GList *pad = pads; pad; pad = g_list_next(pad)) {
        GstStaticPadTemplate *templ = reinterpret_cast<GstStaticPadTemplate *>(pad->data);
        if (templ->direction == direction) {
            GstCaps *caps = gst_static_caps_get(&templ->static_caps);
            for (uint i = 0; i < gst_caps_get_size(caps); ++i) {
                GstStructure *structure = gst_caps_get_structure(caps, i);
                types.insert(QString::fromUtf8(gst_structure_get_name(structure)));
            }
            gst_caps_unref(caps);
        }
    }

    return types;
}

QGstCodecsInfo::QGstCodecsInfo(QGstCodecsInfo::ElementType elementType)
{
    updateCodecs(elementType);
    for (auto &codec : supportedCodecs()) {
        GstElementFactory *factory = gst_element_factory_find(codecElement(codec).constData());
        if (factory) {
            GstPadDirection direction = elementType == Muxer ? GST_PAD_SINK : GST_PAD_SRC;
            m_streamTypes.insert(codec, streamTypes(factory, direction));
            gst_object_unref(GST_OBJECT(factory));
        }
    }
}

QStringList QGstCodecsInfo::supportedCodecs() const
{
    return m_codecs;
}

QString QGstCodecsInfo::codecDescription(const QString &codec) const
{
    return m_codecInfo.value(codec).description;
}

QByteArray QGstCodecsInfo::codecElement(const QString &codec) const

{
    return m_codecInfo.value(codec).elementName;
}

QStringList QGstCodecsInfo::codecOptions(const QString &codec) const
{
    QStringList options;

    QByteArray elementName = m_codecInfo.value(codec).elementName;
    if (elementName.isEmpty())
        return options;

    GstElement *element = gst_element_factory_make(elementName, nullptr);
    if (element) {
        guint numProperties;
        GParamSpec **properties = g_object_class_list_properties(G_OBJECT_GET_CLASS(element),
                                                                 &numProperties);
        for (guint j = 0; j < numProperties; ++j) {
            GParamSpec *property = properties[j];
            // ignore some properties
            if (strcmp(property->name, "name") == 0 || strcmp(property->name, "parent") == 0)
                continue;

            options.append(QLatin1String(property->name));
        }
        g_free(properties);
        gst_object_unref(element);
    }

    return options;
}

void QGstCodecsInfo::updateCodecs(ElementType elementType)
{
    m_codecs.clear();
    m_codecInfo.clear();

    GList *elements = elementFactories(elementType);

    QSet<QByteArray> fakeEncoderMimeTypes;
    fakeEncoderMimeTypes << "unknown/unknown"
                  << "audio/x-raw-int" << "audio/x-raw-float"
                  << "video/x-raw-yuv" << "video/x-raw-rgb";

    QSet<QByteArray> fieldsToAdd;
    fieldsToAdd << "mpegversion" << "layer" << "layout" << "raversion"
                << "wmaversion" << "wmvversion" << "variant" << "systemstream";

    GList *element = elements;
    while (element) {
        GstElementFactory *factory = (GstElementFactory *)element->data;
        element = element->next;

        const GList *padTemplates = gst_element_factory_get_static_pad_templates(factory);
        while (padTemplates) {
            GstStaticPadTemplate *padTemplate = (GstStaticPadTemplate *)padTemplates->data;
            padTemplates = padTemplates->next;

            if (padTemplate->direction == GST_PAD_SRC) {
                GstCaps *caps = gst_static_caps_get(&padTemplate->static_caps);
                for (uint i=0; i<gst_caps_get_size(caps); i++) {
                    const GstStructure *structure = gst_caps_get_structure(caps, i);

                    //skip "fake" encoders
                    if (fakeEncoderMimeTypes.contains(gst_structure_get_name(structure)))
                        continue;

                    GstStructure *newStructure = qt_gst_structure_new_empty(gst_structure_get_name(structure));

                    //add structure fields to distinguish between formats with similar mime types,
                    //like audio/mpeg
                    for (int j=0; j<gst_structure_n_fields(structure); j++) {
                        const gchar* fieldName = gst_structure_nth_field_name(structure, j);
                        if (fieldsToAdd.contains(fieldName)) {
                            const GValue *value = gst_structure_get_value(structure, fieldName);
                            GType valueType = G_VALUE_TYPE(value);

                            //don't add values of range type,
                            //gst_pb_utils_get_codec_description complains about not fixed caps

                            if (valueType != GST_TYPE_INT_RANGE && valueType != GST_TYPE_DOUBLE_RANGE &&
                                valueType != GST_TYPE_FRACTION_RANGE && valueType != GST_TYPE_LIST &&
                                valueType != GST_TYPE_ARRAY)
                                gst_structure_set_value(newStructure, fieldName, value);
                        }
                    }

                    GstCaps *newCaps = gst_caps_new_full(newStructure, nullptr);

                    gchar *capsString = gst_caps_to_string(newCaps);
                    QString codec = QLatin1String(capsString);
                    if (capsString)
                        g_free(capsString);
                    // skip stuff that's not really a known audio/video codec
                    if (codec.startsWith("application"))
                        continue;
                    GstRank rank = GstRank(gst_plugin_feature_get_rank(GST_PLUGIN_FEATURE(factory)));

                    // If two elements provide the same codec, use the highest ranked one
                    QMap<QString, CodecInfo>::const_iterator it = m_codecInfo.constFind(codec);
                    if (it == m_codecInfo.constEnd() || it->rank < rank) {
                        if (it == m_codecInfo.constEnd())
                            m_codecs.append(codec);

                        CodecInfo info;
                        info.elementName = gst_plugin_feature_get_name(GST_PLUGIN_FEATURE(factory));

                        gchar *description = gst_pb_utils_get_codec_description(newCaps);
                        info.description = QString::fromUtf8(description);
                        if (description)
                            g_free(description);

                        info.rank = rank;

                        m_codecInfo.insert(codec, info);
                    }

                    gst_caps_unref(newCaps);
                }
                gst_caps_unref(caps);
            }
        }
    }

    gst_plugin_feature_list_free(elements);
}

GList *QGstCodecsInfo::elementFactories(ElementType elementType) const
{
    GstElementFactoryListType gstElementType = 0;
    switch (elementType) {
    case AudioEncoder:
        gstElementType = GST_ELEMENT_FACTORY_TYPE_AUDIO_ENCODER;
        break;
    case VideoEncoder:
        // GST_ELEMENT_FACTORY_TYPE_VIDEO_ENCODER also lists image encoders. We don't want these here.
        gstElementType = (GstElementFactoryListType)(GST_ELEMENT_FACTORY_TYPE_ENCODER | GST_ELEMENT_FACTORY_TYPE_MEDIA_VIDEO);
        break;
    case Muxer:
        gstElementType = GST_ELEMENT_FACTORY_TYPE_MUXER;
        break;
    case AudioDecoder:
        gstElementType = (GstElementFactoryListType)(GST_ELEMENT_FACTORY_TYPE_DECODER | GST_ELEMENT_FACTORY_TYPE_MEDIA_AUDIO);
        break;
    case VideoDecoder:
        // GST_ELEMENT_FACTORY_TYPE_VIDEO_ENCODER also lists image encoders. We don't want these here.
        gstElementType = (GstElementFactoryListType)(GST_ELEMENT_FACTORY_TYPE_DECODER | GST_ELEMENT_FACTORY_TYPE_MEDIA_VIDEO);
        break;
    case Demuxer:
        gstElementType = GST_ELEMENT_FACTORY_TYPE_DEMUXER;
        break;
    }

    GList *list = gst_element_factory_list_get_elements(gstElementType, GST_RANK_MARGINAL);
    if (elementType == AudioEncoder) {
        // Manually add "audioconvert" to the list
        // to allow linking with various containers.
        auto factory = gst_element_factory_find("audioconvert");
        if (factory)
            list = g_list_prepend(list, factory);
    }

    return list;
}

QSet<QString> QGstCodecsInfo::supportedStreamTypes(const QString &codec) const
{
    return m_streamTypes.value(codec);
}

QStringList QGstCodecsInfo::supportedCodecs(const QSet<QString> &types) const
{
    QStringList result;
    for (auto &candidate : supportedCodecs()) {
        auto candidateTypes = supportedStreamTypes(candidate);
        if (candidateTypes.intersects(types))
            result << candidate;
    }

    return result;
}
