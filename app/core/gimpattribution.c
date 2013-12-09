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

#include <redland.h>
#include "gimpattribution.h"

struct _GimpAttributionPrivate
{
  librdf_world    *world;
  librdf_storage  *storage;
  librdf_model    *model;
  librdf_parser   *parser;
};

G_DEFINE_TYPE (GimpAttribution, gimp_attribution, G_TYPE_OBJECT);

static void
gimp_attribution_init (GimpAttribution *attrib)
{
  attrib->priv = G_TYPE_INSTANCE_GET_PRIVATE (attrib, GIMP_TYPE_ATTRIBUTION, GimpAttributionPrivate);

  attrib->priv->world = librdf_new_world();
  librdf_world_open (attrib->priv->world);
  //attrib->priv->storage = librdf_new_storage (attrib->priv->world, "memory", NULL, NULL);
  attrib->priv->storage = librdf_new_storage (attrib->priv->world, "memory", NULL, NULL);
  attrib->priv->model = librdf_new_model (attrib->priv->world, attrib->priv->storage, NULL);
  attrib->priv->parser = librdf_new_parser (attrib->priv->world, "rdfxml", NULL, NULL);
}

static void
gimp_attribution_finalize (GObject *object)
{
  GimpAttribution *attrib = GIMP_ATTRIBUTION(object);

  librdf_free_world(attrib->priv->world);

  G_OBJECT_CLASS (gimp_attribution_parent_class)->finalize (object);
}

static void
gimp_attribution_class_init (GimpAttributionClass *klass)
{
  GObjectClass* object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (GimpAttributionPrivate));

  object_class->finalize = gimp_attribution_finalize;
}


GimpAttribution*
gimp_attribution_new (void)
{
  GimpAttribution *attrib;

  attrib = g_object_new (GIMP_TYPE_ATTRIBUTION, NULL);

  return attrib;
}

GimpAttribution*
gimp_attribution_new2 (GimpAttribution *attrib1, GimpAttribution *attrib2)
{
  GimpAttribution *attrib;

  attrib = g_object_new (GIMP_TYPE_ATTRIBUTION, NULL);
  gimp_attribution_combine (attrib, attrib1);
  gimp_attribution_combine (attrib, attrib2);

  return attrib;
}

static gboolean
gimp_attribution_load_from_string_with_uri (GimpAttribution *attrib,
                                            const gchar *rdf_xml,
                                            librdf_uri *uri)
{
  int error;
  g_return_val_if_fail (GIMP_IS_ATTRIBUTION (attrib), FALSE);

  error = librdf_parser_parse_string_into_model (attrib->priv->parser,
    (const unsigned char *) rdf_xml,
    uri,
    attrib->priv->model);

  return error == 0;
}

static gboolean
gimp_attribution_load_from_file_with_uri (GimpAttribution *attrib,
                                          const gchar *filename,
                                          librdf_uri *uri)
{
  int        error;
  gboolean   result;
  gchar     *contents;
  gsize      length;

  g_return_val_if_fail (GIMP_IS_ATTRIBUTION (attrib), FALSE);

  result = g_file_get_contents (filename, &contents, &length, NULL);

  if (result) {
    error = librdf_parser_parse_counted_string_into_model (attrib->priv->parser,
      (const unsigned char *) contents,
      length,
      uri,
      attrib->priv->model);
    return error == 0;
  }

  return FALSE;
}

gboolean
gimp_attribution_load_from_string (GimpAttribution *attrib, const gchar *rdf_xml)
{
  librdf_uri *uri;
  gboolean result;

  g_return_val_if_fail (GIMP_IS_ATTRIBUTION (attrib), FALSE);

  uri = librdf_new_uri (attrib->priv->world, (const unsigned char *) "about:this");
  result = gimp_attribution_load_from_string_with_uri (attrib, rdf_xml, uri);

  librdf_free_uri(uri);
  return result;
}

