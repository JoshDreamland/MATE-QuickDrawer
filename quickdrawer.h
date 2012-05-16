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

#ifndef quickdrawer_h
#define quickdrawer_h

typedef struct quickdrawer_instance_
{
	/* Some settings specific to this instance */
	int mode; /* The sorting and selecting mode for nested items */
	gboolean show_primary; /* Whether or not we are showing the active item in the panel */
	MatePanelApplet *applet; /* For grabbing the orientation of the panel in which we are nested, as well as settings. */
	
	/* Status information */
	int size; /* The size of the applet in pixels */
	gdouble x, y; /* The coordinates at which the mouse was last clicked */
	gboolean glowing; /* Whether or not the main icon is glowing (from mouseover) */
	gboolean clicked; /* Whether the mouse is depressed over our button */
	const char* icon_cb; /* The name or path of the icon displayed in the panel */
	
	/* Relevant GtkWidgets */
	GtkWidget *box; /* An Event Box; The widget that handles all events */
	GdkPixbuf *pmainicon, *pmainicon_glow; /* The pixbufs displayed in the panel */
	
	/* Storage */
	struct stored_item_ *items, *lastitem; /* The other items available to the user */
	int high_id; /* The next id to use when adding an item to storage */
	struct spreference *spref; /* Used by preferences.c */
} quickdrawer_instance, *pqi;

GtkWidget *icon_new(const char* fname, int size);

#endif /* defined quickdrawer_h */

