/*
 * Copyright (C) 2017,2023  Elvis Teixeira, Anatoliy Sokolov
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
#include <slope/xyscale.h>

#include <stdio.h>

#define MAX_AXIS 6

typedef struct _SlopeXyScalePrivate
{
  SlopeItem *axis[MAX_AXIS];
  double     left_margin, right_margin;
  double     top_margin, bottom_margin;
  double     horiz_pad, vertical_pad;
  double     fig_x_min, fig_x_max;
  double     fig_y_min, fig_y_max;
  double     fig_width, fig_height;
  double     dat_x_min, dat_x_max;
  double     dat_y_min, dat_y_max;
  double     dat_width, dat_height;
  graphene_point_t mouse_p1;
  graphene_point_t mouse_p2;
  GdkRGBA    mouse_rect_color;
  gboolean   on_drag;
  int        interaction;
} SlopeXyScalePrivate;

G_DEFINE_TYPE_WITH_CODE (SlopeXyScale, slope_xyscale, SLOPE_SCALE_TYPE, G_ADD_PRIVATE (SlopeXyScale))

static void _xyscale_finalize(GObject *self);
static void _xyscale_draw (SlopeScale *self, const graphene_rect_t *rect, cairo_t *cr);
static void _xyscale_map (SlopeScale *self,
                          graphene_point_t *res,
                          const graphene_point_t *src);
static void _xyscale_unmap (SlopeScale *self,
                            graphene_point_t *res,
                            const graphene_point_t *src);
static void _xyscale_rescale(SlopeScale *self);
static void _xyscale_get_figure_rect(SlopeScale *self, graphene_rect_t *rect);
static void _xyscale_get_data_rect (SlopeScale *self, graphene_rect_t *rect);
static void _xyscale_position_axis(SlopeScale *self);
static void _xyscale_apply_padding(SlopeXyScale *self);
static void _xyscale_mouse_event(SlopeScale *self, SlopeMouseEvent *event);
static void _xyscale_zoom_event(SlopeScale *self, SlopeMouseEvent *event);
static void _xyscale_translate_event(SlopeScale *self, SlopeMouseEvent *event);

static void slope_xyscale_class_init(SlopeXyScaleClass *klass)
{
  GObjectClass *   object_klass = G_OBJECT_CLASS(klass);
  SlopeScaleClass *scale_klass  = SLOPE_SCALE_CLASS(klass);
  object_klass->finalize        = _xyscale_finalize;
  scale_klass->draw             = _xyscale_draw;
  scale_klass->map              = _xyscale_map;
  scale_klass->unmap            = _xyscale_unmap;
  scale_klass->rescale          = _xyscale_rescale;
  scale_klass->get_data_rect    = _xyscale_get_data_rect;
  scale_klass->get_figure_rect  = _xyscale_get_figure_rect;
  scale_klass->mouse_event      = _xyscale_mouse_event;
}

static void slope_xyscale_init(SlopeXyScale *self)
{
  SlopeXyScalePrivate *priv = slope_xyscale_get_instance_private (self);
  priv->axis[SLOPE_XYSCALE_AXIS_BOTTOM] =
  slope_xyaxis_new (GTK_ORIENTATION_HORIZONTAL, NULL);
  priv->axis[SLOPE_XYSCALE_AXIS_LEFT] = slope_xyaxis_new (GTK_ORIENTATION_VERTICAL, NULL);
  priv->axis[SLOPE_XYSCALE_AXIS_TOP] = slope_xyaxis_new (GTK_ORIENTATION_HORIZONTAL, NULL);
  priv->axis[SLOPE_XYSCALE_AXIS_RIGHT] = slope_xyaxis_new (GTK_ORIENTATION_VERTICAL, NULL);
  priv->axis[SLOPE_XYSCALE_AXIS_X] = slope_xyaxis_new (GTK_ORIENTATION_HORIZONTAL, NULL);
  priv->axis[SLOPE_XYSCALE_AXIS_Y] = slope_xyaxis_new (GTK_ORIENTATION_VERTICAL, NULL);
  slope_xyscale_set_axis(SLOPE_XYSCALE(self), SLOPE_XYSCALE_FRAME_AXIS);
  int k;
  for (k = 0; k < MAX_AXIS; ++k)
    {
      _item_set_scale(priv->axis[k], SLOPE_SCALE(self));
    }
  priv->left_margin   = 52.0;
  priv->right_margin  = 15.0;
  priv->top_margin    = 15.0;
  priv->bottom_margin = 43.0;
  slope_scale_set_name_top_padding(SLOPE_SCALE(self), priv->top_margin + 2);
  priv->horiz_pad        = 0.05;
  priv->vertical_pad     = 0.05;
  priv->on_drag          = FALSE;
  gdk_rgba_parse (&priv->mouse_rect_color, "dimgray");
  priv->interaction      = SLOPE_XYSCALE_INTERACTION_TRANSLATE;
  slope_scale_rescale(SLOPE_SCALE(self));
}

static void _xyscale_finalize(GObject *self)
{
  SlopeXyScalePrivate *priv = slope_xyscale_get_instance_private (SLOPE_XYSCALE (self));
  if (priv->axis[0] != NULL)
    {
      int k;
      for (k = 0; k < MAX_AXIS; ++k)
        {
          g_object_unref(priv->axis[k]);
        }
      priv->axis[0] = NULL;
    }
  G_OBJECT_CLASS(slope_xyscale_parent_class)->finalize(self);
}

SlopeScale *slope_xyscale_new(void)
{
  SlopeXyScale *self = SLOPE_XYSCALE(g_object_new(SLOPE_XYSCALE_TYPE, NULL));
  return SLOPE_SCALE(self);
}

SlopeScale *slope_xyscale_new_axis(const char *x_title,
                                   const char *y_title,
                                   const char *top_title)
{
  SlopeXyScale *self = SLOPE_XYSCALE(g_object_new(SLOPE_XYSCALE_TYPE, NULL));
  SlopeXyScalePrivate *priv = slope_xyscale_get_instance_private (self);

  slope_xyaxis_set_title(SLOPE_XYAXIS(priv->axis[SLOPE_XYSCALE_AXIS_BOTTOM]),
                         x_title);
  slope_xyaxis_set_title(SLOPE_XYAXIS(priv->axis[SLOPE_XYSCALE_AXIS_LEFT]),
                         y_title);
  slope_xyaxis_set_title(SLOPE_XYAXIS(priv->axis[SLOPE_XYSCALE_AXIS_TOP]),
                         top_title);

  return SLOPE_SCALE(self);
}

static void _xyscale_draw(SlopeScale *self, const graphene_rect_t *rect, cairo_t *cr)
{
  SlopeXyScalePrivate *priv = slope_xyscale_get_instance_private (SLOPE_XYSCALE (self));
  int                  k;

  // TODE: Use graphene_rect_inset.
  priv->fig_x_min = graphene_rect_get_x (rect) + priv->left_margin;
  priv->fig_x_max = graphene_rect_get_x (rect) + graphene_rect_get_width (rect) - priv->right_margin;
  priv->fig_width = priv->fig_x_max - priv->fig_x_min;

  priv->fig_y_min  = graphene_rect_get_y (rect) + priv->top_margin;
  priv->fig_y_max  = graphene_rect_get_y (rect) + graphene_rect_get_height (rect) - priv->bottom_margin;
  priv->fig_height = priv->fig_y_max - priv->fig_y_min;

  cairo_save(cr);
  cairo_new_path(cr);
  cairo_rectangle(
      cr, priv->fig_x_min, priv->fig_y_min, priv->fig_width, priv->fig_height);
  cairo_clip(cr);

  SLOPE_SCALE_CLASS(slope_xyscale_parent_class)->draw(self, rect, cr);

  cairo_restore(cr);

  /* draw axis */
  _xyscale_position_axis(self);
  for (k = 0; k < MAX_AXIS; ++k)
    {
      if (slope_item_get_is_visible(priv->axis[k]) == TRUE)
        {
          _item_draw(priv->axis[k], cr);
        }
    }

  if (priv->on_drag == TRUE)
    {
      static const double dashes[2] = {4.0, 4.0};
      cairo_save(cr);
      cairo_new_path(cr);
      cairo_rectangle(cr,
                      priv->mouse_p1.x,
                      priv->mouse_p1.y,
                      priv->mouse_p2.x - priv->mouse_p1.x,
                      priv->mouse_p2.y - priv->mouse_p1.y);
      cairo_set_dash(cr, dashes, 2, 0.0);
      cairo_set_line_width(cr, 1.0);
      slope_cairo_set_antialias(cr, FALSE);
      gdk_cairo_set_source_rgba (cr, &priv->mouse_rect_color);
      cairo_stroke(cr);
      cairo_restore(cr);
    }
}