gboolean
gimp_attribution_load_from_file (GimpAttribution *attrib,
                                 const gchar *filename)
{
  librdf_uri *uri;
  gboolean result;

  g_return_val_if_fail (GIMP_IS_ATTRIBUTION (attrib), FALSE);

  uri = librdf_new_uri (attrib->priv->world, (const unsigned char *) "about:this");
  result = gimp_attribution_load_from_file_with_uri (attrib, filename, uri);

  librdf_free_uri(uri);
  return result;
}

// packet - as returned by gexiv2_metadata_get_xmp_packet()
gboolean
gimp_attribution_load_from_xmp (GimpAttribution *attrib, const gchar *packet, gchar *base_uri)
{
  gchar   *rdf_start;
  gchar   *rdf_end;
  gchar   *rdf;
  gboolean result;
  librdf_uri *uri;

  g_return_val_if_fail (GIMP_IS_ATTRIBUTION (attrib), FALSE);
  g_return_val_if_fail (packet != NULL, FALSE);

  rdf_start = g_strstr_len (packet, -1, "<rdf:RDF");
  rdf_end = g_strrstr (packet, "</rdf:RDF>") + 10;

  rdf = g_strndup (rdf_start, rdf_end - rdf_start);
  g_return_if_fail (rdf != NULL);

  librdf_model_add (attrib->priv->model,
    librdf_new_node_from_uri_string (attrib->priv->world, (const unsigned char *) "about:this"),
    librdf_new_node_from_uri_string (attrib->priv->world, (const unsigned char *) "http://purl.org/dc/elements/1.1/source"),
    librdf_new_node_from_uri_string (attrib->priv->world, (const unsigned char *) base_uri));

  uri = librdf_new_uri (attrib->priv->world, (const unsigned char *) base_uri);
  result = gimp_attribution_load_from_string_with_uri (attrib, rdf, uri);

  g_free(rdf);
  return result;
}

