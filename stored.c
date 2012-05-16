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

#include <stdlib.h> /* malloc */
#include <string.h> /* strlen, memcpy */
#include <gtk/gtk.h> /* Menus and UI junk */
#include <mate-panel-applet.h> /* Generic applet creation */
#include <mateconf/mateconf-client.h> /* Settings in general */
#include <mate-panel-applet-mateconf.h> /* Settings for applets */
#include <gio/gio.h>  /* Launchers and junk */
#include <gio/gdesktopappinfo.h> /* Continued */
#include <glib.h> /* Time. */

#include "quickdrawer.h"
#include "highlight.h"
#include "stored.h"

/* New GTK function that isn't in this implementation */

unsigned long long make_timestamp()
{
	GTimeVal tv;
	g_get_current_time(&tv);
	return ((unsigned long long)tv.tv_sec) << 32 | ((unsigned long long)tv.tv_usec);
}



/* Define the structure that will contain */
typedef struct stored_item_
{
	int id; /* The order in which the item was added (this is mutable only by the user via some editor). */
	int uses; /* How many times this item has been used */
	gint64 lastaccess; /* Time when we last accessed this */
	const char* file; /* The name of the desktop file representing this shortcut */
	GDesktopAppInfo* dfile; /* The loaded desktop file of our item */
	struct stored_item_*next, *prev; /* The next item on our list, and the previous one for sorting purposes */
	pqi parent; /* The inst housing us */
} stored_item, *psi;

/* Accessors for our structure */

const char* stored_item_get_name(psi si)
{
	return g_app_info_get_name(G_APP_INFO(si->dfile));
}
const char* stored_item_get_description(psi si)
{
	return g_app_info_get_description(G_APP_INFO(si->dfile));
}
const char* stored_item_get_last_access(psi si)
{
	GTimeVal tv;
	if (!si->lastaccess)
		return "Never";
	tv.tv_sec = si->lastaccess >> 32;
	tv.tv_usec = si->lastaccess & 0xFFFFFFFF;
	return g_time_val_to_iso8601(&tv);
}
int stored_item_get_uses(psi si)
{
	return si->uses;
}

/* Allocator function */
static void stored_item_add(pqi inst, int id, int uses, const char* file, int tsec, int tusec)
{
	psi ret;
	GDesktopAppInfo* dai = g_desktop_app_info_new_from_filename(file);
	
	if (!dai)
		return;
	
	ret = (psi)malloc(sizeof(stored_item));
	ret->id = id;
	ret->uses = uses;
	ret->file = file;
	ret->lastaccess = make_timestamp();
	
	if (inst->items)
		ret->prev = inst->lastitem,
		inst->lastitem->next = ret,
		inst->lastitem = ret;
	else
		inst->items = inst->lastitem = ret,
		ret->prev = NULL;
	
	ret->dfile = dai;
	ret->next = NULL;
	
	ret->lastaccess = (((long long)tsec << 32) | (long long)tusec);
	
	ret->parent = inst;
}


void stored_item_move_up(pqi inst, struct stored_item_ *si)
{
	psi it;
	int id = si->id;
	if (!id) return;
	id--;
	
	for (it = si->prev; it != NULL; it = it->prev)
		if (it->id == id) {
			it->id = si->id, si->id = id;
			resolve_item_order(inst);
			return;
	}
	
	for (it = si->next; it != NULL; it = it->next)
		if (it->id == id) {
			it->id = si->id, si->id = id;
			resolve_item_order(inst);
			return;
	}
}
void stored_item_move_down(pqi inst, struct stored_item_ *si)
{
	psi it;
	int id = si->id;
	if (!id) return;
	id++;
	
	for (it = si->prev; it != NULL; it = it->prev)
		if (it->id == id) {
			it->id = si->id, si->id = id;
			resolve_item_order(inst);
			return;
	}
	
	for (it = si->next; it != NULL; it = it->next)
		if (it->id == id) {
			it->id = si->id, si->id = id;
			resolve_item_order(inst);
			return;
	}
}

void stored_item_switch_id(pqi inst, struct stored_item_ *si, struct stored_item_ *si2)
{
	const int id = si->id;
	si->id = si2->id; si2->id = id;
	resolve_item_order(inst);
	save_current_list(inst);
}