static void
_xyscale_map (SlopeScale * self,
              graphene_point_t * res,
              const graphene_point_t *src)
{
  SlopeXyScalePrivate *priv = slope_xyscale_get_instance_private (SLOPE_XYSCALE (self));
  double               tmp;

  tmp    = (src->x - priv->dat_x_min) / priv->dat_width;
  res->x = priv->fig_x_min + tmp * priv->fig_width;

  tmp    = (src->y - priv->dat_y_min) / priv->dat_height;
  res->y = priv->fig_y_max - tmp * priv->fig_height;
}

static void
_xyscale_unmap (SlopeScale *self,
                graphene_point_t *res,
                const graphene_point_t *src)
{
  SlopeXyScalePrivate *priv = slope_xyscale_get_instance_private (SLOPE_XYSCALE (self));
  double               tmp;

  tmp    = (src->x - priv->fig_x_min) / priv->fig_width;
  res->x = priv->dat_x_min + tmp * priv->dat_width;

  tmp    = (priv->fig_y_max - src->y) / priv->fig_height;
  res->y = priv->dat_y_min + tmp * priv->dat_height;
}

static void _xyscale_rescale(SlopeScale *self)
{
  SlopeXyScalePrivate *priv;
  GList *              iter, *list;
  SlopeItem *          item;
  graphene_rect_t      rect;

  priv = slope_xyscale_get_instance_private (SLOPE_XYSCALE (self));
  list = slope_scale_get_item_list(self);

  if (list == NULL)
    {
      slope_xyscale_set_x_range(SLOPE_XYSCALE(self), 0.0, 1.0);
      slope_xyscale_set_y_range(SLOPE_XYSCALE(self), 0.0, 1.0);
      return;
    }

  iter = list;
  item = SLOPE_ITEM(iter->data);
  slope_item_get_data_rect (item, &rect);

  while (iter != NULL)
    {
      graphene_rect_t      item_rect_i;

      item = SLOPE_ITEM(iter->data);
      iter = iter->next;

      slope_item_get_data_rect (item, &item_rect_i);
      graphene_rect_union (&rect, &item_rect_i, &rect);

    }

  priv->dat_x_min = graphene_rect_get_x (&rect);
  priv->dat_x_max = graphene_rect_get_x (&rect) + graphene_rect_get_width (&rect);
  priv->dat_y_min = graphene_rect_get_y (&rect);
  priv->dat_y_max = graphene_rect_get_y (&rect) + graphene_rect_get_height (&rect);

  _xyscale_apply_padding(SLOPE_XYSCALE(self));
}

