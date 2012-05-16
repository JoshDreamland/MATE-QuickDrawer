/*
	Copyright (C) 2010 Josh Ventura
	
	This file is part of the QuickDrawer Applet.
	
	QuickDrawer is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.
	
	QuickDrawer is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.
	
	You should have received a copy of the GNU General Public License
	along with QuickDrawer.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <string.h>
#include <stdlib.h>

#include <gtk/gtk.h> /* Menus and UI junk */
#include <mate-panel-applet.h> /* Generic applet creation */

#include "highlight.h"
#include "quickdrawer.h"
#include "preferences.h"
#include "stored.h"


pqi quickdrawer_new(MatePanelApplet* applet) {
	pqi ret = (pqi)(malloc(sizeof(quickdrawer_instance)));
	ret->glowing = 0;
	ret->clicked = 0;
	ret->applet = applet;
	ret->size = mate_panel_applet_get_size(applet); /* Typically valid for first use */
	ret->pmainicon = ret->pmainicon_glow = NULL;
	ret->items = NULL;
	ret->high_id = 0;
	ret->spref = spreference_new();
	return ret;
}

GtkWidget *icon_new(const char* fname, int size)
{
	if (fname[0] != '/')
		return gtk_image_new_from_icon_name(fname, size);
	return gtk_image_new_from_pixbuf(gdk_pixbuf_new_from_file_at_scale(fname,size,size,1,NULL));
}



static GtkTargetEntry target_table[] = {
	{ "text/uri-list", 0, 0 }
};

void drag_data_received(GtkWidget *widget, GdkDragContext *drag_context, gint x, gint y, GtkSelectionData *data, guint info, guint time, gpointer user_data)
{
	if (data->data)
		add_from_desktop_file((pqi)user_data, (const char*)data->data);
	gtk_drag_finish(drag_context,TRUE,FALSE,time);
	save_current_list((pqi)user_data);
}

gboolean receive_drag(GtkWidget *widget, GdkDragContext *drag_context, gint x, gint y, guint time, gpointer user_data)
{
	GtkTargetList *TL = gtk_target_list_new(target_table,1);
	GdkAtom target = gtk_drag_dest_find_target(widget,drag_context,TL);
	GtkWidget *srcWidget = gtk_drag_get_source_widget(drag_context);
	if (target == GDK_NONE || srcWidget == NULL)
		return FALSE;
	gtk_drag_get_data(srcWidget,drag_context,target,time);
	gtk_widget_queue_draw(widget);
	return TRUE;
}


static gboolean on_mouse_enter (GtkWidget *event_box, GdkEventButton *event, gpointer data)
{
	gtk_widget_queue_draw(event_box);
	((pqi)data)->glowing = TRUE;
	return TRUE;
}

static gboolean on_mouse_leave (GtkWidget *event_box, GdkEventButton *event, gpointer data)
{
	int orientation;
	pqi const inst = (pqi)data;
	gtk_widget_queue_draw(event_box);
	inst->glowing = FALSE; /* No more glow */
	/* Show the menu if it was gestured in the correct direction */
	if (inst->clicked) {
		GtkAllocation alloc;
		int x = event->x, y = event->y;
		gtk_widget_get_allocation(inst->box, &alloc); /* Get the dimensions of our widget */
		gtk_widget_get_pointer(inst->box, &x, &y); /* Get the location of our mouse */
		
		/* Check if the mouse is dragging away from the panel */
		orientation = mate_panel_applet_get_orient(inst->applet);
		if ((orientation == MATE_PANEL_APPLET_ORIENT_UP    && x > -alloc.width /2 && x < 3*alloc.width /2 && y <= 0)
		||  (orientation == MATE_PANEL_APPLET_ORIENT_RIGHT && y > -alloc.height/2 && y < 3*alloc.height/2 && x >= alloc.width)
		||  (orientation == MATE_PANEL_APPLET_ORIENT_DOWN  && x > -alloc.width /2 && x < 3*alloc.width /2 && y >= alloc.height)
		||  (orientation == MATE_PANEL_APPLET_ORIENT_LEFT  && y > -alloc.height/2 && y < 3*alloc.height/2 && x <= 0))
		{
			/* Dragging out menu */
			show_menu(inst);
		}
		else return TRUE;
	}
	return TRUE;
}

static gboolean	on_button_press(GtkWidget *event_box, GdkEventButton *event, gpointer data)
{
	pqi const inst = (pqi)data;
	gtk_widget_queue_draw(event_box);
	if (event->button != 1)
	  return FALSE;
	
	inst->clicked = TRUE;
	inst->x = event->x;
	inst->y = event->y;
	return TRUE;
}

