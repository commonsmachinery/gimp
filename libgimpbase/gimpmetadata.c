/* LIBGIMPBASE - The GIMP Basic Library
 * Copyright (C) 1995-1997 Peter Mattis and Spencer Kimball
 *
 * gimpmetadata.c
 * Copyright (C) 2013 Hartmut Kuhse <hartmutkuhse@src.gnome.org>
 *                    Michael Natterer <mitch@gimp.org>
 *
 * This library is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.  If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>

#include <gio/gio.h>
#include <gexiv2/gexiv2.h>

#include "gimpbasetypes.h"

#include "gimpmetadata.h"
#include "gimpunit.h"

#include "libgimp/libgimp-intl.h"


/**
 * SECTION: gimpmetadata
 * @title: gimpmetadata
 * @short_description: Basic functions for handling #GimpMetadata objects.
 * @see_also: gimp_image_metadata_load_prepare(),
 *            gimp_image_metadata_load_finish(),
 *            gimp_image_metadata_load_prepare(),
 *            gimp_image_metadata_load_finish().
 *
 * Basic functions for handling #GimpMetadata objects.
 **/


#define TAG_LINE_DELIMITER "\v"
#define TAG_TAG_DELIMITER  "#"


static GQuark     gimp_metadata_error_quark  (void);
static gint       gimp_metadata_length       (const gchar  *testline,
                                              const gchar  *delim);
static gboolean   gimp_metadata_get_rational (const gchar  *value,
                                              gint          sections,
                                              gchar      ***numerator,
                                              gchar      ***denominator);
static void       gimp_metadata_add          (GimpMetadata *src,
                                              GimpMetadata *dest);


static const gchar *tiff_tags[] =
{
  "Xmp.tiff",
  "Exif.Image.ImageWidth",
  "Exif.Image.ImageLength",
  "Exif.Image.BitsPerSample",
  "Exif.Image.Compression",
  "Exif.Image.PhotometricInterpretation",
  "Exif.Image.FillOrder",
  "Exif.Image.SamplesPerPixel",
  "Exif.Image.StripOffsets",
  "Exif.Image.RowsPerStrip",
  "Exif.Image.StripByteCounts",
  "Exif.Image.PlanarConfiguration"
};

static const gchar *jpeg_tags[] =
{
  "Exif.Image.JPEGProc",
  "Exif.Image.JPEGInterchangeFormat",
  "Exif.Image.JPEGInterchangeFormatLength",
  "Exif.Image.JPEGRestartInterval",
  "Exif.Image.JPEGLosslessPredictors",
  "Exif.Image.JPEGPointTransforms",
  "Exif.Image.JPEGQTables",
  "Exif.Image.JPEGDCTables",
  "Exif.Image.JPEGACTables"
};

static const gchar *unsupported_tags[] =
{
  "Exif.Image.SubIFDs",
  "Exif.Image.ClipPath",
  "Exif.Image.XClipPathUnits",
  "Exif.Image.YClipPathUnits",
  "Xmp.xmpMM.History",
  "Exif.Image.XPTitle",
  "Exif.Image.XPComment",
  "Exif.Image.XPAuthor",
  "Exif.Image.XPKeywords",
  "Exif.Image.XPSubject",
  "Exif.Image.DNGVersion",
  "Exif.Image.DNGBackwardVersion",
  "Exif.Iop"
};

static const guint8 minimal_exif[] =
{
  0xff, 0xd8, 0xff, 0xe0, 0x00, 0x10, 0x4a, 0x46, 0x49, 0x46, 0x00, 0x01,
  0x01, 0x01, 0x00, 0x5a, 0x00, 0x5a, 0x00, 0x00, 0xff, 0xe1
};

