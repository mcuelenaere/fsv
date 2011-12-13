/* dialog.c */

/* Dialog windows */

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
#include "dialog.h"

#include <time.h>
#include <unistd.h>
#include <gtk/gtk.h>

#include "animation.h"
#include "camera.h"
#include "colexp.h"
#include "color.h"
#include "dirtree.h" /* dirtree_entry_expanded( ) */
#include "filelist.h" /* dir_contents_list_add( ) */
#include "fsv.h"
#include "gui.h"
#include "window.h"

/* OK/Cancel button XPM's */
#include "xmaps/button_ok.xpm"
#include "xmaps/button_cancel.xpm"


/* Main window widget */
static GtkWidget *main_window_w;


/* Correspondence from window_init( ) */
void
dialog_pass_main_window_widget( GtkWidget *window_w )
{
	main_window_w = window_w;
}


/* Callback to close a dialog window */
static void
close_cb( GtkWidget *unused, GtkWidget *window_w )
{
	gtk_widget_destroy( window_w );
}


/* End callback to allow time-bombed transient dialogs */
static void
transient_end_cb( Morph *morph )
{
	GtkWidget *window_w;

	window_w = (GtkWidget *)morph->data;
	gtk_widget_destroy( window_w );

	/* Restore normal mouse cursor */
	gui_cursor( main_window_w, -1 );
}


/**** File -> Change root... ****/

/* Callback for the OK button */
static void
change_root_cb( const char *dir )
{
	if (globals.fsv_mode != FSV_SPLASH)
		fsv_load( dir );
}


void
dialog_change_root( void )
{
	GtkWidget *filesel_window_w;
	const char *root_name;
	char *dir;

	/* Build initial directory name (with trailing slash) */
	root_name = node_absname( root_dnode );
	dir = NEW_ARRAY(char, strlen( root_name ) + 2);
	strcpy( dir, root_name );
	strcat( dir, "/" );

	/* On networked filesystems, the file selection window can be
	 * sloooow in coming up (as each directory component in the default
	 * location has to be stat( )'ed-- takes >10 sec on MIT AFS!) */
	gui_cursor( main_window_w, GDK_WATCH );
	gui_update( );

	filesel_window_w = gui_filesel_window( _("Change Root Directory"), dir, change_root_cb, NULL );
	xfree( dir );

	gui_cursor( main_window_w, -1 );
	gui_update( );

	gui_window_modalize( filesel_window_w, main_window_w );
	gtk_file_selection_hide_fileop_buttons( GTK_FILE_SELECTION(filesel_window_w) );
	/* Disable filesel's file list to make it a directory chooser */
	gtk_widget_set_sensitive( GTK_FILE_SELECTION(filesel_window_w)->file_list, FALSE );

	gtk_widget_show( filesel_window_w );
}


/**** Colors -> Setup... ****/

/* Types of rows in the wildcard pattern list
 * (for "row_type" field in struct WPListRowData) */
enum {
	WPLIST_HEADER_ROW,
	WPLIST_WPATTERN_ROW,
	WPLIST_NEW_WPATTERN_ROW,
	WPLIST_DEFAULT_HEADER_ROW,
	WPLIST_DEFAULT_ROW
};

/* Data associated with each row of the wildcard pattern list */
struct WPListRowData {
	int row_type;
	struct WPatternGroup *wpgroup;
	char *wpattern;
	GtkStyle *style;
};

static struct ColorSetupDialog {
	/* Scratch copy of color configuration */
	struct ColorConfig color_config;

	/* Notebook widget (each page dedicated to a color mode) */
	GtkWidget *notebook_w;

	/* Node type configuration page */
	/* (doesn't have any widgets we need to keep) */

	/* Date/time configuration page */
	struct {
		/* Date edit widgets */
		GtkWidget *old_dateedit_w;
		GtkWidget *new_dateedit_w;

		/* Menu items in timestamp option menu */
		GtkWidget *access_omenu_item_w;
		GtkWidget *modify_omenu_item_w;
		GtkWidget *attrib_omenu_item_w;

		/* Spectrum preview widget */
		GtkWidget *spectrum_preview_w;

		/* Menu items in spectrum type option menu */
		GtkWidget *rainbow_omenu_item_w;
		GtkWidget *heat_omenu_item_w;
		GtkWidget *gradient_omenu_item_w;

		/* Color pickers for interpolated spectrum setup */
		GtkWidget *old_colorpicker_w;
		GtkWidget *new_colorpicker_w;
	} time;

	/* Wildcard pattern configuration page */
	struct {
		/* Wildcard pattern list widget */
		GtkWidget *clist_w;

		/* Flag: TRUE when a row in the list is being dragged */
		boolean row_is_being_dragged;

		/* Action buttons */
		GtkWidget *new_color_button_w;
		GtkWidget *edit_pattern_button_w;
		GtkWidget *delete_button_w;
	} wpattern;
} csdialog;


/* Forward declarations */
static void csdialog_wpattern_clist_row_move_cb( GtkWidget *clist_w, int source_row, int dest_row );


/* Callback for the node type color pickers */
static void
csdialog_node_type_color_picker_cb( RGBcolor *picked_color, RGBcolor *node_type_color )
{
	/* node_type_color points to the appropriate member of
	 * csdialog.color_config.by_nodetype.colors[] */
	node_type_color->r = picked_color->r;
	node_type_color->g = picked_color->g;
	node_type_color->b = picked_color->b;
}


/* Callback for the date edit widgets on the "By date/time" page */
static void
csdialog_time_edit_cb( GtkWidget *dateedit_w )
{
	time_t old_time, new_time;
	time_t cur_time;

	old_time = gui_dateedit_get_time( csdialog.time.old_dateedit_w );
	new_time = gui_dateedit_get_time( csdialog.time.new_dateedit_w );
	cur_time = time( NULL );

	/* Check that neither time is in the future */
	if (difftime( cur_time, new_time ) < 0.0)
		new_time = cur_time;
	if (difftime( cur_time, old_time ) < 0.0)
		old_time = cur_time;

	/* Check that old time is at least one minute before new time */
	if (difftime( new_time, old_time ) < 60.0) {
		if (dateedit_w == csdialog.time.old_dateedit_w)
			new_time = old_time + (time_t)60;
		else if (dateedit_w == csdialog.time.new_dateedit_w)
			old_time = new_time - (time_t)60;
		else {
			g_assert_not_reached( );
			return;
		}
	}

	/* Reset old and new times */
	gtk_signal_handler_block_by_func( GTK_OBJECT(csdialog.time.old_dateedit_w), csdialog_time_edit_cb, NULL );
	gtk_signal_handler_block_by_func( GTK_OBJECT(csdialog.time.new_dateedit_w), csdialog_time_edit_cb, NULL );
	gui_dateedit_set_time( csdialog.time.old_dateedit_w, old_time );
	gui_dateedit_set_time( csdialog.time.new_dateedit_w, new_time );
	gtk_signal_handler_unblock_by_func( GTK_OBJECT(csdialog.time.old_dateedit_w), csdialog_time_edit_cb, NULL );
	gtk_signal_handler_unblock_by_func( GTK_OBJECT(csdialog.time.new_dateedit_w), csdialog_time_edit_cb, NULL );

	csdialog.color_config.by_timestamp.old_time = old_time;
	csdialog.color_config.by_timestamp.new_time = new_time;
}