static gboolean	on_button_release(GtkWidget *event_box, GdkEventButton *event, gpointer data)
{
	GtkAllocation alloc;
	pqi const inst = (pqi)data;
	
	gtk_widget_queue_draw(event_box);
	if (event->button != 1)
		return TRUE;
	inst->clicked = FALSE;
	
	gtk_widget_get_allocation(inst->box, &alloc); /* Get the dimensions of our widget */
	if (inst->x > 0 && inst->y > 0 && inst->x < alloc.width && inst->y < alloc.height)
	if (event->x > 0 && event->y > 0 && event->x < alloc.width && event->y < alloc.height)
		menu_launch_first(inst);
	return TRUE;
}

static void panel_resized(MatePanelApplet *applet, GtkAllocation *alloc, gpointer data)
{
	pqi const inst = (pqi)data;
	int orientation = mate_panel_applet_get_orient(inst->applet);
	if (orientation == MATE_PANEL_APPLET_ORIENT_DOWN || orientation == MATE_PANEL_APPLET_ORIENT_UP)
		inst->size = alloc->height, gtk_widget_set_size_request(inst->box,inst->size,-1);
	if (orientation == MATE_PANEL_APPLET_ORIENT_LEFT || orientation == MATE_PANEL_APPLET_ORIENT_RIGHT )
		inst->size = alloc->width, gtk_widget_set_size_request(inst->box,-1,inst->size);
	resolve_item_order(inst);
}

gboolean draw_mypb(GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
	int off;
	GtkAllocation alloc;
	pqi const inst = (pqi)data;
	gtk_widget_get_allocation(inst->box, &alloc); /* Get the dimensions of our widget */
	off = inst->clicked ? (int)(alloc.width / 16.0) : 0; /* Use it as our draw offset */
	gdk_draw_pixbuf(widget->window,widget->style->fg_gc[gtk_widget_get_state(widget)],inst->glowing?inst->pmainicon_glow:inst->pmainicon,0,0,off-1,off-1,-1,-1,GDK_RGB_DITHER_NONE,0,0);
	return TRUE;
}

static gboolean hello_applet_fill (MatePanelApplet *applet, const gchar *iid, gpointer dataz)
{
	pqi inst;
	printf("Well, here we are again. It's always such a pleasure.\n");
	
	inst = quickdrawer_new(applet);
	inst->box = gtk_event_box_new();
	load_last_list(inst);
	
	g_signal_connect(G_OBJECT (inst->box), "button_press_event",   G_CALLBACK(on_button_press),    inst);
	g_signal_connect(G_OBJECT (inst->box), "button_release_event", G_CALLBACK(on_button_release),  inst);
	g_signal_connect(G_OBJECT (inst->box), "enter_notify_event",   G_CALLBACK(on_mouse_enter),     inst);
	g_signal_connect(G_OBJECT (inst->box), "leave_notify_event",   G_CALLBACK(on_mouse_leave),     inst);
	g_signal_connect(G_OBJECT (inst->box), "drag_data_received",   G_CALLBACK(drag_data_received), inst);
	g_signal_connect(G_OBJECT (inst->box), "drag_drop", G_CALLBACK(receive_drag), inst);
	g_signal_connect(G_OBJECT (inst->box), "expose_event", G_CALLBACK(draw_mypb), inst);
	g_signal_connect(G_OBJECT (applet), "size-allocate", G_CALLBACK(panel_resized), inst);
	
	gtk_drag_dest_set(inst->box,GTK_DEST_DEFAULT_ALL, target_table, 1, GDK_ACTION_COPY);
	mate_panel_applet_set_flags (applet, MATE_PANEL_APPLET_EXPAND_MINOR);
	gtk_container_add (GTK_CONTAINER (applet), inst->box);
	gtk_widget_show_all (GTK_WIDGET (applet));
	init_menus(inst);
	
	return TRUE;
}

#ifndef MATE_PANEL_APPLET_OUT_PROCESS_FACTORY
    #warning Outdated.
	MATE_PANEL_APPLET_BONOBO_FACTORY ("OAFIID:QuickdrawerApplet_Factory", PANEL_TYPE_APPLET, "QuickdrawerApplet", "0", hello_applet_fill, NULL)
#else
	MATE_PANEL_APPLET_OUT_PROCESS_FACTORY ("QuickdrawerAppletFactory", PANEL_TYPE_APPLET, "QuickdrawerApplet", hello_applet_fill, NULL)
#endif
			
