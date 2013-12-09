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

#include <gegl.h>
#include <gtk/gtk.h>
#include <redland.h>

#include "libgimpwidgets/gimpwidgets.h"

#include "dialogs-types.h"

#include "core/gimpitem.h"
#include "core/gimpitemstack.h"
#include "core/gimp.h"
#include "core/gimpimage.h"
#include "core/gimpvocab.h"

#include "attribution-dialog.h"

#include "gimp-intl.h"

enum
{
  ATTRIB_COL_NAME = 0,
  ATTRIB_COL_VALUE,
  ATTRIB_NUM_COLS
};

static void
attrib_dialog_fill_model (GimpAttribution *attrib,
  librdf_node *subject, GtkTreeStore *model, GtkTreeIter *iter, GimpVocab *vocab);

static GtkWidget *
attrib_dialog_create_treeview (void);

static gchar *
attrib_dialog_get_property_value (GimpAttribution *attrib, gchar *uri);

static void
attrib_dialog_set_property_value (GimpAttribution *attrib, const gchar *uri, const gchar *value);

static void
attrib_dialog_response (GtkWidget *widget, gint response_id, GtkWidget *dialog);

static gchar *
attrib_dialog_get_property_value (GimpAttribution *attrib, gchar *uri)
{
  librdf_statement *partial_statement;
  librdf_statement *statement;
  librdf_stream *stream;
  librdf_world *world;
  librdf_model *model;
  librdf_node *node;
  gchar *value = NULL;

  world = (librdf_world *) gimp_attribution_get_world (attrib);
  model = (librdf_model *) gimp_attribution_get_model (attrib);

  partial_statement = librdf_new_statement_from_nodes (world,
    librdf_new_node_from_uri_string (world, (const unsigned char *) "about:this"),
    librdf_new_node_from_uri_string (world, (const unsigned char *) uri),
    NULL);

  stream = librdf_model_find_statements (model, partial_statement);
  statement = librdf_stream_get_object (stream);
  if (statement)
    {
      node = librdf_statement_get_object (statement);
      value = g_strdup ((gchar *) librdf_node_get_literal_value (node));
    }

  librdf_free_stream (stream);
  //librdf_free_statement (statement);
  //librdf_free_statement (partial_statement);

  return value;
}

static void
attrib_dialog_set_property_value (GimpAttribution *attrib, const gchar *uri, const gchar *value)
{
  librdf_statement *partial_statement;
  librdf_statement *statement;
  librdf_stream *stream;
  librdf_world *world;
  librdf_model *model;

  world = (librdf_world *) gimp_attribution_get_world (attrib);
  model = (librdf_model *) gimp_attribution_get_model (attrib);

  partial_statement = librdf_new_statement_from_nodes (world,
    librdf_new_node_from_uri_string (world, (const unsigned char *) "about:this"),
    librdf_new_node_from_uri_string (world, (const unsigned char *) uri),
    NULL);

  stream = librdf_model_find_statements (model, partial_statement);
  while (!librdf_stream_end (stream))
    {
      librdf_statement *statement = librdf_stream_get_object (stream);
      librdf_model_remove_statement (model, statement);
      //librdf_free_statement (statement);
      librdf_stream_next (stream);
    }

  librdf_free_stream (stream);
  //librdf_free_statement (partial_statement);

  statement = librdf_new_statement_from_nodes (world,
    librdf_new_node_from_uri_string (world, (const unsigned char *) "about:this"),
    librdf_new_node_from_uri_string (world, (const unsigned char *) uri),
    librdf_new_node_from_literal (world, (const unsigned char *) value, NULL, 0));

  librdf_model_add_statement (model, statement);
}