/* Callback for the "Color by:" timestamp option menu */
static void
csdialog_time_timestamp_option_menu_cb( GtkWidget *omenu_item_w )
{
	TimeStampType type;

	if (omenu_item_w == csdialog.time.access_omenu_item_w)
		type = TIMESTAMP_ACCESS;
	else if (omenu_item_w == csdialog.time.modify_omenu_item_w)
		type = TIMESTAMP_MODIFY;
	else if (omenu_item_w == csdialog.time.attrib_omenu_item_w)
		type = TIMESTAMP_ATTRIB;
	else {
		g_assert_not_reached( );
		return;
	}

	csdialog.color_config.by_timestamp.timestamp_type = type;
}


/* This is the spectrum function used to paint the preview widget */
static RGBcolor
csdialog_time_spectrum_func( double x )
{
	RGBcolor *boundary_colors[2];
	void *data = NULL;

	if (csdialog.color_config.by_timestamp.spectrum_type == SPECTRUM_GRADIENT) {
		boundary_colors[0] = &csdialog.color_config.by_timestamp.old_color;
		boundary_colors[1] = &csdialog.color_config.by_timestamp.new_color;
		data = boundary_colors;
	}

	return color_spectrum_color( csdialog.color_config.by_timestamp.spectrum_type, x, data );
}


/* Helper function for spectrum_option_menu_cb( ). This enables or
 * disables the color picker buttons as necessary */
static void
csdialog_time_color_picker_set_access( boolean enabled )
{
	RGBcolor disabled_color;
	RGBcolor *color;
	GdkColor *gcolor;

	gtk_widget_set_sensitive( csdialog.time.old_colorpicker_w, enabled );
	gtk_widget_set_sensitive( csdialog.time.new_colorpicker_w, enabled );

	/* Change the color pickers' colors, as simply enabling/disabling
	 * them isn't enough to make the state change obvious */
	if (enabled) {
		color = &csdialog.color_config.by_timestamp.old_color;
		gui_colorpicker_set_color( csdialog.time.old_colorpicker_w, color );
		color = &csdialog.color_config.by_timestamp.new_color;
		gui_colorpicker_set_color( csdialog.time.new_colorpicker_w, color );
	}
	else {
		gcolor = &csdialog.time.old_colorpicker_w->style->bg[GTK_STATE_NORMAL];
		disabled_color.r = (float)gcolor->red / 65535.0;
		disabled_color.g = (float)gcolor->green / 65535.0;
		disabled_color.b = (float)gcolor->blue / 65535.0;
		gui_colorpicker_set_color( csdialog.time.old_colorpicker_w, &disabled_color );
		gui_colorpicker_set_color( csdialog.time.new_colorpicker_w, &disabled_color );
	}
}


/* Callback for the spectrum type option menu */
static void
csdialog_time_spectrum_option_menu_cb( GtkWidget *omenu_item_w )
{
	SpectrumType type;

	if (omenu_item_w == csdialog.time.rainbow_omenu_item_w)
		type = SPECTRUM_RAINBOW;
	else if (omenu_item_w == csdialog.time.heat_omenu_item_w)
		type = SPECTRUM_HEAT;
	else if (omenu_item_w == csdialog.time.gradient_omenu_item_w)
		type = SPECTRUM_GRADIENT;
	else {
		g_assert_not_reached( );
		return;
	}

	/* Set new spectrum type and draw it */
	csdialog.color_config.by_timestamp.spectrum_type = type;
	gui_preview_spectrum( csdialog.time.spectrum_preview_w, csdialog_time_spectrum_func );
	csdialog_time_color_picker_set_access( type == SPECTRUM_GRADIENT );
}


/* Callback for the spectrum's color picker buttons */
static void
csdialog_time_color_picker_cb( RGBcolor *picked_color, RGBcolor *end_color )
{
	/* end_color points to either old_color or new_color in
	 * csdialog.color_config.by_timestamp */
	end_color->r = picked_color->r;
	end_color->g = picked_color->g;
	end_color->b = picked_color->b;

	/* Redraw spectrum */
	gui_preview_spectrum( csdialog.time.spectrum_preview_w, csdialog_time_spectrum_func );
}


/* Helper function for csdialog_wpattern_clist_populate( ). This
 * generates a GtkStyle to permit setting a cell to a solid color */
static GtkStyle *
solid_color_cell_style( GtkWidget *clist_w, RGBcolor *color )
{
	GdkColor gcolor;
	GtkStyle *style;

	gcolor.red = (unsigned short)(65535.0 * color->r);
	gcolor.green = (unsigned short)(65535.0 * color->g);
	gcolor.blue = (unsigned short)(65535.0 * color->b);
	style = gtk_style_copy( clist_w->style );
	style->base[GTK_STATE_NORMAL] = gcolor; /* struct assign */
	style->bg[GTK_STATE_SELECTED] = gcolor; /* struct assign */

        return style;
}


/* Helper function for csdialog_wpattern_clist_populate( ). This adds a
 * new row to the wildcard pattern list at the given index (replacing a
 * row if one is already there) */
static void
wplist_row( int row, struct WPListRowData *row_data )
{
	int r; /* for debugging */
	char *clist_row[2] = { NULL, NULL };

	/* If there is a row at the given index, remove it */
	if (row < GTK_CLIST(csdialog.wpattern.clist_w)->rows)
		gtk_clist_remove( GTK_CLIST(csdialog.wpattern.clist_w), row );

	/* Determine textual content of new row */
	switch (row_data->row_type) {
		case WPLIST_WPATTERN_ROW:
		clist_row[1] = row_data->wpattern;
		break;

		case WPLIST_NEW_WPATTERN_ROW:
		clist_row[1] = _("(New pattern)");
		break;

		case WPLIST_DEFAULT_ROW:
		clist_row[1] = _("(Default color)");
		break;

		default:
		/* Row has no text */
		break;
	}
	r = gtk_clist_insert( GTK_CLIST(csdialog.wpattern.clist_w), row, clist_row );
	g_assert( r == row );

	/* Set row properties appropriately */
	switch (row_data->row_type) {
		case WPLIST_HEADER_ROW:
		case WPLIST_DEFAULT_HEADER_ROW:
		gtk_clist_set_row_style( GTK_CLIST(csdialog.wpattern.clist_w), row, row_data->style );
		gtk_clist_set_selectable( GTK_CLIST(csdialog.wpattern.clist_w), row, FALSE );
		break;

		case WPLIST_WPATTERN_ROW:
		case WPLIST_NEW_WPATTERN_ROW:
		case WPLIST_DEFAULT_ROW:
		gtk_clist_set_cell_style( GTK_CLIST(csdialog.wpattern.clist_w), row, 0, row_data->style );
		break;

		SWITCH_FAIL
	}
	gtk_clist_set_row_data_full( GTK_CLIST(csdialog.wpattern.clist_w), row, row_data, _xfree );
}


/* Updates the wildcard pattern list with state in
 * csdialog.color_config.by_wpattern */
