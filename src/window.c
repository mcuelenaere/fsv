/* window.c */

/* Main window definition */

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
#include "window.h"

#include <gtk/gtk.h>

#include "about.h"
#include "callbacks.h"
#include "camera.h"
#include "color.h"
#include "dialog.h"
#include "dirtree.h"
#include "filelist.h"
#include "fsv.h"
#include "gui.h"
#include "viewport.h"

/* Toolbar button icons */
#include "xmaps/back.xpm"
#include "xmaps/cd-root.xpm"
#include "xmaps/cd-up.xpm"
#include "xmaps/birdseye_view.xpm"

/* Main program icon */
#include "xmaps/fsv-icon.xpm"


/* Color radio menu items */
static GtkWidget *color_by_nodetype_rmenu_item_w;
static GtkWidget *color_by_timestamp_rmenu_item_w;
static GtkWidget *color_by_wpattern_rmenu_item_w;

/* Bird's-eye view button (on toolbar) */
static GtkWidget *birdseye_view_tbutton_w;

/* List of widgets that can be enabled or disabled on the fly using
 * window_set_access( ) */
static GList *sw_widget_list = NULL;

/* Left and right statusbar widgets */
static GtkWidget *left_statusbar_w;
static GtkWidget *right_statusbar_w;


/* Constructs the main program window. The specified mode will be the one
 * initially selected in the Vis menu */
