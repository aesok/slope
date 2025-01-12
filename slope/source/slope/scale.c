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

#include <slope/item_p.h>
#include <slope/scale_p.h>

typedef struct _SlopeScalePrivate
{
  SlopeFigure *figure;
  SlopeView *  view;
  GList *      item_list;
  GdkRGBA      background_color;
  gboolean     managed;
  gboolean     visible;
  char *       name;
  gboolean     show_name;
  GdkRGBA      name_color;
  double       name_top_padding;
  graphene_rect_t layout_rect;
  SlopeItem *  legend;
} SlopeScalePrivate;

G_DEFINE_TYPE_WITH_CODE (SlopeScale, slope_scale, G_TYPE_OBJECT, G_ADD_PRIVATE (SlopeScale))

static void _scale_draw_impl(SlopeScale *     self,
                             const graphene_rect_t *rect,
                             cairo_t *        cr);
static void _scale_draw_legend(SlopeScale *self, cairo_t *cr);
static void _scale_position_legend(SlopeScale *self);
static void _scale_finalize(GObject *self);
static void _scale_add_item(SlopeScale *self, SlopeItem *item);
static void _scale_clear_item_list(gpointer data);
static void _scale_remove_item(SlopeScale *self, SlopeItem *item);

static void slope_scale_class_init(SlopeScaleClass *klass)
{
  GObjectClass *object_klass = G_OBJECT_CLASS(klass);
  object_klass->finalize     = _scale_finalize;
  klass->add_item            = _scale_add_item;
  klass->remove_item         = _scale_remove_item;
  klass->draw                = _scale_draw_impl;
  klass->mouse_event         = _scale_mouse_event_impl;
  klass->position_legend     = _scale_position_legend;
}

static void slope_scale_init(SlopeScale *self)
{
  SlopeScalePrivate *priv  = slope_scale_get_instance_private (self);
  priv->figure             = NULL;
  priv->view               = NULL;
  priv->item_list          = NULL;
  gdk_rgba_parse (&priv->background_color, "white");
  priv->managed            = TRUE;
  priv->visible            = TRUE;
  priv->name               = NULL;
  priv->show_name          = FALSE;
  gdk_rgba_parse (&priv->name_color, "black");
  priv->name_top_padding   = 0.0;
  priv->layout_rect        = GRAPHENE_RECT_INIT (0.0, 0.0, 1.0, 1.0);
  priv->legend             = slope_legend_new (GTK_ORIENTATION_VERTICAL);
}

static void _scale_finalize(GObject *self)
{
  SlopeScalePrivate *priv = slope_scale_get_instance_private (SLOPE_SCALE (self));
  /* frees name safely */
  slope_scale_set_name(SLOPE_SCALE(self), NULL);
  if (priv->item_list != NULL)
    {
      g_list_free_full(priv->item_list, _scale_clear_item_list);
      priv->item_list = NULL;
    }
  g_object_unref(priv->legend);
  G_OBJECT_CLASS(slope_scale_parent_class)->finalize(self);
}

static void _scale_add_item(SlopeScale *self, SlopeItem *item)
{
  SlopeScalePrivate *priv  = slope_scale_get_instance_private (self);
  if (item == NULL)
    {
      return;
    }
  priv->item_list = g_list_append(priv->item_list, item);
  slope_item_detach(item);
  _item_set_scale(item, self);
  slope_scale_rescale(self);
}

SlopeItem *slope_scale_get_item_by_name(SlopeScale *self, const char *itemname)
{
  SlopeScalePrivate *priv = slope_scale_get_instance_private (self);
  GList *            iter;
  iter = priv->item_list;
  while (iter != NULL)
    {
      SlopeItem * curr_item = SLOPE_ITEM(iter->data);
      const char *curr_name = slope_item_get_name(curr_item);

      if (g_strcmp0(curr_name, itemname) == 0)
        {
          return curr_item;
        }

      iter = iter->next;
    }
  /* not found */
  return NULL;
}

static void _scale_remove_item(SlopeScale *self, SlopeItem *item)
{
  SlopeScalePrivate *priv  = slope_scale_get_instance_private (self);
  GList *            iter;

  iter = priv->item_list;
  while (iter != NULL)
    {
      SlopeItem *curr_item = SLOPE_ITEM(iter->data);

      if (curr_item == item)
        {
          priv->item_list = g_list_delete_link(priv->item_list, iter);
          _item_set_scale(curr_item, NULL);
          slope_scale_rescale(self);
        }

      iter = iter->next;
    }
}

void slope_scale_remove_item_by_name(SlopeScale *self, const char *itemname)
{
  slope_scale_remove_item(self, slope_scale_get_item_by_name(self, itemname));
}

