/*
 * Copyright (C) 2017,2023,2025  Elvis Teixeira, Anatoliy Sokolov
 *
 * This source code is free software: you can redistribute it
 * and/or modify it under the terms of the GNU Lesser General
 * Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any
 * later version.
 *
 * This source code is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this program.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SLOPE_FIGURE_H
#define SLOPE_FIGURE_H

#include <glib-object.h>
#include <slope/legend.h>

#define SLOPE_FIGURE_TYPE (slope_figure_get_type())
#define SLOPE_FIGURE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), SLOPE_FIGURE_TYPE, SlopeFigure))
#define SLOPE_FIGURE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), SLOPE_FIGURE_TYPE, SlopeFigureClass))
#define SLOPE_IS_FIGURE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), SLOPE_FIGURE_TYPE))
#define SLOPE_IS_FIGURE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), SLOPE_FIGURE_TYPE))
#define SLOPE_FIGURE_GET_CLASS(obj) \
  (SLOPE_FIGURE_CLASS(G_OBJECT_GET_CLASS(obj)))

SLOPE_BEGIN_DECLS

typedef enum _SlopeFigureFrameMode {
  SLOPE_FIGURE_RECTANGLE,
  SLOPE_FIGURE_ROUNDRECTANGLE,
} SlopeFigureFrameMode;

struct _SlopeFigure
{
  GObject parent;

  /* Padding to allow adding up to 4 members
     without breaking ABI. */
  gpointer padding[4];
};

typedef struct _SlopeFigureClass
{
  GObjectClass parent_class;

  void (*draw)(SlopeFigure *self, const graphene_rect_t *rect, cairo_t *cr);
  void (*add_scale)(SlopeFigure *self, SlopeScale *scale);

  /* Padding to allow adding up to 4 members
     without breaking ABI. */
  gpointer padding[4];
} SlopeFigureClass;

GType slope_figure_get_type(void) G_GNUC_CONST;

SlopeFigure *slope_figure_new(void);

GList *slope_figure_get_scale_list(SlopeFigure *self);

void slope_figure_add_scale(SlopeFigure *self, SlopeScale *scale);

void slope_figure_get_background_color(SlopeFigure *self, GdkRGBA *color);

void slope_figure_set_background_color (SlopeFigure *self, const GdkRGBA *color);

gboolean slope_figure_get_is_managed(SlopeFigure *self);

void slope_figure_set_is_managed(SlopeFigure *self, gboolean managed);

void slope_figure_draw(SlopeFigure *self, const graphene_rect_t *rect, cairo_t *cr);

SlopeView *slope_figure_get_view(SlopeFigure *self);

SlopeItem *slope_figure_get_legend(SlopeFigure *self);

SLOPE_END_DECLS

#endif /* SLOPE_FIGURE_H */
