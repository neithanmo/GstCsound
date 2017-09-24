/* GStreamer
 * Copyright (C) 2017 <neithanmo@gmail.com>
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

#ifndef _GST_CSOUNDSINK_H_
#define _GST_CSOUNDSINK_H_

#include <gst/audio/gstaudiosink.h>
#include <csound/csound.h>

G_BEGIN_DECLS
#define GST_TYPE_CSOUNDSINK   (gst_csoundsink_get_type())
#define GST_CSOUNDSINK(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_CSOUNDSINK,GstCsoundsink))
#define GST_CSOUNDSINK_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_CSOUNDSINK,GstCsoundsinkClass))
#define GST_IS_CSOUNDSINK(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_CSOUNDSINK))
#define GST_IS_CSOUNDSINK_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_CSOUNDSINK))
typedef struct _GstCsoundsink GstCsoundsink;
typedef struct _GstCsoundsinkClass GstCsoundsinkClass;
typedef void (*csoundMessageCallback) (CSOUND *, int attr, const char *format,
    va_list valist);

struct _GstCsoundsink
{
  GstAudioSink base_csoundsink;
  CSOUND *csound;
  gchar *csd_name;
  gint channels;
  gint bpf;

  MYFLT *csound_input;
  guint ksmps;
  GMutex lock;
  gint end_of_score;
};

struct _GstCsoundsinkClass
{
  GstAudioSinkClass base_csoundsink_class;
};

GType gst_csoundsink_get_type (void);

G_END_DECLS
#endif
