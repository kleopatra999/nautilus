/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* Nautilus - Floating status bar.
 *
 * Copyright (C) 2011 Red Hat Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#include <config.h>

#include "nautilus-floating-bar.h"

struct _NautilusFloatingBarDetails {
	gchar *label;

	GtkWidget *label_widget;
	GtkWidget *spinner;
	gboolean show_spinner;

	guint escaping_distance;
	GtkAllocation initial_allocation;
	GdkRectangle escaping_area;
};

enum {
	PROP_LABEL = 1,
	PROP_SHOW_SPINNER,
	PROP_ESCAPING_DISTANCE,
	NUM_PROPERTIES
};

enum {
	ACTION,
	NUM_SIGNALS
};

#define NAUTILUS_FLOATING_BAR_ESCAPING_DISTANCE 20

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };
static guint signals[NUM_SIGNALS] = { 0, };

G_DEFINE_TYPE (NautilusFloatingBar, nautilus_floating_bar,
               GTK_TYPE_BOX);

static void
action_button_clicked_cb (GtkButton *button,
			  NautilusFloatingBar *self)
{
	gint action_id;

	action_id = GPOINTER_TO_INT
		(g_object_get_data (G_OBJECT (button), "action-id"));
	
	g_signal_emit (self, signals[ACTION], 0, action_id);
}

static void
nautilus_floating_bar_finalize (GObject *obj)
{
	NautilusFloatingBar *self = NAUTILUS_FLOATING_BAR (obj);

	g_free (self->priv->label);

	G_OBJECT_CLASS (nautilus_floating_bar_parent_class)->finalize (obj);
}

