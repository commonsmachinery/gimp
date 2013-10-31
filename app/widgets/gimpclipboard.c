/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <string.h>

#include <gegl.h>
#include <gtk/gtk.h>

#include <redland.h>

#include "widgets-types.h"

#include "core/gimp.h"
#include "core/gimpbuffer.h"
#include "core/gimpcurve.h"

#include "gimpclipboard.h"
#include "gimppixbuf.h"
#include "gimpselectiondata.h"
#include "core/gimpimage-metadata.h"
#include "libgimpbase/gimpmetadata.h"
#include <gexiv2/gexiv2.h>

#include "gimp-intl.h"


#define GIMP_CLIPBOARD_KEY "gimp-clipboard"


typedef struct _GimpClipboard GimpClipboard;

struct _GimpClipboard
{
  GSList         *pixbuf_formats;

  GtkTargetEntry *target_entries;
  gint            n_target_entries;

  GtkTargetEntry *svg_target_entries;
  gint            n_svg_target_entries;

  GtkTargetEntry *curve_target_entries;
  gint            n_curve_target_entries;

  GimpBuffer     *buffer;
  gchar          *svg;
  GimpCurve      *curve;
};


static GimpClipboard * gimp_clipboard_get        (Gimp             *gimp);
static void            gimp_clipboard_clear      (GimpClipboard    *gimp_clip);
static void            gimp_clipboard_free       (GimpClipboard    *gimp_clip);

static GdkAtom * gimp_clipboard_wait_for_targets (Gimp             *gimp,
                                                  gint             *n_targets);
static GdkAtom   gimp_clipboard_wait_for_buffer  (Gimp             *gimp);
static GdkAtom   gimp_clipboard_wait_for_svg     (Gimp             *gimp);
static GdkAtom   gimp_clipboard_wait_for_curve   (Gimp             *gimp);
static GdkAtom   gimp_clipboard_wait_for_rdf     (Gimp             *gimp);

static void      gimp_clipboard_send_buffer      (GtkClipboard     *clipboard,
                                                  GtkSelectionData *selection_data,
                                                  guint             info,
                                                  Gimp             *gimp);
static void      gimp_clipboard_send_svg         (GtkClipboard     *clipboard,
                                                  GtkSelectionData *selection_data,
                                                  guint             info,
                                                  Gimp             *gimp);
static void      gimp_clipboard_send_curve       (GtkClipboard     *clipboard,
                                                  GtkSelectionData *selection_data,
                                                  guint             info,
                                                  Gimp             *gimp);


/*  public functions  */

void
gimp_clipboard_init (Gimp *gimp)
{
  GimpClipboard *gimp_clip;
  GSList        *list;

  g_return_if_fail (GIMP_IS_GIMP (gimp));

  gimp_clip = gimp_clipboard_get (gimp);

  g_return_if_fail (gimp_clip == NULL);

  gimp_clip = g_slice_new0 (GimpClipboard);

  g_object_set_data_full (G_OBJECT (gimp), GIMP_CLIPBOARD_KEY,
                          gimp_clip, (GDestroyNotify) gimp_clipboard_free);

  gimp_clip->pixbuf_formats = gimp_pixbuf_get_formats ();

  for (list = gimp_clip->pixbuf_formats; list; list = g_slist_next (list))
    {
      GdkPixbufFormat *format = list->data;

      if (gdk_pixbuf_format_is_writable (format))
        {
          gchar **mime_types;
          gchar **type;

          mime_types = gdk_pixbuf_format_get_mime_types (format);

          for (type = mime_types; *type; type++)
            gimp_clip->n_target_entries++;

          g_strfreev (mime_types);
        }
    }

  if (gimp_clip->n_target_entries > 0)
    {
      gint i = 0;

      gimp_clip->target_entries = g_new0 (GtkTargetEntry,
                                          gimp_clip->n_target_entries);

      for (list = gimp_clip->pixbuf_formats; list; list = g_slist_next (list))
        {
          GdkPixbufFormat *format = list->data;

          if (gdk_pixbuf_format_is_writable (format))
            {
              gchar  *format_name;
              gchar **mime_types;
              gchar **type;

              format_name = gdk_pixbuf_format_get_name (format);
              mime_types  = gdk_pixbuf_format_get_mime_types (format);

              for (type = mime_types; *type; type++)
                {
                  const gchar *mime_type = *type;

                  if (gimp->be_verbose)
                    g_printerr ("clipboard: writable pixbuf format: %s\n",
                                mime_type);

                  gimp_clip->target_entries[i].target = g_strdup (mime_type);
                  gimp_clip->target_entries[i].flags  = 0;
                  gimp_clip->target_entries[i].info   = i;

                  i++;
                }

              g_strfreev (mime_types);
              g_free (format_name);
            }
        }
    }

  gimp_clip->n_svg_target_entries = 2;
  gimp_clip->svg_target_entries   = g_new0 (GtkTargetEntry, 2);

  gimp_clip->svg_target_entries[0].target = g_strdup ("image/svg");
  gimp_clip->svg_target_entries[0].flags  = 0;
  gimp_clip->svg_target_entries[0].info   = 0;

  gimp_clip->svg_target_entries[1].target = g_strdup ("image/svg+xml");
  gimp_clip->svg_target_entries[1].flags  = 0;
  gimp_clip->svg_target_entries[1].info   = 1;

  gimp_clip->n_curve_target_entries = 1;
  gimp_clip->curve_target_entries   = g_new0 (GtkTargetEntry, 1);

  gimp_clip->curve_target_entries[0].target = g_strdup ("application/x-gimp-curve");
  gimp_clip->curve_target_entries[0].flags  = 0;
  gimp_clip->curve_target_entries[0].info   = 0;
}

