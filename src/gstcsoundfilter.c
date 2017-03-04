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
#include "gstcsoundfilter.h"

GST_DEBUG_CATEGORY_STATIC (gst_csoundfilter_debug_category);
#define GST_CAT_DEFAULT gst_csoundfilter_debug_category


/* prototypes */


static void gst_csoundfilter_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_csoundfilter_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_csoundfilter_finalize (GObject * object);

static gboolean gst_csoundfilter_setup (GstAudioFilter * filter,
    const GstAudioInfo * info);
static GstFlowReturn gst_csoundfilter_transform (GstBaseTransform * trans,
    GstBuffer * inbuf, GstBuffer * outbuf);
static void gst_csoundfilter_transform_double (GstCsoundFilter * filter,
    gdouble *idata, guint num_samples);
static void
gst_csoundfilter_transform_float (GstCsoundFilter * csoundfilter,
    gfloat * idata, guint num_samples);
static void gst_csoundfilter_messages (CSOUND * csound, int attr, const char *format,
    va_list valist);
static GstCaps *gst_csoundfilter_transform_caps (GstBaseTransform * base,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter);

/* Filter signals and args */
/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_LOCATION
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

G_DEFINE_TYPE_WITH_CODE (GstCsoundFilter, gst_csoundfilter,
    GST_TYPE_AUDIO_FILTER,
    GST_DEBUG_CATEGORY_INIT (gst_csoundfilter_debug_category, "csoundfilter", 0,
        "debug category for csoundfilter element"));

static void
gst_csoundfilter_class_init (GstCsoundFilterClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstBaseTransformClass *base_transform_class =
      GST_BASE_TRANSFORM_CLASS (klass);
      
  GstAudioFilterClass *audio_filter_class = GST_AUDIO_FILTER_CLASS (klass);
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

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "using csound for audio processing", "Filter/Effect/Audio",
      "Inplement a audio filter/effects using csound",
      "Natanael Mojica <neithanmo@gmail.com>");
  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_csoundfilter_finalize);
  audio_filter_class->setup = GST_DEBUG_FUNCPTR (gst_csoundfilter_setup);

  base_transform_class->transform =
      GST_DEBUG_FUNCPTR (gst_csoundfilter_transform);
  base_transform_class->transform_ip_on_passthrough = FALSE;
  base_transform_class->transform_caps=
      GST_DEBUG_FUNCPTR (gst_csoundfilter_transform_caps);
}

static void
gst_csoundfilter_init (GstCsoundFilter * csoundfilter)
{
    gst_base_transform_set_in_place (GST_BASE_TRANSFORM (csoundfilter), FALSE);
  csoundfilter->in_adapter = gst_adapter_new();
  //csoundfilter->out_adapter = gst_adapter_new();
}

void
gst_csoundfilter_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstCsoundFilter *csoundfilter = GST_CSOUNDFILTER (object);
  switch (property_id) {
    case PROP_LOCATION:
      csoundfilter->csd_name = g_value_dup_string (value);
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
  GstCsoundFilter *csoundfilter = GST_CSOUNDFILTER (object);
  switch (property_id) {
    case PROP_LOCATION:
      g_value_set_string (value, csoundfilter->csd_name);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (csoundfilter, property_id, pspec);
      break;
  }
}

static gboolean
gst_csoundfilter_setup (GstAudioFilter * filter, const GstAudioInfo * info)
{

  GstCsoundFilter *csoundfilter = GST_CSOUNDFILTER (filter);
  gboolean ret = TRUE;
  guint channels;
  csoundfilter->csound = csoundCreate (NULL);
  csoundSetMessageCallback (csoundfilter->csound,
      (csoundMessageCallback) gst_csoundfilter_messages);
  int result = csoundCompileCsd (csoundfilter->csound, csoundfilter->csd_name);
csoundStart(csoundfilter->csound);
  if (result) {
    GST_ELEMENT_ERROR (csoundfilter, RESOURCE, OPEN_READ,
        ("%s", csoundfilter->csd_name), (NULL));
    ret = FALSE;
  }
  csoundfilter->ksmps = csoundGetKsmps (csoundfilter->csound);
  channels = csoundGetNchnlsInput (csoundfilter->csound);
  if ((channels != GST_AUDIO_INFO_CHANNELS (info)) && !result) {
    GST_ERROR_OBJECT (csoundfilter,"Number of channels is not compatible with csounds input channels" 
          "set your caps with  %d channels", channels);
    ret = FALSE;
  }
  int size = csoundGetSizeOfMYFLT ();
  switch (GST_AUDIO_INFO_FORMAT(info)) {
    case GST_AUDIO_FORMAT_F64:
           csoundfilter->process = (GstCsoundFilterProcessFunc)
            gst_csoundfilter_transform_double;
          ret = TRUE;
      break;
    case GST_AUDIO_FORMAT_F32:
          csoundfilter->process = (GstCsoundFilterProcessFunc)
            gst_csoundfilter_transform_float;
            ret = TRUE;
      break;
    default:
      ret = FALSE;
      break;
  }
  return ret;
}

void
gst_csoundfilter_finalize (GObject * object)
{
  GstCsoundFilter *csoundfilter = GST_CSOUNDFILTER (object);
  if(csoundfilter->csound){
     csoundStop (csoundfilter->csound);
     csoundCleanup (csoundfilter->csound);
     csoundDestroy (csoundfilter->csound);
  }
  G_OBJECT_CLASS (gst_csoundfilter_parent_class)->finalize (object);
}


