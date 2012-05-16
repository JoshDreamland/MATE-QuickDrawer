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

#ifndef stored_h
#define stored_h

void show_menu(pqi inst); /* Show loaded desktop files in a menu */
void resolve_item_order(pqi inst); /* Sorts items according to mode for the specified instance */
void add_from_desktop_file(pqi inst, const char* filename); /* takes the path to a desktop file (probably from a drag context) and integrate it */
gboolean save_current_list(pqi inst); /* Saves the currently loaded launchers for next load */
gboolean load_last_list(pqi inst); /* Loads any preferences from last run for the specified panelapplet and corresponding instance */
void menu_launch_first(pqi inst); /* Launch frontmost item */

typedef struct menu_iter_ {
	struct stored_item_ *si;
	char reverse;
	char showfirst;
	char valid;
	GIcon* icon;
	const gchar* text;
} menu_iter;


enum {
  mode_static = 0,
  mode_recent = 1,
  mode_unrecent = 2,
  mode_infrequent = 3,
  mode_frequent = 4
};

void menu_iter_next(menu_iter* it);
void menu_iterate_begin(pqi inst, menu_iter *it, int showfirst);
void menu_iterate_rbegin(pqi inst, menu_iter *it, int showfirst);
void stored_item_switch_id(pqi inst, struct stored_item_ *si, struct stored_item_ *si2);
void stored_item_remove(pqi inst, struct stored_item_ *si);
gint stored_item_get_id(struct stored_item_ *si);

const char* stored_item_get_name(struct stored_item_ *si);
const char* stored_item_get_description(struct stored_item_ *si);
const char* stored_item_get_last_access(struct stored_item_ *si);
int stored_item_get_uses(struct stored_item_ *si);

#endif /* defined stored_h */