void
gimp_clipboard_exit (Gimp *gimp)
{
  GtkClipboard *clipboard;

  g_return_if_fail (GIMP_IS_GIMP (gimp));

  clipboard = gtk_clipboard_get_for_display (gdk_display_get_default (),
                                             GDK_SELECTION_CLIPBOARD);

  if (clipboard && gtk_clipboard_get_owner (clipboard) == G_OBJECT (gimp))
    gtk_clipboard_store (clipboard);

  g_object_set_data (G_OBJECT (gimp), GIMP_CLIPBOARD_KEY, NULL);
}

/**
 * gimp_clipboard_has_buffer:
 * @gimp: pointer to #Gimp
 *
 * Tests if there's image data in the clipboard. If the global cut
 * buffer of @gimp is empty, this function checks if there's image
 * data in %GDK_SELECTION_CLIPBOARD. This is done in a main-loop
 * similar to gtk_clipboard_wait_is_text_available(). The same caveats
 * apply here.
 *
 * Return value: %TRUE if there's image data in the clipboard, %FALSE otherwise
 **/
gboolean
gimp_clipboard_has_buffer (Gimp *gimp)
{
  GimpClipboard *gimp_clip;
  GtkClipboard  *clipboard;

  g_return_val_if_fail (GIMP_IS_GIMP (gimp), FALSE);

  clipboard = gtk_clipboard_get_for_display (gdk_display_get_default (),
                                             GDK_SELECTION_CLIPBOARD);

  if (clipboard                                                &&
      gtk_clipboard_get_owner (clipboard)   != G_OBJECT (gimp) &&
      gimp_clipboard_wait_for_buffer (gimp) != GDK_NONE)
    {
      return TRUE;
    }

  gimp_clip = gimp_clipboard_get (gimp);

  return (gimp_clip->buffer != NULL);
}

/**
 * gimp_clipboard_has_svg:
 * @gimp: pointer to #Gimp
 *
 * Tests if there's SVG data in %GDK_SELECTION_CLIPBOARD.
 * This is done in a main-loop similar to
 * gtk_clipboard_wait_is_text_available(). The same caveats apply here.
 *
 * Return value: %TRUE if there's SVG data in the clipboard, %FALSE otherwise
 **/
gboolean
gimp_clipboard_has_svg (Gimp *gimp)
{
  GimpClipboard *gimp_clip;
  GtkClipboard  *clipboard;

  g_return_val_if_fail (GIMP_IS_GIMP (gimp), FALSE);

  clipboard = gtk_clipboard_get_for_display (gdk_display_get_default (),
                                             GDK_SELECTION_CLIPBOARD);

  if (clipboard                                              &&
      gtk_clipboard_get_owner (clipboard) != G_OBJECT (gimp) &&
      gimp_clipboard_wait_for_svg (gimp)  != GDK_NONE)
    {
      return TRUE;
    }

  gimp_clip = gimp_clipboard_get (gimp);

  return (gimp_clip->svg != NULL);
}

/**
 * gimp_clipboard_has_curve:
 * @gimp: pointer to #Gimp
 *
 * Tests if there's curve data in %GDK_SELECTION_CLIPBOARD.
 * This is done in a main-loop similar to
 * gtk_clipboard_wait_is_text_available(). The same caveats apply here.
 *
 * Return value: %TRUE if there's curve data in the clipboard, %FALSE otherwise
 **/
gboolean
gimp_clipboard_has_curve (Gimp *gimp)
{
  GimpClipboard *gimp_clip;
  GtkClipboard  *clipboard;

  g_return_val_if_fail (GIMP_IS_GIMP (gimp), FALSE);

  clipboard = gtk_clipboard_get_for_display (gdk_display_get_default (),
                                             GDK_SELECTION_CLIPBOARD);

  if (clipboard                                              &&
      gtk_clipboard_get_owner (clipboard) != G_OBJECT (gimp) &&
      gimp_clipboard_wait_for_curve (gimp)  != GDK_NONE)
    {
      return TRUE;
    }

  gimp_clip = gimp_clipboard_get (gimp);

  return (gimp_clip->curve != NULL);
}