// serialize attribution, optionally stamping it with the image attrib (can be NULL)
gchar *
gimp_attribution_serialize_rdf (GimpAttribution *attrib, GimpAttribution *image_attrib)
{
  librdf_uri *uri_dc;
  librdf_uri *uri_dcterms;
  librdf_uri *uri_cc;
  librdf_uri *uri_xhv;
  librdf_uri *uri_og;
  librdf_uri *feature_uri;
  librdf_uri *base_uri;
  librdf_node *feature_node;

  librdf_model *serialize_model;
  librdf_storage *serialize_storage;
  librdf_stream *orig_stream;

  librdf_serializer *serializer;
  unsigned char *result;

  g_return_val_if_fail (GIMP_IS_ATTRIBUTION (attrib), NULL);

  uri_dc = librdf_new_uri (attrib->priv->world,
    (const unsigned char *) "http://purl.org/dc/elements/1.1/");
  uri_dcterms = librdf_new_uri (attrib->priv->world,
    (const unsigned char *) "http://purl.org/dc/terms/");
  uri_cc = librdf_new_uri (attrib->priv->world,
    (const unsigned char *) "http://creativecommons.org/ns#");
  uri_xhv = librdf_new_uri (attrib->priv->world,
    (const unsigned char *) "http://www.w3.org/1999/xhtml/vocab#");
  uri_og = librdf_new_uri (attrib->priv->world,
    (const unsigned char *) "http://ogp.me/ns#");

  // copy the original model, in case we'd like to add clipboard wrapper
  serialize_storage = librdf_new_storage (attrib->priv->world, "memory", NULL, NULL);
  serialize_model = librdf_new_model (attrib->priv->world, serialize_storage, NULL);

  orig_stream = librdf_model_as_stream (attrib->priv->model);
  librdf_model_add_statements (serialize_model, orig_stream);
  librdf_free_stream (orig_stream);

  if (image_attrib != NULL)
    {
      librdf_stream *other_stream;

      base_uri = librdf_new_uri (attrib->priv->world, (const unsigned char *) "about:that");
      librdf_model_add (serialize_model,
        librdf_new_node_from_uri_string (attrib->priv->world, (const unsigned char *) "about:that"),
        librdf_new_node_from_uri_string (attrib->priv->world, (const unsigned char *) "http://purl.org/dc/elements/1.1/source"),
        librdf_new_node_from_uri_string (attrib->priv->world, (const unsigned char *) "about:this"));

      other_stream = librdf_model_as_stream (image_attrib->priv->model);
      librdf_model_add_statements (serialize_model, other_stream);
      librdf_free_stream (other_stream);

      /*librdf_model_add (serialize_model,
        librdf_new_node_from_uri_string (attrib->priv->world, (const unsigned char *) "about:this"),
        librdf_new_node_from_uri_string (attrib->priv->world, (const unsigned char *) "http://purl.org/dc/terms/title"),
        librdf_new_node_from_literal (attrib->priv->world, (const unsigned char *) "GIMP image", NULL, 0));
      librdf_model_add (serialize_model,
        librdf_new_node_from_uri_string (attrib->priv->world, (const unsigned char *) "about:this"),
        librdf_new_node_from_uri_string (attrib->priv->world, (const unsigned char *) "http://purl.org/dc/terms/creator"),
        librdf_new_node_from_literal (attrib->priv->world, (const unsigned char *) "GIMP user", NULL, 0));*/
    }
  else
    {
      base_uri = librdf_new_uri (attrib->priv->world, (const unsigned char *) "about:this");
    }

  serializer = librdf_new_serializer (attrib->priv->world, "rdfxml-abbrev", NULL, NULL);

  librdf_serializer_set_feature (serializer,
    feature_uri = librdf_new_uri (attrib->priv->world, (const unsigned char *) "http://feature.librdf.org/raptor-relativeURIs"),
    feature_node = librdf_new_node_from_literal (attrib->priv->world, (const unsigned char *) "1", NULL, 0));
  librdf_free_uri (feature_uri);
  librdf_free_node (feature_node);

   librdf_serializer_set_feature (serializer,
    feature_uri = librdf_new_uri (attrib->priv->world, (const unsigned char *) "http://feature.librdf.org/raptor-writeBaseURI"),
    feature_node = librdf_new_node_from_literal (attrib->priv->world, (const unsigned char *) "0", NULL, 0));
  librdf_free_uri (feature_uri);
  librdf_free_node (feature_node);

  librdf_serializer_set_namespace (serializer, uri_dc, "dc");
  librdf_serializer_set_namespace (serializer, uri_dcterms, "dcterms");
  librdf_serializer_set_namespace (serializer, uri_cc, "cc");
  librdf_serializer_set_namespace (serializer, uri_xhv, "xhv");
  librdf_serializer_set_namespace (serializer, uri_og, "og");

  result = librdf_serializer_serialize_model_to_string (serializer, base_uri, serialize_model);

  librdf_free_model (serialize_model);
  librdf_free_serializer (serializer);
  librdf_free_uri (base_uri);
  librdf_free_uri (uri_dc);
  librdf_free_uri (uri_dcterms);
  librdf_free_uri (uri_cc);
  librdf_free_uri (uri_xhv);
  librdf_free_uri (uri_og);

  return (gchar *) result;
}

void
gimp_attribution_combine (GimpAttribution *attrib, GimpAttribution *other)
{
  librdf_stream *other_stream;

  g_return_if_fail (GIMP_IS_ATTRIBUTION (attrib));
  g_return_if_fail (GIMP_IS_ATTRIBUTION (other));

  other_stream = librdf_model_as_stream (other->priv->model);
  librdf_model_add_statements (attrib->priv->model, other_stream);
  librdf_free_stream (other_stream);
}

