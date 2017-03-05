/* GStreamer
 * Copyright (C) 2017 Natanael Mojica <neithanmo@gmail.com>
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
 * Free Software Foundation, Inc., 51 Franklin Street, Suite 500,
 * Boston, MA 02110-1335, USA.
 */
/**
 * SECTION:element-csoundsink
 * @see_also: #csoundsrc, #csoundfilter
 * @short_description: capture raw audio samples through Csound
 *
 * This element plays raw audio samples through Csound.
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-1.0 filesrc location=stream.wav ! decodebin ! audioconvert ! csoundsink
 * ]| will play a audio file through csound.
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/audio/gstaudiosink.h>
#include "gstcsoundsink.h"

GST_DEBUG_CATEGORY_STATIC (gst_csoundsink_debug_category);
#define GST_CAT_DEFAULT gst_csoundsink_debug_category

/* prototypes */


static void gst_csoundsink_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_csoundsink_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_csoundsink_dispose (GObject * object);
static void gst_csoundsink_finalize (GObject * object);

static gboolean gst_csoundsink_open (GstAudioSink * sink);
static gboolean gst_csoundsink_prepare (GstAudioSink * sink,
    GstAudioRingBufferSpec * spec);
static gboolean gst_csoundsink_unprepare (GstAudioSink * sink);
static gboolean gst_csoundsink_close (GstAudioSink * sink);
static gint gst_csoundsink_write (GstAudioSink * sink, gpointer data,
    guint length);
static guint gst_csoundsink_delay (GstAudioSink * sink);
static void gst_csoundsink_reset (GstAudioSink * sink);
static void gst_csoundsink_messages (CSOUND * csound, int attr,
    const char *format, va_list valist);

enum
{
  PROP_0,
  PROP_LOCATION
};

/* pad templates */

static GstStaticPadTemplate gst_csoundsink_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw,format=F64LE,rate=[1,max],"
        "channels=[1,max],layout=interleaved")
    );


/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstCsoundsink, gst_csoundsink, GST_TYPE_AUDIO_SINK,
    GST_DEBUG_CATEGORY_INIT (gst_csoundsink_debug_category, "csoundsink", 0,
        "debug category for csoundsink element"));

