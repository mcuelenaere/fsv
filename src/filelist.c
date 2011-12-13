/* filelist.c */

/* File list control */

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
#include "filelist.h"

#include <gtk/gtk.h>

#include "about.h"
#include "camera.h"
#include "dialog.h"
#include "dirtree.h"
#include "geometry.h"
#include "gui.h"
#include "window.h"


/* Time for the filelist to scroll to a given entry (in seconds) */
#define FILELIST_SCROLL_TIME 0.5


/* The file list widget */
static GtkWidget *file_clist_w;

/* Directory currently listed */
static GNode *filelist_current_dnode;

/* Mini node type icons */
static Icon node_type_mini_icons[NUM_NODE_TYPES];


/* Loads the mini node type icons (from XPM data) */
static void
filelist_icons_init( void )
{
	GtkStyle *style;
	GdkColor *trans_color;
	GdkWindow *window;
	GdkPixmap *pixmap;
	GdkBitmap *mask;
	int i;

	style = gtk_widget_get_style( file_clist_w );
	trans_color = &style->bg[GTK_STATE_NORMAL];
	gtk_widget_realize( file_clist_w );
	window = file_clist_w->window;

	/* Make mini node type icons */
	for (i = 1; i < NUM_NODE_TYPES; i++) {
		pixmap = gdk_pixmap_create_from_xpm_d( window, &mask, trans_color, node_type_mini_xpms[i] );
		node_type_mini_icons[i].pixmap = pixmap;
		node_type_mini_icons[i].mask = mask;
	}
}


/* Correspondence from window_init( ) */
void
filelist_pass_widget( GtkWidget *clist_w )
{
	file_clist_w = clist_w;
	filelist_icons_init( );
}


/* This makes entries in the file list selectable or unselectable,
 * depending on whether the directory they are in is expanded or not */
void
filelist_reset_access( void )
{
	boolean enabled;

        enabled = dirtree_entry_expanded( filelist_current_dnode );
	gtk_widget_set_sensitive( file_clist_w, enabled );

	/* Extra fluff for interface niceness */
	if (enabled)
		gui_cursor( file_clist_w, -1 );
	else {
		gtk_clist_unselect_all( GTK_CLIST(file_clist_w) );
		gui_cursor( file_clist_w, GDK_X_CURSOR );
	}
}


/* Compare function for sorting nodes alphabetically */
static int
compare_node( GNode *a, GNode *b )
{
	return strcmp( NODE_DESC(a)->name, NODE_DESC(b)->name );
}


/* Displays contents of a directory in the file list */
void
filelist_populate( GNode *dnode )
{
	GNode *node;
	GList *node_list = NULL, *node_llink;
	Icon *icon;
	int row, count = 0;
	char *empty_row[] = { NULL };
	char strbuf[64];

	g_assert( NODE_IS_DIR(dnode) );

        /* Get an alphabetized list of directory's immediate children */
	node = dnode->children;
	while (node != NULL) {
		G_LIST_PREPEND(node_list, node);
		node = node->next;
	}
	G_LIST_SORT(node_list, compare_node);

	/* Update file clist */
	gtk_clist_freeze( GTK_CLIST(file_clist_w) );
	gtk_clist_clear( GTK_CLIST(file_clist_w) );
	node_llink = node_list;
	while (node_llink != NULL) {
		node = (GNode *)node_llink->data;

		row = gtk_clist_append( GTK_CLIST(file_clist_w), empty_row );
		icon = &node_type_mini_icons[NODE_DESC(node)->type];
		gtk_clist_set_pixtext( GTK_CLIST(file_clist_w), row, 0, NODE_DESC(node)->name, 2, icon->pixmap, icon->mask );
		gtk_clist_set_row_data( GTK_CLIST(file_clist_w), row, node );

		++count;
		node_llink = node_llink->next;
	}
	gtk_clist_thaw( GTK_CLIST(file_clist_w) );

	g_list_free( node_list );

	/* Set node count message in the left statusbar */
	switch (count) {
		case 0:
		strcpy( strbuf, "" );
		break;

		case 1:
		strcpy( strbuf, _("1 node") );
		break;

		default:
		sprintf( strbuf, _("%d nodes"), count );
		break;
	}
	window_statusbar( SB_LEFT, strbuf );

	filelist_current_dnode = dnode;
	filelist_reset_access( );
}


/* This updates the file list to show (and select) a particular node
 * entry. The directory tree is also updated appropriately */
void
filelist_show_entry( GNode *node )
{
	GNode *dnode;
	int row;

	/* Corresponding directory */
	if (NODE_IS_DIR(node))
		dnode = node;
	else
		dnode = node->parent;

	if (dnode != filelist_current_dnode) {
		/* Scroll directory tree to proper entry */
		dirtree_entry_show( dnode );
	}

	/* Scroll file list to proper entry */
	row = gtk_clist_find_row_from_data( GTK_CLIST(file_clist_w), node );
	if (row >= 0)
		gtk_clist_select_row( GTK_CLIST(file_clist_w), row, 0 );
	else
		gtk_clist_unselect_all( GTK_CLIST(file_clist_w) );
	gui_clist_moveto_row( file_clist_w, MAX(0, row), FILELIST_SCROLL_TIME );
}