static void
csdialog_wpattern_clist_populate( void )
{
	struct WPatternGroup *wpgroup;
	struct WPListRowData *row_data;
	GtkStyle *style = NULL;
	GList *wpgroup_llink, *wp_llink;
	int row = 0;
	int row_count, i;
	char *wpattern;

	gtk_clist_freeze( GTK_CLIST(csdialog.wpattern.clist_w) );

	/* Iterate through all the wildcard pattern color groups */
	wpgroup_llink = csdialog.color_config.by_wpattern.wpgroup_list;
	while (wpgroup_llink != NULL) {
		wpgroup = (struct WPatternGroup *)wpgroup_llink->data;
		style = solid_color_cell_style( csdialog.wpattern.clist_w, &wpgroup->color );

		/* Add header row */
		row_data = NEW(struct WPListRowData);
		row_data->row_type = WPLIST_HEADER_ROW;
		row_data->wpgroup = wpgroup;
		row_data->wpattern = NULL;
		row_data->style = style;
                wplist_row( row++, row_data );

		/* Iterate through all the patterns in this group */
		wp_llink = wpgroup->wp_list;
		while (wp_llink != NULL) {
			wpattern = (char *)wp_llink->data;

			/* Add wildcard pattern row */
			row_data = NEW(struct WPListRowData);
			row_data->row_type = WPLIST_WPATTERN_ROW;
			row_data->wpgroup = wpgroup;
			row_data->wpattern = wpattern;
			row_data->style = style;
			wplist_row( row++, row_data );

			wp_llink = wp_llink->next;
		}

		/* Add a "New pattern" row for adding new patterns
		 * to this color group */
		row_data = NEW(struct WPListRowData);
		row_data->row_type = WPLIST_NEW_WPATTERN_ROW;
		row_data->wpgroup = wpgroup;
		row_data->wpattern = NULL;
		row_data->style = style;
		wplist_row( row++, row_data );

		wpgroup_llink = wpgroup_llink->next;
	}

	/* Default color */
	style = solid_color_cell_style( csdialog.wpattern.clist_w, &csdialog.color_config.by_wpattern.default_color );

	/* Add default-color header row */
	row_data = NEW(struct WPListRowData);
	row_data->row_type = WPLIST_DEFAULT_HEADER_ROW;
	row_data->wpgroup = NULL;
	row_data->wpattern = NULL;
	row_data->style = style;
	wplist_row( row++, row_data );

	/* Add default-color row */
	row_data = NEW(struct WPListRowData);
	row_data->row_type = WPLIST_DEFAULT_ROW;
	row_data->wpgroup = NULL;
	row_data->wpattern = NULL;
	row_data->style = style;
	wplist_row( row++, row_data );

	/* Remove any leftover rows */
	row_count = GTK_CLIST(csdialog.wpattern.clist_w)->rows;
	if (row_count > row) {
		for (i = 0; i < (row_count - row); i++)
			gtk_clist_remove( GTK_CLIST(csdialog.wpattern.clist_w), row );
	}

	gtk_clist_thaw( GTK_CLIST(csdialog.wpattern.clist_w) );
}


/* Callback for the color selection dialog popped up by clicking on a
 * color bar in the wildcard pattern list */
static void
csdialog_wpattern_color_selection_cb( RGBcolor *selected_color, RGBcolor *wpattern_color )
{
	/* wpattern_color points to the appropriate color record
	 * somewhere inside csdialog.color_config.by_wpattern */
	wpattern_color->r = selected_color->r;
	wpattern_color->g = selected_color->g;
	wpattern_color->b = selected_color->b;

	/* Update the list */
	csdialog_wpattern_clist_populate( );
}


/* Callback for mouse button release in the wildcard pattern list */
static int
csdialog_wpattern_clist_click_cb( GtkWidget *clist_w, GdkEventButton *ev_button )
{
	struct WPListRowData *row_data;
        RGBcolor *color;
	int row, col;
	const char *title;

	/* Ignore button release following a row drag */
	if (csdialog.wpattern.row_is_being_dragged) {
		csdialog.wpattern.row_is_being_dragged = FALSE;
		return FALSE;
	}

	/* Respond only to mouse button 1 (left button) */
	if (ev_button->button != 1)
		return FALSE;

	gtk_clist_get_selection_info( GTK_CLIST(clist_w), ev_button->x, ev_button->y, &row, &col );
	if ((row < 0) || (col < 0))
		return FALSE;

	row_data = (struct WPListRowData *)gtk_clist_get_row_data( GTK_CLIST(clist_w), row );

	switch (row_data->row_type) {
		case WPLIST_WPATTERN_ROW:
		case WPLIST_NEW_WPATTERN_ROW:
		if (col != 0)
			return FALSE;
		case WPLIST_HEADER_ROW:
		title = _("Group Color");
		color = &row_data->wpgroup->color;
		break;

		case WPLIST_DEFAULT_ROW:
		if (col != 0)
			return FALSE;
		case WPLIST_DEFAULT_HEADER_ROW:
		title = _("Default Color");
		color = &csdialog.color_config.by_wpattern.default_color;
		break;

		SWITCH_FAIL
	}

	/* Bring up color selection dialog */
	gui_colorsel_window( title, color, csdialog_wpattern_color_selection_cb, color );

	return FALSE;
}


/* Callback for row selection/unselection in the wildcard pattern list */
static void
csdialog_wpattern_clist_select_unselect_cb( GtkWidget *clist_w, int row, int unused1, GdkEvent *unused2 )
{
	struct WPListRowData *row_data;
	boolean newwp_row;
	boolean defcolor_row;
	boolean empty_wpgroup = FALSE;
	boolean row_selected;
	boolean new_color_allow;
	boolean edit_pattern_allow;
	boolean delete_allow;

	row_data = gtk_clist_get_row_data( GTK_CLIST(clist_w), row );

	/* Set some flags */
	newwp_row = row_data->row_type == WPLIST_NEW_WPATTERN_ROW;
	defcolor_row = (row_data->row_type == WPLIST_DEFAULT_ROW) || (row_data->row_type == WPLIST_DEFAULT_HEADER_ROW);
	if (!defcolor_row)
		empty_wpgroup = row_data->wpgroup->wp_list == NULL;
	row_selected = GTK_CLIST(clist_w)->selection != NULL;

	/* Decide which actions are allowable */
	new_color_allow = !row_selected || !defcolor_row;
	edit_pattern_allow = row_selected && !defcolor_row;
        delete_allow = row_selected && !defcolor_row && (!newwp_row || empty_wpgroup);

	/* Enable/disable the buttons accordingly */
	gtk_widget_set_sensitive( csdialog.wpattern.new_color_button_w, new_color_allow );
	gtk_widget_set_sensitive( csdialog.wpattern.edit_pattern_button_w, edit_pattern_allow );
	gtk_widget_set_sensitive( csdialog.wpattern.delete_button_w, delete_allow );
}


/* Callback for whenever a row is being dragged in the wildcard pattern
 * list area */
static boolean
csdialog_wpattern_clist_drag_cb( GtkWidget *unused1, GdkDragContext *unused2, int unused3, int unused4, unsigned int unused5 )
{
	/* This gets checked in csdialog_wpattern_clist_click_cb( ) */
	csdialog.wpattern.row_is_being_dragged = TRUE;
	return FALSE;
}


/* Helper function for csdialog_wpattern_clist_row_move_cb( ). This
 * reverses the effects of a row drag from source_row to dest_row */
static void
wplist_undo_illegal_row_move( int source_row, int dest_row )
{
	gtk_signal_handler_block_by_func( GTK_OBJECT(csdialog.wpattern.clist_w), GTK_SIGNAL_FUNC(csdialog_wpattern_clist_row_move_cb), NULL );
	gtk_clist_row_move( GTK_CLIST(csdialog.wpattern.clist_w), dest_row, source_row );
	gtk_signal_handler_unblock_by_func( GTK_OBJECT(csdialog.wpattern.clist_w), GTK_SIGNAL_FUNC(csdialog_wpattern_clist_row_move_cb), NULL );
}


/* Callback for moving a row in the wildcard pattern list. (This gets called
 * once the row has been dragged and dropped into its desired position) */