static const guint8 wilber_jpg[] =
{
  0xff, 0xd8, 0xff, 0xe0, 0x00, 0x10, 0x4a, 0x46, 0x49, 0x46, 0x00, 0x01,
  0x01, 0x01, 0x00, 0x5a, 0x00, 0x5a, 0x00, 0x00, 0xff, 0xdb, 0x00, 0x43,
  0x00, 0x50, 0x37, 0x3c, 0x46, 0x3c, 0x32, 0x50, 0x46, 0x41, 0x46, 0x5a,
  0x55, 0x50, 0x5f, 0x78, 0xc8, 0x82, 0x78, 0x6e, 0x6e, 0x78, 0xf5, 0xaf,
  0xb9, 0x91, 0xc8, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xdb, 0x00, 0x43, 0x01, 0x55, 0x5a,
  0x5a, 0x78, 0x69, 0x78, 0xeb, 0x82, 0x82, 0xeb, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xc0, 0x00, 0x11, 0x08, 0x00, 0x10, 0x00, 0x10, 0x03,
  0x01, 0x22, 0x00, 0x02, 0x11, 0x01, 0x03, 0x11, 0x01, 0xff, 0xc4, 0x00,
  0x16, 0x00, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x01, 0x02, 0xff, 0xc4, 0x00,
  0x1e, 0x10, 0x00, 0x01, 0x05, 0x00, 0x02, 0x03, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x02, 0x03, 0x11, 0x31,
  0x04, 0x12, 0x51, 0x61, 0x71, 0xff, 0xc4, 0x00, 0x14, 0x01, 0x01, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0xff, 0xc4, 0x00, 0x14, 0x11, 0x01, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0xff, 0xda, 0x00, 0x0c, 0x03, 0x01, 0x00, 0x02, 0x11, 0x03, 0x11,
  0x00, 0x3f, 0x00, 0x18, 0xa0, 0x0e, 0x6d, 0xbc, 0xf5, 0xca, 0xf7, 0x78,
  0xb6, 0xfe, 0x3b, 0x23, 0xb2, 0x1d, 0x64, 0x68, 0xf0, 0x8a, 0x39, 0x4b,
  0x74, 0x9c, 0xa5, 0x5f, 0x35, 0x8a, 0xb2, 0x7e, 0xa0, 0xff, 0xd9, 0x00
};

static const guint wilber_jpg_len = G_N_ELEMENTS (wilber_jpg);


/**
 * gimp_metadata_new:
 *
 * Creates a new #GimpMetadata instance.
 *
 * Return value: The new #GimpMetadata.
 *
 * Since: GIMP 2.10
 */
GimpMetadata *
gimp_metadata_new (void)
{
  GExiv2Metadata *metadata = NULL;

  if (gexiv2_initialize ())
    {
      metadata = gexiv2_metadata_new ();

      if (! gexiv2_metadata_open_buf (metadata, wilber_jpg, wilber_jpg_len,
                                      NULL))
        {
          g_object_unref (metadata);

          return NULL;
        }
    }

  return metadata;
}

/**
 * gimp_metadata_duplicate:
 * @metadata: The object to duplicate, or %NULL.
 *
 * Duplicates a #GimpMetadata instance.
 *
 * Return value: The new #GimpMetadata, or %NULL if @metadata is %NULL.
 *
 * Since: GIMP 2.10
 */
GimpMetadata *
gimp_metadata_duplicate (GimpMetadata *metadata)
{
  GimpMetadata *new_metadata = NULL;

  g_return_val_if_fail (metadata == NULL || GEXIV2_IS_METADATA (metadata), NULL);

  if (metadata)
    {
      gchar *xml;

      xml = gimp_metadata_serialize (metadata);
      new_metadata = gimp_metadata_deserialize (xml);
      g_free (xml);
    }

  return new_metadata;
}

typedef struct
{
  gchar         name[1024];
  GimpMetadata *metadata;
} GimpMetadataParseData;

static const gchar*
gimp_metadata_attribute_name_to_value (const gchar **attribute_names,
                                       const gchar **attribute_values,
                                       const gchar  *name)
{
  while (*attribute_names)
    {
      if (! strcmp (*attribute_names, name))
        {
          return *attribute_values;
        }

      attribute_names++;
      attribute_values++;
    }

  return NULL;
}

