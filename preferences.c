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
#include "quickdrawer.h" /* We have to be editing something with these preferences */
#include "stored.h" /* Iterate stored items so we can display them for move and remove */
#include "preferences.h" /* Us */


/* XML for the menu, formatting for the about message
*******************************************************/
static const char menu_xml[] =
"<popup name=\"button3\">\n"
"   <menuitem name=\"Properties Item\" verb=\"QuickDrawerProperties\" _label=\"_Preferences...\"\n"
"             pixtype=\"stock\" pixname=\"gtk-properties\"/>\n"
"   <menuitem name=\"About Item\" verb=\"QuickDrawerAbout\" _label=\"_About...\"\n"
"             pixtype=\"stock\" pixname=\"mate-stock-about\"/>\n"
"</popup>\n",

dbus_xml[] =
"<menuitem name=\"Properties\"  action=\"ActionPreferences\" />\n"
"<menuitem name=\"About\"       action=\"ActionAbout\" />",

about_text[] = 
"<span size='xx-large' weight='ultrabold'>Mate QuickDrawer Applet</span>\n"
"\nA drawer-like applet allowing quick access to an"
"\nitem of interest.\n"
"<small>\nCopyright (c) 2010 Josh Ventura</small>\n";


/* Display a nice about dialog
*****************************************/
#ifndef MATE_PANEL_APPLET_OUT_PROCESS_FACTORY
static void display_about_dialog(BonoboUIComponent *uic, gpointer applet, const char* n)
#else
static void display_about_dialog(GtkAction *action, gpointer applet)
#endif
{
	/* Construct the about box and show it here */
	GtkWidget* db = gtk_dialog_new_with_buttons("About",NULL,GTK_DIALOG_MODAL,GTK_STOCK_OK,GTK_RESPONSE_ACCEPT,NULL);
	GtkWidget* box = gtk_dialog_get_content_area(GTK_DIALOG(db));
		GtkWidget* vbox = gtk_vbox_new(FALSE,16);
			GtkWidget* img = gtk_image_new_from_file("/usr/share/pixmaps/gnome-quickdrawer.svg"); /*gtk_image_new_from_stock("/usr/share/pixmaps/gnome-quickdrawer.svg",GTK_ICON_SIZE_DIALOG);*/
			GtkWidget* lbl = gtk_label_new(NULL);
			gtk_label_set_markup(GTK_LABEL(lbl), about_text);
			gtk_label_set_justify(GTK_LABEL(lbl), GTK_JUSTIFY_CENTER);
		gtk_container_add(GTK_CONTAINER(vbox), img);
		gtk_container_add(GTK_CONTAINER(vbox), lbl);
	gtk_container_add(GTK_CONTAINER(box), vbox);
	
	gtk_widget_show_all(box);
	gtk_dialog_run(GTK_DIALOG(db));
	gtk_widget_destroy(db);
}


/* These are more functions that would be in GTK in a perfect world.
** They're somewhat too specifically tailored to add to the spec, but meh.
****************************************************************************/

/* This function gets a GdkPixbuf from a GIcon for the current theme. */
static GdkPixbuf *gicon_get_pixbuf(GtkWidget *forUseWhere, GIcon *icon, int size)
{
	GtkIconTheme *ithm = gtk_icon_theme_get_for_screen(gtk_widget_get_screen(forUseWhere)); /* Get the image theme of our box. */
	GtkIconInfo *iinfo = gtk_icon_theme_lookup_by_gicon(ithm, icon, 32, 0); /* We need the pixbuf of our icon. */
	return gtk_icon_info_load_icon(iinfo, NULL); /* Now we're cooking with gas. */
}

/* These three functions are used to wrap label text on resize. The idea was donated by Tristan from #GTK+ on irc.gnome.org. */
static void label_size_request(GtkWidget *widget, GtkRequisition *requisition, gpointer data) {
	requisition->width = 10;
}

static void label_size_allocate_after (GtkWidget *widget, GtkAllocation *allocation, GtkWidget* label)
{
	gint width = 10;
	if (allocation->width > width)
		width = allocation->width;

	gtk_widget_set_size_request (label, width, -1);
	gtk_widget_queue_draw (label);
}
static void gtk_label_wrap_nicely(GtkWidget *label)
{
	gtk_label_set_line_wrap(GTK_LABEL(label),TRUE);
	g_signal_connect (G_OBJECT (gtk_widget_get_parent(label)), "size-request", G_CALLBACK (label_size_request), label);
	g_signal_connect_after (G_OBJECT (gtk_widget_get_parent(label)), "size-allocate", G_CALLBACK (label_size_allocate_after), label);
}