/**
 * gimp_clipboard_get_buffer:
 * @gimp: pointer to #Gimp
 *
 * Retrieves either image data from %GDK_SELECTION_CLIPBOARD or from
 * the global cut buffer of @gimp.
 *
 * The returned #GimpBuffer needs to be unref'ed when it's no longer
 * needed.
 *
 * Return value: a reference to a #GimpBuffer or %NULL if there's no
 *               image data
 **/
GimpBuffer *
gimp_clipboard_get_buffer (Gimp *gimp)
{
  GimpClipboard *gimp_clip;
  GtkClipboard  *clipboard;
  GdkAtom        atom;
  GimpBuffer    *buffer = NULL;

  g_return_val_if_fail (GIMP_IS_GIMP (gimp), NULL);

  clipboard = gtk_clipboard_get_for_display (gdk_display_get_default (),
                                             GDK_SELECTION_CLIPBOARD);

  if (clipboard                                                         &&
      gtk_clipboard_get_owner (clipboard)            != G_OBJECT (gimp) &&
      (atom = gimp_clipboard_wait_for_buffer (gimp)) != GDK_NONE)
    {
      GtkSelectionData *data;

      gimp_set_busy (gimp);

      data = gtk_clipboard_wait_for_contents (clipboard, atom);

      if (data)
        {
          GdkPixbuf *pixbuf = gtk_selection_data_get_pixbuf (data);

          gtk_selection_data_free (data);

          if (pixbuf)
            {
              buffer = gimp_buffer_new_from_pixbuf (pixbuf, _("Clipboard"),
                                                    0, 0);
              g_object_unref (pixbuf);
            }
        }

      gimp_unset_busy (gimp);
    }

  gimp_clip = gimp_clipboard_get (gimp);

  if (! buffer && gimp_clip->buffer)
    buffer = g_object_ref (gimp_clip->buffer);

  return buffer;
}

static void
gimp_clipboard_set_metadata_from_rdf (GimpMetadata *metadata,
                                 librdf_world *world,
                                 librdf_model *model,
                                 librdf_uri *source_uri,
                                 const gchar **predicates,
                                 const gchar *tagname,
                                 gboolean seq_type)
{
  librdf_node     *query_predicate;
  librdf_node     *query_subject;
  librdf_node     *query_target;
  librdf_iterator *iterator;

  while (*predicates) {
    query_subject = librdf_new_node_from_uri (world, source_uri);
    query_predicate = librdf_new_node_from_uri_string (world, (const unsigned char *) *predicates);

    iterator = librdf_model_get_targets (model, query_subject, query_predicate);
    if (iterator)
      {
        while (!librdf_iterator_end (iterator))
          {
            gchar *value = NULL;
            query_target = (librdf_node *) librdf_iterator_get_object (iterator);
            if (query_target && librdf_node_is_literal (query_target))
              {
                value = g_strdup ((const gchar *) librdf_node_get_literal_value (query_target));
                //gexiv2_metadata_set_tag_string (metadata, tagname, value);
              }
            else if (query_target && librdf_node_is_resource (query_target))
              {
                value = g_strdup ((const gchar *) librdf_uri_as_string (librdf_node_get_uri (query_target)));
                //gexiv2_metadata_set_tag_string (metadata, tagname, value);
              }
            else if (query_target && librdf_node_is_blank (query_target))
              {
                g_printerr ("clipboard: blank nodes not supported at the moment.\n");
              }

            if (value != NULL)
              {
                if (seq_type)
                  {
                    gimp_metadata_append_tag_value (metadata, tagname, value);
                  }
                else
                  {
                    gexiv2_metadata_set_tag_string (metadata, tagname, value);
                  }
              }

            librdf_free_node (query_target);
            librdf_iterator_next (iterator);
          }
      }

    librdf_free_iterator (iterator);
    //librdf_free_node (query_subject); // FIXME: freeing this frees subject URI string too
    librdf_free_node (query_predicate);

    predicates++;
  }
}