static void _xyscale_apply_padding(SlopeXyScale *self)
{
  SlopeXyScalePrivate *priv = slope_xyscale_get_instance_private (self);
  double               padding;

  /* evaluate width and height of the data space taking
   * int account the padding */
  priv->dat_width = priv->dat_x_max - priv->dat_x_min;
  padding         = priv->dat_width * priv->horiz_pad;
  if (priv->dat_width == 0.0 && padding == 0.0)
    {
      padding =
          0.1 * (priv->dat_x_max > 0.0 ? priv->dat_x_max : -priv->dat_x_max);
      if (padding == 0.0)
        {
          padding = 0.1;
        }
    }

  priv->dat_x_min -= padding;
  priv->dat_x_max += padding;
  priv->dat_width += 2.0 * padding;

  priv->dat_height = priv->dat_y_max - priv->dat_y_min;
  padding          = priv->dat_height * priv->vertical_pad;
  if (priv->dat_height == 0.0 && padding == 0.0)
    {
      padding =
          0.1 * (priv->dat_y_max > 0.0 ? priv->dat_y_max : -priv->dat_y_max);
      if (padding == 0.0)
        {
          padding = 0.1;
        }
    }

  priv->dat_y_min -= padding;
  priv->dat_y_max += padding;
  priv->dat_height += 2.0 * padding;
}

