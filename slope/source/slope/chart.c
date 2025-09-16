/*
 * Copyright (C) 2017,2025  Elvis Teixeira, Anatoliy Sokolov
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

#include <slope/chart.h>

typedef struct _SlopeChartPrivate
{
  GtkWidget *  header;
  GtkWidget *  title;
  GtkWidget *  view;
  SlopeFigure *figure;
} SlopeChartPrivate;

G_DEFINE_TYPE_WITH_CODE (SlopeChart, slope_chart, GTK_TYPE_WINDOW, G_ADD_PRIVATE (SlopeChart))

static void _chart_finalize(GObject *self);

static void slope_chart_class_init(SlopeChartClass *klass)
{
  GObjectClass *object_klass = G_OBJECT_CLASS(klass);
  object_klass->finalize     = _chart_finalize;
}

static void slope_chart_init(SlopeChart *self)
{
  SlopeChartPrivate *priv = slope_chart_get_instance_private (self);

  priv->header = gtk_header_bar_new();
  priv->figure = slope_figure_new();
  priv->view   = slope_view_new_with_figure(priv->figure);
  gtk_window_set_default_size(GTK_WINDOW(self), 530, 500);

  priv->title = gtk_label_new ("Slope");
  gtk_header_bar_set_title_widget (GTK_HEADER_BAR (priv->header), priv->title);
  gtk_header_bar_set_show_title_buttons (GTK_HEADER_BAR(priv->header), TRUE);
  gtk_window_set_titlebar(GTK_WINDOW(self), priv->header);

  gtk_window_set_child (GTK_WINDOW (self), priv->view);
}

static void _chart_finalize(GObject *self)
{
  /* SlopeChartPrivate *priv = SLOPE_CHART_GET_PRIVATE(self); */
  G_OBJECT_CLASS(slope_chart_parent_class)->finalize(self);
}

GtkWidget *slope_chart_new()
{
  GtkWidget *self = GTK_WIDGET(g_object_new(SLOPE_CHART_TYPE, NULL));
  return self;
}

GtkWidget *slope_chart_new_detailed(const gchar *title, int width, int height)
{
  GtkWidget *        self = GTK_WIDGET(g_object_new(SLOPE_CHART_TYPE, NULL));
  SlopeChartPrivate *priv = slope_chart_get_instance_private (SLOPE_CHART (self));
  gtk_window_set_default_size(GTK_WINDOW(self), width, height);
  gtk_label_set_text (GTK_LABEL (priv->title), title);
  return self;
}

void slope_chart_add_scale(SlopeChart *self, SlopeScale *scale)
{
  SlopeChartPrivate *priv = slope_chart_get_instance_private (self);
  slope_figure_add_scale(priv->figure, scale);
}

void slope_chart_redraw(SlopeChart *self)
{
  SlopeChartPrivate *priv = slope_chart_get_instance_private (self);
  slope_view_redraw(SLOPE_VIEW(priv->view));
}

SlopeFigure *slope_chart_get_figure(SlopeChart *self)
{
  SlopeChartPrivate *priv = slope_chart_get_instance_private (self);
  return priv->figure;
}

GtkWidget *slope_chart_get_header(SlopeChart *self)
{
  SlopeChartPrivate *priv = slope_chart_get_instance_private (self);
  return priv->header;
}

GtkWidget *slope_chart_get_view(SlopeChart *self)
{
  SlopeChartPrivate *priv = slope_chart_get_instance_private (self);
  return priv->view;
}

/* slope/chart.c */