static void
gimp_metadata_deserialize_start_element (GMarkupParseContext *context,
                                         const gchar         *element_name,
                                         const gchar        **attribute_names,
                                         const gchar        **attribute_values,
                                         gpointer             user_data,
                                         GError             **error)
{
  GimpMetadataParseData *parse_data = user_data;

  if (! strcmp (element_name, "tag"))
    {
      const gchar *name;

      name = gimp_metadata_attribute_name_to_value (attribute_names,
                                                    attribute_values,
                                                    "name");

      if (! name)
        {
          g_set_error (error, gimp_metadata_error_quark (), 1001,
                       "Element 'tag' does not contain required attribute 'name'.");
          return;
        }

      strncpy (parse_data->name, name, sizeof (parse_data->name));
      parse_data->name[sizeof (parse_data->name) - 1] = 0;
    }
}

static void
gimp_metadata_deserialize_end_element (GMarkupParseContext *context,
                                       const gchar         *element_name,
                                       gpointer             user_data,
                                       GError             **error)
{
}

static void
gimp_metadata_deserialize_text (GMarkupParseContext  *context,
                                const gchar          *text,
                                gsize                 text_len,
                                gpointer              user_data,
                                GError              **error)
{
  GimpMetadataParseData *parse_data = user_data;
  const gchar           *current_element;

  current_element = g_markup_parse_context_get_element (context);

  if (! g_strcmp0 (current_element, "tag"))
    {
      gchar *value = g_strndup (text, text_len);

      gexiv2_metadata_set_tag_string (parse_data->metadata,
                                      parse_data->name,
                                      value);

      g_free (value);
    }
}

static  void
gimp_metadata_deserialize_error (GMarkupParseContext *context,
                                 GError              *error,
                                 gpointer             user_data)
{
  g_printerr ("Metadata parse error: %s\n", error->message);
}

/**
 * gimp_metadata_deserialize:
 * @metadata_xml: A string of serialized metadata XML.
 *
 * Deserializes a string of XML that has been created by
 * gimp_metadata_serialize().
 *
 * Return value: The new #GimpMetadata.
 *
 * Since: GIMP 2.10
 */
GimpMetadata *
gimp_metadata_deserialize (const gchar *metadata_xml)
{
  GimpMetadata          *metadata;
  GMarkupParser          markup_parser;
  GimpMetadataParseData  parse_data;
  GMarkupParseContext   *context;

  g_return_val_if_fail (metadata_xml != NULL, NULL);

  metadata = gimp_metadata_new ();

  parse_data.metadata = metadata;

  markup_parser.start_element = gimp_metadata_deserialize_start_element;
  markup_parser.end_element   = gimp_metadata_deserialize_end_element;
  markup_parser.text          = gimp_metadata_deserialize_text;
  markup_parser.passthrough   = NULL;
  markup_parser.error         = gimp_metadata_deserialize_error;

  context = g_markup_parse_context_new (&markup_parser, 0, &parse_data, NULL);

  g_markup_parse_context_parse (context,
                                metadata_xml, strlen (metadata_xml),
                                NULL);

  g_markup_parse_context_unref (context);

  return metadata;
}

/**
 * gimp_metadata_serialize:
 * @metadata: A #GimpMetadata instance.
 *
 * Serializes @metadata into an XML string that can later be deserialized
 * using gimp_metadata_deserialize().
 *
 * Return value: The serialized XML string.
 *
 * Since: GIMP 2.10
 */