GimpMetadata *
gimp_clipboard_get_metadata (Gimp *gimp)
{
  GtkClipboard     *clipboard;
  GdkAtom           atom;
  GimpMetadata     *metadata = NULL;

  librdf_world     *world;
  librdf_storage   *storage;
  librdf_model     *model;
  librdf_parser    *parser;
  librdf_uri       *uri;

  librdf_node      *source_object;
  librdf_node      *query_subject;
  librdf_node      *query_predicate;
  librdf_iterator  *source_iterator;
  librdf_uri       *source_uri;

  const gchar *predicates_contributor[] = { // unordered array
    "http://purl.org/dc/elements/1.1/contributor",
    "http://purl.org/dc/terms/contributor",
    NULL
  };
  const gchar *predicates_coverage[] = {
    "http://purl.org/dc/elements/1.1/coverage",
    "http://purl.org/dc/terms/coverage",
    NULL
  };
  const gchar *predicates_creator[] = { // ordered array
    "http://purl.org/dc/elements/1.1/creator",
    "http://purl.org/dc/terms/creator",
    NULL
  };
  const gchar *predicates_date[] = { // ordered array
    "http://purl.org/dc/elements/1.1/date",
    "http://purl.org/dc/terms/date",
    NULL
  };
  const gchar *predicates_description[] = {
    "http://purl.org/dc/elements/1.1/description",
    "http://purl.org/dc/terms/description",
    NULL
  };
  const gchar *predicates_format[] = {
    "http://purl.org/dc/elements/1.1/format",
    "http://purl.org/dc/terms/format",
    NULL
  };
  const gchar *predicates_identifier[] = {
    "http://purl.org/dc/elements/1.1/identifier",
    "http://purl.org/dc/terms/identifier",
    NULL
  };
  const gchar *predicates_language[] = { // unordered array
    "http://purl.org/dc/elements/1.1/language",
    "http://purl.org/dc/terms/language",
    NULL
  };
  const gchar *predicates_publisher[] = { // unordered array
    "http://purl.org/dc/elements/1.1/publisher",
    "http://purl.org/dc/terms/publisher",
    NULL
  };
  const gchar *predicates_relation[] = {
    "http://purl.org/dc/elements/1.1/relation",
    "http://purl.org/dc/terms/relation",
    NULL
  };
  const gchar *predicates_license[] = { // moved to rights
    "http://www.w3.org/1999/xhtml/vocab#license",
    "http://purl.org/dc/terms/license",
    "http://creativecommons.org/ns#license",
    NULL
  };
  const gchar *predicates_source[] = {
    "http://purl.org/dc/elements/1.1/source",
    "http://purl.org/dc/terms/source",
    NULL
  };
  const gchar *predicates_subject[] = {
    "http://purl.org/dc/elements/1.1/subject",
    "http://purl.org/dc/terms/subject",
    NULL
  };
  const gchar *predicates_title[] = {
    "http://purl.org/dc/elements/1.1/title",
    "http://purl.org/dc/terms/title",
    NULL
  };
  const gchar *predicates_type[] = {
    "http://purl.org/dc/elements/1.1/type",
    "http://purl.org/dc/terms/type",
    "http://www.w3.org/1999/02/22-rdf-syntax-ns#type",
    NULL
  };


  g_return_val_if_fail (GIMP_IS_GIMP (gimp), NULL);

  clipboard = gtk_clipboard_get_for_display (gdk_display_get_default (),
                                             GDK_SELECTION_CLIPBOARD);

  if (clipboard                                                      &&
      gtk_clipboard_get_owner (clipboard)         != G_OBJECT (gimp) &&
      (atom = gimp_clipboard_wait_for_rdf (gimp)) != GDK_NONE)
    {
      GtkSelectionData *selection_data;
      selection_data = gtk_clipboard_wait_for_contents (clipboard, atom);

      if (selection_data)
        {
          gchar *rdf_xml = g_utf16_to_utf8 (
            (const gunichar2 *) gtk_selection_data_get_data (selection_data),
            gtk_selection_data_get_length (selection_data),
            NULL,
            NULL,
            NULL
          );

          world = librdf_new_world ();
          librdf_world_open (world);

          // use base uri "about:this" to keep redland happy
          uri = librdf_new_uri (world, (const unsigned char *) "about:this");
          storage = librdf_new_storage (world, "memory", NULL, NULL);
          model = librdf_new_model (world, storage, NULL);
          parser = librdf_new_parser (world, "rdfxml", NULL, NULL);

          if (librdf_parser_parse_string_into_model (parser, (const unsigned char *) rdf_xml, uri, model))
            {
              g_printerr ("clipboard: error while parsing RDF metadata.\n");
              goto tidyrdf;
            }

          // get the URI for actual image from about="" (substituted above)
          query_subject = librdf_new_node_from_uri_string (world, (const unsigned char *) "about:this");
          query_predicate = librdf_new_node_from_uri_string (world, (const unsigned char *) "http://purl.org/dc/elements/1.1/source");

          source_iterator = librdf_model_get_targets (model, query_subject, query_predicate);
          if (!source_iterator)
            {
              g_printerr ("clipboard: couldn't create dc:source iterator.\n");
              goto tidyrdf;
            }

          source_object = (librdf_node *) librdf_iterator_get_object (source_iterator);
          if (!source_object)
            {
              g_printerr ("clipboard: dc:source object is NULL\n");
              goto tidyrdf;
            }

          // copy source URI for subsequent queries
          source_uri = librdf_new_uri_from_uri (librdf_node_get_uri (source_object));

          // init metadata
          metadata = gimp_metadata_new ();

          gimp_clipboard_set_metadata_from_rdf (metadata, world, model, source_uri,
            predicates_contributor, "Xmp.dc.contributor", FALSE);
          gimp_clipboard_set_metadata_from_rdf (metadata, world, model, source_uri,
            predicates_coverage, "Xmp.dc.coverage", FALSE);
          gimp_clipboard_set_metadata_from_rdf (metadata, world, model, source_uri,
            predicates_creator, "Xmp.dc.creator", TRUE);
          gimp_clipboard_set_metadata_from_rdf (metadata, world, model, source_uri,
            predicates_date, "Xmp.dc.date", FALSE);
          gimp_clipboard_set_metadata_from_rdf (metadata, world, model, source_uri,
            predicates_description, "Xmp.dc.description", FALSE);
          gimp_clipboard_set_metadata_from_rdf (metadata, world, model, source_uri,
            predicates_format, "Xmp.dc.format", FALSE);
          gimp_clipboard_set_metadata_from_rdf (metadata, world, model, source_uri,
            predicates_identifier, "Xmp.dc.identifier", FALSE);
          gimp_clipboard_set_metadata_from_rdf (metadata, world, model, source_uri,
            predicates_language, "Xmp.dc.language", FALSE);
          gimp_clipboard_set_metadata_from_rdf (metadata, world, model, source_uri,
            predicates_publisher, "Xmp.dc.publisher", FALSE);
          gimp_clipboard_set_metadata_from_rdf (metadata, world, model, source_uri,
            predicates_relation, "Xmp.dc.relation", FALSE);
          gimp_clipboard_set_metadata_from_rdf (metadata, world, model, source_uri,
            predicates_license, "Xmp.dc.rights", FALSE);
          gimp_clipboard_set_metadata_from_rdf (metadata, world, model, source_uri,
            predicates_source, "Xmp.dc.source", TRUE);
          gimp_clipboard_set_metadata_from_rdf (metadata, world, model, source_uri,
            predicates_subject, "Xmp.dc.subject", FALSE);
          gimp_clipboard_set_metadata_from_rdf (metadata, world, model, source_uri,
            predicates_title, "Xmp.dc.title", FALSE);
          gimp_clipboard_set_metadata_from_rdf (metadata, world, model, source_uri,
            predicates_type, "Xmp.dc.type", FALSE);
          
          tidyrdf:

          librdf_free_iterator (source_iterator);
          librdf_free_node (source_object);
          librdf_free_node (query_subject);
          librdf_free_node (query_predicate);

          librdf_free_uri(uri);
          librdf_free_parser(parser);
          librdf_free_model(model);
          librdf_free_storage(storage);
          librdf_free_world(world);
        }
    }

  return metadata;
}