static void
nautilus_floating_bar_get_property (GObject *object,
				    guint property_id,
				    GValue *value,
				    GParamSpec *pspec)
{
	NautilusFloatingBar *self = NAUTILUS_FLOATING_BAR (object);

	switch (property_id) {
	case PROP_LABEL:
		g_value_set_string (value, self->priv->label);
		break;
	case PROP_SHOW_SPINNER:
		g_value_set_boolean (value, self->priv->show_spinner);
		break;
	case PROP_ESCAPING_DISTANCE:
		g_value_set_uint (value, self->priv->escaping_distance);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
nautilus_floating_bar_set_property (GObject *object,
				    guint property_id,
				    const GValue *value,
				    GParamSpec *pspec)
{
	NautilusFloatingBar *self = NAUTILUS_FLOATING_BAR (object);

	switch (property_id) {
	case PROP_LABEL:
		nautilus_floating_bar_set_label (self, g_value_get_string (value));
		break;
	case PROP_SHOW_SPINNER:
		nautilus_floating_bar_set_show_spinner (self, g_value_get_boolean (value));
		break;
	case PROP_ESCAPING_DISTANCE:
		self->priv->escaping_distance = g_value_get_uint (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
update_label (NautilusFloatingBar *self)
{
	gtk_label_set_text (GTK_LABEL (self->priv->label_widget), self->priv->label);
}

static gboolean
overlay_enter_notify_cb (GtkWidget        *parent,
			 GdkEventCrossing *event,
			 gpointer          user_data)
{
	GtkWidget *widget = user_data;

	if (event->window != gtk_widget_get_window (widget)) {
		return FALSE;
	}

	if (gtk_widget_get_halign (widget) == GTK_ALIGN_START) {
		gtk_widget_set_halign (widget, GTK_ALIGN_END);
	} else {
		gtk_widget_set_halign (widget, GTK_ALIGN_START);
	}

	gtk_widget_queue_resize (widget);

	return FALSE;
}

static void
nautilus_floating_bar_parent_set (GtkWidget *widget,
				  GtkWidget *old_parent)
{
	GtkWidget *parent;

	parent = gtk_widget_get_parent (widget);

	if (old_parent != NULL) {
		g_signal_handlers_disconnect_by_func (old_parent,
						      overlay_enter_notify_cb, widget);
	}

	if (parent != NULL) {
		g_signal_connect (parent, "enter-notify-event",
				  G_CALLBACK (overlay_enter_notify_cb), widget);
	}
}

static void
nautilus_floating_bar_show (GtkWidget *widget)
{
	NautilusFloatingBar *self = NAUTILUS_FLOATING_BAR (widget);

	GTK_WIDGET_CLASS (nautilus_floating_bar_parent_class)->show (widget);

	if (self->priv->show_spinner) {
		gtk_spinner_start (GTK_SPINNER (self->priv->spinner));
	}
}

static void
nautilus_floating_bar_hide (GtkWidget *widget)
{
	NautilusFloatingBar *self = NAUTILUS_FLOATING_BAR (widget);

	GTK_WIDGET_CLASS (nautilus_floating_bar_parent_class)->hide (widget);

	gtk_spinner_stop (GTK_SPINNER (self->priv->spinner));
}

static gboolean
nautilus_floating_bar_draw (GtkWidget *widget,
			    cairo_t *cr)
{
	  GtkStyleContext *context;

	  context = gtk_widget_get_style_context (widget);

	  gtk_style_context_save (context);
	  gtk_style_context_set_state (context, gtk_widget_get_state_flags (widget));

	  gtk_render_background (context, cr, 0, 0,
				 gtk_widget_get_allocated_width (widget),
				 gtk_widget_get_allocated_height (widget));

	  gtk_render_frame (context, cr, 0, 0,
			    gtk_widget_get_allocated_width (widget),
			    gtk_widget_get_allocated_height (widget));

	  gtk_style_context_restore (context);

	  return GTK_WIDGET_CLASS (nautilus_floating_bar_parent_class)->draw (widget, cr);;
}

static void
nautilus_floating_bar_constructed (GObject *obj)
{
	NautilusFloatingBar *self = NAUTILUS_FLOATING_BAR (obj);
	GtkWidget *w, *box;

	G_OBJECT_CLASS (nautilus_floating_bar_parent_class)->constructed (obj);

	box = GTK_WIDGET (obj);

	w = gtk_spinner_new ();
	gtk_box_pack_start (GTK_BOX (box), w, FALSE, FALSE, 0);
	gtk_widget_set_visible (w, self->priv->show_spinner);
	self->priv->spinner = w;

	gtk_widget_set_size_request (w, 16, 16);
	gtk_widget_set_margin_left (w, 8);

	w = gtk_label_new (NULL);
	gtk_label_set_ellipsize (GTK_LABEL (w), PANGO_ELLIPSIZE_END);
	gtk_box_pack_start (GTK_BOX (box), w, FALSE, FALSE, 0);
	g_object_set (w,
		      "margin-top", 2,
		      "margin-bottom", 2,
		      "margin-left", 12,
		      "margin-right", 12,
		      NULL);
	self->priv->label_widget = w;
	gtk_widget_show (w);
}

static void
nautilus_floating_bar_init (NautilusFloatingBar *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, NAUTILUS_TYPE_FLOATING_BAR,
						  NautilusFloatingBarDetails);
}

static void
nautilus_floating_bar_class_init (NautilusFloatingBarClass *klass)
{
	GObjectClass *oclass = G_OBJECT_CLASS (klass);
	GtkWidgetClass *wclass = GTK_WIDGET_CLASS (klass);

	oclass->constructed = nautilus_floating_bar_constructed;
	oclass->set_property = nautilus_floating_bar_set_property;
	oclass->get_property = nautilus_floating_bar_get_property;
	oclass->finalize = nautilus_floating_bar_finalize;

	wclass->draw = nautilus_floating_bar_draw;
	wclass->show = nautilus_floating_bar_show;
	wclass->hide = nautilus_floating_bar_hide;
	wclass->parent_set = nautilus_floating_bar_parent_set;

	properties[PROP_LABEL] =
		g_param_spec_string ("label",
				     "Bar's label",
				     "Label displayed by the bar",
				     NULL,
				     G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS);
	properties[PROP_SHOW_SPINNER] =
		g_param_spec_boolean ("show-spinner",
				      "Show spinner",
				      "Whether a spinner should be shown in the floating bar",
				      FALSE,
				      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
	properties[PROP_ESCAPING_DISTANCE] =
		g_param_spec_uint ("escaping-distance",
				   "Escaping distance",
				   "Maximum distance between the mouse pointer and the widget",
				   0,
				   G_MAXUINT,
				   NAUTILUS_FLOATING_BAR_ESCAPING_DISTANCE,
				   G_PARAM_CONSTRUCT_ONLY |
				   G_PARAM_READWRITE |
				   G_PARAM_STATIC_STRINGS);

	signals[ACTION] =
		g_signal_new ("action",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0, NULL, NULL,
			      g_cclosure_marshal_VOID__INT,
			      G_TYPE_NONE, 1,
			      G_TYPE_INT);

	g_type_class_add_private (klass, sizeof (NautilusFloatingBarDetails));
	g_object_class_install_properties (oclass, NUM_PROPERTIES, properties);
}

void
nautilus_floating_bar_set_label (NautilusFloatingBar *self,
				 const gchar *label)
{
	if (g_strcmp0 (self->priv->label, label) != 0) {
		g_free (self->priv->label);
		self->priv->label = g_strdup (label);

		g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LABEL]);

		update_label (self);
	}
}

void
nautilus_floating_bar_set_show_spinner (NautilusFloatingBar *self,
					gboolean show_spinner)
{
	if (self->priv->show_spinner != show_spinner) {
		self->priv->show_spinner = show_spinner;
		gtk_widget_set_visible (self->priv->spinner,
					show_spinner);

		g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SHOW_SPINNER]);
	}
}

GtkWidget *
nautilus_floating_bar_new (const gchar *label,
			   gboolean show_spinner)
{
	return g_object_new (NAUTILUS_TYPE_FLOATING_BAR,
			     "label", label,
			     "show-spinner", show_spinner,
			     "orientation", GTK_ORIENTATION_HORIZONTAL,
			     "spacing", 8,
			     NULL);
}

void
nautilus_floating_bar_add_action (NautilusFloatingBar *self,
				  const gchar *stock_id,
				  gint action_id)
{
	GtkWidget *w, *button;

	w = gtk_image_new_from_stock (stock_id, GTK_ICON_SIZE_MENU);
	gtk_widget_show (w);

	button = gtk_button_new ();
	gtk_button_set_image (GTK_BUTTON (button), w);
	gtk_box_pack_end (GTK_BOX (self), button, FALSE, FALSE, 0);
	gtk_widget_show (button);

	g_object_set_data (G_OBJECT (button), "action-id",
			   GINT_TO_POINTER (action_id));

	g_signal_connect (button, "clicked",
			  G_CALLBACK (action_button_clicked_cb), self);
}

void
nautilus_floating_bar_cleanup_actions (NautilusFloatingBar *self)
{
	GtkWidget *widget;
	GList *children, *l;
	gpointer data;

	children = gtk_container_get_children (GTK_CONTAINER (self));
	l = children;

	while (l != NULL) {
		widget = l->data;
		data = g_object_get_data (G_OBJECT (widget), "action-id");
		l = l->next;

		if (data != NULL) {
			/* destroy this */
			gtk_widget_destroy (widget);
		}
	}

	g_list_free (children);
}