void stored_item_remove(pqi inst, struct stored_item_ *si)
{
	psi it;
	int rid = si->id;
	
	if (si->prev)
		si->prev->next = si->next;
	if (si->next)
		si->next->prev = si->prev;
	if (inst->items == si)
		inst->items = si->next;
	if (inst->lastitem == si)
		inst->lastitem = si->prev;
	
	for (it = inst->items; it != NULL; it = it->next)
		if (it->id > rid)
			--it->id;
	
	g_free((void*)si->file);
	/* g_free(si->dfile); FIXME: Free this */ 
	free(si);
	
	inst->high_id--;
	resolve_item_order(inst);
	save_current_list(inst);
}

gint stored_item_get_id(struct stored_item_ *si)
{
	return si->id;
}

/* These two functions load pixbufs for whatever icons we need */
static void icon_default(pqi inst, int size)
{
	/* GtkIconTheme *ithm = gtk_icon_theme_get_for_screen(gtk_widget_get_screen(inst->box)); / * Get the image theme of our box. */
	inst->pmainicon = gdk_pixbuf_new_from_file_at_scale("/usr/share/pixmaps/quickdrawer-notset.svg",size,size,1,NULL); /*gtk_icon_theme_load_icon(ithm, "/usr/share/pixmaps/quickdrawer-notset.svg", size, 0, NULL);*/
	inst->pmainicon_glow = make_bright_pixbuf(inst->pmainicon);
	g_object_ref(inst->pmainicon); g_object_ref(inst->pmainicon_glow);
}

static void icon_gicon(pqi inst, GIcon* icon, int size)
{
	GtkIconTheme *ithm = gtk_icon_theme_get_for_screen(gtk_widget_get_screen(inst->box)); /* Get the image theme of our box. */
	GtkIconInfo *iinfo = gtk_icon_theme_lookup_by_gicon(ithm, icon, size, 0); /* We need the pixbuf of our icon. */
	inst->pmainicon = gtk_icon_info_load_icon(iinfo, NULL); /* Now we're cooking with gas. */
	inst->pmainicon_glow = make_bright_pixbuf(inst->pmainicon);
	g_object_ref(inst->pmainicon); g_object_ref(inst->pmainicon_glow);
}

/* Now we introduce functions to sort and manage our list of launchable items. */
static void listsort_move(pqi inst, psi back, psi *piter)
{
	psi const move = *piter;
	psi const iter = move->prev;
	*piter = iter; /* "iter" will need revalidated */
	
	/* Preserve inst->lastitem */
	if (inst->lastitem == move)
		inst->lastitem = iter;
	
	if (inst->items == back)
		inst->items = move;
	
	/* Cut "move" out of its current standing */
	iter->next = move->next; /* Cut Forward Next Link */
	if (move->next) move->next->prev = iter; /* Patch Forward Prev Link */
	
	/* Weave "move" into its new location behind "back" */
	if (back->prev) back->prev->next = move; /* Any item before back now points to "move" */
	else inst->items = move;  /* If we've moved it to the beginning, say so in inst */
	move->prev = back->prev; /* That item is now behind "move", not just "back"    */
	back->prev = move;      /* The item behind "back" now is therefore "move"     */
	move->next = back;     /* So move points forward to "back," the earliest item with a greater ID. */
}