void _scale_draw(SlopeScale *self, const graphene_rect_t *rect, cairo_t *cr)
{
  SlopeScalePrivate *priv  = slope_scale_get_instance_private (self);
  SLOPE_SCALE_GET_CLASS(self)->draw(self, rect, cr);
  /* we draw the legend as the last thing to make sure it is always on top */
  if (slope_item_get_is_visible(priv->legend))
    {
      SLOPE_SCALE_GET_CLASS(self)->position_legend(self);
      _scale_draw_legend(self, cr);
    }
}

void _scale_draw_impl(SlopeScale *self, const graphene_rect_t *rect, cairo_t *cr)
{
  SlopeScalePrivate *priv = slope_scale_get_instance_private (self);
  /* TODO: break this in smaller tasks */
  GList *item_iter;
  if (!gdk_rgba_is_clear (&priv->background_color))
    {

      cairo_save(cr);
      cairo_new_path(cr);
      slope_cairo_rect (cr, rect);
      gdk_cairo_set_source_rgba (cr, &priv->background_color);
      cairo_fill(cr);
      cairo_restore(cr);
    }
  item_iter = priv->item_list;
  while (item_iter != NULL)
    {
      _item_draw(SLOPE_ITEM(item_iter->data), cr);
      item_iter = item_iter->next;
    }
  if (priv->name != NULL && priv->show_name == TRUE)
    {
      cairo_text_extents_t txt_ext;
      cairo_text_extents(cr, priv->name, &txt_ext);
      gdk_cairo_set_source_rgba (cr, &priv->name_color);
      slope_cairo_text(cr,
                       graphene_rect_get_x (rect) + (graphene_rect_get_width  (rect) - txt_ext.width) * 0.5,
                       graphene_rect_get_y (rect) + txt_ext.height * 1.2 + priv->name_top_padding,
                       priv->name);
      cairo_stroke(cr);
    }
}

static void _scale_position_legend(SlopeScale *self)
{
  SlopeScalePrivate *priv = slope_scale_get_instance_private (self);
  graphene_rect_t rect;
  slope_scale_get_figure_rect (self, &rect);
  slope_legend_set_position(
      SLOPE_LEGEND(priv->legend), graphene_rect_get_x (&rect) + 6.0, graphene_rect_get_y (&rect) + 6.0);
}

static void _scale_draw_legend(SlopeScale *self, cairo_t *cr)
{
  SlopeScalePrivate *priv = slope_scale_get_instance_private (self);
  slope_legend_clear_items(SLOPE_LEGEND(priv->legend));
  /* the figure's legend is a global legend, so let's update it's
     items in each draw to make sure it always has all items */
  GList *item_iter = priv->item_list;
  while (item_iter != NULL)
    {
      SlopeItem *item = SLOPE_ITEM(item_iter->data);
      slope_legend_add_item(SLOPE_LEGEND(priv->legend), item);
      item_iter = item_iter->next;
    }
  _item_draw(priv->legend, cr);
}

void _scale_set_figure(SlopeScale *self, SlopeFigure *figure)
{
  SlopeScalePrivate *priv = slope_scale_get_instance_private (self);
  GList *            iter;
  if (priv->figure == figure)
    {
      return;
    }
  priv->figure = figure;
  priv->view   = (figure != NULL) ? slope_figure_get_view(figure) : NULL;
  iter         = priv->item_list;
  while (iter != NULL)
    {
      SlopeItem *item = SLOPE_ITEM(iter->data);
      /* update children scale and figure infos */
      _item_set_scale(item, self);
      iter = iter->next;
    }
}

static void _scale_clear_item_list(gpointer data)
{
  if (slope_item_get_is_managed(SLOPE_ITEM(data)) == TRUE)
    {
      g_object_unref(G_OBJECT(data));
    }
}

void slope_scale_get_layout_rect (SlopeScale *self, graphene_rect_t *rect)
{
  SlopeScalePrivate *priv;

  g_return_if_fail (self != NULL);

  priv = slope_scale_get_instance_private (self);
  graphene_rect_init_from_rect (rect, &priv->layout_rect);
}

void slope_scale_set_layout_rect(
    SlopeScale *self, double x, double y, double w, double h)
{
  SlopeScalePrivate *priv  = slope_scale_get_instance_private (self);
  graphene_rect_init (&priv->layout_rect, x, y, w, h);
}

void slope_scale_set_name(SlopeScale *self, const char *name)
{
  SlopeScalePrivate *priv = slope_scale_get_instance_private (self);
  if (priv->name != NULL)
    {
      g_free(priv->name);
    }
  if (name != NULL)
    {
      priv->name      = g_strdup(name);
      priv->show_name = TRUE;
    }
  else
    {
      priv->name = NULL;
    }
}