/**
 * gimp_clipboard_get_svg:
 * @gimp: pointer to #Gimp
 * @svg_length: returns the size of the SVG stream in bytes
 *
 * Retrieves SVG data from %GDK_SELECTION_CLIPBOARD or from the global
 * SVG buffer of @gimp.
 *
 * The returned data needs to be freed when it's no longer needed.
 *
 * Return value: a reference to a #GimpBuffer or %NULL if there's no
 *               image data
 **/
gchar *
gimp_clipboard_get_svg (Gimp  *gimp,
                        gsize *svg_length)
{
  GimpClipboard *gimp_clip;
  GtkClipboard  *clipboard;
  GdkAtom        atom;
  gchar         *svg = NULL;

  g_return_val_if_fail (GIMP_IS_GIMP (gimp), NULL);
  g_return_val_if_fail (svg_length != NULL, NULL);

  clipboard = gtk_clipboard_get_for_display (gdk_display_get_default (),
                                             GDK_SELECTION_CLIPBOARD);

  if (clipboard                                                      &&
      gtk_clipboard_get_owner (clipboard)         != G_OBJECT (gimp) &&
      (atom = gimp_clipboard_wait_for_svg (gimp)) != GDK_NONE)
    {
      GtkSelectionData *data;

      gimp_set_busy (gimp);

      data = gtk_clipboard_wait_for_contents (clipboard, atom);

      if (data)
        {
          const guchar *stream;

          stream = gimp_selection_data_get_stream (data, svg_length);

          if (stream)
            svg = g_memdup (stream, *svg_length);

          gtk_selection_data_free (data);
        }

      gimp_unset_busy (gimp);
    }

  gimp_clip = gimp_clipboard_get (gimp);

  if (! svg && gimp_clip->svg)
    {
      svg = g_strdup (gimp_clip->svg);
      *svg_length = strlen (svg);
    }

  return svg;
}

/**
 * gimp_clipboard_get_curve:
 * @gimp: pointer to #Gimp
 *
 * Retrieves curve data from %GDK_SELECTION_CLIPBOARD or from the global
 * curve buffer of @gimp.
 *
 * The returned curve needs to be unref'ed when it's no longer needed.
 *
 * Return value: a reference to a #GimpCurve or %NULL if there's no
 *               curve data
 **/
GimpCurve *
gimp_clipboard_get_curve (Gimp *gimp)
{
  GimpClipboard *gimp_clip;
  GtkClipboard  *clipboard;
  GdkAtom        atom;
  GimpCurve     *curve = NULL;

  g_return_val_if_fail (GIMP_IS_GIMP (gimp), NULL);

  clipboard = gtk_clipboard_get_for_display (gdk_display_get_default (),
                                             GDK_SELECTION_CLIPBOARD);

  if (clipboard                                                        &&
      gtk_clipboard_get_owner (clipboard)           != G_OBJECT (gimp) &&
      (atom = gimp_clipboard_wait_for_curve (gimp)) != GDK_NONE)
    {
      GtkSelectionData *data;

      gimp_set_busy (gimp);

      data = gtk_clipboard_wait_for_contents (clipboard, atom);

      if (data)
        {
          curve = gimp_selection_data_get_curve (data);

          gtk_selection_data_free (data);
        }

      gimp_unset_busy (gimp);
    }

  gimp_clip = gimp_clipboard_get (gimp);

  if (! curve && gimp_clip->curve)
    curve = g_object_ref (gimp_clip->curve);

  return curve;
}