gchar *
gimp_metadata_serialize (GimpMetadata *metadata)
{
  GString  *string;
  gchar   **exif_data = NULL;
  gchar   **iptc_data = NULL;
  gchar   **xmp_data  = NULL;
  gchar    *value;
  gchar    *escaped;
  gint      i;

  g_return_val_if_fail (GEXIV2_IS_METADATA (metadata), NULL);

  string = g_string_new (NULL);

  g_string_append (string, "<?xml version='1.0' encoding='UTF-8'?>\n");
  g_string_append (string, "<metadata>\n");

  exif_data = gexiv2_metadata_get_exif_tags (metadata);

  if (exif_data)
    {
      for (i = 0; exif_data[i] != NULL; i++)
        {
          value   = gexiv2_metadata_get_tag_string (metadata, exif_data[i]);
          escaped = g_markup_escape_text (value, -1);

          g_string_append_printf (string, "  <tag name=\"%s\">%s</tag>\n",
                                  exif_data[i], escaped);

          g_free (escaped);
          g_free (value);
        }

      g_strfreev (exif_data);
    }

  xmp_data = gexiv2_metadata_get_xmp_tags (metadata);

  if (xmp_data)
    {
      for (i = 0; xmp_data[i] != NULL; i++)
        {
          value   = gexiv2_metadata_get_tag_string (metadata, xmp_data[i]);
          escaped = g_markup_escape_text (value, -1);

          g_string_append_printf (string, "  <tag name=\"%s\">%s</tag>\n",
                                  xmp_data[i], escaped);

          g_free (escaped);
          g_free (value);
        }

      g_strfreev (xmp_data);
    }

  iptc_data = gexiv2_metadata_get_iptc_tags (metadata);

  if (iptc_data)
    {
      for (i = 0; iptc_data[i] != NULL; i++)
        {
          value   = gexiv2_metadata_get_tag_string (metadata, iptc_data[i]);
          escaped = g_markup_escape_text (value, -1);

          g_string_append_printf (string, "  <tag name=\"%s\">%s</tag>\n",
                                  iptc_data[i], escaped);

          g_free (escaped);
          g_free (value);
        }

      g_strfreev (iptc_data);
    }

  g_string_append (string, "</metadata>\n");

  return g_string_free (string, FALSE);
}

/**
 * gimp_metadata_load_from_file:
 * @file:  The #GFile to load the metadata from
 * @error: Return location for error message
 *
 * Loads #GimpMetadata from @file.
 *
 * Return value: The loaded #GimpMetadata.
 *
 * Since: GIMP 2.10
 */