static void _xyscale_get_figure_rect (SlopeScale *self, graphene_rect_t *rect)
{
  SlopeXyScalePrivate *priv = slope_xyscale_get_instance_private (SLOPE_XYSCALE (self));
  graphene_rect_init (rect, priv->fig_x_min, priv->fig_y_min, priv->fig_width, priv->fig_height);
}

static void
_xyscale_get_data_rect (SlopeScale *self, graphene_rect_t *rect)
{
  SlopeXyScalePrivate *priv = slope_xyscale_get_instance_private (SLOPE_XYSCALE (self));
  graphene_rect_init (rect, priv->dat_x_min, priv->dat_y_min, priv->dat_width, priv->dat_height);
}

static void _xyscale_position_axis(SlopeScale *self)
{
  SlopeXyScalePrivate *priv = slope_xyscale_get_instance_private (SLOPE_XYSCALE (self));

  slope_xyaxis_set_position(SLOPE_XYAXIS(priv->axis[SLOPE_XYSCALE_AXIS_BOTTOM]),
                            priv->dat_x_min,
                            priv->dat_x_max,
                            priv->dat_y_min);
  slope_xyaxis_set_position(SLOPE_XYAXIS(priv->axis[SLOPE_XYSCALE_AXIS_TOP]),
                            priv->dat_x_min,
                            priv->dat_x_max,
                            priv->dat_y_max);
  slope_xyaxis_set_position(SLOPE_XYAXIS(priv->axis[SLOPE_XYSCALE_AXIS_LEFT]),
                            priv->dat_y_min,
                            priv->dat_y_max,
                            priv->dat_x_min);
  slope_xyaxis_set_position(SLOPE_XYAXIS(priv->axis[SLOPE_XYSCALE_AXIS_RIGHT]),
                            priv->dat_y_min,
                            priv->dat_y_max,
                            priv->dat_x_max);
  slope_xyaxis_set_position(SLOPE_XYAXIS(priv->axis[SLOPE_XYSCALE_AXIS_X]),
                            priv->dat_x_min,
                            priv->dat_x_max,
                            0.0);
  slope_xyaxis_set_position(SLOPE_XYAXIS(priv->axis[SLOPE_XYSCALE_AXIS_Y]),
                            priv->dat_y_min,
                            priv->dat_y_max,
                            0.0);
}