/* A lot of people requsted this one. It's in GTKmm, but not GTK+. Stolen from somewhere. */
gboolean gtk_tree_model_iter_prev(GtkTreeModel *tree_model, GtkTreeIter *iter)
{
  GtkTreePath *path;
  gboolean ret;

  path = gtk_tree_model_get_path (tree_model, iter);
  ret = gtk_tree_path_prev (path);
  if (ret == TRUE)
      gtk_tree_model_get_iter (tree_model, iter, path);
  gtk_tree_path_free (path);
  return ret;
}


/* The rest of this file is for the properties dialog.
** Functions here include callbacks to make changes and one function to create and display the window.
********************************************************************************************************/

/* This structure */
struct spreference {
	GtkWidget* window;
	GtkListStore* model;
	GtkWidget* tree, *info;
};

struct spreference *spreference_new()
{
	struct spreference *sp = (struct spreference*)malloc(sizeof(struct spreference));
	sp->window = NULL;
	sp->model = NULL;
	sp->tree = NULL;
	sp->info = NULL;
	return sp;
}

static void fforeach(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
	memcpy(data,iter,sizeof(GtkTreeIter));
}


static void move_up(GtkButton *widget, gpointer data)
{
	pqi const inst = (pqi)data;
	GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(inst->spref->tree));
	GtkTreeIter iter = {0}, prev;
	GValue si = {0}, si2 = {0};
	GValue id = {0}, id2 = {0};
	
	gtk_tree_selection_selected_foreach (sel, fforeach, &iter);
	if (!gtk_list_store_iter_is_valid(inst->spref->model, &iter)) return;
	memcpy(&prev,&iter,sizeof(GtkTreeIter));
	
	gtk_tree_model_get_value(GTK_TREE_MODEL(inst->spref->model), &iter, 3, &id);
	gtk_tree_model_get_value(GTK_TREE_MODEL(inst->spref->model), &iter, 2, &si);
	
	if (gtk_tree_model_iter_prev(GTK_TREE_MODEL(inst->spref->model), &prev))
	{
		gtk_tree_model_get_value(GTK_TREE_MODEL(inst->spref->model), &prev, 3, &id2);
		gtk_tree_model_get_value(GTK_TREE_MODEL(inst->spref->model), &prev, 2, &si2);
		
		gtk_list_store_set(inst->spref->model, &iter, 3, g_value_get_int(&id2), -1);
		gtk_list_store_set(inst->spref->model, &prev, 3, g_value_get_int(&id),  -1);
		
		stored_item_switch_id(inst, (struct stored_item_*)g_value_get_pointer(&si), (struct stored_item_*)g_value_get_pointer(&si2));
	}
	gtk_widget_queue_draw(inst->box);
}
static void move_down(GtkButton *widget, gpointer data)
{
	pqi const inst = (pqi)data;
	GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(inst->spref->tree));
	GtkTreeIter iter = {0}, next;
	GValue si = {0}, si2 = {0};
	GValue id = {0}, id2 = {0};
	
	gtk_tree_selection_selected_foreach (sel, fforeach, &iter);
	if (!gtk_list_store_iter_is_valid(inst->spref->model, &iter)) return;
	memcpy(&next,&iter,sizeof(GtkTreeIter));
	
	gtk_tree_model_get_value(GTK_TREE_MODEL(inst->spref->model), &iter, 3, &id);
	gtk_tree_model_get_value(GTK_TREE_MODEL(inst->spref->model), &iter, 2, &si);
	
	if (gtk_tree_model_iter_next(GTK_TREE_MODEL(inst->spref->model), &next))
	{
		gtk_tree_model_get_value(GTK_TREE_MODEL(inst->spref->model), &next, 3, &id2);
		gtk_tree_model_get_value(GTK_TREE_MODEL(inst->spref->model), &next, 2, &si2);
		
		gtk_list_store_set(inst->spref->model, &iter, 3, g_value_get_int(&id2), -1);
		gtk_list_store_set(inst->spref->model, &next, 3, g_value_get_int(&id),  -1);
		
		stored_item_switch_id(inst, (struct stored_item_*)g_value_get_pointer(&si), (struct stored_item_*)g_value_get_pointer(&si2));
	}
	gtk_widget_queue_draw(inst->box);
}
static void move_del(GtkButton *widget, gpointer data)
{
	pqi const inst = (pqi)data;
	GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(inst->spref->tree));
	GValue stored = {0};
	GtkTreeIter iter = {0};
	
	gtk_tree_selection_selected_foreach (sel, fforeach, &iter);
	if (!gtk_list_store_iter_is_valid(inst->spref->model, &iter)) return;
	
	gtk_tree_model_get_value(GTK_TREE_MODEL(inst->spref->model), &iter, 2, &stored);
	gtk_list_store_remove(inst->spref->model, &iter);
	stored_item_remove(inst, (struct stored_item_*)g_value_get_pointer(&stored));
	gtk_widget_queue_draw(inst->box);
}

