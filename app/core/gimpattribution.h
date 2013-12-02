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

#ifndef _GIMP_ATTRIBUTION_H_
#define _GIMP_ATTRIBUTION_H_

#include <glib-object.h>
//#include <glib/gstdio.h>
#include <gexiv2/gexiv2.h>

G_BEGIN_DECLS

#define GIMP_TYPE_ATTRIBUTION             (gimp_attribution_get_type ())
#define GIMP_ATTRIBUTION(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_ATTRIBUTION, GimpAttribution))
#define GIMP_ATTRIBUTION_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GIMP_TYPE_ATTRIBUTION, GimpAttributionClass))
#define GIMP_IS_ATTRIBUTION(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_ATTRIBUTION))
#define GIMP_IS_ATTRIBUTION_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GIMP_TYPE_ATTRIBUTION))
#define GIMP_ATTRIBUTION_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GIMP_TYPE_ATTRIBUTION, GimpAttributionClass))

typedef struct _GimpAttributionClass GimpAttributionClass;
typedef struct _GimpAttribution GimpAttribution;
typedef struct _GimpAttributionPrivate GimpAttributionPrivate;


struct _GimpAttributionClass
{
  GObjectClass parent_class;
};

struct _GimpAttribution
{
  GObject parent_instance;

  GimpAttributionPrivate *priv;
};

GType             gimp_attribution_get_type         (void) G_GNUC_CONST;
GimpAttribution * gimp_attribution_new              (void);
GimpAttribution * gimp_attribution_new2             (GimpAttribution *attrib1,
                                                     GimpAttribution *attrib2);

gboolean          gimp_attribution_load_from_string (GimpAttribution *attrib,
                                                     const gchar *rdf_xml);
gboolean          gimp_attribution_load_from_file   (GimpAttribution *attrib,
                                                     const gchar *filename);
gboolean          gimp_attribution_load_from_xmp    (GimpAttribution *attrib,
                                                     const gchar *packet,
                                                     gchar *base_uri);
gchar           * gimp_attribution_serialize_rdf    (GimpAttribution *attrib,
                                                     GimpAttribution *image_attrib);
void              gimp_attribution_combine          (GimpAttribution *attrib,
                                                     GimpAttribution *other);
void              gimp_attribution_combine_check    (GimpAttribution *attrib,
                                                     GimpAttribution *other);
gboolean          gimp_attribution_is_empty         (GimpAttribution *attrib);
gboolean          gimp_attribution_has_attribution  (GimpAttribution *attrib);
void              gimp_attribution_write_metadata   (GimpAttribution *attrib,
                                                     GExiv2Metadata *metadata);
gpointer        * gimp_attribution_get_model        (GimpAttribution *attrib);
gpointer        * gimp_attribution_get_world        (GimpAttribution *attrib);

G_END_DECLS

#endif /* _GIMP_ATTRIBUTION_H_ */

