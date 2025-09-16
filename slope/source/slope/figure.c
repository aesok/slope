/*
 * Copyright (C) 2017,2023  Elvis Teixeira, Anatoliy Sokolov
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

#include <slope/figure_p.h>
#include <slope/item_p.h>
#include <slope/scale_p.h>
#include <slope/view.h>

typedef struct _SlopeFigurePrivate
{
  SlopeView *view;
  GList *    scale_list;
  GdkRGBA    background_color;
  gboolean   managed;
  gboolean   redraw_requested;
  double     layout_rows;
  double     layout_cols;
  int        frame_mode;
  SlopeItem *legend;
} SlopeFigurePrivate;

G_DEFINE_TYPE_WITH_CODE (SlopeFigure, slope_figure, G_TYPE_OBJECT, G_ADD_PRIVATE (SlopeFigure))

static void _figure_update_layout(SlopeFigure *self);
static void _figure_add_scale(SlopeFigure *self, SlopeScale *scale);
static void _figure_add_rect_path(SlopeFigure *    self,
                                  graphene_rect_t *rect,
                                  const graphene_rect_t *in_rect,
                                  cairo_t *        cr);
static void _figure_draw(SlopeFigure *self, const graphene_rect_t *rect, cairo_t *cr);
static void _figure_clear_scale_list(gpointer data);
static void _figure_finalize(GObject *self);
static void _figure_draw_background(SlopeFigure *    self,
                                    const graphene_rect_t *rect,
                                    cairo_t *        cr);
static void _figure_draw_scales(SlopeFigure *    self,
                                const graphene_rect_t *rect,
                                cairo_t *        cr);
static void _figure_draw_legend(SlopeFigure *    self,
                                const graphene_rect_t *rect,
                                cairo_t *        cr);

static void slope_figure_class_init(SlopeFigureClass *klass)
{
  GObjectClass *object_klass = G_OBJECT_CLASS(klass);
  object_klass->finalize     = _figure_finalize;
  klass->add_scale           = _figure_add_scale;
  klass->draw                = _figure_draw;
}

static void slope_figure_init(SlopeFigure *self)
{
  SlopeFigurePrivate *priv = slope_figure_get_instance_private (self);
  priv->view               = NULL;
  priv->scale_list         = NULL;
  gdk_rgba_parse (&priv->background_color, "white");
  priv->managed            = TRUE;
  priv->redraw_requested   = FALSE;
  priv->frame_mode         = SLOPE_FIGURE_ROUNDRECTANGLE;
  priv->legend             = slope_legend_new (GTK_ORIENTATION_HORIZONTAL);
  slope_item_set_is_visible(SLOPE_ITEM(priv->legend), FALSE);
}

static void _figure_finalize(GObject *self)
{
  SlopeFigurePrivate *priv = slope_figure_get_instance_private (SLOPE_FIGURE (self));
  if (priv->scale_list != NULL)
    {
      g_list_free_full(priv->scale_list, _figure_clear_scale_list);
      priv->scale_list = NULL;
    }
  g_object_unref(G_OBJECT(priv->legend));
  G_OBJECT_CLASS(slope_figure_parent_class)->finalize(self);
}

SlopeFigure *slope_figure_new()
{
  SlopeFigure *self = SLOPE_FIGURE(g_object_new(SLOPE_FIGURE_TYPE, NULL));
  return self;
}

static void _figure_add_scale(SlopeFigure *self, SlopeScale *scale)
{
  SlopeFigurePrivate *priv = slope_figure_get_instance_private (self);
  if (scale == NULL)
    {
      return;
    }
  priv->scale_list = g_list_append(priv->scale_list, scale);
  _scale_set_figure(scale, self);
  slope_scale_rescale(scale);
  _figure_update_layout(self);
}

static void _figure_draw(SlopeFigure *    self,
                         const graphene_rect_t *in_rect,
                         cairo_t *        cr)
{
  graphene_rect_t rect;
  /* save cr's state and clip tho the figure's rectangle,
     fill the background if required */
  cairo_save(cr);
  cairo_new_path(cr);
  cairo_select_font_face(
      cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
  cairo_set_font_size(cr, 11);
  _figure_add_rect_path(self, &rect, in_rect, cr);
  _figure_draw_background(self, &rect, cr);
  cairo_clip(cr);
  _figure_draw_scales(self, &rect, cr);
  _figure_draw_legend(self, &rect, cr);
  /* give back cr in the same state as we received it */
  cairo_restore(cr);
}