static void
csdialog_wpattern_clist_row_move_cb( GtkWidget *clist_w, int source_row, int dest_row )
{
	struct WPListRowData *source_row_data;
	struct WPListRowData *dest_row_data;

	if (dest_row == (GTK_CLIST(clist_w)->rows - 1)) {
		/* Cannot move a row to the very end of the list
		 * (the default-color row goes there) */
		wplist_undo_illegal_row_move( source_row, dest_row );
		return;
	}

	/* Get relevant row data */
	source_row_data = gtk_clist_get_row_data( GTK_CLIST(clist_w), source_row );
	/* GtkCList is a bit odd about what it calls the "destination"
	 * row, so here we impose the convention that it is the first row
	 * to be pushed down by the source row, when the latter is dropped
	 * into its new position */
	if (dest_row < source_row)
		dest_row_data = gtk_clist_get_row_data( GTK_CLIST(clist_w), dest_row );
	else
		dest_row_data = gtk_clist_get_row_data( GTK_CLIST(clist_w), dest_row + 1 );

	/* Make sure that the row move is legal, and if so, do whatever
	 * it implies we should do */
	switch (source_row_data->row_type) {
		case WPLIST_NEW_WPATTERN_ROW:
		case WPLIST_DEFAULT_HEADER_ROW:
		case WPLIST_DEFAULT_ROW:
		/* None of these types of rows may be moved directly */
		wplist_undo_illegal_row_move( source_row, dest_row );
		return;

		case WPLIST_HEADER_ROW:
		/* A header row (along with the group beneath it) can only
		 * be moved to a position occupied by another header row */
		if ((dest_row_data->row_type != WPLIST_HEADER_ROW) && (dest_row_data->row_type != WPLIST_DEFAULT_HEADER_ROW)) {
			wplist_undo_illegal_row_move( source_row, dest_row );
			return;
		}
		/* Move color group to new position */
#define WPGROUP_LIST csdialog.color_config.by_wpattern.wpgroup_list
		G_LIST_REMOVE(WPGROUP_LIST, source_row_data->wpgroup);
		G_LIST_INSERT_BEFORE(WPGROUP_LIST, dest_row_data->wpgroup, source_row_data->wpgroup);
#undef WPGROUP_LIST
		/* Update list */
		csdialog_wpattern_clist_populate( );
		/* The clist widget still has yet to actually *move* the
		 * dragged row into its new position. But since the list
		 * has just been updated, with the rows already where they
		 * should be, the impending "real" row move must be
		 * cancelled */
		gtk_signal_emit_stop_by_name( GTK_OBJECT(clist_w), "row_move" );
		break;

		case WPLIST_WPATTERN_ROW:
		switch (dest_row_data->row_type) {
			case WPLIST_HEADER_ROW:
			case WPLIST_DEFAULT_HEADER_ROW:
			case WPLIST_DEFAULT_ROW:
			/* A pattern row cannot take the place of any of
			 * these types of rows */
			wplist_undo_illegal_row_move( source_row, dest_row );
                        return;

			case WPLIST_WPATTERN_ROW:
			case WPLIST_NEW_WPATTERN_ROW:
			/* Row move is legal. Update official location of
			 * wildcard pattern */
#define S_SIBLINGS source_row_data->wpgroup->wp_list
#define D_SIBLINGS dest_row_data->wpgroup->wp_list
			G_LIST_REMOVE(S_SIBLINGS, source_row_data->wpattern);
			G_LIST_INSERT_BEFORE(D_SIBLINGS, dest_row_data->wpattern, source_row_data->wpattern);
#undef S_SIBLINGS
#undef D_SIBLINGS
			if (source_row_data->wpgroup != dest_row_data->wpgroup) {
				/* Pattern has just changed group
				 * membership, so some row properties need
				 * to be updated */
				source_row_data->wpgroup = dest_row_data->wpgroup;
				source_row_data->style = dest_row_data->style;
				gtk_clist_set_cell_style( GTK_CLIST(clist_w), source_row, 0, dest_row_data->style );
			}
			break;

			SWITCH_FAIL
		}
		break;

		SWITCH_FAIL
	}
}


/* Callback for the color selection dialog popped up by the "New color"
 * button */
static void
csdialog_wpattern_new_color_selection_cb( RGBcolor *selected_color, struct WPListRowData *row_data )
{
	struct WPatternGroup *wpgroup;
	boolean place_before_existing_group = FALSE;

	/* Create new group */
	wpgroup = NEW(struct WPatternGroup);
        wpgroup->color.r = selected_color->r;
        wpgroup->color.g = selected_color->g;
	wpgroup->color.b = selected_color->b;
	wpgroup->wp_list = NULL;

	/* If a row in an existing group was selected, we add the new group
	 * immediately before the existing group */
        if (row_data != NULL)
		if (row_data->wpgroup != NULL)
			place_before_existing_group = TRUE;

#define WPGROUP_LIST csdialog.color_config.by_wpattern.wpgroup_list
	if (place_before_existing_group)
		G_LIST_INSERT_BEFORE(WPGROUP_LIST, row_data->wpgroup, wpgroup);
	else {
		G_LIST_APPEND(WPGROUP_LIST, wpgroup);
		/* Scroll clist to bottom (to make new group visible) */
		gui_clist_moveto_row( csdialog.wpattern.clist_w, -1, 0.0 );
	}
#undef WPGROUP_LIST

	/* Update the list */
	csdialog_wpattern_clist_populate( );
}


/* Callback for the wildcard pattern edit subdialog */
static void
csdialog_wpattern_edit_cb( const char *input_text, struct WPListRowData *row_data )
{
	GList *l; /* for debugging */
	char *wpattern;

	/* Trim leading/trailing whitespace in input */
	wpattern = xstrstrip( xstrdup( input_text ) );

	if (strlen( wpattern ) == 0) {
		/* Ignore empty input */
		xfree( wpattern );
		return;
	}

	/* Check for duplicate pattern in group */
	/* (This doesn't prevent the possibility of duplicate patterns
	 * across groups, but hey, this is better than nothing) */
	if (g_list_find_custom( row_data->wpgroup->wp_list, wpattern, (GCompareFunc)strcmp ) != NULL) {
		/* Yep, there's a duplicate */
		xfree( wpattern );
		return;
	}

	switch (row_data->row_type) {
		case WPLIST_WPATTERN_ROW:
		/* Update existing pattern */
		l = g_list_replace( row_data->wpgroup->wp_list, row_data->wpattern, wpattern );
		g_assert( l != NULL );
		xfree( row_data->wpattern );
		break;

		case WPLIST_NEW_WPATTERN_ROW:
		/* Add new pattern */
		G_LIST_APPEND(row_data->wpgroup->wp_list, wpattern);
		break;

		SWITCH_FAIL
	}

	/* Update the list */
	csdialog_wpattern_clist_populate( );
}