static void _xyscale_set_visible_axis(SlopeXyScale *self, int axis_flag)
{
  SlopeXyScalePrivate *priv = slope_xyscale_get_instance_private (self);

  switch (axis_flag)
    {
    case SLOPE_XYSCALE_NO_AXIS:
      slope_item_set_is_visible(priv->axis[SLOPE_XYSCALE_AXIS_BOTTOM], FALSE);
      slope_item_set_is_visible(priv->axis[SLOPE_XYSCALE_AXIS_LEFT], FALSE);
      slope_item_set_is_visible(priv->axis[SLOPE_XYSCALE_AXIS_TOP], FALSE);
      slope_item_set_is_visible(priv->axis[SLOPE_XYSCALE_AXIS_RIGHT], FALSE);
      slope_item_set_is_visible(priv->axis[SLOPE_XYSCALE_AXIS_X], FALSE);
      slope_item_set_is_visible(priv->axis[SLOPE_XYSCALE_AXIS_Y], FALSE);
      break;
    case SLOPE_XYSCALE_FRAME_AXIS:
      slope_item_set_is_visible(priv->axis[SLOPE_XYSCALE_AXIS_BOTTOM], TRUE);
      slope_item_set_is_visible(priv->axis[SLOPE_XYSCALE_AXIS_LEFT], TRUE);
      slope_item_set_is_visible(priv->axis[SLOPE_XYSCALE_AXIS_TOP], TRUE);
      slope_item_set_is_visible(priv->axis[SLOPE_XYSCALE_AXIS_RIGHT], TRUE);
      slope_item_set_is_visible(priv->axis[SLOPE_XYSCALE_AXIS_X], FALSE);
      slope_item_set_is_visible(priv->axis[SLOPE_XYSCALE_AXIS_Y], FALSE);
      break;
    case SLOPE_XYSCALE_ZERO_AXIS:
      slope_item_set_is_visible(priv->axis[SLOPE_XYSCALE_AXIS_BOTTOM], FALSE);
      slope_item_set_is_visible(priv->axis[SLOPE_XYSCALE_AXIS_LEFT], FALSE);
      slope_item_set_is_visible(priv->axis[SLOPE_XYSCALE_AXIS_TOP], FALSE);
      slope_item_set_is_visible(priv->axis[SLOPE_XYSCALE_AXIS_RIGHT], FALSE);
      slope_item_set_is_visible(priv->axis[SLOPE_XYSCALE_AXIS_X], TRUE);
      slope_item_set_is_visible(priv->axis[SLOPE_XYSCALE_AXIS_Y], TRUE);
      break;
    }
}

void slope_xyscale_set_axis(SlopeXyScale *self, int axis_flag)
{
  SlopeXyScalePrivate *priv = slope_xyscale_get_instance_private (self);
  int                  k;

  switch (axis_flag)
    {
    case SLOPE_XYSCALE_NO_AXIS:
      _xyscale_set_visible_axis(self, SLOPE_XYSCALE_NO_AXIS);
      break;

    case SLOPE_XYSCALE_FRAME_AXIS:
      _xyscale_set_visible_axis(self, SLOPE_XYSCALE_FRAME_AXIS);
      for (k = 0; k < MAX_AXIS; ++k)
        {
          slope_xyaxis_set_components(SLOPE_XYAXIS(priv->axis[k]),
                                      SLOPE_XYAXIS_LINE);
        }
      slope_xyaxis_set_components(
          SLOPE_XYAXIS(priv->axis[SLOPE_XYSCALE_AXIS_BOTTOM]),
          SLOPE_XYAXIS_DEFAULT_DOWN);
      slope_xyaxis_set_components(
          SLOPE_XYAXIS(priv->axis[SLOPE_XYSCALE_AXIS_LEFT]),
          SLOPE_XYAXIS_DEFAULT_DOWN);
      slope_xyaxis_set_components(
          SLOPE_XYAXIS(priv->axis[SLOPE_XYSCALE_AXIS_TOP]), SLOPE_XYAXIS_LINE);
      break;

    case SLOPE_XYSCALE_ZERO_AXIS:
      _xyscale_set_visible_axis(self, SLOPE_XYSCALE_FRAME_AXIS);
      for (k = 0; k < MAX_AXIS; ++k)
        {
          slope_xyaxis_set_components(SLOPE_XYAXIS(priv->axis[k]),
                                      SLOPE_XYAXIS_LINE);
        }
      slope_xyaxis_set_components(
          SLOPE_XYAXIS(priv->axis[SLOPE_XYSCALE_AXIS_X]),
          SLOPE_XYAXIS_DEFAULT_DOWN);
      slope_xyaxis_set_components(
          SLOPE_XYAXIS(priv->axis[SLOPE_XYSCALE_AXIS_Y]),
          SLOPE_XYAXIS_DEFAULT_DOWN);
      break;

    case SLOPE_XYSCALE_FRAME_LINE:
      _xyscale_set_visible_axis(self, SLOPE_XYSCALE_FRAME_AXIS);
      for (k = 0; k < MAX_AXIS; ++k)
        {
          slope_xyaxis_set_components(SLOPE_XYAXIS(priv->axis[k]),
                                      SLOPE_XYAXIS_LINE);
        }
      break;

    case SLOPE_XYSCALE_FRAME_AXIS_GRID:
      _xyscale_set_visible_axis(self, SLOPE_XYSCALE_FRAME_AXIS);
      for (k = 0; k < MAX_AXIS; ++k)
        {
          slope_xyaxis_set_components(SLOPE_XYAXIS(priv->axis[k]),
                                      SLOPE_XYAXIS_LINE);
        }
      slope_xyaxis_set_components(
          SLOPE_XYAXIS(priv->axis[SLOPE_XYSCALE_AXIS_BOTTOM]),
          SLOPE_XYAXIS_DEFAULT_DOWN_GRID);
      slope_xyaxis_set_components(
          SLOPE_XYAXIS(priv->axis[SLOPE_XYSCALE_AXIS_LEFT]),
          SLOPE_XYAXIS_DEFAULT_DOWN_GRID);
      slope_xyaxis_set_components(
          SLOPE_XYAXIS(priv->axis[SLOPE_XYSCALE_AXIS_TOP]), SLOPE_XYAXIS_LINE);
      break;
    }
}