static void _figure_add_rect_path(SlopeFigure *    self,
                                  graphene_rect_t *rect,
                                  const graphene_rect_t *in_rect,
                                  cairo_t *        cr)
{
  SlopeFigurePrivate *priv = slope_figure_get_instance_private (self);
  const float radius = 10.0;

  graphene_rect_init_from_rect (rect, in_rect);

  if (priv->frame_mode == SLOPE_FIGURE_ROUNDRECTANGLE)
    {
      graphene_rect_inset (rect, radius, radius);
      slope_cairo_round_rect (cr, rect, radius);
    }
  else
    {
      slope_cairo_rect (cr, rect);
    }
}

static void _figure_draw_background(SlopeFigure *    self,
                                    const graphene_rect_t *rect,
                                    cairo_t *        cr)
{
  SLOPE_UNUSED(rect);
  SlopeFigurePrivate *priv = slope_figure_get_instance_private (self);
  if (!gdk_rgba_is_clear (&priv->background_color))
    {
      gdk_cairo_set_source_rgba (cr, &priv->background_color);
      cairo_fill_preserve(cr);
    }
}

static void _figure_draw_scales(SlopeFigure *    self,
                                const graphene_rect_t *rect,
                                cairo_t *        cr)
{
  SlopeFigurePrivate *priv = slope_figure_get_instance_private (self);
  double layout_cell_width  = graphene_rect_get_width (rect) / priv->layout_cols;
  double layout_cell_height = graphene_rect_get_height (rect) / priv->layout_rows;
  GList *             scale_iter         = priv->scale_list;
  while (scale_iter != NULL)
    {
      SlopeScale *scale = SLOPE_SCALE(scale_iter->data);
      if (slope_scale_get_is_visible(scale) == TRUE)
        {
          graphene_rect_t layout;

          slope_scale_get_layout_rect (scale, &layout);

          graphene_rect_scale (&layout, layout_cell_width, layout_cell_height, &layout);
          graphene_rect_offset (&layout, graphene_rect_get_x (rect), graphene_rect_get_y (rect));

          _scale_draw (scale, &layout, cr);
        }
      scale_iter = scale_iter->next;
    }
}

static void _figure_draw_legend(SlopeFigure *    self,
                                const graphene_rect_t *rect,
                                cairo_t *        cr)
{
  SLOPE_UNUSED(rect);
  SlopeFigurePrivate *priv = slope_figure_get_instance_private (self);
  if (slope_item_get_is_visible(priv->legend))
    {
      // TODO: better legend position algorithm
      slope_legend_set_position(SLOPE_LEGEND(priv->legend), 20.0, 20.0);
      slope_legend_clear_items(SLOPE_LEGEND(priv->legend));
      /* the figure's legend is a global legend, so let's update it's
         items in each draw to make sure it always has all items */
      GList *scale_iter = priv->scale_list;
      while (scale_iter != NULL)
        {
          SlopeScale *scale     = SLOPE_SCALE(scale_iter->data);
          GList *     item_iter = slope_scale_get_item_list(scale);
          while (item_iter != NULL)
            {
              SlopeItem *item = SLOPE_ITEM(item_iter->data);
              slope_legend_add_item(SLOPE_LEGEND(priv->legend), item);
              item_iter = item_iter->next;
            }
          scale_iter = scale_iter->next;
        }
      _item_draw(priv->legend, cr);
    }
}

