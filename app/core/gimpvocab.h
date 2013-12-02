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

#ifndef _GIMP_VOCAB_H_
#define _GIMP_VOCAB_H_

#include <glib-object.h>

G_BEGIN_DECLS

#define GIMP_TYPE_VOCAB             (gimp_vocab_get_type ())
#define GIMP_VOCAB(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_VOCAB, GimpVocab))
#define GIMP_VOCAB_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GIMP_TYPE_VOCAB, GimpVocabClass))
#define GIMP_IS_VOCAB(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_VOCAB))
#define GIMP_IS_VOCAB_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GIMP_TYPE_VOCAB))
#define GIMP_VOCAB_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GIMP_TYPE_VOCAB, GimpVocabClass))

typedef struct _GimpVocabClass GimpVocabClass;
typedef struct _GimpVocab GimpVocab;
typedef struct _GimpVocabPrivate GimpVocabPrivate;


struct _GimpVocabClass
{
  GObjectClass parent_class;
};

struct _GimpVocab
{
  GObject parent_instance;

  GimpVocabPrivate *priv;
};

GType gimp_vocab_get_type (void) G_GNUC_CONST;
GimpVocab * gimp_vocab_new (void);
gchar * gimp_vocab_get_short_uri (GimpVocab *vocab, gchar *uri);

G_END_DECLS

#endif /* _GIMP_VOCAB_H_ */