void resolve_item_order(pqi inst)
{
	/* Remove the old icon images; we're about to replace them. */
	if (inst->pmainicon)
		g_object_unref(inst->pmainicon);
	if (inst->pmainicon_glow)
		g_object_unref(inst->pmainicon_glow);
	
	if (inst->items != NULL) /* We've got sorting to do */
	{ /* This uses a very simple, O(N**2) sorting method. I don't feel it calls for anything more complicated. */
		psi iter, back, move;
		switch (inst->mode) /* What are we sorting by? */
		{
			case mode_static:
					for (iter = inst->items; iter != NULL; iter = iter->next) /* For each list item */
					{
						move = NULL;
						for (back = iter->prev; back != NULL; back = back->prev) /* For each behind us */
							if (back->id > iter->id) /* If it has a higher ID, move before it so the lowest id is first */
								move = back;
							else break;
						if (move)
							listsort_move(inst,move,&iter);
					}
				break;
			case mode_recent:
					for (iter = inst->items; iter != NULL; iter = iter->next) /* For each list item */
					{
						move = NULL;
						for (back = iter->prev; back != NULL; back = back->prev) /* For each behind us */
							if (back->lastaccess < iter->lastaccess) /* If the previous one was used earlier, move before it so the most recent is first */
								move = back;
							else break;
						if (move)
							listsort_move(inst,move,&iter);
					}
				break;
			case mode_unrecent:
					for (iter = inst->items; iter != NULL; iter = iter->next) /* For each list item */
					{
						move = NULL;
						for (back = iter->prev; back != NULL; back = back->prev) /* For each behind us */
							if (back->lastaccess > iter->lastaccess) /* If the previous one was used later, move before it so the least recent is first */
								move = back;
							else break;
						if (move)
							listsort_move(inst,move,&iter);
					}
				break;
			case mode_infrequent:
					for (iter = inst->items; iter != NULL; iter = iter->next) /* For each list item */
					{
						move = NULL;
						for (back = iter->prev; back != NULL; back = back->prev) /* For each behind us */
							if (back->uses > iter->uses) /* If it was used more times, move before it so the least used comes first */
								move = back;
							else break;
						if (move)
							listsort_move(inst,move,&iter);
					}
				break;
			case mode_frequent:
					for (iter = inst->items; iter != NULL; iter = iter->next) /* For each list item */
					{
						move = NULL;
						for (back = iter->prev; back != NULL; back = back->prev) /* For each behind us */
							if (back->uses < iter->uses) /* If it was used less times, move before it so the most used comes first */
								move = back;
							else break;
						if (move)
							listsort_move(inst,move,&iter);
					}
				break;
		}
		
		/* Resolve the icon */
		if (inst->items->dfile)
		{
			GIcon *ico = g_app_info_get_icon(G_APP_INFO(inst->items->dfile));
			if (ico)
				icon_gicon(inst,ico,inst->size);
			else icon_default(inst,inst->size);
		} else icon_default(inst,inst->size);
	} else icon_default(inst,inst->size);
	gtk_widget_queue_draw(inst->box);
}


/* These functions save and load our list of items using the mateconfig interface.
** Each item has its own key in the preferences. There is also a key for the mode
** and for whether or not to display the current instance in the menu.
*/

/* Load settings from last run */
gboolean load_last_list(pqi inst)
{
	GError *err = NULL;
	register int uses; register const char *fname;
	MatePanelApplet *const applet = inst->applet;
	gboolean show_primary; int i, mode; char key[16];
	int timesec, timeusec;
	
	mode = mate_panel_applet_mateconf_get_int(applet,"mode",&err);
	if (err) { /* Couldn't find an entry for our mode. */
		resolve_item_order(inst); /* The struct is already zeroed... Just default the image. */
		return FALSE;            /* We're a brand new widget! */
	}
	show_primary = mate_panel_applet_mateconf_get_bool(applet,"show-primary",NULL);
	
	for (i = 0; ; i++)
	{
		err = NULL;
		sprintf(key,"file%d",i);
			fname = mate_panel_applet_mateconf_get_string(applet,key,&err);
		if (err || !fname) break;
		sprintf(key,"uses%d",i);
			uses = mate_panel_applet_mateconf_get_int(applet,key,&err);
		sprintf(key,"times%d",i);
			timesec =  mate_panel_applet_mateconf_get_int(applet,key,&err);
		sprintf(key,"timeu%d",i);
			timeusec = mate_panel_applet_mateconf_get_int(applet,key,&err);
		stored_item_add(inst, i, uses, fname, timesec, timeusec);
		inst->high_id = i + 1;
	}
	
	inst->mode = mode;
	inst->show_primary = show_primary;
	resolve_item_order(inst);
	return TRUE;
}

gboolean save_current_list(pqi inst)
{
	GError *err = NULL;
	psi iter; char key[16];
	MatePanelApplet *const applet = inst->applet;
	const gchar *fullkey;
	int maxkey = 0;
	
	mate_panel_applet_mateconf_set_int(applet,"mode",inst->mode, &err);
	if (err) return FALSE;
	
	mate_panel_applet_mateconf_set_bool(applet,"show-primary",inst->show_primary,&err);
	
	for (iter = inst->items; iter != NULL; iter = iter->next)
	{
		err = NULL;
		sprintf(key,"file%d",iter->id);
			mate_panel_applet_mateconf_set_string(applet,key,iter->file,&err);
		sprintf(key,"uses%d",iter->id);
			mate_panel_applet_mateconf_set_int(applet,key,iter->uses,&err);
		sprintf(key,"times%d",iter->id);
			mate_panel_applet_mateconf_set_int(applet,key,iter->lastaccess >> 32, &err);
		sprintf(key,"timeu%d",iter->id);
			mate_panel_applet_mateconf_set_int(applet,key,iter->lastaccess & 0xFFFFFFFF, &err);
		if (iter->id >= maxkey)
			maxkey = iter->id + 1;
	}
	sprintf(key,"file%d",maxkey);
	fullkey = mate_panel_applet_mateconf_get_full_key(applet,key);
	mateconf_client_unset(mateconf_client_get_default(),fullkey,NULL);
	return TRUE;
}