/**
 * gimp_clipboard_set_buffer:
 * @gimp:   pointer to #Gimp
 * @buffer: a #GimpBuffer, or %NULL.
 *
 * Offers the buffer in %GDK_SELECTION_CLIPBOARD.
 **/
void
gimp_clipboard_set_buffer (Gimp       *gimp,
                           GimpBuffer *buffer)
{
  GimpClipboard *gimp_clip;
  GtkClipboard  *clipboard;

  g_return_if_fail (GIMP_IS_GIMP (gimp));
  g_return_if_fail (buffer == NULL || GIMP_IS_BUFFER (buffer));

  clipboard = gtk_clipboard_get_for_display (gdk_display_get_default (),
                                             GDK_SELECTION_CLIPBOARD);
  if (! clipboard)
    return;

  gimp_clip = gimp_clipboard_get (gimp);

  gimp_clipboard_clear (gimp_clip);

  if (buffer)
    {
      gimp_clip->buffer = g_object_ref (buffer);

      gtk_clipboard_set_with_owner (clipboard,
                                    gimp_clip->target_entries,
                                    gimp_clip->n_target_entries,
                                    (GtkClipboardGetFunc) gimp_clipboard_send_buffer,
                                    (GtkClipboardClearFunc) NULL,
                                    G_OBJECT (gimp));

      /*  mark the first entry (image/png) as suitable for storing  */
      if (gimp_clip->n_target_entries > 0)
        gtk_clipboard_set_can_store (clipboard, gimp_clip->target_entries, 1);
    }
  else if (gtk_clipboard_get_owner (clipboard) == G_OBJECT (gimp))
    {
      gtk_clipboard_clear (clipboard);
    }
}

/**
 * gimp_clipboard_set_svg:
 * @gimp: pointer to #Gimp
 * @svg: a string containing the SVG data, or %NULL
 *
 * Offers SVG data in %GDK_SELECTION_CLIPBOARD.
 **/
void
gimp_clipboard_set_svg (Gimp        *gimp,
                        const gchar *svg)
{
  GimpClipboard *gimp_clip;
  GtkClipboard  *clipboard;

  g_return_if_fail (GIMP_IS_GIMP (gimp));

  clipboard = gtk_clipboard_get_for_display (gdk_display_get_default (),
                                             GDK_SELECTION_CLIPBOARD);
  if (! clipboard)
    return;

  gimp_clip = gimp_clipboard_get (gimp);

  gimp_clipboard_clear (gimp_clip);

  if (svg)
    {
      gimp_clip->svg = g_strdup (svg);

      gtk_clipboard_set_with_owner (clipboard,
                                    gimp_clip->svg_target_entries,
                                    gimp_clip->n_svg_target_entries,
                                    (GtkClipboardGetFunc) gimp_clipboard_send_svg,
                                    (GtkClipboardClearFunc) NULL,
                                    G_OBJECT (gimp));

      /*  mark the first entry (image/svg) as suitable for storing  */
      gtk_clipboard_set_can_store (clipboard, gimp_clip->svg_target_entries, 1);
    }
  else if (gtk_clipboard_get_owner (clipboard) == G_OBJECT (gimp))
    {
      gtk_clipboard_clear (clipboard);
    }
}

/**
 * gimp_clipboard_set_text:
 * @gimp: pointer to #Gimp
 * @text: a %NULL-terminated string in UTF-8 encoding
 *
 * Offers @text in %GDK_SELECTION_CLIPBOARD and %GDK_SELECTION_PRIMARY.
 **/
void
gimp_clipboard_set_text (Gimp        *gimp,
                         const gchar *text)
{
  GtkClipboard *clipboard;

  g_return_if_fail (GIMP_IS_GIMP (gimp));
  g_return_if_fail (text != NULL);

  gimp_clipboard_clear (gimp_clipboard_get (gimp));

  clipboard = gtk_clipboard_get_for_display (gdk_display_get_default (),
                                             GDK_SELECTION_CLIPBOARD);
  if (clipboard)
    gtk_clipboard_set_text (clipboard, text, -1);

  clipboard = gtk_clipboard_get_for_display (gdk_display_get_default (),
                                             GDK_SELECTION_PRIMARY);
  if (clipboard)
    gtk_clipboard_set_text (clipboard, text, -1);
}

/**
 * gimp_clipboard_set_curve:
 * @gimp: pointer to #Gimp
 * @curve: a #GimpCurve, or %NULL
 *
 * Offers curve data in %GDK_SELECTION_CLIPBOARD.
 **/