void
window_init( FsvMode fsv_mode )
{
	GtkWidget *main_window_w;
	GtkWidget *main_vbox_w;
	GtkWidget *menu_bar_w;
	GtkWidget *menu_w;
	GtkWidget *menu_item_w;
	GtkWidget *hpaned_w;
	GtkWidget *vpaned_w;
	GtkWidget *left_vbox_w;
	GtkWidget *right_vbox_w;
	GtkWidget *hbox_w;
	GtkWidget *button_w;
	GtkWidget *frame_w;
	GtkWidget *dir_ctree_w;
	GtkWidget *file_clist_w;
        GtkWidget *gl_area_w;
	GtkWidget *x_scrollbar_w;
	GtkWidget *y_scrollbar_w;
	int window_width, window_height;

	/* Main window widget */
	main_window_w = gtk_window_new( GTK_WINDOW_TOPLEVEL );
	gtk_window_set_title( GTK_WINDOW(main_window_w), "fsv" );
	gtk_window_set_wmclass( GTK_WINDOW(main_window_w), "main", "fsv" );
	window_width = 3 * gdk_screen_width( ) / 4;
	window_height = 2584 * window_width / 4181;
	gtk_widget_set_usize( main_window_w, window_width, window_height );
	gtk_signal_connect( GTK_OBJECT(main_window_w), "delete_event", GTK_SIGNAL_FUNC(gtk_main_quit), NULL );
	gtk_quit_add_destroy( 1, GTK_OBJECT(main_window_w) );

	/* Main vertical box widget */
	main_vbox_w = gui_vbox_add( main_window_w, 0 );

	/* Build menu bar */

	/* Menu bar widget */
	menu_bar_w = gtk_menu_bar_new( );
	gtk_box_pack_start( GTK_BOX(main_vbox_w), menu_bar_w, FALSE, FALSE, 0 );
	gtk_widget_show( menu_bar_w );

	/* File menu */
	menu_w = gui_menu_add( menu_bar_w, _("File") );
	/* File menu items */
	menu_item_w = gui_menu_item_add( menu_w, _("Change root..."), on_file_change_root_activate, NULL );
	gui_keybind( menu_item_w, _("^N") );
	G_LIST_APPEND(sw_widget_list, menu_item_w);
#if 0
	gui_menu_item_add( menu_w, _("Save settings"), on_file_save_settings_activate, NULL );
#endif
	gui_separator_add( menu_w );
	menu_item_w = gui_menu_item_add( menu_w, _("Exit"), on_file_exit_activate, NULL );
	gui_keybind( menu_item_w, _("^Q") );

	/* Vis menu */
	menu_w = gui_menu_add( menu_bar_w, _("Vis") );
	/* Vis menu items */
	gui_radio_menu_begin( fsv_mode -1 );
#if 0 /* DiscV mode needs more work */
	gui_radio_menu_item_add( menu_w, _("DiscV"), on_vis_discv_activate, NULL );
/* Note: don't forget to remove the "-1" three lines up */
#endif
	gui_radio_menu_item_add( menu_w, _("MapV"), on_vis_mapv_activate, NULL );
	gui_radio_menu_item_add( menu_w, _("TreeV"), on_vis_treev_activate, NULL );

	/* Color menu */
	menu_w = gui_menu_add( menu_bar_w, _("Colors") );
	/* Color menu items */
	gui_radio_menu_begin( 0 );
	menu_item_w = gui_radio_menu_item_add( menu_w, _("By node type"), on_color_by_nodetype_activate, NULL );
	G_LIST_APPEND(sw_widget_list, menu_item_w);
	color_by_nodetype_rmenu_item_w = menu_item_w;
	menu_item_w = gui_radio_menu_item_add( menu_w, _("By timestamp"), on_color_by_timestamp_activate, NULL );
	G_LIST_APPEND(sw_widget_list, menu_item_w);
	color_by_timestamp_rmenu_item_w = menu_item_w;
	menu_item_w = gui_radio_menu_item_add( menu_w, _("By wildcards"), on_color_by_wildcards_activate, NULL );
	G_LIST_APPEND(sw_widget_list, menu_item_w);
	color_by_wpattern_rmenu_item_w = menu_item_w;
	gui_separator_add( menu_w );
	gui_menu_item_add( menu_w, _("Setup..."), on_color_setup_activate, NULL );

#ifdef DEBUG
	/* Debug menu */
	menu_w = gui_menu_add( menu_bar_w, "Debug" );
	/* Debug menu items */
	gui_menu_item_add( menu_w, "Memory totals", debug_show_mem_totals, NULL );
	gui_menu_item_add( menu_w, "Memory summary", debug_show_mem_summary, NULL );
	gui_menu_item_add( menu_w, "Memory stats", debug_show_mem_stats, NULL );
	gui_separator_add( menu_w );
	gui_menu_item_add( menu_w, "GLib memory profile", g_mem_profile, NULL );
	gui_menu_item_add( menu_w, "GMemChunk info", g_mem_chunk_info, NULL );
#endif

	/* Help menu (right-justified) */
	menu_w = gui_menu_add( menu_bar_w, _("Help") );
	gtk_menu_item_right_justify( GTK_MENU_ITEM(GTK_MENU(menu_w)->parent_menu_item) );
	/* Help menu items */
	gui_menu_item_add( menu_w, _("Contents..."), on_help_contents_activate, NULL );
	gui_separator_add( menu_w );
	gui_menu_item_add( menu_w, _("About fsv..."), on_help_about_fsv_activate, NULL );

	/* Done with the menu bar */

	/* Main horizontal paned widget */
	hpaned_w = gui_hpaned_add( main_vbox_w, window_width / 5 );

	/* Vertical box for everything in the left pane */
	left_vbox_w = gtk_vbox_new( FALSE, 0 );
	gtk_paned_add1( GTK_PANED(hpaned_w), left_vbox_w );
	gtk_widget_show( left_vbox_w );

	/* Horizontal box for toolbar buttons */
	hbox_w = gui_hbox_add( left_vbox_w, 2 );

	/* "back" button */
	button_w = gui_button_add( hbox_w, NULL, on_back_button_clicked, NULL );
	gui_pixmap_xpm_add( button_w, back_xpm );
	G_LIST_APPEND(sw_widget_list, button_w);
	/* "cd /" button */
	button_w = gui_button_add( hbox_w, NULL, on_cd_root_button_clicked, NULL );
	gui_pixmap_xpm_add( button_w, cd_root_xpm );
	G_LIST_APPEND(sw_widget_list, button_w);
	/* "cd .." button */
	button_w = gui_button_add( hbox_w, NULL, on_cd_up_button_clicked, NULL );
	gui_pixmap_xpm_add( button_w, cd_up_xpm );
	G_LIST_APPEND(sw_widget_list, button_w);
	/* "bird's-eye view" toggle button */
	button_w = gui_toggle_button_add( hbox_w, NULL, FALSE, on_birdseye_view_togglebutton_toggled, NULL );
	gui_pixmap_xpm_add( button_w, birdseye_view_xpm );
	G_LIST_APPEND(sw_widget_list, button_w);
	birdseye_view_tbutton_w = button_w;

	/* Frame to encase the directory tree / file list */
	frame_w = gui_frame_add( left_vbox_w, NULL );

	/* Vertical paned widget for directory tree / file list */
	vpaned_w = gui_vpaned_add( frame_w, window_height / 3 );

	/* Directory tree goes in top pane */
	dir_ctree_w = gui_ctree_add( NULL );
	gtk_paned_add1( GTK_PANED(vpaned_w), dir_ctree_w->parent );
	gtk_widget_show( dir_ctree_w->parent );

	/* File list goes in bottom pane */
	file_clist_w = gui_clist_add( NULL, 3, NULL );
	gtk_paned_add2( GTK_PANED(vpaned_w), file_clist_w->parent );
	gtk_widget_show( file_clist_w->parent );

	/* Left statusbar */
	left_statusbar_w = gui_statusbar_add( left_vbox_w );

	/* Vertical box for everything in the right pane */
	right_vbox_w = gtk_vbox_new( FALSE, 0 );
	gtk_paned_add2( GTK_PANED(hpaned_w), right_vbox_w );
	gtk_widget_show( right_vbox_w );

	/* Horizontal box for viewport and y-scrollbar */
	hbox_w = gui_hbox_add( right_vbox_w, 0 );
	gui_widget_packing( hbox_w, EXPAND, FILL, AT_START );

	/* Main viewport (OpenGL area widget) */
	gl_area_w = gui_gl_area_add( hbox_w );
	gtk_signal_connect( GTK_OBJECT(gl_area_w), "event", GTK_SIGNAL_FUNC(viewport_cb), NULL );

	/* y-scrollbar */
	y_scrollbar_w = gui_vscrollbar_add( hbox_w, NULL );
	G_LIST_APPEND(sw_widget_list, y_scrollbar_w);
	/* x-scrollbar */
	x_scrollbar_w = gui_hscrollbar_add( right_vbox_w, NULL );
	G_LIST_APPEND(sw_widget_list, x_scrollbar_w);

	/* Right statusbar */
	right_statusbar_w = gui_statusbar_add( right_vbox_w );

	/* Bind program icon to main window */
	gui_window_icon_xpm( main_window_w, fsv_icon_xpm );

	/* Attach keybindings */
	gui_keybind( main_window_w, NULL );

	/* Send out the widgets to their respective modules */
	dialog_pass_main_window_widget( main_window_w );
	dirtree_pass_widget( dir_ctree_w );
	filelist_pass_widget( file_clist_w );
	camera_pass_scrollbar_widgets( x_scrollbar_w, y_scrollbar_w );

	/* Showtime! */
	gtk_widget_show( main_window_w );
}