static void butn_close(GtkButton *widget, gpointer data)
{
	pqi const inst = (pqi)data;
	gtk_widget_destroy(inst->spref->window);
	
	((pqi)data)->spref->tree = NULL;
	((pqi)data)->spref->model = NULL;
	((pqi)data)->spref->window = NULL;
}
static gboolean prefdestroy(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	((pqi)data)->spref->window = NULL;
	((pqi)data)->spref->model = NULL;
	((pqi)data)->spref->tree = NULL;
	return FALSE;
}

static void check_mode_static    (GtkButton *widget, gpointer data) { ((pqi)data)->mode = mode_static;      resolve_item_order((pqi)data); }
static void check_mode_recent    (GtkButton *widget, gpointer data) { ((pqi)data)->mode = mode_recent;      resolve_item_order((pqi)data); }
static void check_mode_unrecent  (GtkButton *widget, gpointer data) { ((pqi)data)->mode = mode_unrecent;    resolve_item_order((pqi)data); }
static void check_mode_infrequent(GtkButton *widget, gpointer data) { ((pqi)data)->mode = mode_infrequent;  resolve_item_order((pqi)data); }
static void check_mode_frequent  (GtkButton *widget, gpointer data) { ((pqi)data)->mode = mode_frequent;    resolve_item_order((pqi)data); }

static void check_show_primary(GtkToggleButton *tb, gpointer data) { ((pqi)data)->show_primary = gtk_toggle_button_get_active(tb); }

static void display_info_text(GtkTreeView *tree_view, gpointer data)
{
	pqi const inst = (pqi)data;
	const char *name, *desc, *time; int uses;
	GtkTreeIter iter = { 0 };
	struct stored_item_ *si;
	GValue siv = { 0 };
	char *mtext;
	
	const char *mbase = "%s\nUses: %d\nLast access: %s\n\n%s";
	GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(inst->spref->tree));
	gtk_tree_selection_selected_foreach (sel, fforeach, &iter);
	
	if (gtk_list_store_iter_is_valid(inst->spref->model, &iter))
	{
		gtk_tree_model_get_value(GTK_TREE_MODEL(inst->spref->model), &iter, 2, &siv);
		si = (struct stored_item_*)g_value_get_pointer(&siv);
		time = stored_item_get_last_access(si);
		desc = stored_item_get_description(si); if (!desc) desc = "Item does not have a description.";
		name = stored_item_get_name(si); if (!name) name = "Untitled";
		uses = stored_item_get_uses(si);
		mtext = (char*)malloc(strlen(mbase) + strlen(name) + strlen(time) + strlen(desc) + 12);
		sprintf(mtext, mbase, name, uses, time, desc);
		gtk_label_set_text(GTK_LABEL(inst->spref->info), mtext);
		free(mtext);
	}
}