void
gimp_attribution_combine_check (GimpAttribution *attrib, GimpAttribution *other)
{
  librdf_stream *other_stream;
  librdf_statement *other_statement;

  g_return_if_fail (GIMP_IS_ATTRIBUTION (attrib));
  g_return_if_fail (GIMP_IS_ATTRIBUTION (other));

  other_stream = librdf_model_as_stream (other->priv->model);
  while (!librdf_stream_end (other_stream))
    {
      other_statement = librdf_stream_get_object (other_stream);

      if (!librdf_model_contains_statement (other->priv->model, other_statement))
        {
          librdf_model_add_statement (other->priv->model, other_statement);
        }
      librdf_stream_next (other_stream);
    }
  librdf_model_add_statements (attrib->priv->model, other_stream);
  librdf_free_stream (other_stream);
}

// returns TRUE if subject_uri has properties related to attribution
static gboolean
gimp_attribution_has_attribution_subject (GimpAttribution *attrib,
                                          librdf_uri *subject_uri)
{
  gchar *attrib_predicates[] = {
    // contributor
    "http://purl.org/dc/elements/1.1/contributor",
    "http://purl.org/dc/terms/contributor",
    // creator
    "http://purl.org/dc/elements/1.1/creator",
    "http://purl.org/dc/terms/creator",
    // license
    "http://www.w3.org/1999/xhtml/vocab#license",
    "http://purl.org/dc/terms/license",
    "http://creativecommons.org/ns#license",
    NULL
  };

  librdf_node  *subject_node;
  librdf_uri   *subject_uri_local;
  librdf_node  *predicate_node;
  librdf_uri   *predicate_uri;
  gboolean     result = FALSE;
  guint        i;

  librdf_statement *partial_statement;
  librdf_stream    *stream;

  g_return_val_if_fail (GIMP_IS_ATTRIBUTION (attrib), FALSE);

  for (i = 0; i < g_strv_length (attrib_predicates); i++) {
    subject_uri_local = librdf_new_uri_from_uri (subject_uri);
    subject_node = librdf_new_node_from_uri (attrib->priv->world, subject_uri_local);

    predicate_uri = librdf_new_uri (attrib->priv->world, (const unsigned char *) attrib_predicates[i]);
    predicate_node = librdf_new_node_from_uri (attrib->priv->world, predicate_uri);

    partial_statement = librdf_new_statement (attrib->priv->world);
    librdf_statement_set_subject (partial_statement, subject_node);
    librdf_statement_set_predicate (partial_statement, predicate_node);

    stream = librdf_model_find_statements (attrib->priv->model, partial_statement);
    if (librdf_stream_get_object (stream) != NULL)
      result = TRUE;

    librdf_free_stream (stream);
    librdf_free_statement (partial_statement);
  }

  return result;
}

// returns TRUE if the default node or any dc:source has properties related to attribution
gboolean
gimp_attribution_has_attribution (GimpAttribution *attrib)
{
  librdf_uri   *subject_uri;
  librdf_node  *subject_node;
  librdf_node  *predicate_node;

  librdf_statement *partial_statement;
  librdf_stream    *stream;

  gboolean result = FALSE;

  g_return_val_if_fail (GIMP_IS_ATTRIBUTION (attrib), FALSE);

  subject_uri = librdf_new_uri (attrib->priv->world, (const unsigned char *) "about:this");
  if (gimp_attribution_has_attribution_subject (attrib, subject_uri)) {
    librdf_free_uri (subject_uri);
    return TRUE;
  }

  subject_node = librdf_new_node_from_uri (attrib->priv->world, subject_uri);
  predicate_node = librdf_new_node_from_uri_string (attrib->priv->world,
    (const unsigned char *) "http://purl.org/dc/elements/1.1/source");

  partial_statement = librdf_new_statement (attrib->priv->world);
  librdf_statement_set_subject (partial_statement, subject_node);
  librdf_statement_set_predicate (partial_statement, predicate_node);

  stream = librdf_model_find_statements (attrib->priv->model, partial_statement);

  while (!librdf_stream_end (stream)) {
    librdf_statement *statement = librdf_stream_get_object (stream);
    librdf_node *source_object = librdf_statement_get_object (statement);
    librdf_uri *source_uri = librdf_node_get_uri (source_object);

    if (gimp_attribution_has_attribution_subject (attrib, source_uri))
      result = TRUE;
    librdf_stream_next (stream);
  }

  librdf_free_stream (stream);
  librdf_free_statement (partial_statement);

  return result;
}