void _scale_handle_mouse_event(SlopeScale *self, SlopeMouseEvent *event)
{
  SlopeScalePrivate *priv = slope_scale_get_instance_private (self);
  GList *iter;
  /* this object's own custom handling */
  SLOPE_SCALE_GET_CLASS(self)->mouse_event(self, event);
  iter = priv->item_list;
  while (iter != NULL)
    {
      SlopeItem *item = SLOPE_ITEM(iter->data);
      _item_handle_mouse_event(item, event);
      iter = iter->next;
    }
}

void _scale_mouse_event_impl(SlopeScale *self, SlopeMouseEvent *event)
{
  /* provide a place holder "do nothing" implementation */
  SLOPE_UNUSED(self);
  SLOPE_UNUSED(event);
  /* pass */
}

void slope_scale_detach(SlopeScale *self)
{
  SlopeScalePrivate *priv = slope_scale_get_instance_private (self);
  if (priv->figure != NULL)
    {
      /* TODO: implement slope_figure_remove_scale() and use it here */
      priv->figure = NULL;
      priv->view   = NULL;
    }
}

SlopeFigure *slope_scale_get_figure(SlopeScale *self)
{
  SlopeScalePrivate *priv = slope_scale_get_instance_private (self);
  return priv->figure;
}

SlopeView *slope_scale_get_view(SlopeScale *self)
{
  SlopeScalePrivate *priv = slope_scale_get_instance_private (self);
  return priv->view;
}

gboolean slope_scale_get_is_managed(SlopeScale *self)
{
  SlopeScalePrivate *priv = slope_scale_get_instance_private (self);
  return priv->managed;
}

void slope_scale_set_is_managed(SlopeScale *self, gboolean managed)
{
  SlopeScalePrivate *priv = slope_scale_get_instance_private (self);
  priv->managed = managed;
}

gboolean slope_scale_get_is_visible(SlopeScale *self)
{
  SlopeScalePrivate *priv = slope_scale_get_instance_private (self);
  return priv->visible;
}

void slope_scale_set_is_visible(SlopeScale *self, gboolean visible)
{
  SlopeScalePrivate *priv = slope_scale_get_instance_private (self);
  priv->visible = visible;
}

void
slope_scale_get_background_color (SlopeScale *self, GdkRGBA *color )
{
  SlopeScalePrivate *priv = slope_scale_get_instance_private (self);
  *color = priv->background_color;
}

SlopeItem *slope_scale_get_legend(SlopeScale *self)
{
  SlopeScalePrivate *priv = slope_scale_get_instance_private (self);
  return priv->legend;
}

void
slope_scale_set_background_color (SlopeScale *self, const GdkRGBA *color)
{
  SlopeScalePrivate *priv = slope_scale_get_instance_private (self);
  priv->background_color = *color;
}

void slope_scale_get_figure_rect (SlopeScale *self, graphene_rect_t *rect)
{
  SLOPE_SCALE_GET_CLASS (self)->get_figure_rect (self, rect);
}

void
slope_scale_get_data_rect(SlopeScale *self, graphene_rect_t *rect)
{
  SLOPE_SCALE_GET_CLASS(self)->get_data_rect(self, rect);
}

void slope_scale_remove_item(SlopeScale *self, SlopeItem *item)
{
  SLOPE_SCALE_GET_CLASS(self)->remove_item(self, item);
}

GList *slope_scale_get_item_list(SlopeScale *self)
{
  SlopeScalePrivate *priv = slope_scale_get_instance_private (self);
  return priv->item_list;
}

char *slope_scale_get_name(SlopeScale *self)
{
  SlopeScalePrivate *priv = slope_scale_get_instance_private (self);
  return priv->name;
}

void slope_scale_set_show_name(SlopeScale *self, gboolean show)
{
  SlopeScalePrivate *priv = slope_scale_get_instance_private (self);
  priv->show_name = show;
}

gboolean slope_scale_get_show_name(SlopeScale *self)
{
  SlopeScalePrivate *priv = slope_scale_get_instance_private (self);
  return priv->show_name;
}

void slope_scale_set_name_top_padding(SlopeScale *self, double padding)
{
  SlopeScalePrivate *priv = slope_scale_get_instance_private (self);
  priv->name_top_padding = padding;
}

void slope_scale_add_item(SlopeScale *self, SlopeItem *item)
{
  SLOPE_SCALE_GET_CLASS(self)->add_item(self, item);
}

void
slope_scale_map (SlopeScale *self, graphene_point_t *res, const graphene_point_t *src)
{
  SLOPE_SCALE_GET_CLASS(self)->map(self, res, src);
}

void
slope_scale_unmap (SlopeScale *self, graphene_point_t *res, const graphene_point_t *src)
{
  SLOPE_SCALE_GET_CLASS(self)->unmap(self, res, src);
}

void slope_scale_rescale(SlopeScale *self)
{
  SLOPE_SCALE_GET_CLASS(self)->rescale(self);
}

/* slope/scale.c */