/* Callback for the buttons to the right of the wildcard pattern list */
static void
csdialog_wpattern_button_cb( GtkWidget *button_w )
{
	struct WPListRowData *row_data = NULL;
	RGBcolor default_new_color = { 0.0, 0.0, 0.75 }; /* I like blue */
        RGBcolor *color;
	int row = -1;
        const char *title = NULL;

	if (GTK_CLIST(csdialog.wpattern.clist_w)->selection != NULL) {
		/* Get data for currently selected row */
		row = GPOINTER_TO_INT(GTK_CLIST(csdialog.wpattern.clist_w)->selection->data);
		row_data = gtk_clist_get_row_data( GTK_CLIST(csdialog.wpattern.clist_w), row );
		g_assert( row_data != NULL );
	}

	if (button_w == csdialog.wpattern.new_color_button_w) {
                /* Bring up color selection dialog for new color group */
		title = _("New Color Group");
		if (row_data == NULL)
			color = &default_new_color;
		else
			color = &row_data->wpgroup->color;
		gui_colorsel_window( title, color, csdialog_wpattern_new_color_selection_cb, row_data );
	}
	else if (button_w == csdialog.wpattern.edit_pattern_button_w) {
		/* Bring up pattern edit subdialog */
		g_assert( row_data != NULL );
		switch (row_data->row_type) {
			case WPLIST_WPATTERN_ROW:
			title = _("Edit Wildcard Pattern");
			break;

			case WPLIST_NEW_WPATTERN_ROW:
			title = _("New Wildcard Pattern");
			break;

			SWITCH_FAIL
		}
		gui_entry_window( title, row_data->wpattern, csdialog_wpattern_edit_cb, row_data );
	}
	else if (button_w == csdialog.wpattern.delete_button_w) {
		/* Delete a pattern or color group */
		g_assert( row_data != NULL );
		switch (row_data->row_type) {
			case WPLIST_WPATTERN_ROW:
			/* Delete pattern */
			G_LIST_REMOVE(row_data->wpgroup->wp_list, row_data->wpattern);
			xfree( row_data->wpattern );
			/* Remove corresponding row */
			gtk_clist_remove( GTK_CLIST(csdialog.wpattern.clist_w), row );
			/* As a courtesy to the user, leave the next
			 * row selected */
			gtk_clist_select_row( GTK_CLIST(csdialog.wpattern.clist_w), row, 1 );
			break;

			case WPLIST_NEW_WPATTERN_ROW:
			/* Delete color group ONLY if group is empty */
			if (row_data->wpgroup->wp_list != NULL)
				return;
			G_LIST_REMOVE(csdialog.color_config.by_wpattern.wpgroup_list, row_data->wpgroup);
			xfree( row_data->wpgroup );
			/* Remove corresponding rows */
			gtk_clist_remove( GTK_CLIST(csdialog.wpattern.clist_w), row );
			g_assert( row >= 1 );
			gtk_clist_remove( GTK_CLIST(csdialog.wpattern.clist_w), row - 1 );
			break;

			SWITCH_FAIL
		}
	}
}


/* Callback for the "OK" button */
static void
csdialog_ok_button_cb( GtkWidget *unused, GtkWidget *window_w )
{
	ColorMode mode;

	/* Commit new color configuration, and set color mode to match
	 * current notebook page */
	mode = (ColorMode)gtk_notebook_get_current_page( GTK_NOTEBOOK(csdialog.notebook_w) );
	color_set_config( &csdialog.color_config, mode );

        /* Update option menu to reflect current color mode */
	window_set_color_mode( mode );

	gtk_widget_destroy( window_w );
}


/* Callback for dialog window destruction */
static void
csdialog_destroy_cb( GtkObject *unused )
{
	/* We'd leak memory like crazy if we didn't do this */
	color_config_destroy( &csdialog.color_config );
}


