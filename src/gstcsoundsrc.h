/* GStreamer
 * Copyright (C) 2017 Natanael Mojica Jimenez  <neithanmo@gmail.com>
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

#ifndef _GST_CSOUNDSRC_H_
#define _GST_CSOUNDSRC_H_

#include <gst/gst.h>
#include <gst/base/gstbasesrc.h>
#include <gst/audio/audio.h>
#include <csound/csound.h>

G_BEGIN_DECLS
#define GST_TYPE_CSOUNDSRC   (gst_csoundsrc_get_type())
#define GST_CSOUNDSRC(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_CSOUNDSRC,GstCsoundsrc))
#define GST_CSOUNDSRC_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_CSOUNDSRC,GstCsoundsrcClass))
#define GST_IS_CSOUNDSRC(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_CSOUNDSRC))
#define GST_IS_CSOUNDSRC_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_CSOUNDSRC))
typedef struct _GstCsoundsrc GstCsoundsrc;
typedef struct _GstCsoundsrcClass GstCsoundsrcClass;

typedef void (*csoundMessageCallback) (CSOUND *, int attr, const char *format,
    va_list valist);

typedef void (*csoundsrcProcessFunc) (GstCsoundsrc *, MYFLT *);

struct _GstCsoundsrc
{
  GstBaseSrc base_csoundsrc;
  CSOUND *csound;
  gchar *csd_name;

  /* <private> */
  csoundsrcProcessFunc process;
  GstAudioFormatPack pack_func;
  gint pack_size;
  GstAudioInfo info;
  gint channels;

  MYFLT *csound_output;
  guint64 samples_to_generate;
  guint ksmps;

  GstClockTimeDiff timestamp_offset;
  GstClockTime next_time;       /* next timestamp */
  gint64 next_sample;           /* next sample to send */
  gint64 sample_stop;
  GMutex lock;
  gint end_of_score;

};


struct _GstCsoundsrcClass
{
  GstBaseSrcClass base_csoundsrc_class;
};

GType gst_csoundsrc_get_type (void);

G_END_DECLS
#endif