static void
attrib_dialog_fill_model (GimpAttribution *attrib,
  librdf_node *subject, GtkTreeStore *model, GtkTreeIter *iter, GimpVocab *vocab)
{
  librdf_stream *stream;
  librdf_statement *partial_statement;
  librdf_node *dc_source = librdf_new_node_from_uri_string (
    (librdf_world *) gimp_attribution_get_world (attrib),
    (guchar *) "http://purl.org/dc/elements/1.1/source");
  partial_statement = librdf_new_statement_from_nodes (
    (librdf_world *) gimp_attribution_get_world (attrib),
    librdf_new_node_from_node (subject), NULL, NULL);

  stream = librdf_model_find_statements (
    (librdf_model *) gimp_attribution_get_model (attrib),
    partial_statement);

  while (!librdf_stream_end (stream))
    {
      librdf_statement *statement = librdf_stream_get_object (stream);

      librdf_node *object = librdf_statement_get_object (statement);
      librdf_node *predicate = librdf_statement_get_predicate (statement);

      GtkTreeIter object_iter;

      gchar *value = NULL;
      gchar *name = NULL;

      name = gimp_vocab_get_short_uri (vocab, (gchar *) librdf_uri_as_string (librdf_node_get_uri (predicate)));

      if (librdf_node_is_literal (object))
        {
          value = g_strdup ((const gchar *) librdf_node_get_literal_value (object));
          gtk_tree_store_append (model, &object_iter, iter);
          gtk_tree_store_set (model, &object_iter,
                              ATTRIB_COL_NAME, name,
                              ATTRIB_COL_VALUE, value,
                             -1);
        }
      else if (librdf_node_is_resource (object))
        {
          value = gimp_vocab_get_short_uri (vocab, (gchar *) librdf_uri_as_string (librdf_node_get_uri (object)));
          gtk_tree_store_append (model, &object_iter, iter);
          gtk_tree_store_set (model, &object_iter,
                              ATTRIB_COL_NAME, name,
                              ATTRIB_COL_VALUE, value,
                             -1);

          if (librdf_node_equals (predicate, dc_source) != 0)
            {
              gtk_tree_store_append (model, &object_iter, NULL);
              gtk_tree_store_set (model, &object_iter,
                      ATTRIB_COL_NAME, "(source)",
                      ATTRIB_COL_VALUE, value,
                       -1);
              attrib_dialog_fill_model (attrib, object, model, &object_iter, vocab);
            }
        }
      else if (librdf_node_is_blank (object))
        {
          value = "Blank node";
          gtk_tree_store_append (model, &object_iter, iter);
          gtk_tree_store_set (model, &object_iter,
                              ATTRIB_COL_NAME, name,
                              ATTRIB_COL_VALUE, value,
                             -1);
          attrib_dialog_fill_model (attrib, object, model, &object_iter, vocab);
        }

      librdf_stream_next (stream);
    }

  librdf_free_stream (stream);
  librdf_free_statement (partial_statement);
}

static GtkWidget *
attrib_dialog_create_treeview (void)
{
  GtkWidget *tv;
  GtkTreeStore *model;
  GtkCellRenderer     *renderer;
  GtkTreeViewColumn   *column;

  model = gtk_tree_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
  tv = gtk_tree_view_new_with_model (GTK_TREE_MODEL (model));
  g_object_unref (model);

  // column 1
  column = gtk_tree_view_column_new ();
  gtk_tree_view_column_set_title (column, "Property");
  gtk_tree_view_append_column (GTK_TREE_VIEW (tv), column);
  renderer = gtk_cell_renderer_text_new();
  gtk_tree_view_column_pack_start(column, renderer, TRUE);
  gtk_tree_view_column_add_attribute(column, renderer, "text", ATTRIB_COL_NAME);

  gtk_tree_view_column_set_sort_column_id (column, ATTRIB_COL_NAME);
  gtk_tree_view_column_set_sort_order (column, GTK_SORT_ASCENDING);

  // column 2
  column = gtk_tree_view_column_new ();
  gtk_tree_view_column_set_title (column, "Value");
  gtk_tree_view_append_column (GTK_TREE_VIEW (tv), column);
  renderer = gtk_cell_renderer_text_new();
  gtk_tree_view_column_pack_start(column, renderer, TRUE);
  gtk_tree_view_column_add_attribute(column, renderer, "text", ATTRIB_COL_VALUE);

  return tv;
}

/*  public functions  */