#ifndef MATE_PANEL_APPLET_OUT_PROCESS_FACTORY
static void display_settings_dialog(BonoboUIComponent *uic, gpointer data, const char* name)
#else
static void display_settings_dialog(GtkAction *action, gpointer data)
#endif
{
	pqi const inst = (pqi)data;
	menu_iter iter;
	
	/* Get this out of the way so ISO C90 can sleep at night */	
	GtkWidget
	  *vbox,
	    *lvbox,
	      *tree, *treeframe,
	      *lbuttons,
	        *butn_down,
	        *butn_del,
	        *butn_up,
	    *rtable,
	      *cb_showprimary,
	      /*cb_shuffle, // This was going to be an option, but it became unnecessary */
	      *modegroup,
	        *modetable,
	          *rb_static,
	          *rb_recent,
	          *rb_unrecent,
	          *rb_infrequent,
	          *rb_frequent,
	        *infoframe,
	      *closebutton;
	
	GSList* rbgroup;
	GtkListStore *list_store;
	GtkTreeIter tree_iter;
	
	if (inst->spref->window != NULL)
		gtk_window_present(GTK_WINDOW(inst->spref->window));
	else
	{
		/* Construct the properties dialog and show it here */
		inst->spref->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
		gtk_window_set_default_size(GTK_WINDOW(inst->spref->window),320,400);
		/* Split vertically for check options and treeview */
		vbox = gtk_hbox_new(TRUE,8);
			/* Into the first half, insert a vbox */
			lvbox = gtk_vbox_new(FALSE,6);
				/* Into the top-left section, insert a treeview */
				treeframe = gtk_frame_new(NULL);
				tree = gtk_tree_view_new();
				gtk_container_add(GTK_CONTAINER(treeframe),tree);
				gtk_box_pack_start(GTK_BOX(lvbox), treeframe, 1, 1, 2);
				/* Into the bottom-left section, insert a button box */
				lbuttons = gtk_table_new(1,3,FALSE);
					/* Into the button box, insert three buttons */
					/* Down arrow button */
					butn_down = gtk_button_new();
						/* Into that button, an up arrow. */
						gtk_container_add(GTK_CONTAINER(butn_down), gtk_arrow_new(GTK_ARROW_DOWN, GTK_SHADOW_NONE));
					gtk_table_attach(GTK_TABLE(lbuttons),butn_down, 0,1, 0,1, GTK_FILL,GTK_FILL, 2,2);
					/* Remove button */
					butn_del = gtk_button_new_with_label("Remove");
					gtk_table_attach(GTK_TABLE(lbuttons),butn_del, 1,2, 0,1, GTK_EXPAND|GTK_FILL,GTK_FILL, 2,2);
					/* Up arrow button */
					butn_up = gtk_button_new();
						/* Into that button, an up arrow. */
						gtk_container_add(GTK_CONTAINER(butn_up),   gtk_arrow_new(GTK_ARROW_UP,   GTK_SHADOW_NONE));
					gtk_table_attach(GTK_TABLE(lbuttons),butn_up, 2,3, 0,1, GTK_FILL,GTK_FILL, 2,2);
				gtk_box_pack_start(GTK_BOX(lvbox), lbuttons, 0, 1, 2);
			gtk_box_pack_start(GTK_BOX(vbox), lvbox, 1, 1, 2);
			/* Into the second half, some radios and check boxes */
			rtable = gtk_table_new(5,2,FALSE);
				/* Into the first cell, a simple check widget for whether or not to display all items in the list. */
				cb_showprimary = gtk_check_button_new_with_label("Display the frontmost icon in the menu as well as the panel");
					gtk_label_wrap_nicely(gtk_bin_get_child(GTK_BIN(cb_showprimary)));
					gtk_table_attach(GTK_TABLE(rtable),cb_showprimary, 0,2, 0,1, GTK_EXPAND|GTK_FILL,GTK_FILL, 6,6);
				/* Into the second cell, nothing. I thought I had an idea for another setting, but I didn't.
				cb_shuffle = gtk_check_button_new_with_label("Iterate between items each use");
					gtk_label_wrap_nicely(gtk_bin_get_child(GTK_BIN(cb_showprimary)));
					gtk_table_attach(GTK_TABLE(rtable),cb_shuffle, 0,2, 1,2, GTK_EXPAND|GTK_FILL,GTK_FILL, 6,6); */
				/* Into the third cell, a framed group of mode options as radio buttons */
				modegroup = gtk_frame_new("Sorting mode");
					modetable = gtk_table_new(5,1,TRUE);
					rb_static = gtk_radio_button_new_with_label(NULL,"List by order added");
						rbgroup = gtk_radio_button_get_group(GTK_RADIO_BUTTON(rb_static));
						gtk_widget_set_tooltip_text(rb_static,"The order you added items to the QuickDrawer (the order you see to the left) is the order they appear in the menu.");
						gtk_table_attach_defaults(GTK_TABLE(modetable),rb_static, 0,1, 0,1);
					rb_recent = gtk_radio_button_new_with_label(rbgroup,"Sort by most recently used");
						rbgroup = gtk_radio_button_get_group(GTK_RADIO_BUTTON(rb_recent));
						gtk_widget_set_tooltip_text(rb_recent,"The item you used most recently appears closest to the panel.");
						gtk_table_attach_defaults(GTK_TABLE(modetable),rb_recent, 0,1, 1,2);
					rb_unrecent = gtk_radio_button_new_with_label(rbgroup,"Sort by least recently used");
						rbgroup = gtk_radio_button_get_group(GTK_RADIO_BUTTON(rb_unrecent));
						gtk_widget_set_tooltip_text(rb_unrecent,"The item you used the longest time ago appears closest to the panel.");
						gtk_table_attach_defaults(GTK_TABLE(modetable),rb_unrecent, 0,1, 2,3);
					rb_infrequent = gtk_radio_button_new_with_label(rbgroup,"Sort by least frequently used");
						rbgroup = gtk_radio_button_get_group(GTK_RADIO_BUTTON(rb_infrequent));
						gtk_widget_set_tooltip_text(rb_infrequent,"The item you use the least appears closest to the panel. Good for to-do items.");
						gtk_table_attach_defaults(GTK_TABLE(modetable),rb_infrequent, 0,1, 3,4);
					rb_frequent = gtk_radio_button_new_with_label(rbgroup,"Sort by most frequently used");
						/* rbgroup = gtk_radio_button_get_group(GTK_RADIO_BUTTON(rb_frequent)); */
						gtk_widget_set_tooltip_text(rb_frequent,"The item you use most often appears closest to the panel; this is likely the behavior you want, if you want an automatic behavior at all.");
						gtk_table_attach_defaults(GTK_TABLE(modetable),rb_frequent, 0,1, 4,5);
					gtk_container_add(GTK_CONTAINER(modegroup),modetable);
				gtk_table_attach(GTK_TABLE(rtable),modegroup, 0,2, 1,3, GTK_EXPAND|GTK_FILL,GTK_SHRINK, 6,6);
				/* Into the forth, a frame with a label displaying the properties of the selected launcher */
				infoframe = gtk_frame_new("Launcher properties");
					inst->spref->info = gtk_label_new("Select a launcher to the left for information about it.");
					gtk_widget_set_size_request(inst->spref->info,-1,-1);
					gtk_misc_set_alignment(GTK_MISC(inst->spref->info),0.03,0.01);
					gtk_container_add(GTK_CONTAINER(infoframe),inst->spref->info);
					gtk_label_wrap_nicely(inst->spref->info);
				gtk_table_attach(GTK_TABLE(rtable),infoframe, 0,2, 3,4, GTK_EXPAND|GTK_FILL,GTK_FILL|GTK_EXPAND, 6,6);
				/* A close button */
				gtk_table_attach(GTK_TABLE(rtable),gtk_label_new(NULL), 0,1, 4,5, GTK_FILL | GTK_EXPAND, GTK_SHRINK, 6,6);
				closebutton = gtk_button_new_with_label("  Close  ");
				gtk_table_attach(GTK_TABLE(rtable),closebutton, 1,2, 4,5, GTK_SHRINK, GTK_SHRINK, 6,6);
			gtk_box_pack_start(GTK_BOX(vbox), rtable, 0, 1, 2);
		gtk_container_add(GTK_CONTAINER(inst->spref->window),vbox);
	
		/* Populate the tree view */
		list_store = gtk_list_store_new(4,GDK_TYPE_PIXBUF,G_TYPE_STRING,G_TYPE_POINTER,G_TYPE_INT);
		for (menu_iterate_begin(inst,&iter,TRUE); iter.valid; menu_iter_next(&iter)) {
			gtk_list_store_append(list_store, &tree_iter);
			gtk_list_store_set (list_store, &tree_iter, 0, gicon_get_pixbuf(tree, iter.icon, 32), 1, iter.text, 2, iter.si, 3, stored_item_get_id(iter.si), -1);
		}
		gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(list_store), 3, GTK_SORT_ASCENDING);
		gtk_tree_view_set_model(GTK_TREE_VIEW(tree), GTK_TREE_MODEL(list_store));
		gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree), 0, "icon",      gtk_cell_renderer_pixbuf_new(), "pixbuf", 0, NULL);
		gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree), 1, "description", gtk_cell_renderer_text_new(),   "text", 1, NULL);
		gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree), FALSE);
		
		/* Connect some signals */
		g_signal_connect(G_OBJECT(butn_up),     "clicked", G_CALLBACK(move_up),    inst);
		g_signal_connect(G_OBJECT(butn_down),   "clicked", G_CALLBACK(move_down),  inst);
		g_signal_connect(G_OBJECT(butn_del),    "clicked", G_CALLBACK(move_del),   inst);
		g_signal_connect(G_OBJECT(closebutton), "clicked", G_CALLBACK(butn_close), inst);
		
		switch (inst->mode)
		{
			case mode_static: gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(rb_static), TRUE); break;
			case mode_recent: gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(rb_recent),   TRUE); break;
			case mode_unrecent: gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(rb_unrecent), TRUE); break;
			case mode_infrequent: gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(rb_infrequent), TRUE); break;
			case mode_frequent: gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(rb_frequent),  TRUE); break;
		}
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cb_showprimary), inst->show_primary);
		
		g_signal_connect(G_OBJECT(rb_static),     "clicked", G_CALLBACK(check_mode_static),     inst);
		g_signal_connect(G_OBJECT(rb_recent),     "clicked", G_CALLBACK(check_mode_recent),     inst);
		g_signal_connect(G_OBJECT(rb_unrecent),   "clicked", G_CALLBACK(check_mode_unrecent),   inst);
		g_signal_connect(G_OBJECT(rb_infrequent), "clicked", G_CALLBACK(check_mode_infrequent), inst);
		g_signal_connect(G_OBJECT(rb_frequent),   "clicked", G_CALLBACK(check_mode_frequent),   inst);
		
		g_signal_connect(G_OBJECT(cb_showprimary),   "toggled", G_CALLBACK(check_show_primary),   inst);
		
		g_signal_connect(G_OBJECT(tree), "cursor-changed", G_CALLBACK(display_info_text), inst);
		
		g_signal_connect(G_OBJECT(inst->spref->window), "delete-event", G_CALLBACK(prefdestroy), inst);
		
		/* Show it and sto it */
		inst->spref->tree = tree;
		inst->spref->model = list_store;
		gtk_widget_show_all(inst->spref->window);
	} /* Window is NULL */
}

