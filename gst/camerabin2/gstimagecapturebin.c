/* GStreamer
 * Copyright (C) 2010 Thiago Santos <thiago.sousa.santos@collabora.co.uk>
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
/**
 * SECTION:element-gstimagecapturebin
 *
 * The gstimagecapturebin element does FIXME stuff.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v videotestsrc num-buffers=3 ! imagecapturebin
 * ]|
 * FIXME Describe what the pipeline does.
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstimagecapturebin.h"

/* prototypes */


enum
{
  PROP_0,
  PROP_LOCATION
};

#define DEFAULT_LOCATION "img_%d"

/* pad templates */

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw-yuv; video/x-raw-rgb")
    );

/* class initialization */

GST_BOILERPLATE (GstImageCaptureBin, gst_image_capture_bin, GstBin,
    GST_TYPE_BIN);

/* Element class functions */
static GstStateChangeReturn
gst_image_capture_bin_change_state (GstElement * element, GstStateChange trans);

static void
gst_image_capture_bin_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstImageCaptureBin *imagebin = GST_IMAGE_CAPTURE_BIN_CAST (object);

  switch (prop_id) {
    case PROP_LOCATION:
      g_free (imagebin->location);
      imagebin->location = g_value_dup_string (value);
      if (imagebin->sink) {
        g_object_set (imagebin, "location", imagebin->location, NULL);
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_image_capture_bin_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstImageCaptureBin *imagebin = GST_IMAGE_CAPTURE_BIN_CAST (object);

  switch (prop_id) {
    case PROP_LOCATION:
      g_value_set_string (value, imagebin->location);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_image_capture_bin_finalize (GObject * object)
{
  GstImageCaptureBin *imgbin = GST_IMAGE_CAPTURE_BIN_CAST (object);

  g_free (imgbin->location);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_image_capture_bin_base_init (gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sink_template));

  gst_element_class_set_details_simple (element_class, "Image Capture Bin",
      "Sink/Video", "Image Capture Bin used in camerabin2",
      "Thiago Santos <thiago.sousa.santos@collabora.co.uk>");
}

static void
gst_image_capture_bin_class_init (GstImageCaptureBinClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;

  gobject_class = G_OBJECT_CLASS (klass);
  element_class = GST_ELEMENT_CLASS (klass);

  gobject_class->set_property = gst_image_capture_bin_set_property;
  gobject_class->get_property = gst_image_capture_bin_get_property;
  gobject_class->finalize = gst_image_capture_bin_finalize;

  element_class->change_state =
      GST_DEBUG_FUNCPTR (gst_image_capture_bin_change_state);

  g_object_class_install_property (gobject_class, PROP_LOCATION,
      g_param_spec_string ("location", "Location",
          "Location to save the captured files. A %%d can be used as a "
          "placeholder for a capture count",
          DEFAULT_LOCATION, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
gst_image_capture_bin_init (GstImageCaptureBin * imagebin,
    GstImageCaptureBinClass * imagebin_class)
{
  GstPadTemplate *tmpl;

  tmpl = gst_static_pad_template_get (&sink_template);
  imagebin->ghostpad =
      gst_ghost_pad_new_no_target_from_template ("sink", tmpl);
  gst_object_unref (tmpl);
  gst_element_add_pad (GST_ELEMENT_CAST (imagebin), imagebin->ghostpad);

  imagebin->location = g_strdup (DEFAULT_LOCATION);
}

static gboolean
gst_image_capture_bin_create_elements (GstImageCaptureBin * imagebin)
{
  GstElement *colorspace;
  GstElement *encoder;
  GstElement *muxer;
  GstElement *sink;
  GstPad *pad = NULL;

  if (imagebin->elements_created)
    return TRUE;

  /* create elements */
  colorspace =
      gst_element_factory_make ("ffmpegcolorspace", "imagebin-colorspace");
  if (!colorspace)
    goto error;

  encoder = gst_element_factory_make ("jpegenc", "imagebin-encoder");
  if (!encoder)
    goto error;

  muxer = gst_element_factory_make ("jifmux", "imagebin-muxer");
  if (!muxer)
    goto error;

  sink = gst_element_factory_make ("multifilesink", "imagebin-sink");
  if (!sink)
    goto error;

  imagebin->sink = sink;
  g_object_set (sink, "location", imagebin->location, "async", FALSE, NULL);

  /* add and link */
  gst_bin_add_many (GST_BIN_CAST (imagebin), colorspace, encoder, muxer, sink,
      NULL);
  if (!gst_element_link_many (colorspace, encoder, muxer, sink, NULL))
    goto error;

  /* add ghostpad */
  pad = gst_element_get_static_pad (colorspace, "sink");
  if (!gst_ghost_pad_set_target (GST_GHOST_PAD (imagebin->ghostpad), pad))
    goto error;
  gst_object_unref (pad);

  imagebin->elements_created = TRUE;
  return TRUE;

error:
  if (pad)
    gst_object_unref (pad);
  return FALSE;
}

static GstStateChangeReturn
gst_image_capture_bin_change_state (GstElement * element, GstStateChange trans)
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GstImageCaptureBin *imagebin = GST_IMAGE_CAPTURE_BIN_CAST (element);

  switch (trans) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      if (!gst_image_capture_bin_create_elements (imagebin)) {
        return GST_STATE_CHANGE_FAILURE;
      }
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, trans);

  switch (trans) {
    case GST_STATE_CHANGE_READY_TO_NULL:
      break;
    default:
      break;
  }

  return ret;
}

gboolean
gst_image_capture_bin_plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, "imagecapturebin", GST_RANK_NONE,
      gst_image_capture_bin_get_type ());
}
