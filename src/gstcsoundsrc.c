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
 * SECTION:element-gstcsoundsrc
 *
 * input audio data through csound engine.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-1.0 -v csoundsrc location=hello.csd ! audioconvert ! autoaudiosink
 * ]|
 * 
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/base/gstbasesrc.h>
#include "gstcsoundsrc.h"


#define ALLOWED_CAPS \
    "audio/x-raw,"                                                 \
    " format=(string){"GST_AUDIO_NE(F32)","GST_AUDIO_NE(F64)"}," \
    " rate=(int)[1,MAX],"                                          \
    " channels=(int)[1,MAX],"                                      \
    " layout=(string) interleaved"

#define DEFAULT_SAMPLES_PER_BUFFER   1024
#define DEFAULT_IS_LIVE              FALSE
#define DEFAULT_TIMESTAMP_OFFSET     G_GINT64_CONSTANT (0)

GST_DEBUG_CATEGORY_STATIC (gst_csoundsrc_debug_category);
#define GST_CAT_DEFAULT gst_csoundsrc_debug_category

/* prototypes */


static void gst_csoundsrc_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_csoundsrc_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_csoundsrc_dispose (GObject * object);
static void gst_csoundsrc_finalize (GObject * object);

/*virtual functions */
static GstCaps *gst_csoundsrc_fixate (GstBaseSrc * src, GstCaps * caps);
static gboolean gst_csoundsrc_set_caps (GstBaseSrc * src, GstCaps * caps);
static gboolean gst_csoundsrc_start (GstBaseSrc * src);
static gboolean gst_csoundsrc_stop (GstBaseSrc * src);
static void gst_csoundsrc_get_times (GstBaseSrc * src, GstBuffer * buffer,
    GstClockTime * start, GstClockTime * end);
static gboolean gst_csoundsrc_is_seekable (GstBaseSrc * src);
static GstFlowReturn gst_csoundsrc_fill (GstBaseSrc * src, guint64 offset,
    guint size, GstBuffer * buf);
static void gst_csoundsrc_get_csamples(GstCsoundsrc * csoundsrc,
    MYFLT * data);
static void gst_csoundsrc_messages (CSOUND * csound, int attr,
    const char *format, va_list valist);

enum
{
  PROP_0,
  PROP_LOCATION,
  PROP_IS_LIVE,
  PROP_TIMESTAMP_OFFSET
};

static GstStaticPadTemplate gst_csoundsrc_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (ALLOWED_CAPS)
    );

/* class initialization */
G_DEFINE_TYPE_WITH_CODE (GstCsoundsrc, gst_csoundsrc, GST_TYPE_BASE_SRC,
    GST_DEBUG_CATEGORY_INIT (gst_csoundsrc_debug_category, "csoundsrc", 0,
        "debug category for csoundsrc element"));