#ifndef MATE_PANEL_APPLET_OUT_PROCESS_FACTORY
	static const BonoboUIVerb menu_verbs [] = {
		BONOBO_UI_VERB ("QuickDrawerProperties", display_settings_dialog),
		BONOBO_UI_VERB ("QuickDrawerAbout",      display_about_dialog),
		BONOBO_UI_VERB_END
	};
	
	int init_menus(pqi inst)
	{
		mate_panel_applet_setup_menu (MATE_PANEL_APPLET(inst->applet), menu_xml, menu_verbs, inst);
		return 0;
	}
#else
	static const GtkActionEntry menu_actions [] = {
		{ "ActionPreferences", GTK_STOCK_PROPERTIES, "_Preferences", NULL, NULL, G_CALLBACK(display_settings_dialog) },
		{ "ActionAbout",       GTK_STOCK_ABOUT,      "_About",       NULL, NULL, G_CALLBACK(display_about_dialog) }
	};
	
	int init_menus(pqi inst)
	{
		GtkActionGroup *action_group = gtk_action_group_new ("QuickDrawer Applet Actions");
		/* gtk_action_group_set_translation_domain (action_group, GETTEXT_PACKAGE); TODO: Implement */
		gtk_action_group_add_actions (action_group, menu_actions,  G_N_ELEMENTS (menu_actions), inst);
		mate_panel_applet_setup_menu (MATE_PANEL_APPLET (inst->applet), dbus_xml, action_group);
		g_object_unref (action_group);
		return 0;
	}
#endif


