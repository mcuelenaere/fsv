/* dirtree.c */

/* Directory tree control */

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
#include "dirtree.h"

#include <gtk/gtk.h>

#include "about.h"
#include "camera.h"
#include "colexp.h"
#include "dialog.h"
#include "filelist.h"
#include "geometry.h"
#include "gui.h"
#include "window.h"

/* Mini collapsed/expanded directory icon XPM's */
#define mini_folder_xpm mini_folder_closed_xpm
#include "xmaps/mini-folder.xpm"
#include "xmaps/mini-folder-open.xpm"


/* Time for the directory tree to scroll to a given entry (in seconds) */
#define DIRTREE_SCROLL_TIME 0.5


/* The directory tree widget */
static GtkWidget *dir_ctree_w;

/* Mini collapsed/expanded directory icons */
static Icon dir_colexp_mini_icons[2];

/* Current directory */
static GNode *dirtree_current_dnode;


/* Callback for button press in the directory tree area */
static int
dirtree_select_cb( GtkWidget *ctree_w, GdkEventButton *ev_button )
{
	GNode *dnode;
	int row;

	/* If About presentation is up, end it */
	about( ABOUT_END );

	if (globals.fsv_mode == FSV_SPLASH)
		return FALSE;

	gtk_clist_get_selection_info( GTK_CLIST(ctree_w), ev_button->x, ev_button->y, &row, NULL );
	if (row < 0)
		return FALSE;

	dnode = (GNode *)gtk_clist_get_row_data( GTK_CLIST(ctree_w), row );
	if (dnode == NULL)
		return FALSE;

	/* A single-click from button 1 highlights the node, shows the
	 * name, and updates the file list if necessary. (and also selects
	 * the row, but GTK+ does that automatically for us) */
	if ((ev_button->button == 1) && (ev_button->type == GDK_BUTTON_PRESS)) {
		geometry_highlight_node( dnode, FALSE );
		window_statusbar( SB_RIGHT, node_absname( dnode ) );
		if (dnode != dirtree_current_dnode)
			filelist_populate( dnode );
		dirtree_current_dnode = dnode;
		return FALSE;
	}

	/* A double-click from button 1 gets the camera moving */
	if ((ev_button->button == 1) && (ev_button->type == GDK_2BUTTON_PRESS)) {
		camera_look_at( dnode );
		/* Preempt the forthcoming tree expand/collapse
		 * (the standard action spawned by a double-click) */
		gtk_signal_emit_stop_by_name( GTK_OBJECT(ctree_w), "button_press_event" );
		return TRUE;
	}

	/* A click from button 3 selects the row, highlights the node,
	 * shows the name, updates the file list if necessary, and brings
	 * up a context-sensitive menu */
	if (ev_button->button == 3) {
		gtk_clist_select_row( GTK_CLIST(ctree_w), row, 0 );
		geometry_highlight_node( dnode, FALSE );
		window_statusbar( SB_RIGHT, node_absname( dnode ) );
		if (dnode != dirtree_current_dnode)
			filelist_populate( dnode );
		dirtree_current_dnode = dnode;
		context_menu( dnode, ev_button );
		return FALSE;
	}

	return FALSE;
}


/* Callback for collapse of a directory tree entry */
static void
dirtree_collapse_cb( GtkWidget *ctree_w, GtkCTreeNode *ctnode )
{
	GNode *dnode;

	if (globals.fsv_mode == FSV_SPLASH)
		return;

	dnode = (GNode *)gtk_ctree_node_get_row_data( GTK_CTREE(ctree_w), ctnode );
	colexp( dnode, COLEXP_COLLAPSE_RECURSIVE );
}


/* Callback for expand of a directory tree entry */
static void
dirtree_expand_cb( GtkWidget *ctree_w, GtkCTreeNode *ctnode )
{
	GNode *dnode;

	if (globals.fsv_mode == FSV_SPLASH)
		return;

	dnode = (GNode *)gtk_ctree_node_get_row_data( GTK_CTREE(ctree_w), ctnode );
	colexp( dnode, COLEXP_EXPAND );
}