/* Now we have functions to work with desktop files.
** As it happens, I really only needed one. The point is to get a desktop file
** into our stored item format, which is actually just a pre-loaded desktop
** file and the pixbuf for quicker menu display.
*/

/* This method will take the path to a desktop file (probably from a drag context) and integrate it */
void add_from_desktop_file(pqi inst, const char* filename)
{
	/* FIXME: This part's slightly wasteful */
	int len = strlen(filename);
	
	if (len > 7 && memcmp(filename,"file://",7) == 0)
		filename += 7, len -= 7;
	while (filename[len-1] == '\n' || filename[len-1] == '\r' || filename[len-1] == ' ' || filename[len-1] == '\t')
		--len;
	
	char *fnn = (char*)malloc(len+1);
	memcpy(fnn,filename,len);
	fnn[len] = 0;
	
	stored_item_add(inst,inst->high_id++,0,fnn,0,0);
	resolve_item_order(inst);
}



/* This next bit is completely UI-related.
** Its purpose is to show our list in menu form so the user can pick one. The trick is deploying
** the menu in an attractive, easy-to-work with position. It would not be desirable for the most
** immediate item on the menu to be furthest from the active site of the widget.
** 
*/

/* Positioning the menu in GTK is a complete BITCH. 
   This position function does what there should have already been a position function to do. I'd add it to the spec if I could. */
struct xy { int x1,y1,x2,y2; };
void set_to_corner(GtkMenu* menu, gint *x, gint *y, gboolean *set_to_false, gpointer data)
{
	pqi const inst = (pqi)data; /* Wrap for beauty */
	
	/* Get some information about our widget */
	struct xy mxy;
	GdkWindow *window; GdkRectangle monitor; GtkAllocation alloc; GtkRequisition smenu;
	
	*set_to_false = FALSE; /* Don't touch my menu. */
	window = gtk_widget_get_parent_window(inst->box); /* getting parent window */
	gdk_window_get_origin(window, &mxy.x1, &mxy.y1); /* parent's left-top screen coordinates */
	gdk_screen_get_monitor_geometry(gtk_widget_get_screen(inst->box), gtk_menu_get_monitor(menu), &monitor); /* Get the coordinates of the rectangle this screen displays */
	mxy.x2 = mxy.x1, mxy.y2 = mxy.y1;
	
	gtk_widget_get_allocation(inst->box, &alloc); /* Get the dimensions of our widget */
	switch (mate_panel_applet_get_orient(inst->applet)) /* Which points we use depends on the orientation of our panel */
	{
		case MATE_PANEL_APPLET_ORIENT_UP:    mxy.x2 += alloc.width;  break;
		case MATE_PANEL_APPLET_ORIENT_RIGHT: mxy.x1 += alloc.width;  mxy.x2 += alloc.width;  mxy.y2 += alloc.height; break;
		case MATE_PANEL_APPLET_ORIENT_DOWN:  mxy.y1 += alloc.height; mxy.y2 += alloc.height; mxy.x2 += alloc.width;  break;
		case MATE_PANEL_APPLET_ORIENT_LEFT:  mxy.y2 += alloc.height; break;
	}
	
	mxy.x1 += monitor.x, mxy.x2 += monitor.x,
	mxy.y1 += monitor.y, mxy.y2 += monitor.y; /* Account for x and y offsets if this screen isn't at the top-left corner */
	
	gtk_widget_size_request(GTK_WIDGET(menu), &smenu); /* Get the dimensions of our menu */
	
	if (mxy.x1 + smenu.width > monitor.x + monitor.width)
	{
		if (mxy.x2 - smenu.width > monitor.x)
			mxy.x1 = mxy.x2 - smenu.width;
		else
			mxy.x1 = monitor.x + monitor.width - smenu.width;
	}
	if (mxy.y1 + smenu.height > monitor.y + monitor.height)
	{
		if (mxy.y2 - smenu.height > monitor.y)
			mxy.y1 = mxy.y2 - smenu.height;
		else
			mxy.y1 = monitor.y + monitor.height - smenu.height;
	}
	
	*x = mxy.x1, *y = mxy.y1; /* Just use the position we logged earlier */
}

