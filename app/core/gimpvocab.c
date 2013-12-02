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

#include "gimpvocab.h"
#include <raptor.h>

struct _GimpVocabPrivate
{
  raptor_world            *world;
  raptor_namespace_stack  *namespaces;
};


G_DEFINE_TYPE (GimpVocab, gimp_vocab, G_TYPE_OBJECT);

static void
gimp_vocab_init (GimpVocab *vocab)
{
  vocab->priv = G_TYPE_INSTANCE_GET_PRIVATE (vocab, GIMP_TYPE_VOCAB, GimpVocabPrivate);

  vocab->priv->world = raptor_new_world ();
  raptor_world_open (vocab->priv->world);
	vocab->priv->namespaces = raptor_new_namespaces (vocab->priv->world, 3);

  raptor_namespaces_start_namespace_full (vocab->priv->namespaces,
    (const unsigned char *) "dc",
    (const unsigned char *) "http://purl.org/dc/elements/1.1/", 0);

  raptor_namespaces_start_namespace_full (vocab->priv->namespaces,
    (const unsigned char *) "dcterms",
    (const unsigned char *) "http://purl.org/dc/terms/", 0);

  raptor_namespaces_start_namespace_full (vocab->priv->namespaces,
    (const unsigned char *) "cc",
    (const unsigned char *) "http://creativecommons.org/ns#", 0);

  raptor_namespaces_start_namespace_full (vocab->priv->namespaces,
    (const unsigned char *) "xhv",
    (const unsigned char *) "http://www.w3.org/1999/xhtml/vocab#", 0);

  raptor_namespaces_start_namespace_full (vocab->priv->namespaces,
    (const unsigned char *) "og",
    (const unsigned char *) "http://ogp.me/ns#", 0);

}

static void
gimp_vocab_finalize (GObject *object)
{
  GimpVocab *vocab = GIMP_VOCAB (object);

	raptor_free_world (vocab->priv->world);

  G_OBJECT_CLASS (gimp_vocab_parent_class)->finalize (object);
}

static void
gimp_vocab_class_init (GimpVocabClass *klass)
{
  GObjectClass* object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (GimpVocabPrivate));

  object_class->finalize = gimp_vocab_finalize;
}

GimpVocab *
gimp_vocab_new (void)
{
	GimpVocab *vocab;

	vocab = g_object_new (GIMP_TYPE_VOCAB, NULL);

	return vocab;
}

gchar *
gimp_vocab_get_short_uri (GimpVocab *vocab, gchar *uri)
{
  raptor_uri   *ruri;
  raptor_qname *qname;
  const guchar  *prefix;
  const guchar  *local_name;
  gchar  *result;

  ruri = raptor_new_uri (vocab->priv->world, (guchar *) uri);
  qname = raptor_new_qname_from_namespace_uri (vocab->priv->namespaces, ruri, 10);

  if (qname == NULL)
    return g_strdup ((gchar *) uri);

  local_name = raptor_qname_get_local_name (qname);
  prefix = raptor_namespace_get_prefix (raptor_qname_get_namespace (qname));

  result = g_strdup_printf ("%s:%s", prefix, local_name);
  raptor_free_qname (qname);
  return result;
}

