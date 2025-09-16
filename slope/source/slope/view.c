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

#include <slope/figure_p.h>
#include <slope/view.h>

typedef struct _SlopeViewPrivate
{
  SlopeFigure *figure;
  gboolean     mouse_pressed;
} SlopeViewPrivate;

G_DEFINE_TYPE_WITH_CODE (SlopeView, slope_view, GTK_TYPE_DRAWING_AREA, G_ADD_PRIVATE (SlopeView))

static void _view_finalize(GObject *self);
static void _view_set_figure(SlopeView *self, SlopeFigure *figure);
static void _view_snapshot (GtkWidget *self, GtkSnapshot *snapshot);
static void _motion_controller_motion (GtkEventControllerMotion* controller,
                                       gdouble x, gdouble y,
                                       gpointer user_data);
static void _gesture_click_pressed (GtkGestureClick* self, gint n_press,
                                    gdouble x, gdouble y, gpointer user_data);
static void _gesture_click_released (GtkGestureClick* self, gint n_press,
                                     gdouble x, gdouble y, gpointer user_data);

static void slope_view_class_init(SlopeViewClass *klass)
{
  GObjectClass *object_klass = G_OBJECT_CLASS(klass);
  object_klass->finalize     = _view_finalize;
  klass->set_figure          = _view_set_figure;

  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  widget_class->snapshot = _view_snapshot;
}

static void slope_view_init(SlopeView *self)
{
  GtkWidget *       gtk_widget = GTK_WIDGET(self);
  SlopeViewPrivate *priv       = slope_view_get_instance_private (self);
  priv->figure                 = NULL;
  priv->mouse_pressed          = FALSE;
  /* minimum width and height of the widget */
  gtk_widget_set_size_request(gtk_widget, 250, 250);

  GtkEventController* motion_controller = gtk_event_controller_motion_new ();
  g_signal_connect(G_OBJECT (motion_controller),
                   "motion", G_CALLBACK(_motion_controller_motion), NULL);
  gtk_widget_add_controller (gtk_widget, motion_controller);

  GtkGesture* gesture_click = gtk_gesture_click_new ();
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture_click), 0);
  g_signal_connect(G_OBJECT (gesture_click),
                   "pressed", G_CALLBACK(_gesture_click_pressed), NULL);
  g_signal_connect(G_OBJECT (gesture_click),
                   "released", G_CALLBACK(_gesture_click_released), NULL);
  gtk_widget_add_controller (gtk_widget, GTK_EVENT_CONTROLLER (gesture_click));
}

static void _view_finalize(GObject *self)
{
  SlopeViewPrivate *priv = slope_view_get_instance_private (SLOPE_VIEW (self));
  if (priv->figure != NULL)
    {
      if (slope_figure_get_is_managed(priv->figure))
        {
          g_object_unref(priv->figure);
        }
      priv->figure = NULL;
    }
  G_OBJECT_CLASS(slope_view_parent_class)->finalize(self);
}

GtkWidget *slope_view_new()
{
  GtkWidget *self = GTK_WIDGET(g_object_new(SLOPE_VIEW_TYPE, NULL));
  return self;
}

GtkWidget *slope_view_new_with_figure(SlopeFigure *figure)
{
  GtkWidget *self = GTK_WIDGET(g_object_new(SLOPE_VIEW_TYPE, NULL));
  slope_view_set_figure(SLOPE_VIEW(self), figure);
  return self;
}

static void
_view_snapshot (GtkWidget *self, GtkSnapshot *snapshot)
{
  SlopeViewPrivate *priv = slope_view_get_instance_private (SLOPE_VIEW (self));
  graphene_rect_t out_bounds;
  cairo_t *cr;

  g_return_if_fail (priv->figure != NULL);

  if (!gtk_widget_compute_bounds (self, self, &out_bounds))
    return;

  cr = gtk_snapshot_append_cairo (snapshot, &out_bounds);
  slope_figure_draw (priv->figure, &out_bounds, cr);
}