/* Loads the mini collapsed/expanded directory icons (from XPM data) */
static void
dirtree_icons_init( void )
{
	static char **dir_colexp_mini_xpms[] = {
		mini_folder_closed_xpm,
		mini_folder_open_xpm
	};
	GtkStyle *style;
	GdkColor *trans_color;
	GdkWindow *window;
	GdkPixmap *pixmap;
	GdkBitmap *mask;
	int i;

	style = gtk_widget_get_style( dir_ctree_w );
	trans_color = &style->bg[GTK_STATE_NORMAL];
	gtk_widget_realize( dir_ctree_w );
	window = dir_ctree_w->window;

	/* Make icons for collapsed and expanded directories */
	for (i = 0; i < 2; i++) {
		pixmap = gdk_pixmap_create_from_xpm_d( window, &mask, trans_color, dir_colexp_mini_xpms[i] );
		dir_colexp_mini_icons[i].pixmap = pixmap;
		dir_colexp_mini_icons[i].mask = mask;
	}
}


/* Correspondence from window_init( ) */
void
dirtree_pass_widget( GtkWidget *ctree_w )
{
	dir_ctree_w = ctree_w;

	/* Connect signal handlers */
	gtk_signal_connect( GTK_OBJECT(dir_ctree_w), "button_press_event", GTK_SIGNAL_FUNC(dirtree_select_cb), NULL );
	gtk_signal_connect( GTK_OBJECT(dir_ctree_w), "tree_collapse", GTK_SIGNAL_FUNC(dirtree_collapse_cb), NULL );
	gtk_signal_connect( GTK_OBJECT(dir_ctree_w), "tree_expand", GTK_SIGNAL_FUNC(dirtree_expand_cb), NULL );

	dirtree_icons_init( );
}


/* Clears out all entries from the directory tree */
void
dirtree_clear( void )
{
	gtk_clist_clear( GTK_CLIST(dir_ctree_w) );
	dirtree_current_dnode = NULL;
}


/* Adds a new entry to the directory tree */
void
dirtree_entry_new( GNode *dnode )
{
	GtkCTreeNode *parent_ctnode = NULL;
	const char *name;
	boolean expanded;

	g_assert( NODE_IS_DIR(dnode) );

	parent_ctnode = DIR_NODE_DESC(dnode->parent)->ctnode;
	if (strlen( NODE_DESC(dnode)->name ) > 0)
		name = NODE_DESC(dnode)->name;
	else
		name = _("/. (root)");
	expanded = g_node_depth( dnode ) <= 2;

	DIR_NODE_DESC(dnode)->ctnode = gui_ctree_node_add( dir_ctree_w, parent_ctnode, dir_colexp_mini_icons, name, expanded, dnode );

	if (parent_ctnode == NULL) {
		/* First entry was just added. Keep directory tree frozen
		 * most of the time while scanning, otherwise it tends to
		 * flicker annoyingly */
		gtk_clist_freeze( GTK_CLIST(dir_ctree_w) );
	}
	else if (GTK_CTREE_ROW(parent_ctnode)->expanded) {
		/* Pre-update (allow ctree to register new row) */
		gtk_clist_thaw( GTK_CLIST(dir_ctree_w) );
		gui_update( );
		gtk_clist_freeze( GTK_CLIST(dir_ctree_w) );
		/* Select last row */
		gtk_ctree_select( GTK_CTREE(dir_ctree_w), DIR_NODE_DESC(dnode)->ctnode );
		/* Scroll directory tree down to last row */
		gui_clist_moveto_row( dir_ctree_w, -1, 0.0 );
		/* Post-update (allow ctree to perform select/scroll) */
		gtk_clist_thaw( GTK_CLIST(dir_ctree_w) );
		gui_update( );
		gtk_clist_freeze( GTK_CLIST(dir_ctree_w) );
	}
}


/* Call this after the last call to dirtree_entry_new( ) */
void
dirtree_no_more_entries( void )
{
	gtk_clist_thaw( GTK_CLIST(dir_ctree_w) );
}