/* Now we implement a set of iterator functions that will handle item lookup hell for us. */
void menu_iterate_begin(pqi inst, menu_iter *it, int showfirst)
{
	it->reverse = FALSE;
	it->showfirst = showfirst;
	it->si = (inst->items->next? showfirst? inst->items : inst->items->next : inst->items);
	it->valid = (it->si != NULL);
	
	if (it->valid)
		it->text = g_app_info_get_display_name(G_APP_INFO(it->si->dfile)),
		it->icon = g_app_info_get_icon(G_APP_INFO(it->si->dfile));
}
void menu_iterate_rbegin(pqi inst, menu_iter *it, int showfirst)
{
	it->reverse = TRUE;
	it->showfirst = showfirst;
	it->si = inst->lastitem;
	it->valid = (it->si != NULL); /* Wehther or not we show the first item, whether or not this is the first item, if it exists, we return it. */
	
	if (it->valid)
		it->text = g_app_info_get_display_name(G_APP_INFO(it->si->dfile)),
		it->icon = g_app_info_get_icon(G_APP_INFO(it->si->dfile));
}

void menu_iter_next(menu_iter* it)
{
	if (it->reverse)
		it->si = it->si->prev,
		it->valid = (it->si != NULL && (it->showfirst || it->si->prev));
	else
		it->si = it->si->next,
		it->valid = (it->si != NULL);
	
	if (it->valid)
		it->text = g_app_info_get_display_name(G_APP_INFO(it->si->dfile)),
		it->icon = g_app_info_get_icon(G_APP_INFO(it->si->dfile));
}


/* This function launches whatever item is selected in the menu deployed by the function below. */
void menu_launch(GtkMenuItem *unused, gpointer data)
{
	psi const si = (psi)data; GError *err = NULL;
	g_app_info_launch((GAppInfo*)si->dfile,NULL,NULL,&err);
	si->uses++; si->lastaccess = make_timestamp();
	resolve_item_order(((psi)data)->parent);
	save_current_list(((psi)data)->parent);
}

/* This function shows a menu for the user to select which item to launch. */
void show_menu(pqi inst)
{
	menu_iter iter; int i = 0,
	orient = mate_panel_applet_get_orient(inst->applet);
	GtkMenu* menu = GTK_MENU(gtk_menu_new());
	gboolean reverse;
	
	if (inst->clicked && inst->items)
	{
		reverse = (orient == MATE_PANEL_APPLET_ORIENT_UP);
		for (reverse? menu_iterate_rbegin(inst,&iter,inst->show_primary):menu_iterate_begin(inst,&iter,inst->show_primary); iter.valid; menu_iter_next(&iter))
		{
			GtkWidget *mi = gtk_image_menu_item_new(),
			          *lbl = gtk_label_new(iter.text),
			          *icon = gtk_image_new_from_gicon(iter.icon, GTK_ICON_SIZE_DND);
			gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), icon);
			gtk_image_menu_item_set_always_show_image(GTK_IMAGE_MENU_ITEM(mi), TRUE);
			gtk_label_set_justify(GTK_LABEL(lbl),GTK_JUSTIFY_LEFT);
			gtk_misc_set_alignment(GTK_MISC(lbl),0,0.5);
			gtk_container_add(GTK_CONTAINER(mi),lbl);
			gtk_widget_show_all(mi);
			gtk_menu_insert(menu,mi,i++);
			g_signal_connect(G_OBJECT(mi), "activate", G_CALLBACK(menu_launch), iter.si);
		}
		
		gtk_menu_attach_to_widget(menu,inst->box,NULL);
		gtk_menu_popup(menu,NULL,NULL,set_to_corner, inst, 1, 0);
	}
	inst->clicked = FALSE;
}

/* This function launches the first item on our list; the one we're displaying in the panel. */
void menu_launch_first(pqi inst)
{
	GError *err = NULL;
	if (inst && inst->items)
	{
		g_app_info_launch((GAppInfo*)inst->items->dfile,NULL,NULL,&err);
		inst->items->uses++; inst->items->lastaccess = make_timestamp();
		resolve_item_order(inst);
		save_current_list(inst);
	}
}

