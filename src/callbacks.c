/* callbacks.c */

/* GUI callbacks */

/* fsv - 3D File System Visualizer
 * Copyright (C)1999 Daniel Richard G. <skunk@mit.edu>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#include "common.h"
#include "callbacks.h"

#include <gtk/gtk.h>

#include "about.h"
#include "camera.h"
#include "color.h"
#include "dialog.h"
#include "fsv.h"


/* Radio menu items fire a callback on deselection as well as selection,
 * which is not quite what we want */
#define IGNORE_MENU_ITEM_DESELECT(menuitem) \
	if (!GTK_CHECK_MENU_ITEM(menuitem)->active) return


/**** MAIN WINDOW **************************************/


/** Menus **/


/* File -> Change root... */
void
on_file_change_root_activate( GtkMenuItem *menuitem, gpointer user_data )
{
	dialog_change_root( );
}


/* File -> Save settings */
void
on_file_save_settings_activate( GtkMenuItem *menuitem, gpointer user_data )
{
	g_message( "Configuration file not yet implemented" );
#if 0
	fsv_write_config( );
	color_write_config( );
#endif
}


/* File -> Exit */
void
on_file_exit_activate( GtkMenuItem *menuitem, gpointer user_data )
{
	exit( EXIT_SUCCESS );
}


/* Vis -> MapV */
void
on_vis_mapv_activate( GtkMenuItem *menuitem, gpointer user_data )
{
	IGNORE_MENU_ITEM_DESELECT(menuitem);
	if (globals.fsv_mode != FSV_MAPV)
		fsv_set_mode( FSV_MAPV );
}


/* Vis -> TreeV */
void
on_vis_treev_activate( GtkMenuItem *menuitem, gpointer user_data )
{
	IGNORE_MENU_ITEM_DESELECT(menuitem);
	if (globals.fsv_mode != FSV_TREEV)
		fsv_set_mode( FSV_TREEV );
}


/* Colors -> By node type */
void
on_color_by_nodetype_activate( GtkMenuItem *menuitem, gpointer user_data )
{
	IGNORE_MENU_ITEM_DESELECT(menuitem);
	color_set_mode( COLOR_BY_NODETYPE );
}


/* Colors -> By timestamp */
void
on_color_by_timestamp_activate( GtkMenuItem *menuitem, gpointer user_data )
{
	IGNORE_MENU_ITEM_DESELECT(menuitem);
	color_set_mode( COLOR_BY_TIMESTAMP );
}


/* Colors -> By wildcards */
void
on_color_by_wildcards_activate( GtkMenuItem *menuitem, gpointer user_data )
{
	IGNORE_MENU_ITEM_DESELECT(menuitem);
	color_set_mode( COLOR_BY_WPATTERN );
}


/* Colors -> Setup... */
void
on_color_setup_activate( GtkMenuItem *menuitem, gpointer user_data )
{
	dialog_color_setup( );
}


/* Help -> Contents... */
void
on_help_contents_activate( GtkMenuItem *menuitem, gpointer user_data )
{
	dialog_help( );
}


/* Help -> About fsv... */
void
on_help_about_fsv_activate( GtkMenuItem *menuitem, gpointer user_data )
{
	about( ABOUT_BEGIN );
}


/** Toolbar **/


/* "Back" button */
void
on_back_button_clicked( GtkButton *button, gpointer user_data )
{
	camera_look_at_previous( );
}


/* "cd /" button */
void
on_cd_root_button_clicked( GtkButton *button, gpointer user_data )
{
	camera_look_at( root_dnode );
}


/* "cd .." button */
void
on_cd_up_button_clicked( GtkButton *button, gpointer user_data )
{
	if (NODE_IS_DIR(globals.current_node->parent))
		camera_look_at( globals.current_node->parent );
}


/* "Bird's-eye view" toggle button */
void
on_birdseye_view_togglebutton_toggled( GtkToggleButton *togglebutton, gpointer user_data )
{
	camera_birdseye_view( togglebutton->active );
}


/**** DIALOG: ROOT DIRECTORY SELECTION ***********************************/


/* to be implemented */




/**** DIALOG: COLOR SETUP **************************************/


/* to be implemented */




/* end callbacks.c */