GimpMetadata  *
gimp_metadata_load_from_file (GFile   *file,
                              GError **error)
{
  GExiv2Metadata *meta = NULL;
  gchar          *path;
  gchar          *filename;

  g_return_val_if_fail (G_IS_FILE (file), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  path = g_file_get_path (file);

  if (! path)
    {
      g_set_error (error, gimp_metadata_error_quark (), 0,
                   _("Can load metadata only from local files"));
      return NULL;
    }

#ifdef G_OS_WIN32
  filename = g_win32_locale_filename_from_utf8 (path);
#else
  filename = g_strdup (path);
#endif

  g_free (path);

  if (gexiv2_initialize ())
    {
      meta = gexiv2_metadata_new ();

      if (! gexiv2_metadata_open_path (meta, filename, error))
        {
          g_object_unref (meta);
          g_free (filename);

          return NULL;
        }
    }

  g_free (filename);

  return meta;
}

/**
 * gimp_metadata_save_to_file:
 * @metadata: A #GimpMetadata instance.
 * @file:     The file to save the metadata to
 * @error:    Return location for error message
 *
 * Saves @metadata to @file.
 *
 * Return value: %TRUE on success, %FALSE otherwise.
 *
 * Since: GIMP 2.10
 */
gboolean
gimp_metadata_save_to_file (GimpMetadata  *metadata,
                            GFile         *file,
                            GError       **error)
{
  gchar    *path;
  gchar    *filename;
  gboolean  success;

  g_return_val_if_fail (GEXIV2_IS_METADATA (metadata), FALSE);
  g_return_val_if_fail (G_IS_FILE (file), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  path = g_file_get_path (file);

  if (! path)
    {
      g_set_error (error, gimp_metadata_error_quark (), 0,
                   _("Can save metadata only to local files"));
      return FALSE;
    }

#ifdef G_OS_WIN32
  filename = g_win32_locale_filename_from_utf8 (path);
#else
  filename = g_strdup (path);
#endif

  g_free (path);

  success = gexiv2_metadata_save_file (metadata, filename, error);

  g_free (filename);

  return success;
}

/**
 * gimp_metadata_set_from_exif:
 * @metadata:         A #GimpMetadata instance.
 * @exif_data:        The blob of Exif data to set
 * @exif_data_length: Length of @exif_data, in bytes
 * @error:            Return location for error message
 *
 * Sets the tags from a piece of Exif data on @metadata.
 *
 * Return value: %TRUE on success, %FALSE otherwise.
 *
 * Since: GIMP 2.10
 */
gboolean
gimp_metadata_set_from_exif (GimpMetadata  *metadata,
                             const guchar  *exif_data,
                             gint           exif_data_length,
                             GError       **error)
{

  GByteArray   *exif_bytes;
  GimpMetadata *exif_metadata;
  guint8        data_size[2] = { 0, };

  g_return_val_if_fail (GEXIV2_IS_METADATA (metadata), FALSE);
  g_return_val_if_fail (exif_data != NULL, FALSE);
  g_return_val_if_fail (exif_data_length > 0, FALSE);
  g_return_val_if_fail (exif_data_length < 65536, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  data_size[0] = (exif_data_length & 0xFF00) >> 8;
  data_size[1] = (exif_data_length & 0x00FF);

  exif_bytes = g_byte_array_new ();
  exif_bytes = g_byte_array_append (exif_bytes,
                                    minimal_exif, G_N_ELEMENTS (minimal_exif));
  exif_bytes = g_byte_array_append (exif_bytes,
                                    data_size, 2);
  exif_bytes = g_byte_array_append (exif_bytes,
                                    (guint8 *) exif_data, exif_data_length);

  exif_metadata = gimp_metadata_new ();

  if (! gexiv2_metadata_open_buf (exif_metadata,
                                  exif_bytes->data, exif_bytes->len, error))
    {
      g_object_unref (exif_metadata);
      g_byte_array_free (exif_bytes, TRUE);
      return FALSE;
    }

  if (! gexiv2_metadata_has_exif (exif_metadata))
    {
      g_set_error (error, gimp_metadata_error_quark (), 0,
                   _("Parsing Exif data failed."));
      g_object_unref (exif_metadata);
      g_byte_array_free (exif_bytes, TRUE);
      return FALSE;
    }

  gimp_metadata_add (exif_metadata, metadata);
  g_object_unref (exif_metadata);
  g_byte_array_free (exif_bytes, TRUE);

  return TRUE;
}

/**
 * gimp_metadata_set_from_xmp:
 * @metadata:        A #GimpMetadata instance.
 * @xmp_data:        The blob of Exif data to set
 * @xmp_data_length: Length of @exif_data, in bytes
 * @error:           Return location for error message
 *
 * Sets the tags from a piece of XMP data on @metadata.
 *
 * Return value: %TRUE on success, %FALSE otherwise.
 *
 * Since: GIMP 2.10
 */
gboolean
gimp_metadata_set_from_xmp (GimpMetadata  *metadata,
                            const guchar  *xmp_data,
                            gint           xmp_data_length,
                            GError       **error)
{
  GimpMetadata *xmp_metadata;

  g_return_val_if_fail (GEXIV2_IS_METADATA (metadata), FALSE);
  g_return_val_if_fail (xmp_data != NULL, FALSE);
  g_return_val_if_fail (xmp_data_length > 0, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  xmp_data        += 10;
  xmp_data_length -= 10;

  xmp_metadata = gimp_metadata_new ();

  if (! gexiv2_metadata_open_buf (xmp_metadata,
                                  xmp_data, xmp_data_length, error))
    {
      g_object_unref (xmp_metadata);
      return FALSE;
    }

  if (! gexiv2_metadata_has_xmp (xmp_metadata))
    {
      g_set_error (error, gimp_metadata_error_quark (), 0,
                   _("Parsing XMP data failed."));
      g_object_unref (xmp_metadata);
      return FALSE;
    }

  gimp_metadata_add (xmp_metadata, metadata);
  g_object_unref (xmp_metadata);

  return TRUE;
}

/**
 * gimp_metadata_set_pixel_size:
 * @metadata: A #GimpMetadata instance.
 * @width:    Width in pixels
 * @height:   Height in pixels
 *
 * Sets Exif.Image.ImageWidth and Exif.Image.ImageLength on @metadata.
 *
 * Since: GIMP 2.10
 */
void
gimp_metadata_set_pixel_size (GimpMetadata *metadata,
                              gint          width,
                              gint          height)
{
  gchar buffer[32];

  g_return_if_fail (GEXIV2_IS_METADATA (metadata));

  g_snprintf (buffer, sizeof (buffer), "%d", width);
  gexiv2_metadata_set_tag_string (metadata, "Exif.Image.ImageWidth", buffer);

  g_snprintf (buffer, sizeof (buffer), "%d", height);
  gexiv2_metadata_set_tag_string (metadata, "Exif.Image.ImageLength", buffer);
}

/**
 * gimp_metadata_set_bits_per_sample:
 * @metadata: A #GimpMetadata instance.
 * @bps:      Bytes per pixel, per component
 *
 * Sets Exif.Image.BitsPerSample on @metadata.
 *
 * Since: GIMP 2.10
 */
void
gimp_metadata_set_bits_per_sample (GimpMetadata *metadata,
                                   gint          bps)
{
  gchar buffer[32];

  g_return_if_fail (GEXIV2_IS_METADATA (metadata));

  g_snprintf (buffer, sizeof (buffer), "%d %d %d", bps, bps, bps);
  gexiv2_metadata_set_tag_string (metadata, "Exif.Image.BitsPerSample", buffer);
}

/**
 * gimp_metadata_get_resolution:
 * @metadata: A #GimpMetadata instance.
 * @xres:     Return location for the X Resolution, in ppi
 * @yres:     Return location for the Y Resolution, in ppi
 * @unit:     Return location for the unit unit
 *
 * Returns values based on Exif.Image.XResolution,
 * Exif.Image.YResolution and Exif.Image.ResolutionUnit of @metadata.
 *
 * Return value: %TRUE on success, %FALSE otherwise.
 *
 * Since: GIMP 2.10
 */
gboolean
gimp_metadata_get_resolution (GimpMetadata *metadata,
                              gdouble      *xres,
                              gdouble      *yres,
                              GimpUnit     *unit)
{
  gchar  *xr;
  gchar  *yr;
  gchar  *un;
  gint    exif_unit = 2;
  gchar **xnom      = NULL;
  gchar **xdenom    = NULL;
  gchar **ynom      = NULL;
  gchar **ydenom    = NULL;

  g_return_val_if_fail (GEXIV2_IS_METADATA (metadata), FALSE);

  xr = gexiv2_metadata_get_tag_string (metadata, "Exif.Image.XResolution");
  yr = gexiv2_metadata_get_tag_string (metadata, "Exif.Image.YResolution");

  if (! (xr && yr))
    {
      g_free (xr);
      g_free (yr);
      return FALSE;
    }

  un = gexiv2_metadata_get_tag_string (metadata, "Exif.Image.ResolutionUnit");

  if (un)
    {
      exif_unit = atoi (un);
      g_free (un);
    }

  if (exif_unit == 3)
    *unit = GIMP_UNIT_MM;
  else
    *unit = GIMP_UNIT_INCH;

  if (gimp_metadata_get_rational (xr, 1, &xnom, &xdenom))
    {
      gdouble x1 = g_ascii_strtod (xnom[0], NULL);
      gdouble x2 = g_ascii_strtod (xdenom[0], NULL);
      gdouble xrd;

      if (x2 == 0.0)
        return FALSE;

      xrd = x1 / x2;

      if (exif_unit == 3)
        xrd *= 2.54;

      *xres = xrd;
    }

  if (gimp_metadata_get_rational (yr, 1, &ynom, &ydenom))
    {
      gdouble y1 = g_ascii_strtod (ynom[0], NULL);
      gdouble y2 = g_ascii_strtod (ydenom[0], NULL);
      gdouble yrd;

      if (y2 == 0.0)
        return FALSE;

      yrd = y1 / y2;

      if (exif_unit == 3)
        yrd *= 2.54;

      *yres = yrd;
    }

  g_free (xr);
  g_free (yr);

  g_strfreev (xnom);
  g_strfreev (xdenom);
  g_strfreev (ynom);
  g_strfreev (ydenom);

  return TRUE;
}

/**
 * gimp_metadata_set_resolution:
 * @metadata: A #GimpMetadata instance.
 * @xres:     The image's X Resolution, in ppi
 * @yres:     The image's Y Resolution, in ppi
 * @unit:     The image's unit
 *
 * Sets Exif.Image.XResolution, Exif.Image.YResolution and
 * Exif.Image.ResolutionUnit @metadata.
 *
 * Since: GIMP 2.10
 */
void
gimp_metadata_set_resolution (GimpMetadata *metadata,
                              gdouble       xres,
                              gdouble       yres,
                              GimpUnit      unit)
{
  gchar buffer[32];
  gint  exif_unit;

  g_return_if_fail (GEXIV2_IS_METADATA (metadata));

  if (gimp_unit_is_metric (unit))
    {
      xres /= 2.54;
      yres /= 2.54;

      exif_unit = 3;
    }
  else
    {
      exif_unit = 2;
    }

  g_ascii_formatd (buffer, sizeof (buffer), "%.0f/1", xres);
  gexiv2_metadata_set_tag_string (metadata, "Exif.Image.XResolution", buffer);

  g_ascii_formatd (buffer, sizeof (buffer), "%.0f/1", yres);
  gexiv2_metadata_set_tag_string (metadata, "Exif.Image.YResolution", buffer);

  g_snprintf (buffer, sizeof (buffer), "%d", exif_unit);
  gexiv2_metadata_set_tag_string (metadata, "Exif.Image.ResolutionUnit", buffer);
}

/**
 * gimp_metadata_is_tag_supported:
 * @tag:       A metadata tag name
 * @mime_type: A mime type
 *
 * Returns whether @tag is supported in a file of type @mime_type.
 *
 * Return value: %TRUE if the @tag supported with @mime_type, %FALSE otherwise.
 *
 * Since: GIMP 2.10
 */
gboolean
gimp_metadata_is_tag_supported (const gchar *tag,
                                const gchar *mime_type)
{
  gint j;

  g_return_val_if_fail (tag != NULL, FALSE);
  g_return_val_if_fail (mime_type != NULL, FALSE);

  for (j = 0; j < G_N_ELEMENTS (unsupported_tags); j++)
    {
      if (g_str_has_prefix (tag, unsupported_tags[j]))
        {
          return FALSE;
        }
    }

  if (! strcmp (mime_type, "image/jpeg"))
    {
      for (j = 0; j < G_N_ELEMENTS (tiff_tags); j++)
        {
          if (g_str_has_prefix (tag, tiff_tags[j]))
            {
              return FALSE;
            }
        }
    }
  else if (! strcmp (mime_type, "image/tiff"))
    {
      for (j = 0; j < G_N_ELEMENTS (jpeg_tags); j++)
        {
          if (g_str_has_prefix (tag, jpeg_tags[j]))
            {
              return FALSE;
            }
        }
    }

  return TRUE;
}

void
gimp_metadata_append_tag_value (GimpMetadata *metadata,
                            const gchar *tagname,
                            gchar *value)
{
  gchar** values;
  gchar** temp;
  guint length = 1;
  guint i;

  values = gexiv2_metadata_get_tag_multiple (metadata, tagname);
  while (values[length - 1] != NULL) length++;

  temp = g_new (gchar*, length + 1);

  for (i = 0; i < length + 1; i++) {
    temp[i] = values[i];
  }

  temp[length - 1] = g_strdup(value);
  temp[length] = NULL;

  gexiv2_metadata_set_tag_multiple (metadata, tagname, (const gchar **) temp);
}

//GimpMetadata *
void
gimp_metadata_merge_creator (GimpMetadata *metadata1,
                             GimpMetadata *metadata2)
{
  //GimpMetadata* result;
  gchar** values;
  guint i;

  //result = gimp_metadata_duplicate(metadata1);

  values = gexiv2_metadata_get_tag_multiple (metadata2, "Xmp.dc.creator");
  i = 0;
  while (values[i] != NULL) 
    {
      //gimp_metadata_append_tag_value(result, "Xmp.dc.creator", values[i]);
      gimp_metadata_append_tag_value(metadata1, "Xmp.dc.creator", values[i]);
      i++;
    }

  values = gexiv2_metadata_get_tag_multiple (metadata2, "Xmp.dc.source");
  i = 0;
  while (values[i] != NULL) 
    {
      //gimp_metadata_append_tag_value(result, "Xmp.dc.source", values[i]);
      gimp_metadata_append_tag_value(metadata1, "Xmp.dc.creator", values[i]);
      i++;
    }

  //return result;
}

/* private functions */

static GQuark
gimp_metadata_error_quark (void)
{
  static GQuark quark = 0;

  if (G_UNLIKELY (quark == 0))
    quark = g_quark_from_static_string ("gimp-metadata-error-quark");

  return quark;
}

/**
 * determines the amount of delimiters in serialized
 * metadata string
 */
static gint
gimp_metadata_length (const gchar *testline,
                      const gchar *delim)
{
  gchar *delim_test;
  gint   i;
  gint   sum;

  delim_test = g_strdup (testline);

  sum =0;

  for (i=0; i < strlen (delim_test); i++)
    {
      if (delim_test[i] == delim[0])
        sum++;
    }

  g_free (delim_test);

  return sum;
}

/**
 * gets rational values from string
 */
static gboolean
gimp_metadata_get_rational (const gchar   *value,
                            gint           sections,
                            gchar       ***numerator,
                            gchar       ***denominator)
{
  GSList *nomlist = NULL;
  GSList *denomlist = NULL;

  GSList *nlist, *dlist;

  gchar   sect[] = " ";
  gchar   rdel[] = "/";
  gchar **sects;
  gchar **nom = NULL;
  gint    i;
  gint    n;

  gchar **num;
  gchar **den;

  if (! value)
    return FALSE;

  if (gimp_metadata_length (value, sect) == (sections -1))
    {
      i = 0;
      sects = g_strsplit (value, sect, -1);
      while (sects[i] != NULL)
        {
          if(gimp_metadata_length (sects[i], rdel) == 1)
            {
              nom = g_strsplit (sects[i], rdel, -1);
              nomlist = g_slist_prepend (nomlist, g_strdup (nom[0]));
              denomlist = g_slist_prepend (denomlist, g_strdup (nom[1]));
            }
          else
            {
              return FALSE;
            }
          i++;
        }
    }
  else
    {
      return FALSE;
    }

  n = i;

  num = g_new0 (gchar*, i + 1);
  den = g_new0 (gchar*, n + 1);

  for (nlist = nomlist; nlist; nlist = nlist->next)
    num[--i] = nlist->data;

  for (dlist = denomlist; dlist; dlist = dlist->next)
    den[--n] = dlist->data;

  *numerator = num;
  *denominator = den;

  g_slist_free (nomlist);
  g_slist_free (denomlist);

  g_strfreev (sects);
  g_strfreev (nom);

  return TRUE;
}

static void
gimp_metadata_add (GimpMetadata *src,
                   GimpMetadata *dest)
{
  gchar *value;
  gint   i;

  if (gexiv2_metadata_get_supports_exif (src) &&
      gexiv2_metadata_get_supports_exif (dest))
    {
      gchar **exif_data = gexiv2_metadata_get_exif_tags (src);

      if (exif_data)
        {
          for (i = 0; exif_data[i] != NULL; i++)
            {
              value = gexiv2_metadata_get_tag_string (src, exif_data[i]);
              gexiv2_metadata_set_tag_string (dest, exif_data[i], value);
              g_free (value);
            }

          g_strfreev (exif_data);
        }
    }


  if (gexiv2_metadata_get_supports_xmp (src) &&
      gexiv2_metadata_get_supports_xmp (dest))
    {
      gchar **xmp_data = gexiv2_metadata_get_xmp_tags (src);

      if (xmp_data)
        {
          for (i = 0; xmp_data[i] != NULL; i++)
            {
              value = gexiv2_metadata_get_tag_string (src, xmp_data[i]);
              gexiv2_metadata_set_tag_string (dest, xmp_data[i], value);
              g_free (value);
            }

          g_strfreev (xmp_data);
        }
    }

  if (gexiv2_metadata_get_supports_iptc (src) &&
      gexiv2_metadata_get_supports_iptc (dest))
    {
      gchar **iptc_data = gexiv2_metadata_get_iptc_tags (src);

      if (iptc_data)
        {
          for (i = 0; iptc_data[i] != NULL; i++)
            {
              value = gexiv2_metadata_get_tag_string (src, iptc_data[i]);
              gexiv2_metadata_set_tag_string (dest, iptc_data[i], value);
              g_free (value);
            }

          g_strfreev (iptc_data);
        }
    }
}