void
dialog_color_setup( void )
{
	GtkWidget *window_w;
	GtkWidget *main_vbox_w;
	GtkWidget *vbox_w;
	GtkWidget *vbox2_w;
	GtkWidget *hbox_w;
	GtkWidget *hbox2_w;
	GtkWidget *frame_w;
	GtkWidget *table_w;
	GtkWidget *label_w;
	GtkWidget *optmenu_w;
        RGBcolor *color;
        ColorMode color_mode;
	int i;
	char *clist_col_titles[2];
	char strbuf[256];

	clist_col_titles[0] = _("Color ");
	clist_col_titles[1] = _("Wildcard pattern");

	window_w = gui_dialog_window( _("Color Setup"), NULL );
	gui_window_modalize( window_w, main_window_w );
	main_vbox_w = gui_vbox_add( window_w, 5 );
	csdialog.notebook_w = gui_notebook_add( main_vbox_w );

	/* Get current color mode/configuration */
        color_mode = color_get_mode( );
	color_get_config( &csdialog.color_config );


	/**** "By node type" page ****/

	hbox_w = gui_hbox_add( NULL, 7 );
	gui_box_set_packing( hbox_w, EXPAND, NO_FILL, AT_START );
	gui_notebook_page_add( csdialog.notebook_w, _("By node type"), hbox_w );

	vbox_w = gui_vbox_add( hbox_w, 10 );
	gtk_container_set_border_width( GTK_CONTAINER(vbox_w), 3 );
	gui_box_set_packing( vbox_w, EXPAND, NO_FILL, AT_START );
	vbox2_w = gui_vbox_add( hbox_w, 10 );
	gtk_container_set_border_width( GTK_CONTAINER(vbox2_w), 3 );
	gui_box_set_packing( vbox2_w, EXPAND, NO_FILL, AT_START );

	/* Create two-column listing of node type colors */
	for (i = 1; i < NUM_NODE_TYPES; i++) {
		if ((i % 2) == 1)
			frame_w = gui_frame_add( vbox_w, NULL );
		else
			frame_w = gui_frame_add( vbox2_w, NULL );
		gtk_frame_set_shadow_type( GTK_FRAME(frame_w), GTK_SHADOW_ETCHED_OUT );
		hbox_w = gui_hbox_add( frame_w, 10 );

		/* Color picker button */
		sprintf( strbuf, _("Color: %s"), node_type_names[i] );
		color = &csdialog.color_config.by_nodetype.colors[i];
		gui_colorpicker_add( hbox_w, color, strbuf, csdialog_node_type_color_picker_cb, color );

		/* Node type icon */
		gui_pixmap_xpm_add( hbox_w, node_type_xpms[i] );
		/* Node type label */
		gui_label_add( hbox_w, _(node_type_names[i]) );
	}


	/**** "By date/time" page ****/

	vbox_w = gui_vbox_add( NULL, 10 );
	gui_notebook_page_add( csdialog.notebook_w, _("By date/time"), vbox_w );

	/* Arrange the top part using a table */
	hbox_w = gui_hbox_add( vbox_w, 0 );
	table_w = gui_table_add( hbox_w, 3, 2, FALSE, 4 );
	gui_widget_packing( table_w, EXPAND, NO_FILL, AT_START );
        /* Old label */
	hbox2_w = gui_hbox_add( NULL, 0 );
	gui_table_attach( table_w, hbox2_w, 0, 1, 0, 1 );
	label_w = gui_label_add( hbox2_w, _("Oldest:") );
	gui_widget_packing( label_w, NO_EXPAND, NO_FILL, AT_END );
	/* New label */
	hbox2_w = gui_hbox_add( NULL, 0 );
	gui_table_attach( table_w, hbox2_w, 0, 1, 1, 2 );
	label_w = gui_label_add( hbox2_w, _("Newest:") );
	gui_widget_packing( label_w, NO_EXPAND, NO_FILL, AT_END );
	/* Timestamp selection label */
	hbox2_w = gui_hbox_add( NULL, 0 );
	gui_table_attach( table_w, hbox2_w, 0, 1, 2, 3 );
	label_w = gui_label_add( hbox2_w, _("Color by:") );
	gui_widget_packing( label_w, NO_EXPAND, NO_FILL, AT_END );
	/* Old date edit widget */
	csdialog.time.old_dateedit_w = gui_dateedit_add( NULL, csdialog.color_config.by_timestamp.old_time, csdialog_time_edit_cb, NULL );
        gui_table_attach( table_w, csdialog.time.old_dateedit_w, 1, 2, 0, 1 );
	/* New date edit widget */
	csdialog.time.new_dateedit_w = gui_dateedit_add( NULL, csdialog.color_config.by_timestamp.new_time, csdialog_time_edit_cb, NULL );
        gui_table_attach( table_w, csdialog.time.new_dateedit_w, 1, 2, 1, 2 );
	/* Timestamp selection option menu */
	csdialog.time.access_omenu_item_w = gui_option_menu_item( _("Time of last access"), csdialog_time_timestamp_option_menu_cb, NULL );
	csdialog.time.modify_omenu_item_w = gui_option_menu_item( _("Time of last modification"), csdialog_time_timestamp_option_menu_cb, NULL );
	csdialog.time.attrib_omenu_item_w = gui_option_menu_item( _("Time of last attribute change"), csdialog_time_timestamp_option_menu_cb, NULL );
	optmenu_w = gui_option_menu_add( NULL, csdialog.color_config.by_timestamp.timestamp_type );
        gui_table_attach( table_w, optmenu_w, 1, 2, 2, 3 );

	/* Time spectrum */
	frame_w = gui_frame_add( vbox_w, NULL );
	gtk_frame_set_shadow_type( GTK_FRAME(frame_w), GTK_SHADOW_IN );
	csdialog.time.spectrum_preview_w = gui_preview_add( frame_w );
	gui_preview_spectrum( csdialog.time.spectrum_preview_w, csdialog_time_spectrum_func );

	/* Horizontal box for spectrum color pickers and menu */
	hbox_w = gui_hbox_add( vbox_w, 0 );

	/* Old end */
        color = &csdialog.color_config.by_timestamp.old_color;
	csdialog.time.old_colorpicker_w = gui_colorpicker_add( hbox_w, color, _("Older Color"), csdialog_time_color_picker_cb, color );
	gui_hbox_add( hbox_w, 5 );
	gui_label_add( hbox_w, _("Older") );

	/* Spectrum type selection */
	csdialog.time.rainbow_omenu_item_w = gui_option_menu_item( _("Rainbow"), csdialog_time_spectrum_option_menu_cb, NULL );
	csdialog.time.heat_omenu_item_w = gui_option_menu_item( _("Heat"), csdialog_time_spectrum_option_menu_cb, NULL );
	csdialog.time.gradient_omenu_item_w = gui_option_menu_item( _("Gradient"), csdialog_time_spectrum_option_menu_cb, NULL );
	optmenu_w = gui_option_menu_add( hbox_w, csdialog.color_config.by_timestamp.spectrum_type );
	gui_widget_packing( optmenu_w, EXPAND, NO_FILL, AT_START );

	/* New end */
	gui_box_set_packing( hbox_w, NO_EXPAND, NO_FILL, AT_END );
        color = &csdialog.color_config.by_timestamp.new_color;
	csdialog.time.new_colorpicker_w = gui_colorpicker_add( hbox_w, color, _("Newer Color"), csdialog_time_color_picker_cb, color );
	gui_hbox_add( hbox_w, 5 );
	gui_label_add( hbox_w, _("Newer") );

	/* Color pickers are accessible only for gradient spectrum */
	csdialog_time_color_picker_set_access( csdialog.color_config.by_timestamp.spectrum_type == SPECTRUM_GRADIENT );


	/**** "By wildcards" page ****/

	hbox_w = gui_hbox_add( NULL, 10 );
	gui_notebook_page_add( csdialog.notebook_w, _("By wildcards"), hbox_w );

	/* List of colors and associated wildcard patterns */
	csdialog.wpattern.clist_w = gui_clist_add( hbox_w, 2, clist_col_titles );
	gtk_clist_set_reorderable( GTK_CLIST(csdialog.wpattern.clist_w), TRUE );
	gtk_signal_connect( GTK_OBJECT(csdialog.wpattern.clist_w), "button_release_event", GTK_SIGNAL_FUNC(csdialog_wpattern_clist_click_cb), NULL );
	gtk_signal_connect( GTK_OBJECT(csdialog.wpattern.clist_w), "select_row", GTK_SIGNAL_FUNC(csdialog_wpattern_clist_select_unselect_cb), NULL );
	gtk_signal_connect( GTK_OBJECT(csdialog.wpattern.clist_w), "unselect_row", GTK_SIGNAL_FUNC(csdialog_wpattern_clist_select_unselect_cb), NULL );
	gtk_signal_connect( GTK_OBJECT(csdialog.wpattern.clist_w), "drag_motion", GTK_SIGNAL_FUNC(csdialog_wpattern_clist_drag_cb), NULL );
	gtk_signal_connect( GTK_OBJECT(csdialog.wpattern.clist_w), "row_move", GTK_SIGNAL_FUNC(csdialog_wpattern_clist_row_move_cb), NULL );

        /* Action buttons */
	vbox_w = gui_vbox_add( hbox_w, 0 );
	csdialog.wpattern.new_color_button_w = gui_button_add( vbox_w, _("New color"), csdialog_wpattern_button_cb, NULL );
	gui_separator_add( vbox_w );
	csdialog.wpattern.edit_pattern_button_w = gui_button_add( vbox_w, _("Edit pattern"), csdialog_wpattern_button_cb, NULL );
	gtk_widget_set_sensitive( csdialog.wpattern.edit_pattern_button_w, FALSE );
        gui_separator_add( vbox_w );
	csdialog.wpattern.delete_button_w = gui_button_add( vbox_w, _("Delete"), csdialog_wpattern_button_cb, NULL );
	gtk_widget_set_sensitive( csdialog.wpattern.delete_button_w, FALSE );

	csdialog.wpattern.row_is_being_dragged = FALSE;
	csdialog_wpattern_clist_populate( );


	/* Horizontal box for OK and Cancel buttons */
	hbox_w = gui_hbox_add( main_vbox_w, 0 );
	gtk_box_set_homogeneous( GTK_BOX(hbox_w), TRUE );
	gui_box_set_packing( hbox_w, EXPAND, FILL, AT_START );

	/* OK and Cancel buttons */
	gui_button_with_pixmap_xpm_add( hbox_w, button_ok_xpm, _("OK"), csdialog_ok_button_cb, window_w );
	gui_hbox_add( hbox_w, 0 ); /* spacer */
	gui_button_with_pixmap_xpm_add( hbox_w, button_cancel_xpm, _("Cancel"), close_cb, window_w );

	/* Set page to current color mode */
	gtk_notebook_set_page( GTK_NOTEBOOK(csdialog.notebook_w), color_mode );

	/* Some cleanup will be required once the window goes away */
	gtk_signal_connect( GTK_OBJECT(window_w), "destroy", GTK_SIGNAL_FUNC(csdialog_destroy_cb), NULL );

	gtk_widget_show( window_w );
}