gboolean
gimp_attribution_is_empty (GimpAttribution *attrib)
{
  g_return_val_if_fail (GIMP_IS_ATTRIBUTION (attrib), FALSE);
  return librdf_model_size (attrib->priv->model) == 0;
}


static void
gimp_metadata_append_tag_value (GExiv2Metadata *metadata,
                                const gchar *tagname,
                                gchar *value)
{
  gchar** values;
  gchar** temp;
  guint length = 1;
  guint i;

  values = gexiv2_metadata_get_tag_multiple (metadata, tagname);
  while (values[length - 1] != NULL)
    {
      // don't add value, if already in the list
      if (g_strcmp0 (values[length - 1], value) == 0)
        return;
      length++;
    }

  temp = g_new (gchar*, length + 1);

  for (i = 0; i < length - 1; i++) {
    temp[i] = values[i];
  }

  temp[length - 1] = g_strdup(value);
  temp[length] = NULL;

  gexiv2_metadata_set_tag_multiple (metadata, tagname, (const gchar **) temp);
}

void
gimp_attribution_write_metadata (GimpAttribution *attrib, GExiv2Metadata *metadata)
{
  gchar *source_query_string = \
    "PREFIX dc: <http://purl.org/dc/elements/1.1/>"
    "PREFIX dcterms: <http://purl.org/dc/terms/>"
    "PREFIX rdf: <http://www.w3.org/1999/02/22-rdf-syntax-ns#>"
    ""
    "SELECT ?subject ?label WHERE {"
    "    {"
    "        { ?subject dc:source ?label . }"
    "        UNION"
    "        { ?subject dcterms:source ?label . }"
    "        FILTER(isLiteral(?label) || isURI(?label))"
    "    }"
    "    UNION"
    "    {"
    "        { ?subject dc:source ?node . }"
    "        UNION"
    "        { ?subject dcterms:source ?node . }"
    "        ?node a rdf:Seq ."
    "        ?node ?pred ?label ."
    "        FILTER(isLiteral(?label) || isURI(?label))"
    "        FILTER(?label != rdf:Seq)"
    "    }"
    "    UNION"
    "    {"
    "        { ?subject dc:source ?node . }"
    "        UNION"
    "        { ?subject dcterms:source ?node . }"
    "        ?node a rdf:Bag ."
    "        ?node ?pred ?label ."
    "        FILTER(isLiteral(?label) || isURI(?label))"
    "        FILTER(?label != rdf:Bag)"
    "    }"
    "    UNION"
    "    {"
    "        { ?subject dc:source ?node . }"
    "        UNION"
    "        { ?subject dcterms:source ?node . }"
    "        ?node a rdf:Alt ."
    "        ?node rdf:_1 ?label ."
    "        FILTER(isLiteral(?label) || isURI(?label))"
    "    }"
    "}";

  gchar *creator_query_string = \
    "PREFIX dc: <http://purl.org/dc/elements/1.1/>"
    "PREFIX dcterms: <http://purl.org/dc/terms/>"
    "PREFIX rdf: <http://www.w3.org/1999/02/22-rdf-syntax-ns#>"
    "PREFIX cc: <http://creativecommons.org/ns#>"
    ""
    "SELECT ?subject ?label WHERE {"
    "    {"
    "        { ?subject dc:creator ?label . }"
    "        UNION"
    "        { ?subject dcterms:creator ?label . }"
    "        UNION"
    "        { ?subject <twitter:creator> ?label . }"
    "        UNION"
    "        { ?subject <cc:attributionName> ?label . }"
    "        FILTER(isLiteral(?label) || isURI(?label))"
    "    }"
    "    UNION"
    "    {"
    "        { ?subject dc:creator ?node . }"
    "        UNION"
    "        { ?subject dcterms:creator ?node . }"
    "        ?node a rdf:Seq ."
    "        ?node ?pred ?label ."
    "        FILTER(isLiteral(?label) || isURI(?label))"
    "        FILTER(?label != rdf:Seq)"
    "    }"
    "    UNION"
    "    {"
    "        { ?subject dc:creator ?node . }"
    "        UNION"
    "        { ?subject dcterms:creator ?node . }"
    "        ?node a rdf:Bag ."
    "        ?node ?pred ?label ."
    "        FILTER(isLiteral(?label) || isURI(?label))"
    "        FILTER(?label != rdf:Bag)"
    "    }"
    "    UNION"
    "    {"
    "        { ?subject dc:creator ?node . }"
    "        UNION"
    "        { ?subject dcterms:creator ?node . }"
    "        ?node a rdf:Alt ."
    "        ?node rdf:_1 ?label ."
    "        FILTER(isLiteral(?label) || isURI(?label))"
    "    }"
    "}";

  librdf_query *query;
  librdf_query_results *results;

  // iterate through sources
  gexiv2_metadata_clear_tag (metadata,  "Xmp.dc.source");

  query = librdf_new_query (attrib->priv->world, "sparql", NULL,
                            (const unsigned char *) source_query_string, NULL);
  results = librdf_model_query_execute(attrib->priv->model, query);

  while (!librdf_query_results_finished (results))
    {
      librdf_node *node = librdf_query_results_get_binding_value_by_name (results, "label");
      gchar *value = NULL;

      if (librdf_node_is_literal (node))
          value = g_strdup ((const gchar *) librdf_node_get_literal_value (node));
      else if (librdf_node_is_resource (node))
          value = g_strdup ((const gchar *) librdf_uri_as_string (librdf_node_get_uri (node)));
      else if (librdf_node_is_blank (node))
          g_printerr ("shouldn't happen. blank nodes are omitted in the query\n");

      if (value != NULL) {
        gimp_metadata_append_tag_value (metadata, "Xmp.dc.source", value);
        g_free (value);
      }

      librdf_query_results_next (results);
    }

  librdf_free_query_results(results);
  librdf_free_query(query);

  // iterate through creators
  gexiv2_metadata_clear_tag (metadata,  "Xmp.dc.creator");

  query = librdf_new_query (attrib->priv->world, "sparql", NULL,
                            (const unsigned char *) creator_query_string, NULL);
  results = librdf_model_query_execute(attrib->priv->model, query);

  while (!librdf_query_results_finished (results))
    {
      librdf_node *node = librdf_query_results_get_binding_value_by_name (results, "label");
      gchar *value = NULL;

      if (librdf_node_is_literal (node))
          value = g_strdup ((const gchar *) librdf_node_get_literal_value (node));
      else if (librdf_node_is_resource (node))
          value = g_strdup ((const gchar *) librdf_uri_as_string (librdf_node_get_uri (node)));
      else if (librdf_node_is_blank (node))
          g_printerr ("shouldn't happen. blank nodes are omitted in the query\n");

      if (value != NULL) {
        gimp_metadata_append_tag_value (metadata, "Xmp.dc.creator", value);
        g_free (value);
      }

      librdf_query_results_next (results);
    }

  librdf_free_query_results(results);
  librdf_free_query(query);
}

gpointer *
gimp_attribution_get_model (GimpAttribution *attrib)
{
  g_return_val_if_fail (GIMP_IS_ATTRIBUTION (attrib), NULL);
  return attrib->priv->model;
}

gpointer *
gimp_attribution_get_world (GimpAttribution *attrib)
{
  g_return_val_if_fail (GIMP_IS_ATTRIBUTION (attrib), NULL);
  return attrib->priv->world;
}