SlopeItem *slope_xyscale_get_axis(SlopeXyScale *self, int axis_id)
{
  SlopeXyScalePrivate *priv = slope_xyscale_get_instance_private (self);
  return priv->axis[axis_id];
}

void slope_xyscale_set_x_range(SlopeXyScale *self, double min, double max)
{
  SlopeXyScalePrivate *priv = slope_xyscale_get_instance_private (self);

  priv->dat_x_min = min;
  priv->dat_x_max = max;
  priv->dat_width = max - min;
}

void slope_xyscale_set_y_range(SlopeXyScale *self, double min, double max)
{
  SlopeXyScalePrivate *priv = slope_xyscale_get_instance_private (self);

  priv->dat_y_min  = min;
  priv->dat_y_max  = max;
  priv->dat_height = max - min;
}

static void _xyscale_mouse_event(SlopeScale *self, SlopeMouseEvent *event)
{
  SlopeXyScalePrivate *priv = slope_xyscale_get_instance_private (SLOPE_XYSCALE (self));
  SlopeFigure *        figure = slope_scale_get_figure(self);
  graphene_rect_t      figure_rect;

  /* If the mouse event is outside this scale,we have nothing to do
     with it. If on_drag is true, disable it to ensure the zoom rectangle
     does not remain visible */
  slope_scale_get_figure_rect (self, &figure_rect);
  if (event->type == SLOPE_MOUSE_MOVE
      || !graphene_rect_contains_point (&figure_rect, &GRAPHENE_POINT_INIT (event->x, event->y)))
    {
      if (priv->on_drag == TRUE)
        {
          priv->on_drag = FALSE;
          _figure_request_redraw(figure);
        }
      return;
    }

  if (event->type == SLOPE_MOUSE_DOUBLE_PRESS)
    {
      /* Double mouse presses change interaction type */
      if (priv->interaction == SLOPE_XYSCALE_INTERACTION_ZOOM)
        {
          priv->interaction = SLOPE_XYSCALE_INTERACTION_TRANSLATE;
        }
      else
        {
          priv->interaction = SLOPE_XYSCALE_INTERACTION_ZOOM;
        }
    }
  else if (event->button == SLOPE_MOUSE_BUTTON_RIGHT)
    {
      slope_scale_rescale(self);
      _figure_request_redraw(figure);
    }

  /* Delegate to action handlers */
  else if (priv->interaction == SLOPE_XYSCALE_INTERACTION_ZOOM)
    {
      _xyscale_zoom_event(self, event);
    }
  else if (priv->interaction == SLOPE_XYSCALE_INTERACTION_TRANSLATE)
    {
      _xyscale_translate_event(self, event);
    }
}