/* Help -> Contents... */
void
dialog_help( void )
{
	static double t;
	GtkWidget *window_w;
	GtkWidget *frame_w;
	GtkWidget *hbox_w;
	char location[] = "file://localhost" DOCDIR "/fsv.html";
	char cmdbuf[2048];

	/* Browser may take a few seconds to start up... */
	gui_cursor( main_window_w, GDK_WATCH );
	gui_update( );

	/* Create message window to acknowledge action */
	window_w = gui_dialog_window( _("Help"), NULL );
	gtk_container_set_border_width( GTK_CONTAINER(window_w), 5 );
	frame_w = gui_frame_add( window_w, NULL );
	hbox_w = gui_hbox_add( frame_w, 10 );
	gui_label_add( hbox_w, _("Launching help browser . . .") );
	gtk_widget_show( window_w );
	/* and time-bomb it */
	morph_finish( &t );
	t = 0.0;
	morph_full( &t, MORPH_LINEAR, 1.0, 4.0, NULL, transient_end_cb, window_w );

	if (xfork( )) {
		/* Browser startup command */
		sprintf( cmdbuf,
		    "netscape -noraise -remote \"openURL(%s,new-window)\" > /dev/null 2>&1 || "
		    "netscape -no-about-splash -dont-save-geometry-prefs %s > /dev/null 2>&1 || "
		    "xterm -title \"fsv help\" -e lynx %s > /dev/null 2>&1",
		    location, location, location );

		/* Execute command */
		system( cmdbuf );

		/* End subprocess */
		_exit( 0 );
	}
}


/* Callback for the "Look at target node" button on the "Target" page
 * of the Properties dialog for symlinks */
static void
look_at_target_node_cb( GtkWidget *unused, GNode *node )
{
	/* Target node may be buried inside a collapsed tree--
	 * if it is, expand it out into the open */
	if (NODE_IS_DIR(node->parent))
		if (!dirtree_entry_expanded( node->parent ))
			colexp( node->parent, COLEXP_EXPAND_ANY );

	camera_look_at( node );
}


/* The Properties dialog */
static void
dialog_node_properties( GNode *node )
{
	const struct NodeInfo *node_info;
	GtkWidget *window_w;
	GtkWidget *main_vbox_w;
	GtkWidget *notebook_w;
	GtkWidget *vbox_w;
	GtkWidget *table_w;
	GtkWidget *pixmap_w;
	GtkWidget *hbox_w;
	GtkWidget *label_w;
	GtkWidget *separator_w;
	GtkWidget *button_w;
	GtkWidget *vbox2_w;
	GtkWidget *clist_w;
	GtkWidget *entry_w;
	GNode *target_node;
	char strbuf[1024];
	char *proptext;

	/* Get the lowdown on the node. get_node_info( ) may cause some
	 * disk activity, so change the cursor meanwhile (just in case) */
	gui_cursor( main_window_w, GDK_WATCH );
	gui_update( );
	node_info = get_node_info( node );
	gui_cursor( main_window_w, GDK_X_CURSOR );

	window_w = gui_dialog_window( _("Properties"), NULL );
	gui_window_modalize( window_w, main_window_w );
	main_vbox_w = gui_vbox_add( window_w, 5 );
	notebook_w = gui_notebook_add( main_vbox_w );

	/**** "General" page ****/

	vbox_w = gui_vbox_add( NULL, 10 );
	gui_notebook_page_add( notebook_w, _("General"), vbox_w );
	table_w = gui_table_add( vbox_w, 6, 2, FALSE, 0 );

	/* Node type icon */
	hbox_w = gui_hbox_add( NULL, 8 );
	gui_table_attach( table_w, hbox_w, 0, 1, 0, 1 );
	pixmap_w = gui_pixmap_xpm_add( hbox_w, node_type_xpms[NODE_DESC(node)->type] );
	gui_widget_packing( pixmap_w, NO_EXPAND, NO_FILL, AT_END );
	/* Name */
	hbox_w = gui_hbox_add( NULL, 8 );
	label_w = gui_label_add( hbox_w, node_info->name );
	gtk_label_set_justify( GTK_LABEL(label_w), GTK_JUSTIFY_LEFT );
	gui_table_attach( table_w, hbox_w, 1, 2, 0, 1 );

	separator_w = gui_separator_add( NULL );
	gui_table_attach( table_w, separator_w, 0, 2, 1, 2 );

	/* Labels: type, location, size, owner, group */
        strcpy( strbuf, "" );
	strcat( strbuf, _("Type:\n\n") );
	strcat( strbuf, _("Location:\n\n") );
	if (NODE_IS_DIR(node))
		strcat( strbuf, _("Total size:\n\n") );
	else {
		strcat( strbuf, _("Size:\n") );
		strcat( strbuf, _("Allocation:\n\n") );
	}
	strcat( strbuf, _("Owner:\n") );
	strcat( strbuf, _("Group:") );
	hbox_w = gui_hbox_add( NULL, 8 );
	label_w = gui_label_add( hbox_w, strbuf );
	gui_widget_packing( label_w, NO_EXPAND, NO_FILL, AT_END );
	gtk_label_set_justify( GTK_LABEL(label_w), GTK_JUSTIFY_RIGHT );
	gui_table_attach( table_w, hbox_w, 0, 1, 2, 3 );

	proptext = xstrdup( "" );
	/* Type */
	STRRECAT(proptext, _(node_type_names[NODE_DESC(node)->type]));
	STRRECAT(proptext, "\n\n");
	/* Location */
        STRRECAT(proptext, node_info->prefix);
	STRRECAT(proptext, "\n\n");
	if (NODE_IS_DIR(node)) {
		/* Total size */
		sprintf( strbuf, _("%s bytes"), node_info->subtree_size );
		STRRECAT(proptext, strbuf);
		if (DIR_NODE_DESC(node)->subtree.size >= 1024) {
			sprintf( strbuf, " (%s)", node_info->subtree_size_abbr );
			STRRECAT(proptext, strbuf);
		}
	}
	else {
		/* Size */
		sprintf( strbuf, _("%s bytes"), node_info->size );
		STRRECAT(proptext, strbuf);
		if (NODE_DESC(node)->size >= 1024) {
			sprintf( strbuf, " (%s)", node_info->size_abbr );
			STRRECAT(proptext, strbuf);
		}
		STRRECAT(proptext, "\n");
		/* Allocation */
		sprintf( strbuf, _("%s bytes"), node_info->size_alloc );
		STRRECAT(proptext, strbuf);
	}
	STRRECAT(proptext, "\n\n");
	/* Owner (user) */
	sprintf( strbuf, _("%s (uid %u)"), node_info->user_name, NODE_DESC(node)->user_id );
	STRRECAT(proptext, strbuf);
	STRRECAT(proptext, "\n");
	/* Group */
	sprintf( strbuf, _("%s (gid %u)"), node_info->group_name, NODE_DESC(node)->group_id );
	STRRECAT(proptext, strbuf);

	hbox_w = gui_hbox_add( NULL, 8 );
	label_w = gui_label_add( hbox_w, proptext );
	gtk_label_set_justify( GTK_LABEL(label_w), GTK_JUSTIFY_LEFT );
	gui_table_attach( table_w, hbox_w, 1, 2, 2, 3 );

	separator_w = gui_separator_add( NULL );
	gui_table_attach( table_w, separator_w, 0, 2, 3, 4 );

	/* Labels for date/time stamps */
	strcpy( strbuf, "" );
	strcat( strbuf, _("Modified:\n") );
	strcat( strbuf, _("AttribCh:\n") );
	strcat( strbuf, _("Accessed:") );
	hbox_w = gui_hbox_add( NULL, 8 );
	label_w = gui_label_add( hbox_w, strbuf );
	gui_widget_packing( label_w, NO_EXPAND, NO_FILL, AT_END );
	gtk_label_set_justify( GTK_LABEL(label_w), GTK_JUSTIFY_RIGHT );
	gui_table_attach( table_w, hbox_w, 0, 1, 4, 5 );

	/* Date/time stamps */
	strcpy( proptext, "" );
	/* Modified */
	STRRECAT(proptext, node_info->mtime);
	STRRECAT(proptext, "\n");
	/* Attributes changed */
	STRRECAT(proptext, node_info->ctime);
	STRRECAT(proptext, "\n");
	/* Accessed */
	STRRECAT(proptext, node_info->atime);

	hbox_w = gui_hbox_add( NULL, 8 );
	label_w = gui_label_add( hbox_w, proptext );
	gtk_label_set_justify( GTK_LABEL(label_w), GTK_JUSTIFY_LEFT );
	gui_table_attach( table_w, hbox_w, 1, 2, 4, 5 );

	separator_w = gui_separator_add( NULL );
	gui_table_attach( table_w, separator_w, 0, 2, 5, 6 );

	/* Node-type-specific information pages */

	switch (NODE_DESC(node)->type) {

		case NODE_DIRECTORY:
		/**** "Contents" page ****/

		vbox_w = gui_vbox_add( NULL, 10 );
		gui_notebook_page_add( notebook_w, _("Contents"), vbox_w );

		hbox_w = gui_hbox_add( vbox_w, 0 );
		gui_widget_packing( hbox_w, EXPAND, NO_FILL, AT_START );
		vbox2_w = gui_vbox_add( hbox_w, 10 );
		gui_widget_packing( vbox2_w, EXPAND, NO_FILL, AT_START );

		gui_label_add( vbox2_w, _("This directory contains:") );

		/* Directory contents listing */
		clist_w = dir_contents_list( node );
		gtk_box_pack_start( GTK_BOX(vbox2_w), clist_w, FALSE, FALSE, 0 );
		gtk_widget_show( clist_w );

		gui_separator_add( vbox2_w );

		strcpy( proptext, "" );
		/* Total size readout */
		sprintf( strbuf, _("%s bytes"), node_info->subtree_size );
		STRRECAT(proptext, strbuf);
		if (DIR_NODE_DESC(node)->subtree.size >= 1024) {
			sprintf( strbuf, " (%s)", node_info->subtree_size_abbr );
			STRRECAT(proptext, strbuf);
		}
		gui_label_add( vbox2_w, proptext );
                break;


#ifdef HAVE_FILE_COMMAND
		case NODE_REGFILE:
		/**** "File type" page ****/

		vbox_w = gui_vbox_add( NULL, 10 );
		gui_notebook_page_add( notebook_w, _("File type"), vbox_w );

		gui_label_add( vbox_w, _("This file is recognized as:") );

		/* 'file' command output */
		gui_text_area_add( vbox_w, node_info->file_type_desc );
                break;
#endif /* HAVE_FILE_COMMAND */


		case NODE_SYMLINK:
		/**** "Target" page ****/

		vbox_w = gui_vbox_add( NULL, 10 );
		gui_notebook_page_add( notebook_w, _("Target"), vbox_w );

		/* (Relative) name of target */
		gui_label_add( vbox_w, _("This symlink points to:") );
		hbox_w = gui_hbox_add( vbox_w, 0 );
		entry_w = gui_entry_add( hbox_w, node_info->target, NULL, NULL );
		gtk_entry_set_editable( GTK_ENTRY(entry_w), FALSE );

		hbox_w = gui_hbox_add( vbox_w, 0 ); /* spacer */

		/* Absolute name of target */
		gui_label_add( vbox_w, _("Absolute name of target:") );
		hbox_w = gui_hbox_add( vbox_w, 0 );
                if (!strcmp( node_info->target, node_info->abstarget ))
			entry_w = gui_entry_add( hbox_w, _("(same as above)"), NULL, NULL );
                else
			entry_w = gui_entry_add( hbox_w, node_info->abstarget, NULL, NULL );
		gtk_entry_set_editable( GTK_ENTRY(entry_w), FALSE );

		/* This is NULL if target isn't in the filesystem tree */
		target_node = node_named( node_info->abstarget );

		/* The "Look at target node" feature does not work in TreeV
		 * mode if directories have to be expanded to see the
		 * target node, because unbuilt TreeV geometry does not
		 * have a definite location */
		if ((globals.fsv_mode == FSV_TREEV) && (target_node != NULL))
			if (NODE_IS_DIR(target_node->parent))
				if (!dirtree_entry_expanded( target_node->parent ))
					target_node = NULL;

		/* Button to point camera at target node (if present) */
		hbox_w = gui_hbox_add( vbox_w, 10 );
		button_w = gui_button_add( hbox_w, _("Look at target node"), look_at_target_node_cb, target_node );
		gui_widget_packing( button_w, EXPAND, NO_FILL, AT_START );
		gtk_widget_set_sensitive( button_w, target_node != NULL );
		gtk_signal_connect( GTK_OBJECT(button_w), "clicked", GTK_SIGNAL_FUNC(close_cb), window_w );
		break;


		default:
		/* No additional information for this node type */
		break;
	}

	/* Close button */
	button_w = gui_button_add( main_vbox_w, _("Close"), close_cb, window_w );

	xfree( proptext );

	gtk_widget_show( window_w );
}