static void _figure_clear_scale_list(gpointer data)
{
  if (slope_scale_get_is_managed(SLOPE_SCALE(data)) == TRUE)
    {
      g_object_unref(G_OBJECT(data));
    }
}

void _figure_set_view(SlopeFigure *self, SlopeView *view)
{
  SlopeFigurePrivate *priv = slope_figure_get_instance_private (self);
  GList *             iter;
  priv->view = view;
  iter       = priv->scale_list;
  while (iter != NULL)
    {
      SlopeScale *scale = SLOPE_SCALE(iter->data);
      _scale_set_figure(scale, self);
      iter = iter->next;
    }
}

static void _figure_update_layout(SlopeFigure *self)
{
  SlopeFigurePrivate *priv = slope_figure_get_instance_private (self);
  priv->layout_rows        = 0.0;
  priv->layout_cols        = 0.0;
  GList *iter              = priv->scale_list;
  while (iter != NULL)
    {
      graphene_rect_t scale_rect;
      graphene_point_t bottom_right;

      slope_scale_get_layout_rect (SLOPE_SCALE(iter->data), &scale_rect);
      graphene_rect_get_bottom_right (&scale_rect, &bottom_right);

      priv->layout_cols = fmax (priv->layout_cols, bottom_right.x);
      priv->layout_rows = fmax (priv->layout_rows, bottom_right.y);

      iter = iter->next;
    }
}

void _figure_handle_mouse_event(SlopeFigure *self, SlopeMouseEvent *event)
{
  SlopeFigurePrivate *priv = slope_figure_get_instance_private (self);
  /* delegate the handling of the event down to the scales and it's
     items */
  GList *iter = priv->scale_list;
  while (iter != NULL)
    {
      SlopeScale *scale = SLOPE_SCALE(iter->data);
      _scale_handle_mouse_event(scale, event);
      iter = iter->next;
    }
  if (priv->redraw_requested == TRUE)
    {
      slope_view_redraw(priv->view);
      priv->redraw_requested = FALSE;
    }
}

void _figure_request_redraw(SlopeFigure *self)
{
  SlopeFigurePrivate *priv = slope_figure_get_instance_private (self);
  priv->redraw_requested = TRUE;
}

GList *slope_figure_get_scale_list(SlopeFigure *self)
{
  SlopeFigurePrivate *priv = slope_figure_get_instance_private (self);
  return priv->scale_list;
}

void
slope_figure_get_background_color (SlopeFigure *self, GdkRGBA *color)
{
  SlopeFigurePrivate *priv = slope_figure_get_instance_private (self);
  *color = priv->background_color;
}

void
slope_figure_set_background_color (SlopeFigure *self, const GdkRGBA *color)
{
  SlopeFigurePrivate *priv = slope_figure_get_instance_private (self);
  priv->background_color = *color;
}

SlopeView *slope_figure_get_view(SlopeFigure *self)
{
  SlopeFigurePrivate *priv = slope_figure_get_instance_private (self);
  return priv->view;
}

gboolean slope_figure_get_is_managed(SlopeFigure *self)
{
  SlopeFigurePrivate *priv = slope_figure_get_instance_private (self);
  return priv->managed;
}

SlopeItem *slope_figure_get_legend(SlopeFigure *self)
{
  SlopeFigurePrivate *priv = slope_figure_get_instance_private (self);
  return priv->legend;
}

void slope_figure_set_is_managed(SlopeFigure *self, gboolean managed)
{
  SlopeFigurePrivate *priv = slope_figure_get_instance_private (self);
  priv->managed = managed;
}

void slope_figure_draw(SlopeFigure *self, const graphene_rect_t *rect, cairo_t *cr)
{
  SLOPE_FIGURE_GET_CLASS(self)->draw(self, rect, cr);
}

void slope_figure_add_scale(SlopeFigure *self, SlopeScale *scale)
{
  SLOPE_FIGURE_GET_CLASS(self)->add_scale(self, scale);
}

/* slope/figure.c */