/* This enables/disables the switchable widgets */
void
window_set_access( boolean enabled )
{
	GtkWidget *widget;
	GList *llink;

	llink = sw_widget_list;
	while (llink != NULL) {
		widget = (GtkWidget *)llink->data;

		gtk_widget_set_sensitive( widget, enabled );

		llink = llink->next;
	}
}


/* Resets the Color radio menu to the given mode */
void
window_set_color_mode( ColorMode mode )
{
	GtkWidget *rmenu_item_w;
	GtkSignalFunc handler;

	switch (mode) {
		case COLOR_BY_NODETYPE:
		rmenu_item_w = color_by_nodetype_rmenu_item_w;
		handler = GTK_SIGNAL_FUNC(on_color_by_nodetype_activate);
		break;

		case COLOR_BY_TIMESTAMP:
		rmenu_item_w = color_by_timestamp_rmenu_item_w;
		handler = GTK_SIGNAL_FUNC(on_color_by_timestamp_activate);
		break;

		case COLOR_BY_WPATTERN:
		rmenu_item_w = color_by_wpattern_rmenu_item_w;
		handler = GTK_SIGNAL_FUNC(on_color_by_wildcards_activate);
		break;

		SWITCH_FAIL
	}

	gtk_signal_handler_block_by_func( GTK_OBJECT(rmenu_item_w), handler, NULL );
	gtk_check_menu_item_set_active( GTK_CHECK_MENU_ITEM(rmenu_item_w), TRUE );
	gtk_signal_handler_unblock_by_func( GTK_OBJECT(rmenu_item_w), handler, NULL );
}


/* Pops out the bird's-eye view toggle button
 * Note: This should only be called from camera.c, as the bird's-eye-view
 * mode flag (local to that module) must be updated in tandem */
void
window_birdseye_view_off( void )
{
	gtk_signal_handler_block_by_func( GTK_OBJECT(birdseye_view_tbutton_w), GTK_SIGNAL_FUNC(on_birdseye_view_togglebutton_toggled), NULL );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(birdseye_view_tbutton_w), FALSE );
	gtk_signal_handler_unblock_by_func( GTK_OBJECT(birdseye_view_tbutton_w), GTK_SIGNAL_FUNC(on_birdseye_view_togglebutton_toggled), NULL );
}


/* Displays a message in one of the statusbars */
void
window_statusbar( StatusBarID sb_id, const char *message )
{
	switch (sb_id) {
		case SB_LEFT:
		gui_statusbar_message( left_statusbar_w, message );
		break;

		case SB_RIGHT:
		gui_statusbar_message( right_statusbar_w, message );
		break;

		SWITCH_FAIL
	}
}


/* end window.c */