static void
gst_csoundsrc_class_init (GstCsoundsrcClass * klass)
{
  GObjectClass *gobject_class;
  GstBaseSrcClass *base_src_class;
  gobject_class = (GObjectClass *) klass;

  base_src_class = (GstBaseSrcClass *) klass;

  gst_element_class_add_static_pad_template (GST_ELEMENT_CLASS (klass),
      &gst_csoundsrc_src_template);

  gobject_class->set_property = gst_csoundsrc_set_property;
  gobject_class->get_property = gst_csoundsrc_get_property;

  g_object_class_install_property (gobject_class, PROP_LOCATION,
      g_param_spec_string ("location", "Location",
          "Location of the csd file used for csound", NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_IS_LIVE,
      g_param_spec_boolean ("is-live", "Is Live",
          "Whether to act as a live source", DEFAULT_IS_LIVE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_TIMESTAMP_OFFSET, g_param_spec_int64 ("timestamp-offset",
          "Timestamp offset",
          "An offset added to timestamps set on buffers (in ns)", G_MININT64,
          G_MAXINT64, DEFAULT_TIMESTAMP_OFFSET,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "Csound audio source", "Source/audio",
      "Input audio through Csound", "Natanael Mojica <neithanmo@gmail.com>");

  gobject_class->dispose = GST_DEBUG_FUNCPTR (gst_csoundsrc_dispose);
  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_csoundsrc_finalize);
  base_src_class->fixate = GST_DEBUG_FUNCPTR (gst_csoundsrc_fixate);
  base_src_class->set_caps = GST_DEBUG_FUNCPTR (gst_csoundsrc_set_caps);
  base_src_class->start = GST_DEBUG_FUNCPTR (gst_csoundsrc_start);
  base_src_class->stop = GST_DEBUG_FUNCPTR (gst_csoundsrc_stop);
  base_src_class->get_times = GST_DEBUG_FUNCPTR (gst_csoundsrc_get_times);
  base_src_class->is_seekable = GST_DEBUG_FUNCPTR (gst_csoundsrc_is_seekable);
  base_src_class->fill = GST_DEBUG_FUNCPTR (gst_csoundsrc_fill);

}

static void
gst_csoundsrc_init (GstCsoundsrc * csoundsrc)
{
  gst_base_src_set_format (GST_BASE_SRC (csoundsrc), GST_FORMAT_TIME);
  gst_base_src_set_live (GST_BASE_SRC (csoundsrc), DEFAULT_IS_LIVE);
  gst_base_src_set_blocksize (GST_BASE_SRC (csoundsrc), -1);

  csoundsrc->process = (csoundsrcProcessFunc) gst_csoundsrc_get_csamples;
  csoundsrc->timestamp_offset = DEFAULT_TIMESTAMP_OFFSET;
}

void
gst_csoundsrc_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstCsoundsrc *csoundsrc = GST_CSOUNDSRC (object);

  GST_DEBUG_OBJECT (csoundsrc, "set_property");
  switch (property_id) {
    case PROP_LOCATION:
      csoundsrc->csd_name = g_value_dup_string (value);
      break;
    case PROP_IS_LIVE:
      gst_base_src_set_live (GST_BASE_SRC (csoundsrc),
          g_value_get_boolean (value));
      break;
    case PROP_TIMESTAMP_OFFSET:
      csoundsrc->timestamp_offset = g_value_get_int64 (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_csoundsrc_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstCsoundsrc *csoundsrc = GST_CSOUNDSRC (object);

  GST_DEBUG_OBJECT (csoundsrc, "get_property");

  switch (property_id) {
    case PROP_LOCATION:
      g_value_set_string (value, csoundsrc->csd_name);
      break;
    case PROP_IS_LIVE:
      g_value_set_boolean (value,
          gst_base_src_is_live (GST_BASE_SRC (csoundsrc)));
      break;
    case PROP_TIMESTAMP_OFFSET:
      g_value_set_int64 (value, csoundsrc->timestamp_offset);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_csoundsrc_dispose (GObject * object)
{
  GstCsoundsrc *csoundsrc = GST_CSOUNDSRC (object);
  csoundsrc->csound_output = NULL;

  GST_DEBUG_OBJECT (csoundsrc, "dispose");

  /* clean up as possible.  may be called multiple times */
  G_OBJECT_CLASS (gst_csoundsrc_parent_class)->dispose (object);
}

void
gst_csoundsrc_finalize (GObject * object)
{
  GstCsoundsrc *csoundsrc = GST_CSOUNDSRC (object);

  GST_DEBUG_OBJECT (csoundsrc, "finalize");
  if (csoundsrc->csound) {
    csoundStop (csoundsrc->csound);
    csoundCleanup (csoundsrc->csound);
    csoundDestroy (csoundsrc->csound);
  }
  G_OBJECT_CLASS (gst_csoundsrc_parent_class)->finalize (object);
}


/* decide on caps */

/* called if, in negotiation, caps need fixating */
static GstCaps *
gst_csoundsrc_fixate (GstBaseSrc * src, GstCaps * caps)
{
  GstCsoundsrc *csoundsrc = GST_CSOUNDSRC (src);

  GstStructure *structure;
  gint rate;

  gint caps_channels;
  caps = gst_caps_make_writable (caps);

  structure = gst_caps_get_structure (caps, 0);

  GST_DEBUG_OBJECT (src, "fixating samplerate to %d", GST_AUDIO_DEF_RATE);

  rate = csoundGetSr (csoundsrc->csound);
  gst_structure_fixate_field_nearest_int (structure, "rate", rate);

  if (csoundGetSizeOfMYFLT () == 8) {
    GST_INFO_OBJECT (csoundsrc,
        "csound only support F64 audio samples - fixed caps");
    gst_structure_set (structure, "format", G_TYPE_STRING, GST_AUDIO_NE (F64),
        NULL);
  }

  else if (csoundGetSizeOfMYFLT () == 4) {
    GST_INFO_OBJECT (src, "csound only support F32 audio samples - fixed caps");
    gst_structure_set (structure, "format", G_TYPE_STRING, GST_AUDIO_NE (F32),
        NULL);
  }

  /* fixate to channels setting in csound side */
  gst_structure_set (structure, "channels", G_TYPE_INT, csoundsrc->channels,
      NULL);

  if (gst_structure_get_int (structure, "channels", &caps_channels)
      && caps_channels > 2) {
    if (!gst_structure_has_field_typed (structure, "channel-mask",
            GST_TYPE_BITMASK))
      gst_structure_set (structure, "channel-mask", GST_TYPE_BITMASK, 0ULL,
          NULL);
  }

  caps = GST_BASE_SRC_CLASS (gst_csoundsrc_parent_class)->fixate (src, caps);

  return caps;
}

/* notify the subclass of new caps */
static gboolean
gst_csoundsrc_set_caps (GstBaseSrc * src, GstCaps * caps)
{
  GstCsoundsrc *csoundsrc = GST_CSOUNDSRC (src);
  GstAudioInfo info;
  GST_DEBUG_OBJECT (src, "setting caps");
  if (!gst_audio_info_from_caps (&info, caps))
    goto invalid_caps;

  GST_DEBUG_OBJECT (csoundsrc, "negotiated to caps %" GST_PTR_FORMAT, caps);

  csoundsrc->info = info;

  gst_base_src_set_blocksize (src, GST_AUDIO_INFO_BPF (&info) * csoundsrc->ksmps * csoundsrc->channels);
  return TRUE;

  /* ERROR */
invalid_caps:
  {
    GST_ERROR_OBJECT (csoundsrc, "received invalid caps");
    return FALSE;
  }

  return TRUE;
}


/* start and stop processing, ideal for opening/closing the resource */
static gboolean
gst_csoundsrc_start (GstBaseSrc * src)
{
  GstCsoundsrc *csoundsrc = GST_CSOUNDSRC (src);
  csoundsrc->csound = csoundCreate (NULL);
  csoundSetMessageCallback (csoundsrc->csound,
      (csoundMessageCallback) gst_csoundsrc_messages);
  int result = csoundCompileCsd (csoundsrc->csound, csoundsrc->csd_name);

  if (result) {
    GST_ELEMENT_ERROR (csoundsrc, RESOURCE, OPEN_READ,
        ("%s", csoundsrc->csd_name), (NULL));
    return FALSE;
  }
  csoundsrc->ksmps = csoundGetKsmps (csoundsrc->csound);
  if (csoundsrc->ksmps % 2 != 0) {
    GST_WARNING_OBJECT (csoundsrc, "csound ksmps is not a power-of-two");
  }
  csoundsrc->channels = csoundGetNchnls (csoundsrc->csound);
  GST_DEBUG_OBJECT (csoundsrc, "ksmps: %d , channels: %d", csoundsrc->ksmps,
      csoundsrc->channels);
  csoundsrc->next_sample = 0;
  csoundsrc->next_time = 0;
  csoundStart (csoundsrc->csound);
  
  csoundsrc->csound_output = csoundGetSpout (csoundsrc->csound);
  GST_DEBUG_OBJECT (csoundsrc, "start");

  return TRUE;
}

static gboolean
gst_csoundsrc_stop (GstBaseSrc * src)
{
  GstCsoundsrc *csoundsrc = GST_CSOUNDSRC (src);

  csoundStop (csoundsrc->csound);

  GST_DEBUG_OBJECT (csoundsrc, "stop");

  return TRUE;
}

/* given a buffer, return start and stop time when it should be pushed
 * out. The base class will sync on the clock using these times. */
static void
gst_csoundsrc_get_times (GstBaseSrc * src, GstBuffer * buffer,
    GstClockTime * start, GstClockTime * end)
{
  if (gst_base_src_is_live (src)) {
    GstClockTime timestamp = GST_BUFFER_TIMESTAMP (buffer);

    if (GST_CLOCK_TIME_IS_VALID (timestamp)) {
      /* get duration to calculate end time */
      GstClockTime duration = GST_BUFFER_DURATION (buffer);

      if (GST_CLOCK_TIME_IS_VALID (duration)) {
        *end = timestamp + duration;
      }
      *start = timestamp;
    }
  } else {
    *start = -1;
    *end = -1;
  }

}

/* check if the resource is seekable */
static gboolean
gst_csoundsrc_is_seekable (GstBaseSrc * src)
{
  // not seekeable !!
  return FALSE;
}

/* ask the subclass to fill the buffer with data from offset and size */
static GstFlowReturn
gst_csoundsrc_fill (GstBaseSrc * basesrc, guint64 offset,
    guint length, GstBuffer * buffer)
{
    
  GstCsoundsrc *csoundsrc = GST_CSOUNDSRC (basesrc);

  GstClockTime next_time;
  gint64 next_sample;
  gint bytes, samples;
  GstMapInfo map;
  gint samplerate, bpf;

  g_mutex_lock (&csoundsrc->lock);

  if (csoundsrc->end_of_score) {
    GST_INFO_OBJECT (csoundsrc, "eos");
    return GST_FLOW_EOS;
  }

  samplerate = GST_AUDIO_INFO_RATE (&csoundsrc->info);
  bpf = GST_AUDIO_INFO_BPF (&csoundsrc->info);

  if (length == -1)
    samples = csoundsrc->ksmps * csoundsrc->channels;
  else
    samples = length / bpf;
  
  csoundsrc->samples_to_generate = samples;

  next_sample = csoundsrc->next_sample + samples;
  bytes = csoundsrc->samples_to_generate * bpf;
  next_time = gst_util_uint64_scale_int (next_sample, GST_SECOND, samplerate);
  
  gst_buffer_set_size (buffer, bytes);
  GST_BUFFER_TIMESTAMP (buffer) = csoundsrc->timestamp_offset + next_time;
  GST_BUFFER_DURATION (buffer) = csoundsrc->next_time - next_time;

  gst_object_sync_values (GST_OBJECT (csoundsrc),
      GST_BUFFER_TIMESTAMP (buffer));

  csoundsrc->next_time = next_time;
  csoundsrc->next_sample = next_sample;

  GST_LOG_OBJECT (csoundsrc, "generating %lu samples at ts %" GST_TIME_FORMAT,
      csoundsrc->samples_to_generate, GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buffer)));
  
  gst_buffer_map (buffer, &map, GST_MAP_READWRITE);
  csoundsrc->process (csoundsrc, map.data);
  MYFLT *data = (MYFLT *) map.data;
  /* ouput scaling */
  gdouble scale = (1.0 / 32767.0);
  
  for (gint i = 0; i < csoundsrc->samples_to_generate * csoundsrc->channels; i++) {
    *data++ *= scale;
  }
  
  gst_buffer_unmap (buffer, &map);
  g_mutex_unlock (&csoundsrc->lock);
  
  return GST_FLOW_OK;
}

static void
gst_csoundsrc_get_csamples (GstCsoundsrc * csoundsrc, MYFLT * data)
{
  guint bytes_to_move = csoundsrc->ksmps * sizeof (data) * csoundsrc->channels;
  guint loops_to_fill = csoundsrc->samples_to_generate / (csoundsrc->ksmps);
  for (gint i = 0; i < loops_to_fill; i++) {
    memcpy (data, csoundsrc->csound_output, bytes_to_move);
    csoundsrc->end_of_score = csoundPerformKsmps (csoundsrc->csound);
    data += csoundsrc->ksmps * csoundsrc->channels;
  }
}

/*callback for std-out messages*/
static void
gst_csoundsrc_messages (CSOUND * csound, int attr, const char *format,
    va_list valist)
{
  char *result = NULL;
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