static GstCaps *
gst_csoundfilter_transform_caps (GstBaseTransform * base,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
  GstCaps *res;
  GstStructure *structure;
  gint i;
  //GST_DEBUG_OBJECT(GST_CSOUNDFILTER(base), "transform caps");
  /*check if audio format is supported by csound
   if not, fixing the caps and its audio format */
  res = gst_caps_copy (caps);
  
  if(csoundGetSizeOfMYFLT() == 8){
  for (i = 0; i < gst_caps_get_size (res); i++) {
    structure = gst_caps_get_structure (res, i);
    if (direction == GST_PAD_SRC) {
      GST_INFO_OBJECT (base, "csound only support F64 audio samples - fixed caps");
      gst_structure_set (structure, "format", G_TYPE_STRING, GST_AUDIO_NE (F64), NULL);
    } else {
      GST_INFO_OBJECT (base, "csound only support F64 audio samples - fixed caps");
      gst_structure_set (structure, "format", G_TYPE_STRING, GST_AUDIO_NE (F64), NULL);
     }
    }
   }
  
  else  if(csoundGetSizeOfMYFLT() == 4){
  for (i = 0; i < gst_caps_get_size (res); i++) {
    structure = gst_caps_get_structure (res, i);
    if (direction == GST_PAD_SRC) {
      GST_INFO_OBJECT (base, "csound only support F32 audio samples - fixed caps");
      gst_structure_set (structure, "format", G_TYPE_STRING, GST_AUDIO_NE (F32), NULL);
    } else {
      GST_INFO_OBJECT (base,"csound only support F32 audio samples - fixed caps");
      gst_structure_set (structure, "format", G_TYPE_STRING, GST_AUDIO_NE (F32), NULL);
     }
   }
 } 

  if (filter) {
    GstCaps *intersection;
    intersection =
        gst_caps_intersect_full (filter, res, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (res);
    res = intersection;
    GST_DEBUG_OBJECT (base, "Intersection %" GST_PTR_FORMAT, res);
  }

  return res;
    
}


static GstFlowReturn
gst_csoundfilter_transform (GstBaseTransform * trans, GstBuffer * inbuf,
    GstBuffer * outbuf)
{
  GstCsoundFilter *csoundfilter = GST_CSOUNDFILTER (trans);
  guint num_samples;
  GstClockTime timestamp, stream_time;
  GstMapInfo omap;
  
  guint channels = GST_AUDIO_FILTER_CHANNELS (csoundfilter);
  gint bytes = csoundfilter->ksmps * channels * sizeof(gdouble);
  gst_buffer_ref(inbuf);
  timestamp = GST_BUFFER_TIMESTAMP (inbuf);
  stream_time =
      gst_segment_to_stream_time (&trans->segment, GST_FORMAT_TIME, timestamp);
  if (GST_CLOCK_TIME_IS_VALID (stream_time))
    gst_object_sync_values (GST_OBJECT (csoundfilter), stream_time);
  
  gst_adapter_push (csoundfilter->in_adapter, inbuf);
  gst_buffer_map(outbuf, &omap, GST_MAP_WRITE);
  if (GST_CLOCK_TIME_IS_VALID (stream_time))
    gst_object_sync_values (GST_OBJECT (csoundfilter), stream_time);

  csoundfilter->process (csoundfilter, omap.data, bytes);
  return GST_FLOW_OK;
}


static void
gst_csoundfilter_transform_double (GstCsoundFilter * csoundfilter,
    gdouble * odata, guint num_samples)
{
  csoundfilter->spin = csoundGetSpin (csoundfilter->csound);
  csoundfilter->spout = csoundGetSpout (csoundfilter->csound);
  guint channels = GST_AUDIO_FILTER_CHANNELS (csoundfilter);
  //gdouble *odata;
  gdouble *idata;
  //odata = gst_adapter_map(csoundfilter->out_adapter, num_samples);
  while(gst_adapter_available(csoundfilter->in_adapter) >= num_samples) {
    idata = gst_adapter_map(csoundfilter->in_adapter, num_samples);
    memmove (csoundfilter->spin, idata, num_samples);
    csoundPerformKsmps (csoundfilter->csound);
    memmove (odata, csoundfilter->spout, num_samples);
    gst_adapter_unmap(csoundfilter->in_adapter);
    gst_adapter_flush (csoundfilter->in_adapter, num_samples);
    odata = odata + csoundfilter->ksmps * channels;
  }
}


static void
gst_csoundfilter_transform_float (GstCsoundFilter * csoundfilter,
    gfloat * idata, guint num_samples)
{
  csoundfilter->spin = csoundGetSpin (csoundfilter->csound);
  csoundfilter->spout = csoundGetSpout (csoundfilter->csound);
  guint channels = GST_AUDIO_FILTER_CHANNELS (csoundfilter);
  guint i;
  guint sample = 0;
  guint bytes_to_move = csoundfilter->ksmps * sizeof (gfloat) * channels;
  guint ciclos = num_samples / (csoundfilter->ksmps);
  for (i = 0; i < ciclos; i++) {
    memmove (csoundfilter->spin, idata, bytes_to_move);
    csoundPerformKsmps (csoundfilter->csound);
    memmove (idata, csoundfilter->spout, bytes_to_move);
    idata = idata + csoundfilter->ksmps * channels;
    sample = sample + csoundfilter->ksmps;
  }
  if ((num_samples % csoundfilter->ksmps) != 0) {
    memmove (csoundfilter->spin, idata,
        (num_samples - sample) * sizeof (gfloat) * channels);
    csoundPerformKsmps (csoundfilter->csound);
    memmove (idata, csoundfilter->spout,
        (num_samples - sample) * sizeof (gfloat) * channels);
  }
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