static void _xyscale_zoom_event(SlopeScale *self, SlopeMouseEvent *event)
{
  SlopeXyScalePrivate *priv = slope_xyscale_get_instance_private (SLOPE_XYSCALE (self));
  SlopeFigure *        figure = slope_scale_get_figure(self);

  if (event->type == SLOPE_MOUSE_PRESS)
    {
      priv->mouse_p1.x = event->x;
      priv->mouse_p1.y = event->y;
      priv->mouse_p2   = priv->mouse_p1;
      priv->on_drag    = TRUE;
    }

  else if (event->type == SLOPE_MOUSE_MOVE_PRESSED && priv->on_drag == TRUE)
    {
      priv->mouse_p2.x = event->x;
      priv->mouse_p2.y = event->y;
      _figure_request_redraw(figure);
    }

  else if (event->type == SLOPE_MOUSE_RELEASE)
    {
      graphene_point_t data_p1, data_p2;
      priv->on_drag = FALSE;

      if (priv->mouse_p2.x < priv->mouse_p1.x)
        {
          double tmp       = priv->mouse_p1.x;
          priv->mouse_p1.x = priv->mouse_p2.x;
          priv->mouse_p2.x = tmp;
        }

      if (priv->mouse_p2.y < priv->mouse_p1.y)
        {
          double tmp       = priv->mouse_p1.y;
          priv->mouse_p1.y = priv->mouse_p2.y;
          priv->mouse_p2.y = tmp;
        }

      /* Only actually change the zoom if the zoom rect is at least
         4X4 pixels */
      if (SLOPE_ABS(priv->mouse_p1.x - priv->mouse_p2.x) > 4 &&
          SLOPE_ABS(priv->mouse_p1.y - priv->mouse_p2.y) > 4)
        {
          slope_scale_unmap(self, &data_p1, &priv->mouse_p1);
          slope_scale_unmap(self, &data_p2, &priv->mouse_p2);

          slope_xyscale_set_x_range(SLOPE_XYSCALE(self), data_p1.x, data_p2.x);
          slope_xyscale_set_y_range(SLOPE_XYSCALE(self), data_p2.y, data_p1.y);
        }

      _figure_request_redraw(figure);
    }
}

static void _xyscale_translate_event(SlopeScale *self, SlopeMouseEvent *event)
{
  SlopeXyScalePrivate *priv = slope_xyscale_get_instance_private (SLOPE_XYSCALE (self));
  SlopeFigure *        figure = slope_scale_get_figure(self);

  if (event->type == SLOPE_MOUSE_PRESS)
    {
      priv->mouse_p1.x = event->x;
      priv->mouse_p1.y = event->y;
    }

  else if (event->type == SLOPE_MOUSE_MOVE_PRESSED)
    {
      graphene_point_t data_p1, data_p2;
      double     dx, dy;

      priv->mouse_p2.x = event->x;
      priv->mouse_p2.y = event->y;

      slope_scale_unmap(self, &data_p1, &priv->mouse_p1);
      slope_scale_unmap(self, &data_p2, &priv->mouse_p2);

      dx = data_p2.x - data_p1.x;
      dy = data_p2.y - data_p1.y;

      data_p1.x = priv->dat_x_min - dx;
      data_p1.y = priv->dat_y_min - dy;

      data_p2.x = priv->dat_x_max - dx;
      data_p2.y = priv->dat_y_max - dy;

      slope_xyscale_set_x_range(SLOPE_XYSCALE(self), data_p1.x, data_p2.x);
      slope_xyscale_set_y_range(SLOPE_XYSCALE(self), data_p1.y, data_p2.y);

      priv->mouse_p1 = priv->mouse_p2;
      _figure_request_redraw(figure);
    }
}

void slope_xyscale_set_interaction(SlopeXyScale *self, int interaction)
{
  SlopeXyScalePrivate *priv = slope_xyscale_get_instance_private (self);
  priv->interaction         = interaction;
}

/* slope/xyscale.c */
