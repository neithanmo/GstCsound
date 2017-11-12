/* GStreamer
 * Copyright (C) 2017 FIXME <fixme@example.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef _GST_CSOUNDFILTER_H_
#define _GST_CSOUNDFILTER_H_

#include <gst/base/gstbasetransform.h>
#include <gst/audio/audio.h>
#include <csound/csound.h>

G_BEGIN_DECLS

#define GST_TYPE_CSOUNDFILTER   (gst_csoundfilter_get_type())
#define GST_CSOUNDFILTER(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_CSOUNDFILTER,GstCsoundfilter))
#define GST_CSOUNDFILTER_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_CSOUNDFILTER,GstCsoundfilterClass))
#define GST_IS_CSOUNDFILTER(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_CSOUNDFILTER))
#define GST_IS_CSOUNDFILTER_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_CSOUNDFILTER))

typedef struct _GstCsoundfilter GstCsoundfilter;
typedef struct _GstCsoundfilterClass GstCsoundfilterClass;

typedef void (*GstCsoundFilterProcessFunc) (GstCsoundfilter *, MYFLT * ,
    guint , guint );

typedef void (*csoundMessageCallback) (CSOUND *, int attr, const char *format,
    va_list valist);


struct _GstCsoundfilter
{
  GstBaseTransform base_csoundfilter;

  CSOUND *csound;
  gchar *csd_name;

  /* <private> */

  GstCsoundFilterProcessFunc process;
  GstAdapter *in_adapter;
  guint64 prev_offset;
  MYFLT *spin;
  MYFLT *spout;
  guint ksmps,
        cs_ochannels,
        cs_ichannels;
  gint16 end_score;

};

struct _GstCsoundfilterClass
{
  GstBaseTransformClass base_csoundfilter_class;
};

GType gst_csoundfilter_get_type (void);

GST_EXPORT
CSOUND *gst_csoundfilter_get_instance(GstCsoundfilter *csoundfilter);

G_END_DECLS

#endif