void
gimp_clipboard_set_curve (Gimp      *gimp,
                          GimpCurve *curve)
{
  GimpClipboard *gimp_clip;
  GtkClipboard  *clipboard;

  g_return_if_fail (GIMP_IS_GIMP (gimp));
  g_return_if_fail (curve == NULL || GIMP_IS_CURVE (curve));

  clipboard = gtk_clipboard_get_for_display (gdk_display_get_default (),
                                             GDK_SELECTION_CLIPBOARD);
  if (! clipboard)
    return;

  gimp_clip = gimp_clipboard_get (gimp);

  gimp_clipboard_clear (gimp_clip);

  if (curve)
    {
      gimp_clip->curve = g_object_ref (curve);

      gtk_clipboard_set_with_owner (clipboard,
                                    gimp_clip->curve_target_entries,
                                    gimp_clip->n_curve_target_entries,
                                    (GtkClipboardGetFunc) gimp_clipboard_send_curve,
                                    (GtkClipboardClearFunc) NULL,
                                    G_OBJECT (gimp));

      gtk_clipboard_set_can_store (clipboard, gimp_clip->curve_target_entries, 1);
    }
  else if (gtk_clipboard_get_owner (clipboard) == G_OBJECT (gimp))
    {
      gtk_clipboard_clear (clipboard);
    }
}


/*  private functions  */

static GimpClipboard *
gimp_clipboard_get (Gimp *gimp)
{
  return g_object_get_data (G_OBJECT (gimp), GIMP_CLIPBOARD_KEY);
}

static void
gimp_clipboard_clear (GimpClipboard *gimp_clip)
{
  if (gimp_clip->buffer)
    {
      g_object_unref (gimp_clip->buffer);
      gimp_clip->buffer = NULL;
    }

  if (gimp_clip->svg)
    {
      g_free (gimp_clip->svg);
      gimp_clip->svg = NULL;
    }

  if (gimp_clip->curve)
    {
      g_object_unref (gimp_clip->curve);
      gimp_clip->curve = NULL;
    }
}

static void
gimp_clipboard_free (GimpClipboard *gimp_clip)
{
  gint i;

  gimp_clipboard_clear (gimp_clip);

  g_slist_free (gimp_clip->pixbuf_formats);

  for (i = 0; i < gimp_clip->n_target_entries; i++)
    g_free ((gchar *) gimp_clip->target_entries[i].target);

  g_free (gimp_clip->target_entries);

  for (i = 0; i < gimp_clip->n_svg_target_entries; i++)
    g_free ((gchar *) gimp_clip->svg_target_entries[i].target);

  g_free (gimp_clip->svg_target_entries);

  for (i = 0; i < gimp_clip->n_curve_target_entries; i++)
    g_free ((gchar *) gimp_clip->curve_target_entries[i].target);

  g_free (gimp_clip->curve_target_entries);

  g_slice_free (GimpClipboard, gimp_clip);
}

static GdkAtom *
gimp_clipboard_wait_for_targets (Gimp *gimp,
                                 gint *n_targets)
{
  GtkClipboard *clipboard;

  clipboard = gtk_clipboard_get_for_display (gdk_display_get_default (),
                                             GDK_SELECTION_CLIPBOARD);

  if (clipboard)
    {
      GtkSelectionData *data;
      GdkAtom           atom = gdk_atom_intern_static_string ("TARGETS");

      data = gtk_clipboard_wait_for_contents (clipboard, atom);

      if (data)
        {
          GdkAtom  *targets;
          gboolean  success;

          success = gtk_selection_data_get_targets (data, &targets, n_targets);

          gtk_selection_data_free (data);

          if (success)
            {
              if (gimp->be_verbose)
                {
                  gint i;

                  for (i = 0; i < *n_targets; i++)
                    g_printerr ("clipboard: offered type: %s\n",
                                gdk_atom_name (targets[i]));

                  g_printerr ("\n");
                }

              return targets;
            }
        }
    }

  return NULL;
}

static GdkAtom
gimp_clipboard_wait_for_buffer (Gimp *gimp)
{
  GimpClipboard *gimp_clip = gimp_clipboard_get (gimp);
  GdkAtom       *targets;
  gint           n_targets;
  GdkAtom        result    = GDK_NONE;

  targets = gimp_clipboard_wait_for_targets (gimp, &n_targets);

  if (targets)
    {
      GSList *list;

      for (list = gimp_clip->pixbuf_formats; list; list = g_slist_next (list))
        {
          GdkPixbufFormat  *format = list->data;
          gchar           **mime_types;
          gchar           **type;

          if (gimp->be_verbose)
            g_printerr ("clipboard: checking pixbuf format '%s'\n",
                        gdk_pixbuf_format_get_name (format));

          mime_types = gdk_pixbuf_format_get_mime_types (format);

          for (type = mime_types; *type; type++)
            {
              gchar   *mime_type = *type;
              GdkAtom  atom      = gdk_atom_intern (mime_type, FALSE);
              gint     i;

              if (gimp->be_verbose)
                g_printerr ("  - checking mime type '%s'\n", mime_type);

              for (i = 0; i < n_targets; i++)
                {
                  if (targets[i] == atom)
                    {
                      result = atom;
                      break;
                    }
                }

              if (result != GDK_NONE)
                break;
            }

          g_strfreev (mime_types);

          if (result != GDK_NONE)
            break;
        }

      g_free (targets);
    }

  return result;
}

