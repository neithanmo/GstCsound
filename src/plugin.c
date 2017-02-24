/* GStreamer soundtouch plugin
 * Copyright (C) 2017 Natanael Mojica <neithanmo@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>
#include <csound/csound.h>
#include "gstcsoundfilter.h"
#include "gstcsoundsrc.h"

static gboolean
plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, "csoundfilter", GST_RANK_NONE, GST_TYPE_CSOUNDFILTER)
         && gst_element_register (plugin, "csoundsrc", GST_RANK_NONE,GST_TYPE_CSOUNDSRC);

}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    csound,
    "audio filter/effects and audio source",
    plugin_init, "1.90.9", "LGPL", "gst-plugins-bad", "unknown")