static void
_motion_controller_motion (GtkEventControllerMotion* controller,
                           gdouble x, gdouble y,
                           gpointer user_data)
{
  SLOPE_UNUSED(user_data);

  GtkWidget * gtk_widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (controller));
  SlopeViewPrivate *priv = slope_view_get_instance_private (SLOPE_VIEW (gtk_widget));
  SlopeMouseEvent mouse_event;

  mouse_event.type = SLOPE_MOUSE_MOVE;
  if (priv->mouse_pressed == TRUE)
    mouse_event.type = SLOPE_MOUSE_MOVE_PRESSED;

  mouse_event.button = SLOPE_MOUSE_BUTTON_NONE;
  mouse_event.x = x;
  mouse_event.y = y;

  _figure_handle_mouse_event(priv->figure, &mouse_event);
}

static void
_gesture_click_pressed (GtkGestureClick* self, gint n_press,
                        gdouble x, gdouble y, gpointer user_data)
{
  SLOPE_UNUSED(user_data);

  GtkWidget * gtk_widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (self));
  SlopeViewPrivate *priv = slope_view_get_instance_private (SLOPE_VIEW (gtk_widget));
  guint button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (self));
  SlopeMouseEvent mouse_event;

  priv->mouse_pressed = TRUE;

  mouse_event.type = SLOPE_MOUSE_PRESS;
  if (n_press == 2) mouse_event.type = SLOPE_MOUSE_DOUBLE_PRESS;
  if (button == GDK_BUTTON_PRIMARY)
  {
    mouse_event.button = SLOPE_MOUSE_BUTTON_LEFT;
  }
  else if (button == GDK_BUTTON_SECONDARY)
  {
    mouse_event.button = SLOPE_MOUSE_BUTTON_RIGHT;
  }
  else
  {
    mouse_event.button = SLOPE_MOUSE_BUTTON_NONE;
  }

  mouse_event.x = x;
  mouse_event.y = y;

  _figure_handle_mouse_event(priv->figure, &mouse_event);
}

static void
_gesture_click_released (GtkGestureClick* self, gint n_press,
                         gdouble x, gdouble y, gpointer user_data)
{
  SLOPE_UNUSED(n_press);
  SLOPE_UNUSED(user_data);

  GtkWidget * gtk_widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (self));
  SlopeViewPrivate *priv = slope_view_get_instance_private (SLOPE_VIEW (gtk_widget));
  guint button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (self));
  SlopeMouseEvent mouse_event;

  priv->mouse_pressed = FALSE;

  mouse_event.type = SLOPE_MOUSE_RELEASE;

  if (button == GDK_BUTTON_PRIMARY)
  {
    mouse_event.button = SLOPE_MOUSE_BUTTON_LEFT;
  }
  else if (button == GDK_BUTTON_SECONDARY)
  {
    mouse_event.button = SLOPE_MOUSE_BUTTON_RIGHT;
  }
  else
  {
    mouse_event.button = SLOPE_MOUSE_BUTTON_NONE;
  }

  mouse_event.x = x;
  mouse_event.y = y;

  _figure_handle_mouse_event(priv->figure, &mouse_event);
}

static void _view_set_figure(SlopeView *self, SlopeFigure *figure)
{
  SlopeViewPrivate *priv = slope_view_get_instance_private (self);
  if (priv->figure != figure)
    {
      if (priv->figure != NULL)
        {
          _figure_set_view(priv->figure, NULL);
        }
      priv->figure = figure;
      if (priv->figure != NULL)
        {
          _figure_set_view(priv->figure, self);
        }
    }
}

SlopeFigure *slope_view_get_figure(SlopeFigure *self)
{
  if (self != NULL)
  {
    SlopeViewPrivate *priv = slope_view_get_instance_private (SLOPE_VIEW (self));
    return priv->figure;
  }
  return NULL;
}

void slope_view_redraw(SlopeView *self)
{
  gtk_widget_queue_draw(GTK_WIDGET(self));
}

void slope_view_set_figure(SlopeView *self, SlopeFigure *figure)
{
  SLOPE_VIEW_GET_CLASS(self)->set_figure(self, figure);
}

/* slope/view.c */