/**** Context-sensitive right-click menu ****/

/* (I know, it's not a dialog, but where else to put this? :-) */

/* Helper callback for context_menu( ) */
static void
collapse_cb( GtkWidget *unused, GNode *dnode )
{
	colexp( dnode, COLEXP_COLLAPSE_RECURSIVE );
}


/* ditto */
static void
expand_cb( GtkWidget *unused, GNode *dnode )
{
	colexp( dnode, COLEXP_EXPAND );
}


/* ditto */
static void
expand_recursive_cb( GtkWidget *unused, GNode *dnode )
{
	colexp( dnode, COLEXP_EXPAND_RECURSIVE );
}


/* ditto */
static void
look_at_cb( GtkWidget *unused, GNode *node )
{
	camera_look_at( node );
}


/* ditto */
static void
properties_cb( GtkWidget *unused, GNode *node )
{
	dialog_node_properties( node );
}


void
context_menu( GNode *node, GdkEventButton *ev_button )
{
	static GtkWidget *popup_menu_w = NULL;

	/* Recycle previous popup menu */
	if (popup_menu_w != NULL) {
		g_assert( GTK_IS_MENU(popup_menu_w) );
		gtk_widget_destroy( popup_menu_w );
	}

	/* Check for the special case in which the menu has only one item */
	if (!NODE_IS_DIR(node) && (node == globals.current_node)) {
		dialog_node_properties( node );
		return;
	}

	/* Create menu */
	popup_menu_w = gtk_menu_new( );
	if (NODE_IS_DIR(node)) {
		if (dirtree_entry_expanded( node ))
			gui_menu_item_add( popup_menu_w, _("Collapse"), collapse_cb, node );
		else {
			gui_menu_item_add( popup_menu_w, _("Expand"), expand_cb, node );
			if (DIR_NODE_DESC(node)->subtree.counts[NODE_DIRECTORY] > 0)
				gui_menu_item_add( popup_menu_w, _("Expand all"), expand_recursive_cb, node );
		}
	}
	if (node != globals.current_node)
		gui_menu_item_add( popup_menu_w, _("Look at"), look_at_cb, node );
	gui_menu_item_add( popup_menu_w, _("Properties"), properties_cb, node );

	gtk_menu_popup( GTK_MENU(popup_menu_w), NULL, NULL, NULL, NULL, ev_button->button, ev_button->time );
}


/* end dialog.c */
