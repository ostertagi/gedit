/*
 * gedit-panel.c
 * This file is part of gedit
 *
 * Copyright (C) 2005 - Paolo Maggi
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the gedit Team, 2005. See the AUTHORS file for a
 * list of people on the gedit Team.
 * See the ChangeLog files for a list of changes.
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gedit-panel.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>

#include "gedit-close-button.h"
#include "gedit-window.h"
#include "gedit-debug.h"

#define PANEL_ITEM_KEY "GeditPanelItemKey"

struct _GeditPanelPrivate
{
	GtkOrientation orientation;

	GtkWidget *main_box;

	/* Title bar (vertical panel only) */
	GtkWidget *title_image;
	GtkWidget *title_label;

	/* Notebook */
	GtkWidget *notebook;
};

typedef struct _GeditPanelItem GeditPanelItem;

struct _GeditPanelItem
{
	gchar *id;
	gchar *display_name;
	GtkWidget *icon;
};

/* Properties */
enum {
	PROP_0,
	PROP_ORIENTATION
};

/* Signals */
enum {
	ITEM_ADDED,
	ITEM_REMOVED,
	CLOSE,
	FOCUS_DOCUMENT,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void	gedit_panel_constructed	(GObject *object);

G_DEFINE_TYPE_WITH_PRIVATE (GeditPanel, gedit_panel, GTK_TYPE_BIN)

static void
gedit_panel_finalize (GObject *object)
{
	G_OBJECT_CLASS (gedit_panel_parent_class)->finalize (object);
}

static void
gedit_panel_get_property (GObject    *object,
			  guint       prop_id,
			  GValue     *value,
			  GParamSpec *pspec)
{
	GeditPanel *panel = GEDIT_PANEL (object);

	switch (prop_id)
	{
		case PROP_ORIENTATION:
			g_value_set_enum (value, panel->priv->orientation);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gedit_panel_set_property (GObject      *object,
			  guint         prop_id,
			  const GValue *value,
			  GParamSpec   *pspec)
{
	GeditPanel *panel = GEDIT_PANEL (object);

	switch (prop_id)
	{
		case PROP_ORIENTATION:
			panel->priv->orientation = g_value_get_enum (value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gedit_panel_close (GeditPanel *panel)
{
	gtk_widget_hide (GTK_WIDGET (panel));
}

static void
gedit_panel_focus_document (GeditPanel *panel)
{
	GtkWidget *toplevel;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (panel));

	if (gtk_widget_is_toplevel (toplevel) && GEDIT_IS_WINDOW (toplevel))
	{
		GeditView *view;

		view = gedit_window_get_active_view (GEDIT_WINDOW (toplevel));
		if (view != NULL)
		{
			gtk_widget_grab_focus (GTK_WIDGET (view));
		}
	}
}

static void
gedit_panel_get_size (GtkWidget     *widget,
                      GtkOrientation orientation,
                      gint          *minimum,
                      gint          *natural)
{
	GtkBin *bin = GTK_BIN (widget);
	GtkWidget *child;

	if (minimum)
		*minimum = 0;

	if (natural)
		*natural = 0;

	child = gtk_bin_get_child (bin);
	if (child && gtk_widget_get_visible (child))
	{
		if (orientation == GTK_ORIENTATION_HORIZONTAL)
		{
			gtk_widget_get_preferred_width (child, minimum, natural);
		}
		else
		{
			gtk_widget_get_preferred_height (child, minimum, natural);
		}
	}
}

static void
gedit_panel_get_preferred_width (GtkWidget *widget,
                                 gint      *minimum,
                                 gint      *natural)
{
	gedit_panel_get_size (widget, GTK_ORIENTATION_HORIZONTAL, minimum, natural);
}

static void
gedit_panel_get_preferred_height (GtkWidget *widget,
                                  gint      *minimum,
                                  gint      *natural)
{
	gedit_panel_get_size (widget, GTK_ORIENTATION_VERTICAL, minimum, natural);
}

static void
gedit_panel_size_allocate (GtkWidget     *widget,
                           GtkAllocation *allocation)
{
	GtkBin *bin = GTK_BIN (widget);
	GtkWidget *child;

	GTK_WIDGET_CLASS (gedit_panel_parent_class)->size_allocate (widget, allocation);

	child = gtk_bin_get_child (bin);
	if (child && gtk_widget_get_visible (child))
	{
		gtk_widget_size_allocate (child, allocation);
	}
}

static void
gedit_panel_grab_focus (GtkWidget *w)
{
	GeditPanel *panel = GEDIT_PANEL (w);
	GtkWidget *tab;
	gint n;

	n = gtk_notebook_get_current_page (GTK_NOTEBOOK (panel->priv->notebook));
	if (n == -1)
		return;

	tab = gtk_notebook_get_nth_page (GTK_NOTEBOOK (panel->priv->notebook),
					 n);
	g_return_if_fail (tab != NULL);

	gtk_widget_grab_focus (tab);
}

static void
gedit_panel_class_init (GeditPanelClass *klass)
{
	GtkBindingSet *binding_set;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->constructed = gedit_panel_constructed;
	object_class->finalize = gedit_panel_finalize;
	object_class->get_property = gedit_panel_get_property;
	object_class->set_property = gedit_panel_set_property;

	widget_class->get_preferred_width = gedit_panel_get_preferred_width;
	widget_class->get_preferred_height = gedit_panel_get_preferred_height;
	widget_class->size_allocate = gedit_panel_size_allocate;
	widget_class->grab_focus = gedit_panel_grab_focus;

	klass->close = gedit_panel_close;
	klass->focus_document = gedit_panel_focus_document;

	g_object_class_install_property (object_class,
	                                 PROP_ORIENTATION,
	                                 g_param_spec_enum ("orientation",
	                                                    "Panel Orientation",
	                                                    "The panel's orientation",
	                                                    GTK_TYPE_ORIENTATION,
	                                                    GTK_ORIENTATION_VERTICAL,
	                                                    G_PARAM_READWRITE |
	                                                    G_PARAM_CONSTRUCT_ONLY |
	                                                    G_PARAM_STATIC_STRINGS));

	signals[ITEM_ADDED] =
		g_signal_new ("item_added",
			      G_OBJECT_CLASS_TYPE (klass),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GeditPanelClass, item_added),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1,
			      GTK_TYPE_WIDGET);
	signals[ITEM_REMOVED] =
		g_signal_new ("item_removed",
			      G_OBJECT_CLASS_TYPE (klass),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GeditPanelClass, item_removed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1,
			      GTK_TYPE_WIDGET);

	/* Keybinding signals */
	signals[CLOSE] =
		g_signal_new ("close",
			      G_OBJECT_CLASS_TYPE (klass),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (GeditPanelClass, close),
		  	      NULL, NULL,
		  	      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	signals[FOCUS_DOCUMENT] =
		g_signal_new ("focus_document",
			      G_OBJECT_CLASS_TYPE (klass),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (GeditPanelClass, focus_document),
		  	      NULL, NULL,
		  	      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	binding_set = gtk_binding_set_by_class (klass);

	gtk_binding_entry_add_signal (binding_set,
				      GDK_KEY_Escape,
				      0,
				      "close",
				      0);
	gtk_binding_entry_add_signal (binding_set,
				      GDK_KEY_Return,
				      GDK_CONTROL_MASK,
				      "focus_document",
				      0);
}

/* This is ugly, since it supports only known
 * storage types of GtkImage, otherwise fall back
 * to the empty icon.
 * See http://bugzilla.gnome.org/show_bug.cgi?id=317520.
 */
static void
set_gtk_image_from_gtk_image (GtkImage *image,
			      GtkImage *source)
{
	switch (gtk_image_get_storage_type (source))
	{
		case GTK_IMAGE_EMPTY:
			gtk_image_clear (image);
			break;
		case GTK_IMAGE_PIXBUF:
			{
				GdkPixbuf *pb;

				pb = gtk_image_get_pixbuf (source);
				gtk_image_set_from_pixbuf (image, pb);
			}
			break;
		G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
		case GTK_IMAGE_STOCK:
			{
				gchar *s_id;
				GtkIconSize s;

				gtk_image_get_stock (source, &s_id, &s);
				gtk_image_set_from_stock (image, s_id, s);
			}
			break;
		case GTK_IMAGE_ICON_SET:
			{
				GtkIconSet *is;
				GtkIconSize s;

				gtk_image_get_icon_set (source, &is, &s);
				gtk_image_set_from_icon_set (image, is, s);
			}
			break;
		G_GNUC_END_IGNORE_DEPRECATIONS;
		case GTK_IMAGE_ANIMATION:
			{
				GdkPixbufAnimation *a;

				a = gtk_image_get_animation (source);
				gtk_image_set_from_animation (image, a);
			}
			break;
		case GTK_IMAGE_ICON_NAME:
			{
				const gchar *n;
				GtkIconSize s;

				gtk_image_get_icon_name (source, &n, &s);
				gtk_image_set_from_icon_name (image, n, s);
			}
			break;
		default:
			gtk_image_set_from_icon_name (image,
						      "text-x-generic",
						      GTK_ICON_SIZE_MENU);
	}
}

static void
sync_title (GeditPanel     *panel,
	    GeditPanelItem *item)
{
	if (panel->priv->orientation != GTK_ORIENTATION_VERTICAL)
		return;

	if (item != NULL)
	{
		gtk_label_set_text (GTK_LABEL (panel->priv->title_label),
				    item->display_name);

		set_gtk_image_from_gtk_image (GTK_IMAGE (panel->priv->title_image),
					      GTK_IMAGE (item->icon));
	}
	else
	{
		gtk_label_set_text (GTK_LABEL (panel->priv->title_label),
				    _("Empty"));

		gtk_image_set_from_icon_name (GTK_IMAGE (panel->priv->title_image),
					      "text-x-generic",
					      GTK_ICON_SIZE_MENU);
	}
}

static void
notebook_page_changed (GtkNotebook *notebook,
                       GtkWidget   *page,
                       guint        page_num,
                       GeditPanel  *panel)
{
	GtkWidget *item;
	GeditPanelItem *data;

	item = gtk_notebook_get_nth_page (notebook, page_num);
	g_return_if_fail (item != NULL);

	data = (GeditPanelItem *)g_object_get_data (G_OBJECT (item),
						    PANEL_ITEM_KEY);
	g_return_if_fail (data != NULL);

	sync_title (panel, data);
}

static void
panel_show (GeditPanel *panel,
	    gpointer    user_data)
{
	GtkNotebook *nb;
	gint page;

	nb = GTK_NOTEBOOK (panel->priv->notebook);

	page = gtk_notebook_get_current_page (nb);

	if (page != -1)
		notebook_page_changed (nb, NULL, page, panel);
}

static void
gedit_panel_init (GeditPanel *panel)
{
	panel->priv = gedit_panel_get_instance_private (panel);

	panel->priv->main_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_show (panel->priv->main_box);
	gtk_container_add (GTK_CONTAINER (panel), panel->priv->main_box);
}

static void
close_button_clicked_cb (GtkWidget *widget,
			 GtkWidget *panel)
{
	gtk_widget_hide (panel);
}

static GtkWidget *
create_close_button (GeditPanel *panel)
{
	GtkWidget *button;

	button = gedit_close_button_new ();

	gtk_widget_set_tooltip_text (button, _("Hide panel"));

	g_signal_connect (button,
			  "clicked",
			  G_CALLBACK (close_button_clicked_cb),
			  panel);

	return button;
}

static void
build_notebook_for_panel (GeditPanel *panel)
{
	/* Create the panel notebook */
	panel->priv->notebook = gtk_notebook_new ();

	gtk_notebook_set_tab_pos (GTK_NOTEBOOK (panel->priv->notebook),
				  GTK_POS_BOTTOM);
	gtk_notebook_set_scrollable (GTK_NOTEBOOK (panel->priv->notebook),
				     TRUE);
	gtk_notebook_popup_enable (GTK_NOTEBOOK (panel->priv->notebook));

	gtk_widget_show (GTK_WIDGET (panel->priv->notebook));

	g_signal_connect (panel->priv->notebook,
			  "switch-page",
			  G_CALLBACK (notebook_page_changed),
			  panel);
}

static void
build_horizontal_panel (GeditPanel *panel)
{
	GtkWidget *box;
	GtkWidget *sidebar;
	GtkWidget *close_button;

	box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

	gtk_box_pack_start (GTK_BOX (box),
			    panel->priv->notebook,
			    TRUE,
			    TRUE,
			    0);

	/* Toolbar, close button and first separator */
	sidebar = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_container_set_border_width (GTK_CONTAINER (sidebar), 4);

	gtk_box_pack_start (GTK_BOX (box),
			    sidebar,
			    FALSE,
			    FALSE,
			    0);

	close_button = create_close_button (panel);

	gtk_box_pack_start (GTK_BOX (sidebar),
			    close_button,
			    FALSE,
			    FALSE,
			    0);

	gtk_widget_show_all (box);

	gtk_box_pack_start (GTK_BOX (panel->priv->main_box),
			    box,
			    TRUE,
			    TRUE,
			    0);
}

static void
build_vertical_panel (GeditPanel *panel)
{
	GtkStyleContext *context;
	GtkWidget *title_frame;
	GtkWidget *title_hbox;
	GtkWidget *icon_name_hbox;
	GtkWidget *dummy_label;
	GtkWidget *close_button;

	/* Create title */
	title_frame = gtk_frame_new (NULL);
	context = gtk_widget_get_style_context (GTK_WIDGET (title_frame));
	gtk_style_context_add_class (context, "title");
	gtk_box_pack_start (GTK_BOX (panel->priv->main_box), title_frame,
	                             FALSE, FALSE, 0);

	title_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_container_add (GTK_CONTAINER (title_frame), title_hbox);

	icon_name_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start (GTK_BOX (title_hbox),
			    icon_name_hbox,
			    TRUE,
			    TRUE,
			    0);

	panel->priv->title_image = gtk_image_new_from_icon_name ("text-x-generic",
	                                                         GTK_ICON_SIZE_MENU);
	gtk_box_pack_start (GTK_BOX (icon_name_hbox),
			    panel->priv->title_image,
			    FALSE,
			    TRUE,
			    0);

	dummy_label = gtk_label_new (" ");

	gtk_box_pack_start (GTK_BOX (icon_name_hbox),
			    dummy_label,
			    FALSE,
			    FALSE,
			    0);

	panel->priv->title_label = gtk_label_new (_("Empty"));
	gtk_widget_set_halign (panel->priv->title_label, GTK_ALIGN_START);
	gtk_label_set_ellipsize(GTK_LABEL (panel->priv->title_label), PANGO_ELLIPSIZE_END);

	gtk_box_pack_start (GTK_BOX (icon_name_hbox),
			    panel->priv->title_label,
			    TRUE,
			    TRUE,
			    0);

	close_button = create_close_button (panel);

	gtk_box_pack_start (GTK_BOX (title_hbox),
			    close_button,
			    FALSE,
			    FALSE,
			    0);

	gtk_widget_show_all (title_frame);

	gtk_box_pack_start (GTK_BOX (panel->priv->main_box),
			    panel->priv->notebook,
			    TRUE,
			    TRUE,
			    0);
}

static void
gedit_panel_constructed (GObject *object)
{
	GeditPanel *panel = GEDIT_PANEL (object);
	GtkStyleContext *context;

	/* Build the panel, now that we know the orientation
			   (_init has been called previously) */

	context = gtk_widget_get_style_context (GTK_WIDGET (panel));

	build_notebook_for_panel (panel);
	if (panel->priv->orientation == GTK_ORIENTATION_HORIZONTAL)
	{
		build_horizontal_panel (panel);
		gtk_style_context_add_class (context, GTK_STYLE_CLASS_HORIZONTAL);
	}
	else
	{
		build_vertical_panel (panel);
		gtk_style_context_add_class (context, GTK_STYLE_CLASS_VERTICAL);
	}

	g_signal_connect (panel,
			  "show",
			  G_CALLBACK (panel_show),
			  NULL);

	G_OBJECT_CLASS (gedit_panel_parent_class)->constructed (object);
}

/**
 * gedit_panel_new:
 * @orientation: a #GtkOrientation
 *
 * Creates a new #GeditPanel with the given @orientation. You shouldn't create
 * a new panel use gedit_window_get_side_panel() or gedit_window_get_bottom_panel()
 * instead.
 *
 * Returns: a new #GeditPanel object.
 */
GtkWidget *
gedit_panel_new (GtkOrientation orientation)
{
	return GTK_WIDGET (g_object_new (GEDIT_TYPE_PANEL,
					 "orientation", orientation,
					 NULL));
}

static GtkWidget *
build_tab_label (GeditPanel  *panel,
		 GtkWidget   *item,
		 const gchar *name,
		 GtkWidget   *icon)
{
	GtkWidget *hbox, *label_hbox, *label_ebox;
	GtkWidget *label;

	/* set hbox spacing and label padding (see below) so that there's an
	 * equal amount of space around the label */
	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);

	label_ebox = gtk_event_box_new ();
	gtk_event_box_set_visible_window (GTK_EVENT_BOX (label_ebox), FALSE);
	gtk_box_pack_start (GTK_BOX (hbox), label_ebox, TRUE, TRUE, 0);

	label_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_container_add (GTK_CONTAINER (label_ebox), label_hbox);

	/* setup icon */
	gtk_box_pack_start (GTK_BOX (label_hbox), icon, FALSE, FALSE, 0);

	/* setup label */
	label = gtk_label_new (name);
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_misc_set_padding (GTK_MISC (label), 0, 0);
	gtk_box_pack_start (GTK_BOX (label_hbox), label, TRUE, TRUE, 0);

	gtk_widget_set_tooltip_text (label_ebox, name);

	gtk_widget_show_all (hbox);

	if (panel->priv->orientation == GTK_ORIENTATION_VERTICAL)
	{
		gtk_widget_hide(label);
	}

	g_object_set_data (G_OBJECT (item), "label", label);
	g_object_set_data (G_OBJECT (item), "hbox", hbox);

	return hbox;
}

static gboolean
item_exists (GeditPanel  *panel,
	     const gchar *id)
{
	GeditPanelItem *data;
	GList *items, *l;
	gboolean exists = FALSE;

	items = gtk_container_get_children (GTK_CONTAINER (panel->priv->notebook));

	for (l = items; l != NULL; l = g_list_next (l))
	{
		data = (GeditPanelItem *)g_object_get_data (G_OBJECT (l->data),
						            PANEL_ITEM_KEY);
		g_return_val_if_fail (data != NULL, FALSE);

		if (strcmp (data->id, id) == 0)
		{
			exists = TRUE;
			break;
		}
	}

	g_list_free (items);

	return exists;
}

/**
 * gedit_panel_add_item:
 * @panel: a #GeditPanel
 * @item: the #GtkWidget to add to the @panel
 * @id: unique name for the new item
 * @display_name: the name to be shown in the @panel
 * @image: (allow-none): the image to be shown in the @panel, or %NULL
 *
 * Adds a new item to the @panel.
 *
 * Returns: %TRUE is the item was successfully added.
 */
gboolean
gedit_panel_add_item (GeditPanel  *panel,
		      GtkWidget   *item,
		      const gchar *id,
		      const gchar *display_name,
		      GtkWidget   *image)
{
	GeditPanelItem *data;
	GtkWidget *tab_label;
	GtkWidget *menu_label;
	gint w, h;

	g_return_val_if_fail (GEDIT_IS_PANEL (panel), FALSE);
	g_return_val_if_fail (GTK_IS_WIDGET (item), FALSE);
	g_return_val_if_fail (id != NULL, FALSE);
	g_return_val_if_fail (display_name != NULL, FALSE);
	g_return_val_if_fail (image == NULL || GTK_IS_IMAGE (image), FALSE);

	if (item_exists (panel, id))
	{
		g_critical ("You are trying to add an item with an id that already exists");
		return FALSE;
	}

	data = g_slice_new (GeditPanelItem);
	data->id = g_strdup (id);
	data->display_name = g_strdup (display_name);

	if (image == NULL)
	{
		/* default to empty */
		data->icon = gtk_image_new_from_icon_name ("text-x-generic",
						           GTK_ICON_SIZE_MENU);
	}
	else
	{
		data->icon = image;
	}

	gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &w, &h);
	gtk_widget_set_size_request (data->icon, w, h);

	g_object_set_data (G_OBJECT (item),
		           PANEL_ITEM_KEY,
		           data);

	tab_label = build_tab_label (panel, item, data->display_name, data->icon);

	menu_label = gtk_label_new (display_name);
	gtk_widget_set_halign (menu_label, GTK_ALIGN_START);

	if (!gtk_widget_get_visible (item))
	{
		gtk_widget_show (item);
	}

	gtk_notebook_append_page_menu (GTK_NOTEBOOK (panel->priv->notebook),
				       item,
				       tab_label,
				       menu_label);

	g_signal_emit (G_OBJECT (panel), signals[ITEM_ADDED], 0, item);

	return TRUE;
}

/**
 * gedit_panel_add_item_with_stock_icon:
 * @panel: a #GeditPanel
 * @item: the #GtkWidget to add to the @panel
 * @id: unique name for the new item
 * @display_name: the name to be shown in the @panel
 * @stock_id: (allow-none): a stock id, or %NULL
 *
 * Same as gedit_panel_add_item() but using an image from stock.
 *
 * Returns: %TRUE is the item was successfully added.
 */
gboolean
gedit_panel_add_item_with_stock_icon (GeditPanel  *panel,
				      GtkWidget   *item,
				      const gchar *id,
				      const gchar *display_name,
				      const gchar *stock_id)
{
	GtkWidget *icon = NULL;

	if (stock_id != NULL)
	{
		G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
		icon = gtk_image_new_from_stock (stock_id,
						 GTK_ICON_SIZE_MENU);
		G_GNUC_END_IGNORE_DEPRECATIONS;
	}

	return gedit_panel_add_item (panel, item, id, display_name, icon);
}

/**
 * gedit_panel_remove_item:
 * @panel: a #GeditPanel
 * @item: the item to be removed from the panel
 *
 * Removes the widget @item from the panel if it is in the @panel and returns
 * %TRUE if there was not any problem.
 *
 * Returns: %TRUE if it was well removed.
 */
gboolean
gedit_panel_remove_item (GeditPanel *panel,
			 GtkWidget  *item)
{
	GeditPanelItem *data;
	gint page_num;

	g_return_val_if_fail (GEDIT_IS_PANEL (panel), FALSE);
	g_return_val_if_fail (GTK_IS_WIDGET (item), FALSE);

	page_num = gtk_notebook_page_num (GTK_NOTEBOOK (panel->priv->notebook),
					  item);

	if (page_num == -1)
		return FALSE;

	data = (GeditPanelItem *)g_object_get_data (G_OBJECT (item),
					            PANEL_ITEM_KEY);
	g_return_val_if_fail (data != NULL, FALSE);

	g_free (data->id);
	g_free (data->display_name);
	g_slice_free (GeditPanelItem, data);

	g_object_set_data (G_OBJECT (item),
		           PANEL_ITEM_KEY,
		           NULL);

	/* ref the item to keep it alive during signal emission */
	g_object_ref (G_OBJECT (item));

	gtk_notebook_remove_page (GTK_NOTEBOOK (panel->priv->notebook),
				  page_num);

	/* if we removed all the pages, reset the title */
	if (gtk_notebook_get_n_pages (GTK_NOTEBOOK (panel->priv->notebook)) == 0)
		sync_title (panel, NULL);

	g_signal_emit (G_OBJECT (panel), signals[ITEM_REMOVED], 0, item);

	g_object_unref (G_OBJECT (item));

	return TRUE;
}

/**
 * gedit_panel_activate_item:
 * @panel: a #GeditPanel
 * @item: the item to be activated
 *
 * Switches to the page that contains @item.
 *
 * Returns: %TRUE if it was activated
 */
gboolean
gedit_panel_activate_item (GeditPanel *panel,
			   GtkWidget  *item)
{
	gint page_num;

	g_return_val_if_fail (GEDIT_IS_PANEL (panel), FALSE);
	g_return_val_if_fail (GTK_IS_WIDGET (item), FALSE);

	page_num = gtk_notebook_page_num (GTK_NOTEBOOK (panel->priv->notebook),
					  item);

	if (page_num == -1)
		return FALSE;

	gtk_notebook_set_current_page (GTK_NOTEBOOK (panel->priv->notebook),
				       page_num);

	return TRUE;
}

/**
 * gedit_panel_get_active:
 * @panel: a #GeditPanel
 *
 * Gets the active item in @panel
 *
 * Returns: (transfer none): the active item in @panel
 */
GtkWidget *
gedit_panel_get_active (GeditPanel *panel)
{
	gint current;

	g_return_val_if_fail (GEDIT_IS_PANEL (panel), NULL);

	current = gtk_notebook_get_current_page (GTK_NOTEBOOK (panel->priv->notebook));

	if (current == -1)
	{
		return NULL;
	}

	return gtk_notebook_get_nth_page (GTK_NOTEBOOK (panel->priv->notebook), current);
}

/**
 * gedit_panel_item_is_active:
 * @panel: a #GeditPanel
 * @item: a #GtkWidget
 *
 * Returns whether @item is the active widget in @panel
 *
 * Returns: %TRUE if @item is the active widget
 */
gboolean
gedit_panel_item_is_active (GeditPanel *panel,
			    GtkWidget  *item)
{
	gint cur_page;
	gint page_num;

	g_return_val_if_fail (GEDIT_IS_PANEL (panel), FALSE);
	g_return_val_if_fail (GTK_IS_WIDGET (item), FALSE);

	page_num = gtk_notebook_page_num (GTK_NOTEBOOK (panel->priv->notebook),
					  item);

	if (page_num == -1)
		return FALSE;

	cur_page = gtk_notebook_get_current_page (
				GTK_NOTEBOOK (panel->priv->notebook));

	return (page_num == cur_page);
}

/**
 * gedit_panel_get_orientation:
 * @panel: a #GeditPanel
 *
 * Gets the orientation of the @panel.
 *
 * Returns: the #GtkOrientation of #GeditPanel
 */
GtkOrientation
gedit_panel_get_orientation (GeditPanel *panel)
{
	g_return_val_if_fail (GEDIT_IS_PANEL (panel), GTK_ORIENTATION_VERTICAL);

	return panel->priv->orientation;
}

/**
 * gedit_panel_get_n_items:
 * @panel: a #GeditPanel
 *
 * Gets the number of items in a @panel.
 *
 * Returns: the number of items contained in #GeditPanel
 */
gint
gedit_panel_get_n_items (GeditPanel *panel)
{
	g_return_val_if_fail (GEDIT_IS_PANEL (panel), -1);

	return gtk_notebook_get_n_pages (GTK_NOTEBOOK (panel->priv->notebook));
}

gint
_gedit_panel_get_active_item_id (GeditPanel *panel)
{
	gint cur_page;
	GtkWidget *item;
	GeditPanelItem *data;

	g_return_val_if_fail (GEDIT_IS_PANEL (panel), 0);

	cur_page = gtk_notebook_get_current_page (
				GTK_NOTEBOOK (panel->priv->notebook));
	if (cur_page == -1)
		return 0;

	item = gtk_notebook_get_nth_page (
				GTK_NOTEBOOK (panel->priv->notebook),
				cur_page);

	data = (GeditPanelItem *)g_object_get_data (G_OBJECT (item),
					            PANEL_ITEM_KEY);
	g_return_val_if_fail (data != NULL, 0);

	return g_str_hash (data->id);
}

void
_gedit_panel_set_active_item_by_id (GeditPanel *panel,
				    gint        id)
{
	gint n, i;

	g_return_if_fail (GEDIT_IS_PANEL (panel));

	if (id == 0)
		return;

	n = gtk_notebook_get_n_pages (
				GTK_NOTEBOOK (panel->priv->notebook));

	for (i = 0; i < n; i++)
	{
		GtkWidget *item;
		GeditPanelItem *data;

		item = gtk_notebook_get_nth_page (
				GTK_NOTEBOOK (panel->priv->notebook), i);

		data = (GeditPanelItem *)g_object_get_data (G_OBJECT (item),
						            PANEL_ITEM_KEY);
		g_return_if_fail (data != NULL);

		if (g_str_hash (data->id) == id)
		{
			gtk_notebook_set_current_page (
				GTK_NOTEBOOK (panel->priv->notebook), i);

			return;
		}
	}
}

/* ex:set ts=8 noet: */