static GdkAtom
gimp_clipboard_wait_for_svg (Gimp *gimp)
{
  GdkAtom *targets;
  gint     n_targets;
  GdkAtom  result = GDK_NONE;

  targets = gimp_clipboard_wait_for_targets (gimp, &n_targets);

  if (targets)
    {
      GdkAtom svg_atom     = gdk_atom_intern_static_string ("image/svg");
      GdkAtom svg_xml_atom = gdk_atom_intern_static_string ("image/svg+xml");
      gint    i;

      for (i = 0; i < n_targets; i++)
        {
          if (targets[i] == svg_atom)
            {
              result = svg_atom;
              break;
            }
          else if (targets[i] == svg_xml_atom)
            {
              result = svg_xml_atom;
              break;
            }
        }

      g_free (targets);
    }

  return result;
}

static GdkAtom
gimp_clipboard_wait_for_curve (Gimp *gimp)
{
  GdkAtom *targets;
  gint     n_targets;
  GdkAtom  result = GDK_NONE;

  targets = gimp_clipboard_wait_for_targets (gimp, &n_targets);

  if (targets)
    {
      GdkAtom curve_atom = gdk_atom_intern_static_string ("application/x-gimp-curve");
      gint    i;

      for (i = 0; i < n_targets; i++)
        {
          if (targets[i] == curve_atom)
            {
              result = curve_atom;
              break;
            }
        }

      g_free (targets);
    }

  return result;
}

static GdkAtom
gimp_clipboard_wait_for_rdf (Gimp *gimp)
{
  GdkAtom *targets;
  gint     n_targets;
  GdkAtom  result = GDK_NONE;

  targets = gimp_clipboard_wait_for_targets (gimp, &n_targets);

  if (targets)
    {
      GdkAtom rdf_atom = gdk_atom_intern_static_string ("application/rdf+xml");
      gint    i;

      for (i = 0; i < n_targets; i++)
        {
          if (targets[i] == rdf_atom)
            {
              result = rdf_atom;
              break;
            }
        }

      g_free (targets);
    }

  return result;
}

static void
gimp_clipboard_send_buffer (GtkClipboard     *clipboard,
                            GtkSelectionData *selection_data,
                            guint             info,
                            Gimp             *gimp)
{
  GimpClipboard *gimp_clip = gimp_clipboard_get (gimp);
  GdkPixbuf     *pixbuf;

  gimp_set_busy (gimp);

  pixbuf = gimp_viewable_get_pixbuf (GIMP_VIEWABLE (gimp_clip->buffer),
                                     gimp_get_user_context (gimp),
                                     gimp_buffer_get_width (gimp_clip->buffer),
                                     gimp_buffer_get_height (gimp_clip->buffer));

  if (pixbuf)
    {
      if (gimp->be_verbose)
        g_printerr ("clipboard: sending pixbuf data as '%s'\n",
                    gimp_clip->target_entries[info].target);

      gtk_selection_data_set_pixbuf (selection_data, pixbuf);
    }
  else
    {
      g_warning ("%s: gimp_viewable_get_pixbuf() failed", G_STRFUNC);
    }

  gimp_unset_busy (gimp);
}

static void
gimp_clipboard_send_svg (GtkClipboard     *clipboard,
                         GtkSelectionData *selection_data,
                         guint             info,
                         Gimp             *gimp)
{
  GimpClipboard *gimp_clip = gimp_clipboard_get (gimp);

  gimp_set_busy (gimp);

  if (gimp_clip->svg)
    {
      if (gimp->be_verbose)
        g_printerr ("clipboard: sending SVG data as '%s'\n",
                    gimp_clip->svg_target_entries[info].target);

      gimp_selection_data_set_stream (selection_data,
                                      (const guchar *) gimp_clip->svg,
                                      strlen (gimp_clip->svg));
    }

  gimp_unset_busy (gimp);
}

static void
gimp_clipboard_send_curve (GtkClipboard     *clipboard,
                           GtkSelectionData *selection_data,
                           guint             info,
                           Gimp             *gimp)
{
  GimpClipboard *gimp_clip = gimp_clipboard_get (gimp);

  gimp_set_busy (gimp);

  if (gimp_clip->curve)
    {
      if (gimp->be_verbose)
        g_printerr ("clipboard: sending curve data as '%s'\n",
                    gimp_clip->curve_target_entries[info].target);

      gimp_selection_data_set_curve (selection_data, gimp_clip->curve);
    }

  gimp_unset_busy (gimp);
}
