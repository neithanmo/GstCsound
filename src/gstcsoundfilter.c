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
 * SECTION:element-csoundfilter
 *
 * Inplement a audio filter and/or audio effects using Csound.
 *
 * The procedures are in the csound csd file. We recomended to set a ksmps low in your
 * csd file.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-1.0 -v audiotestsrc ! audioconvert ! csoundfilter location=user.csd ! audioconvert ! fakesink
 * ]|
 *
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include "gstcsoundfilter.h"

GST_DEBUG_CATEGORY_STATIC (gst_csoundfilter_debug_category);
#define GST_CAT_DEFAULT gst_csoundfilter_debug_category

#define FLOAT_SAMPLES 4
#define DOUBLE_SAMPLES 8

#define DEFAULT_LOOP                 FALSE

/* prototypes */
static void gst_csoundfilter_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_csoundfilter_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_csoundfilter_dispose (GObject * object);
static void gst_csoundfilter_finalize (GObject * object);

static GstCaps *gst_csoundfilter_transform_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter);
static GstCaps *gst_csoundfilter_fixate_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * othercaps);
static gboolean gst_csoundfilter_accept_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps);
static gboolean gst_csoundfilter_set_caps (GstBaseTransform * trans,
    GstCaps * incaps, GstCaps * outcaps);

static gboolean gst_csoundfilter_start (GstBaseTransform * trans);
static gboolean gst_csoundfilter_stop (GstBaseTransform * trans);

static GstFlowReturn gst_csoundfilter_transform (GstBaseTransform * trans,
    GstBuffer * inbuf, GstBuffer * outbuf);

static GstFlowReturn gst_csoundfilter_prepare_output_buffer (GstBaseTransform * base,
    GstBuffer * inbuf, GstBuffer ** outbuf);

static void gst_csoundfilter_messages (CSOUND * csound, int attr, const char *format,
    va_list valist);

static void
gst_csoundfilter_trans (GstCsoundfilter * csoundfilter,
    MYFLT * odata, guint in_bytes, guint out_bytes);


enum
{
  PROP_0,
  PROP_LOCATION,
  PROP_LOOP
};

#define ALLOWED_CAPS \
    "audio/x-raw,"                                                 \
    " format=(string){"GST_AUDIO_NE(F32)","GST_AUDIO_NE(F64)"}," \
    " rate=(int)[1,MAX],"                                          \
    " channels=(int)[1,MAX],"                                      \
    " layout=(string) interleaved"

/* pad templates */
static GstStaticPadTemplate gst_csoundfilter_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (ALLOWED_CAPS)
    );

static GstStaticPadTemplate gst_csoundfilter_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (ALLOWED_CAPS)
    );


/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstCsoundfilter, gst_csoundfilter, GST_TYPE_BASE_TRANSFORM,
  GST_DEBUG_CATEGORY_INIT (gst_csoundfilter_debug_category, "csoundfilter", 0,
  "debug category for csoundfilter element"));