/* Callback for a click in the file list area */
static int
filelist_select_cb( GtkWidget *clist_w, GdkEventButton *ev_button )
{
	GNode *node;
	int row;

	/* If About presentation is up, end it */
	about( ABOUT_END );

	if (globals.fsv_mode == FSV_SPLASH)
		return FALSE;

	gtk_clist_get_selection_info( GTK_CLIST(clist_w), ev_button->x, ev_button->y, &row, NULL );
	if (row < 0)
		return FALSE;

	node = (GNode *)gtk_clist_get_row_data( GTK_CLIST(clist_w), row );
	if (node == NULL)
		return FALSE;

	/* A single-click from button 1 highlights the node and shows the
	 * name (and also selects the row, but GTK+ does that for us) */
	if ((ev_button->button == 1) && (ev_button->type == GDK_BUTTON_PRESS)) {
		geometry_highlight_node( node, FALSE );
		window_statusbar( SB_RIGHT, node_absname( node ) );
		return FALSE;
	}

	/* A double-click from button 1 gets the camera moving */
	if ((ev_button->button == 1) && (ev_button->type == GDK_2BUTTON_PRESS)) {
		camera_look_at( node );
		return FALSE;
	}

	/* A click from button 3 selects the row, highlights the node,
	 * shows the name, and pops up a context-sensitive menu */
	if (ev_button->button == 3) {
		gtk_clist_select_row( GTK_CLIST(clist_w), row, 0 );
		geometry_highlight_node( node, FALSE );
		window_statusbar( SB_RIGHT, node_absname( node ) );
		context_menu( node, ev_button );
		return FALSE;
	}

	return FALSE;
}


/* Creates/initializes the file list widget */
void
filelist_init( void )
{
	GtkWidget *parent_w;

	/* Replace current clist widget with a single-column one */
	parent_w = file_clist_w->parent->parent;
	gtk_widget_destroy( file_clist_w->parent );
	file_clist_w = gui_clist_add( parent_w, 1, NULL );
	gtk_signal_connect( GTK_OBJECT(file_clist_w), "button_press_event", GTK_SIGNAL_FUNC(filelist_select_cb), NULL );

	filelist_populate( root_dnode );

	/* Do this so that directory tree gets scrolled to the to at
	 * end of initial camera pan (right after filesystem scan) */
	filelist_current_dnode = NULL;
}


/* This replaces the file list widget with another one made specifically
 * to monitor the progress of an impending scan */
void
filelist_scan_monitor_init( void )
{
	char *col_titles[3];
	char *empty_row[3] = { NULL, NULL, NULL };
	GtkWidget *parent_w;
	Icon *icon;
	int i;

	col_titles[0] = _("Type");
	col_titles[1] = _("Found");
	col_titles[2] = _("Bytes");

	/* Replace current clist widget with a 3-column one */
	parent_w = file_clist_w->parent->parent;
	gtk_widget_destroy( file_clist_w->parent );
	file_clist_w = gui_clist_add( parent_w, 3, col_titles );

	/* Place icons and static text */
	for (i = 1; i <= NUM_NODE_TYPES; i++) {
		gtk_clist_append( GTK_CLIST(file_clist_w), empty_row );
		if (i < NUM_NODE_TYPES) {
			icon = &node_type_mini_icons[i];
			gtk_clist_set_pixtext( GTK_CLIST(file_clist_w), i - 1, 0, _(node_type_plural_names[i]), 2, icon->pixmap, icon->mask );
		}
		else
                        gtk_clist_set_text( GTK_CLIST(file_clist_w), i - 1, 0, _("TOTAL") );
		gtk_clist_set_selectable( GTK_CLIST(file_clist_w), i - 1, FALSE );
	}
}


/* Updates the scan-monitoring file list with the given values */
void
filelist_scan_monitor( int *node_counts, int64 *size_counts )
{
	const char *str;
	int64 size_total = 0;
	int node_total = 0;
	int i;

	gtk_clist_freeze( GTK_CLIST(file_clist_w) );
	for (i = 1; i <= NUM_NODE_TYPES; i++) {
		/* Column 2 */
		if (i < NUM_NODE_TYPES) {
			str = i64toa( node_counts[i] );
			node_total += node_counts[i];
		}
		else
			str = i64toa( node_total );
		gtk_clist_set_text( GTK_CLIST(file_clist_w), i - 1, 1, str );

		/* Column 3 */
		if (i < NUM_NODE_TYPES) {
			str = i64toa( size_counts[i] );
			size_total += size_counts[i];
		}
		else
			str = i64toa( size_total );
		gtk_clist_set_text( GTK_CLIST(file_clist_w), i - 1, 2, str );
	}
	gtk_clist_thaw( GTK_CLIST(file_clist_w) );
}


/* Creates the clist widget used in the "Contents" page of the Properties
 * dialog for a directory */
GtkWidget *
dir_contents_list( GNode *dnode )
{
        char *col_titles[2];
	char *clist_row[2];
	GtkWidget *clist_w;
	Icon *icon;
	int i;

	g_assert( NODE_IS_DIR(dnode) );

	col_titles[0] = _("Node type");
	col_titles[1] = _("Quantity");

	/* Don't use gui_clist_add( ) as this one shouldn't be placed
	 * inside a scrolled window */
        clist_w = gtk_clist_new_with_titles( 2, col_titles );
	gtk_clist_set_selection_mode( GTK_CLIST(clist_w), GTK_SELECTION_SINGLE );
	for (i = 0; i < 2; i++)
		gtk_clist_set_column_auto_resize( GTK_CLIST(clist_w), i, TRUE );

	clist_row[0] = NULL;
	for (i = 1; i < NUM_NODE_TYPES; i++) {
		clist_row[1] = (char *)i64toa( DIR_NODE_DESC(dnode)->subtree.counts[i] );
		gtk_clist_append( GTK_CLIST(clist_w), clist_row );
		icon = &node_type_mini_icons[i];
		gtk_clist_set_pixtext( GTK_CLIST(clist_w), i - 1, 0, _(node_type_plural_names[i]), 2, icon->pixmap, icon->mask );
	}

	return clist_w;
}


/* end filelist.c */