static void
gst_csoundsink_class_init (GstCsoundsinkClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstAudioSinkClass *audio_sink_class = GST_AUDIO_SINK_CLASS (klass);

  /* Setting up pads and setting metadata should be moved to
     base_class_init if you intend to subclass this class. */
  gst_element_class_add_static_pad_template (GST_ELEMENT_CLASS (klass),
      &gst_csoundsink_sink_template);

  gobject_class->set_property = gst_csoundsink_set_property;
  gobject_class->get_property = gst_csoundsink_get_property;

  g_object_class_install_property (gobject_class, PROP_LOCATION,
      g_param_spec_string ("location", "Location",
          "Location of the csd file used for csound", NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "Csound audio sink", "Sink/audio",
      "Output audio to csound", "Natanael Mojica <neithanmo@gmail.com>");

  gobject_class->set_property = gst_csoundsink_set_property;
  gobject_class->get_property = gst_csoundsink_get_property;
  gobject_class->dispose = gst_csoundsink_dispose;
  gobject_class->finalize = gst_csoundsink_finalize;
  audio_sink_class->open = GST_DEBUG_FUNCPTR (gst_csoundsink_open);
  audio_sink_class->prepare = GST_DEBUG_FUNCPTR (gst_csoundsink_prepare);
  audio_sink_class->unprepare = GST_DEBUG_FUNCPTR (gst_csoundsink_unprepare);
  audio_sink_class->close = GST_DEBUG_FUNCPTR (gst_csoundsink_close);
  audio_sink_class->write = GST_DEBUG_FUNCPTR (gst_csoundsink_write);
  audio_sink_class->delay = GST_DEBUG_FUNCPTR (gst_csoundsink_delay);
  audio_sink_class->reset = GST_DEBUG_FUNCPTR (gst_csoundsink_reset);

}

static void
gst_csoundsink_init (GstCsoundsink * csoundsink)
{
}

void
gst_csoundsink_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstCsoundsink *csoundsink = GST_CSOUNDSINK (object);

  GST_DEBUG_OBJECT (csoundsink, "set_property");

  switch (property_id) {
    case PROP_LOCATION:
      csoundsink->csd_name = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_csoundsink_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstCsoundsink *csoundsink = GST_CSOUNDSINK (object);

  GST_DEBUG_OBJECT (csoundsink, "get_property");

  switch (property_id) {
    case PROP_LOCATION:
      g_value_set_string (value, csoundsink->csd_name);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_csoundsink_dispose (GObject * object)
{
  GstCsoundsink *csoundsink = GST_CSOUNDSINK (object);

  GST_DEBUG_OBJECT (csoundsink, "dispose");

  /* clean up as possible.  may be called multiple times */

  G_OBJECT_CLASS (gst_csoundsink_parent_class)->dispose (object);
}

void
gst_csoundsink_finalize (GObject * object)
{
  GstCsoundsink *csoundsink = GST_CSOUNDSINK (object);

  GST_DEBUG_OBJECT (csoundsink, "finalize");
  if (csoundsink->csound) {
    csoundCleanup (csoundsink->csound);
    csoundDestroy (csoundsink->csound);
  }

  /* clean up object here */

  G_OBJECT_CLASS (gst_csoundsink_parent_class)->finalize (object);
}


static gboolean
gst_csoundsink_open (GstAudioSink * sink)
{
  GstCsoundsink *csoundsink = GST_CSOUNDSINK (sink);
  csoundsink->csound = csoundCreate (NULL);
  csoundSetMessageCallback (csoundsink->csound,
      (csoundMessageCallback) gst_csoundsink_messages);
  GST_DEBUG_OBJECT (csoundsink, "open");

  return TRUE;
}


static gboolean
gst_csoundsink_prepare (GstAudioSink * sink, GstAudioRingBufferSpec * spec)
{
  GstCsoundsink *csoundsink = GST_CSOUNDSINK (sink);
  int result = csoundCompileCsd (csoundsink->csound, csoundsink->csd_name);
  if (result) {
    GST_ELEMENT_ERROR (csoundsink, RESOURCE, OPEN_READ,
        ("%s", csoundsink->csd_name), NULL);
    return FALSE;
  }
  csoundsink->ksmps = csoundGetKsmps (csoundsink->csound);
  csoundsink->channels = csoundGetNchnlsInput (csoundsink->csound);
  csoundsink->bpf = GST_AUDIO_INFO_BPF (&spec->info);
  int rate = GST_AUDIO_INFO_RATE (&spec->info);
  if (csoundsink->ksmps % 2 != 0) {
    GST_WARNING_OBJECT (csoundsink, "csound ksmps is not a power-of-two");
  }
  csoundStart (csoundsink->csound);

  GST_DEBUG_OBJECT (csoundsink, "prepare");
  spec->segsize = sizeof (gdouble) * csoundsink->channels * csoundsink->ksmps;
  spec->latency_time = gst_util_uint64_scale (spec->segsize,
      (GST_SECOND / GST_USECOND), rate * csoundsink->bpf);
  spec->segtotal = spec->buffer_time / spec->latency_time;

  GST_DEBUG_OBJECT (csoundsink, "buffer time: %" G_GINT64_FORMAT " usec",
      spec->buffer_time);
  GST_DEBUG_OBJECT (csoundsink, "latency time: %" G_GINT64_FORMAT " usec",
      spec->latency_time);
  GST_DEBUG_OBJECT (csoundsink, "buffer_size %d, segsize %d, segtotal %d",
      csoundsink->ksmps, spec->segsize, spec->segtotal);
  return TRUE;
}


static gboolean
gst_csoundsink_unprepare (GstAudioSink * sink)
{
  GstCsoundsink *csoundsink = GST_CSOUNDSINK (sink);

  GST_DEBUG_OBJECT (csoundsink, "unprepare");

  return TRUE;
}


static gboolean
gst_csoundsink_close (GstAudioSink * sink)
{
  GstCsoundsink *csoundsink = GST_CSOUNDSINK (sink);
  csoundStop (csoundsink->csound);
  GST_DEBUG_OBJECT (csoundsink, "close");

  return TRUE;
}


static gint
gst_csoundsink_write (GstAudioSink * sink, gpointer data, guint length)
{
  GstCsoundsink *csoundsink = GST_CSOUNDSINK (sink);
  csoundsink->csound_input = csoundGetSpin (csoundsink->csound);
  memcpy (csoundsink->csound_input, data, length);
  gint ret = csoundPerformKsmps (csoundsink->csound);
  if (ret) {
    GST_ELEMENT_ERROR (csoundsink, RESOURCE, WRITE,
        ("Score finished in csoundPerformKsmps()"), NULL);
    return 0;
  }
  return length;
}


static guint
gst_csoundsink_delay (GstAudioSink * sink)
{
  GstCsoundsink *csoundsink = GST_CSOUNDSINK (sink);

  GST_DEBUG_OBJECT (csoundsink, "delay");

  return 0;
}


static void
gst_csoundsink_reset (GstAudioSink * sink)
{
  GstCsoundsink *csoundsink = GST_CSOUNDSINK (sink);
  csoundReset (csoundsink->csound);
  GST_DEBUG_OBJECT (csoundsink, "reset");

}

static void
gst_csoundsink_messages (CSOUND * csound, int attr, const char *format,
    va_list valist)
{
  char **result = NULL;
  vasprintf (&result, format, valist);
  switch (attr) {
    case CSOUNDMSG_ERROR:
      GST_WARNING (result);
      break;
    case CSOUNDMSG_WARNING:
      GST_WARNING (result);
      break;
    case CSOUNDMSG_ORCH:
      GST_INFO (result);
      break;
    case CSOUNDMSG_REALTIME:
      GST_LOG (result);
      break;
    case CSOUNDMSG_DEFAULT:
      GST_LOG (result);
      break;
    default:
      GST_LOG (result);
      break;
  }
  g_free (result);

}