static void
gst_csoundfilter_class_init (GstCsoundfilterClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstBaseTransformClass *base_transform_class = GST_BASE_TRANSFORM_CLASS (klass);

  /* Setting up pads and setting metadata should be moved to
     base_class_init if you intend to subclass this class. */
  gst_element_class_add_static_pad_template (GST_ELEMENT_CLASS (klass),
      &gst_csoundfilter_src_template);
  gst_element_class_add_static_pad_template (GST_ELEMENT_CLASS (klass),
      &gst_csoundfilter_sink_template);

  gobject_class->set_property = gst_csoundfilter_set_property;
  gobject_class->get_property = gst_csoundfilter_get_property;

  g_object_class_install_property (gobject_class, PROP_LOCATION,
      g_param_spec_string ("location", "Location",
          "Location of the csd file used by csound", NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

   g_object_class_install_property (gobject_class, PROP_LOOP,
      g_param_spec_boolean ("loop", "Loop",
           "do a loop on the score", DEFAULT_LOOP,
            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "using csound for audio processing", "Filter/Effect/Audio",
      "Inplement a audio filter/effects using csound",
      "Natanael Mojica <neithanmo@gmail.com>");

  gobject_class->dispose = gst_csoundfilter_dispose;
  gobject_class->finalize = gst_csoundfilter_finalize;
  base_transform_class->transform_caps = GST_DEBUG_FUNCPTR (gst_csoundfilter_transform_caps);
  base_transform_class->fixate_caps = GST_DEBUG_FUNCPTR (gst_csoundfilter_fixate_caps);
  base_transform_class->accept_caps = GST_DEBUG_FUNCPTR (gst_csoundfilter_accept_caps);
  base_transform_class->set_caps = GST_DEBUG_FUNCPTR (gst_csoundfilter_set_caps);
  base_transform_class->start = GST_DEBUG_FUNCPTR (gst_csoundfilter_start);
  base_transform_class->transform = GST_DEBUG_FUNCPTR (gst_csoundfilter_transform);
  base_transform_class->stop = GST_DEBUG_FUNCPTR (gst_csoundfilter_stop);
  base_transform_class->prepare_output_buffer = GST_DEBUG_FUNCPTR (gst_csoundfilter_prepare_output_buffer);
  base_transform_class->transform_ip_on_passthrough = FALSE;

}

static void
gst_csoundfilter_init (GstCsoundfilter *csoundfilter)
{
  gst_base_transform_set_in_place (GST_BASE_TRANSFORM (csoundfilter), FALSE);
}

void
gst_csoundfilter_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstCsoundfilter *csoundfilter = GST_CSOUNDFILTER (object);

  switch (property_id) {
    case PROP_LOCATION:
      csoundfilter->csd_name = g_value_dup_string (value);
      break;
    case PROP_LOOP:
      csoundfilter->loop = g_value_get_boolean (value);
    break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (csoundfilter, property_id, pspec);
      break;
  }
}

void
gst_csoundfilter_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstCsoundfilter *csoundfilter = GST_CSOUNDFILTER (object);

  switch (property_id) {
    case PROP_LOCATION:
      g_value_set_string (value, csoundfilter->csd_name);
      break;
    case PROP_LOOP:
      g_value_set_boolean (value, csoundfilter->loop);
    break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (csoundfilter, property_id, pspec);
      break;
  }
}

void
gst_csoundfilter_dispose (GObject * object)
{
  GstCsoundfilter *csoundfilter = GST_CSOUNDFILTER (object);

  G_OBJECT_CLASS (gst_csoundfilter_parent_class)->dispose (object);
}

void
gst_csoundfilter_finalize (GObject * object)
{
  GstCsoundfilter *csoundfilter = GST_CSOUNDFILTER (object);
  
  csoundfilter->spout = NULL;
  csoundfilter->spin = NULL;
  
  if(csoundfilter->csound){
     csoundCleanup (csoundfilter->csound);
     csoundDestroy (csoundfilter->csound);
  }
  
  g_object_unref(csoundfilter->in_adapter);
  csoundfilter->in_adapter = NULL;
  G_OBJECT_CLASS (gst_csoundfilter_parent_class)->finalize (object);
}

static GstCaps *
gst_csoundfilter_transform_caps (GstBaseTransform * base, GstPadDirection direction,
    GstCaps * caps, GstCaps * filter)
{

  GstCaps *res;
  GstStructure *structure;
  gint i;
  GST_DEBUG_OBJECT(GST_CSOUNDFILTER(base), "transform caps");
  /*check if audio format is supported by csound
   if not, fixing the caps and its audio format */
  res = gst_caps_copy (caps);

  if(csoundGetSizeOfMYFLT() == DOUBLE_SAMPLES){
  for (i = 0; i < gst_caps_get_size (res); i++) {
    structure = gst_caps_get_structure (res, i);
    if (direction == GST_PAD_SRC) {
      gst_structure_set (structure, "format", G_TYPE_STRING, GST_AUDIO_NE (F64), NULL);
    } else {
      gst_structure_set (structure, "format", G_TYPE_STRING, GST_AUDIO_NE (F64), NULL);
     }
    }
   }

  else  if(csoundGetSizeOfMYFLT() == FLOAT_SAMPLES){
  for (i = 0; i < gst_caps_get_size (res); i++) {
    structure = gst_caps_get_structure (res, i);
    if (direction == GST_PAD_SRC) {
      gst_structure_set (structure, "format", G_TYPE_STRING, GST_AUDIO_NE (F32), NULL);
    } else {
      gst_structure_set (structure, "format", G_TYPE_STRING, GST_AUDIO_NE (F32), NULL);
     }
   }
 }

  if (filter) {
    GstCaps *intersection;
    intersection = gst_caps_intersect_full (filter, res, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (res);
    res = intersection;
    GST_DEBUG_OBJECT (base, "Intersection %" GST_PTR_FORMAT, res);
  }

  return res;

}

static GstCaps *
gst_csoundfilter_fixate_caps (GstBaseTransform * trans, GstPadDirection direction,
    GstCaps * caps, GstCaps * othercaps)
{
  GstCsoundfilter *csoundfilter = GST_CSOUNDFILTER (trans);

  GstStructure *structure;
  gint rate;
  gint caps_channels;
  caps = gst_caps_make_writable (caps);
  structure = gst_caps_get_structure (caps, 0);
  rate = csoundGetSr (csoundfilter->csound);
  gst_structure_fixate_field_nearest_int (structure, "rate", rate);
  GST_DEBUG_OBJECT (csoundfilter, "fixating samplerate to %d", rate);
  /* fixate to channels setting in csound side */
  gst_structure_get_int (structure, "channels", &caps_channels);

  if (direction == GST_PAD_SRC) {
    gst_structure_set (structure, "channels", G_TYPE_INT, csoundfilter->cs_ichannels, NULL);
  } else if (direction == GST_PAD_SINK) {
      gst_structure_set (structure, "channels", G_TYPE_INT, csoundfilter->cs_ochannels, NULL);
  }

  if (caps_channels && caps_channels > 2) {
    if (!gst_structure_has_field_typed (structure, "channel-mask",
            GST_TYPE_BITMASK))
      gst_structure_set (structure, "channel-mask", GST_TYPE_BITMASK, 0ULL,
          NULL);
  }

  return caps;
}

static gboolean
gst_csoundfilter_accept_caps (GstBaseTransform * trans, GstPadDirection direction,
    GstCaps * caps)
{
  GstCsoundfilter *csoundfilter = GST_CSOUNDFILTER (trans);
  return TRUE;
}

static gboolean
gst_csoundfilter_set_caps (GstBaseTransform * trans, GstCaps * incaps,
    GstCaps * outcaps)
{
  GstCsoundfilter *csoundfilter = GST_CSOUNDFILTER (trans);
  GST_DEBUG_OBJECT (csoundfilter, "csoundfilter input caps configured  to: %" GST_PTR_FORMAT, incaps);
  GST_DEBUG_OBJECT (csoundfilter, "csoundfilter ouput caps configured  to: %" GST_PTR_FORMAT, outcaps);
  return TRUE;
}

/* states */
static gboolean
gst_csoundfilter_start (GstBaseTransform * trans)
{

  GstCsoundfilter *csoundfilter = GST_CSOUNDFILTER (trans);

  gboolean ret = TRUE;
  csoundfilter->csound = csoundCreate (NULL);
  csoundfilter->in_adapter = gst_adapter_new();
  csoundSetMessageCallback (csoundfilter->csound,
      (csoundMessageCallback) gst_csoundfilter_messages);
  int result = csoundCompileCsd (csoundfilter->csound, csoundfilter->csd_name);
  csoundStart(csoundfilter->csound);
  csoundfilter->spin = csoundGetSpin (csoundfilter->csound);
  csoundfilter->spout = csoundGetSpout (csoundfilter->csound);

  if (result) {
    GST_ELEMENT_ERROR (csoundfilter, RESOURCE, OPEN_READ,
        ("%s", csoundfilter->csd_name), (NULL));
    ret = FALSE;
  }

  csoundfilter->ksmps = csoundGetKsmps (csoundfilter->csound);
  csoundfilter->cs_ochannels = csoundGetNchnls (csoundfilter->csound);
  csoundfilter->cs_ichannels = csoundGetNchnlsInput (csoundfilter->csound);
  csoundfilter->process = gst_csoundfilter_trans;
  return ret;
}

static gboolean
gst_csoundfilter_stop (GstBaseTransform * trans)
{
  GstCsoundfilter *csoundfilter = GST_CSOUNDFILTER (trans);
  csoundStop (csoundfilter->csound);
  return TRUE;
}

static GstFlowReturn
gst_csoundfilter_prepare_output_buffer (GstBaseTransform * base,
    GstBuffer * inbuf, GstBuffer ** outbuf)
{
  GstCsoundfilter *csoundfilter = GST_CSOUNDFILTER (base);
  gsize new_size;

  if (csoundfilter->cs_ichannels != csoundfilter->cs_ochannels){
    guint input_bpf = csoundfilter->cs_ichannels * sizeof(MYFLT);
    guint num_samples = gst_buffer_get_size (inbuf)/input_bpf;
    new_size = num_samples * sizeof(MYFLT) * csoundfilter->cs_ochannels ;
  }else{
    new_size = gst_buffer_get_size (inbuf);
  }
  
  *outbuf = gst_buffer_new_allocate (NULL, new_size, NULL);
  
  if(*outbuf == NULL){
      GST_ELEMENT_ERROR (csoundfilter, RESOURCE, FAILED,
        ("%s", "Cant to allocate output buffers"), NULL);
      return GST_FLOW_ERROR;
  }
    
  *outbuf = gst_buffer_make_writable (*outbuf);
  return GST_FLOW_OK;
}

/* transform */
static GstFlowReturn
gst_csoundfilter_transform (GstBaseTransform * trans, GstBuffer * inbuf,
    GstBuffer * outbuf)
{
  GstCsoundfilter *csoundfilter = GST_CSOUNDFILTER (trans);

  GstClockTime timestamp, stream_time;
  GstMapInfo omap;
  gst_buffer_map(outbuf, &omap, GST_MAP_WRITE);
  gst_adapter_push (csoundfilter->in_adapter, inbuf);
  guint in_bytes = csoundfilter->ksmps * csoundfilter->cs_ichannels * sizeof(MYFLT);
  guint out_bytes = csoundfilter->ksmps * csoundfilter->cs_ochannels * sizeof(MYFLT);
  gst_buffer_ref(inbuf);
  timestamp = GST_BUFFER_TIMESTAMP (inbuf);

  GST_DEBUG_OBJECT (csoundfilter, "sync to %" GST_TIME_FORMAT,
      GST_TIME_ARGS (timestamp));

  stream_time = gst_segment_to_stream_time (&trans->segment, GST_FORMAT_TIME, timestamp);
  
  if (GST_CLOCK_TIME_IS_VALID (stream_time))
    gst_object_sync_values (GST_OBJECT (csoundfilter), stream_time);

  csoundfilter->process (csoundfilter, omap.data, in_bytes, out_bytes);
  gst_buffer_unmap(outbuf, &omap);

  if (csoundfilter->end_score){
    GST_DEBUG_OBJECT (csoundfilter, "reached the end of the csound score - looking for loop property %d", csoundfilter->end_score);
    if(csoundfilter->loop){
      csoundSetScoreOffsetSeconds(csoundfilter->csound, 0.0);
      csoundRewindScore(csoundfilter->csound);
    }else{
      GST_DEBUG_OBJECT (csoundfilter, "End of the csound score - sending a eos");
      return GST_FLOW_EOS;
    }
  }

  return GST_FLOW_OK;
}

static void
gst_csoundfilter_trans (GstCsoundfilter * csoundfilter,
    MYFLT * odata, guint in_bytes, guint out_bytes)
{
  gsize offset = 0;
  while( gst_adapter_available_fast(csoundfilter->in_adapter) >= (in_bytes + offset ) ) {
    gst_adapter_copy(csoundfilter->in_adapter, csoundfilter->spin, offset, in_bytes);
    memmove (odata, csoundfilter->spout, out_bytes);
    offset += in_bytes;
    csoundfilter->end_score = csoundPerformKsmps (csoundfilter->csound);
    odata += csoundfilter->ksmps * csoundfilter->cs_ochannels;
  }
  gst_adapter_flush (csoundfilter->in_adapter, offset);
  
}

static void
gst_csoundfilter_messages (CSOUND * csound, int attr, const char *format, va_list valist)
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

CSOUND *gst_csoundfilter_get_instance(GstCsoundfilter *csoundfilter){
    return csoundfilter->csound;
}