/* This updates the directory tree to show (and select) a particular
 * directory entry, repopulating the file list with the contents of the
 * directory if not already listed */
void
dirtree_entry_show( GNode *dnode )
{
	int row;

	g_assert( NODE_IS_DIR(dnode) );

	/* Repopulate file list if directory is different */
	if (dnode != dirtree_current_dnode) {
		filelist_populate( dnode );
/* TODO: try removing this update from here */
		gui_update( );
	}

	/* Scroll directory tree to proper entry */
	row = gtk_clist_find_row_from_data( GTK_CLIST(dir_ctree_w), dnode );
	if (row >= 0)
		gtk_clist_select_row( GTK_CLIST(dir_ctree_w), row, 0 );
	else
		gtk_clist_unselect_all( GTK_CLIST(dir_ctree_w) );
	gui_clist_moveto_row( dir_ctree_w, MAX(0, row), DIRTREE_SCROLL_TIME );

	dirtree_current_dnode = dnode;
}


/* Returns TRUE if the entry for the given directory is expanded */
boolean
dirtree_entry_expanded( GNode *dnode )
{
	g_assert( NODE_IS_DIR(dnode) );

	return GTK_CTREE_ROW(DIR_NODE_DESC(dnode)->ctnode)->expanded;
}


/* Helper function */
static void
block_colexp_handlers( void )
{
	gtk_signal_handler_block_by_func( GTK_OBJECT(dir_ctree_w), GTK_SIGNAL_FUNC(dirtree_collapse_cb), NULL );
	gtk_signal_handler_block_by_func( GTK_OBJECT(dir_ctree_w), GTK_SIGNAL_FUNC(dirtree_expand_cb), NULL );
}


/* Helper function */
static void
unblock_colexp_handlers( void )
{
	gtk_signal_handler_unblock_by_func( GTK_OBJECT(dir_ctree_w), GTK_SIGNAL_FUNC(dirtree_collapse_cb), NULL );
	gtk_signal_handler_unblock_by_func( GTK_OBJECT(dir_ctree_w), GTK_SIGNAL_FUNC(dirtree_expand_cb), NULL );
}


/* Recursively collapses the directory tree entry of the given directory */
void
dirtree_entry_collapse_recursive( GNode *dnode )
{
	g_assert( NODE_IS_DIR(dnode) );

	block_colexp_handlers( );
	gtk_ctree_collapse_recursive( GTK_CTREE(dir_ctree_w), DIR_NODE_DESC(dnode)->ctnode );
	unblock_colexp_handlers( );
}


/* Expands the directory tree entry of the given directory. If any of its
 * ancestor directory entries are not expanded, then they are expanded
 * as well */
void
dirtree_entry_expand( GNode *dnode )
{
	GNode *up_node;

	g_assert( NODE_IS_DIR(dnode) );

	block_colexp_handlers( );
	up_node = dnode;
	while (NODE_IS_DIR(up_node)) {
		if (!dirtree_entry_expanded( up_node ))
			gtk_ctree_expand( GTK_CTREE(dir_ctree_w), DIR_NODE_DESC(up_node)->ctnode );
		up_node = up_node->parent;
	}
	unblock_colexp_handlers( );
}


/* Recursively expands the entire directory tree subtree of the given
 * directory */
void
dirtree_entry_expand_recursive( GNode *dnode )
{
	g_assert( NODE_IS_DIR(dnode) );

#if DEBUG
	/* Guard against expansions inside collapsed subtrees */
	/** NOTE: This function may be upgraded to behave similarly to
	 ** dirtree_entry_expand( ) w.r.t. collapsed parent directories.
	 ** This has been avoided thus far since such a behavior would
	 ** not be used by the program. */
	if (NODE_IS_DIR(dnode->parent))
		g_assert( dirtree_entry_expanded( dnode->parent ) );
#endif

	block_colexp_handlers( );
	gtk_ctree_expand_recursive( GTK_CTREE(dir_ctree_w), DIR_NODE_DESC(dnode)->ctnode );
	unblock_colexp_handlers( );
}


/* end dirtree.c */