GtkWidget *
attribution_dialog_new (GimpImage *image, GtkWidget *parent)
{
  GtkWidget           *dialog;
  GtkWidget           *vbox;
  GtkWidget           *sw;
  GtkWidget           *notebook;
  GtkTreeIter          iter;
  librdf_node         *subject;
  GimpContainer       *container;
  GList               *list;
  GimpVocab           *vocab = gimp_vocab_new ();
  guint                i;

  g_return_val_if_fail (GIMP_IS_IMAGE (image), NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (parent), NULL);

  dialog = gimp_dialog_new (_("RDF metadata"),
                            "gimp-rdf-metadata-dialog",
                            parent, 0,
                            gimp_standard_help_func,
                            NULL,
                            GTK_STOCK_SAVE, GTK_RESPONSE_OK,
                            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                            NULL);

  g_signal_connect (dialog, "response",
                    G_CALLBACK (attrib_dialog_response),
                    dialog);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);
  gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))),
                      vbox, TRUE, TRUE, 0);
  gtk_widget_show (vbox);

  notebook = gtk_notebook_new ();
  gtk_container_add (GTK_CONTAINER (vbox), notebook);
  gtk_widget_show (notebook);

  // display (edit) image metadata

  {
    GtkWidget *table;
    GtkWidget *label;
    GtkWidget *title_entry;
    GtkWidget *creator_entry;

    table = gtk_table_new (2, 2, FALSE);
    gtk_notebook_append_page (GTK_NOTEBOOK (notebook), table, gtk_label_new ("Image"));
    gtk_table_set_row_spacings (GTK_TABLE (table), 12);
    gtk_table_set_col_spacings (GTK_TABLE (table), 12);
    gtk_container_set_border_width (GTK_CONTAINER (table), 12);

    gtk_table_attach (GTK_TABLE (table), label = gtk_label_new ("Title:"), 0, 1, 0, 1,
      GTK_FILL, 0, 0, 0);
    gtk_misc_set_alignment (GTK_MISC (label), 0, 0);
    gtk_table_attach (GTK_TABLE (table), label = gtk_label_new ("Creator:"), 0, 1, 1, 2,
      GTK_FILL, 0, 0, 0);
    gtk_misc_set_alignment (GTK_MISC (label), 0, 0);

    gtk_table_attach (GTK_TABLE (table), title_entry = gtk_entry_new (), 1, 2, 0, 1,
      GTK_EXPAND | GTK_FILL, 0, 0, 0);
    gtk_table_attach (GTK_TABLE (table), creator_entry = gtk_entry_new (), 1, 2, 1, 2,
      GTK_EXPAND | GTK_FILL, 0, 0, 0);

    gtk_entry_set_text (GTK_ENTRY (title_entry),
      attrib_dialog_get_property_value (gimp_image_get_attribution (image),
        "http://purl.org/dc/terms/title"));
    gtk_entry_set_text (GTK_ENTRY (creator_entry),
      attrib_dialog_get_property_value (gimp_image_get_attribution (image),
        "http://purl.org/dc/terms/creator"));

    g_object_set_data (G_OBJECT (dialog), "title-entry", title_entry);
    g_object_set_data (G_OBJECT (dialog), "creator-entry", creator_entry);
    g_object_set_data (G_OBJECT (dialog), "image", image);

    gtk_widget_show_all (table);
  }

  // display layers metadata

  container = gimp_image_get_layers (image);

  for (list = gimp_item_stack_get_item_iter (GIMP_ITEM_STACK (container)), i = 0;
       list;
       list = g_list_next (list), i++)
    {
      GimpLayer *layer = list->data;
      GtkTreeStore        *model;
      GtkWidget           *tv;
      GimpAttribution     *attrib;

      attrib = gimp_item_get_attribution (GIMP_ITEM (layer));

      sw = gtk_scrolled_window_new (NULL, NULL);
      //gtk_container_add (GTK_CONTAINER (vbox), sw);
      gtk_notebook_append_page (GTK_NOTEBOOK (notebook), sw,
        gtk_label_new (g_strdup_printf("Layer %d", i)));
      gtk_widget_show (sw);

      tv = attrib_dialog_create_treeview ();
      gtk_container_add (GTK_CONTAINER (sw), tv);
      gtk_widget_show (tv);

      model = GTK_TREE_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (tv)));

       // find root node
      subject = librdf_new_node_from_uri_string (
        (librdf_world *) gimp_attribution_get_world (attrib),
        (const unsigned char *) "about:this");
      gtk_tree_store_append (model, &iter, NULL);
      gtk_tree_store_set (model, &iter,
                          ATTRIB_COL_NAME, "(default)",
                          ATTRIB_COL_VALUE, NULL,
                          -1);

      attrib_dialog_fill_model (attrib, subject, model, &iter, vocab);
      gtk_tree_view_expand_all (GTK_TREE_VIEW (tv));
    }

  //gimp_attribution_combine (merge_attrib, gimp_image_get_attribution (image));
  gtk_window_set_default_size (GTK_WINDOW (dialog), 600, 600);
  return dialog;
}

static void
attrib_dialog_response (GtkWidget *widget, gint response_id, GtkWidget *dialog)
{
  if (response_id == GTK_RESPONSE_OK)
    {
      GtkEntry *title_entry;
      GtkEntry *creator_entry;
      GimpImage *image;
      GimpAttribution *attrib;

      title_entry = g_object_get_data (G_OBJECT (dialog), "title-entry");
      creator_entry = g_object_get_data (G_OBJECT (dialog), "creator-entry");
      image = g_object_get_data (G_OBJECT (dialog), "image");
      attrib = gimp_image_get_attribution (image);
      
      attrib_dialog_set_property_value (attrib,
        "http://purl.org/dc/terms/title", gtk_entry_get_text (title_entry));
      attrib_dialog_set_property_value (attrib,
        "http://purl.org/dc/terms/creator", gtk_entry_get_text (creator_entry));
    }
  gtk_widget_destroy (dialog);
}